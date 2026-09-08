// DetectionGeometryFilter.hpp — 人物/头部双框融合与几何过滤
#pragma once
#include <cstdint>
#include <vector>
#include "common/Types.hpp"

namespace ttbox::core {

struct DetectionGeometryFilterConfig {
    bool enabled = false;
    bool allow_body_only = true;
    bool allow_head_only = false;
    bool lower_body_block = true;
    int body_class_id = 0;
    int head_class_id = 1;
    float min_confidence = 0.25f;       // 用户总置信度基线
    float min_head_conf = 0.18f;
    float min_body_conf = 0.26f;
    float paired_head_min_conf = 0.20f;
    float head_only_min_conf = 0.75f;
    float head_only_center_max_px = 175.0f;
    float min_body_width_px = 8.0f;
    float min_body_height_px = 26.0f;
    float min_head_width_px = 1.5f;
    float min_head_height_px = 1.5f;
    float max_head_aspect = 2.55f;
    float max_body_aspect = 1.05f;
    float pair_expand_x = 0.25f;
    float pair_expand_y = 0.12f;
    float border_margin_px = 2.0f;
    bool reject_border = true;
    float border_center_max_px = 105.0f;
};

struct DetectionGeometryFilterStats {
    uint32_t input = 0;
    uint32_t output = 0;
    uint32_t heads = 0;
    uint32_t bodies = 0;
    uint32_t paired = 0;
    uint32_t body_only = 0;
    uint32_t head_only = 0;
    uint32_t rejected = 0;
};

class DetectionGeometryFilter {
public:
    explicit DetectionGeometryFilter(DetectionGeometryFilterConfig config = {}) : config_(config) {}
    void set_config(const DetectionGeometryFilterConfig& config) { config_ = config; }
    const DetectionGeometryFilterConfig& config() const { return config_; }
    const DetectionGeometryFilterStats& stats() const { return stats_; }
    std::vector<DetectionBox> filter(const std::vector<DetectionBox>& boxes, float center_x, float center_y);
private:
    DetectionGeometryFilterConfig config_;
    DetectionGeometryFilterStats stats_;
};

} // namespace ttbox::core
