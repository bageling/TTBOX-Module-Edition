// PidController.hpp — 第13阶段：PID 控制器实现（封装 Pid1Controller + 输出链）
//
// 实现 IController：TargetPoint → MouseCommand。
// 行为与 AimThread 现状完全一致（X/Y 独立 Pid1Controller + 输出链），只是把
// "坐标转换 → 控制" 的边界正式化，不改变任何已验证的控制算法。
#pragma once

#include <cmath>
#include <cstdint>

#include "aim/Pid1Controller.hpp"
#include "common/CoreContracts.hpp"
#include "controller/IController.hpp"
#include "pipeline/Target.hpp"

namespace ttbox::core::aim {

// PID 控制器参数（每帧热更新，不重置内部状态）
struct PidControllerParams {
    float kp_x = 17.0f;
    float kp_y = 10.0f;
    float kd_x = 0.0f;
    float kd_y = 0.0f;
    float predict_x = 0.008f;
    float predict_y = 0.008f;
    float rate_x = 1.0f;
    float rate_y = 1.0f;
    float smooth_x = 9900.0f;
    float smooth_y = 9900.0f;
    float sensitivity = 1.0f;     // 全局灵敏度
    float output_scale = 1.0f;    // 输出缩放
    float output_deadzone = 1.0f; // 输出死区（低于此值的输出归零）
    float reference_x = 0.0f;     // 参考点 x（帧中心+aim_offset，由 Coordinate 提供）
    float reference_y = 0.0f;
};

class PidController : public IController {
public:
    // 与 pid1.cpp 原始 init 参数一致：X predict=3.0，Y predict=0.0
    PidController();

    // 运行中热更新参数（不重置内部状态）
    void configure(const PidControllerParams& params);

    // 更新参考点（帧中心+aim_offset，由上游 Coordinate 算出；不重置内部状态）
    void set_reference(float rx, float ry) {
        params_.reference_x = rx;
        params_.reference_y = ry;
    }

    // IController：目标点 → 鼠标命令（只计算，不写任何设备）
    MouseCommand update(const TargetPoint& point) override;

    // 重置内部状态（目标切换/丢失/退出时）
    void reset() override;

    // 遥测：最近一次内部输出（诊断用）
    float last_error_x() const { return last_error_x_; }
    float last_error_y() const { return last_error_y_; }
    float last_pid_x() const { return last_pid_x_; }
    float last_pid_y() const { return last_pid_y_; }

private:
    Pid1Controller pid_x_;
    Pid1Controller pid_y_;
    PidControllerParams params_;
    float remainder_x_ = 0.0f;
    float remainder_y_ = 0.0f;
    float last_error_x_ = 0.0f;
    float last_error_y_ = 0.0f;
    float last_pid_x_ = 0.0f;
    float last_pid_y_ = 0.0f;
    bool configured_ = false;
};

}  // namespace ttbox::core::aim
