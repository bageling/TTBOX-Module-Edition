# fullscreen_image_screen.py — 在指定屏幕(虚拟屏 DISPLAY2)全屏显示测试目标图
# 用法: python fullscreen_image_screen.py <image_path> <screen_index> [seconds]
# 通过 tkinter geometry 定位到跨屏坐标，确保目标显示在虚拟屏(HDMI输出)上
import tkinter as tk
import sys, time
from PIL import Image, ImageTk

seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 120.0
img_path = sys.argv[1]
screen_idx = int(sys.argv[2]) if len(sys.argv) > 2 else 1  # 默认 DISPLAY2 虚拟屏

import ctypes
# 枚举屏幕
user32 = ctypes.windll.user32
n = user32.GetSystemMetrics(80)  # SM_CMONITORS
monitors = []
class MONITORINFO(ctypes.Structure):
    _fields_ = [("cbSize", ctypes.c_uint32),
                ("rcMonitor", ctypes.c_long * 4),
                ("rcWork", ctypes.c_long * 4),
                ("dwFlags", ctypes.c_uint32)]
def monitor_enum_proc(hmon, hdc, lprc, dw):
    mi = MONITORINFO()
    mi.cbSize = ctypes.sizeof(MONITORINFO)
    if user32.GetMonitorInfoW(hmon, ctypes.byref(mi)):
        monitors.append(tuple(mi.rcMonitor))
    return True
MONITORENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
user32.EnumDisplayMonitors(None, None, MONITORENUMPROC(monitor_enum_proc), None)
print("monitors:", monitors, flush=True)

if screen_idx >= len(monitors):
    print(f"screen index {screen_idx} out of range, using last", flush=True)
    screen_idx = len(monitors) - 1
x, y, w, h = monitors[screen_idx]
print(f"target screen {screen_idx}: {w}x{h} at ({x},{y})", flush=True)

root = tk.Tk()
root.overrideredirect(True)
root.geometry(f"{w}x{h}+{x}+{y}")
root.attributes("-topmost", True)
root.configure(bg="black")

img = Image.open(img_path)
# 缩放到目标屏幕分辨率（拉伸铺满）
img = img.resize((w, h), Image.LANCZOS)
photo = ImageTk.PhotoImage(img)
label = tk.Label(root, image=photo, bg="black")
label.place(x=0, y=0, relwidth=1, relheight=1)
root.update()
print(f"FULLSCREEN_IMAGE_SCREEN showing {img_path} on screen {screen_idx} ({w}x{h}) for {seconds}s", flush=True)
root.after(int(seconds * 1000), root.destroy)
root.mainloop()
print("done", flush=True)
