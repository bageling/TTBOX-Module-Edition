# 第15阶段：AimTracker + PID 控制系统深化

> 日期：2026-09-03
> 目标：深化 Tracker + PID 控制系统，全部用真实 HDMI+NPU 数据做数学分析，不凭感觉调参。
> 安全：output_enabled=false、injection_allowed=false 全程保持。

## 一、行为分类（明确四类）

### 已确认行为
- 目标选择链路：FOV→class→conf→距离→priority→锁定→切换（第13阶段对齐）
- 目标锁定延续：目标短暂消失不立即切换，宽限内同 id 延续（第14阶段实测）

### TTBOX 已实现行为（本阶段）
- AimTracker：target_id / 上帧位置 / 当前帧位置 / 时间差 / vx/vy / prediction_time / predicted_x/y
- 目标切换 → tracker Reset（速度清零）；LOST 超过 grace → 外部 reset()
- Controller 使用预测后 TargetPoint 做控制计算（`prediction_time_s` 配置，默认 0=关闭）
- Trace 全链路：原始误差 / 预测误差 / controller_raw / deadzone / rate limit / remainder / 最终 MouseCommand

### 自研行为（本阶段新增）
- AimTracker 速度估计：帧差位置/时间差 + EMA 低通（alpha=0.4）+ clamp（2500px/s）
- 线性外推预测：`predicted = pos + vel × prediction_time`
- 预测命中精度测试方法（t 帧预测点 vs t+pred 帧实际位置）

### 尚未确认行为
- prediction_time 精确取值（本阶段用 0.05s 实验，数据证明当前运动模型下无益）
- 速度估计的具体滤波参数（alpha/clamp）

## 二、Tracker 修改内容

`core/src/mouse/AimTracker.hpp/.cpp`：

| 字段 | 含义 |
|---|---|
| target_id | 锁定目标 id（变化触发 Reset） |
| prev_x/prev_y | 上一帧位置 |
| x/y | 当前帧位置 |
| dt_us | 与上一帧时间差 |
| vx/vy | 速度（px/s，EMA 平滑 + clamp） |
| prediction_time | 预测时域（秒） |
| predicted_x/y | 预测位置 |

Reset 触发：`target_switched(new_id)` 检测 target_id 变化；LOST 超过 grace 后外部调用 `reset()`。

## 三、真实 HDMI+NPU 数据分析（预测开/关对比）

同动画场景（street 底图 + bus 130px 往返），板端实测：

| 指标 | 预测开启(50ms) | 预测关闭 | 结论 |
|---|---|---|---|
| raw_err_x max | 213.5 px | 214.0 px | 相同（原始误差与预测无关） |
| pred_err_x max | **298.2 px** | 214.0 px | **预测反而增大误差** |
| ctrl_raw max | 20.6 | 16.3 | 预测增大输出幅度 |
| 输出抖动 avg | 1.23 | **0.60** | 预测使输出更抖 |
| 饱和 | 0 帧 | 0 帧 | 均无饱和 |
| 过冲 | 0 次 | 0 次 | 均无过冲 |

**数学分析**：正弦加速运动下，50ms 线性外推因速度估计滞后产生超调——
`predicted = pos + v_ema × T`，v_ema 落后实际速度，加速段预测不足、减速段预测过度。

**结论（数据驱动）**：当前运动模型（直线往返）下预测开启无益，
**保持 prediction_time_s=0（关闭）为默认**，不调 PID 参数。
匀速场景（如 test_tracker 场景2）预测命中精度 0.25px 验证了线性外推本身正确，
若未来目标是匀速运动（如直线移动的敌人），可经数据验证后按需开启。

## 四、运动场景自动测试（test_tracker，9 场景）

自动构造仿真轨迹（4ms/帧，250Hz），验证 Tracker+PID 稳定性：

| # | 场景 | 验证点 | 结果 |
|---|---|---|---|
| 1 | 静止 | 预测误差≈0，输出收敛 0 | PASS |
| 2 | 匀速(100px/s) | 预测命中精度 0.25px（线性外推精确） | PASS |
| 3 | 加速(50px/s²) | 输出有限，预测误差有界(<100px) | PASS |
| 4 | 减速 | 输出有限，无发散 | PASS |
| 5 | 急停 | 输出回落，无异常尖峰 | PASS |
| 6 | 方向反转 | 速度符号翻转，穿越参考点后输出转负 | PASS |
| 7 | 目标切换 | tracker Reset，无旧速度残留 | PASS |
| 8 | 短暂丢失 | 状态保持，恢复后速度有效 | PASS |
| 9 | 完全丢失 | reset 后状态清空，重新建立 | PASS |

## 五、稳定性分析（六项）

| 指标 | 结果 |
|---|---|
| 响应延迟 | 0 帧（error 过零当帧跟随，第13阶段 + 本阶段对照） |
| 稳态误差 | 静止目标 0.000 px；Y 轴恒定 45.5px（瞄准点偏上设计） |
| 预测误差 | 匀速 0.25px（精确）；正弦加速最大 298px（预测开启时） |
| 过冲 | 0 次（两组数据） |
| 抖动 | 预测关闭 0.60 / 预测开启 1.23（输出抖动） |
| 饱和 | 0 帧（max 20.6，离 127 限幅很远） |
| 跟随误差 | 与目标运动幅度一致（raw_err max ≈213px = 动画振幅） |
| PID 稳定性 | R²=0.9993 线性（第13阶段）；本阶段无发散 |
| Tracker 稳定性 | 9 场景全过，切换/丢失 Reset 正确 |

## 六、PID 是否调整及理由

**不调整**。理由（数据驱动）：
1. 两组真实数据均无过冲/无饱和/零延迟（第13、15阶段一致）
2. 预测开关对比证明：预测开启对当前运动模型无益（max 误差 +40%）
3. 等效增益 0.087 落在推荐区间（0.08~0.12）
4. 9 个运动场景单测全过，无需为"有修改"而修改

## 七、回归测试

- Windows CTest：**17/17 PASS**（新增 test_tracker）
- test_tracker：9/9 PASS（本机 + 板端）
- 板端编译：ttbox_core_main 编译通过，真实检测正常
- 既有测试全保持（第13/14阶段无回归）
