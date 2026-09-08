# model/ — 模型系统设计稿与转换体系

> 这一组回答三个核心问题：
> 1. 拿一个新 YOLO 模型放进 TTBOX 要做哪几步？
> 2. 从 v11 换成 v26，C++ 核心代码要不要改？
> 3. 模型切换失败会不会把当前模型搞死？

| 文档 | 状态 | 用途 |
|---|---|---|
| [TTBOX_MODEL_SYSTEM_DESIGN.md](TTBOX_MODEL_SYSTEM_DESIGN.md) | v1.0 设计稿 | 模型库目录 / manifest / Registry / Adapter / 切换 / 恢复 — 779 行总体设计 |
| [TTBOX_MODEL_CONVERSION.md](TTBOX_MODEL_CONVERSION.md) | v1.0 设计稿 | pt → onnx → rknn → manifest 转换流水，404 行 |
| [TTBOX_模型转换体系调查报告.md](TTBOX_模型转换体系调查报告.md) | 现状报告 | 当前转换工具链做了什么、还差什么（362 行） |
| [TTBOX模型系统阶段交接.md](TTBOX模型系统阶段交接.md) | 交接记录 | 2026-09-06 阶段 16 末交接给下一阶段 |

## 何时更新

- 新模型加入 ModelRegistry → 更新 SYSTEM_DESIGN 中的 manifest 字段说明
- 转换链路有新工具或新校验 → 更新 CONVERSION
- 阶段推进 → 写一份新的“阶段交接”文档，覆盖上一份

## 维护纪律

- 这 4 篇**不是**教程，是设计稿/调查报告。教程放在 `docs/小白教程/`。
- 已实现的代码约定 → 必须与 SYSTEM_DESIGN 保持一致；不一致时改文档或改代码，**不能双轨**。
