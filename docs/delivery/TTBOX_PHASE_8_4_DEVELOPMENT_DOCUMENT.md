# TTBOX Phase 8.4 开发文档

## 自动标定深度逆向与行为级重构

> 日期：2026-09-01  
> 项目：TTBOX 自动标定、个人移动曲线与 03「移动控制」深化  
> 当前 Git 基线：`91bb7a0`  
> 说明：本开发文档记录当前工作树成果；Phase 8.4 当时的最终提交需以仓库最新 Git 记录为准。

---

## 1. 阶段目标

本阶段以真机上的参考实现作为行为参考，逆向确认自动标定的真实工作方式，再用 TTBOX 自己的 Domain、Gateway、RuntimeConfig、Core 和 Web 重建闭环。

严格边界：

- 参考产品只提供行为证据。
- TTBOX 不依赖参考 daemon、API、ABI、配置目录或品牌命名。
- 不把页面字段存在当成 Core 能力。
- 没有真实浏览器和设备副作用证据的功能不标 REAL。

---

## 2. 参考实现逆向结论

### 2.1 真实 owner

参考 Web 层主要负责请求透传。自动标定算法真正位于 AArch64 daemon 的 `AppDaemon::AimLoop()`，而不是 Python Web 路由。

通过真机代码、符号、字符串、常量和运行行为确认了以下对象和字段：

- `AutoCalibrationSession`
- `CalibrationCandidateWindow`
- `CalibrationObservation`
- `MedianObservationCenter`
- `CalibrationEstimatedAxisGain`
- `AutoCalibrationRuntimeState`
- `stabilize_x / stabilize_y`
- `measure_x_response / measure_y_response`
- `measure_x_settle / measure_y_settle`
- `raw_measured_error / measured_error`
- `target_motion_recovered`

### 2.2 参考状态机

```text
IDLE
  → PREPARING
  → STABILIZE_X
  → MEASURE_X_RESPONSE
  → MEASURE_X_SETTLE
  → STABILIZE_Y
  → MEASURE_Y_RESPONSE
  → MEASURE_Y_SETTLE
  → SAVING
  → COMPLETED
```

任意阶段都可能进入：

- CANCELLED
- FAILED

### 2.3 参考采样行为

已确认的行为方向：

- 使用真实检测目标，而不是单纯的 target_found 布尔值。
- 目标候选带 track ID、类别和矩形框。
- 目标需要在窗口内保持身份一致。
- 中心使用窗口统计，存在 `MedianObservationCenter`。
- X/Y 轴分别测量。
- 动作采用往返/回中腿，不是单次正向移动。
- 参考增益范围约为 `0.03~8.0 px/count`。
- 参考响应延迟范围约为 `0~50ms`。
- 参考幅度常量为：`8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128`。

参考二进制为已编译产物，完整拟合函数体并未全部恢复，因此未知部分明确记录为推断，不冒充源码事实。

---

## 3. TTBOX 自有领域模型

新增 `ttbox_motion/calibration.py`，定义 TTBOX 自己的对象：

- `CalibrationAxis`
- `CalibrationState`
- `CalibrationObservation`
- `AxisFit`
- `CalibrationSession`

领域模型负责：

- 会话状态转换
- 目标身份一致性
- 采样记录
- X/Y 分轴拟合
- 中位数和 MAD 稳健统计
- 结果范围校验
- 取消、失败和完成状态

TTBOX 首版产品阈值：

- 连续稳定观测至少 10 个。
- 中心抖动小于 `1px`。
- 框尺寸变化小于 `5%`。
- 稳定持续约 `800ms`。
- X/Y 每轴至少 5 个有效动作观测。
- 增益必须位于 `0.03~8.0 px/count`。
- 响应延迟必须位于 `0~50ms`。
- MAD 相对中位数超过 `35%` 时失败，不强行应用。

这些阈值是 TTBOX 产品决策，不声称是参考产品逐字常量。

---

## 4. TTBOX 当前运行链路

```text
浏览器自动标定页
  → window.ttbox.api
  → POST /api/control/calibration/start
  → scripts/ttbox_web.py
  → _calib_target() 读取 Core GET_STATUS
  → 目标身份/类别/中心/框尺寸稳定判断
  → X 轴采样
  → X 轴拟合
  → Y 轴采样
  → Y 轴拟合
  → validating
  → applying
  → 写 calibration.json 与 RuntimeProfile
  → Core 热更新
  → GET 回读
  → 页面显示完成状态
```

Core 侧新增/接通的真实数据：

- 目标 track ID
- 目标类别 ID
- 目标框宽高
- 目标中心位置
- 标定期间的 `calibration_bias_x/y` 消费
- RuntimeProfile 中的标定和个人曲线配置

标定动作复用正式控制/PID/输出链，不增加一条临时鼠标旁路。

---

## 5. 自动标定状态和 Web 表现

TTBOX 状态包括：

```text
idle
preparing
stabilize_x
sampling_x
analyzing_x
stabilize_y
sampling_y
analyzing_y
validating
applying
completed
cancelled
failed
```

状态 API 返回的信息包括：

- 当前状态和当前轴
- 有效样本数
- 候选目标 ID、类别、宽高
- 稳定帧数
- 中心抖动
- 尺寸变化
- 当前幅度列表
- 已耗时
- X/Y 拟合结果
- 错误原因

前端轮询只更新状态，不触发普通保存 Toast。

手动保存才显示“手动标定参数已保存”。自动标定启动显示“标定运行中”，不再显示普通“已保存”。

---

## 6. 个人移动曲线训练

Phase 8.4 同步把原先的训练页面从假接口改成 TTBOX 自有能力。

### 6.1 Domain

新增：

- `MotionProfileStore`
- 训练会话和租约
- 样本严格校验
- 训练统计
- 模型生成
- profile 激活/停用
- 原子文件写入

持久化目录：

```text
/opt/ttbox/config/motion-profiles/<profile_id>/profile.json
```

### 6.2 Core

新增 `PersonalMotion` 模型消费：

- 读取 RuntimeProfile 中的 knots。
- 按误差距离进行插值。
- 与默认输出进行混合。
- 不绕过目标选择、PID、死区和热键安全门。
- 模型无效时 fail-closed，继续默认链路。

### 6.3 浏览器闭环证据

在板端隔离 Gateway 上完成过真实浏览器链路：

```text
打开页面
  → 创建 profile
  → 创建训练 session
  → 上传 12 个样本
  → 训练质量 100
  → 激活
  → 刷新后显示 12 / 168 和已启用
  → 停用
```

该证据证明页面、Gateway、Domain、持久化和激活链路真实工作；真实目标场景下的最终 HID 效果仍保持 VERIFY。

---

## 7. “已保存”提示根因

实际复现结果：

- 普通控件一次修改只发送一次 `PUT /api/config`。
- 没有发现 input/change 双请求。
- 自动标定轮询不直接调用“已保存” Toast。
- 真正的问题是状态提示语义混用，而不是简单的重复保存事件。

处理结果：

- 普通配置继续使用同步状态徽标。
- 自动标定使用“标定运行中”等阶段状态。
- 自动标定内部应用候选参数不弹普通保存成功提示。
- 明确的手动标定保存才显示保存提示。

---

## 8. 修改文件范围

### Core

- `core/src/aim/AimThread.cpp/.hpp`
- `core/src/common/Metrics.hpp`
- `core/src/runtime/CoreRuntime.cpp`
- `core/src/ipc/IpcServer.cpp`
- `core/src/model/RuntimeProfile.cpp`
- `core/src/mouse/MouseTypes.hpp`
- `core/src/mouse/PersonalMotion.cpp/.hpp`
- `core/CMakeLists.txt`
- Core 相关测试文件

### Gateway / Domain

- `scripts/ttbox_web.py`
- `ttbox_motion/calibration.py`
- `ttbox_motion/training.py`
- `ttbox_motion/__init__.py`

### Web

- `web/static/app.js`
- `web/static/motion_training.js`
- `web/static/motion_training_mobile.js`
- `web/templates/motion_training.html`

### 文档与测试

- `docs/architecture/TTBOX_CALIBRATION_BEHAVIOR_MODEL.md`
- `docs/architecture/TTBOX_YU_REFERENCE_MAP.md`
- `docs/architecture/TTBOX_PHASE_8_3_CODE_LINEAGE.md`
- `docs/product/TTBOX_WEB_FEATURE_MATRIX.md`
- `docs/product/features/personal-motion-training.md`
- `platform/tests/test_calibration_behavior.py`
- `platform/tests/test_motion_training.py`

---

## 9. 验证结果

本地验证：

- Python 语法检查通过。
- 平台测试：`50 passed`。
- Core 编译通过。
- Core 原有测试：`103 passed`。
- PersonalMotion 测试：`4 passed`。
- 标定行为测试：`5 passed`。
- 个人训练测试：`11 passed`。

真机验证：

- TTBOX Core 编译通过。
- `ttbox-core.service` active。
- `ttbox-web.service` active。
- `ttbox-release-manager.service` active。
- TTBOX Web 监听 8081。
- Core 状态曾确认 `running=true`，采集链路正常。
- 自动标定无目标时浏览器/API 真实拒绝。
- 自动标定状态读取真实返回完整状态字段。
- 真实 HDMI 稳定目标下的 X/Y 全流程成功闭环尚未完成，因此不能标 REAL。

---

## 10. 当前状态统计

按严格证据纪律：

- **REAL**：配置读写、手动标定参数保存/回读、状态读取、个人训练 Domain/Gateway 浏览器链路、Core 编译和逻辑测试。
- **VERIFY**：真实目标下自动标定完整完成、个人曲线最终 HID 效果、C 桥相关移动控制效果。
- **PLANNED**：尚未定义正式 TTBOX 协议或 Core 消费点的高级扩展。
- **BROKEN**：Phase 8.4 已修复的原自动标定假接口和训练固定成功接口不再作为正式实现使用。

数量统计必须以每次具体验收矩阵为准，不能把同一功能的配置链路和硬件效果合并后虚标为 REAL。

---

## 11. 安全与运行边界

本阶段：

- 没有执行整机 reboot、shutdown 或 poweroff。
- 没有修改网络、SSH、防火墙、内核或 BSP。
- 只在允许范围内切换 TTBOX Core/Web 单服务。
- 没有把 YU daemon、YU API 或 YU 配置接入 TTBOX 正式运行链。
- 真机正式代码来源仍是本地 Git 仓库；板端仅用于编译和验收。

---

## 12. 结论与后续

Phase 8.4 已完成自动标定的行为级逆向、TTBOX 自有 Domain 建模、Gateway/Core/Web 主链实现、个人曲线训练原生化和基础真机验证。

当前唯一影响“完整完成”的关键缺口，是需要在真实 HDMI 目标场景下由浏览器完成：

```text
开始
  → 稳定候选
  → X 轴采样/拟合
  → Y 轴采样/拟合
  → 验证
  → 应用
  → Core 回读
  → 刷新保持
  → 取消恢复
```

在该场景证据产生前，自动标定保持 VERIFY，不标 REAL。

关联文档：

- `docs/architecture/TTBOX_CALIBRATION_BEHAVIOR_MODEL.md`
- `docs/architecture/TTBOX_YU_REFERENCE_MAP.md`
- `docs/product/TTBOX_WEB_FEATURE_MATRIX.md`
- `docs/product/features/personal-motion-training.md`
