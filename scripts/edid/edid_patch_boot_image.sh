#!/usr/bin/env bash
# edid_patch_boot_image.sh — 把 TTBOX EDID 持久化到内核启动镜像
# 用法：sudo bash /opt/ttbox/scripts/edid/edid_patch_boot_image.sh [edid.bin]
# 目标：/boot/Image + /boot/vmlinuz；首次备份到 /var/lib/ttbox/boot_image_backups/
set -euo pipefail

EDID_PATH="${1:-/opt/ttbox/runtime/edid/current.bin}"
STATE_DIR="/var/lib/ttbox"
STATE_PATH="$STATE_DIR/boot_image_edid_slots.json"
BACKUP_DIR="$STATE_DIR/boot_image_backups"

if [ ! -f "$EDID_PATH" ]; then
  echo "{\"ok\": false, \"error\": \"EDID file not found: $EDID_PATH\"}"
  exit 1
fi
mkdir -p "$STATE_DIR" "$BACKUP_DIR"

TARGETS=()
[ -e /boot/Image ] && TARGETS+=(/boot/Image)
[ -e "/boot/vmlinuz-$(uname -r)" ] && TARGETS+=("/boot/vmlinuz-$(uname -r)")
if [ "${#TARGETS[@]}" = "0" ]; then
  echo '{"ok": false, "error": "未找到可用内核镜像"}'
  exit 1
fi

python3 - "$EDID_PATH" "$STATE_PATH" "$BACKUP_DIR" "${TARGETS[@]}" <<'PY'
import json, os, shutil, stat, struct, sys
from pathlib import Path

EDID_SIZE = 256
EDID_HEADER = b"\x00\xff\xff\xff\xff\xff\xff\x00"
BUILTIN = (b"RK-UHD", b"IFP Display", b"RKP3588", b"T749")


def valid_edid(b):
    return (len(b) >= EDID_SIZE and b.startswith(EDID_HEADER)
            and (sum(b[:128]) & 0xff) == 0 and (sum(b[128:256]) & 0xff) == 0)


def is_builtin(b):
    n = b[95:108].rstrip(b"\x0a\x20")
    return any(n.startswith(x) for x in BUILTIN)


def is_noise(b):
    if struct.unpack('<H', b[10:12])[0] == 0:
        return True
    n = b[95:108]
    return any(c < 0x20 and c not in (0x0a, 0x20) for c in n)


def find_slots(data):
    out = []
    i = data.find(EDID_HEADER)
    while i != -1:
        blk = data[i:i + EDID_SIZE]
        if (valid_edid(blk) and not is_builtin(blk) and not is_noise(blk)
                and data[i + EDID_SIZE:i + EDID_SIZE + 8] == EDID_HEADER):
            out.append(i)
            out.append(i + EDID_SIZE)
            i += EDID_SIZE * 2
            continue
        i = data.find(EDID_HEADER, i + 1)
    seen, res = set(), []
    for o in out:
        if o not in seen:
            seen.add(o)
            res.append(o)
    return res


def main():
    edid_path, state_path, backup_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    targets = sys.argv[4:]
    edid = Path(edid_path).read_bytes()
    if len(edid) != EDID_SIZE or not valid_edid(edid):
        print(json.dumps({"ok": False, "error": f"EDID 无效: {len(edid)}B"}))
        return 1
    state = {}
    try:
        with open(state_path) as f:
            state = json.load(f)
    except Exception:
        pass
    anypatched = False
    for t in targets:
        path = Path(t)
        data = bytearray(path.read_bytes())
        offsets = find_slots(bytes(data))
        if not offsets:
            print(f"{t}: no main EDID slot (skip)")
            continue
        bak = Path(backup_dir) / (path.name + ".bak-ttbox-edid-original")
        if not bak.exists():
            shutil.copy2(path, bak)
        for off in offsets:
            data[off:off + EDID_SIZE] = edid
        tmp = path.with_name(path.name + ".ttbox-tmp")
        with open(tmp, "wb") as fh:
            fh.write(bytes(data))
            fh.flush()
            os.fsync(fh.fileno())
        os.chmod(tmp, stat.S_IMODE(path.stat().st_mode))
        os.replace(tmp, path)
        state[str(path)] = {"offsets": offsets}
        anypatched = True
        print(f"{t}: patched offsets={offsets}")
    if anypatched:
        with open(state_path, "w") as f:
            json.dump(state, f, indent=2)
    print(json.dumps({"ok": anypatched, "state": state_path, "edid": edid_path}))
    return 0 if anypatched else 1


if __name__ == "__main__":
    sys.exit(main())
PY
