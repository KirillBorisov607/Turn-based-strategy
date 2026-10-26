"""A faster cut of the alpha trailer: short shots, sliding captions, slow push-ins, flashes between acts.

Usage: python make_trailer2.py <out.mp4>    (then to_h264.ps1 for phones)
Shots are frame ranges of clips recorded by KeepersDirector (record.sh) in C:/ProjectsUE/_logs/rec/<clip>.
"""
import json
import os
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

REC = r"C:/ProjectsUE/_logs/rec"
W, H, FPS = 1280, 720, 30
FADE = 5
GOLD = (236, 196, 108)
CREAM = (245, 236, 214)


def font(size, bold=False):
    for name in (["georgiab.ttf", "cambriab.ttc"] if bold else ["georgia.ttf", "cambria.ttc"]):
        path = os.path.join(r"C:/Windows/Fonts", name)
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def load(clip, index):
    im = cv2.imread(os.path.join(REC, clip, "frame_%05d.jpg" % index))
    return None if im is None else cv2.resize(im, (W, H), interpolation=cv2.INTER_AREA)


def push_in(im, t, amount):
    """Slow zoom towards the centre: t 0..1."""
    if amount <= 0:
        return im
    s = 1 + amount * t
    w, h = int(W / s), int(H / s)
    x, y = (W - w) // 2, (H - h) // 2
    return cv2.resize(im[y:y + h, x:x + w], (W, H), interpolation=cv2.INTER_LINEAR)


def caption(frame, kicker, text, i, n):
    """A caption bar sliding in from the left, upper left of the frame."""
    if not text:
        return frame
    appear = min(1.0, i / 8)
    leave = min(1.0, (n - 1 - i) / 6)
    a = max(0.0, min(appear, leave))
    if a <= 0:
        return frame
    ease = 1 - (1 - appear) ** 3
    base = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    big, small = font(40, True), font(20, True)
    tw = d.textlength(text, font=big)
    x = int(-tw - 80 + (tw + 140) * ease)
    y = 92
    d.rectangle((x - 40, y - 12, x + tw + 30, y + 58), fill=(12, 14, 20, int(200 * a)))
    d.rectangle((x - 40, y - 12, x - 32, y + 58), fill=GOLD + (int(255 * a),))
    if kicker:
        d.text((x, y - 40), kicker, font=small, fill=GOLD + (int(230 * a),))
    d.text((x + 2, y + 3), text, font=big, fill=(0, 0, 0, int(200 * a)))
    d.text((x, y), text, font=big, fill=CREAM + (int(255 * a),))
    return cv2.cvtColor(np.array(Image.alpha_composite(base, over).convert("RGB")), cv2.COLOR_RGB2BGR)


def card(lines, background=None, seconds=2.0, zoom=0.04):
    if background is not None:
        base = Image.fromarray(cv2.cvtColor(background, cv2.COLOR_BGR2RGB)).filter(ImageFilter.GaussianBlur(10))
        base = Image.blend(base, Image.new("RGB", (W, H), (14, 16, 22)), 0.5)
    else:
        base = Image.new("RGB", (W, H), (14, 16, 22))
    d = ImageDraw.Draw(base)
    total = sum(f.size + 18 for _, f, _ in lines)
    y = (H - total) // 2
    for text, f, color in lines:
        w = d.textlength(text, font=f)
        d.text(((W - w) / 2 + 3, y + 4), text, font=f, fill=(0, 0, 0))
        d.text(((W - w) / 2, y), text, font=f, fill=color)
        y += f.size + 18
    im = cv2.cvtColor(np.array(base), cv2.COLOR_RGB2BGR)
    n = int(seconds * FPS)
    return [push_in(im, i / n, zoom) for i in range(n)]


# (clip, first, last, step, push-in, kicker, caption, flash[, info])
SHOTS = []
DATA = json.load(open(r"C:/ProjectsUE/_logs/game_data.json", encoding="utf-8"))
ICONS = r"C:/ProjectsUE/_logs/icons"
CARDS = r"C:/ProjectsUE/_logs/card_icons"


def _icon(name, size):
    for path in (os.path.join(ICONS, name + ".png"), os.path.join(CARDS, "T_Card_" + name + ".png")):
        if os.path.exists(path):
            return Image.open(path).convert("RGBA").resize((size, size), Image.LANCZOS)
    return None


def info(frame, kind, i, n):
    """Infographic over a shot: a hero card with stat bars, an objective badge."""
    if not kind:
        return frame
    p = min(1.0, i / 10)
    base = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)).convert("RGBA")
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    what, _, arg = kind.partition(":")
    if what == "hero":
        h = next(x for x in DATA["heroes"] if x["id"] == arg)
        x0 = int(W - 380 + (1 - p) * 200)
        d.rounded_rectangle((x0, 470, x0 + 350, 690), 18, fill=(12, 14, 20, 215), outline=GOLD + (255,), width=2)
        d.text((x0 + 20, 482), h["name"], font=font(30, True), fill=GOLD)
        d.text((x0 + 20, 520), h["class"], font=font(18), fill=CREAM)
        for k, (label, v, m, col) in enumerate((("Здоровье", h["hp"], 50, (214, 86, 70)), ("Движение", h["mp"], 6, (96, 160, 230)),
                                                 ("Действия", h["ap"], 4, GOLD))):
            y = 560 + k * 40
            d.text((x0 + 20, y), label, font=font(17), fill=CREAM)
            d.rounded_rectangle((x0 + 130, y + 4, x0 + 290, y + 20), 8, fill=(255, 255, 255, 40))
            fill = int(160 * min(v / m, 1) * min(1.0, i / 25))
            if fill > 6:
                d.rounded_rectangle((x0 + 130, y + 4, x0 + 130 + fill, y + 20), 8, fill=col + (255,))
            d.text((x0 + 300, y - 2), str(v), font=font(20, True), fill=CREAM)
    elif what == "goal":
        icon_name, _, text = arg.partition("|")
        ic = _icon(icon_name, 64)
        x0 = int(W - 470 + (1 - p) * 300)
        d.rounded_rectangle((x0, 600, x0 + 440, 690), 18, fill=(12, 14, 20, 215), outline=GOLD + (255,), width=2)
        if ic:
            over.alpha_composite(ic, (x0 + 14, 613))
        d.text((x0 + 92, 610), "ЗАДАНИЕ", font=font(16, True), fill=GOLD)
        d.text((x0 + 92, 634), text, font=font(26, True), fill=CREAM)
    return cv2.cvtColor(np.array(Image.alpha_composite(base, over).convert("RGB")), cv2.COLOR_RGB2BGR)


def numbers_card(background, seconds=3.4):
    """Six numbers counting up over a blurred frame."""
    base = Image.fromarray(cv2.cvtColor(background, cv2.COLOR_BGR2RGB)).filter(ImageFilter.GaussianBlur(12))
    base = Image.blend(base, Image.new("RGB", (W, H), (14, 16, 22)), 0.55)
    items = [(6, "героев"), (24, "способности"), (20, "событий"), (16, "реликвий"), (5, "заданий боя"), (9, "видов врагов")]
    frames = []
    n = int(seconds * FPS)
    for i in range(n):
        im = base.copy()
        d = ImageDraw.Draw(im)
        for k, (v, label) in enumerate(items):
            x = 150 + (k % 3) * 360
            y = 150 + (k // 3) * 250
            q = max(0.0, min(1.0, (i - k * 4) / 18))
            if q <= 0:
                continue
            value = str(int(round(v * (1 - (1 - q) ** 3))))
            d.text((x, y), value, font=font(120, True), fill=GOLD)
            d.text((x + 4, y + 140), label, font=font(30), fill=CREAM)
        frames.append(cv2.cvtColor(np.array(im), cv2.COLOR_RGB2BGR))
    return frames


def build(shots_spec):
    shots = []
    for spec in shots_spec:
        clip, first, last, step, zoom, kicker, text, flash = spec[:8]
        kind = spec[8] if len(spec) > 8 else ""
        if clip == "NUMBERS":
            shots.append(numbers_card(load("battle2", 200)))
            continue
        frames = [f for f in (load(clip, i) for i in range(first, last, step)) if f is not None]
        n = len(frames)
        out = []
        for i, im in enumerate(frames):
            im = push_in(im, i / max(n - 1, 1), zoom)
            im = info(im, kind, i, n)
            im = caption(im, kicker, text, i, n)
            if flash and i < 3:
                im = cv2.addWeighted(im, 0.4 + 0.2 * i, np.full_like(im, 255), 0.6 - 0.2 * i, 0)
            out.append(im)
        shots.append(out)
        print(clip, first, last, n)
    return shots


def write(shots, out):
    writer = cv2.VideoWriter(out, cv2.VideoWriter_fourcc(*"mp4v"), FPS, (W, H))
    tail = None
    for shot in shots:
        if tail is not None:
            for i in range(min(FADE, len(shot))):
                t = (i + 1) / (FADE + 1)
                shot[i] = cv2.addWeighted(tail[i], 1 - t, shot[i], t, 0)
        body = shot[:-FADE] if len(shot) > 2 * FADE else shot
        for im in body:
            writer.write(im)
        tail = shot[-FADE:] if len(shot) > 2 * FADE else None
    if tail is not None:
        for i, im in enumerate(tail):
            writer.write(cv2.addWeighted(im, 1 - (i + 1) / FADE, np.zeros_like(im), (i + 1) / FADE, 0))
    writer.release()


def main(out):
    spec = eval(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), os.environ.get("SHOTS", "trailer_shots.py")), encoding="utf-8").read())
    first = load(spec[0][0], spec[0][1])
    shots = [card([("Хранители земель", font(84, True), GOLD), ("пошаговая тактика на гексах", font(32), CREAM)], first, 2.2)]
    shots += build(spec)
    shots.append(card([("Хранители земель", font(70, True), GOLD), ("пошаговая тактика · смешное фэнтези", font(30), CREAM),
                       ("альфа-тест — скоро", font(30, True), CREAM)], None, 3.0))
    write(shots, out)
    print("written", out, sum(len(s) for s in shots) / FPS, "s")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(REC, "trailer2.mp4"))
