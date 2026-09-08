# AIBox A8：模型仓库 + RuntimeProfile + ROI/FOV + 热更新 + 模型切换 — 验收报告

- 阶段：A8
- 日期：2026-08-14
- 平台：Orange Pi 5 Plus（RK3588）Ubuntu 24.04 / 6.1.0-1025
- 结论：**A8 PASS**（完成后停止，不进入 A9）

> 原则落实：模型适配 Runtime，而不是 Runtime 限制模型。模型仓库负责模型生命周期，RuntimeProfile 负责用户配置，ModelAdapter 负责模型语义，高速链路保持 C++。

---

## 1. 模型仓库 — PASS

### 目录结构（`models/`，C++ ModelRegistry 自动创建）

```
models/
├── registry/       # active.json（当前激活模型）
├── installed/      # <model_id>/manifest.json + metadata.json + model.rknn + validation/
├── staging/        # 上传/转换中
├── cache/          # 转换缓存（预留）
└── quarantine/     # 验证失败模型（reason.json）
```

### 操作验证（test_model_registry 单元 + test_model_switch_hw 硬件）

| 操作 | 单元（模拟） | 硬件（真实模型） |
|---|---|---|
| list | ✓ | — |
| import | ✓ | ✓ 3 模型 |
| validate（RKNN+Adapter 真实校验） | ✓（模拟） | ✓ 3 模型 |
| install | ✓ | ✓ 3 模型 |
| activate / deactivate | ✓ | ✓ + 切换 |
| remove | ✓ | ✓ |
| 禁止删除 active 模型 | ✓ | ✓ 拒绝 |
| 激活失败恢复旧模型 | ✓ | ✓（未安装模型 → active 保持） |
| quarantine | ✓ | — |

每个 installed 模型生成 `manifest.json`（version/sha256/signature/origin/converter_version/runtime_version/status 预留云端字段）+ `metadata.json`（ModelAdapter.analyze 输出）。

---

## 2. ONNX → RKNN — PASS

新增离线工具 [scripts/convert_onnx_to_rknn.py](file:///g:/工作区/ttbox逆向/ttbox2/scripts/convert_onnx_to_rknn.py)（Python，仅离线，禁止进入高速链路）：

- 支持 FP16 / INT8（w8a8，channel 量化）
- 转换前 ONNX 图检查：**input shape / dtype / layout / color order / output shape / output count / 未知算子**
- 失败明确报告（错误原因写入 JSON 报告）
- 转换报告输出：`ok/errors/warnings/input_size/output_shapes/sha256/file_size/elapsed_ms`
- 可选直接写入模型仓库 staging + manifest

### v26m 640 ONNX → RKNN INT8（A8 第 11 项完整链路）

```
ONNX [1,3,640,640] → RKNN INT8 [1,300,6]（23,544,846 B，38 s）
  → 检查报告 ok=true（output_count=1, output_shapes=[1,300,6], color=RGB, 警告=Mod 算子可支持）
  → Metadata（ModelAdapter: decode=e2e, classes=0）
  → Model Registry（staging → validate → install → activate）
  → ModelAdapter → Runtime（30 帧推理 + FOV 热更新）
  → test_model_adapter PASS / test_model_switch_hw PASS
```

---

## 3. ModelAdapter 保持通用 — PASS

A8 未破坏 A7 通用性：
- decode_type 仍由 `rknn_query + Tensor Shape + Tensor Metadata` 自动推断（E2E/Single/DFL），无文件名/版本判断
- 三模型继续兼容：
  - 黄瓦 320 INT8 → dfl，2 类，strides 8/16/32
  - yolo261n 640 FP16 → single，80 类，xywh
  - v26m 640 INT8 → e2e，[1,300,6]，xyxy
- A1-A7 回归：单测 54/54 + test_model_adapter 3/3 + test_model_runtime 3/3 PASS

---

## 4. RuntimeProfile（用户模型配置）— PASS

`model_id / capture{width,height,offset_x,offset_y} / inference{confidence,iou,class_filter,max_detections} / fov{enabled,shape,radius,center_x,center_y}`

- 模型本身（ModelMetadata）与用户参数彻底分离：RuntimeProfile **不写入 RKNN 或 ModelMetadata**
- JSON 序列化/反序列化单测（roundtrip）PASS
- 数值边界校验单测 PASS（confidence/iou∈[0,1]、class_filter 非负、FOV 中心/半径范围）
- config/default.json 新增 `runtime_profile` 段（默认值）

---

## 5. ROI 与模型输入分离 — PASS

```
屏幕 ROI（如 640×640 或用户自定义区域）
  ↓ RGA 硬件裁剪（imcrop ROI 矩形）
模型输入（黄瓦 320 / v26m 640）
  ↓ RKNN
解码坐标映射：模型空间 → ROI 空间 → 原图（加 offset）
```

- RGA 支持 ROI（`roi_x/y/w/h`），默认 0 = 保持原 center_crop 语义（不破坏 A1-A6）
- 解码坐标映射含 ROI 偏移（`map_coords`），单测验证映射正确
- **边界检查**：`CaptureProfile::valid(frame_w, frame_h)` 拒绝越界（offset+size > 全帧）——单测 PASS
- 硬件验证：test_rga_roi_hw 1432 帧 0 错误，ROI 输出尺寸正确，运行中热切换 ROI（安全点，mid buffer 懒重建）PASS

---

## 6. FOV — PASS

处理链（顺序与 A8 一致）：

```
Capture → RGA → RKNN → Decoder → Confidence → IoU/NMS → FOV Filter → Final Detection
```

- FOV Filter 在 NMS **之后**应用（DecodeNMS 内）
- circle / rect 两种形状，归一化中心与半径
- 单测：FOV 过滤边缘目标（circle 移除/保留中心）PASS；默认关闭时行为不变 PASS
- **E2E 禁止无条件再次 NMS**：`e2e_skip_nms=true`（ModelAdapter 依 decode_type==e2e 自动设置）；process_e2e 仅 conf 过滤 + FOV Filter，不再跑 classwise NMS；单测验证两行重叠框不被抑制（保留模型 TopK 语义）PASS

---

## 7. 配置热更新 — PASS

`RuntimeConfig`（内存中，Lock-free 读）：

```
runtime_config.update(shared_ptr<const RuntimeProfile>)  // 原子替换
worker 每帧: snapshot() → 变化才 apply_runtime(conf/iou/class_filter/max_detections/FOV) + set_roi
```

- 允许运行时修改：confidence / iou / class_filter / max_detections / FOV / FOV radius / FOV center / ROI
- **禁止每帧 JSON**：RuntimeConfig 全内存；`applied_profile_` 指针比较避免每帧重复设置
- WorkerPool 集成（Params.runtime_config 可选，无则行为不变）；decoder/RGA 安全点更新
- 硬件验证：test_model_switch_hw 中 conf/iou 热更新 + FOV 热更新 + RGA ROI 热切换 PASS
- ROI 尺寸变化：V4L2 buffer 不变（全帧采集），RGA mid buffer 懒重建于安全点，不阻塞、不泄漏

---

## 8. 模型切换 — PASS

流程：上传(import) → 转换(离线脚本) → 验证(validate) → 安装(install) → 激活(activate)

- 激活前对 installed 模型做真实 RKNN+Adapter 校验；失败 → **active 保持旧模型**（恢复）PASS
- 资源无泄漏：3 轮 init+analyze+decoder+destroy 无泄漏；rga_hw 无 fd 泄漏；switch_hw PASS
- 检查覆盖：RKNN Context / 线程 / DMA / RGA FD / Buffer（加载卸载循环 + fd 检查）

---

## 9. 未来云端预留 — PASS（接口预留，不实现）

`IModelSource`（LocalFileSource 已实现；CloudModelSource 仅声明，fetch 返回"未实现"错误）
Manifest 预留：`version / sha256 / signature / origin / converter_version / runtime_version`
未来链路（未实现）：云端下发 → staging → 验证 → installed → activate

---

## 10. 性能回归 — PASS

### 黄瓦 320 INT8（3 Worker · 8 buffers · A76 绑核 · 全锁频）

| 指标 | A7 基线 | A8 | 结论 |
|---|---|---|---|
| Pipeline FPS | 239.3 | **239.5** | ≈240 保持 ✓ |
| error | 0 | 0 | ✓ |
| poll_timeout | 0 | 0 | ✓ |
| decode | 142-196µs | 192-198µs | 同量级 |

### v26m 640 INT8（3 Worker）

| 指标 | A7 基线 | A8 |
|---|---|---|
| Pipeline FPS | 29.8 | **29.8** |
| decode（e2e 免二次 NMS） | 1-5µs | 1-5µs |
| error / poll_timeout | 0 / 0 | 0 / 0 |

Adapter/抽象层未造成性能下降；未为性能修改任何模型语义。

---

## 11. 测试与回归汇总

| 项目 | 结果 |
|---|---|
| ttbox_core_tests 单元测试（含 A8 新增 14 项：runtime_profile 4 / registry 5 / fov_roi 6） | **54/54 PASS** |
| test_model_adapter（三模型） | PASS |
| test_model_runtime（三模型 3 轮） | PASS |
| test_model_switch_hw（仓库生命周期+切换+热更新） | PASS |
| test_rga_roi_hw（ROI 硬件裁剪+热切换） | PASS |
| test_rga_hw（无 fd 泄漏） | PASS |
| test_rknn_hw（300 帧 21.5 FPS） | PASS |
| test_capture_hw / WorkerPool（capture 241 FPS） | PASS |
| v26m ONNX→INT8 转换+报告 | PASS |
| 黄瓦 3W 性能 | 239.5 FPS（损失 <0.2%） |
| v26m 3W 性能 | 29.8 FPS |
| 内存/线程（3 轮加载卸载、1432 帧 ROI、1000+帧 3W 并发） | 无泄漏 / error=0 |

---

## 12. 最终验收 — A8 PASS

```
[✓] 模型仓库 PASS（import/validate/install/activate/deactivate/remove/禁止删除 active）
[✓] ONNX→RKNN PASS（FP16/INT8 + 完整检查 + 失败报告）
[✓] 模型验证 PASS（RKNN+Adapter 真实校验）
[✓] 模型安装/删除/切换 PASS（激活失败恢复旧模型）
[✓] RuntimeProfile PASS（用户配置与模型分离）
[✓] ROI PASS（Capture ROI ≠ 模型输入，边界检查，热切换）
[✓] FOV PASS（NMS 后过滤；E2E 免二次 NMS）
[✓] confidence/IoU PASS（热更新生效）
[✓] 热更新 PASS（内存 RuntimeConfig，无逐帧 JSON）
[✓] A1-A7 回归 PASS（单测 54/54 + 硬件回归）
[✓] 性能回归 黄瓦 239.5 FPS（A8 无性能损失）
[✓] v26m ONNX→INT8 全链路 PASS
```

**A8 PASS。按约定停止在 A8，不进入 A9。**
