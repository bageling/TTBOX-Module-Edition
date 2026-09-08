"""TTBOX 自动标定行为模型。

只负责状态、观测和稳健拟合；设备读写与 HTTP 编排由上层负责。
"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from statistics import median
from typing import Iterable


class CalibrationAxis(str, Enum):
    X = "x"
    Y = "y"


class CalibrationState(str, Enum):
    IDLE = "idle"
    PREPARING = "preparing"
    STABILIZE_X = "stabilize_x"
    SAMPLING_X = "sampling_x"
    ANALYZING_X = "analyzing_x"
    STABILIZE_Y = "stabilize_y"
    SAMPLING_Y = "sampling_y"
    ANALYZING_Y = "analyzing_y"
    VALIDATING = "validating"
    APPLYING = "applying"
    COMPLETED = "completed"
    CANCELLED = "cancelled"
    FAILED = "failed"


@dataclass
class CalibrationObservation:
    axis: CalibrationAxis
    injected_count: float
    measured_delta_px: float
    response_delay_ms: float
    target_id: str
    valid: bool = True


@dataclass
class AxisFit:
    axis: CalibrationAxis
    gain_px_per_count: float = 0.0
    response_delay_ms: float = 0.0
    sample_count: int = 0
    rejected_count: int = 0
    consistency: float = 0.0
    converged: bool = False
    failure_reason: str = ""


@dataclass
class CalibrationSession:
    state: CalibrationState = CalibrationState.IDLE
    failure_reason: str = ""
    current_axis: CalibrationAxis | None = None
    observations: list[CalibrationObservation] = field(default_factory=list)

    def start(self) -> None:
        if self.state is not CalibrationState.IDLE:
            raise ValueError("标定会话已启动")
        self.state = CalibrationState.PREPARING

    def begin_axis(self, axis: CalibrationAxis) -> None:
        if axis is CalibrationAxis.X:
            allowed = {CalibrationState.PREPARING, CalibrationState.ANALYZING_X}
            next_state = CalibrationState.STABILIZE_X
        else:
            allowed = {CalibrationState.ANALYZING_X, CalibrationState.STABILIZE_Y}
            next_state = CalibrationState.STABILIZE_Y
        if self.state not in allowed:
            raise ValueError(f"当前状态不能开始{axis.value}轴")
        self.current_axis = axis
        self.state = next_state

    def begin_sampling(self) -> None:
        if self.state is CalibrationState.STABILIZE_X:
            self.state = CalibrationState.SAMPLING_X
        elif self.state is CalibrationState.STABILIZE_Y:
            self.state = CalibrationState.SAMPLING_Y
        else:
            raise ValueError("当前状态不能进入采样")

    def begin_analysis(self) -> None:
        if self.state is CalibrationState.SAMPLING_X:
            self.state = CalibrationState.ANALYZING_X
        elif self.state is CalibrationState.SAMPLING_Y:
            self.state = CalibrationState.ANALYZING_Y
        else:
            raise ValueError("当前状态不能进入分析")

    def complete_axis(self) -> None:
        if self.state is CalibrationState.ANALYZING_X:
            self.current_axis = CalibrationAxis.Y
            self.state = CalibrationState.STABILIZE_Y
        elif self.state is CalibrationState.ANALYZING_Y:
            self.state = CalibrationState.VALIDATING
        else:
            raise ValueError("当前状态不能完成轴标定")

    def cancel(self) -> None:
        self.state = CalibrationState.CANCELLED

    def fail(self, reason: str) -> None:
        self.failure_reason = str(reason)
        self.state = CalibrationState.FAILED


def fit_axis_measurements(
    axis: CalibrationAxis,
    observations: Iterable[CalibrationObservation],
    *,
    max_relative_mad: float = 0.35,
    min_samples: int = 5,
) -> AxisFit:
    """按轴估计 px/count 和延迟，使用中位数/MAD 排除离群动作。

    目标身份必须一致；每个观测的增益为 measured_delta/injected_count。
    """
    items = [o for o in observations if o.axis is axis and o.valid]
    result = AxisFit(axis=axis)
    result.sample_count = len(items)
    if len(items) < min_samples:
        result.failure_reason = f"{axis.value}轴有效样本不足"
        return result
    target_ids = {o.target_id for o in items if o.target_id}
    if len(target_ids) > 1:
        result.failure_reason = "目标身份在标定过程中发生变化"
        return result
    ratios = []
    for item in items:
        if item.injected_count <= 0:
            continue
        ratios.append(item.measured_delta_px / item.injected_count)
    if len(ratios) < min_samples:
        result.failure_reason = f"{axis.value}轴有效输入不足"
        return result
    center = median(ratios)
    deviations = [abs(value - center) for value in ratios]
    mad = median(deviations)
    threshold = max(abs(center) * max_relative_mad, 1e-6)
    kept = [value for value in ratios if abs(value - center) <= threshold]
    result.rejected_count = len(ratios) - len(kept)
    if len(kept) < min_samples - 1:
        result.failure_reason = f"{axis.value}轴测量一致性不足"
        return result
    result.gain_px_per_count = median(kept)
    result.consistency = max(0.0, 1.0 - mad / max(abs(center), 1e-6))
    if mad / max(abs(center), 1e-6) > max_relative_mad:
        result.failure_reason = f"{axis.value}轴测量一致性不足"
        return result
    result.response_delay_ms = median([o.response_delay_ms for o in items])
    if not 0.03 <= result.gain_px_per_count <= 8.0:
        result.failure_reason = f"{axis.value}轴增益超出范围"
        return result
    if not 0.0 <= result.response_delay_ms <= 50.0:
        result.failure_reason = f"{axis.value}轴响应延迟超出范围"
        return result
    result.converged = True
    return result


def derive_pid_params(
    gain_x_px_per_count: float,
    gain_y_px_per_count: float,
    response_delay_ms: float,
    *,
    smooth: float = 9900.0,
    bandwidth: float = 10000.0,
) -> dict:
    """由标定实测物理量自动推导 PID 参数（自动调参核心）。

    依据 pid1 控制器数学（见 core/src/aim/Pid1Controller.hpp）：
      - smooth=9900 时 Kp/Kd 通道被 soft-limit 压到 (bandwidth-smooth)/bandwidth，
        即约 1%；Ki 通道固定压到 (bandwidth-1000)/bandwidth ≈ 90%。
      - 单帧准星移动 ≈ 输出 × gain px。
      - 不过冲约束：单帧移动 < 当前误差 → kp_eff × gain < 1。
      - 响应目标：每帧吃掉约 20% 误差（时间常数 ≈5 帧 @133fps ≈ 37ms）。
      - 阻尼：系统延迟越大，所需 KD 越大（抑制相位滞后振荡）。
      - 积分：延迟越大，predict（Ki 通道增益）必须越小（上轮仿真证明
        predict 过大 + 延迟 → 剧烈振荡）。

    返回 kp/kd/predict 名义值（写入 RuntimeProfile 的 mouse 段）。
    """
    if gain_x_px_per_count <= 0 or gain_y_px_per_count <= 0:
        raise ValueError("增益必须 > 0")
    gain = min(gain_x_px_per_count, gain_y_px_per_count)
    delay = max(0.0, float(response_delay_ms))
    kp_scale = max((bandwidth - smooth) / bandwidth, 0.002)
    # KP：以"单帧移动 ≤ 15% 误差"为目标（响应快且不过冲；比 20% 更保守，
    # 实测高 gain+高延迟下 20% 会进入 Ki 正反馈极限环）
    kp_eff = 0.15 / gain
    kp = max(4.0, min(60.0, kp_eff / kp_scale))
    # 极端场景（超高增益 + 高延迟）：命令在延迟窗口内过冲是极限环主因，
    # 仿真扫描证明 KP 下限 4 可稳定（g1.5+d60 需要 kp≈4~7）
    if gain >= 1.2 and delay >= 50.0:
        kp = max(4.0, kp * 0.4)
    # KD：阻尼随延迟线性增强（30ms→≈0.95×kp，60ms→≈1.2×kp）。
    # 注意：KD 增强实验证明过阻尼不会消除高增益极限环，保持正常比例即可。
    kd = max(4.0, min(50.0, kp * (0.7 + delay / 120.0)))
    # predict（Ki 通道）：延迟越大越保守；上限 0.35（噪声下 Ki 正反馈
    # 是振荡主因，见 core/tools/pid_sim 仿真结论），60ms 延迟降为 0.15
    predict = max(0.1, min(0.35, 0.35 - delay / 300.0))
    return {
        'kp': round(kp, 2),
        'kd': round(kd, 2),
        'predict': round(predict, 3),
    }
