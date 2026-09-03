// FovAngle.hpp — A10.1 FOV 角度→像素移动换算（可选输出模式）
//
// 参考 PD Aim 的 fov() 算法：基于视场角 + 分辨率把像素误差换算为"目标偏角"，
// 再按"每弧度移动像素"（move_speed）转成鼠标移动量。比 kp×err 更随 FOV/分辨率自适应。
//   per_pixel_rad_x = move_speed_x / 2π      （X 整圈 2π 对应 move_speed_x 像素）
//   per_pixel_rad_y = move_speed_y / π        （Y 半圈 π，参考 PD Aim）
//   sup_distance    = res/2 / tan(fov/2)      （视距，像素）
//   angle           = atan(|err| / sup_distance)
//   move            = angle × per_pixel_rad（方向 = err 符号）
// 误差单位：像素（crop/ROI 坐标系）；res = 对应轴分辨率（roi_w / roi_h）。
#pragma once

#include <cmath>
#include <cstdint>

namespace ttbox::core::aim {

constexpr float kPiF = 3.14159265358979323846f;

inline float fov_move(float err_px, float res_px, float fov_deg, float move_speed,
                      bool x_axis) {
    const float per_pixel_rad = x_axis ? move_speed / (2.0f * kPiF) : move_speed / kPiF;
    const float half_fov = fov_deg * kPiF / 180.0f * 0.5f;
    // 非法 FOV（≤0 或 ≥90°）或无分辨率：回退线性（不崩溃）
    if (!(half_fov > 0.0f) || !(half_fov < kPiF * 0.5f) || !(res_px > 0.0f)) return err_px;
    const float sup = res_px * 0.5f / std::tan(half_fov);
    const float angle = std::atan(std::fabs(err_px) / sup);
    float mv = angle * per_pixel_rad;
    return err_px < 0.0f ? -mv : mv;
}

inline float fov_move_x(float err_x, float res_x, float hfov_deg, float speed) {
    return fov_move(err_x, res_x, hfov_deg, speed, true);
}

inline float fov_move_y(float err_y, float res_y, float vfov_deg, float speed) {
    return fov_move(err_y, res_y, vfov_deg, speed, false);
}

}  // namespace ttbox::core::aim
