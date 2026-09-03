// PreviewModule.hpp — 低帧实时预览（10fps，独立于 AI 推理节奏）
//
// 架构：
//   LatestFrame::get()（shared_ptr 快照，零拷贝引用）
//     → mmap 帧 DMA-BUF（只读）
//     → CPU 双线性缩放到 640×360（BGR3）
//     → libjpeg 编码
//     → 缓存最新 JPEG（覆盖旧帧，无排队）
//
// 纪律：
//   - 不阻塞 Capture：快照 shared_ptr 保活，用完即释放，V4L2 buffer 及时归还
//   - 不等待 RKNN：独立线程，与推理互不影响
//   - 无 JPEG backlog：单缓冲，新帧覆盖旧帧
//   - Web 断开不影响本模块（缓存持续刷新，HTTP 只读缓存）
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture/V4L2Capture.hpp"
#include "model/RuntimeProfile.hpp"
#include "rga/RgaProcessor.hpp"

namespace ttbox::core {

class PreviewModule {
public:
    struct Params {
        uint32_t out_width = 640;    // 预览输出宽（yu UI 预览框尺寸量级）
        uint32_t out_height = 360;   // 预览输出高
        int fps = 60;                // 预览帧率（1~60；RGA 1~3ms + JPEG ~10ms ≈ 12.7ms < 16.6ms 预算）
        int jpeg_quality = 70;       // JPEG 质量
        RuntimeConfig* runtime_config = nullptr;  // 截取区域（capture ROI）热更新源：预览跟随 AI 视野
    };

    PreviewModule() = default;
    ~PreviewModule() { stop(); }
    PreviewModule(const PreviewModule&) = delete;
    PreviewModule& operator=(const PreviewModule&) = delete;

    // frame_source：Capture 的 LatestFrame（只读引用）
    bool start(const LatestFrame* frame_source, const Params& params, std::string* error = nullptr);
    void stop();
    bool running() const { return running_.load(); }

    // 取最新 JPEG 快照（无帧时返回 false；Web 请求只读缓存，不触发采集）
    bool snapshot(std::vector<uint8_t>* jpeg_out) const;

    // 指标
    struct Metrics {
        std::atomic<uint64_t> frames{0};        // 已编码帧数
        std::atomic<uint64_t> dropped{0};       // 编码中跳过的帧（上一帧未编完）
        std::atomic<double> fps{0.0};           // 实际预览 FPS
        std::atomic<double> encode_ms{0.0};     // 编码耗时（avg，滚动）
        std::atomic<uint32_t> width{0};
        std::atomic<uint32_t> height{0};
        std::atomic<uint32_t> bytes{0};         // 最新 JPEG 大小
    };
    const Metrics& metrics() const { return metrics_; }

private:
    void loop();
    bool encode_frame(const FrameBuffer& frame, std::vector<uint8_t>* jpeg_out, std::string* error);

    const LatestFrame* latest_ = nullptr;
    Params params_{};
    std::atomic<bool> running_{false};
    std::thread thread_;

    // RGA 硬件缩放（替代 CPU 双线性：消除 Preview 对 AI 的 CPU 抢占）
    std::unique_ptr<RgaProcessor> rga_;
    uint32_t applied_roi_x_ = 0, applied_roi_y_ = 0, applied_roi_w_ = 0, applied_roi_h_ = 0;  // 已应用 ROI（变化才 set_roi）

    // 最新 JPEG 缓存（单缓冲，覆盖语义）
    mutable std::mutex jpeg_mutex_;
    std::vector<uint8_t> jpeg_;

    Metrics metrics_;
};

}  // namespace ttbox::core
