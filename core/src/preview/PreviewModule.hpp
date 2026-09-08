// PreviewModule.hpp — Capture 中心可调尺寸预览
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "capture/V4L2Capture.hpp"
#include "common/Types.hpp"
#include "model/RuntimeProfile.hpp"

namespace ttbox::core {

class PreviewModule {
public:
    struct Params {
        uint32_t crop_width = 640;
        uint32_t crop_height = 640;
        int fps = 15;
        int jpeg_quality = 70;
        RuntimeConfig* runtime_config = nullptr;
        bool draw_detections = false;
    };

    PreviewModule() = default;
    ~PreviewModule() { stop(); }
    PreviewModule(const PreviewModule&) = delete;
    PreviewModule& operator=(const PreviewModule&) = delete;

    bool start(const LatestFrame* frame_source, const Params& params, std::string* error = nullptr);
    void stop();
    bool running() const { return running_.load(); }
    bool snapshot(std::vector<uint8_t>* jpeg_out) const;

    using DetectionsProvider = std::function<std::vector<DetectionBox>()>;
    void set_detections_provider(DetectionsProvider provider) {
        std::lock_guard<std::mutex> lock(provider_mutex_);
        detections_provider_ = std::move(provider);
    }

    struct Metrics {
        std::atomic<uint64_t> frames{0};
        std::atomic<uint64_t> dropped{0};
        std::atomic<double> fps{0.0};
        std::atomic<double> encode_ms{0.0};
        std::atomic<uint32_t> width{0};
        std::atomic<uint32_t> height{0};
        std::atomic<uint32_t> bytes{0};
    };
    const Metrics& metrics() const { return metrics_; }

private:
    void loop();
    bool encode_frame(const FrameBuffer& frame, std::vector<uint8_t>* jpeg_out,
                      std::string* error);
    void draw_boxes(uint8_t* crop, uint32_t width, uint32_t height, uint32_t stride,
                    const std::vector<DetectionBox>& boxes, uint32_t origin_x,
                    uint32_t origin_y) const;
    void smooth_boxes(const std::vector<DetectionBox>& raw, std::vector<DetectionBox>* out);
    void resolve_crop_size(uint32_t* crop_width, uint32_t* crop_height) const;

    const LatestFrame* latest_ = nullptr;
    Params params_{};
    std::atomic<bool> running_{false};
    std::thread thread_;

    mutable std::mutex provider_mutex_;
    DetectionsProvider detections_provider_;

    std::vector<DetectionBox> smooth_prev_;
    uint64_t smooth_lost_count_ = 0;

    mutable std::mutex jpeg_mutex_;
    std::vector<uint8_t> jpeg_;

    Metrics metrics_;
};

}  // namespace ttbox::core
