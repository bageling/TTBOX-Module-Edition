// AimTracker.cpp — A10 目标跟踪器实现
#include "mouse/AimTracker.hpp"

namespace ttbox::core::aim {

void AimTracker::update(float cx, float cy, int target_id, uint64_t now_us) {
    if (!state_.valid || target_id != state_.target_id) {
        // 首次 / 目标切换：直接建立状态，速度清零
        state_.valid = true;
        state_.x = cx;
        state_.y = cy;
        state_.vx = 0.0f;
        state_.vy = 0.0f;
        state_.target_id = target_id;
        state_.timestamp_us = now_us;
        prev_ = state_;
        return;
    }
    const uint64_t dt_us = now_us > state_.timestamp_us ? now_us - state_.timestamp_us : 0;
    prev_ = state_;
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

void AimTracker::predict(float prediction_s, float* px, float* py) const {
    *px = state_.x + state_.vx * prediction_s;
    *py = state_.y + state_.vy * prediction_s;
}

void AimTracker::reset() {
    prev_ = TrackedTarget{};
    state_ = TrackedTarget{};
}

}  // namespace ttbox::core::aim
