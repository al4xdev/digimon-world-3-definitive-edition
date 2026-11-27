#!/usr/bin/env python3
"""The forge's recipes. The shops keep their own buying screen, but every weapon, armour and accessory
they sell asks for Hunt Materials as well as Bits, and those of the hunting regions around the shop (the vendor's own region; commons in large amounts, a rare for the best gear). One item sold in two towns
can ask for different materials in each, so the table is by shop and item.

  tools/venv/bin/python tools/gen_recipes.py            # write port/game/definitive_recipes.c and the list for review
  tools/venv/bin/python tools/gen_recipes.py --check    # exit 1 if the committed table is not the rule's

Needs the user's disc (the species, through game_tables): without it --check says so and passes. Reads the items
from src/main/records.c, the shops' goods and towns through gen_equipment, the regions, their encounters, their
commons and the compass's map graph through game_tables, the Rare Targets from port/game/definitive_hunt_targets.c.
The numbers are definitive.h's (DEF_RECIPE_*) and LEVEL_AT / AMOUNT below. Rule, on the old buying price:
  - the shop's regions: those whose maps are up to DEF_RECIPE_REACH exits from one of the shop's maps, on the shop's
    server, at most DEF_RECIPE_OVER over the level the shop's dearest item is bought at (LEVEL_AT); none: the lowest of
    them (and the ones within DEF_RECIPE_OVER of it);
  - the shop's commons: those its regions' random encounters leave; a shop with fewer than MIN_POOL looks one exit
    further at a time, up to REACH_MAX, and keeps the wider ring only where it adds a common (the rares stay those of
    the DEF_RECIPE_REACH regions);
  - an item's common, the shop's equipment walked cheapest first: of the commons whose species' mid level is at most
    WINDOW further from the item's level than the nearest one's, the best by |mid level - item level| + SPREAD * the
    times this shop already asked it (every material in some recipe, no shop on one material); a
    weapon takes the best, an armour or accessory the second when it is within SECOND of the best (the same shelf asks
    for both); AMOUNT of it by price;
  - a common no recipe asks then goes to the shop of its regions' server whose dearest item's level is nearest the
    common's mid level, as the common of that shop's three dearest items (a safety net: no common needs it today);
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
# The commons' spread: a shop's pool widens up to
# REACH_MAX exits while it has fewer than MIN_POOL commons; an item's candidates are within WINDOW levels of the nearest,
# scored by the level gap plus SPREAD per earlier use in the shop; armour takes the second within SECOND points.
MIN_POOL, REACH_MAX, WINDOW, SPREAD, SECOND = 3, 7, 8, 2, 2
# The checks: outside OFF_LEVEL_SHOP (Zhu Que's rings on the second server, cheap but only reached late: no low region
# is near it, accepted), no recipe's common is further than MAX_GAP levels from the item's.
OFF_LEVEL_SHOP, MAX_GAP = 23, 25


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
    import game_tables as gt
    battles = gt.battle_info()
    regions = gt.regions_of(battles)
    com, level, sp = gt.commons(battles)
    idx = {x["id"]: i for i, x in enumerate(sp)}
    info = {}
    for k, r in regions.items():
        c = Counter(com[idx[s]] for b in r["enc"] for s, _ in battles[b]["enemies"] if s in idx and com[idx[s]] != 0xFF)
        info[k] = (gt.mean_level(r, battles), c)
    mid = {}
    for m in range(16):
        lv = [level[i] for i, c in enumerate(com) if c == m]
        mid[m] = (min(lv) + max(lv)) / 2
    return info, {m: k for k, r in regions.items() for m in r["maps"]}, mid, gt.map_graph(), gt.map_areas()


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
    from collections import Counter
    lists, shops_of = ge.goods()
    towns = ge.shop_towns()
    out, plans = [], {}
    for n, ids in lists:
        for shop in shops_of[n]:
            eq = sorted({i for i in ids if its.get(i, {}).get("cat") in EQUIP}, key=lambda i: (its[i]["price"], i))
            if not eq or shop not in towns:
                continue
            top_level = curve(LEVEL_AT, its[eq[-1]]["price"])
            regs = shop_regions(sorted(towns[shop]), info, map_region, graph, areas, top_level, c)
            pool = sorted({m for k in regs for m in info[k][1]})
            for reach in range(c["DEF_RECIPE_REACH"] + 1, REACH_MAX + 1):
                if len(pool) >= MIN_POOL:
                    break
                wide = shop_regions(sorted(towns[shop]), info, map_region, graph, areas, top_level,
                                    dict(c, DEF_RECIPE_REACH=reach))
                wider = sorted({m for k in wide for m in info[k][1]})
                if len(wider) > len(pool):
                    pool = wider
            assert pool, f"shop {shop}: no region around it leaves a common"
            dealt = sorted((k for k in regs if k in rare_region), key=lambda k: (-rare_level[rare_region[k]], k))
            dear = sorted((i for i in eq if its[i]["price"] >= c["DEF_RECIPE_RARE_FROM"]),
                          key=lambda i: (-its[i]["price"], i))
            rare_of = {i: rare_region[dealt[r % len(dealt)]] for r, i in enumerate(dear)} if dealt else {}
            used = Counter()
            for i in eq:
                lv = curve(LEVEL_AT, its[i]["price"])
                g0 = min(abs(mid[m] - lv) for m in pool)
                score = {m: abs(mid[m] - lv) + SPREAD * used[m] for m in pool if abs(mid[m] - lv) <= g0 + WINDOW}
                near = sorted(score, key=lambda m: (score[m], -m))
                second = (its[i]["cat"] != "RECORDS_ITEM_WEAPON" and len(near) > 1 and
                          score[near[1]] - score[near[0]] < SECOND)
                common = near[1 if second else 0]
                used[common] += 1
                rare = rare_of.get(i, -1)
                rn = 0 if rare < 0 else 2 if its[i]["price"] >= c["DEF_RECIPE_TWO_RARES"] else 1
                out.append((shop, i, its[i], common, amount(its[i]["price"]), rare, rn))
            plans[shop] = (eq, top_level, areas[min(towns[shop]) & 0xFF] >> 7)
    asked = Counter(r[3] for r in out)
    servers = {m: {k >> 7 for k, (_lv, cnt) in info.items() if m in cnt and k != 0xFF} for m in range(16)}
    for m in range(16):
        shops = [sh for sh, (_eq, _top, srv) in plans.items() if srv in servers[m]]
        if asked[m] or not shops:
            continue
        sh = min(shops, key=lambda sh: (abs(plans[sh][1] - mid[m]), sh))
        dear = set(sorted(plans[sh][0], key=lambda i: (-its[i]["price"], i))[:3])
        out = [r[:3] + (m,) + r[4:] if r[0] == sh and r[1] in dear else r for r in out]
    return sorted(out, key=lambda r: (r[0], r[1]))


def spread_ok(rows):
    """The spread checks: (every common asked, no shop on a single common, the worst level gap outside
    OFF_LEVEL_SHOP within MAX_GAP), with the details for the message."""
    from collections import defaultdict
    _info, _mr, mid, _g, _a = world()
    every = {r[3] for r in rows} == set(range(16))
    per = defaultdict(set)
    for r in rows:
        per[r[0]].add(r[3])
    single = sorted(sh for sh, ms in per.items() if len(ms) < 2)
    gap = max(abs(mid[r[3]] - curve(LEVEL_AT, r[2]["price"])) for r in rows if r[0] != OFF_LEVEL_SHOP)
    return every, single, gap


def render(rows):
    lines = ["/* Generated by tools/gen_recipes.py: the forge's recipes, by shop and item ID: a common Hunt",
             " * Material of the shop's regions and how many, then a rare item (its index in def_hunt_targets, -1: none) and how",
             " * many. Regenerate instead of editing. */", '#include "definitive.h"', "", "const DefRecipe def_recipes[] = {"]
    for shop, i, it, com, n, rare, rn in rows:
        lines.append(f"    {{ {shop}, {com}, {i}, {n}, {rn}, {rare} }}, /* {it['name']} ({it['price']}) */")
    lines += ["};", f"const int def_recipe_count = {len(rows)};", ""]
    return "\n".join(lines)


def write_md(rows):
    import game_tables as gt
    OUT.mkdir(parents=True, exist_ok=True)
    en = (ROOT / "tools/text_override/en.txt").read_text()
    mats = {int(i): n.strip() for i, n in re.findall(r"^\$HuntMat(\d+): (.*)$", en, re.M)}
    names = re.findall(r"/\* (?:.*?): (.*?) \*/", (ROOT / "port/game/definitive_hunt_targets.c").read_text()
                       .split("def_super_rares")[0])
    import gen_equipment as ge
    towns = ge.shop_towns()
    areas = gt.map_areas()
    c = constants()
    lines = ["# The forge's recipes, by shop", "",
             "Every piece of equipment a shop sells asks, besides its Bits, for a common material of the hunting regions "
             f"around the shop (up to {c['DEF_RECIPE_REACH']} exits away, on its server, at the shop's level): the one "
             "nearest the level the item is bought at; from "
             f"{c['DEF_RECIPE_RARE_FROM']} Bits (old price) a rare item of one of those regions too (two from "
             f"{c['DEF_RECIPE_TWO_RARES']}).", ""]
    shop = None
    for sh, i, it, com, n, rare, rn in rows:
        if sh != shop:
            shop = sh
            where = ", ".join(sorted({gt.area_name(areas[m & 0xFF]) for m in towns[sh]}))
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
        every, single, gap = spread_ok(rows)
        print(f"{'ok' if ok else 'FAIL'} the recipes are the rule's ({len(rows)} rows); {'ok' if sane else 'FAIL'} "
              "materials and counts in range")
        print(f"{'ok' if every else 'FAIL'} all 16 commons asked; {'ok' if not single else 'FAIL'} no shop on one "
              f"common{'' if not single else ' (' + ', '.join(map(str, single)) + ')'}; {'ok' if gap <= MAX_GAP else 'FAIL'} "
              f"worst level gap {gap:.1f} (outside shop {OFF_LEVEL_SHOP}, at most {MAX_GAP})")
        return 0 if ok and sane and every and not single and gap <= MAX_GAP else 1
    GEN.write_text(text)
    write_md(rows)
    print(f"{GEN.relative_to(ROOT)}: {len(rows)} recipes; {OUT / 'receitas.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
