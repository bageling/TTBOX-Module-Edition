// Deadzone.hpp — A10 输出死区（X/Y 独立）
//
// |output| < deadzone → 0。默认 1.0（count 单位，在缩放后应用）。
#pragma once

#include <cmath>

#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

inline float deadzone_x(float v, const MouseProfile& p) {
    return std::fabs(v) < p.deadzone_x ? 0.0f : v;
}

inline float deadzone_y(float v, const MouseProfile& p) {
    return std::fabs(v) < p.deadzone_y ? 0.0f : v;
}

}  // namespace ttbox::core::aim
