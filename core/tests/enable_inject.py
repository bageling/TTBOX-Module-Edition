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
# GET_CONFIG
d=rpc({"type":"GET_CONFIG"})
prof=d.get("data",{}).get("runtime_profile",{})
m=prof.get("mouse",{})
print("BEFORE enabled=",m.get("enabled"),"calibrating=",m.get("calibrating"))
# 只改 mouse
m["enabled"]=True
m["calibrating"]=True
prof["mouse"]=m
# SET_CONFIG
r=rpc({"type":"SET_CONFIG","params":{"profile":prof}})
print("SET status:",r.get("status"),"error:",r.get("error",""))
# 确认
d2=rpc({"type":"GET_CONFIG"})
m2=d2.get("data",{}).get("runtime_profile",{}).get("mouse",{})
print("AFTER enabled=",m2.get("enabled"),"calibrating=",m2.get("calibrating"))
