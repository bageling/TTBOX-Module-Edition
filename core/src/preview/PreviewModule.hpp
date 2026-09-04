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
#include "common/Types.hpp"

#include <functional>

namespace ttbox::core {

class PreviewModule {
public:
    struct Params {
        uint32_t out_width = 640;    // 预览输出宽（yu UI 预览框尺寸量级）
        uint32_t out_height = 360;   // 预览输出高
        int fps = 60;                // 预览帧率（1~60；RGA 1~3ms + JPEG ~10ms ≈ 12.7ms < 16.6ms 预算）
        int jpeg_quality = 70;       // JPEG 质量
        RuntimeConfig* runtime_config = nullptr;  // 截取区域（capture ROI）热更新源：预览跟随 AI 视野
        bool draw_detections = false; // 在预览帧上绘制检测框（身体框+头部小框，OpenCV）
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

    // 检测框提供者：返回最新检测框（原图坐标系），预览绘制时映射到预览坐标系。
    using DetectionsProvider = std::function<std::vector<DetectionBox>()>;
    void set_detections_provider(DetectionsProvider provider) {
        std::lock_guard<std::mutex> lk(provider_mutex_);
        detections_provider_ = std::move(provider);
    }

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

    // 检测框绘制：把原图系检测框画到预览 BGR 缓冲（CPU 直拷路径）
    void draw_detections_cpu(uint8_t* roi, uint32_t roi_w, uint32_t roi_h,
                             uint32_t roi_stride, const std::vector<DetectionBox>& boxes) const;
    // 公共：坐标裁剪 + OpenCV 绘制（原图系 → 预览系统一入口）
    void draw_boxes(uint8_t* buf, uint32_t w, uint32_t h, uint32_t stride,
                    const std::vector<DetectionBox>& boxes,
                    float src_w, float src_h, float ox, float oy) const;
    // 检测框平滑：EMA 融合连续帧，消除抖动闪烁；丢帧超阈值则清空
    void smooth_boxes(const std::vector<DetectionBox>& raw, std::vector<DetectionBox>* out);

    const LatestFrame* latest_ = nullptr;
    Params params_{};
    std::atomic<bool> running_{false};
    std::thread thread_;

    // 检测框提供者（AimThread 每帧更新；预览线程取最新快照）
    mutable std::mutex provider_mutex_;
    DetectionsProvider detections_provider_;

    // 检测框平滑状态（预览线程独享，无锁）
    std::vector<DetectionBox> smooth_prev_;
    uint64_t smooth_lost_count_ = 0;
    uint64_t smooth_hold_count_ = 0;  // 单目标短暂丢失保持帧数（防头部小框闪烁）

    // RGA 硬件缩放（替代 CPU 双线性：消除 Preview 对 AI 的 CPU 抢占）
    std::unique_ptr<RgaProcessor> rga_;
    uint32_t applied_roi_x_ = 0, applied_roi_y_ = 0, applied_roi_w_ = 0, applied_roi_h_ = 0;  // 已应用 ROI（变化才 set_roi）

    // 最新 JPEG 缓存（单缓冲，覆盖语义）
    mutable std::mutex jpeg_mutex_;
    std::vector<uint8_t> jpeg_;

    Metrics metrics_;
};

}  // namespace ttbox::core
