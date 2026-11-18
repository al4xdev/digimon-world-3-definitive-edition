#!/usr/bin/env python3
"""The Super Rare hunts: port/game/definitive_superhunt.c compiled alone against a
driver with stand-ins for the game's state, a small world and the hunts' table: the map graph's distances, the hunts'
order (open, locked, done, repeated, cancelled), the territory, the compass, the point, the battle (one enemy, the HP
it keeps, the buffs, the flight at DEF_SUPER_FLEE_PCT), the crystal, the record and a replay script. Also that the
generated tables are consistent: twelve Super Rares in their order of level, existing battles and slots, Galacticmon
never, a territory with maps where it can sit for each; the map graph's edges within its nodes. Needs only the host gcc."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/port-test/definitive_superhunt"
sys.path.insert(0, str(ROOT / "tools"))


def tables_ok():
    gen = (ROOT / "port/game/definitive_hunt_targets.c").read_text()
    body = re.search(r"def_super_rares\[DEF_SUPER_COUNT\] = \{(.*?)\};", gen, re.S).group(1)
    sup9 = [tuple(int(x) for x in m) for m in
            re.findall(r"\{ (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (-?\d+), (\d+), (\d+) \}", body)]
    sup = [x[:6] for x in sup9]
    src = (ROOT / "src/fieldstg/fieldstg_80083784.c").read_text()
    nb = int(re.search(r"FieldstgBattle fieldstg_battles\[(\d+)\]", src).group(1))
    sec = [int(x) for x in re.findall(r"-?\d+", re.sub(r"/\*.*?\*/", "",
                                                       re.search(r"def_hunt_sector_of\[DEF_HUNT_AREAS\] = \{(.*?)\};",
                                                                 gen, re.S).group(1)))]
    graph = (ROOT / "port/game/definitive_map_graph.c").read_text()

    def arr(name):
        b = re.search(name + r"\[\d+\] = \{(.*?)\};", graph, re.S).group(1)
        return [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", b)]

    nodes, first, edges, wild = arr("def_map_nodes"), arr("def_map_first"), arr("def_map_edges"), arr("def_map_wild")
    import gen_hunt_targets as gh
    areas = gh.map_areas()
    battles = gh.battle_info()
    ok = (len(sup) == 13 and [s[2] for s in sup] == sorted(s[2] for s in sup) and len(sec) == 0x2E
          and all(0 < b < nb and k < 3 and 0 < lv <= 99 and 0 < hp <= 30000 and 0 <= se < 5 and sv < 2
                  for b, k, lv, hp, se, sv in sup)
          and len({battles[b]["slots"][[x[0] for x in battles[b]["slots"]].index(k)][1] for b, k, *_ in sup}) == 13
          and all(battles[b]["slots"][[x[0] for x in battles[b]["slots"]].index(k)][1] == x[7]
                  for (b, k, *_), x in zip(sup, sup9))
          and all(0 < x[8] <= 30000 for x in sup9)
          and sup9[0][2] == 3 and sup9[0][6] >= 0 and all(x[6] == -1 for x in sup9[1:])
          and not any(battles[b]["slots"][[x[0] for x in battles[b]["slots"]].index(k)][1] in {442, 466, 467}
                      for b, k, *_ in sup))
    terr = {(sec[areas[m & 0xFF] & 0x7F], areas[m & 0xFF] >> 7) for i, m in enumerate(nodes)
            if wild[i] and 0x200 <= m < 0x300 and areas[m & 0xFF] != 0xFF and (areas[m & 0xFF] & 0x7F) < len(sec)}
    tut_maps = [m for i, m in enumerate(nodes) if wild[i] and 0x200 <= m < 0x300 and areas[m & 0xFF] != 0xFF
                and areas[m & 0xFF] & 0x7F == sup9[0][6] and areas[m & 0xFF] >> 7 == sup9[0][5]]
    ok_t = all((se, sv) in terr for _, _, _, _, se, sv in sup) and len(tut_maps) > 0
    ok_g = (nodes == sorted(nodes) and len(first) == len(nodes) + 1 and first[-1] == len(edges)
            and all(0 <= e < len(nodes) for e in edges) and len(wild) == len(nodes))
    print(f"{'ok' if ok else 'FAIL'} the hunts: the level 3 tutorial in one area, then 12 Super Rares in their order "
          f"of level; 13 species, existing battles and slots, their species and prices; no Galacticmon")
    print(f"{'ok' if ok_t else 'FAIL'} each hunt's ground has maps with random encounters (the tutorial's: "
          f"{', '.join(hex(m) for m in tut_maps)})")
    print(f"{'ok' if ok_g else 'FAIL'} the map graph: {len(nodes)} maps sorted, {len(edges)} exits within them")
    return ok and ok_t and ok_g


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    exe = OUT / "definitive_superhunt_driver"
    hunt_flee = OUT / "hunt_flee.c"
    # def_hunt_flee lives in definitive_hunt.c; the driver checks the Super Rare's part of it through a copy of that one
    # function, so this test needs nothing else of the regional hunt.
    src = (ROOT / "port/game/definitive_hunt.c").read_text()
    body = re.search(r"int def_hunt_flee\(void\) \{.*?\n\}", src, re.S).group(0)
    hunt_flee.write_text('#include "common.h"\n#include "pad.h"\n#include "definitive.h"\n'
                         "static struct { signed char rare, hard; } hunt;\n" + body + "\n")
    cc = ["gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include"), "-I", str(ROOT / "tests/host"),
          "-I", str(ROOT / "psxstack/include"), "-I", str(ROOT / "port/game"),
          str(ROOT / "port/game/definitive_superhunt.c"), str(hunt_flee),
          str(ROOT / "tests/port/definitive_superhunt_driver.c"), "-o", str(exe)]
    if subprocess.run(cc).returncode != 0:
        print("definitive_superhunt: FAIL: does not compile")
        return 1
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    print(run.stdout, end="")
    bad = run.returncode != 0
    bad |= not tables_ok()
    print("definitive_superhunt: " + ("FAIL" if bad else "pass"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
