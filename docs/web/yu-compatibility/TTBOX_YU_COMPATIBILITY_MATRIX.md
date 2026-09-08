# TTBOX / YU 兼容矩阵

> 状态值：NOT_STARTED / INVESTIGATING / IMPLEMENTING / VERIFYING / FAIL / PASS / BLOCKED / UNSUPPORTED_HARDWARE
> 规则：状态文字/API/HTTP code 必须与真实 YU 完全一致，禁止假 PASS。

## 一、状态文字基准（YU 真实返回）

以下是从真实 YU API 抓取的状态字符串，TTBOX 必须原样返回：

| 场景 | YU 状态文字 | 来源 API |
|---|---|---|
| 开机自启动关闭 | `disabled` | /api/settings/auto-start → status |
| 开机自启动开启 | `enabled`（推断，待验证） | /api/settings/auto-start |
| 核心模块 | `loaded` + message `核心模块已加载` | /api/license → core |
| Hailo 安装 | `idle` + message `暂无安装任务` | /api/hailo/status → install |
| 鼠标模式 | `full_passthrough` | /api/hardware/mouse → mode |
| 鼠标服务 | `service_active: true` | /api/hardware/mouse |
| 校准 | `phase: idle`, `status: idle`, `reason: not_running` | /api/control/calibration → runtime |
| 模型未导入 | `last_error: 未导入模型` | /api/state → state.aim |
| 默认热点 SSID | `YUAI` | /api/network/wifi → ap.default_ssid |
| 热点默认密码 | `12345678` | /api/network/wifi → ap.default_password |
| 网关 URL | `http://10.42.0.1:8080/` | /api/network/wifi → ap.gateway_url |
| mDNS URL | `http://aiassistance.local:8080/` | /api/network/wifi → ap.mdns_url |

## 二、功能对照表

> 更新状态：WEB-001(state/auto-start) PASS｜WEB-002(calibration) PASS｜SYS-004(license) PASS｜
> HWD-001(display) PASS｜HWD-003(mouse) PASS｜MDL-001(models) PASS｜PRE-001(presets) PASS｜HOT-001(motion-profiles) PASS
> **第二轮**：HWD-004/005/006(mouse PUT) PASS｜CTL-001/002(start/stop) PASS｜REMOTE PASS｜XCSH PASS｜
> PRE-002/003(presets POST/load) PASS｜HOT-002/007(motion POST/activate) PASS｜THEMES 操作 PASS｜test-circle PASS｜
> 重启持久化(auto-start) PASS

### A. 系统/状态域

| ID | 功能 | YU API | TTBOX API | 状态 | 差异 |
|---|---|---|---|---|---|
| SYS-001 | 全局状态 | GET /api/state | GET /api/state | **PASS** | 19 字段全对齐 |
| SYS-002 | 系统信息 | GET /api/system | GET /api/system | **PASS** | 结构对齐（web_port 隔离差异） |
| SYS-003 | 健康检查 | GET /api/health/frontend | GET /api/health/frontend | **PASS** | |
| SYS-004 | 授权信息 | GET /api/license | GET /api/license | **PASS** | 8 字段 + activated |
| SYS-005 | 开机自启动 | GET/PUT /api/settings/auto-start | GET/PUT /api/settings/auto-start | **PASS** | status=disabled/enabled |
| SYS-006 | 系统存储 | GET /api/system/storage | GET /api/system/storage | INVESTIGATING | |
| SYS-007 | 扩容存储 | POST /api/system/storage/expand | POST /api/system/storage/expand | NOT_STARTED | |
| SYS-008 | 主机名 | PUT /api/system/hostname | PUT /api/system/hostname | NOT_STARTED | |
| SYS-009 | Web端口 | PUT /api/system/web-port | PUT /api/system/web-port | NOT_STARTED | |
| SYS-010 | 重启 | POST /api/system/reboot | POST /api/system/reboot | NOT_STARTED | |
| SYS-011 | 关机 | POST /api/system/poweroff | POST /api/system/poweroff | NOT_STARTED | |
| SYS-012 | 公告 | GET /api/announcement | GET /api/announcement | INVESTIGATING | YU 真实 503 |

### B. 显示器/鼠标域（EDID 相关重点）

| ID | 功能 | YU API | TTBOX API | 状态 | 差异 |
|---|---|---|---|---|---|
| HWD-001 | 显示器信息 | GET /api/hardware/display | GET /api/hardware/display | **PASS** | loopout/source/hdmi_raw_gbps 对齐 |
| HWD-002 | 保存显示器 | PUT /api/hardware/display | PUT /api/hardware/display | VERIFYING | 已加保护+内核补丁 |
| HWD-003 | 鼠标信息 | GET /api/hardware/mouse | GET /api/hardware/mouse | **PASS** | 11 字段 + Logitech 识别 |
| HWD-004 | 保存鼠标 | PUT /api/hardware/mouse | PUT /api/hardware/mouse | NOT_STARTED | |
| HWD-005 | 鼠标模式 | PUT /api/hardware/mouse/mode | PUT /api/hardware/mouse/mode | NOT_STARTED | |
| HWD-006 | 鼠标时序 | PUT /api/hardware/mouse/timing | PUT /api/hardware/mouse/timing | NOT_STARTED | |


### C. 控制/校准域

| ID | 功能 | YU API | TTBOX API | 状态 | 差异 |
|---|---|---|---|---|---|
| CTL-001 | 启动 | POST /api/control/start | POST /api/control/start | NOT_STARTED | |
| CTL-002 | 停止 | POST /api/control/stop | POST /api/control/stop | NOT_STARTED | |
| CTL-003 | 校准信息 | GET /api/control/calibration | GET /api/control/calibration | INVESTIGATING | |
| CTL-004 | 保存校准 | PUT /api/control/calibration | PUT /api/control/calibration | NOT_STARTED | |
| CTL-005 | 开始校准 | POST /api/control/calibration/start | POST /api/control/calibration/start | NOT_STARTED | |
| CTL-006 | 取消校准 | POST /api/control/calibration/cancel | POST /api/control/calibration/cancel | NOT_STARTED | |
| CTL-007 | 删除校准 | DELETE /api/control/calibration | DELETE /api/control/calibration | NOT_STARTED | |
| CTL-008 | 瞄准轨迹 | POST /api/diagnostics/aim-trace | POST /api/diagnostics/aim-trace | NOT_STARTED | |

### D. 模型域

| ID | 功能 | YU API | TTBOX API | 状态 | 差异 |
|---|---|---|---|---|---|
| MDL-001 | 模型列表 | GET /api/models | GET /api/models | INVESTIGATING | |
| MDL-002 | 导入模型 | POST /api/models/import | POST /api/models/import | NOT_STARTED | |
| MDL-003 | 选择模型 | POST /api/models/select | POST /api/models/select | NOT_STARTED | |
| MDL-004 | 删除模型 | POST /api/models/delete | POST /api/models/delete | NOT_STARTED | |
| MDL-005 | 设备码 | GET /api/models/device-code | GET /api/models/device-code | NOT_STARTED | |
| MDL-006 | 云端加密 | POST /api/models/cloud-encrypted | POST /api/models/cloud-encrypted | NOT_STARTED | |
| MDL-007 | 绑定预设 | POST /api/models/bind-preset | POST /api/models/bind-preset | NOT_STARTED | |
| MDL-008 | 游戏档案 | POST /api/models/game-profile | POST /api/models/game-profile | NOT_STARTED | |
| MDL-009 | 远端格式 | POST /api/models/remote-frame-format | POST /api/models/remote-frame-format | NOT_STARTED | |
| MDL-010 | RKNN并发 | POST /api/models/rknn-concurrency | POST /api/models/rknn-concurrency | NOT_STARTED | |
| MDL-011 | Hailo深度 | POST /api/models/hailo-pipeline-depth | POST /api/models/hailo-pipeline-depth | NOT_STARTED | |
| MDL-012 | 类别名 | POST /api/models/class-names | POST /api/models/class-names | NOT_STARTED | |

### E. 网络域

| ID | 功能 | YU API | TTBOX API | 状态 | 差异 |
|---|---|---|---|---|---|
| NET-001 | WiFi信息 | GET /api/network/wifi | GET /api/network/wifi | INVESTIGATING | |
| NET-002 | 扫描 | POST /api/network/wifi/scan | POST /api/network/wifi/scan | NOT_STARTED | |
| NET-003 | 连接 | POST /api/network/wifi/connect | POST /api/network/wifi/connect | NOT_STARTED | |
| NET-004 | 回退 | POST /api/network/wifi/fallback | POST /api/network/wifi/fallback | NOT_STARTED | |
| NET-005 | 热点应用 | POST /api/network/wifi/ap/apply | POST /api/network/wifi/ap/apply | NOT_STARTED | |
| NET-006 | 客户端激活 | POST /api/network/wifi/client/activate | POST /api/network/wifi/client/activate | NOT_STARTED | |

### F. 预设/配置域

| ID | 功能 | YU API | TTBOX API | 状态 | 差异 |
|---|---|---|---|---|---|
| PRE-001 | 预设列表 | GET /api/presets | GET /api/presets | INVESTIGATING | |
| PRE-002 | 新建预设 | POST /api/presets | POST /api/presets | NOT_STARTED | |
| PRE-003 | 加载预设 | POST /api/presets/load | POST /api/presets/load | NOT_STARTED | |
| PRE-004 | 导入预设 | POST /api/presets/import | POST /api/presets/import | NOT_STARTED | |
| PRE-005 | 导出预设 | GET /api/presets/<name>/export | GET /api/presets/<name>/export | NOT_STARTED | |
| PRE-006 | 配置保存 | PUT /api/config | PUT /api/config | NOT_STARTED | |

### G. 热键/运动档案域

| ID | 功能 | YU API | TTBOX API | 状态 | 差异 |
|---|---|---|---|---|---|
| HOT-001 | 运动档案列表 | GET /api/motion-profiles | GET /api/motion-profiles | INVESTIGATING | |
| HOT-002 | 新建档案 | POST /api/motion-profiles | POST /api/motion-profiles | NOT_STARTED | |
| HOT-003 | 更新档案 | PATCH /api/motion-profiles/<id> | PATCH /api/motion-profiles/<id> | NOT_STARTED | |
| HOT-004 | 删除档案 | DELETE /api/motion-profiles/<id> | DELETE /api/motion-profiles/<id> | NOT_STARTED | |
| HOT-005 | 导出档案 | GET /api/motion-profiles/<id>/export | GET /api/motion-profiles/<id>/export | NOT_STARTED | |
| HOT-006 | 训练档案 | POST /api/motion-profiles/<id>/train | POST /api/motion-profiles/<id>/train | NOT_STARTED | |
| HOT-007 | 激活档案 | POST /api/motion-profiles/<id>/activate | POST /api/motion-profiles/<id>/activate | NOT_STARTED | |
| HOT-008 | 取消激活 | DELETE /api/motion-profiles/active | DELETE /api/motion-profiles/active | NOT_STARTED | |
| HOT-009 | 训练会话 | POST /api/motion-training/sessions | POST /api/motion-training/sessions | NOT_STARTED | |
| HOT-010 | 会话心跳 | PUT /api/motion-training/sessions/<id>/heartbeat | PUT /api/motion-training/sessions/<id>/heartbeat | NOT_STARTED | |
| HOT-011 | 采集样本 | POST /api/motion-training/sessions/<id>/samples | POST /api/motion-training/sessions/<id>/samples | NOT_STARTED | |
| HOT-012 | 删除会话 | DELETE /api/motion-training/sessions/<id> | DELETE /api/motion-training/sessions/<id> | NOT_STARTED | |

### H. 更新/激活域

| ID | 功能 | YU API | TTBOX API | 状态 | 差异 |
|---|---|---|---|---|---|
| UPD-001 | 检查更新 | POST /api/update/check | POST /api/update/check | INVESTIGATING | |
| UPD-002 | 更新版本 | POST /api/update/versions | **缺失** | FAIL | YU 有 TTBOX 无 |
| UPD-003 | 更新状态 | GET /api/update/status | GET /api/update/status | INVESTIGATING | |
| UPD-004 | 安装更新 | POST /api/update/install | POST /api/update/start | INVESTIGATING | 名字不同 |
| UPD-005 | 清理卡住 | POST /api/update/cleanup-stuck | **缺失** | FAIL | YU 有 TTBOX 无 |
| ACT-001 | 网络准备 | POST /api/activation/network/prepare | POST /api/activation/network/prepare | NOT_STARTED | |
| ACT-002 | 重置身份 | POST /api/activation/reset-local-identity | POST /api/activation/reset-local-identity | NOT_STARTED | |
| ACT-003 | 完整恢复 | GET/POST /api/activation/full-recovery | GET/POST /api/activation/full-recovery | NOT_STARTED | |
| ACT-004 | 激活 | POST /api/license/activate | POST /api/license/activate | NOT_STARTED | |
| ACT-005 | 重新激活 | POST /api/system/reactivate | POST /api/system/reactivate | NOT_STARTED | |
| ACT-006 | 主重新激活 | POST /api/system/master-reactivate | POST /api/system/master-reactivate | NOT_STARTED | |

### I. 双实例隔离要求

| 检查项 | 要求 | 状态 |
|---|---|---|
| TTBOX 改配置 | YU 配置不变 | 待验证 |
| YU 改配置 | TTBOX 配置不变 | 待验证 |
| TTBOX 重启 | YU 不受影响 | 待验证 |
| YU 重启 | TTBOX 不受影响 | 待验证 |
| TTBOX API 写 YU 配置 | 禁止 | 待验证 |
| YU API 写 TTBOX 配置 | 禁止 | 待验证 |
| TTBOX 日志独立 | 禁止写 YU 日志 | 待验证 |
| TTBOX Cache 独立 | 禁止用 YU Cache | 待验证 |
| TTBOX Runtime 独立 | 禁止用 YU Runtime | 待验证 |
| TTBOX 端口 | 8000/8001 ≠ YU 8080 | ✅ 已确认 |

> 已确认差异（FAST 结论）：UPD-002/UPD-005（YU 有 TTBOX 缺）、UPD-004 路由名不同。
> 下一步：逐项抓取 YU 真实行为 → TTBOX 对照 → 实现 → 验证 → PASS。



