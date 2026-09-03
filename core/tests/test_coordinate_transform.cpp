// test_coordinate_transform.cpp — 坐标域、瞄准点和准星偏移回归
#include <cmath>
#include <iostream>
#include "mouse/CoordinateTransform.hpp"

int main() {
    using namespace ttbox::core;
    using namespace ttbox::core::aim;
    AimPointProfile p;
    p.offset_x = 0.5f;
    p.offset_y = 0.15f;
    p.aim_offset_x = 10.0f;
    p.aim_offset_y = -5.0f;
    float ex = 0.0f, ey = 0.0f;
    DetectionBox box{90, 40, 110, 140, 0.9f, 1};
    if (!CoordinateTransform::pixel_error(box, 1, p, 200, 200, &ex, &ey)) return 1;
    // target=(100,55), reference=(110,95) => (-10,-40)
    if (std::abs(ex + 10.0f) > 1e-5f || std::abs(ey + 40.0f) > 1e-5f) {
        std::cerr << "坐标变换或瞄准点错误: " << ex << "," << ey << "\n"; return 1;
    }
    std::cout << "test_coordinate_transform: PASS\n";
    return 0;
}
