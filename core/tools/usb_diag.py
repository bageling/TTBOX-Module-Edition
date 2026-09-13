#!/usr/bin/env python3
import socket, struct, os, sys, subprocess, json

# 1. cmd.sock test
print("=== cmd.sock SEQPACKET test ===")
s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
try:
    s.connect("/run/ttbox-mouse-passthrough/cmd.sock")
    print("  connect OK")
    hdr = struct.pack("<HBB I", 0x4F50, 1, 1, 1)
    s.send(hdr)
    s.settimeout(2)
    resp = s.recv(4096)
    print(f"  recv {len(resp)} bytes, hdr_type={resp[3]}")
    s.close()
except Exception as e:
    print(f"  FAILED: {e}")
    try: s.close()
    except: pass

# 2. ttbox_core status via raw socket
print()
print("=== ttbox_core GET_STATUS (IPC socket) ===")
s2 = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
try:
    s2.connect("/tmp/ttbox_core.sock")
    msg = json.dumps({"type": "GET_STATUS"}).encode()
    s2.send(msg)
    s2.settimeout(3)
    chunks = []
    while True:
        try:
            chunk = s2.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
        except socket.timeout:
            break
    raw = b"".join(chunks)
    if raw:
        data = json.loads(raw)
        d = data.get("data", {})
        m = d.get("metrics", {})
        for k in ["detect_count","injection_allowed","mouse_control_connected",
                  "mouse_control_send_count","capture_fps","dropped_frames",
                  "gated_frames","aim_active","last_mouse_control_dx","last_mouse_control_dy",
                  "last_mouse_control_timestamp_us"]:
            print(f"  {k}: {m.get(k)}")
        print(f"  runtime_running: {d.get('runtime_running')}")
        print(f"  running: {d.get('running')}")
    else:
        print("  no data received")
    s2.close()
except Exception as e:
    print(f"  FAILED: {e}")
    try: s2.close()
    except: pass

# 3. DWC3 / typec
print()
print("=== DWC3 ===")
for f in ["state","uevent","maximum_speed","current_speed"]:
    p = f"/sys/class/udc/fc000000.usb/{f}"
    try: print(f"  {f}: {open(p).read().strip()}")
    except: pass

print()
print("=== typec ===")
for f in ["data_role","power_role"]:
    p = f"/sys/class/typec/port0/{f}"
    try: print(f"  {f}: {open(p).read().strip()}")
    except: pass

partner = "/sys/class/typec/port0-partner"
print(f"  partner exists: {os.path.isdir(partner)}")
if os.path.isdir(partner):
    print(f"  partner contents: {os.listdir(partner)}")

# 4. USB devices
print()
print("=== USB devices ===")
usb = "/sys/bus/usb/devices"
for name in sorted(os.listdir(usb)):
    vd = os.path.join(usb, name, "idVendor")
    if not os.path.exists(vd): continue
    vid = open(vd).read().strip()
    pid = open(os.path.join(usb, name, "idProduct")).read().strip()
    try: spd = open(os.path.join(usb, name, "speed")).read().strip()
    except: spd = "?"
    try: prod = open(os.path.join(usb, name, "product")).read().strip()
    except: prod = ""
    try: cls = open(os.path.join(usb, name, "bDeviceClass")).read().strip()
    except: cls = "?"
    print(f"  {name}: {vid}:{pid} class={cls} spd={spd} {prod}")

# 5. dmesg
print()
print("=== dmesg usb (tail) ===")
r2 = subprocess.run(["dmesg"], capture_output=True, text=True)
for line in r2.stdout.splitlines()[-60:]:
    ll = line.lower()
    if any(k in ll for k in ["usb","gadget","raw","dwc","hidg"]):
        print(f"  {line}")
