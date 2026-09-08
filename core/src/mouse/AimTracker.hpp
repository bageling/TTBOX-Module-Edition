// AimTracker.hpp — 目标跟踪器（第15阶段深化）
//
// 职责：跟踪锁定目标的位置/速度，提供预测位置供 Controller 使用。
// 记录：target_id、上一帧位置、当前帧位置、时间差、vx/vy、prediction_time、predicted_x/y。
// Reset 触发：目标切换（target_id 变化）、LOST 超过 grace（外部调用 reset()）。
//
// 设计（TTBOX 已实现行为）：
//   - 速度 = 帧差位置/时间差，EMA 低通平滑降检测框抖动，clamp 防伪速度尖峰
//   - 预测 = 当前位置 + 速度 × prediction_time（线性外推）
//   - 不依赖 RKNN/V4L2/Mouse/HID，纯数学，可独立测试
#pragma once

#include <cstdint>

namespace ttbox::core::aim {

struct TrackedTarget {
    bool valid = false;
    float x = 0.0f;          // 当前帧目标中心（crop 系 px）
    float y = 0.0f;
    float prev_x = 0.0f;     // 上一帧位置
    float prev_y = 0.0f;
    float vx = 0.0f;         // 像素/秒
    float vy = 0.0f;
    int target_id = -1;
    uint64_t timestamp_us = 0;   // 当前帧时间戳
    uint64_t dt_us = 0;          // 与上一帧时间差
    float prediction_time = 0.0f;    // 预测时域（秒，由调用方设置）
    float predicted_x = 0.0f;        // 预测位置
    float predicted_y = 0.0f;
};

class AimTracker {
public:
    // 每帧更新目标中心。target_id 变化或时间跳跃过大 → 速度清零（避免伪速度）。
    void update(float cx, float cy, int target_id, uint64_t now_us);
    // 预测位置：pos + vel × prediction_time（s），并存入 state
    void predict(float prediction_time_s, float* px, float* py);
    // 目标丢失/切换后重置
    void reset();
    const TrackedTarget& state() const { return state_; }
    // 目标切换判定：上次 update 的 target_id 与当前传入不同
    bool target_switched(int new_id) const {
        return state_.valid && state_.target_id != -1 && state_.target_id != new_id;
    }

private:
    static constexpr uint64_t kMaxDtUs = 200000;   // 200ms 视为目标切换/跳跃
    static constexpr float kVelEmaAlpha = 0.4f;    // 速度 EMA 系数（降抖动）
    static constexpr float kMaxVelPxPerSec = 2500.0f;  // 速度 clamp
    TrackedTarget state_;
};

}  // namespace ttbox::core::aim
