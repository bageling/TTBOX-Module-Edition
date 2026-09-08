# PID 真实数据分析报告（第13阶段）

> 日期：2026-09-03
> 数据来源：**真实 HDMI + NPU 检测链路**（PC 全屏画面 → HDMI → RK3588 → yolo261n → DecodeNMS → TargetSelector → Coordinate → Pid1Controller）
> 采集方式：PidTrace 逐帧 CSV（`pid_trace_enabled=true`，板端 `/opt/ttbox/data/pid_trace.csv`）
> 目的：用真实数据分析 PID 行为，**不凭感觉调参**。

## 一、采集了什么

每帧记录 16 个字段：

```
timestamp_us, frame_number, target_id, target_x, target_y,
reference_x, reference_y, error_x, error_y,
controller_raw_x, controller_raw_y, final_command_x, final_command_y,
target_switch, target_lost, confidence
```

## 二、数据场景

| 场景 | 画面 | 帧数 | 说明 |
|---|---|---|---|
| 静态目标 | street 全屏（bus 静止） | 2138 | 稳态行为 |
| 动态目标 | street 左右滑动（bus 移动，振幅100px/周期6s） | 2046 | 收敛/过冲/延迟 |

## 三、静态目标分析（稳态）

| 指标 | X 轴 | Y 轴 |
|---|---|---|
| error 均值 | -43.09 px | +13.34 px |
| error 标准差 | 0.0000 | 0.0000 |
| controller_raw 均值 | -3.850 | +0.580 |
| 帧间抖动 | 0.000 px | 0.000 px |

**结论**：
- 检测框完全稳定（静止画面 → 误差零抖动）
- controller_raw 与 error 比例 ≈ 0.089（X）、0.043（Y）
- final_command 全部 0（**注入关闭，Gate 生效** ✓）

## 四、动态目标分析（收敛/过冲/延迟）

| 指标 | 结果 |
|---|---|
| error_x 范围 | -37.1 ~ +163.3 px（峰峰 200.4px，目标真实移动） |
| error_y 范围 | 45.0 ~ 46.1 px（恒定 ≈45.5px = 瞄准点 y1+15%框高的固定偏移，非异常） |
| 响应延迟 | **0 帧**（22 个过零样本全部当帧跟随） |
| 过冲 | **0 次**（误差过零后输出无反向残留） |
| 输出饱和 | **0 帧**（\|raw\| 最大 13.2，远低于 127/32767） |
| 线性度 | R²=0.9993（输出严格跟随误差） |
| 等效增益 k_fit | 0.0828（理论 kp=25，但 pid1.cpp 内部 kp_gain 爬升机制限制稳态等效≈0.087） |
| 抖动 | X 2.04px 平均（含动画运动分量）；Y 0.09px（静止无抖动） |

## 五、结论：当前参数健康，无需调参

1. **无过冲**：误差过零后 controller_raw 立即同号跟随，无回弹
2. **无饱和**：最大输出 13.2，离限幅很远，余量充足
3. **零延迟**：error 变化当帧反映到输出（响应延迟 0 帧）
4. **高线性**：R²=0.9993，输出与误差严格成比例
5. **等效增益合理**：0.087 落在推荐区间（0.08~0.12 温和自瞄）

**因此第13阶段不修改 PID 参数**——真实数据证明当前行为健康。
若未来需要"锁更紧"（等效增益 0.12+）或"更软"（0.08-），
再依据同样流程采集数据对比，不凭感觉调。
