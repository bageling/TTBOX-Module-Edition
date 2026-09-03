# YU → TTBOX 夜间差异矩阵

> 基线：2026-09-02；标准来源为仓库既有 YU 参考映射 + TTBOX 当前源码 + RK3588 真机状态。`REAL` 只表示浏览器真实读改保存并验证到实际链路；配置回读但未证明硬件行为标 `VERIFY`。

| YU 功能 | YU 行为 | TTBOX 当前状态 | 差距 | 优先级 | 是否已修复 | 验证 |
|---|---|---|---|---|---|---|
| Overview | 状态、预览、FPS、延迟、启停、配置 | `/api/state`、MJPEG、RuntimeConfig、IPC 已接 | 运行效果仍需分项复验 | P0 | 部分 | REAL/VERIFY |
| Hotkey | 主/副键、any/all、类别、灵敏度、偏移 | RuntimeProfile→AimThread Gate 已接 | 全局禁用/备用偏移无 Core 能力 | P0 | 部分 | REAL/PLANNED |
| Profiles | 配置保存、恢复、profile 语义 | 单 RuntimeProfile + 预设基础链 | 多 profile 切换语义需完整验证 | P1 | 部分 | VERIFY |
| Movement Control | PID、预测、跟随、死区、丢锁宽限、拉枪、提前量、标定、训练 | 多数字段已进 RuntimeProfile；训练域已存在 | 拉枪/提前量/训练真实输出闭环仍需验证 | P0 | 部分 | REAL/VERIFY |
| Assist / Aim | 目标、预测、平滑、FOV、激活、优先级 | TargetSelector、Pid1、AimStateMachine | YU 目标对象语义与 TTBOX 单 Detection 不同 | P0 | 未完成 | VERIFY |
| Assist / Recoil | 压枪及触发条件 | Web/配置链存在 | 真 HID 效果证据不足 | P0 | 部分 | VERIFY |
| Assist / Trigger | 自动开火/连点 | Web/配置链存在 | 真设备副作用需专项验收 | P0 | 部分 | VERIFY |
| Assist / Back flick | 回甩 | Web/配置链存在 | 真设备副作用需专项验收 | P1 | 部分 | VERIFY |
| Assist / Crosshair | 准星找色 | 配置/UI存在 | Core真实消费点需核验 | P1 | 部分 | VERIFY |
| Model Library | 列表、选择、导入、删除、元数据、并发 | ModelRegistry + IPC + Gateway | 激活后实际运行模型切换需重测 | P0 | 部分 | REAL/VERIFY |
| WiFi | 状态、扫描、连接、AP/Client | nmcli 真实接口 | 修改网络属于独立高风险验收，本夜不执行 | P1 | 未执行 | VERIFY |
| Hardware | HDMI/显示/EDID/温度/存储 | sysfs/V4L2/真实状态接口 | 显示写配置副作用需分项验收 | P1 | 部分 | VERIFY |
| Keyboard/Mouse | 输入状态、设备状态、启停、AI输出 | Input reader、AiboxHidOutput、HID 状态模型 | 当前板端实际代理路径与 TTBOX 输出路径存在差异；圆周测试接口已明确标记未接入 | P0 | 部分 | BROKEN/VERIFY/PLANNED |
| Network | 主机名、端口、局域网屏蔽 | 路由存在 | 网络修改本夜不执行；需后续专项 | P1 | 部分 | VERIFY |
| Presets | 保存、加载、删除、重命名、导入导出 | 基础链存在 | 完整持久化回归待执行 | P1 | 部分 | VERIFY |
| System Status | 版本、CPU/RAM/温度/存储、自启 | 真实系统接口 | 个别字段一致性待复验 | P1 | 部分 | VERIFY |
| Fan Control | 温度关联 PWM/风扇 | Core/系统路径存在 | 已运行但需读取证据归档 | P1 | 部分 | VERIFY |
| Preview | 低帧、ROI、实时画面 | `/api/preview.mjpg` + PreviewModule | 需持续帧内容验证 | P1 | 部分 | REAL/VERIFY |
| Update | 检查、安装、取消、回滚、日志 | Release/Update 组件存在 | 安装/回滚副作用本夜不执行 | P1 | 部分 | VERIFY |
| Cloud/Theme/Hailo | YU 外部商业/异构硬件能力 | 按 TTBOX 产品边界预留/不纳入 | 非 TTBOX 当前硬件能力 | P2 | 规划 | PLANNED |

## 链路状态汇总

```text
页面 → app.js/apiClient → Gateway → IPC/RuntimeConfig → Core → OS/HID/RKNN
```

当前最重要的 P0 差距：

1. TTBOX `DetectionBox → selected.box`，没有 YU 等价 Target Object 的可验证完整层。
2. 板端存在官方鼠标代理，但 TTBOX 当前运行状态需继续确认实际输出消费者是否一致。
3. 多数 Assist/Movement 字段已经能保存和回读，但“参数改变造成真实设备行为改变”证据不足。
4. 不能把 API 回读等同于硬件闭环。

## 本夜执行边界

- 不执行整机重启、网络修改、SSH/防火墙修改、内核/BSP 修改、系统升级。
- 只允许必要的 TTBOX 单服务验证。
- 不把 YU daemon 接入 TTBOX。
- 不修改前端 CSS、框放大或坐标缩放来掩盖模型差异。
