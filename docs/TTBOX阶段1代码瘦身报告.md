# TTBOX阶段1代码瘦身报告

> 工作区：`C:/Users/Administrator/Desktop/TTBOX`
> 阶段：只清理，不重构
> 状态：已完成并停止，未进入 Preprocess / Detector / Coordinate 重构。

## 一、执行结论

本阶段完成了有证据支持的代码瘦身：

1. 删除历史重复 Core：`yu-backend/yu-core-src/core/`。
2. 保留 `core/tools/web/ttbox-infer.service` 与 `core/tools/web/ttbox-infer.sh`，因为 Web 管理代码仍存在真实引用；没有贸然归档，避免改变当前生产逻辑。
3. 将旧 Runtime 服务文件标记为 deprecated，明确其不是唯一生产入口。
4. 移除部署脚本对已废弃 `test_worker_hw` 二进制的复制依赖。
5. 保留 HardwareRunner、HID/Output、模型配置、Worker CPU Direct/RGA、Web/Gateway/IPC，因为本阶段均能找到真实引用或属于当前运行路径。
6. 保持真实鼠标输出关闭：`config/default.json` 中 `output_enabled=false`。

最终主链目标保持不变，当前实现未被改写：

```text
HDMI
 ↓
Capture
 ↓
Worker / Detector 当前实现
 ↓
Detection
 ↓
AimThread 当前实现
 ↓
HID 当前实现
```

## 二、删除的文件/目录

### 删除目录

```text
yu-backend/yu-core-src/core/
```

删除前统计：

- 文件数：209
- 文件总大小：1,428,170 字节
- 与根 `core/` 存在大量字节级完全重复文件

### 删除依据

删除前执行了全仓库文本引用扫描和字节级重复扫描：

- `yu-backend/yu-core-src/core/` 路径只出现在架构文档中。
- 没有 CMake 构建引用。
- 没有 systemd `ExecStart` 引用。
- 没有部署脚本将该目录作为源码输入。
- 没有运行脚本调用该目录下的程序。
- 目录中的 Core 源码、测试、工具、服务文件与根 `core/` 大量完全相同。
- 根 `core/` 已通过 Windows CMake 和 CTest 验证，可作为唯一施工 Core。

因此该目录被判定为历史副本，不是当前构建、运行或测试输入。

## 三、删除后的引用检查

删除后再次扫描：

### `yu-backend/yu-core-src/core`

剩余命中仅位于以下历史文档：

- `docs/TTBOX架构重构报告.md`
- `docs/阶段1代码瘦身清理清单.md`
- `docs/architecture/TTBOX_CANDIDATE_RECT_INVESTIGATION.md`
- `docs/architecture/TTBOX_TARGET_OBJECT_MODEL.md`

这些是历史说明文字，不是代码、构建或运行依赖。

### CMake 源文件检查

对根 `core/CMakeLists.txt` 中所有 `src/*.cpp` 和 `src/*.c` 进行存在性检查：

```text
cmake_missing_sources = []
```

结果：CMake 没有引用已删除的源码文件。

## 四、移入 legacy 的文件

本阶段没有将 `ttbox-infer.service` 或 `ttbox-infer.sh` 移入 legacy。

原因：

- `core/tools/web/ttbox_web.py` 仍然直接引用 `ttbox-infer.service`。
- Web 层仍会查询该服务状态。
- Web 层仍会调用启动、停止和重启逻辑。
- Web 层仍读取旧推理配置 `infer.json`。
- Web 层仍依赖 `active_model.txt`。
- `a10_deploy.sh` 仍会复制该服务和脚本。

直接归档会改变当前管理面行为，不符合“本阶段只清理、不改变当前生产逻辑”的要求。

当前采取的处理是：

- 在 `core/tools/web/ttbox-infer.service` 顶部增加 `DEPRECATED` 标记。
- 在 `core/tools/web/ttbox-infer.sh` 顶部增加 `DEPRECATED` 标记。
- 保留文件和现有引用，留待后续完成迁移后再归档。

## 五、Runtime 清理结果

### 保留：`core/src/main.cpp`

位置：

```text
core/src/main.cpp:37
```

职责：

- 注册 SIGINT/SIGTERM。
- 创建 Application。
- 调用 initialize。
- 进入 run。
- 退出时调用 shutdown。

该文件继续作为唯一生产入口。

### 保留：`core/tools/hardware_runner_main.cpp`

定位：

```text
测试/验收工具
```

保留依据：

- 根 `core/CMakeLists.txt` 在 RKNN 可用时注册该工具。
- `HardwareRunner` 有对应真实实现。
- 存在 `test_hardware_runner` 测试。
- 工具支持 Trace/Null 等不注入真实鼠标的验证方式。

明确结论：

```text
hardware_runner_main.cpp 不是生产入口
```

### 旧 `test_worker_hw`

已从根 CMake 的生产部署复制路径中移除。

`a10_deploy.sh` 已由：

```bash
cp $BUILD/ttbox_core_main $BUILD/test_worker_hw $OPT/runtime/
```

改为：

```bash
cp $BUILD/ttbox_core_main $OPT/runtime/
[ -f $BUILD/hardware_runner_main ] && cp $BUILD/hardware_runner_main $OPT/tests/ || true
```

这一步没有修改 Worker、Detector 或 Runtime 内部逻辑，只清理旧部署复制依赖。

## 六、HID / Output 清理结果

本阶段没有删除 HID 或 Output 实现。

### 保留的生产/运行实现

- `AiboxHidOutput`
- `OutputBackend`
- `LocalHidBackend`
- `UsbProxyBackend`
- `FifoHidOutput`
- `MouseControlClient`

### 保留的测试实现

- `TraceHidOutput`
- `NullHidOutput`
- `test_output_backend`
- `test_mouse_control_client`
- `test_hid_loopback`
- `test_hid_load_sim`
- `test_hid_forward_hw`

### 保留的 HID 透传实现

- `HidForwarder`
- `HidRuntime`
- `IHidRuntime`
- `HidPackageRegistry`
- `HidPackageConfig`
- `HidPackageManifest`
- `ttbox-hid-bridge.c`

保留依据：这些文件被 CMake、Application、HardwareRunner、测试、服务文件或部署脚本引用。当前阶段没有足够证据证明其完全无用，因此不删除。

真实鼠标输出状态：

```json
"output_enabled": false
```

## 七、配置清理结果

本阶段不重构配置系统，也没有删除配置字段。

### 仍存在并有引用的配置来源

- `config/default.json`
- `config/yolo261n-rk3588.json`
- `RuntimeProfile.model_id`
- `models/registry/active.json`
- `active_model.txt`
- `infer.json`
- `scripts/ttbox_web.py` 中的模型配置逻辑
- `core/tools/web/ttbox_web.py` 中的模型配置逻辑
- `yu-backend/app.py` 中的模型管理逻辑

### 当前确认

- `model_label` 有 Application/Web/配置引用。
- `model_input_width` 和 `model_input_height` 有 Application、测试和模型配置引用。
- `RuntimeProfile.model_id` 有 Core IPC/Web/模型管理引用。
- `active_model.txt` 被旧推理脚本和 Web 管理路径读取。
- `infer.json` 被旧推理脚本、Web 和部署脚本读取。
- `ModelRegistry` 使用 `active.json` 维护激活模型状态。

这些来源存在重复，但均有实际引用。本阶段只记录，不强行改为单一配置系统。

## 八、Worker 清理结果

本阶段没有修改 Worker 的预处理行为。

保留：

- CPU Direct 路径
- RGA 路径
- ROI 逻辑
- crop 逻辑
- resize 逻辑
- stride 处理
- 格式转换
- FP16 转换
- RKNN 输入输出路径
- Decoder / DecodeNMS / NMS

原因：这些内容属于当前实际处理路径，且用户明确要求暂停 Preprocess + Detector 重构。本阶段只清除明确废弃的部署入口，不改变 Worker 内部行为。

## 九、Web / Gateway / IPC 清理结果

本阶段保留：

- `scripts/ttbox_web.py`
- `scripts/ttbox_gateway.py`
- `core/tools/web/ttbox_web.py`
- `core/src/ipc/IpcServer.cpp/.hpp`
- `platform/*`
- `yu-backend/*`

保留原因：

- Web 层仍提供管理接口。
- Gateway 仍负责 IPC 转译。
- IPC 是 Web 与 C++ Core 的管理边界。
- Platform supervisor 仍负责服务管理。
- `yu-backend` 仍包含应用和兼容管理路径。

本阶段没有删除 endpoint 或改动接口，避免改变当前生产逻辑。

## 十、CMake 修改

修改文件：

```text
core/CMakeLists.txt
```

修改内容：

将旧注释：

```text
旧 test_worker_hw 已弃用。硬件链路由后续 HardwareRunner 独立入口承载。
```

更新为：

```text
旧独立 Worker 入口已移除。硬件链路由 HardwareRunner 验收入口承载。
```

根 CMake 中没有引用已删除的 `yu-backend/yu-core-src/core`。

## 十一、其他代码/脚本修改

### `core/tools/web/a10_deploy.sh`

删除旧部署依赖：

```bash
$BUILD/test_worker_hw
```

部署脚本现在只复制：

- `ttbox_core_main` 到 Runtime
- `hardware_runner_main` 到 Tests（如果存在）

### `core/tools/web/ttbox-infer.service`

增加 deprecated 标记，说明：

- 该服务是历史 Web 控制推理包装。
- 不是唯一生产入口。
- 当前生产入口是 `main.cpp → Application → CoreRuntime`。

### `core/tools/web/ttbox-infer.sh`

增加 deprecated 标记，说明：

- 该脚本属于旧 Web 管理兼容路径。
- 当前仍有真实引用，因此暂不删除。

## 十二、清理前后的关键文件统计

### 删除前

```text
总文件数：1082
root/core 文件数：594
yu-backend/yu-core-src/core 文件数：209
yu-backend/yu-core-src/core 总大小：1,428,170 字节
```

### 删除后

```text
yu-backend/yu-core-src/core：不存在
yu-backend/yu-core-src：保留为空目录容器
```

## 十三、构建验证

执行：

```bash
cmake --build core/build --config Release --parallel 4
```

结果：

```text
build_exit=0
```

Release 构建通过，产物位于：

```text
C:/Users/Administrator/Desktop/TTBOX/core/build/Release
```

## 十四、CTest 验证

执行：

```bash
ctest --test-dir core/build -C Release --output-on-failure
```

结果：

```text
100% tests passed out of 14
Total Test time = 3.78 sec
EXIT 0
```

即：

```text
14/14 passed
```

通过测试：

- `ttbox_core_tests`
- `test_aim_target_mailbox`
- `test_aim_thread`
- `test_pid1`
- `test_hotkey_gate`
- `test_hotkey_config`
- `test_selector_stress`
- `test_lifecycle_stress`
- `test_fov_angle`
- `test_detection_geometry_filter`
- `test_coordinate_transform`
- `test_output_backend`
- `test_mouse_control_client`
- `test_personal_motion`

## 十五、生产入口确认

当前生产入口：

```text
core/src/main.cpp
```

调用关系：

```text
core/src/main.cpp
 ↓
Application
 ↓
CoreRuntime
```

`hardware_runner_main.cpp` 仅作为测试/验收工具保留。

## 十六、当前生产主链确认

当前主链没有被重构，只进行了外围清理：

```text
HDMI
 ↓
V4L2Capture
 ↓
LatestFrame
 ↓
WorkerPool / InferenceWorker
 ↓
RGA 或 CPU Direct
 ↓
RKNNEngine
 ↓
Decoder / DecodeNMS / NMS
 ↓
AimTargetMailbox
 ↓
AimThread
 ↓
OutputBackend / IHidOutput
 ↓
HID / USB Proxy / FIFO 后端
```

压缩表达：

```text
HDMI
 → Capture
 → Worker / Detector 当前实现
 → Detection
 → AimThread 当前实现
 → HID 当前实现
```

## 十七、仍不能删除的历史/兼容代码

### 1. `core/tools/web/ttbox-infer.service`

原因：

- Web 层仍查询其状态。
- Web 层仍控制启停。
- 部署脚本仍安装它。
- 直接删除会改变当前 Web 管理行为。

处理：增加 deprecated 标记，暂时保留。

### 2. `core/tools/web/ttbox-infer.sh`

原因：

- Web 层仍通过服务间接使用。
- 仍读取旧模型来源和 `infer.json`。
- 仍被历史部署脚本复制。

处理：增加 deprecated 标记，暂时保留。

### 3. `hardware_runner_main.cpp` 与 `HardwareRunner.*`

原因：

- 有 CMake 注册。
- 有硬件验收测试。
- 属于测试/验收能力，不是生产逻辑。

处理：保留并明确为非生产工具。

### 4. HID/Output 多套实现

原因：

- 生产路径、测试路径、板端透传路径各有真实引用。
- 当前阶段禁止重新设计 MouseOutput。
- 无法证明完全无引用。

处理：全部保留，下一阶段再单独收敛。

### 5. 多套模型配置

原因：

- 当前均有真实读取或管理引用。
- 本阶段禁止配置系统重构。

处理：保留并记录下一阶段统一任务。

### 6. Worker CPU Direct / RGA

原因：

- 属于当前处理逻辑。
- 用户明确暂停 Preprocess + Detector 重构。

处理：不修改。

### 7. Web/Gateway/Platform 兼容层

原因：

- 管理面仍有真实功能和入口。
- 删除会改变产品管理能力。

处理：保留。

## 十八、阶段边界

本阶段已完成代码瘦身并停止。

明确没有执行：

- Preprocess 重构
- Detector 重构
- Coordinate 重构
- 新框架引入
- 新 IPC 引入
- 新消息总线引入
- 新数据库引入
- MouseOutput 重新设计
- 真实鼠标输出启用
- 删除有真实引用的 HID/Output 实现
- 配置系统统一重构
- RK3588 实机操作

## 十九、知识库归档

已将本阶段结论归档至知识库：

- `技术/工程/TTBOX阶段1代码瘦身报告.md`
- 更新 `技术/工程/工程.md`
- 更新 `首页.md`

## 二十、最终结论

阶段 1 已完成：

```text
删除历史重复 Core
↓
移除旧 test_worker_hw 部署复制依赖
↓
标记旧推理服务 deprecated
↓
保留所有仍有真实引用的实现
↓
确认 CMake 无缺失源码
↓
Release build_exit=0
↓
CTest 14/14 passed
```

当前停止在阶段 1，不进入 Preprocess、Detector 或四段主链重构。
