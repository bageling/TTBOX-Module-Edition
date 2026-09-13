#!/usr/bin/env python3
"""板端验证：usbproxy BUTTON_CMD -> event.sock STATE_SNAPSHOT 按钮掩码链路。"""
import socket, struct, sys, time

CMD_SOCK = "/run/ttbox-mouse-passthrough/cmd.sock"
EVENT_SOCK = "/run/ttbox-mouse-passthrough/event.sock"


def hdr(typ, rid):
    return struct.pack("<HBB", 0x4F50, 1, typ) + struct.pack("<I", rid)


def get_state():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    s.settimeout(3)
    s.connect(CMD_SOCK)
    s.sendall(hdr(6, 1))
    r = s.recv(128)
    s.close()
    assert len(r) >= 17 and r[3] == 7, "GET_STATE response abnormal: %s" % r.hex()
    return r[8]


def send_button(button, action):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    s.settimeout(3)
    s.connect(CMD_SOCK)
    s.sendall(hdr(5, 2) + struct.pack("<BB", button, action))
    s.close()


def read_snapshot(ev, timeout=2.0):
    ev.settimeout(timeout)
    r = ev.recv(128)
    assert r[3] == 10, "expected STATE_SNAPSHOT, got %s" % r.hex()
    return r[8]


def main():
    ev = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    ev.settimeout(3)
    ev.connect(EVENT_SOCK)
    ev.sendall(hdr(8, 3))
    ack = ev.recv(64)
    assert ack[3] == 9, "subscribe ACK abnormal: %s" % ack.hex()

    print("initial get_state mask=%#04x" % get_state())
    print("initial snapshot mask=%#04x" % read_snapshot(ev))

    send_button(2, 1)  # button2 down
    time.sleep(0.15)
    down_masks = [read_snapshot(ev) for _ in range(3)]
    print("after down snapshot masks=%s" % ["%#04x" % m for m in down_masks])

    send_button(2, 2)  # button2 up
    time.sleep(0.15)
    up_masks = [read_snapshot(ev) for _ in range(3)]
    print("after up snapshot masks=%s" % ["%#04x" % m for m in up_masks])
    print("final get_state mask=%#04x" % get_state())

    ok = all(m == 0x02 for m in down_masks) and all(m == 0x00 for m in up_masks)
    print("BUTTON_CMD->SNAPSHOT %s" % ("PASS" if ok else "FAIL"))
    ev.close()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
