"""Synthesises the battle sound effects of Keepers of the Lands (our own, so free of licences).

Usage: python make_sfx.py <out folder>  ->  SFX_<Name>.wav, 44.1 kHz mono 16 bit; imported by import_sfx.py.
"""
import os
import sys
import wave

import numpy as np

RATE = 44100
rng = np.random.default_rng(7)


def t(seconds):
    return np.arange(int(seconds * RATE)) / RATE


def env(n, attack=0.005, decay=0.2, curve=4.0):
    x = np.arange(n) / RATE
    a = np.clip(x / max(attack, 1e-4), 0, 1)
    d = np.exp(-curve * np.maximum(x - attack, 0) / max(decay, 1e-4))
    return a * d


def lowpass(x, cutoff):
    """One-pole low-pass; cutoff may be an array (a sweep)."""
    cutoff = np.broadcast_to(np.asarray(cutoff, dtype=float), x.shape)
    alpha = 1 - np.exp(-2 * np.pi * cutoff / RATE)
    y = np.empty_like(x)
    acc = 0.0
    for i in range(len(x)):
        acc += alpha[i] * (x[i] - acc)
        y[i] = acc
    return y


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def noise(seconds):
    return rng.uniform(-1, 1, int(seconds * RATE))


def tone(freq, seconds, shape="sine"):
    tt = t(seconds)
    phase = 2 * np.pi * np.cumsum(np.broadcast_to(np.asarray(freq, dtype=float), tt.shape)) / RATE
    if shape == "square":
        return np.sign(np.sin(phase)) * 0.6
    if shape == "tri":
        return 2 / np.pi * np.arcsin(np.sin(phase))
    return np.sin(phase)


def bell(freq, seconds, decay):
    tt = t(seconds)
    s = np.sin(2 * np.pi * freq * tt) + 0.5 * np.sin(2 * np.pi * freq * 2.76 * tt) + 0.25 * np.sin(2 * np.pi * freq * 5.4 * tt)
    return s * np.exp(-tt / decay)


def place(total, part, at):
    start = int(at * RATE)
    end = min(len(total), start + len(part))
    total[start:end] += part[:end - start]
    return total


def finish(x, gain=0.85):
    x = x - np.mean(x)
    peak = np.max(np.abs(x)) or 1
    x = x / peak * gain
    fade = min(len(x), int(0.01 * RATE))
    x[-fade:] *= np.linspace(1, 0, fade)
    return x


def hit():
    n = noise(0.22)
    body = lowpass(n, np.linspace(3000, 300, len(n))) * env(len(n), 0.002, 0.08)
    thump = tone(np.linspace(140, 60, len(n)), 0.22) * env(len(n), 0.002, 0.12)
    return finish(body * 1.4 + thump)


def slime():
    d = 0.35
    wob = tone(110 + 60 * np.sin(2 * np.pi * 9 * t(d)), d) * env(int(d * RATE), 0.01, 0.2)
    sq = lowpass(noise(d), 900) * env(int(d * RATE), 0.005, 0.1) * 2
    return finish(wob + sq)


def heal():
    out = np.zeros(int(0.9 * RATE))
    for i, f in enumerate((523.25, 659.25, 783.99, 1046.5)):
        place(out, bell(f, 0.6, 0.25) * 0.6, i * 0.08)
    sparkle = highpass(noise(0.9), 6000) * env(int(0.9 * RATE), 0.2, 0.4) * 0.15
    return finish(out + sparkle, 0.7)


def puff():
    d = 0.6
    n = noise(d)
    w = lowpass(n, np.linspace(2500, 400, len(n))) * env(len(n), 0.04, 0.3, 3)
    return finish(w, 0.7)


def rock():
    d = 0.8
    rumble = lowpass(noise(d), 180) * env(int(d * RATE), 0.05, 0.5, 3) * 3
    crunch = np.zeros(int(d * RATE))
    for k in range(10):
        c = lowpass(noise(0.05), 2500) * env(int(0.05 * RATE), 0.001, 0.02)
        place(crunch, c * rng.uniform(0.4, 1), rng.uniform(0.0, 0.45))
    return finish(rumble + crunch)


def leaves():
    d = 0.7
    out = np.zeros(int(d * RATE))
    for k in range(40):
        c = highpass(noise(0.03), 3000) * env(int(0.03 * RATE), 0.002, 0.012)
        place(out, c * rng.uniform(0.2, 1), rng.uniform(0, 0.6))
    return finish(out + heal()[:len(out)] * 0.15, 0.6)


def gust():
    d = 0.9
    n = noise(d)
    sweep = 400 + 2200 * np.sin(np.pi * np.linspace(0, 1, len(n)))
    w = lowpass(highpass(n, 200), sweep) * np.sin(np.pi * np.linspace(0, 1, len(n))) ** 1.5
    return finish(w, 0.7)


def splash():
    d = 0.8
    burst = lowpass(noise(d), np.linspace(5000, 600, int(d * RATE))) * env(int(d * RATE), 0.003, 0.25, 3)
    out = burst.copy()
    for k in range(8):
        f = rng.uniform(500, 1200)
        b = tone(np.linspace(f, f * 1.8, int(0.06 * RATE)), 0.06) * env(int(0.06 * RATE), 0.002, 0.03)
        place(out, b * 0.4, rng.uniform(0.1, 0.6))
    return finish(out, 0.75)


def coin():
    out = np.zeros(int(0.5 * RATE))
    place(out, bell(1568, 0.4, 0.12), 0.0)
    place(out, bell(2093, 0.4, 0.15), 0.07)
    return finish(out, 0.6)


def card():
    d = 0.16
    n = highpass(noise(d), 1500) * env(int(d * RATE), 0.002, 0.04)
    return finish(n, 0.5)


def level_up():
    out = np.zeros(int(1.1 * RATE))
    for i, f in enumerate((392.0, 523.25, 659.25, 783.99)):
        s = (tone(f, 0.5, "tri") * 0.6 + tone(f * 2, 0.5) * 0.2) * env(int(0.5 * RATE), 0.01, 0.25)
        place(out, s, i * 0.09)
    place(out, bell(1046.5, 0.7, 0.3) * 0.5, 0.36)
    return finish(out, 0.7)


def victory():
    out = np.zeros(int(1.8 * RATE))
    notes = [(523.25, 0.0, 0.18), (523.25, 0.2, 0.18), (523.25, 0.4, 0.18), (659.25, 0.6, 0.5), (587.33, 0.95, 0.2), (659.25, 1.15, 0.6)]
    for f, at, length in notes:
        s = (tone(f, length + 0.2, "tri") * 0.7 + tone(f * 1.5, length + 0.2) * 0.15) * env(int((length + 0.2) * RATE), 0.01, length, 2)
        place(out, s, at)
    return finish(out, 0.7)


def defeat():
    out = np.zeros(int(1.5 * RATE))
    for i, f in enumerate((392.0, 349.23, 311.13, 261.63)):
        s = tone(f, 0.5, "tri") * env(int(0.5 * RATE), 0.01, 0.35, 2)
        place(out, s, i * 0.25)
    return finish(out, 0.6)


SOUNDS = {
    "Hit": hit, "Slime": slime, "Heal": heal, "Puff": puff, "Rock": rock, "Leaves": leaves,
    "Gust": gust, "Splash": splash, "Coin": coin, "Card": card, "LevelUp": level_up,
    "Victory": victory, "Defeat": defeat,
}


def save(path, x):
    data = (np.clip(x, -1, 1) * 32767).astype(np.int16)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())


def main(out):
    os.makedirs(out, exist_ok=True)
    for name, make in SOUNDS.items():
        save(os.path.join(out, f"SFX_{name}.wav"), make())
    print(len(SOUNDS), "sounds in", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else r"C:/ProjectsUE/_logs/sfx")
