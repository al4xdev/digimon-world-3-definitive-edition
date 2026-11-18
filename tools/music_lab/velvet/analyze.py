#!/usr/bin/env python3
"""Compare two renders of the room's score (play.py --wav): what makes it sound coarse, in numbers.

  uv run --with numpy python tools/music_lab/velvet/analyze.py render/before.wav render/after.wav

For each WAV (a 44.1 kHz stereo dump of the headless game, played from the vsync play.py installed the score at):
  peak / clipped    the SPU sums its voices and saturates at 16 bits: clipped samples are a harsh buzz
  clicks            samples whose step from the previous one is 6x the local typical step (a hard edge: a cut-off
                    release or an instant attack)
  cuts              2 ms windows whose level falls 15 dB or more from the window before while the signal was loud
                    (a note stopped dead instead of dying away)
  >8 kHz, >4 kHz    share of the energy above those frequencies (the "grosseira" edge)
  onset 5 ms        mean peak level in the 5 ms after the loudest 1 % of onsets, over the 50 ms mean level
"""
import json
import sys
import wave
from pathlib import Path

import numpy as np

FRAME_SAMPLES = 882  # PAL: samples per vsync


def load(path):
    path = Path(path)
    meta = json.loads(Path(str(path) + ".json").read_text())
    with wave.open(str(path)) as w:
        assert w.getnchannels() == 2 and w.getframerate() == 44100
        raw = w.readframes(w.getnframes())
    x = np.frombuffer(raw, dtype="<i2").reshape(-1, 2).astype(np.float64)
    a = (meta["start_frame"] + 4) * FRAME_SAMPLES
    return x[a:a + meta["frames"] * FRAME_SAMPLES - 4 * FRAME_SAMPLES], meta


def measure(x):
    m = x.mean(axis=1)
    out = {}
    out["peak"] = int(np.abs(x).max())
    out["clipped"] = int((np.abs(x) >= 32767).sum())
    out["rms"] = float(np.sqrt((m ** 2).mean()))
    d = np.abs(np.diff(m))
    win = 441
    n = len(d) // win * win
    typical = np.median(d[:n].reshape(-1, win), axis=1)
    local = np.repeat(typical, win)
    loud = np.repeat(np.sqrt((m[:n].reshape(-1, win) ** 2).mean(axis=1)), win) > 0.3 * out["rms"]
    out["clicks"] = int(((d[:n] > 6 * local + 0.05 * out["rms"]) & loud).sum())
    w2 = 88  # 2 ms
    n2 = len(m) // w2 * w2
    lv = np.sqrt((m[:n2].reshape(-1, w2) ** 2).mean(axis=1)) + 1e-9
    out["cuts"] = int(((lv[1:] < lv[:-1] * 10 ** (-15 / 20)) & (lv[:-1] > 0.5 * out["rms"])).sum())
    spec = np.abs(np.fft.rfft(m * np.hanning(len(m)))) ** 2
    f = np.fft.rfftfreq(len(m), 1 / 44100)
    total = spec.sum()
    out["above_8k"] = float(spec[f > 8000].sum() / total)
    out["above_4k"] = float(spec[f > 4000].sum() / total)
    # attack: where the 5 ms level jumps the most, how steep
    w5 = 220
    n5 = len(m) // w5 * w5
    l5 = np.sqrt((m[:n5].reshape(-1, w5) ** 2).mean(axis=1)) + 1e-9
    rise = l5[1:] / l5[:-1]
    out["steep_onsets"] = int((rise > 3).sum())
    return out


def main():
    paths = sys.argv[1:]
    rows = []
    for p in paths:
        x, meta = load(p)
        rows.append((Path(p).stem, measure(x), meta))
    keys = list(rows[0][1])
    print(f"{'':<14}" + "".join(f"{name:>14}" for name, _m, _meta in rows))
    for k in keys:
        cells = []
        for _n, m, _meta in rows:
            v = m[k]
            cells.append(f"{v:>14.4f}" if isinstance(v, float) and v < 10 else f"{v:>14.0f}")
        print(f"{k:<14}" + "".join(cells))
    print(f"{'busy_max':<14}" + "".join(f"{meta['busy_max']:>14}" for _n, _m, meta in rows))


if __name__ == "__main__":
    main()
