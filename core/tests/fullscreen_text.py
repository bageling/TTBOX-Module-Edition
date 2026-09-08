# fullscreen_text.py — 全屏显示 HDMI_VIRTUAL_TEST_123456 巨大白字（黑底），验证 HDMI 虚拟显示器链路
import tkinter as tk
import sys, time

seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
TEXT = "HDMI_VIRTUAL_TEST_123456"

root = tk.Tk()
root.attributes("-fullscreen", True)
root.attributes("-topmost", True)
root.overrideredirect(True)
root.configure(bg="black")

# 巨大文字占满视野
label = tk.Label(root, text=TEXT, fg="white", bg="black",
                 font=("Consolas", 160, "bold"))
label.pack(expand=True)

root.update()
print(f"FULLSCREEN_TEXT showing {TEXT} for {seconds}s", flush=True)
root.after(int(seconds * 1000), root.destroy)
root.mainloop()
print("done", flush=True)