import socket,json
def rpc(req):
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(5)
    s.sendall(json.dumps(req).encode()+b"\n")
    r=b""
    while True:
        try:
            ch=s.recv(65536)
            if not ch: break
            r+=ch
        except socket.timeout: break
    s.close()
    return json.loads(r.decode(errors="replace"))
# 真实热键模式：mouse.enabled=true, calibrating=false（需物理按键触发）
d=rpc({"type":"GET_CONFIG"})
prof=d.get("data",{}).get("runtime_profile",{})
m=prof.get("mouse",{})
m["enabled"]=True
m["calibrating"]=False
prof["mouse"]=m
r=rpc({"type":"SET_CONFIG","params":{"profile":prof}})
print("SET real-hotkey status:",r.get("status"),r.get("error",""))
d2=rpc({"type":"GET_CONFIG"})
m2=d2.get("data",{}).get("runtime_profile",{}).get("mouse",{})
print("enabled=",m2.get("enabled"),"calibrating=",m2.get("calibrating"),
      "aim_hotkey=",m2.get("aim_hotkey"),"aim_hotkey2=",m2.get("aim_hotkey2"),"mode=",m2.get("aim_hotkey_mode"))
