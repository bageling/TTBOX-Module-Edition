// AimError.hpp — 目标任务到画面中心的误差计算。
#pragma once
#include "pipeline/AimTargetTask.hpp"
namespace ttbox::core::aim {
struct AimError { float x=0.0f; float y=0.0f; };
inline AimError error_from_center(const AimTargetTask& task) {
    const float cx = static_cast<float>(task.frame_width) * 0.5f;
    const float cy = static_cast<float>(task.frame_height) * 0.5f;
    return {task.aim_point.x - cx, task.aim_point.y - cy};
}
}
