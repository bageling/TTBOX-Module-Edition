# Core 代码体检报告（Core Code Health）

> 扫描范围：`core/src` + `core/tests` + `platform/`。
> 方法：静态扫描（TODO/吞错误/魔法数/未实现）+ 行为审计（线程/热更新/输出链）+ 压力测试实证。
> 日期：2026-08-30。扫描后**可直接修的已全部修完并过回归**，详见"已修复"节。

## 一、已修复（本轮直接修）

| # | 严重度 | 问题 | 修复 | 回归 |
|---|---|---|---|---|
| F1 | **高（Fail-Closed）** | `RuntimeProfile::validate()` 无 isfinite 防线：JSON `1e999` 解析为 inf 可穿过 obj_num → kp_x=inf → PID 输出乱飞（已在 dev 链路实锤复现） | validate 开头加全部数值字段 isfinite 总闸，非有限值整体拒绝 | 新增 `runtime_profile_validate_rejects_nonfinite`，✅ |
| F2 | **高（乱飞）** | `AimThread` 余数累计直接 `static_cast<int16_t>`：异常大值时截断是实现定义行为 | 输出前 clamp 到 HID count 范围 + 余数 isfinite 兜底清零（并 reset PID） | CTest 全绿 ✅ |
| F3 | 中（死代码） | 旧控制链三件套残留生产目录：`TtboxPpidController.hpp` / `SmithPredictor.hpp` / `test_aim_algorithm.cpp`（已无任何消费者） | 删除 + CMake 清理 | CTest 10/10 ✅ |
| F4 | 低（一致性） | `motion.tsx` 丢 `usePageTitle`；RuntimeContext 文案"Core 未连接"与全站"后台服务未连接"不一致 | 补齐/统一 | 前端 build ✅ |

## 二、确认健康（审计通过，无需改动）

| 项 | 结论 |
|---|---|
| **Hotkey Gate 唯一输出路径**（C4） | 全仓 `output_->send` 仅 `AimThread.cpp:144` 一处；`move_x/y` 仅在 AimThread 内产生且 Gate 兜底在 send 前一行。TtboxHidOutput 二级门（enabled+hotkey mask）独立兜底。**无第二条输出路径** |
| **FIFO Fail-Closed**（C5） | `FifoHidOutput`：open 失败 return false；写失败（EPIPE/ENXIO）close 并 return false；停止/析构前发送 disable 帧清 Bridge 门控；0,0 帧也走完整链路（幂等无害）。异常默认不移动 ✅ |
| **TtboxHidOutput Fail-Closed**（C5） | 三重门：`enabled_` 静态闸 → `mouse.enabled`+热键 mask 实时判定（fail-closed：无配置源/配置缺失拒绝注入）→ open 失败不写 |
| **Mailbox 最新帧语义**（C7） | shared_ptr 原子快照（release/acquire），offer 覆盖式、take_latest 取最大 frame_number，无锁无阻塞；任务只含检测结果不含图像，无大拷贝 |
| **AimThread dt 丢弃**（C2） | pid1 原实现为离散单步（无 dt 项），`(void)dt` 与参考实现一致，非遗漏 |
| **pid1 热更新**（C3） | `configure()` 只改 kp/kd/predict/rate/smooth，**不触碰 kp_gain/integral_gain 渐变状态与滤波器状态**——热更新不破坏内部状态（与 pid1.cpp 语义一致） |
| **RuntimeConfig 热更新并发**（C11） | mutex 快照 + shared_ptr 原子替换；压测（2 写×1000 轮 + 2 读 200ms）无撕裂，快照始终完整（test_lifecycle_stress ✅） |
| **生命周期**（C14） | AimThread start/stop ×60 + 同实例 stop→start ×20 无泄漏无崩溃（running_ exchange + join 语义正确） |
| **TargetSelector 防横跳**（C6） | 三层选择（track_lock→rect_lock→score）实测：抖动稳定 id、双目标交叉不切换、检测顺序翻转稳定、宽限后正确释放、teleport 正确重挂（6 用例 ✅） |
| **TODO/FIXME** | 生产代码 0 处（仅 JSON 注释里 \uXXXX 字样误匹配） |
| **编译警告** | MSVC /W4 全绿 0 警告 |

## 三、遗留与 BLOCKED（不伪造）

| 项 | 状态 | 说明 |
|---|---|---|
| `HttpClient.cpp:176` https_post_form | 仅板端/授权路径（TTBOX_CORE_BUILD_AUTH=OFF 时不编译），留 TODO 属上游实现 | BLOCKED — 无 OpenSSL/真机环境 |
| PipelineMetrics 真实统计（C8） | 结构占位（fps/e2e_ms 等恒 0），Web 已按"暂无数据"处理 | BLOCKED — 需板端 WorkerPool/AimThread 统计接入，**P50/P95/P99 目前无采集能力，已记录** |
| MJPEG 预览流（C8 关联） | 无出流 | BLOCKED — 需板端 V4L2 |
| RGA/DMA-BUF 零拷贝实证（C9） | 代码级审计：RgaProcessor 注释与设计无 CPU memcpy（数据在 DMA fd 间流转）；Decode/NMS 的 memcpy 是 float↔u16 位模式转换（必要，非图像拷贝）；HID Forwarder memcpy 是报文组装（必要） | 板端性能实测 BLOCKED — 无 RK3588 |
| platform/ 契约层 | 纯 stdlib 契约 + 测试 34 passed；systemd/RK3588 适配属部署阶段 | BLOCKED — 无实机 |

## 四、建议（不阻塞）

1. `Application.cpp:60` class_filter 解析 `catch(...){}` 吞非法 token：建议至少 LOG_WARN（当前行为=静默忽略错误类别，可接受但可观测性差）。
2. `test_ipc*.cpp` 固定端口 39128-39131 在并行 CI 下可能互撞：已加重试缓解，长期可改端口池。
