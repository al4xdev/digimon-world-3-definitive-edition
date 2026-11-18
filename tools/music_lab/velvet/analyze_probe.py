#!/usr/bin/env python3
"""Per-part attack, release and edge noise of play.py --probe renders (one 1 s note per part, 4 s apart).

  uv run --with numpy python tools/music_lab/velvet/analyze_probe.py render/probe_off.wav render/probe_on.wav

attack ms    note-on to 90 % of the note's peak envelope (1 ms smoothing)
release ms   note-off to -20 dB / -40 dB of the level at the note-off
tail dB      level 1.2 s after the note-off, relative to the peak (the reverb, and a long release)
on edge      largest sample step in the 3 ms after the onset over the note's rms: a hard attack is a click
off edge     the same around the note-off: a cut-off release is a click
>8 kHz       share of the energy above 8 kHz in the first 30 ms of the note (the attack's brightness)
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze import load  # noqa: E402

NAMES = ["Flute", "Oboe", "Piano", "Violin", "Viola", "Horn", "Cello", "DoubleBass"]
SR = 44100


def env(m, ms=1.0):
    w = max(1, int(SR * ms / 1000))
    a = np.abs(m)
    k = np.ones(w) / w
    return np.convolve(a, k, mode="same")


def first_note(m):
    """Where the first note sounds (play.py's 40-vsync warm-up runs before the slice: it is not at 1 s)."""
    e = env(m[:3 * SR], 2.0)
    return int(np.argmax(e > 0.1 * e.max()))


def part(m, i, t0):
    lo = max(0, t0 + int((4 * i - 0.2) * SR))
    seg = m[lo:lo + int(3.6 * SR)]
    e = env(seg)
    peak = e.max()
    onset = int(np.argmax(e > 0.02 * peak))
    t90 = int(np.argmax(e > 0.9 * peak))
    off = onset + SR  # the note is held 1 s from its (audible) start
    ref = e[off - 20:off].mean()
    after = e[off:]
    r20 = int(np.argmax(after < ref * 0.1)) if (after < ref * 0.1).any() else len(after)
    r40 = int(np.argmax(after < ref * 0.01)) if (after < ref * 0.01).any() else len(after)
    rms = np.sqrt((seg[onset:off] ** 2).mean())
    on_edge = np.abs(np.diff(seg[onset:onset + int(0.003 * SR)])).max() / max(rms, 1)
    off_edge = np.abs(np.diff(seg[off - int(0.003 * SR):off + int(0.003 * SR)])).max() / max(rms, 1)
    tail = 20 * np.log10(e[off + int(1.2 * SR):off + int(1.25 * SR)].mean() / peak + 1e-9)
    a = seg[onset:onset + int(0.03 * SR)] * np.hanning(int(0.03 * SR))
    sp = np.abs(np.fft.rfft(a)) ** 2
    f = np.fft.rfftfreq(len(a), 1 / SR)
    return dict(peak=peak, attack=(t90 - onset) / SR * 1000, rel20=r20 / SR * 1000, rel40=r40 / SR * 1000, tail=tail,
                on_edge=on_edge, off_edge=off_edge, hf=sp[f > 8000].sum() / sp.sum())


def main():
    runs = []
    for p in sys.argv[1:]:
        x, _meta = load(p)
        m = x.mean(axis=1)
        runs.append((Path(p).stem, [part(m, i, first_note(m)) for i in range(8)]))
    print(f"{'part':<11}{'run':<11}{'peak':>7}{'attack ms':>10}{'rel -20dB':>10}{'rel -40dB':>10}{'tail dB':>8}"
          f"{'on edge':>8}{'off edge':>9}{'>8kHz':>8}")
    for i, name in enumerate(NAMES):
        for stem, rows in runs:
            r = rows[i]
            print(f"{name:<11}{stem:<11}{r['peak']:>7.0f}{r['attack']:>10.1f}{r['rel20']:>10.0f}{r['rel40']:>10.0f}"
                  f"{r['tail']:>8.1f}{r['on_edge']:>8.2f}{r['off_edge']:>9.2f}{r['hf']:>8.4f}")


if __name__ == "__main__":
    main()
