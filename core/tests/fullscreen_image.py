# fullscreen_image.py — 全屏显示一张图片文件（含中心 ROI 目标），验证 HDMI 真实链路 COCO 检测
# 用法: python fullscreen_image.py <image_path> [seconds]
import tkinter as tk
import sys, time, os

seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 120.0
img_path = sys.argv[1] if len(sys.argv) > 1 else r"C:/Users/Administrator/Desktop/TTBOX/core/tests/hdmi_street_full.png"

root = tk.Tk()
root.attributes("-fullscreen", True)
root.attributes("-topmost", True)
root.overrideredirect(True)
root.configure(bg="black")

try:
    photo = tk.PhotoImage(file=img_path)
except Exception as e:
    print(f"load fail: {e}", flush=True)
    sys.exit(1)

# 全屏自适应缩放（拉伸铺满屏幕，确保中心 ROI 覆盖图片中央主体）
sw = root.winfo_screenwidth()
sh = root.winfo_screenheight()
iw, ih = photo.width(), photo.height()
# 计算缩放比例（保持比例、cover 全屏）
scale = max(sw / iw, sh / ih)
nw, nh = int(iw * scale), int(ih * scale)
if hasattr(photo, 'subsample'):
    # PhotoImage 用 zoom/subsample 缩放（整数倍）
    pass

label = tk.Label(root, image=photo, bg="black")
label.place(x=0, y=0, relwidth=1, relheight=1)
# PhotoImage 固定为原始分辨率居中（不拉伸，保证目标清晰居中）
label.configure(image=photo)
root.update()
print(f"FULLSCREEN_IMAGE showing {img_path} ({iw}x{ih}) for {seconds}s, screen {sw}x{sh}", flush=True)
root.after(int(seconds * 1000), root.destroy)
root.mainloop()
print("done", flush=True)