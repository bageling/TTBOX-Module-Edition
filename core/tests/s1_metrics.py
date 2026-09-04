import socket,json
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
m=gstatus().get("data",{}).get("metrics",{})
b=(m.get("detection_boxes") or [{}])[0]
tgt={k:m.get(k) for k in ("aim_has_target","aim_target_class_id","aim_target_x1","aim_target_y1","aim_target_x2","aim_target_y2")}
print("detect_count=",m.get("detect_count"))
print("boxes=",json.dumps(b,ensure_ascii=False))
print("target=",json.dumps(tgt))
print("injection_allowed=",m.get("injection_allowed"),"connected=",m.get("mouse_control_connected"),"send_count=",m.get("mouse_control_send_count"))
print("pid_out=",m.get("pid_output_x"),m.get("pid_output_y"))
print("screen_center_hint: 1280,720")
bx1,by1,bx2,by2=b.get("x1",0),b.get("y1",0),b.get("x2",0),b.get("y2",0)
print("bbox_center=",round((bx1+bx2)/2,1),round((by1+by2)/2,1))
print("error_x_est=",round((bx1+bx2)/2-1280,1),"error_y_est=",round((by1+by2)/2-720,1))
