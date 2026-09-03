// PreviewModule.cpp — 低帧实时预览实现
#include "preview/PreviewModule.hpp"

#if defined(_WIN32)
// Windows 无 V4L2：Preview 模块仅板端编译（CMake 守卫）
namespace ttbox::core {
}
#else

#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <jpeglib.h>
#include <csetjmp>

#include "common/Logger.hpp"

namespace ttbox::core {

namespace {

using clock = std::chrono::steady_clock;

// 简化 JPEG 错误处理：setjmp 返回
struct JpegErrorMgr {
    jpeg_error_mgr base;
    jmp_buf jmp;
};
void jpeg_error_exit(j_common_ptr cinfo) {
    auto* err = reinterpret_cast<JpegErrorMgr*>(cinfo->err);
    longjmp(err->jmp, 1);
}

// BGR3 → JPEG（libjpeg 顺序扫描）
bool bgr3_to_jpeg(const uint8_t* bgr, uint32_t w, uint32_t h, uint32_t stride,
                  int quality, std::vector<uint8_t>* out, std::string* error) {
    jpeg_compress_struct cinfo{};
    JpegErrorMgr jerr{};
    cinfo.err = jpeg_std_error(&jerr.base);
    jerr.base.error_exit = jpeg_error_exit;
    if (setjmp(jerr.jmp)) {
        jpeg_destroy_compress(&cinfo);
        if (error) *error = "JPEG 编码失败";
        return false;
    }
    jpeg_create_compress(&cinfo);

    unsigned char* mem = nullptr;
    unsigned long mem_size = 0;
    jpeg_mem_dest(&cinfo, &mem, &mem_size);

    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;  // BGR 数据按 RGB 编码（颜色通道互换无害于预览）
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    std::vector<uint8_t> rgb_row(w * 3);
    while (cinfo.next_scanline < cinfo.image_height) {
        const uint8_t* src = bgr + static_cast<size_t>(cinfo.next_scanline) * stride;
        // BGR → RGB 交换
        for (uint32_t x = 0; x < w; ++x) {
            rgb_row[x * 3 + 0] = src[x * 3 + 2];
            rgb_row[x * 3 + 1] = src[x * 3 + 1];
            rgb_row[x * 3 + 2] = src[x * 3 + 0];
        }
        uint8_t* rows[1] = {rgb_row.data()};
        jpeg_write_scanlines(&cinfo, rows, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    if (mem && mem_size > 0) {
        out->assign(mem, mem + mem_size);
        std::free(mem);
        return true;
    }
    if (error) *error = "JPEG 输出为空";
    return false;
}

}  // namespace

bool PreviewModule::start(const LatestFrame* frame_source, const Params& params, std::string* error) {
    if (!frame_source) {
        if (error) *error = "预览帧源为空";
        return false;
    }
    if (params.out_width == 0 || params.out_height == 0 || params.fps <= 0 || params.fps > 60) {
        if (error) *error = "预览参数无效";
        return false;
    }
    if (running_.exchange(true)) return false;
    latest_ = frame_source;
    params_ = params;
    metrics_.width.store(params.out_width);
    metrics_.height.store(params.out_height);

    // 使用 RGA 硬件缩放（替代 CPU 双线性）：全画面直接拉伸到预览尺寸。
    // 为什么：CPU 双线性缩放 2560x1440 → 640x360 每帧 70~100ms 占用 CPU，
    //         拖累 AI 流水线约 6%；RGA 硬件缩放仅 1~3ms 且不占 CPU。
    rga_ = std::make_unique<RgaProcessor>();
    RgaProcessor::Params rp;
    rp.output_width = params.out_width;
    rp.output_height = params.out_height;
    rp.center_crop = false;   // 预览要全画面（不裁剪）
    rp.out_color = 0;         // BGR888（与 bgr3_to_jpeg 输入一致）
    std::string rerr;
    if (!rga_->init(rp, &rerr)) {
        if (error) *error = "预览 RGA 初始化失败: " + rerr;
        running_ = false;
        rga_.reset();
        return false;
    }
    thread_ = std::thread(&PreviewModule::loop, this);
    return true;
}

void PreviewModule::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    if (rga_) {
        rga_->destroy();
        rga_.reset();
    }
    std::lock_guard<std::mutex> lk(jpeg_mutex_);
    jpeg_.clear();
}

bool PreviewModule::snapshot(std::vector<uint8_t>* jpeg_out) const {
    if (!jpeg_out) return false;
    std::lock_guard<std::mutex> lk(jpeg_mutex_);
    if (jpeg_.empty()) return false;
    *jpeg_out = jpeg_;
    return true;
}

void PreviewModule::loop() {
    using clock = std::chrono::steady_clock;
    const auto start_time = clock::now();
    const auto interval = std::chrono::milliseconds(1000 / params_.fps);
    auto next_tick = clock::now();

    while (running_.load()) {
        // 限帧：固定节奏（latest 语义，编码超时则跳过）
        const auto t0 = clock::now();
        if (t0 < next_tick) {
            std::this_thread::sleep_for(next_tick - t0);
        }
        next_tick = clock::now() + interval;

        if (!latest_) continue;
        auto frame = latest_->get();
        if (!frame || frame->size == 0 || frame->info.dma_fd < 0) {
            // 无帧可用：等待下一拍（不计数）
            continue;
        }

        // 截取区域跟随（capture ROI）：预览显示 AI 看到的画面而非全屏。
        // ROI 变化（含热更新 offset/size）时重新 set_roi；无 ROI 时保持全画面。
        if (params_.runtime_config != nullptr) {
            if (auto prof = params_.runtime_config->snapshot()) {
                const uint32_t rw = prof->capture.width, rh = prof->capture.height;
                const uint32_t fw = frame->info.width, fh = frame->info.height;
                if (rw > 0 && rh > 0 && fw > 0 && fh > 0 && rw <= fw && rh <= fh) {
                    const int32_t cx = static_cast<int32_t>(fw / 2) + prof->capture.offset_x;
                    const int32_t cy = static_cast<int32_t>(fh / 2) + prof->capture.offset_y;
                    const int32_t rx = std::max<int32_t>(0, std::min<int32_t>(
                        cx - static_cast<int32_t>(rw / 2), static_cast<int32_t>(fw - rw)));
                    const int32_t ry = std::max<int32_t>(0, std::min<int32_t>(
                        cy - static_cast<int32_t>(rh / 2), static_cast<int32_t>(fh - rh)));
                    if (rx != applied_roi_x_ || ry != applied_roi_y_ ||
                        rw != applied_roi_w_ || rh != applied_roi_h_) {
                        // YU 语义：预览输出尺寸 = 截取尺寸（1:1，不拉伸）。
                        // 尺寸变化时重建 RGA 输出 buffer（含 set_roi）；仅偏移变化只 set_roi。
                        if (rw != applied_roi_w_ || rh != applied_roi_h_) {
                            RgaProcessor::Params rp;
                            rp.output_width = rw;
                            rp.output_height = rh;
                            rp.center_crop = false;
                            rp.out_color = 0;
                            rga_->destroy();
                            std::string rerr2;
                            if (rga_->init(rp, &rerr2)) {
                                metrics_.width.store(rw);
                                metrics_.height.store(rh);
                            } else {
                                TTBOX_LOG_WARN("预览 RGA 重建失败: " + rerr2);
                            }
                        }
                        rga_->set_roi(static_cast<uint32_t>(rx), static_cast<uint32_t>(ry), rw, rh);
                        applied_roi_x_ = rx; applied_roi_y_ = ry;
                        applied_roi_w_ = rw; applied_roi_h_ = rh;
                    }
                }
            }
        }

        // 编码前一帧是否超时？若上次编码还占着（单线程不会有），此处无竞态。
        const auto te0 = clock::now();
        std::vector<uint8_t> jpeg;
        std::string err;
        if (encode_frame(*frame, &jpeg, &err)) {
            {
                std::lock_guard<std::mutex> lk(jpeg_mutex_);
                jpeg_ = std::move(jpeg);
            }
            metrics_.frames.fetch_add(1);
            metrics_.bytes.store(static_cast<uint32_t>(jpeg_.size()));
        } else {
            metrics_.dropped.fetch_add(1);
            if (metrics_.dropped.load() <= 3) {
                TTBOX_LOG_WARN("Preview 编码失败: " + err);
            }
        }
        const double enc_ms = std::chrono::duration<double, std::milli>(clock::now() - te0).count();
        metrics_.encode_ms.store(enc_ms);
        const double elapsed_s = std::chrono::duration<double>(clock::now() - start_time).count();
        if (elapsed_s > 0.0) {
            metrics_.fps.store(static_cast<double>(metrics_.frames.load()) / elapsed_s);
        }
    }
}

bool PreviewModule::encode_frame(const FrameBuffer& frame, std::vector<uint8_t>* jpeg_out, std::string* error) {
    const uint32_t dw = params_.out_width;
    const uint32_t dh = params_.out_height;
    if (dw == 0 || dh == 0) {
        if (error) *error = "帧尺寸无效";
        return false;
    }
    // CPU 直拷优先（YU ultra 同款）：mmap va 行抽取 ROI → JPEG，完全避开 RGA 撕裂/花屏。
    if (frame.info.cpu_va != nullptr && applied_roi_w_ > 0 && applied_roi_h_ > 0) {
        const uint8_t* base = static_cast<const uint8_t*>(frame.info.cpu_va);
        const uint32_t sstride = frame.info.stride;
        const size_t row_bytes = static_cast<size_t>(applied_roi_w_) * 3;
        std::vector<uint8_t> roi;
        roi.reserve(static_cast<size_t>(applied_roi_w_) * applied_roi_h_ * 3);
        for (uint32_t y = 0; y < applied_roi_h_; ++y) {
            const uint8_t* src = base + static_cast<size_t>(applied_roi_y_ + y) * sstride
                               + static_cast<size_t>(applied_roi_x_) * 3;
            roi.insert(roi.end(), src, src + row_bytes);
        }
        return bgr3_to_jpeg(roi.data(), applied_roi_w_, applied_roi_h_,
                            row_bytes, params_.jpeg_quality, jpeg_out, error);
    }

    if (!rga_) {
        if (error) *error = "预览 RGA 未初始化";
        return false;
    }

    // RGA 回退：DMA-BUF fd → 预览尺寸 BGR888（常驻输出 buffer，无 CPU memcpy）
    RgaOutput rga_out;
    std::string rerr;
    if (!rga_->process(frame, &rga_out, &rerr)) {
        if (error) *error = "预览 RGA 缩放失败: " + rerr;
        return false;
    }

    // RGA 输出虚拟地址（mmap 常驻）直接喂 JPEG 编码器
    return bgr3_to_jpeg(static_cast<const uint8_t*>(rga_out.vir_addr),
                        rga_out.width, rga_out.height, rga_out.stride,
                        params_.jpeg_quality, jpeg_out, error);
}

}  // namespace ttbox::core
#endif  // _WIN32
