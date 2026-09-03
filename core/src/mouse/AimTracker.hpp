// AimTracker.hpp — A10 目标跟踪器
//
// 速度估计：帧差 Δx/Δt + EMA 低通平滑（降检测框抖动噪声）+ 限幅。
// 维护 previous_position/timestamp/velocity/target_id。
// 目标切换、目标丢失、瞄准退出时 Reset（清除速度，避免跳变）。
// 预测：predicted = pos + vel × prediction_s。
#pragma once

#include <cstdint>

namespace ttbox::core::aim {

struct TrackedTarget {
    bool valid = false;
    float x = 0.0f;        // 目标中心（crop 系 px）
    float y = 0.0f;
    float vx = 0.0f;       // 像素/秒
    float vy = 0.0f;
    int target_id = -1;
    uint64_t timestamp_us = 0;
};

class AimTracker {
public:
    // 每帧更新目标中心。target_id 变化或时间跳跃过大 → 速度清零（避免伪速度）。
    void update(float cx, float cy, int target_id, uint64_t now_us);
    // 预测位置：pos + vel × prediction_s（s）
    void predict(float prediction_s, float* px, float* py) const;
    void reset();
    const TrackedTarget& state() const { return state_; }

private:
    static constexpr uint64_t kMaxDtUs = 200000;   // 200ms 视为目标切换/跳跃
    static constexpr float kVelEmaAlpha = 0.4f;    // 速度 EMA 系数（降抖动）
    static constexpr float kMaxVelPxPerSec = 2500.0f;  // 速度 clamp（≥8px/帧@125Hz 视为异常）
    TrackedTarget prev_;
    TrackedTarget state_;
};

}  // namespace ttbox::core::aim
