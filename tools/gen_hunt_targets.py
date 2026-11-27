#!/usr/bin/env python3
"""LEGACY: frozen. The table it wrote is now hand-maintained, one case at a time, so
it no longer writes into port/game/; it still runs as a library and for its proposal under .dumps/.

The hunt's tables: the Rare Target of each
region (one Digimon), the twelve Super Rares and their territories, and the common materials. Review the
proposal before it is applied.

  tools/venv/bin/python tools/gen_hunt_targets.py            # proposal: .dumps/tasks/hunt/alvos.md; the game is untouched
  tools/venv/bin/python tools/gen_hunt_targets.py --apply    # also write port/game/definitive_hunt_targets.c

A region is a world map area plus the server bit (ststatus_map_areas, the byte as stored). Reads every stage's battle
lists from src/wstag/*.c (one list, or one per route of the story: FieldstgBattleLists x[N], which the game picks with
fieldstg_find_battle_lists by gamestate_data.route), each stage file's map from its header comment, the battles and their
enemies from src/fieldstg/fieldstg_80083784.c, and the species' names and types from the user's disc through
gen_enemy_weaknesses.py.

Rare Target rules:
  - one per region with a random encounter list of nonzero rate, fixed; ONE Digimon (an enemy slot of an existing
    battle: the game copies that enemy alone into the fight);
  - level: the mean level of the region's random encounters' enemies + LEVEL_OVER (it may be below the source's own
    level: a strong Digimon is lowered in a weak place); HP x HP_MULT (the runtime: DEF_HUNT_RARE_HP_MULT);
  - any species, a common one too, but: not one of the region's own encounters when another fits, the least repeated,
    a level of its own near the region's (its stats were made for that), then the region's theme (the types most common
    in its encounters), then one never met in a random encounter anywhere; never Galacticmon (the final boss) nor a
    Super Rare;
  - its rare item "<species> Core", with a numeral only when the species is the target of several regions.

Super Rares (SUPERS, chosen by hand): one Digimon each, its strongest battle's slot, a territory (a sector of one
server, SECTORS); level: the strongest Rare Target of the territory + SUPER_OVER; HP evenly from SUPER_HP_MIN to
SUPER_HP_MAX in the order of their level, which is also the hunts' order.

Common materials: every species met in a random encounter, in the order of the highest level it has in one (then its
index), in MATERIALS even groups, the weakest first.
"""
import argparse
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gen_enemy_weaknesses as gw  # noqa: E402

LEVEL_OVER = 5       # a Rare Target fights this many levels above its region's mean encounter level
HP_MULT = 4          # DEF_HUNT_RARE_HP_MULT: shown in the proposal only (the runtime applies it)
LEVEL_NEAR = (8, 16) # a candidate's own level within these of the target level: the best, then the second band
MATERIALS = 16       # DEF_HUNT_MATERIALS (port/game/definitive.h)
NO_THEME = {1}       # the species type with no theme (53 species): never counts for a region's theme
SUPER_OVER = 8
SUPER_HP_MIN, SUPER_HP_MAX = 15000, 30000
SUPER_PRICE_MIN, SUPER_PRICE_MAX = 8000, 16000  # a crystal at the Black Market ("sells high, ~10000 Bits")
# The tutorial hunt (hunt 0): the Sepikmon's first, given at the Velvet Room's first visit.
# Ogremon, the classic bully, in Central Park by Asuka City (world map area 0x1D: map 0x21D alone, Kunemon and Tapirmon
# of level 1), at level 3. Its HP makes it flee once or twice (DEF_SUPER_FLEE_PCT of it is 120) so the compass and
# the chase are shown before it falls.
TUTORIAL = dict(name="Ogremon", battle=204, slot=1, level=3, hp=480, sector=2, server=0, area=0x1D, price=1500)
OUT = ROOT / ".dumps/tasks/hunt"
GEN = ROOT / "port/game/definitive_hunt_targets.c"
NUM = r"(0x[0-9A-Fa-f]+|\d+)"

# The sectors of a server (hand-made: the game does not group its areas), by the world map areas (0..0x2D) walked
# through from each city; the second server (bit 7) mirrors them. Duel Island (6) is reached by a warp only: none.
SECTORS = {
    0: ("North", "Norte", [0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x8, 0x9, 0xA]),               # Xuan Wu City, the snow
    1: ("East", "Leste", [0xE, 0xF, 0x16, 0x17, 0x18, 0x1E]),                         # Qing Long City, Tyranno Valley
    2: ("Center", "Centro", [0xB, 0xD, 0x11, 0x13, 0x14, 0x15, 0x1D, 0x20]),           # Amaterasu City, Wire Forest
    3: ("West", "Oeste", [0x7, 0x10, 0x19, 0x1A, 0x1B, 0x1C, 0x21, 0x22, 0x26, 0x27]), # Bai Hu City, the deserts
    4: ("South", "Sul", [0xC, 0x12, 0x1F, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D]),  # Zhu Que, swamps
}
SERVERS = ("Asuka", "Amaterasu")
# The Super Rares: (species name, sector, server). Themed: the whale on the southern coast, the pharaoh in the desert,
# the tyrant in Tyranno Valley, the tree in the forest...
SUPERS = [
    ("Cherrymon", 2, 0),
    ("Whamon", 4, 0),
    ("Gryphonmon", 0, 0),
    ("Pharaohmon", 3, 0),
    ("MasterTyrannomon", 1, 0),
    ("WaruMonzaemon", 2, 1),
    ("HiAndromon", 3, 1),
    ("Kyukimon", 0, 1),
    ("Kimeramon", 4, 1),
    ("GrapLeomon", 1, 1),
    ("BK Seraphimon", 2, 1),
    ("Knightmon", 4, 0),
]


def ints(text):
    return [int(x, 0) for x in re.findall(r"-?0x[0-9A-Fa-f]+|-?\d+", text)]


def map_areas():
    s = (ROOT / "src/ststatus/ststatus_80099B6C.c").read_text()
    body = re.search(r"u8 ststatus_map_areas\[0xF0\] = \{(.*?)\};", s, re.S).group(1)
    return ints(body)


def stage_maps():
    """{wstag file number: [maps]}: each stage file names its own stage in its first comment ("WSTAG321: stage 0x28A").
    fieldstg_stages's second field is not the file's number, so it cannot be used for this."""
    out = defaultdict(list)
    for path in sorted((ROOT / "src/wstag").glob("wstag*.c")):
        m = re.search(r"/\* WSTAG(\d+): stage (0x[0-9A-Fa-f]+|\d+)", path.read_text())
        if m:
            out[int(m.group(1))].append(int(m.group(2), 0))
    return out


def stage_lists(path):
    """[(encounter battles with a nonzero rate, scripted battles)] for each FieldstgBattleLists entry of a stage file:
    the single ones and every route's entry of the per-route arrays."""
    s = path.read_text()
    listed = {n: int(b, 0) for n, b in re.findall(r"FieldstgListedBattle (\w+) = \{\s*" + NUM, s)}
    lists = {}
    for n, rate, refs in re.findall(r"FieldstgBattleList (\w+) = \{\s*" + NUM + r",\s*\{([^}]*)\}", s, re.S):
        lists[n] = (int(rate, 0), [listed[r] for r in re.findall(r"&(\w+)", refs) if r in listed])
    out = []
    for body in re.findall(r"FieldstgBattleLists \w+(?:\[\d+\])? = \{(.*?)\n\};", s, re.S):
        for enc, scripted in re.findall(r"-?\w+,\s*-?\w+,\s*-?\w+,\s*\{([^}]*)\},\s*(&\w+|NULL)", body):
            battles_enc = set()
            for e in re.findall(r"&(\w+)", enc):
                if e in lists and lists[e][0] != 0:
                    battles_enc.update(b for b in lists[e][1] if b)
            scripted = scripted.lstrip("&")
            battles_scr = set(b for b in (lists[scripted][1] if scripted in lists else []) if b)
            out.append((battles_enc, battles_scr))
    return out


def battle_info():
    """[{enemies: [(species, level)], slots: [(slot, species, level, hp)], kind, boss}] per fieldstg_battles index."""
    src = (ROOT / "src/fieldstg/fieldstg_80083784.c").read_text()
    body = src[src.index("RecordsEnemy fieldstg_battle_enemies"):]
    body = body[:body.index("\n};")]
    rows = [tuple(int(x) for x in m) for m in re.findall(r"\{\s*(-?\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", body)]
    bt = src[src.index("FieldstgBattle fieldstg_battles"):]
    out = []
    for es, _fs, kind, bl in re.findall(r"\{\s*\{([^}]*)\},\s*(0x[0-9A-Fa-f]+|\d+),\s*(\d+),\s*\{([^}]*)\}\s*\}", bt):
        idx = [int(x) for x in re.findall(r"\[(\d+)\]", es)]
        out.append(dict(enemies=[(rows[i][0], rows[i][1]) for i in idx if i],
                        slots=[(k, rows[i][0], rows[i][1], rows[i][2]) for k, i in enumerate(idx) if i],
                        kind=int(kind), boss=int(kind) == 2 and sum(int(x) for x in bl.split(",")) >= 9))
    return out


def roman(n):
    return "X" * (n // 10) + ["", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"][n % 10]


def material_names():
    """The Hunt Materials' names, $HuntMat0.. in the text overrides."""
    s = (ROOT / "tools/text_override/en.txt").read_text()
    return {int(i): n.strip() for i, n in re.findall(r"^\$HuntMat(\d+): (.*)$", s, re.M)}


def regions_of(battles):
    areas = map_areas()
    regions = defaultdict(lambda: dict(enc=set(), scr=set(), maps=set()))
    for f, maps in sorted(stage_maps().items()):
        lists = stage_lists(ROOT / f"src/wstag/wstag{f}.c")
        for m in maps:
            region = areas[m & 0xFF]
            for enc, scr in lists:
                regions[region]["enc"] |= enc
                regions[region]["scr"] |= scr
            regions[region]["maps"].add(m)
    return {k: v for k, v in regions.items() if v["enc"]}


def sources(battles, sp):
    """{species: [(battle, slot, level, hp)]}: every battle slot of a bestiary species, Galacticmon's left out."""
    out = defaultdict(list)
    for b, x in enumerate(battles):
        # Galacticmon's battles, and the card duels (kind 5: the eight "Cardmon", one species each), are never a source.
        if any(s in gw.FINAL_BOSS for s, _ in x["enemies"]) or x["kind"] == 5:
            continue
        for k, s, lv, hp in x["slots"]:
            if s in sp and s not in gw.FINAL_BOSS:
                out[s].append((b, k, lv, hp))
    return out


def supers(battles, sp, src, rare_rows):
    byname = {x["name"]: i for i, x in sp.items()}
    top = defaultdict(int)
    for r in rare_rows:
        top[(SECTOR_OF.get(r["region"] & 0x7F), r["region"] >> 7)] = max(top[(SECTOR_OF.get(r["region"] & 0x7F),
                                                                            r["region"] >> 7)], r["level"])
    out = []
    for name, sector, server in SUPERS:
        s = byname[name]
        b, k, lv, hp = max(src[s], key=lambda x: (x[2], x[0]))
        out.append(dict(species=s, name=name, battle=b, slot=k, own=lv, sector=sector, server=server,
                        level=min(99, top[(sector, server)] + SUPER_OVER), base_hp=hp))
    out.sort(key=lambda x: (x["level"], x["species"]))
    for i, x in enumerate(out):
        x["hp"] = SUPER_HP_MIN + (SUPER_HP_MAX - SUPER_HP_MIN) * i // max(1, len(out) - 1)
        x["price"] = SUPER_PRICE_MIN + (SUPER_PRICE_MAX - SUPER_PRICE_MIN) * i // max(1, len(out) - 1)
        x["area"] = -1
    t = dict(TUTORIAL)
    k = [x[0] for x in battles[t["battle"]]["slots"]].index(t["slot"])
    t["species"] = battles[t["battle"]]["slots"][k][1]
    assert sp[t["species"]]["name"] == t["name"], sp[t["species"]]["name"]
    t["own"], t["base_hp"] = battles[t["battle"]]["slots"][k][2], battles[t["battle"]]["slots"][k][3]
    return [t] + out


SECTOR_OF = {a: s for s, (_, _, areas) in SECTORS.items() for a in areas}


def propose():
    battles = battle_info()
    sp = {x["id"]: x for x in gw.species()}
    src = sources(battles, sp)
    regions = regions_of(battles)
    super_ids = {next(i for i, x in sp.items() if x["name"] == n) for n, _, _ in SUPERS}
    met = {s for r in regions.values() for b in r["enc"] for s, _ in battles[b]["enemies"]}
    uses = Counter()
    rows = []
    for region in sorted(regions, key=lambda k: (mean_level(regions[k], battles), k)):
        r = regions[region]
        own = {s for b in r["enc"] for s, _ in battles[b]["enemies"]}
        theme = Counter(sp[s]["type"] for s in own if s in sp and sp[s]["type"] not in NO_THEME)
        level = min(99, round(mean_level(r, battles)) + LEVEL_OVER)

        def key(s):
            b, k, lv, hp = best_source(src[s], level)
            gap = abs(lv - level)
            band = 0 if gap <= LEVEL_NEAR[0] else 1 if gap <= LEVEL_NEAR[1] else 2
            return (s in own, band, uses[sp[s]["name"]], -theme[sp[s]["type"]], s in met, gap, s)

        cands = [s for s in src if s not in super_ids]
        pick = min(cands, key=key)
        uses[sp[pick]["name"]] += 1  # by name: a few species share one (the BK forms do not)
        b, k, lv, hp = best_source(src[pick], level)
        rows.append(dict(region=region, area=region & 0x7F, server=region >> 7, maps=sorted(r["maps"]),
                         mean=mean_level(r, battles), top=max(lv_ for b_ in r["enc"] for _, lv_ in battles[b_]["enemies"]),
                         species=pick, name=sp[pick]["name"], battle=b, slot=k, own=lv, own_hp=hp, level=level,
                         hp=min(30000, hp * level // lv * HP_MULT), local=pick in own,
                         theme=theme[sp[pick]["type"]] > 0))
    count = Counter(r["name"] for r in rows)
    seen = Counter()
    for r in sorted(rows, key=lambda r: (r["level"], r["region"])):
        if count[r["name"]] > 1:
            seen[r["name"]] += 1
            r["variant"] = seen[r["name"]]
        else:
            r["variant"] = 0
        r["rare_name"] = f"{r['name']} Core" + (f" {roman(r['variant'])}" if r["variant"] else "")
    rows.sort(key=lambda x: x["region"])
    return rows, supers(battles, sp, src, rows)


def mean_level(r, battles):
    lv = [lv for b in r["enc"] for _, lv in battles[b]["enemies"]]
    return sum(lv) / len(lv)


def best_source(rows, level):
    """The battle slot of a species whose own level is closest to `level` (its stats were made nearest to it)."""
    return min(rows, key=lambda x: (abs(x[2] - level), x[0], x[1]))


def area_name(area):
    names = getattr(area_name, "cache", None)
    if names is None:
        import dump_text as dt
        import disc_files as df
        e = dt.find_file("AMTMAP", "ENG")
        d = (df.DISC / e.path).read_bytes()
        (_, offs), = dt.tables_of(d)
        tables = dt.char_tables()
        names = area_name.cache = [dt.decode(d, o, tables, latin=False)[0].replace("<1>", " / ") for o in offs]
    i = (area & 0x7F) + 1
    return names[i] if i < len(names) else "?"


COMPASS_REACH = 3  # DEF_SUPER_COMPASS_REACH


def map_graph():
    """The compass's map graph (port/game/definitive_map_graph.c, tools/gen_map_graph.py): {map: [maps]}."""
    s = (ROOT / "port/game/definitive_map_graph.c").read_text()

    def arr(name):
        return ints(re.search(name + r"\[\d*\] = \{(.*?)\};", s, re.S).group(1))
    nodes, first, edges = arr("def_map_nodes"), arr("def_map_first"), arr("def_map_edges")
    return {m: [nodes[e] for e in edges[first[i]:first[i + 1]]] for i, m in enumerate(nodes)}


def compass_md(sup):
    """Per hunt, the maps out of its territory where the compass still shows (def_super_in_compass_zone): the areas
    up to COMPASS_REACH exits away, with their distance."""
    graph, areas = map_graph(), map_areas()

    def ground(x, m):
        if not 0x200 <= m < 0x300 or areas[m & 0xFF] == 0xFF:
            return False
        a, srv = areas[m & 0xFF] & 0x7F, areas[m & 0xFF] >> 7
        if x["area"] >= 0:
            return a == x["area"] and srv == x["server"]
        return SECTOR_OF.get(a) == x["sector"] and srv == x["server"]

    out = ["", f"## Bússola: os mapas em volta do território (até {COMPASS_REACH} saídas)", "",
           "A bússola aparece no território e nos mapas até 3 saídas fora dele (a regra vale para todas as caçadas; "
           "a temperatura é a distância ao mapa do alvo). Por caçada, as áreas de fora onde ela aparece "
           "(entre parênteses, a menor distância em saídas):", ""]
    for i, x in enumerate(sup):
        dist = {m: 0 for m in graph if ground(x, m)}
        todo = list(dist)
        while todo:
            m = todo.pop(0)
            if dist[m] >= COMPASS_REACH:
                continue
            for n in graph[m]:
                if n not in dist:
                    dist[n] = dist[m] + 1
                    todo.append(n)
        near = {}
        for m, d in dist.items():
            if d > 0:
                key = " ".join(area_name(areas[m & 0xFF] & 0x7F).split()) if 0x200 <= m < 0x300 and areas[m & 0xFF] != 0xFF \
                    else f"0x{m:X}"
                near[key] = min(near.get(key, 9), d)
        inside = sum(1 for d in dist.values() if d == 0)
        out.append(f"- **{i} {x['name']}**: {inside} mapas no território; em volta: "
                   + (", ".join(f"{k} ({d})" for k, d in sorted(near.items(), key=lambda t: (t[1], t[0]))) or "nenhum"))
    return out


def write_md(rows, sup):
    OUT.mkdir(parents=True, exist_ok=True)
    lines = ["# Raros regionais e super raros (proposta)", "",
             f"Raro regional: 1 Digimon, nível = média dos encontros da região + {LEVEL_OVER}, HP × {HP_MULT} (teto 30000). "
             "Escolha: fora dos encontros da própria região, o menos repetido, nível próprio perto do da região, depois o "
             "tema (o tipo mais comum nos encontros dela), depois um que não aparece em encontro aleatório. "
             "Aprove, risque ou troque qualquer linha (diga a espécie que quer).", "",
             "| região | área | servidor | setor | média | maior | raro | tema? | fonte (batalha:slot, nível próprio) | nível | HP | Núcleo |",
             "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for r in rows:
        sec = SECTOR_OF.get(r["area"])
        sec = SECTORS[sec][1] if sec is not None else "—"
        lines.append(f"| 0x{r['region']:02X} | {area_name(r['area']) if r['region'] != 0xFF else 'veículos'} | "
                     f"{SERVERS[r['server']] if r['region'] != 0xFF else '—'} | {sec} | {r['mean']:.1f} | {r['top']} | "
                     f"{r['name']}{' (da região!)' if r['local'] else ''} | {'sim' if r['theme'] else ''} | "
                     f"{r['battle']}:{r['slot']}, Lv{r['own']} | {r['level']} | {r['hp']} | {r['rare_name']} |")
    rep = Counter(r["name"] for r in rows)
    lines += ["", f"{len(rows)} regiões, {len(rep)} espécies diferentes; repetidas: "
              + (", ".join(f"{n} ×{c}" for n, c in rep.most_common() if c > 1) or "nenhuma"), "",
              "## Super raros (ordem da caçada = ordem do nível)", "",
              f"Nível = o raro regional mais forte do território + {SUPER_OVER}; HP de {SUPER_HP_MIN} a {SUPER_HP_MAX} "
              "na ordem. Território = um setor de um servidor.", "",
              "| # | super raro | território | fonte (batalha:slot, nível próprio) | nível | HP | cristal (preço) |",
              "|---|---|---|---|---|---|---|"]
    for i, x in enumerate(sup):
        where = (area_name(x["area"]).replace("\n", " ") if x["area"] >= 0 else SECTORS[x["sector"]][1]) + \
            f" ({SERVERS[x['server']]})"
        lines.append(f"| {i} | {x['name']} | {where} | {x['battle']}:{x['slot']}, Lv{x['own']} | {x['level']} | "
                     f"{x['hp']} | {x['name']} Crystal ({x['price']} Bits) |")
    lines += compass_md(sup)
    lines += ["", "## Setores (feitos à mão; o servidor Amaterasu espelha)", ""]
    for s, (en, pt, areas) in SECTORS.items():
        lines.append(f"- **{pt}**: " + ", ".join(f"{area_name(a)} (0x{a:02X})" for a in areas))
    (OUT / "alvos.md").write_text("\n".join(lines) + "\n")


def commons(battles):
    """The common Hunt Material of each bestiary species (its index in SDIGIEDT, the bestiary's), 0xFF for none, and the
    highest random-encounter level of each species that has one."""
    sp = gw.species()
    idx = {x["id"]: i for i, x in enumerate(sp)}
    level = {}
    for f in stage_maps():
        for enc, _scr in stage_lists(ROOT / f"src/wstag/wstag{f}.c"):
            for b in enc:
                for s, lv in battles[b]["enemies"]:
                    if s in idx:
                        level[idx[s]] = max(level.get(idx[s], 0), lv)
    order = sorted(level, key=lambda i: (level[i], i))
    out = [0xFF] * len(sp)
    for rank, i in enumerate(order):
        out[i] = rank * MATERIALS // len(order)
    return out, level, sp


def write_commons_md(com, level, sp):
    names = material_names()
    lines = ["# Materiais de Caça comuns", "",
             "Cada inimigo derrotado numa batalha aleatória vencida deixa o material da sua espécie com 25% de chance. "
             "As espécies que aparecem em batalha aleatória (todas as rotas), na ordem do maior nível em que aparecem, "
             "em 16 grupos iguais.", "",
             "| material | níveis | espécies |", "| --- | --- | --- |"]
    for m in range(MATERIALS):
        group = [i for i, c in enumerate(com) if c == m]
        lv = [level[i] for i in group]
        lines.append(f"| {m} {names.get(m, '?')} | {min(lv)}–{max(lv)} | {', '.join(sp[i]['name'] for i in group)} |")
    lines.append(f"\nSem material (nunca em batalha aleatória): {sum(c == 0xFF for c in com)} espécies.")
    (OUT / "materiais.md").write_text("\n".join(lines) + "\n")


def write_c(rows, sup, com):
    lines = ["/* Generated by tools/gen_hunt_targets.py --apply: the Rare Target of each region (one enemy slot of an",
             " * existing battle, the level it fights at, its rare item's numeral: 0 when its species is no other region's),",
             " * the Super Rares in the hunts' order, then each bestiary species' common material. Regenerate instead of",
             " * editing; review the table. */",
             '#include "definitive.h"', "",
             "const DefHuntTarget def_hunt_targets[] = {"]
    for r in rows:
        lines.append(f"    {{ 0x{r['region']:02X}, {r['battle']}, {r['slot']}, {r['level']}, {r['variant']} }}, /* {r['name']}: "
                     f"{r['rare_name']} */")
    lines += ["};", f"const int def_hunt_target_count = {len(rows)};", "",
              "const DefSuperRare def_super_rares[DEF_SUPER_COUNT] = {"]
    for x in sup:
        lines.append(f"    {{ {x['battle']}, {x['slot']}, {x['level']}, {x['hp']}, {x['sector']}, {x['server']}, {x['area']}, "
                     f"{x['species']}, {x['price']} }}, /* {x['name']} */")
    lines += ["};", "",
              "/* The sector of each world map area (0..0x2D; the second server's are the same), -1: none. */",
              "const signed char def_hunt_sector_of[DEF_HUNT_AREAS] = {"]
    sec = [SECTOR_OF.get(a, -1) for a in range(0x2E)]
    for i in range(0, len(sec), 16):
        lines.append(f"    /* 0x{i:02X} */ " + ", ".join(f"{c:2d}" for c in sec[i:i + 16]) + ",")
    lines += ["};", "", "const unsigned char def_hunt_commons[DEF_BESTIARY_SPECIES] = {"]
    for i in range(0, len(com), 16):
        lines.append(f"    /* {i:3d} */ " + ", ".join(f"0x{c:02X}" for c in com[i:i + 16]) + ",")
    lines += ["};", ""]
    GEN.write_text("\n".join(lines))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true", help="also write port/game/definitive_hunt_targets.c")
    args = ap.parse_args()
    if args.apply:
        sys.exit("LEGACY: port/game/definitive_hunt_targets.c is hand-maintained now; "
                 "this tool no longer overwrites it")
    rows, sup = propose()
    assert len(sup) == 13 and len({x["name"] for x in sup}) == 13
    com, level, sp = commons(battle_info())
    assert len(com) == 193, len(com)  # DEF_BESTIARY_SPECIES
    write_md(rows, sup)
    write_commons_md(com, level, sp)
    print(f"{len(rows)} regions with random encounters, {len({r['species'] for r in rows})} species: {OUT / 'alvos.md'}")
    print(f"commons: {sum(c != 0xFF for c in com)} species with one")
    if args.apply:
        write_c(rows, sup, com)
        print(f"written {GEN}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
