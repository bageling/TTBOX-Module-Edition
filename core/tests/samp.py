import socket,json
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
m=gstatus().get("data",{}).get("metrics",{})
k=("aim_has_target","aim_target_class_id","aim_error_x","aim_error_y",
   "target_point_x","target_point_y","reference_x","reference_y",
   "pid_output_x","pid_output_y","last_mouse_control_dx","last_mouse_control_dy",
   "mouse_control_send_count","injection_allowed")
print("|".join(str(m.get(x)) for x in k))
