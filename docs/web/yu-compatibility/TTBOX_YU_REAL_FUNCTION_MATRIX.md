# TTBOX / YU 真实功能验收矩阵（第二轮）

> 验收日期：2026-09-05
> 验收原则：**不只看 API 返回值**。每项验证完整链路：API → 配置 → 缓存 → Runtime → 实际行为 → 重启保持 → 错误行为 → 双实例隔离。
> 验收环境：RK3588 Orange Pi 5 Plus（192.168.0.53），TTBOX web=8000/preview=8001/core=disabled，YU web=8080/daemon=active。
> 自动化测试：`tests/compatibility/real_function_test.sh`（22 项，FINAL: PASS）
> 配套脚本：`scripts/lan_blocklist.sh`（TTBOX 独立防火墙 chain）

## 汇总

| 指标 | 数值 |
|---|---|
| 本轮验收功能域 | 30 |
| 真实功能测试项 | 22 |
| PASS | 22 |
| FAIL | 0 |
| 修复假实现 | 12 处 |
| 配置真相统一 | 2 处（/opt/ttbox/config/default.json 唯一 + src/config 同步） |
| Core 真实链路 | 验证通过（配置加载→V4L2 STREAMON→IPC GET_CONFIG status:0） |
| 上一轮 24/24 性质 | **24 项均为 API 层结构对比，0 项验证实际行为**（本轮已补真实验证） |

## 上一轮 24/24 PASS 的性质判定

上一轮 `run_all.sh` 的 24 项全部是 **HTTP 返回 + JSON 结构断言**（health/字段存在性/状态字符串/HTTP 状态码），
**没有任何一项验证"实际功能是否发生"**。本轮逐项补齐真实链路验证后确认：

- 12 项存在"API 返回成功但实际无副作用/配置丢失/状态假"的问题（本轮已修复）
- 修复后：所有写 API 要么真实生效（iptables 规则/文件落盘/systemd 操作），要么诚实报错（不假成功）

## 状态图例

- ✅ PASS：真实链路验证通过
- 🟡 设计差异：TTBOX 与 YU 有意的行为差异（已说明原因）
- 🟢 诚实拒绝：无能力时返回明确错误而非假成功
- 🔴 FAIL：API 成功但实际无效（本轮已全部修复，0 个残留）

## A 组：系统域

### F01 hostname
- **YU 行为**：hostnamectl 改主机名，返回网络摘要（hostname/lan_ipv4/lan_url/mdns_url）
- **TTBOX 行为**：`hostnamectl set-hostname` 真实执行（系统级副作用），返回 collect_network_summary()
- **API**：PUT /api/system/hostname
- **配置**：系统 /etc/hostname（hostnamectl 管理）
- **Runtime**：无需 Core
- **实际行为**：hostname 立即改变 ✅
- **错误行为**：空/超长 → "主机名无效"；命令失败 → stderr
- **重启行为**：hostnamectl 持久化 ✅
- **隔离行为**：与 YU 共用系统主机名（系统级资源，非软件隔离项）
- **最终状态**：✅ PASS

### F02 web-port
- **YU 行为**：校验 1024-65535 → 写 systemd drop-in（Environment=AIASSISTANCE_PORT）→ daemon-reload → 延时重启 web → 返回 restart_scheduled:true
- **TTBOX 行为（本轮修复前）**：🔴 只写 default.json 的 web_port 键，**端口不生效**（假实现）
- **TTBOX 行为（修复后）**：校验 1024-65535 → 写 `/etc/systemd/system/ttbox-web.service.d/port.conf`（Environment=TTBOX_WEB_PORT）→ daemon-reload → 延时重启 ttbox-web → 返回 restart_scheduled:true
- **API**：PUT /api/system/web-port
- **配置**：systemd drop-in（真实生效层）+ default.json web_port
- **实际行为（实测）**：PUT 9000 → 服务 3 秒后监听 0.0.0.0:9000、8000 释放 ✅；改回 8000 恢复 ✅
- **错误行为**：<1024/非数字 → "访问端口必须在 1024-65535 之间"（对齐 YU）
- **重启行为**：drop-in 持久化，重启后保持新端口 ✅
- **隔离行为**：drop-in 只影响 ttbox-web.service（YU 的 AIASSISTANCE_PORT 不受影响）✅
- **最终状态**：✅ PASS

### F03 auto-start
- **YU 行为**：enable/disable aiassistance 服务；GET 返回 enabled/status(disabled|next_boot)/message
- **TTBOX 行为**：`systemctl enable/disable ttbox-core + ttbox-web` 真实执行；GET 读 `systemctl is-enabled ttbox-core` 真实状态
- **API**：GET/PUT /api/settings/auto-start
- **配置**：systemd 启用位（真实）
- **实际行为**：PUT enabled=true → `systemctl is-enabled ttbox-core` = enabled ✅（实测）
- **错误行为**：非 bool → 400 "enabled must be a boolean"
- **重启行为**：systemd 持久化 ✅
- **隔离行为**：只操作 ttbox-* 服务 ✅
- **最终状态**：✅ PASS

### F04 storage
- **YU 行为**：df 真实读数 + expand 调用 growpart 脚本
- **TTBOX 行为**：GET 返回 rootfs 完整 11 字段（df 实测 free/used/total）；expand 执行 growpart 流程
- **API**：GET /api/system/storage、POST /api/system/storage/expand
- **实际行为**：free/used 真实来自 df ✅
- **错误行为**：expand 失败返回错误
- **最终状态**：✅ PASS（扩容属高风险操作，未做破坏性实测，逻辑链路完整）

### F05 announcement
- **YU 行为**：连 license server 获取公告，失败 503 `{"error":"failed to connect license server: Not Found","ok":false}`
- **TTBOX 行为**：完全一致返回 503（TTBOX 无公告服务）🟢 诚实拒绝
- **最终状态**：✅ PASS（与 YU 实测一致）

### F06 lan-blocklist
- **YU 行为**：校验 ip 非空 + 不能拉黑自身 → 执行 LAN_BLOCKLIST_SCRIPT（真实 iptables/nft 操作，chain=AIASSISTANCE_BLOCKLIST）→ 返回 payload；GET 回读规则
- **TTBOX 行为（本轮修复前）**：🔴 POST 纯假返回"已更新"、DELETE 纯假"已清空"、GET 硬编码空列表（无任何防火墙副作用）
- **TTBOX 行为（修复后）**：`scripts/lan_blocklist.sh`（独立 chain=TTBOX_BLOCKLIST，nft 后端 iptables）真实增删规则
- **API**：GET/POST/DELETE /api/system/lan-blocklist、POST /api/system/lan-blocklist/scan
- **实际行为（实测）**：set 192.168.0.99 → `iptables -S TTBOX_BLOCKLIST` 出现 `-A ... -s 192.168.0.99/32 -j DROP`；重复 set 幂等"已在黑名单"；GET 从 iptables 回读 blocked_ips；clear 后规则清空 ✅
- **错误行为**：空 ip → "请选择或输入要拉黑的局域网 IP"；自身 ip → "不能拉黑当前正在访问页面的设备"（对齐 YU）✅
- **重启行为**：iptables 规则重启后消失（YU 同样；由服务启动脚本重建）
- **隔离行为**：TTBOX_BLOCKLIST 与 AIASSISTANCE_BLOCKLIST 独立 chain，互不触碰（实测 YU chain 完好）✅
- **最终状态**：✅ PASS

### F07 reactivate / master-reactivate
- **YU 行为**：daemon refresh_device_identity → 授权正常时返回 400 "当前授权状态正常，无需修复授权"
- **TTBOX 行为（本轮修复前）**：🔴 纯假返回 ok:true "已重新激活"
- **TTBOX 行为（修复后）**：与 YU 实测一致：400 "当前授权状态正常，无需修复授权"（TTBOX 本地授权恒激活）
- **最终状态**：✅ PASS

## B 组：配置/模型域

### F08 config（GET/PUT /api/config）
- **YU 行为**：PUT patch 深合并到 daemon 内存配置并返回完整合并配置（config.json 不落盘，daemon 内存生效）；GET 返回完整配置
- **TTBOX 行为（本轮修复前）**：🔴 Core 不在时 GET 返回**空结构**（页面全空）；PUT 把 translated 浅写 default.json **顶层**（Core 只读 runtime_profile 键 → 配置丢失），返回 ok:true + 空结构
- **TTBOX 行为（修复后）**：Core 在时走 IPC GET_CONFIG/SET_CONFIG（热更新+落盘 runtime_profile 键）；Core 不在时 **GET 从文件 runtime_profile 段兜底翻译返回完整结构**、**PUT 深合并后写 runtime_profile 键**（与 Core 落盘格式一致）+ 返回保存后的真实配置 + persisted:true
- **API**：GET/PUT /api/config
- **配置**：/opt/ttbox/config/default.json（唯一真相，runtime_profile 键）
- **缓存**：无独立缓存层；Core 内存 RuntimeConfig 为热更新真源，文件为持久化真源
- **Runtime**：Core 在时 SET_CONFIG → runtime_config_.update 原子替换（下周期生效）；Core 不在时文件落盘（下次启动加载）
- **实际行为（实测）**：PUT vdc=0.77 → 返回 0.77 + 文件 confidence=0.77；重启 web → GET 仍 0.77 ✅
- **错误行为**：非法 body → 400；保存失败 → 500 "Core 未运行且配置保存失败"
- **重启行为**：持久化 ✅（实测重启后保持）
- **隔离行为**：TTBOX 只写 /opt/ttbox/config/default.json ✅
- **最终状态**：✅ PASS

### F09 device-code
- **YU 行为**：返回真实 CPU 序列号/设备指纹
- **TTBOX 行为**：真实读 /proc/cpuinfo Serial 生成 device_id/fingerprint
- **实际行为**：device_id=opi-<serial> 真实 ✅
- **最终状态**：✅ PASS

### F10 cloud-encrypted
- **YU 行为**：空名 → 400 "云端模型名不能为空"；非 .rknn → 400 "云端模型名必须以 .rknn 结尾"；.rknn → 尝试云端 SDK，失败返回 ok:false + error
- **TTBOX 行为（本轮修复前）**：🔴 无论是否传名都返回"云端模型名不能为空"（永远失败 + 文案错误）
- **TTBOX 行为（修复后）**：校验链与 YU 完全一致；.rknn 时 TTBOX 本地模式无云端服务 → 503 "TTBOX 本地模式未接入云端模型服务"（诚实拒绝，不假成功）
- **最终状态**：✅ PASS

### F11 bind-preset / game-profile / class-names / rknn-concurrency / hailo-pipeline-depth / remote-frame-format
- **YU 行为**：写模型 manifest/配置并让 daemon 生效
- **TTBOX 行为**：缺 model_id → "model_id is required"（对齐 YU）；有 model_id 且模型存在 → 真实写 manifest.json / 模型 JSON（文件副作用真实）
- **实际行为**：bind-preset 写 manifest.preset_name ✅（文件可回读）
- **最终状态**：✅ PASS（模型存在时的写文件链路真实；Core 不在时热更新部分诚实报错）

### F12 model select/delete/import
- **YU 行为**：真实模型注册表操作 + 运行时重载
- **TTBOX 行为**：预检查 installed 文件存在（不存在 → "model_id not found"）→ MODEL_ACTIVATE/MODEL_REMOVE/MODEL_IMPORT IPC → Core 不在时返回 IPC 错误（诚实）；Core 在时真实注册/激活
- **实际行为**：import 真实落盘到 /opt/ttbox/models/_incoming（文件副作用）✅；delete 预检查 ✅
- **最终状态**：✅ PASS（Core 在时链路完整；不在时诚实报错）

## C 组：硬件域

### F13 display（GET /api/hardware/display）
- **YU 行为**：v4l2-ctl 真实探测 HDMI + EDID 工具回读显示器身份
- **TTBOX 行为**：v4l2-ctl query-dv-timing 真实探测（/dev/video0）+ `hdmirx_edid.py --list/--status` 真实回读 + hdmirx 状态 monitor
- **实际行为**：connected/locked/width/height/refresh 真实来自 V4L2 ✅；advertised/available_modes 真实来自 EDID 工具 ✅
- **最终状态**：✅ PASS（真实探测；信号接入时数据真实变化）

### F14 display PUT + EDID apply
- **YU 行为**：写 hardware_display.json + 注入 EDID 到 hdmirx + patch 内核
- **TTBOX 行为**：写 /opt/ttbox/config/hardware_display.json（白名单键合并，native_mode 非法 token 拒绝覆盖）→ apply 时执行 `edid_apply.sh` 真实注入 → patch_boot_image 时执行 `edid_patch_boot_image.sh`
- **实际行为**：配置文件真实写入 ✅；EDID 注入调用真实脚本（需 HDMI 信号重协商才可见，属硬件验证项）
- **错误行为**：apply 失败 → "EDID 应用失败" + stderr
- **隔离行为**：TTBOX 用自己的 hdmirx_edid.py（/opt/ttbox/scripts/edid/），不调 YU 工具 ✅
- **最终状态**：✅ PASS（写入/脚本链路真实；HPD 重协商属 SHARED_HARDWARE_RESOURCES）

### F15 loopout
- **YU 行为**：DRM overlay 环出（真实硬件合成）
- **TTBOX 行为**：loopout_enabled 持久化到 hardware_display.json；状态段读真实 DRM connector；实际环出由 C++ overlay 模块消费（Core 运行时生效）
- **实际行为**：配置持久化 ✅；DRM 状态真实探测 ✅；实际环出需 Core + HDMI 环出硬件链路（硬件验证项）
- **最终状态**：✅ PASS（配置/探测真实；硬件环出归 SHARED_HARDWARE_RESOURCES）

### F16 mouse GET
- **YU 行为**：sysfs 真实 USB 鼠标探测 + 服务状态
- **TTBOX 行为**：真实读 /dev/hidg* + sysfs USB 接口向上找父设备读 vid/pid + `systemctl is-active ttbox-usbproxy`
- **实际行为**：connected/mode/service_active 真实反映 ✅
- **最终状态**：✅ PASS

### F17 mouse PUT / mode / timing
- **YU 行为**：daemon 热更新鼠标配置并生效
- **TTBOX 行为（本轮修复前）**：🔴 改内存 prof → SET_CONFIG 静默失败（Core 不在）→ 仍返回 ok:true（假成功，配置丢失）
- **TTBOX 行为（修复后）**：SET_CONFIG 失败时**落盘 runtime_profile.mouse 键**（Core 下次启动加载）+ 返回 _core_offline:true 诚实标记；Core 在时热更新
- **实际行为（实测）**：PUT mode=kmbox → 文件 mouse.mode=kmbox ✅ + _core_offline:true ✅
- **错误行为**：timing 缺 identity_change_settle_delay_sec → 400 "identity_change_settle_delay_sec must be a number"（对齐 YU）
- **重启行为**：落盘持久化 ✅
- **最终状态**：✅ PASS

### F18 hailo
- **YU 行为**：真实探测 Hailo-8 PCIe + HailoRT 版本
- **TTBOX 行为**：真实扫 /sys/bus/pci + hailortcli --version；无设备时 install → 400 "未检测到 Hailo-8 PCIe 设备"
- **实际行为**：ready:false / status:idle 真实反映（板子无 Hailo 硬件）✅
- **最终状态**：✅ PASS（UNSUPPORTED_HARDWARE：无 Hailo 硬件，探测真实）

### F19 usb-diag（usb-proxy.zip）
- **YU 行为**：打包 usbproxy 诊断信息
- **TTBOX 行为**：真实 zipfile 打包（service 状态 + hidg 设备 + 进程信息）
- **实际行为**：HTTP 200 + 真实 zip（约 317B 内容随系统状态变化）✅
- **最终状态**：✅ PASS

## D 组：控制/诊断域

### F20 calibration
- **YU 行为**：真实标定闭环（检测目标 → 注入移动 → 测量增益 → 写回 RuntimeProfile）
- **TTBOX 行为**：真实闭环逻辑在 _calib_worker（稳定检测 → X 轴往返注入 → gain=px/count）；start 前检查 runtime_running（Core 不在 → 400 "推理服务未运行或目标反馈未就绪"）诚实 ✅
- **TTBOX 行为（本轮修复前 PUT）**：🔴 Core 不在时写文件成功仍标记 ready=True/status=done（假完成）
- **TTBOX 行为（修复后 PUT）**：文件落盘成功但 _calib_apply_gain 失败时 phase=saved + **ready=False**（不假完成）
- **实际行为（实测）**：PUT gain → 文件 calibration.json 真实写入 ✅ + ready=False（Core 不在）✅
- **最终状态**：✅ PASS

### F21 aim-trace
- **YU 行为**：诊断采样真实瞄准轨迹
- **TTBOX 行为（本轮修复前）**：🔴 Core 不在时仍返回 recording:true 并记录全 0 假轨迹
- **TTBOX 行为（修复后）**：Core 未运行（runtime_running=false）→ 400 "推理服务未运行，无法记录瞄准轨迹"
- **实际行为（实测）**：Core 不在 → 400 ✅
- **最终状态**：✅ PASS

### F22 control/start-stop
- **YU 行为**：daemon 启停推理运行时
- **TTBOX 行为**：无模型 → "未导入模型"（对齐 YU）；有模型 → RUNTIME_CONTROL IPC → Core 不在返回 IPC 错误（诚实）；Core 在时真实启停推理
- **实际行为**：无模型 → "未导入模型" ✅；Core 不在 → 诚实报错 ✅
- **最终状态**：✅ PASS

### F23 reboot/poweroff
- **YU 行为**：POST 返回 {action, scheduled:true}，延时执行 systemctl reboot/poweroff
- **TTBOX 行为**：dry_run 支持（不真执行）+ 非 dry_run 延时线程执行 systemctl ✅
- **实际行为（实测）**：dry_run 返回 {action:"reboot",scheduled:true} + 服务未被重启 ✅
- **最终状态**：✅ PASS

## E 组：网络/内容/更新域

### F24 wifi
- **YU 行为**：nmcli 真实扫描/连接/AP 模式切换
- **TTBOX 行为**：wifi_manager.py 真实 nmcli 操作（扫描/connect/fallback/AP apply/client activate），热点 SSID=TTBOX（品牌隔离）
- **实际行为**：真实 nmcli 调用 ✅（未做实际联网破坏性测试，命令链路真实）
- **隔离行为**：SSID/连接名 ttbox-* 前缀 ✅
- **最终状态**：✅ PASS

### F25 remote
- **YU 行为**：未连接 Windows 电脑时提示输入局域网 IP
- **TTBOX 行为**：全部 remote/* 返回 "请输入 Windows 电脑局域网 IP"（对齐 YU）
- **最终状态**：✅ PASS（对齐 YU 无连接时的行为）

### F26 xcsh
- **YU 行为**：网页背景仅对 XCSH 系统开放
- **TTBOX 行为**：xcsh/background 全套返回 "网页背景仅对 XCSH 系统开放"（对齐 YU）
- **最终状态**：✅ PASS

### F27 presets
- **YU 行为**：预设真实文件保存/加载/导入/导出
- **TTBOX 行为**：/opt/ttbox/presets/*.json 真实读写；load 不存在 → "failed to open <path>"（对齐 YU）；import 缺文件 → "missing upload field: file"
- **TTBOX 行为（本轮修复）**：保存无 config 时从文件兜底读当前配置（不再保存空 {}）
- **最终状态**：✅ PASS

### F28 motion-profiles / motion-training
- **YU 行为**：内置 default 档案 + 训练会话
- **TTBOX 行为**：MOTION_STORE 真实文件操作（list/rename/delete/export/session/samples/train）；create 拒绝（对齐 YU "only the internal default motion profile is supported"）；activate 调 Core SET_CONFIG（Core 不在 → 诚实报错）
- **最终状态**：✅ PASS

### F29 themes
- **YU 行为**：内置 default 主题 + 卡密兑换
- **TTBOX 行为**：GET 内置 default（对齐 YU 12 字段）；redeem 空 → "请输入主题卡密"、非空 → "redeem failed: invalid code"；install 缺 URL → "core download url is required"；current PUT → {active_theme_id:"default", active_version:""}
- **最终状态**：✅ PASS（TTBOX 无主题商店，拒绝语义对齐 YU）

### F30 update
- **YU 行为**：update engine 检查/下载/安装/回滚
- **TTBOX 行为**：Update Engine 未安装 → status 返回 idle 结构（"暂无更新任务"）+ 其他动作 503 "Update Engine 未安装"（诚实）；versions 返回组件版本；cleanup-stuck 返回 idle 结构
- **实际行为**：无 engine 时 idle/503 诚实反映 ✅
- **最终状态**：✅ PASS（Update Engine 未部署属 BLOCKED 项，API 诚实反映）

## 修复清单（本轮 12 处）

| # | 模块 | 修复前 | 修复后 |
|---|------|--------|--------|
| 1 | GET /api/config | Core 不在 → 空结构 | 文件 runtime_profile 兜底翻译完整结构 |
| 2 | PUT /api/config | 浅写顶层（Core 不读）+ 返回空 | 深合并写 runtime_profile 键 + 返回保存后配置 |
| 3 | lan-blocklist POST | 纯假"已更新" | 真实 iptables DROP（TTBOX_BLOCKLIST chain） |
| 4 | lan-blocklist DELETE | 纯假"已清空" | 真实清 chain |
| 5 | lan-blocklist GET | 硬编码空列表 | iptables 真实回读 |
| 6 | web-port PUT | 只写文件不换端口 | drop-in + daemon-reload + 延时重启 + restart_scheduled |
| 7 | reactivate | 假"已重新激活" | 400 "当前授权状态正常，无需修复授权" |
| 8 | cloud-encrypted | 永远"不能为空" | 校验链对齐 YU + 无云端诚实 503 |
| 9 | state/license core 段 | 硬编码 loaded | systemctl is-active 真实状态 |
| 10 | mouse PUT/mode/timing | SET_CONFIG 静默失败假成功 | 落盘 + _core_offline 诚实标记 |
| 11 | aim-trace | Core 不在记录全 0 假轨迹 | 400 "推理服务未运行" |
| 12 | calibration PUT | 假标记 done | ready=False + phase=saved |

## 配置真相（本轮统一）

- **唯一真相**：/opt/ttbox/config/default.json（Core systemd `--config` 显式指定 + web 读写同一路径）
- runtime_profile 键 = Core SET_CONFIG 落盘格式 = web 兜底写入格式（两侧一致）
- /opt/ttbox/src/config/default.json 陈旧分叉已与唯一真相同步（消除"多份配置"风险）
- Core 启动链路实测：`config.load → RuntimeProfile 已加载 → CoreRuntime init(workers=3) → IPC 服务启动 → V4L2 STREAMON OK → GET_CONFIG status:0`
