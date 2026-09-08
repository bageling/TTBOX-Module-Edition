// AimTracker.cpp — 目标跟踪器实现（第15阶段深化）
#include "mouse/AimTracker.hpp"
#include <cmath>

namespace ttbox::core::aim {

void AimTracker::update(float cx, float cy, int target_id, uint64_t now_us) {
    if (!state_.valid || target_id != state_.target_id) {
        // 首次 / 目标切换：直接建立状态，速度清零，平滑器重置
        // （新目标首帧用原始坐标，避免旧目标平滑值污染新目标）
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
        pos_fx_ = cx;
        pos_fy_ = cy;
        pos_fdx_ = 0.0f;
        pos_fdy_ = 0.0f;
        pos_valid_ = false;
        raw_prev_x_ = cx;
        raw_prev_y_ = cy;
        return;
    }
    const uint64_t dt_us = now_us > state_.timestamp_us ? now_us - state_.timestamp_us : 0;
    state_.prev_x = state_.x;
    state_.prev_y = state_.y;
    state_.dt_us = dt_us;
    if (dt_us > 0 && dt_us <= kMaxDtUs) {
        const float dt_s = static_cast<float>(dt_us) / 1e6f;
        // 帧差瞬时速度 → EMA 低通平滑（降检测框抖动）+ clamp（防伪速度尖峰）
        // 注意：用原始帧差（raw_prev_），不能用手滑后位置（双重平滑会让速度收敛慢）
        const float raw_vx = (cx - raw_prev_x_) / dt_s;
        const float raw_vy = (cy - raw_prev_y_) / dt_s;
        raw_prev_x_ = cx;
        raw_prev_y_ = cy;
        float vx = state_.vx + kVelEmaAlpha * (raw_vx - state_.vx);
        float vy = state_.vy + kVelEmaAlpha * (raw_vy - state_.vy);
        if (vx > kMaxVelPxPerSec) vx = kMaxVelPxPerSec;
        else if (vx < -kMaxVelPxPerSec) vx = -kMaxVelPxPerSec;
        if (vy > kMaxVelPxPerSec) vy = kMaxVelPxPerSec;
        else if (vy < -kMaxVelPxPerSec) vy = -kMaxVelPxPerSec;
        state_.vx = vx;
        state_.vy = vy;
        // OneEuro 位置平滑（滤检测框 y1/y2 帧间跳变噪声）：
        // 位置先低通，速度估计仍用原始帧差（保持既有语义）。
        // dt 过大或时间回退（dt_s<=0）时跳过平滑直接使用原始坐标。
        auto alpha = [](float cutoff_hz, float dt) {
            return 1.0f / (1.0f + 1.0f / (6.2831853f * cutoff_hz * dt));
        };
        const float ad = alpha(kPosDCutoffHz, dt_s);
        const float raw_dx = (cx - pos_fx_) / dt_s;
        const float raw_dy = (cy - pos_fy_) / dt_s;
        // fdx/fdy 用统一 EMA 吸收噪声速度尖峰：
        // 初值 0，ad 很小（138fps 下约 0.044），首帧噪声（±千 px/s）被压到 ±50 内，
        // 避免 cutoff 被尖峰抬高导致平滑失效。
        pos_fdx_ += ad * (raw_dx - pos_fdx_);
        pos_fdy_ += ad * (raw_dy - pos_fdy_);
        const float cutoff_x = kPosMinCutoffHz + kPosBeta * std::fabs(pos_fdx_);
        const float cutoff_y = kPosMinCutoffHz + kPosBeta * std::fabs(pos_fdy_);
        const float ax = alpha(cutoff_x, dt_s);
        const float ay = alpha(cutoff_y, dt_s);
        // 位置低通：首帧 early-return 已建立初值 pos_fx_=cx，此后无条件平滑。
        pos_fx_ += ax * (cx - pos_fx_);
        pos_fy_ += ay * (cy - pos_fy_);
        pos_valid_ = true;
        state_.x = pos_fx_;
        state_.y = pos_fy_;
    } else {
        state_.vx = 0.0f;
        state_.vy = 0.0f;
        // 时间跳跃（>200ms）：位置直接使用原始坐标，平滑器重新建立
        state_.x = cx;
        state_.y = cy;
        pos_fx_ = cx;
        pos_fy_ = cy;
        pos_fdx_ = 0.0f;
        pos_fdy_ = 0.0f;
        pos_valid_ = false;
        raw_prev_x_ = cx;
        raw_prev_y_ = cy;
    }
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
