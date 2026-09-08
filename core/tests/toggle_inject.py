import socket,json,sys
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
mode=sys.argv[1] if len(sys.argv)>1 else "off"
d=rpc({"type":"GET_CONFIG"})
prof=d.get("data",{}).get("runtime_profile",{})
m=prof.get("mouse",{})
if mode=="off":
    m["enabled"]=False; m["calibrating"]=False
else:
    m["enabled"]=True; m["calibrating"]=True
prof["mouse"]=m
r=rpc({"type":"SET_CONFIG","params":{"profile":prof}})
print("SET",mode,"status:",r.get("status"),r.get("error",""))
d2=rpc({"type":"GET_CONFIG"})
m2=d2.get("data",{}).get("runtime_profile",{}).get("mouse",{})
print("AFTER enabled=",m2.get("enabled"),"calibrating=",m2.get("calibrating"))
