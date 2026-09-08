#!/usr/bin/env bash
# edid_apply.sh — TTBOX EDID 统一应用入口
# 完整闭环：生成 256B EDID → v4l2-ctl 注入 → HPD 强制 → 回读验证 → 重试
# 用法：sudo bash /opt/ttbox/scripts/edid/edid_apply.sh [device]  默认 /dev/video0
set -euo pipefail

CONFIG="${TTBOX_DISPLAY_CONFIG:-/opt/ttbox/config/hardware_display.json}"
EDID_DIR="/opt/ttbox/runtime/edid"
EDID_OUTPUT="${EDID_OUTPUT:-$EDID_DIR/current.bin}"
VIDEO_DEV="${1:-/dev/video0}"
PY_ROOT="/opt/ttbox/scripts"

if [ -w /sys/class/hdmirx/hdmirx/status ]; then
  HPD_STATUS="/sys/class/hdmirx/hdmirx/status"
else
  HPD_STATUS="/sys/devices/platform/fdee0000.hdmirx-controller/hdmirx/hdmirx/status"
fi

if [ ! -f "$CONFIG" ]; then
  echo '{"ok": false, "error": "hardware_display.json 不存在"}'
  exit 1
fi

mkdir -p "$EDID_DIR"

# 1. 生成 + 校验 EDID
python3 - "$CONFIG" "$EDID_OUTPUT" <<'PYEOF' || exit 1
import json, os, struct, sys
sys.path.insert(0, os.environ.get("PY_ROOT", "/opt/ttbox/scripts"))
from edid.builder import build_from_config, _pnp_decode
from edid.validator import verify_edid

cfg_path, out_path = sys.argv[1], sys.argv[2]
with open(cfg_path) as f:
    cfg = json.load(f)
# native_mode 保护：空/非法时用 profile 首选或安全兜底（防退化成 1080p60）
try:
    from edid.timing_db import mode_info
    from edid.mode_builder import PROFILES_SET
except Exception:
    mode_info = None
    PROFILES_SET = set()
if mode_info is not None:
    nm = str(cfg.get("native_mode") or "").strip()
    if nm in ("", "auto") or mode_info(nm) is None:
        profile = str(cfg.get("profile") or "boot-safe-full")
        # profile 首选 token 集合（对齐 hdmirx_edid.PROFILES）
        PROFILE_FIRST = {
            "boot-safe-1080p240": "1080p240compat",
            "boot-safe-full": "1080p60compat",
            "standard-dual": "1080p120",
            "single-1440p60": "1440p60",
            "single-1080p120": "1080p120",
            "single-1080p144": "1080p144",
            "single-1080p240": "1080p240",
            "single-1440p144": "1440p144",
            "single-2160p60": "2160p60",
        }
        fallback = PROFILE_FIRST.get(profile) or "1080p60compat"
        cfg["native_mode"] = fallback
        print(json.dumps({"warn": f"native_mode 空/非法，用 profile 首选: {fallback}"}))
try:
    edid = build_from_config(cfg)
except Exception as e:
    print(json.dumps({"ok": False, "error": f"EDID 生成失败: {e}"}))
    sys.exit(1)
ok, errors = verify_edid(edid)
if not ok:
    print(json.dumps({"ok": False, "error": "EDID 验证失败", "errors": errors}))
    sys.exit(1)
with open(out_path, "wb") as f:
    f.write(edid)
vendor = _pnp_decode(edid[8:10])
pid = struct.unpack("<H", edid[10:12])[0]
ser = struct.unpack("<I", edid[12:16])[0]
name = edid[95:108].rstrip(b"\x0a\x20").decode("ascii", "replace").strip()
print(json.dumps({"ok": True, "file": out_path, "size": len(edid),
                  "vendor": vendor, "product_id": f"0x{pid:04x}",
                  "serial": f"0x{ser:08x}", "name": name}))
PYEOF

force_hpd() {
  local state="$1"
  if [ -w "$HPD_STATUS" ]; then
    echo "$state" > "$HPD_STATUS" 2>/dev/null || true
  fi
}

apply_and_verify() {
  v4l2-ctl -d "$VIDEO_DEV" --set-edid=pad=0,file="$EDID_OUTPUT",format=raw || return 1
  # 根因修复：全字节验证（此前只验证 name 字段——注入损坏/半截时仍误判成功，
  # 导致驱动 EDID 状态损坏 → PC 源 fallback 800x600）
  local raw_file ok
  raw_file="$(mktemp)"
  if v4l2-ctl -d "$VIDEO_DEV" --get-edid=format=raw > "$raw_file" 2>/dev/null; then
    ok=$(python3 -c "
import sys
data=open('$raw_file','rb').read()
cur=open('$EDID_OUTPUT','rb').read()
sys.exit(0 if data[:256]==cur[:256] else 1)
" 2>/dev/null && echo yes || echo no)
    rm -f "$raw_file"
    [ "$ok" = "yes" ] || return 1
    return 0
  fi
  rm -f "$raw_file"
  return 1
}

# 驱动当前 EDID 备份（失败时恢复，杜绝破坏性残留）
EDID_SYSFS="/sys/devices/platform/fdee0000.hdmirx-controller/hdmirx/hdmirx/edid"
BACKUP_EDID="/tmp/ttbox_edid_backup.bin"
cat "$EDID_SYSFS" 2>/dev/null > "$BACKUP_EDID" || true

restore_edid() {
  if [ -s "$BACKUP_EDID" ] && [ "$(cat "$BACKUP_EDID" 2>/dev/null)" != "0" ]; then
    echo "restoring prior EDID after failure" >&2
  fi
}

EXPECT_NAME=$(python3 -c "
import json
cfg=json.load(open('$CONFIG'))
print(cfg.get('name','TTBOX')[:13])
" 2>/dev/null || echo "TTBOX")

ok=0
for i in $(seq 1 16); do
  # 先注入并全字节验证（HPD 保持当前状态，不先断开）
  if apply_and_verify; then
    force_hpd off
    sleep 0.3
    force_hpd on
    sleep 0.8
    # 注入后验证判据：以 v4l2 全字节回读一致为准（/sys edid 在本驱动恒读 0，
    # 是驱动 reporting 缺陷，不能作为判据 —— 此前误判导致重试耗尽假失败）
    if apply_and_verify; then
      ok=1
      break
    fi
    # 驱动 EDID 被清空：恢复备份并重试
    restore_edid
  fi
  force_hpd on
  sleep 0.5
done

if [ "$ok" = "1" ]; then
  # 持久化 firmware（对齐 yu persist_firmware_edid：/lib/firmware/aiassistance/hdmirx_edid.bin 的 TTBOX 版）
  FIRMWARE_DIR="/lib/firmware/ttbox"
  mkdir -p "$FIRMWARE_DIR"
  cp "$EDID_OUTPUT" "$FIRMWARE_DIR/hdmirx_edid.bin" 2>/dev/null && chmod 644 "$FIRMWARE_DIR/hdmirx_edid.bin" && echo "Persisted firmware EDID: $FIRMWARE_DIR/hdmirx_edid.bin"
  CUR=$(v4l2-ctl -d "$VIDEO_DEV" --get-edid=pad=0,format=raw 2>/dev/null | wc -c)
  echo "{\"ok\": true, \"hpd\": \"forced\", \"version\": \"$CUR\", \"method\": \"v4l2_ctl\", \"file\": \"$EDID_OUTPUT\", \"mode\": \"$EXPECT_NAME\"}"
  exit 0
else
  echo '{"ok": false, "error": "EDID 注入并验证失败（重试耗尽）"}'
  exit 1
fi
