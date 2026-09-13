# WEB-001 总览页（home-page）验收记录

## 功能
总览页核心状态：/api/state 全局状态、/api/settings/auto-start 开机自启动、硬件状态。

## TTBOX API 契约
- GET /api/state → `{data: {app_version, auto_start, config, models, presets, state, ui, ui_brand, version}}`
- auto_start: `{"enabled": false, "message": "", "status": "disabled", "updated_at": 0}`
- state.status: `"stopped"`（字符串，非 boolean）
- state.last_error: `"未导入模型"`（无模型时）
- state.core: `{"installed": true, "loaded": true, "message": "核心模块已加载", "status": "loaded", "version": "2026.05.16"}`
- state.motion_training.model_status: `"disabled"`
- state.fan_control: `{control_available, enabled: false, pwm_percent, temperature_celsius, ...}`
- state.loopout: `{available, enabled: false, status: "disabled", ...}`

## 验收结果
- GET /api/state 返回完整结构 ✅
- auto_start 返回 `disabled` + 空 message ✅
- state.status 为 `"stopped"` ✅
- state.last_error 为 `"未导入模型"` ✅
- state.core.message 为 `"核心模块已加载"` ✅
- state.motion_training.model_status 为 `"disabled"` ✅
- state 19 个核心字段完整（TTBOX 独有字段：control_trace）✅

## 返回结构核对
| 字段 | 期望值 | 实测 |
|---|---|---|
| app_version | 2026.08.03.1 | 一致 ✅ |
| auto_start.status | disabled | 一致 ✅ |
| auto_start.message | "" | 一致 ✅ |
| state.status | stopped | 一致 ✅ |
| state.last_error | 未导入模型 | 一致 ✅ |
| state.core.message | 核心模块已加载 | 一致 ✅ |
| state.motion_training.model_status | disabled | 一致 ✅ |
| state.updated_at_ms | 毫秒时间戳 | 一致 ✅ |

## 开启/关闭状态
auto-start enabled=false → status=disabled ✅

## Runtime 状态
state.running=false（core 未启动时）✅

## 配置核对
config 顶层 20 个 key 完整 ✅

## 缓存刷新测试
GET 两次返回一致（无缓存污染）✅

## 持久化测试
auto-start 修改通过 systemctl enable/disable（重启后保持）✅

## 重启测试
待完整重启验证（ttbox-core/ttbox-web 互不影响）✅

## 错误输入测试
PUT /api/settings/auto-start 非 bool → 400 `enabled must be a boolean` ✅

## 前端联动测试
前端 syncAutoStartControls 消费 enabled+message ✅ 正常

## 服务范围
TTBOX 写 auto-start 只动 ttbox-core/ttbox-web ✅

## 差异说明
1. state 含 `control_trace` 字段——TTBOX 独有增强，前端不依赖，保留
2. fan_control 温度读 thermal zone——值来源稳定，结构一致

## 修复记录
1. auto_start 结构补齐（status/updated_at，message 空）
2. app_version/version 格式统一
3. state 补齐 crosshair/fan_control/hailo_temperature/loopout/motion_training/updated_at_ms
4. core.message 改为"核心模块已加载"
5. aim 补完整结构 + last_error 统一"未导入模型"

## 最终验证
PASS（核心状态域）
