#!/usr/bin/env bash
# Phase 2: Hotkey & Profiles 验收
set -u
YU="http://127.0.0.1:8080"
TT="http://127.0.0.1:8000"
PASS_N=0
FAIL_N=0
FAIL_LIST=""

ok() { PASS_N=$((PASS_N+1)); printf '  ✅ %s\n' "$*"; }
bad() { FAIL_N=$((FAIL_N+1)); FAIL_LIST="$FAIL_LIST\n  ❌ $*"; printf '  ❌ %s\n' "$*"; }

note() { printf '%s\n' "$*"; }

note '════════ Phase 2: Hotkey & Profiles ════════'

# --- 获取 YU 配置 ---
YU_STATE=$(curl -s "$YU/api/state" 2>/dev/null)
YU_CFG=$(echo "$YU_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('config',{}); print(json.dumps(d))" 2>/dev/null)

TT_STATE=$(curl -s "$TT/api/state" 2>/dev/null)
TT_CFG=$(echo "$TT_STATE" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('config',{}); print(json.dumps(d))" 2>/dev/null)

# HP-001: auto-start 结构
note '--- HP-001 auto-start ---'
YU_AS=$(curl -s "$YU/api/settings/auto-start")
TT_AS=$(curl -s "$TT/api/settings/auto-start")
YU_OK=$(echo "$YU_AS" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('ok'))" 2>/dev/null)
TT_OK=$(echo "$TT_AS" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('ok'))" 2>/dev/null)
[ "$YU_OK" = "True" ] && [ "$TT_OK" = "True" ] && ok "HP-001 auto-start ok" || bad "HP-001 auto-start YU=$YU_OK TT=$TT_OK"

YU_AS_EN=$(echo "$YU_AS" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}); print(d.get('enabled'),d.get('status'))" 2>/dev/null)
TT_AS_EN=$(echo "$TT_AS" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}); print(d.get('enabled'),d.get('status'))" 2>/dev/null)
[ "$YU_AS_EN" = "$TT_AS_EN" ] && ok "HP-001 auto-start 字段一致 ($YU_AS_EN)" || bad "HP-001 auto-start 字段 YU=$YU_AS_EN TT=$TT_AS_EN"

# HP-002: controller 字段存在（从 state.config 获取）
note '--- HP-002 controller 字段 ---'
YU_CTRL=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('kp_x' in d and 'kp_y' in d)" 2>/dev/null)
TT_CTRL=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('ai',{}).get('controller',{}); print('kp_x' in d and 'kp_y' in d)" 2>/dev/null)
[ "$YU_CTRL" = "True" ] && [ "$TT_CTRL" = "True" ] && ok "HP-002 controller 字段存在" || bad "HP-002 controller YU=$YU_CTRL TT=$TT_CTRL"

# HP-003: aim_profiles 字段存在
note '--- HP-003 aim_profiles ---'
YU_AP=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('aim_profiles',[]); print(len(d))" 2>/dev/null)
TT_AP=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('aim_profiles',[]); print(len(d))" 2>/dev/null)
[ "$YU_AP" -gt 0 ] 2>/dev/null && [ "$TT_AP" -gt 0 ] 2>/dev/null && ok "HP-003 aim_profiles 存在 (YU=$YU_AP TT=$TT_AP)" || bad "HP-003 aim_profiles YU=$YU_AP TT=$TT_AP"

# HP-004: hotkey 字段
note '--- HP-004 hotkey 字段 ---'
YU_HK=$(echo "$YU_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('aim_profiles',[{}])[0].get('hotkey',''); print(d)" 2>/dev/null)
TT_HK=$(echo "$TT_CFG" | python3 -c "import json,sys; d=json.load(sys.stdin).get('aim_profiles',[{}])[0].get('hotkey',''); print(d)" 2>/dev/null)
[ -n "$YU_HK" ] && [ -n "$TT_HK" ] && ok "HP-004 hotkey 存在 (YU=$YU_HK TT=$TT_HK)" || bad "HP-004 hotkey YU=$YU_HK TT=$TT_HK"

# HP-005: PUT /api/config 修改生效
note '--- HP-005 config PUT 修改 ---'
# 用 TTBOX 自己的 PUT
TT_PUT=$(curl -s -X PUT "$TT/api/config" -H "Content-Type: application/json" -d '{"video_detection_confidence": 0.26}' 2>/dev/null)
TT_PUT_OK=$(echo "$TT_PUT" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('ok'))" 2>/dev/null)
# 读回
TT_VAL=$(curl -s "$TT/api/state" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}).get('config',{}).get('video_detection_confidence',''); print(d)" 2>/dev/null)
TT_VAL_OK=$(python3 -c "print('OK' if abs(float('$TT_VAL')-0.26)<1e-4 else 'BAD')" 2>/dev/null)
[ "$TT_PUT_OK" = "True" ] && [ "$TT_VAL_OK" = "OK" ] && ok "HP-005 config PUT 修改生效 (TT=$TT_VAL)" || bad "HP-005 config PUT TT_OK=$TT_PUT_OK TT_VAL_OK=$TT_VAL_OK"

# 恢复原值
curl -s -X PUT "$TT/api/config" -H "Content-Type: application/json" -d '{"video_detection_confidence": 0.25}' >/dev/null 2>&1

# HP-006: motion-profiles 存在
note '--- HP-006 motion-profiles ---'
YU_MP=$(curl -s "$YU/api/motion-profiles" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}); print(type(d).__name__)" 2>/dev/null)
TT_MP=$(curl -s "$TT/api/motion-profiles" | python3 -c "import json,sys; d=json.load(sys.stdin).get('data',{}); print(type(d).__name__)" 2>/dev/null)
[ -n "$YU_MP" ] && [ -n "$TT_MP" ] && ok "HP-006 motion-profiles 存在 (YU=$YU_MP TT=$TT_MP)" || bad "HP-006 motion-profiles YU=$YU_MP TT=$TT_MP"

# HP-007: motion-profiles 拒绝创建
note '--- HP-007 motion-profiles 拒绝创建 ---'
YU_MPC=$(curl -s -X POST "$YU/api/motion-profiles" -H "Content-Type: application/json" -d '{"name":"test"}' | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('ok'))" 2>/dev/null)
TT_MPC=$(curl -s -X POST "$TT/api/motion-profiles" -H "Content-Type: application/json" -d '{"name":"test"}' | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('ok'))" 2>/dev/null)
[ "$YU_MPC" = "False" ] && [ "$TT_MPC" = "False" ] && ok "HP-007 motion-profiles 拒绝创建 (YU=$YU_MPC TT=$TT_MPC)" || bad "HP-007 motion-profiles YU=$YU_MPC TT=$TT_MPC"

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
