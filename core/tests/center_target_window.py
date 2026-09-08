# center_target_window.py — 在主屏(DISPLAY1)打开一个居中窗口显示测试目标图，
# 使目标中心对准 HDMI 帧中心 (1920,1080)，模型中心裁剪区正好覆盖目标
# 用法: python center_target_window.py <image_path> [w] [h] [seconds]
import tkinter as tk
import sys
from PIL import Image, ImageTk

seconds = float(sys.argv[4]) if len(sys.argv) > 4 else 600.0
img_path = sys.argv[1]
w = int(sys.argv[2]) if len(sys.argv) > 2 else 1280
h = int(sys.argv[3]) if len(sys.argv) > 3 else 720

# HDMI 帧 3840x2160，主屏 2560x1440 位于左 2/3
# 模型中心裁剪区 = HDMI 帧中心 640x640 = (1600,760)-(2240,1400)
# 主屏窗口中心要对准 (1920,1080) -> 主屏坐标 (x0+w/2, y0+h/2) = (1920,1080)
x0 = 1920 - w // 2
y0 = 1080 - h // 2
print(f"window {w}x{h} at ({x0},{y0}) -> center (1920,1080) = HDMI frame center", flush=True)

root = tk.Tk()
root.overrideredirect(True)
root.geometry(f"{w}x{h}+{x0}+{y0}")
root.attributes("-topmost", True)
root.configure(bg="black")

img = Image.open(img_path)
img = img.resize((w, h), Image.LANCZOS)
photo = ImageTk.PhotoImage(img)
label = tk.Label(root, image=photo, bg="black")
label.place(x=0, y=0, relwidth=1, relheight=1)
root.update()
print(f"CENTER_TARGET showing {img_path} {w}x{h} for {seconds}s", flush=True)
root.after(int(seconds * 1000), root.destroy)
root.mainloop()
print("done", flush=True)
