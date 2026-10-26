"""A short upbeat fantasy theme for the trailer (our own synthesis): drums, bass, plucked-lute arpeggios, a melody.

Usage: python make_music.py <out.wav> <seconds>
"""
import sys
import wave

import numpy as np

RATE = 44100
BPM = 132
BEAT = 60 / BPM
rng = np.random.default_rng(3)


def pluck(freq, seconds):
    """Karplus-Strong string."""
    n = int(seconds * RATE)
    period = max(2, int(RATE / freq))
    buf = rng.uniform(-1, 1, period)
    out = np.empty(n)
    idx = 0
    for i in range(n):
        out[i] = buf[idx]
        nxt = (idx + 1) % period
        buf[idx] = 0.996 * 0.5 * (buf[idx] + buf[nxt])
        idx = nxt
    return out


def sine(freq, seconds, decay=0.4):
    tt = np.arange(int(seconds * RATE)) / RATE
    return np.sin(2 * np.pi * freq * tt) * np.exp(-tt / decay)


def kick():
    tt = np.arange(int(0.25 * RATE)) / RATE
    f = 110 * np.exp(-tt * 18) + 45
    return np.sin(2 * np.pi * np.cumsum(f) / RATE) * np.exp(-tt * 9)


def snare():
    tt = np.arange(int(0.2 * RATE)) / RATE
    n = rng.uniform(-1, 1, len(tt))
    return (n * 0.7 + np.sin(2 * np.pi * 190 * tt) * 0.4) * np.exp(-tt * 22)


def hat():
    tt = np.arange(int(0.06 * RATE)) / RATE
    n = rng.uniform(-1, 1, len(tt))
    n = n - np.concatenate([[0], n[:-1]])
    return n * np.exp(-tt * 70) * 0.5


def place(track, sound, at, gain=1.0):
    s = int(at * RATE)
    e = min(len(track), s + len(sound))
    if s < len(track):
        track[s:e] += sound[:e - s] * gain


TRANSPOSE = [0]


def note(name):
    names = {"C": -9, "C#": -8, "D": -7, "D#": -6, "E": -5, "F": -4, "F#": -3, "G": -2, "G#": -1, "A": 0, "A#": 1, "B": 2}
    pitch, octave = name[:-1], int(name[-1])
    return 440.0 * 2 ** ((names[pitch] + (octave - 4) * 12 + TRANSPOSE[0]) / 12)


def main(out, seconds, loop=False, bpm=None, transpose=0, seed=3, melody_shift=0):
    """loop: whole bars, no intro and no fades, so the end joins the start (battle music).
    bpm, transpose (semitones), seed and melody_shift make variations of the theme (for the shorts)."""
    global BEAT, rng
    BEAT = 60 / (bpm or BPM)
    rng = np.random.default_rng(seed)
    if loop:
        seconds = round(seconds / (4 * BEAT)) * 4 * BEAT
    TRANSPOSE[0] = transpose
    n = int(seconds * RATE)
    tail = int(0.8 * RATE)
    drums, bass, lute, lead = (np.zeros(n + tail) for _ in range(4))
    # D minor heroic progression: Dm - Bb - F - C, two bars each chord... one bar each.
    chords = [("D3", ["D4", "F4", "A4", "D5"]), ("A#2", ["A#3", "D4", "F4", "A#4"]),
              ("F2", ["F3", "A3", "C4", "F4"]), ("C3", ["C4", "E4", "G4", "C5"])]
    melody = ["A4", "D5", "F5", "E5", "D5", "C5", "A#4", "A4", "F4", "A4", "C5", "A4", "G4", "E4", "G4", "C5"]
    bar = 4 * BEAT
    bars = int(seconds / bar) + 1
    for b in range(bars):
        root, arp = chords[b % 4]
        t0 = b * bar
        intro = b < 2 and not loop
        # Drums: kick on 1 and 3 (and the "and" of 4), snare on 2 and 4, eighth hats.
        if not intro:
            for k in (0, 2, 3.5):
                place(drums, kick(), t0 + k * BEAT, 0.9)
            for k in (1, 3):
                place(drums, snare(), t0 + k * BEAT, 0.5)
        for k in range(8):
            place(drums, hat(), t0 + k * BEAT / 2, 0.25 if intro else 0.35)
        # Bass: root on eighths with an octave jump.
        for k in range(8):
            f = note(root) * (2 if k % 4 == 3 else 1)
            place(bass, sine(f, BEAT / 2 * 0.95, 0.25), t0 + k * BEAT / 2, 0.55)
        # Lute arpeggio in sixteenths.
        for k in range(16):
            f = note(arp[[0, 1, 2, 3, 2, 1, 2, 3][k % 8]])
            place(lute, pluck(f, 0.5), t0 + k * BEAT / 4, 0.32)
        # Melody from the third bar: two notes per beat pair.
        if b >= 2 or loop:
            for k in range(4):
                f = note(melody[((b - 2) * 4 + k + melody_shift) % len(melody)])
                s = pluck(f, BEAT * 1.2) * 0.6 + sine(f, BEAT * 1.2, 0.5) * 0.4
                place(lead, s, t0 + k * BEAT, 0.5)
    mix_l = drums * 0.9 + bass + lute * 0.8 + lead * 0.9
    mix_r = drums * 0.9 + bass + lute * 1.0 + lead * 0.7
    if loop:
        # The tails of the last notes wrap round to the start, so the loop joins without a click.
        mix_l[:tail] += mix_l[n:]
        mix_r[:tail] += mix_r[n:]
    mix_l, mix_r = mix_l[:n], mix_r[:n]
    # Fade in, fade out.
    fade = np.ones(n)
    if not loop:
        fi, fo = int(0.5 * RATE), int(2.5 * RATE)
        fade[:fi] = np.linspace(0, 1, fi)
        fade[-fo:] = np.linspace(1, 0, fo)
    stereo = np.stack([mix_l * fade, mix_r * fade], axis=1)
    stereo /= np.max(np.abs(stereo)) or 1
    data = (stereo * 0.85 * 32767).astype(np.int16)
    with wave.open(out, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())
    print("written", out, seconds, "s")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else r"C:/ProjectsUE/_logs/rec/theme.wav", float(sys.argv[2]) if len(sys.argv) > 2 else 48.0,
         "--loop" in sys.argv)
