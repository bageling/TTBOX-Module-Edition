import socket,json
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
m=gstatus().get("data",{}).get("metrics",{})
for k in ("aim_has_target","aim_target_class_id","aim_target_x1","aim_target_y1","aim_target_x2","aim_target_y2",
          "aim_error_x","aim_error_y","target_point_x","target_point_y","reference_x","reference_y",
          "pid_output_x","pid_output_y","last_mouse_control_dx","last_mouse_control_dy",
          "mouse_control_send_count","injection_allowed","screen_w","screen_h"):
    if k in m: print(k,"=",m[k])
