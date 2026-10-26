"""Vertical shorts (TikTok / YouTube Shorts / Reels) about Keepers of the Lands, with animated infographics.

Usage: python make_shorts.py [filter]   ->  C:/ProjectsUE/_shorts/<NN>_<slug>.mp4 and plan.json
Footage: clips recorded by KeepersDirector (record.sh / record_shorts.sh) in C:/ProjectsUE/_logs/rec.
Data: game_data.json (shorts_data.py), icons (export_all_icons.py, export_unit_icons.py), card medallions.
Each short: 1080x1920, 30 fps; header (series + title), the game in the middle, an infographic under it,
a progress line, our music (a variation of the theme) with pops on every reveal; H.264 + AAC via mux_audio.ps1.
"""
import json
import math
import os
import subprocess
import sys
import wave

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_music  # noqa: E402

REC = r"C:/ProjectsUE/_logs/rec"
OUT = r"C:/ProjectsUE/_shorts"
ICONS = r"C:/ProjectsUE/_logs/icons"
CARDS = r"C:/ProjectsUE/_logs/card_icons"
SFX = r"C:/ProjectsUE/_logs/sfx"
DATA = json.load(open(r"C:/ProjectsUE/_logs/game_data.json", encoding="utf-8"))
W, H, FPS = 1080, 1920, 30
VID_Y, VID_H = 372, 608
PANEL_Y = 1010
GOLD = (236, 196, 108)
CREAM = (245, 236, 214)
DIM = (170, 176, 190)
INK = (16, 18, 26)
RED = (214, 86, 70)
GREEN = (110, 200, 120)
BLUE = (96, 160, 230)


def font(size, kind="body"):
    files = {"title": "georgiab.ttf", "body": "segoeui.ttf", "bold": "segoeuib.ttf", "num": "bahnschrift.ttf"}
    return ImageFont.truetype(os.path.join(r"C:/Windows/Fonts", files[kind]), size)


F = {k: font(*k) for k in [(72, "title"), (58, "title"), (46, "title"), (40, "title"), (34, "bold"), (30, "bold"), (28, "body"),
                           (26, "body"), (26, "bold"), (24, "bold"), (22, "body"), (56, "num"), (40, "num"), (90, "num"), (130, "num")]}


def ease_out(x):
    x = min(max(x, 0.0), 1.0)
    return 1 - (1 - x) ** 3


def ease_back(x):
    x = min(max(x, 0.0), 1.0)
    c = 1.70158
    return 1 + (c + 1) * (x - 1) ** 3 + c * (x - 1) ** 2


def wrap(draw, text, fnt, width):
    lines, line = [], ""
    for word in text.split():
        test = (line + " " + word).strip()
        if draw.textlength(test, font=fnt) <= width:
            line = test
        else:
            if line:
                lines.append(line)
            line = word
    if line:
        lines.append(line)
    return lines


_icon_cache = {}


def icon(name, size):
    key = (name, size)
    if key not in _icon_cache:
        path = None
        for folder, file in ((ICONS, name + ".png"), (CARDS, "T_Card_" + name + ".png")):
            if os.path.exists(os.path.join(folder, file)):
                path = os.path.join(folder, file)
                break
        im = Image.open(path).convert("RGBA") if path else Image.new("RGBA", (size, size), (0, 0, 0, 0))
        _icon_cache[key] = im.resize((size, size), Image.LANCZOS)
    return _icon_cache[key]


def spell_icon(spell_id):
    if os.path.exists(os.path.join(ICONS, "Icon_" + spell_id + ".png")):
        return "Icon_" + spell_id
    return {"Gust": "Icon_WindGust", "Fireball": "Ember", "IcePath": "Feather", "ChainLightning": "Action", "Dash": "Move",
            "Backstab": "Fight", "SmokeBomb": "Puff", "Crush": "Fight", "Throw": "Charm", "Rage": "Hp"}.get(spell_id, "Unknown")


def portrait(unit_id):
    for name in ("T_Portrait_" + unit_id, "TX_Icon_" + unit_id):
        if os.path.exists(os.path.join(ICONS, name + ".png")):
            return name
    return None


# --------------------------------------------------------------------------------------------------------------------
# Background, header, footer

def background():
    y = np.linspace(0, 1, H)[:, None]
    top, bottom = np.array([14, 16, 24]), np.array([30, 24, 22])
    grad = np.zeros((H, W, 3), np.float32)
    for c in range(3):
        grad[:, :, c] = (top[c] * (1 - y) + bottom[c] * y)
    im = Image.fromarray(grad.astype(np.uint8)).convert("RGBA")
    d = ImageDraw.Draw(im)
    # A faint hex pattern, like the battle fields.
    r = 46
    for row in range(-1, int(H / (r * 1.5)) + 2):
        for col in range(-1, int(W / (r * 1.732)) + 2):
            cx = col * r * 1.732 + (r * 0.866 if row % 2 else 0)
            cy = row * r * 1.5
            pts = [(cx + r * math.cos(math.radians(60 * k + 30)), cy + r * math.sin(math.radians(60 * k + 30))) for k in range(6)]
            d.polygon(pts, outline=(255, 255, 255, 10))
    # Warm glow behind the video.
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((-200, VID_Y - 160, W + 200, VID_Y + VID_H + 160), fill=(236, 170, 80, 38))
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(80)))
    return im


BG = None


def header_layer(series, title, accent):
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    f = F[(24, "bold")]
    tw = d.textlength(series, font=f)
    d.rounded_rectangle((60, 92, 60 + tw + 40, 136), 22, fill=accent + (255,))
    d.text((80, 98), series, font=f, fill=INK)
    fnt = F[(72, "title")] if d.textlength(title, font=F[(72, "title")]) < 960 * 1.7 else F[(58, "title")]
    lines = wrap(d, title, fnt, 960)[:2]
    y = 156
    for line in lines:
        d.text((63, y + 4), line, font=fnt, fill=(0, 0, 0, 200))
        d.text((60, y), line, font=fnt, fill=CREAM)
        y += fnt.size + 8
    # Frame around the video.
    d.rounded_rectangle((26, VID_Y - 14, W - 26, VID_Y + VID_H + 14), 26, outline=accent + (255,), width=4)
    # Footer.
    d.text((60, 1628), "ХРАНИТЕЛИ ЗЕМЕЛЬ", font=F[(30, "bold")], fill=GOLD)
    d.text((60, 1668), "пошаговая тактика на гексах · подпишись", font=F[(24, "bold")], fill=DIM)
    return layer


# --------------------------------------------------------------------------------------------------------------------
# Footage

class Footage:
    """Frames of recorded clips: a list of (clip, first, last, step) played in order and looped."""

    def __init__(self, parts, zoom=1.06, crop=None):
        """crop: (x0, y0, x1, y1) fractions of the source frame to show (the hero, without the side panel)."""
        self.crop = crop
        self.files = []
        for clip, first, last, step in parts:
            folder = os.path.join(REC, clip)
            for i in range(first, last, step):
                path = os.path.join(folder, "frame_%05d.jpg" % i)
                if os.path.exists(path):
                    self.files.append(path)
        self.zoom = zoom
        self.cache = {}

    def frame(self, i):
        if not self.files:
            return np.zeros((VID_H, W - 60, 3), np.uint8)
        path = self.files[i % len(self.files)]
        im = self.cache.get(path)
        if im is None:
            im = cv2.imread(path)
            if self.crop:
                h0, w0 = im.shape[:2]
                x0, y0c, x1, y1 = self.crop
                im = im[int(y0c * h0):int(y1 * h0), int(x0 * w0):int(x1 * w0)]
            h, w = im.shape[:2]
            target_w, target_h = W - 60, VID_H
            scale = max(target_w / w, target_h / h) * self.zoom
            im = cv2.resize(im, (int(w * scale), int(h * scale)), interpolation=cv2.INTER_LINEAR)
            y0 = (im.shape[0] - target_h) // 2
            x0 = (im.shape[1] - target_w) // 2
            im = cv2.cvtColor(im[y0:y0 + target_h, x0:x0 + target_w], cv2.COLOR_BGR2RGB)
            if len(self.cache) > 400:
                self.cache.clear()
            self.cache[path] = im
        return im


class StillFootage(Footage):  # noqa: D101
    """A screenshot with a slow push-in."""

    def __init__(self, path, box=None, seconds=15):
        im = Image.open(path).convert("RGB")
        if box:
            im = im.crop(box)
        self.base = im
        self.n = int(seconds * FPS)
        self.files = [path]

    def frame(self, i):
        t = min(i / max(self.n, 1), 1)
        im = self.base
        s = 1.0 + 0.08 * t
        w, h = im.size
        target = (W - 60) / VID_H
        cw = w / s
        ch = cw / target
        if ch > h / s:
            ch = h / s
            cw = ch * target
        x0, y0 = (w - cw) / 2, (h - ch) / 2
        crop = im.crop((int(x0), int(y0), int(x0 + cw), int(y0 + ch))).resize((W - 60, VID_H), Image.BILINEAR)
        return np.array(crop)


# --------------------------------------------------------------------------------------------------------------------
# Infographic pieces

def bar(d, x, y, w, label, value, max_value, color, t, start):
    p = ease_out((t - start) / 0.8)
    d.text((x, y), label, font=F[(26, "body")], fill=DIM)
    d.rounded_rectangle((x + 170, y + 6, x + 170 + w, y + 30), 12, fill=(255, 255, 255, 28))
    fill = int(w * min(value / max_value, 1) * p)
    if fill > 8:
        d.rounded_rectangle((x + 170, y + 6, x + 170 + fill, y + 30), 12, fill=color + (255,))
    shown = int(round(value * p))
    d.text((x + 190 + w, y - 6), str(shown), font=F[(40, "num")], fill=CREAM)


def card(layer, x, y, w, h, title, text, icon_name, t, start, accent=GOLD, number=None):
    """A sliding card: icon on the left, title and text on the right."""
    p = ease_out((t - start) / 0.45)
    if p <= 0:
        return
    dx = int((1 - p) * -120)
    a = int(255 * p)
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle((x + dx, y, x + dx + w, y + h), 22, fill=(24, 26, 36, int(235 * p)), outline=accent + (a,), width=3)
    if icon_name:
        size = h - 36
        ic = icon(icon_name, size)
        s = ease_back((t - start) / 0.5)
        sz = max(int(size * s), 1)
        ic2 = ic.resize((sz, sz))
        layer.alpha_composite(ic2, (x + dx + 18 + (size - sz) // 2, y + 18 + (size - sz) // 2))
        tx = x + dx + size + 40
    else:
        tx = x + dx + 26
    if number is not None:
        d.text((tx, y + 14), number, font=F[(40, "num")], fill=accent + (a,))
        tx += 56
    d.text((tx, y + 14), title, font=F[(34, "bold")], fill=CREAM + (a,))
    lines = wrap(d, text, F[(26, "body")], x + w - tx - 20)
    yy = y + 58
    for line in lines[:3]:
        d.text((tx, yy), line, font=F[(26, "body")], fill=DIM + (a,))
        yy += 32


def text_block(layer, x, y, w, text, t, start, fnt=None, color=CREAM):
    p = ease_out((t - start) / 0.5)
    if p <= 0:
        return y
    d = ImageDraw.Draw(layer)
    fnt = fnt or F[(30, "bold")]
    for line in wrap(d, text, fnt, w):
        d.text((x, y + int((1 - p) * 20)), line, font=fnt, fill=color + (int(255 * p),))
        y += fnt.size + 10
    return y


def big_number(layer, x, y, value, label, t, start, color=GOLD):
    p = ease_out((t - start) / 0.9)
    if p <= 0:
        return
    d = ImageDraw.Draw(layer)
    d.text((x, y), str(int(round(value * p))), font=F[(130, "num")], fill=color + (int(255 * min(1, p * 2)),))
    d.text((x + 6, y + 140), label, font=F[(28, "body")], fill=DIM + (int(255 * p),))


def countdown(layer, t, start, seconds=3):
    if not (start <= t < start + seconds):
        return
    k = seconds - int(t - start)
    frac = (t - start) % 1
    d = ImageDraw.Draw(layer)
    r = int(70 + 30 * (1 - frac))
    cx, cy = W // 2, VID_Y + VID_H // 2
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(16, 18, 26, 200), outline=GOLD + (255,), width=6)
    tw = d.textlength(str(k), font=F[(90, "num")])
    d.text((cx - tw / 2, cy - 58), str(k), font=F[(90, "num")], fill=GOLD)


# --------------------------------------------------------------------------------------------------------------------
# Rendering

def mix_audio(path, seconds, variant, pops):
    tmp = path + ".music.wav"
    make_music.main(tmp, seconds + 0.6, bpm=[132, 120, 140, 126, 116][variant % 5], transpose=[0, 2, -3, 5, -2][variant % 5],
                    seed=3 + variant, melody_shift=(variant * 3) % 16)
    with wave.open(tmp) as w:
        music = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, 2).astype(np.float32) / 32767
    music *= 0.55
    for at, name in pops:
        sfx_path = os.path.join(SFX, "SFX_%s.wav" % name)
        if not os.path.exists(sfx_path):
            continue
        with wave.open(sfx_path) as w:
            s = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float32) / 32767
        start = int(at * 44100)
        end = min(len(music), start + len(s))
        if start < len(music):
            music[start:end] += (s[:end - start] * 0.55)[:, None]
    music /= max(1.0, np.max(np.abs(music)) / 0.95)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(44100)
        w.writeframes((music * 32767).astype(np.int16).tobytes())
    os.remove(tmp)


def render(short, index):
    global BG
    if BG is None:
        BG = background()
    slug = "%02d_%s" % (index, short["slug"])
    raw = os.path.join(OUT, "work", slug + ".mp4")
    final = os.path.join(OUT, slug + ".mp4")
    os.makedirs(os.path.dirname(raw), exist_ok=True)
    seconds = short["seconds"]
    n = int(seconds * FPS)
    head = header_layer(short["series"], short["title"], short.get("accent", GOLD))
    base = BG.copy()
    base.alpha_composite(head)
    base_np = np.array(base.convert("RGB"))
    writer = cv2.VideoWriter(raw, cv2.VideoWriter_fourcc(*"mp4v"), FPS, (W, H))
    footage = short["footage"]
    for i in range(n):
        t = i / FPS
        frame = base_np.copy()
        frame[VID_Y:VID_Y + VID_H, 30:W - 30] = footage.frame(i)
        layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        short["panel"](layer, t)
        d = ImageDraw.Draw(layer)
        # Progress line at the top.
        d.rectangle((0, 0, int(W * (i + 1) / n), 8), fill=short.get("accent", GOLD) + (255,))
        over = np.array(layer)
        alpha = over[:, :, 3:4].astype(np.float32) / 255
        frame = (frame * (1 - alpha) + over[:, :, :3] * alpha).astype(np.uint8)
        # Fade in and out.
        fade = min(1.0, (i + 1) / 8, (n - i) / 10)
        if fade < 1:
            frame = (frame * fade).astype(np.uint8)
        writer.write(cv2.cvtColor(frame, cv2.COLOR_RGB2BGR))
    writer.release()
    audio = os.path.join(OUT, "work", slug + ".wav")
    mix_audio(audio, seconds, index, short.get("pops", []))
    subprocess.run(["powershell", "-ExecutionPolicy", "Bypass", "-File", r"C:\ProjectsUE\_tools\mux_audio.ps1", "-Video", raw,
                    "-Audio", audio, "-Out", final, "-Kbps", "6000", "-Width", "1080", "-Height", "1920"], check=False,
                   capture_output=True)
    print(slug, seconds, "s", os.path.exists(final) and os.path.getsize(final) // 1024, "KB")
    return final


if __name__ == "__main__":
    from shorts_catalog import catalog
    shorts = catalog()
    only = sys.argv[1] if len(sys.argv) > 1 else ""
    os.makedirs(OUT, exist_ok=True)
    plan = []
    # "a-b" renders that range of numbers; SKIP_EXISTING=1 keeps finished shorts (parallel runs).
    first, last = 1, len(shorts)
    if "-" in only and only.replace("-", "").isdigit():
        first, last = (int(x) for x in only.split("-"))
        only = ""
    for k, s in enumerate(shorts, 1):
        if not first <= k <= last or (only and only not in s["slug"] and only != str(k)):
            continue
        if os.environ.get("SKIP_EXISTING") and os.path.exists(os.path.join(OUT, "%02d_%s.mp4" % (k, s["slug"]))):
            continue
        render(s, k)
    json.dump([{k: v for k, v in s.items() if k in ("slug", "series", "title", "caption", "tags", "seconds")} for s in shorts],
              open(os.path.join(OUT, "plan.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
