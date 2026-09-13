"""TTBOX EDID Monitor — 读取真实显示器信息（DRM connector + hdmirx RX 状态）。
数据来自内核 /sys/class/drm 与 hdmirx sysfs。
"""
import glob
import os
import struct
import subprocess


def _read_text(path, limit=4096):
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            return f.read(limit).strip()
    except OSError:
        return ""


def _read_bytes(path, limit=512):
    try:
        with open(path, "rb") as f:
            return f.read(limit)
    except OSError:
        return b""


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


def _descriptor_text(edid, offset):
    raw = bytes(ch for ch in edid[offset + 5:offset + 18] if 32 <= ch <= 126)
    return raw.decode("ascii", "ignore").split("\n", 1)[0].strip()


def parse_edid_identity(edid):
    """从 EDID 二进制解析 identity，返回 dict 或 None。"""
    if len(edid) < 128 or not edid.startswith(b"\x00\xff\xff\xff\xff\xff\xff\x00"):
        return None
    vendor = _decode_vendor(edid)
    pid = struct.unpack("<H", edid[10:12])[0]
    ser = struct.unpack("<I", edid[12:16])[0]
    name = ""
    for off in (54, 72, 90, 108, 126):
        if off + 18 <= len(edid) and edid[off] == 0 and edid[off + 1] == 0 and edid[off + 3] == 0xFC:
            name = _descriptor_text(edid, off)
            break
    return {
        "vendor": vendor,
        "product_id": f"0x{pid:04x}",
        "serial": f"0x{ser:08x}",
        "name": name,
    }


def read_connector_modes(base):
    """读取 connector 的 modes 列表。"""
    modes = []
    text = _read_text(os.path.join(base, "modes"), 65536)
    if not text:
        return modes
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        w = h = r = 0
        for tok in line.split():
            if "x" in tok and "@" in tok:
                try:
                    dim, hz = tok.split("@")
                    w, h = dim.split("x")
                    w, h, r = int(w), int(h), int(float(hz))
                except Exception:
                    pass
        if w and h:
            modes.append({"width": w, "height": h, "refresh": r, "text": line})
    return modes


def read_hdmirx_status():
    """读取 RK3588 HDMI-RX 真实状态，不改变任何硬件节点。"""
    base = "/sys/devices/platform/fdee0000.hdmirx-controller/hdmirx/hdmirx"
    status = _read_text(os.path.join(base, "status"))
    edid_group = _read_text(os.path.join(base, "edid"))
    debug = _read_text("/sys/kernel/debug/hdmirx/status", 8192)
    lock_line = next((line.strip() for line in debug.splitlines() if line.startswith("Clk-Ch:")), "")
    timing = next((line.strip() for line in debug.splitlines() if line.startswith("Timing:")), "")
    mode = next((line.strip() for line in debug.splitlines() if line.startswith("Mode:")), "")
    locked = bool(lock_line) and "Unlock" not in lock_line
    readback = {"valid": False, "size": 0, "name": "", "vendor": "", "product_id": "", "serial": "", "source": ""}
    for dev in ("/dev/video0", "/dev/video1", "/dev/video2"):
        if not os.path.exists(dev):
            continue
        try:
            r = subprocess.run(
                ["v4l2-ctl", "-d", dev, "--get-edid=pad=0,format=raw"],
                capture_output=True, timeout=5,
            )
            raw = r.stdout if r.returncode == 0 else b""
        except (OSError, subprocess.SubprocessError):
            raw = b""
        identity = parse_edid_identity(raw) if raw else None
        if identity:
            readback.update(identity)
            readback.update({"valid": True, "size": len(raw), "source": dev})
            break
    return {
        "connected": status == "connected", "status": status,
        "edid": edid_group, "edid_group": edid_group,
        "edid_readback": readback,
        "locked": locked, "lock_channels": lock_line, "timing": timing, "mode": mode,
    }

def read_real_monitor_info():
    """读取真实显示器信息（DRM connector）。"""
    result = {
        "connected": False, "edid_valid": False,
        "name": "unknown", "vendor": "???",
        "product_id": "0x0000", "serial": "0x00000000",
        "native_width": 0, "native_height": 0, "modes": [],
    }
    connectors = sorted(glob.glob("/sys/class/drm/*HDMI*"))
    connected = [c for c in connectors if _read_text(os.path.join(c, "status")) == "connected"]
    if not connected:
        return result
    base = connected[0]
    result["connected"] = True
    result["connector"] = os.path.basename(base)
    edid = _read_bytes(os.path.join(base, "edid"), 512)
    identity = parse_edid_identity(edid)
    if identity:
        result.update(identity)
        result["edid_valid"] = True
    modes = read_connector_modes(base)
    result["modes"] = modes
    if modes:
        best = max(modes, key=lambda m: (m["width"] * m["height"], m["refresh"]))
        result["native_width"] = best["width"]
        result["native_height"] = best["height"]
    return result


def monitor_has_resolution(monitor, width, height):
    """显示器是否支持该分辨率。"""
    return any(m.get("width") == width and m.get("height") == height
               for m in monitor.get("modes", []))


def monitor_supports_refresh(monitor, width, height, refresh):
    """显示器在该分辨率下是否支持该刷新率（refresh-1 内）。"""
    saw_unknown = False
    for m in monitor.get("modes", []):
        if m.get("width") != width or m.get("height") != height:
            continue
        r = int(m.get("refresh") or 0)
        if r == 0:
            saw_unknown = True
            continue
        if r + 1 >= refresh:
            return True
    return saw_unknown and refresh <= 60


def at_or_below_native(monitor, width, height):
    """是否不超过显示器原生分辨率。"""
    nw = int(monitor.get("native_width") or 0)
    nh = int(monitor.get("native_height") or 0)
    if nw <= 0 or nh <= 0:
        return True
    return width <= nw and height <= nh
