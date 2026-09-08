# e2e/ — 端到端验收 + Web 路由 + 隔离

> 用于跨阶段、跨模块的整体验收：跑通一条链路、隔离 TTBOX/YU 共存、查 Web 路由是否接对功能。

| 文档 | 日期 | 用途 |
|---|---|---|
| [TTBOX_E2E_SCENARIO_MATRIX.md](TTBOX_E2E_SCENARIO_MATRIX.md) | 2026-09-05 | 端到端场景矩阵：每条场景写明 入口 / 期望 / 实测 / 状态 |
| [TTBOX_ISOLATION.md](TTBOX_ISOLATION.md) | 2026-09-05 | TTBOX/YU 共存隔离：哪些资源互斥、抢占顺序、停一边会不会影响另一边 |
| [WEB_FUNCTION_MAP.md](WEB_FUNCTION_MAP.md) | 2026-09-04 | Web 路由 × 功能矩阵：✅存在已接通 / ⚠️存在未接通 / ❌不存在 |

## 何时更新

- 新增端到端场景 → 更新 SCENARIO_MATRIX
- 隔离规则变化（独占设备、共享资源）→ 更新 ISOLATION
- 新增/删除 Web 路由 → 更新 WEB_FUNCTION_MAP

## 维护纪律

- 三篇都必须有“日期 + 板子 + 验证方式”，无验证不算数
- 状态从 ⚠️ 变 ✅ 或 ❌ 时，旧条目不删，划删除线留底
