# fullscreen_anim_pan.py — 全屏滑动动画：street 图在屏幕内左右平移，bus 目标持续移动
# 用途：为 PID Trace 分析提供"目标持续移动"的真实 HDMI 画面（yolo261n 可稳定检测 bus）
# 用法: python fullscreen_anim_pan.py [秒] [振幅px] [周期s]
import tkinter as tk
import sys, time, math
from PIL import Image, ImageTk

seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 60.0
amp = float(sys.argv[2]) if len(sys.argv) > 2 else 120.0   # 平移振幅（px）
period = float(sys.argv[3]) if len(sys.argv) > 3 else 8.0  # 往返周期（s）

IMG = r"C:/Users/Administrator/Desktop/TTBOX/core/tests/hdmi_street_full.png"
img = Image.open(IMG).convert("RGB")
W, H = img.size  # 2560x1440

root = tk.Tk()
root.attributes("-fullscreen", True)
root.attributes("-topmost", True)
root.overrideredirect(True)
canvas = tk.Canvas(root, width=W, height=H, highlightthickness=0)
canvas.pack()

# 预渲染平移帧（振幅范围内）
frames = []
steps = 48  # 一个周期的帧数
for i in range(steps):
    off = amp * math.sin(2.0 * math.pi * i / steps)
    x = int(round(off))
    shifted = img.transform((W, H), Image.AFFINE, (1, 0, -x, 0, 1, 0), resample=Image.BILINEAR)
    frames.append(ImageTk.PhotoImage(shifted))

print(f"FULLSCREEN_ANIM_PAN: {W}x{H} amp={amp}px period={period}s steps={steps}", flush=True)

t0 = time.time()
idx = 0
def tick():
    global idx
    dt = time.time() - t0
    if dt >= seconds:
        root.destroy()
        return
    idx = int((dt / period) * steps) % steps
    canvas.delete("all")
    canvas.create_image(0, 0, anchor="nw", image=frames[idx])
    root.after(33, tick)  # ~30fps

root.after(50, tick)
root.mainloop()
print("done", flush=True)
