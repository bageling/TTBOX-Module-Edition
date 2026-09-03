# AIBox2 代码清单 — v0.0.1（草案，待确认）

> **⚠ 已过时（2026-08-29 Web 重置）**：本文描述的 TTBOX 原始目录结构中，`core/tools/web/`（旧 Python 控制台与部署脚本）已整体移除；相关行仅作历史参考。TTBOX 主线代码以 MAINLINE_MANIFEST.json 为准。

> 本文档是发布收敛前的代码盘点（**只盘点，不清理**）。清理/归档动作需经确认后执行。
> 原则：不新增功能、不改变已验收逻辑/性能参数/接口语义，只整理不重写。

## 1. 项目概览

- 项目根：`ttbox2/`（OrangePi5Plus RK3588 AI 采集/推理/自瞄 + HID 透传系统）
- 两套实现：
  - **C++ Core**（`ttbox/core/`）= **正式生产代码**（A1-A10 验收基于此）
  - **Python 参考**（`ttbox/` 顶层包）= Golden Reference / 开发期验证，正式部署（`/opt/ttbox`）不引用
- 两套部署体系：
  - 体系 A（当前正式）：`ttbox/core/tools/web/a10_*.sh` → `/opt/ttbox`（C++ runtime + ttbox_web.py）
  - 体系 B（早期/参考）：`scripts/systemd/` + `scripts/build/` → `/opt/ttbox2`（Python 包）

---

## 2. 目录扫描分类

| 路径 | 分类 | 说明 |
|---|---|---|
| `ttbox/core/src/` | **正式代码** | C++ 生产核心（推理/采集/HID/模型/配置） |
| `ttbox/core/include/` | 正式 | 版本头 |
| `ttbox/core/tools/web/` | **正式** | 部署脚本（a10_*）、systemd 单元、ttbox_web.py、inject_edid.sh、infer.json |
| `ttbox/core/tools/*.cpp` | 正式/工具 | ttbox_hid_health、ttbox_hid_pkg、ipc_ping；ttbox_hid_forward 已被 C 桥替代（见 §9） |
| `ttbox/core/tests/` | 测试 | 26 个 C++ 测试（unit/hardware，见 §4） |
| `ttbox/core/third_party/rknn/` | 正式 | rknn_api.h |
| `ttbox/core/CMakeLists.txt` | 正式 | 构建定义 |
| `ttbox/`（顶层包） | **参考/废弃** | Python 参考实现，正式部署不引用（详见 §6） |
| `build/ttbox-deploy-payload/` | **正式（部署产物）** | golden 板部署快照：deploy/ 脚本、C++ 源码快照、hid/、config/、models/、scripts/ |
| `build/ttbox-deploy-payload.tar.gz` | 正式 | 上述打包物 |
| `build/imgs/` | 正式/工具 | ONNX→RKNN calibration 图片 |
| `build/board_*.sh`（120 个） | **临时调试** | 板端 HID/EDID/状态调试脚本，全仓库零引用 |
| `build/*.log`、`mx_all.tgz` | 临时/分析 | P-1 性能分析日志输出 |
| `build/mon.sh`、`sysmon.sh`、`top_threads.sh`、`set_aff.sh`、`run_matrix.sh` 等 | 临时/分析 | P-1 调度矩阵实验工具（含明文 sudo 密码，非部署版） |
| `build/ttbox-hid-bridge.c` | **正式候选** | 3 路双向 HID 透传桥（已上板运行，替代 ttbox-hid-forward），需纳入正式部署 |
| `build/hid_desc_tool.c`、`probe_model.cpp`、`test_img_decode.cpp`、`stb_image.h`、`inspect_onnx.py`、`convert_v26m.py`、`verify_align.py`、`v26m_640.onnx`、`build_edid256.ps1` | 临时工具 | 一次性探测/转换工具 |
| `build/rk3588-sdk-kernel/`、`armbian_*.json`、`hdmirx_commits.json`、`mainline_*.c/h` 等 | 内核研究 | hdmirx 驱动移植工作产物 |
| `config/` | 正式 | 配置文件（多来源问题见 §7） |
| `docs/` | 正式 | 架构/A9/A10/性能文档 |
| `hid/` | 正式 | HID 包（config/descriptors/VERSION/manifest） |
| `models/` | 正式 | 本地模型 yolo261n-rk3588.rknn |
| `resources/edid/` | 正式 | EDID 资源（1080p240） |
| `scripts/` | 正式 | 部署脚本（a9_setup_hid_gadget.sh 等）+ 测试工具（a9_*） |
| `scripts/systemd/`、`scripts/build/` | 参考（体系 B） | Python 参考部署的 systemd/构建脚本 |
| `tests/` | 参考测试 | 6 个 Python 参考测试 |
| `tmp_*.py`（24 个） | **临时调试** | 一次性探测脚本（val/onnx/rga 等） |
| `tmp_verify_core.sh` | 临时 | 一次性验证脚本 |
| `vendor/legacy/` | **废弃/参考** | 旧版 Python 单循环实现（README 标注"勿改"），rootfs_install 排除，不进镜像 |
| `DietPi_OrangePi5Plus-ARMv8-Bookworm.img/.xz` | 工具 | 基础系统镜像 |
| `pyproject.toml`、`README.md` | 正式 | 项目元数据 |

---

## 3. 正式模块清单（C++ Core）

| 模块（用户目标结构） | 实现文件 | 职责 |
|---|---|---|
| capture | `core/src/capture/V4L2Capture.cpp/.hpp`、`DmaBuf.cpp/.hpp` | V4L2 MPLANE + DMA-BUF 低延迟采集 |
| rga | `core/src/rga/RgaProcessor.cpp/.hpp` | RGA 硬件缩放/ROI（librga） |
| inference/rknn | `core/src/rknn/RKNNEngine.cpp/.hpp`、`NpuMonitor.cpp/.hpp` | librknnrt 推理、zero-copy set_input、NPU 监控 |
| inference/worker | `core/src/rknn/WorkerPool.cpp/.hpp` | 多 Worker 并发推理、LatestFrame 共享、runtime profile 应用 |
| model | `core/src/model/ModelAdapter/ModelRegistry/ModelMetadata/IModelSource/RuntimeProfile/Decoder` | 模型适配、仓库（installed/staging/active）、元数据、运行时配置 |
| detection | `core/src/rknn/DecodeNMS.cpp/.hpp` | YOLO 解码 + NMS + class filter + FOV |
| coordinate | `core/src/model/RuntimeProfile`（FOV/ROI/CaptureProfile） | 归一化坐标过滤、ROI 裁剪 |
| hid | `core/src/hid/`（HidRuntime/HidForwarder/HidPackage*/HidParser） | HID 透传、gadget 生命周期、包管理 |
| edid | `tools/web/inject_edid.sh`、`scripts/edid/`、`resources/edid/` | EDID 注入/生成 |
| config | `core/src/config/ConfigManager` + `model/RuntimeProfile` | 配置加载（扁平）+ 运行时热更新（内存快照） |
| hardware | 分散于 V4L2Capture/RKNNEngine/WorkerPool（见 §7 硬编码） | 硬件参数当前未集中 |
| web | `tools/web/ttbox_web.py` | 管理控制台（C++ 服务编排） |
| common | `core/src/common/`（Logger/Json/Metrics/Stats/Types） | 基础库 |
| ipc | `core/src/ipc/IpcServer` | IPC 服务 |
| app | `core/src/app/Application.cpp`、`src/main.cpp` | 常驻运行时入口（ttbox_core_main） |

依赖方向（现状）：`app → common/config/ipc`；`rknn → model(Decoder)/common`；`model → common`；`hid → common`；`rga/capture → common`。**Web 仅通过文件/系统调用操作服务，不反向依赖业务模块；HID 与 Model/RKNN 无交叉依赖**（满足用户要求，无需重构）。

---

## 4. 测试清单

### 4.1 C++ 单元测试（`ttbox_core_tests`，host/板端均可跑）

`tests/test_main.cpp、test_logger.cpp、test_config.cpp、test_ipc.cpp、test_application.cpp、test_decode.cpp、test_model_metadata.cpp、test_decoder_dispatch.cpp、test_runtime_profile.cpp、test_model_registry.cpp、test_fov_roi.cpp、test_hid.cpp、test_hid_package.cpp、test_latestframe.cpp、test_rga.cpp`

### 4.2 C++ 硬件验收测试（`TTBOX_CORE_BUILD_HW_TESTS=ON`，板端）

`test_capture_hw、test_rga_hw、test_rknn_hw、test_worker_hw、test_decode_align、test_model_adapter、test_model_runtime、test_model_switch_hw、test_rga_roi_hw、test_hid_forward_hw、test_hid_load_sim、test_hid_loopback、ttbox_hid_test`

### 4.3 Benchmark（性能基准，板端）

- `test_worker_hw`（--frames 300 --duration 180，1/2/3 Worker 矩阵）—— A5/A6 性能验收基准

### 4.4 工具可执行（生产/运维）

`ttbox_core_main、ipc_ping、ttbox-hid-health、ttbox-hid-pkg`（ttbox-hid-forward 已被 C 桥替代）

### 4.5 Python 参考测试（`tests/`，体系 B）

`test_capture.py、test_passthrough.py、test_decode.py、test_pipeline.py、test_config.py、test_aim.py`

---

## 5. 临时调试 / 废弃代码清单（归档候选，待确认）

### 5.1 临时调试脚本（建议归档）

- `build/board_*.sh` × 120（HID gadget 克隆/EDID/键鼠/状态诊断等，零引用）
- 根目录 `tmp_*.py` × 24、`tmp_verify_core.sh`
- `build/hid_desc_tool.c`（临时 descriptor 读取工具）
- P-1 分析：`build/mon.sh、sysmon.sh、top_threads.sh、check_tids.sh、set_aff.sh、run_matrix.sh、debug_cpu.sh、test_cpu.sh、test_while.sh、test_combo.sh、build/setup_freq.sh`（顶层测试版，含明文密码，与部署版 `deploy/setup_freq.sh` 区分）
- 分析产物：`build/*.log` ×31、`build/mx_all.tgz`、`build/page_check.html`
- 探测工具：`build/probe_model.cpp、test_img_decode.cpp、stb_image.h、inspect_onnx.py、convert_v26m.py、verify_align.py、v26m_640.onnx、build_edid256.ps1`

### 5.2 废弃/参考实现（不删除，标注或移出主路径）

- `vendor/legacy/`（旧版，README 标注勿改，构建排除）
- `ttbox/` 顶层 Python 包（Golden Reference，正式部署不引用）：
  - 明确参考：`aim/`、`inference/`、`capture/`、`output/`、`web/console.py`、`pipeline.py`、`config.py`、`main.py`
  - 与 C++ 功能重复且已由 C++ 替代：`output/passthrough.py`（→ C++ hid/ + C 桥）、`web/console.py`（→ ttbox_web.py）
- `scripts/systemd/`、`scripts/build/`（体系 B 参考部署）
- 根目录 `tests/`（Python 参考测试）

### 5.3 内核研究产物（是否保留取决于方向）

- `build/rk3588-sdk-kernel/`、`armbian_*.json`、`hdmirx_commits.json`、`mainline_*.c/h`、`rk_hdmirx_*.c/h`、`synopsys-hdmirx-mainline.c`、`linux-rockchip-develop-6.1.tar.gz`

---

## 6. 正式部署引用关系（保留依据）

| 部署项 | 引用来源 |
|---|---|
| `ttbox/core/tools/web/ttbox_web.py` | `ttbox-web.service` ExecStart；`a10_deploy.sh` 复制到 /opt/ttbox/web/ |
| `infer.json` | `ttbox-infer.sh`（python3 解析 workers/cores/buffers/in_w/in_h → test_worker_hw 参数） |
| `scripts/a9_setup_hid_gadget.sh` | `a10_deploy.sh`、`ttbox-hid.service`、`ttbox-firstboot.sh` |
| `scripts/convert_onnx_to_rknn.py` | `a10_deploy.sh` |
| `hid/` 包 | `a10_deploy.sh`、`ttbox-hid-init.sh` |
| `config/default.json` | `Application`、`test_worker_hw`（ConfigManager） |
| `resources/edid/`、`config/hdmirx_edid_identity.json` | `inject_edid.sh` |
| C++ 二进制 | `a10_deploy.sh` 从 core/build 复制到 /opt/ttbox/runtime/ |
| systemd 单元（5+2） | `a10_deploy.sh` 安装到 /etc/systemd/system/ |

---

## 7. 配置来源清单与冲突（待统一，统一时保持行为不变）

### 7.1 配置来源

| 文件 | 内容 | 消费方 |
|---|---|---|
| `config/default.json`（58 key） | conf/nms/aim/PID/bezier/模型描述/`runtime_profile` 块 | C++ Application、test_worker_hw（扁平 key）；`runtime_profile` 块目前无人消费 |
| `config/yolo261n-rk3588.json` | default.json 子集 | 仅 `ttbox/inference/models.py` 读输入尺寸；conf/nms 为死配置 |
| `ttbox/core/tools/web/infer.json` | model/workers=3/cores="4,5,6"/buffers=8/in_w/in_h=320 | `ttbox-infer.sh` → test_worker_hw 参数 |
| `hid/config/hid_config.json` | HID 开关/affinity/queue/gadget | `HidPackageConfig`、`ttbox_web.py` |
| `config/hdmirx_edid_identity.json` | EDID 身份 | `inject_edid.sh` |

### 7.2 已知多来源/冲突（统一候选，**不改行为**）

| 参数 | 现状 | 建议 |
|---|---|---|
| confidence | default.json conf=0.55（生产） vs 代码默认 0.25 vs 模型配置 0.25（死） vs ModelAdapter 硬编码覆盖 0.25 | 消除死配置与硬编码覆盖，以生产值单一来源 |
| IoU | nms=0.45 多处一致 | 单一来源 |
| buffers | infer.json=8（生产） vs 代码默认 4 | 统一默认 8（不影响 infer.json 生产路径） |
| cores "4,5,6" | 文档语义=CPU affinity(A76)，C++ 消费语义=NPU core_mask | 明确语义，避免误用 |
| class_filter/max_detections/ROI | 扁平 key 与 `runtime_profile` 嵌套块双表示并存 | 收敛到单一路径 |
| FOV/color_order | 多来源但值一致 | 确认唯一来源 |
| 硬编码（worker=3/CPU 4-5-6/8 buffers/锁频） | 散落 infer.json + 各脚本 + 代码默认 | **集中到 hardware profile**（新文件，不散落） |

---

## 8. 性能路径检查（现状核对）

| 禁止项 | 现状 |
|---|---|
| 逐帧 JSON | ✅ C++ 用 RuntimeConfig 内存快照，禁逐帧 JSON/IPC |
| 逐帧文件 IO | ✅ 状态/预览低频写入 |
| 大块 memcpy | ✅ RKNNEngine zero-copy set_input |
| 频繁 malloc/free | ✅ SPSC 固定容量队列，无热路径分配 |
| 重复创建 Worker/RKNN context | ✅ 生命周期管理（WorkerPool/RKNNEngine 启动时创建） |
| RGA fd 泄漏 | ⚠️ 需板端回归验证（test_rga_hw） |
| 大量 debug log | ✅ TTBOX_LOG 分级 |

当前硬件配置（生产，来自 infer.json + ttbox-infer.sh）：3 Worker / cores=4,5,6（NPU core_mask）/ 8 V4L2 buffers / 320×320 / 锁频策略。**CPU A76 affinity 实际由外部 set_aff.sh（taskset）执行，生产 service 未绑核**——整理时保持现状，不引入行为变化。

---

## 9. 服务清单

| systemd 单元 | 二进制/脚本 | 状态 |
|---|---|---|
| ttbox-runtime.service | ttbox_core_main（C++） | 常驻 |
| ttbox-web.service | ttbox_web.py | 常驻 |
| ttbox-hid.service（oneshot） | ttbox-hid-init.sh + a9_setup_hid_gadget.sh enable + ttbox-hid-health | 常驻（RemainAfterExit） |
| ttbox-hid-forward.service | **当前为 C 桥**（ttbox-hid-bridge 编译产物，替代原 C++ ttbox_hid_forward） | 常驻 |
| ttbox-hid-watchdog.service | ttbox-hid-watchdog.sh | 常驻 |
| ttbox-infer.service | ttbox-infer.sh → test_worker_hw | web 触发，非自启 |
| ttbox-edid-inject.service / ttbox-firstboot.service | 相应脚本 | 一次性 |

独立性与恢复：Runtime/Web/HID 各自独立 systemd 单元，Restart 策略已配置，单模块崩溃可恢复。

---

## 10. 归档/清理/保留建议（草案，待确认）

### 10.1 建议归档（移入 `archive/` 或 `docs/` 标注，不删除源码能力）

1. `build/board_*.sh` ×120、`build/hid_desc_tool.c`、`build/page_check.html`
2. 根目录 `tmp_*.py` ×24、`tmp_verify_core.sh`
3. `build/*.log` ×31、`build/mx_all.tgz`
4. P-1 分析脚本（build 顶层 11 个）——**注意保留正式版 `deploy/setup_freq.sh`**
5. 探测工具（build 顶层 8 个）

### 10.2 建议保留

1. `build/ttbox-deploy-payload/` + `.tar.gz`（正式部署快照）
2. `build/imgs/`（calibration）
3. **`build/ttbox-hid-bridge.c`**（正式运行时依赖，建议移入 `scripts/` 或纳入 a10_deploy 编译部署）
4. `scripts/` 全部、`hid/`、`config/`、`resources/edid/`、`models/`、`docs/`
5. 内核研究产物（若 hdmirx 方向继续）

### 10.3 待用户决策

- A. Python 参考实现（`ttbox/` 顶层包 + `tests/` + `scripts/systemd/` + `scripts/build/`）：**保留原位**（文档标注参考）或 **归档移出**？
- B. `vendor/legacy/`：已排除构建，**保留原位**（勿改）或归档？
- C. 内核研究产物是否归档？
- D. C 桥部署方式：纳入 a10_deploy 编译流程 or 单独脚本？
- E. 配置统一范围：仅消除死配置/硬编码（不改值），或同时收敛到 `runtime_profile` 块？

---

*草案结束。清理/重构动作在确认后执行。*
