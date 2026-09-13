import socket,struct,time
# 0x4F50 / 1 / 6 (GET_STATE) -> resp 9B: <BQ mask, ts_ns
s=socket.socket(socket.AF_UNIX,socket.SOCK_SEQPACKET)
s.connect("/run/ttbox-mouse-passthrough/cmd.sock")
s.settimeout(3)
hdr=struct.pack("<HBB",0x4F50,1,6)+struct.pack("<I",1)
s.sendall(hdr)
r=s.recv(128)
magic,ver,typ=struct.unpack_from("<HBB",r,0)
rid=struct.unpack_from("<I",r,4)[0]
mask=r[8]
ts=struct.unpack_from("<q",r,9)[0]
print(f"GET_STATE magic=0x{magic:04x} type={typ} mask={mask:#04x} ts_ns={ts}")
s.close()
# 连续两次 MOVE 验证 pending 计数行为
s=socket.socket(socket.AF_UNIX,socket.SOCK_SEQPACKET)
s.connect("/run/ttbox-mouse-passthrough/cmd.sock")
s.settimeout(3)
for i in range(3):
    pkt=struct.pack("<HBB",0x4F50,1,4)+struct.pack("<I",100+i)+struct.pack("<iii",17,-9,0)
    s.sendall(pkt)
    time.sleep(0.1)
print("sent 3 test MOVE_CMD (17,-9)")
s.close()
