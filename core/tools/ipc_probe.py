# 板端诊断：打印 GET_STATUS 原始结构
import json, socket

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(2.0)
s.connect('/tmp/ttbox_core.sock')
s.sendall(json.dumps({'type': 'GET_STATUS'}).encode() + b'\n')
data = b''
while True:
    chunk = s.recv(65536)
    if not chunk:
        break
    data += chunk
    if b'\n' in data:
        break
s.close()
text = data.split(b'\n')[0].decode()
d = json.loads(text)
print('top keys:', sorted(d.keys()))
m = d.get('metrics')
if isinstance(m, dict):
    print('metrics keys (%d):' % len(m), sorted(m.keys()))
else:
    print('metrics type:', type(m), 'value:', str(m)[:300])
