#!/usr/bin/env python3
"""The expansion save (port/game/definitive_expansion.h): its core with the real sections (the personas) and the
bestiary's first record, against a real file in a scratch directory: the JSON round trip, the generation pairing (a
mismatch reads as empty), the card's old persona bits migrated, unknown keys ignored and kept, the atomic write (a
temporary that fails leaves the old file), a newer version never overwritten, a file not JSON moved aside, the
savestate, the path beside the card. Needs only the host gcc, no disc."""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/port-test/definitive_expansion"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    work = OUT / "work"
    if work.exists():
        shutil.rmtree(work)  # the test's own scratch, rebuilt every run
    work.mkdir()
    exe = OUT / "definitive_expansion_driver"
    game = ROOT / "port/game"
    cc = ["gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror",
          "-I", str(ROOT / "include"), "-I", str(ROOT / "psxstack/include"), "-I", str(game),
          str(game / "definitive_expansion.c"), str(game / "definitive_expansion_sections.c"),
          str(game / "definitive_persona_save.c"), str(game / "definitive_respec_save.c"),
          str(game / "definitive_bestiary.c"),
          str(ROOT / "port/third_party/cjson/cJSON.c"), str(ROOT / "tests/port/definitive_super_stub.c"),
          str(ROOT / "tests/port/definitive_expansion_driver.c"), "-o", str(exe)]
    if subprocess.run(cc).returncode != 0:
        print("definitive_expansion: FAIL: does not compile")
        return 1
    run = subprocess.run([str(exe), str(work)], capture_output=True, text=True)
    print(run.stdout, end="")
    bad = run.returncode != 0 or any(line.startswith("FAIL") for line in run.stdout.splitlines())
    print("definitive_expansion: " + ("FAIL" if bad else "pass"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
