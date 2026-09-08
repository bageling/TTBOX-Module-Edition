# 模块化目录（modules/）

## 这是什么？

`modules/` 是 **TTBOX-模块版** 的"语义视图"：
用数字编号 + 中文名称，把整个系统按"数据流顺序"切成 9 个模块，
让任何人（包括小白）都能一眼看懂系统是怎么一层层工作的。

## 重要说明：modules/ 是视图，真实源码在 core/src/

为了**不破坏已经验证成功的代码**，真实源码**没有物理搬移**，
仍然在 `core/src/` 下的原始位置（这是 CMake 编译实际引用的地方）。

`modules/` 里每个 README 都写清楚了：
- 这个模块是什么、干什么
- 输入/输出
- 谁调用它
- **对应的真实源码文件在哪**（核心源码映射）

## 9 个模块一览

```
画面进来（HDMI）
   ↓
[01-capture]   采集层   从 /dev/video0 抓画面帧
   ↓
[02-image]     图像层   RGA 裁剪/缩放，喂给 AI
   ↓
[03-detector]  AI层     RKNN + NPU 推理
   ↓
[04-decoder]   解码层   DecodeNMS 把数字变成检测框
   ↓
[05-selector]  选择层   TargetSelector 挑一个目标
   ↓
[06-coordinate]坐标层   CoordinateTransform 算偏差
   ↓
[07-controller]控制层   AimThread + PID 算移动量
   ↓
[08-mouse]     输出层   HID 注入鼠标指令（当前关闭）
   ↓
[09-runtime]   运行时   总指挥：组装、配置、IPC、状态
```

## 分层金字塔（对应定位文档）

```
        功能插件层（plugins/）
         Web控制层（plugins/web + IPC）
        目标选择/坐标/控制/输出（modules 05-08）
         AI检测层（modules 03-04）
        采集与图像层（modules 01-02）
         硬件层（RK3588 + HDMI RX + NPU）
```

## 如何开始学习？

1. 先看 `docs/小白教程/01-TTBOX是什么.md` 了解整体
2. 再看 `modules/01-capture/README.md` 从第一层开始
3. 每个模块 README 末尾都有"关键文件"，打开对应源码边看边学
