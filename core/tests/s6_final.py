import socket,json
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
m=gstatus().get("data",{}).get("metrics",{})
print("injection_allowed=",m.get("injection_allowed"))
print("mouse_control_connected=",m.get("mouse_control_connected"))
print("send_count=",m.get("mouse_control_send_count"))
print("write_ok=",m.get("mouse_control_socket_write_ok"))
print("write_fail=",m.get("mouse_control_socket_write_fail"))
print("last_dx=",m.get("last_mouse_control_dx"),"last_dy=",m.get("last_mouse_control_dy"))
