# TTBOX 架构重构报告

> 阶段：第一阶段——架构清理与生产链盘点
> 工作区：`C:/Users/Administrator/Desktop/TTBOX`
> 盘点依据：当前工作区源码、配置、服务文件、脚本与构建配置
> 证据等级：已读取的源码为事实；未在 RK3588 板端执行的内容标记为“板端未验证”

## 一、结论摘要

当前仓库已经存在一条“意图上的”四阶段主链，但生产实现仍有明显分叉：

```text
正式入口 main.cpp
  → Application
  → CoreRuntime
  → V4L2Capture
  → WorkerPool / InferenceWorker
  → RgaProcessor 或 CPU Direct
  → RKNNEngine
  → Decoder / DecodeNMS / GeometryFilter
  → AimTargetMailbox
  → AimThread（TargetSelector + CoordinateTransform + PID）
  → IHidOutput / OutputBackend
  → LocalHidBackend 或 UsbProxyBackend
```

同时存在第二条硬件验证入口：

```text
hardware_runner_main.cpp
  → HardwareRunner
  → V4L2Capture
  → WorkerPool
  → ModelAdapter / Decoder
  → AimThread
  → Null / Trace / FIFO / /dev/hidg1
```

还存在一套旧的部署包装路径：

```text
ttbox-infer.service
  → ttbox-infer.sh
  → /opt/ttbox/runtime/test_worker_hw
  → active_model.txt / models/current / infer.json
```

这三者没有形成唯一生产入口。第一阶段应先冻结硬件验证入口和旧推理服务，生产运行只认 `main.cpp → Application → CoreRuntime`，然后将硬件验证能力改为调用同一 Runtime 的显式验收模式。

## 二、当前真实架构

### 1. 进程入口

| 文件 | 角色 | 当前状态 | 结论 |
|---|---|---|---|
| `core/src/main.cpp:37` | C++ Core 主入口 | 已实现 | **唯一候选生产入口**。注册信号、初始化 Application、进入事件循环 |
| `core/tools/hardware_runner_main.cpp:21` | 硬件链路测试/验收入口 | 已实现、可独立启动 | **非生产入口**。拥有第二套 Runner 生命周期和多种输出选项 |
| `platform/supervisor/board_runner.py:13` | Python systemd 编排入口 | 已实现 | 管理 `ttbox-core`，不应承载 AI 主链 |
| `scripts/ttbox_web.py:1` | Flask Web 入口 | 已实现 | 管理面/旁路，通过 IPC 访问 Core |
| `scripts/ttbox_gateway.py:17` | Python Web 网关入口 | 已实现 | 另一套 Web API/IPC 转译入口 |
| `ttbox-hid-bridge.c:37` | HID 桥接入口 | 已实现 | 设备桥接旁路，不属于 AI Runtime |

### 2. 正式 C++ Runtime

`core/src/app/Application.cpp:339-349` 创建并初始化 `CoreRuntime`；`core/src/app/Application.cpp:480-520` 自动启动并重试 `CoreRuntime`。`core/src/runtime/CoreRuntime.cpp:18-39` 创建并配置 `V4L2Capture`、`WorkerPool`、`AimTargetMailbox`；`CoreRuntime.cpp:41-109` 启动顺序为采集→Worker→物理鼠标读取→AimThread→预览。

因此，当前真正接近生产的 C++ 主链是：

```text
core/src/main.cpp
  → core/src/app/Application.cpp
  → core/src/runtime/CoreRuntime.cpp
  → core/src/capture/V4L2Capture.cpp
  → core/src/rknn/WorkerPool.cpp
  → core/src/rknn/RKNNEngine.cpp + core/src/rknn/DecodeNMS.cpp
  → core/src/aim/AimThread.cpp
  → core/src/output/OutputBackend.cpp / AiboxHidOutput.cpp / FifoHidOutput.cpp
```

### 3. 视频采集

`core/src/capture/V4L2Capture.cpp:157-345`：

- 打开设备并执行 `VIDIOC_QUERYCAP`。
- 当前明确要求 `V4L2_CAP_VIDEO_CAPTURE_MPLANE` 与 `V4L2_CAP_STREAMING`。
- 通过 `VIDIOC_G_FMT`读取实际宽高、像素格式、plane、stride、sizeimage。
- 使用 MMAP、`VIDIOC_QUERYBUF`、`mmap`、`VIDIOC_EXPBUF` 导出 DMA-BUF。
- 通过 `LatestFrame` 覆盖式发布最新帧。
- 当前源码搜索未发现 `VIDIOC_G_SELECTION`、`VIDIOC_S_SELECTION`、`V4L2_SEL_*` 或硬件 Selection/Crop 调用。

**已确认**：DMA-BUF 导出与 stride/分辨率记录存在。

**未确认**：`rk_hdmirx` 是否在目标板驱动层支持 Selection/Crop。需要在 RK3588 上使用 `v4l2-ctl --list-formats-ext`、`--all`、Selection ioctl 实测；当前代码没有主动探测 Selection 能力。

### 4. 目标识别

`core/src/rknn/WorkerPool.cpp:65-166` 每个 Worker 独立创建：

- `RKNNEngine`
- `RgaProcessor`
- Decoder（优先 `ModelAdapter`，否则 `DecoderImpl`）
- FP16 转换缓存与原生输出缓存

`WorkerPool.cpp:237-305` 当前预处理存在两条路径：

1. `cpu_va != nullptr` 且 RuntimeProfile 有 capture 尺寸时，CPU 按行拷贝/最近邻缩放 ROI。
2. 否则调用 `RgaProcessor::process`。

`WorkerPool.cpp:306` 之后依据模型实际 input type 进行 UINT8/INT8/FP16 分流，随后取得 RKNN 输出并交给 Decoder。`AimTargetMailbox` 是识别层与坐标层之间的边界。

### 5. 坐标计算

`core/src/aim/AimThread.cpp:34-190` 同时承担：

- 目标选择：`TargetSelector`
- 瞄准点计算：`AimPointProfile`
- 参考点与误差：`CoordinateTransform`
- PID 更新、预测/平滑参数、余数累积
- 热键门控
- 输出动作构造
- 调用 `output_->send`

因此算法逻辑总体存在，但职责仍集中在 `AimThread`，还没有独立的 `CoordinateCalculator` 对外只输出 `mouse_dx/mouse_dy`。

### 6. 鼠标输出

`core/src/output/IHidOutput.hpp` 是当前动作接口。`OutputBackend.cpp:121-125` 将 `OutputAction` 转发到后端；后端可为：

- `LocalHidBackend`：本地 HID gadget
- `UsbProxyBackend`：Unix socket USB proxy
- 旧兼容实现 `AiboxHidOutput`
- `FifoHidOutput`
- `TraceHidOutput` / `NullHidOutput`

**当前重构阶段风险**：`config/default.json:59-61` 默认 `output_backend=usb_proxy`，已将 `output_enabled` 修改为 `false`。真实输出仅作为显式板端验收选项。

## 三、文件归类

### ① 视频采集

- `core/src/capture/V4L2Capture.hpp/.cpp`
- `core/src/capture/DmaBuf.hpp/.cpp`
- `core/src/rga/RgaProcessor.hpp/.cpp`（硬件预处理，最终归入采集→预处理边界）
- `core/src/preview/PreviewModule.hpp/.cpp`（旁路预览，不进入识别主链）
- `core/tests/test_latestframe.cpp`
- `core/tests/test_capture_hw.cpp`
- `core/tests/test_rga*.cpp`
- `scripts/edid/*`
- `scripts/a9_hw_survey.sh`

### ② 目标识别

- `core/src/model/Decoder.hpp`
- `core/src/model/ModelAdapter.hpp/.cpp`
- `core/src/model/ModelMetadata.hpp`
- `core/src/model/ModelRegistry.hpp/.cpp`
- `core/src/model/ModelManagement.hpp/.cpp`
- `core/src/rknn/RKNNEngine.hpp/.cpp`
- `core/src/rknn/WorkerPool.hpp/.cpp`
- `core/src/rknn/DecodeNMS.hpp/.cpp`
- `core/src/rknn/DetectionGeometryFilter.hpp/.cpp`
- `core/src/rknn/NpuMonitor.hpp/.cpp`
- `core/third_party/rknn/rknn_api.h`
- `core/tests/test_decode*.cpp`
- `core/tests/test_model*.cpp`
- `core/tests/test_detection_geometry_filter.cpp`

### ③ 坐标计算

- `core/src/aim/AimThread.hpp/.cpp`（当前混合 owner，后续拆分）
- `core/src/aim/Pid1Controller.hpp`
- `core/src/mouse/TargetSelector.hpp/.cpp`
- `core/src/mouse/AimTracker.hpp/.cpp`
- `core/src/mouse/AimPointProfile.hpp/.cpp`
- `core/src/mouse/CoordinateTransform.hpp/.cpp`
- `core/src/mouse/AimStateMachine.hpp/.cpp`
- `core/src/mouse/PersonalMotion.hpp/.cpp`
- `core/src/mouse/FovAngle.hpp`
- `core/src/mouse/MouseTypes.hpp`
- `core/src/pipeline/AimTargetMailbox.hpp`
- `core/src/pipeline/AimTargetTask.hpp`
- `core/tests/test_coordinate_transform.cpp`
- `core/tests/test_mouse.cpp`
- `core/tests/test_pid1.cpp`
- `core/tests/test_selector*.cpp`
- `core/tests/test_aim*.cpp`

### ④ 鼠标输出

- `core/src/output/IHidOutput.hpp`
- `core/src/output/OutputBackend.hpp/.cpp`
- `core/src/output/LocalHidBackend.hpp/.cpp`
- `core/src/output/MouseControlClient.hpp/.cpp`
- `core/src/output/FifoHidOutput.hpp/.cpp`
- `core/src/output/AiboxHidOutput.hpp/.cpp`
- `core/src/output/TraceHidOutput.hpp`
- `core/src/hid/*`
- `core/src/input/PhysicalMouseReader.hpp/.cpp`（输入门控，非输出）
- `ttbox-hid-bridge.c`
- `scripts/hid-gadget.sh`
- `core/tools/ttbox_hid_*`

### ⑤ Web / 管理 / 旁路

- `scripts/ttbox_web.py`
- `scripts/ttbox_gateway.py`
- `core/tools/web/ttbox_web.py`
- `core/src/ipc/IpcServer.hpp/.cpp`
- `platform/*`
- `release/*`
- `yu-backend/*`
- `web/*`
- `scripts/*` 中部署、调试、EDID、授权和管理脚本
- `core/src/auth/*`
- `core/src/model/ModelRegistry*` 的仓库管理部分

## 四、重复与分叉点

### 1. Runtime 启动入口重复

- `core/src/main.cpp → Application → CoreRuntime`：正式候选入口。
- `core/tools/hardware_runner_main.cpp → HardwareRunner`：第二套 Capture/Worker/Aim 生命周期。
- `core/tools/web/ttbox-infer.sh → test_worker_hw`：旧推理服务，参数和二进制名与当前 CMake 生产目标不一致。
- `platform/supervisor/board_runner.py`：Python 只应监督唯一 C++ 入口，当前服务名与部署文件存在多套约定。

**处理**：保留 `main.cpp` 为生产入口；`HardwareRunner` 改成测试/验收适配器；旧 `ttbox-infer.service` 与 `ttbox-infer.sh` 标记冻结，不进入默认部署。

### 2. Capture/预处理路径重复

- `V4L2Capture` 只输出完整驱动格式和 DMA-BUF。
- `WorkerPool` CPU Direct 自己计算 ROI、stride、缩放。
- `WorkerPool` 同时调用 `RgaProcessor` 做 ROI/缩放。
- `PreviewModule` 又读取 RuntimeProfile capture 尺寸处理预览。

**处理**：第一阶段先建立统一 `detect_size` 语义；在板端确认 Selection 能力前，唯一 Preprocess owner 暂定 RGA。CPU Direct 仅保留为显式测试/无 RGA 回退，不得与 RGA 同时作为默认生产路径。

### 3. 模型配置来源重复

目前至少有：

- `config/default.json`：`model_label`、`model_input_width`、`model_input_height`。
- `core/src/app/Application.cpp:119-127`：`model_path` 优先，否则由 `model_label` 拼接路径。
- `RuntimeProfile.model_id`：内存运行配置关联模型。
- `ModelRegistry`：`models/registry/active.json`、installed/<id>/manifest.json、metadata.json。
- `core/tools/web/ttbox-infer.sh`：`active_model.txt`、`models/current`、`infer.json` 三路回退。
- `scripts/ttbox_web.py`、`scripts/ttbox_gateway.py`：Web 参数翻译中的 `model_id`。

**处理**：唯一权威应为 `ModelRegistry::active_model()` + 对应 installed manifest/metadata；Runtime 只从 ActiveModel 解析模型路径、输入尺寸、输出描述和解码类型。`model_label`、`model_input_*` 仅保留兼容读取并标记迁移；`active_model.txt`、`infer.json` 不再作为生产真相。

### 4. 输出实现重复

- `AiboxHidOutput`
- `OutputBackend + LocalHidBackend`
- `OutputBackend + UsbProxyBackend`
- `FifoHidOutput`
- `HidForwarder/HidRuntime`
- `ttbox-hid-bridge.c`

**处理**：统一为 `MouseOutput`/`IHidOutput` 接口；生产主链只保留一个后端选择点，默认 `Null/Trace` 关闭注入；本地 HID、USB Proxy、FIFO 作为显式适配器，不在坐标层感知。

### 5. 源码副本重复

`yu-backend/yu-core-src/core` 与根目录 `core` 存在大量字节级完全相同文件，包括测试、模型、采集、坐标、输出和 Web 工具。

**处理**：根目录 `core` 是唯一施工与构建源；`yu-backend/yu-core-src` 作为历史/打包输入冻结，不纳入根 CMake 构建，不在本阶段机械删除，后续单独制定清理迁移步骤。

## 五、保留、合并、删除、冻结

### 保留

- `core/src/main.cpp`、`Application`、`CoreRuntime`：生产生命周期骨架。
- `V4L2Capture`、`DmaBuf`：采集基础。
- `RgaProcessor`：板端硬件预处理候选实现。
- `RKNNEngine`、`ModelAdapter`、`Decoder`、`DecodeNMS`、NMS/GeometryFilter：识别基础。
- `TargetSelector`、`CoordinateTransform`、PID/Tracker：坐标算法基础。
- `IHidOutput` 与后端适配器：输出抽象基础，但需要收敛单写者。
- `IpcServer`、Web：管理面，通过 IPC 访问 Core。

### 合并

- `HardwareRunner` 的硬件初始化/验收能力合并到唯一 Runtime 的测试接口。
- `AiboxHidOutput`、`LocalHidBackend`、`UsbProxyBackend` 的发送门控合并到唯一输出抽象。
- `ModelRegistry`、`RuntimeProfile`、`ModelAdapter` 的模型元数据/活动模型关系合并为 ActiveModel 配置链。
- Worker 中 CPU Direct 与 RGA 的预处理接口合并为唯一 `Preprocess` 抽象，生产默认只选一个路径。

### 删除候选

以下内容先列为候选，不在第一阶段直接删除：

- `ttbox-infer.sh` 对 `test_worker_hw` 的旧生产包装。
- `active_model.txt`、`infer.json` 生产依赖。
- `yu-backend/yu-core-src/core` 的重复源码副本。
- 已被统一 OutputBackend 取代且无真实调用点的旧输出实现。
- 仅部署脚本引用、当前 CMake 不构建的旧测试/二进制入口。

### 暂时冻结

- `platform/*` supervisor/生命周期：只做管理，不改成新的 Runtime。
- `scripts/*` 部署、EDID、授权、调试脚本：只记录调用关系，不纳入主链重构。
- `yu-backend/*`：作为兼容/历史集成层，不作为唯一 Core 源。
- 真实 HID 注入：阶段一和阶段二保持关闭。
- RKNN 三核并发策略：先保留现有实现，等板端采集和单 Worker 链路证据完整后再调整。

## 六、统一目标架构

```text
唯一生产入口：core/src/main.cpp
  ↓
Application（生命周期、IPC、配置装载）
  ↓
CoreRuntime（唯一生产 Runtime owner）
  ↓
VideoCapture（HDMI → V4L2 → DMA-BUF/Frame）
  ↓
Preprocess（唯一 detect_size 转换点；优先硬件 Crop/RGA）
  ↓
Detector（RKNN → Decode → NMS → Detection）
  ↓
CoordinateCalculator（Selection → AimPoint → 屏幕映射 → mouse_dx/mouse_dy）
  ↓
MouseOutput（Null/Trace 默认；HID/USB Proxy 显式启用）
```

### detect_size 规则

- 统一概念名为 `detect_size`。
- `detect_size` 表示 Detector 实际接收的工作尺寸；允许 192/256/320/416/640，但由 ActiveModel 的真实输入元数据约束。
- `capture crop` 表示采集/预处理的空间区域，不再继续扩展为 `roi_size`、`worker_crop_size` 等并行概念。
- 如果模型输入尺寸与采集 crop 尺寸不同，转换只发生在唯一 Preprocess 层，并记录源区域、目标尺寸、stride、缩放方式。
- 当前代码尚未有 `detect_size` 字段；现有 `capture.width/height` 与 `model_input_width/height` 是两套概念，属于第一阶段明确的结构缺口。

## 七、第一阶段实施顺序

1. 已完成本报告的源码盘点与调用链归类。
2. 将 `main.cpp → Application → CoreRuntime` 明确标为唯一生产链。
3. 为 `RuntimeProfile`/ActiveModel 增加统一 `detect_size` 语义，先做兼容映射，不立即删除旧字段。
4. 将 Worker CPU Direct 与 RGA 收敛到唯一 Preprocess owner，生产默认关闭 CPU Direct。
5. 为 Capture 增加板端 Selection 能力探测记录接口；在实际 RK3588 上确认后再决定驱动 crop 或 RGA crop。
6. 将 `HardwareRunner` 改为测试专用薄适配器，避免第二套生产生命周期。
7. 将输出默认值改为关闭，并确保 Null/Trace 在 host 测试中不访问真实设备。
8. 收敛模型来源到 ModelRegistry ActiveModel；旧文件只读兼容并发出迁移日志。
9. 清理/冻结旧 systemd 推理服务与重复源码副本，待全量引用审计后再删除。

## 八、验收问题的当前答案

| 问题 | 当前答案 | 状态 |
|---|---|---|
| 视频从哪里进来？ | `V4L2Capture` 打开 `/dev/video0`，MMAP + DMA-BUF，`LatestFrame` 发布 | 代码已确认；板端未验证 |
| 检测尺寸在哪里确定？ | 运行时由 `model_input_width/height` 与 RKNN 实际输入共同决定；ROI 在 `RuntimeProfile.capture` | **概念分裂** |
| YOLO/RKNN 从哪里调用？ | `WorkerPool.cpp` 创建 `RKNNEngine` 并调用 set_input/run/get_outputs | 代码已确认 |
| Detection 在哪里产生？ | Worker 解码器（`Decoder`/`DecodeNMS`）产生 `DetectionBox` 后发布 `AimTargetTask` | 代码已确认 |
| 坐标在哪里计算？ | `AimThread.cpp` 内部调用 `TargetSelector`、`AimPointProfile`、`CoordinateTransform`、PID | 代码已确认；职责混合 |
| mouse_dx/mouse_dy 在哪里产生？ | `AimThread.cpp` 计算 `move_x/move_y`，封装 `OutputAction` | 代码已确认 |
| 鼠标最终从哪里输出？ | `OutputBackend` 选择 Local HID/USB Proxy，或旧 Aibox/FIFO 后端 | 多实现 |
| 唯一生产入口是什么？ | 设计上是 `core/src/main.cpp`；服务文件存在冲突，需清理确认 | 当前候选 |
| 唯一模型真相是什么？ | 当前没有唯一真相；ModelRegistry 与旧文件并存 | 未完成 |
| 哪些旧代码退出生产链？ | `test_worker_hw` 包装、`active_model.txt`/`infer.json` 回退、重复 `yu-backend/yu-core-src`、未接线旧输出 | 待实施 |

## 九、当前验证结果

- Windows CMake 配置：**通过**。已生成 `core/build`，MSVC 2022，C++ 工具链可用。
- 完整 `pytest -q`：**失败于收集阶段**。原因包括仓库目录 `platform` 与 Python 标准库同名导致 `platform.tests` 导入冲突，以及 `yu-backend/ipc_test.py` 在 Windows 缺少 `socket.AF_UNIX`。这属于测试配置/平台兼容问题，不代表核心逻辑测试结果。
- C++ 编译：**通过**，命令为 `cmake --build core/build --config Release --parallel 4`，退出码 0。
- CTest：**14/14 通过**，命令为 `ctest --test-dir core/build -C Release --output-on-failure`。
- RK3588 V4L2/RGA/RKNN/HID：当前工作机未具备目标板设备，**板端未验证**。
- 当前工作区不是 Git 仓库（目录来自压缩包拉取，缺少 `.git`），无法在本地执行 Git diff/提交回滚；后续如需增量施工，先恢复版本控制底板或建立独立备份标记。

## 十、明确边界

本文件只完成架构盘点、分类、重复点识别与实施计划；没有删除生产代码，没有启用真实鼠标注入，没有宣称已完成 RK3588 硬件验证。
