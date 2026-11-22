#!/usr/bin/env python3
"""The Guide (port/game/definitive_quest.c): the rules compiled against a driver with stand-ins for the game's state
(the main quest row of every story step and its key-item variants, the missions, each auction's state, price and
travel rule, the notices once per change, nothing in a replay script). Then the tables checked against the game's own
data: the sixteen auctions against FIELDSTG's fieldstg_flag_events (its step, its "attended" flag 0x400 + N, its price
condition 0x7074 + the price's index), every main quest step 0..45 with a plain row after its variants, every flag a
main quest row asks for (a stage flag, or a key item's 0x8000 | item) one the stages really set (an event end's
set_flag or a talk's flags_set, src/wstag), and every text the tables name present in tools/text_override/en.txt within
its window's lines. Needs only the host gcc."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/port-test/definitive_quest"
Q_LINES, WHEN_LINES = 6, 2  # definitive_quest_screen.c: GD_Q_LINES for a quest's text, two for an auction's line


def stage_set_flags():
    """The flags the stages set to 1: their event ends' set_flag(F, 1) and their talks' flags_set lists. An item's
    flag (types 0x80..0x8E, the same item index for all eight) is read as 0x8000 | item, the Guide's ITEM(id)."""
    out = set()
    for path in sorted((ROOT / "src/wstag").glob("wstag*.c")):
        s = path.read_text()
        flags = [int(f, 16) for f in re.findall(r"set_flag\((0x[0-9A-Fa-f]+), 1\)", s)]
        lists = dict(re.findall(r"u16 (\w+)\[\d+\] = \{([^}]*)\}", s))
        for talks in re.findall(r"FieldstgTalk \w+\[\d+\] = \{(.*?)\};", s, re.S):
            for set_list in re.findall(r"\{ \w+, (\w+), \d+ \}", talks):
                v = [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", lists.get(set_list, ""))]
                flags += [f for f, value in zip(v[::2], v[1::2]) if value == 1]
        out |= {0x8000 | (f & 0x1FF) if 0x80 <= (f >> 8) & ~1 <= 0x8E else f for f in flags}
    return out


def tables_ok():
    src = (ROOT / "port/game/definitive_quest.c").read_text()
    fs = (ROOT / "src/fieldstg/fieldstg_80083D08.c").read_text()
    en = {k: v for k, v in re.findall(r"^\$(\w+): (.*)$", (ROOT / "tools/text_override/en.txt").read_text(), re.M)}
    ok = True
    events = [tuple(int(x) for x in m) for m in re.findall(r"\{ (-?\d+), (\d+), (\d+), (\d+), (\d+) \}",
                                                             fs[fs.index("fieldstg_flag_events["):])][:16]
    auctions = [(int(p), int(i), int(m), w) for p, i, m, w in
                re.findall(r"\{ (\d+), (\d+), (\d+), def_str_(AucWhen\d+) \}", src)]
    a_ok = len(auctions) == 16 == len(events)
    for n, ((progress, _item, money, _when), ev) in enumerate(zip(auctions, events)):
        a_ok &= ev[0] == progress and ev[1] == 0x400 + n and ev[2] == 0x7074 + money
    print(f"{'ok' if a_ok else 'FAIL'} the auctions are FIELDSTG's: step, attended flag and price, in order")
    ok &= a_ok
    flag = r"(ANY|ITEM\(\d+\)|STORY\(\w+\)|0x[0-9A-F]+)"
    rows = re.findall(r"\{ (\d+), (\d+), " + flag + r", (\d), (0x[0-9A-F]+|0), def_str_(\w+), def_str_(\w+) \}", src)
    body = src[src.index("const DefQuest def_quests[]"):src.index("const int def_quest_count")]
    main = [r for r in rows if f"def_str_{r[5]}," in body]
    q_ok = bool(main)
    for p in range(46):
        covering = [r for r in main if int(r[0]) <= p <= int(r[1])]
        plain = [i for i, r in enumerate(covering) if r[2] == "ANY"]
        q_ok &= bool(plain) and plain[0] == len(covering) - 1  # the variants first, then one plain row last
    print(f"{'ok' if q_ok else 'FAIL'} every story step 0..45 has a plain main quest row after its variants")
    ok &= q_ok
    asked = {r[2]: int(r[2], 16) if r[2].startswith("0x") else 0x8000 | int(r[2][5:-1]) for r in main
             if r[2].startswith(("0x", "ITEM("))}
    unset = sorted(name for name, f in asked.items() if f not in stage_set_flags())
    print(f"{'ok' if not unset else 'FAIL'} every flag a main quest row asks for is one a stage sets"
          + (f": {unset}" if unset else ""))
    ok &= not unset
    names = set(re.findall(r"def_str_(\w+)", src))
    missing = sorted(n for n in names if n not in en)
    print(f"{'ok' if not missing else 'FAIL'} every text the tables name is in en.txt" + (f": {missing}" if missing else ""))
    ok &= not missing
    long_ = [n for n in names if n in en and n.endswith("T") and en[n].count("<1>") + 1 > Q_LINES]
    long_ += [n for n in names if n.startswith("AucWhen") and n in en and en[n].count("<1>") + 1 > WHEN_LINES]
    print(f"{'ok' if not long_ else 'FAIL'} the texts fit their windows' lines" + (f": {sorted(long_)}" if long_ else ""))
    ok &= not long_
    return ok


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    exe = OUT / "definitive_quest_driver"
    cc = ["gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include"), "-I", str(ROOT / "tests/host"),
          "-I", str(ROOT / "psxstack/include"), "-I", str(ROOT / "port/game"),
          str(ROOT / "port/game/definitive_quest.c"), str(ROOT / "port/game/definitive_text.c"),
          str(ROOT / "tests/port/definitive_quest_driver.c"), "-o", str(exe)]
    if subprocess.run(cc).returncode != 0:
        print("definitive_quest: FAIL: does not compile")
        return 1
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    print(run.stdout, end="")
    bad = run.returncode != 0
    bad |= not tables_ok()
    print("definitive_quest: " + ("FAIL" if bad else "pass"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
