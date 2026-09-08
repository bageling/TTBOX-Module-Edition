#!/usr/bin/env bash
# Phase 4: Assist 验收
set -u
YU="http://127.0.0.1:8080"
TT="http://127.0.0.1:8000"
PASS_N=0; FAIL_N=0; FAIL_LIST=""
ok() { PASS_N=$((PASS_N+1)); printf '  ✅ %s\n' "$*"; }
bad() { FAIL_N=$((FAIL_N+1)); FAIL_LIST="$FAIL_LIST\n  ❌ $*"; printf '  ❌ %s\n' "$*"; }
note() { printf '%s\n' "$*"; }
note '════════ Phase 4: Assist ════════'

# AS-001: 辅助控制字段存在
YU_STATE=$(curl -s "$YU/api/state" 2>/dev/null); TT_STATE=$(curl -s "$TT/api/state" 2>/dev/null)
YU_CFG=$(echo "$YU_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('config',{}); print(json.dumps(d))" 2>/dev/null)
TT_CFG=$(echo "$TT_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('config',{}); print(json.dumps(d))" 2>/dev/null)

YU_AS=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('pull_curve_enabled' in d)")
TT_AS=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('pull_curve_enabled' in d)")
[ "$YU_AS" = "True" ] && [ "$TT_AS" = "True" ] && ok "AS-001 pull_curve_enabled" || bad "AS-001 $YU_AS $TT_AS"

# AS-002: continuous_lead
YU_CL=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('continuous_lead_enabled' in d)")
TT_CL=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('continuous_lead_enabled' in d)")
[ "$YU_CL" = "True" ] && [ "$TT_CL" = "True" ] && ok "AS-002 continuous_lead_enabled" || bad "AS-002 $YU_CL $TT_CL"

# AS-003: humanize
YU_HZ=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('humanize_enabled' in d)")
TT_HZ=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('humanize_enabled' in d)")
[ "$YU_HZ" = "True" ] && [ "$TT_HZ" = "True" ] && ok "AS-003 humanize_enabled" || bad "AS-003 $YU_HZ $TT_HZ"

# AS-004: block_physical_mouse
YU_BP=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('block_physical_mouse_x_while_aiming' in d)")
TT_BP=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('block_physical_mouse_x_while_aiming' in d)")
[ "$YU_BP" = "True" ] && [ "$TT_BP" = "True" ] && ok "AS-004 block_physical_mouse" || bad "AS-004 $YU_BP $TT_BP"

# AS-005: aim_fire_lock_y
YU_FL=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('aim_fire_lock_y' in d)")
TT_FL=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('aim_fire_lock_y' in d)")
[ "$YU_FL" = "True" ] && [ "$TT_FL" = "True" ] && ok "AS-005 aim_fire_lock_y" || bad "AS-005 $YU_FL $TT_FL"

# AS-006: calibration 段
YU_CAL=$(echo "$YU_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('state',{}).get('calibration',{}); print('phase' in d and 'ready' in d)")
TT_CAL=$(echo "$TT_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('state',{}).get('calibration',{}); print('phase' in d and 'ready' in d)")
[ "$YU_CAL" = "True" ] && [ "$TT_CAL" = "True" ] && ok "AS-006 calibration" || bad "AS-006 $YU_CAL $TT_CAL"

note "PASS: $PASS_N  FAIL: $FAIL_N"
[ "$FAIL_N" -gt 0 ] && echo "FINAL: FAIL" && exit 1
echo "FINAL: PASS"
