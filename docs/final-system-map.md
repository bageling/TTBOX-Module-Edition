# TTBOX 最终系统地图（Final System Map）

> 生成：2026-08-30。状态：代码级完成 + 主机测试全绿；板端项标注 BLOCKED。
> 数据流 / 控制流 / 配置流 / 状态流 四条线分别说明。

```
                          TTBOX 完整系统
   ┌────────────────────────────┬────────────────────────────┐
   │        CORE（C++17）        │        WEB（React+Vite）    │
   │  core/  · CTest 11/11 ✅    │  ttbox-web/ · build ✅     │
   └────────────────────────────┴────────────────────────────┘
```

## 1. 数据流（实时链路，240FPS 目标）

```
V4L2 Capture ──DMA fd──▶ RGA（crop/scale，零 CPU 拷贝）──▶ RKNN（NPU 推理）
                                                              │
                                                              ▼
WorkerPool ──DecodeNMS（fp16→fp32 位转换 + NMS）──▶ AimTargetMailbox（latest-frame 原子快照）
                                                              │
                                                              ▼
AimThread：TargetSelector（三层锁定防横跳）→ AimPoint → CoordinateTransform（参考点+误差）
        → Pid1Controller（X predict=3.0 / Y predict=0）→ 余数累计 → int16 clamp
        → Hotkey Gate（兜底归零）→ IHidOutput
                                                              │
                              ┌───────────────────────────────┤
                              ▼                               ▼
                   TtboxHidOutput（/dev/hidg0）      FifoHidOutput（bridge 协议）
                   [enabled + 热键 mask 双重保险]     [每帧重发控制帧 + 断开发 disable]
```

**符号与单位**（C2 审计确认）：
- 检测框/参考点/误差：crop 系**像素**（roi_w×roi_h）
- `error = target_aim_point − reference_point`（目标在右 → 正 X）
- PID 输出：HID count 域（float → 余数保留小数 → int16 clamp）
- FIFO 输出边界做符号翻转（设备坐标系相反）；TTBOX 不翻转
- FOV 模式（fov_mode=true）时先经角度换算（hfov/vfov→move_speed），默认关闭

## 2. 控制流（用户操作 → 生效）

```
Web UI（滑条/开关/下拉）
   │  SET_CONFIG（原子：解析→validate→update→落盘）
   ▼
RuntimeConfig（mutex 快照 + shared_ptr 原子替换）
   │  AimThread 每周期 snapshot()；Worker 每帧 snapshot()
   ▼
AimThread 接线：pid.configure()（不重置状态） / scfg（selector/fov/lost_grace）
   ▼
Pid1Controller / TargetSelector / Hotkey Gate 立即按新参数工作（无需重启）
```

运行控制：`RUNTIME_CONTROL start/stop/restart` → CoreRuntime（幂等，stop 后 join 线程）。

## 3. 配置流（唯一事实源）

```
配置文件 default.json ──▶ RuntimeProfile（to_json/from_json/validate）
                              │
        ┌─────────────────────┼──────────────────────┐
        ▼                     ▼                      ▼
   GET_CONFIG（读）      SET_CONFIG（写+校验）   MODEL_*（模型管理）
   Web 页面加载          热更新+落盘            模型库页
```

Web 侧不保存独立配置副本：保存=深拷贝当前→改字段→PUT→**回读 canonical**（validate 归一化以 Core 为准）。

## 4. 状态流（实时监控）

```
GET_STATUS（5s 轮询）→ {running, runtime_running, version, uptime_ms,
                        ipc_socket, config_file, metrics{fps,e2e_ms,...占位}}
   │
   ▼
RuntimeProvider（coreOnline/runtimeRunning/currentConfig/activeModel/lastError）
   │  断线自动重连（轮询不停止，恢复下一拍补拉）
   ▼
总览/系统状态/自检卡（Web 服务/IPC/配置读写/模型管理）
```

## 5. 模块完成度矩阵

| 模块 | 状态 | 说明 |
|---|---|---|
| Capture（V4L2） | ✅ 代码完整 | 板端 BLOCKED 实测 |
| RGA | ✅ 代码完整 | 板端 BLOCKED |
| RKNN / WorkerPool | ✅ 代码完整 | 板端 BLOCKED |
| Mailbox latest-frame | ✅ 测试过 | test_aim_target_mailbox |
| Decode/NMS | ✅ 测试过 | test_decode / geometry_filter |
| TargetSelector | ✅ 压力测试 6 用例 | 防横跳/交叉/抖动 |
| AimPoint / CoordinateTransform | ✅ 测试过 | test_mouse |
| Pid1Controller | ✅ 1:1 对照 | test_pid1 逐点一致 |
| Hotkey Gate | ✅ 7+ 场景 | gate/配置化/旁路审计 |
| HID（TTBOX/FIFO） | ✅ Fail-Closed 审计 | 板端 BLOCKED |
| RuntimeConfig 热更新 | ✅ 并发压测 | 无撕裂 |
| IPC（11 条） | ✅ 测试过 | 含模型管理 6 条 |
| ModelRegistry | ✅ 全通 | 收件目录/激活保护 |
| 生命周期 | ✅ 压测 60 次 | 无泄漏 |
| Web 前端 6 页 | ✅ 真实链路 | 无假数据 |
| 性能指标（P50/P95） | ⏸ 缺采集 | 板端统计接入（GAP-C） |
| MJPEG 预览 | ⏸ 缺出流 | GAP-E |

## 6. 当前 BLOCKED 清单（全部如实）

1. 板端 RK3588 全链路实测（Capture/RGA/RKNN/HID 真机）
2. PipelineMetrics 真实统计（FPS/延迟 P50/P95/P99）
3. MJPEG 预览流
4. 模型 metadata（输入尺寸/类别）需板端 RKNN validator
5. https_post_form（授权层，AUTH=OFF 不涉及）
