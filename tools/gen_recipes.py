#!/usr/bin/env python3
"""PRD 4 layer E: the forge's recipes. The shops keep their own buying screen, but every weapon, armour and accessory
they sell asks for Hunt Materials as well as Bits, and those of the hunting regions around the shop (the owner,
2026-11-17: the vendor's own region; commons in large amounts, a rare for the best gear). One item sold in two towns
can ask for different materials in each, so the table is by shop and item.

  tools/venv/bin/python tools/gen_recipes.py            # write port/game/definitive_recipes.c and the list for review
  tools/venv/bin/python tools/gen_recipes.py --check    # exit 1 if the committed table is not the rule's

Needs the user's disc (the species, through gen_hunt_targets): without it --check says so and passes. Reads the items
from src/main/records.c, the shops' goods and towns through gen_equipment, the regions, their encounters, their
commons and the compass's map graph through gen_hunt_targets, the Rare Targets from port/game/definitive_hunt_targets.c.
The numbers are definitive.h's (DEF_RECIPE_*) and LEVEL_AT / AMOUNT below. Rule, on the old buying price:
  - the shop's regions: those whose maps are up to DEF_RECIPE_REACH exits from one of the shop's maps, on the shop's
    server, at most DEF_RECIPE_OVER over the level the shop's dearest item is bought at (LEVEL_AT); none: the lowest of
    them (and the ones within DEF_RECIPE_OVER of it);
  - an item's common: of the commons those regions' random encounters leave, the one whose species' levels are nearest
    the level the item is bought at; a weapon takes the nearest, an armour or accessory the second nearest when there
    are two (the same shelf asks for both); AMOUNT of it by price;
  - from DEF_RECIPE_RARE_FROM a rare item too, of one of the shop's regions: the regions by their Rare Target's level,
    highest first, dealt to the shop's rare items dearest first; two from DEF_RECIPE_TWO_RARES.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
GEN = ROOT / "port/game/definitive_recipes.c"
OUT = ROOT / ".dumps/tasks/forge"
EQUIP = {"RECORDS_ITEM_WEAPON", "RECORDS_ITEM_ARMOR", "RECORDS_ITEM_ACCESSORY"}
ROMAN = ["", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"]


def constants():
    h = (ROOT / "port/game/definitive.h").read_text()
    c = {k: int(v, 0) for k, v in re.findall(r"#define (DEF_RECIPE_\w+)\s+(\w+)", h)}
    for k in ("DEF_RECIPE_REACH", "DEF_RECIPE_OVER", "DEF_RECIPE_RARE_FROM", "DEF_RECIPE_TWO_RARES"):
        assert k in c, k
    return c


def items():
    s = (ROOT / "src/main/records.c").read_text()
    body = s[s.index("RecordsItem records_items[] = {"):]
    body = body[:body.index("\n};")]
    return {int(m.group(6)): dict(price=int(m.group(2)), cat=m.group(4), name=m.group(7))
            for m in re.finditer(r"\{ (\S+), (\d+), (\d+), (\w+), (\d+), \{ 0, 0 \} \}, /\* (\d+) (.*?) \*/", body)}


def sold():
    s = (ROOT / "src/stitshop/stitshop_800859C0.c").read_text()
    out = set()
    for _n, body in re.findall(r"s16 stitshop_goods_(\d+)\[\] = \{(.*?)\};", s, re.S):
        out |= {int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", body) if int(x, 0)}
    return out


def rares():
    """[(level, index, battle, variant, slot)] of def_hunt_targets, in table order (the index is the rare item's)."""
    s = (ROOT / "port/game/definitive_hunt_targets.c").read_text()
    rows = re.findall(r"\{ 0x([0-9A-F]+), (\d+), (\d+), (\d+), (\d+) \}", s)
    return [(int(lv), i, int(b), int(v), int(k)) for i, (_r, b, k, lv, v) in enumerate(rows)]


# The level an item of this old price is bought at (where its shops open in the story), and how many of its common it
# asks for: (price, value) points, linear in the price's log between them.
LEVEL_AT = [(40, 3), (200, 6), (500, 10), (1000, 14), (2500, 21), (5000, 27), (9500, 37), (15000, 44), (22000, 46),
            (30000, 48), (60000, 50)]
AMOUNT = [(40, 2), (200, 3), (500, 5), (1000, 6), (2500, 8), (5000, 12), (9500, 15), (15000, 20), (22000, 25),
          (30000, 30), (60000, 40)]


def curve(points, price):
    import math
    if price <= points[0][0]:
        return points[0][1]
    for (p0, v0), (p1, v1) in zip(points, points[1:]):
        if price <= p1:
            t = (math.log(price) - math.log(p0)) / (math.log(p1) - math.log(p0))
            return v0 + (v1 - v0) * t
    return points[-1][1]


def amount(price):
    v = round(curve(AMOUNT, price))
    return v if v < 10 else (v + 2) // 5 * 5


def world():
    """The hunt's data: {region: (mean level, Counter of commons over its encounters' enemies)}, {map: region},
    {common: its species' mid level}, the map graph and the map areas."""
    from collections import Counter
    import gen_hunt_targets as gh
    battles = gh.battle_info()
    regions = gh.regions_of(battles)
    com, level, sp = gh.commons(battles)
    idx = {x["id"]: i for i, x in enumerate(sp)}
    info = {}
    for k, r in regions.items():
        c = Counter(com[idx[s]] for b in r["enc"] for s, _ in battles[b]["enemies"] if s in idx and com[idx[s]] != 0xFF)
        info[k] = (gh.mean_level(r, battles), c)
    mid = {}
    for m in range(16):
        lv = [level[i] for i, c in enumerate(com) if c == m]
        mid[m] = (min(lv) + max(lv)) / 2
    return info, {m: k for k, r in regions.items() for m in r["maps"]}, mid, gh.map_graph(), gh.map_areas()


def shop_regions(maps, info, map_region, graph, areas, top_level, c):
    from collections import deque
    server = areas[min(maps) & 0xFF] >> 7
    dist = {m: 0 for m in maps}
    q, found = deque(maps), {}
    while q:
        m = q.popleft()
        k = map_region.get(m)
        if k is not None and k >> 7 == server and k not in found:
            found[k] = dist[m]
        if dist[m] < c["DEF_RECIPE_REACH"]:
            for n in graph.get(m, []):
                if n not in dist:
                    dist[n] = dist[m] + 1
                    q.append(n)
    keep = [k for k in found if info[k][0] <= top_level + c["DEF_RECIPE_OVER"]]
    if not keep and found:
        low = min(info[k][0] for k in found)
        keep = [k for k in found if info[k][0] <= low + c["DEF_RECIPE_OVER"]]
    return sorted(keep, key=lambda k: (found[k], k))


def table():
    """[(shop, item, its row, common, n, rare, rare n)] sorted by shop and item."""
    import gen_equipment as ge
    c, its = constants(), items()
    info, map_region, mid, graph, areas = world()
    rare_level = {i: lv for lv, i, *_ in rares()}
    rare_region = {}
    for i, (r, _b, _k, _lv, _v) in enumerate(re.findall(r"\{ 0x([0-9A-F]+), (\d+), (\d+), (\d+), (\d+) \}",
                                                        (ROOT / "port/game/definitive_hunt_targets.c").read_text()
                                                        .split("def_super_rares")[0])):
        rare_region[int(r, 16)] = i
    lists, shops_of = ge.goods()
    towns = ge.shop_towns()
    out = []
    for n, ids in lists:
        for shop in shops_of[n]:
            eq = sorted({i for i in ids if its.get(i, {}).get("cat") in EQUIP}, key=lambda i: (its[i]["price"], i))
            if not eq or shop not in towns:
                continue
            top_level = curve(LEVEL_AT, its[eq[-1]]["price"])
            regs = shop_regions(sorted(towns[shop]), info, map_region, graph, areas, top_level, c)
            pool = sorted({m for k in regs for m in info[k][1]})
            assert pool, f"shop {shop}: no region around it leaves a common"
            dealt = sorted((k for k in regs if k in rare_region), key=lambda k: (-rare_level[rare_region[k]], k))
            dear = sorted((i for i in eq if its[i]["price"] >= c["DEF_RECIPE_RARE_FROM"]),
                          key=lambda i: (-its[i]["price"], i))
            rare_of = {i: rare_region[dealt[r % len(dealt)]] for r, i in enumerate(dear)} if dealt else {}
            for i in eq:
                lv = curve(LEVEL_AT, its[i]["price"])
                near = sorted(pool, key=lambda m: (abs(mid[m] - lv), -m))
                second = its[i]["cat"] != "RECORDS_ITEM_WEAPON" and len(near) > 1
                common = near[1 if second else 0]
                rare = rare_of.get(i, -1)
                rn = 0 if rare < 0 else 2 if its[i]["price"] >= c["DEF_RECIPE_TWO_RARES"] else 1
                out.append((shop, i, its[i], common, amount(its[i]["price"]), rare, rn))
    return sorted(out, key=lambda r: (r[0], r[1]))


def render(rows):
    lines = ["/* Generated by tools/gen_recipes.py (PRD 4 layer E): the forge's recipes, by shop and item ID: a common Hunt",
             " * Material of the shop's regions and how many, then a rare item (its index in def_hunt_targets, -1: none) and how",
             " * many. Regenerate instead of editing. */", '#include "definitive.h"', "", "const DefRecipe def_recipes[] = {"]
    for shop, i, it, com, n, rare, rn in rows:
        lines.append(f"    {{ {shop}, {com}, {i}, {n}, {rn}, {rare} }}, /* {it['name']} ({it['price']}) */")
    lines += ["};", f"const int def_recipe_count = {len(rows)};", ""]
    return "\n".join(lines)


def write_md(rows):
    import gen_hunt_targets as gh
    OUT.mkdir(parents=True, exist_ok=True)
    en = (ROOT / "tools/text_override/en.txt").read_text()
    mats = {int(i): n.strip() for i, n in re.findall(r"^\$HuntMat(\d+): (.*)$", en, re.M)}
    names = re.findall(r"/\* (?:.*?): (.*?) \*/", (ROOT / "port/game/definitive_hunt_targets.c").read_text()
                       .split("def_super_rares")[0])
    import gen_equipment as ge
    towns = ge.shop_towns()
    areas = gh.map_areas()
    c = constants()
    lines = ["# The forge's recipes (PRD 4 layer E), by shop", "",
             "Every piece of equipment a shop sells asks, besides its Bits, for a common material of the hunting regions "
             f"around the shop (up to {c['DEF_RECIPE_REACH']} exits away, on its server, at the shop's level): the one "
             "nearest the level the item is bought at; from "
             f"{c['DEF_RECIPE_RARE_FROM']} Bits (old price) a rare item of one of those regions too (two from "
             f"{c['DEF_RECIPE_TWO_RARES']}).", ""]
    shop = None
    for sh, i, it, com, n, rare, rn in rows:
        if sh != shop:
            shop = sh
            where = ", ".join(sorted({gh.area_name(areas[m & 0xFF]) for m in towns[sh]}))
            lines += ["", f"## Shop {sh} ({where}; maps {' '.join('%X' % m for m in sorted(towns[sh]))})", "",
                      "| ID | item | old price | common | rare |", "| --- | --- | --- | --- | --- |"]
        r = "—" if rare < 0 else f"{names[rare] if rare < len(names) else rare} x{rn}"
        lines.append(f"| {i} | {it['name']} | {it['price']} | {mats.get(com, com)} x{n} | {r} |")
    (OUT / "receitas.md").write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    try:
        rows = table()
    except FileNotFoundError as e:  # the species and their names are the disc's
        print(f"skip the recipes' rule needs the user's disc ({e.filename})")
        return 0
    text = render(rows)
    if a.check:
        ok = GEN.exists() and GEN.read_text() == text
        sane = all(0 <= com < 16 and 0 < n <= 99 and (rare == -1) == (rn == 0) for *_x, com, n, rare, rn in rows)
        print(f"{'ok' if ok else 'FAIL'} the recipes are the rule's ({len(rows)} rows); {'ok' if sane else 'FAIL'} "
              "materials and counts in range")
        return 0 if ok and sane else 1
    GEN.write_text(text)
    write_md(rows)
    print(f"{GEN.relative_to(ROOT)}: {len(rows)} recipes; {OUT / 'receitas.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
