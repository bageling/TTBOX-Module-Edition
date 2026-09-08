# P-1 1080p240 性能测试报告（ttbox2）

> **阶段**：P-1 1080p240 性能实测（原始基线，未做任何性能优化）
> **日期**：2026-08-14
> **测试结论**：**未达到 240fps**。640×640 FP16 模型（yolo261n-rk3588）物理上限约 19fps/单核，2W 最佳 34.6fps（240fps 输入利用率 14.4%）。瓶颈为 NPU 推理本身（run≈33.6ms）+ RKNN set_input runtime 转换（12.5ms）+ 多 Worker 时 V4L2 4-buffer 池 starvation。

---

## 1. 测试环境

| 项目 | 值 |
|---|---|
| 平台 | Orange Pi 5 Plus（RK3588，8 核 A76×4 + A55×4） |
| 系统 | Ubuntu 24.04.1，内核 6.1.0-1025-rockchip |
| HDMI RX | rk_hdmirx /dev/video0，**1920×1080p239.76 已锁定**（Pixelclock 594.264MHz，2040×1215） |
| V4L2 格式 | BGR3（BGR24），MPLANE，MMAP，DMA-BUF 导出 |
| RGA | librga 1.10.0（im2d，1920×1080 → 640×640 center-crop + resize） |
| NPU | rknpu DRM 模式 0.9.7，devfreq 1GHz（三核 core0/1/2） |
| RKNN Runtime | librknnrt 2.3.2 |
| 模型 | `yolo261n-rk3588.rknn`：输入 **FP16 NHWC [1,640,640,3]** size=2457600B；输出 **FP16 [1,84,8400]** size=1411200B（单输出，scale=1.0 zp=0） |
| 次要模型 | `黄瓦.rknn`：输入 **INT8 NHWC [1,320,320,3]** size=307200B；输出 6 个 INT8 NCHW（yolov5 DFL 分离格式） |
| 测试程序 | `test_worker_hw`（A1-A6 C++ Core 正式链路：V4L2 → DMA-BUF → RGA → FP16 转换 → RKNN → raw output → Decode/NMS），配置来自 `config/default.json`（conf=0.25, iou=0.45, 640×640），未修改任何 A1-A6 逻辑 |

测试链路：`HDMI RX 1080p240 → V4L2(DMA-BUF) → RGA(center-crop+resize) → u8→FP16 → RKNN set_input/run/raw-output → Decode/NMS → 统计`

**测试素材**（`C:\Users\Administrator\Desktop\测试目标`，共 6 张，仅用于 Decode/NMS 正确性验证，不计入高速链路）：img1 449×337 jfif；img2/img3 1009×515 jpg；img4/img5/img6 512×384 jfif。

**测试方式**：全部 C++（A1-A6 Core），无 Python 进链路，无逐帧 JSON，无 CPU memcpy 计入 Pipeline（u8→FP16 转换单独计时 `convert`）。每组 1000 帧（≥300 要求），`--buffers 4`（默认配置）。

---

## 2. 测试矩阵结果（yolo261n 640 FP16，每组 1000 帧）

### 2.1 总览

| 指标 | 1 Worker (core_mask=0/AUTO) | 2 Worker (cores 1,2) | 3 Worker (cores 1,2,4) |
|---|---|---|---|
| **完整 Pipeline 吞吐 FPS** | **18.9** | **34.6** | **27.0** |
| Capture FPS（输入） | 239.8 | 35.0 | 27.2 |
| 每 Worker 平均 FPS | 18.9 | 17.3 | 9.0 |
| 运行时长 / 帧数 | 52.90s / 1000 | 28.89s / 1000 | 36.98s / 1000 |
| CPU 使用率（总体） | 6.5% | 10.9% | 10.4% |
| V4L2 poll_timeouts | 0 | 494 | 656 |
| dropped（captured-processed） | 11684 | 10 | 4 |
| skipped（认领跳过） | 0 | 511 | 1657 |
| errors | 0 | 0 | 0 |

> 输入 FPS 恒为 239.76（V4L2 已锁定）；**推理吞吐 FPS ≠ 输入 FPS**——1W 时 capture 240fps 但推理只消费 18.9fps（11900+ 帧被 latest-frame 覆盖丢弃）；2W/3W 时 capture 被 V4L2 buffer 池拖到与池吞吐一致（starvation）。

### 2.2 Pipeline 各阶段耗时（avg / P50 / P95，单位 us）

| 阶段 | 1W | 2W | 3W |
|---|---|---|---|
| set_input（RKNN） | 12559 / 12520 / 12569 | 12610 / 12540 / 13220 | 13920~14250 / 12530~12580 / 36750~39200（p99 有 38ms 尖峰） |
| run（RKNN） | 33574 / 33540 / 33852 | 36122~36807 / 36129~36840 / 36980~37702 | 41068~41707 / 41691~42313 / 42408~43036 |
| output（raw, want_float=0） | 149 / 147 / 171 | 184~190 / 185~190 / 225~229 | 200~223 / 181~239 / 261~309 |
| total（set+run+out+decode） | 49385 / 49265 / 49722 | 52324~52912 / 52194~52828 / 53418~53847 | 59094~59542 / 57539~58743 / 65654~75377 |
| convert（u8→FP16，CPU） | 1461 / 1452 / 1468 | 1480~1490 / 1456 / 1475~1532 | 1646~1687 / 1458~1461 / 1540~1591 |
| decode（含 FP16→FP32） | 3098 / 3091 / 3168 | 3325~3376 / 3327~3335 / 3494~3522 | 3599~3691 / 3336~3652 / 3894~4057 |
| nms | ~0 | ~0 | ~0 |
| E2E（帧采集→推理完成） | 54834 / 54786 / 56331 | 67270~71837 / 57057~62063 / 102765~104685 | 63432~63877 / 62049~62901 / 70968~83409 |

**单帧构成（1W，全链路 50.8ms = convert 1.46 + total 49.4）**：
- run **33.6ms（66%）** — NPU 单次推理固有耗时
- set_input **12.6ms（25%）** — pass_through=0 的 runtime 转换开销
- decode **3.1ms（6%）** — FP16 输出逐元素 FP16→FP32 CPU 转换
- convert **1.5ms（3%）** — u8→FP16 CPU 查表
- output **0.15ms（0.3%）** — want_float=0 原生输出

---

## 3. 三个 NPU Core 利用率

系统为 DRM 版 rknpu（无 `/sys/kernel/debug/rknpu/load`），采用 **/proc/interrupts 三核独立中断速率 + devfreq 频率** 采集：

| 阶段 | Core0 IRQ 速率 | Core1 IRQ 速率 | Core2 IRQ 速率 | NPU 频率 | 结论 |
|---|---|---|---|---|---|
| 1W（AUTO） | 1663/s | **0** | **0** | 1GHz | **只用 Core0 单核** |
| 2W（cores 1,2） | 1523/s | 1524/s | 0 | 1GHz | 双核均衡并行 |
| 3W（cores 1,2,4） | 778/s | 778/s | 778/s | 1GHz | 三核并行，但每核速率减半 |
| 空闲 | 470/s | 215/s | 89/s | 1GHz | 基线噪声 |

关键结论：
- **三核确实同时工作**（3W 时三个 IRQ 都在产生），**但每核效率显著下降**：run 从 1W 33.6ms → 2W 36.5ms（+8.6%）→ 3W 41.5ms（+23.6%）。多 context 分核并行存在 NPU 共享资源竞争。
- 单 context 三核（`core_mask=7` 对照）：run = **32.6ms，几乎与单核 33.6ms 相同** → 该 FP16 模型算子**无法跨核切分**，三核并行对单次推理无效。
- NPU devfreq `load` 节点恒显示 100%（空闲也 100），不可靠，以 IRQ 速率为准。

---

## 4. DDR / CPU / buffer 使用情况

| 阶段 | DDR（dmc）load | DDR 频率 | CPU 使用率 |
|---|---|---|---|
| 1W | 23% | 2112MHz | 6.5% |
| 2W | 39% | 2112MHz | 10.9% |
| 3W | 35% | 2112MHz | 10.4% |
| 空闲 | 2% | 2112MHz | - |

- **DDR 峰值 39%，未饱和**——DDR 不是主瓶颈，但三核并行时 run 变慢（33.6→41.5ms）与其相关（每核同时搬运输入/权重/输出）。
- **CPU 整体 <11%**：推理路径上 CPU 仅在 u8→FP16 convert（1.5ms）与 FP16→FP32 decode（3.1ms）阶段忙碌，整体不饱和。
- **V4L2 buffer（MMAP ×4，latest-frame 模式）是 2W/3W 的硬瓶颈**：
  - 1W：poll_timeouts=0，capture 239.8fps（流畅）
  - 2W：poll_timeouts=494，capture 35.0fps = 池吞吐（starvation）
  - 3W：poll_timeouts=656，capture 27.2fps = 池吞吐
  - 黄瓦 2W/3W 同样被限制到 51~58fps。

---

## 5. 重点检查项结论（10 项）

| # | 检查项 | 结论 |
|---|---|---|
| 1 | RKNN set_input 是否仍有大额转换 | ✅ **命中**。pass_through=0 下 12.6ms/帧（raw 模式对照：pass_through=1 仅 0.21ms） |
| 2 | FP16/INT8 输入路径 | FP16 set_input 12.5ms vs 黄瓦 INT8 0.07ms（差 ~170 倍）。FP16 输入走 runtime 转换路径开销巨大 |
| 3 | RGA 输出能否直接给 RKNN | ❌ **不能**。pass_through=1 零拷贝直喂 FP16 NHWC 产生错误推理（图片验证 317 个误检 vs pt0 正确 2 个）。RGA 输出需经 runtime 转换（pt0）或验证正确 native 输入格式 |
| 4 | RKNN output 是否已 want_float=0 | ✅ 已满足。raw output 0.15ms vs want_float=1 的 1.65ms（test_rknn_hw 对照） |
| 5 | Decode 是否 CPU 大量 FP16→FP32 | ✅ **命中**。decode 3.1ms 为逐元素 half→float（黄瓦 INT8 反量化仅 0.08ms，差 ~40 倍） |
| 6 | 3 个 NPU Core 是否真正并行 | ⚠️ **并行但无收益**。IRQ 证实三核同时工作；但 run 反而 33.6→41.5ms，单 context mask=7 也无效（32.6ms≈单核） |
| 7 | DMA-BUF/V4L2 buffer 是否成为瓶颈 | ✅ **命中**。4-buffer 池在 2W/3W 下 starvation（poll_timeouts 494/656，capture=池吞吐） |
| 8 | DDR 带宽是否成为瓶颈 | ❌ 未饱和（峰值 39%）。但三核时 run 变慢与 DDR 竞争相关 |
| 9 | Worker 抢帧是否不均衡 | ⚠️ 分配均衡（3W：334/333/333），但 skipped 1657 次（帧在 worker 忙时被认领跳过） |
| 10 | 240fps 下是否 buffer starvation | ✅ **命中**。2W/3W capture 被限制到 27~35fps，无法维持 240fps 输入消费 |

---

## 6. 理论 FPS vs 实际 FPS

| 配置 | 理论 FPS（单帧耗时推导） | 实际 FPS | 达成率 |
|---|---|---|---|
| 1W | 1000/50.8ms ≈ 19.7 | 18.9 | 96% |
| 2W | 2×19.7 ≈ 39.4（线性外推） | 34.6 | 88%（run 变慢 8.6% + capture 限制） |
| 3W | 3×19.7 ≈ 59.1（线性外推） | 27.0 | 46%（run 变慢 23.6% + capture starvation） |
| 黄瓦 1W（INT8 320） | 1000/6.0ms ≈ 166 | 120.3 | 72%（轮询/认领开销） |
| 黄瓦 2W/3W | 348/500 | 57.9 / 51.3 | —（被 V4L2 buffer 池限制） |

补充（纯推理对照，test_rknn_hw raw 100 帧，无 capture/RGA/decode）：
- pass_through=1：total 33.4ms → 29.9fps
- pass_through=0：total 46.0ms → 21.8fps（set_input 12.7ms）

---

## 7. 240fps 输入利用率

| 配置 | 推理吞吐 | 输入 240fps | 利用率 |
|---|---|---|---|
| 1W | 18.9 | 239.8 | **7.9%** |
| 2W | 34.6 | 35.0 | **14.4%** |
| 3W | 27.0 | 27.2 | **11.3%** |
| 黄瓦 1W | 120.3 | 238.6 | **50.1%** |
| 黄瓦 2W/3W | 57.9 / 51.3 | 58.3 / 51.4 | **24% / 21%**（capture 受限） |

---

## 8. 是否达到 240fps：差距与原因

**未达到。** 最佳配置 2W 仅 34.6fps，距 240fps 差 **205.4fps（14.4%）**。

原因（按贡献排序）：
1. **NPU 单帧推理 33.6ms（1W）是物理上限**：640×640 FP16 yolov6n 在 RK3588 1GHz 上单核约 30fps 算力，三核并行无法加速该模型（core_mask=7 实测 32.6ms≈单核）。→ 决定单 worker ~19fps 的天花板。
2. **set_input 12.6ms（pt0 runtime 转换）**：占 25%，pt1 虽可省（0.21ms）但结果错误，需验证正确 native 输入格式后才能利用。
3. **多 Worker 收益递减**：2W 1.83×、3W 反而 0.78×（run 变慢 + V4L2 4-buffer starvation 把 capture 压到 27fps）。
4. 即使完美消除 set_input/convert/decode/buffer 所有开销，单 worker 理论也仅 ~30fps（run 33.6ms），三核若真正并行且无竞争最多 ~3×30=90fps，仍**达不到 240fps**。

**结论：在当前 640 模型 + 1080p240 输入下，任何配置都无法接近 240fps**；240fps 目标要求换更小模型（黄瓦 320 INT8 单帧 6ms，理论上限 ~166fps/1W，消除 buffer 瓶颈后 2W+ 可望 240fps）。

---

## 9. 下一步最高 ROI 优化项（仅建议，本次未执行）

1. **V4L2 buffer 池 4 → 8/16**（`--buffers`）：消除 2W/3W capture starvation。收益最大（黄瓦 2W 从 58 → 有望 200+；yolo 2W 从 34.6 → ~38）。
2. **验证 pass_through=1 的正确输入格式**（FP16 归一化 /255 或 runtime native 布局）：若可行，set_input 12.6ms → 0.2ms，1W 从 18.9 → ~26fps（+37%）。
3. **Decode FP16→FP32 批量/NEON 向量化**：3.1ms → <1ms。
4. **convert u8→FP16 NEON 向量化**：1.5ms → <0.5ms。
5. **模型层**：INT8 量化 640 模型（set_input 降 ~170×、decode 降 ~40×，run 可能更快）；或换 320 模型（黄瓦 1W 已 120fps，消除 buffer 瓶颈后 2W 可覆盖 240fps 输入）。
6. **DDR/时钟策略**：三核并行时 DDR 39% 有压力，可评估 DMC 频率/总线优先级（影响小）。

---

## 10. 附：Decode/NMS 正确性验证（测试图片）

独立工具 `test_img_decode`（stb_image 解码 → 双线性缩放 → u8→FP16 → RKNN → raw output → DecodeNMS），**不计入高速 Pipeline**：

| 图片 | 分辨率 | pass_through=0 检测结果 |
|---|---|---|
| img4（人脸） | 512×384 | 2 个检测：class=0 score=0.774 box=(192,43)-(294,159)；score=0.657 box=(113,90)-(146,148) → **人脸定位正确** |
| img2/3 | 1009×515 | 1080/1698 检测（候选 1250/2002，画面多目标密集） |
| img1/5/6 | 449×337 / 512×384 | 462/2332/2848 检测 |

- **pass_through=0（当前链路）Decode/NMS 验证 PASS**：真实人脸被正确框出（score 0.77/0.66，坐标覆盖面部区域）。
- **pass_through=1 对照**：同一图片产生 317 个 class=0 score=0.998 的小框 → **输入格式错误导致推理失效**，确认当前必须保持 pt0（与 Python rknnlite 对齐）。

**测试过程未修改任何 A1-A6 功能代码、未改动性能参数、未进入 A7-A10。** 原始基线数据文件：板端 `/tmp/p1_1000.log`（yolo 1000 帧）、`/tmp/hw300.log`（黄瓦 300 帧）、`/tmp/raw100.log`（pass_through 对照）、`/tmp/mon_p1.log`（NPU/DDR/IRQ 监控 420s）。
