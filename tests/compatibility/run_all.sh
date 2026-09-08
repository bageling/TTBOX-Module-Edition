#!/usr/bin/env bash
# run_all.sh — TTBOX / YU 兼容性回归测试
# 真实请求 YU(8080) 与 TTBOX(8000) 的 API，对比结构/状态文字/HTTP code。
# 用法：bash tests/compatibility/run_all.sh   （在项目根目录执行）
set -uo pipefail

YU="http://127.0.0.1:8080"
TT="http://127.0.0.1:8000"
PASS=0
FAIL=0
FAILED_ITEMS=()

hr() { printf '%-60s' "$1"; }

check() { # check <name> <yu_json> <tt_json> <field>
  local name="$1" y="$2" t="$3" field="$4"
  local yv tv
  yv=$(echo "$y" | python3 -c "import sys,json;d=json.load(sys.stdin);print(json.dumps(d.get('data',{}).get('$field') if '$field' else d,ensure_ascii=False))" 2>/dev/null)
  tv=$(echo "$t" | python3 -c "import sys,json;d=json.load(sys.stdin);print(json.dumps(d.get('data',{}).get('$field') if '$field' else d,ensure_ascii=False))" 2>/dev/null)
  if [ "$yv" = "$tv" ]; then
    PASS=$((PASS+1)); echo "[PASS] $name"
  else
    FAIL=$((FAIL+1)); FAILED_ITEMS+=("$name")
    echo "[FAIL] $name"
    echo "   YU: $yv"
    echo "   TT: $tv"
  fi
}

echo "════════ TTBOX / YU COMPATIBILITY TEST ════════"
echo

# 1. health
hr "health/frontend"
yu=$(curl -s --max-time 8 "$YU/api/health/frontend")
tt=$(curl -s --max-time 8 "$TT/api/health/frontend")
if [ "$(echo "$yu" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("ok"))')" = "$(echo "$tt" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("ok"))')" ]; then
  PASS=$((PASS+1)); echo "[PASS] health/frontend ok字段"
else
  FAIL=$((FAIL+1)); FAILED_ITEMS+=("health"); echo "[FAIL] health/frontend"; echo "  YU=$yu TT=$tt"
fi

# 2. auto-start（状态文字）
yu=$(curl -s --max-time 8 "$YU/api/settings/auto-start")
tt=$(curl -s --max-time 8 "$TT/api/settings/auto-start")
check "auto-start.status" "$yu" "$tt" "status"
check "auto-start.message" "$yu" "$tt" "message"
check "auto-start.enabled" "$yu" "$tt" "enabled"

# 3. state.status / last_error / core.message
yu=$(curl -s --max-time 8 "$YU/api/state")
tt=$(curl -s --max-time 8 "$TT/api/state")
check "state.status" "$yu" "$tt" "state.status"
check "state.core.message" "$yu" "$tt" "state.core.message"
check "state.motion_training.model_status" "$yu" "$tt" "state.motion_training.model_status"

# 4. calibration（reason 状态文字）
yu=$(curl -s --max-time 8 "$YU/api/control/calibration")
tt=$(curl -s --max-time 8 "$TT/api/control/calibration")
check "calibration.runtime.reason" "$yu" "$tt" "runtime.reason"
check "calibration.runtime.phase" "$yu" "$tt" "runtime.phase"
check "calibration.calibration.valid" "$yu" "$tt" "calibration.valid"

# 5. models 结构（selected_model_id 字段存在性——两系统模型库独立，值不比较）
yu=$(curl -s --max-time 8 "$YU/api/models")
tt=$(curl -s --max-time 8 "$TT/api/models")
ys=$(echo "$yu" | python3 -c "import sys,json;d=json.load(sys.stdin)['data'];print('ok' if 'selected_model_id' in d and isinstance(d.get('models'),list) else 'missing')" 2>/dev/null)
ts=$(echo "$tt" | python3 -c "import sys,json;d=json.load(sys.stdin)['data'];print('ok' if 'selected_model_id' in d and isinstance(d.get('models'),list) else 'missing')" 2>/dev/null)
if [ "$ys" = "$ts" ]; then PASS=$((PASS+1)); echo "[PASS] models 结构(selected_model_id+models[])"; else FAIL=$((FAIL+1)); FAILED_ITEMS+=("models-struct"); echo "[FAIL] models 结构 YU=$ys TT=$ts"; fi

# 6. presets
yu=$(curl -s --max-time 8 "$YU/api/presets")
tt=$(curl -s --max-time 8 "$TT/api/presets")
check "presets" "$yu" "$tt" "presets"

# 7. mouse 结构字段
yu=$(curl -s --max-time 8 "$YU/api/hardware/mouse")
tt=$(curl -s --max-time 8 "$TT/api/hardware/mouse")
check "mouse.mode" "$yu" "$tt" "mode"
check "mouse.set_config_supported" "$yu" "$tt" "set_config_supported"

# 8. display 结构（source/hdmi_raw_gbps 存在性 + loopout_enabled）
yu=$(curl -s --max-time 8 "$YU/api/hardware/display")
tt=$(curl -s --max-time 8 "$TT/api/hardware/display")
ys=$(echo "$yu" | python3 -c "import sys,json;d=json.load(sys.stdin)['data'];m=d['display_mode']['advertised_modes'];print('ok' if m and 'source' in m[0] and 'hdmi_raw_gbps' in m[0] else 'missing')" 2>/dev/null)
ts=$(echo "$tt" | python3 -c "import sys,json;d=json.load(sys.stdin)['data'];m=d['display_mode']['advertised_modes'];print('ok' if m and 'source' in m[0] and 'hdmi_raw_gbps' in m[0] else 'missing')" 2>/dev/null)
if [ "$ys" = "$ts" ]; then PASS=$((PASS+1)); echo "[PASS] display modes source/hdmi_raw_gbps"; else FAIL=$((FAIL+1)); FAILED_ITEMS+=("display-modes"); echo "[FAIL] display modes source/hdmi_raw_gbps YU=$ys TT=$ts"; fi
check "display.loopout_enabled" "$yu" "$tt" "display_mode.loopout_enabled"

# 9. announcement（503 对齐）
ycode=$(curl -s -o /dev/null -w '%{http_code}' --max-time 8 "$YU/api/announcement")
tcode=$(curl -s -o /dev/null -w '%{http_code}' --max-time 8 "$TT/api/announcement")
if [ "$ycode" = "$tcode" ]; then PASS=$((PASS+1)); echo "[PASS] announcement HTTP $ycode"; else FAIL=$((FAIL+1)); FAILED_ITEMS+=("announcement"); echo "[FAIL] announcement YU=$ycode TT=$tcode"; fi

# 10. update/status（idle 状态文字）
yu=$(curl -s --max-time 8 "$YU/api/update/status")
tt=$(curl -s --max-time 8 "$TT/api/update/status")
check "update/status.status" "$yu" "$tt" "status.status"
check "update/status.message" "$yu" "$tt" "status.message"

# 11. lan-blocklist（message 状态文字 + supported）
yu=$(curl -s --max-time 8 "$YU/api/system/lan-blocklist")
tt=$(curl -s --max-time 8 "$TT/api/system/lan-blocklist")
check "lan-blocklist.message" "$yu" "$tt" "message"
check "lan-blocklist.supported" "$yu" "$tt" "supported"

# 12. themes（active_theme_id + 主题结构）
yu=$(curl -s --max-time 8 "$YU/api/themes")
tt=$(curl -s --max-time 8 "$TT/api/themes")
check "themes.active_theme_id" "$yu" "$tt" "active_theme_id"

# 13. activation（reset-local-identity 拒绝）
yu=$(curl -s -X POST --max-time 8 "$YU/api/activation/reset-local-identity")
tt=$(curl -s -X POST --max-time 8 "$TT/api/activation/reset-local-identity")
yv=$(echo "$yu" | python3 -c "import sys,json;print(json.load(sys.stdin).get('ok'))" 2>/dev/null)
tv=$(echo "$tt" | python3 -c "import sys,json;print(json.load(sys.stdin).get('ok'))" 2>/dev/null)
if [ "$yv" = "$tv" ]; then PASS=$((PASS+1)); echo "[PASS] activation.reset-local-identity"; else FAIL=$((FAIL+1)); FAILED_ITEMS+=("activation-reset"); echo "[FAIL] activation YU=$yv TT=$tv"; fi

# 14. license/activate 空 key（错误结构）
yu=$(curl -s -X POST --max-time 8 "$YU/api/license/activate" -H 'Content-Type: application/json' -d '{}')
tt=$(curl -s -X POST --max-time 8 "$TT/api/license/activate" -H 'Content-Type: application/json' -d '{}')
check "license/activate.error" "$yu" "$tt" "error"

echo
echo "════════ 结果 ════════"
echo "PASS: $PASS"
echo "FAIL: $FAIL"
if [ "$FAIL" -gt 0 ]; then
  echo "失败项: ${FAILED_ITEMS[*]}"
  echo "FINAL: FAIL"
  exit 1
fi
echo "FINAL: PASS"
exit 0
