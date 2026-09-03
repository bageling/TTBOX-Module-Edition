// MotionMerge.hpp — A10 物理 + AI 合并
//
// 保持物理鼠标透传。结构：Physical dx/dy + AI dx/dy → MotionMerge → HID Scheduler。
// YU 的物理+AI 精确合并数学语义未确认，故不假设简单相加：
// 通过可替换的 MergeMode 策略接口控制。V1 默认：
//   kPhysicalPassthroughAiInjection —— AI 增量注入 + 物理透传（可按 X/Y 屏蔽物理）。
#pragma once

#include <cstdint>

#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

class MotionMerge {
public:
    // 合并策略（可替换接口；V1 仅实现一种）
    enum class MergeMode : int {
        kPhysicalPassthroughAiInjection = 0,
    };

    // 物理 + AI → 最终移动。
    //   phys：物理鼠标报告（透传）；ai：AI 注入增量
    //   block_x/block_y：瞄准时是否屏蔽物理该轴
    //   clamp int16（HID int16 上限保护；int8 拆包由 HID Scheduler/RateLimiter 处理）
    static MergedMove merge(const PhysicalMotion& phys, const AiMove& ai,
                            bool block_x, bool block_y,
                            MergeMode mode = MergeMode::kPhysicalPassthroughAiInjection) {
        MergedMove out;
        out.buttons = phys.buttons;
        out.wheel = phys.wheel;
        switch (mode) {
            case MergeMode::kPhysicalPassthroughAiInjection: {
                const int32_t px = block_x ? 0 : phys.dx;
                const int32_t py = block_y ? 0 : phys.dy;
                out.dx = clamp_i16(px + ai.dx);
                out.dy = clamp_i16(py + ai.dy);
                break;
            }
        }
        return out;
    }

private:
    static int16_t clamp_i16(int32_t v) {
        if (v > 32767) return 32767;
        if (v < -32768) return -32768;
        return static_cast<int16_t>(v);
    }
};

}  // namespace ttbox::core::aim
