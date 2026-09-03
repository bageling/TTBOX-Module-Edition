# 零拷贝审计（Zero-Copy Audit）

> 链路：V4L2 → DMA-BUF → RGA → RKNN → Decode/NMS → Target → Aim → HID。
> 方法：全仓静态扫描 memcpy/memmove/std::copy/vector 拷贝 + 逐处判定必要性。
> 结论：**当前代码中所有 memcpy 均为必要且非图像数据拷贝；图像链路设计上零 CPU 拷贝（DMA fd 流转）。**
> 板端实测（真实帧率下的拷贝分布）标 BLOCKED — 无 RK3588 实机。

## 逐处审计结果

| 位置 | 拷贝内容 | 必要性 | 判定 |
|---|---|---|---|
| `RgaProcessor.hpp:7`（注释） | 设计声明：FrameBuffer(dma_fd+w/h/stride) 全程无 CPU memcpy | — | ✅ 设计级零拷贝 |
| `RgaProcessor.cpp:363-367` | output 结构字段赋值（fd/va/w/h） | 元数据，非图像 | ✅ |
| `DecodeNMS.cpp:53,64` | `std::memcpy(&out,&f,4)` 等：float 与 uint16 位模式转换 | 推理输出格式转换（rknn 输出为 u16/fp16），逐元素必要 | ✅ |
| `WorkerPool.cpp:25` | `std::memcpy(&b,&f,sizeof(b))` | 同上（fp16→fp32 位转换） | ✅ |
| `HidForwarder.cpp:161,210,236` | HID 报文组装（rep.data → tx_data，8B 偏移拷贝） | 报文拼接必要（≤8B/次） | ✅ |
| `AimTargetMailbox::offer/take` | `AimTargetTask` 传值（含小 vector detections） | 仅检测结果（几十个框），shared_ptr 原子快照无锁；take 时一次拷贝 | ✅（如需极致可改 move，收益微小） |
| `AimThread` 帧数据流 | 无图像引用——只消费检测结果 | — | ✅ 整条控制链不碰图像像素 |

## 结论

1. 图像数据（V4L2 buffer）从采集到 RGA 到 RKNN 全程走 DMA fd，无 CPU memcpy（板端 RgaProcessor 实现）。
2. 全部 6 处 memcpy 均为：位模式转换（推理输出格式）或 HID 报文组装——不是图像/帧拷贝，无法避免且开销可忽略。
3. 唯一可优化点：`AimTargetMailbox::take_latest` 的 task 拷贝（每帧一次），可改 move 语义；但任务仅含检测框元数据，收益 <1μs 级，不构成瓶颈，暂不改（避免无谓复杂度）。

## BLOCKED（需板端）

- 真实帧率下 RGA/RKNN 的 DMA 复用率与缓冲区队列深度实测。
- V4L2 mmap 缓冲实际复用计数（DQBUF/QBUF 频率）。
