// IController.hpp — 第13阶段：控制器（Controller）统一接口
//
// 模块边界：
//   Coordinate ──TargetPoint──▶ Controller ──MouseCommand──▶ Mouse（未来，本阶段停止）
//
// 控制器只负责：
//   输入 TargetPoint（目标点）→ 计算误差 → PID/控制算法 → 输出 MouseCommand（鼠标命令）
//
// 控制器绝对不负责：
//   - 写 /dev/hidg*（那是 Mouse 模块的事）
//   - 知道 USB / HID 设备 / 网络
//   - 目标选择 / 坐标转换（那是上游模块的事）
//
// 设计目标：未来可以替换控制算法（PID/比例/滤波/预测），上层（AimThread）无需改动。
// 实现：PidController（封装现有 Pid1Controller + 输出链，见 PidController.hpp）。
#pragma once

#include <cstdint>

#include "common/CoreContracts.hpp"  // MouseCommand
#include "pipeline/Target.hpp"       // TargetPoint

namespace ttbox::core::aim {

// 控制器统一接口：TargetPoint → MouseCommand
class IController {
public:
    virtual ~IController() = default;

    // 输入目标点，计算并返回鼠标移动命令。
    // 语义：valid=false 的目标点 → 返回 valid=false 且 dx=dy=0 的 MouseCommand（安全输出）。
    // 实现必须保证：绝不直接写任何 HID/设备；只计算命令。
    virtual MouseCommand update(const TargetPoint& point) = 0;

    // 重置内部状态（目标切换/目标丢失/瞄准退出时调用，防止旧状态影响新目标）。
    virtual void reset() = 0;
};

}  // namespace ttbox::core::aim
