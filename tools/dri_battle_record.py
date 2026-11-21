#!/usr/bin/env python3
"""Run one DRI challenge headless with the party fighting (CIRCLE often: Fight in the battle menu's default places,
game_mods.c; CROSS now and then: the EU disc's confirm, which the battle messages wait for, and Tag on the menu, so
rarely), keep the party alive, record an mp4 and print the enemy's state.

  python3 tools/dri_battle_record.py CHALLENGE [--frames N] [--name NAME] [--god] [--every K] [--turns T]

Needs .dumps/dri/field.state made by THIS build (see .plan/tasks/dri-player-forms.md "Testing without going through
the story"). Output: .dumps/dri/NAME.mp4, NAME.log and the battle log NAME.battle.log (DW3_BATTLE_LOG: every action's
technique). With --turns T, a stage that has taken T turns is left at 1 HP, so the party's next hit fells it and the
next stage comes in: every stage of the line is seen, each in its own clip (NAME_s0.mp4 ...), short enough for a video
model to read.
"""
import argparse, os, re, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mcp_game import Game  # noqa: E402

BIN = ROOT / "build/port/dw2003"
DISC = ROOT / "iso/dw2003.cue"
OUT = ROOT / ".dumps/dri"
GOD_HP = 30000


def sym(name):
    for line in subprocess.check_output(["nm", "-n", str(BIN)], text=True).splitlines():
        m = re.match(rf"^([0-9a-fA-F]+)\s+\S+\s+{re.escape(name)}$", line.strip())
        if m:
            return int(m.group(1), 16)
    raise SystemExit(f"symbol {name} not found")


def encode(frames_dir, fps, first, count, mp4):
    subprocess.run(["ffmpeg", "-y", "-framerate", str(fps), "-start_number", str(first), "-i", str(frames_dir / "f_%05d.ppm"),
                    "-frames:v", str(count), "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "23",
                    "-vf", "scale=640:480:flags=neighbor", str(mp4)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("challenge", type=int)
    ap.add_argument("--frames", type=int, default=1500)
    ap.add_argument("--every", type=int, default=2, help="game frames per video frame (2 = 30 fps in real time)")
    ap.add_argument("--name")
    ap.add_argument("--god", action="store_true", help="refill the party's HP every step")
    ap.add_argument("--turns", type=int, default=0, help="fell a stage after this many of its turns (0: never)")
    ap.add_argument("--state", default=str(OUT / "field.state"))
    a = ap.parse_args()
    name = a.name or f"ch{a.challenge}"
    frames_dir = OUT / f"{name}_frames"
    frames_dir.mkdir(parents=True, exist_ok=True)
    for old in frames_dir.glob("*.ppm"):
        old.unlink()
    blog = OUT / f"{name}.battle.log"
    blog.write_text("")
    FB, DBG, UNL = sym("fightstg_battle"), sym("def_dri_debug"), sym("dri_unlocked")
    # HOME of its own: the game keeps its settings and memory cards under ~/.local/share/dw2003 (DW3_CONFIG_DIR is not
    # read), and a test run must not touch the player's; the battle menu's places are then the defaults (Fight on CIRCLE)
    home = OUT / "home"
    home.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, DW3_DEFINITIVE="1", DW3_BATTLE_LOG=str(blog), HOME=str(home))
    g = Game.spawn([str(BIN), "--disc", str(DISC), "--cd-speed", "instant", "--max-frames", "0", "--watchdog", "0"],
                   cwd=str(ROOT), env=env, hold=True)
    log = open(OUT / f"{name}.log", "w")

    def say(s):
        print(s)
        log.write(s + "\n")

    def member(side_off, i):
        d, _b, _t, mx, hp, mmx, mp = struct.unpack("<7h", g.peek(FB + side_off + i * 0x20, 14))
        return d, hp, mx, mp

    def active():
        """The stage fighting: the first enemy member still standing (the battle brings them in in order)."""
        for i in range(3):
            d, hp, _mx, _mp = member(0x70, i)
            if d and hp > 0:
                return i
        return -1

    seen = 0  # bytes of the battle log read
    stages = []  # [stage index, first video frame, [technique IDs]]
    try:
        g.load_state(a.state)
        g.step(300)
        g.poke(UNL, struct.pack("<i", 31))
        g.poke(DBG, struct.pack("<i", a.challenge))
        n = 0
        last_enemy = None
        felled = -1
        for it in range(a.frames // a.every):
            g.step(a.every)
            g.screenshot(str(frames_dir / f"f_{n:05d}.ppm"))
            n += 1
            if a.god:  # GOD_HP each: a 9999 hit does not end the show; a drop is an enemy hit
                for i in range(3):
                    d, hp, mx, _mp = member(0x10, i)
                    if d and hp > 0:
                        if hp < GOD_HP and mx == GOD_HP:
                            say(f"[{it * a.every:5d}] enemy hit party {i} ({d}) for {GOD_HP - hp}")
                        g.poke(FB + 0x10 + i * 0x20 + 6, struct.pack("<2h", GOD_HP, GOD_HP))
            cur = active()
            if cur >= 0 and (not stages or stages[-1][0] != cur):
                stages.append([cur, n - 1, []])
                say(f"[{it * a.every:5d}] stage {cur} fights: {member(0x70, cur)}")
            text = blog.read_text(errors="replace")
            for line in text[seen:].splitlines():
                m = re.search(r"action side 1 kind (-?\d+)", line)
                if m and stages:
                    stages[-1][2].append(int(m.group(1)))
                    say(f"[{it * a.every:5d}] stage {stages[-1][0]} acts: {line.strip()}")
            seen = len(text)
            if a.turns and stages and cur == stages[-1][0] and felled != cur and len(stages[-1][2]) >= a.turns:
                _d, hp, _mx, _mp = member(0x70, cur)
                if hp > 1:
                    g.poke(FB + 0x70 + cur * 0x20 + 8, struct.pack("<h", 1))
                    felled = cur
                    say(f"[{it * a.every:5d}] stage {cur} left at 1 HP after {len(stages[-1][2])} turns")
            if it % 16 == 9:
                g.pad(["CROSS"], 2, 2, sync=True)
            elif it % 2 == 0:
                g.pad(["CIRCLE"], 2, 2, sync=True)
            if it % 30 == 0:
                en = [member(0x70, i) for i in range(3)]
                if en != last_enemy:
                    say(f"[{it * a.every:5d}] enemies (digimon,hp,maxhp,mp) {en}  party {[member(0x10, i) for i in range(3)]}")
                    last_enemy = en
            if a.turns and stages and cur < 0 and len(stages) >= 3:
                say(f"[{it * a.every:5d}] every stage felled")
                g.step(240)
                break
        g.pad_free()
        say("--- game log (dri/def/battle lines) ---")
        for l in g.log_text(400).splitlines():
            if any(k in l for k in ("dri:", "digivolve", "technique", "tech", "camera", "crash", "error")):
                say(l)
    finally:
        g.stop()
    fps = 60 // a.every
    mp4 = OUT / f"{name}.mp4"
    encode(frames_dir, fps, 0, n, mp4)
    say(f"wrote {mp4} ({mp4.stat().st_size // 1024} KB, {n} frames)")
    for k, (stage, first, acts) in enumerate(stages):
        end = stages[k + 1][1] if k + 1 < len(stages) else n
        clip = OUT / f"{name}_s{stage}.mp4"
        encode(frames_dir, fps, max(0, first - fps), end - max(0, first - fps), clip)
        say(f"stage {stage}: video {first / fps:.1f}-{end / fps:.1f} s, actions {acts}; wrote {clip.name}")


main()
