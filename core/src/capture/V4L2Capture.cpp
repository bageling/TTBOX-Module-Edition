// V4L2Capture.cpp — V4L2 MPLANE 采集实现（RK3588 /dev/video0）
#include "capture/V4L2Capture.hpp"

#if defined(_WIN32)
// Windows 占位：本阶段无 V4L2 硬件，CMake 仅在 Unix 编译本文件。
namespace ttbox::core {
}
#else

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "capture/DmaBuf.hpp"
#include "common/Logger.hpp"
#include "common/CpuAffinity.hpp"

namespace ttbox::core {

namespace {

// ioctl 包装：统一错误处理
int ioctl_call(int fd, unsigned long request, void* arg) {
    return ::ioctl(fd, request, arg);
}

}  // namespace

// ===========================================================================
// LatestFrame
// ===========================================================================

std::shared_ptr<FrameBuffer> LatestFrame::publish(std::shared_ptr<FrameBuffer> frame) {
    // 无锁化（C++17 原子 shared_ptr 自由函数）：capture 线程不再与 3 worker/preview 抢 mutex，
    // 消除 publish 阻塞导致的帧延迟抖动。
    auto old = std::atomic_load_explicit(&current_, std::memory_order_acquire);
    std::atomic_store_explicit(&current_, std::move(frame), std::memory_order_release);
    return old;
}

std::shared_ptr<FrameBuffer> LatestFrame::get() const {
    return std::atomic_load_explicit(&current_, std::memory_order_acquire);
}

void LatestFrame::clear() {
    std::atomic_store_explicit(&current_, std::shared_ptr<FrameBuffer>(), std::memory_order_release);
}

// ===========================================================================
// 内部结构
// ===========================================================================

namespace {

// 单个 plane 资源（mmap + dma-buf fd，RAII）
struct PlaneRes {
    void* addr = nullptr;
    size_t length = 0;
    DmaBufFd dma_fd;        // move-only
    uint32_t plane_index = 0;
};

// 单个 V4L2 buffer 资源
struct BufferRes {
    int index = -1;
    std::vector<PlaneRes> planes;
};

// 等待归还的 buffer（weak 引用：无强引用时方可安全 QBUF）
struct PendingRelease {
    int index = -1;
    std::vector<size_t> plane_lengths;
    std::weak_ptr<FrameBuffer> weak;
};

}  // namespace

struct V4L2Capture::Impl {
    std::vector<BufferRes> buffers;          // REQBUFS 后固定
    std::vector<PendingRelease> pending;     // 待归还队列
    std::vector<bool> captured;              // index 是否已 DQBUF 未归还
};

// ===========================================================================
// 工具
// ===========================================================================

namespace {

const char* fourcc_name(uint32_t fourcc) {
    static thread_local char buf[5];
    buf[0] = static_cast<char>(fourcc & 0xFF);
    buf[1] = static_cast<char>((fourcc >> 8) & 0xFF);
    buf[2] = static_cast<char>((fourcc >> 16) & 0xFF);
    buf[3] = static_cast<char>((fourcc >> 24) & 0xFF);
    buf[4] = '\0';
    return buf;
}

PixelFormat map_pixel_format(uint32_t fourcc) {
    // V4L2_PIX_FMT_BGR24 = 'BGR3'
    if (fourcc == V4L2_PIX_FMT_BGR24) return PixelFormat::kBGR888;
    if (fourcc == V4L2_PIX_FMT_RGB24) return PixelFormat::kRGB888;
    if (fourcc == V4L2_PIX_FMT_RGB32 || fourcc == V4L2_PIX_FMT_BGR32) return PixelFormat::kRGBA8888;
    if (fourcc == V4L2_PIX_FMT_NV12) return PixelFormat::kNV12;
    return PixelFormat::kUnknown;
}

double tv_to_ms(const struct timeval& tv) {
    return static_cast<double>(tv.tv_sec) * 1000.0 +
           static_cast<double>(tv.tv_usec) / 1000.0;
}

}  // namespace

std::string V4L2Capture::FormatInfo::fourcc_str() const {
    return fourcc_name(pixelformat);
}

// ===========================================================================
// 生命周期
// ===========================================================================

V4L2Capture::V4L2Capture() : impl_(std::make_unique<Impl>()) {}

V4L2Capture::~V4L2Capture() {
    stop();
    close();
}

bool V4L2Capture::configure(const Params& params, std::string* error) {
    if (running_.load()) {
        if (error) *error = "capture 运行中，禁止 re-configure";
        return false;
    }
    if (params.device.empty()) {
        if (error) *error = "设备路径为空";
        return false;
    }
    if (params.num_buffers == 0 || params.num_buffers > 32) {
        if (error) *error = "num_buffers 必须在 1~32";
        return false;
    }
    params_ = params;
    return true;
}

bool V4L2Capture::open(std::string* error) {
    if (opened_) {
        if (error) *error = "设备已打开";
        return false;
    }

    // 重试循环：hdmirx 驱动在开机后需要 1~2 秒才能完成 format change
    // （从中间格式过渡到最终 2560x1440），如果格式不稳定就关闭重开。
    const int kMaxRetries = 5;
    for (int retry = 0; retry < kMaxRetries; ++retry) {
        if (opened_) {
            // 前一次重试的残余
            close();
            opened_ = false;
        }

        // ---- 1. open ----
        fd_ = ::open(params_.device.c_str(), O_RDWR);
        if (fd_ < 0) {
            if (error) *error = "open(" + params_.device + ") 失败: " + std::string(std::strerror(errno));
            return false;
        }
        TTBOX_LOG_INFO("V4L2 设备已打开: " + params_.device + " fd=" + std::to_string(fd_));

        // ---- 2. QUERYCAP ----
        struct v4l2_capability cap {};
        if (ioctl_call(fd_, VIDIOC_QUERYCAP, &cap) != 0) {
            if (error) *error = "VIDIOC_QUERYCAP 失败";
            close();
            return false;
        }
        const uint32_t caps = cap.capabilities;
        const bool mplane_ok = (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0;
        const bool streaming_ok = (caps & V4L2_CAP_STREAMING) != 0;
        TTBOX_LOG_INFO("QUERYCAP: driver=" + std::string(reinterpret_cast<const char*>(cap.driver)) +
                       " card=" + std::string(reinterpret_cast<const char*>(cap.card)) +
                       " MPLANE=" + std::string(mplane_ok ? "yes" : "no") +
                       " STREAMING=" + std::string(streaming_ok ? "yes" : "no"));
        if (!mplane_ok || !streaming_ok) {
            if (error) *error = "设备缺少 V4L2_CAP_VIDEO_CAPTURE_MPLANE 或 V4L2_CAP_STREAMING";
            close();
            return false;
        }

        // ---- 3. G_FMT（读取实际格式，不强制改分辨率）----
        struct v4l2_format fmt {};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (ioctl_call(fd_, VIDIOC_G_FMT, &fmt) != 0) {
            if (error) *error = "VIDIOC_G_FMT 失败";
            close();
            return false;
        }

        const uint32_t first_w = fmt.fmt.pix_mp.width;
        const uint32_t first_h = fmt.fmt.pix_mp.height;

        // ---- 4. 等待格式稳定（hdmirx 开机过渡）----
        // 延迟 800ms 后重新读取格式，如果改变了说明驱动还在过渡中
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        struct v4l2_format fmt2 {};
        fmt2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (ioctl_call(fd_, VIDIOC_G_FMT, &fmt2) != 0) {
            if (error) *error = "VIDIOC_G_FMT(重试) 失败";
            close();
            return false;
        }

        if (fmt2.fmt.pix_mp.width != first_w || fmt2.fmt.pix_mp.height != first_h) {
            TTBOX_LOG_WARN("V4L2 格式不稳定: " + std::to_string(first_w) + "x" + std::to_string(first_h) +
                           " -> " + std::to_string(fmt2.fmt.pix_mp.width) + "x" + std::to_string(fmt2.fmt.pix_mp.height) +
                           "，重试 open (" + std::to_string(retry + 1) + "/" + std::to_string(kMaxRetries) + ")");
            ::close(fd_);
            fd_ = -1;
            continue;
        }

        // 格式稳定，使用 fmt2 继续
        fmt = fmt2;

        // 可选硬件 Selection/Crop：失败时保留完整帧，后续由 RGA fallback。
        format_.selection_supported = false;
        format_.selection_applied = false;
        if (params_.crop_width > 0 && params_.crop_height > 0) {
            struct v4l2_selection sel {};
            sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            sel.target = V4L2_SEL_TGT_CROP;
            sel.r.left = static_cast<__s32>(params_.crop_x);
            sel.r.top = static_cast<__s32>(params_.crop_y);
            sel.r.width = params_.crop_width;
            sel.r.height = params_.crop_height;
            if (ioctl_call(fd_, VIDIOC_S_SELECTION, &sel) == 0) {
                format_.selection_supported = true;
                format_.selection_applied = true;
                TTBOX_LOG_INFO("V4L2 Selection/Crop 已应用: " +
                               std::to_string(sel.r.left) + "," + std::to_string(sel.r.top) + " " +
                               std::to_string(sel.r.width) + "x" + std::to_string(sel.r.height));
                if (ioctl_call(fd_, VIDIOC_G_FMT, &fmt) != 0) {
                    if (error) *error = "Selection 成功后 VIDIOC_G_FMT 失败";
                    close();
                    return false;
                }
            } else {
                TTBOX_LOG_WARN("V4L2 Selection/Crop 不可用，回退完整帧 + Preprocess/RGA: " +
                               std::string(std::strerror(errno)));
            }
        }

        format_.width = fmt.fmt.pix_mp.width;
        format_.height = fmt.fmt.pix_mp.height;
        format_.pixelformat = fmt.fmt.pix_mp.pixelformat;
        format_.num_planes = fmt.fmt.pix_mp.num_planes;
        format_.bytesperline.clear();
        format_.sizeimage.clear();
        for (uint32_t p = 0; p < format_.num_planes; ++p) {
            format_.bytesperline.push_back(fmt.fmt.pix_mp.plane_fmt[p].bytesperline);
            format_.sizeimage.push_back(fmt.fmt.pix_mp.plane_fmt[p].sizeimage);
        }
        if (format_.num_planes == 0 || format_.num_planes > 8) {
            if (error) *error = "G_FMT 返回非法 num_planes=" + std::to_string(format_.num_planes);
            close();
            return false;
        }
        {
            std::string log = "G_FMT: " + std::to_string(format_.width) + "x" +
                              std::to_string(format_.height) + " fourcc=" +
                              format_.fourcc_str() + " planes=" + std::to_string(format_.num_planes);
            for (uint32_t p = 0; p < format_.num_planes; ++p) {
                log += " [p" + std::to_string(p) + " bpl=" +
                       std::to_string(format_.bytesperline[p]) + " size=" +
                       std::to_string(format_.sizeimage[p]) + "]";
            }
            TTBOX_LOG_INFO(log);
        }

        // ---- 5. REQBUFS（MMAP，默认 4，驱动实际为准）----
        struct v4l2_requestbuffers req {};
        req.count = params_.num_buffers;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        req.memory = V4L2_MEMORY_MMAP;
        if (ioctl_call(fd_, VIDIOC_REQBUFS, &req) != 0) {
            if (error) *error = "VIDIOC_REQBUFS 失败";
            close();
            return false;
        }
        buffer_count_ = req.count;
        if (buffer_count_ == 0) {
            if (error) *error = "REQBUFS 返回 0 个 buffer（驱动拒绝 MMAP）";
            close();
            return false;
        }
        if (buffer_count_ < params_.num_buffers) {
            TTBOX_LOG_WARN("REQBUFS: 请求 " + std::to_string(params_.num_buffers) +
                           " 个 buffer，驱动实际提供 " + std::to_string(buffer_count_) +
                           " 个，使用实际数量");
        } else {
            TTBOX_LOG_INFO("REQBUFS: " + std::to_string(buffer_count_) + " 个 buffer");
        }

        // ---- 6. 每 buffer 每 plane：QUERYBUF + mmap + EXPBUF ----
        impl_->buffers.resize(buffer_count_);
        impl_->captured.assign(buffer_count_, false);
        for (uint32_t i = 0; i < buffer_count_; ++i) {
            struct v4l2_buffer buf {};
            struct v4l2_plane planes[8] {};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            buf.length = format_.num_planes;
            buf.m.planes = planes;
            if (ioctl_call(fd_, VIDIOC_QUERYBUF, &buf) != 0) {
                if (error) *error = "VIDIOC_QUERYBUF[" + std::to_string(i) + "] 失败";
                close();
                return false;
            }

            BufferRes& bres = impl_->buffers[i];
            bres.index = static_cast<int>(i);
            bres.planes.resize(format_.num_planes);
            for (uint32_t p = 0; p < format_.num_planes; ++p) {
                PlaneRes& pres = bres.planes[p];
                pres.plane_index = p;
                pres.length = planes[p].length;
                pres.addr = ::mmap(nullptr, pres.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_,
                                   static_cast<off_t>(planes[p].m.mem_offset));
                if (pres.addr == MAP_FAILED) {
                    pres.addr = nullptr;
                    pres.length = 0;
                    if (error) *error = "mmap[buf=" + std::to_string(i) + ",plane=" + std::to_string(p) + "] 失败";
                    metrics_.errors.fetch_add(1);
                    close();
                    return false;
                }

                // EXPBUF：导出 DMA-BUF fd（失败则该 plane 无 fd，不整体失败）
                struct v4l2_exportbuffer eb {};
                eb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                eb.index = i;
                eb.plane = p;
                eb.flags = 0;
                if (ioctl_call(fd_, VIDIOC_EXPBUF, &eb) != 0) {
                    TTBOX_LOG_WARN("VIDIOC_EXPBUF[buf=" + std::to_string(i) + ",plane=" +
                                   std::to_string(p) + "] 失败: " + std::string(std::strerror(errno)));
                    metrics_.errors.fetch_add(1);
                } else {
                    pres.dma_fd = DmaBufFd(static_cast<int>(eb.fd), static_cast<uint32_t>(pres.length));
                }
                TTBOX_LOG_INFO("buffer[" + std::to_string(i) + "] plane[" + std::to_string(p) +
                               "] mmap=" + std::to_string(reinterpret_cast<uintptr_t>(pres.addr)) +
                               " len=" + std::to_string(pres.length) +
                               " dma_fd=" + std::to_string(pres.dma_fd.fd()));
            }
        }
        opened_ = true;
        TTBOX_LOG_INFO("V4L2Capture open 完成: " + std::to_string(buffer_count_) + " buffers");
        return true;
    }

    // 所有重试均失败
    if (error) *error = "V4L2 格式始终不稳定（" + std::to_string(kMaxRetries) + " 次重试后放弃）";
    return false;
}

bool V4L2Capture::start(std::string* error) {
    if (!opened_) {
        if (error) *error = "设备未 open";
        return false;
    }
    if (running_.load()) {
        if (error) *error = "capture 已在运行";
        return false;
    }

    // ---- QBUF 全部 buffer ----
    for (uint32_t i = 0; i < buffer_count_; ++i) {
        struct v4l2_buffer buf {};
        struct v4l2_plane planes[8] {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = format_.num_planes;
        buf.m.planes = planes;
        for (uint32_t p = 0; p < format_.num_planes; ++p) {
            planes[p].length = static_cast<uint32_t>(impl_->buffers[i].planes[p].length);
        }
        if (ioctl_call(fd_, VIDIOC_QBUF, &buf) != 0) {
            if (error) *error = "VIDIOC_QBUF[" + std::to_string(i) + "] 失败";
            return false;
        }
    }
    TTBOX_LOG_INFO("QBUF 全部 " + std::to_string(buffer_count_) + " 个 buffer");

    // ---- STREAMON ----
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl_call(fd_, VIDIOC_STREAMON, &type) != 0) {
        if (error) *error = "VIDIOC_STREAMON 失败: " + std::string(std::strerror(errno));
        return false;
    }
    TTBOX_LOG_INFO("STREAMON OK");

    // 重置采集指标（G1 纪律：每次 start 都是新采集会话，FPS=本会话帧数/本会话秒数）。
    // 若不复位，capture_frames 是进程级累计值，restart 后 start_time 归零而帧数不归零，
    // 导致 capture_fps 虚高（曾实测 149 万 fps，实为累计帧数/秒的假象）。
    metrics_.capture_frames = 0;
    metrics_.dqbuf_frames = 0;
    metrics_.qbuf_frames = 0;
    metrics_.dropped_latest_frames = 0;
    metrics_.poll_timeouts = 0;
    metrics_.errors = 0;
    metrics_.capture_fps = 0.0;

    running_.store(true);
    capture_thread_ = std::thread(&V4L2Capture::capture_loop, this);
    // 采集线程绑定大核（CPU4~7）：采集是硬实时链路，避免被调度到小核造成抖动。
    // 失败仅告警（调度策略仍可用），不影响启动。
    {
        std::string aerr;
        if (!CpuAffinity::set_thread_affinity(CpuAffinity::kBigCoreMask, &aerr)) {
            TTBOX_LOG_WARN("capture 线程绑定大核失败: " + aerr);
        } else {
            TTBOX_LOG_INFO("capture 线程已绑定大核 (cpu4-7)");
        }
    }
    TTBOX_LOG_INFO("capture thread 已启动");
    return true;
}

void V4L2Capture::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
    // STREAMOFF（即使失败也继续清理）
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl_call(fd_, VIDIOC_STREAMOFF, &type) != 0) {
        TTBOX_LOG_WARN("STREAMOFF 失败: " + std::string(std::strerror(errno)));
    }
    // 归还所有仍被占用的 buffer（stop 语义：consumer 应已停止引用）
    for (uint32_t i = 0; i < buffer_count_; ++i) {
        if (impl_->captured[i]) {
            struct v4l2_buffer buf {};
            struct v4l2_plane planes[8] {};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            buf.length = format_.num_planes;
            buf.m.planes = planes;
            ioctl_call(fd_, VIDIOC_QBUF, &buf);
            impl_->captured[i] = false;
            metrics_.qbuf_frames.fetch_add(1);
        }
    }
    impl_->pending.clear();
    latest_.clear();
    TTBOX_LOG_INFO("capture 已停止（STREAMOFF + 归还全部 buffer）");
}

void V4L2Capture::close() {
    stop();
    if (!opened_) {
        return;
    }
    // munmap + close dma fds（RAII：BufferRes 析构自动 close dma_fd）
    for (auto& bres : impl_->buffers) {
        for (auto& pres : bres.planes) {
            if (pres.addr != nullptr) {
                ::munmap(pres.addr, pres.length);
                pres.addr = nullptr;
                pres.length = 0;
            }
        }
    }
    impl_->buffers.clear();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    opened_ = false;
    TTBOX_LOG_INFO("V4L2Capture close 完成（munmap + dma fd 已关闭）");
}

// ===========================================================================
// capture loop
// ===========================================================================

void V4L2Capture::capture_loop() {
    using clock = std::chrono::steady_clock;
    const auto start_time = clock::now();

    while (running_.load()) {
        // 1. 归还可归还的旧 buffer
        release_ready_buffers();

        // 2. poll。有待归还 buffer 时用短超时轮询（A-5 并发修复）：
        //    多消费者场景下全部 buffer 可能同时被 current_/worker 持有，
        //    capture 会阻塞在 poll；只有及时回来调用 release_ready_buffers()
        //    归还已过期的 buffer，才能避免 capture 饥饿（否则降到 ~1fps）。
        const int eff_timeout = impl_->pending.empty()
                                    ? params_.poll_timeout_ms
                                    : std::min<int>(params_.poll_timeout_ms, 50);
        struct pollfd pfd { fd_, POLLIN, 0 };
        int pr = ::poll(&pfd, 1, eff_timeout);
        if (pr == 0) {
            metrics_.poll_timeouts.fetch_add(1);
            continue;
        }
        if (pr < 0) {
            if (errno == EINTR) continue;
            metrics_.errors.fetch_add(1);
            TTBOX_LOG_ERROR("poll 失败: " + std::string(std::strerror(errno)));
            break;
        }

        // 3. DQBUF
        struct v4l2_buffer buf {};
        struct v4l2_plane planes[8] {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = format_.num_planes;
        buf.m.planes = planes;
        if (ioctl_call(fd_, VIDIOC_DQBUF, &buf) != 0) {
            if (errno == EAGAIN) continue;
            metrics_.errors.fetch_add(1);
            TTBOX_LOG_ERROR("DQBUF 失败: " + std::string(std::strerror(errno)));
            continue;
        }
        const int index = static_cast<int>(buf.index);
        if (index < 0 || index >= static_cast<int>(buffer_count_)) {
            metrics_.errors.fetch_add(1);
            continue;
        }
        metrics_.dqbuf_frames.fetch_add(1);
        impl_->captured[index] = true;

        // 4. 构造 FrameBuffer（zero-copy：data 为空，metadata + dma_fd）
        auto frame = std::make_shared<FrameBuffer>();
        frame->info.width = format_.width;
        frame->info.height = format_.height;
        frame->info.stride = format_.bytesperline.empty() ? format_.width * 3 : format_.bytesperline[0];
        frame->info.format = map_pixel_format(format_.pixelformat);
        frame->info.sequence = buf.sequence;
        frame->info.timestamp_ms = tv_to_ms(buf.timestamp);
        frame->info.frame_number = static_cast<uint64_t>(buf.sequence);
        frame->info.timestamp_us = static_cast<uint64_t>(frame->info.timestamp_ms * 1000.0);
        frame->info.buffer_index = static_cast<uint32_t>(index);
        frame->info.num_planes = format_.num_planes;
        frame->info.dma_fd = impl_->buffers[index].planes.empty() ? -1
                               : impl_->buffers[index].planes[0].dma_fd.fd();
        frame->info.cpu_va = impl_->buffers[index].planes.empty() ? nullptr
                               : impl_->buffers[index].planes[0].addr;
        if (!format_.sizeimage.empty()) {
            frame->size = format_.sizeimage[0];
        }

        // 5. 发布到 LatestFrame（旧帧被覆盖 → 进入待归还）
        // 记录最新帧时间戳（v4l2 单调时钟，与 steady_clock 同基准）——
        // 供 buffer_age_ms（帧龄）计算：采集健康时 ≈ 0~7ms，停流/积压时持续增大
        metrics_.last_frame_ts_ms.store(static_cast<int64_t>(frame->info.timestamp_ms));
        auto old = latest_.publish(std::move(frame));
        if (old) {
            metrics_.dropped_latest_frames.fetch_add(1);
            impl_->pending.push_back(PendingRelease{
                index,
                plane_lengths_of(old->info.buffer_index),
                std::weak_ptr<FrameBuffer>(old),
            });
        }
        metrics_.capture_frames.fetch_add(1);

        // 6. 尝试立即归还
        release_ready_buffers();

        // 7. FPS（滚动 1s 窗口，简单累计）
        const auto elapsed_s =
            std::chrono::duration<double>(clock::now() - start_time).count();
        if (elapsed_s > 0.0) {
            metrics_.capture_fps.store(static_cast<double>(metrics_.capture_frames.load()) / elapsed_s);
        }
    }
}

std::vector<size_t> V4L2Capture::plane_lengths_of(uint32_t index) {
    // 返回 buffer[index] 各 plane length（供 QBUF 回填）
    std::vector<size_t> lens;
    if (index < impl_->buffers.size()) {
        for (const auto& p : impl_->buffers[index].planes) {
            lens.push_back(p.length);
        }
    }
    return lens;
}

void V4L2Capture::release_ready_buffers() {
    for (auto it = impl_->pending.begin(); it != impl_->pending.end();) {
        if (it->weak.lock() != nullptr) {
            ++it;  // 仍有消费者/最新帧引用 → 不能归还
            continue;
        }
        // 无任何强引用 → 安全归还给驱动
        struct v4l2_buffer buf {};
        struct v4l2_plane planes[8] {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = it->index;
        buf.length = format_.num_planes;
        buf.m.planes = planes;
        for (size_t p = 0; p < it->plane_lengths.size() && p < format_.num_planes; ++p) {
            planes[p].length = static_cast<uint32_t>(it->plane_lengths[p]);
        }
        if (ioctl_call(fd_, VIDIOC_QBUF, &buf) == 0) {
            metrics_.qbuf_frames.fetch_add(1);
        } else {
            metrics_.errors.fetch_add(1);
            TTBOX_LOG_WARN("QBUF[" + std::to_string(it->index) + "] 失败: " +
                           std::string(std::strerror(errno)));
        }
        impl_->captured[it->index] = false;
        it = impl_->pending.erase(it);
    }
}

// ===========================================================================
// 查询
// ===========================================================================

uint32_t V4L2Capture::in_use_count() const {
    uint32_t n = 0;
    for (const auto& c : impl_->captured) {
        if (c) ++n;
    }
    return n;
}

std::vector<int> V4L2Capture::dma_fds() const {
    std::vector<int> fds;
    for (const auto& bres : impl_->buffers) {
        if (!bres.planes.empty()) {
            fds.push_back(bres.planes[0].dma_fd.fd());
        }
    }
    return fds;
}

}  // namespace ttbox::core

#endif  // !_WIN32
