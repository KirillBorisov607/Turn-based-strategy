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


def snowflake(d):
    for k in range(6):
        a = math.radians(60 * k)
        x, y = C + 78 * math.cos(a), C + 78 * math.sin(a)
        d.line((C, C, x, y), fill=DARK, width=16)
        d.line((C, C, x, y), fill=LIGHT, width=9)
        for side in (-1, 1):
            b = a + side * math.radians(40)
            mx, my = C + 48 * math.cos(a), C + 48 * math.sin(a)
            d.line((mx, my, mx + 26 * math.cos(b), my + 26 * math.sin(b)), fill=LIGHT, width=7)


def dagger(d):
    outline_poly(d, [(C, 40), (C + 18, 70), (C + 14, 150), (C - 14, 150), (C - 18, 70)])
    outline_poly(d, [(C - 46, 150), (C + 46, 150), (C + 46, 166), (C - 46, 166)], fill=(236, 196, 108, 255), width=5)
    outline_poly(d, [(C - 10, 166), (C + 10, 166), (C + 10, 214), (C - 10, 214)], fill=(150, 100, 60, 255), width=5)


def cloud(d):
    for x, y, r in ((96, 140, 40), (140, 116, 50), (176, 146, 36), (128, 158, 42)):
        d.ellipse((x - r - 4, y - r - 4, x + r + 4, y + r + 4), fill=DARK)
    for x, y, r in ((96, 140, 40), (140, 116, 50), (176, 146, 36), (128, 158, 42)):
        d.ellipse((x - r, y - r, x + r, y + r), fill=(214, 214, 220, 255))


def hammer(d):
    d.line((90, 200, 160, 110), fill=DARK, width=22)
    d.line((90, 200, 160, 110), fill=(150, 100, 60, 255), width=12)
    outline_poly(d, [(120, 60), (200, 120), (176, 152), (96, 92)])


def throw_arc(d):
    d.arc((60, 70, 200, 210), 200, 340, fill=DARK, width=26)
    d.arc((60, 70, 200, 210), 200, 340, fill=LIGHT, width=14)
    outline_poly(d, [(184, 96), (214, 128), (174, 136)])


def dash_lines(d):
    for k, y in enumerate((96, 128, 160)):
        d.line((56 + k * 12, y, 140, y), fill=DARK, width=14)
        d.line((56 + k * 12, y, 140, y), fill=LIGHT, width=7)
    outline_poly(d, [(140, 80), (206, 128), (140, 176)])


def fireball(d):
    flame(d)
    d.ellipse((96, 120, 160, 184), fill=(255, 120, 40, 255), outline=DARK, width=5)


def rage(d):
    outline_poly(d, star_pts(C, C, 86, 44, 8), fill=(255, 110, 70, 255))
    d.line((100, 116, 122, 128), fill=DARK, width=8)
    d.line((156, 116, 134, 128), fill=DARK, width=8)
    d.arc((104, 140, 152, 176), 200, 340, fill=DARK, width=7)


def boulder(d):
    # A rock flying in an arc.
    d.arc((40, 60, 200, 220), 190, 300, fill=DARK, width=14)
    d.arc((40, 60, 200, 220), 190, 300, fill=LIGHT, width=6)
    outline_poly(d, [(138, 120), (184, 108), (214, 140), (204, 186), (160, 200), (124, 172)], fill=(170, 160, 150, 255))
    d.line((160, 140, 178, 160), fill=DARK, width=5)


def ram(d):
    # A log with an iron head.
    outline_poly(d, [(48, 112), (168, 112), (168, 152), (48, 152)], fill=(160, 106, 60, 255))
    outline_poly(d, [(168, 100), (212, 112), (212, 152), (168, 164)], fill=(150, 156, 170, 255))
    for x in (84, 124):
        d.line((x, 112, x, 152), fill=DARK, width=6)


def charge(d):
    # A spear tip and speed lines.
    for k, y in enumerate((96, 160)):
        d.line((48 + k * 16, y, 120, y), fill=DARK, width=12)
        d.line((48 + k * 16, y, 120, y), fill=LIGHT, width=6)
    d.line((70, 128, 170, 128), fill=DARK, width=18)
    d.line((70, 128, 170, 128), fill=(160, 106, 60, 255), width=10)
    outline_poly(d, [(166, 104), (222, 128), (166, 152)], fill=(200, 206, 220, 255))


def rooster(d):
    # A rooster's head: comb, eye, beak, wattle.
    for x, y, r in ((104, 66, 20), (130, 56, 22), (156, 66, 20)):
        d.ellipse((x - r - 5, y - r - 5, x + r + 5, y + r + 5), fill=DARK)
        d.ellipse((x - r, y - r, x + r, y + r), fill=(230, 50, 40, 255))
    d.ellipse((80, 70, 186, 196), fill=DARK)
    d.ellipse((86, 76, 180, 190), fill=LIGHT)
    outline_poly(d, [(176, 118), (222, 132), (176, 148)], fill=(240, 170, 40, 255), width=5)
    d.ellipse((140, 104, 160, 124), fill=DARK)
    outline_poly(d, [(160, 150), (178, 152), (172, 196), (156, 186)], fill=(230, 50, 40, 255), width=5)


def witch(d):
    # A pointed hat with a buckle.
    outline_poly(d, [(128, 40), (176, 170), (80, 170)], fill=(70, 50, 100, 255))
    outline_poly(d, [(48, 168), (208, 168), (208, 188), (48, 188)], fill=(70, 50, 100, 255))
    outline_poly(d, [(112, 146), (144, 146), (144, 168), (112, 168)], fill=(236, 196, 108, 255), width=5)


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
    # abilities of the Mage, the Rogue and the Barbarian
    "Fireball": ((200, 70, 30), fireball),
    "IcePath": ((70, 150, 210), snowflake),
    "ChainLightning": ((80, 70, 170), bolt),
    "Dash": ((58, 120, 178), dash_lines),
    "Backstab": ((120, 30, 40), dagger),
    "SmokeBomb": ((80, 86, 96), cloud),
    "Crush": ((130, 90, 50), hammer),
    "Throw": ((176, 120, 40), throw_arc),
    "Rage": ((170, 30, 30), rage),
    # enemies
    "Boulder": ((110, 100, 92), boulder),
    "RamStrike": ((120, 80, 50), ram),
    "Charge": ((150, 40, 40), charge),
    # story
    "Rooster": ((200, 150, 60), rooster),
    "Witch": ((110, 150, 90), witch),
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
