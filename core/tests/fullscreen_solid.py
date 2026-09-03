# fullscreen_solid.py — 全屏显示纯色，用于验证 PC 屏幕 HDMI -> 板子 HDMI RX 连接链路
import sys, time, tkinter as tk

color = sys.argv[1] if len(sys.argv) > 1 else "#ff0000"  # 默认纯红
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0

root = tk.Tk()
root.attributes("-fullscreen", True)
root.configure(bg=color)
# 强制置顶
root.attributes("-topmost", True)
root.overrideredirect(True)
print(f"FULLSCREEN color={color} for {seconds}s", flush=True)
root.update()
root.after(int(seconds * 1000), root.destroy)
root.mainloop()
print("done", flush=True)