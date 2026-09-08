# fullscreen_anim.py — 全屏动画(循环换色+文本)，验证 HDMI 链路实时性
import tkinter as tk
import time

colors = ["#ff0000", "#00ff00", "#0000ff", "#ffffff", "#000000", "#ffff00"]
texts = ["ANIM_1", "ANIM_2", "ANIM_3", "ANIM_4", "ANIM_5", "ANIM_6"]

root = tk.Tk()
root.attributes("-fullscreen", True)
root.attributes("-topmost", True)
root.overrideredirect(True)
label = tk.Label(root, font=("Consolas", 200, "bold"))

idx = [0]
def tick():
    c = colors[idx[0] % len(colors)]
    t = texts[idx[0] % len(texts)]
    root.configure(bg=c)
    label.config(text=t, fg="#ffffff" if c not in ("#ffffff", "#ffff00") else "#000000", bg=c)
    idx[0] += 1
    root.after(500, tick)

label.pack(expand=True)
root.after(200, tick)
print("ANIM_START", flush=True)
root.after(30000, root.destroy)
root.mainloop()
print("ANIM_DONE", flush=True)