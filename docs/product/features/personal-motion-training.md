# TTBOX 个人移动曲线训练

## 产品目的

让用户用真实鼠标完成一组目标点击/跟随练习，TTBOX 记录鼠标轨迹和反应时间，生成属于当前设备的移动曲线模型，并在用户主动启用后参与 AI 移动输出。

## 用户场景

用户进入 03「移动控制」→「个性曲线训练」：

1. 选择或使用默认个人档案。
2. 点击开始采集，页面进入 Pointer Lock 训练画布。
3. 完成反应训练和连续切换训练。
4. 结束采集后，样本真实落盘。
5. 点击生成模型，系统校验样本数量和质量。
6. 点击启用，个人曲线按混合参数参与 AI 移动；停用后恢复默认曲线。

## TTBOX Domain Model

```text
MotionProfile {
  id: string                 # [a-z0-9][a-z0-9._-]{0,63}
  name: string
  schema: "ttbox.motion-profile.v1"
  samples: MotionSample[]    # 持久化在 profile.json
  model: MotionModel
  statistics: MotionStatistics
}

MotionSample {
  schema: "ttbox.motion-sample.v1"
  mode: "reaction" | "continuous"
  completion: "dwell"
  canvas: {width, height}
  start: Point
  target: Point
  radius: number
  browser: {pointer_lock, raw_update, coalesced_events, user_agent}
  points: [{dt, dx, dy}]
}

MotionModel {
  schema: "ttbox.motion-model.v1"
  version: 1
  knots: number[32]          # 0~1 的路径速度/曲率归一化参数
  quality: 0~100
  ready: bool
  coverage: {reaction, continuous}
}

MotionMix {
  curve_blend: 0~1
  speed_blend: 0~1
  reaction_blend: 0~1
  max_reaction_delay_ms: 0~1000
}
```

## 持久化

- 根目录：`/opt/ttbox/config/motion-profiles/`
- 每个档案：`<id>/profile.json`
- 写入使用临时文件替换，避免半写文件。
- 当前启用档案和混合参数进入 TTBOX RuntimeProfile，不读取其他产品目录。

## API Contract

```text
GET    /api/motion-profiles
POST   /api/motion-profiles                 {name}
PATCH  /api/motion-profiles/<id>            {name}
DELETE /api/motion-profiles/<id>
GET    /api/motion-profiles/<id>/export     下载 JSON ZIP
POST   /api/motion-training/sessions        {profile_id}
PUT    /api/motion-training/sessions/<id>/heartbeat
POST   /api/motion-training/sessions/<id>/samples
DELETE /api/motion-training/sessions/<id>
POST   /api/motion-profiles/<id>/train
POST   /api/motion-profiles/<id>/activate   {curve_blend,speed_blend,reaction_blend,max_reaction_delay_ms}
DELETE /api/motion-profiles/active
DELETE /api/motion-profiles/<id>/samples
```

统一响应：`{ok: bool, data: object, error?: string}`。

## 样本校验

- 单个样本 JSON 不超过 256KB。
- points 数量 2~2048。
- canvas 宽高 64~4096。
- 坐标、dt、距离必须为有限数。
- dt 必须为 0~2000ms；总时长不超过 120000ms。
- 每个点的累计路径必须在画布范围内。
- 目标半径 1~512；最终轨迹必须进入目标圆。
- mode/completion/schema 必须匹配。
- 训练租约有效期 30 秒，心跳续租；同一设备同时只允许一个会话。

## 训练行为

- 反应样本目标数：72。
- 连续样本目标数：96。
- 至少 12 个样本且两种模式都有样本才允许生成模型。
- 模型 knots 由样本路径速度、路径效率、反应延迟归一化生成；生成过程确定性、可重复。
- quality 由样本覆盖率、路径效率和延迟稳定性计算；quality >= 60 才允许启用。

## Core 行为

- Core 读取 TTBOX RuntimeProfile 中的个人模型配置。
- 个人模型不替换目标选择、PID 或安全门；只作为输出曲线混合项。
- `personal_motion_enabled=false` 时完全走默认控制链。
- 启用模型但模型无效时，Core fail-closed，继续默认曲线并返回错误状态。
- 本阶段先落地模型格式、校验和 RuntimeProfile 接入；真实 HID 效果必须经过板端目标场景验证后才能标 REAL。

## 失败行为

- 无效 profile/session/sample：4xx，保持原文件不变。
- 租约失效：409，页面结束采集并允许重新开始。
- 样本不足：训练返回 422，说明缺少哪类样本。
- 模型未达到质量门槛：启用返回 422。
- 磁盘写入失败：5xx，不返回成功。

## 兼容性

- TTBOX 公共协议使用 `ttbox.motion-*` 命名。
- 参考实现只用于确认产品行为，不是 TTBOX 运行依赖。
- 不兼容其他产品的 daemon、ABI、密钥或目录。

## 自动标定行为

自动标定不是手动写入三个数字，而是 TTBOX 的闭环校准会话：

```text
准备环境
  → 稳定 X 轴候选（同一目标 ID/类别，中心抖动 <1px，尺寸变化 <5%，持续800ms）
  → X 轴 8/16/24/32/40 count 采样
  → X 轴 Median/MAD 拟合增益和延迟
  → 稳定 Y 轴候选
  → Y 轴 8/16/24/32/40 count 采样
  → Y 轴 Median/MAD 拟合
  → 验证范围/一致性
  → 应用并回读 Core
  → 完成或失败回滚
```

状态：`idle / preparing / stabilize_x / sampling_x / analyzing_x / stabilize_y / sampling_y / analyzing_y / validating / applying / completed / cancelled / failed`。

自动标定内部状态更新只更新页面状态和进度，不弹普通“已保存”提示；只有用户明确点击手动保存，才显示保存提示。

