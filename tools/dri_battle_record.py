#!/usr/bin/env python3
"""Run one DRI challenge headless, mash CROSS (the EU disc confirms with CROSS), keep the party alive, record an mp4 and print the enemy's state.

  python3 tools/dri_battle_record.py CHALLENGE [--frames N] [--name NAME] [--god] [--every K]

Needs .dumps/dri/field.state made by THIS build (see .plan/tasks/dri-player-forms.md "Testing without going through
the story"). Output: .dumps/dri/NAME.mp4 and NAME.log.
"""
import argparse, os, re, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mcp_game import Game  # noqa: E402

BIN = ROOT / "build/port/dw2003"
DISC = ROOT / "iso/dw2003.cue"
OUT = ROOT / ".dumps/dri"


def sym(name):
    for line in subprocess.check_output(["nm", "-n", str(BIN)], text=True).splitlines():
        m = re.match(rf"^([0-9a-fA-F]+)\s+\S+\s+{re.escape(name)}$", line.strip())
        if m:
            return int(m.group(1), 16)
    raise SystemExit(f"symbol {name} not found")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("challenge", type=int)
    ap.add_argument("--frames", type=int, default=1500)
    ap.add_argument("--every", type=int, default=2, help="game frames per video frame (2 = 30 fps in real time)")
    ap.add_argument("--name")
    ap.add_argument("--god", action="store_true", help="refill the party's HP every step")
    ap.add_argument("--state", default=str(OUT / "field.state"))
    a = ap.parse_args()
    name = a.name or f"ch{a.challenge}"
    frames_dir = OUT / f"{name}_frames"
    frames_dir.mkdir(parents=True, exist_ok=True)
    for old in frames_dir.glob("*.ppm"):
        old.unlink()
    FB, DBG, UNL = sym("fightstg_battle"), sym("def_dri_debug"), sym("dri_unlocked")
    env = dict(os.environ, DW3_DEFINITIVE="1")
    g = Game.spawn([str(BIN), "--disc", str(DISC), "--cd-speed", "instant", "--max-frames", "0", "--watchdog", "0"],
                   cwd=str(ROOT), env=env, hold=True)
    log = open(OUT / f"{name}.log", "w")

    def say(s):
        print(s)
        log.write(s + "\n")

    def member(side_off, i):
        d, _b, _t, mx, hp, mmx, mp = struct.unpack("<7h", g.peek(FB + side_off + i * 0x20, 14))
        return d, hp, mx, mp

    try:
        g.load_state(a.state)
        g.step(300)
        g.poke(UNL, struct.pack("<i", 31))
        g.poke(DBG, struct.pack("<i", a.challenge))
        n = 0
        last_enemy = None
        for it in range(a.frames // a.every):
            g.step(a.every)
            g.screenshot(str(frames_dir / f"f_{n:05d}.ppm"))
            n += 1
            if a.god:  # 30000 HP each: a 9999 hit does not end the show; a drop is an enemy hit
                for i in range(3):
                    d, hp, mx, _mp = member(0x10, i)
                    if d and hp > 0:
                        if hp < 30000 and mx == 30000:
                            say(f"[{it * a.every:5d}] enemy hit party {i} ({d}) for {30000 - hp}")
                        g.poke(FB + 0x10 + i * 0x20 + 6, struct.pack("<2h", 30000, 30000))
            if it % 8 == 5:
                g.pad(["CROSS"], 2, 2, sync=True)
            if it % 30 == 0:
                en = [member(0x70, i) for i in range(3)]
                if en != last_enemy:
                    say(f"[{it * a.every:5d}] enemies (digimon,hp,maxhp,mp) {en}  party {[member(0x10, i) for i in range(3)]}")
                    last_enemy = en
        g.pad_free()
        say("--- game log (dri/def/battle lines) ---")
        for l in g.log_text(400).splitlines():
            if any(k in l for k in ("dri:", "digivolve", "technique", "tech", "camera", "crash", "error")):
                say(l)
    finally:
        g.stop()
    mp4 = OUT / f"{name}.mp4"
    subprocess.run(["ffmpeg", "-y", "-framerate", str(60 // a.every), "-i", str(frames_dir / "f_%05d.ppm"), "-c:v", "libx264",
                    "-pix_fmt", "yuv420p", "-crf", "23", "-vf", "scale=640:480:flags=neighbor", str(mp4)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    say(f"wrote {mp4} ({mp4.stat().st_size // 1024} KB, {n} frames)")


main()
