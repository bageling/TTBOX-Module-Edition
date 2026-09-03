# TTBOX Phase 8.3 开发文档

## 03「移动控制」产品化与代码血缘接通

> 日期：2026-09-01  
> 项目：TTBOX  3.0  03「移动控制」  
> 基线提交：`91bb7a0`（Phase 8.3 阶段提交）

---

## 1. 阶段目标

本阶段不是照搬参考产品页面，而是从 TTBOX 当前 Git 仓库出发，逐个控件追踪完整链路：

```text
Web 控件
  → web/static/app.js / apiClient
  → scripts/ttbox_web.py Gateway
  → RuntimeProfile / RuntimeConfig
  → Core IPC
  → AimThread / PID / 输出链
  → HID 或真实设备状态
```

每项功能按照实际证据分为：

- **ORIGINAL**：TTBOX Core 已经具备，本阶段直接复用。
- **FIXED**：能力存在，但 Web、Gateway 或消费链路断开、漏接或接错。
- **EXTENDED**：在已有能力上做正式扩展。
- **PLANNED**：页面有产品位置，但 Core 没有真实能力。
- **BROKEN**：当前入口存在，但实际链路不能正常工作。

---

## 2. 03 页面功能总数

按阶段血缘审查口径，03「移动控制」共 **13 项功能**：

1. PID：Kp/Ki/Kd、预测、跟随速率、基础死区
2. 移动倍率 sens
3. 目标丢失宽限
4. 开火时 Y 轴锁定
5. 拉枪曲线
6. 持续提前量
7. 屏蔽物理移动
8. 自动标定
9. 手动标定参数
10. 记录移动日志
11. 恢复本页默认值
12. X/Y 轴预判与跟随参数子项
13. 个性移动曲线训练

说明：页面控件数量会多于 13 个，但同一产品能力下的字段和控件按功能组统计。

---

## 3. 主要发现

### 3.1 1.0 Core 原生能力

以下能力在 Git 历史和 Core 消费点中已确认存在，不需要重新设计：

- `sens` 移动倍率
- PID 参数与跟随/预判参数
- 目标丢失宽限
- 基础输出死区
- 开火锁定 Y 轴
- 持续提前量
- 屏蔽物理移动
- 自动标定所需的 RuntimeConfig、目标位置反馈和配置热更新基础
- 移动日志采集基础
- 页面默认值恢复

其中部分能力实际依赖旧 C 桥或运行链状态，配置可保存不等于 HID 效果已经在当前板端验收，文档中单独标记 VERIFY。

### 3.2 Gateway 或 Web 接线问题

本阶段识别出的主要接线问题：

- 主 Gateway 的自动标定接口曾经是假成功/固定返回，没有调用真实标定逻辑。
- 自动标定前端字段和后端字段名称不一致。
- 记录移动日志按钮被模板条件隐藏。
- 普通配置和自动标定状态提示语义混用，容易把状态变化误认为“已保存”。
- 部分 C 桥功能需要配置注入，但主 Gateway 未将配置写入 C 桥实际读取的位置。
- `AimThread` 输出链曾被历史提交误删，导致 sens、死区和置信度读取在本地主线中失效。

### 3.3 真正缺失能力

- 个性移动曲线训练：原先只有页面脚本和产品位置，Core 没有正式模型、训练会话和消费链路。
- 03 页部分高级功能仍需 TTBOX 自己的正式协议和 Core 消费设计，不能因为参考产品有对应字段就伪造完成。

---

## 4. 代码血缘总图

```text
web/templates/index.html
  控件：sens、controller_*、autoCalibration*、recordAimTraceButton
        ↓
web/static/app.js
  collectConfig / requestConfigApply / pollAutoCalibration
        ↓
window.ttbox.api.request
        ↓
POST/PUT/GET /api/config
或 /api/control/calibration*
        ↓
scripts/ttbox_web.py
  Gateway 参数校验、RuntimeProfile 转换、Core IPC
        ↓
core/src/ipc/IpcServer.cpp
  SET_CONFIG / GET_CONFIG、校验、热更新、持久化
        ↓
core/src/model/RuntimeProfile.cpp
        ↓
core/src/aim/AimThread.cpp
  目标选择 → PID → 预测/跟随 → 死区/倍率 → 输出动作
        ↓
core/src/output/* / HID
```

---

## 5. 关键处理

### 5.1 输出链回归

恢复并核对了历史版本中已经存在的输出逻辑：

- sensitivity 全局倍率
- output scale
- output deadzone
- Core 置信度真实读取
- aim error 遥测

这属于恢复 TTBOX 已有能力，不是临时新增旁路。

### 5.2 自动标定接口接通

自动标定从主 Gateway 假接口改为真实状态机入口，支持：

- GET 当前状态
- POST 启动
- POST 取消
- DELETE 清除
- PUT 手动标定参数

真实目标闭环和行为级算法在 Phase 8.4 继续深化。

### 5.3 记录移动日志露出

Gateway 已有真实采样线程和日志输出路径；本阶段修正页面显示条件，让产品入口与真实 API 对齐。

### 5.4 个性曲线训练边界

Phase 8.3 不把已有页面误标为完成。没有 Core 训练能力时保留为 PLANNED，后续在 Phase 8.4 以 TTBOX 自有 Domain 和 RuntimeProfile 方式施工。

---

## 6. 功能状态矩阵

| 功能组 | Core 状态 | Gateway/Web 状态 | 阶段结论 |
|---|---|---|---|
| sens / PID / 预测 / 跟随 / 死区 | 原生存在 | 已接通或修复历史回归 | ORIGINAL / FIXED |
| 丢失宽限 | 原生存在 | 链路完整 | ORIGINAL |
| 锁 Y / 持续提前量 / 物理屏蔽 | 能力存在，部分在 C 桥 | 配置注入和运行效果需板端确认 | FIXED / VERIFY |
| 拉枪曲线 | 类和配置存在，消费点断开 | 页面和配置可用 | BROKEN / EXTENDED |
| 自动标定 | 基础能力存在 | 主 Gateway 从假桩改为真实入口 | FIXED |
| 手动标定 | 配置和换算能力存在 | 保存/回读接通 | FIXED |
| 移动日志 | 采样能力存在 | 页面入口修复 | ORIGINAL / FIXED |
| 默认值恢复 | 链路完整 | 页面可用 | ORIGINAL |
| 个性曲线训练 | Core 原先缺失 | 页面无真实后端 | PLANNED |

---

## 7. 验证结果

已完成的验证：

- Core 原有测试：`103/103` 通过。
- Phase 8.3 相关浏览器配置读写、修改、保存、回读、恢复：已实测。
- 普通参数修改一次只发送一次 `PUT /api/config`。
- 未发现 input/change 双事件导致的重复保存请求。
- 自动标定无目标时返回真实拒绝，而不是固定成功。
- 真机 TTBOX Web 运行在 8081，Core 和 Web 服务均保持运行。

未在本阶段虚标的项目：

- 真实游戏目标下的自动标定完整成功路径。
- C 桥功能的最终 HID 效果。
- 个性移动曲线的 Core 和真实 HID 效果。

---

## 8. 结论

Phase 8.3 完成了 03「移动控制」的代码血缘审查和第一轮产品化接线。核心结论是：TTBOX 不是缺少所有移动控制能力，而是已有能力分散在 Core、Gateway、旧 C 桥和前端之间，历史重构造成了若干断点。

本阶段按“已有能力优先接通、没有能力诚实规划”的原则处理，没有用假 API 冒充 Core 能力，也没有将参考产品运行时接入 TTBOX。

详细逐控件证据见：

- `docs/architecture/TTBOX_PHASE_8_3_CODE_LINEAGE.md`
- `docs/product/TTBOX_WEB_FEATURE_MATRIX.md`
