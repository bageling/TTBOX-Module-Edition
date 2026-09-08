# TTBOX 模型系统设计文档

> 版本：v1.0（设计稿，待用户验收后进入实现）
> 日期：2026-09-06
> 依据：板端真实运行状态 + 核心源码逐行调查（ModelRegistry / ModelAdapter / RKNNEngine / DecodeNMS / WorkerPool / Application / ttbox-web.py）

---

## 1. 当前模型架构总览（现状，非目标）

```
Web (ttbox-web.py :8000)
  │  POST /api/models/import|select|delete|class-names|rknn-concurrency ...
  ▼
IPC (Unix socket)
  ▼
Application (ttbox_core_main)
  ├─ ModelManagement → ModelRegistry（文件仓库：/opt/ttbox/models）
  │     installed/<model_id>/{model.rknn, metadata.json}
  │     registry/active.json
  ├─ RuntimeConfig（内存热更新 + /opt/ttbox/config/default.json 落盘）
  └─ CoreRuntime
        └─ WorkerPool（N=worker_cores，每个 Worker 独立 RKNNEngine + Decoder）
              RKNNEngine.init(model_path) → rknn_query → 零拷贝绑定 → 推理
              DecodeNMS.process() → detections → TargetSelector → AimThread
```

**当前真实运行模型**：`jwdl_sjzv11`，256×256 INT8 NHWC，6 输出，加载 ~20ms，零拷贝 I/O 已绑定。

## 2. 文件与目录结构（现状）

```
/opt/ttbox/models/
├── installed/
│   └── jwdl_sjzv11/
│       ├── model.rknn        10,980,940 B (sha256 1592a676…)
│       └── metadata.json     {"input_width":256,"input_height":256,"output_count":6}
├── registry/
│   └── active.json           {"model_id":"jwdl_sjzv11","activated_at":…}
├── _incoming/                （上传收件目录，import 只允许引用此目录内文件）
├── staging/  cache/  quarantine/   （ModelRegistry 预留，当前为空）
```

**关键缺口**：`installed/<id>/` 下没有 `manifest.json`、没有 `labels.txt`、没有 `validation/`。ModelRegistry::list 已有兼容分支（无 manifest 但有 model.rknn 也能列出）。

## 3. ModelRegistry 职责与实现（现状）

源码：`core/src/model/ModelRegistry.cpp`（459 行）

| 操作 | 行为 | 失败处理 |
|---|---|---|
| `import(src, id, manifest)` | 校验 src 在 `_incoming/` 内 → 拷入 staging | 路径非法直接拒绝 |
| `validate(id)` | 调用注入的 validator → 写 `validation/ok.json` + 合并探测尺寸 | 失败留 staging |
| `install(id)` | 要求已有 validation/ok.json → staging→installed | 未验证拒绝 |
| `activate(id)` | 激活前**再次跑 validator**，通过才写 active.json | 失败**保持旧激活**（自动回滚） |
| `remove(id)` | 禁止删除 active 模型 | 拒绝 |
| `list()` | 兼容无 manifest 但有 model.rknn | 正常列出 |

ModelManifest 字段（已实现）：model_id / label / version / sha256 / signature / origin / converter_version / runtime_version / input_width / input_height / output_count / class_count / class_names / rknn_concurrency / status / created_at。

## 4. ModelAdapter 职责与实现（现状）

源码：`core/src/model/ModelAdapter.cpp`（258 行）

- `analyze(info, cfg)`：从 RKNN runtime 查询结果构建 ModelMetadata（输入尺寸/类型/布局/输出结构）
- `infer_decode_type()`：**按输出结构推断，不按模型名**：

| 输出结构 | decode_type | 代表模型 |
|---|---|---|
| 单输出 3D `[1,N,F]`，F≤16 | kE2e | v26m 端到端 |
| 单输出 `(1,C,M)`，C≥5 | kSingle | yolo261n 后处理融合 |
| ≥6 输出成对 reg/cls（reg dims[1]≥32 且 %4==0） | kDflPairDist | sjz-XCSH 256（当前） |
| ≥2 偶数输出成对 box/cls | kDfl | 黄瓦 320 双尺度 |
| 3 的倍数 reg/cls/aux | kDflDist | 大腕 6 输出 DFL |

- `infer_class_count()`：从输出通道数推断（Single: C-4==81 → 80 类，否则 C-4；DFL: cls 通道数）
- `create_decoder()`：注入 conf/iou/class_filter/max_detections/input 尺寸 → DecodeNMS

**现状问题**：Application.cpp:391-417 注入的校验器只写 input_width/input_height/output_count 三项，**class_count/decode_type 未持久化**（metadata.json 只有 3 字段的原因）。

## 5. RKNNEngine 生命周期（现状）

源码：`core/src/rknn/RKNNEngine.cpp`（486 行）

```
init()   rknn_init → rknn_query(IN_OUT_NUM) → INPUT_ATTR(NHWC: dims[1]=H,dims[2]=W) → OUTPUT_ATTR
         → init_zero_copy()（rknn_create_mem + rknn_set_io_mem）
run()    set_input（INT8/FP16 模型统一喂 UINT8 原始像素 0-255，runtime 自行量化）
         → run_zero_copy → get_outputs
destroy()  rknn_destroy + 释放 mem
```

- 单 Worker 持有一个独立 RKNNEngine 实例（`unique_ptr`）
- 加载耗时实测 ~20ms，`info()` 提供动态输入尺寸（Worker 启动时读取，**不硬编码**）

## 6. 解码/NMS 与模型输出的耦合点（现状）

源码：`core/src/rknn/DecodeNMS.cpp`（833 行）

```
DecodeNMS::process(info, out_bufs, detections)
  ├─ 单输出 3D [1,N,F] F≤16       → process_e2e   （v26m：已解码的 xyxy+conf+cls）
  ├─ 单输出 (1,C,M) C≥5            → process_single（yolo261n：xywh + 类别概率，C-4==81 时乘 objectness）
  ├─ ≥6 输出成对 reg/cls           → process_dfl_pair_dist（当前 256 模型：reg 64 bin / cls 7）
  └─ ≥2 偶数成对 box/cls           → process_dfl
```

**已确认**：新模型只要输出结构落入以上四类之一，**无需改 C++**；结构全新才需要新增一个 process_* 分支（在 DecodeNMS.cpp 内加，不影响外部）。

## 7. WorkerPool 与模型加载（现状）

源码：`core/src/rknn/WorkerPool.cpp`（408 行）

- `start(params)`：按 `worker_cores` 数量创建 N 个 `InferenceWorker`，每个 worker：
  - `RKNNEngine.init(model_path)` ← 来自 `config.model_path`
  - `Preprocess.init`（RGA/CPU）
  - `ModelAdapter.analyze` + `create_decoder`
- 输入尺寸：从 `engine_->info().input_width/height` 动态取（不读 config 硬编码）
- **无热加载能力**：start 后不能换模型；换模型必须 stop() → 改 config → 重新 start()

## 8. 模型切换流程（现状）

```
POST /api/models/select {model_id}
  → Web 检查 installed/<id>/model.rknn 存在
  → IPC MODEL_ACTIVATE → ModelRegistry.activate()
       ├─ 再次 validator 探测（RKNN init + input/output 尺寸）
       ├─ 通过 → 写 registry/active.json
       └─ 失败 → 保持旧 active（不破坏当前模型）
  → Web 同步写 config：model_label / model_path / model_input_width / model_input_height
  → 返回 {"ok":true, "restart_required":true}（诚实告知）
  → 用户重启 ttbox-core → build_runtime_params 读 config → WorkerPool 加载新模型
```

**现状结论：切换后必须重启 Core 才生效**（Application.cpp:788 注释明确，UI 已如实提示）。切换失败不会搞死当前模型（activate 失败不碰 active.json + 不碰 config）。

## 9. Web 模型 API 全表（现状）

| 端点 | 方法 | 行为 |
|---|---|---|
| `/api/models` | GET | 模型库清单 + selected_model_id |
| `/api/models/device-code` | GET | 设备指纹 |
| `/api/models/cloud-encrypted` | POST | 本地模式诚实 503 |
| `/api/models/import` | POST | .rknn 上传 → `_incoming/` → MODEL_IMPORT → VALIDATE → INSTALL |
| `/api/models/delete` | POST | MODEL_REMOVE（禁删 active） |
| `/api/models/select` | POST | MODEL_ACTIVATE + 写 config（重启生效） |
| `/api/models/class-names` | POST | 写 manifest.json 的 class_names/class_count |
| `/api/models/rknn-concurrency` | POST | 写 manifest（重启生效） |
| `/api/models/bind-preset` | POST | 写 manifest 绑定预设 |
| `/api/models/game-profile` | POST | 写 manifest 游戏档 |
| `/api/models/remote-frame-format` | POST | 写 manifest |
| `/api/models/hailo-pipeline-depth` | POST | 写 manifest（hailo 预留） |

## 10. RuntimeConfig 模型相关配置（现状）

`/opt/ttbox/config/default.json`（平铺复刻 YU 格式）：

```json
{
  "model_path": "/opt/ttbox/models/installed/jwdl_sjzv11/model.rknn",
  "model_label": "jwdl_sjzv11",
  "model_input_width": 256,
  "model_input_height": 256,
  "model_color_order": "rgb",
  "model_registry_root": "/opt/ttbox/models",
  "runtime_profile": { "model_id": "jwdl_sjzv11", "capture": {"width":640,"height":640}, ... },
  "worker_cores": "2,3,4"
}
```

**单一真源**：config 的 model_path/model_label 决定 WorkerPool 实际加载；registry active.json 仅参考（F004 已修复脱节）。

## 11. 模型校验现状（真实）

- 文件级：非空 + ≥1KB（ModelManagement 注入的 fallback）
- 板端：RKNN init 探测（能加载 + 读 input/output 尺寸），**不做真实推理验证**
- 校验结果持久化：只写 input_width/input_height/output_count 到 metadata.json

**缺口**：无真实推理验证、无 class_count/decode_type 持久化、无 sha256 校验（manifest.sha256 字段存在但未用）。

## 12. Core 重启恢复（现状）

```
systemctl restart ttbox-core
  → initialize() → ConfigManager 读 default.json
  → build_runtime_params() → model_path（空则用 model_label 拼 /opt/ttbox/models/<label>/<label>.rknn）
  → CoreRuntime.start() → WorkerPool.start() → 每 worker RKNNEngine.init
  → 加载失败 → worker 启动失败 → 整个 start 失败（fail-closed）
```

**现状结论：重启自动恢复当前模型**（只要 config.model_path 指向的文件存在且可加载）；但**无**"从 registry active 恢复 + 真实推理验证"的管线，config 文件是唯一真源。

---

## 13. 目标架构（设计稿）

### 13.1 模型库最终目录结构

```
/opt/ttbox/models/
├── installed/
│   └── <model_id>/
│       ├── model.rknn              ← 转换产物（唯一必选）
│       ├── manifest.json           ← 模型元数据（最终版，见 13.2）
│       ├── labels.txt              ← 类别名（每行一个，可选，UI 可覆盖）
│       ├── metadata.json           ← 保留（runtime 探测结果，与 manifest 合并语义）
│       └── validation/
│           ├── ok.json             ← 校验通过记录（含实测尺寸/decode_type/class_count）
│           └── smoke_result.json   ← 真实推理冒烟结果（帧率/首个检测）
├── registry/active.json            ← 当前激活（参考）
├── _incoming/                      ← 上传收件目录
├── staging/                        ← import 落点
├── cache/                          ← 已删除模型的回收（可恢复）
└── quarantine/                     ← 校验失败隔离
```

### 13.2 manifest.json 最终设计

```json
{
  "model_id": "jwdl_sjzv11",
  "label": "自定义显示名",
  "version": "1.0.0",
  "sha256": "1592a676…",              ← 上传时计算，校验时核对
  "origin": "local_upload",
  "converter": {
    "toolkit": "rknn-toolkit2",
    "version": "1.5.2",                ← 与板端 librknnrt 匹配要求
    "target": "rk3588",
    "quant": "int8",
    "calibration_set": "自定义数据集 500 张"
  },
  "runtime": {
    "input_width": 256,
    "input_height": 256,
    "color_order": "rgb",
    "decode_type": 3,                  ← ModelAdapter 推断结果持久化
    "class_count": 7,
    "class_names": ["类别0", "类别1", "…"],
    "rknn_concurrency": 1
  },
  "status": "installed",
  "created_at": 1788613124
}
```

**兼容策略**：读取时优先 manifest.json；缺失则走 ModelRegistry::list 兼容分支（已有）+ 探测写回（升级路径）。

### 13.3 ModelRegistry 职责（目标）

保留现有全部行为，新增：
1. `import` 时计算并记录 sha256
2. `validate` 调用增强 validator（见 13.5）
3. `activate` 成功后把探测结果回写 manifest.json（class_count/decode_type/实测尺寸）
4. 删除模型移动至 `cache/`（保留 7 天），禁删 active 不变

### 13.4 ModelAdapter 职责（目标）

- 保持"按输出结构推断"不变
- `analyze` 输出扩展：`output_names`（可选，rknn_query 支持时）
- 新增 `validate_inference()`：对真实输入跑 1 次推理，确认能出结果、输出尺寸与推断一致
- decode_type 推断结果持久化到 manifest（validator 阶段写入）

### 13.5 转换 Pipeline（目标）

```
.pt (Ultralytics YOLOv8/11/26)
  │  torch.onnx.export（opset=12，动态 batch 关闭，CPU）
  ▼
.onnx  （check：onnx.checker + 输出名/sim 后结构）
  │  rknn-toolkit2 1.5.x（与板端 librknnrt 1.5.2 匹配）
  │  config: mean/std 或归一化方案、量化数据集（INT8 需要）、target_platform=rk3588
  ▼
.rknn
  │  校验①：rknn-toolkit 侧模拟运行（python inference）
  │  校验②：板端 RKNNEngine.init + 结构分析（ModelAdapter）
  │  校验③：板端真实推理冒烟（至少 1 次真实输入 → 有输出、无报错）
  ▼
manifest.json + model.rknn + labels.txt
  ▼
上传 → import → validate → install → activate
```

### 13.6 热切换 Pipeline（目标，实现阶段评估）

现状无热加载。目标方案（不重构架构，给 WorkerPool 加 reload）：

```
POST /api/models/select
  → MODEL_ACTIVATE（registry + validator 前置校验，失败回滚）
  → 新模型文件已在 installed（已 install）
  → Web 写 config（model_label/model_path/尺寸）
  → IPC MODEL_RELOAD（新消息，v0.4）
  → WorkerPool.reload_model(path)：
      先 new 一个 RKNNEngine + Adapter + Decoder（完整校验+冒烟）
      成功 → 原子替换 workers 内 engine/decoder（旧 engine destroy）
      失败 → 保持旧 worker 继续跑（当前模型不受影响）
  → 返回 {"ok":true, "hot":true}
```

**风险**：Worker 循环中换 engine 需要锁/双缓冲；失败必须保留旧模型。实现顺序排在文档验收之后。

### 13.7 重启恢复 Pipeline（目标）

```
Core 启动 → config → WorkerPool.start()
  → 每 worker：RKNNEngine.init
  → 启动后自动对当前模型做一次冒烟推理（1 帧真实输入）
  → 失败 → 日志告警 + 服务保持（不 fail-closed 于模型冒烟，避免整机起不来）
  → 成功 → 正常流水线
```

### 13.8 缓存与回滚策略

- 删除模型 → `cache/<model_id>` 保留 7 天，可从 cache 恢复 install
- 激活失败 → active.json 不变（已有）；config 不变（Web 层只有在 ACTIVATE ok 后才写 config）
- 热切换失败 → 旧 worker 继续（13.6 设计）；冷切换失败 → 旧 config 继续（启动时读的是旧 config）

### 13.9 兼容 v8/v11/v26 方案（目标）

| 模型 | 输出结构 | decode_type | 需改 C++？ |
|---|---|---|---|
| YOLOv8（黄瓦 320 实测） | 多尺度 box/cls 或 DFL 结构 | 以实际 tensor 为准：`kDfl`/`kDflDist` | 否（现有结构覆盖时） |
| YOLOv11（yolo261n 640 实测） | 单输出 `(1,84,8400)` | `kSingle` | 否（已有） |
| YOLOv26（v26m 640 实测） | 单输出 `[1,300,6]` | `kE2e` | 否（已有） |
| 新结构模型 | 未知 | 未知 | 只新增 Adapter/Decoder 分支 |

**结论：v11→v26 不需要改 C++ 核心**（两者都走"单输出"分支，由 ModelAdapter 自动区分 E2e/Single）。

---

## 14. 现有代码改动清单（实现阶段）

| 文件 | 改动 | 理由 |
|---|---|---|
| `core/src/app/Application.cpp` | validator 扩展：写 class_count/decode_type/冒烟推理 | 校验补全（红线 7） |
| `core/src/model/ModelRegistry.cpp` | import 算 sha256；activate 回写 manifest；delete→cache | 13.3 |
| `core/src/model/ModelAdapter.cpp` | 新增 validate_inference() | 13.4 |
| `core/src/rknn/WorkerPool.cpp` | 新增 reload_model()（可选 v0.4） | 13.6 |
| `plugins/web/bin/ttbox-web.py` | select 流程加冒烟反馈；manifest 读写对齐 | 配合 |
| `core/tests/CMakeLists.txt` | 修 ctest 模型路径 `/opt/ttbox2/` → `/opt/ttbox/` | 测试路径过期 |
| `docs/` | 本设计 + 转换文档 | 文档 |

## 15. 绝对不能改的文件（红线）

- `core/src/preview/PreviewModule.*`（Preview 与模型解耦已验收，模型改动不得触碰）
- `core/src/runtime/CoreRuntime.cpp` 的 Preview 段（同上）
- `plugins/web/static/*` / `plugins/web/templates/*`（前端禁改）
- `core/src/target/*`（TargetSelector/PID 禁改）
- `core/src/hid/*`（HID 禁改）
- EDID 注入相关（已完成）
- 不新建第二套 RuntimeConfig / ModelManager / 模型缓存

---

## 16. 验收标准（5 问直接回答）

### Q1：新模型放入 TTBOX 需要什么步骤？

```
① 用 rknn-toolkit2 1.5.x 把 .pt/.onnx 转成 rk3588 的 .rknn（INT8 需量化数据集）
② 校验①：转换端模拟推理通过
③ 校验②：板端 RKNNEngine 能 init（旧流程自动做）
④ 校验③（目标）：板端真实推理冒烟 ≥1 次通过
⑤ 准备 labels.txt（每行一类，可选）+ 填写 manifest 元数据（目标流程）
⑥ 浏览器 → 模型库 → 上传 .rknn → import → validate → install（已实现）
⑦ 模型库 → 选择该模型 → select → 重启 ttbox-core（当前）→ 生效
   （热切换实现后：select 直接生效）
⑧ /api/models 确认 class_count/input 尺寸正确（目标流程自动持久化）
```

### Q2：v11 → v26 需要改 C++ 代码吗？

**不需要。** v11（yolo261n）走 `kSingle` 单输出分支，v26m 走 `kE2e` 单输出分支，两者都由 `DecodeNMS::process` 的单输出判断自动分发（dims[2]≤16 → e2e，否则 single），`ModelAdapter::infer_decode_type` 自动推断。当前代码已实测兼容两类（测试：test_model_adapter / test_model_switch_hw）。

### Q3：模型输入 320 → 416 时 Preview 会变化吗？

**不会。** Preview 输出尺寸 = `runtime_profile.capture.width/height`（默认 640×640 中心裁剪），与模型输入完全解耦（已验收：crop_size 640→800→640 实测 JPEG 跟随）。模型输入 320→416 只影响：
- `model_input_width/height` config（WorkerPool 从 `engine_->info()` 动态读取，其实 config 值仅作 fallback）
- 推理输入张量大小
- Preview 画框坐标由 `map_coords` 换算（模型尺寸变化不影响输出帧尺寸，只影响缩放系数）

### Q4：切换失败会搞死当前模型吗？

**不会。** 现状：`MODEL_ACTIVATE` 前 validator 失败 → active.json 不变 + Web 不写 config → 当前模型继续运行。目标热切换同样保证：reload 失败 → 旧 worker 继续（13.6）。删除 active 模型也被 ModelRegistry::remove 拒绝。

### Q5：Core 重启会自动恢复当前模型吗？

**会。** 现状：重启 → ConfigManager 读 `default.json` → `build_runtime_params` 用 `model_path`（空则按 `model_label` 拼路径）→ WorkerPool 加载。只要 config 里的路径指向存在的可加载 .rknn，重启自动恢复（板端已验证多次：重启后模型信息日志正常出现）。目标增强：启动后自动冒烟推理确认（13.7）。

---

## 17. 已确认的现存问题清单（实现阶段处理）

| # | 问题 | 位置 | 影响 |
|---|---|---|---|
| M1 | metadata.json 只有 3 字段，class_count/decode_type 未持久化 | Application.cpp validator | UI 显示 class_count=0 |
| M2 | 无 manifest.json（安装流程未生成） | ModelRegistry/Web | 元数据缺失 |
| M3 | 校验无真实推理冒烟 | validator | 模型"能加载但推理错误"无法提前发现 |
| M4 | 无热切换，select 后需重启 Core | Application.cpp:788 | 用户体验差 |
| M5 | ctest 模型路径 `/opt/ttbox2/` 过期 | core/tests/CMakeLists.txt | 板端测试跑不起来 |
| M6 | 删除模型直接删文件（无 cache 回收） | ModelRegistry::remove | 误删不可恢复 |
| M7 | sha256 字段未使用 | ModelRegistry | 传输/文件完整性无校验 |
| M8 | 板端 librknnrt 1.5.2 旧，新 toolkit 转换的模型可能不兼容 | 运行库 | 转换版本必须匹配 |

---

## 18. 里程碑

| 阶段 | 内容 | 验收 |
|---|---|---|
| 本阶段（当前） | 调查 + 本设计文档 + 转换文档 | 5 问回答明确（§16） |
| 实现 A | validator 扩展（class_count/decode_type/冒烟）+ manifest 生成 | 上传新模型全链路真实 PASS |
| 实现 B | import sha256 + delete→cache + ctest 路径修复 | 单测 + 板端测试 PASS |
| 实现 C（可选） | WorkerPool reload 热切换 | select 不重启生效 + 失败回滚实测 |
| 实现 D | 重启冒烟确认 | 重启后日志有 smoke 结果 |

## 19. 测试策略

- 单测（PC）：ModelRegistry 生命周期（fake validator）、ModelAdapter 结构推断、DecodeNMS 各分支（合成张量）
- 板端硬件测试：`test_model_adapter` / `test_model_runtime` / `test_model_switch_hw`（已存在，路径修正后可用）
- E2E：上传 .rknn → select → 重启 → /api/models 校验 → 真实检测框
- 回归：API 层 24/24、Hotkey 8/8、Movement 9/9、Preview 640/800 闭环（已有脚本重跑）

## 26. 进入实现阶段的固定接口

### 26.1 Registry 返回对象

Registry 不再只返回 `ModelManifest`，实现阶段增加只读 `ModelRecord`：

```text
model_id
model_dir
model_path
manifest
runtime_metadata
availability
failure_code
failure_message
```

`availability` 只允许：`READY`、`INVALID`、`UNSUPPORTED`、`LOAD_FAILED`、`ACTIVE`。Web 和 Core 只消费这个对象，不自行拼接 `/opt/ttbox/models` 路径。

### 26.2 唯一当前模型对象

运行时维护一个不可变的 `CurrentModelSnapshot`：

```text
model_id
model_generation
state
model_path
metadata
loaded_at
last_validation
last_error
```

- Registry 的 `active.json` 是持久化选择。
- `CurrentModelSnapshot` 是当前进程实际加载结果。
- `/api/models` 同时返回 `selected_model_id` 和 `running_model_id`；两者不一致时状态必须是 `SWITCHING` 或 `LOAD_FAILED`，不能伪装一致。

### 26.2.1 运行模型提交门（已实现，2026-09）

`core/src/model/ModelRuntimeState.hpp` 实现了「新模型状态提交门」，是 selected→running 的唯一合法通道：

```text
select(new_id)                    # 记录目标模型，清空 failure_code
mark_init_failed()                # rknn_init 失败 → failure_code=RKNN_INIT_FAILED
mark_inference_failed()           # 推理失败 → INFERENCE_FAILED
mark_decode_failed()              # 解码失败 → DECODE_FAILED
mark_inference_pass()             # 推理成功（仅标记，不提交）
mark_decode_pass()                # 解码成功（仅标记）
commit_ready()                    # 仅当 推理PASS && 解码PASS && 目标非空 时，
                                  # 才把 running_model_id 更新为目标，清空 failure_code
```

核心保证：

- init / 推理 / 解码任一失败 → `running_model_id` 保持旧模型不变，只更新 failure_code。
- 只有推理和解码都真实成功后，running_model_id 才切换为新模型（无假 PASS）。
- 已有测试覆盖：
  - `core/tests/test_model_runtime_state.cpp`（4 条状态门断言）
  - `core/tests/test_rknn_failure_injection.cpp`（PC 跑状态门全路径；板端通过
    `RKNNEngine::Params::test_init_hook / test_run_hook`（仅测试注入，生产为空）额外验证
    Engine 接线，ctest 名 `test_rknn_failure_injection`，双平台均通过）。
- 应用层（Application.cpp）已有对应消费：启动失败/重试失败写 `model_failure_code_`
  （RKNN_INIT_FAILED / INFERENCE_FAILED_OR_OUTPUT_INVALID），
  `model_ready() && running_model_id_ != active` 时才提交 running 并清码。

### 26.3 IPC 模型命令

保留现有命令名，统一返回真实状态：

```text
MODEL_LIST
MODEL_IMPORT
MODEL_VALIDATE
MODEL_INSTALL
MODEL_ACTIVATE
MODEL_REMOVE
MODEL_STATUS
MODEL_SWITCH
```

`MODEL_ACTIVATE` 只改变 Registry 选择；`MODEL_SWITCH` 才执行运行时切换。这样不会再出现“active.json 已改，但 Core 仍跑旧模型却 API 返回成功”。

### 26.4 模型状态机

```text
ABSENT
  → STAGING
  → VALIDATING
  → READY
  → SWITCHING
  → RUNNING

VALIDATING → INVALID / UNSUPPORTED / LOAD_FAILED
SWITCHING  → RUNNING（B 成功）
SWITCHING  → ROLLBACK（B 失败）
ROLLBACK   → RUNNING（A 恢复）
ROLLBACK   → LOAD_FAILED（A 也失败）
RUNNING    → STOPPED
```

状态转换必须由 Core/Registry 统一写入；Web 不直接写 `status`。

## 27. 目录迁移方案

当前板端使用 `/opt/ttbox/models/installed/<model_id>/`，最终目标是 `/opt/ttbox/models/<model_id>/`。迁移不得一次性破坏旧模型，按以下顺序实施：

1. Registry 同时识别新目录和旧 `installed/` 目录。
2. 新导入模型只写 `/opt/ttbox/models/<model_id>/`。
3. 启动扫描时为旧目录生成兼容 `ModelRecord`，缺 manifest 的旧模型标记 `INVALID` 或 `LEGACY_UNVERIFIED`，不能标 READY。
4. 对当前 `jwdl_sjzv11` 生成完整 manifest，并完成真实验证后再移动。
5. `active.json` 只保存 `model_id`，模型绝对路径由 Registry 解析。
6. 连续两个版本验证通过后，再删除旧 `installed/` 兼容分支。

不允许 Web 直接访问 `installed/`、`staging/` 或 `quarantine/`。

## 28. 现有实现与最终目标差异矩阵

| 能力 | 当前真实状态 | 最终实现要求 |
|---|---|---|
| 模型扫描 | Registry 主要列 installed，兼容无 manifest | 扫描 `/opt/ttbox/models/<id>` 并注册每个状态 |
| manifest | C++ 旧字段已存在，板端缺失 | manifest v2 必须存在且字段校验严格 |
| checksum | 字段存在，未强制重算 | import/install/activate 前强制 SHA256 |
| metadata | validator 只写部分字段，API 出现 0 | Adapter 全量 metadata 写入 validation/report |
| 真实推理 | 现有测试骨架不等于入库门槛 | 板端一次真实推理成功才 READY |
| 模型选择 | active.json + config/model_path 并存 | Registry active 为唯一持久化选择 |
| 当前运行模型 | API 主要返回 active/selected | 增加 running_model_id 和 generation |
| 模型切换 | stop/start，需重启 Core | Core 内 drain、加载、首帧、提交、回滚 |
| 失败回滚 | 激活校验失败保留 active | 运行时失败恢复 A 并验证 A 首帧 |
| 删除 | active 禁删，直接 remove_all | 非 active 先移 cache，异步清理 |
| 转换 | ONNX→RKNN 脚本存在 | `.pt→ONNX→RKNN→manifest→板端验证` 闭环 |

## 29. 实现顺序不可打乱

### A：先修模型库真相

- manifest v2 解析和严格校验。
- SHA256 计算和比对。
- 扫描新目录并兼容旧目录。
- `MODEL_LIST/MODEL_STATUS` 返回真实可用性。
- 修复 API 中尺寸、输出数、类别数为 0 的问题。

验收：板端 `/api/models` 对 `jwdl_sjzv11` 明确返回 `INVALID` 或完整真实 metadata，不能继续返回“installed + 全 0”。

### B：再修入库验证

- RKNN init/query。
- Adapter 全量分析。
- 一次真实推理。
- Decode/NMS 合法性检查。
- 失败报告和 quarantine。

验收：任意验证失败模型不能进入 READY/ACTIVE。

### C：再做运行时切换

- WorkerPool drain。
- generation 丢弃旧任务。
- Context/DMA-BUF 安全释放。
- Detection/Target/Aim 缓存清理。
- B 首帧成功后提交。
- B 失败恢复 A。

验收：A 运行时切换坏模型，A 仍能继续运行或被重新加载恢复；API 返回真实结果。

### D：最后做重启恢复

- 启动只读取 Registry active + RuntimeProfile.model_id 规则。
- 验证、加载、首帧成功后 READY。
- 写入 `running_model_id`。

验收：配置、active、实际运行模型三者不一致时启动失败并明确报告。

## 30. 本阶段设计边界

本次继续设计仍不修改功能代码、不修改 Web 前端、不修改 Preview、TargetSelector、PID、HID 和 EDID。下一次进入实现时，优先从阶段 A 开始，先把当前 `jwdl_sjzv11` 的 manifest、metadata 和 `/api/models` 真实状态链打通，再进入热切换。

## 31. 模型库单模型记录

实现阶段统一使用一个只读 `ModelRecord` 表示模型，避免 Web、Registry、Runtime 各自拼装字段：

```text
ModelRecord {
  model_id
  model_dir
  model_path
  manifest
  metadata
  availability
  selected
  running
  generation
  failure_code
  failure_message
  validation_report_path
}
```

字段来源固定：

- `model_id/model_dir/model_path`：Registry 扫描结果。
- `manifest`：模型目录的 `manifest.json`。
- `metadata`：RKNN query + ModelAdapter 生成的运行时快照。
- `availability/failure_*`：Registry 验证结果。
- `selected`：`registry/active.json` 是否指向该模型。
- `running`：Core 当前 `CurrentModelSnapshot` 是否正在运行。
- `generation`：当前运行代次；切换时递增，旧任务必须丢弃。

`selected=true` 不等于 `running=true`。只有模型完成加载、首帧推理和状态提交后，两者才可以同时为 true。

## 32. 扫描和注册规则

Registry 初始化时执行一次完整扫描，之后由显式 `rescan()` 或导入/删除操作触发增量更新：

1. 扫描 `/opt/ttbox/models/<model_id>/` 一级目录。
2. 忽略 `registry/`、`staging/`、`quarantine/`、`cache/` 和隐藏临时目录。
3. 校验 model_id 只能包含字母、数字、`_`、`-`，拒绝路径穿越和目录嵌套。
4. 要求 `model.rknn` 与 `manifest.json` 同目录。
5. 缺文件、JSON 损坏或字段非法时注册为 `INVALID`，仍可在列表中显示失败原因。
6. 不从文件名推断模型类型、输入尺寸、类别数量或 decoder。
7. 旧 `installed/<model_id>/` 只作为迁移兼容来源，禁止新模型继续写入。
8. 同一个 model_id 若新目录和旧目录同时存在，以新目录为准；旧目录标记冲突，不自动覆盖。

扫描结果必须是内存快照；Web 只读取快照，不在请求线程重新遍历磁盘。

## 33. manifest 校验规则

manifest 不是装饰信息，而是模型安装契约。校验分为三层：

### 33.1 语法层

- JSON 必须是 object。
- `schema_version` 必须是支持的版本。
- 字符串不能为空，数值不得为负。
- model_id 必须和目录名一致。
- `format` 必须为 `rknn`。
- `task` 当前必须为 `detect`。
- `hardware` 必须包含 `rk3588`。

### 33.2 逻辑层

- `input_width/input_height` 在允许范围内且大于 0。
- `class_count > 0`。
- `class_names` 为空时状态只能是 metadata incomplete；非空时长度必须等于 class_count。
- `output_count > 0`。
- `output_format` 必须属于已注册 decoder 协议。
- `quantization` 必须和 `input_dtype` 匹配。
- checksum 算法当前固定 `sha256`。

### 33.3 运行层

- manifest 输入尺寸必须和 `rknn_query` 一致。
- manifest 输入 layout/dtype 必须和 runtime 一致。
- manifest output_count 必须和 runtime 一致。
- manifest class_count、output_format、stride 等必须和 Adapter 分析结果一致。
- 运行库版本不兼容时状态为 `UNSUPPORTED`，不能降级成 READY。

## 34. `/api/models` 最终返回契约

保持现有路由，不改前端路由；后端将返回真实字段：

```json
{
  "ok": true,
  "data": {
    "selected_model_id": "MODEL_A",
    "running_model_id": "MODEL_A",
    "state": "RUNNING",
    "generation": 7,
    "models": [
      {
        "model_id": "MODEL_A",
        "name": "MODEL_A",
        "version": "1.0.0",
        "status": "ACTIVE",
        "availability": "READY",
        "running": true,
        "input_width": 640,
        "input_height": 640,
        "input_layout": "NHWC",
        "input_dtype": "UINT8",
        "quantization": "int8",
        "class_count": 1,
        "class_names": ["person"],
        "output_format": "single_xywh_cls",
        "output_count": 1,
        "failure_code": "",
        "failure_message": ""
      }
    ]
  }
}
```

以下情况必须返回 `ok=false` 或明确失败状态：Registry 不可用、切换失败、目标模型不存在、校验失败、Core 恢复失败。不能用空数组加 `ok=true` 掩盖底层异常。

## 35. 模型库并发和文件一致性

Registry 的导入、安装、激活、删除、扫描必须经过同一把进程内互斥锁；文件操作采用临时文件加原子 rename：

- `manifest.json.tmp → manifest.json`
- `active.json.tmp → active.json`
- `validation/report.json.tmp → validation/report.json`

导入流程先写 staging，完成后再一次性提交模型目录；扫描不能读到半个 manifest 或半个模型文件。激活和删除冲突时，active 保护优先，删除必须失败。

跨进程情况下使用 lock file 或 systemd 级串行约束，至少保证 Web IPC 和 Core 启动扫描不会同时改同一模型目录。

## 36. 删除、替换和版本规则

当前 model_id 作为稳定身份，模型升级使用新 `version`；建议不在同一个目录原地覆盖正在运行的文件：

```text
staging/MODEL_ID-v2
→ validate
→ install MODEL_ID-v2
→ switch
→ 旧版本进入 cache
```

如果产品要求同一 model_id 升级，必须先完整验证新目录，再用原子目录替换；旧目录保留到新模型首帧成功后才能清理。active 模型、running 模型以及 rollback 依赖的上一模型均禁止删除。

## 37. 失败码统一表

| failure_code | 含义 | 状态 |
|---|---|---|
| `MODEL_NOT_FOUND` | 目录或模型文件不存在 | `INVALID` |
| `MANIFEST_MISSING` | 缺少 manifest.json | `INVALID` |
| `MANIFEST_INVALID` | JSON 或字段校验失败 | `INVALID` |
| `CHECKSUM_MISMATCH` | 文件与 manifest 不一致 | `INVALID` |
| `UNSUPPORTED_OUTPUT` | 没有可用 Decoder | `UNSUPPORTED` |
| `RUNTIME_INCOMPATIBLE` | Runtime/模型代差不兼容 | `UNSUPPORTED` |
| `RKNN_INIT_FAILED` | rknn_init 失败 | `LOAD_FAILED` |
| `RKNN_QUERY_FAILED` | 输入输出 metadata 查询失败 | `LOAD_FAILED` |
| `SMOKE_INFERENCE_FAILED` | 首次真实推理失败 | `LOAD_FAILED` |
| `DECODE_INVALID` | Decode 输出不合法 | `LOAD_FAILED` |
| `SWITCH_ROLLBACK_FAILED` | 新模型失败且旧模型恢复失败 | `LOAD_FAILED` |

API、日志、validation/report 和 manifest 状态使用同一 failure_code，避免前端收到一套、日志又是另一套。

## 38. 实现完成定义

模型库阶段只有同时满足以下条件才算完成：

1. `/opt/ttbox/models/<model_id>/model.rknn` 和 manifest 能被 Registry 扫描。
2. `/api/models` 返回真实输入尺寸、输出数量、类别数量，不再出现当前 `jwdl_sjzv11` 的全 0 metadata。
3. checksum、manifest、RKNN query、Adapter、一次真实推理和 Decode 全部通过后才是 READY。
4. 失败模型能显示具体状态和 failure_code，不进入 ACTIVE。
5. 当前选中模型和实际运行模型分开可见。
6. A→B 切换失败时 A 可继续运行或被验证恢复。
7. Core 重启后经过 Registry 验证和第一帧成功才进入 RUNNING。
8. 模型输入尺寸变化只影响 AI 输入和坐标映射，不影响 Preview 输出尺寸。
9. 新增已支持输出协议的 YOLO 模型不修改主推理流水线。

达到以上条件后，模型库才从“文件管理功能”升级为“可证明真实可运行的模型系统”。
