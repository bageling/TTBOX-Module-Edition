// AimStateMachine.hpp — A10 瞄准状态机
//
// IDLE → SELECTING（目标检测命中）→ AIMING（selector 选中 + 热键有效）
//      → LOST_GRACE（目标丢失）→ 找回恢复 AIMING / 超时回 IDLE。
// 默认丢失宽限 78ms。
// 退出 AIMING / 进入新目标时产生 reset 需求（Reset MotionController + Tracker）。
#pragma once

#include <cstdint>

#include "mouse/MouseTypes.hpp"

namespace ttbox::core::aim {

struct AimStateEvent {
    bool has_target = false;     // selector 是否有有效目标
    bool hotkey_active = true;   // 瞄准热键是否按下（V1 外部门控；未提供视为 true）
    uint64_t now_ms = 0;
};

class AimStateMachine {
public:
    AimState state() const { return state_; }
    const char* state_name() const;

    // 推进状态机。返回 true 表示本次发生了需要 Reset（controller/tracker）的转换。
    bool update(const AimStateEvent& e, float lost_grace_ms);
    void reset() { state_ = AimState::kIdle; lost_since_ms_ = 0; }

private:
    AimState state_ = AimState::kIdle;
    uint64_t lost_since_ms_ = 0;
    bool was_aiming_ = false;  // 上一帧是否在 AIMING（用于离开时产生 reset）
};

}  // namespace ttbox::core::aim
