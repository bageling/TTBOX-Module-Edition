// PersonalMotion.hpp — TTBOX 个人移动曲线运行时采样器。
// 只消费已校验的归一化 knots；无效/关闭模型返回单位倍率，保持默认控制链。
#pragma once

#include <cstdint>
#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

class PersonalMotion {
public:
    // 根据当前误差距离（像素）返回默认输出倍率与个人曲线的混合倍率。
    // knots 按误差距离归一化到 0~128 像素均匀采样。
    float scale(float error_distance, const PersonalMotionConfig& config) const;

private:
    static bool valid(const PersonalMotionConfig& config);
};

}  // namespace ttbox::core::aim
