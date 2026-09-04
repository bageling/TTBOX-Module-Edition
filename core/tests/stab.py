import socket,json,time
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
for i in range(10):
    m=gstatus().get("data",{}).get("metrics",{})
    t=m.get("aim_has_target")
    ex=m.get("aim_error_x",0); ey=m.get("aim_error_y",0)
    dx=m.get("last_mouse_control_dx",0); dy=m.get("last_mouse_control_dy",0)
    print(i, "tgt=",t,"err=(",round(ex,1),round(ey,1),") move=(",dx,dy,") send=",m.get("mouse_control_send_count"))
    time.sleep(1)
