"""Readers of the game's tables (battles, enemy species, stages, maps) shared by the generators and the tests."""
import re
import struct
from collections import defaultdict
from pathlib import Path

import disc_files as df
import dump_text as dt

ROOT = Path(__file__).resolve().parents[1]
MATERIALS = 16       # DEF_HUNT_MATERIALS (port/game/definitive.h)
NUM = r"(0x[0-9A-Fa-f]+|\d+)"
FINAL_BOSS = {442, 466, 467}  # Galacticmon's phases
# Battles wfightmn gives rules of their own (state.type 1..4: 0x143, 0xB, 0x1E, 0x144) and the final boss's.
SPECIAL = {0x143, 0xB, 0x1E, 0x144, 325, 326}


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


def battle_rows():
    """The enemy rows (species, level, hp, mp, stat_scale) and, per battle, (row indices, kind, escape blocked)."""
    src = (ROOT / "src/fieldstg/fieldstg_80083784.c").read_text()
    body = src[src.index("RecordsEnemy fieldstg_battle_enemies"):]
    body = body[:body.index("\n};")]
    rows = [tuple(int(x) for x in m) for m in re.findall(r"\{\s*(-?\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", body)]
    bt = src[src.index("FieldstgBattle fieldstg_battles"):]
    battles = []
    for es, _fs, kind, bl in re.findall(r"\{\s*\{([^}]*)\},\s*(0x[0-9A-Fa-f]+|\d+),\s*(\d+),\s*\{([^}]*)\}\s*\}", bt):
        slots = [int(x) for x in re.findall(r"\[(\d+)\]", es)]
        flags = [int(x, 0) for x in bl.split(",")]
        battles.append(dict(rows=[i for i in slots if i], slots=slots, kind=int(kind), escape=flags[11], blocked=sum(flags)))
    return rows, battles


def constants():
    h = (ROOT / "port/game/definitive.h").read_text()
    c = {k: int(v, 0) for k, v in re.findall(r"#define (DEF_\w+)\s+(0x[0-9A-Fa-f]+|\d+)", h)}
    for k in ("DEF_DRI_CHALLENGES", "DEF_DRI_STAGES", "DEF_DRI_MEMBERS", "DEF_DRI_TIERS", "DEF_DRI_LEVEL_FIRST", "DEF_DRI_LEVEL_GAP", "DEF_DRI_LEVEL_STEP",
              "DEF_DRI_COMMON_LO", "DEF_DRI_COMMON_HI", "DEF_DRI_RARE_OVER", "DEF_DRI_TWO_RARES", "DEF_DRI_SCALE_EXP_PCT",
              "DEF_HUNT_HP_MAX", "DEF_DRI_RIVALS", "DEF_DRI_PER_AGENT",
              "DEF_HUNT_MATERIALS"):
        assert k in c, k
    return c


def species_records():
    """The enemy species of SDIGIEDT, in the bestiary's order."""
    ed = (df.DISC / "AAA/PRO/SDIGIEDT.PRO").read_bytes()
    tables = dt.char_tables()
    e = dt.find_file("DIGNAM", "ENG")
    d = (df.DISC / e.path).read_bytes()
    (_, offs), = dt.tables_of(d)
    names = [dt.decode(d, o, tables, latin=False)[0] for o in offs]
    out, o = [], 0
    while True:
        f = struct.unpack_from("<7h5h12hBB", ed, o)
        if f[0] == 0:
            return out
        out.append(dict(id=f[0], name=names[f[3]] if f[3] < len(names) else "?", techs=[t for t in f[4:7] if t],
                        resists=list(f[12:19]), type=f[24]))
        o += 0x46


def species_names():
    """{id: name} of the enemy species (the first row of an ID: the later ones are the same species' second records)."""
    s = (ROOT / "port/game/definitive_enemies.c").read_text()
    out, order = {}, []
    for sid, name in re.findall(r"\{ (\d+), \{[^}]*\}, [01] \}, /\* (.*?) \*/", s):
        order.append(int(sid))
        out.setdefault(int(sid), name)
    return out, order


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


def mean_level(r, battles):
    lv = [lv for b in r["enc"] for _, lv in battles[b]["enemies"]]
    return sum(lv) / len(lv)


def area_name(area):
    names = getattr(area_name, "cache", None)
    if names is None:
        e = dt.find_file("AMTMAP", "ENG")
        d = (df.DISC / e.path).read_bytes()
        (_, offs), = dt.tables_of(d)
        tables = dt.char_tables()
        names = area_name.cache = [dt.decode(d, o, tables, latin=False)[0].replace("<1>", " / ") for o in offs]
    i = (area & 0x7F) + 1
    return names[i] if i < len(names) else "?"


def map_graph():
    """The compass's map graph (port/game/definitive_map_graph.c, tools/gen_map_graph.py): {map: [maps]}."""
    s = (ROOT / "port/game/definitive_map_graph.c").read_text()

    def arr(name):
        return ints(re.search(name + r"\[\d*\] = \{(.*?)\};", s, re.S).group(1))
    nodes, first, edges = arr("def_map_nodes"), arr("def_map_first"), arr("def_map_edges")
    return {m: [nodes[e] for e in edges[first[i]:first[i + 1]]] for i, m in enumerate(nodes)}


def commons(battles):
    """The common Hunt Material of each bestiary species (its index in SDIGIEDT, the bestiary's), 0xFF for none, and the
    highest random-encounter level of each species that has one."""
    sp = species_records()
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


def stage_of(sid, level, rows, battles, c):
    """Species `sid` at `level`, from its own rows: per level the strongest (stat_scale, then HP)."""
    used = {r for b in battles for r in b["rows"] if rows[r][0] == sid}
    assert used, f"species {sid} fights in no battle"
    by_level = {}
    for r in sorted(used):
        _s, lv, hp, mp, sc = rows[r]
        if lv not in by_level or (sc, hp) > by_level[lv][1:3]:
            by_level[lv] = (lv, sc, hp, mp)
    pts = sorted(by_level.values())
    lo = [p for p in pts if p[0] <= level]
    hi = [p for p in pts if p[0] >= level]
    if lo and hi:
        a, b = lo[-1], hi[0]
        t = 0 if b[0] == a[0] else (level - a[0]) / (b[0] - a[0])
        sc, hp = a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t
        mp = a[3] if level - a[0] <= b[0] - level else b[3]
    else:
        a = hi[0] if hi else lo[-1]
        f = (level / a[0]) ** (c["DEF_DRI_SCALE_EXP_PCT"] / 100)
        sc, hp, mp = a[1] * f, a[2] * f, a[3]
    return dict(digimon=sid, level=level, hp=min(c["DEF_HUNT_HP_MAX"], int(hp + 0.5)), mp=mp,
                stat_scale=max(1, int(sc + 0.5)))
