#!/usr/bin/env python3
"""Definitive combat 3-tier fixed stat buff/debuff system, turn-based expiration, Layer H technique patches, and
Overlevel Rush gap = 8: compiles port/game/definitive.c with stand-in game structs and drives every case.
Needs only the host gcc, no disc."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/port-test/definitive_buffs"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Minimal psxstack_game_gen.h stub so definitive.c compiles standalone without a CMake configure dir.
    (OUT / "psxstack_game_gen.h").write_text(
        "/* standalone test stub */\n"
        "#define PORT_ARENA_SIZE 16\n"
    )
    exe = OUT / "definitive_buffs_driver"
    cc = [
        "gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-DPC_PORT",
        "-I", str(OUT),
        "-I", str(ROOT / "include"),
        "-I", str(ROOT / "tests/host"),
        "-I", str(ROOT / "psxstack/include"),
        "-I", str(ROOT / "psxstack/include/psxstack"),
        "-I", str(ROOT / "port/game"),
        "-I", str(ROOT),
        str(ROOT / "port/game/definitive.c"), str(ROOT / "tests/port/definitive_super_stub.c"),
        str(ROOT / "port/game/definitive_mask.c"),
        str(ROOT / "port/game/definitive_progression.c"),
        str(ROOT / "port/game/definitive_mp.c"),
        str(ROOT / "port/game/definitive_text.c"),
        str(ROOT / "tests/port/definitive_buffs_driver.c"),
        "-o", str(exe),
    ]
    res = subprocess.run(cc, capture_output=True, text=True)
    if res.returncode != 0:
        print("definitive_buffs: FAIL: does not compile\n" + res.stderr)
        return 1
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    bad = [l for l in run.stdout.splitlines() if l.startswith("FAIL")]
    print(run.stdout, end="")
    print("definitive_buffs: " + ("FAIL" if bad or run.returncode else "pass"))
    return 1 if bad or run.returncode else 0


if __name__ == "__main__":
    sys.exit(main())
