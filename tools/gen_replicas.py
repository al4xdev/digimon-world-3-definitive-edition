#!/usr/bin/env python3
"""PRD 4 layer G: the Black Market's Legendary Replicas, the equipment the player can only find, win or get from an
event (so can miss), sold again as replicas for rare items and Bits.

  tools/venv/bin/python tools/gen_replicas.py            # write port/game/definitive_replicas.c and the list for review
  tools/venv/bin/python tools/gen_replicas.py --check    # exit 1 if the committed table is not the rule's

Reads the items from src/main/records.c (records_items: category, selling price, the English name in the comment), the
shops' goods from src/stitshop/stitshop_800859C0.c (stitshop_goods_N) and the game's own item flags from src/ (no
disc). The numbers are definitive.h's (DEF_REPLICAS, DEF_BM_REPLICA_BITS). Rule:
  - a weapon, armour or accessory that no shop sells and that has a selling price (the ones worth 0 are the quest
    weapons' stages, Rusty Katana to Muramasa, which the blacksmith events trade);
  - that no event names: an item flag (type 0x80..0x8E, gamestate_check_item / gamestate_change_item) in a stage's
    flag list or a set_flag/get_flag call, or an item gamestate_cond_items tests (an event that checks or takes an item
    must not be fed by the Black Market);
  - the DEF_REPLICAS with the highest selling price, ties by item ID;
  - a replica costs DEF_BM_REPLICA_BITS times that selling price in Bits, rounded to 1000, and one each of its own two
    Rare Targets' rare items (CORES below, picked by hand by the item's theme and strength, the owner's choice of
    2026-11-17: an element's Power S takes two late bosses of that element, the cheaper replicas earlier ones; no Rare
    Target serves two replicas), read from port/game/definitive_hunt_targets.c by the target's species name.
The row's index is the replica's bit in the card record (REC_EXTRA + 26..27): once saves carry it, a change of the list
must keep the rows already shipped where they are.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "port/game/definitive_replicas.c"
OUT = ROOT / ".dumps/tasks/market"
EQUIP = {"RECORDS_ITEM_WEAPON", "RECORDS_ITEM_ARMOR", "RECORDS_ITEM_ACCESSORY"}


def constants():
    h = (ROOT / "port/game/definitive.h").read_text()
    c = {k: int(v, 0) for k, v in re.findall(r"#define (DEF_REPLICAS|DEF_BM_\w+)\s+(\w+)", h)}
    for k in ("DEF_REPLICAS", "DEF_BM_REPLICA_BITS"):
        assert k in c, k
    return c


def items():
    s = (ROOT / "src/main/records.c").read_text()
    body = s[s.index("RecordsItem records_items[] = {"):]
    body = body[:body.index("\n};")]
    return {int(m.group(6)): dict(price=int(m.group(2)), sell=int(m.group(3)), cat=m.group(4), name=m.group(7))
            for m in re.finditer(r"\{ (\S+), (\d+), (\d+), (\w+), (\d+), \{ 0, 0 \} \}, /\* (\d+) (.*?) \*/", body)}


def sold():
    s = (ROOT / "src/stitshop/stitshop_800859C0.c").read_text()
    out = set()
    for _n, body in re.findall(r"s16 stitshop_goods_(\d+)\[\] = \{(.*?)\};", s, re.S):
        out |= {int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", body) if int(x, 0)}
    return out


def item_of(flag):
    """The item a flag word names (gamestate_get_flag / gamestate_set_flag: type (flag >> 8) & ~1, index flag & 0x1FF),
    or None."""
    return flag & 0x1FF if 0x80 <= ((flag >> 8) & ~1) <= 0x8E else None


def event_items():
    """{item: [where]} of the items the game's events name."""
    found = {}
    for f in sorted((ROOT / "src").rglob("*.c")):
        s = f.read_text(errors="replace")
        words = []
        for m in re.finditer(r"u16 \w+\[\d*\] = \{([^}]*)\};", s):  # flag lists: (flag, value) pairs to 0xFFFF
            vals = [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", m.group(1))]
            for i in range(0, len(vals) - 1, 2):
                if vals[i] == 0xFFFF:
                    break
                words.append(vals[i])
        words += [int(x, 0) for x in re.findall(r"(?:set_flag|get_flag)\((0x[0-9A-Fa-f]+|\d+)", s)]
        for w in words:
            it = item_of(w)
            if it:
                found.setdefault(it, []).append(f.name)
    g = (ROOT / "src/main/gamestate.c").read_text()
    body = g[g.index("s32 gamestate_cond_items("):]
    body = body[:body.index("\n}\n")]
    for x in re.findall(r"gamestate_data\.items\[(\d+)\]", body):
        found.setdefault(int(x), []).append("gamestate_cond_items")
    return found


# Each replica's two Rare Targets, by species name (the item's theme, then its strength).
CORES = {
    "Fire Power S": ("Deltamon", "BK WarGrowlmon"),        # Fire, L46 and L48
    "Water Power S": ("MegaSeadramon", "Scorpiomon"),      # Water, L40 and L42
    "Ice Power S": ("KingEtemon", "Tapirmon"),             # Ice, L46 and L52
    "Wind Power S": ("Airdramon", "Fujinmon"),             # Wind, L27 and L51 (the only late Wind boss)
    "Bolt Power S": ("Snimon", "Raijinmon"),               # Thunder, L48 and L52
    "Metal Power S": ("BK MegaGargomon", "MetalGreymon"),  # Machine, L46 and L53
    "Dark Power S": ("Phantomon", "Armageddemon"),         # Dark, L32 and L50 (the only two)
    "Machinegun": ("Megadramon", "Puppetmon"),             # Machine, L41 and L50
    "Magician Rod": ("Persiamon", "Blossomon"),            # the mystics, L38 and L42
    "Ghost Fang": ("Raremon", "Vilemon"),                  # the undead and the demon, L44 and L50
    "Pulse Guard": ("Yanmamon", "Maildramon"),             # Thunder, L35 and L50
    "Drill Horn": ("BK Imperialdramon", "Triceramon"),     # a machine and a horn, L39 and L41
    "Chain Saw": ("Okuwamon", "MetalSeadramon"),           # blades and a machine, L26 and L40
    "Naginata": ("Vikemon", "Antylamon"),                  # Ice warriors, L43 and L46
    "Recover Crest": ("MagnaAngemon", "Cherrymon"),        # holy and healing, L26 and L41
    "MP Mega Proxy": ("Seadramon", "Gekomon"),             # Water, L40 and L42
}


def targets():
    """{species name: its index in def_hunt_targets} (each Rare Target's species is its own)."""
    s = (ROOT / "port/game/definitive_hunt_targets.c").read_text().split("def_super_rares")[0]
    names = re.findall(r"\{ 0x[0-9A-F]+, \d+, \d+, \d+, \d+ \}, /\* (.*?): ", s)
    return {n: i for i, n in enumerate(names)}


def r1000(v):
    return (v + 500) // 1000 * 1000


def table():
    c, its, shop, events = constants(), items(), sold(), event_items()
    cands = [i for i, it in its.items() if it["cat"] in EQUIP and i not in shop and it["sell"] > 0 and i not in events]
    cands.sort(key=lambda i: (-its[i]["sell"], i))
    t = targets()
    rows = [(i, its[i], r1000(its[i]["sell"] * c["DEF_BM_REPLICA_BITS"]), tuple(t[n] for n in CORES[its[i]["name"]]))
            for i in cands[:c["DEF_REPLICAS"]]]
    return rows, its, shop, events, c


def render(rows, c):
    lines = ["/* Generated by tools/gen_replicas.py (PRD 4 layer G): the Black Market's Legendary Replicas, equipment no",
             " * shop sells, with the two Rare Targets' rare items each takes and their Bits. A row's index is its bit in the card record (REC_EXTRA + 26..27): rows already",
             " * shipped keep their place. Regenerate instead of editing. */",
             '#include "definitive.h"', "", "const DefReplica def_replicas[DEF_REPLICAS] = {"]
    names = {v: k for k, v in targets().items()}
    for i, it, bits, cores in rows:
        lines.append(f"    {{ {i}, {{ {', '.join(map(str, cores))} }}, {bits} }}, /* {it['name']}: sells for {it['sell']}; "
                     f"{', '.join(names[x] for x in cores)} */")
    lines += ["};", f"const int def_replica_count = {len(rows)};", ""]
    return "\n".join(lines)


def write_md(rows, its, shop, events, c):
    OUT.mkdir(parents=True, exist_ok=True)
    names = {v: k for k, v in targets().items()}
    lines = ["# Legendary Replicas (PRD 4 layer G)", "",
             "Rule: a weapon, armour or accessory no shop sells, with a selling price, named by no event; the "
             f"{c['DEF_REPLICAS']} with the highest selling price (ties by ID). Price: {c['DEF_BM_REPLICA_BITS']}x the "
             "selling price, rounded to 1000, plus one each of its own two Rare Targets' Cores (by theme and strength).",
             "", "| bit | ID | item | sells | Bits | Cores |", "| --- | --- | --- | --- | --- | --- |"]
    for k, (i, it, bits, cores) in enumerate(rows):
        lines.append(f"| {k} | {i} | {it['name']} | {it['sell']} | {bits} | {' + '.join(names[x] for x in cores)} |")
    rest = sorted((i for i, it in its.items() if it["cat"] in EQUIP and i not in shop and i not in [r[0] for r in rows]),
                  key=lambda i: (-its[i]["sell"], i))
    lines += ["", "Left out (no shop sells them):", ""]
    for i in rest:
        why = "named by an event: " + ", ".join(sorted(set(events[i]))) if i in events else (
            "sells for 0" if its[i]["sell"] == 0 else "below the cut")
        lines.append(f"- {i} {its[i]['name']} (sells {its[i]['sell']}): {why}")
    (OUT / "replicas.md").write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    rows, its, shop, events, c = table()
    text = render(rows, c)
    if a.check:
        ok = GEN.exists() and GEN.read_text() == text
        full = len(rows) == c["DEF_REPLICAS"] and len({r[0] for r in rows}) == len(rows)
        safe = all(i not in shop and i not in events and its[i]["cat"] in EQUIP for i, *_ in rows)
        cores = [x for *_, cs in rows for x in cs]
        own = len(cores) == len(set(cores)) == c["DEF_REPLICAS"] * c["DEF_BM_REPLICA_RARES"]
        print(f"{'ok' if ok else 'FAIL'} the replicas are the rule's ({len(rows)} items); {'ok' if full else 'FAIL'} "
              f"{c['DEF_REPLICAS']} distinct; {'ok' if safe else 'FAIL'} none sold in a shop or named by an event; "
              f"{'ok' if own else 'FAIL'} each its own Cores, none shared")
        return 0 if ok and full and safe and own else 1
    GEN.write_text(text)
    write_md(rows, its, shop, events, c)
    print(f"{GEN.relative_to(ROOT)}: {len(rows)} replicas; {OUT / 'replicas.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
