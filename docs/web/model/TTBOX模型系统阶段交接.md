# TTBOX 模型系统阶段交接文档

生成时间：2026-09-06
交接人：Hermes Agent（dmgj profile）
接收人：下一阶段接手智能体 / 下一次会话

## 一句话状态

模型系统已完整调查取证（全部真实源码+板端证据），**尚未动功能代码、尚未写设计文档**；接手人直接基于本文档写 `docs/web/TTBOX_MODEL_SYSTEM_DESIGN.md` 与 `docs/web/TTBOX_MODEL_CONVERSION.md` 即可进入实现阶段。

---

## 仓库 / Git 状态

- 工作目录：`C:\Users\Administrator\Desktop\TTBOX-Module-Edition`
- HEAD：`4c8c407 fix(deploy): web/preview 服务依赖改为 Wants，core 重启不再连带停止`
- 未提交改动：106 modified + 35 untracked（预览阶段改动 + 模型阶段只读调查）
- 板子：`ssh ttbox53`（192.168.0.53 root 免密）
- 板端源码树：`/opt/ttbox/src/core`（多次备份 `core.bak-*`）
- 板端服务：ttbox-core / ttbox-web / ttbox-preview 均 active

---

## 一、当前模型系统真实状态（已完成调查）

### 1. 板端模型目录（真实）

```
/opt/ttbox/models/
├── installed/jwdl_sjzv11/
│   ├── model.rknn      10,980,940 bytes
│   └── metadata.json   {"input_height":256,"input_width":256,"output_count":6}
└── registry/active.json  {"activated_at":1788613124799,"model_id":"jwdl_sjzv11"}
```

**没有 manifest.json**（与 ModelRegistry 期望的目录结构不符，靠 list() 的兼容分支列出）。
**没有 labels.txt / calibration/ / source/ / validation/ 目录。**
模型 sha256：`1592a676bc6b76e8680021b5d434cd0825d7ec7bee1e8b3807a520aebe57a11a`

### 2. 当前唯一可运行模型

- 模型 id：`jwdl_sjzv11`
- 运行日志：`输入 256x256 INT8 NHWC size=196608B | 输出 6 | 加载 ~20ms`
- 零拷贝 I/O 已绑定：input=196608 bytes, outputs=6
- RKNN Runtime：板端 `/opt/ttbox/lib/librknnrt.so` = **1.5.2** (c6b7b351a@2023-08-23T15:28:22)
- 板端另存在 `/opt/ttbox/src/config/yolo261n-rk3588.json`（旧配置模板，非当前使用）

### 3. ModelRegistry（core/src/model/ModelRegistry.hpp / .cpp，459 行）

职责已实现：init / import / validate / install / activate / deactivate / remove / list

- 目录结构：`registry/ installed/ staging/ cache/ quarantine/`（还有 `_incoming/` 收件目录）
- `ModelManifest` 结构（已实现）：model_id / label / version / sha256 / signature / origin / converter_version / runtime_version / input_width / input_height / output_count / class_count / class_names / rknn_concurrency / status / created_at
- `ModelStatus`：staging=1 / installed=2 / quarantined=3
- 关键行为：
  - `validate()` 必须通过注入的 validator；通过后写 `validation/ok.json` + 合并探测尺寸进 manifest
  - `install()` 要求已 validate（staging/validation/ok.json 存在）
  - `activate()` 前调用 validator 校验，**失败保持原激活**（自动回滚旧模型）
  - `remove()` 拒绝删除 active 模型
  - `list()` 无 manifest.json 但有 model.rknn 也列出（根因修复，否则 MODEL_LIST 空白）
  - **未做**：checksum 校验、真实一次推理验证、quarantine 自动移动（validate 失败留 staging，由调用方调 quarantine）

### 4. ModelAdapter（core/src/model/ModelAdapter.hpp / .cpp，258 行）

- `analyze(info, cfg, error)`：从 RKNN runtime 查询结果构建完整 ModelMetadata
- `infer_decode_type()`：**按输出结构推断，禁止按模型名/文件名判断** —— 已做的分类：
  - 单输出 3D `[1,N,F]` F<=16 → kE2e（v26m 类）
  - 单输出 `(1,C,M)` C>=5 → kSingle（yolo261n 类）
  - 多输出 3 的倍数 `reg(1,64,H,W)+cls(1,C,H,W)+aux(1,1,H,W)` → kDflDist（大腕256）
  - 多输出成对 box/cls → kDfl（黄瓦类）
- `infer_class_count()`：按 decode_type 从输出 dims 推断
- `create_decoder()`：注入 conf/iou/class_filter/max_detections/input 尺寸，创建 DecoderImpl；E2E 跳过无条件二次 NMS
- **当前应用侧校验器（Application.cpp:391-417）只填 input_width/input_height/output_count**，没有 class_count/decode_type（metadata.json 只有 3 字段就是这个原因）

### 5. ModelMetadata（core/src/model/ModelMetadata.hpp，91 行）

关键枚举（已实现）：
- DecodeType：kUnknown / kSingle / kDfl / kE2e / kDflDist
- QuantType：kNone / kInt8 / kUint8
- ColorOrder：kBgr / kRgb
- NmsType：kClasswise / kGlobal
- CoordFormat：kXywh / kXyxy / kLtrb

完整字段：输入（width/height/channels/dtype/layout/color_order/quantization_type/input_size）+ 输出（count/dtypes/layouts/shapes）+ 解码（decode_type/strides/class_count/objectness/dfl/nms_type/coordinate_format）+ default_conf/default_iou + label

### 6. RKNNEngine（core/src/rknn/RKNNEngine.hpp / .cpp，486 行）

- `init()`：rknn_init → 查询 IN_OUT_NUM → 输入/输出属性 → core_mask
- `init_zero_copy()`：rknn_create_mem + rknn_set_io_mem 绑定（当前模型已启用）
- `set_input()`：**INT8/FP16 模型统一喂 UINT8 原始像素（0-255），让 runtime 自行量化**（根因修复：FP16 喂 0-1 half 会"失明"）
- `run()` / `get_outputs()` / `get_raw_outputs()` / `infer()`
- `destroy()` 完整释放 context + mem
- 生命周期边界：RKNNEngine = 单 Worker 独立实例，WorkerPool 持有 `std::unique_ptr`

### 7. ModelManagement（core/src/model/ModelManagement.hpp / .cpp）

- 持有 ModelRegistry + 注入 validator
- 默认 `file_level_validator`：文件存在 + 非空 + ≥1KB（防手滑传文本文件），**不能解析 RKNN 头**
- 生产环境（板端 TTBOX_CORE_HAS_RKNN）：注入真 RKNN 探测 validator（Application.cpp:391）

### 8. Application 模型管理接线（core/src/app/Application.cpp）

- `model_registry_root` config 指定仓库根（当前 `/opt/ttbox/models`）
- IPC 注册：MODEL_LIST / MODEL_IMPORT / MODEL_VALIDATE / MODEL_INSTALL / MODEL_ACTIVATE / MODEL_REMOVE
- `handle_model_import`：路径安全（只允许 `_incoming/` 内文件）
- `handle_model_list`：active 以 `config.model_label / config.model_path` 实际加载为准（F004 修复，registry active 仅参考）
- `handle_model_activate`：**激活后需要重启 AI 流水线才加载新模型（当前 Core 无热加载能力，如实告知 UI）**
- `handle_config_update`：内存热更新 + 落盘 `/opt/ttbox/config/default.json` 的 runtime_profile 键

### 9. Web API（plugins/web/bin/ttbox-web.py，3589 行）

模型相关端点：
- `GET /api/models` — MODEL_LIST → 模型库清单 + selected_model_id
- `POST /api/models/import` — .rknn 上传 → `_incoming/` → MODEL_IMPORT → VALIDATE → INSTALL
- `POST /api/models/select` — 检查 installed 存在 → MODEL_ACTIVATE → 同步 config model_label/model_path
- `POST /api/models/delete` — MODEL_REMOVE
- `POST /api/models/class-names` — 直接写 installed manifest.json 的 class_names/class_count
- `POST /api/models/rknn-concurrency` — 写 manifest（重启 AI 后生效，诚实提示）
- `POST /api/models/cloud-encrypted` — 本地模式诚实返回 503（不假装）
- `POST /api/models/bind-preset` / `game-profile` / `remote-frame-format` / `hailo-pipeline-depth` — 写 manifest 字段
- `GET /api/models/device-code` — 设备指纹

### 10. RuntimeConfig 模型相关配置

`/opt/ttbox/config/default.json` 顶层键（复刻 YU 平铺格式）：
- `model_path` / `model_label` / `model_input_width` (256) / `model_input_height` (256)
- `model_color_order` (rgb) / `model_class_names_text` / `model_notes` / `model_uploaded`
- `model_registry_root` (`/opt/ttbox/models`)
- `runtime_profile.model_id` (`jwdl_sjzv11`)

### 11. 模型切换生命周期现状

**无热切换能力。** 现状：
1. `POST /api/models/select` → MODEL_ACTIVATE（写 registry/active.json + 校验器探测）
2. 同步 config 的 model_label/model_path
3. **需要重启 Core/ 重启 AI 流水线才加载新模型**（Application.cpp:788 注释明确）
4. WorkerPool 在 CoreRuntime.start() 时按 config 读模型，每 worker 独立 RKNNEngine

### 12. Core 重启恢复

- 启动时 `config.default.json` → build_runtime_params → model_path/label → WorkerPool 各 worker RKNN init
- RuntimeProfile 加载（runtime_profile 键 → model_id）
- **没有**"从 registry active 恢复并验证"的管线；恢复完全依赖 config 的 model_path 文件存在

### 13. 转换工具链现状

**仓库内没有 .pt→ONNX→RKNN 转换脚本/工具**（已排查）：
- 无 `tools/onnx*`、无 `scripts/convert*`、无 rknn-toolkit2 依赖声明
- `core/src/bench/npu_bench.cpp`：多 context 吞吐探针（rknn_init + 并发 run）
- `core/tools/yolo_probe.c`：YOLO 探针（板端）
- 当前模型的来源/转换过程未在仓库留档（转换工具版本未知）
- RKNN Runtime 1.5.2 对应的转换器通常是 rknn-toolkit2 1.5.x（未验证，勿断言）

### 14. 现有测试（真实文件）

- `core/tests/test_model_registry.cpp`：临时目录 + fake validator，验证 import→validate→install→activate→remove、禁止删 active、quarantine
- `core/tests/test_model_adapter.cpp`：板端真实 RKNN，验证 analyze/create_decoder
- `core/tests/test_model_runtime.cpp`：板端 3 轮加载/卸载 + 30 帧推理（每模型）
- `core/tests/test_model_switch_hw.cpp`：真实 validator + 切换 + 激活失败恢复 + 泄漏 3 轮
- `core/tests/test_ipc_model.cpp`：IPC 模型消息
- CMake 中 ctest 引用模型路径为 `/opt/ttbox2/models/...`（旧路径，板端实际是 `/opt/ttbox/models/...`，需要修的测试参数）

---

## 二、已确认的架构红线（禁止违反）

1. **不修改 Web 前端**（web/static、web/templates）
2. **不重写 TTBOX 总体架构**
3. **不新建第二套 RuntimeConfig / ModelManager / 缓存系统**
4. **不重写 TargetSelector / PID / HID / Preview 架构**
5. **预览与模型输入彻底解耦**（Preview = 原始 Capture 中心可调 Crop，默认 640×640；模型输入 256/320/416/640 都不能影响 Preview）
6. **不允许为单个模型写死推理流水线**；模型差异全部进 ModelAdapter/Decoder
7. **禁止假 PASS**：验证失败必须 INVALID/UNSUPPORTED/LOAD_FAILED，不能加载失败报成功
8. 状态字符串对齐 YU：`disabled/enabled/idle/not_running/未导入模型` 等

---

## 三、已完成且验证（PASS）

| 项 | 证据 |
|---|---|
| 当前模型真实加载运行 | 板端日志 `rknn_init OK` / 模型信息 / 零拷贝绑定，3 worker 全就绪 |
| ModelRegistry 生命周期 | test_model_registry（单测） |
| ModelAdapter 结构推断 | test_model_adapter / test_model_switch_hw（板端） |
| 激活失败回滚旧模型 | ModelRegistry::activate 源码 + test_model_switch_hw |
| 无 manifest 模型可列出 | ModelRegistry::list 兼容分支 |
| INT8/FP16 统一喂 UINT8 像素 | RKNNEngine::set_input + 真实模型运行 |
| Preview 可调中心 Crop | 上一阶段真机验证：640/800 切换，JPEG 头实测 |

## 四、未完成（技术性，可续接）

1. **设计文档未写**：`docs/web/TTBOX_MODEL_SYSTEM_DESIGN.md`、`docs/web/TTBOX_MODEL_CONVERSION.md` 均不存在
2. **manifest.json 未统一**：板端 installed 模型没有 manifest.json（只有 3 字段 metadata.json）
3. **模型校验器不完整**：只探测 input_width/input_height/output_count，缺 class_count/decode_type/一次真实推理
4. **无模型热切换**：切换需重启 Core（可续接：RuntimeController/WorkerPool 热重载）
5. **转换工具链不完整**：无 .pt→ONNX→RKNN 脚本、无 INT8 calibration 流程、无转换后自动验证
6. **测试 ctest 模型路径过期**：CMakeLists 中 `/opt/ttbox2/models/...` 应为 `/opt/ttbox/models/...`
7. **重启恢复管线**：Core 重启后从唯一配置恢复模型并进行真实推理验证（当前只靠 config.model_path 文件存在）

## 五、已停止（不做，无半成品）

- 本轮未写任何模型系统功能代码（保持只读调查，符合"先调查、后设计"要求）
- 未新建任何转换脚本 / 未安装 rknn-toolkit2 / 未改模型文件
- 未触碰 Web 前端、EDID、PID、HID、Preview 架构（预览阶段的可调 Crop 改动除外，那是上一阶段已验收产物）

---

## 六、接手第一步

1. 读本文档 + 以下核心源码建立完整认知：
   - `core/src/model/ModelRegistry.hpp/.cpp`
   - `core/src/model/ModelAdapter.hpp/.cpp`
   - `core/src/model/ModelMetadata.hpp`
   - `core/src/rknn/RKNNEngine.hpp/.cpp`
   - `core/src/model/ModelManagement.hpp/.cpp`
   - `core/src/app/Application.cpp`（模型回调段 379-445 / 702-804）
   - `plugins/web/bin/ttbox-web.py`（模型 API 段 1334-1612）
2. 板端快速复现当前状态：
   ```bash
   ssh ttbox53 'curl -s http://127.0.0.1:8000/api/models; find /opt/ttbox/models -maxdepth 4 -type f -printf "%p %s\n"'
   ```
3. 先写 `docs/web/TTBOX_MODEL_SYSTEM_DESIGN.md`（复用本文档"当前状态"段 + 目标设计），再写 `docs/web/TTBOX_MODEL_CONVERSION.md`
4. 任何设计必须遵守第二节红线；每次改动需本地 diff 检查 + 板端真实编译验证

---

## 七、关键决策依据（调查结论）

1. **不创建第二套模型配置系统**：现有 `ModelRegistry + ModelManifest + metadata.json` 已覆盖需求，manifest.json 直接扩展（新增字段），不重造
2. **DecodeNMS 与模型输出的耦合点已收敛**：所有格式分支在 ModelAdapter::infer_decode_type + DecodeNMS::process_* 内部，新模型到来时新增 decode 分支到 DecodeNMS，ModelAdapter 自动推断
3. **模型输入尺寸获取**：以 RKNN runtime `rknn_query(RKNN_QUERY_INPUT_ATTR)` 为准（256 实测），config model_input_width/height 是启动 fallback
4. **类别数量获取**：Adapter 从输出 dims 推断；类别名不在 RKNN 文件内，manifest/UI 维护
5. **多模型兼容验证已有测试骨架**：test_model_switch_hw / test_model_runtime 可演化成模型库验收
6. **Preview 解耦已定案**：模型输入变化绝不影响 Preview 尺寸（上阶段验收）