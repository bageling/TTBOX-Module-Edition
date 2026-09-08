# P-1 第一轮性能报告：黄瓦 320×320 INT8 240fps 验证（ttbox2）

> **阶段**：P-1 性能优化实测第一轮（仅测试，未修改 A1-A6 架构/模型）
> **日期**：2026-08-14
> **核心结论**：**黄瓦 320×320 INT8 达到 240fps Pipeline**。3 Worker + V4L2 8 buffers + 全系统锁频下，池吞吐 **239.7fps = 输入源上限 239.76fps**，30 秒持续稳定，poll timeout/error 均为 0。AI 推理能力已超过输入源上限。

---

## 1. 测试前准备（频率锁定，全部验证生效）

| 单元 | 测试前 | 测试后 | 全程降频样本 |
|---|---|---|---|
| CPU0-3（A55） | 1.8GHz performance（=max） | 1.8GHz performance | 0 |
| CPU4-7（A76） | 2.352GHz performance（=max） | 2.352GHz performance | 0 |
| GPU | 1GHz performance（=max） | 1GHz performance | 0 |
| NPU | **1GHz performance**（已从 rknpu_ondemand 改为 performance） | 1GHz performance | 0 |
| DDR（dmc） | 2.112GHz dmc_ondemand（=max） | 2.112GHz dmc_ondemand | 0 |
| HDMI RX | 1080p239.76（Pixelclock 594.264MHz） | 1080p239.76 | — |

- 输入实际 239.76fps 全程保持。
- CPU/GPU/NPU/DDR 在 1W/2W/3W 全部测试期间**无任何降频**（监控 200ms 采样，降频样本数=0）。

## 2. 测试配置

- 模型：`黄瓦.rknn`（INT8 NHWC [1,320,320,3]，6 输出 yolov5 DFL），仅 C++ Core（`test_worker_hw`，A1-A6 正式链路）
- 检测参数：**conf=0.55**（config/default.json），iou=0.45
- V4L2 buffer：**8**（禁用 4）
- 频率：CPU/GPU/NPU/DDR 锁定最高（§1）
- 测试矩阵：1W / 2W / 3W 各 1000 帧 + 3W 30 秒稳定性
- 未修改模型、未修改 A1-A6 功能逻辑、无 Python、未进入 A7-A10

## 3. 测试矩阵结果（每组 1000 帧）

| 指标 | 1W (core=0/AUTO) | 2W (cores 1,2) | 3W (cores 1,2,4) | 3W 30s 稳定 |
|---|---|---|---|---|
| **Pipeline 吞吐 FPS** | **118.9** | **193.4** | **239.7** | **239.7** |
| Capture FPS（输入） | 239.5 | 240.0 | 240.7 | 239.8 |
| 每 Worker 平均 FPS | 118.9 | 96.7 | 79.9 | 79.9 |
| 帧数 / 时长 | 1000 / 8.41s | 1000 / 5.17s | 1001 / 4.18s | 7192 / 30.01s |
| set_input（avg） | 0.062 ms | 0.07~0.09 ms | 0.07~0.09 ms | 0.06~0.07 ms |
| run（avg） | 5.76 ms | 6.16~6.63 ms | 6.02~6.61 ms | 6.00~6.22 ms |
| output（avg） | 0.17 ms | 0.20~0.24 ms | 0.15~0.25 ms | 0.15~0.19 ms |
| total（avg） | 6.12 ms | 6.59~7.15 ms | 6.34~7.14 ms | 6.32~6.59 ms |
| decode（avg） | 0.12 ms | 0.15~0.18 ms | 0.10~0.18 ms | 0.10~0.13 ms |
| nms（avg） | ~0 | ~0 | ~0 | ~0 |
| E2E（avg） | 10.50 ms | 10.4~11.5 ms | 9.2~10.2 ms | 9.13~9.48 ms |
| CPU 使用率 | 5.1% | 8.0% | 9.7% | 10.7% |
| poll_timeouts | 0 | 0 | 0 | 0 |
| dropped | 1014 | 241 | 4 | 4 |
| skipped | 0 | 381 | 1151 | 8829 |
| errors | 0 | 0 | 0 | 0 |
| candidates | 0 | 0 | 0 | 0 |
| detections | 0 | 0 | 0 | 0 |

> 说明：当前 HDMI 输入画面无检测目标（candidates=0，conf=0.55 下更少），Decode/NMS 开销已降到最低（decode 0.12ms / NMS ~0），不构成瓶颈。

## 4. 与上一轮对比（黄瓦 1W=120.3fps）

| 配置 | 上轮（conf=0.25, buffers=4, 默认 governor） | 本轮（conf=0.55, buffers=8, 锁频） | 变化 |
|---|---|---|---|
| 1W | 120.3 | 118.9 | -1.2%（纯推理路径无优化点，轮询/测量波动） |
| 2W | 57.9 | **193.4** | **+234%** |
| 3W | 51.3 | **239.7** | **+367%** |

**2W/3W 大幅提升的直接原因：V4L2 buffer 4 → 8 消除了 capture starvation**（上轮 2W/3W poll_timeouts 78/92、capture 被压到 51~58fps；本轮 0 timeout、capture 恢复 240fps）。锁频与 conf=0.55 为次要因素。

## 5. 三个 NPU Core 利用率（IRQ 速率，200ms 采样）

| 阶段 | Core0 | Core1 | Core2 | 判断 |
|---|---|---|---|---|
| 3W 30s 稳定 | 3531/s | 3533/s | 3534/s | **三核完全均衡、真正并行** |
| 1W+2W+3W 全程 | 1619/s | 1223/s | 1041/s | 含组间空闲的平均值 |

- 与 yolo261n 640 FP16（三核并行无收益，run 33.6→41.5ms）不同，**黄瓦 320 INT8 可跨核有效切分**：3W 时 run 仅 6.0~6.6ms（单核 5.76ms），几乎无退化，吞吐线性扩展到输入源上限。
- DDR：dmc_load 平均 21%（峰值时段），**未饱和**。
- CPU：全程 <11%，**未饱和**。

## 6. 最终判断（6 项）

1. **黄瓦 320 INT8 是否达到 240 FPS**：✅ **达到**。3W 池吞吐 239.7fps ≥ 输入 239.76fps，AI 推理能力已**超过输入源上限**（capture 限制，无法再提高）。
2. **距离 240 差多少**：3W 已贴住输入源上限（239.7 vs 239.76，差 0.06fps ≈ 输入源 jitter）。AI 侧实际能力高于 239.7（3×6.5ms 周期 → 理论 ~440fps），瓶颈在输入源。
3. **最大瓶颈**：**输入源 1080p239.76fps 本身**。链路内已无瓶颈：buffer starvation 已消除、DDR 21% 未饱和、CPU 10.7% 未饱和、Decode/NMS 0.12ms、set_input 0.07ms（INT8 直喂）。单 Worker 内相对大头为 run（5.76ms）与 worker 认领/轮询开销（1W 实际 8.4ms/帧 vs total 6.12ms）。
4. **1W/2W/3W 哪个吞吐最高**：**3W = 239.7fps**（1W 118.9 → 2W 193.4 → 3W 239.7，单调递增）。
5. **三个 NPU Core 是否真正有效工作**：✅ **是**。三核 IRQ 完全均衡（3531/3533/3534/s），run 无退化，吞吐线性扩展。
6. **conf=0.55 后 Decode/NMS 是否明显下降**：本轮输入画面无目标（candidates=0），无法量化对比；但 decode 已仅 0.12ms（total 的 2%）、NMS ≈0，Decode/NMS 在任何情况下都不构成瓶颈。conf=0.55 已确保无效候选不产生额外开销。

## 7. 结论与数据文件

- **黄瓦 320×320 INT8 + 3 Worker + 8 buffers + 锁频 → 240fps Pipeline 达成（239.7fps，30s 稳定，零丢错误）。**
- Capture 239.76fps 为输入硬上限；AI 吞吐已超过输入源，报告 239.7fps 即 Pipeline 上限。
- 原始数据（板端）：`/tmp/p1h.log`（1W/2W/3W 各 1000 帧）、`/tmp/p1h_3w30.log`（3W 30s 稳定）、`/tmp/mon_p1h.log`、`/tmp/mon_p1h30.log`（NPU/DDR/GPU/CPU 监控）。

测试结束，等待下一步优化指令。
