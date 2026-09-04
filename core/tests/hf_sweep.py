import subprocess, time, os, paramiko
base = r"C:\Users\Administrator\Desktop\TTBOX-Module-Edition\core\tests"

def getpos():
    r = subprocess.run([os.path.join(base,"GetPos.exe")], capture_output=True, text=True)
    x,y = r.stdout.strip().split(",")
    return int(x), int(y)

def setpos(x,y):
    subprocess.run([os.path.join(base,"SetPos.exe"),str(x),str(y)], capture_output=True)

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect("192.168.0.53", username="root", password="orangepi", look_for_keys=False, allow_agent=False, timeout=15)

print("freq | sent | actual_move | loss | note")
for hz in [100, 500, 1000, 1500, 2000]:
    # 复位到 x=200 避免撞墙，每个频率用不同起始 x 避免墙干扰——统一 200 每次复位
    setpos(200, 720)
    time.sleep(0.4)
    x0,_ = getpos()
    # 板端发 1 秒 dx=1
    i,o,e = c.exec_command(f"python3 /tmp/hf_mover.py {hz} 1 1 2>&1")
    sent_out = o.read().decode(errors="replace")
    # 等发送完成 + 注入完成
    time.sleep(1.5)
    x1,_ = getpos()
    # 解析 sent
    sent = 0
    for tok in sent_out.split():
        if tok.startswith("sent="):
            sent = int(tok.split("=")[1])
    move = x1 - x0
    loss = max(0, sent - move)
    print(f"{hz:5d} | {sent:5d} | {move:7d} | {loss:5d} | {sent_out.strip()}")
c.close()
