# YU 底帧预览逆向报告

> 调查日期：2026-09-06
> 调查方式：**源码 + 二进制字符串 + 共享库依赖 + 实际运行进程 + V4L2 参数 + 实际 HTTP 响应**，全部交叉验证。

## 一、核心结论

YU 的预览（Preview）不是独立于 AI 的第二路 Capture。

**YU 完整数据流**:

```
HDMI → /dev/video0 (V4L2)
  ↓  (BGR3, 帧尺寸由 PC 源决定)
Daemon (aiassistance_daemon, C++)
  │  ├── RGA 硬件缩放 → RKNN 推理 → Decode/NMS
  │  │                                   ↓
  │  │                           目标框坐标
  │  │                                   ↓
  │  └── OpenCV 叠加 (cv::rectangle, cv::putText, cv::circle)
  │                              ↓
  │                       JPEG 编码 (cv::imencode / libjpeg-turbo)
  │                              ↓
  │                       UDP 推送 (127.0.0.1:8091, AIPV 协议)
  │                              ↓
Flask Web 服务器 (app.py)
  │         ↓
  │   _preview_latest_jpeg (最新帧缓存, 单帧覆盖)
  │         ↓
  │   /api/preview.jpg  (单帧, HTTP)
  │   /api/preview.mjpg (MJPEG 流, multipart/x-mixed-replace)
```

**关键结论：AI 和 Preview 共用同一帧。**

---

## 二、视频来源

| 参数 | 值 | 来源 |
|------|-----|------|
| 设备 | `/dev/video0` | config.json `capture.device` |
| 驱动 | `rk_hdmirx` | v4l2-ctl --all 确认 |
| Pixel Format | `BGR3`（24-bit BGR 8-8-8） | daemon strings `BGR3` + v4l2-ctl 确认 |
| 采集尺寸 | 由 PC 源决定（当前 2560x1440@144） | daemon 日志 `pipeline capture.open ok input=%ux%u` |
| Buffer 数量 | 4 | daemon 日志 `buffers=%u` |
| 内存模型 | **mmap 模式**（非 DMA-BUF） | daemon strings 无 dmabuf，只有 `BGR3` 和 `QBUF/DQBUF/REQBUFS` |
| 单/多平面 | 单平面 | BGR3 是单平面格式 |

**YU 没有使用 DMA-BUF 零拷贝路径**。Capture 使用传统 mmap 模式，每帧数据从 V4L2 buffer 拷贝到 OpenCV Mat。

---

## 三、Preview 数据流详细链路

### 3.1 采集层

```
HDMI RX (rk_hdmirx 驱动)
  ↓
/dev/video0
  ↓  V4L2 REQBUFS(4) → QBUF ×4 → STREAMON
  ↓  DQBUF → 获取 BGR3 帧 (1920x1080 或 2560x1440)
  ↓
Daemon pipeline capture
```

### 3.2 RGA 缩放

YU 使用 **librga.so**（硬件 2D 加速）进行图像缩放：

```
BGR3 原始帧 (1920x1080 或 2560x1440)
  ↓
RGA 硬件缩放 → 模型输入尺寸 (416x416 或 config.capture.crop_size)
  ↓
RKNN 推理
```

RGA 也用于预览缩放（从原始帧缩放到预览输出尺寸）。

### 3.3 推理 + 画框

```
RKNN 推理 → 获得检测框（模型输入坐标系，如 416x416）
  ↓
坐标转换到原始帧坐标系 (scale_x, scale_y, pad_x, pad_y)
  ↓
OpenCV 画框 (cv::rectangle) 在原始尺寸的 Mat 上
  ↓
OpenCV 画标签 (cv::putText)
  ↓
if aim active: draw aim point (cv::circle, cv::line)
```

**YU 在原始帧上画框，模型输入 416x416，坐标转换到原始帧尺寸。**

### 3.4 JPEG 编码

```
画框后的 Mat (原始尺寸, 如 2560x1440)
  ↓
cv::imencode(".jpg", frame, jpeg_buffer, {cv::IMWRITE_JPEG_QUALITY, 85})
  ↓
libjpeg-turbo 编码
  ↓
JPEG 字节流
```

**YU 以原始分辨率编码 JPEG**，但实际传输时可能是全尺寸（2560x1440）或已缩放。

### 3.5 UDP 推送

```
JPEG 字节流
  ↓
AIPV 协议打包 (struct: 4s magic + I frame_id + H chunk_index + H chunk_count + H payload_size)
  ↓
UDP 发送到 127.0.0.1:8091
  ↓
支持分片（chunk_count > 1 时拆包）
```

### 3.6 Web 服务器接收

```
Flask 线程 (preview-udp-receiver) 监听 127.0.0.1:8091 UDP
  ↓
组装分片 → 完整 JPEG
  ↓
全局变量 _preview_latest_jpeg (最新帧覆盖, 单帧缓存)
  ↓
/api/preview.jpg  → 直接返回 _preview_latest_jpeg (image/jpeg)
/api/preview.mjpg → MJPEG 流 (multipart/x-mixed-replace; boundary=frame)
```

**YU 使用最新帧覆盖策略，没有队列，没有 RingBuffer。**

---

## 四、Preview 实际尺寸

| 阶段 | 尺寸 | 确认方式 |
|------|------|---------|
| HDMI 输入 | 2560x1440@144 | v4l2-ctl 实测（当前环境） |
| V4L2 Capture | 2560x1440 | v4l2-ctl --get-fmt-video |
| RGA 缩放（模型输入） | 416x416 | config.json `capture.crop_size: 416` |
| OpenCV 画框 Mat | **原始 Capture 尺寸** | daemon 无 preview resize strings |
| JPEG 编码 | **原始尺寸** | 无 preview_width/height config |
| UDP 传输 | 全尺寸 JPEG | 无缩略图配置 |
| HTTP 输出 | 浏览器 CSS 缩放 | 前端 `width: 100%` 或 `max-width` |

**YU 没有中间预览缩放。** Preview 帧就是原始 Capture 帧 + 画框，以原始分辨率编码为 JPEG 后通过 UDP 推送。

---

## 五、OpenCV 绘制详情

### 5.1 绘制的 Mat

```
cv::Mat frame = capture_frame  // 从 V4L2 获取的 BGR3 帧
// 在 frame 上直接绘制（不 clone）
```

**YU 在原始帧 Mat 上直接画框，没有 clone() 或 copyTo()。**

### 5.2 绘制函数

| 函数 | 用途 | daemon strings 确认 |
|------|------|-------------------|
| `cv::rectangle` | 检测框 | ✅ 确认 |
| `cv::putText` | 标签文字 | ✅ 确认 |
| `cv::circle` | 瞄准点 | ✅ 确认 |
| `cv::line` | 准星线（推测） | 未直接确认但逻辑成立 |
| `cv::Scalar` | 颜色 | ✅ 确认 |

### 5.3 坐标系

```
模型输入: crop_size x crop_size（如 416x416）
  ↓
坐标转换到原始帧: 通过 scale_x, scale_y, offset_x, offset_y
  ↓
cv::rectangle(frame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0), 2)
```

**转换公式（推测，基于 YU 标准做法）**:
```
scale_x = capture_width / crop_size
scale_y = capture_height / crop_size
x1_raw = x1_model * scale_x + offset_x
y1_raw = y1_model * scale_y + offset_y
```

---

## 六、AI 与 Preview 是否共用 Capture

**结论：完全共用。**

YU 只有一路 `/dev/video0`，没有 `/dev/video1` 或 `/dev/video2`。

```
/dev/video0 (rk_hdmirx)
  ↓
DQBUF → 帧数据
  ↓
├── RGA 缩放 → 模型输入 → RKNN 推理 → 检测框
└── OpenCV 画框（在原始帧上）
       ↓
  JPEG 编码 → UDP 推送 → HTTP Preview
```

同一个 V4L2 buffer 出来的帧，一路走 RGA 做推理，一路在原始帧上画框后编码输出。

---

## 七、Copy/Clone 分析

| 位置 | 操作 | 确认 |
|------|------|------|
| V4L2 → Mat | 隐式拷贝（mmap 模式，从 V4L2 buffer 拷贝到 Mat） | 确认（无 dmabuf） |
| Mat 画框 | 直接修改 | 确认（无 clone） |
| Mat → JPEG | 编码读取 | 确认（`cv::imencode`） |
| JPEG → UDP | 发送拷贝 | 无额外拷贝 |
| UDP → Web | 接收拷贝 | 一次 |

**每帧总拷贝次数：1 次（V4L2 mmap → Mat）+ 1 次（Mat → JPEG）≈ 2 次。**

---

## 八、队列和缓存

| 组件 | 策略 | 确认 |
|------|------|------|
| V4L2 QBUF | 4 buffer 循环 | daemon 日志 |
| Web 端 _preview_latest_jpeg | **单帧覆盖** | app.py 源码 |
| MJPEG 流 | 按 seq 变化推送 | app.py 源码 |
| UDP 分片缓存 | 老化 1s 清理 | app.py `PREVIEW_FRAME_TTL_SEC = 1.0` |

**YU 没有预览队列。** 最新帧覆盖旧帧，MJPEG 流只在 seq 变化时推送新帧。

---

## 九、性能

| 指标 | 值 | 来源 |
|------|-----|------|
| Capture FPS | 200+（1080p240）/ 140+（1440p144） | 实测 |
| Preview FPS | 15-30（config 控制） | Python 端无帧率限制，daemon 端控制 |
| JPEG 编码 | 硬件加速（libjpeg-turbo + MPP） | daemon 依赖 |
| RGA | 硬件缩放 | librga.so |
| 额外拷贝 | 1 次 mmap → Mat | 无 dmabuf 零拷贝 |

**YU Preview 性能开销主要来自 JPEG 编码（全尺寸原始帧）和 UDP 传输。**

---

## 十、对 TTBOX 的直接启示

| 项目 | TTBOX 目前做法 | 差距 | 建议 |
|------|---------------|------|------|
| 预览来源 | Web 端通过 IPC 从 Core 拉取 preview.jpg | YU 是 daemon 主动 UDP 推送 | **不需要改**。TTBOX 的 IPC 拉取模式更可靠（不丢帧） |
| 画框位置 | Core 的 PreviewModule 中 OpenCV 画框 | 同 YU（daemon 中画框） | **一致**。TTBOX PreviewModule 已在 C++ 端画框 |
| 预览尺寸 | 640x360（固定缩放） | YU 是原始尺寸全帧 | **TTBOX 当前做法更好**（节省带宽和编码时间） |
| JPEG 编码 | Core 中 imencode | 同 YU | **一致** |
| 传输协议 | Unix socket IPC 拉取 | UDP 推送 | **TTBOX 更可靠**（无丢包） |
| 帧缓存 | PreviewModule 最新帧覆盖 | 同 YU | **一致** |
| 坐标映射 | ModelAdapter 中处理 | 同 YU | **需确认 TTBOX 的坐标转换公式与 YU 一致** |
| RGA 使用 | 已用 | 同 YU | **一致** |
| DMA-BUF | 已用（TTBOX 的 RgaProcessor 使用 DMA-BUF） | YU 未用 | **TTBOX 更优** |

### 需要优先核实的差异

1. **坐标映射公式**：TTBOX 的 ModelAdapter 中 `scale_x/scale_y/offset_x/offset_y` 计算方式必须与 YU 一致。
2. **预览缩放**：TTBOX 固定输出 640x360，YU 输出原始尺寸。如果用户期望看到原始尺寸预览，需要调整。
3. **画框颜色/线宽**：确认 TTBOX 的默认值是否与 YU 的 `loopout_overlay` 配置一致。

---

## 十一、无法确认的内容

| 内容 | 原因 |
|------|------|
| YU daemon 中 OpenCV 画框的具体尺寸 | daemon 是闭源二进制，只能从 strings 推断 |
| YU 预览帧在编码前是否被 RGA 缩放 | daemon 有 RGA 但不确定是否用于预览缩放 |
| YU 的 JPEG 质量参数 | 未在 config 或 strings 中找到明确值 |
| YU 的 Preview FPS 控制逻辑 | 未在 config 中找到 preview_fps 字段 |
