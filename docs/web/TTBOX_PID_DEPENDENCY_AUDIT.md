# TTBOX 功能-PID 参数体系排查报告

> 排查时间：2026-09-08
> 排查方式：全量代码追踪（core/src 生产链路 + RuntimeProfile 序列化 + Web 翻译层）
> 结论：**所有移动输出功能全部基于 PID 输出链实现**，没有绕过 PID 的独立输出路径；但存在一批"死参数/半死参数"和两个废弃控制器残留，已全部定位。

## 一、核心结论（先说人话）

TTBOX 的鼠标移动只有**一条路**：

```
目标检测 → 目标选择 → 像素误差 → Pid1Controller(PID) → 灵敏度缩放
→ 个人曲线 → 拉枪曲线(注入) → 压枪(注入) → 死区 → 余数 → HID整数
→ 拟人化整形 → 热键安全门 → 输出
```

**压枪、拉枪、拟人化、标定、FOV 模式、目标跟踪**——这些功能全部挂在 PID 输出链上：
- 要么**消费 PID 输出**（拟人化整形的是 PID 输出的整数移动量）
- 要么**在 PID 输出之后注入**（压枪/拉枪加在 PID 缩放后的输出上）
- 要么**给 PID 供料**（目标跟踪器算出预测误差喂给 PID）
- 最终**全部统一经过热键安全门**，热键不按 = 输出归零

没有任何功能绕过 PID 直接写鼠标。

---

## 二、功能逐项排查矩阵

| 功能 | 是否基于 PID | 参数来源 | 输出位置 | 状态 |
|---|---|---|---|---|
| 自瞄（核心） | ✅ 是（Pid1Controller） | kp/kd/predict/rate/smooth | PID 输出 → 全链 | 生产主线 |
| 目标跟踪预测（AimTracker） | ✅ 给 PID 供料 | 硬编码内部参数 | 预测误差 → PID 输入 | 生产在用 |
| 拉枪曲线（PullCurve） | ✅ 注入 PID 输出链 | strength/jitter/min_distance | scaled 之后、死区之前 | 生产在用 |
| 压枪（RecoilController） | ✅ 注入 PID 输出链 | strength/speed/gain_y 标定 | pull_curve 后、死区前 | 生产在用 |
| 拟人化整形（PersonalTrajectoryShader） | ✅ 整形 PID 输出 | Fitts 参数 + gain 标定 | move 量化后、Gate 前 | 生产在用（默认关） |
| 锁定确认（AimStateMachine） | ⭕ 不产生移动 | confirmation_frames 等 | 只控制 reset | 生产在用（默认关） |
| 目标选择（TargetSelector） | ⭕ 不产生移动 | confidence/fov_range | 只选目标 | 生产在用 |
| FOV 角度模式（fov_mode） | ⚠️ 走 PID 但单位可疑 | hfov/vfov/move_speed | 替换控制误差后进 PID | 默认关，有隐患（见 §五.2） |
| 自动标定（calibration） | ⚠️ 复用 PID 输出链测响应 | gain_x/y（标定产物） | 偏置注入控制误差域 | 已修复，只写 gain 不写 kp |
| 个人移动曲线（PersonalMotion） | ✅ 输出倍率 | curve_blend/knots | scaled 缩放 | 生产在用（默认关） |

---

## 三、PID 输出链全貌（代码位置）

生产链路唯一入口：`core/src/aim/AimThread.cpp`（415 行，单线程循环）。

```
AimThread::loop() 每帧执行（L29-414）：
├─ L52-80  读 RuntimeProfile 快照 → 配置 PID 与输出链（每帧热更新，免重启）
│           pid_x_.configure(kp_x, kd_x, predict_x, rate_x, smooth_x)  ← 每帧调用
├─ L81     目标选择（TargetSelector）
├─ L85-102 热键 Gate 判定（any/all + calibrating 强制放行）
├─ L108    锁定确认状态机（目标切换/丢失 → 重置 PID）
├─ L120-125 热键未按 → PID reset + 余数清零（防旧状态绕过 Gate）
├─ L126-183 误差计算：瞄准点 → 参考点 → ex/ey
│           L139-146 目标跟踪器预测（可选）
│           L171-174 标定偏置注入控制误差
│           L175-181 FOV 角度模式（可选，替换控制误差）
├─ L191-192 PID 核心：aibox_x = pid_x_.update(control_x)  ← 唯一控制器
├─ L195-197 输出缩放：scaled = aibox × (sensitivity × output_scale)
├─ L200-202 个人曲线倍率（PersonalMotion）
├─ L206-207 拉枪曲线注入（scaled_y += pull_curve.apply(...)）
├─ L212-217 压枪注入（scaled_y/x += recoil_.update(...)）
├─ L219-231 死区 → 余数累计 → int16 clamp
├─ L240-255 拟人化整形（PersonalTrajectoryShader，默认关）
├─ L259-262 热键 Gate 兜底（最终强制归零）
└─ L263     输出 → OutputBackend
```

PID 内核：`core/src/aim/Pid1Controller.hpp`（204 行，与外部参考 pid1.cpp 逐行一致）
- update() 公式：K_p = kp×error；K_i = predict 通道（I 项）；K_d = kd×(error差分)
- smoothTerm soft-limit（bandwidth=10000）防跳变
- 自适应 kp_gain/integral_gain（阈值 1920/50）

---

## 四、参数全集盘点：定义 vs 实际消费

### 4.1 生产链路真正消费的参数（AimThread.cpp 实测）

```
profile->mouse 消费集（30 个）：
kp_x/kp_y, kd_x/kd_y, predict_x/predict_y, rate_x/rate_y, smooth_x/smooth_y,  ← PID 核心 10 个
sensitivity, output_scale, output_deadzone,                                  ← 输出链 3 个
confidence, lost_grace_ms, aim_point(offset/head_aim/class_offsets),         ← 目标选择/瞄准点
fov_mode, hfov, vfov, move_speed_x/y,                                        ← FOV 模式 5 个
calibrating, calibration_bias_x/y, gain_y_px_per_count,                      ← 标定/压枪换算 4 个
enabled, aim_hotkey, aim_hotkey2, aim_hotkey_mode,                           ← 热键 Gate 4 个
pull_curve, personal_motion, personal_trajectory, lock_confirm, recoil       ← 插件配置 5 个
```

### 4.2 定义了但没有任何代码消费的死参数（15 个）

| 参数 | 定义位置 | 结论 |
|---|---|---|
| ki_x / ki_y | MouseTypes.hpp L217-218 | 死（pid1 用 predict 通道做 I 项，不用 ki） |
| deadzone_x / deadzone_y | MouseTypes.hpp L242-243 | 死（生产用 output_deadzone） |
| smooth | MouseTypes.hpp L244 | 死（生产用 smooth_x/smooth_y） |
| aim_part | MouseTypes.hpp L227 | 死（瞄准点由 aim_point.offset 决定） |
| proxy_mode | MouseTypes.hpp L207 | 死（V1 只有 full_passthrough） |
| prediction_s | MouseTypes.hpp L213 | 死（预测由 set_prediction_time 外部设置） |
| selector_search_radius | MouseTypes.hpp L251 | 死（仅序列化） |
| aim_fire_lock_y | MouseTypes.hpp L252 | 死（开火锁 Y 未接入） |
| y_axis_fire_hotkey | MouseTypes.hpp L253 | 死（压枪有自己的 hotkey 配置） |
| y_axis_fire_release_delay_sec | MouseTypes.hpp L254 | 死（压枪用 target_lost_release_ms） |
| response_delay_ms | MouseTypes.hpp L236 | 死（Smith 预测器未接入） |
| smith_dead_ms | MouseTypes.hpp L237 | 死（同上） |
| alpha / beta / gamma / predict_dt_ms | MouseTypes.hpp L238-241 | 死（AimTracker 用内部硬编码参数） |
| block_physical_x / block_physical_y | MouseTypes.hpp L269-270 | 死（仅序列化，无消费） |
| continuous_lead（配置结构） | MouseTypes.hpp L108-115 | 死（持续提前量未接入，头文件无引用） |
| humanize（配置结构） | MouseTypes.hpp L118-123 | 死（未接入，压枪/拟人化各自实现） |
| gain_x_px_per_count | MouseTypes.hpp L234 | 半死（标定写入+序列化，但消费侧只用 gain_y） |

---

## 五、关键发现与隐患

### 5.1 废弃控制器残留（不影响生产，但容易误导）

1. **MotionController**（`mouse/MotionController.hpp`）：旧式 kp×err 简单 PID。
   仍被 AimThread.hpp L112 作为成员 `controller_` 持有，
   但生产代码**只调用 reset()，从不调用 update()**（AimThread.cpp L108/L149 实测）。
   真正的控制全走 pid_x_/pid_y_（Pid1Controller）。→ 可安全删除成员。

2. **controller/PidController + IController**（`core/src/controller/`）：
   Pid1Controller 的"正式化封装"（IController 接口），
   **仅测试使用**（test_pipeline/test_tracker/test_win_e2e/test_real_model/pipeline_bench 5 处），
   生产链路不引用。参数默认值还是旧值（kp=17/10 等，与 pid1 标准 25 不一致）→ 测试残留。

3. **Smooth.hpp / OutputScale.hpp / Deadzone.hpp / RateLimit.hpp / AlphaBetaGammaFilter.hpp**：
   全仓零生产引用（AlphaBetaGammaFilter 只被自己引用）→ 纯死代码。

### 5.2 FOV 角度模式单位隐患（默认关，不影响主线）

`fov_mode=true` 时（AimThread.cpp L175-181）：
```
control_x = fov_move_x(ex, ...)  ← 已换算成"角度→鼠标移动量"
aibox_x  = pid_x_.update(control_x)  ← 又乘 kp=25 + smoothTerm
```
fov_move 返回的是 count 域移动量，进 PID 后再被 kp 放大 25 倍。
该模式本意是"替代 kp×err"，当前实现存在双重缩放嫌疑。
**默认 false 不受影响**；启用前需实测确认或修正注入点（应注入 scaled 层而非控制误差层）。

### 5.3 标定与 PID 的关系（上一阶段已修复，结论保留）

- 标定**只写 gain_x/y_px_per_count**（物理量 px/count），**不再写 kp**。
- pid1 自适应 kp_gain 已处理灵敏度差异，标定不需要动 PID 参数。
- 压枪（recoil_px_per_count）、拟人化（response_px_per_count）都消费 gain_y 标定结果。

### 5.4 Web 暴露面 vs 消费面（用户可调参数）

Web 翻译层（`plugins/web/bin/ttbox-web.py` CONTROLLER_NUMS L389-399）暴露：
```
kp_x/kp_y, kd_x/kd_y, predict_x/predict_y, rate_x/rate_y, smooth_x/smooth_y,
output_deadzone, lost_grace_ms, aim_offset_x/aim_offset_y,
sens→sensitivity（L547），+ pull_curve/humanize/personal_trajectory/
lock_confirm/head_aim 的 enabled 开关
```
**全部是生产链路真实消费的参数**，无假控件。
注意：`output_scale` 在 web 层未直接暴露（只有 C++ 默认 1.0），
`ki_x/ki_y/deadzone_x/y/smooth/aim_part` 等死参数已从 Web 隐藏（此前删除）。

---

## 六、每个功能的 PID 血缘（回答"是不是以 pid 参数为基础"）

### 6.1 直接消费 PID 参数的（调 kp/kd/predict/rate/smooth 立刻生效）

| 功能 | 消费哪些 PID 参数 | 说明 |
|---|---|---|
| 自瞄（核心） | kp/kd/predict/rate/smooth（X/Y 各 5 个） | pid_x_/pid_y_ 每帧 configure，热更新免重启 |
| 目标跟踪预测 | 不直接消费（内部硬编码），**但输出喂给 PID** | 预测误差 → control_x/y → PID，算 PID 上游 |
| 个人曲线倍率 | 不消费 PID 参数，**作用在 PID 输出上** | scaled = aibox × personal_gain |

### 6.2 不消费 PID 参数、但注入 PID 输出链的（调 PID 会影响它们经过的后续处理）

| 功能 | 注入点 | 说明 |
|---|---|---|
| 拉枪曲线 | scaled 后、死区前 | 附加量也走死区/余数/Gate，受 output_deadzone 约束 |
| 压枪 | pull_curve 后、死区前 | 附加量走同一输出链；换算用 gain_y（标定产物，非 PID 参数） |
| 拟人化整形 | move 量化后、Gate 前 | 整形对象就是 PID 最终输出（int16 count） |

### 6.3 完全不产生移动的（PID 无关，但决定 PID 何时工作）

| 功能 | 职责 |
|---|---|
| 目标选择（TargetSelector） | 选目标，喂误差；不产生移动 |
| 锁定确认（AimStateMachine） | 状态机；确认后放行 PID（否则 reset） |
| 热键 Gate | 安全门；关时 PID 输出强制归零 |

### 6.4 结论一句话

> **TTBOX 所有"会动鼠标"的功能 = PID 输出链上的节点（上游供料 / 核心 / 下游整形或注入），**
> **没有任何功能自带独立移动输出路径；**
> **热键安全门是唯一出口，关闭时任何功能都无法输出。**

---

## 七、遗留问题清单（本阶段只排查不修改）

| # | 问题 | 影响 | 建议 |
|---|---|---|---|
| 1 | MotionController 废弃成员（只 reset 不 update） | 误导，无功能影响 | 删除成员 + include |
| 2 | controller/ 目录 PidController 仅测试用，默认值旧 | 测试与生产参数不一致风险 | 测试改用 pid1 标准值或标注 TEST-ONLY |
| 3 | Smooth/OutputScale/Deadzone/RateLimit/AlphaBetaGammaFilter 纯死代码 | 代码噪声 | 确认零引用后删除（需先查 CMake） |
| 4 | fov_mode 双重缩放嫌疑 | 启用该模式时输出异常 | 启用前实测或改注入点 |
| 5 | gain_x_px_per_count 半死（只写不读） | X 轴标定结果压枪用不上 | 确认 YU 语义后决定消费侧 |
| 6 | 死参数 15 个仍序列化落盘 | default.json 冗余 | 待用户确认后清理（涉及 Web 翻译层同步） |







