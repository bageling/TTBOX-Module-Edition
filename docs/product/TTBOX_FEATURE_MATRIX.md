# TTBOX 功能矩阵

## 状态定义

| 状态 | 含义 |
|------|------|
| 🟢 IMPLEMENTED | 完整实现，已通过真机测试 |
| 🟡 PARTIAL | 部分实现，有核心能力但缺 UI 或反向 |
| 🔵 PLANNED | TTBOX 产品规划中，有明确需求 |
| ⚪ RESERVED | 未来保留，无明确时间表 |
| 🔴 BLOCKED | 有明确依赖尚无法实现 |

## 功能矩阵

| ID | 功能 | 页面 | 状态 | Core | IPC | Gateway | UI | Spec | 备注 |
|----|------|------|------|------|-----|---------|----|------|------|
| T001 | Runtime Start/Stop | Dashboard | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 启动/停止 AI 流水线 |
| T002 | Runtime Status | Dashboard | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 运行状态/帧率/延迟 |
| T003 | Performance Metrics | Dashboard | 🟢 | ✅ | ✅ | ✅ | ✅ | — | fps/e2e/排队/温度 |
| T004 | Preview | Dashboard | 🟢 | ✅ | ✅ | ✅ | ✅ | — | MJPEG 实时预览 |
| T005 | System Resources | Dashboard | 🟢 | — | ✅ | ✅ | ✅ | — | CPU/内存/温度/存储 |
| T006 | Capture Control | Capture | 🟢 | ✅ | ✅ | ✅ | ✅ | — | HDMI 采集控制 |
| T007 | Crop Region | Capture | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 截取区域/偏移 |
| T008 | AI Detection | AI | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 置信度/IOU/类别过滤 |
| T009 | Model Management | AI | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 导入/删除/切换/激活 |
| T010 | Model Metadata | AI | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 输入尺寸/输出数/类别 |
| T011 | Model Class Names | AI | 🟢 | — | ✅ | ✅ | ✅ | — | 类别名编辑 |
| T012 | Model Concurrency | AI | 🟢 | ✅ | ✅ | ✅ | ✅ | — | NPU 并发数 1~3 |
| T013 | Target Selection | AI | 🟢 | ✅ | — | ✅ | ✅ | — | FOV/选择策略 |
| T014 | Target Tracking | AI | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 跟踪目标数/跟踪 ID |
| T015 | PID Control | Control | 🟢 | ✅ | — | ✅ | ✅ | — | kp/ki/kd 参数 |
| T016 | Mouse Output | Control | 🟢 | ✅ | — | ✅ | ✅ | — | 鼠标输出模式 |
| T017 | Hotkey Config | Control | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 热键绑定 |
| T018 | HID Output | Control | 🟢 | ✅ | — | — | — | — | 核心实现，Web 通过硬件面板 |
| T019 | Profiles | Profiles | 🟢 | — | — | ✅ | ✅ | — | 预设保存/加载/删除 |
| T020 | Display / EDID | Hardware | 🟢 | — | — | ✅ | ✅ | — | 显示器模式/EDID 注入 |
| T021 | Hardware Mouse | Hardware | 🟢 | — | — | ✅ | ✅ | — | 鼠标硬件信息 |
| T022 | Device Enumeration | Hardware | 🟢 | — | — | ✅ | ✅ | — | 串口设备探测 |
| T023 | Wi-Fi | Network | 🟢 | — | — | ✅ | ✅ | — | 扫描/连接/AP 模式 |
| T024 | LAN Blocklist | Network | 🟢 | — | — | ✅ | ✅ | — | 局域网设备扫描 |
| T025 | Auto-start | System | 🟢 | — | — | ✅ | ✅ | — | 开机自启 (systemd) |
| T026 | Hostname | System | 🟢 | — | — | ✅ | ✅ | — | 主机名修改 |
| T027 | Storage | System | 🟢 | — | — | ✅ | ✅ | — | 存储信息 |
| T028 | Reboot/Shutdown | System | 🟢 | — | — | ✅ | ✅ | — | 重启/关机 |
| T029 | License | System | 🟢 | ✅ | ✅ | ✅ | ✅ | — | 本地授权 |
| T030 | Aim Trace | Diagnostics | 🟡 | ✅ | ✅ | ✅ | ⏳ | — | 瞄准轨迹记录（Web 采样） |
| T031 | Prediction | AI | 🔵 | — | — | — | — | prediction | 目标运动预测（核心有 AlphaBeta 滤波） |
| T032 | Motion Training | Future | 🔵 | — | — | — | — | motion-training | 运动轨迹学习与训练 |
| T033 | Auto Calibration | Future | 🔵 | — | — | — | — | calibration | 自动标定 |
| T034 | Remote Sync | Future | 🔵 | — | — | — | — | remote-sync | 远程模型同步 |
| T035 | Update Manager | System | 🔵 | — | — | — | — | update | 系统更新管理 |
| T036 | Theme System | Future | ⚪ | — | — | — | — | — | 主题更换 |
| T037 | Announcement | Future | ⚪ | — | — | — | — | — | 系统公告 |
| T038 | Hailo Acceleration | Future | ⚪ | — | — | — | — | — | Hailo-8 加速（RK3588 无此硬件） |

## 统计

| 状态 | 数量 |
|------|------|
| 🟢 IMPLEMENTED | 29 |
| 🟡 PARTIAL | 1 |
| 🔵 PLANNED | 5 |
| ⚪ RESERVED | 3 |
| 🔴 BLOCKED | 0 |
| **总计** | **38** |

## 覆盖模块

| 模块 | 功能数 | 已实现 |
|------|--------|--------|
| Dashboard | 5 | 5 🟢 |
| Capture | 2 | 2 🟢 |
| AI | 7 | 6 🟢 + 1 🟡 |
| Control | 4 | 4 🟢 |
| Profiles | 1 | 1 🟢 |
| Hardware | 3 | 3 🟢 |
| Network | 2 | 2 🟢 |
| System | 6 | 5 🟢 + 1 🔵 |
| Diagnostics | 1 | 1 🟡 |
| Future | 7 | 0 🔵/⚪ |