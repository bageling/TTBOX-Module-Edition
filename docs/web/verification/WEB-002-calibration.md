# WEB-002 校准/控制域验收记录

## 功能
自动校准状态：GET/PUT /api/control/calibration（runtime + calibration 结构）。

## YU 实际行为
- GET /api/control/calibration → `{data: {calibration, runtime}}`
- runtime: `{phase: "idle", status: "idle", reason: "not_running", running: false, ready: false, progress: 0.0, total_rounds: 10, candidate_rect: {x,y,width,height}, ...}`
- calibration（空）: `{valid: false, gain_x_px_per_count: 0.550000011920929, gain_y: 同, response_delay_ms: 8.333000183105469, confidence: 0.0, model_id: "", calibrated_at: "", capture_width: 0, capture_height: 0, crop_size: 0}`

## TTBOX API（修复后）
- 结构完全一致 ✅
- runtime 补 candidate_rect ✅
- reason 未运行时 = "not_running" ✅
- calibration 空时全 10 字段 + float32 精度 ✅

## 返回结果对比
| 字段 | YU | TTBOX | 一致 |
|---|---|---|---|
| runtime.phase | idle | idle | ✅ |
| runtime.status | idle | idle | ✅ |
| runtime.reason | not_running | not_running | ✅ |
| runtime.candidate_rect | {0,0,0,0} | {0,0,0,0} | ✅ |
| calibration.gain_x | 0.550000011920929 | 0.550000011920929 | ✅ |
| calibration.response_delay_ms | 8.333000183105469 | 8.333000183105469 | ✅ |
| calibration.valid | false | false | ✅ |

## 状态文字对比
reason: "not_running"（未运行时）✅

## 差异
1. TTBOX runtime 多 candidate_width/height/current_axis/valid_sample_count/state/axis_fits（独有增强，前端不依赖）
2. calibration 非空时的 capture_height（TTBOX 从 crop_size 映射，YU 独立存储）

## 修复
1. reason 初始值 idle → not_running
2. runtime 补 candidate_rect
3. calibration 空时补全 10 字段
4. 数值精度对齐 float32

## 最终验证
PASS
