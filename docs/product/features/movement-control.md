# TTBOX 移动控制（03 移动控制页 / control-page）

> 最后更新：2026-09-01（Phase 8.3）
> 血缘审查：`docs/architecture/TTBOX_PHASE_8_3_CODE_LINEAGE.md`

## 产品目标

移动控制页管理 AI 瞄准的「怎么动」：PID 控制参数、移动倍率、拉枪曲线、持续提前量、屏蔽物理移动、开火锁 Y，以及鼠标响应自动标定。用户不需要理解控制理论，只调「追不上就加一点、来回冲就降一点」这类白话参数。

## 页面结构（5 个分区）

| 分区 | 内容 | 状态 |
|------|------|------|
| 01 自动标定 | 一键测鼠标响应（px/count + 延迟），写入 kp | REAL（手动/状态）；自动闭环 VERIFY（需真实游戏目标） |
| 02 PID | 移动倍率 + KP/KI/KD/Predict/Rate/死区（X/Y 独立） | REAL |
| 03 拉枪曲线 | 开关 + 强度 + 随机抖动 + 启用距离 | 配置链路 REAL；输出消费 PLANNED（29d3622 重构断线，PullCurve.hpp 类完整待接线） |
| 04 持续提前量 | 开关 + 进入距离 + 系数 + 渐入渐出 + 近距禁用 | 配置链路 REAL；C 桥实现 VERIFY（运行链未启用） |
| 05 屏蔽物理移动 | 瞄准时屏蔽 X/Y 物理轴 | 配置链路 REAL；C 桥实现 VERIFY（运行链未启用） |

## 数据流

```
浏览器控件 → window.ttbox.api → PUT/GET /api/config
  → scripts/ttbox_web.py（YU↔RuntimeProfile 翻译）
  → IPC SET_CONFIG → Core RuntimeConfig 热更新
  → AimThread 每帧读快照 → Pid1（kp/kd/predict/rate/smooth）
  → 输出链 ×sens×output_scale → output_deadzone 门控 → OutputAction
  → LocalHidBackend/AiboxHidOutput → /dev/hidg0
```

自动标定链路：

```
浏览器 → POST /api/control/calibration/start
  → Gateway 状态机（stabilize→moving→measuring→done，10 轮 X 往返）
  → Core GET_STATUS.metrics.aim_pos_x/y（AimThread 真实目标中心）
  → mouse.calibrating 放行 → kp 驱动鼠标 → 目标位移 → 测 gain=px/count
  → 写 /opt/ttbox/config/calibration.json + kp 换算写 RuntimeProfile
```

## 关键实现事实

1. **sens/死区/置信度输出链**在 9f30e5f 已接通真机，`aecddfb`（小白化工程）误回退，Phase 8.3 已按真机版回归（15 个 Core 文件）。
2. **自动标定假桩**（8e8f8c7 创建主 Gateway 时就是假桩）已替换为真实状态机（迁移旧后端实现，反馈源改 Core metrics，注入走 calibrating 模式）。
3. **kp 换算语义**：标定保存后 kp = K_LOOP/(gain×rate×sens×output_scale)，K_LOOP=1/7 对齐 YU 原机 P 增益。手动保存也会覆盖 kp（真实业务行为，UI 有提示）。
4. **目标反馈**：AimThread Status.predicted_x/y（选中目标中心）→ Metrics.aim_pos_x/y → GET_STATUS（Phase 8.3 新增暴露，Core 最小扩展）。
5. **标定放行**：mouse.calibrating=true 期间 AimThread/OutputBackend 无视热键强制放行 AI 移动（与 C 桥 compute_aiming 语义一致），结束自动恢复。

## 已知边界（诚实标注）

- 拉枪曲线输出消费：PullCurve.hpp 类完整但 AimThread 未接线（29d3622 重构遗留），Phase 8.3 未新增算法、保留 PLANNED。
- 持续提前量/屏蔽物理移动/开火锁Y：实现都在 C 桥（ttbox-hid-bridge.c），当前板端运行链未启用 C 桥（无进程、无 features.conf），配置链路 REAL、运行效果 VERIFY。
- 自动标定自动闭环：需要真实游戏画面 + 静止目标才能跑完整 10 轮；当前真机无目标时 start 被真实拒绝（业务正确）。
- 个性曲线训练：Core 无能力，UI 保留 PLANNED，不造假 API。

## 验证记录（2026-09-01 浏览器真实闭环）

| 项 | 操作 | 结果 |
|----|------|------|
| kp_x | 21.5 → 22.7 → 保存 → 回读 22.7 → 恢复 21.5 | ✅ |
| 拉枪强度 | 0.9 → 1.11 → 回读 → 恢复 0.9 | ✅ |
| 提前量系数 | 0.6 → 0.71 → 回读 → 恢复 0.6 | ✅ |
| 屏蔽物理 X | false → true → 回读 true → 恢复 false | ✅ |
| 开火锁Y | true → false → 回读 false → 恢复 true | ✅ |
| 自动标定 start | 无目标 → 真实拒绝（含原因） | ✅（业务正确） |
| 手动标定保存 | 0.651/0.649/8.4 → 落盘 + kp 换算 → 回读 → 恢复基线 | ✅ |
| 移动日志按钮 | 页面可见（此前隐藏） | ✅ |
| 运行环境 | 服务 active、采集 239fps、温度 66°C | ✅ |

## 后续计划

1. 拉枪曲线接入 AimThread 输出链（复用 PullCurve.hpp，无新算法）。
2. C 桥运行链启用后验证 fire_lock/block_physical/continuous_lead 实机效果。
3. 自动标定完整闭环需真实游戏场景验收。
