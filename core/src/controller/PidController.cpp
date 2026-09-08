// PidController.cpp — PID 控制器实现
// 行为与 AimThread 已验证链路完全一致（X/Y Pid1Controller + 输出链），
// 仅把"目标点 → 鼠标命令"的边界正式化，不改变控制算法。
#include "controller/PidController.hpp"

#include <algorithm>

namespace ttbox::core::aim {

// 与 pid1.cpp main() 原始参数一致：X predict=3.0，Y predict=0.0。
// 注：start() 后由 AimThread 用 RuntimeProfile 的 mouse.* 参数覆盖。
PidController::PidController() {
    pid_x_.init(25.0, 25.0, 3.0, 0.3, 9900.0);
    pid_y_.init(25.0, 25.0, 0.0, 0.3, 9900.0);
}

void PidController::configure(const PidControllerParams& params) {
    params_ = params;
    pid_x_.configure(params.kp_x, params.kd_x, params.predict_x,
                     params.rate_x, params.smooth_x);
    pid_y_.configure(params.kp_y, params.kd_y, params.predict_y,
                     params.rate_y, params.smooth_y);
    configured_ = true;
}

void PidController::reset() {
    pid_x_.reset();
    pid_y_.reset();
    remainder_x_ = 0.0f;
    remainder_y_ = 0.0f;
    last_error_x_ = 0.0f;
    last_error_y_ = 0.0f;
    last_pid_x_ = 0.0f;
    last_pid_y_ = 0.0f;
}

// 目标点 → 鼠标命令（只计算，绝不写设备）。
// 误差 = 目标点 - 参考点（参考点由上游 Coordinate 算出后存入 params_）。
// 输出链与 AimThread 已验证链路完全一致：P_PID × sens × scale → 死区 → 余数累积 → int16 clamp。
MouseCommand PidController::update(const TargetPoint& point) {
    MouseCommand cmd;
    cmd.valid = false;
    cmd.dx = 0;
    cmd.dy = 0;
    // 无有效目标点：清余数防旧状态泄漏，输出安全命令（不写设备）。
    if (!point.valid) {
        remainder_x_ = 0.0f;
        remainder_y_ = 0.0f;
        last_error_x_ = 0.0f;
        last_error_y_ = 0.0f;
        return cmd;
    }
    // NaN/Inf 拒绝：控制器输入必须有限，否则输出安全命令并重置。
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        reset();
        return cmd;
    }
    // 误差（帧坐标，像素）
    const float ex = point.x - params_.reference_x;
    const float ey = point.y - params_.reference_y;
    if (!std::isfinite(ex) || !std::isfinite(ey)) {
        reset();
        return cmd;
    }
    last_error_x_ = ex;
    last_error_y_ = ey;
    // Pid1Controller（与 AimThread 相同：X predict=3.0 / Y predict=0.0）
    const float pid_x = static_cast<float>(pid_x_.update(ex));
    const float pid_y = static_cast<float>(pid_y_.update(ey));
    last_pid_x_ = pid_x;
    last_pid_y_ = pid_y;
    // 输出链：P_PID × sens(全局灵敏度) × output_scale
    const float out_gain = params_.sensitivity * params_.output_scale;
    float scaled_x = pid_x * out_gain;
    float scaled_y = pid_y * out_gain;
    // 输出死区：低于死区的输出归零（防微抖）
    if (std::abs(scaled_x) < params_.output_deadzone) scaled_x = 0.0f;
    if (std::abs(scaled_y) < params_.output_deadzone) scaled_y = 0.0f;
    // 小数余量累积，避免小幅连续误差被整数截断
    remainder_x_ += scaled_x;
    remainder_y_ += scaled_y;
    constexpr float kHidMax = 32767.0f;
    constexpr float kHidMin = -32768.0f;
    const float cx_f = std::clamp(remainder_x_, kHidMin, kHidMax);
    const float cy_f = std::clamp(remainder_y_, kHidMin, kHidMax);
    const int16_t move_x = static_cast<int16_t>(cx_f);
    const int16_t move_y = static_cast<int16_t>(cy_f);
    remainder_x_ -= static_cast<float>(move_x);
    remainder_y_ -= static_cast<float>(move_y);
    // 兜底：余数非有限则立即清零防持续乱飞
    if (!std::isfinite(remainder_x_) || !std::isfinite(remainder_y_)) {
        remainder_x_ = 0.0f;
        remainder_y_ = 0.0f;
        pid_x_.reset();
        pid_y_.reset();
    }
    cmd.dx = move_x;
    cmd.dy = move_y;
    cmd.valid = true;
    return cmd;
}

}  // namespace ttbox::core::aim
