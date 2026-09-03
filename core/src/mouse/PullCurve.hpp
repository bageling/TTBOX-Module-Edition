// PullCurve.hpp — YU 拉枪曲线插件（自瞄时附加弧线 + 抖动）
//
// 对齐 YU trace 行为：目标误差距离 ≥ min_distance 时激活，
// 在拉枪方向（X 主导）附加垂直弧线，并在 Y 上叠加抖动。
// 由 AimThread 在 deadzone 之前调用（对齐 YU 输出链顺序）。
#pragma once

#include <cmath>

#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

class PullCurve {
public:
    // err_x/err_y：当前像素误差；out_x/out_y：当前缩放后输出（count）。
    // 返回附加的 Y 弧线量（count）；直接修改 out_y 亦可由调用方处理。
    float apply(float err_x, float err_y, float out_x, [[maybe_unused]] float out_y,
                const PullCurveConfig& cfg, float dt_ms) {
        if (!cfg.enabled) return 0.0f;
        const float dist = std::hypot(err_x, err_y);
        if (dist < cfg.min_distance) return 0.0f;
        // 弧线：沿拉枪方向附加垂直分量（对齐 C 桥 arc = strength × |dx| × 0.08）
        float arc = cfg.strength * std::fabs(out_x) * 0.08f;
        if (arc > 24.0f) arc = 24.0f;
        // 方向：X 拉枪方向（正/负）决定弧线方向（C 桥语义：dx>=0 加正 Y）
        const float dir = (out_x >= 0.0f) ? 1.0f : -1.0f;
        float v = arc * dir;
        // 抖动（正弦伪随机，基于时间）
        if (cfg.jitter_px > 0.0f) {
            t_ms_ += dt_ms;
            const float phase = t_ms_ * 0.001f * 6.2832f * 2.0f;  // ~2Hz 抖动
            v += std::sin(phase) * cfg.jitter_px * 0.5f;
        }
        return v;
    }
    void reset() { t_ms_ = 0.0f; }

private:
    float t_ms_ = 0.0f;
};

}  // namespace ttbox::core::aim
