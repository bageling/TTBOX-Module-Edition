// V4L2Capture.hpp — V4L2 → MMAP → DQBUF → VIDIOC_EXPBUF → DMA-BUF fd → LatestFrame
//
// 阶段 A-2 采集模块。边界设计：
//   V4L2Capture → LatestFrame(FrameBuffer{info.dma_fd}) → RgaProcessor（下一阶段）
//
// 关键语义：
//   - latest-frame：新帧覆盖旧帧，无队列；consumer 永远拿最新帧
//   - DMA-BUF fd 对应的 V4L2 buffer 不能在 consumer 仍使用时 QBUF 归还：
//     通过 shared_ptr/weak_ptr 引用计数判定（weak.lock() 为空 = 无消费者引用 → 可归还）
//   - 不允许 V4L2 buffer starvation：多 buffer 池（默认 4）提供余量；
//     全部被占时 poll 超时等待，不忙等、不崩溃
//
// 线程模型：capture thread（内部）+ 任意 consumer。start()/stop() 负责线程生命周期。
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/Types.hpp"
#include "capture/ICapture.hpp"

namespace ttbox::core {

// ---------------------------------------------------------------------------
// LatestFrame：线程安全 latest-frame 容器
// ---------------------------------------------------------------------------

class LatestFrame {
public:
    // 发布新帧：替换当前帧（旧帧被覆盖）。返回被替换的旧帧（可为空）。
    std::shared_ptr<FrameBuffer> publish(std::shared_ptr<FrameBuffer> frame);

    // 获取当前最新帧（shared_ptr 保活：旧帧在 consumer 使用时不会提前归还）。
    std::shared_ptr<FrameBuffer> get() const;

    void clear();

private:
    mutable std::mutex mutex_;
    std::shared_ptr<FrameBuffer> current_;
};

// ---------------------------------------------------------------------------
// 采集指标（线程安全计数）
// ---------------------------------------------------------------------------

struct V4L2Metrics {
    std::atomic<uint64_t> capture_frames{0};        // 成功发布到 latest 的帧数
    std::atomic<uint64_t> dqbuf_frames{0};          // DQBUF 成功次数
    std::atomic<uint64_t> qbuf_frames{0};           // QBUF 归还次数
    std::atomic<uint64_t> dropped_latest_frames{0}; // 被新帧覆盖丢弃的帧数（latest 语义）
    std::atomic<uint64_t> poll_timeouts{0};
    std::atomic<uint64_t> errors{0};
    std::atomic<double> capture_fps{0.0};
    std::atomic<int64_t> last_frame_ts_ms{0};   // 最新帧 v4l2 单调时间戳（ms，供 buffer_age_ms 计算）
};

// ---------------------------------------------------------------------------
// V4L2Capture
// ---------------------------------------------------------------------------

class V4L2Capture : public capture::ICapture {
public:
    struct Params {
        std::string device = "/dev/video0";
        uint32_t num_buffers = 4;   // 请求 buffer 数（以驱动实际为准，实际更少时降级）
        int poll_timeout_ms = 1000; // poll 超时（ms）
        // 可选 V4L2 Selection/Crop；0 表示不请求硬件裁剪。
        uint32_t crop_x = 0;
        uint32_t crop_y = 0;
        uint32_t crop_width = 0;
        uint32_t crop_height = 0;
    };

    // 实际协商格式（open 后有效，不强制改分辨率）
    struct FormatInfo {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t pixelformat = 0;   // fourcc
        uint32_t num_planes = 0;
        std::vector<uint32_t> bytesperline;
        std::vector<uint32_t> sizeimage;
        bool selection_supported = false;
        bool selection_applied = false;
        std::string fourcc_str() const;
    };

    V4L2Capture();
    ~V4L2Capture();
    V4L2Capture(const V4L2Capture&) = delete;
    V4L2Capture& operator=(const V4L2Capture&) = delete;

    // configure：保存参数（不打开设备）
    bool configure(const Params& params, std::string* error = nullptr);

    // open：QUERYCAP → G_FMT → REQBUFS → 每 plane QUERYBUF+mmap+EXPBUF
    bool open(std::string* error = nullptr) override;

    // start：QBUF 全部 buffer → STREAMON → 启动 capture thread
    bool start(std::string* error = nullptr) override;

    // stop：请求退出 → join 线程 → STREAMOFF → 归还所有已 DQBUF 的 buffer
    void stop() override;

    // close：munmap 全部 plane → close 全部 dma-buf fd → close 设备 fd（幂等）
    void close() override;

    bool running() const { return running_.load(); }

    // 消费者入口：最新帧（info.dma_fd 供下一阶段 RgaProcessor 消费）
    std::shared_ptr<FrameBuffer> latest_frame() const override { return latest_.get(); }

    // 共享 LatestFrame 引用（A-5 Worker 池：多 worker 并发取最新帧，无队列）。
    // 生命周期：调用方必须保证本 capture 在 worker 池停止前存活。
    LatestFrame* latest_frame_ref() { return &latest_; }

    const FormatInfo& format() const { return format_; }
    const V4L2Metrics& metrics() const { return metrics_; }
    uint32_t buffer_count() const { return buffer_count_; }
    // 当前被占用（已 DQBUF 未 QBUF 归还）的 buffer 数（排队深度探测）
    uint32_t in_use_count() const;

    // 调试/验收：每个 buffer 主 plane 的 DMA-BUF fd 列表
    std::vector<int> dma_fds() const;

private:
    void capture_loop();
    void release_ready_buffers();
    std::vector<size_t> plane_lengths_of(uint32_t index);

    struct Impl;
    std::unique_ptr<Impl> impl_;

    Params params_;
    FormatInfo format_;
    V4L2Metrics metrics_;
    LatestFrame latest_;
    std::atomic<bool> running_{false};
    std::thread capture_thread_;
    uint32_t buffer_count_ = 0;
    int fd_ = -1;
    bool opened_ = false;
};

}  // namespace ttbox::core
