#!/usr/bin/env python3
"""Create this demo's original geometric artwork using Pillow; no external assets."""
from pathlib import Path
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent / "geometric"
HERE.mkdir(parents=True, exist_ok=True)


def background(path, variant):
    im = Image.new("RGB", (1600, 900))
    px = im.load()
    for y in range(900):
        for x in range(1600):
            glow = max(0, 1 - (((x - 1090) / 850) ** 2 + ((y - 240) / 650) ** 2) ** 0.5)
            px[x, y] = (int(9 + 19 * glow), int(16 + (63 if variant else 38) * glow), int(38 + (85 if variant else 79) * glow))
    draw = ImageDraw.Draw(im, "RGBA")
    draw.polygon([(890, 0), (1600, 0), (1600, 665)], fill=(52, 149, 213, 26))
    draw.polygon([(1190, 0), (1600, 0), (1600, 405)], fill=(107, 214, 233, 34))
    draw.ellipse((1020, 150, 1470, 600), outline=(151, 226, 246, 105), width=4)
    draw.arc((890, 30, 1580, 720), 205, 330, fill=(124, 208, 250, 85), width=3)
    im.save(path, "PNG")


def icon(name):
    im = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((8, 8, 120, 120), radius=29, fill=(36, 92, 126, 235), outline=(133, 226, 240, 240), width=3)
    c = (238, 251, 255, 255)
    if name == "search":
        d.ellipse((34, 28, 78, 72), outline=c, width=7); d.line((72, 68, 95, 92), fill=c, width=8)
    elif name == "home":
        d.line((31, 63, 64, 34, 97, 63), fill=c, width=7, joint="curve"); d.line((41, 59, 41, 94, 87, 94, 87, 59), fill=c, width=7)
    elif name == "controls":
        for y, x in ((42, 47), (64, 80), (86, 54)):
            d.line((31, y, 97, y), fill=c, width=5); d.ellipse((x-6, y-6, x+6, y+6), fill=c)
    elif name == "refresh":
        d.arc((33, 32, 95, 94), 30, 310, fill=c, width=7); d.polygon(((86, 30), (101, 32), (95, 48)), fill=c)
    elif name == "settings":
        d.ellipse((37, 37, 91, 91), outline=c, width=8); d.ellipse((57, 57, 71, 71), fill=c)
        for a, b in (((64, 25), (64, 39)), ((64, 89), (64, 103)), ((25, 64), (39, 64)), ((89, 64), (103, 64))):
            d.line((*a, *b), fill=c, width=7)
    elif name == "library":
        for x in (32, 51, 70, 89): d.rounded_rectangle((x, 34, x+10, 94), radius=3, fill=c)
    else:
        d.polygon(((64, 29), (97, 52), (85, 92), (43, 92), (31, 52)), fill=c)
    im.save(HERE / (name + ".png"), "PNG")


background(HERE / "background.png", False)
background(HERE / "hero.png", True)
for item in ("search", "home", "controls", "refresh", "settings", "library", "example"):
    icon(item)
print("Created original geometric demo images in", HERE)
