# TTBOX 架构深度理解

> 本文记录 TTBOX 每个功能的实现逻辑：谁调用谁、数据怎么流、每个函数干什么。
> 基于本地源码逐行阅读（plugins/web/bin/ttbox-web.py + scripts/edid/ + ttbox_motion/）。

## 一、总体架构（3 层）

```
Web 前端 (plugins/web/static, 不动)
   ↓ HTTP (Flask)
Web 后端 (plugins/web/bin/ttbox-web.py)
   ↓ Unix Socket IPC (/tmp/ttbox_core.sock)
TTBOX Core (C++: V4L2/RGA/RKNN/DecodeNMS/AimThread/PID)
   ↓
硬件 (HDMI RX / NPU / RGA / HID)
```

Web 后端是**唯一**的 API 层，负责：参数翻译、状态聚合、IPC 通信、EDID/校准/运动档案等辅助功能。

## 二、IPC 通信层（Web → Core）

### `ipc_request(req_type, params, timeout)` — 唯一的 Core 通信入口
```
调用链: API 端点 → ipc_request → Unix Socket → Core 处理 → JSON 响应
```
逻辑：
1. 构造 `{type: req_type, params: params}` JSON
2. 连 `/tmp/ttbox_core.sock`（AF_UNIX SOCK_STREAM）
3. 发送 `JSON + '\n'`
4. 读到 `\n` 为止 → `json.loads` 返回
5. 错误分类：socket 不可用→status=3；无响应→status=3；连接失败→status=3；超时→status=3

### 核心 IPC 类型（谁用什么）
| IPC 类型 | 谁调用 | 干什么 |
|---|---|---|
| GET_CONFIG | _get_runtime_profile | 读 RuntimeProfile（Core 是配置唯一真源） |
| SET_CONFIG | 所有 PUT 端点 | 写 RuntimeProfile（热更新，Core 立即应用） |
| GET_STATUS | _get_status | 读运行状态/metrics（帧率/目标/延迟） |
| MODEL_LIST | collect_yu_state | 读模型库列表 + active |
| MODEL_IMPORT/VALIDATE/INSTALL | import_model | 导入模型三阶段 |
| MODEL_ACTIVATE | select_model | 激活模型 |
| MODEL_REMOVE | delete_model | 删除模型 |
| RUNTIME_CONTROL | control/start-stop | 启动/停止推理 |

## 三、配置翻译层（YU 格式 ↔ RuntimeProfile）

前端用 YU 扁平格式（ai.controller.kp_x 等），Core 用 RuntimeProfile 嵌套格式（mouse.kp_x 等）。
**两个翻译函数是核心**：

### `yu_body_to_profile(body)` — YU 前端保存 → RuntimeProfile
```
PUT /api/config → yu_body_to_profile → SET_CONFIG
```
逐字段映射：
- controller 数值直通：kp_x/ki_x/kd_x/predict_x/rate_x/smooth_x/output_deadzone（CONTROLLER_NUMS 表）
- controller 布尔直通：aim_fire_lock_y/block_physical_*（CONTROLLER_BOOLS 表）
- 热键：字符串('left') → 位掩码(1)（HOTKEY_BITS 表）
- 插件结构：pull_curve/continuous_lead/humanize/personal_motion → 嵌套 dict
- sens → mouse.sensitivity；pos → aim_point.offset_y
- 推理：confidence/iou/class_filter（位掩码→列表）
- 采集：crop_size → width/height；offset_x/y
- FOV：range_factor<1 → 圆形选择区（保留 prev shape/center）

### `profile_to_yu(prof)` — RuntimeProfile → YU 前端格式
```
GET /api/state → profile_to_yu → config（前端 populate 回读）
```
反向映射，含默认值兜底（缺字段返回 YU 默认：kp=7.0, pull_curve_strength=0.8 等）。

## 四、状态聚合层（collect_yu_state — /api/state 的数据源）

```
GET /api/state → collect_yu_state()
```
逻辑：**合并 3 个 IPC 响应 + 2 个本地函数**：
1. `_get_status()` → GET_STATUS → metrics（帧率/目标/延迟/运行状态）
2. `_get_runtime_profile()` → GET_CONFIG → RuntimeProfile
3. `MODEL_LIST` → 模型列表 + active（**registry active 覆盖 profile.model_id**，防缓存回跳）
4. `_auto_start_payload()` → systemctl is-enabled ttbox-core
5. `_calibration_payload()` → 校准运行时状态

关键映射（metrics → state 子结构）：
| metrics 字段 | state 目标 |
|---|---|
| aim_active/aim_target_id/aim_pos_x | state.aim（active/锁定目标） |
| detect_count/tracks/fps/infer_ms | state.detection |
| capture_fps/input_width | state.capture |
| e2e_ms | state.latency |
| last_error | state.last_error（无模型→"未导入模型"） |

## 五、自动校准域（真实闭环）

### 数据结构
- `_cal` dict：状态机（phase/status/state/reason/round/progress）+ 观测（candidate/stable/jitter）
- `_cal_lock`：线程锁
- `CALIBRATION_FILE=/opt/ttbox/config/calibration.json`：标定结果持久化

### 完整链路（POST /api/control/calibration/start）
```
start → 校验(推理在跑+有目标) → _calib_worker 线程
  → stabilize：12s 内目标稳定检测（同ID/类别/尺寸，抖动<1px，尺寸变化<5%，持续800ms）
  → 分轴采样：X/Y 轴，幅度 8/16/24/32/40
     每幅度：读基线位置 → SET_CONFIG 注入 calibration_bias_x/y → 采样20次目标位移
  → 算 gain = px位移 / count
  → _calib_apply_gain: K_LOOP(0.142857) / (gain×rate×sens×scale) → kp_x/kp_y → SET_CONFIG 热更新
  → 结果写 calibration.json
```

### 关键设计
- 标定时 `mouse.calibrating=true`（AimThread 放行 AI 移动，无视热键）
- 真实目标反馈来自 Core GET_STATUS.metrics（aim_pos_x/y = AimThread 选中目标中心）
- Y 轴复用 X 轴逻辑，X/Y 独立增益

## 六、EDID 域（scripts/edid/ + edid_apply.sh）

### 工具链文件职责
| 文件 | 干什么 |
|---|---|
| timing_db.py | 时序库（SAFE_MODES 8档 + TIMING_MAP 13个 + CVT-RB 公式 + mode_info 解析） |
| builder.py | EDID 生成器（256B：Base Block + CTA-861 Extension，含 DTD 打包/校验和） |
| validator.py | 校验 header/checksum/版本/扩展块 |
| monitor.py | DRM connector 读取 + hdmirx RX 状态 |
| mode_builder.py | 模式构建（显示器能力过滤 + load_config 白名单校验） |
| hdmirx_edid.py | CLI 工具（--list/--status/--profile/--native/--apply/--builtin） |
| edid_apply.sh | 应用闭环（bash 调 python + v4l2-ctl） |
| edid_patch_boot_image.sh | 内核持久化（EDID 写进 /boot/Image） |

### edid_apply.sh 完整链路
```
读 hardware_display.json
 → native_mode 保护（空/非法 → profile 首选兜底，防退化 1080p60）
 → build_from_config 生成 256B EDID
 → verify_edid 校验
 → 循环(16次): force_hpd off → v4l2-ctl --set-edid 注入 → get-edid 回读 name 验证 → force_hpd on
 → 成功: 持久化 /lib/firmware/ttbox/hdmirx_edid.bin + 输出 ok
```

### HPD 强制原理
- 写 `/sys/class/hdmirx/hdmirx/status` = off/on
- 驱动检测 HPD 变化 → 源端（Windows）重新枚举显示器 → 读到新 EDID

## 七、运动档案域（ttbox_motion/training.py）

### 数据模型
- profile: `{id, name, created_at_ms, samples[], model{knots[], quality, ready}, statistics}`
- session: `{id, profile_id, lease_expires_at}`（30 秒租约，心跳续期）
- active.json: `{profile_id, mix{curve, speed, reaction, max_reaction_delay_ms}}`

### 完整链路
```
POST /api/motion-training/sessions → start_session（租约30s）
PUT heartbeat → 续租
POST samples → append_sample（严格校验 schema/模式/画布/点序列）
POST /train → 算 knots（32节点曲线，按样本平均速度归一化）
POST /activate → 写 active.json + _apply_personal_motion_to_core（写 RuntimeProfile）
```

## 八、各功能 API 完整调用链

### 1. 总览页数据（/api/state）
```
GET /api/state
→ collect_yu_state()
→ ipc GET_STATUS（metrics）+ GET_CONFIG（profile）+ MODEL_LIST（模型）
→ profile_to_yu（配置翻译）+ _auto_start_payload + _calibration_payload
→ 返回完整 data（config/models/presets/state 19 子结构）
```

### 2. 配置保存（PUT /api/config）
```
PUT /api/config
→ yu_body_to_profile（YU扁平→RuntimeProfile）
→ _deep_merge_profile（合并进现有 profile）
→ ipc SET_CONFIG（Core 热更新，立即生效）
→ _get_runtime_profile 回读 → profile_to_yu 返回
```

### 3. 开机自启动（/api/settings/auto-start）
```
GET: systemctl is-enabled ttbox-core → enabled? → {enabled,status,message,updated_at}
PUT: systemctl enable/disable ttbox-core + ttbox-web → 返回 _auto_start_payload
（Core 是唯一被 enable 的目标，web 跟随）
```

### 4. 显示器信息（/api/hardware/display）
```
GET: v4l2-ctl query-dv-timing（当前时序）
  + 读 hardware_display.json（配置）
  + TTBOX_HDMIRX_EDID --list（模式列表，含 source/hdmi_raw_gbps）
  + TTBOX_HDMIRX_EDID --status（当前 EDID 身份）
  + _loopout_payload（DRM 状态）
PUT: 白名单合并 → 写 hardware_display.json → edid_apply.sh（注入）→ patch_boot_image
```

### 5. 鼠标信息（/api/hardware/mouse）
```
GET: 探测 /sys/bus/usb/devices（HID 接口03→父设备 vid/pid/name）
  + systemctl is-active ttbox-usbproxy（service 状态）
  + profile 的 mouse 配置 → 11 字段结构
PUT mode/timing: 写 profile → SET_CONFIG → _mouse_apply_payload（applied/state 文字）
```

### 6. 模型管理（/api/models/*）
```
GET /models: ipc MODEL_LIST → models[] + selected_model_id
POST /import: 存文件 → MODEL_IMPORT → MODEL_VALIDATE → MODEL_INSTALL（三阶段）
POST /select: MODEL_ACTIVATE → 写 profile.model_id + config model_path → SET_CONFIG
POST /delete: 检查 installed 存在 → MODEL_REMOVE
```

### 7. 控制启动/停止（/api/control/start-stop）
```
POST /start: 检查 model_id（无→"未导入模型"）→ ipc RUNTIME_CONTROL start → 返回完整 state
POST /stop: ipc RUNTIME_CONTROL stop → 返回完整 state
```

### 8. 预设（/api/presets）
```
GET: 列 /opt/ttbox/presets/*.json 的 stem
POST: 有 config 用 config，无 config 用 _get_runtime_profile（保存当前配置为预设）
POST /load: 读预设 json → yu_body_to_profile → merge → SET_CONFIG
```

### 9. 主题（/api/themes）
```
GET: 内置 default 主题（active/compatible/installed 12 字段）
POST /redeem: 校验 code → 无效报"请输入主题卡密"
POST /install: 校验 download_url → 无报"core download url is required"
PUT /current: 只允许 default → {active_theme_id, active_version}
```

### 10. 网络（/api/network/wifi）
```
GET: wifi_manager.wifi_status() → 探测 nmcli/无线网卡/AP 状态 → 12 字段
（networkmanager 探测 /sys/class/net 无线接口，未检测到→"未检测到无线网卡"）
```

## 九、校准 API 调用链（/api/control/calibration）

```
GET: _calibration_payload() → {runtime(状态机), calibration(标定结果)}
PUT: 白名单写 calibration.json + _calib_apply_gain（kp 换算写回 Core）
POST /start: 校验推理运行+目标存在 → _calib_worker 线程（见第五节完整闭环）
POST /cancel: _calib_set(status=idle, phase=cancelled) + 恢复 calibrating=false
DELETE: _clear_calibration() 删 calibration.json
```

## 十、运动档案 API 调用链（/api/motion-profiles + /api/motion-training）

```
GET /motion-profiles: MOTION_STORE.list_profiles()（读所有 profile 目录）
POST /motion-profiles: 对齐 YU 拒绝创建（仅 default）
PATCH /motion-profiles/<id>: rename
DELETE /motion-profiles/<id>: 删目录 + 若 active 则 deactivate
POST /motion-profiles/<id>/train: MOTION_STORE.train() → knots 曲线生成
POST /motion-profiles/<id>/activate: MOTION_STORE.activate() + _apply_personal_motion_to_core
DELETE /motion-profiles/active: deactivate（删 active.json）
POST /motion-training/sessions: start_session（租约 30s）
PUT /motion-training/sessions/<id>/heartbeat: 续租
POST /motion-training/sessions/<id>/samples: append_sample（严格校验）
```

## 十一、核心设计原则（从代码提炼）

1. **Core 是配置唯一真源**：所有 PUT → SET_CONFIG → Core 热更新，回读永远以 Core 为准
2. **单一翻译层**：yu_body_to_profile / profile_to_yu 两函数承担全部字段映射，无第二套
3. **诚实反映状态**：Core 不可达时返回 error（不伪造成功）；服务未启用显示 inactive/disabled
4. **registry 优先**：MODEL_LIST 的 active 覆盖 profile.model_id（防缓存回跳）
5. **校准是真实闭环**：注入 bias → 采样目标位移 → 算 gain → 写回 kp（非模拟）
6. **EDID 是完整闭环**：生成 → 校验 → 注入 → HPD → 回读 → 内核持久化
7. **隔离硬性**：所有路径 /opt/ttbox（不碰 /opt/aiassistance），服务名 ttbox-*（不碰 aiassistance-*）





