// Smooth.hpp — A10 平滑层（独立于 raw PID output）
//
// raw_output → smoothing → final_output。
// 使用简单一阶低通（TTBox 自实现）：out = alpha × raw + (1-alpha) × prev。
// alpha = 0 关闭（默认，与 A9 无平滑行为一致）。
// 参数进入 RuntimeProfile（mouse.smooth）。
// 说明：YU 的 smooth=9900 数学公式未确认，此处为 TTBox 自己的实现，不与 YU 声称一致。
#pragma once

namespace ttbox::core::aim {

class SmoothFilter {
public:
    // alpha：0~1（0=不平滑）。X/Y 独立状态。
    float apply_x(float raw, float alpha) {
        if (alpha <= 0.0f) { prev_x_ = raw; return raw; }
        if (alpha >= 1.0f) { prev_x_ = raw; return raw; }
        prev_x_ = alpha * raw + (1.0f - alpha) * prev_x_;
        return prev_x_;
    }
    float apply_y(float raw, float alpha) {
        if (alpha <= 0.0f) { prev_y_ = raw; return raw; }
        if (alpha >= 1.0f) { prev_y_ = raw; return raw; }
        prev_y_ = alpha * raw + (1.0f - alpha) * prev_y_;
        return prev_y_;
    }
    void reset() { prev_x_ = 0.0f; prev_y_ = 0.0f; }

private:
    float prev_x_ = 0.0f;
    float prev_y_ = 0.0f;
};

}  // namespace ttbox::core::aim
