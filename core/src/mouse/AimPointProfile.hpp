// AimPointProfile.hpp — A10 瞄准点计算
//
// 目标基础点 = 框中心 + offset × 框尺寸（crop 坐标系）。
// class_offsets 按 class_id + priority 覆盖默认 offset（不同模型允许不同 profile）。
// 禁止把 aim point 写死在模型 Adapter。
#pragma once

#include <cstdint>
#include <vector>

#include "common/Types.hpp"
#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

// 计算瞄准点（crop 坐标系）。
//   box：目标框（crop 系）；class_id：目标类别
//   prof：瞄准点配置（默认 offset + class_offsets）
//   输出 tx/ty = 目标框内瞄准点（像素，crop 系）。
// 返回 false 仅当 box 无效。
bool aim_point_at(const DetectionBox& box, int class_id, const AimPointProfile& prof,
                  float* tx, float* ty);

// 获取 class_id 命中的类偏移（按 priority 最高）；无命中返回默认。
void class_offset_for(const AimPointProfile& prof, int class_id,
                      float* offset_x, float* offset_y);

}  // namespace ttbox::core::aim
