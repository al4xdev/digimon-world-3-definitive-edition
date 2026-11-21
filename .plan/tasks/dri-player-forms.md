# DRI Gauntlet: player forms as enemies (handoff, 2026-11-19)

## The owner's request

The DRI lines should show the **canonical** evolutions. The game has them as **player forms** (records_digimon):
Greymon, ExVeemon, Angemon, Growlmon, Gallantmon, and so on. The disc has no enemy record for them. That was the
original idea: take the player's forms and put them in as enemies. The previous agent used enemy species only, as
stand-ins. Also: in the Terriermon line, Andromon becomes **Antylamon** (Andiramon).

## Done so far (uncommitted, builds, `tests/port/definitive_dri.py` passes)

- `DefDriStage` has a new field, `short base` (port/game/definitive.h):
  - 0 means `digimon` is an ordinary enemy species.
  - Otherwise `digimon` is a player form, and `base` is the enemy species that lends it its record (stats,
    resistances, battle plan) and its level rows (HP and stat_scale in the generator).
- `tools/gen_dri_challenges.py`:
  - `FORMS` (player-form IDs and their names).
  - Stages written as `(form, base)`.
  - `stage_name`.
  - New LINES:
    - Terriermon: Guardromon 435 → Antylamon 434 → (MegaGargomon 372, base BK MegaGargomon 443)
    - Veemon: (ExVeemon 259, base Devidramon 135) → Paildramon 450 → (Imperialdramon 148, base BK Imperialdramon 444)
    - Patamon: (Angemon 20, base Airdramon 7) → MagnaAngemon 452 → (Seraphimon 214, base BK Seraphimon 447)
    - Agumon: (Greymon 5, base Tyrannomon 8) → MetalGreymon 449 → (WarGreymon 213, base BK WarGreymon 448)
    - Guilmon: (Growlmon 367, base Flarerizamon 69) → WarGrowlmon 451 → (Gallantmon 369, base BK WarGrowlmon 445)
  - port/game/definitive_dri_table.c regenerated; `--check` is ok.
- `def_dri_record(id)` (port/game/definitive_dri_screen.c):
  - Copies the base's disc record and applies `def_enemy_override`.
  - Then takes the form's name, type and techniques: techniques[0], [4] (or the last one it has), and [6], picked as
    `def_purify_record` picks them. Item set to 0.
  - Hooked into `fightstg_enemy_get_record` after `def_purify_record` (src/fightstg/fightstg_8008D3B4.c ~7322;
    declared in include/port.h).
- `def_species_name` (port/game/definitive.c): when the enemy table has no row, falls back to
  `records_get_digimon(...)->name_id`. The DRI list now shows "Greymon - MetalGreymon - WarGreymon" and so on.
- **Black-screen bug fixed:**
  - Cause: enemy cameras read the model as a **type B** record (`fightstg_camera_get_preset` slot&0xF0,
    `fightstg_camera_get_default` → `record_b->default_camera`). Player forms are **type A** (12 cameras, a different
    layout). Read as B, they give a garbage camera and the whole 3D scene goes black.
  - Fix: `def_enemy_camera_model(id)` returns the stage's `base`. The model stays the form's; the cameras come from
    the base.
- Same field added in tools/gen_time_room.py → definitive_time_room_table.c (`, 0`) and in
  tests/port/definitive_dri_driver.c.
- tests/port/definitive_dri.py accepts stages with a base.

## Seen in game (challenge 3, `def_dri_debug = 3` on .dumps/field21d.state)

- The battle opens with Greymon as the enemy (name and model correct). The scene renders after the camera fix, and
  Greymon attacks and evades normally.
- **The model looks small.** The player-form model is at party scale, framed by Tyrannomon's camera.
- **Greymon does > 9999 damage per hit** against a party at level ~6. That is expected given the levels (48 against 6),
  but **balance has not been measured**. The record in memory (`made.0`) has tech 12 / tech_2 12 / tech_3 138
  (Greymon's techniques) and stats 64 110 138 112 94 (Tyrannomon's).
- The **DIGIVOLVE between stages** into a player form works, per the owner: another session implemented and tested it,
  because the final boss fight (definitive_finale.c) uses it. Before touching `fightstg_digivolve_create_enemy`, read
  how the finale does it and reuse that.
- Not tested yet: the other lines and the Megas in this session.

## Owner's decision (2026-11-19): 16 simulations, spread across the agents

Today every one of the 8 DRI Agents opens the same list of 5. The new plan is to fill the card record's **16** slots
(`DEF_DRI_CHALLENGES`) and spread them out: **2 per agent**, so no single DRI holds a pile of them. **The owner will
plan every evolution line on Saturday, calmly.** After that, the job is the signature technique and the Digimon on the
enemy side (item 0 below).

What the code needs for this (not done):
- Agent → challenges: `DefDriAgent` (or a table next to it) gets the agent's challenge indices; `list_open`/`list_fill`
  in definitive_dri_screen.c show only those. The `def_dri_debug` 100 list can stay showing everything.
- **Levels:** `DEF_DRI_LEVEL_FIRST 24 + k * DEF_DRI_LEVEL_GAP 8` reaches 24 + 15 × 8 = 144 at challenge 15, past 99.
  Redistribute the levels (a smaller gap, or tiers per agent or per story progress).
- `DEF_DRI_RIVALS` ≥ 16 and texts `$DriRival{k}`, `Hello`, `Won`, `Lost` in en.txt and pt.txt for every new line
  (the generator asserts this).
- A family of accessories (rewards) per line: only the elemental "Power" sets exist today (Fire/Wind/Bolt/Metal/Dark
  …), so check whether there are enough for 16 or whether lines share sets.
- Unlock: the generator's rule (commons + Rare Targets by level) must cover the new levels.

### The 16 lines, as settled on 2026-11-21 (P = player form with a `base`, E = enemy species of the disc)

Checked against the data (DIGNAM has 250 names; records_digimon stats sums tell the tier: Champions are name ids 9-19
Angewomon included, Ultimates 20-35, Megas 36+):

| # | Agent | Line | State |
|---|---|---|---|
| 1 | Kazuki | Angemon P > MagnaAngemon E > Seraphimon P | built |
| 2 | Kazuki | Devimon P > Myotismon P > MaloMyotismon P | new |
| 3 | Michael | Kyubimon P > Taomon E > Sakuyamon P | new |
| 4 | Michael | Angewomon P > MagnaAngemon E > Phoenixmon P | new (Rosemon is the alternative Mega) |
| 5 | Bob | ExVeemon P > Paildramon E > Imperialdramon P | built |
| 6 | Bob | Stingmon P > Paildramon E > Imperialdramon-F P | new |
| 7 | Johann | Greymon P > MetalGreymon E > WarGreymon P | built |
| 8 | Johann | BlueGreymon (Greymon recoloured) > SkullGreymon P > BK WarGreymon E | new; needs a recolour hook and a name text |
| 9 | Jimmy | Growlmon P > WarGrowlmon E > Gallantmon P | built |
| 10 | Jimmy | Flarerizamon E > Megadramon E > Beelzemon P | new |
| 11 | Ken | Dinohumon P > Kyukimon E > Marsmon P | new |
| 12 | Ken | Musyamon E > Knightmon E > MetalGarurumon P | new |
| 13 | Yuji | Grizzmon P > GrapLeomon E > Cannondramon P | new |
| 14 | Yuji | Kuwagamon E > Okuwamon E > GranKuwagamon P | new |
| 15 | Yang | Hookmon P > Armormon E > GuardiAngemon P | new |
| 16 | Yang | Guardromon E > Antylamon E > MegaGargomon P | built (was the Terriermon line) |

Not usable: **Gatomon** is in no table (only the inn's NPC). **Agunimon (468), BladeGarurumon (469), AncientGreymon (470)**
are DNA fusion results: a `records_digimon` stub each (stats 1, no techniques, in no `blast_forms`), but the disc's model
table (file 0x1CC) has a type A model for each (records 0, 14, 11), and their DNA attacks are plain `records_techniques`
rows: Agunimon 0x1B0 (80 MP, power 160), BladeGarurumon 0x1B1 (480 MP, 800, kind 26), AncientGreymon 0x1B2 (0 MP, 700).
The pairs (`pair_technique`, offered when the partner named by `level_thresholds[6]` is switched in): Agunimon = Dinohumon +
Kyubimon, Hookmon + Greymon; BladeGarurumon = Grizzmon + Growlmon; AncientGreymon = Imperialdramon-P + Diaboromon. The other
DNA results are full forms (Paildramon 0x176 ExVeemon + Stingmon, Phoenixmon 0x177 Taomon + MagnaAngemon, MegaGargomon 0x178
WarGrowlmon + Digitamamon, Omnimon 0x179 WarGreymon + MetalGarurumon, Imperialdramon PM 0x17A Imperialdramon + Seraphimon,
Beelzemon 0x17B Gallantmon + MaloMyotismon, Diabolomon 0x17C BK WarGreymon + GranKuwagamon). Idea (2026-11-21): make the three
bosses with their DNA attack as signature; a record is needed (def_dri_record style), the model's mesh and animation are
untested. Out for now: MetalGarurumon is used (#12); Omnimon, Diaboromon, Rosemon, Imperialdramon-P.
Rejected from a review: Dinobeemon / BlackWarGrowlmon / Kyukimon "recolours" (a recolour keeps its model's name, and BK
WarGrowlmon already is the black one); Kabuterimon as a Champion (it is an Ultimate here).

Older draft, kept for the reasoning (a suggestion only: **the owner decides**):
- Kazuki (Patamon): Angemon P → MagnaAngemon E → Seraphimon P | the fallen path: Devimon P → Myotismon P →
  MaloMyotismon P
- Bob (Veemon): ExVeemon P → Paildramon E → Imperialdramon P | Stingmon P → Paildramon → Imperialdramon−F/−P P
- Johann (Agumon): Greymon P → MetalGreymon E → WarGreymon P | Greymon → SkullGreymon P → BK WarGreymon E, or
  Omnimon P
- Jimmy (Guilmon): Growlmon P → WarGrowlmon E → Gallantmon P | … → BK WarGrowlmon E / Beelzemon P
- Michael (Renamon): Kyubimon P → Taomon P → Sakuyamon P | Angewomon P → … → Rosemon P / Phoenixmon P
- Ken (Kotemon), Yuji (Kumamon), Yang (Monmon): Dinohumon / Grizzmon / Hookmon P → Kyukimon / GrapLeomon / Armormon P →
  Marsmon / Cannondramon / GuardiAngemon P (check which Mega belongs to which in each partner's own tree)
- Terriermon (Guardromon → Antylamon → MegaGargomon) has no agent: give it to one of them.
- Other P Megas still unused: MetalGarurumon, GranKuwagamon, Diaboromon, BladeGarurumon, AncientGreymon, Agunimon.
  Full list of forms: records_digimon IDs 1..55 (src/main/records.c; names from `tools/dump_text.py DIGNAM`).

## Known small model

Greymon (and other player forms) shows up small on the enemy side. The owner says this is a **classic bug of the
original game** (some forms really are small). It is not ours: **do not fix it** unless the owner asks.

## Open: next session

0. **Owner's decision (2026-11-19): each form fights with its own signature technique**, the one that was
   player-only until now: Greymon Mega Flame, ExVeemon Vee Laser, Angemon Hand of Fate, Growlmon Pyro Blaster,
   WarGreymon Terra Force, Imperialdramon Positron Laser, Seraphimon Seven Heavens, Gallantmon Royal Saber,
   MegaGargomon Mega Barrage, and so on. Take this seriously: no "whatever technique comes out" and no base species'
   technique.
   - Today `def_dri_record` takes `techniques[0]` (attack), `[4]` (or the last) and `[6]` (finisher) from the form.
     Confirm, form by form, that the signature (STFGTREP/SKLNAM; `tools/dump_text.py SKLNAM`) is in a slot the
     battle plan really uses (tech / tech_2 / tech_3), and that the plan fires it (actions' conditions,
     `counter_action`).
   - If needed, write a per-form table in the generator (signature technique ID → tech_3, plus a plan that uses it)
     instead of the generic choice.
   - Check that the player technique works when cast from the enemy side: animation, camera, effect, MP cost.
1. **The new mobs' AI** (raised by the owner).
   - The base's `actions[3]` / `counter_action` plan refers to slots tech / tech_2 / tech_3, which are now the form's
     techniques. Check that player techniques work as enemy actions (power, MP, animation with a type-A camera from
     the enemy side).
   - Perhaps write a plan per form, as `def_purify_record` does with BK WarGreymon's plan.
2. Test every form in the battle (the transition itself already works; see above): every form (ExVeemon, Angemon, Growlmon, MegaGargomon, Imperialdramon, Seraphimon,
   WarGreymon, Gallantmon), including technique animations (they may read A/B cameras from the enemy side as well).
3. Model scale: settled, nothing to do (the original game's bug; see "Known small model").
4. Balance with a party at the right level (a lv 48+ save), comparing a form's technique power with the base species'
   original technique.
5. **Purify** (`def_purify_record`) likely has the same black-camera bug when its forms are type A.
   `def_enemy_camera_model` covers the DRI only: consider making it generic (a type-A record in an enemy slot → a
   type-B stand-in).
6. Docs: the DRI table in the README in 6 languages (README*.md, "The DRI Gauntlet's five lines") and
   docs/definitive-edition/hunt-and-economy.md.
7. Rival text for the Terriermon line: "Momentai!" is Terriermon's catchphrase, not Henry's (rewrite it, or put it in
   the partner's mouth).

## Testing without going through the story

- Field state: `build/port/dw2003 --disc iso/dw2003.cue --cd-speed instant --script
  tests/replay/scripts/first_battle_save.json --save-state 38640:.dumps/field21d.state --save-state-exit`.
  Regenerate it after **every** build (a state only loads in the same binary).
- MCP:
  - `game_start --load-state .dumps/field21d.state`, step 300, then `def_dri_debug = N` starts challenge N.
    100 opens the list; `dri_unlocked` = 31 unlocks everything.
  - In battle: `fightstg_battle+0x70 + i*0x20` is enemy member i (digimon, …, +6 max_hp, +8 hp).
    The party starts at +0x10.
  - Battle messages wait for ○ (it is not a freeze).
