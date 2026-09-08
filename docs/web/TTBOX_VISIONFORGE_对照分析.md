# VisionForge → TTBOX 多目标追踪 / 锁头 / 压枪 / 拟人化 对照分析

> 定位：跨仓库参考分析，供 TTBOX 后续升级决策使用。
> 参考源：`C:\Users\Administrator\Desktop\VisionForge-main`（Python 高端自瞄引擎 v17.8.77）
> 对照对象：`C:\Users\Administrator\Desktop\TTBOX-Module-Edition\core\src\`（TTBOX 稳定版）
> 结论先行：**VisionForge 的多目标追踪 / 锁定确认 / 头部约束 / 拟人化整形（压枪/抖动），都是 TTBOX 目前缺失或薄弱的成熟实现，算法思路可移植到 TTBOX 的 C++（不是 Python 直接搬）。**

---

## 一、这份源码是什么（先定性）

VisionForge 是一个**成熟的自瞄 / 视觉辅助引擎**（安防之外的射击辅助赛道），不是空壳：
- 多目标追踪（BYTETrack：卡尔曼 + 二阶段 IoU 关联）
- 头部瞄准锁定（head_aim_policy：把瞄准点约束进头框内部）
- 目标锁定画像（把 Mahalanobis/GIoU/卡方等专家参数藏成 5 个用户滑块）
- 拟人化整形（personal_trajectory_shaper：Fitts 定律时长 + 速度包络 + 曲线/抖动/端点翻转，即"压枪/压枪随机化"核心）
- 还带账号/授权壳 + FastAPI 服务端（这部分无用，TTBOX 不参考）

语言是 **Python**，算法思路可直接对照移植到 TTBOX 的 C++，但代码不能直接用。

---

## 二、逐模块对照

### 模块 A：多目标追踪（BYTETrack）

| 维度 | VisionForge | TTBOX 现状 |
|---|---|---|
| 定位 | 检测层跨帧 ID 关联（多目标） | 检测层只有"单目标锁定"（TargetSelector） |
| 实现 | `src/bytetrack_tracker.py`：每条轨迹一个 8 维匀速卡尔曼 `_KalmanFilterByteTrack`；两阶段关联（高置信度→低置信度）；轨迹确认/缓冲/删除 | `core/src/mouse/TargetSelector*.hpp/.cpp`：轨迹表 `tracks_` **只有增删没有单目标匹配**，靠"对角×1.0+8px"附近选最近、位置找回、距离排序 |
| 关联指标 | IoU×1.8 + 归一化距离×0.55 + 尺寸比×0.15 加权（`_associate`，支持 IoU 不够时中心/尺寸连读，处理快速相机运动和姿态变化） | 只有中心距离（`(c.cx-t.cx)²+...`）；无 IoU、无尺寸比、无"低置信度第二关联" |
| 轨迹生命周期 | `track_buffer=30` 帧未匹配即删除，**有明确上限不会无限增长** | `tracks_` **只增不删**（我之前已发现：无 erase/上限，目标频繁进出会缓慢泄漏） |
| 轨迹确认 | `hits >= new_track_thresh` 确认后才算真目标 | 无"确认帧"概念，第一帧就锁定 |

**TTBOX 差距**：
1. 少真正的多目标 ID 关联（BYTETrack 那种给每个检测打 track_id）。
2. 轨迹表**无清理上限**（内存隐患）。
3. 无"确认帧 + 高低置信度两阶段"抗抖。

> 移植建议：TTBOX 若要从"单锁"升"真多目标"，核心是把 `TargetSelector::tracks_` 升级成 BYTETrack 那种卡尔曼+加权 IoU 关联。但 **安防（TargetSelector 锁本人）只需单目标**，多目标 ID 主要是给 Detection 层打号用，是否升级取决于产品要不要"锁定身份不串"。

---

### 模块 B：目标锁定确认（锁定/防跳变）

| 维度 | VisionForge | TTBOX 现状 |
|---|---|---|
| 实现 | `src/control_gate.py`（1009 行） | `core/src/mouse/AimStateMachine.hpp/.cpp` |
| 核心机制 | **ENTER/HOLD 双置信度阈值** + 丢失宽限（`locked_target_grace_ms=45`）。v17 解决"真实小/远头在阈值附近抖动导致控制闪烁" | 单一 `lost_grace_ms=78` 丢失宽限 + IDLE→SELECTING→AIMING→LOST_GRACE 状态机 |
| 丢失保持 | `allow_missing_target_hold_control`：已确认目标丢 1-2 帧仍保持，避免闪烁转成开关（有界紧束缚防鬼移动） | 丢失宽限内保持原目标；宽限耗尽放弃（AimThread via state_machine_） |
| 快速进入 | `instant_enter_enabled`：近距离/高置信目标**跳过进入确认窗直接锁** | 无（第一帧直接锁） |

**TTBOX 差距**：TTBOX 的 `AimStateMachine` 结构已具备（IDLE/AIMING/LOST_GRACE），但**缺 ENTER/HOLD 双阈值**和 **instant-enter**——即"锁定后即使置信度略降也不闪烁放弃"和"近距离目标秒锁"。这是 TTBOX 目标锁定稳定性的最值得借鉴点。

---

### 模块 C：头部瞄准约束（锁头精确版）

| 维度 | VisionForge | TTBOX 现状 |
|---|---|---|
| 实现 | `src/head_aim_policy.py`：`head_aim_anchor_x/y_ratio`（锚点）、`safe_inset_fraction`（内部安全内缩 0.12）、`max_anchor_lag_px/fraction`（锚点滞后钳制） | `core/src/mouse/MouseTypes.hpp` 的 `aim_point.offset_x/y`（AimPointProfile）+ `aim_part`（0=脚 10=头，`offset_y=1.0-ap*0.09`） |
| 机制 | 把瞄准点**约束在头框内部安全区**，并且**限制锚点每秒最大滞后**，防止大偏移时瞄准点飞出头部 | 按框高比例算部位偏移（头=框高 1/4，胸=框高 1/10） |

**TTBOX 差距**：TTBOX 目前是"框高比例算头/胸位置"，而 VisionForge 是"锚点 + 安全内缩 + 滞后钳制"三件套，**大幅抑制瞄准点在目标头部边缘抖动**（对锁头精度提升显著）。值得移植到 `CoordinateTransform`/`AimPointProfile`。

---

### 模块 D：压枪 / 抖动 / 拟人化（这是 TTBOX 最薄弱的，也最值得消化）

| 维度 | VisionForge | TTBOX 现状 |
|---|---|---|
| 实现 | `src/personal_trajectory_shaper.py`（920 行）：完整"个人轨迹整形" | `core/src/mouse/Humanize.hpp`（正弦抖动）+ `core/src/mouse/PersonalMotion.hpp`（倍率曲线）+ `core/src/mouse/PullCurve.hpp`（拉枪弧线） |
| 输入 | 控制器算出的 raw(dx,dy) + 误差/速度/目标年龄 | PID 输出后的 scaled 域 |
| 时长模型 | **Fitts 定律**：目标距离→预测移动时长（intercept_ms + slope×log2(dist)）+ 用户 profile 时长分布 | 无距离→时长模型 |
| 速度包络 | `_speed_envelope_16`：16 点速度曲线（smoothstep / 偏峰铃形）叠加用户校准包络 | 无（输出直接是 PID 恒定增益） |
| 增益整形 | `transport_gain` 产生**中间段加速**（加 0~20% 增益）| 无 |
| 抖动 | 一阶马尔科夫随机游走 `_curve_state`（AR(1) 高斯），**垂直于移动方向**叠加，幅度由 `variation_budget`（按响应灵敏度×目标半径限制视觉抖动）钳制 | 纯正弦 `sin(t*w)`，固定幅度，X/Y 相位错开；无幅度自适应、无垂直约束 |
| 自抑制 | `_adaptive_strength`：capture_priority(初期停用)/大误差/目标快速移动/目标太老/方向突变/接近目标——这些情况**自动降强度或停用**，防止鬼/乱 | 无（Humanize 恒开，无误差/速度/年龄自适应） |
| 方向保护 | `_guard_output`：限制 |extra|≤max_extra_px、保持与误差同号、禁止反向投影 | 无（无额外幅度上限审查） |
| 端点 | `_endpoint_flips`/`direction_change_cosine`：方向突变检测重置 | 无 |

**TTBOX 差距（压枪/拟人化是最大短板）**：
1. TTBOX 的 `Humanize` 是**死参数未接入**（只有类，AimThread 没调用，之前确认过）；VisionForge 是**完整的压枪/抖动引擎**。
2. TTBOX 的 `PersonalMotion` 只是"倍率曲线"，不是"速度包络+时长+自适应抑制"的拟人整形。
3. TTBOX 的 `PullCurve`（拉枪）是单向弧线，VisionForge 的整形是**双向、自适应、有安全钳制的完整人格装置**。

> 移植建议：VisionForge `personal_trajectory_shaper` 的**核心三件套**最值得移植到 TTBOX 输出链 Quantize 之前：
> - Fitts 时长模型（距离→时长）
> - 速度包络（smoothstep 加速/减速）
> - 垂直向 AR(1) 抖动 + 自适应强度抑制（大误差/快速目标/老目标/方向突变自动停用）
> - 安全守卫（不超过 raw+max_extra_px、保持方向、禁止反向）

---

## 三、结论：TTBOX 从这份源码最该学什么（按优先级）

| 优先级 | 移植项 | 对应 TTBOX 文件 | 收益 |
|---|---|---|---|
| 🥇 1 | **拟人化整形引擎**（Fitts 时长+速度包络+垂直抖动+自适应抑制+安全守卫） | 新建 `mouse/PersonalTrajectoryShader`，接入 `AimThread.cpp` 输出链（Quantize 前） | 压枪/抖动/拟人化从"死参数"变"真功能"，直接对标 VisionForge |
| 🥈 2 | **目标锁定确认**（ENTER/HOLD 双阈值 + instant-enter） | 改 `AimThread.cpp` 状态机 / `TargetSelector` | 目标锁定更稳，不闪烁，近距离秒锁 |
| 🥉 3 | **头部锚点约束**（安全内缩+滞后钳制） | 改 `CoordinateTransform` / `AimPointProfile` | 锁头精度大幅提升，抑制头部边缘抖动 |
| 4 | **BYTETrack 真多目标 ID**（卡尔曼+加权 IoU+轨迹上限） | 升级 `TargetSelector` | 多目标不串身份；顺带修掉 tracks_ 只增不删的内存隐患 |

> 注意：第 1 项（拟人化）与"安全门 Gate"不冲突——整形后仍过热键 Gate 归零（TTBOX AimThread 已有此兜底，测试已证）。第 2/3 项改动涉及稳定链路（TargetSelector/AimStateMachine），必须严格走"先调查→小范围→测试"流程，不破坏现有 109 测试。

---

## 四、参考源文件真实性声明

本报告所有 VisionForge 描述均基于以下真实文件逐行阅读，非猜测：
- `src/bytetrack_tracker.py`（卡尔曼+Iou 两阶段关联）
- `src/control_gate.py`（锁定确认/丢失宽限/instant-enter）
- `src/head_aim_policy.py`（头部锚点约束）
- `src/personal_trajectory_shaper.py`（Fitts 时长/速度包络/垂直抖动/自适应抑制/安全守卫）
- `docs/TARGET_LOCK_PROFILE_DESIGN.md`（画像调参设计）
- `legacy_original/tracker.py` + `kalman.py`（单目标 Kalman）

TTBOX 描述基于 `core/src/` 真实文件：
- `mouse/TargetSelector.hpp/.cpp`、`mouse/AimTracker.hpp/.cpp`、`mouse/AimStateMachine.hpp`、
- `mouse/MouseTypes.hpp`（HumanizeConfig/PullCurveConfig/PersonalMotionConfig/aim_point/aim_part）、
- `mouse/Humanize.hpp`、`mouse/PersonalMotion.hpp`、`mouse/PullCurve.hpp`、
- `aim/AimThread.cpp`（输出链 160-215 行：PID→sens→personal_gain→pull_curve→deadzone→int16→Gate）

---

## 五、落地状态更新（2026-09-08，本阶段完成）

| 优先级 | 移植项 | 状态 | 落地文件 | 测试 |
|---|---|---|---|---|
| 🥇 1 | 拟人化整形引擎 | ✅ 已落地 | 新建 `mouse/PersonalTrajectoryShader.hpp`，`AimThread.cpp` 输出链接线（move 计算后、Gate 前），`MouseTypes.hpp` 加 `PersonalTrajectoryConfig`，`RuntimeProfile.cpp` 序列化 20 字段，`plugins/web` 新增控件 | `test_personal_trajectory_shader` 6/6，默认关 |
| 🥈 2 | 目标锁定确认（ENTER/HOLD+instant-enter） | ✅ 已落地 | 重写 `mouse/AimStateMachine.cpp` update（确认帧+双阈值+秒锁），`MouseTypes.hpp` 加 `LockConfirmConfig`，`RuntimeProfile.cpp` 序列化，`plugins/web` 新增控件 | `test_lock_confirm` 10/10，默认=旧行为 |
| 🥉 3 | 头部瞄准约束 | ✅ 已落地 | `MouseTypes.hpp` 加 `HeadAimConfig`，`AimPointProfile.cpp` 实现 `constrain_aim_point_to_head`，`AimThread.cpp` 接线，`RuntimeProfile.cpp` 序列化，`plugins/web` 新增控件 | `test_head_aim_constraint` 11/11，默认关 |
| 4 | BYTETrack 真多目标 ID | ✅ 已落地 | 升级 `TargetSelector.cpp/hpp`：卡尔曼速度学习 + 轨迹生命周期删除（修只增不删）+ max_tracks 上限 + use_kalman_predict 开关（默认关保兼容） | `test_bytetrack_upgrade` 5/5，109/109 全量不变 |

核心原则遵守：**新功能全部默认关 / 保持现有行为**，109 现有用例一个不破坏；全量回归 **109 tests + ctest 25/25 全 PASS**；板端已同步编译部署，`ttbox_core_main`（780024 字节）真实模型推理 PASS。