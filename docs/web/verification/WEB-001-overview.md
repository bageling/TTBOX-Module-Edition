# WEB-001 总览页（home-page）验收记录

## 功能
总览页核心状态：/api/state 全局状态、/api/settings/auto-start 开机自启动、硬件状态。

## YU 实际行为
- GET /api/state → `{data: {app_version, auto_start, config, models, presets, state, ui, ui_brand, version}}`
- auto_start: `{"enabled": false, "message": "", "status": "disabled", "updated_at": 0}`
- state.status: `"stopped"`（字符串，非 boolean）
- state.last_error: `"未导入模型"`（无模型时）
- state.core: `{"installed": true, "loaded": true, "message": "核心模块已加载", "status": "loaded", "version": "2026.05.16"}`
- state.motion_training.model_status: `"disabled"`
- state.fan_control: `{control_available, enabled: false, pwm_percent, temperature_celsius, ...}`
- state.loopout: `{available, enabled: false, status: "disabled", ...}`

## TTBOX API（修复后）
- GET /api/state → 同结构 ✅
- auto_start: `{"enabled": false, "message": "", "status": "disabled", "updated_at": 0}` ✅ 一致
- state.status: `"stopped"` ✅
- state.last_error: `"未导入模型"` ✅
- state.core.message: `"核心模块已加载"` ✅
- state.motion_training.model_status: `"disabled"` ✅
- state 19 个字段全对齐（仅 YU 缺失 = 无）

## 请求参数对比
GET 无参数。

## 返回结果对比
| 字段 | YU | TTBOX | 一致 |
|---|---|---|---|
| app_version | 2026.08.03.1 | 2026.08.03.1 | ✅ |
| auto_start.status | disabled | disabled | ✅ |
| auto_start.message | "" | "" | ✅ |
| state.status | stopped | stopped | ✅ |
| state.last_error | 未导入模型 | 未导入模型 | ✅ |
| state.core.message | 核心模块已加载 | 核心模块已加载 | ✅ |
| state.motion_training.model_status | disabled | disabled | ✅ |
| state.updated_at_ms | 毫秒时间戳 | 毫秒时间戳 | ✅ |

## 开启状态对比
auto-start enabled=false → status=disabled（与 YU 一致）✅

## 关闭状态对比
同上 ✅

## 状态文字对比
全部一致 ✅

## Runtime 对比
state.running: YU=false TTBOX=false（core 未启动时）✅

## 配置对比
config 顶层 20 个 key 完全一致 ✅

## 缓存刷新测试
GET 两次返回一致（无缓存污染）✅

## 持久化测试
auto-start 修改通过 systemctl enable/disable（重启后保持）✅

## 重启测试
待完整重启验证（YU/TTBOX 互不影响）

## 错误输入测试
PUT /api/settings/auto-start 非 bool → 400 `enabled must be a boolean`（对齐 YU）✅

## 前端联动测试
前端 syncAutoStartControls 消费 enabled+message ✅ 正常

## 双实例隔离测试
TTBOX 写 auto-start 只动 ttbox-core/ttbox-web，不碰 aiassistance 服务 ✅

## 差异
1. TTBOX state 多 `control_trace` 字段（YU 无）——TTBOX 独有增强，前端不依赖，保留
2. TTBOX fan_control 温度读 thermal zone（YU 读 hwmon8）——值来源不同但结构一致

## 修复
1. auto_start 结构对齐（补 status/updated_at，message 空）
2. app_version/version 格式对齐
3. state 补 crosshair/fan_control/hailo_temperature/loopout/motion_training/updated_at_ms
4. core.message 改为"核心模块已加载"
5. aim 补完整结构 + last_error 对齐"未导入模型"

## 最终验证
PASS（核心状态域）
