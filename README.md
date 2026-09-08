# TTBOX-模块版（TTBOX-Module-Edition）

> 一台基于 **RK3588（OrangePi 5 Plus）** 的 AI 视觉盒子：
> 用 HDMI 线"看"电脑屏幕，AI 识别目标，再通过自研 usbproxy 把鼠标指令注入电脑，
> 完成 **AI 看画面 → 认目标 → 动鼠标** 的全闭环。

```
电脑画面 ──HDMI──▶ RK3588 盒子（AI 核心）
                     │
                     ▼
        V4L2 采集 → RGA 缩放 → RKNN NPU 推理 → DecodeNMS 解码
                     │
                     ▼
        TargetSelector 选目标 → PID 算位移 → AimThread 产出指令
                     │
                     ▼
        MouseControlClient → usbproxy → HID → Windows 鼠标真实移动
```

**它解决什么问题**：用 AI 代替人眼，快速发现画面里的目标并自动瞄准。
**它不做什么**：不读内存、不碰游戏数据，只"看"画面（纯外部视觉）。

---

## 一、两条线路（务必分清）

| 线路 | 方向 | 作用 |
|---|---|---|
| **HDMI 输入线** | 电脑 → 盒子 | 盒子的"眼睛"：看电脑画面 |
| **USB 输出线** | 盒子 → 电脑 | 盒子的"手"：模拟鼠标移动 |

电脑画面从 HDMI 进来，AI 处理后，鼠标指令从 USB 出去。两条线互不干扰。

---

## 二、硬件与运行环境

| 组件 | 说明 |
|---|---|
| AI 盒子 | OrangePi 5 Plus（RK3588 芯片，3 核 NPU） |
| 操作系统 | Armbian Linux（板端） |
| 视频输入 | HDMI RX（2560×1440 电脑画面） |
| AI 核心 | C++ 程序 `ttbox_core_main`（systemd 托管） |
| 模型 | `.rknn` 格式（瑞芯微专用），YOLO 系列，默认 `jwdl_sjzv11` |
| Web 控制台 | Python 服务（`ttbox-web`，端口 8000） |
| 鼠标注入 | 自研 `usbproxy`（raw-gadget + libusb，模拟物理鼠标） |
| Windows 宿主机 | 开发/编译/单元测试用（ONNX CPU 后端） |

**板端服务（systemd）**：

| 服务 | 作用 |
|---|---|
| `ttbox-core` | AI 核心（采集→推理→瞄准→输出指令） |
| `ttbox-preview` | 预览图服务（8001 端口，Web 显示画面） |
| `ttbox-web` | Web 控制台（8000 端口） |
| `ttbox-usbproxy` | 鼠标注入代理（USB gadget） |
| `ttbox-edid` | HDMI 输入 EDID 身份 |

---

## 三、仓库目录结构

```
TTBOX-Module-Edition/
├── core/              ★ C++ AI 核心（采集/推理/瞄准/输出，真源码）
│   ├── src/               核心源码（21 个模块，见下表）
│   ├── tests/             C++ 单元测试 + 板端辅助脚本
│   ├── third_party/       rknn / onnxruntime 头文件与库
│   └── CMakeLists.txt     构建脚本（ONNX 开关见"如何编译"）
├── usbproxy/          ★ 自研鼠标注入代理（raw-gadget + libusb）
├── framework/         Python 框架（插件管理/配置/服务/安全）
├── plugins/           功能插件（web/preview/model/monitor/fan/wifi/...）
├── platform/          平台层（systemd/运行时/模型/健康/更新）
├── modules/           ★ 模块化语义视图（按数据流 01-09 讲解，小白从这里看）
├── models/            模型目录（installed/ 已安装模型）
├── config/            配置文件模板
├── scripts/           运维脚本 + Web 主程序 ttbox_web.py
├── tools/             工具（模型转换 convert_onnx_to_rknn.py）
├── ttbox_motion/      运动控制（校准/训练）
├── docs/              ★ 文档中心（架构/小白教程/验证/模型/规划，全中文）
├── tests/             集成测试脚本
└── deploy/            部署相关
```

### core/src/ 核心模块（每个目录干什么）

| 目录 | 职责 | 关键文件 |
|---|---|---|
| `capture/` | HDMI 画面采集（V4L2 + DMA-BUF） | V4L2Capture.cpp |
| `rga/` | 图像缩放/裁剪（RGA 硬件加速） | RgaProcessor.cpp |
| `rknn/` | RKNN 推理引擎 + 解码 + 预处理 | RKNNEngine.cpp / DecodeNMS.cpp / Preprocess.cpp / WorkerPool.cpp / Detector.cpp |
| `model/` | 模型库（注册/管理/适配/元数据） | ModelRegistry.cpp / ModelAdapter.cpp / ModelManagement.cpp |
| `mouse/` | 目标选择/坐标变换/瞄准跟踪 | TargetSelector.cpp / CoordinateTransform.cpp / AimTracker.cpp |
| `aim/` | 瞄准线程 + PID 控制器 | AimThread.cpp / Pid1Controller.hpp |
| `controller/` | 控制器接口 + PID 兼容层 | PidController.cpp |
| `output/` | 输出后端（usbproxy 客户端等） | OutputBackend.cpp / MouseControlClient.cpp |
| `hid/` | HID 包管理（配置/清单/运行时） | HidRuntime.cpp / HidParser.cpp |
| `input/` | 物理鼠标读取（热键来源） | PhysicalMouseReader.cpp |
| `ipc/` | 进程间通信（Web ↔ 核心） | IpcServer.cpp |
| `app/` | 应用入口与装配 | Application.cpp |
| `runtime/` | 运行时装配（硬件/核心） | CoreRuntime.cpp / HardwareRunner.cpp |
| `preview/` | 预览图生成（OpenCV 画框 + JPEG） | PreviewModule.cpp |
| `common/` | 公共工具（日志/JSON/指标/CPU亲和） | Logger.cpp / Json.cpp / Metrics.hpp |
| `config/` | 配置管理 | ConfigManager.cpp |
| `auth/` | 授权/许可证（可禁用） | AiboxLicenseClient.cpp / DisabledAuth.cpp |
| `detector/` | 检测器接口 + 高性能 RKNN 引擎 | highperf/HighPerfRknnEngine.cpp |
| `pipeline/` | 瞄准目标邮箱/任务（线程间传递） | AimTargetMailbox.hpp / Target.hpp |
| `bench/` | NPU 基准测试 | npu_bench.cpp |

---

## 四、完整数据链路（数据从哪里来到哪里去）

### 1. 视频采集链路（眼睛）

```
电脑画面 → HDMI 线 → RK3588 HDMI RX 芯片 → /dev/video0
  → V4L2Capture（DMA-BUF 零拷贝采集）
  → RgaProcessor（RGA 硬件缩放/裁剪，固定中心 640×640 给模型）
  → RKNNEngine（NPU 推理）
  → DecodeNMS（YOLO 输出解码 → 检测框）
```

### 2. 瞄准链路（大脑）

```
Detection（检测框列表）
  → DetectionGeometryFilter（过滤无效框）
  → TargetSelector（选目标：FOV 内最靠近准星的）
  → CoordinateTransform（坐标 → 误差值）
  → Pid1Controller（PID 算位移 dx/dy）
  → AimThread（门控/热键判断）
  → MouseCommand
```

### 3. 输出链路（手）

```
MouseCommand → OutputBackend → MouseControlClient（0x4F50 协议）
  → usbproxy cmd.sock
  → HID report → Raw Gadget → Windows 识别为鼠标 → 光标真实移动
```

### 4. 预览链路（眼睛给用户看）

```
Capture Frame → OpenCV 画检测框 → JPEG → 8001 端口 → Web 页面显示
（预览始终是原始分辨率，绝不跟随模型输入尺寸变化）
```

---

## 五、模型系统

### 模型目录规范

板端模型库根目录：`/opt/ttbox/models/`，每个模型一个独立目录：

```
/opt/ttbox/models/installed/<model_id>/
├── model.rknn        RKNN 模型文件（必选）
├── manifest.json     模型描述文件（必选，见下）
├── metadata.json     运行时元数据（自动生成）
└── validation/       验证记录（自动生成）
```

目录分类：`installed/`（已安装）、`staging/`（导入待验证）、`_incoming/`（上传中）、`quarantine/`（隔离）、`registry/`（注册记录）、`cache/`（缓存）。

### manifest.json 是什么

每个模型的"身份证"，记录：模型 ID、名称、输入尺寸、类别数、量化方式、输出格式、校验和、转换工具版本等。ModelRegistry 读取它来注册模型，校验失败就拒绝加载。

### 模型注册流程（ModelRegistry）

```
扫描模型目录 → 读 manifest → 检查文件存在 → 校验 checksum
→ 验证元数据 → 注册进 registry → 可被选中激活
```

### 当前模型

| 项 | 值 |
|---|---|
| 当前激活模型 | `jwdl_sjzv11` |
| 输入 | 256×256（NHWC，INT8 量化） |
| 类别数 | 7 |
| 输出解码 | dfl_pair_dist（成对 DFL 输出） |
| 模型家族 | YOLO 系列 |

### 模型切换流程（热切换）

```
模型 A 运行中 → 请求切换 B
→ 停止接收新 A 推理任务 → 等待 A 任务结束 → 释放 A 的 RKNN 资源
→ 加载 B → 验证 B → 清空旧 Detection / Target 缓存
→ 启动 B → 第一帧 B 检测 → 当前模型 = B
切换失败 → 回滚到 A（A 仍可恢复）
```

### 核心重启后恢复

```
Core 重启 → 读配置里的模型 ID → ModelRegistry 扫描模型库
→ 验证该模型 → 加载 → 完成第一次真实推理 → 状态 READY
```

---

## 六、瞄准控制（PID 与热键）

### PID 控制器

瞄准使用 `Pid1Controller`（core/src/aim/Pid1Controller.hpp），这是当前**唯一实际生效**的 PID 实现，参数以用户提供的 `pid1.cpp` 为权威标准：

| 参数 | X 轴 | Y 轴 | 含义 |
|---|---|---|---|
| kp（比例） | 25 | 25 | 误差越大动得越快 |
| kd（微分） | 25 | 25 | 提前刹车防过冲 |
| predict（前馈） | 3.0 | 0 | 预判目标移动（X 带，Y 不带） |
| rate | 0.3 | 0.3 | 输出速率 |
| smooth | 9900 | 9900 | 平滑/软限幅（10000 为上限） |

**死参数说明**：`ki_x/ki_y`、`aim_part`、`smooth`（单数）、`fov_range`（mouse 结构内）、拉枪曲线、持续提前量、屏蔽物理移动等字段在核心中**没有消费点**（前端已标注"未接线"），调节无效，仅保留。详细清单见 `docs/web/鼠标参数标注清单.md`。

### 三大安全门（fail-closed）

| 门 | 位置 | 作用 |
|---|---|---|
| Gate1 | AimThread | `injection_allowed`：需 `mouse.enabled` 且热键命中才注入 |
| Gate2 | OutputBackend | `gate_allows`：输出后端再验证一次 |
| Gate3 | 静态总闸 | `output_enabled`：启动时快照，改配置需重启 Core 才生效 |

默认状态全部关闭：**开机不会自动注入鼠标**，热键/开关关闭即时停止。

**注意**：板端当前为调试 pid1 参数临时开启（`output_enabled=true`、`mouse.enabled=true`），正式交付前会恢复安全默认。

### 热键逻辑

- 物理鼠标按键（如左键）通过 PhysicalMouseReader 读取 → 作为热键触发源
- 按住热键：AI 注入生效；松开热键：立即停止移动（rest 归零）
- 无按键时 `injection_allowed=false`，40 帧采样无泄漏（已验证）

---

## 七、配置系统

板端主配置：`/opt/ttbox/config/default.json`（仓库 `config/default.json` 是安全模板）。

| 配置区 | 作用 | 关键项 |
|---|---|---|
| 顶层 | 基础/模型/输出 | `model_registry_root`、`output_enabled`、`output_backend`、`model_input_width/height`、`class_filter_text` |
| `runtime_profile.capture` | 采集裁剪 | `width/height/offset_x/offset_y` |
| `runtime_profile.inference` | 推理参数 | `confidence/iou/class_filter/max_detections` |
| `runtime_profile.fov` | 视野（选目标范围） | `enabled/shape/radius/center_x/center_y` |
| `runtime_profile.mouse` | 鼠标/PID/热键 | `kp_x/kp_y/kd_x/kd_y/predict_x/predict_y/rate/smooth_x/y/output_deadzone/enabled/aim_hotkey/aim_hotkey2/aim_offset_x/y` |

**Web 保存行为**：HTTP `PUT /api/config` 深合并——只更新你改的字段，其它参数保持原样（已验证：改 kp_x 不会冲乱其它 pid1 值）。

---

## 八、Web 控制台

Web 服务（Python，`scripts/ttbox_web.py` + `plugins/web/`）通过 Unix socket `/tmp/ttbox_core.sock` 与 C++ 核心通信（IPC 协议见 `docs/ipc-protocol.md`）。

| 功能 | 说明 |
|---|---|
| 总览 | 实时状态：FPS、推理耗时、检测数、目标状态 |
| 预览 | 实时画面（8001 端口，画检测框） |
| 模型库 | 上传/导入/安装/切换/删除模型 |
| 鼠标控制 | PID 参数、热键、死区、瞄准偏移 |
| 视野设置 | FOV 形状/半径/中心 |
| 系统 | 服务状态、日志、风扇、网络 |

**主要 API**（HTTP）：

| 方法 | 路径 | 作用 |
|---|---|---|
| GET | `/api/status` | 实时状态 |
| GET/PUT | `/api/config` | 读/写配置（PUT 深合并） |
| GET/POST | `/api/models` | 模型列表/操作 |
| POST | `/api/models/activate` | 激活模型 |
| POST | `/api/models/upload` | 上传模型文件 |
| GET | `/api/preview` | 预览图 |

---

## 九、如何编译（开发者）

### Windows 宿主机（开发/单元测试，ONNX CPU 后端）

```bash
cd core
cmake -B build-win -G "MinGW Makefiles" -DTTBOX_CORE_WITH_ONNX=ON
cmake --build build-win -j4
./build-win/ttbox_core_tests.exe    # 单元测试
```

### 板端 RK3588（RKNN NPU 后端）

```bash
cd /opt/ttbox/src          # 板端源码根（CMakeLists 在顶层）
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target ttbox_core_main -j4
# 安装到 /opt/ttbox/bin/
systemctl restart ttbox-core
```

### 板端 usbproxy

```bash
cd /opt/ttbox/usbproxy && make
systemctl restart ttbox-usbproxy
```

**平台差异说明**：同一套源码，两平台构建。

- Windows：`-DTTBOX_CORE_WITH_ONNX=ON` 编译 `model/backend/`（ONNX CPU 后端），用于本地开发/回归测试
- 板端：不启用 ONNX，RKNN NPU 直接推理；CMakeLists 自动排除 `model/backend/`
- 两份源码逐文件 md5 一致（本机 `core/src` = 板端 `/opt/ttbox/src/src`）

---

## 十、测试

### 单元测试（Windows，109 个用例）

```bash
cd core && cmake --build build-win -j4 && ./build-win/ttbox_core_tests.exe
```

覆盖：PID（test_pid1）、解码/解码分发（test_decode / test_decoder_dispatch）、配置、HID、瞄准目标邮箱、瞄准线程、FOV/ROI、热键配置等。

### 板端验证

- 服务健康：`systemctl is-active ttbox-core ttbox-web ttbox-preview ttbox-usbproxy`
- 状态查询：`/opt/ttbox/bin/ipc_ping --type GET_STATUS`
- 模型库：Web 模型库页面操作 + `GET /api/models`
- 性能基线（历史实测）：Capture 141.7 FPS / Infer 43.1 FPS / E2E 66.6ms

---

## 十一、文档导航（全中文）

### 完全不懂代码？从这些开始

| 文档 | 内容 |
|---|---|
| [docs/小白教程/01-TTBOX是什么.md](docs/小白教程/01-TTBOX是什么.md) | TTBOX 是什么 |
| [docs/小白教程/02-TTBOX怎么工作.md](docs/小白教程/02-TTBOX怎么工作.md) | 整体工作流程 |
| [docs/小白教程/05-模型是什么.md](docs/小白教程/05-模型是什么.md) | 模型概念 |
| [docs/小白教程/09-如何添加模型.md](docs/小白教程/09-如何添加模型.md) | 换模型步骤 |

### 想深入了解？

| 文档 | 内容 |
|---|---|
| [docs/架构/系统总览.md](docs/架构/系统总览.md) | 系统分层全景 |
| [docs/架构/完整链路.md](docs/架构/完整链路.md) | PC 画面到鼠标指令全链路 |
| [docs/架构/核心模块.md](docs/架构/核心模块.md) | 每个模块详解 |
| [docs/web/architecture/INDEX.md](docs/web/architecture/INDEX.md) | Web 架构归档 |
| [docs/web/model/INDEX.md](docs/web/model/INDEX.md) | 模型系统归档 |
| [docs/ipc-protocol.md](docs/ipc-protocol.md) | IPC 协议 |
| [docs/web/鼠标参数标注清单.md](docs/web/鼠标参数标注清单.md) | 死参数权威清单 |
| [docs/web/PID对比.html](docs/web/PID对比.html) | pid1 vs YU PID 参数对比 |
| [modules/README.md](modules/README.md) | 模块化语义视图（01-09） |

---

## 十二、当前状态与已知事项

### 已完成并真机验证

- ✅ AI 检测闭环（真实 HDMI 画面：采集→RGA→RKNN→DecodeNMS）
- ✅ AI → HID 鼠标注入全闭环（8 场景验证：居中/左/右/上/下/热键停/高频 0 丢包）
- ✅ 模型库（staging/installed 机制、热切换、重启恢复）
- ✅ PID 参数对齐 pid1 标准（代码默认值 + 板端配置 + 前端默认值三处一致）
- ✅ Web 保存深合并（调参不冲乱其它参数）
- ✅ Windows 回归 109/109 PASS；板端服务全部 active

### 进行中

- 真机 PID 手感验证（需画面出现目标后测 kp=25 实际效果）
- 最终 kp_x/kp_y 值待真机确定
- 板端源码树完整性与本机同步核对（md5 已对齐，backend/ 为平台差异）

### 已知事项

1. **安全门当前为调试态**：板端 `output_enabled=true`、`mouse.enabled=true` 是为 PID 调试临时开启，正式交付前恢复 `false`
2. **死参数已标注不删除**：前端显示"未接线"标签，核心无消费点
3. **裸 IPC 传部分 mouse 配置会触发旧默认值覆盖**：调试工具请走 HTTP `PUT /api/config`（全量安全），不要直接发部分 SET_CONFIG
4. 板端源码根在 `/opt/ttbox/src`，编译入口是其顶层 CMakeLists；`/opt/ttbox/src/src` 与仓库 `core/src` 逐文件 md5 一致

---

## 十三、许可证

本项目以 **MIT 许可证** 开源。详见 [LICENSE](LICENSE)。

`usbproxy/` 底层使用 [raw-gadget](https://github.com/xairy/raw-gadget) 与 libusb 开源库（Apache-2.0），保留上游版权声明。

---

*TTBOX-模块版 —— 让 AI 视觉盒子简单、清晰、可维护，AI → HID 全闭环。*

