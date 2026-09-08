# preview/ — Preview 真相 + YU Preview 逆向

> 这两篇用于回答：Preview 到底是什么、模型变化会不会影响 Preview、与 YU Preview 差多远。

| 文档 | 日期 | 用途 |
|---|---|---|
| [TTBOX_PREVIEW_TRUTH.md](TTBOX_PREVIEW_TRUTH.md) | 2026-09-05 | TTBOX Preview 当前真相：原始分辨率 JPEG + OpenCV 画框 + 中心640×640 与 Preview 解耦 |
| [YU_PREVIEW_REVERSE_ENGINEERING.md](YU_PREVIEW_REVERSE_ENGINEERING.md) | 2026-09-06 | YU Preview 逆向：抓包、接口、协议、对照差异 |

## 何时更新

- Preview 架构变更（编码/JPEG 管线/MJPEG 边界） → 更新 TRUTH
- YU 接口或行为变化 → 更新 REVERSE
- 出现 “模型尺寸变了 Preview 跟着变” 这种 bug → 立即回头补 TRUTH 边界条款

## 硬约束（写入此处）

> **模型输入尺寸变化不影响 Preview 分辨率**。  
> Preview 永远走“原始 Capture Frame → OpenCV 画框 → 原始分辨率 JPEG”这条路径。  
> 模型输入裁剪/缩放只发生在 AI 流水线内部，且裁剪中心点与模型输入尺寸完全由 ModelAdapter 决定。
