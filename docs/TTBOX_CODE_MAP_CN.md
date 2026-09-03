# TTBOX 全仓库代码地图（小白版）

## 本文档的目的

这不是技术文档。这是给**完全不懂代码的人**看的项目说明书。
每一个文件、每一行关键代码，我都会用简单的话解释它在做什么。

---

## 一、程序入口：main.cpp

### 一句话
TTBOX 的启动大门。

### 小白理解
就像你按电脑电源键开机一样，main.cpp 就是程序启动时最先跑的那个文件。

### 它做了什么
1. 注册 Ctrl+C 信号处理（点了退出键怎么优雅关机）
2. 创建 Application 对象（总管家）
3. 调用 initialize() 初始化
4. 调用 run() 开始运行
5. 调用 shutdown() 退出

### 文件位置
`core/src/main.cpp`

### 修改风险
🔴 高（但一般不需要改）

---

## 二、总管家：Application.cpp

### 一句话
TTBOX 的大脑，负责一切初始化和生命周期管理。

### 小白理解
Application 是 TTBOX 的总统。它负责：
- 读取命令行参数（启动时你给了什么指令）
- 读取配置文件（你设了什么参数）
- 初始化各个模块（采集、AI、控制、输出）
- 处理退出信号

### 关键函数

| 函数名 | 一句话 |
|--------|--------|
| `initialize()` | 初始化所有子系统 |
| `run()` | 开始运行主循环 |
| `shutdown()` | 停止所有子系统并退出 |
| `build_runtime_params()` | 读取配置，构造运行参数 |
| `handle_model_*()` | 模型管理（导入/删除/激活等） |

### 文件位置
`core/src/app/Application.cpp`

### 上游
- OS（操作系统）

### 下游
- CoreRuntime
- IpcServer
- LicenseDaemon
- ModelManagement

### 修改风险
🔴 高

---

## 三、核心运行时：CoreRuntime.cpp

### 一句话
AI 流水线的总开关。

### 小白理解
你点了"启动"按钮后，CoreRuntime 就开始干活了：
1. 打开摄像头（采集）
2. 启动 AI 推理（WorkerPool）
3. 启动目标控制（AimThread）
4. 启动预览画面（PreviewModule）

你点了"停止"后，它也按顺序关掉所有东西。

### 文件位置
`core/src/runtime/CoreRuntime.cpp`

### 上游
- Application

### 下游
- V4L2Capture（采集）
- WorkerPool（AI 推理）
- AimThread（目标控制）
- PreviewModule（预览）

### 修改风险
🔴 高

---

## 四、采集：V4L2Capture.cpp

### 一句话
从 HDMI 采集卡读取游戏画面的模块。

### 小白理解
就像你用摄像头拍照一样，这个模块从 HDMI 接口一帧一帧地读取画面。
它用 V4L2（Linux 视频驱动标准）和 DMA-BUF（零拷贝技术）来高效地获取画面数据。

### 关键概念
- **DMA-BUF**：直接把画面数据放在显卡能读的地方，不经过 CPU 复制，速度更快
- **LatestFrame**：始终保存最新的一帧，谁需要谁拿去
- **buffer**：驱动里预先分配好的内存块，用来存放视频帧

### 文件位置
`core/src/capture/V4L2Capture.cpp`

### 上游
- HDMI 输入（硬件）

### 下游
- RGA / WorkerPool

### 修改风险
🔴 高（涉及硬件驱动和实时性能）

---

## 五、硬件缩放：RgaProcessor.cpp

### 一句话
把采集到的画面缩放到 AI 模型需要的尺寸。

### 小白理解
摄像头拍到的画面是 2560×1440，但 AI 模型只能吃 256×256 的小图。
RGA 硬件模块负责把大图裁剪+缩放到小图，而且不占用 CPU 时间。

### 关键概念
- **center_crop**：从画面中心截取一块（默认）
- **ROI**：Region of Interest，用户指定的截取区域
- **imcrop / imresize**：RGA 硬件提供的裁剪和缩放功能

### 文件位置
`core/src/rga/RgaProcessor.cpp`

### 上游
- V4L2Capture

### 下游
- WorkerPool / RKNNEngine

### 修改风险
🟡 中

---

## 六、NPU 推理：RKNNEngine.cpp

### 一句话
把图片喂给 AI 模型，让 NPU 算出结果。

### 小白理解
NPU（神经网络处理器）是专门用来跑 AI 的芯片。
这个模块负责：
1. 把缩放后的图片数据送进 NPU
2. 让 NPU 执行 AI 模型计算
3. 把计算结果取出来

### 关键概念
- **core_mask**：用 NPU 的哪个核心（0=自动，1/2/4=指定核心）
- **rknn_inputs_set / rknn_run / rknn_outputs_get**：NPU 推理三步走
- **pass_through**：零拷贝模式，减少内存复制

### 文件位置
`core/src/rknn/RKNNEngine.cpp`

### 上游
- RGA

### 下游
- DecodeNMS

### 修改风险
🔴 高（NPU 硬件相关，容易崩）

---

## 七、推理线程池：WorkerPool.cpp

### 一句话
管理多个 AI 推理线程，让它们协同工作。

### 小白理解
TTBOX 有 3 个 NPU 核心，每个核心可以同时推理一帧。
WorkerPool 就是管理这 3 个工人的监工：
- 每帧画面来了，分配给一个工人处理
- 工人处理完，把结果交上来
- 3 个工人轮流干，流水线不停

### 关键概念
- **worker_cores**：每个工人绑定的 NPU 核心（如 1,2,4）
- **seq % N == id**：帧按序号分给对应工人，保证不重复
- **CPU 直拷**：直接从采集内存里拷贝画面数据，不经过 RGA 硬件

### 文件位置
`core/src/rknn/WorkerPool.cpp`

### 上游
- V4L2Capture

### 下游
- RKNNEngine
- DecodeNMS
- AimTargetMailbox

### 修改风险
🔴 高

---

## 八、AI 结果解析：DecodeNMS.cpp

### 一句话
把 AI 算出来的原始数据解析成目标框。

### 小白理解
AI 模型输出的不是"这里有个敌人"，而是一堆数字。
DecodeNMS 负责把这堆数字翻译成：
- 目标框（x1, y1, x2, y2）
- 置信度（多大概率是目标）
- 类别（是人还是其他）

### 关键概念
- **DFL（Distribution Focal Loss）**：YOLO v8/v11 模型的输出格式
- **NMS（Non-Maximum Suppression）**：去掉重复检测框
- **几何过滤（Geometry Filter）**：去掉不合理的目标（如太小、太偏）

### 文件位置
`core/src/rknn/DecodeNMS.cpp`

### 上游
- RKNNEngine

### 下游
- TargetSelector

### 修改风险
🟡 中（换模型类型时可能需要调整）

---

## 九、目标选择：TargetSelector.cpp

### 一句话
从多个检测结果中选一个最好的目标。

### 小白理解
AI 可能检测出 5 个目标，但一次只能瞄准一个。
TargetSelector 根据规则选一个：
- 离瞄准点最近的目标
- 连续出现多帧的目标（更稳定）
- 跟踪已有目标（不会突然跳走）

### 文件位置
`core/src/mouse/TargetSelector.cpp`

### 上游
- DecodeNMS

### 下游
- AimThread

### 修改风险
🟡 中

---

## 十、目标控制：AimThread.cpp

### 一句话
锁定目标后，计算鼠标怎么移动才能追上它。

### 小白理解
选好目标后，AimThread 负责：
1. 跟踪目标的移动（目标会跑，你得跟上）
2. 预测目标的下一个位置（提前量）
3. 计算鼠标移动量（PID 控制算法）
4. 把移动指令发给鼠标

### 关键概念
- **PID 控制**：比例-积分-微分控制，一种经典的自动控制算法
- **预测**：根据目标的历史轨迹预测它下一步的位置
- **热键门控**：只有按住鼠标右键时才启动瞄准

### 文件位置
`core/src/aim/AimThread.cpp`

### 上游
- TargetSelector

### 下游
- OutputBackend

### 修改风险
🔴 高（直接影响瞄准行为）

---

## 十一、PID 控制：Pid1Controller.hpp

### 一句话
计算鼠标移动量的核心算法。

### 小白理解
PID 控制器的目标是让鼠标准星和目标的偏差缩小到 0。
它用三个参数来控制：
- **P（比例）**：偏差越大，移动越快
- **I（积分）**：长期偏差，慢慢纠正
- **D（微分）**：防止超调，刹车作用

### 文件位置
`core/src/aim/Pid1Controller.hpp`

### 修改风险
🟡 中（调参影响手感，但不会崩）

---

## 十二、鼠标输出：OutputBackend.cpp

### 一句话
把瞄准指令转换成真实的鼠标移动。

### 小白理解
AimThread 算出了"应该往右移动 10 个像素"，
OutputBackend 负责把这个指令发给 HID 设备，
HID 设备再通过 USB 线告诉电脑："鼠标向右动 10 个像素"。

### 文件位置
`core/src/output/OutputBackend.cpp`

### 上游
- AimThread

### 下游
- HID 桥接

### 修改风险
🟡 中

---

## 十三、进程间通信：IpcServer.cpp

### 一句话
Web 后端和 C++ 核心之间的电话线。

### 小白理解
Web 页面（浏览器）和 C++ 核心是两个不同的程序。
它们通过 Unix Socket（一种进程间通信方式）来沟通：
- Web 发指令："启动 AI"
- Web 查状态："当前帧率多少？"
- Web 改配置："置信度改到 0.5"

### 文件位置
`core/src/ipc/IpcServer.cpp`

### 修改风险
🟡 中

---

## 十四、配置文件：ConfigManager.cpp

### 一句话
读取和管理 JSON 格式的配置文件。

### 小白理解
TTBOX 的所有参数都存在 JSON 文件里。
ConfigManager 负责读这些文件，并把参数分发给各个模块。

### 文件位置
`core/src/config/ConfigManager.cpp`

### 修改风险
🟢 低

---

## 十五、运行时配置：RuntimeProfile.cpp

### 一句话
所有可调参数的定义和翻译。

### 小白理解
你在 Web 页面上看到的每个参数（置信度、截取尺寸、PID 参数等），
都在这里定义。它还负责把 YU 格式的参数翻译成 TTBOX 内部格式。

### 文件位置
`core/src/model/RuntimeProfile.cpp`

### 修改风险
🟢 低（但加新参数时容易漏）

---

## 十六、模型仓库：ModelRegistry.cpp

### 一句话
管理 AI 模型文件的注册、导入、安装、激活、删除。

### 小白理解
你在 Web 页面上传模型后，ModelRegistry 负责：
1. 把模型文件复制到仓库目录
2. 校验模型文件是否有效
3. 安装到"已安装"目录
4. 激活为当前使用的模型

### 文件位置
`core/src/model/ModelRegistry.cpp`

### 修改风险
🟢 低

---

## 十七、画面预览：PreviewModule.cpp

### 一句话
在 Web 页面上显示实时画面。

### 小白理解
不需要额外硬件，你可以在浏览器里看到 AI 正在分析的画面。
它独立于 AI 推理运行，不会影响性能。

### 文件位置
`core/src/preview/PreviewModule.cpp`

### 修改风险
🟢 低

---

## 十八、Web API 后端：ttbox_gateway.py

### 一句话
Web 页面的 API 服务器。

### 小白理解
你在浏览器里点的每个按钮、看的每个数据，
都是通过这个 Python 程序转发的。
它把 HTTP 请求翻译成 IPC 消息发给 C++ 核心。

### 文件位置
`scripts/ttbox_gateway.py`

### 修改风险
🟢 低

---

## 十九、HID 桥接：hid/

### 一句话
把鼠标指令通过 USB 模拟成真实鼠标。

### 小白理解
电脑不认识 TTBOX 的"往右移动 10 像素"指令，
它只认识鼠标发来的 USB 信号。
HID 桥接就是充当翻译官，把指令变成 USB 鼠标信号。

### 目录位置
`hid/`

### 文件
- `manifest.json`：HID 配置清单
- `config/`：HID 配置文件
- `descriptors/`：USB 设备描述符

### 修改风险
🟡 中

---

## 二十、EDID 工具：scripts/edid/

### 一句话
告诉游戏电脑"这是一个什么样的显示器"。

### 小白理解
HDMI 信号是双向的：游戏电脑会问显示器"你支持什么分辨率？"
EDID 就是 TTBOX 的回答："我支持 1080p240、1440p144......"

### 目录位置
`scripts/edid/`

### 修改风险
🟢 低

---

## 二十一、授权模块：auth/

### 一句话
检查设备是否被授权使用。

### 小白理解
TTBOX 需要授权才能使用。
这个模块负责检查授权状态、激活设备。

### 目录位置
`core/src/auth/`

### 修改风险
🟡 中

---

## 我想改功能，应该去哪里？

| 我想改... | 在这里... |
|-----------|-----------|
| HDMI 采集 | `core/src/capture/V4L2Capture.cpp` |
| 画面缩放 | `core/src/rga/RgaProcessor.cpp` |
| AI 推理 | `core/src/rknn/RKNNEngine.cpp` |
| 推理线程数 | `core/src/rknn/WorkerPool.cpp` |
| 换模型 | `core/src/model/ModelRegistry.cpp` |
| AI 结果解析 | `core/src/rknn/DecodeNMS.cpp` |
| 目标选择 | `core/src/mouse/TargetSelector.cpp` |
| 目标跟踪/预测 | `core/src/aim/AimThread.cpp` |
| PID 参数 | `core/src/aim/Pid1Controller.hpp` |
| 鼠标输出 | `core/src/output/OutputBackend.cpp` |
| HID 桥接 | `hid/` |
| Web 页面 | `web/` |
| Web API | `scripts/ttbox_gateway.py` |
| 实时配置 | `core/src/model/RuntimeProfile.cpp` |
| 授权 | `core/src/auth/` |

---

## 哪些文件不能随便改

### 🟢 低风险（可以放心改）

- `docs/` — 文档
- `README.md` — 说明文件
- 注释 — 不影响代码行为
- 部分 Web UI — 前端页面

### 🟡 中风险（改之前想清楚）

- `config/` — 配置文件（参数错了可能不工作）
- `core/src/model/` — 模型管理（加新模型要小心）
- `scripts/ttbox_gateway.py` — Web API（接口改了前端可能崩）
- `core/src/rga/RgaProcessor.cpp` — 硬件缩放（尺寸不对可能花屏）
- `core/src/rknn/DecodeNMS.cpp` — AI 解码（换模型类型时可能需要改）

### 🔴 高风险（非必要不要改）

- `core/src/capture/V4L2Capture.cpp` — 采集（驱动相关，改错就没画面）
- `core/src/rknn/RKNNEngine.cpp` — NPU 推理（硬件相关，容易崩）
- `core/src/rknn/WorkerPool.cpp` — 推理线程池（并发问题，难调试）
- `core/src/runtime/CoreRuntime.cpp` — 核心运行时（启动/停止逻辑）
- `core/src/ipc/IpcServer.cpp` — 进程通信（改了可能 Web 连不上）
- `core/src/aim/AimThread.cpp` — 目标控制（影响瞄准行为）
- `core/src/output/OutputBackend.cpp` — 鼠标输出（改错可能没鼠标）
- `hid/` — HID 桥接（硬件相关）

---

## 出问题以后先查哪里

| 现象 | 先查这里 |
|------|---------|
| HDMI 没画面 | `capture/V4L2Capture.cpp` → `V4L2` 驱动 → `EDID` |
| AI 不推理 | `rknn/RKNNEngine.cpp` → `WorkerPool.cpp` → `Model` |
| FPS 下降 | `capture` → `rga` → `WorkerPool` → `RKNN` → `Decode` |
| 检测框异常 | `DecodeNMS.cpp` → `NMS` → `GeometryFilter` |
| 目标选择异常 | `TargetSelector.cpp` → `AimThread` |
| 鼠标不动 | `AimThread` → `OutputBackend` → `HID` |
| Web 打不开 | `scripts/ttbox_gateway.py` → `IpcServer.cpp` |
| Web 参数改了不生效 | `ttbox-bridge.js` → `gateway` → `IpcServer` → `RuntimeConfig` |
| 温度过高 | 散热 → `WorkerPool`(减少并发) → `风扇` |

---

## 完整数据流

### 帧数据流

```
HDMI 输入
  ↓
V4L2Capture 采集帧
  ↓ DMA-BUF（零拷贝）
RGA 硬件缩放（裁剪+缩放到 256×256）
  ↓ DMA-BUF
RKNNEngine 推理（NPU 算 AI 模型）
  ↓ 原始张量
DecodeNMS 解析（提取目标框）
  ↓ DetectionBox 数组
GeometryFilter 过滤（去掉不合理目标）
  ↓
TargetSelector 选择最佳目标
  ↓ AimTargetTask
AimThread 跟踪+控制
  ↓ PID 计算
OutputBackend 输出鼠标指令
  ↓
HID 桥接
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
V4L2Capture / WorkerPool / AimThread 等模块
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

---

## 文件关系表

| 文件 | 作用 | 上游 | 下游 | 风险 |
|------|------|------|------|------|
| `main.cpp` | 程序入口 | OS | Application | 🔴 |
| `Application.cpp` | 总管家 | main | CoreRuntime | 🔴 |
| `CoreRuntime.cpp` | 运行时核心 | Application | 各模块 | 🔴 |
| `V4L2Capture.cpp` | HDMI 采集 | HDMI | Pipeline | 🔴 |
| `RgaProcessor.cpp` | 硬件缩放 | Capture | WorkerPool | 🟡 |
| `RKNNEngine.cpp` | NPU 推理 | Worker | Decode | 🔴 |
| `WorkerPool.cpp` | 推理线程池 | LatestFrame | RKNN/Decode | 🔴 |
| `DecodeNMS.cpp` | AI 解码 | RKNN | TargetSelector | 🟡 |
| `TargetSelector.cpp` | 目标选择 | Decode | AimThread | 🟡 |
| `AimThread.cpp` | 目标控制 | Target | Output | 🔴 |
| `Pid1Controller.hpp` | PID 算法 | AimThread | Output | 🟡 |
| `OutputBackend.cpp` | 鼠标输出 | AimThread | HID | 🟡 |
| `IpcServer.cpp` | 进程通信 | Gateway | Application | 🟡 |
| `ModelRegistry.cpp` | 模型仓库 | Gateway | Filesystem | 🟢 |
| `RuntimeProfile.cpp` | 运行时配置 | Gateway | 各模块 | 🟢 |
| `PreviewModule.cpp` | 画面预览 | Capture | Web | 🟢 |
| `ConfigManager.cpp` | 配置管理 | Application | 各模块 | 🟢 |
| `ttbox_gateway.py` | Web API | Browser | IPC | 🟢 |

---

## 重要函数索引

| 函数 | 作用 | 所在文件 |
|------|------|---------|
| `Application::initialize()` | 初始化整个系统 | `Application.cpp` |
| `Application::run()` | 开始运行 | `Application.cpp` |
| `CoreRuntime::start()` | 启动核心链路 | `CoreRuntime.cpp` |
| `CoreRuntime::stop()` | 停止核心链路 | `CoreRuntime.cpp` |
| `V4L2Capture::capture_loop()` | 采集线程主循环 | `V4L2Capture.cpp` |
| `RgaProcessor::process()` | 执行硬件缩放 | `RgaProcessor.cpp` |
| `RKNNEngine::init()` | 加载模型到 NPU | `RKNNEngine.cpp` |
| `RKNNEngine::run()` | 执行一次推理 | `RKNNEngine.cpp` |
| `InferenceWorker::loop()` | 推理线程主循环 | `WorkerPool.cpp` |
| `DecodeNMS::process()` | 解码 AI 输出 | `DecodeNMS.cpp` |
| `TargetSelector::select()` | 选择最佳目标 | `TargetSelector.cpp` |
| `AimThread::loop()` | 目标控制主循环 | `AimThread.cpp` |
| `Pid1Controller::runAxis()` | PID 计算 | `Pid1Controller.hpp` |
| `OutputBackend::send()` | 发送鼠标指令 | `OutputBackend.cpp` |
| `IpcServer::handle_request()` | 处理 IPC 请求 | `IpcServer.cpp` |
| `ModelRegistry::list()` | 列出所有模型 | `ModelRegistry.cpp` |
| `PreviewModule::loop()` | 预览线程主循环 | `PreviewModule.cpp` |
