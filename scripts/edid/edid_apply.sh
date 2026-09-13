#!/usr/bin/env bash
# edid_apply.sh — TTBOX EDID 统一应用入口
# 对齐 YU：生成 EDID → HPD 重协商 + 注入 HDMI-RX → 回读校验 → 保存固件副本。
# 默认自动切 HPD 让源端重新读取 EDID；TTBOX_EDID_REHANDSHAKE=0 可退回纯注入。
# 不修改 DRM/真实显示器输出。
# 用法：sudo bash /opt/ttbox/scripts/edid/edid_apply.sh [device]  默认 /dev/video0
set -euo pipefail

CONFIG="${TTBOX_DISPLAY_CONFIG:-/opt/ttbox/config/hardware_display.json}"
EDID_DIR="/opt/ttbox/runtime/edid"
EDID_OUTPUT="${EDID_OUTPUT:-$EDID_DIR/current.bin}"
VIDEO_DEV="${1:-/dev/video0}"
if [ "$VIDEO_DEV" != "/dev/video0" ]; then
  echo "错误的 HDMI-RX 设备 $VIDEO_DEV：EDID 注入必须使用 /dev/video0；/dev/dri/card0 仅用于 loopout" >&2
  exit 2
fi
export PY_ROOT="${PY_ROOT:-/opt/ttbox/scripts}"
# 默认按 YU 流程重协商；明确指定 0 才退回纯注入。
REHANDSHAKE="${TTBOX_EDID_REHANDSHAKE:-1}"
HPD_STATUS=""
if [ "$REHANDSHAKE" = "1" ]; then
  if [ -w /sys/class/hdmirx/hdmirx/status ]; then
    HPD_STATUS="/sys/class/hdmirx/hdmirx/status"
  elif [ -w /sys/devices/platform/fdee0000.hdmirx-controller/hdmirx/hdmirx/status ]; then
    HPD_STATUS="/sys/devices/platform/fdee0000.hdmirx-controller/hdmirx/hdmirx/status"
  else
    echo '{"ok": false, "error": "已请求重协商，但未找到可写 HDMI-RX HPD 节点"}'
    exit 1
  fi
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
name = edid[77:90].rstrip(b"\x0a\x20").decode("ascii", "replace").strip()
print(json.dumps({"ok": True, "file": out_path, "size": len(edid),
                  "vendor": vendor, "product_id": f"0x{pid:04x}",
                  "serial": f"0x{ser:08x}", "name": name}))
PYEOF

set_hpd() {
  local state="$1"
  [ -n "$HPD_STATUS" ] || return 0
  printf '%s\n' "$state" > "$HPD_STATUS" 2>/dev/null
}

apply_and_verify() {
  v4l2-ctl -d "$VIDEO_DEV" --set-edid=pad=0,file="$EDID_OUTPUT",format=raw || return 1
  # 根因修复：全字节验证（此前只验证 name 字段——注入损坏/半截时仍误判成功，
  # 导致驱动 EDID 状态损坏 → PC 源 fallback 800x600）
  local raw_file ok
  raw_file="$(mktemp)"
  if v4l2-ctl -d "$VIDEO_DEV" --get-edid=pad=0,format=raw > "$raw_file" 2>/dev/null; then
    ok=$(python3 -c "
import sys
data=open('$raw_file','rb').read()
cur=open('$EDID_OUTPUT','rb').read()
# 驱动必须返回完整 EDID，且长度和内容都一致；半截回读不能算成功。
sys.exit(0 if len(data) == len(cur) and data == cur else 1)
" 2>/dev/null && echo yes || echo no)
    rm -f "$raw_file"
    [ "$ok" = "yes" ] || return 1
    return 0
  fi
  rm -f "$raw_file"
  return 1
}

wait_for_lock() {
  local timeout="${TTBOX_EDID_LOCK_TIMEOUT_SEC:-14}"
  local deadline=$(( $(date +%s) + timeout ))
  local status timing
  while [ "$(date +%s)" -lt "$deadline" ]; do
    status="$(cat /sys/kernel/debug/hdmirx/status 2>/dev/null || true)"
    if printf '%s\n' "$status" | grep -qE 'Clk-Ch:Lock[[:space:]]+Ch0:Lock[[:space:]]+Ch1:Lock[[:space:]]+Ch2:Lock'; then
      timing="$(mktemp)"
      if v4l2-ctl -d "$VIDEO_DEV" --query-dv-timing >"$timing" 2>&1 && ! grep -qE 'failed|No locks' "$timing"; then
        rm -f "$timing"
        return 0
      fi
      rm -f "$timing"
    fi
    sleep 1
  done
  return 1
}

EXPECT_NAME=$(python3 -c "
import json
cfg=json.load(open('$CONFIG'))
print(cfg.get('name','TTBOX')[:13])
" 2>/dev/null || echo "TTBOX")

if [ "$REHANDSHAKE" = "1" ]; then
  # RK3588/YU 实际流程：HPD 断开后源端不一定一次就完成重新枚举。
  # 采用有限重试，每轮都重新拉低/拉高 HPD，直到 EDID 回读且 RX 锁定。
  trap 'set_hpd on 2>/dev/null || true' EXIT
fi

if [ "$REHANDSHAKE" = "1" ]; then
  APPLIED=0
  LOCKED=0
  ATTEMPTS="${TTBOX_EDID_REHANDSHAKE_ATTEMPTS:-12}"
  case "$ATTEMPTS" in ''|*[!0-9]*) ATTEMPTS=12 ;; esac
  [ "$ATTEMPTS" -gt 0 ] || ATTEMPTS=1
  attempt=1
  while [ "$attempt" -le "$ATTEMPTS" ]; do
    set_hpd off
    sleep 0.2
    if apply_and_verify; then
      APPLIED=1
      set_hpd on
      sleep "${TTBOX_EDID_HPD_SETTLE_SEC:-0.5}"
      if wait_for_lock; then
        LOCKED=1
        trap - EXIT
        break
      fi
    fi
    attempt=$((attempt + 1))
    sleep 0.5
  done
  if [ "$LOCKED" != "1" ]; then
    if [ "$APPLIED" = "1" ]; then
      echo '{"ok": false, "error": "EDID 已写入且回读一致，但 HDMI-RX 多轮重新枚举后仍未锁定输入", "edid_applied": true, "locked": false}'
    else
      echo '{"ok": false, "error": "EDID 注入或回读校验失败，重新枚举未完成", "edid_applied": false, "locked": false}'
    fi
    exit 1
  fi
else
  if ! apply_and_verify; then
    echo '{"ok": false, "error": "EDID 注入或回读校验失败，未执行重试/HPD切换"}'
    exit 1
  fi
fi

if [ "$REHANDSHAKE" = "1" ] && [ "$LOCKED" = "1" ]; then
  # 持久化 firmware（TTBOX 独立路径，供下一次启动恢复）
  FIRMWARE_DIR="/lib/firmware/ttbox"
  mkdir -p "$FIRMWARE_DIR"
  if ! cp "$EDID_OUTPUT" "$FIRMWARE_DIR/hdmirx_edid.bin" 2>/dev/null || ! chmod 644 "$FIRMWARE_DIR/hdmirx_edid.bin"; then
    echo '{"ok": false, "error": "EDID 已写入驱动，但固件副本保存失败"}'
    exit 1
  fi
  echo "Persisted firmware EDID: $FIRMWARE_DIR/hdmirx_edid.bin"
  CUR=$(v4l2-ctl -d "$VIDEO_DEV" --get-edid=pad=0,format=raw 2>/dev/null | wc -c)
  echo "{\"ok\": true, \"hpd\": \"$([ \"$REHANDSHAKE\" = \"1\" ] && echo rehandshake || echo unchanged)\", \"version\": \"$CUR\", \"method\": \"v4l2_ctl\", \"file\": \"$EDID_OUTPUT\", \"mode\": \"$EXPECT_NAME\"}"
  exit 0
fi

# 非重协商模式注入成功后同样持久化，保持原有行为。
FIRMWARE_DIR="/lib/firmware/ttbox"
mkdir -p "$FIRMWARE_DIR"
if ! cp "$EDID_OUTPUT" "$FIRMWARE_DIR/hdmirx_edid.bin" 2>/dev/null || ! chmod 644 "$FIRMWARE_DIR/hdmirx_edid.bin"; then
  echo '{"ok": false, "error": "EDID 已写入驱动，但固件副本保存失败"}'
  exit 1
fi
echo "Persisted firmware EDID: $FIRMWARE_DIR/hdmirx_edid.bin"
CUR=$(v4l2-ctl -d "$VIDEO_DEV" --get-edid=pad=0,format=raw 2>/dev/null | wc -c)
echo "{\"ok\": true, \"hpd\": \"unchanged\", \"version\": \"$CUR\", \"method\": \"v4l2_ctl\", \"file\": \"$EDID_OUTPUT\", \"mode\": \"$EXPECT_NAME\"}"
exit 0
