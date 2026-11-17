#!/usr/bin/env python3
"""The hero's skins (.plan/backlog/skins-npc.md): port/game/definitive_skins.c compiled alone against a driver with stand-ins
for the game's state: a hunt won unlocks a skin, the skin worn (locked or a replay script: none), the hero's CLUT (hair
and clothes changed, the skin's colours, transparent and the STP bit kept) and the record. Needs only the
host gcc."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/port-test/definitive_skins"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    exe = OUT / "definitive_skins_driver"
    cc = ["gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include"), "-I", str(ROOT / "tests/host"),
          "-I", str(ROOT / "psxstack/include"), "-I", str(ROOT / "port/game"),
          str(ROOT / "port/game/definitive_skins.c"), str(ROOT / "tests/port/definitive_skins_driver.c"),
          "-o", str(exe), "-lm"]
    if subprocess.run(cc).returncode != 0:
        print("definitive_skins: FAIL: does not compile")
        return 1
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    print(run.stdout, end="")
    bad = run.returncode != 0
    print("definitive_skins: " + ("FAIL" if bad else "pass"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
