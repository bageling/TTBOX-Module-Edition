# TTBOX-模块版（TTBOX-Module-Edition）

> 一个模块化、可插拔、可维护的 **RK3588 AI 视觉盒子基础平台**。
> 通过 HDMI "看"电脑屏幕，用 AI 识别画面中的目标。

![系统一句话](docs/images/ttbox-one-line.png)

---

## TTBOX 是什么？

TTBOX 是一台基于 **RK3588 芯片（OrangePi 5 Plus）** 的 AI 视觉盒子：

```
电脑画面 ──HDMI线──▶ 盒子（RK3588）
                       │
                       ▼
              AI 分析画面（每秒 43~142 帧）
                       │
                       ▼
              "画面里有什么？目标在哪？"
                       │
                       ▼
              目标选择 → 坐标 → 控制（可选，当前关闭）
```

**它解决什么问题**：用 AI 代替人眼，快速发现画面中的目标并定位。
**它不做什么**：不读内存、不看游戏数据——只"看"画面（外部视觉）。

---

## 硬件与运行环境

| 组件 | 说明 |
|---|---|
| AI 盒子 | OrangePi 5 Plus（RK3588 芯片，3 核 NPU） |
| 操作系统 | Armbian Linux |
| 输入 | HDMI（电脑画面，2560×1440） |
| AI 核心 | C++（ttbox_core，systemd 托管） |
| 控制台 | Web 页面（Python 服务） |
| 模型 | .rknn 格式（瑞芯微专用） |

**两条线路（务必分清）**：
- **HDMI**：画面从电脑 → 盒子（盒子的"眼睛"）
- **USB**：指令从盒子 → 电脑（盒子的"手"，当前关闭）

**USB 不是视频链路**，它只是盒子与电脑的独立连接（伪装鼠标键盘）。

---

## 当前完成到哪一步？

### ✅ 已完成并验证

```
PC真实画面 → HDMI → RK3588 HDMI RX → /dev/video0
  → V4L2 采集（141.7 FPS）
  → RGA 图像处理（裁剪+缩放）
  → RKNN NPU 推理（43.1 FPS）
  → DecodeNMS 解码（坐标误差 0.00）
  → Detection 检测结果（person/car/bus，置信度 0.80~0.90）
```

### 📊 真实性能（2026-09-03 实测）

| 指标 | 值 |
|---|---|
| Capture FPS | **141.7** |
| Inference FPS | **43.1** |
| infer（推理） | **63.55 ms** |
| decode（解码） | **5.39 ms** |
| E2E（端到端） | **66.6 ms** |

### ⛔ 保持关闭（安全红线）

- `output_enabled = false`
- `injection_allowed = false`
- 鼠标注入未开启

---

## 怎么查看目录？

```
TTBOX-Module-Edition/
├── core/          C++ 核心（AI 高速链路，真实源码）
├── framework/     Python 框架（插件管理/配置/服务/安全）
├── plugins/       功能插件（web/preview/model/fan/wifi/...）
├── platform/      平台层（systemd/运行时/模型/健康）
├── modules/       ★ 模块化语义视图（小白从这里看结构，01-09）
├── models/        模型（真实模型在板端）
├── config/        配置文件
├── scripts/       运维脚本 + Web 主程序
├── ttbox_motion/  运动控制（校准/训练）
└── docs/          ★ 文档中心（架构/教程/验证/AI/开发/规划）
```

---

## 怎么开始学习？

### 完全不懂代码？看这里 👇

| 教程 | 内容 |
|---|---|
| [01-TTBOX是什么](docs/小白教程/01-TTBOX是什么.md) | 什么是 TTBOX |
| [02-TTBOX怎么工作](docs/小白教程/02-TTBOX怎么工作.md) | 整体工作流程 |
| [03-电脑画面怎么进入盒子](docs/小白教程/03-电脑画面怎么进入盒子.md) | HDMI 视频链路 |
| [04-AI是怎么识别目标的](docs/小白教程/04-AI是怎么识别目标的.md) | AI 识别原理 |
| [05-模型是什么](docs/小白教程/05-模型是什么.md) | 模型概念 |
| [06-检测框是什么](docs/小白教程/06-检测框是什么.md) | 检测结果 |
| [07-坐标是怎么计算的](docs/小白教程/07-坐标是怎么计算的.md) | 坐标换算 |
| [08-插件是什么](docs/小白教程/08-插件是什么.md) | 插件系统 |
| [09-如何添加模型](docs/小白教程/09-如何添加模型.md) | 换模型步骤 |
| [10-如何排查问题](docs/小白教程/10-如何排查问题.md) | 问题排查 |

### 想深入了解？看这里 👇

| 文档 | 内容 |
|---|---|
| [系统总览](docs/架构/系统总览.md) | 系统分层全景 |
| [完整链路](docs/架构/完整链路.md) | PC 画面到鼠标指令全链路 |
| [核心模块](docs/架构/核心模块.md) | 每个模块详解 |
| [插件系统](docs/架构/插件系统.md) | 插件怎么工作 |
| [数据流](docs/架构/数据流.md) | 数据在每个环节的样子 |
| [目录结构](docs/架构/目录结构.md) | 仓库目录说明 |
| [真实HDMI闭环验证](docs/验证/真实HDMI闭环验证.md) | 真实验证记录与数据 |
| [模型系统](docs/AI/模型系统.md) | 模型概念与清单 |
| [模型输入格式](docs/AI/模型输入格式.md) | yolo261n 关键修复 |
| [修改指南](docs/开发/修改指南.md) | 改代码去哪看 |
| [项目路线图](docs/规划/项目路线图.md) | 已完成/进行中/未来 |
| [整理前代码现状](docs/整理前代码现状.md) | 整理前的真实快照 |

---

## 怎么运行？（板端）

```bash
# 启动/停止/查看
systemctl start ttbox-core.service
systemctl status ttbox-core.service

# 手动运行（调试）
/opt/ttbox/core/build/ttbox_core_main \
  --config /opt/ttbox/config/default.json \
  --ipc /tmp/ttbox_core.sock

# 查看实时状态
/opt/ttbox/core/build/ipc_ping --type GET_STATUS
```

## 怎么编译？（开发者）

```bash
# 板端（RK3588）
cd /opt/ttbox/core/build && cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target ttbox_core_main -j4

# Windows（开发机）
cmake -B core/build -S core
cmake --build core/build --config Release
ctest --test-dir core/build -C Release
```

---

## 技术栈

| 层 | 技术 |
|---|---|
| 硬件 | RK3588（3 核 NPU）+ HDMI RX |
| AI | RKNN 模型 + rknnrt 运行时 |
| 图像 | RGA 硬件加速（im2d） |
| 采集 | V4L2 + DMA-BUF |
| 核心 | C++17 |
| 框架/Web | Python 3 + 插件架构 |
| 构建 | CMake + CTest / pytest |

## 许可证

本项目以 **MIT 许可证** 开源。详见 [LICENSE](LICENSE)。

---

*TTBOX-模块版 —— 让 AI 视觉盒子简单、清晰、可维护。*
