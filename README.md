# TTBOX 模块版

一台用 **AI 看画面、自动动鼠标** 的边缘计算盒子。

它的完整能力是：

```text
电脑画面 ──HDMI 线──▶ TTBOX 盒子（RK3588）
                          │
                          ▼
                  AI 识别画面里的目标
                          │
                          ▼
                  USB 线模拟真实鼠标移动
```

简单说：**盒子只用 HDMI 线看电脑屏幕，用 AI 找到目标，再用 USB 线把鼠标指令送进电脑。**

它不读内存、不改游戏数据，只处理外部画面，所以接入电脑时不需要安装任何软件。

---

## 一、新人三步看懂这个仓库

1. **先看目录**：`core/` 是真正跑 AI 的 C++ 程序，`usbproxy/` 是鼠标注入代理，`plugins/` 是网页控制台，`docs/` 是所有中文说明。
2. **再看链路**：画面采集 → 图像缩放 → AI 推理 → 目标选择 → 位移计算 → 鼠标注入。
3. **最后动手**：按本文"快速上手"把盒子接好，打开网页控制台就能看状态。

看完本文还不会？直接看 [docs/README.md](docs/README.md) 的"小白路线"，全部是中文。

---

## 二、快速上手（5 分钟）

### 1. 接线

```text
电脑 HDMI 输出  ──▶  盒子 HDMI 输入口
盒子 USB 输出口 ──▶  电脑 USB 口
```

两条线各干各的：

| 线 | 方向 | 作用 |
|---|---|---|
| HDMI 输入线 | 电脑 → 盒子 | 盒子的眼睛，看电脑画面 |
| USB 输出线 | 盒子 → 电脑 | 盒子的手，模拟鼠标移动 |

### 2. 通电并打开网页

盒子通电等约 30 秒，然后在电脑浏览器打开：

```text
http://<盒子IP>:8000
```

例如盒子 IP 是 `192.168.0.53`，就打开 `http://192.168.0.53:8000`。

### 3. 检查画面是否进来

网页"总览"页如果显示：

- 采集 FPS 大于 0：说明 HDMI 画面进来了。
- 检测数量大于 0：说明 AI 已经认出目标。
- 预览有画面：说明盒子正在把画面推给你看。

### 4. 激活模型

到"模型库"页选择一个模型，点激活。激活成功后，状态会显示当前模型 ID。

### 5. 打开注入并按住热键测试

到"鼠标控制"页：

1. 打开"鼠标注入"开关（`mouse.enabled=true`）。
2. 设置热键（默认左键/右键）。
3. 按住热键，AI 才会把鼠标指令注入电脑；松开热键立即停止。

> 重要：注入必须按热键才会生效，不是开机就一直控制鼠标。

---

## 三、板端运行环境

| 组件 | 说明 |
|---|---|
| 硬件 | OrangePi 5 Plus，瑞芯微 RK3588，3 核 NPU |
| 系统 | Armbian / Ubuntu Linux（aarch64） |
| 视频输入 | HDMI RX，板端设备 `/dev/video0` |
| AI 核心 | C++ 程序 `ttbox_core_main` |
| 模型 | `.rknn` 格式，YOLO 系列 |
| 网页 | Python 服务，端口 8000 |
| 预览 | Python 服务，端口 8001 |
| 鼠标注入 | 自研 `usbproxy` |

### 板端服务

| 服务 | 作用 |
|---|---|
| `ttbox-core` | AI 核心：采集 → 推理 → 瞄准 → 输出 |
| `ttbox-web` | 网页控制台（8000 端口） |
| `ttbox-preview` | 预览画面（8001 端口） |
| `ttbox-usbproxy` | 鼠标注入代理 |
| `ttbox-edid` | HDMI 身份注入（默认关闭，需要时才开） |

查看服务状态：

```bash
systemctl is-active ttbox-core ttbox-web ttbox-preview ttbox-usbproxy
```

板端安装路径：

```text
/opt/ttbox/
├── bin/ttbox_core_main         AI 核心主程序
├── config/default.json         运行配置
├── scripts/ttbox_web.py        网页后端
├── usbproxy/usb-proxy          鼠标注入代理
├── models/installed/           已安装模型
└── src/core/                   源码和构建目录
```

> 板端只运行 TTBOX 自己的服务和 `/opt/ttbox` 目录，所有操作都以 TTBOX 为准。

---

## 四、仓库目录结构

```text
TTBOX-Module-Edition/
├── core/               AI 核心 C++ 源码 + 单元测试
│   ├── src/            生产源码（采集/推理/瞄准/输出）
│   ├── tests/          C++ 测试 + 真机调试脚本
│   ├── tools/          板端工具（压测、诊断、状态查询）
│   └── CMakeLists.txt  构建脚本
├── usbproxy/           自研鼠标注入代理源码
├── plugins/            网页/预览等插件
├── scripts/            运维脚本 + 网页主程序
├── framework/          Python 框架（插件管理）
├── config/             配置模板
├── deploy/             systemd 服务文件和依赖说明
├── docs/               中文文档中心
├── modules/            模块讲解 README（只读视图，不是源码）
├── platform/           V1 实验骨架（未接入运行链路）
├── tools/              模型转换工具
├── ttbox_motion/       运动控制（校准/训练）
└── tests/              集成测试脚本
```

`core/src/` 里每个目录的职责：

| 目录 | 干什么 |
|---|---|
| `capture/` | 用 V4L2 从 HDMI 采集画面 |
| `rga/` | 硬件缩放/裁剪画面 |
| `rknn/` | NPU 推理、解码、预处理、工作线程池 |
| `model/` | 模型库：注册、校验、切换 |
| `mouse/` | 选目标、坐标换算、瞄准跟踪 |
| `aim/` | 瞄准线程和 PID 计算 |
| `output/` | 输出后端，连接 usbproxy |
| `ipc/` | 网页与核心之间的进程通信 |
| `preview/` | 生成预览图 |
| `input/` | 读取物理鼠标按键（热键来源） |
| `app/`、`runtime/` | 启动装配和运行时管理 |
| `common/`、`config/` | 公共工具和配置 |
| `auth/` | 授权（可禁用） |
| `hid/`、`detector/`、`pipeline/`、`bench/` | HID 包、检测器接口、任务队列、NPU 基准 |

---

## 五、完整数据链路

```text
HDMI 画面
  → V4L2Capture（采集，零拷贝 DMA-BUF）
  → RgaProcessor（RGA 缩放/裁剪到模型输入）
  → RKNNEngine（NPU 推理）
  → DecodeNMS（把模型输出变成检测框）
  → TargetSelector（选一个目标）
  → Pid1Controller（算 dx/dy 位移）
  → AimThread（热键门控）
  → MouseControlClient（usbproxy 协议包）
  → usbproxy（Raw Gadget）
  → Windows 鼠标真实移动
```

预览链路是另一条独立的小路：

```text
采集帧 → OpenCV 画检测框 → JPEG → 8001 端口 → 网页显示
```

---

## 六、配置怎么改

板端真正的运行配置：

```text
/opt/ttbox/config/default.json
```

仓库里的 `config/default.json` 只是模板。

改配置推荐用网页：

```text
PUT /api/config
```

它是"深合并"：只改你传的字段，其它参数不会被冲掉。

几个关键配置：

| 配置 | 作用 |
|---|---|
| `output_enabled` | 总开关，是否允许鼠标输出 |
| `mouse.enabled` | 鼠标注入开关 |
| `mouse.aim_hotkey` / `aim_hotkey2` | 注入热键 |
| `rknn_external_dma_input` | 是否让 RGA 的 DMA-BUF 直连 NPU（当前默认开） |
| `model_id` | 当前激活模型 |
| `worker_cores` | 推理线程绑定的 CPU 核心 |

---

## 七、开发者怎么编译

### Windows 本机（开发 + 单元测试）

```bash
cd core
cmake -B build-win -G "MinGW Makefiles" -DTTBOX_CORE_WITH_ONNX=ON
cmake --build build-win -j8
ctest --test-dir build-win --output-on-failure
```

Windows 下需要先准备 MSYS2 工具链，编译时把 `C:\msys64\ucrt64\bin` 加到 `PATH`。

### 板端 RK3588（真机）

```bash
cd /opt/ttbox/src/core
cmake --build build -j6
systemctl stop ttbox-core
cp build/ttbox_core_main /opt/ttbox/bin/ttbox_core_main
systemctl start ttbox-core
```

### 板端 usbproxy

```bash
cd /opt/ttbox/src/usbproxy
make
systemctl restart ttbox-usbproxy
```

---

## 八、测试

### 本机单元测试

```bash
cd core
cmake --build build-win -j8
ctest --test-dir build-win --output-on-failure
```

当前状态：26 个 CTest 全绿，聚合用例 117 个通过。

### 网页后端测试

```bash
.venv-win/Scripts/python.exe -m pytest framework/tests/test_web_plugin.py -q
```

### 板端常用验证

```bash
# 服务健康
systemctl is-active ttbox-core ttbox-web ttbox-preview ttbox-usbproxy

# 实时指标（网页 8000 端口）
python3 /tmp/monitor_board_runtime.py --seconds 30

# usbproxy 按键/移动测试
python3 /opt/ttbox/src/core/tests/usbproxy_buttontest.py
```

---

## 九、当前真实状态（2026-09-13）

| 项目 | 数据 |
|---|---|
| 采集 | 约 240 FPS（1080p 源） |
| 推理（空闲态聚合） | 约 30 FPS |
| 端到端延迟 | 约 5.8 ms |
| 预览 | 30 FPS，丢帧 0 |
| RKNN 输入 | external DMA 直绑已开启 |
| 模型热切换 | EP ↔ 320dawan 连续切换通过 |
| 板端服务 | core/web/preview/usbproxy 全部 active |

## 十、文档导航

全部是中文，推荐阅读顺序：

| 顺序 | 文档 | 内容 |
|---|---|---|
| 1 | [docs/README.md](docs/README.md) | 文档总入口 |
| 2 | [docs/小白教程/01-TTBOX是什么.md](docs/小白教程/01-TTBOX是什么.md) | 用大白话讲 TTBOX |
| 3 | [docs/小白教程/02-TTBOX怎么工作.md](docs/小白教程/02-TTBOX怎么工作.md) | 工作流程 |
| 4 | [docs/架构/系统总览.md](docs/架构/系统总览.md) | 系统分层 |
| 5 | [docs/架构/完整链路.md](docs/架构/完整链路.md) | 完整数据链路 |
| 6 | [docs/问题排查.md](docs/问题排查.md) | 常见问题排查 |
| 7 | [docs/废弃代码清单.md](docs/废弃代码清单.md) | 已废弃/不再接线的代码清单 |

## 十一、常见问题

**网页打不开？**
检查 `ttbox-web` 服务是否 active，检查防火墙，确认浏览器地址是 `http://<盒子IP>:8000`。

**预览黑屏？**
先看"总览"里的采集 FPS。采集为 0 表示 HDMI 信号没进盒子：换 HDMI 线、检查电脑是否把画面复制输出到盒子，必要时重新枚举 HDMI。

**鼠标不动？**
依次检查：`output_enabled` 是否为 true、`mouse.enabled` 是否为 true、热键是否按住、`ttbox-usbproxy` 是否 active、`/run/ttbox-mouse-passthrough/cmd.sock` 是否存在。

**模型库选项乱跳？**
不要手动改 `model_id` 配置。模型切换只走网页模型库或 `/api/models/select` 接口。

**板端文件装在哪个目录？**
TTBOX 的一切代码、配置、模型和运行状态都在 `/opt/ttbox` 内，不依赖其它目录。

---

许可证：MIT，详见 [LICENSE](LICENSE)。
