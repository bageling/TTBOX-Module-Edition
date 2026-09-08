# TTBOX 模型转换体系调查报告（ONNX → RKNN → ModelAdapter → C++ 推理）

> 日期：2026-09-06
> 性质：调查 + 分析 + 设计。本阶段未修改任何业务代码。
> 证据来源：TTBOX 仓库源码精读、YU 真机转换器源码逐段精读（/opt/aiassistance/Python/convert_onnx_to_rknn.py 1250 行 + web/app.py 转换段）、Rockchip 官方 rknn_model_zoo（yolov8 convert.py + postprocess.cc 实读）、Ultralytics 官方 exporter 文档/源码、真实开源项目对比、RKNN 2.3.2 真机验证记录。

---

## 1. TTBOX 当前模型架构（真实调用链）

```text
                    ┌──────────────────────────────────────────┐
                    │              模型进入系统                  │
                    │  Web 上传 /api/models/import(.rknn)       │
                    │       或 /api/models/import-onnx(.onnx)   │
                    └───────────────────┬──────────────────────┘
                                        ↓
                Web 层（scripts/ttbox_web.py）
                  · hashlib 计算 sha256（唯一哈希实现）
                  · ONNX → 调 YU 转换器（toolkit2 2.3.2）→ RKNN
                  · 统一 IPC：MODEL_IMPORT(带 source_format/sha256)
                                        ↓
                Core（C++）IPC 层 IpcServer.cpp
                  · MODEL_IMPORT → Application::handle_model_import
                  · MODEL_VALIDATE / MODEL_INSTALL / MODEL_ACTIVATE / MODEL_REMOVE
                                        ↓
                ModelRegistry（core/src/model/ModelRegistry.cpp）
                  · import：收件目录 → staging/<id>/model.rknn + manifest.json
                  · validate：注入 validator（板端=RKNNEngine 真加载+ModelAdapter.analyze）
                  · install：staging → installed/<id>/（原子写）
                  · activate：registry/active.json
                  · remove：active 模型禁删（核心层保护）
                                        ↓
        运行时加载（Application::build_runtime_params）
                  · active_model() → installed/<id>/model.rknn
                  · manifest.input_width/height → Worker 尺寸
                                        ↓
                CoreRuntime → WorkerPool（3 worker）
                  · RKNNEngine::init（rknn_init + rknn_query + core_mask）
                  · ModelAdapter::analyze(info, cfg) → ModelMetadata
                  · ModelAdapter::create_decoder → DecoderImpl(DecodeNMS)
                  · 每帧：RGA 裁剪 → RKNNEngine 零拷贝推理 → Decoder.process
                                        ↓
                DecodeNMS（按 Metadata.decode_type 分发 4 种布局）
                  ↓ DetectionBox
                TargetSelector → PID → HID（本轮确认不动）
```

**8 个问题的当前答案**：

1. **模型从哪里进入**：Web 上传 → `_incoming/` 收件目录 → IPC MODEL_IMPORT → Registry staging → validate → install。
2. **文件如何保存**：`/opt/ttbox/models/installed/<model_id>/{model.rknn, manifest.json, metadata.json, validation/}`；staging/quarantine/registry/active.json 辅助。
3. **metadata 保存什么**：manifest.json = 模型索引（model_id/label/version/sha256/source_format/origin/尺寸/类别数/status）；metadata.json = validate 时 ModelAdapter.analyze 的完整运行时快照（decode_type/strides/quantization/输出 shapes）。
4. **ModelAdapter 负责什么**：`analyze()`——从 rknn_query 结果+配置构建 ModelMetadata；**decode_type 从输出张量结构自动推断（禁止按文件名）**；strides 从网格尺寸反推；`create_decoder()` 注入 conf/iou/filter 生成解码器。
5. **Decoder 如何决定输出格式**：DecoderImpl 包装 DecodeNMS；DecodeNMS.process 按 `Metadata.decode_type` 分发 4 条路径（kSingle/kDfl/kDflDist/kE2e），decode_type 由 Adapter 推断而非 Decoder 自判。
6. **ModelRegistry 负责什么**：仓库唯一入口（扫描/导入/校验/安装/激活/删除/列表），Web 与 Runtime 都不直接碰文件系统。
7. **ONNX 转换由谁负责**：Web 层（ttbox_web.py）调用 YU 转换器（toolkit2 2.3.2 venv），转换后走统一 MODEL_IMPORT。**Core 不参与转换**。
8. **哪些已足够、缺什么**：
   - 已足够：Registry 生命周期、Adapter 结构推断、DecodeNMS 四布局、RKNNEngine 零拷贝、状态提交门。
   - **缺失**：① 转换器是"外部借用"（/opt/aiassistance 路径依赖，非 TTBOX 资产）；② manifest 不记录转换时的 output_layout/class_names 来源（converter 侧已知但没落到 metadata.json）；③ class_names 仅靠用户上传 labels 文件，ONNX 内置 names 元数据没有利用；④ 无"转换报告"持久化（输出布局判定结果只存在于转换日志）。

---

## 2. YU 模型转换架构（真实调用链，源码逐段核实）

```text
Web POST /api/models/import (model_type=onnx)
  ↓ 互斥锁（同一时刻一个转换，否则 409）
临时目录 /tmp/aiassistance_onnx_import_xxx/
  ├─ source/<name>.onnx        ← 保存上传文件
  ├─ source/calibration.zip    ← 可选校准包
  └─ output/                   ← 转换产物
        ↓
subprocess: <venv>/bin/python convert_onnx_to_rknn.py
  --onnx ... --dataset-root ... --dataset-count 5
  --target-platform rk3588 --output <名>_raw_int8_rk3588.rknn
        ↓  失败且匹配特征 → 自动重试 --skip-shape-inference --output-layout graph
        ↓
daemon_call("import_model", source_path=...)  ← C++ daemon 统一入库
  ↓
临时目录删除（finally）
```

### 转换器核心机制（convert_onnx_to_rknn.py 实读）

| 机制 | 实现细节 |
|---|---|
| **输出布局 auto 判定** | 依次尝试 4 种结构检测，先到先得：①raw6=Ultralytics `cv2/cv3.<scale>.2/Conv` 成对头（正则匹配节点名！）②raw9=`raw9_aux.<scale>/ReduceSum` 辅助头 ③YOLO26 e2e=图输出 [1,≤1000,6] 且存在 [0,2,1] Transpose 前驱 ④raw3=YOLOv5 `m.<scale>/Conv`；全部失败→保底 graph 原输出 |
| **ONNX 图预处理** | 拓扑重排（RKNN Toolkit 兼容）、Resize 空 roi 归一、常量输出 keepalive 保护（raw6 第 4/6 输出加微小动态 alias 防 NPU 常量折叠） |
| **RKNN config** | mean=[0,0,0] std=[255,255,255]（0-255 直通）、quantized_dtype=asymmetric_quantized-8、target_platform=rk3588 |
| **输入规格** | 从 ONNX 图自动检测 H/W（动态尺寸报错要求显式指定） |
| **class names** | 从 ONNX metadata 的 `names` 字段解析（ast.literal_eval dict{int:str}），转换后打印 `Classes (N): [...]` 供 web 端 parse_converter_class_names 提取 |
| **校准集** | 用户 zip（≤上限文件数、防路径穿越、图片后缀过滤、超量均匀采样）或内置 209 张通用图；--dataset-count 5 均匀采样 → dataset.txt（图片路径列表） |
| **异常处理** | 转换失败自动降级重试一次；产物校验（存在+非空）；临时目录 finally 清理；web 层互斥锁 |
| **4 通道直回归警告** | raw6 若检出 direct-regression 头（4 通道 box）打印警告（老 runtime 会有大框问题） |

### 必须保持一致的 YU 行为（已验证有效）

1. mean 0/0/0 + std 255/255/255（0-255 直通，与 TTBOX 预处理一致）
2. INT8 asymmetric_quantized-8 + target rk3588
3. 输出布局"结构检测优先，graph 保底"的思想（**不按模型名判断**）
4. 转换失败自动降级重试一次（graph 布局兜底）
5. class names 从 ONNX metadata 提取
6. 校准集：用户 zip 优先 + 内置缺省 + 均匀采样
7. 互斥 + 临时目录隔离 + finally 清理

### 属于 YU 自有业务、不能复制进 TTBOX 的

- daemon unix socket IPC 协议（TTBOX 有自己的 MODEL_IMPORT IPC）
- model.json/model-list.json 双文件注册表（TTBOX 用 manifest.json + active.json，结构不同但职责等价）
- license/model_key 模型加密链（TTBOX 无此需求）
- 云端/远端推理合并逻辑

---

## 3. Rockchip 官方方案（rknn_model_zoo 实读）

### 官方 YOLOv8 转换链

```text
ultralytics_yolov8（官方 RKOPT fork，改 head）
  └─ export: 移除模型内后处理，输出改为 3 分支 × 3 张量 = 9 输出
       每分支: box [1,64,H,W]（DFL 原始）+ cls [1,C,H,W] + score_sum [1,1,H,W]（ReduceSum 快速过滤）
        ↓
convert.py（examples/yolov8/python/convert.py，74 行）
  └─ rknn.config(mean=[0,0,0], std=[255,255,255], target_platform=rk3588)
     rknn.load_onnx → rknn.build(do_quantization=True, dataset=COCO子集txt)
        ↓
C++ postprocess.cc（CPU 端）
  ├─ dfl_len = output_attrs[0].dims[1] / 4        （=16 bins）
  ├─ output_per_branch = n_output / 3             （布局自识别！）
  ├─ score_sum INT8 快速过滤（先于类别遍历）
  ├─ cls max/argmax（INT8 域直接比较，省反量化）
  ├─ DFL softmax → ltrb 距离 → 像素框
  └─ NMS
```

**官方关键做法**（与 TTBOX 对照）：

| 官方做法 | TTBOX 现状 | 结论 |
|---|---|---|
| 修改导出图：删模型内后处理，暴露 raw 头 | YU 转换器用正则找到 `cv2/cv3.<scale>.2/Conv` 直接把中间张量声明为输出（不改训练 fork，**转换期重定向输出**，效果等价且更通用） | TTBOX 现路线更优（无需用户换 fork） |
| score_sum 第三张量加速过滤 | TTBOX kDflDist 的 aux 路径已有等价物（`aux_obj` 值域检测自动启用，DecodeNMS.cpp:547-573） | 已覆盖 |
| `output_per_branch = n_output / 3` 布局自识别 | TTBOX `infer_decode_type`：n_outputs%3==0 且 reg≥32 且 aux==1 → kDflDist | **结构判定等价** |
| INT8 域直接比较（不逐元素反量化） | TTBOX read_elem 逐元素反量化 | 潜在优化点（非必须） |
| CPU DFL softmax + NMS | TTBOX process_dfl_dist 同样的 softmax 解码 + classwise NMS | 已覆盖 |
| dataset=COCO 子集 txt | YU 同款 txt 格式 + 采样 | 已覆盖 |

**重要发现（兼容性验证）**：官方 yolov8 优化版 9 输出 `[1,64,H,W],[1,C,H,W],[1,1,H,W]×3` **恰好命中 TTBOX kDflDist 推断条件**（reg.dims[1]=64≥32 且 %16==0；aux.dims[1]=1）——即 TTBOX 现有 C++ 解码器**无需修改即可解码官方优化版 YOLOv8**。唯一注意点：官方 box 通道排布为 edge 主序（edge*n_bins），TTBOX process_dfl_dist 也是 `edge*n_bins + bin` 主序（DecodeNMS.cpp:580），一致。

### 官方其他 YOLO 差异

| 版本 | 官方优化输出 | TTBOX 对应 |
|---|---|---|
| YOLOv5 | 3 输出 raw（anchor 基） | 需 anchor 解码（TTBOX 无此 Decoder，暂不支持） |
| YOLOv8/11 | 9 输出 raw6+sum（DFL） | **kDflDist 命中** |
| YOLOv10 | e2e（无 NMS） | kE2e 可接（[1,N,6]） |
| YOLO26 | e2e（新版导出）或 raw6（RKOPT fork） | kE2e / raw6→kDfl |

---

## 4. Ultralytics 官方 exporter（rknn.py 实读）

`ultralytics/utils/export/rknn.py` 的 `onnx2rknn()`：

```python
onnx2rknn(onnx_file, output_dir, name="rk3588",
          quantize=None|8, dataset=None, metadata=None, batch=1)
  → 内部：rknn.config(...) → load_onnx → build(do_quantization=quantize==8,
    dataset=校准图片列表txt) → export_rknn
  → 要求 ONNX opset ≤ 19；RK3588 原生支持；rv1103/1106 不支持 INT8
  → 产物是 <name>_rknn_model/ 目录（rknn + metadata.yaml）
```

**关键事实**：
1. Ultralytics 官方 exporter 保留**原始图输出**（e2e 或 concat 单输出），不做 Model Zoo 式 raw 头重定向——FP16 可用，但 INT8 上后处理节点量化不友好（官方 FAQ 明确 INT8 需要删后处理）。
2. metadata.yaml（names/stride 等）由 exporter 生成——这是标准元数据来源。
3. 导出必须在 x86 Linux（rknn-toolkit2 不支持 ARM 上转换）。

**结论（回答"是否直接调 Ultralytics exporter"）**：
- **不直接调用 Ultralytics exporter 作为 TTBOX 主转换路径。** 理由：
  1. 它产出的是"原始图输出"模型，INT8 精度与性能都不是最优（官方自己的 Model Zoo 都不这么做）；
  2. 它依赖完整 ultralytics Python 环境（含 torch），而 YU 转换器只需要 onnx+rknn-toolkit2 两个包，依赖轻一个量级；
  3. TTBOX 已验证的 YU 转换器功能是它的超集（结构检测输出重定向 + keepalive 保护 + 降级重试）。
- 但 **吸收它的两个标准**：opset ≤19 的输入要求写进文档；metadata 中的 names 字段作为 class_names 来源之一。
- 保留为**用户侧可选项**：用户可自行 `yolo export format=rknn` 后上传 FP16 rknn（走 RKNN 直传入口），TTBOX 不阻塞。

---

## 5. 真实项目对比表

| 项目 | 转换入口 | ONNX 处理 | Toolkit 版本 | INT8 | Calibration | Output 结构 | Decoder | NMS | Metadata | RK3588 | YOLO 版本 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **airockchip/rknn_model_zoo** | examples/<model>/python/convert.py | **官方 RKOPT fork 修改 head**（删后处理+9 输出+ReduceSum） | 2.x（随 runtime 配对） | 默认 i8 | COCO 子集 txt（20 张） | 9 输出 raw（box64/cls/sum×3） | C++ postprocess（DFL softmax） | CPU classwise NMS | 无（README 手工） | ✓ | v5/v7/v8/10/X/11 |
| **zycer/yolo26_rknn_ultralytics** | rknn_export/convert.py | ultralytics RKOPT 导出（无后处理），6 输出 raw6（reg64/cls 成对，无 sum） | rknn-toolkit2（新版） | i8 默认 | txt 路径列表 | 6 输出 raw6 | CPU DFL+NMS | CPU | yaml | ✓ | YOLO26 |
| **sc-30-bit/YOLOv8-RKNN** | rknn_model_zoo convert.py | 改 ultralytics head.py+model.py 导出 ONNX | 2.x | i8 | model zoo 缺省 | 9 输出 raw | model zoo C++ | CPU | 无 | ✓ | YOLOv8 改进版 |
| **YU 转换器**（真机已验证） | web 上传→convert_onnx_to_rknn.py | **不改图结构，转换期正则定位 raw 头并重定向输出**；keepalive 保护；失败降级 graph | **2.3.2（与 runtime 配对）** | i8（asym-8） | 用户 zip / 内置 209 张，采样 5 | auto：raw6→raw9→e2e→raw3→graph | C++ daemon（raw3/6/9/e2e 四布局） | CPU | ONNX metadata names | ✓ | v5/v8/v11/v26 |
| **TTBOX 现状** | web 上传→同款转换器 | 同 YU | **2.3.2（真机锁定）** | i8 | 同 YU | kSingle/kDfl/kDflDist/kE2e（Adapter 推断） | DecodeNMS 四路径 | CPU classwise | manifest+metadata.json | ✓（真机） | 已验证 4 种布局 |
| tristanpenman/yolo-rknn | 实验性 | 原始图 | 旧版 | fp | 无 | 原始单输出 | Python | CPU | 无 | 部分 | v8 |

**结论**：
- 所有成功项目共同点 = **INT8 必须去模型内后处理 + CPU 解码/NMS + 校准 txt**。
- TTBOX 借 YU 转换器已获得其中最关键能力（转换期输出重定向），且是唯一"不改导出 fork"的方案。
- **绝对不能照搬**：各项目按固定 YOLO 版本硬编码输出布局的做法；模型 zoo 手工改 head.py 的流程（对用户不可用）；任何要求用户改训练代码的流程。

---

## 6. YOLO 输出兼容性矩阵（本阶段核心结论）

| 布局 | 来源 | ONNX/转换期特征 | RKNN 输出形状 | C++ 解码 | TTBOX 判定依据（结构推断，已实现） |
|---|---|---|---|---|---|
| raw6（DFL 成对） | YOLOv8/11/26 RKOPT 导出、YU auto | cv2/cv3 成对 Conv 头 | 6 个：[1,64,H,W]+[1,C,H,W] ×3 尺度 | DFL softmax→ltrb | n_outputs%2==0 → kDfl（现用）；n_outputs%3==0 且 reg≥32 → kDflDist |
| raw6+sum（9 输出） | 官方 model zoo yolov8 | 同上 + ReduceSum 分支 | 9 个：[1,64,H,W]+[1,C,H,W]+[1,1,H,W] ×3 | 同上 + sum 快速过滤 | **kDflDist 命中**（reg=64、aux=1） |
| raw9（aux 辅助） | YOLO26 训练辅助头 | raw9_aux ReduceSum | 9 个（box/cls/aux 三元组） | 同 kDflDist | kDflDist 命中 |
| e2e | YOLO26/YOLOv10 端到端导出 | 图输出 [1,N,6] | [1,300,6] xyxy+score+cls | 直读（无解码/NMS） | 单输出 dims[2]≤16 → kE2e |
| single（concat） | 官方原版导出（未 RKOPT） | 单输出 concat | (1, 4+nc, M) | xywh 直读 + cls | 单输出 dims[1]≥5 → kSingle |
| raw3（anchor） | YOLOv5 | m.<scale>/Conv | 3 个 [1,A*(5+nc),H,W] | anchor 解码 | **TTBOX 无此 Decoder（唯一缺口）** |

**"用户随便上传 ONNX，TTBOX 凭什么知道用哪种 Decoder？"——回答：**
凭 **RKNN runtime query 出的输出张量结构**（数量/维度/dtype/layout/scale/zp），经 `ModelAdapter::infer_decode_type` 结构规则推断。已实现的规则覆盖 raw3 之外的全部主流布局，且与官方 model zoo C++ 的布局自识别逻辑等价。**无需 if model_name == ...，结构推断已足够**；推断不出的（kUnknown）在 validate 阶段直接拒绝入库（真机已验证此路径）。唯一结构歧义点（6 输出：成对 kDfl vs 3 组 kDflDist）已用 reg 通道数≥32+aux 维度区分，真机两种模型并存验证通过。

剩余风险：kSingle 与 kE2e 的边界（[1,6,M] M 大时误判）——现有规则 `dims[2]≤16 → e2e`，而 e2e 的 N≤1000、F=6，判定可靠；raw6 中 cls 通道恰好 <32 的极端小类别模型（<8 类）会被 kDfl/kDflDist 误分——**建议在 manifest 增加 converter 侧声明的 layout 字段做二次确认**（见设计）。

---

## 7. 职责边界（最终版）

```text
┌─ 转换器（Web 层 Python，toolkit2 2.3.2）────────────────┐
│ ONNX 语法校验 · 输入规格检测 · 输出布局判定/重定向        │
│ 图预处理（拓扑/keepalive） · INT8 量化 · 校准集管理       │
│ RKNN 生成 · 产物存在性校验 · class_names 提取            │
│ 失败降级重试 · 互斥 · 临时目录隔离与清理                  │
│ 产出：model.rknn + 转换报告(layout/class_names/样本数)    │
└──────────────────────┬──────────────────────────┘
                       ↓ MODEL_IMPORT(source_format, sha256, 转换报告)
┌─ ModelRegistry（Core C++）──────────────────────────────┐
│ 收件→staging→validate→install→activate→remove 原子生命周期│
│ validator 注入点：RKNNEngine 真加载 + Adapter.analyze     │
│ active.json / selected 保护 / 列表                        │
└──────────────────────┬──────────────────────────┘
                       ↓
┌─ ModelAdapter（加载时一次）─────────────────────────────┐
│ rknn_query → ModelMetadata（输入/输出/量化/dtype）        │
│ decode_type 结构推断（可被 manifest 声明覆盖/确认）        │
│ strides 反推 · class_count 推断 · decoder 工厂            │
└──────────────────────┬──────────────────────────┘
                       ↓
┌─ Decoder（DecodeNMS，每帧）──┐  ┌─ Runtime（RKNNEngine）─┐
│ 反量化 · DFL softmax · box   │  │ load/init/run/零拷贝    │
│ score/cls · NMS · 坐标映射   │  │ core_mask · buffer      │
└──────────────────────────────┘  └────────────────────────┘
                       ↓ DetectionBox
        TargetSelector → PID → HID（不动）
```

---

## 8. 最终推荐架构

```text
            用户上传（.onnx 或 .rknn）
                     │
        ┌────────────┴─────────────┐
        ↓                          ↓
   [.rknn 直传]              [.onnx]
        │                          │
        │              ┌───────────▼───────────────┐
        │              │ Model Analyzer（Python）   │  ← 新增，薄层
        │              │ · onnx 图解析（opset/输入/  │
        │              │   输出头结构/class names）  │
        │              │ · 产出 convert_plan.json   │
        │              └───────────┬───────────────┘
        │                          ↓
        │              ┌───────────────────────────┐
        │              │ Converter（现状保留）       │
        │              │ YU 对齐 + toolkit2 2.3.2   │
        │              │ INT8 + 校准集 + 重试        │
        │              └───────────┬───────────────┘
        │                          ↓ model.rknn + 转换报告
        └────────────┬─────────────┘
                     ↓ MODEL_IMPORT(source_format, sha256, convert_report)
        ┌────────────────────────────────┐
        │ ModelRegistry（现有，不动）      │
        │ validate = RKNNEngine 真加载    │
        │  + ModelAdapter.analyze         │
        │  + 结构与报告交叉验证            │  ← 新增校验点
        └───────────────┬────────────────┘
                        ↓ installed/<id>/{model.rknn,manifest.json,metadata.json}
        ┌────────────────────────────────┐
        │ 运行时（现有，不动）             │
        │ CoreRuntime→WorkerPool→        │
        │ RKNNEngine→ModelAdapter→       │
        │ Decoder→TargetSelector→PID→HID │
        └────────────────────────────────┘
```

**核心原则：不新造框架，只做两件事**——
1. 把"借用 YU 路径"变成 TTBOX 自有 `tools/converter/`（拷贝 YU 转换器脚本为仓库资产 + 配置化 venv 路径），Analyzer 作为转换前薄层并入同一脚本；
2. 转换报告（layout 判定结果/class_names）随 MODEL_IMPORT 落进 metadata.json，Adapter 推断与转换期声明交叉验证，消除 6 输出歧义。

---

## 9. 十问回答

**Q1 TTBOX 应该在哪里实现 ONNX→RKNN？**
保持在 Web 层（Python），但把 YU 转换器脚本收编为 TTBOX 仓库资产 `tools/converter/convert_onnx_to_rknn.py`，venv 路径/校准目录全部环境变量化（现状已是 env 可配，只差脚本资产化）。Core（C++）永远不碰转换。

**Q2 转换器应该使用什么接口？**
保持现有 CLI 接口（`--onnx --dataset-root --dataset-count --target-platform --output --output-layout`），新增两个输出：`--report <json>`（layout 判定结果/class_names/输入规格/采样数），供 MODEL_IMPORT 落 metadata。Web 端封装为 `_run_onnx_conversion()`（已有），加解析 report。

**Q3 是否复用 YU 转换器？怎么复用？**
复用。方式 = 拷贝脚本进仓库（它是自包含单文件，仅依赖 onnx+rknn.api）+ 保留"失败降级 graph 重试"逻辑 + 不复制 YU 的 daemon/加密/云端业务。转换器脚本 GPL/自有版权需确认（YU 是参考实现，行为对齐、代码重写或取得授权，实施阶段定）。

**Q4 是否需要 Model Analyzer？**
需要，但是薄层（~100 行 Python，onnx 库已在 venv 内）：转换前解析图结构→输出 convert_plan（layout 候选/输入尺寸/class names）；**它的产物同时服务转换器（output-layout 参数）和 validate 交叉验证**。不做独立服务，并入转换脚本 `--dry-run` 模式（YU 已有此模式，正好复用）。

**Q5 如何自动判断模型类型？**
两级：①转换期 Analyzer 按 YU 的结构检测链（cv2/cv3 成对头→raw9_aux→e2e shape→v5 conv）判 layout；②加载期 Adapter 按 RKNN 输出张量结构推断 decode_type。两级结果交叉验证，不一致→拒绝入库并给出明确错误（防止"转换对的模型配错解码器"）。

**Q6 如何自动判断 output layout？**
同 Q5。唯一补充：6 输出模型的 kDfl/kDflDist 歧义由转换报告确认（报告里有 layout 字段），运行期仍以结构推断为准（防止 manifest 被手改）。

**Q7 如何选择 Decoder？**
现状机制保持：ModelAdapter.analyze → decode_type → create_decoder → DecodeNMS 分发。**不新增 Decoder 选择逻辑**，只新增：manifest/转换报告与推断结果的一致性校验。

**Q8 INT8 calibration dataset 如何管理？**
保持 YU 行为：用户 zip（后缀过滤+路径安全+均匀采样上限）优先，缺省用内置通用图集（TTBOX 自备一套放 `/opt/ttbox/converter/calibration/`，不依赖 YU 目录）；--dataset-count 默认 5；dataset txt 格式（图片绝对路径行）。校准图**不进模型库**，只进转换临时目录。

**Q9 ModelMetadata 最终保存哪些字段？**
manifest.json（入库索引）：model_id/label/version/format/source_format/sha256/origin/status/created_at/input_w/input_h/output_count/class_count/class_names。metadata.json（validate 运行时快照，已有 20 项）新增 3 项：`convert_layout`（转换期判定）、`converter_version`（"ttbox-converter/1.0 + toolkit2 2.3.2"）、`calibration_images`（采样数）。不塞更多（不为丰富而丰富）。

**Q10 以后增加 YOLO11/YOLO26/新模型，哪些文件要改？**
- YOLO11（raw6/9 输出）：**0 个文件**——YU 转换器 auto 已支持（cv2/cv3 正则就是 v8/v11 通用的），kDflDist 已命中。
- YOLO26 e2e 导出：**0 个文件**——kE2e 已验证（v26m 真机）。
- YOLO26 raw6 导出（RKOPT）：**0 个文件**——raw6→kDfl 现成。
- YOLOv5（anchor）：`DecodeNMS.cpp` 新增 1 条解码路径 + `ModelMetadata.hpp` 加 1 个 DecodeType + Adapter 推断分支 —— 共 3 个文件，全部集中在模型层。
- 全新结构：同上 3 文件 + 转换器 layout 检测 1 处。
- **永远不动**：Capture/RGA/HDMI/EDID/RKNNEngine/WorkerPool/TargetSelector/PID/HID。

---

## 10. 缺口清单与下一步（进入实施阶段的依据）

| 类别 | 项 | 动作 |
|---|---|---|
| 已有 | Registry 生命周期/Adapter 推断/DecodeNMS 四布局/零拷贝/状态门/双入口 Web | 不动 |
| 缺失① | 转换器是外部借用（/opt/aiassistance 路径依赖） | 收编为 `tools/converter/` 仓库资产 |
| 缺失② | 转换报告不落盘（layout/class_names 只在日志） | 转换器加 `--report`，Web 解析后随 MODEL_IMPORT 落 metadata.json |
| 缺失③ | ONNX 内置 class names 未利用 | Analyzer 提取 names → 入 metadata.json（用户上传 labels 仍可覆盖） |
| 缺失④ | TTBOX 无内置校准图集（依赖 YU 的 209 张） | 自备一套放 `/opt/ttbox/converter/calibration/` |
| 缺失⑤ | validate 无"结构 vs 报告"交叉验证 | validator 内比对 convert_layout 与推断结果 |
| 缺失⑥ | YOLOv5 anchor Decoder | 本阶段不做（无真实需求模型），列为已知边界 |
| 不动 | 全部稳定链路 | — |

**下一步实施应修改的文件（预期，实施阶段确认）**：
1. `tools/converter/convert_onnx_to_rknn.py`（新增，YU 对齐 + --report）
2. `scripts/ttbox_web.py`（转换调用指向自有脚本 + 解析 report 传参）
3. `core/src/app/Application.cpp`（handle_model_import 接收 convert_report 落 metadata）
4. `core/src/model/ModelRegistry.cpp`（validator 交叉验证）
5. `deploy/`（校准图集 + venv 检查脚本）
**完全不需要动**：ModelAdapter/DecodeNMS/RKNNEngine/WorkerPool/CoreRuntime/TargetSelector/PID/HID/Capture/RGA。
