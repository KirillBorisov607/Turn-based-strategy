"""Draws spell icons in the style of the project's icons: rounded tile, colour gradient, pale shape with a dark outline."""
import math
from PIL import Image, ImageDraw, ImageFilter

S = 512          # drawn at 4x, saved at 128
OUT = r"C:\ProjectsUE\_icons"
INK = (55, 45, 60, 255)
W = 14           # outline width at 4x


def tile(top, bottom):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    grad = Image.new("RGBA", (S, S))
    gd = ImageDraw.Draw(grad)
    for y in range(S):
        t = y / (S - 1)
        gd.line([(0, y), (S, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([8, 8, S - 8, S - 8], radius=70, fill=255)
    img.paste(grad, (0, 0), mask)
    # soft inner light
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([60, 20, S - 60, S - 160], fill=(255, 255, 255, 40))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(40)))
    return img, mask


def finish(img, mask, name):
    # keep everything inside the rounded tile
    clipped = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    clipped.paste(img, (0, 0), mask)
    clipped.resize((128, 128), Image.LANCZOS).save(f"{OUT}\\{name}.png")
    print("saved", name)


def shape(draw, points, fill):
    draw.polygon(points, fill=fill, outline=INK, width=W)


def raise_rock():
    img, mask = tile((150, 135, 120), (95, 80, 70))
    d = ImageDraw.Draw(img)
    # ground cracks
    d.ellipse([70, 380, 440, 470], fill=(70, 55, 45, 255), outline=INK, width=W)
    # stone column (hexagonal pillar seen from the side)
    shape(d, [(170, 410), (170, 170), (256, 120), (342, 170), (342, 410), (256, 450)], (205, 205, 210, 255))
    shape(d, [(170, 170), (256, 120), (342, 170), (256, 215)], (235, 235, 240, 255))
    d.line([(256, 215), (256, 450)], fill=INK, width=W)
    # rising rubble and arrows
    for x, y, r in [(110, 360, 26), (400, 350, 22), (130, 300, 14), (390, 290, 12)]:
        d.ellipse([x - r, y - r, x + r, y + r], fill=(185, 180, 175, 255), outline=INK, width=8)
    d.line([(215, 260), (235, 300), (215, 340)], fill=(140, 140, 150, 255), width=8)
    d.line([(300, 250), (290, 300)], fill=(140, 140, 150, 255), width=8)
    finish(img, mask, "Icon_RaiseRock")


def shield_bash():
    img, mask = tile((110, 150, 200), (50, 75, 130))
    d = ImageDraw.Draw(img)
    # motion lines
    for y in (170, 250, 330):
        d.line([(60, y), (150, y)], fill=(225, 235, 250, 255), width=18)
    # round shield
    d.ellipse([150, 110, 450, 410], fill=(205, 210, 220, 255), outline=INK, width=W)
    d.ellipse([195, 155, 405, 365], fill=(150, 160, 180, 255), outline=INK, width=10)
    d.ellipse([265, 225, 335, 295], fill=(235, 220, 160, 255), outline=INK, width=10)
    # impact star
    cx, cy = 455, 150
    pts = []
    for i in range(10):
        a = math.pi / 5 * i
        r = 55 if i % 2 == 0 else 22
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    shape(d, pts, (255, 235, 140, 255))
    finish(img, mask, "Icon_ShieldBash")


def sprout():
    img, mask = tile((160, 210, 120), (70, 135, 60))
    d = ImageDraw.Draw(img)
    d.ellipse([110, 370, 400, 460], fill=(120, 85, 55, 255), outline=INK, width=W)
    d.line([(256, 410), (256, 230)], fill=INK, width=34)
    d.line([(256, 410), (256, 230)], fill=(120, 175, 80, 255), width=16)
    # two leaves
    shape(d, [(256, 250), (180, 150), (90, 170), (150, 245)], (190, 235, 140, 255))
    shape(d, [(256, 220), (320, 110), (420, 110), (370, 200)], (190, 235, 140, 255))
    d.line([(256, 250), (130, 185)], fill=INK, width=7)
    d.line([(256, 220), (380, 125)], fill=INK, width=7)
    for x, y in [(130, 330), (390, 300), (420, 380)]:
        d.ellipse([x - 12, y - 12, x + 12, y + 12], fill=(240, 255, 210, 255))
    finish(img, mask, "Icon_Sprout")


def roots():
    img, mask = tile((175, 150, 100), (90, 70, 45))
    d = ImageDraw.Draw(img)
    # a foot trapped by roots
    d.rounded_rectangle([170, 150, 340, 330], radius=60, fill=(205, 205, 215, 255), outline=INK, width=W)
    for i, (sx, sy, ex, ey) in enumerate([(80, 440, 380, 220), (430, 440, 140, 230), (60, 300, 450, 330), (250, 460, 250, 120)]):
        pts = []
        for k in range(21):
            t = k / 20
            x = sx + (ex - sx) * t + math.sin(t * 9 + i) * 22
            y = sy + (ey - sy) * t + math.cos(t * 7 + i) * 18
            pts.append((x, y))
        d.line(pts, fill=INK, width=34, joint="curve")
        d.line(pts, fill=(150, 105, 60, 255), width=18, joint="curve")
    for x, y in [(380, 220), (140, 230), (450, 330)]:
        shape(d, [(x, y), (x + 40, y - 30), (x + 60, y + 5), (x + 20, y + 25)], (150, 205, 100, 255))
    finish(img, mask, "Icon_Roots")


raise_rock()
shield_bash()
sprout()
roots()
ims = [Image.open(f"{OUT}\\{n}.png") for n in ["Icon_RaiseRock", "Icon_ShieldBash", "Icon_Sprout", "Icon_Roots", "Icon_SwordSlash", "Icon_Heal"]]
sheet = Image.new("RGBA", (128 * len(ims), 128), (40, 40, 40, 255))
for i, im in enumerate(ims):
    sheet.paste(im, (i * 128, 0), im)
sheet.save(f"{OUT}\\_new_sheet.png")
