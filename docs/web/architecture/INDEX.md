# architecture/ — TTBOX 当前架构真相

> 这三篇都是“真相报告”，不是设计稿。每篇第一行都标明验证日期和板子。

| 文档 | 验证日期 | 板子 | 用途 |
|---|---|---|---|
| [TTBOX_CURRENT_ARCHITECTURE.md](TTBOX_CURRENT_ARCHITECTURE.md) | 2026-09-04 | 192.168.0.53 | 161 行只列“存在且已接通”的功能，新接手第一份读这个 |
| [TTBOX_ARCHITECTURE_DEEP_DIVE.md](TTBOX_ARCHITECTURE_DEEP_DIVE.md) | 2026-09 阶段 14 末 | 192.168.0.53 | 276 行深入到每个函数的调用链与数据格式，调试时查这里 |
| [SHARED_HARDWARE_RESOURCES.md](SHARED_HARDWARE_RESOURCES.md) | 2026-09-05 | 192.168.0.53 | TTBOX/YU 共用 RK3588 上的硬件资源边界（HDMI-IN、USB-HID、NPU 抢占等） |

## 何时更新

- 模块新增/删除/重命名 → 更新 CURRENT + DEEP_DIVE
- 与 YU 出现新的硬件冲突 → 更新 SHARED_HARDWARE_RESOURCES
- 任何“功能宣称”与这三篇不符 → 立即修正这三篇或修正功能，不允许事实漂移
