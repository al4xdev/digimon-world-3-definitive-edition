#!/usr/bin/env python3
"""Definitive progression (digivolution floors): every form of rows 9..52 has the Digimon level its stage needs
(Perfect 20, Mega 40, Adult none), from def_form_floor in port/game/definitive_progression.c."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/port-test/definitive_form_floor"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    exe = OUT / "definitive_form_floor"
    cc = [
        "gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror",
        "-I", str(ROOT / "port/game"),
        str(ROOT / "port/game/definitive_progression.c"),
        str(ROOT / "tests/port/definitive_form_floor_driver.c"),
        "-o", str(exe),
    ]
    res = subprocess.run(cc, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"definitive_form_floor: FAIL to compile:\n{res.stderr}")
        return 1
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    print(run.stdout, end="")
    if run.returncode != 0:
        print("definitive_form_floor: FAIL")
        return 1
    print("definitive_form_floor: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
