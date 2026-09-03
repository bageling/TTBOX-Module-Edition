# RK3588 2K@144 + YOLO 320/640 性能审计报告

> 只读审计：未修改源码/模型/配置/内核/驱动/systemd/affinity。全部结果实测；无法测试标注 UNTESTED。
> 环境：OrangePi 5 Plus / DietPi Debian 12 / kernel 6.1.99-rockchip-rk3588 / RKNN 2.3.2 (librknnrt /usr/lib/librknnrt.so, driver v0.9.8) / RKNNLite 2.3.2 / numpy 2.4.6 / evdev 1.6.1

---

## 一、系统/硬件

| 项 | 实测值 |
|---|---|
| 板卡 | RK3588 OPi 5 Plus |
| RAM | 8 GB (MemTotal 8124444 kB) |
| Kernel | 6.1.99-rockchip-rk3588 |
| DTB | /boot/dtb-6.1.99-rockchip-rk3588 |
| CPU | 8 核：cpu0-3 A55 max 1800MHz + cpu4-7 A76 max 2352MHz，governor 全部 schedutil |
| 频率(空闲) | 小核 1200MHz / 大核 1200~1416MHz（负载时大核 2352MHz） |
| NPU | driver v0.9.8；`/dev/rknpu*` 不存在（NPU 走其他节点）；`/dev/dri/card0+renderD128, card1+renderD129` |
| librknnrt | /usr/lib/librknnrt.so (2.3.2) |
| rknnlite | 2.3.2 (/usr/local/lib/python3.11/dist-packages/rknnlite) |
| numpy / evdev | 2.4.6 / 1.6.1 |
| RGA | `/dev/rga` 存在；**librga 用户态库缺失**（不可用） |
| AI 进程 | 审计时无常驻 AI 进程（测试残留已清理） |

## 二、现有 AI/YOLO（从源码/配置/模型读取）

| 项 | 值（实测读取） |
|---|---|
| YOLO 版本 | yolo261n（自定义命名，非 v8/v11 官方版本）|
| 模型文件 | /opt/ttbox2/models/yolo261n-rk3588.rknn，7.45 MB，格式 RKNN v6（magic `RKNN`, version 6）|
| 输入尺寸 | **640×640**（config `model_input_width/height=640`；模型文件字符串 `shape: [1,3,640,640]`）|
| **320 模型** | **不存在**（models/ 仅 1 个文件；禁止冒充，320 相关全部 UNTESTED）|
| Layout | 原始 ONNX **NCHW**；runtime 要求 **NHWC** 输入（日志：`need NHWC data format`）|
| 量化 | **UINT8 输入量化**（实测 uint8 直接接受）+ **FP16 输出**（文件字符串 `output0.dtype: float16`，runtime 转 float32）|
| input dtype | uint8 |
| 输出 | 1 个 tensor `(1,84,8400)` float32 |
| 类别数 | 80（84 = 4 xywh + 80 cls；config 无类别名 → classes=[]）|
| Decode | 单输出 xywh 直解（decode.py `decode_outputs` 单输出分支）|
| DFL | 无（单输出路径不用 dfl；dfl 函数仅多输出路径存在）|
| Sigmoid | **无**（cls 为 logit 直解，与 legacy 一致）|
| NMS | **CPU 执行**（numpy classwise NMS，非 E2E NMS）|
| core_mask / async | core_mask=0 (auto)，async_mode=False |
| 多 context | 单实例 |

## 三、HDMI 2K144

【实测】当前 HDMI 输入：
- 设备 `/dev/video0`，驱动 `rk_hdmirx`，MPLANE
- **当前格式：1920×1080 BGR3**（pixelformat=0x33524742, bpl=5760, sizeimage=6220800）
- 连续采集 **1000 帧**：实际 FPS=**59.63**、timeout=0、error=0、buffer error=0、**sequence 连续 0→999（drop=0）**、timestamp 间隔 mean/p50/p95=16.67ms（恒定）
- **2K@144 输入不可达**：hdmirx 为被动接收，输入格式由 HDMI 信号源决定；当前信号源只提供 1080p60。板端无法主动要求 2K144，也未接入 144Hz 信号源。
- **结论：HDMI 2K144 = FAIL（输入侧无法达到）**；在当前 1080p60 输入下驱动层稳定 59.6fps 无丢帧，HDMI 采集本身 PASS

## 四、纯采集性能

【实测】
- DQBUF 层：60fps 无瓶颈（1000 帧无 drop/timeout）
- DMA buffer 直读（全部像素 numpy sum）：**25.67ms**
- DMA buffer 直读（native C resize 直读 mmap）：**84.19ms**
- memcpy 到普通 RAM（np.array copy）：**6.17ms**
- 普通 RAM memcpy（对照）：~0ms
- **V4L2/DQBUF 不是瓶颈；DMA 内存 CPU 直读是瓶颈**（读 6MB dma 内存 25-84ms vs 普通内存 <1ms）

## 五、320 全链路

**UNTESTED** —— 320×320 模型不存在（禁止用 640 冒充）。

## 六、640 全链路（30.1s, 445 帧, 实测）

| 阶段 | mean | p50 | p95 | p99 | max |
|---|---|---|---|---|---|
| capture | 0.01 | 0.01 | 0.02 | 0.02 | 0.22 |
| copy+resize | 22.33 | 22.16 | 24.89 | 26.41 | 32.11 |
| infer | 60.95 | 60.94 | 65.85 | 69.78 | 72.07 |
| decode+NMS | 6.56 | 6.75 | 7.70 | 8.30 | 9.28 |
| **e2e** | **67.53** | **67.79** | **72.77** | **76.33** | **78.63** |

- capture_fps(est)=44.8、pipeline_fps=**14.8**、detect_fps=0（画面无模型目标）、dropped=452、avg_detect=0
- 对照运行（真实 pipeline state）：fps 13.0、infer 57.22、resize 26.74、e2e 65.24

## 七、NPU 三核（mask 矩阵，复用 5.2 同环境实测 + auto 确认）

| mask | init | infer p50 | 说明 |
|---|---|---|---|
| 1/2/4（单核）| ✅ | 74.0~75.2 | core 对应核 ~37% |
| 3（双核 c0+c1）| ✅ | 72.5 | 无收益 |
| 5/6（c0+c2 / c1+c2）| **❌ INIT_FAIL** | - | 不支持 |
| 7（三核）| ✅ | 72.4 | **无收益**（NPU 仅 core0 38%）|
| auto | ✅ | 73.4 | 单核 core0 |

- **结论：单核=双核=三核（实测无差异），最佳 = auto（单核 core0）。NPU 三核无效。**

## 八、CPU 调度（实测记录，未改 affinity）

- 系统总 CPU ~15%；推理线程所在大核（cpu7）峰值 **91%**；capture/preprocess 线程占用小核（cpu0-3，低负载）
- 大核在推理时 2352MHz 满频，小核 1200-1800MHz；无 throttling（温度 ≤59°C）
- 线程级分布采样脚本因进程识别问题未取得逐线程表（补充项），以每核使用率分布为准（如实记录）

## 九、预处理瓶颈拆解（实测）

| 环节 | 耗时 |
|---|---|
| V4L2 DMA buffer 直读 | 25.67ms（numpy 全读）~ 84.19ms（C 直读 resize）|
| memcpy → 普通 RAM | 6.17ms |
| CPU resize（C native，普通内存）| 12.12ms |
| RGA | 不可用（无 librga）|
| RKNN input copy（set_inputs，5.10 实测）| **33ms** |

**耗时排序：RKNN input copy (33ms) > DMA 直读 (25-84ms) > native resize (12ms) > memcpy (6ms)**
当前 pipeline 已用 copy 规避 DMA 直读（copy 6ms + resize 12ms ≈ 22ms 实测），DMA 直读问题已解决；**当前最大预处理头是 RKNN input copy（Runtime 内部）**。

## 十、最新帧策略

- 【静态分析】当前为 **latest-frame / drop-old**：capture 线程缓存最新帧，read() 取最新，旧帧被覆盖（无队列、无阻塞、无 ring-buffer）
- 【实测】capture 供帧 44.8fps（22ms 间隔），AI 消费 14.8fps → 30s 内 dropped=452（旧帧被最新帧替换）
- **144Hz 输入 + AI 低 FPS 时只处理最新帧，不会处理旧帧、无延迟累积**（frame age 上限 = 帧产生间隔 ≈ 22ms）

## 十一、E2E

- 各阶段实测：capture 0.01 → copy+resize 22.33 → infer 60.95 → decode+NMS 6.56 → aim 0.57（真实 pipeline state）
- e2e：**P50 67.79 / P95 72.77 / P99 76.33 / MAX 78.63**
- Frame Age：估算 **0~22ms**（capture 帧间隔，p50≈11ms）——最新帧语义下 age 受 capture 帧率限制

## 十二、最终瓶颈判断（按实测数据，非经验）

| 排序 | 环节 | 实测耗时 | 说明 |
|---|---|---|---|
| P1 | **RKNN input copy** (set_inputs) | 33ms | Runtime 内部复制+量化转换，Python 层不可控 |
| P2 | **NPU 推理** (rknn_call) | 32.9ms | 模型计算；NPU 单核 37% 利用率 |
| P3 | copy+resize | 22ms | memcpy 6ms + native resize 12ms + 竞争 |
| P4 | decode+NMS | 6.6ms | numpy class 扫描主开销 |
| P5 | DMA read | 已规避（copy 路径）| 直读 25-84ms 已用 copy 解决 |
| - | V4L2/DQBUF | ~0 | 60fps 无瓶颈 |
| - | aim | 0.57ms | 无目标时 |
| - | HID/IPC | 未测（无 HID 场景）| UNTESTED |

## 十三、最终对比

| 指标 | 320 | 640 |
|---|---|---:|
| Preprocess (copy+resize) | UNTESTED | 22.33ms |
| Infer P50 | UNTESTED | 60.94ms |
| Infer P95 | UNTESTED | 65.85ms |
| Decode+NMS | UNTESTED | 6.75ms |
| E2E P50 | UNTESTED | 67.79ms |
| E2E P95 | UNTESTED | 72.77ms |
| Pipeline FPS | UNTESTED | 14.8 |
| Frame Drop (30s) | UNTESTED | 452 |
| Frame Age | UNTESTED | ~0-22ms |
| CPU | UNTESTED | ~15% |
| NPU | UNTESTED | core0 37-40% |

- HDMI 实际 FPS = **59.63**（1080p60 输入；2K144 不可达）
- 320 AI FPS = **UNTESTED**（无模型）
- 640 AI FPS = **14.8**
- 144/14.8 ≈ **9.7 倍**（在假想 144Hz 输入下的性能差距；实际输入 60fps，AI 无法逐帧）

**回答**：
- HDMI 能否 144FPS？→ **当前不能**（信号源 1080p60；驱动层 60fps 稳定无丢帧；2K144 输入未接入，无法验证）
- 320 能否实时？→ **无法测试**（模型不存在，禁止冒充）
- 640 能否实时？→ **不能**（14.8fps < 30fps 实时线）
- 每帧 AI 是否可能？→ **当前不能**（AI 68ms > 60fps 帧间隔 16.7ms）
- NPU 三核是否有效？→ **无效**（mask=7 与单核无差异，mask=5/6 init 失败）
- 当前最大瓶颈？→ **RKNN input copy（33ms）+ NPU infer（33ms）**，两者占 e2e ~88%
- 最值得优化 3 项？→ ① 减小/消除 RKNN set_inputs 复制转换（33ms，需 Runtime 支持/输入格式匹配）；② 更小输入模型（320/416，需 rknn-toolkit2 导出，当前阻塞）；③ DMA 直读规避已生效（copy 路径），进一步可评估 RGA（需补 librga）

## 十四、结论

- **当前 640 模型 + 1080p60 输入下的实测基线：AI FPS 14.8、E2E P50 67.8ms、P99 76.3ms**
- **2K@144 输入在本环境不可达**（信号源限制），HDMI 驱动层 60fps 稳定
- **320 全链路、320 模型信息全部 UNTESTED**（模型不存在，未伪造）
- NPU 三核/双核/多实例/async/affinity 均实测无收益，最优配置维持 auto 单核
- 最大瓶颈 = RKNN input copy + NPU infer（各 ~33ms，占 e2e 88%）
- 最值得优化：① set_inputs 复制（Runtime 层）② 更小模型（需导出工具）③ 输入路径（RGA/格式优化，需 librga）
