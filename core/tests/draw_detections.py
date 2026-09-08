# draw_detections.py — 把 test_decode_coords 输出的 5 组真实原图坐标画到 2560×1440 画布
from PIL import Image, ImageDraw, ImageFont

W, H = 2560, 1440
img = Image.new("RGB", (W, H), (20, 30, 40))
d = ImageDraw.Draw(img)

# 圆心（ROI 中心对应模型中心）
d.ellipse([W/2-8, H/2-8, W/2+8, H/2+8], fill=(255,255,0))

# (label, (x1,y1,x2,y2), cls, conf)
dets = [
    ("CENT", (1225.3, 665.3, 1350.3, 790.3), 0, 0.862),
    ("TL",   (1053.4, 493.4, 1147.2, 587.2), 1, 0.862),
    ("TR",   (1428.4, 493.4, 1522.2, 587.2), 2, 0.862),
    ("BL",   (1053.4, 868.4, 1147.2, 962.2), 3, 0.862),
    ("BR",   (1428.4, 868.4, 1522.2, 962.2), 4, 0.862),
]
colors = [(0,255,0),(0,180,255),(255,160,0),(255,80,200),(180,255,0)]
for label, (x1,y1,x2,y2), cls, conf in dets:
    c = colors[cls]
    d.rectangle([x1,y1,x2,y2], outline=c, width=4)
    cx, cy = (x1+x2)/2, (y1+y2)/2
    d.ellipse([cx-6,cy-6,cx+6,cy+6], fill=c)
    d.text((x1, y1-14), f"{label} class{cls} {conf:.3f}", fill=c)
    d.text((cx+10, cy-6), f"({cx:.0f},{cy:.0f})", fill=c)

# 边框标注
d.text((10, 10), "TTBOX coordinate-loopback: model 256x256 INT8 -> DecodeNMS -> frame 2560x1440", fill=(255,255,255))

out = r"C:/Users/Administrator/Desktop/TTBOX/core/tests/out_decode_detections.png"
img.save(out)
print("saved", out)