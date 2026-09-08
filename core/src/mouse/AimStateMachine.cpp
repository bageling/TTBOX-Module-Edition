// AimStateMachine.cpp — A10 瞄准状态机实现
#include "mouse/AimStateMachine.hpp"

namespace ttbox::core::aim {

const char* AimStateMachine::state_name() const {
    switch (state_) {
        case AimState::kSelecting: return "SELECTING";
        case AimState::kAiming: return "AIMING";
        case AimState::kLostGrace: return "LOST_GRACE";
        default: return "IDLE";
    }
}

bool AimStateMachine::update(const AimStateEvent& e, float lost_grace_ms) {
    const bool was_aiming = state_ == AimState::kAiming;
    bool need_reset = false;

    if (state_ == AimState::kIdle || state_ == AimState::kSelecting) {
        if (e.has_target) {
            // IDLE/SELECTING 且热键有效 → AIMING；无热键则停 SELECTING
            if (e.hotkey_active) {
                state_ = AimState::kAiming;
                need_reset = true;  // 进入瞄准：清理历史状态
            } else {
                state_ = AimState::kSelecting;
            }
        } else {
            state_ = AimState::kIdle;
        }
    } else if (state_ == AimState::kAiming) {
        if (e.has_target && e.hotkey_active) {
            // 持续瞄准
        } else if (!e.has_target) {
            state_ = AimState::kLostGrace;
            lost_since_ms_ = e.now_ms;
        } else if (!e.hotkey_active) {
            state_ = AimState::kIdle;
            need_reset = true;
        }
    } else {  // LOST_GRACE
        if (e.has_target && e.hotkey_active) {
            state_ = AimState::kAiming;  // 找回（需热键仍有效）
        } else if (e.has_target) {
            // 目标找回但热键已松开：直接回 IDLE 并 reset。
            // 否则会闪一帧 AIMING 并输出一帧 AI（注入已由 C 桥热键门控挡住，
            // 但状态显示与 AI 帧输出都不应发生）。
            state_ = AimState::kIdle;
            need_reset = true;
        } else if (e.now_ms - lost_since_ms_ >= static_cast<uint64_t>(lost_grace_ms)) {
            state_ = AimState::kIdle;
            need_reset = true;
        }
    }

    // 离开 AIMING（非过渡到 LOST_GRACE/仍瞄准）也要求 reset
    if (was_aiming && state_ != AimState::kAiming && state_ != AimState::kLostGrace) {
        need_reset = true;
    }
    return need_reset;
}

}  // namespace ttbox::core::aim
