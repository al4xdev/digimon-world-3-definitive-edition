#!/usr/bin/env python3
"""Girls Power (port/game/girls_power.c): Kaili is the hero, Junior her friend. The game is run with DW3_GIRLS_DUMP,
which writes the eight field sprite files of the two as the port repaints them, and each is checked against the disc's
own file: every frame the animation tables name has the other's CLUT, every frame changed, and the other's head is on
(her pink hair, index 4 of her palette, in the top half of his frames; his cap's lavender, index 7 of his, in hers).
Also: under a replay script the swap is off unless DW3_GIRLS is set (the scripts are shared with the emulator); in a
normal run the save's DEF_SAVE_KAILI (New Game's hero question, after the difficulty) decides.
Needs the built port (build/port/dw2003) and the disc (iso/dw2003.cue)."""
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import disc_files  # noqa: E402

BINARY = ROOT / "build/port/dw2003"
DISC = ROOT / "iso/dw2003.cue"
SCRIPT = ROOT / "tests/replay/scripts/first_battle_save.json"
JUNIOR, KAILI = 0x187, 0x190  # the files whose idle frames give the CLUTs
# file id: (animation table, becomes Kaili)
FILES = {0x186: (0xCA, True), 0x187: (0x70, True), 0x3C9: (0x0B, True), 0x3CA: (0x64, True), 0x3CB: (0x1E, True),
         0x3CC: (0x17, True), 0x190: (0xA8, False), 0x2F8: (0x26, False)}
HAIR, CAP = 4, 7


def sub(d, i):
    return struct.unpack_from("<i", d, i * 4)[0]


def frames(d, table):
    out, p = [], sub(d, table)
    while True:
        rec = struct.unpack_from("<6h", d, p)
        if rec[0] == 0:
            return sorted(set(out))
        for s in rec[1:]:
            q = sub(d, s)
            while True:
                t, f = struct.unpack_from("<2i", d, q)
                if t <= 0:
                    break
                out.append(f)
                q += 16
        p += 12


def tim(d, f):
    p = sub(d, f)
    q = p + 8
    clut = d[q + 12:q + 12 + 32]
    q += struct.unpack_from("<I", d, q)[0]
    w, h = struct.unpack_from("<2H", d, q + 8)
    px = d[q + 12:q + 12 + w * 2 * h]
    rows = [[(px[y * w * 2 + x // 2] >> (4 if x & 1 else 0)) & 15 for x in range(w * 4)] for y in range(h)]
    return clut, rows


def run(env, args, timeout=300):
    e = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", **env)
    return subprocess.run([str(BINARY), "--disc", str(DISC), *args], env=e, capture_output=True, text=True,
                          timeout=timeout, cwd=str(ROOT))


def main():
    if not BINARY.exists() or not DISC.exists():
        print("skip: needs build/port/dw2003 and iso/dw2003.cue")
        return 0
    ok = True
    clut_of = {}
    for who in (JUNIOR, KAILI):
        d = disc_files.read(who)
        clut_of[who] = tim(d, frames(d, FILES[who][0])[0])[0]
    with tempfile.TemporaryDirectory() as out:
        r = run({"DW3_GIRLS": "1", "DW3_GIRLS_DUMP": out}, ["--script", str(SCRIPT), "--max-frames", "2500"])
        on_line = "girls power: the hero is Kaili" in r.stderr
        print(f"{'ok' if on_line else 'FAIL'} DW3_GIRLS=1 turns it on under a script")
        ok &= on_line
        for fid, (table, to_kaili) in FILES.items():
            path = Path(out) / f"{fid:03X}.bin"
            if not path.exists():
                print(f"FAIL {fid:#x}: not written")
                ok = False
                continue
            orig, new = disc_files.read(fid), path.read_bytes()
            want_clut = clut_of[KAILI if to_kaili else JUNIOR]
            key = HAIR if to_kaili else CAP
            fl = frames(orig, table)
            n_clut = n_changed = n_head = 0
            for f in fl:
                c, rows = tim(new, f)
                _, rows0 = tim(orig, f)
                n_clut += c == want_clut
                n_changed += rows != rows0
                n_head += any(v == key for r_ in rows[:len(rows) // 2] for v in r_)
            good = n_clut == len(fl) and n_changed == len(fl) and n_head >= 0.95 * len(fl)
            print(f"{'ok' if good else 'FAIL'} {fid:#x}: {len(fl)} frames, {n_clut} with the other's CLUT, "
                  f"{n_changed} changed, {n_head} with the other's head")
            ok &= good
    r = run({}, ["--script", str(SCRIPT), "--max-frames", "2500"])
    off = "girls power" not in r.stderr
    print(f"{'ok' if off else 'FAIL'} off under a replay script without DW3_GIRLS")
    ok &= off
    print("girls power:", "pass" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
