// PersonalMotion.cpp — TTBOX 个人移动曲线确定性插值。
#include "mouse/PersonalMotion.hpp"

#include <algorithm>
#include <cmath>

namespace ttbox::core::aim {

bool PersonalMotion::valid(const PersonalMotionConfig& config) {
    if (!config.enabled || config.knots.empty() || config.curve_blend < 0.0f || config.curve_blend > 1.0f) {
        return false;
    }
    for (const float knot : config.knots) {
        if (!std::isfinite(knot) || knot < 0.0f || knot > 1.0f) return false;
    }
    return true;
}

float PersonalMotion::scale(float error_distance, const PersonalMotionConfig& config) const {
    if (!valid(config)) return 1.0f;
    const float normalized = std::clamp(std::fabs(error_distance) / 128.0f, 0.0f, 1.0f);
    const float position = normalized * static_cast<float>(config.knots.size() - 1);
    const size_t left = static_cast<size_t>(position);
    const size_t right = std::min(left + 1, config.knots.size() - 1);
    const float fraction = position - static_cast<float>(left);
    const float personal = config.knots[left] + (config.knots[right] - config.knots[left]) * fraction;
    return 1.0f + (personal - 1.0f) * config.curve_blend;
}

}  // namespace ttbox::core::aim
