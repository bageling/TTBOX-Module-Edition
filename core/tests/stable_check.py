import socket,json,time
s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
s.connect("/tmp/ttbox_core.sock")
s.settimeout(4)
out=[]
for i in range(6):
    try:
        s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
        r=s.recv(65536)
        m=json.loads(r).get("data",{}).get("metrics",{})
        dc=m.get("detect_count",0)
        boxes=m.get("detection_boxes") or []
        b=boxes[0] if boxes else {}
        cx=(b.get("x1",0)+b.get("x2",0))/2
        cy=(b.get("y1",0)+b.get("y2",0))/2
        out.append("i=%d dc=%d cls=%s conf=%.3f box=[%.0f,%.0f->%.0f,%.0f] c=(%.0f,%.0f) cap=%.0f decode=%.1fms e2e=%.1fms"%(
            i,dc,b.get("class_id","-"),b.get("score",0),
            b.get("x1",0),b.get("y1",0),b.get("x2",0),b.get("y2",0),
            cx,cy,m.get("capture_fps",0),m.get("decode_ms",0),m.get("e2e_ms",0)))
    except Exception as ex:
        out.append("i=%d ERR %s"%(i,str(ex)[:80]))
        # 重连
        try: s.close()
        except: pass
        s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        s.connect("/tmp/ttbox_core.sock")
        s.settimeout(4)
    time.sleep(3)
print("\n".join(out))
s.close()
