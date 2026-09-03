# TTBOX 领域模型

## 领域模型概述

TTBOX 领域模型定义了系统中所有核心概念和它们之间的关系。

所有 UI、API、Core 都围绕这个领域模型设计。

## 核心领域对象

```
┌─────────────────────────────────────────────────────────────────┐
│                         TTBOX 系统                               │
│                                                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
│  │ Capture  │  │    AI    │  │ Control  │  │    System    │   │
│  │ 采集控制  │  │  AI推理   │  │ 输出控制  │  │  系统管理    │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

## Capture（采集领域）

### CaptureConfig

```
CaptureConfig {
  device: string       // 采集设备路径，如 /dev/video0
  buffers: uint32      // 采集缓冲区数量，默认 4
  crop: CropConfig     // 截取区域配置
}

CropConfig {
  width: uint32        // 截取宽度，默认 256
  height: uint32       // 截取高度，默认 256
  offset_x: int32      // 水平偏移，相对屏幕中心
  offset_y: int32      // 垂直偏移，相对屏幕中心
}

CaptureMetrics {
  capture_fps: float   // 采集帧率
  buffer_age: float    // 帧龄（排队等待时间）
  buffer_count: uint32 // 总缓冲区数
  dequeued: uint32     // 当前占用缓冲区数
}
```

## AI（推理领域）

### ModelConfig

```
ModelConfig {
  id: string           // 模型唯一标识
  path: string         // 模型文件路径
  label: string        // 模型显示名称
  backend: string      // 推理后端，如 rknn
  concurrency: uint32  // NPU 并发数，1~3
}

ModelManifest {
  model_id: string
  label: string
  input_width: uint32
  input_height: uint32
  output_count: uint32
  class_count: uint32
  class_names: string[]
  rknn_concurrency: uint32
  status: string       // staging / installed
  origin: string       // local / cloud
}
```

### DetectionConfig

```
DetectionConfig {
  confidence: float    // 置信度阈值，默认 0.55
  iou: float           // IOU 阈值，默认 0.45
  class_filter: int[]  // 类别过滤
  max_detections: uint32 // 最大检测数
}

FovConfig {
  enabled: bool
  shape: string        // circle / rect
  radius: float
  center_x: float
  center_y: float
}

DetectionMetrics {
  fps: float           // 推理帧率
  infer_ms: float      // 推理耗时
  detect_count: uint32 // 检测目标数
  tracks: uint32       // 跟踪目标数
}
```

### TargetSelectionConfig

```
TargetSelectionConfig {
  roi: CropConfig      // 选择区域
  center_x: float      // 选择中心（归一化）
  center_y: float
  confidence: float    // 选择置信度阈值
  aim_offset: Point    // 瞄准偏移
}

TargetMetrics {
  target_frames: uint64  // 有目标帧数
  no_target_frames: uint64 // 无目标帧数
  aim_active: bool     // 瞄准激活中
  aim_error: Point     // 瞄准误差
}
```

## Control（输出控制领域）

### PidConfig

```
PidConfig {
  kp_x: float
  kp_y: float
  ki_x: float
  ki_y: float
  kd_x: float
  kd_y: float
  i_max: float
  output_deadzone: float
}
```

### MouseConfig

```
MouseConfig {
  enabled: bool
  proxy_mode: string   // full_passthrough / ai
  aim_hotkey: uint8    // 瞄准热键
  aim_hotkey2: uint8
  aim_hotkey_mode: int // 0=any, 1=all
  smooth_x: float
  smooth_y: float
  rate_x: float
  rate_y: float
}
```

### PersonalMotionConfig

```text
PersonalMotionConfig {
  enabled: bool
  curve_blend: float              // 0~1，个人曲线混合比例
  speed_blend: float              // 0~1，个人速度特征混合比例
  reaction_blend: float           // 0~1，个人反应特征混合比例
  max_reaction_delay_ms: float    // 0~1000
  knots: float[]                  // 最多32个，归一化输出曲线
}
```

### CalibrationSession

```text
CalibrationSession {
  state: idle|preparing|stabilize_x|sampling_x|analyzing_x|
        stabilize_y|sampling_y|analyzing_y|validating|applying|
        completed|cancelled|failed
  current_axis: x|y|empty
  current_iteration: int
  total_iterations: int
  amplitude_counts: int
  candidate_track_id: int
  candidate_class_id: int
  candidate_width: float
  candidate_height: float
  stable_frames: int
  stable_ms: float
  center_jitter_px: float
  size_variation: float
  valid_sample_count: int
  axis_fits: object
  failure_reason: string
}
```

### AimConfig

```
AimConfig {
  sensitivity: float
  fov_range: float
  target_switch_stable_frames: uint32
  aim_axis_x_scale: float
  aim_axis_y_scale: float
  control_max_step: int
}
```

## System（系统领域）

### SystemConfig

```
SystemConfig {
  auto_start: bool
  hostname: string
  web_port: uint16
  worker_cores: string  // 如 "1,2,4"
  capture_buffers: uint32
  cpu_min_freq_percent: uint32
}

SystemMetrics {
  cpu_percent: float
  memory: MemoryInfo
  temperature: TempInfo
  storage: StorageInfo
  uptime: float
  load_average: float[]
}

LicenseInfo {
  valid: bool
  status: string
  mode: string
  activated: bool
}

DisplayInfo {
  connected: bool
  width: uint32
  height: uint32
  refresh: float
  edid_valid: bool
  name: string
  vendor: string
}

DisplayConfig {
  device: string
  name: string
  vendor: string
  product_id: string
  serial: string
  native_mode: string
  native_only: bool
  profile: string
}
```

## Preview（预览领域）

### PreviewConfig

```
PreviewConfig {
  fps: int
  out_width: uint32
  out_height: uint32
  jpeg_quality: int
}

PreviewMetrics {
  fps: float
  encode_ms: float
  width: uint32
  height: uint32
  bytes: uint32
}
```

## Profiles（预设领域）

### ProfileConfig

```
ProfileConfig {
  name: string
  config: json  // 完整配置快照
}
```

## Network（网络领域）

### WifiConfig

```
WifiConfig {
  available: bool
  connected: WifiConnection
  mode: string       // client / ap
  networks: WifiNetwork[]
  ap: ApConfig
}

WifiConnection {
  ssid: string
  signal: int
  ip4: string
}

WifiNetwork {
  ssid: string
  signal: int
  security: string
  active: bool
}
```

## 领域关系

```
TTBOX System
  ├── Capture → HDMI Input
  │     └── CropConfig → AI 截取区域
  ├── AI
  │     ├── ModelConfig → RKNNEngine
  │     ├── DetectionConfig → DecodeNMS
  │     ├── FovConfig → TargetSelector
  │     └── Tracking → AimThread
  ├── Control
  │     ├── PidConfig → Pid1Controller
  │     ├── MouseConfig → OutputBackend
  │     └── AimConfig → AimThread
  ├── Preview → LatestFrame
  ├── Profiles → ConfigManager
  ├── Network → WifiManager
  └── System → ConfigManager
```

## 设计原则

1. **领域驱动**：所有配置以领域模型为中心，不直接映射历史产品字段
2. **单一真源**：每个配置项只有一处定义
3. **类型安全**：C++ 结构体提供类型校验
4. **可配置**：所有行为参数可通过 Web 配置
5. **可观测**：所有关键指标可通过 Metrics 查询