#!/usr/bin/env python3
"""The enemy personas (port/game/definitive_persona.h): checks the hand-written table's rows and prints it for review,
then drives the engine through each rule with its own case rows. Needs only the host gcc, no disc."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/port-test/definitive_persona"


def build(name, extra):
    exe = OUT / name
    cc = [
        "gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-DPC_PORT",
        "-I", str(ROOT / "include"),
        "-I", str(ROOT / "tests/host"),
        "-I", str(ROOT / "psxstack/include"),
        "-I", str(ROOT / "port/game"),
        str(ROOT / "port/game/definitive_persona.c"),
        str(ROOT / "port/game/definitive_hunt_targets.c"),
        str(ROOT / "tests/port/definitive_persona_driver.c"),
        *extra,
        "-o", str(exe),
    ]
    res = subprocess.run(cc, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"definitive_persona: FAIL: {name} does not compile\n" + res.stderr)
        return None
    return exe


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    failed = False
    for name, extra in (("table", [str(ROOT / "port/game/definitive_persona_table.c")]),
                        ("cases", ["-DPERSONA_CASES"])):
        exe = build(name, extra)
        if exe is None:
            failed = True
            continue
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        print(run.stdout, end="")
        failed |= run.returncode != 0 or any(l.startswith("FAIL") for l in run.stdout.splitlines())
    print("definitive_persona: " + ("FAIL" if failed else "pass"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
