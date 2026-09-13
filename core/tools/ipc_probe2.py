# 板端诊断：打印 GET_STATUS data 结构
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
d = json.loads(data.split(b'\n')[0].decode())
dd = d.get('data')
print('data type:', type(dd))
if isinstance(dd, dict):
    print('data keys:', sorted(dd.keys()))
    for k in dd:
        v = dd[k]
        if isinstance(v, dict):
            print('  %s: dict keys=%s' % (k, sorted(v.keys())[:50]))
        else:
            print('  %s: %r' % (k, str(v)[:100]))
    metrics = dd.get('metrics') if isinstance(dd.get('metrics'), dict) else {}
    print('preview:', {k: metrics.get(k) for k in ('preview_fps', 'preview_encode_ms', 'preview_width', 'preview_height', 'preview_dropped')})
    print('mouse:', {k: metrics.get(k) for k in ('mouse_control_connected', 'mouse_control_socket_write_ok', 'mouse_control_socket_write_fail', 'mouse_control_send_count', 'injection_allowed', 'gated_frames', 'mouse_control_send_count')})
elif isinstance(dd, list):
    print('data is list len', len(dd), 'first:', str(dd[0])[:300] if dd else '')
