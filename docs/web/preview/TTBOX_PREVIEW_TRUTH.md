# TTBOX Preview 真相记录

## 验收时间

2026-09-06，板端 `192.168.0.53`，当前 HDMI 输入 `2560×1440p144`。

## 最终 Preview 设计

Preview 不是全屏 Capture，也不是 AI 模型输入图像，而是原始 Capture 帧中心可调裁剪区域。默认尺寸为 `640×640`，实际尺寸由现有 `capture.crop_size` 配置控制。

```text
Capture 2560×1440
  ├── AI 独立 RGA/RKNN 链路
  └── Preview 中心 Crop(crop_size, crop_size)
        默认 Crop = 640×640
        默认 Origin = (960,400)
        ↓
      OpenCV 画框
        ↓
      JPEG = 当前 Crop 尺寸
        ↓
      HTTP/MJPEG
```

`capture.crop_size` 修改后，Preview 在运行时读取新的 `runtime_profile.capture.width/height`，中心 Origin 和 JPEG 尺寸随之变化。

本阶段没有修改 Web 前端、EDID、TargetSelector、PID、HID 或 AI 推理坐标链。

## 源码实现

### Preview 文件

- `core/src/preview/PreviewModule.hpp`
- `core/src/preview/PreviewModule.cpp`
- `core/src/runtime/CoreRuntime.cpp`

### 固定裁剪

`PreviewModule::kPreviewCropSize = 640`。

`PreviewModule::encode_frame()` 使用 Capture 原始帧的 `width/height/stride/cpu_va`，计算：

```text
origin_x = (frame_width - 640) / 2
origin_y = (frame_height - 640) / 2
```

然后逐行复制：

```text
source + (origin_y + y) * frame_stride + origin_x * 3
```

到 Preview 专用 `640×640` BGR 缓冲。

### 当前 2K 输入

```text
Capture = 2560×1440
Crop = 640×640
Origin = (960,400)
Crop stride = 1920 bytes
```

### 1K 输入预期

当真实 Capture 为 `1920×1080` 时，同一公式得到：

```text
Capture = 1920×1080
Crop = 640×640
Origin = (640,220)
```

本次未切换到 1K 信号，因此 1K 项目仍待真机信号验证。

## 坐标系

DecodeNMS 输出的是原始 Frame 坐标。Preview 画框时执行：

```text
x_crop = x_frame - origin_x
y_crop = y_frame - origin_y
```

OpenCV `Mat` 尺寸是 `640×640`，`cv::rectangle()` 接收的 Rect 是 Crop 坐标，不再执行 Preview 缩放。

AI 模型的 `256×256` 输入仅属于 AI RGA/RKNN 链路，不决定 Preview 尺寸。

## JPEG 和缓存

1. `LatestFrame::get()` 获取最新 Capture 帧，不建立 Preview 帧队列。
2. Preview 做一次 CPU copy，只保留中心 `640×640`。
3. 在 Preview Crop Mat 上绘制最新检测框。
4. JPEG 编码固定使用 `640×640`。
5. `jpeg_` 通过互斥锁单缓冲覆盖旧 JPEG。
6. Preview 停止时清空 `jpeg_`、字节数和尺寸指标。

## 真机证据

### Core 日志

```text
V4L2Capture open 完成: 4 buffers
STREAMON OK
模型信息: 输入 256x256 INT8 NHWC
Preview 已启动: 640x640 center crop @15fps +draw_detections
```

### JPEG header

2026-09-06 11:16 CST，2K HDMI 信号下读取 JPEG SOF header：

| Endpoint | HTTP | JPEG 宽 | JPEG 高 | JPEG 字节数 |
|---|---:|---:|---:|---:|
| `127.0.0.1:8000/api/preview.jpg` | 200 | 640 | 640 | 34103 |
| `127.0.0.1:8001/api/preview.jpg` | 200 | 640 | 640 | 34203 |

SOF marker：`0xC0`。

### Preview 帧率

连续 5 次读取 8001 状态，帧序号如下：

```text
714, 730, 745, 761, 776
```

相邻约 1 秒帧增量为 `15/16`，与 `15 FPS` 限帧一致。

注意：8001 插件 status 当前 `width/height` 字段仍为 `0`，这是插件没有从 Core IPC 响应复制 JPEG 尺寸元数据；实际 JPEG header 已确认是 `640×640`。

## 服务状态

2026-09-06 验收结束：

```text
ttbox-core active
ttbox-preview active
ttbox-web active
```

## 模型解耦

当前真实运行模型：

```text
model = jwdl_sjzv11
input = 256×256 INT8 NHWC
```

Preview 固定中心 `640×640`，不读取模型输入宽高，也不使用 AI RGA 输出。

不同模型输入尺寸的真实切换验证尚未完成；当前板端没有第二个已确认可运行的不同输入尺寸模型，记为 `BLOCKED`，未伪造结果。

## 当前结论

```text
2K Capture 2560×1440 ........ PASS
中心 Crop 640×640 .......... PASS
Origin 960,400 .............. PASS
JPEG 640×640 ................ PASS
Preview 15 FPS .............. PASS
Preview 与 AI Crop 分离 ..... PASS
Preview 最新帧覆盖 .......... PASS
Preview 停止清缓存 .......... PASS
1K 1920×1080 真机 ........... BLOCKED
第二模型输入尺寸 ............ BLOCKED
```

最终确认：当前 TTBOX Preview 是原始 Capture 帧中心的 `640×640` 区域，不是全屏，不是 `640×360`，也不是模型输入尺寸。
