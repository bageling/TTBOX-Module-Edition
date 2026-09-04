import socket,json,base64
def gstatus():
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
    s.connect("/tmp/ttbox_core.sock"); s.settimeout(4)
    s.sendall(json.dumps({"type":"GET_STATUS"}).encode()+b"\n")
    r=s.recv(65536); s.close()
    return json.loads(r)
d=gstatus()
data=d.get("data",{})
print("== 安全相关字段 ==")
for k in ("output_enabled","injection_allowed","mouse_enabled","calibrating","aim_has_target"):
    # 在 data 顶层与 data.mouse 里找
    for src,name in ((data,k),(data.get("mouse",{}),k)):
        if k in src:
            print(f"{name}: {src[k]}")
            break
print("== full keys ==")
def walk(o,pre=""):
    if isinstance(o,dict):
        for k,v in o.items():
            if any(x in k.lower() for x in ("enabled","allow","mouse","output","inject","calib","hotkey","target","safe")):
                print(f"{pre}{k}: {v if not isinstance(v,(dict,list)) else type(v).__name__}")
            if isinstance(v,(dict,list)): walk(v,pre+k+".")
walk(d)
