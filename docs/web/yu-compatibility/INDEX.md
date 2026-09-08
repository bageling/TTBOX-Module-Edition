# yu-compatibility/ — YU 对照基线与差距矩阵

> YU = 竞品（标准 RJY 2.3.2）。我们用它当“功能参考线”，不抄袭代码，只对齐行为。

## 阅读顺序（按这个顺序看才不乱）

1. [YU_FUNCTION_BASELINE.md](YU_FUNCTION_BASELINE.md) — YU **真实运行**功能基线（不是宣称）
2. [TTBOX_YU_REAL_FUNCTION_MATRIX.md](TTBOX_YU_REAL_FUNCTION_MATRIX.md) — TTBOX vs YU 真实功能逐项对照
3. [YU_TTBOX_OVERLAY_COMPARE.md](YU_TTBOX_OVERLAY_COMPARE.md) — 叠加层/抓包层差异
4. [TTBOX_YU_COMPATIBILITY_MATRIX.md](TTBOX_YU_COMPATIBILITY_MATRIX.md) — 兼容性矩阵（含状态机：NOT_STARTED/INVESTIGATING/IMPLEMENTING/VERIFYING/FAIL/PASS/BLOCKED/UNSUPPORTED_HARDWARE）
5. [TTBOX_YU_FINAL_REPORT.md](TTBOX_YU_FINAL_REPORT.md) — 最终对齐报告（阶段性收尾）

## 何时更新

- YU 抓包新增能力 → 更新 YU_FUNCTION_BASELINE
- TTBOX 新增一项能力 → 更新 REAL_FUNCTION_MATRIX 与 COMPATIBILITY_MATRIX
- 阶段性对齐收尾 → 写一份新的 FINAL_REPORT

## 硬纪律

- **状态值不允许乐观**。没接通的写 NOT_STARTED / IMPLEMENTING，不要写 PASS。
- **禁止抄代码**。任何与 YU 行为一致的实现必须是“独立实现 + 行为对照”，不是 copy。
- **matrix 与代码不同步时**，修代码或修文档，不允许双轨。
