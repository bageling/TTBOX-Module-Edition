# fullscreen_multi_target.py — 第14阶段多目标场景 v3
# bus 在中心 ROI 内左右往返 + 周期性消失（验证丢失恢复）
import tkinter as tk
import sys, time, math
from PIL import Image, ImageTk

seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 120.0
amp = float(sys.argv[2]) if len(sys.argv) > 2 else 130.0
lost_interval = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0  # 每5s消失2s

BASE = r"C:/Users/Administrator/Desktop/TTBOX-Module-Edition/core/tests/hdmi_street_full.png"
BUS = r"C:/Users/Administrator/Desktop/rknn-3588-npu-yolo-accelerate-saturated-main/media/bus.jpg"

base = Image.open(BASE).convert("RGB")
W, H = base.size
bus = Image.open(BUS).convert("RGB").resize((460, 460), Image.LANCZOS)

root = tk.Tk()
root.attributes("-fullscreen", True)
root.attributes("-topmost", True)
root.overrideredirect(True)
canvas = tk.Canvas(root, width=W, height=H, highlightthickness=0)
canvas.pack()

steps = 60
frames = []
for i in range(steps):
    t = 2.0 * math.pi * i / steps
    frame = base.copy()
    bx = W // 2 + int(amp * math.sin(t))
    by = H // 2 - 120
    phase = (i / steps) * (lost_interval / (lost_interval + 2.0))
    if math.sin(phase * 2.0 * math.pi) > 0.2:
        frame.paste(bus, (bx - 230, by - 230))
    frames.append(ImageTk.PhotoImage(frame))

print(f"MULTI_TARGET_V3: amp={amp} lost={lost_interval}s", flush=True)
t0 = time.time()

def tick():
    dt = time.time() - t0
    if dt >= seconds:
        root.destroy()
        return
    idx = int(dt / 2.0 * steps) % steps
    canvas.delete("all")
    canvas.create_image(0, 0, anchor="nw", image=frames[idx])
    root.after(33, tick)

root.after(50, tick)
root.mainloop()
print("done", flush=True)
