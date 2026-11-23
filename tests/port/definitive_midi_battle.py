#!/usr/bin/env python3
"""The player's MIDI in a real random battle (port/game/definitive_midi.c, the encounter hook in src/fieldstg, bank 74 in
src/main/sound.c), on the port alone through the debug channel.

A card is made first: tests/replay/scripts/first_battle_save.json up to its `saved` checkpoint (the party in the Asuka
lobby, saved). Then, per case, the port loads that card (Continue), DW3_TIME_TEST puts the player on map 0x233 (an
encounter area), the encounter timer is forced to 1 and the pad held until the battle stage. Three cases, each with its
own folder (DW3_MIDI_DIR) and --wav:
  order / shuffle   "All, in order" plays the first file (a.mid) and writes `last:` down; "All, shuffled" plays one of two;
  chosen   battle_music.txt names a file: the port logs "midi: <file>, N notes", the sound module is playing bank 74's key
           (sound_module.current) and the audio is not silent;
  default  nothing chosen: no midi line, the stage's own key plays;
  gone     a file chosen that is not in the folder: the game's music, no midi line, no crash.
Needs build/port/dw2003 (tests/port/run.py builds it) and the disc; says so and exits 2 without them.
Exit codes: 0 pass, 1 fail, 2 something missing."""
import json
import os
import shutil
import struct
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests/port"))
import mcp_game as M  # noqa: E402

OUT = ROOT / "build/port-test/definitive_midi_battle"
KEY = 0x60000000 | 74 << 18
MAP = 0x233
GAMESTATE_TIMER = 0x80048D34 + 0x30  # gamestate_data.encounter_timer


def make_card():
    """The card with a save in it: first_battle_save's steps up to `saved`, run headless."""
    OUT.mkdir(parents=True, exist_ok=True)
    card1, card2 = OUT / "card1.mcd", OUT / "card2.mcd"
    if card1.exists() and card1.stat().st_size == 131072:
        return card1, card2
    src = json.loads((ROOT / "tests/replay/scripts/first_battle_save.json").read_text())
    steps = []
    for s in src["steps"]:
        steps.append(s)
        if s.get("type") == "checkpoint" and s.get("name") == "saved":
            break
    src["steps"] = steps
    (OUT / "saved.script.json").write_text(json.dumps(src))
    run = subprocess.run([str(M.BINARY), "--disc", str(M.DISC), "--script", str(OUT / "saved.script.json"), "--memcard1",
                          str(card1), "--memcard2", str(card2)], cwd=OUT, capture_output=True, text=True, timeout=300)
    if run.returncode != 0:
        raise SystemExit(f"definitive_midi_battle: the save script failed ({run.returncode}): {run.stderr[-300:]}")
    return card1, card2


def case(name, choice, files):
    import definitive_midi as t  # the converter test's MIDI writer
    work = OUT / name
    if work.exists():
        shutil.move(str(work), f"/tmp/definitive_midi_battle_{name}_{os.getpid()}")
    folder = work / "midi"
    folder.mkdir(parents=True)
    for fname in files:
        (folder / fname).write_bytes(t.song(60, 3))
    (folder / "battle_music.txt").write_text(choice)
    card1, card2 = make_card()
    shutil.copy(card1, work / "c1.mcd")
    shutil.copy(card2, work / "c2.mcd")
    wav = work / "battle.wav"
    env = {"PATH": "/usr/bin:/bin", "DW3_DEFINITIVE": "1", "DW3_PORT_SKIP_DIALOGUES": "1", "DW3_MIDI_DIR": str(folder), "DW3_MIDI_SEED": "5",
           "DW3_TIME_TEST": f"{MAP:#x}:200:200:0x10"}
    g = M.Game.spawn([str(M.BINARY), "--disc", str(M.DISC), "--cd-speed", "instant", "--max-frames", "0", "--watchdog", "0",
                      "--memcard1", str(work / "c1.mcd"), "--memcard2", str(work / "c2.mcd"), "--wav", str(wav)],
                     env=env, hold=True, cwd=str(work))
    result = {"battle": False, "key": None, "log": ""}
    try:
        mp = lambda: g.status()["map"]
        for _ in range(80):
            if mp() == 0xE02:
                break
            g.pad(["START"], 2, 18, sync=True)
        for _ in range(60):
            if mp() == 0xE00:
                break
            g.pad(["START"], 30, 30, sync=True)
        g.step(60)
        for _ in range(40):
            g.pad(["START"], 2, 18, sync=True)
        g.step(60)
        g.pad(["DOWN"], 4, 20, sync=True)  # Continue
        for _ in range(60):
            if mp() == MAP:
                break
            g.pad(["CROSS"], 4, 20, sync=True)
            g.step(100)
        g.step(200)
        for i in range(500):
            g.poke_ps1(GAMESTATE_TIMER, struct.pack("<i", 1))  # the next step in an encounter area starts one
            g.pad([("LEFT", "DOWN", "RIGHT", "UP")[i // 3 % 4]], 12, 2, sync=True)
            if g.status()["stage"] == 6:
                result["battle"] = True
                break
        g.step(300)
        sm = M.symbols().host("sound_module")
        mem = g.peek(sm.addr, sm.size)
        # sound_module.current: the word after the three entries; found by value (the host's layout has pointers in it)
        result["key"] = KEY if struct.pack("<I", KEY) in mem[0x4200:0x4300] else None
        result["log"] = g.log_text(4000)
        g.step(120)
    finally:
        g.stop()
    result["midi_lines"] = [ln for ln in result["log"].splitlines() if "port: midi:" in ln and "notes" in ln]
    peak = 0
    if wav.exists() and wav.stat().st_size > 44:
        with wave.open(str(wav)) as w:
            n = w.getnframes()
            w.setpos(max(0, n - 44100 * 3))
            data = w.readframes(44100 * 3)
            peak = max((abs(v) for v in struct.unpack(f"<{len(data) // 2}h", data)), default=0)
    result["peak"] = peak
    result["crash"] = sorted(work.glob("crash-*.txt"))
    return result


def main():
    if not M.BINARY.is_file() or not M.DISC.is_file():
        print("definitive_midi_battle: no build/port/dw2003 or no disc: skipped")
        return 2
    bad = False
    for name, choice, files, want_midi in (("chosen", "battle.mid\n", ["battle.mid"], True),
                                           ("order", "mode: order\n", ["a.mid", "b.mid"], True),
                                           ("shuffle", "mode: shuffle\n", ["a.mid", "b.mid"], True),
                                           ("default", "", ["battle.mid"], False),
                                           ("gone", "missing.mid\n", ["battle.mid"], False)):
        r = case(name, choice, files)
        ok = r["battle"] and not r["crash"] and (r["key"] == KEY) == want_midi and bool(r["midi_lines"]) == want_midi
        if want_midi:
            first = {"order": "a.mid", "shuffle": ".mid"}.get(name, "battle.mid")
            ok = ok and first in r["midi_lines"][0] and r["peak"] > 300
            if name in ("order", "shuffle"):  # the position is written down for the next battle and the next session
                ok = ok and "last: " in (OUT / name / "midi" / "battle_music.txt").read_text()
        print(f"{'ok' if ok else 'FAIL'} {name}: battle {r['battle']}, bank 74's key playing {r['key'] == KEY}, midi lines "
              f"{len(r['midi_lines'])}, audio peak {r['peak']}, crash {[c.name for c in r['crash']]}")
        bad |= not ok
    print("definitive_midi_battle: " + ("FAIL" if bad else "pass"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
