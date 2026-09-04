import socket,json,time
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
d=gstatus()
m=d.get("data",{}).get("metrics",{})
print("== 链路指标 ==")
for k in ("capture_fps","inference_fps","infer_ms","infer_input_ms","infer_output_ms","decode_ms","e2e_ms","detect_count","target_frames","no_target_frames"):
    if k in m: print(f"  {k}: {m[k]}")
print("== 检测 ==")
print("  boxes:", json.dumps(m.get("detection_boxes"),ensure_ascii=False))
print("  target:", json.dumps({k:m.get(k) for k in ("aim_has_target","aim_target_class_id","aim_target_x1","aim_target_y1","aim_target_x2","aim_target_y2","aim_target_width","aim_target_height")},ensure_ascii=False))
print("== 安全 ==")
print("  injection_allowed:", m.get("injection_allowed"), "mouse_control_connected:", m.get("mouse_control_connected"), "send_count:", m.get("mouse_control_send_count"))
print("  pid_output:", m.get("pid_output_x"), m.get("pid_output_y"), "mouse_dx/dy:", m.get("mouse_dx"), m.get("mouse_dy"))
