import socket,json,time
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
m=gstatus().get("data",{}).get("metrics",{})
for k in ("injection_allowed","mouse_control_connected","mouse_control_send_count",
          "mouse_control_socket_write_ok","mouse_control_socket_write_fail",
          "last_mouse_control_dx","last_mouse_control_dy","last_mouse_control_wheel",
          "last_mouse_control_timestamp_us","pid_output_x","pid_output_y"):
    if k in m: print(k,"=",m[k])
