# TTBOX Web 功能矩阵（Web → 插件 → API → Runtime 映射）

> 实测基准：2026-09-04 板端。状态：✅存在已接通 / ⚠️存在未接通 / ❌不存在

## 1. 核心链路

| Web 功能 | 对应插件 | 对应 API | 底层 Runtime | 真实存在 | 已接通 | 真机测试 |
|---|---|---|---|---|---|---|
| 控制台首页 | web | GET /api/state | Core GET_STATUS/GET_CONFIG/MODEL_LIST | ✅ | ✅ | PASS |
| 实时预览 | web(+preview) | GET /api/preview.mjpg | Core GET_PREVIEW | ✅ | ✅ | PASS |
| 插件管理页 | web | GET /plugins | PluginManager | ✅ | ✅ | PASS |
| 插件列表 | web | GET /api/plugins | PluginManager.discover+registry | ✅ | ✅ | PASS |
| 插件详情 | web | GET /api/plugins/<id> | PluginManager.registry | ✅ | ✅ | PASS |
| 插件启停 | web | POST /api/plugins/<id>/{start,stop,restart} | ProcessPluginRuntime | ✅ | ⚠️ 待验证 | — |
| 插件禁用/启用 | web | POST /api/plugins/<id>/{disable,enable} | PluginManager.registry | ✅ | ✅ | PASS |
| 插件健康 | web | GET /api/plugins/<id>/status | PluginManager.status | ✅ | ⚠️ 依赖插件进程 | — |
| 插件安装 | web | POST /api/plugins/install | PluginManager.install(.tpk) | ✅ | ⚠️ 未测 | — |
| 插件市场 | web | GET /api/plugins/market | LocalRepository | ✅ | ✅ | PASS |

## 2. AI / 模型

| Web 功能 | 对应插件 | 对应 API | 底层 Runtime | 真实存在 | 已接通 | 真机测试 |
|---|---|---|---|---|---|---|
| AI 开关/热键 | web | PUT /api/config | SET_CONFIG→AimThread | ✅ | ✅ | PASS |
| 模型列表 | web | GET /api/models | Core MODEL_LIST | ✅ | ✅ | PASS |
| 模型导入 | web | POST /api/models/import | MODEL_IMPORT/INSTALL | ✅ | ⚠️ 未测 | — |
| 模型激活 | web | POST /api/models/*/activate | MODEL_ACTIVATE | ✅ | ✅ | PASS |
| 检测状态 | web | GET /api/state→detect_count | GET_STATUS | ✅ | ✅ | PASS |
| 目标锁定 | web | GET /api/state→aim_* | GET_STATUS | ✅ | ✅ | PASS |
| /api/model/list（framework stub） | model | GET /api/model/list | — | ⚠️ 假端点 | ❌ | 需修复 |

## 3. 移动控制

| Web 功能 | 对应插件 | 对应 API | 底层 Runtime | 真实存在 | 已接通 | 真机测试 |
|---|---|---|---|---|---|---|
| PID 参数 | web | PUT /api/config→runtime_profile | SET_CONFIG→Pid1Controller | ✅ | ✅ | PASS |
| 灵敏度 | web | GET/PUT /api/config | RuntimeProfile | ✅ | ✅ | PASS |
| 热键配置 | web | PUT /api/config→aim_keys | SET_CONFIG | ✅ | ✅ | PASS |
| 运动配置 | web | GET/PUT /api/config | RuntimeProfile | ✅ | ✅ | PASS |
| 运动数据采集 | web | /api/motion-sample/* | Core 运动采样 | ✅ | ⚠️ 未测 | — |

## 4. 系统 / 硬件 / 网络

| Web 功能 | 对应插件 | 对应 API | 底层 Runtime | 真实存在 | 已接通 | 真机测试 |
|---|---|---|---|---|---|---|
| 系统版本 | web | GET /api/system/version | — | ✅ | ✅ | PASS |
| 系统状态 | system | GET /api/system/status | SystemService | ✅ | ✅ | PASS |
| 网络状态 | network | GET /api/network/status | NetworkService | ✅ | ✅ | PASS |
| WiFi 状态 | wifi | GET /api/wifi/status | WifiService | ✅ | ✅ | PASS |
| 硬件监控 | monitor | GET /api/hwmon | MonitorService | ✅ | ✅ | PASS |
| 风扇状态 | fan | GET /api/fan/status | FanService | ✅ | ✅ | PASS |
| 存储/扩容 | web | GET/POST /api/system/storage | 磁盘适配器 | ✅ | ✅ | PASS |
| 重启/关机 | web | POST /api/system/{reboot,poweroff} | systemctl | ✅ | ⚠️ 高危未测 | — |
| /api/core/status（framework stub） | core | GET /api/core/status | — | ⚠️ 假端点 | ❌ | 需修复 |
| /api/model/active（framework stub） | model | GET /api/model/active | — | ⚠️ 假端点 | ❌ | 需修复 |

## 5. 旧 Web 已过时项

| 旧项 | 状态 | 处置 |
|---|---|---|
| 固定菜单（无插件入口） | ❌ 过时 | 新首页含插件系统状态入口 |
| 无 Plugin Manager 控制面 | ❌ 过时 | 新 /plugins 页面 |
| 8081 端口（/opt/ttbox/web） | ❌ 过时 | 切换 8080 插件 Web |
| /api/state 中"来源YU"残留 | ⚠️ | 文案已中性化，功能保留 |

## 6. 完成标准对照

- 浏览器 → TTBOX Web(8080) → /api/* → IPC → Core：✅ 已跑通
- 浏览器 → /plugins → /api/plugins → PluginManager → 插件进程：⚠️ 本阶段接通
- fail-closed 保持：✅ output_enabled=false / injection_allowed=false / send_count=0
