// AimTargetTask.hpp
#pragma once
#include <cstdint>
#include <vector>
#include "common/Types.hpp"
namespace ttbox::core::aim {
struct AimPoint { float x = 0.0f; float y = 0.0f; };
struct AimTargetTask {
    uint64_t frame_number = 0;
    uint64_t timestamp_us = 0;
    int worker_id = -1;
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    bool has_target = false;
    DetectionBox target{};
    AimPoint aim_point{};
    float target_width = 0.0f;
    float target_height = 0.0f;
    bool crosshair_detected = false;
    AimPoint crosshair{};
    std::vector<DetectionBox> detections;
};
}
