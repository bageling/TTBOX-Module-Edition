# TTBOX-模块版（TTBOX-Module-Edition）

> 一个模块化、可插拔、可维护的 **RK3588 AI 视觉盒子**。
> 通过 HDMI "看"电脑屏幕，用 AI 识别目标，并通过自研 usbproxy 完成 **AI → HID 鼠标注入全闭环**。

![系统一句话](docs/images/ttbox-one-line.png)

---

## TTBOX 是什么？

TTBOX 是一台基于 **RK3588 芯片（OrangePi 5 Plus）** 的 AI 视觉盒子：

```
电脑画面 ──HDMI──▶ RK3588（盒子）
                     │
                     ▼
        V4L2 采集 → RGA → RKNN NPU → DecodeNMS
                     │
                     ▼
        TargetSelector → PID → AimThread
                     │
                     ▼
        MouseControlClient → MOVE_CMD
                     │
                     ▼
        TTBOX usbproxy → Raw Gadget → HID
                     │
                     ▼
              Windows 鼠标真实移动
```

**它解决什么问题**：用 AI 代替人眼，快速发现画面中的目标并自动定位。
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
| 模型 | .rknn 格式（瑞芯微专用，默认 yolo261n COCO 80 类） |
| 输出 | TTBOX usbproxy（自研，raw-gadget + libusb） |

**两条线路（务必分清）**：
- **HDMI**：画面从电脑 → 盒子（盒子的"眼睛"）
- **USB**：指令从盒子 → 电脑（盒子的"手"，TTBOX usbproxy 模拟鼠标）

---

## 当前完成到哪一步？

### ✅ 已完成并真机验证（2026-09-04）

**① AI 检测闭环（真实 HDMI 画面）**

```
PC真实画面 → HDMI → RK3588 HDMI RX → /dev/video0
  → V4L2 采集（141.7 FPS）
  → RGA 图像处理（裁剪+缩放）
  → RKNN NPU 推理（43.1 FPS）
  → DecodeNMS 解码（坐标误差 0.00）
  → Detection 检测结果（person，持续锁定）
```

**② AI → HID 鼠标注入全闭环（8 场景真机验证全 PASS）**

```
Detection → TargetSelector → PID → AimThread
  → MouseControlClient → MOVE_CMD(0x4F50)
  → TTBOX usbproxy cmd.sock
  → HID report → Raw Gadget → Windows 真实光标移动
```

已验证场景：AI 关闭不注入 / 目标居中 / 目标左 / 目标右 / 目标上 / 目标下 / 热键关闭即停 / 100~2000Hz 高频 0 丢包。

**③ TTBOX usbproxy（自研 USB 鼠标代理）**

- 完全摆脱外部加密 usb-proxy 与 AI 注入后端，TTBOX 独立完成 AI→HID 全链路
- full passthrough 模式：克隆 Logitech 046d:c53f，物理鼠标 + AI 位移搭车合并
- synthetic 模式：独立合成 Corsair 9A80:7072 鼠标，AI 独立注入
- 自研二进制协议：cmd.sock/event.sock 0x4F50 全 15 种消息
- RT 调度：SCHED_FIFO 98 + CPU affinity，1000Hz 零丢失

### 📊 真实性能（2026-09-03/04 实测）

| 指标 | 值 |
|---|---|
| Capture FPS | **141.7** |
| Inference FPS | **43.1** |
| infer（推理） | **63.55 ms** |
| decode（解码） | **5.39 ms** |
| E2E（端到端） | **66.6 ms** |
| 高频注入 | **100~2000Hz 全部 0 丢包、1:1 位移** |

### ⛔ 安全红线（默认保持）

- `output_enabled = false`
- `injection_allowed = false`
- `mouse.enabled = false`
- 开机不会自动注入鼠标；热键/开关关闭即时停止（fail-closed）

---

## 怎么查看目录？

```
TTBOX-Module-Edition/
├── core/          C++ 核心（AI 高速链路，真实源码）
├── usbproxy/      ★ TTBOX usb-proxy（自研，raw-gadget+libusb）
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

### usbproxy/ 内部结构

| 文件 | 作用 |
|---|---|
| `usb-proxy.cpp` | 主入口（full/synthetic 双模式 + 参数解析） |
| `proxy.cpp` | raw-gadget 端点转发 + AI 位移搭车合并注入 |
| `mouse_control.cpp/.hpp` | cmd.sock/event.sock 0x4F50 协议层（15 种消息） |
| `synthetic.cpp/.h` | synthetic 模式：合成鼠标描述符 + RT 注入线程 |
| `board/run-ttbox-usb-proxy.sh` | 启动脚本（full 自动找物理鼠标） |
| `systemd/ttbox-usbproxy.service` | systemd 托管（Restart=always） |
| `gadget-config.json` | synthetic 身份配置（Corsair 9A80:7072） |

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
# AI 核心 + usbproxy 服务
systemctl start ttbox-core.service
systemctl start ttbox-usbproxy.service

# 手动运行 usbproxy（调试）
cd /opt/ttbox/usbproxy && ./usb-proxy \
  --device=fc000000.usb --driver=dwc3-gadget \
  --synthetic_mouse --enable_mouse_control

# 查看实时状态
/opt/ttbox/core/build/ipc_ping --type GET_STATUS
```

## 怎么编译？（开发者）

```bash
# 板端（RK3588）AI 核心
cd /opt/ttbox/core/build && cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target ttbox_core_main -j4

# 板端 usbproxy
cd /opt/ttbox/usbproxy && make

# Windows（开发机）
cmake -B core/build -S core
cmake --build core/build --config Release
ctest --test-dir core/build -C Release
```

---

## 技术栈

| 层 | 技术 |
|---|---|
| 硬件 | RK3588（3 核 NPU）+ HDMI RX + USB OTG |
| AI | RKNN 模型 + rknnrt 运行时 |
| 图像 | RGA 硬件加速（im2d） |
| 采集 | V4L2 + DMA-BUF |
| 鼠标注入 | raw-gadget + libusb（usbproxy） |
| 核心 | C++17 |
| 框架/Web | Python 3 + 插件架构 |
| 构建 | CMake + CTest / pytest |

## 许可证

本项目以 **MIT 许可证** 开源。详见 [LICENSE](LICENSE)。
usbproxy/ 底层使用 [raw-gadget](https://github.com/xairy/raw-gadget) 与 libusb 开源库（Apache-2.0），保留上游版权声明。

---

*TTBOX-模块版 —— 让 AI 视觉盒子简单、清晰、可维护，AI → HID 全闭环。*

