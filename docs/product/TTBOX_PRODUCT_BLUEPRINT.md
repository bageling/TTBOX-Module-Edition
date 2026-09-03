# TTBOX 产品蓝图

## TTBOX 是什么

TTBOX 是一个**独立完整的实时 AI 视觉辅助系统**，运行在 RK3588 硬件平台上。

### 产品定位

- **独立产品**：TTBOX 不是任何其他产品的兼容版或复刻版
- **完整系统**：从硬件采集到 AI 推理到输出控制的完整链路
- **实时优先**：端到端延迟控制在 10ms 级别

### 目标用户

- FPS 游戏玩家
- 视觉辅助系统开发者
- AI 实时推理系统集成者

### 硬件平台

- Orange Pi 5 Plus（RK3588）
- HDMI 输入（内置 HDMI RX 采集）
- USB HID 输出（模拟鼠标）

### 核心能力

| 能力 | 描述 |
|------|------|
| 实时采集 | 通过 HDMI 采集游戏画面，最高 240fps |
| AI 推理 | 使用 NPU 加速 AI 模型推理，143fps+ |
| 目标检测 | 实时检测画面中的目标 |
| 目标选择 | 从多个目标中选择最佳瞄准对象 |
| 目标跟踪 | 连续跟踪目标运动 |
| PID 控制 | 计算平滑的鼠标移动指令 |
| 硬件输出 | 通过 USB HID 模拟鼠标输出 |
| Web 控制台 | 浏览器管理设备 |
| 模型管理 | 上传/切换/管理 AI 模型 |
| 实时预览 | 浏览器查看实时画面 |

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Web 控制台 (Browser)                       │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  ttbox-bridge.js (Web UI 桥接)                       │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                        │ HTTP                               │
├────────────────────────┼─────────────────────────────────────┤
│  TTBOX Gateway        │                                      │
│  ┌────────────────────┴─────────────────────────────────┐   │
│  │  scripts/ttbox_gateway.py (Web API 服务器)            │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                        │ Unix Socket                         │
├────────────────────────┼─────────────────────────────────────┤
│  TTBOX Platform Layer │                                      │
│  ┌────────────────────┴─────────────────────────────────┐   │
│  │  platform/ (Python 平台管理)                          │   │
│  │  ├── runtime/controller.py (运行时控制)                │   │
│  │  ├── model/manager.py (模型管理)                      │   │
│  │  ├── supervisor/ (进程管理)                            │   │
│  │  ├── health/ (健康检查)                                │   │
│  │  └── update/ (更新管理)                                │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                        │ IPC                                 │
├────────────────────────┼─────────────────────────────────────┤
│  TTBOX Core (C++17)   │                                      │
│  ┌────────────────────┴─────────────────────────────────┐   │
│  │  IpcServer (IPC 通信)                                │   │
│  │  Application (应用生命周期)                           │   │
│  │  CoreRuntime (运行时核心)                             │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                        │                                     │
├────────────────────────┼─────────────────────────────────────┤
│  TTBOX Data Pipeline  │                                      │
│                        │                                     │
│  HDMI → Capture → RGA → RKNN → Decode → Target → Aim → Output → HID │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 核心数据流

### 帧数据流

```
HDMI 输入
  ↓ /dev/video0, V4L2
V4L2Capture 采集帧
  ↓ DMA-BUF（零拷贝）
RGA 硬件缩放（裁剪+缩放到模型输入尺寸，如 256×256）
  ↓ DMA-BUF
RKNNEngine 推理（NPU 执行 AI 模型）
  ↓ 原始张量
DecodeNMS 解码（解析 AI 输出 → 目标框）
  ↓ DetectionBox 数组
GeometryFilter 过滤（去掉不合理目标）
  ↓
TargetSelector 选择最佳目标
  ↓ AimTargetTask
AimThread 跟踪+控制（AlphaBeta 滤波 + PID 控制）
  ↓ OutputAction
OutputBackend 输出（鼠标指令）
  ↓ HID 协议
HID 桥接（hid/）
  ↓ USB
电脑收到鼠标移动
```

### 配置数据流

```
Web 页面（浏览器）
  ↓ HTTP
scripts/ttbox_gateway.py
  ↓ Unix Socket
IpcServer.cpp
  ↓ RuntimeConfig
Application / CoreRuntime
  ↓
WorkerPool → AimThread 等模块
```

### 模型数据流

```
模型文件（.rknn）
  ↓
ModelRegistry::import() → 复制到 staging 目录
  ↓
ModelRegistry::validate() → 校验模型文件
  ↓
ModelRegistry::install() → 安装到 installed 目录
  ↓
ModelRegistry::activate() → 设置为当前模型
  ↓
RKNNEngine::init() → 加载到 NPU 内存
  ↓
开始推理
```

## 产品功能树

```
TTBOX
│
├── Dashboard（总览面板）
│   ├── Runtime Status（运行状态）
│   ├── Performance Metrics（性能指标）
│   ├── Preview（实时预览）
│   └── System Resources（系统资源）
│
├── Runtime（运行时控制）
│   ├── Start / Stop AI Pipeline
│   └── Runtime Profile
│
├── Capture（采集控制）
│   ├── HDMI Input
│   ├── Crop Region（截取区域）
│   └── Resolution（分辨率）
│
├── AI（AI 推理）
│   ├── Model Management（模型管理）
│   ├── Detection（检测配置）
│   ├── Target Selection（目标选择）
│   ├── Tracking（目标跟踪）
│   ├── Prediction（目标预测）
│   └── Aim（瞄准控制）
│
├── Control（输出控制）
│   ├── PID（PID 控制参数）
│   ├── Mouse Output（鼠标输出）
│   ├── Hotkey（热键配置）
│   └── HID（HID 设备）
│
├── Profiles（预设管理）
│   ├── Save Profile
│   ├── Load Profile
│   └── Auto-save
│
├── Hardware（硬件管理）
│   ├── Display（显示器配置 / EDID）
│   ├── Mouse Hardware（鼠标硬件信息）
│   └── Device Enumeration（设备枚举）
│
├── Network（网络配置）
│   ├── Wi-Fi
│   ├── LAN Blocklist
│   └── Remote Access
│
├── System（系统管理）
│   ├── Auto-start
│   ├── Hostname
│   ├── Storage
│   ├── Reboot / Shutdown
│   └── License
│
└── Future（未来规划）
    ├── Motion Training（运动轨迹训练）
    ├── Auto Calibration（自动标定）
    ├── Remote Sync（远程同步）
    ├── Theme System（主题系统）
    └── Update Manager（更新管理）
```

## 产品模块清单

| 模块 | 状态 | 描述 |
|------|------|------|
| Dashboard | 🟢 完整实现 | 总览面板，含运行时状态、性能指标、预览 |
| Runtime Control | 🟢 完整实现 | 启动/停止 AI 流水线 |
| Capture | 🟢 完整实现 | HDMI 采集控制 |
| AI Detection | 🟢 完整实现 | AI 推理与目标检测 |
| Target Selection | 🟢 完整实现 | 目标选择与跟踪 |
| PID Control | 🟢 完整实现 | PID 控制算法 |
| Mouse Output | 🟢 完整实现 | 鼠标输出控制 |
| HID Output | 🟢 完整实现 | HID 设备输出 |
| Model Management | 🟢 完整实现 | 模型上传/切换/删除 |
| Preview | 🟢 完整实现 | 实时画面预览 |
| Display / EDID | 🟢 完整实现 | 显示器配置与 EDID 注入 |
| Profiles | 🟢 完整实现 | 预设保存/加载 |
| Network | 🟢 完整实现 | Wi-Fi 与网络配置 |
| System | 🟢 完整实现 | 主机名/重启/关机/存储 |
| License | 🟢 完整实现 | 本地授权 |
| Hardware Mouse | 🟢 完整实现 | 鼠标硬件探测 |
| Device Enumeration | 🟢 完整实现 | 串口设备检测 |
| LAN Blocklist | 🟢 完整实现 | 局域网设备扫描 |
| Auto-start | 🟢 完整实现 | 开机自启 |
| Aim Trace | 🟡 部分实现 | 瞄准轨迹记录（Web 端实现） |
| Prediction | 🔵 规划中 | 目标运动预测（核心有 AlphaBeta 滤波，未完整接入） |
| Motion Training | 🟢 已实现 | TTBOX 本地个人移动曲线训练、模型生成与启用 |
| Auto Calibration | 🟡 部分实现 | TTBOX 行为级自动标定：分轴采样、稳健拟合、状态驱动；真实目标场景待验收 |
| Remote Sync | 🔵 规划中 | 远程模型同步 |
| Theme System | ⚪ 保留 | 主题系统 |
| Update Manager | 🔵 规划中 | 系统更新 |

## 技术栈

| 组件 | 技术 |
|------|------|
| 核心语言 | C++17 |
| AI 推理 | RKNN（Rockchip NPU API） |
| 硬件缩放 | RGA（Rockchip Graphics Accelerator） |
| 视频采集 | V4L2（Video for Linux 2） |
| 零拷贝 | DMA-BUF |
| 进程通信 | Unix Domain Socket |
| Web 后端 | Python Flask |
| 平台管理 | Python |
| 前端 | JavaScript（桥接式 Web UI） |
| 构建系统 | CMake |
| 系统服务 | systemd |

## 核心能力具体指标

| 指标 | 当前值 | 目标值 |
|------|--------|--------|
| 采集帧率 | 240fps | 240fps ✅ |
| 推理帧率 | 143fps | 143fps ✅ |
| 端到端延迟 | 10ms | 10ms ✅ |
| 采集分辨率 | 1920×1080 | 1920×1080 ✅ |
| 模型格式 | RKNN | RKNN ✅ |
| 模型输入尺寸 | 256×256 | 可配置 |
| Worker 并发 | 1~3 核 | 可配置 ✅ |
| 预览帧率 | 15fps | 15fps ✅ |

## 产品架构原则

1. **独立产品定义**：TTBOX 不是任何其他产品的兼容版本
2. **实时优先**：所有设计以最低延迟为最高优先级
3. **零拷贝**：核心链路上尽可能减少内存复制
4. **配置驱动**：行为由配置控制，不硬编码
5. **模块隔离**：各模块通过明确接口通信
6. **Web 管理**：全部功能通过 Web 控制台管理

## 历史项目定位

历史项目（YU / AIBox）仅作为以下参考：

- 产品设计参考
- 功能需求参考
- 参数默认值参考

历史项目**不参与** TTBOX 的架构定义。

TTBOX 拥有自己的：
- 产品蓝图（本文档）
- 功能矩阵
- 领域模型
- API 契约
- 核心架构
- 运行时模型