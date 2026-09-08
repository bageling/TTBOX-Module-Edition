import socket,json,base64
s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
s.connect("/tmp/ttbox_core.sock")
s.settimeout(5)
req=json.dumps({"type":"GET_PREVIEW"}).encode()+"\n".encode()
s.sendall(req)
data=b""
while True:
    try:
        chunk=s.recv(65536)
        if not chunk: break
        data+=chunk
    except socket.timeout: break
open("/tmp/preview_raw.bin","wb").write(data)
print("received bytes:",len(data))
try:
    d=json.loads(data.decode(errors="replace"))
    print("json keys:", list(d.keys()) if isinstance(d,dict) else type(d))
    txt=json.dumps(d,ensure_ascii=False)
    print("head:",txt[:200])
    # 尝试找到 base64 图像字段
    if isinstance(d,dict):
        inner=d.get("data",{})
        if isinstance(inner,dict):
            for k in ("jpeg_base64","base64","image","jpeg"):
                if k in inner:
                    v=inner[k]
                    if isinstance(v,str) and len(v)>100:
                        try:
                            raw=base64.b64decode(v)
                            open("/tmp/preview_get.jpg","wb").write(raw)
                            print("saved jpeg inner field",k,len(raw))
                        except Exception as ex: print("b64 fail inner",k,ex)
        for k in ("data","preview","image","jpeg","jpg","base64","png"):
            if k in d:
                v=d[k]
                if isinstance(v,str) and len(v)>100:
                    try:
                        raw=base64.b64decode(v)
                        open("/tmp/preview_get2.jpg","wb").write(raw)
                        print("saved jpeg field",k,len(raw))
                    except Exception as ex: print("b64 fail",k,ex)
except Exception as ex:
    print("not json:",str(ex)[:100])
s.close()
