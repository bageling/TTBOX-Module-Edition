#!/usr/bin/env bash
# inject_edid.sh — 注入 hdmirx EDID，让 PC 端虚拟屏以高分辨率输出
# 用法: sudo bash inject_edid.sh [1|2]   1=340M(默认/1080p)  2=600M(4K)
set -euo pipefail
EDID_NODE="/sys/devices/platform/fdee0000.hdmirx-controller/hdmirx/hdmirx/edid"
VERSION="${1:-2}"
if [ ! -w "$EDID_NODE" ]; then
  echo "EDID node not writable: $EDID_NODE" >&2
  exit 1
fi
echo "$VERSION" > "$EDID_NODE"
CUR=$(cat "$EDID_NODE")
echo "EDID injected: version=$CUR (1=340M 1080p, 2=600M 4K)"
