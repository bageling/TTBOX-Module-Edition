#!/usr/bin/env python3
"""TTBOX hdmirx_edid 兼容工具 — 完全独立于 yu(/opt/aiassistance)。

功能（输出格式与 yu 的 /opt/aiassistance/bin/hdmirx_edid 兼容，供 web 前端消费）：
  --list     列出 TTBOX 支持的 EDID 模式（Profiles + Modes）
  --status   读取当前生效 EDID 真实身份 + 格式/时序

数据来源（全部 TTBOX 自身）：
  --list   -> scripts/edid/timing_db.py 的 TIMING_MAP
  --status -> v4l2-ctl --get-edid=format=raw 解码当前驱动 EDID
"""
import argparse
import os
import subprocess
import struct
import sys

sys.path.insert(0, "/opt/ttbox/scripts")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))       # scripts/edid/
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # scripts/

try:
    from edid.timing_db import TIMING_MAP
except Exception:
    TIMING_MAP = {}

PROFILES = {
    "standard-dual": ["1080p120", "1440p60"],
    "boot-safe-1080p240": ["1080p60compat", "1080p120compat", "1080p240compat", "1440p60"],
    "boot-safe-full": ["1080p60compat", "1080p120", "1080p144", "1080p240compat", "1440p60", "1440p120", "1440p144", "2160p60"],
    "rk3588-full": ["1440p144", "1080p60", "1080p90", "1080p120", "1080p144", "1080p240", "1440p60", "1440p120", "2160p60"],
    "single-1440p60": ["1440p60"],
    "single-1080p120-compat": ["1080p120compat"],
    "single-1080p60": ["1080p60"],
    "single-1080p60-compat": ["1080p60compat"],
    "single-1080p90": ["1080p90"],
    "single-1080p120": ["1080p120"],
    "single-1080p144": ["1080p144"],
    "single-1080p240": ["1080p240"],
    "single-1080p240-compat": ["1080p240compat"],
    "single-1440p120": ["1440p120"],
    "single-1440p144": ["1440p144"],
    "single-2160p60": ["2160p60"],
}


def _decode_vendor(data):
    if len(data) < 10:
        return "???"
    v = (data[8] << 8) | data[9]
    try:
        chars = [
            chr(((v >> 10) & 0x1F) + ord("A") - 1),
            chr(((v >> 5) & 0x1F) + ord("A") - 1),
            chr((v & 0x1F) + ord("A") - 1),
        ]
        return "".join(chars) if all("A" <= c <= "Z" for c in chars) else "???"
    except Exception:
        return "???"


def cmd_list():
    """--list: 兼容 yu 输出（Profiles + Modes）。"""
    print("Profiles:")
    for name, tokens in PROFILES.items():
        print(f"  {name:<24} {tokens}")
    print()
    print("Modes:")
    for token, t in TIMING_MAP.items():
        pc_khz = int(round(t.pixel_clock * 1000))
        total = (t.h_active + t.h_blank) * (t.v_active + t.v_blank)
        actual_hz = (t.pixel_clock * 1_000_000) / total if total else t.refresh
        exp = " experimental" if t.refresh >= 144 else ""
        print(f"  {token:<14} {t.width}x{t.height}@{int(t.refresh)}  "
              f"pixel_clock={pc_khz} kHz  actual={actual_hz:.2f} Hz{exp}")
    print()
    print("Aliases: fhd60/fhd120/fhd144/fhd240, qhd60/qhd120/qhd144, 4k60/uhd60")


def _current_edid_raw(device):
    try:
        out = subprocess.run(
            ["v4l2-ctl", "-d", device, "--get-edid=format=raw"],
            capture_output=True, timeout=5,
        )
        if out.returncode != 0 or not out.stdout:
            return None
        return out.stdout
    except Exception:
        return None


def cmd_status(device):
    """--status: 读驱动当前 EDID 身份 + DTD1 时序。"""
    print(f"Device: {device}")
    raw = _current_edid_raw(device)
    if raw is None or len(raw) < 108:
        print("Current EDID: none")
        print("Format: unknown")
        return 1
    edid_name = raw[95:108].rstrip(b"\x0a\x20").decode("ascii", "replace")
    vendor = _decode_vendor(raw)
    pid = struct.unpack("<H", raw[10:12])[0]
    ser = struct.unpack("<I", raw[12:16])[0]
    print(f"Current EDID: name={edid_name} vendor={vendor} "
          f"product=0x{pid:04x} serial=0x{ser:08x}")
    pc_10khz = raw[54] | (raw[55] << 8)
    h_active = raw[56] | ((raw[57] & 0xF) << 8)
    v_active = raw[59] | ((raw[60] & 0xF) << 8)
    h_blank = ((raw[57] & 0xF0) >> 4) | (raw[58] << 4)
    v_blank = ((raw[60] & 0xF0) >> 4) | (raw[61] << 4)
    total = (h_active + h_blank) * (v_active + v_blank)
    fps = (pc_10khz * 10000) / total if total else 0
    print(f"Format: {h_active}x{v_active} rgb bytesperline unknown")
    print(f"Timings: {h_active}x{v_active} pixelclock={pc_10khz*10000} Hz fps={fps:.2f}")
    return 0


def main():
    parser = argparse.ArgumentParser(prog="hdmirx_edid.py",
                                     description="TTBOX EDID 工具（独立于 yu）")
    group = parser.add_mutually_exclusive_group(required=True)
def cmd_build(profile, native, added, name, vendor, product_id, serial,
              device, apply, output):
    """--profile/--native/--add: 生成 EDID（可选注入），对齐 yu CLI。"""
    import json
    from edid.builder import build_from_config
    from edid.mode_builder import load_config

    # 基准配置：先用 load_config 校验默认，再覆盖 CLI 参数
    cfg = load_config("/opt/ttbox/config/hardware_display.json")
    if profile:
        cfg["profile"] = profile
        # profile 首选模式 = PROFILES 定义里的第一个 token（对齐 yu）
        if not native and profile in PROFILES and PROFILES[profile]:
            cfg["native_mode"] = PROFILES[profile][0]
    if native:
        cfg["native_mode"] = native
    if added:
        cfg["added_modes"] = list(added)
        cfg["native_only"] = False
    if name:
        cfg["name"] = name[:13]
    if vendor:
        cfg["vendor"] = vendor.upper()[:3]
    if product_id:
        cfg["product_id"] = product_id
    if serial:
        cfg["serial"] = serial

    try:
        edid = build_from_config(cfg)
    except Exception as e:
        print(f"EDID 生成失败: {e}", file=sys.stderr)
        return 1
    out = output or "/opt/ttbox/runtime/edid/current.bin"
    import os
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(edid)
    print(f"Generated {len(edid)}B EDID -> {out} "
          f"(name={cfg['name']} vendor={cfg['vendor']} native={cfg['native_mode']})")
    if apply:
        r = subprocess.run(
            ["v4l2-ctl", "-d", device, f"--set-edid=pad=0,file={out},format=raw"],
            capture_output=True, text=True, timeout=10)
        if r.returncode != 0:
            print(f"注入失败: {r.stderr or r.stdout}", file=sys.stderr)
            return 1
        print(f"Applied EDID to {device}")
    return 0


def cmd_builtin(version):
    """--builtin 1|2: 切驱动内置 EDID 组（sysfs）。"""
    node = "/sys/devices/platform/fdee0000.hdmirx-controller/hdmirx/hdmirx/edid"
    if not os.access(node, os.W_OK):
        print(f"EDID node not writable: {node}", file=sys.stderr)
        return 1
    try:
        with open(node, "w") as f:
            f.write(str(version))
    except OSError as e:
        print(f"写入失败: {e}", file=sys.stderr)
        return 1
    cur = open(node).read().strip()
    print(f"EDID builtin group: {cur}")
    return 0


def main():
    import os
    parser = argparse.ArgumentParser(prog="hdmirx_edid.py",
                                     description="TTBOX EDID 工具（独立于 yu）")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--list", action="store_true", help="列出支持的 EDID 模式")
    group.add_argument("--status", action="store_true", help="读取当前 EDID 状态")
    group.add_argument("--profile", metavar="NAME", help="用 profile 生成 EDID")
    group.add_argument("--native", metavar="MODE", help="用首选模式生成 EDID（内置或 WxH@Hz）")
    group.add_argument("--builtin", metavar="1|2", help="切驱动内置 EDID 组")
    parser.add_argument("--device", default="/dev/video0")
    parser.add_argument("--add", action="append", default=[], metavar="MODE",
                        help="追加模式（可多次）")
    parser.add_argument("--name", default=None, help="显示器名 ≤13 字符")
    parser.add_argument("--vendor", default=None, help="3 字母厂商")
    parser.add_argument("--product-id", default=None, help="16 位产品 ID")
    parser.add_argument("--serial", default=None, help="32 位序列号")
    parser.add_argument("--apply", action="store_true", help="生成后注入 (VIDIOC_S_EDID)")
    parser.add_argument("--output", default=None, help="EDID 输出路径")
    args = parser.parse_args()
    if args.list:
        cmd_list()
        return 0
    if args.status:
        return cmd_status(args.device)
    if args.builtin:
        return cmd_builtin(args.builtin)
    if args.profile or args.native:
        return cmd_build(args.profile, args.native, args.add,
                         args.name, args.vendor, args.product_id,
                         args.serial, args.device, args.apply, args.output)
    return 1


if __name__ == "__main__":
    sys.exit(main())
