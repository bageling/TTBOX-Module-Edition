# 模块 03：AI 模型层（Detector / RKNN / NPU）

## 它是什么？

AI 模型层负责把图像"看懂"。它是整个盒子的大脑。
实际干活的是 RK3588 芯片里专门算 AI 的 NPU（神经网络处理器）。

## 它干什么？

1. **加载模型**：把 .rknn 模型文件加载进 NPU
2. **推理**：把预处理好的图像送进 NPU，NPU 算出"画面里有什么"
3. **多核并行**：RK3588 有 3 个 NPU 核，可以让 3 个 Worker 同时推理

## 什么是 RKNN？

RKNN 是瑞芯微（Rockchip）公司专门为自家芯片做的 AI 模型格式。
`.rknn` 文件就是模型，由 ONNX 等格式转换而来，只能跑在 RK 芯片上。

## 什么是 NPU？

NPU = Neural Processing Unit，专门负责人工智能计算的处理器。
普通 CPU 算图像慢，NPU 算图像飞快（一帧只要几毫秒到几十毫秒）。

## 输入是什么？

- `PreprocessedFrame`（模块 02 输出的模型输入尺寸图像）

## 输出是什么？

- RKNN 原始输出张量（一堆数字，人看不懂，需要模块 04 解码）

## 谁调用它？

- `WorkerPool`（Worker 线程池，每帧取最新画面 → 本层推理）
- 3 个 Worker 绑定 3 个 NPU 核（core_mask 1/2/4）

## 它不能干什么？

- 不能直接输出"人看得懂的检测框"（数字要解码）
- 不能选择目标、不能移动鼠标

## 修改它会影响什么？

- 影响推理速度（模型大小、输入尺寸）
- 影响能识别什么（模型类别）
- 影响 NPU 利用率

## 关键文件

| 文件 | 作用 |
|---|---|
| `core/src/rknn/RKNNEngine.cpp/.hpp` | RKNN 引擎封装（加载/推理/获取输出） |
| `core/src/rknn/WorkerPool.cpp/.hpp` | 多 Worker 并发推理池 |
| `core/src/rknn/Detector.cpp/.hpp` | 推理+解码的统一边界 |
| `core/src/detector/highperf/HighPerfRknnEngine.cpp/.hpp` | 高性能零拷贝路径 |
