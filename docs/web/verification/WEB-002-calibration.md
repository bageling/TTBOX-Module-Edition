# WEB-002 校准/控制域验收记录

## 功能
自动校准状态：GET/PUT /api/control/calibration（runtime + calibration 结构）。

## TTBOX API 契约
- GET /api/control/calibration → `{data: {calibration, runtime}}`
- runtime: `{phase: "idle", status: "idle", reason: "not_running", running: false, ready: false, progress: 0.0, total_rounds: 10, candidate_rect: {x,y,width,height}, ...}`
- calibration（空）: `{valid: false, gain_x_px_per_count: 0.550000011920929, gain_y: 同, response_delay_ms: 8.333000183105469, confidence: 0.0, model_id: "", calibrated_at: "", capture_width: 0, capture_height: 0, crop_size: 0}`

## 验收结果
- 结构完整 ✅
- runtime 含 candidate_rect ✅
- 未运行时 reason = "not_running" ✅
- calibration 空时全 10 字段 + float32 精度 ✅

## 返回结构核对
| 字段 | 期望值 | 实测 |
|---|---|---|
| runtime.phase | idle | 一致 ✅ |
| runtime.status | idle | 一致 ✅ |
| runtime.reason | not_running | 一致 ✅ |
| runtime.candidate_rect | {0,0,0,0} | 一致 ✅ |
| calibration.gain_x | 0.550000011920929 | 一致 ✅ |
| calibration.response_delay_ms | 8.333000183105469 | 一致 ✅ |
| calibration.valid | false | 一致 ✅ |

## 差异说明
1. runtime 多 candidate_width/height/current_axis/valid_sample_count/state/axis_fits（TTBOX 独有增强，前端不依赖）
2. calibration 非空时的 capture_height 从 crop_size 映射（TTBOX 统一数据源）

## 修复
1. reason 初始值 idle → not_running
2. runtime 补 candidate_rect
3. calibration 空时补全 10 字段
4. 数值精度统一 float32

## 最终验证
PASS
