// AimTracker.cpp — 目标跟踪器实现（第15阶段深化）
#include "mouse/AimTracker.hpp"

namespace ttbox::core::aim {

void AimTracker::update(float cx, float cy, int target_id, uint64_t now_us) {
    if (!state_.valid || target_id != state_.target_id) {
        // 首次 / 目标切换：直接建立状态，速度清零
        state_.valid = true;
        state_.x = cx;
        state_.y = cy;
        state_.prev_x = cx;
        state_.prev_y = cy;
        state_.vx = 0.0f;
        state_.vy = 0.0f;
        state_.target_id = target_id;
        state_.timestamp_us = now_us;
        state_.dt_us = 0;
        return;
    }
    const uint64_t dt_us = now_us > state_.timestamp_us ? now_us - state_.timestamp_us : 0;
    state_.prev_x = state_.x;
    state_.prev_y = state_.y;
    state_.dt_us = dt_us;
    if (dt_us > 0 && dt_us <= kMaxDtUs) {
        const float dt_s = static_cast<float>(dt_us) / 1e6f;
        // 帧差瞬时速度 → EMA 低通平滑（降检测框抖动）+ clamp（防伪速度尖峰）
        const float raw_vx = (cx - state_.x) / dt_s;
        const float raw_vy = (cy - state_.y) / dt_s;
        float vx = state_.vx + kVelEmaAlpha * (raw_vx - state_.vx);
        float vy = state_.vy + kVelEmaAlpha * (raw_vy - state_.vy);
        if (vx > kMaxVelPxPerSec) vx = kMaxVelPxPerSec;
        else if (vx < -kMaxVelPxPerSec) vx = -kMaxVelPxPerSec;
        if (vy > kMaxVelPxPerSec) vy = kMaxVelPxPerSec;
        else if (vy < -kMaxVelPxPerSec) vy = -kMaxVelPxPerSec;
        state_.vx = vx;
        state_.vy = vy;
    } else {
        state_.vx = 0.0f;
        state_.vy = 0.0f;
    }
    state_.x = cx;
    state_.y = cy;
    state_.timestamp_us = now_us;
}

void AimTracker::predict(float prediction_time_s, float* px, float* py) {
    state_.prediction_time = prediction_time_s;
    state_.predicted_x = state_.x + state_.vx * prediction_time_s;
    state_.predicted_y = state_.y + state_.vy * prediction_time_s;
    if (px) *px = state_.predicted_x;
    if (py) *py = state_.predicted_y;
}

void AimTracker::reset() {
    state_ = TrackedTarget{};
}

}  // namespace ttbox::core::aim
