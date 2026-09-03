// Humanize.hpp — YU 拟人化插件（输出附加抖动，减轻机械感）
//
// 对齐 YU humanize 语义：目标输出附加正弦抖动（jitter_px 幅度、
// jitter_frequency 频率），curve_strength 保留用于曲线混合。
// 在 AimThread 输出链 quantize 之前调用。
#pragma once

#include <cmath>

#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

class Humanize {
public:
    // raw_x/raw_y：量化前输出（count）。返回抖动偏移（count，X/Y 独立）。
    void apply(float* x, float* y, float dt_ms, const HumanizeConfig& cfg) {
        if (!cfg.enabled || cfg.jitter_px <= 0.0f) return;
        t_ms_ += dt_ms;
        const float w = cfg.jitter_frequency > 0.0f
                            ? cfg.jitter_frequency * 6.2832f * 0.001f
                            : 8.0f * 6.2832f * 0.001f;
        const float a = cfg.jitter_px * 0.5f;
        *x += std::sin(t_ms_ * w) * a;
        *y += std::sin(t_ms_ * w * 1.31f + 1.7f) * a;  // X/Y 相位错开
    }
    void reset() { t_ms_ = 0.0f; }

private:
    float t_ms_ = 0.0f;
};

}  // namespace ttbox::core::aim
