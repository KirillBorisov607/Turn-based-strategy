"""Draws the card icons of Keepers of the Lands (256 px PNG): a coloured medallion with a light glyph.

Usage: python make_card_icons.py <out folder>   -> T_Card_<Name>.png, imported by import_card_icons.py
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

S = 256
C = S / 2
LIGHT = (250, 240, 214, 255)
DARK = (30, 24, 20, 255)


def medallion(color):
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((8, 12, S - 8, S - 4), fill=(0, 0, 0, 110))
    d.ellipse((8, 8, S - 8, S - 8), fill=tuple(int(v * 0.55) for v in color) + (255,))
    d.ellipse((18, 18, S - 18, S - 18), fill=color + (255,))
    # A soft highlight at the top.
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((48, 28, S - 48, 120), fill=(255, 255, 255, 60))
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(10)))
    return im, ImageDraw.Draw(im)


def outline_poly(d, pts, fill=LIGHT, width=7):
    d.polygon(pts, fill=fill)
    d.line(pts + [pts[0]], fill=DARK, width=width, joint="curve")


def star_pts(cx, cy, r1, r2, n=5, rot=-90):
    pts = []
    for i in range(n * 2):
        r = r1 if i % 2 == 0 else r2
        a = math.radians(rot + i * 180 / n)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def heart(d):
    pts = []
    for t in range(0, 360, 4):
        a = math.radians(t)
        x = 16 * math.sin(a) ** 3
        y = -(13 * math.cos(a) - 5 * math.cos(2 * a) - 2 * math.cos(3 * a) - math.cos(4 * a))
        pts.append((C + x * 5.2, C + 8 + y * 5.2))
    outline_poly(d, pts)


def boot(d):
    outline_poly(d, [(92, 56), (148, 56), (150, 150), (196, 166), (200, 196), (84, 196), (84, 150)])
    d.line((84, 176, 200, 176), fill=DARK, width=6)


def bolt(d):
    outline_poly(d, [(140, 40), (82, 140), (124, 140), (104, 216), (176, 108), (132, 108), (156, 40)])


def coin(d):
    d.ellipse((60, 60, 196, 196), fill=LIGHT, outline=DARK, width=8)
    d.ellipse((84, 84, 172, 172), outline=DARK, width=5)
    outline_poly(d, star_pts(C, C, 30, 13), fill=(236, 196, 108, 255), width=4)


def gem(d):
    outline_poly(d, [(C, 52), (196, 104), (C, 208), (60, 104)])
    d.line((60, 104, 196, 104), fill=DARK, width=5)
    d.line((C, 52, 104, 104, C, 208, 152, 104, C, 52), fill=DARK, width=4)


def shield(d):
    pts = [(C, 48), (196, 74), (190, 140)]
    for t in range(0, 91, 10):
        a = math.radians(t)
        pts.append((C + 62 * math.cos(a), 140 + 70 * math.sin(a)))
    pts += [(C - 62 * math.cos(math.radians(t)), 140 + 70 * math.sin(math.radians(t))) for t in range(90, -1, -10)]
    pts += [(66, 140), (60, 74)]
    outline_poly(d, pts)
    d.line((C, 64, C, 196), fill=DARK, width=5)


def sword(d, angle=0, cx=C, cy=C):
    def rot(p):
        x, y = p[0] - C, p[1] - C
        a = math.radians(angle)
        return (cx + x * math.cos(a) - y * math.sin(a), cy + x * math.sin(a) + y * math.cos(a))
    outline_poly(d, [rot(p) for p in [(120, 40), (136, 40), (138, 160), (118, 160)]])
    outline_poly(d, [rot(p) for p in [(92, 160), (164, 160), (164, 174), (92, 174)]], width=6)
    outline_poly(d, [rot(p) for p in [(120, 174), (136, 174), (136, 212), (120, 212)]], fill=(150, 100, 60, 255), width=6)


def swords(d):
    sword(d, -38, C - 6, C)
    sword(d, 38, C + 6, C)


def banner(d):
    d.line((84, 44, 84, 216), fill=DARK, width=12)
    d.line((84, 44, 84, 216), fill=(150, 100, 60, 255), width=6)
    outline_poly(d, [(88, 52), (190, 52), (164, 92), (190, 132), (88, 132)])
    outline_poly(d, star_pts(130, 92, 22, 9), fill=(214, 70, 60, 255), width=3)


def dice(d):
    d.rounded_rectangle((64, 64, 192, 192), 22, fill=LIGHT, outline=DARK, width=8)
    for x, y in ((96, 96), (160, 160), (C, C), (160, 96), (96, 160)):
        d.ellipse((x - 11, y - 11, x + 11, y + 11), fill=DARK)


def drum(d):
    d.rectangle((70, 96, 186, 176), fill=LIGHT, outline=DARK, width=7)
    d.ellipse((70, 76, 186, 116), fill=LIGHT, outline=DARK, width=7)
    for x in range(78, 190, 22):
        d.line((x, 104, x + 11, 176), fill=(200, 80, 60, 255), width=5)
    d.line((96, 40, 140, 92), fill=DARK, width=10)
    d.line((176, 44, 136, 92), fill=DARK, width=10)


def flame(d):
    pts = []
    for t in range(0, 361, 6):
        a = math.radians(t)
        r = 62 + 18 * math.sin(3 * a)
        pts.append((C + r * 0.75 * math.sin(a), 150 - r * (1.25 if math.cos(a) > 0 else 0.7) * math.cos(a)))
    outline_poly(d, pts, fill=(255, 196, 80, 255))
    outline_poly(d, [(C, 110), (154, 160), (C, 196), (102, 160)], fill=LIGHT, width=4)


def acorn(d):
    d.ellipse((84, 100, 172, 214), fill=(214, 160, 96, 255), outline=DARK, width=7)
    d.chord((72, 74, 184, 150), 180, 360, fill=(120, 80, 40, 255), outline=DARK, width=7)
    d.rectangle((72, 110, 184, 124), fill=(120, 80, 40, 255), outline=DARK, width=6)
    d.line((C, 74, C + 12, 48), fill=DARK, width=9)


def feather(d):
    pts = [(176, 44), (196, 70), (110, 190), (80, 210), (90, 176)]
    outline_poly(d, pts)
    d.line((182, 56, 76, 216), fill=DARK, width=5)
    for i in range(5):
        t = 0.2 + i * 0.13
        x, y = 182 + (76 - 182) * t, 56 + (216 - 56) * t
        d.line((x, y, x + 22, y + 6), fill=DARK, width=3)


def horseshoe(d):
    d.arc((66, 60, 190, 196), 160, 380, fill=DARK, width=36)
    d.arc((70, 64, 186, 192), 160, 380, fill=LIGHT, width=24)
    for a in (190, 230, 270, 310, 350):
        r = math.radians(a)
        x, y = C + 58 * math.cos(r), 128 + 64 * math.sin(r)
        d.ellipse((x - 5, y - 5, x + 5, y + 5), fill=DARK)


def cloak(d):
    outline_poly(d, [(C, 50), (164, 70), (196, 206), (C, 186), (60, 206), (92, 70)])
    d.ellipse((C - 16, 60, C + 16, 92), fill=(236, 196, 108, 255), outline=DARK, width=5)


def bell(d):
    outline_poly(d, [(C, 52), (164, 84), (172, 160), (196, 184), (60, 184), (84, 160), (92, 84)])
    d.ellipse((C - 14, 182, C + 14, 210), fill=LIGHT, outline=DARK, width=6)


def leaf(d):
    pts = [(C + 70 * math.sin(math.radians(t)) * (1 - abs(t - 180) / 360), 50 + t * 0.45) for t in range(0, 361, 10)]
    outline_poly(d, [(C, 44), (184, 110), (C, 214), (72, 110)])
    d.line((C, 60, C, 200), fill=DARK, width=5)


def question(d):
    d.arc((86, 52, 170, 136), 180, 400, fill=DARK, width=30)
    d.arc((90, 56, 166, 132), 180, 400, fill=LIGHT, width=18)
    d.rectangle((118, 128, 138, 160), fill=LIGHT, outline=DARK, width=5)
    d.ellipse((114, 174, 142, 202), fill=LIGHT, outline=DARK, width=5)


def road(d):
    outline_poly(d, [(108, 52), (148, 52), (196, 210), (60, 210)])
    for y in (80, 120, 160, 196):
        d.line((C, y, C, y + 18), fill=DARK, width=6)


ICONS = {
    # growth
    "Hp": ((196, 62, 56), heart),
    "Move": ((58, 120, 178), boot),
    "Action": ((206, 150, 44), bolt),
    # event effects
    "Gold": ((206, 150, 44), coin),
    "Relic": ((120, 72, 168), gem),
    "Armor": ((92, 104, 120), shield),
    "Boots": ((58, 120, 178), horseshoe),
    "Glory": ((176, 52, 52), banner),
    "Fight": ((168, 54, 40), swords),
    "Chance": ((70, 110, 90), dice),
    "Leave": ((80, 92, 84), road),
    # relics
    "Drum": ((176, 52, 52), drum),
    "Cloak": ((70, 60, 130), cloak),
    "Feather": ((58, 120, 178), feather),
    "Acorn": ((86, 132, 60), acorn),
    "Ember": ((200, 90, 40), flame),
    "Coin": ((206, 150, 44), coin),
    "Banner": ((176, 52, 52), banner),
    "Charm": ((58, 140, 150), horseshoe),
    "Bell": ((206, 150, 44), bell),
    "Leaf": ((86, 132, 60), leaf),
    "Unknown": ((90, 90, 110), question),
}


def main(out):
    os.makedirs(out, exist_ok=True)
    for name, (color, glyph) in ICONS.items():
        im, d = medallion(color)
        glyph(d)
        im.save(os.path.join(out, f"T_Card_{name}.png"))
    print(len(ICONS), "icons in", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else r"C:/ProjectsUE/_logs/card_icons")
