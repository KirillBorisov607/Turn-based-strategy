"""Assembles the alpha-test trailer from clips recorded by KeepersDirector (record.sh).

Usage: python make_trailer.py <out.mp4>
Clips are frame folders in C:/ProjectsUE/_logs/rec/<name>; the cut list is CUTS below.
Title cards and captions are drawn with PIL (Cyrillic fonts of Windows), clips cross-fade.
"""
import os
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

REC = r"C:/ProjectsUE/_logs/rec"
W, H, FPS = 1280, 720, 30
FADE = 12
GOLD = (236, 196, 108)
CREAM = (245, 236, 214)


def font(size, bold=False):
    for name in (["georgiab.ttf", "cambriab.ttf"] if bold else ["georgia.ttf", "cambria.ttc"]):
        path = os.path.join(r"C:/Windows/Fonts", name)
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def frames(name, start, end, step=1):
    folder = os.path.join(REC, name)
    files = sorted(f for f in os.listdir(folder) if f.endswith(".jpg"))
    for f in files[start:end:step]:
        im = cv2.imread(os.path.join(folder, f))
        if im is not None:
            yield cv2.resize(im, (W, H), interpolation=cv2.INTER_AREA)


def card(lines, background=None, seconds=2.5):
    """A title card: dark backdrop (or a blurred frame), centred lines of text."""
    if background is not None:
        base = Image.fromarray(cv2.cvtColor(background, cv2.COLOR_BGR2RGB)).filter(ImageFilter.GaussianBlur(9))
        base = Image.blend(base, Image.new("RGB", (W, H), (14, 16, 22)), 0.55)
    else:
        base = Image.new("RGB", (W, H), (14, 16, 22))
    draw = ImageDraw.Draw(base)
    total = sum(f.size + 18 for _, f, _ in lines)
    y = (H - total) // 2
    for text, f, color in lines:
        w = draw.textlength(text, font=f)
        draw.text(((W - w) / 2 + 2, y + 3), text, font=f, fill=(0, 0, 0))
        draw.text(((W - w) / 2, y), text, font=f, fill=color)
        y += f.size + 18
    im = cv2.cvtColor(np.array(base), cv2.COLOR_RGB2BGR)
    return [im] * int(seconds * FPS)


def caption(frame, text, alpha):
    """A caption bar at the bottom of a clip, faded in by alpha."""
    if alpha <= 0:
        return frame
    base = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(over)
    f = font(34, True)
    w = draw.textlength(text, font=f)
    x, y = (W - w) / 2, H - 120
    draw.rounded_rectangle((x - 28, y - 14, x + w + 28, y + 52), 12, fill=(12, 14, 20, int(190 * alpha)))
    draw.text((x, y), text, font=f, fill=GOLD + (int(255 * alpha),))
    base = Image.alpha_composite(base.convert("RGBA"), over).convert("RGB")
    return cv2.cvtColor(np.array(base), cv2.COLOR_RGB2BGR)


# (clip, first frame, last frame, step, caption)
CUTS = [
    ("menu", 20, 200, 1, "Привал у костра"),
    ("squad", 20, 380, 1, "Собери отряд"),
    ("campaign", 60, 470, 1, "Выбери путь по карте"),
    ("battle", 120, 900, 1, "Перехитри врага: он показывает, куда ударит"),
    ("battle2", 60, 600, 1, "Используй местность"),
]


def main(out):
    shots = []
    first = next(frames(CUTS[0][0], CUTS[0][1], CUTS[0][1] + 1))
    shots.append(card([("Хранители земель", font(76, True), GOLD), ("пошаговая тактика на гексах", font(32), CREAM)], first, 3.2))
    for name, start, end, step, text in CUTS:
        if not os.path.isdir(os.path.join(REC, name)):
            print("skip", name)
            continue
        clip = list(frames(name, start, end, step))
        n = len(clip)
        for i in range(n):
            a = min(1.0, i / 15, (n - 1 - i) / 15) if text else 0
            clip[i] = caption(clip[i], text, a)
        shots.append(clip)
        print(name, n, "frames")
    shots.append(card([("Альфа-тест", font(64, True), GOLD), ("дипломный проект · Unreal Engine 5.8", font(30), CREAM),
                       ("октябрь 2026", font(26), CREAM)], None, 3.5))

    writer = None
    for codec in ("avc1", "mp4v"):
        writer = cv2.VideoWriter(out, cv2.VideoWriter_fourcc(*codec), FPS, (W, H))
        if writer.isOpened():
            print("codec", codec)
            break
    previous_tail = None
    for shot in shots:
        if previous_tail is not None:
            for i in range(min(FADE, len(shot))):
                t = (i + 1) / (FADE + 1)
                shot[i] = cv2.addWeighted(previous_tail[i], 1 - t, shot[i], t, 0)
        body = shot[:-FADE] if len(shot) > 2 * FADE else shot
        for im in body:
            writer.write(im)
        previous_tail = shot[-FADE:] if len(shot) > 2 * FADE else None
    if previous_tail is not None:
        for i, im in enumerate(previous_tail):
            writer.write(cv2.addWeighted(im, 1 - (i + 1) / FADE, np.zeros_like(im), (i + 1) / FADE, 0))
    writer.release()
    print("written", out, os.path.getsize(out) // 1024, "KB")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(REC, "trailer.mp4"))
