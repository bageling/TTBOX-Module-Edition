// test_fov_angle.cpp — FOV 像素误差换算测试。
#include <cassert>
#include <cmath>
#include <iostream>
#include "mouse/FovAngle.hpp"
int main() {
    using namespace ttbox::core::aim;
    [[maybe_unused]] const float center = fov_move_x(0.0f, 1920.0f, 90.0f, 500.0f);
    [[maybe_unused]] const float right = fov_move_x(100.0f, 1920.0f, 90.0f, 500.0f);
    [[maybe_unused]] const float left = fov_move_x(-100.0f, 1920.0f, 90.0f, 500.0f);
    assert(center == 0.0f); assert(right > 0.0f); assert(left < 0.0f);
    assert(std::fabs(right) == std::fabs(left));
    std::cout << "test_fov_angle: PASS\n";
}
