import socket,struct,time,sys
hz=int(sys.argv[1]) if len(sys.argv)>1 else 1000
dur=float(sys.argv[2]) if len(sys.argv)>2 else 1.0
dx=int(sys.argv[3]) if len(sys.argv)>3 else 1
s=socket.socket(socket.AF_UNIX,socket.SOCK_SEQPACKET)
s.connect("/run/orangepi-mouse-passthrough/cmd.sock")
s.settimeout(2)
period=1.0/hz
start=time.time()
sent=0
end=start+dur
while time.time()<end:
    pkt=struct.pack("<HBBI",0x4F50,1,4,sent+1)+struct.pack("<iii",dx,0,0)
    s.sendall(pkt)
    sent+=1
    # 尽力维持目标频率
    next_t=start+sent*period
    sleep=next_t-time.time()
    if sleep>0: time.sleep(sleep)
elapsed=time.time()-start
print("HZ=%d sent=%d elapsed=%.3fs actual_hz=%.1f"%(hz,sent,elapsed,sent/elapsed))
s.close()
