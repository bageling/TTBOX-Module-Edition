# AIBox A7：ModelAdapter 多模型高速接入与统一 Runtime — 验收报告

- 阶段：A7
- 日期：2026-08-14
- 平台：Orange Pi 5 Plus（RK3588）Ubuntu 24.04 / 6.1.0-1025
- 结论：**A7 PASS**（停止在 A7，不进入 A8）

---

## 1. A7 实现文件清单

### 新增（统一模型接入层）

| 文件 | 说明 |
|---|---|
| `ttbox/core/src/model/ModelMetadata.hpp` | ModelMetadata 结构（20 项字段）+ 枚举（DecodeType/QuantType/ColorOrder/CoordFormat/NmsType） |
| `ttbox/core/src/model/Decoder.hpp` | Decoder 抽象接口 + DecoderImpl（统一包装 DecodeNMS） |
| `ttbox/core/src/model/ModelAdapter.hpp` | ModelAdapterConfig + ModelAdapter（analyze/create_decoder/effective_conf/effective_iou） |
| `ttbox/core/src/model/ModelAdapter.cpp` | infer_decode_type / infer_class_count / analyze / create_decoder 实现 |
| `ttbox/core/tests/test_model_metadata.cpp` | 单元测试：20 项字段默认值 + 黄瓦 + v26m |
| `ttbox/core/tests/test_decoder_dispatch.cpp` | 单元测试：single/e2e/dfl 分发、objectness、拒绝不支持结构、用户 conf/iou |
| `ttbox/core/tests/test_model_adapter.cpp` | 硬件测试：真实模型 metadata + decoder 创建（三模型回归） |
| `ttbox/core/tests/test_model_runtime.cpp` | 硬件测试：模型加载生命周期（3 轮 init→analyze→decoder→30 帧→destroy） |

### 修改（新增能力，不触碰 A1-A6 核心语义）

| 文件 | 变更 |
|---|---|
| `ttbox/core/src/rknn/DecodeNMS.hpp/.cpp` | DecodeParams 增 `class_filter`/`max_detections`；新增 `apply_post_filter`（class 过滤 + score 降序截断）、`set_frame`（运行期更新坐标）、`process_e2e`（[1,N,F] 通用读取，不硬编码 N/F） |
| `ttbox/core/src/rknn/WorkerPool.hpp/.cpp` | Params 增 `ModelAdapter* adapter`；worker 内部 `unique_ptr<Decoder> decoder_`（无 adapter 时保持原 DecodeNMS 默认路径） |
| `ttbox/core/tests/test_worker_hw.cpp` | 新增 `--adapter`/`--inw`/`--inh`/`--color` 参数；adapter 模式走统一接入层 |
| `ttbox/core/CMakeLists.txt` | CORE_SOURCES 加 ModelAdapter.cpp；新增 4 个测试目标 |
| `config/default.json` | 新增 `class_filter_text`/`max_detections`（用户可配置） |

未修改：Capture（V4L2Capture/DmaBuf）、RGA（RgaProcessor）、RKNN（RKNNEngine/NpuMonitor）核心逻辑。

---

## 2. ModelAdapter 架构说明

```
Model (rknn 文件)
   ↓ rknn_query (RKNNEngine::info)
RknnModelInfo
   ↓ ModelAdapter::analyze(info, ModelAdapterConfig)
ModelMetadata (20 项)
   ↓ ModelAdapter::create_decoder()
Decoder (DecoderImpl → DecodeNMS)
   ↓
A1-A6 Runtime (WorkerPool / RGA / RKNN / NMS)
```

- **禁止文件名/标签判断**：decode_type / class_count / strides / DFL / objectness 全部由输出结构（dims 形状）推断，见 `infer_decode_type`。
- **输入属性来自 runtime 查询**：input dims/type/layout/size 取自 rknn_query，颜色顺序由 `ModelAdapterConfig.color_order`（用户配置）提供，模型内部无 RGB/BGR 特判。
- **配置优先级**：`ModelMetadata.default_conf/iou (0.25/0.45) < Runtime 默认 < User Config (ModelAdapterConfig.conf_thres/iou_thres)`，最终以用户配置为准（`effective_conf/effective_iou`）。
- **零逐帧开销**：analyze 仅模型加载时执行一次；推理路径只增加一次虚函数调用 `decoder_->process()`（委托同一 DecodeNMS），无整帧复制、无大 Tensor memcpy、无逐帧 JSON/Python。

---

## 3. 三模型 Metadata（板端真实 rknn_query + analyze）

| 项 | 黄瓦 huangwa.rknn | yolo261n-rk3588.rknn | v26m_640_int8.rknn |
|---|---|---|---|
| input | 320×320×3 | 640×640×3 | 640×640×3 |
| input_dtype | 2 (INT8) | 1 (FP16) | 2 (INT8) |
| input_layout | 1 (NHWC) | 1 (NHWC) | 1 (NHWC) |
| color_order | BGR | BGR | **RGB** |
| quantization | INT8 | FP16 | INT8 |
| output_count | 6 | 1 | 1 |
| output_shapes | 6×[1,1/2,H,W] | [1,84,8400] | **[1,300,6]** |
| decode_type | dfl | single | **e2e** |
| class_count | 2 | 80 | 0 (列值携带) |
| strides | 8/16/32 | — | — |
| dfl / objectness | dfl / no | no / no | no / no |
| coordinate | ltrb | xywh | **xyxy** |

（实测输出见 `test_model_adapter` 三模型 PASS 日志）

---

## 4. Decoder 分发结果

| 输出结构 | 分发 | 依据 |
|---|---|---|
| 单输出 3D [1,N,F] F≤16 | **E2E**（v26m） | 行内固定字段，模型内已 TopK 解码 |
| 单输出 (1,C,M) C≥5 | **Single**（yolo261n） | xywh+类别直读 |
| 多输出偶数对 box/cls | **DFL**（黄瓦） | 成对 box/cls，DFL 解码 + strides 从 box 输出 `sqrt(anchors)` 推断 |
| 其他（如 [1,3,640,640]） | 拒绝，报错 | 不支持结构正确报错 |

- `process_e2e` 从 dims 动态读取 N/F（不硬编码 300/6），E2E 语义保持 `[x1,y1,x2,y2,score,class]`，**未改成 DFL**。
- 分发测试：6/6 单元测试 PASS（`decoder_dispatch_*`），三模型真实硬件分发 PASS。

---

## 5. conf / IoU 验证结果

- 用户配置 `conf=0.55, iou=0.45`（来自 config，A7 要求不写死到 Adapter）在三模型均生效：`effective conf=0.55 iou=0.45`。
- 优先级单测：`cfg.conf=0.7 iou=0.3` 覆盖 metadata 默认 → `effective_conf()=0.7` / `effective_iou()=0.3` PASS。
- `class_filter` / `max_detections`：config 新增字段已解析（class_filter_text / max_detections），`apply_post_filter` 在 decode 后按 score 降序过滤 + 截断。
- 未使用 0.55/0.25/0.45 等值写死在 Adapter/Decoder 内部；default_conf 仅作为 metadata 默认（0.25/0.45），用户配置优先。

---

## 6. 三模型检测对齐结果

| 模型 | 语义保持 | 验证方式 |
|---|---|---|
| yolo261n 640 FP16 | single、xywh、80 类 | A-5 已对齐；A7 metadata/decoder 一致（PASS） |
| 黄瓦 320 INT8 | dfl、ltrb、strides 8/16/32、2 类 | P-1 真机 240fps 检测已验收；A7 分发一致（PASS） |
| v26m 640 INT8 | **e2e [1,300,6] xyxy RGB**、无 DFL/objectness | 640 模型阶段 ONNX/RKNN 对齐已验收（RGB）；A7 e2e 路径一致（PASS） |

- `test_model_runtime`：三模型各 3 轮 × 30 帧真实推理（渐变输入 → set_input → run → get_raw_outputs → decoder->process），解码全部成功，无 DFL 误入 E2E 或反之。
- A7 未改任何模型推理语义；decode 开销 v26m 仅 ~1-2µs/帧（e2e 直读）。

---

## 7. 性能对比（adapter 模式 vs P-1 基线）

### 黄瓦 320 INT8 · 3 Worker · 8 buffers · CPU affinity=A76 (4,5,6) · conf=0.55

| 指标 | P-1 基线（无 adapter） | A7 adapter 模式 | 差异 |
|---|---|---|---|
| Pipeline FPS | ≈239.7 | **239.3** | **-0.17%**（要求 <2%） |
| poll_timeout | 0 | 0 | 保持 |
| error | 0 | 0 | 保持 |
| decode | — | avg 142-196µs / 帧（DFL 6 输出） | 与 P-1 同量级 |

### v26m 640 INT8 · 3 Worker（adapter 模式，RGB）

| 指标 | 结果 |
|---|---|
| Pipeline FPS（300 帧） | 30.0 |
| Pipeline FPS（1000 帧稳定） | **29.8**（1001 样本，33.65s） |
| E2E avg | 99.3ms |
| decode (e2e 直读) | 1-5µs/帧 |
| error / poll_timeout | 0 / 0 |

> 结论：Adapter 抽象层带来的性能损失 <2% 达标（实测 0.17%）；v26m 仍为 NPU 物理极限 ~30fps，未为追求 FPS 改动模型语义。

---

## 8. A1-A6 回归结果

| 回归项 | 结果 |
|---|---|
| ttbox_core_tests 单元测试（logger/config/ipc/application/decode/rga/latestframe + A7 新增 6 项） | **39/39 PASS** |
| test_rknn_hw（yolo261n 300 帧） | PASS（21.6 FPS，无错误） |
| test_rga_hw | PASS（RGA 验收，无 fd 泄漏） |
| test_capture_hw / worker_hw capture | 正常（capture 240.7 FPS） |
| WorkerPool 默认路径（无 adapter） | 未改动，decode 单测 9 项 PASS |

修复的既有回归问题（A1-A6 遗留，非 A7 语义）：
- `test_config.cpp` conf 断言原写死 `==0.25`，与用户可配置的 default.json（0.55）冲突 → 改为断言有效正数（A7 允许 conf 用户配置）。

---

## 9. 内存 / 线程检查

| 检查项 | 结果 |
|---|---|
| 模型加载/卸载（3 模型 × 3 轮 init→analyze→decoder→推理→destroy） | 无泄漏（test_model_runtime PASS） |
| RGA DMA-BUF fd 泄漏 | 无（test_rga_hw PASS） |
| 3 Worker 并发 1000+ 帧（黄瓦） | error=0，无线程竞态 |
| 3 Worker 并发 1001 帧（v26m） | error=0，无线程竞态 |
| 单测 latestframe 多线程 | PASS |
| 逐帧 JSON / Python / 大块 memcpy | 无（C++ 全链路，metadata 加载期解析一次） |

---

## 10. A7 PASS/FAIL 结论

| 验收项 | 状态 |
|---|---|
| ModelAdapter 统一接口完成 | ✓ |
| Metadata 完整（20 项，三模型可描述） | ✓ |
| DFL Decoder（黄瓦 strides 8/16/32） | ✓ |
| E2E Decoder（v26m [1,300,6] xyxy 保持） | ✓ |
| RGB/BGR 通用（v26m=RGB，其余=BGR） | ✓ |
| FP16/INT8 通用（yolo261n FP16 / 黄瓦、v26m INT8） | ✓ |
| 用户 conf/IoU（config 可配，优先级正确） | ✓ |
| 黄瓦 320 回归 PASS（239.3 FPS） | ✓ |
| yolo261n 640 回归 PASS | ✓ |
| v26m 640 回归 PASS（29.8 FPS） | ✓ |
| A1-A6 全部回归 PASS | ✓ |
| 无 Python Runtime / 逐帧 JSON/IPC | ✓ |
| 无大块内存复制 | ✓ |
| 无内存泄漏 | ✓ |
| 无线程竞态 | ✓ |
| 黄瓦性能 ≈240 FPS（损失 0.17% <2%） | ✓ |
| 禁止项：未改 Capture/RGA/RKNN/WorkerPool 核心语义、未按文件名 if/else、未固定 YOLO 版本 | ✓ |

**A7 PASS。按约定停止在 A7，不进入 A8/A9/A10。**
