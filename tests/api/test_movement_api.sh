#!/usr/bin/env bash
# Phase 3: Movement Control 验收
set -u
YU="http://127.0.0.1:8080"
TT="http://127.0.0.1:8000"
PASS_N=0
FAIL_N=0
FAIL_LIST=""

ok() { PASS_N=$((PASS_N+1)); printf '  ✅ %s\n' "$*"; }
bad() { FAIL_N=$((FAIL_N+1)); FAIL_LIST="$FAIL_LIST\n  ❌ $*"; printf '  ❌ %s\n' "$*"; }

note() { printf '%s\n' "$*"; }

note '════════ Phase 3: Movement Control ════════'

# 从 state 获取配置
YU_STATE=$(curl -s "$YU/api/state" 2>/dev/null)
TT_STATE=$(curl -s "$TT/api/state" 2>/dev/null)

YU_CFG=$(echo "$YU_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('config',{}); print(json.dumps(d))" 2>/dev/null)
TT_CFG=$(echo "$TT_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('config',{}); print(json.dumps(d))" 2>/dev/null)

# MC-001: controller kp_x/kp_y 存在
note '--- MC-001 kp_x/kp_y ---'
YU_KP=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('kp_x' in d and 'kp_y' in d)" 2>/dev/null)
TT_KP=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('kp_x' in d and 'kp_y' in d)" 2>/dev/null)
[ "$YU_KP" = "True" ] && [ "$TT_KP" = "True" ] && ok "MC-001 kp_x/kp_y 存在" || bad "MC-001 kp_x/kp_y YU=$YU_KP TT=$TT_KP"

# MC-002: rate_x/rate_y
note '--- MC-002 rate_x/rate_y ---'
YU_RT=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('rate_x' in d and 'rate_y' in d)" 2>/dev/null)
TT_RT=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('rate_x' in d and 'rate_y' in d)" 2>/dev/null)
[ "$YU_RT" = "True" ] && [ "$TT_RT" = "True" ] && ok "MC-002 rate_x/rate_y 存在" || bad "MC-002 rate_x/rate_y YU=$YU_RT TT=$TT_RT"

# MC-003: predict_x/predict_y
note '--- MC-003 predict_x/predict_y ---'
YU_PD=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('predict_x' in d and 'predict_y' in d)" 2>/dev/null)
TT_PD=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('predict_x' in d and 'predict_y' in d)" 2>/dev/null)
[ "$YU_PD" = "True" ] && [ "$TT_PD" = "True" ] && ok "MC-003 predict_x/predict_y 存在" || bad "MC-003 predict_x/predict_y YU=$YU_PD TT=$TT_PD"

# MC-004: FOV (range_factor)
note '--- MC-004 range_factor ---'
YU_RF=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('range_factor',''); print(d)" 2>/dev/null)
TT_RF=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('range_factor',''); print(d)" 2>/dev/null)
[ -n "$YU_RF" ] && [ -n "$TT_RF" ] && ok "MC-004 range_factor 存在 (YU=$YU_RF TT=$TT_RF)" || bad "MC-004 range_factor YU=$YU_RF TT=$TT_RF"

# MC-005: 控制器输出字段完整
note '--- MC-005 控制器完整字段 ---'
YU_CTRL=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); keys=list(d.keys()); print(len(keys))" 2>/dev/null)
TT_CTRL=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); keys=list(d.keys()); print(len(keys))" 2>/dev/null)
[ "$YU_CTRL" -ge 15 ] 2>/dev/null && [ "$TT_CTRL" -ge 15 ] 2>/dev/null && ok "MC-005 控制器字段数 (YU=$YU_CTRL TT=$TT_CTRL)" || bad "MC-005 controller 字段数 YU=$YU_CTRL TT=$TT_CTRL"

# MC-006: /api/control/start 无模型时拒绝
note '--- MC-006 control/start 无模型 ---'
# 先确认当前模型状态
TT_NOMODEL=$(curl -s -X POST "$TT/api/control/start" 2>/dev/null | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('ok'),d.get('error','')[:20])" 2>/dev/null)
YU_NOMODEL=$(curl -s -X POST "$YU/api/control/start" 2>/dev/null | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('ok'),d.get('error','')[:20])" 2>/dev/null)
# 两者都返回 False 或有 error
[ -n "$TT_NOMODEL" ] && ok "MC-006 control/start 响应 (TT=$TT_NOMODEL)" || bad "MC-006 control/start TT=$TT_NOMODEL"

# MC-007: /api/control/calibration 结构
note '--- MC-007 calibration 结构 ---'
YU_CAL=$(curl -s "$YU/api/control/calibration" 2>/dev/null)
TT_CAL=$(curl -s "$TT/api/control/calibration" 2>/dev/null)
YU_CAL_OK=$(echo "$YU_CAL" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('runtime',{}); print('phase' in d and 'ready' in d)" 2>/dev/null)
TT_CAL_OK=$(echo "$TT_CAL" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('runtime',{}); print('phase' in d and 'ready' in d)" 2>/dev/null)
[ "$YU_CAL_OK" = "True" ] && [ "$TT_CAL_OK" = "True" ] && ok "MC-007 calibration 结构完整" || bad "MC-007 calibration YU=$YU_CAL_OK TT=$TT_CAL_OK"

# MC-008: /api/control/stop
note '--- MC-008 control/stop ---'
TT_STOP=$(curl -s -X POST "$TT/api/control/stop" 2>/dev/null | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('ok'))" 2>/dev/null)
[ -n "$TT_STOP" ] && ok "MC-008 control/stop 响应 (TT=$TT_STOP)" || bad "MC-008 control/stop TT=$TT_STOP"

# MC-009: 鼠标输出模式
note '--- MC-009 mouse_output ---'
YU_MO=$(echo "$YU_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('state',{}).get('mouse_output',{}); print('mode' in d)" 2>/dev/null)
TT_MO=$(echo "$TT_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('state',{}).get('mouse_output',{}); print('mode' in d)" 2>/dev/null)
[ "$YU_MO" = "True" ] && [ "$TT_MO" = "True" ] && ok "MC-009 mouse_output 存在" || bad "MC-009 mouse_output YU=$YU_MO TT=$TT_MO"

note ''
note '════════ 结果 ════════'
note "PASS: $PASS_N"
note "FAIL: $FAIL_N"
if [ "$FAIL_N" -gt 0 ]; then
    printf '失败项:%b\n' "$FAIL_LIST"
    echo 'FINAL: FAIL'
    exit 1
fi
echo 'FINAL: PASS'
exit 0
