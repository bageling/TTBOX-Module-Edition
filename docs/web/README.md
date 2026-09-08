# docs/web/ — 真实实现真相与外部对照

> **目的**：这是 TTBOX 当前实现真相、YU 对照基线、模型/Preview/E2E 验收报告的归档区。
> **与 docs/README 的区别**：`docs/README.md` 是“小白使用路线”，这里只放**真相文档**，不是教程。
> **谁来读**：接手智能体、用户验收、跨阶段核对。

---

## 一、子目录速查

| 目录 | 回答什么问题 | 何时更新 |
|---|---|---|
| [architecture/](architecture/) | TTBOX 现在**到底是怎么跑起来的**？每个模块谁调谁、数据怎么走、边界在哪？ | 架构变更、模块重排时 |
| [model/](model/) | 模型库怎么存、怎么切、怎么换 YOLO 不动核心代码？ | 模型系统实现、转换、新模型接入时 |
| [preview/](preview/) | Preview 真相是什么、YU Preview 是怎么实现的？ | Preview 架构变更、与 YU 对照有变化时 |
| [yu-compatibility/](yu-compatibility/) | YU 真实功能基线 + TTBOX 与 YU 的差距 + 最终对齐报告 | YU 抓包 / TTBOX 功能补齐时 |
| [e2e/](e2e/) | 端到端验收场景矩阵、隔离能力、Web 路由图 | Web/端到端验收、隔离验证时 |
| [verification/](verification/) | 单条 API/单条功能的验证记录（按编号 WEB-XXX） | 每个 API/功能验完即加 |
| [yu-baseline/](yu-baseline/) | YU 真实抓包原文：API/行为/配置/页面/错误/重启 | 抓包后归档 |

---

## 二、按任务查文档

### 任务：理解 TTBOX 现在是怎么实现的
1. [architecture/TTBOX_CURRENT_ARCHITECTURE.md](architecture/TTBOX_CURRENT_ARCHITECTURE.md) — 161 行，只列已验证存在且接通的
2. [architecture/TTBOX_ARCHITECTURE_DEEP_DIVE.md](architecture/TTBOX_ARCHITECTURE_DEEP_DIVE.md) — 每个功能实现逻辑
3. [architecture/SHARED_HARDWARE_RESOURCES.md](architecture/SHARED_HARDWARE_RESOURCES.md) — TTBOX/YU 共用硬件约束

### 任务：增加/切换一个模型
1. [model/TTBOX_MODEL_SYSTEM_DESIGN.md](model/TTBOX_MODEL_SYSTEM_DESIGN.md) — 模型库 + 切换 + 恢复总设计
2. [model/TTBOX_MODEL_CONVERSION.md](model/TTBOX_MODEL_CONVERSION.md) — pt → onnx → rknn → manifest 转换流水
3. [model/TTBOX_模型转换体系调查报告.md](model/TTBOX_模型转换体系调查报告.md) — 当前转换工具链现状
4. [model/TTBOX模型系统阶段交接.md](model/TTBOX模型系统阶段交接.md) — 阶段交接记录

### 任务：理解 Preview 真相与 YU 区别
1. [preview/TTBOX_PREVIEW_TRUTH.md](preview/TTBOX_PREVIEW_TRUTH.md) — TTBOX Preview 真相
2. [preview/YU_PREVIEW_REVERSE_ENGINEERING.md](preview/YU_PREVIEW_REVERSE_ENGINEERING.md) — YU Preview 逆向

### 任务：看 TTBOX 与 YU 的差距
1. [yu-compatibility/YU_FUNCTION_BASELINE.md](yu-compatibility/YU_FUNCTION_BASELINE.md) — YU 真实功能基线
2. [yu-compatibility/TTBOX_YU_REAL_FUNCTION_MATRIX.md](yu-compatibility/TTBOX_YU_REAL_FUNCTION_MATRIX.md) — 真实功能对照矩阵
3. [yu-compatibility/TTBOX_YU_COMPATIBILITY_MATRIX.md](yu-compatibility/TTBOX_YU_COMPATIBILITY_MATRIX.md) — 兼容性矩阵（含状态机）
4. [yu-compatibility/TTBOX_YU_FINAL_REPORT.md](yu-compatibility/TTBOX_YU_FINAL_REPORT.md) — 最终对齐报告
5. [yu-compatibility/YU_TTBOX_OVERLAY_COMPARE.md](yu-compatibility/YU_TTBOX_OVERLAY_COMPARE.md) — 叠加层对比

### 任务：端到端验收 / Web 路由图
1. [e2e/TTBOX_E2E_SCENARIO_MATRIX.md](e2e/TTBOX_E2E_SCENARIO_MATRIX.md) — 端到端场景矩阵
2. [e2e/TTBOX_ISOLATION.md](e2e/TTBOX_ISOLATION.md) — TTBOX/YU 共存隔离能力
3. [e2e/WEB_FUNCTION_MAP.md](e2e/WEB_FUNCTION_MAP.md) — Web 路由 × 功能矩阵

### 任务：看单条 API 验证
→ [verification/](verification/) 目录下 `WEB-XXX-overview.md` 系列

### 任务：看 YU 真实抓包原文
→ [yu-baseline/](yu-baseline/) 目录下 `api/ behavior/ config/ errors/ pages/ restart/ runtime/`

---

## 三、维护纪律

1. **新文档必须放进对应子目录**，不放回根目录。
2. **同主题文档合并**到所在子目录，不在根目录堆叠。
3. **每篇文档开头三行**必须含“日期 + 板子 + 状态”，便于交接判断新鲜度。
4. **不再使用的旧文档**挪进 `archived/`，不删，方便溯源。

---

## 四、最近更新

- 2026-09-07：docs/web 首次按主题归档为 5 个子目录 + 索引页
