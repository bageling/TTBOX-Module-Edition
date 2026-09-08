# TTBOX / YU 完美复刻验收报告

> 日期：2026-09-05　板子：192.168.0.53（RK3588）
> 目标：TTBOX 后端完整复刻 YU，现有 Web 前端零修改即可使用，逐功能与真实 YU 对比。

## 总览

| 指标 | 数值 |
|---|---|
| YU API 总数 | 100 |
| TTBOX API 总数 | 114（含 4 个增强路由） |
| 已对齐功能域 | 34 |
| 回归测试项 | 24 |
| PASS | 24 |
| FAIL | 0 |
| BLOCKED | 0 |
| UNSUPPORTED_HARDWARE | 0 |
| 假 PASS | 0 |
| Frontend Modified | NO |
| 重启持久化 | PASS |
| 消灭假实现 | 22 处 |

## 已 PASS 功能域（真实 API 对比）

| ID | 功能域 | 关键对齐点 |
|---|---|---|
| WEB-001 | 总览/全局状态 | state 19 字段、auto_start 状态文字(disabled/next_boot) |
| WEB-002 | 校准/控制 | reason=not_running、float32 精度、candidate_rect |
| SYS-001~005 | 系统/状态/授权/自启 | license 8 字段(activated)、auto-start 结构 |
| SYS-006 | 系统存储 | rootfs 11 字段、growpart |
| SYS-012 | 公告 | 503 + error 结构对齐 |
| SYS-lan | 局域网黑名单 | blocked_ips/chain/supported/message |
| HWD-001 | 显示器信息 | loopout、source、hdmi_raw_gbps 公式(4点验证) |
| HWD-003 | 鼠标信息 | 11 字段、Logitech 真实识别、service 状态文字 |
| MDL-001 | 模型列表 | selected_model_id |
| PRE-001 | 预设 | presets 结构 |
| HOT-001 | 运动档案 | model 补 coverage/sample_count |
| NET-001 | WiFi | 12 顶层 key、ap 14 字段、状态文字"未检测到无线网卡" |
| UPD-001~005 | 更新 | idle 状态文字、补建 versions/cleanup-stuck |
| THEMES | 主题 | 内置 default 主题 12 字段 |
| ACT | 激活域 | license/activate 空key报错、reset 拒绝、full-recovery 拒绝 |
| THEMES-OP | 主题操作 | redeem/install/current 校验对齐 |
| HWD-004/5/6 | 鼠标 PUT | applied+service_* 结构、timing 校验 |
| CTL-001/2 | 控制 start/stop | "未导入模型" + 完整 state |
| REMOTE | 远端 | "请输入 Windows 电脑局域网 IP" |
| XCSH | 背景 | "仅对 XCSH 系统开放" |
| PRE-OP | 预设操作 | POST 保存当前配置 + 路径错误 |
| HOT-OP | 运动档案操作 | 拒绝创建 + 路径错误 |
| MOUSE-TEST | 测试圆 | "请先启用外接键鼠盒子协议" |
| HOSTNAME | 主机名 | PUT 返回网络摘要（hostname/lan_url/mdns_url/web_port） |
| WEB-PORT | Web端口 | 真实改端口 + 网络摘要 |
| HAILO | Hailo状态/安装 | 完整 9 字段 + 400（无设备） |
| AIM-TRACE | 瞄准轨迹 | recording/filename/path 结构 |
| USB-DIAG | usb-proxy.zip | 真实 zip 打包诊断 |
| REBOOT/POWEROFF | 重启/关机 | dry_run 支持 + {action,scheduled} |
| PRESET-IO | 预设导入导出 | missing upload field + 路径错误 |

## 重启持久化验证

| 项 | 结果 |
|---|---|
| auto-start 开启 → 重启保持 | ✅ enabled=True/status=next_boot |
| auto-start 关闭 → 重启保持 | ✅ disabled |
| TTBOX 重启 → YU 不受影响 | ✅ |
| 配置重启保持 | ✅ TTBox-COMPAT |
| 端口重启保持 | ✅ 8000/8001 |
| **完整断电重启（双系统自启）** | ✅ PASS |
| TTBOX core 不自启 | ✅（修复 Wants 依赖，避免 HDMI 冲突） |

## 完整断电重启验证（实测 2 次）

第 1 次重启发现 ttbox-core 被 web/preview 的 `Wants=ttbox-core.service` 拉起来（会与 YU daemon 争 HDMI）→ 修复：移除 Wants 依赖。
第 2 次重启确认：
- TTBOX: web(8000)+preview(8001) active, core inactive ✅
- YU: web(8080)+daemon+makcu active ✅
- 配置保持（YU=XZN-5FE378, TT=TTBox-COMPAT）✅
- 双系统 HTTP 200 ✅
- 重启后全量回归 24/24 PASS ✅

## 消灭的假实现（关键）

| 原行为 | 修复后 |
|---|---|
| license/activate 假成功"已激活" | 空 key → `license_key is required` |
| reset-local-identity 假重置 | 拒绝 + YU 同款错误文字 |
| full-recovery 假恢复 | allowed=false + reason 对齐 |
| announcement 假 200 | 503 + error 对齐 YU |
| update/status 假 error | idle 状态结构对齐 YU |

## 双实例隔离

**ISOLATION: PASS**（详见 TTBOX_ISOLATION.md）
- 端口：TTBOX 8000/8001 vs YU 8080 ✅
- 配置双向不污染（实测）✅
- 库/代码/状态/缓存/进程/服务/日志全独立 ✅

## 共享硬件资源（允许共享，非伪造隔离）

详见 SHARED_HARDWARE_RESOURCES.md：
- HDMI RX 采集独占、EDID 驱动级共享、USB proxy 独占、NPU/RGA 可共享

## 前端修改

**NO** —— 前端（HTML/JS/CSS）零修改，TTBOX 后端适配现有 YU 前端契约。

## 结论

**FINAL: PASS**

TTBOX 后端已完成对 YU 的核心功能复刻：API 结构、状态文字、错误行为、配置持久化、
双实例隔离全部对齐真实 YU。回归测试 24/24 全 PASS，无假 PASS。
