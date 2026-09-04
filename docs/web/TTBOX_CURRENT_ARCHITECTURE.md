# TTBOX 当前架构能力地图（真实盘点）

> 本文档基于 2026-09-04 板端（192.168.0.53）实测，只列真实存在且已接线的能力。
> 未实现的接口一律标注 [未接线]，不臆造。

## 1. 总体拓扑（实测）

```
浏览器
  ↓ HTTP
TTBOX Web 插件 (plugins/web, Flask, 0.0.0.0:8080)
  ├── 静态前端 (templates/ static/)
  ├── /api/*  (旧功能超集, IPC 直连 Core)
  └── /api/plugins/*  (Plugin Manager 控制面)
        ↓ Unix socket JSON+'\n'
TTBOX Core (ttbox-core.service, C++, IPC /tmp/ttbox_core.sock)
  ├── Capture (V4L2 HDMI RX, /dev/video0)
  ├── RGA → RKNN NPU → DecodeNMS
  ├── Preview (GET_PREVIEW IPC)
  └── Control (AimThread, fail-closed 门控)
        ↓ 0x4F50 MOVE_CMD
TTBOX USBProxy (ttbox-usbproxy.service, raw-gadget)
  └── HID → Windows
```

## 2. systemd 服务（实测 5 个 TTBOX 服务）

| 服务 | 状态 | 端口/接口 | 说明 |
|---|---|---|---|
| ttbox-core.service | enabled+active | IPC /tmp/ttbox_core.sock | C++ Runtime |
| ttbox-usbproxy.service | enabled+active | USB Gadget HID | 克隆 Logitech 046d:c53f |
| ttbox-edid.service | enabled+active | — | EDID oneshot |
| ttbox-web.service | enabled+active | 8080 | Web 插件（待切换） |
| ttbox-release-manager.service | enabled+active | 8090 | 发布管理 |

## 3. Core IPC 命令集（ttbox_web.py 实测引用）

- GET_STATUS / GET_CONFIG / SET_CONFIG / GET_PREVIEW
- MODEL_LIST / MODEL_ACTIVATE / MODEL_IMPORT / MODEL_INSTALL / MODEL_REMOVE / MODEL_VALIDATE
- RUNTIME_CONTROL

## 4. Plugin Manager（framework/plugin_manager, 实测存在）

**形态**：Python 库，无独立守护进程；由 Web 插件 `framework_api.py` 实例化并作为控制面暴露。生命周期启停插件进程（ProcessPluginRuntime，subprocess.Popen）。

**数据模型**（models.py）：
- PluginState: installed / disabled / enabled / running / stopped / failed / uninstalled / invalid
- PluginHealth: unknown / healthy / degraded / failed
- PluginRecord: plugin_id / version / plugin_type / path / enabled / state / autostart / health / permissions / dependencies / api_version / entry / installed_at / updated_at / error

**能力**（manager.py 实测）：
- 发现：discover()（扫描插件目录 + 注册表）
- 生命周期：start / stop / restart / enable / disable
- 安装：install（.tpk 本地/URL/仓库源，完整性校验 SHA256 + 签名）
- 升级/回滚：upgrade / rollback（事务日志 .transaction.json）
- 卸载：uninstall（依赖检查）
- 市场：search_plugins / get_market_plugin / get_market_versions（LocalRepository 本地模拟）
- 安全：SecurityPolicy 权限声明与检查（framework/security/policy.py）
- 服务编排：FrameworkRuntime（framework/runtime.py）start/stop core 服务

## 5. 插件清单（板端 /opt/ttbox/plugins, 12 个目录 10 个有效插件）

| 插件 | 类型 | autostart | 入口 | 真实能力 |
|---|---|---|---|---|
| web | process | ✅ | bin/ttbox-web → ttbox-web.py | Flask Web 控制面板 |
| preview | process | ✅ | bin/ttbox-preview → ttbox-preview.py | 低帧 JPEG/MJPEG 预览 8082 |
| model | process | ✅ | bin/ttbox-model → ttbox-model.py | 模型管理（upload/validate/install/activate） |
| system | process | ✅ | bin/ttbox-system → plugin_entry.py | 系统状态（hostname/network） |
| upgrade | process | ✅ | bin/ttbox-upgrade → plugin_entry.py | 升级状态（update_engine） |
| fan | process | ❌ | bin/ttbox-fan → plugin_entry.py | 风扇/温度状态（/sys 只读） |
| network | process | ❌ | bin/ttbox-network → plugin_entry.py | 网络状态（hostname -I） |
| wifi | process | ❌ | bin/ttbox-wifi → plugin_entry.py | Wi-Fi 状态/扫描/连接（nmcli） |
| log | builtin | ❌ | — | 日志读取（/var/log/ttbox） |
| monitor | builtin | ❌ | — | CPU/内存/磁盘/温度（/proc /sys） |

无效条目（registry 中 invalid）：`__pycache__`、`repository`（非插件目录，需清理）。

## 6. 插件注册表现状（2026-09-04 实测）

所有插件 state=installed / health=unknown，**无一运行**——Plugin Manager 未执行过 start_all，插件进程全部未拉起（ps 无 ttbox-* 插件进程）。web/preview/model/system/upgrade 声明 autostart=true 但未生效。

## 7. 核心 Runtime 能力（GET_STATUS 实测字段）

- Capture: capture_fps≈141（2560×1440 HDMI /dev/video0）
- Detection: fps≈42、infer_ms≈65、decode_ms≈5.7、e2e_ms≈68
- Detect: detect_count、detection_boxes[]、no_target_frames、gated_frames
- Aim: aim_active、aim_has_target、aim_target_class_id、aim_target_id、aim_error_x/y、aim_pos_x/y
- Mouse 控制: mouse_control_connected、mouse_control_send_count、mouse_control_socket_write_ok/fail、last_mouse_control_dx/dy/wheel
- PID: pid_output_x/y、last_mouse_control_*
- Preview: preview_fps≈15、preview_bytes、preview_encode_ms、preview_dropped
- 安全: injection_allowed（false）、output_enabled（config false）
- 工程指标: buffer_count、buffer_age_ms、dropped_frames、frames_total、e2e_p50/p95/p99

## 8. Web / API 层（plugins/web, Flask）

**页面路由**：`/`（控制台）、`/plugins`（插件管理）、`/activate`、`/motion_training`
**静态**：style.css / app.js / apiClient.js / motion_training.js / update.js / activate.js
**API 分组**（ttbox-web.py 实测）：
- 系统: /api/health/frontend、/api/system、/api/system/version、/api/system/storage、/api/system/reboot、/api/system/poweroff、/api/system/hostname、/api/system/web-port
- 状态: /api/state（合成完整状态含 config/status/models/presets）
- 配置: GET/PUT /api/config、/api/settings/auto-start
- 模型: /api/models、/api/models/import、/api/models/device-code、/api/models/cloud-encrypted
- 预设: /api/presets/<name>/export
- 事件: /api/events（SSE）
- 预览: /api/preview.jpg、/api/preview.mjpg
- 运动: /api/motion-profiles/*、/api/motion-sample 等
- 网络/WiFi: /api/network/*、/api/wifi/*（nmcli 适配器）
- 升级: /api/update/*（update_engine）
- 热键: /api/hotkey 相关
- Plugin Manager 控制面（framework_api.py）: 见第 9 节

## 9. Plugin Manager API 清单（framework_api.py 实测）

| 端点 | 方法 | 说明 |
|---|---|---|
| /plugins | GET | 插件管理页面（plugins.html） |
| /api/plugins | GET | 插件列表（discover+registry） |
| /api/plugins/<id> | GET | 插件详情 |
| /api/plugins/<id>/status | GET | 插件状态 |
| /api/plugins/<id>/{enable,disable,start,stop,restart,rollback,uninstall} | POST | 生命周期动作 |
| /api/plugins/install | POST | 安装（local_file/url/repository） |
| /api/plugins/upgrade | POST | 升级 |
| /api/plugins/market | GET | 插件市场（本地模拟仓库） |
| /api/plugins/market/<id> | GET | 市场详情 |
| /api/plugins/market/<id>/versions | GET | 市场版本 |
| /api/{system,network,wifi,fan,monitor,upgrade}/status | GET | 系统插件状态（SystemPluginHost 适配器） |
| /api/hwmon | GET | 硬件监控（monitor 插件） |
| /api/core/status | GET | ⚠️ 假端点：硬编码 available:true，未接 IPC |
| /api/model/list、/api/model/active | GET | ⚠️ 假端点：硬编码，未接真实模型 |

## 10. 模型系统（实测）

- 配置: config model_path=/opt/ttbox/models/yolo261n-rk3588/yolo261n-rk3588.rknn（实际推理）
- registry/active.json: `{"activated_at":..., "model_id":"jwdl_sjzv11"}`（Web 显示标签源）
- installed/: jwdl_sjzv11（manifest.json + metadata.json + model.rknn + validation/ok.json）
- 接口: MODEL_LIST / MODEL_ACTIVATE / MODEL_IMPORT / MODEL_INSTALL / MODEL_REMOVE / MODEL_VALIDATE（Core IPC）
- 已知问题: 页面显示模型 jwdl_sjzv11 与 config 实际推理模型 yolo261n-rk3588 不一致（标签 vs 实载分离）

## 11. 硬件/系统适配器（plugins/system_common.py 实测）

- FanService: /sys/class/thermal + hwmon pwm 温度读取（set_mode 未配置）
- WifiService: nmcli device/scan/connect
- NetworkService: hostname + hostname -I
- MonitorService: /proc/stat、/proc/meminfo、磁盘、温度
- SystemService: hostnamectl
- LogService: /var/log/ttbox/ttbox.log
- UpgradeService: update_engine（status 返回 state=unavailable, source=update_engine）

## 12. 安全状态（fail-closed 实测）

- config: output_enabled=false、mouse.enabled=false、mouse.calibrating=false
- runtime: injection_allowed=false、aim_active=false、mouse_control_send_count=0
- 恢复服务/启停插件不得触碰这些门控

## 13. 本阶段修复记录（2026-09-04）

1. ✅ ttbox-web.service 切换到插件 Web（/opt/ttbox/plugins/web/bin/ttbox-web，0.0.0.0:8080），旧 Web（/opt/ttbox/web 8081）已停用
2. ✅ framework_api 假端点修复：/api/core/status 接真实 GET_STATUS IPC；/api/model/list、/api/model/active 接真实 MODEL_LIST IPC
3. ✅ 插件 registry 清理：移除 __pycache__/repository 无效条目；discovery 跳过 __*__/repository 目录；manager.discover() 同步删除磁盘已不存在的记录
4. ✅ 新增 ttbox-preview.service（8082）并启用；web/preview 插件在 /api/plugins 中实时显示 running/healthy（HTTP 探活）
5. ✅ 插件启停真实可用（fan start→running/healthy，stop→stopped，进程级验证）
