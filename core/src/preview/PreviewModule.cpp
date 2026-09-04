// PreviewModule.cpp — 低帧实时预览实现
#include "preview/PreviewModule.hpp"

#if defined(_WIN32)
// Windows 无 V4L2：Preview 模块仅板端编译（CMake 守卫）
namespace ttbox::core {
}
#else

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <jpeglib.h>
#include <csetjmp>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

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

// 检测框绘制统一入口：原图系 (src_w x src_h, 原点 ox,oy) → 预览系 (w x h)。
// 与 DecodeNMS 坐标映射一致：preview = (orig - origin) * (out / src)。
void PreviewModule::draw_boxes(uint8_t* buf, uint32_t w, uint32_t h, uint32_t stride,
                               const std::vector<DetectionBox>& boxes,
                               float src_w, float src_h, float ox, float oy) const {
    if (!buf || boxes.empty() || w == 0 || h == 0) return;
    const float sx = src_w > 0.0f ? static_cast<float>(w) / src_w : 1.0f;
    const float sy = src_h > 0.0f ? static_cast<float>(h) / src_h : 1.0f;
    cv::Mat img(h, w, CV_8UC3, buf, stride);
    for (const auto& b : boxes) {
        const float px1 = (b.x1 - ox) * sx;
        const float py1 = (b.y1 - oy) * sy;
        const float px2 = (b.x2 - ox) * sx;
        const float py2 = (b.y2 - oy) * sy;
        const int X1 = static_cast<int>(std::max(0.0f, std::min(px1, static_cast<float>(w - 1))));
        const int Y1 = static_cast<int>(std::max(0.0f, std::min(py1, static_cast<float>(h - 1))));
        const int X2 = static_cast<int>(std::max(0.0f, std::min(px2, static_cast<float>(w - 1))));
        const int Y2 = static_cast<int>(std::max(0.0f, std::min(py2, static_cast<float>(h - 1))));
        if (X2 <= X1 || Y2 <= Y1) continue;
        // 颜色：身体类(1)=绿，头部类(0)=红，其他=黄
        const cv::Scalar color = (b.class_id == 1) ? cv::Scalar(0, 200, 0)
                               : (b.class_id == 0) ? cv::Scalar(0, 0, 255)
                               : cv::Scalar(0, 200, 200);
        const int thickness = (b.class_id == 1) ? 2 : 1;
        cv::rectangle(img, cv::Point(X1, Y1), cv::Point(X2, Y2), color, thickness);
        char label[64];
        std::snprintf(label, sizeof(label), "%d %.2f", b.class_id, b.score);
        cv::putText(img, label, cv::Point(X1, std::max(10, Y1 - 4)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1, cv::LINE_AA);
    }
}

// CPU 直拷路径：预览输出 = ROI 1:1，检测框原图系减 ROI 原点
void PreviewModule::draw_detections_cpu(uint8_t* roi, uint32_t roi_w, uint32_t roi_h,
                                        uint32_t roi_stride, const std::vector<DetectionBox>& boxes) const {
    draw_boxes(roi, roi_w, roi_h, roi_stride, boxes,
               static_cast<float>(applied_roi_w_), static_cast<float>(applied_roi_h_),
               static_cast<float>(applied_roi_x_), static_cast<float>(applied_roi_y_));
}

// 检测框平滑：EMA 融合连续帧，消除抖动闪烁；丢帧超阈值则清空
void PreviewModule::smooth_boxes(const std::vector<DetectionBox>& raw,
                                 std::vector<DetectionBox>* out) {
    if (!out) return;
    if (raw.empty()) {
        ++smooth_lost_count_;
        if (smooth_lost_count_ > 5) {  // 连续 5 帧无目标 → 清空旧框，防残影
            smooth_prev_.clear();
        }
        *out = smooth_prev_;  // 短暂丢帧时保留上一帧，防闪烁
        return;
    }
    smooth_lost_count_ = 0;
    const float alpha = 0.35f;  // 新帧权重（低=更平滑，高=更跟手）
    std::vector<DetectionBox> result;
    result.reserve(raw.size());
    for (const auto& b : raw) {
        DetectionBox blended = b;
        // 按类别+中心最近匹配上一帧，只平滑坐标
        const float cx = (b.x1 + b.x2) * 0.5f, cy = (b.y1 + b.y2) * 0.5f;
        const DetectionBox* best = nullptr;
        float best_d = 1e18f;
        for (const auto& p : smooth_prev_) {
            if (p.class_id != b.class_id) continue;
            const float pcx = (p.x1 + p.x2) * 0.5f, pcy = (p.y1 + p.y2) * 0.5f;
            const float d = (pcx - cx) * (pcx - cx) + (pcy - cy) * (pcy - cy);
            if (d < best_d) { best_d = d; best = &p; }
        }
        if (best && best_d < 40000.0f) {  // <200px 视为同一目标
            blended.x1 = best->x1 * (1 - alpha) + b.x1 * alpha;
            blended.y1 = best->y1 * (1 - alpha) + b.y1 * alpha;
            blended.x2 = best->x2 * (1 - alpha) + b.x2 * alpha;
            blended.y2 = best->y2 * (1 - alpha) + b.y2 * alpha;
            blended.score = best->score * (1 - alpha) + b.score * alpha;
        }
        result.push_back(blended);
    }
    smooth_prev_ = result;
    *out = std::move(result);
}

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
                        // 设计：预览输出尺寸 = 截取尺寸（1:1，不拉伸）。
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
    // CPU 直拷优先（ultra 同款）：mmap va 行抽取 ROI → JPEG，完全避开 RGA 撕裂/花屏。
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
        // 检测框绘制（开启时）：原图系 → ROI 预览系；EMA 平滑防闪烁
        if (params_.draw_detections) {
            std::vector<DetectionBox> raw, boxes;
            {
                std::lock_guard<std::mutex> lk(provider_mutex_);
                if (detections_provider_) raw = detections_provider_();
            }
            smooth_boxes(raw, &boxes);
            draw_detections_cpu(roi.data(), applied_roi_w_, applied_roi_h_,
                                static_cast<uint32_t>(row_bytes), boxes);
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

    // 检测框绘制（开启时）：RGA 输出 = ROI 区域（1:1 或缩放），原图系 → 预览系
    if (params_.draw_detections) {
        std::vector<DetectionBox> raw, boxes;
        {
            std::lock_guard<std::mutex> lk(provider_mutex_);
            if (detections_provider_) raw = detections_provider_();
        }
        smooth_boxes(raw, &boxes);
        if (!boxes.empty()) {
            draw_boxes(static_cast<uint8_t*>(rga_out.vir_addr), rga_out.width, rga_out.height,
                       rga_out.stride, boxes,
                       static_cast<float>(applied_roi_w_ > 0 ? applied_roi_w_ : frame.info.width),
                       static_cast<float>(applied_roi_h_ > 0 ? applied_roi_h_ : frame.info.height),
                       static_cast<float>(applied_roi_x_), static_cast<float>(applied_roi_y_));
        }
    }

    // RGA 输出虚拟地址（mmap 常驻）直接喂 JPEG 编码器
    return bgr3_to_jpeg(static_cast<const uint8_t*>(rga_out.vir_addr),
                        rga_out.width, rga_out.height, rga_out.stride,
                        params_.jpeg_quality, jpeg_out, error);
}

}  // namespace ttbox::core
#endif  // _WIN32
