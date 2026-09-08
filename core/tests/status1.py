import socket,json,time
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
m=gstatus().get("data",{}).get("metrics",{})
for k in ("detect_count","aim_has_target","aim_target_class_id","aim_target_x1","aim_target_y1","aim_target_x2","aim_target_y2",
          "injection_allowed","mouse_enabled","mouse_control_connected","mouse_control_send_count",
          "last_mouse_control_dx","last_mouse_control_dy","pid_output_x","pid_output_y",
          "target_frames","no_target_frames","capture_fps","decode_ms","e2e_ms"):
    if k in m: print(k,"=",m[k])
