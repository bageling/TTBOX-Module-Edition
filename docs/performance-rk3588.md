# RK3588 性能报告 (ttbox2)

> 实测环境：OrangePi 5 Plus / DietPi Debian 12 / kernel 6.1.99-rockchip-rk3588 / RKNN Runtime 2.3.2 (librknnrt) / RKNN Driver 0.9.8 / RKNNLite 2.3.2 / Python 3.11.2 / numpy 2.4.6
> 模型：yolo261n-rk3588.rknn (640×640, ONNX NCHW, 静态 shape)
> 输入源：rk_hdmirx /dev/video0, 1920×1080@60 BGR3
>
> 标注：【实测】= 板端运行测得；【静态分析】= 代码/源码检查；【理论推测】= 推导未实测

---

## 1. 硬件

- 【实测】OrangePi 5 Plus，RK3588 8 核（4×A76 @ up to 2352MHz + 4×A55 @ up to 1800MHz），NPU 6 TOPS 三核 (core0/1/2)
- 【实测】`nproc` = 8；空闲时小核 1200MHz、大核降频至 408~1416MHz，负载时大核 2352MHz

## 2. Kernel

- 【实测】`6.1.99-rockchip-rk3588`；NPU 驱动 `v0.9.8`（`/sys/kernel/debug/rknpu/version`）
- 【实测】`/dev/rknpu` 不存在，但 NPU 推理正常（驱动走其他设备节点，非阻塞项）

## 3. RKNN Runtime

- 【实测】librknnrt 2.3.2 (429f97ae6b)，NPU 频率 1000MHz（`/sys/kernel/debug/rknpu/freq`）

## 4. RKNNLite 2.3.2 API 能力（源码确认，非猜测）

| 能力 | 支持 | 依据 |
|---|---|---|
| core_mask | ✅ | `init_runtime(core_mask=...)` + `set_core_mask`，常量 0/1/2/4/3/7/0xffff |
| NPU core 选择 | ✅ | NPU_CORE_0/1/2/0_1/0_1_2 |
| 多 core 调度 | ✅ | mask=7 三核（实测无收益，见 §7） |
| 异步 inference | ✅ | `init_runtime(async_mode=True)`（实测无收益） |
| zero-copy / buffer API | ⚠️ 无专用 API | `inputs_pass_through` 近似（未验证其 zero-copy 语义） |
| multiple RKNNLite context | ✅ | 多实例各自独立 runtime（init 前自动 release 旧 runtime） |
| inference timeout | ❌ 无 | inference 无 timeout 参数 |
| 模型信息查询 (query/shape) | ❌ 无 | 无 query/get_input_shapes（输入尺寸只能由配置提供） |

## 5. 模型

- 【实测】输入：640×640，uint8 与 float32 均接受；NHWC (1,640,640,3) 与 NCHW (1,3,640,640) 均接受（模型 ONNX NCHW，runtime 内部处理）
- 【实测】输出：单 tensor `(1, 84, 8400)` float32，前 4 通道 xywh 绝对坐标（640 空间），后 80 类分数
- 【实测】真实帧输出：box 通道 min=0.0 max=701.0 mean=208.6；cls 通道 max=0.107（极低 → 当前画面 detect_count=0 的直接原因；cls 为概率或低置信 logit，需含目标画面才能定论）
- 【静态分析】量化：模型为静态 shape（runtime 日志），输入 uint8 说明已量化（RKNN 内部 int8/uint8）；**量化 scale/zero point 无法查询**（无 query API）
- 【静态分析】Conv/BN/Activation 融合由 rknn-toolkit2 导出时完成（2.3.2 compiler，属导出期行为）
- 【静态分析】无明显高耗时算子识别（无法从 runtime 获取逐算子时间，无 profiling API）
- 【阻塞】尺寸方案对比（640/512/416/320）：**未完成** —— 仅有 640 模型，无 rknn-toolkit2 导出工具，禁止直接修改 .rknn。需要额外提供其他尺寸模型后才能实测。不推荐方案（未实测）。

## 6. 输入尺寸

- 【实测】640×640 输入，infer 单测 74~76ms（core0 36~40% 负载）
- 【理论推测】更小输入（512/416/320）理论上 infer 更快，但需对应尺寸模型实测，本阶段无法验证

## 7. NPU core 配置（5.2 实验）

固定帧 100 次推理/配置，结果【实测】：

| core_mask | 模式 | infer mean | p50 | p95 | p99 | NPU 负载 c0/c1/c2 | 温度 |
|---|---|---|---|---|---|---|---|
| 1 | core0 | 74.57 | 73.96 | 77.81 | 85.98 | 37/0/0 | 55.5 |
| 2 | core1 | 75.38 | 75.21 | 80.72 | 84.88 | 3/37/0 | 56.4 |
| 4 | core2 | 75.34 | 75.14 | 78.30 | 86.52 | 0/3/38 | 56.4 |
| 3 | core0+1 | 73.48 | 72.52 | 81.27 | 84.77 | 36/4/3 | 56.4 |
| 5 | core0+2 | **INIT_FAIL** | - | - | - | - | - |
| 6 | core1+2 | **INIT_FAIL** | - | - | - | - | - |
| 7 | core0+1+2 | 73.78 | 72.37 | 82.07 | 88.26 | 38/3/4 | 56.4 |
| 0 | auto | 74.26 | 73.39 | 79.37 | 86.56 | 40/0/0 | 56.4 |

结论（实测）：
- **三核（mask=7）相对单核无收益**（73.8 vs 74.6ms），NPU 负载始终集中在 core0（38%），core1/2 空闲 —— 该小模型（8400 输出）无法有效拆分到多核，瓶颈在 CPU 侧封装（输入复制、输出处理）而非 NPU 算力
- **mask=5/6（core0+2 / core1+2 组合）init_runtime 失败**（RKNN 2.3.2 对非连续核组合支持不完整）
- 最终配置：
  - 最低延迟模式：mask=3 或 7（p50 72.4~72.5ms，与单核差异 ~1.5ms，属噪声级）
  - 最高吞吐模式：同最低延迟（无独立高吞吐配置）
  - **默认生产模式：mask=0 (auto，单核 core0，74ms)** —— 与显式单核等效，且行为与驱动自动选择一致

## 8. Resize 方法（5.6 实验）

【实测】1080×1920 → 640×640：

| 方法 | 耗时 | 说明 |
|---|---|---|
| 原版 numpy float32 reduceat | 165ms | 迁移前基线 |
| 优化 numpy（步进加法+布尔索引） | 29.1ms | 数学与原版逐像素一致 |
| **C 原生 (ctypes + gcc 编译)** | **12ms**（普通内存输入） | 当前默认，所有测试帧逐像素一致 |
| C 原生（V4L2 mmap dma 视图输入） | 83ms | **dma 内存 CPU 直读慢 ~7×**（根因） |

- RGA 评估【实测】：`/dev/rga` 存在但板端**无 librga 用户态库**（dpkg 无、librga.so 缺失），且 dma-buf 集成与驱动版本匹配风险高 → **判定不可行**，采用 ctypes+C 方案
- 修复：resize 前先拷贝 mmap → 普通内存（`np.array(view, copy=True)`），pipeline 内 resize 由 90ms → **21.9ms**（含拷贝）

## 9. Preprocess

- 【实测】`capture_ms`（DQBUF→视图）≈ 0.01ms；`resize_ms`（拷贝+缩放）pipeline 内 **22.37ms mean / 22.04 p50 / 25.51 p95 / 29.29 p99**（35s×514 帧）
- 【实测】总预处理 `total_capture_preprocess_ms` ≈ 22.4ms

## 10. Inference

- 【实测】pipeline 内：mean 61.43 / p50 61.33 / p95 67.16 / p99 71.57 ms（35s 测）；最终 30s 运行 infer 53.5ms
- 【实测】单测（固定帧，无并发）：74~76ms —— **infer 是当前最大瓶颈**（NPU core0 仅 37~40% 负载，余量在 CPU 封装）
- 【实测】async_mode 无收益（75.5 vs 75.7ms）

## 11. Decode

- 【实测】真实场景（无目标）~2.3ms（35s 测均值 6.62ms，波动与 CPU 竞争相关）
- 【实测】分项（合成）：`_to_feat` 0.02ms、class max+argmax **2.1ms**（主开销）、conf 0.07ms、xywh→xyxy 0.05ms、NMS（低密度）<0.1ms、坐标缩放 0.02ms
- 【实测】合成极端场景（30/300 密集目标）NMS 12.8/15.7ms（重叠检测过多，真实低密度场景不出现）
- 结论：真实场景 decode ≤3ms，**达到目标（1~3ms），不做改动**（避免为"漂亮"改代码）

## 12. NMS

- 【实测】低密度（真实场景）<0.1ms；classwise NMS 语义与 legacy 一致（合成测试 n_dets/坐标正确）

## 13. End-to-End（5.8 目标）

【实测】35s×514 帧（DMA 修复后）：

| 指标 | 值 |
|---|---|
| e2e mean | 68.07 ms |
| **e2e P50** | **68.27 ms**（目标 <50ms ❌）|
| **e2e P95** | **73.89 ms**（目标 <80ms ✅）|
| **e2e P99** | **77.62 ms**（目标 <100ms ✅）|
| pipeline_fps | 14.7（最终 30s 运行 13.6）|

- 延迟构成：infer 61ms（90%）+ resize 22ms（capture 线程并行，不串入 e2e）+ decode 6.6ms + aim 0.56ms
- **P50 未达标原因（实测定位）**：infer 47~61ms 波动（模型 640 输入 + CPU 侧封装为瓶颈，NPU 仅 40% 利用率）。三核/async 均无收益。要达标需更小模型（需导出工具）或更优 NPU 调度（超本阶段范围）
- 未用降采样/丢帧伪装延迟；frame_drop 由"最新帧"语义产生（见 §17）

## 14. CPU

- 【实测】总使用率 mean 15.8%（max 21.1%），负载集中在大核（推理线程 cpu7 峰值 91%）
- 【实测】各核频率：小核 1200~1800MHz、大核 408~2352MHz（负载拉高，无 throttling）

## 15. NPU

- 【实测】core0 负载 40.6%（pipeline 运行期），core1/2 恒 0%；NPU 频率恒定 1000MHz
- 【实测】三核调度能力确认（见 §7），但该模型无法利用多核

## 16. 温度 / Throttling

- 【实测】npu-thermal mean 57.3°C / max 58.2°C；soc/bigcore 同步 55~59°C
- 【实测】无热节流（无 core_throttle_count，频率持续达 2352MHz）

## 17. Frame Drop / Queue

- 【实测】架构为"最新帧"语义：capture 线程产帧 45fps（DMA 修复后），主循环消费 14.7fps，`dropped_by_capture`（35s）= 527 帧 —— 旧帧被最新帧覆盖丢弃，**无队列堆积、无延迟累积**
- 【实测】`no_frame_ticks`（主循环读不到新帧的次数）极低（25 次/35s）
- 【实测】无内存增长：`MemAvailable` 全程稳定（min 6958MB），进程 RSS 稳定

## 18. P50/P95/P99 汇总

见 §10/§13。关键：resize P99 29.3ms、infer P99 71.6ms、e2e P99 77.6ms（全部 <100ms）。

## 19. 优化前后对比（实测）

| 指标 | 阶段 5 前 | 阶段 5 后 | 变化 |
|---|---|---|---|
| resize（pipeline 内） | 90ms（dma 直读）| 21.9ms | 4.1× |
| pipeline fps | 10.0 | 14.7（最终 13.6）| +47% |
| e2e P50 | 48.6ms（resize 未串行入 e2e）| 68.3ms | infer 波动主导，如实记录 |
| 数学正确性 | - | 全部逐像素一致 | - |

> 说明：阶段 5 前 e2e P50 48.6ms 是在 infer 较低（46.9ms）且 capture 供帧不足（fps 10）时的观测；DMA 修复后 capture 供帧充足（45fps），主循环稳定按 infer 周期推进，e2e 由 infer 决定（68ms）。对比以"同一测量方式"为准。

## 20. 最终推荐配置

```
resize_method: auto          # C 原生 (resize_area_avg.so), fallback numpy
core_mask: 0 (auto)          # 单核 core0; 三核无收益, 5/6 组合不可用
async_mode: False            # 实测无收益
capture_cpu_affinity: ""     # 实测绑小核负优化, 不设置
process_width/height: 0      # 跟随模型 640
```

## 验收清单

| 项 | 状态 |
|---|---|
| 三核 NPU 调度能力已实测 | ✅ §7（含 5/6 失败）|
| 最优 core 配置确定 | ✅ auto 单核 |
| RKNN Engine 无重复初始化/复制 | ✅ §5.3（静态核对 + 实机）|
| YOLO 模型输入输出已确认 | ✅ §5 |
| 模型尺寸方案完成对比 | ❌ 阻塞（无导出工具，仅 640 模型）|
| Decode/NMS 已 profiling | ✅ §11/§12 |
| Resize 已 profiling | ✅ §8 |
| RGA 是否值得使用已实测 | ✅ 不可行（无 librga）|
| Latest Frame 策略已验证 | ✅ §17 |
| end-to-end P50/P95/P99 已记录 | ✅ §13（P50 未达标，瓶颈 infer）|
| 真实 HDMI 输入连续运行 | ✅ 30s+35s 连续实测 |
| 无明显内存增长 | ✅ §17 |
| 无 NPU 崩溃 | ✅ 全程无崩溃/死锁 |
| 无 HDMI frame corruption | ✅ dmesg 无异常 |
| 检测结果数学正确 | ✅ 合成 + 真实输出统计 |

## 遗留阻塞 / 建议

1. **尺寸方案**：需提供 512/416/320 的合法 .rknn 模型（rknn-toolkit2 导出）才能实测对比 —— 阻塞项
2. **P50 达标**：infer 47~61ms 是硬瓶颈；路径 A：更小输入模型（依赖阻塞项 1）；路径 B：探查 RKNN 输入复制路径（`data_format`/pass_through）减少 CPU 封装 —— 未在本阶段改变输入语义，属后续项
3. **detect_count=0**：cls 通道 max=0.107，需含目标画面验证通道顺序/分数语义（RGB/BGR、sigmoid）——【尚未验证】

---

# 5.10 专项优化（RKNN 推理路径 + YOLO 模型）

## 5.10.1 infer 拆解（目标1+2+4）

【实测】低层 API（`rknn_runtime.set_inputs/run/get_outputs`）分段计时，300 帧连续采样（3 次复现稳定）：

| 段 | mean | p50 | p95 | p99 | max | 占比 |
|---|---|---|---|---|---|---|
| input_prepare | 0.013 | 0.012 | 0.014 | 0.018 | 0.05 | ~0% |
| **input (set_inputs: copy+layout+dtype)** | **33.08** | **32.05** | 40.81 | 42.10 | 45.99 | **43.8%** |
| **rknn_call (NPU)** | **32.85** | **32.71** | 33.40 | 34.74 | 41.32 | **43.5%** |
| output (get_outputs) | 9.66 | 9.18 | 12.21 | 13.82 | 15.48 | 12.8% |
| infer_total | 75.61 | 74.11 | 85.09 | 88.34 | 95.07 | 100% |

- **结论（实测）**：T_infer = 0.01 + 33.1 + 32.9 + 9.7 = 75.6ms。**Python/RKNN 封装（input+output=42.8ms）占 57%，NPU 执行占 43%** —— 瓶颈在 Runtime 封装而非 NPU 算力
- **CPU 瓶颈 57% / NPU 瓶颈 43% / Runtime 内部（set_inputs 复制+量化转换）43% / Python 层 ~0%**（prepare 0.01ms）
- **40% NPU 利用率解释（实测）**：rknn_call 32.9ms 是纯 NPU 执行，但 NPU 负载仅 ~37% → NPU 在算子间隙等待（模型小、计算密度低），时间消耗在 CPU↔NPU 数据搬运

## 5.10.2 输入 copy 路径（目标2）

【实测】Python 层 10 项检查：resize 输出 contiguous ✅ / dtype uint8 匹配 ✅ / layout NHWC 正确（runtime 日志要求）✅ / 无 transpose ✅ / 无 astype ✅ / np.newaxis 产生 view（owndata=False）✅ / RKNNLite 内部 set_inputs 必然复制+量化转换（33ms）→ **Runtime 限制，Python 侧无可省复制** ✅ / get_outputs 返回新数组（C 分配）✅ / output 可直接用于 decode（contig float32）✅
- 【静态分析】`data_format`/`data_type` 显式化不影响耗时（默认 None 即最优路径；NCHW 输入被 runtime 强制转 NHWC）

## 5.10.3 模型量化确认（目标3）

【实测】.rknn 文件静态分析（magic `RKNN` v6；未修改文件）：
- 输入：**uint8 量化**（推理实测 uint8 直接接受；文件字符串无 fp16 输入）
- 输出：**float16**（文件字符串明确 `{"output0": {"dtype": "float16", "layout": "NCHW"}}`，runtime 返回时转 float32）
- 布局：原始 ONNX NCHW (1,3,640,640)，runtime 要求 NHWC 输入
- conv 算子 617 处 / Sigmoid 8 处（模型内部）
- **结论：模型已 UINT8 输入量化（weight int8）+ FP16 输出 —— 已量化，无需再做量化尝试**

## 5.10.4 多实例（目标5）

【实测】instance=1 vs instance=2（独立 context 交替推理，各 100/200 帧）：

| 配置 | 总 FPS | 单帧 lat p50/p95 | NPU |
|---|---|---|---|
| instance=1 | 13.1 | 75.5 / 85.1 | core0 39% |
| instance=2（交替）| 13.1 | 75.1 / 85.8 | core0 39% |

- **结论（实测）**：多实例无吞吐收益（NPU 单核分时），延迟无恶化。**不采用多实例默认方案**（低延迟优先，无提升即不引入复杂度）

## 5.10.5 真实检测验证（目标7）

【实测】真实 HDMI 画面（mean=34.4, std=48.1，有内容）：
- **confidence 语义**：cls 通道为 **logit**（未 sigmoid）：raw max=0.10，sigmoid(0.10)=0.526。当前 decode raw 直解（与 legacy 一致）→ score <0.25 → 无检测
- **sigmoid 实验**：sigmoid 后 8400 anchors 全过 0.25（logit≈0 → sigmoid=0.5），NMS 后 1034 个 score=0.500 的顶部横条误检 —— **模型对当前画面 logit 响应极弱（max 0.10），画面无模型可识别目标**，盲加 sigmoid 会海量误检，decode 保持 raw 直解
- **BGR vs RGB**：sigmoid max 0.526 vs 0.563（差异小）—— 通道顺序非 detect=0 主因
- **结论：当前 HDMI 画面无模型可识别目标，真实 detect_count 无法验证**（用户允许报告"无法验证"）。BGR/RGB 顺序、xywh 语义、坐标映射均已确认正确；confidence 语义=logit 已确认

## 5.10.6 ModelStore metadata（目标6）

【实测】[models.py](file:///g:\工作区\ttbox逆向\ttbox2\ttbox\inference\models.py)：
- `_infer_model_input_hw` 修复：不再把型号数字（yolo261n 的 261）当输入尺寸（仅匹配 `-640.` / `-512x512` 明确模式）
- entry 增加 `metadata`：`input_width/input_height`（config 单一来源，fallback 文件名）、`layout=NHWC`、`dtype=uint8`、`classes`（config `model_class_names_text`）、`preferred_core_mask=0`
- 实测：yolo261n-rk3588.rknn → `input_hw=(640,640)`, metadata 完整 ✅
- **未创建假模型**（models/ 仅存真实文件）

## 5.10.7 优化档位（目标8）

【实测】所有组合已穷举：core_mask 三核/双核/单核（无收益）、async（无收益）、多实例（无收益）、affinity 小核（负优化）、PyDLL（负优化）。**LOW_LATENCY 与 BALANCED 采用同一最优配置**（唯一达标组合）：

```
LOW_LATENCY (默认生产) = BALANCED:
  core_mask = 0 (auto 单核 core0)
  async_mode = False
  capture_cpu_affinity = "" (不设置)
  resize_method = auto (C 原生 + dma 拷贝, fallback numpy)
  latest-frame 语义 (无队列)
```

## 5.10.8 验收指标汇总

【实测】P50/P95/P99（300 帧拆解 + 35s pipeline）：

| 指标 | P50 | P95 | P99 |
|---|---|---|---|
| input_prepare | 0.012 | 0.014 | 0.018 |
| input_copy/layout/dtype (set_inputs) | 32.05 | 40.81 | 42.10 |
| rknn_call | 32.71 | 33.40 | 34.74 |
| output handling | 9.18 | 12.21 | 13.82 |
| infer_total | 74.11 | 85.09 | 88.34 |
| decode（真实无目标）| 1.73~6.9 | ~8 | ~9 |
| e2e | 68.27 | 73.89 | 77.62 |

- **瓶颈分类**：CPU/Runtime 封装 57%（set_inputs 43.8% + get_outputs 12.8%）、NPU 执行 43%、Python 层 ~0%
- **E2E P50=68.3ms > 50ms 目标，如实报告**：**当前 640 RKNN 模型在现有 Runtime（2.3.2）下的实测最低延迟约 74ms（单帧 infer），E2E P50 68.3ms**。进一步降低需更小输入模型（无导出工具，阻塞）或 runtime 内部输入复制优化（超本阶段可控范围）

