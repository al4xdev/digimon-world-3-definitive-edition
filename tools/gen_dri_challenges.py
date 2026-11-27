#!/usr/bin/env python3
"""LEGACY: frozen. The table it wrote is now hand-maintained, one case at a time, so
it no longer writes into port/game/; it still runs as a library and for its proposal under .dumps/.

The DRI Agents' Gauntlet. Each challenge is one battle that runs the four stages of an evolution line, Rookie to
Mega: the first three are its enemy members and the battle brings in the next when one falls
(wfightmn_knockout); the Mega comes in as a wave in a fallen member's slot (def_dri_next_wave). So the "next form" comes
in at its own full HP while the party keeps its state. Review the list.

  tools/venv/bin/python tools/gen_dri_challenges.py            # write port/game/definitive_dri_table.c and the list
  tools/venv/bin/python tools/gen_dri_challenges.py --check    # exit 1 if the committed table is not the rule's

Reads the battles and their enemies from src/fieldstg/fieldstg_80083784.c, the stages' battle lists from src/wstag/*.c
(gen_hunt_targets.py's readers), the species from port/game/definitive_enemies.c (its IDs, and its comments' names for
the list), the items from src/main/records.c, the Rare Targets from port/game/definitive_hunt_targets.c and the commons'
names from tools/text_override/en.txt: no disc. The numbers are definitive.h's (DEF_DRI_*, DEF_HUNT_*).

The lines (LINES below) are a choice, not a rule: the disc's enemy species have no Greymon, ExVeemon, Angemon, Growlmon
or Gallantmon, so those stages are the player's own forms fighting on an enemy species' record (FORMS, def_dri_record);
no Gargomon, Rapidmon or Gatomon exists at all. Each DRI Agent offers two lines (DEF_DRI_PER_AGENT). The rule for the rest,
by a line's rank r (its place in difficulty, LINES' `rank`; the index is only its save slot):
  - its Champion fights at DEF_DRI_LEVEL_FIRST + r * DEF_DRI_LEVEL_GAP, each next stage DEF_DRI_LEVEL_STEP
    higher and its Rookie DEF_DRI_LEVEL_STEP lower (the Rookie came last: the other stages kept their levels); a stage's HP and stat_scale follow its species' own rows in the battle table (the game raises a species
    that way: Tyrannomon's stat_scale is 16 at level 6, 83 at 52): between two of its levels, the line between them;
    past its lowest or highest, that row times (level ratio) ** (DEF_DRI_SCALE_EXP_PCT / 100), the growth of the game's
    own rows of one species; MP as the nearest row's; HP never past DEF_HUNT_HP_MAX (an s16);
  - a player form's stage has MP FORM_MP: an enemy pays its techniques' MP (fightstg_80086A00.c: short of it, it only
    says so), and the form's signature costs what the base's own row may not have (Persiamon's has 40);
  - a stage past the Champion never has less HP than the one before it grown by the level ratio (as above): the DDNA
    Ultimates are boss rows, a mob base's Mega would otherwise come in frailer than them (Beelzemon on Ghoulmon after
    Megadramon); the Rookie does not raise its Champion;
  - the battle it is set up as: a kind-2 battle (escape blocked, the bosses' immunities, its XP and Bits on a win) with
    none of the line's species in it, not one wfightmn gives special rules (types 1-4) nor the final boss's, its own level
    closest to the Mega's;
  - its unlock: the strongest common Hunt Material whose band (the species that drop it, by their highest random
    encounter) tops out at or under the Champion's level, DEF_DRI_COMMON_LO..HI of it rising with the challenge (rounded
    to 5); and one rare item (two from challenge DEF_DRI_TWO_RARES): Rare Targets at most DEF_DRI_RARE_OVER levels above
    the Champion, the line's own species first, then the strongest, then a second lead species (two from rank
    DEF_DRI_TWO_RARES on);
  - its rewards: the line's family of elemental "Power" accessories (records.c: "Fire Power 1", "2", "S"), tier n for the
    n-th stage felled past the Rookie. No new items.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gen_hunt_targets as gh  # noqa: E402

GEN = ROOT / "port/game/definitive_dri_table.c"
OUT = ROOT / ".dumps/tasks/dri"
FORM_MP = 9999  # the bosses' and most rows' MP
FINAL_BOSS = {442, 466, 467}  # Galacticmon's phases (gen_enemy_weaknesses.FINAL_BOSS)
# Battles wfightmn gives rules of their own (state.type 1..4: 0x143, 0xB, 0x1E, 0x144) and the final boss's.
SPECIAL = {0x143, 0xB, 0x1E, 0x144, 325, 326}

# Player forms that fight as a stage (records_digimon IDs, src/main/records.c; names from ?SDIGNAM): none has an enemy
# record on the disc, so a stage written (form, base) shows the form and takes the base enemy species' level rows here and
# its record in the game (stats, resistances; def_dri_record), under the form's name, type and techniques, with a plan
# that casts its signature. A base is an enemy species whose strong resistance is the form's own best one (the port's
# weakness table, definitive_enemies.c) and, where one fits, of a body like the form's (its cameras frame the form).
FORMS = {3: "Agumon", 31: "Patamon", 145: "Veemon", 366: "Guilmon", 373: "Renamon", 383: "Kotemon", 384: "Monmon",
         385: "Kumamon", 5: "Greymon", 6: "Devimon", 20: "Angemon", 26: "SkullGreymon", 59: "Phoenixmon", 66: "Myotismon",
         148: "Imperialdramon", 196: "MetalGarurumon", 213: "WarGreymon", 214: "Seraphimon", 230: "GranKuwagamon",
         234: "Angewomon", 259: "ExVeemon", 260: "Stingmon", 359: "Imperialdramon-F", 367: "Growlmon", 369: "Gallantmon",
         372: "MegaGargomon", 374: "Kyubimon", 376: "Sakuyamon", 377: "Beelzemon", 378: "MaloMyotismon", 386: "Dinohumon",
         387: "Hookmon", 388: "Grizzmon", 392: "GuardiAngemon", 393: "Cannondramon", 394: "Marsmon"}

# The lines: two per DRI Agent (`agent`, an index of AGENTS), each agent's own Rookie's line and a
# second one. A line's index is its slot in the second card record (unlocked bit, best stage) and its Tamer's texts
# ($DriRival<index>), so the first five keep the slots they had in 0.8.5; `rank` (0..15, all different) is its place in
# difficulty: its levels and its unlock's materials follow the rank, not the index. Every agent's own line ranks in the
# first half and his second line in the second half, so each agent has an early and a late one. Stages as enemy species
# IDs, Rookie first, or (player form, base species); the family's three item IDs. A player Rookie fights on its
# Champion's base (the same body's cameras, the line's resistances). The disc has eleven Rookie species and the party's
# eight Rookie forms, so a line with neither its own Rookie takes the nearest in kind: Hagurumon for Guardromon, Kiwimon
# (a bird) for the line that ends in Phoenixmon, Kunemon (the thunder larva) for both insect lines, Goburimon (an imp of
# a Virus) for Impmon's, and Ken's Kotemon for his second line as Johann's Agumon is for his.
LINES = [
    dict(name="Terriermon", agent=7, rank=9, family="Metal Power", stages=(122, 435, 434, (372, 443)), rewards=(354, 355, 356),
         note="no Gargomon nor Rapidmon: Guardromon, a machine, then Antylamon (Andiramon), the Tamers' Ultimate"),
    dict(name="Veemon", agent=2, rank=3, family="Bolt Power", stages=((145, 135), (259, 135), 450, (148, 444)), rewards=(351, 352, 353),
         note="the player forms ExVeemon and Imperialdramon, on Devidramon's and BK Imperialdramon's records"),
    dict(name="Patamon", agent=0, rank=4, family="Wind Power", stages=((31, 7), (20, 7), 452, (214, 447)), rewards=(348, 349, 350),
         note="the player forms Angemon and Seraphimon, on Airdramon's and BK Seraphimon's records"),
    dict(name="Agumon", agent=3, rank=6, family="Fire Power", stages=((3, 8), (5, 8), 449, (213, 448)), rewards=(339, 340, 341),
         note="the player forms Greymon and WarGreymon, on Tyrannomon's and BK WarGreymon's records"),
    dict(name="Guilmon", agent=4, rank=7, family="Dark Power", stages=((366, 69), (367, 69), 451, (369, 445)), rewards=(357, 358, 359),
         note="the player forms Growlmon and Gallantmon, on Flarerizamon's and BK WarGrowlmon's records; Dark for a "
              "Virus line (Fire is Agumon's)"),
    dict(name="Devimon", agent=0, rank=12, family="Dark Power", stages=(217, (6, 302), (66, 177), (378, 236)),
         rewards=(357, 358, 359),
         note="Patamon's fallen path, all player forms: Devimon on Gargoylemon, Myotismon on LadyDevimon, MaloMyotismon "
              "on VenomMyotismon (the form it rose from)"),
    dict(name="Renamon", agent=1, rank=5, family="Ice Power", stages=((373, 327), (374, 327), 453, (376, 231)), rewards=(345, 346, 347),
         note="Kyubimon on Lynxmon (a fire beast), Taomon, Sakuyamon on Puppetmon (Water, Ice, Thunder and Machine strong)"),
    dict(name="Angewomon", agent=1, rank=13, family="Water Power", stages=(198, (234, 437), 452, (59, 193)),
         rewards=(342, 343, 344),
         note="no Gatomon on the disc: Angewomon on Persiamon, MagnaAngemon, Phoenixmon on Gryphonmon"),
    dict(name="Wormmon", agent=2, rank=11, family="Bolt Power", stages=(32, (260, 238), 450, (359, 444)),
         rewards=(351, 352, 353),
         note="Stingmon on Snimon (an insect), Paildramon, Imperialdramon-F on BK Imperialdramon's record"),
    dict(name="Agumon (dark)", agent=3, rank=14, family="Fire Power", stages=((3, 8), (5, 8), (26, 215), 448),
         rewards=(339, 340, 341),
         note="Greymon (BlueGreymon once the recolour is done), SkullGreymon on SkullMammothmon, BK WarGreymon"),
    dict(name="Impmon", agent=4, rank=15, family="Dark Power", stages=(80, 69, 54, (377, 312)), rewards=(357, 358, 359),
         note="no Impmon line on the disc: Flarerizamon, Megadramon, Beelzemon on Ghoulmon"),
    dict(name="Kotemon", agent=5, rank=0, family="Wind Power", stages=((383, 241), (386, 241), 454, (394, 337)),
         rewards=(348, 349, 350),
         note="Dinohumon on Divermon, Kyukimon, Marsmon on Zanbamon (a sword Mega)"),
    dict(name="Musyamon", agent=5, rank=8, family="Ice Power", stages=((383, 241), 173, 178, (196, 356)), rewards=(345, 346, 347),
         note="the warriors' line: Musyamon, Knightmon, MetalGarurumon on HiAndromon (Machine strong)"),
    dict(name="Kumamon", agent=6, rank=2, family="Metal Power", stages=((385, 14), (388, 14), 456, (393, 115)),
         rewards=(354, 355, 356),
         note="Grizzmon on Monzaemon (a bear), GrapLeomon, Cannondramon on Machinedramon"),
    dict(name="Kuwagamon", agent=6, rank=10, family="Bolt Power", stages=(32, 51, 226, (230, 60)), rewards=(351, 352, 353),
         note="the insects: Kuwagamon, Okuwamon, GranKuwagamon on H-Kabuterimon (a beetle, Thunder strong)"),
    dict(name="Monmon", agent=7, rank=1, family="Water Power", stages=((384, 134), (387, 134), 455, (392, 270)),
         rewards=(342, 343, 344),
         note="Hookmon on Cyclonemon (a cannon arm), Armormon, GuardiAngemon on Vikemon"),
]

# The DRI Agents: (map, actor ID) pairs after whose own talk the list opens (the actor ID is FieldstgPlacedActor.id, the
# actor's base.key1); map -1 matches any map. Found on the lab with tools/find_dri_agents.py (needs the disc): the DDNA
# quests' researchers (Kazuki and Michael on 0x21D; Bob, Johann, Jimmy, Ken, Yuji, Yang). Their story's pig forms
# ("Oink...", actors 157..161) are left out: a quest's state, not an agent.
# The eight DRI Agents, each with his DDNA quest: (map, actor ID). Only these open
# the Gauntlet: an actor of the same sprite elsewhere is an ordinary NPC. The agent's Rookie is read from his own talk
# (the "joined" conditions), so it is not listed here. Each lists the LINES whose `agent` is his, easier first.
AGENTS = [  # (map, actor ID, name, Rookie)
    (0x21D, 43, "Kazuki", "Patamon"),
    (0x21D, 44, "Michael", "Renamon"),
    (0x226, 43, "Bob", "Veemon"),
    (0x22B, 43, "Johann", "Agumon"),
    (0x22E, 43, "Jimmy", "Guilmon"),
    (0x258, 43, "Ken", "Kotemon"),
    (0x258, 44, "Yuji", "Kumamon"),
    (0x258, 163, "Yang", "Monmon"),
]


def constants():
    h = (ROOT / "port/game/definitive.h").read_text()
    c = {k: int(v, 0) for k, v in re.findall(r"#define (DEF_\w+)\s+(0x[0-9A-Fa-f]+|\d+)", h)}
    for k in ("DEF_DRI_CHALLENGES", "DEF_DRI_STAGES", "DEF_DRI_MEMBERS", "DEF_DRI_TIERS", "DEF_DRI_LEVEL_FIRST", "DEF_DRI_LEVEL_GAP", "DEF_DRI_LEVEL_STEP",
              "DEF_DRI_COMMON_LO", "DEF_DRI_COMMON_HI", "DEF_DRI_RARE_OVER", "DEF_DRI_TWO_RARES", "DEF_DRI_SCALE_EXP_PCT",
              "DEF_HUNT_HP_MAX", "DEF_DRI_RIVALS", "DEF_DRI_PER_AGENT",
              "DEF_HUNT_MATERIALS"):
        assert k in c, k
    return c


def species():
    """{id: name} of the enemy species (the first row of an ID: the later ones are the same species' second records)."""
    s = (ROOT / "port/game/definitive_enemies.c").read_text()
    out, order = {}, []
    for sid, name in re.findall(r"\{ (\d+), \{[^}]*\}, [01] \}, /\* (.*?) \*/", s):
        order.append(int(sid))
        out.setdefault(int(sid), name)
    return out, order


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


def items():
    s = (ROOT / "src/main/records.c").read_text()
    body = s[s.index("RecordsItem records_items[] = {"):]
    body = body[:body.index("\n};")]
    return {int(m.group(3)): dict(cat=m.group(2), name=m.group(4))
            for m in re.finditer(r"\{ (\S+), \d+, \d+, (\w+), \d+, \{ 0, 0 \} \}, /\* (\d+) (.*?) \*/", body)}


def rares(rows, battles):
    """[dict(index, level, lead, variant)] of def_hunt_targets (the index is the rare item's; the lead is the target's
    species, its battle's enemy in its slot)."""
    s = (ROOT / "port/game/definitive_hunt_targets.c").read_text()
    out = []
    for i, (_r, b, k, lv, v) in enumerate(re.findall(r"\{ 0x([0-9A-F]+), (\d+), (\d+), (\d+), (\d+) \}", s)):
        out.append(dict(index=i, level=int(lv), lead=rows[battles[int(b)]["slots"][int(k)]][0], variant=int(v)))
    return out


def material_tops(order):
    """Each common material's band top: the highest random-encounter level of the species that drop it."""
    s = (ROOT / "port/game/definitive_hunt_targets.c").read_text()
    body = re.search(r"def_hunt_commons\[DEF_BESTIARY_SPECIES\] = \{(.*?)\};", s, re.S).group(1)
    com = [int(x, 0) for x in re.sub(r"/\*.*?\*/", "", body).split(",") if x.strip()]
    info = gh.battle_info()
    index = {}
    for i, sid in enumerate(order):
        index.setdefault(sid, i)
    level = {}
    for f in gh.stage_maps():
        for enc, _scr in gh.stage_lists(ROOT / f"src/wstag/wstag{f}.c"):
            for b in enc:
                for sid, lv in info[b]["enemies"]:
                    if sid in index:
                        level[index[sid]] = max(level.get(index[sid], 0), lv)
    tops = {}
    for i, m in enumerate(com):
        if m < 0xFF and i in level:
            tops[m] = max(tops.get(m, 0), level[i])
    return tops


def rival_texts():
    """The Tamers' texts ($DriRival<n>, ...Hello, ...Won, ...Lost in en.txt), by name."""
    s = (ROOT / "tools/text_override/en.txt").read_text()
    return {k: v.strip() for k, v in re.findall(r"^\$(DriRival\d+\w*): (.*)$", s, re.M)}


def material_names():
    s = (ROOT / "tools/text_override/en.txt").read_text()
    return {int(i): n.strip() for i, n in re.findall(r"^\$HuntMat(\d+): (.*)$", s, re.M)}


def roman(n):
    return "X" * (n // 10) + ["", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"][n % 10]


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


def round5(v):
    return max(5, (v + 2) // 5 * 5)


def table():
    c = constants()
    names, order = species()
    rows, battles = battle_rows()
    its = items()
    tops = material_tops(order)
    targets = rares(rows, battles)
    n = len(LINES)
    assert 0 < n <= c["DEF_DRI_CHALLENGES"], n
    assert n <= c["DEF_DRI_RIVALS"], f"{n} lines, DEF_DRI_RIVALS {c['DEF_DRI_RIVALS']}: each line needs its Tamer"
    assert sorted(line["rank"] for line in LINES) == list(range(n)), "every rank once"
    for a in range(len(AGENTS)):
        assert sum(line["agent"] == a for line in LINES) == c["DEF_DRI_PER_AGENT"], f"agent {a}: not two lines"
    say = rival_texts()
    for k in range(n):
        for part in ("", "Hello", "Won", "Lost"):
            assert f"DriRival{k}{part}" in say, f"tools/text_override/en.txt has no $DriRival{k}{part}"
    out = []
    for k, line in enumerate(LINES):
        looks = [s[0] if isinstance(s, tuple) else s for s in line["stages"]]
        bases = [s[1] if isinstance(s, tuple) else s for s in line["stages"]]
        stages = set(looks) | set(bases)
        assert len(looks) == c["DEF_DRI_STAGES"] and all(b in names for b in bases), line
        assert all(lk == b or (lk in FORMS and lk not in names) for lk, b in zip(looks, bases)), line
        assert len(line["rewards"]) == c["DEF_DRI_TIERS"], line
        for item in line["rewards"]:
            assert its.get(item, {}).get("cat") == "RECORDS_ITEM_ACCESSORY", (line, item)
        rank = line["rank"]
        first = c["DEF_DRI_LEVEL_FIRST"] + rank * c["DEF_DRI_LEVEL_GAP"]  # the Champion's
        rookies = c["DEF_DRI_STAGES"] - c["DEF_DRI_TIERS"]  # stages before the Champion: the Rookie
        levels = [first + (i - rookies) * c["DEF_DRI_LEVEL_STEP"] for i in range(c["DEF_DRI_STAGES"])]
        st = [dict(stage_of(b, lv, rows, battles, c), digimon=lk, base=0 if lk == b else b)
              for lk, b, lv in zip(looks, bases, levels)]
        for i, x in enumerate(st):
            if x["base"]:
                x["mp"] = FORM_MP
            if i > rookies:
                grown = st[i - 1]["hp"] * (x["level"] / st[i - 1]["level"]) ** (c["DEF_DRI_SCALE_EXP_PCT"] / 100)
                x["hp"] = min(c["DEF_HUNT_HP_MAX"], max(x["hp"], int(grown + 0.5)))
        top = levels[-1]
        cand = [i for i, b in enumerate(battles)
                if b["kind"] == 2 and b["escape"] and b["rows"] and i not in SPECIAL
                and not any(rows[r][0] in stages or rows[r][0] in FINAL_BOSS for r in b["rows"])]
        battle = min(cand, key=lambda i: (abs(max(rows[r][1] for r in battles[i]["rows"]) - top), i))
        fits = [m for m, t in tops.items() if t <= first]
        common = max(fits) if fits else 0
        common_n = round5(c["DEF_DRI_COMMON_LO"] + (c["DEF_DRI_COMMON_HI"] - c["DEF_DRI_COMMON_LO"]) * rank // max(1, n - 1))
        pool = sorted((t for t in targets if t["level"] <= first + c["DEF_DRI_RARE_OVER"]),
                      key=lambda t: (t["lead"] not in stages, -t["level"], t["variant"], t["index"]))
        picks = pool[:1]
        if rank >= c["DEF_DRI_TWO_RARES"] and pool:
            other = [t for t in pool[1:] if t["lead"] != pool[0]["lead"]] or pool[1:]
            picks += other[:1]
        assert picks, f"no rare item for challenge {k}"
        out.append(dict(k=k, line=line, stages=st, battle=battle, common=common, common_n=common_n,
                        rares=[(t["index"], 1) for t in picks], rare_info=picks))
    return out, names, its, rows, battles


def stage_name(s, names):
    """A stage's name: the species', or the player form's with the base it fights as."""
    return f"{FORMS[s['digimon']]} (as {names[s['base']]})" if s["base"] else names[s["digimon"]]


def rare_name(t, names):
    return f"{names.get(t['lead'], t['lead'])} Core" + (f" {roman(t['variant'])}" if t["variant"] else "")


def c_text(rows_, names):
    lines = ["/* Generated by tools/gen_dri_challenges.py: the DRI Agents' Gauntlet, an evolution line's",
             " * four stages in one battle (the first three its enemy members, the Mega a wave), the battle it is set up as,",
             " * the Synergy accessory of each tier, and what unlocking it takes; then the DRI Agents. Regenerate instead of",
             " * editing; review the list. */",
             '#include "definitive.h"', "",
             "const DefDriChallenge def_dri_challenges[] = {"]
    for r in rows_:
        st = r["stages"]
        desc = ", ".join(f"{stage_name(s, names)} Lv{s['level']}" for s in st)
        lines.append(f"    /* {r['k']} {r['line']['name']} line (rank {r['line']['rank']}), {r['line']['family']}: {desc} */")
        stages = ", ".join(f"{{ {s['digimon']}, {s['level']}, {s['hp']}, {s['mp']}, {s['stat_scale']}, {s['base']} }}"
                           for s in st)
        rare = r["rares"] + [(-1, 0)] * (2 - len(r["rares"]))
        lines.append(f"    {{ {{ {stages} }}, {r['battle']}, {{ {', '.join(str(x) for x in r['line']['rewards'])} }}, "
                     f"{r['common']}, {r['common_n']}, {{ {rare[0][0]}, {rare[1][0]} }}, {{ {rare[0][1]}, {rare[1][1]} }} }},")
    lines += ["};", f"const int def_dri_challenge_count = {len(rows_)};", "",
              "/* (map, actor ID; map -1: any map) and the challenges he offers, easier first. */",
              "const DefDriAgent def_dri_agents[] = {"]
    for i, (m, a, who, _rookie) in enumerate(AGENTS):
        mine = sorted((r for r in rows_ if r["line"]["agent"] == i), key=lambda r: r["line"]["rank"])
        lines.append(f"    {{ {m if m < 0 else hex(m).upper().replace('0X', '0x')}, {a}, "
                     f"{{ {', '.join(str(r['k']) for r in mine)} }} }}, /* {who} */")
    if not AGENTS:
        lines.append("    { 0, 0 }, /* none yet (a placeholder; the count is 0): tools/find_dri_agents.py on the lab */")
    lines += ["};", f"const int def_dri_agent_count = {len(AGENTS)};", ""]
    return "\n".join(lines)


def write_md(rows_, names, its, battles, rows):
    mats = material_names()
    out = ["# Desafios do Gauntlet DRI", "",
           "Cada desafio é uma batalha só com os quatro estágios da linha, do Rookie ao Mega, em ordem: quando um cai,",
           "o próximo entra com o HP cheio e a party continua como estava (o Mega entra no lugar de um caído). Nível do Champion: "
           "`DEF_DRI_LEVEL_FIRST` + rank × `DEF_DRI_LEVEL_GAP` (rank: o lugar na dificuldade); cada estágio `DEF_DRI_LEVEL_STEP` acima do anterior.",
           "HP e `stat_scale` vêm da linha da própria espécie na tabela de batalhas mais perto do nível, escalados pela",
           "razão dos níveis. Recompensa: o acessório elemental da família, tier n no n-ésimo estágio derrubado depois do Rookie, uma vez.",
           "", "| # | rank | agente | linha | estágios (nível, HP, escala) | batalha-modelo (XP, Bits da vitória) | destravar | "
           "recompensas | nota |",
           "| --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    rw = (ROOT / "src/stfgtrep/stfgtrep_80082E70.c").read_text()
    rw = rw[rw.index("StfgtrepReward stfgtrep_rewards[] = {"):]
    rewards = [tuple(int(x) for x in m) for m in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", rw[:rw.index("\n};")])]
    for r in sorted(rows_, key=lambda r: r["line"]["rank"]):
        st = "; ".join(f"{stage_name(s, names)} Lv{s['level']} HP{s['hp']} x{s['stat_scale']}/16" for s in r["stages"])
        b = r["battle"]
        who = ", ".join(f"{names.get(rows[i][0], rows[i][0])} Lv{rows[i][1]}" for i in battles[b]["rows"])
        cost = f"{mats.get(r['common'], r['common'])} ×{r['common_n']}, " + ", ".join(
            f"{rare_name(t, names)} ×1" for t in r["rare_info"])
        rew = ", ".join(its[i]["name"] for i in r["line"]["rewards"])
        tamer = rival_texts().get(f"DriRival{r['k']}", "?")
        out.append(f"| {r['k']} | {r['line']['rank']} | {AGENTS[r['line']['agent']][2]} | {r['line']['name']} "
                   f"({r['line']['family']}; Tamer: {tamer}) | {st} | {b}: {who} "
                   f"({rewards[b][1]} XP, {rewards[b][2]} Bits) | {cost} | {rew} | {r['line']['note']} |")
    out += ["", "DRI Agents: " + (", ".join(f"{who} mapa 0x{m:X} ator {a}" if m >= 0 else f"{who} ator {a} (qualquer mapa)"
                                         for m, a, who, _r in AGENTS) or "nenhum ainda (rodar tools/find_dri_agents.py no lab)")]
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "desafios.md").write_text("\n".join(out) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="exit 1 if the committed table is not the rule's (expected to differ: the table is hand-maintained)")
    a = ap.parse_args()
    rows_, names, its, rows, battles = table()
    text = c_text(rows_, names)
    if a.check:
        ok = GEN.exists() and GEN.read_text() == text
        print(f"{'ok' if ok else 'FAIL'} port/game/definitive_dri_table.c is the rule's ({len(rows_)} challenges, "
              f"{len(AGENTS)} DRI Agents)")
        return 0 if ok else 1
    write_md(rows_, names, its, battles, rows)
    print(f"written {OUT / 'desafios.md'} ({len(rows_)} challenges); {GEN} is hand-maintained (LEGACY), untouched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
