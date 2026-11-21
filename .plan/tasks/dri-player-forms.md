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
rows (`records_techniques[id - 1]`: the table is indexed by id minus one): Agunimon 0x1B0 (80 MP, power 160), BladeGarurumon
0x1B1 (80 MP, 160), AncientGreymon 0x1B2 (480 MP, 800, kind 26). Paildramon 0x176 (100, 500), MegaGargomon 0x178 (160, 360),
Omnimon 0x179 (240, 850), Imperialdramon PM 0x17A (250, 580), Beelzemon 0x17B (320, 600), Diabolomon 0x17C (260, 999); Phoenixmon
0x177 is a target-4 effect (power 0, kind 36), not damage.
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


## Session notes, 2026-11-21 (testing, signature techniques)

### Test harness (committed: tools/dri_battle_record.py)
- `python3 tools/dri_battle_record.py N --frames 3600 --every 3 --god --name NAME` runs challenge N headless from
  `.dumps/dri/field.state` (regenerate it after every build, see "Testing without going through the story"), mashes
  CROSS (**the EU disc confirms with CROSS**), gives the party 30000 HP (`--god`: a 9999 hit no longer ends the show;
  a drop is logged as an enemy hit), records every Nth frame and encodes `.dumps/dri/NAME.mp4`.
- `agy -p "<question about the mp4's path>" --model gemini-4-hydrogen-medium --dangerously-skip-permissions` watches the
  video and answers. A 15 s clip took ~1 min; a 90 s, 16 MB clip passed 10 minutes: record **short** clips (<= 40 s).
- The test run does not isolate the player's config dir (`~/.local/share/dw2003`): the game ignores `DW3_CONFIG_DIR`.
  The debug pad speaks PS1 buttons, so the player's pad.map does not matter.

### Found
- `records_techniques` is indexed by **id - 1**.
- A form's `techniques[6]` is its signature (Nova Blast, Terra Force, Seven Heavens, Mega Crusher, Final Purification,
  Giant Missile...). `def_dri_record` already put it in `tech_3`, but an action plan only reaches `tech_3` with action type
  3, and the base's plan (Tyrannomon: three plain attacks) never does: the Greymon only ever said "Attack!" (seen on video).
- Fix (in the tree, `dri_form_techniques` in definitive_dri_screen.c): the plan is BK MegaGargomon's, `{3,18,3} {2,1,48}
  {1,0,0}`: signature every third turn, else tech_2 3 times in 8, else the attack; tech_2 is the form's strongest
  single-hit damaging technique of slots 1-5 (slot 4 is often a buff or a heal). **Confirmed on video** (agy, 90 s clip
  `ch3_plan2.mp4`, Greymon): 10 enemy turns = 3 plain "Attack!", 3 "Nova Blast!" (the signature) and 4 "Flame Lance!"
  (tech_2); the named moves play their own effect and camera cut; no garbled or black frames besides the battle-entry fade.
- Every signature is single-target damage except Phoenixmon's (target 4, kind 21), Gate of Destiny / Dimension Scissors /
  Mugenhadou style kinds (5, 6, 26...) that are untested from the enemy side.
- The first Greymon hits did 9999 (a party at level ~6 against 48): balance is still not measured.

### Open, in order
1. (Greymon done.) Check the signature casts (name in the message box, animation, camera) for every other form of the 16
   lines, grouped by line, short clips (<= 40 s).
2. Check that the odd kinds (Phoenixmon target 4, Dimension Scissors, Gate of Destiny, Mugenhadou) behave from the enemy side.
3. Write the 16 lines into `LINES` in tools/gen_dri_challenges.py (with the `base` of every P form), the levels (24 + 15 x 8
   passes 99), the rival texts, the rewards, the per-agent split (2 each).
4. The DNA bosses (Agunimon, BladeGarurumon, AncientGreymon): a record each, their DNA attack as the signature, model untested.

### Done later on 2026-11-21 (commit fdfea8b)
- The 16 lines are in `LINES` (tools/gen_dri_challenges.py) and the table. A line's **index** is its save slot (0..4 keep
  0.8.5's lines and their unlock/best state) and its `$DriRival<index>` texts; its **rank** (0..15) drives levels
  (`DEF_DRI_LEVEL_FIRST` 24 + rank x `DEF_DRI_LEVEL_GAP` 4, the last Mega at 90) and the unlock's materials. Each agent's own
  Rookie line ranks 0..7 (Ken 0, Yang 1, Yuji 2, Bob 3, Kazuki 4, Michael 5, Johann 6, Jimmy 7), his second line 8..15 in
  the same agent order (Beelzemon last).
- `DefDriAgent.challenges[DEF_DRI_PER_AGENT]` (generated, easier first); the list shows only the talking agent's two
  (`def_dri_agent_index`, `dri.list`); `def_dri_debug = 100` still lists all 16.
- Bases chosen by resistance: the base's strong (300) family is the form's own best resistance, and a similar body where
  one fits: Devimon/Gargoylemon, Myotismon/LadyDevimon, MaloMyotismon/VenomMyotismon, Kyubimon/Lynxmon,
  Sakuyamon/Puppetmon, Angewomon/Persiamon, Phoenixmon/Gryphonmon, Stingmon/Snimon, Imperialdramon-F/BK Imperialdramon,
  SkullGreymon/SkullMammothmon, Beelzemon/Ghoulmon, Dinohumon/Divermon, Marsmon/Zanbamon, MetalGarurumon/HiAndromon,
  Grizzmon/Monzaemon, Cannondramon/Machinedramon, GranKuwagamon/H-Kabuterimon, Hookmon/Cyclonemon, GuardiAngemon/Vikemon.
  Line 9 starts with plain Greymon until the BlueGreymon recolour exists.
- Generator rules added: a player form's stage has MP 9999 (an enemy pays its techniques' MP; Persiamon's row had 40, less
  than Celestial Arrow's 44); a stage's HP is never below the previous stage's grown by the level ratio (the DDNA Ultimates
  are boss rows, a mob-based Mega came in frailer).
- Rewards: the 7 Power families shared, by the Mega's element: Fire 3/9, Water 7/15, Ice 6/12, Wind 2/11, Bolt 1/8/14,
  Metal 0/13, Dark 4/5/10.
- 11 new Tamers in 6 languages (Oikawa, Rika, Kari, Ken, BlackWarGreymon, Impmon, a kendo master, Matt, a fighter, a
  forest hunter, a pirate captain; titles only, no names); Rival0 no longer says "Momentai!" himself.
- tests/port/definitive_dri.py passes again (its driver now stubs `records_language`, which the language switch reads).
- The recorder (tools/dri_battle_record.py): the party fights with CIRCLE (Fight's default place, game_mods.c) and CROSS
  now and then (the battle messages' confirm; on the menu it is Tag, so rarely); `--turns T` leaves a stage at 1 HP after
  T turns so every stage is seen, each in its own clip; the battle log (`DW3_BATTLE_LOG`) gives every enemy action's
  technique; `HOME` is `.dumps/dri/home`, so the player's cards and settings are never touched.

### Bug reported by the owner (PT-BR, 2026-11-21): fixed in pt.txt (STATUS 1 `NV`, 11 `CARTAS`, 19 `△: Fechar Menu`, `$GuideMenu` `GUIA`), to be seen in game
The field menu in Portuguese: the level label in the party panel is garbled (reads like "N(M O" where it should say NIVEL),
the top bar still says "B: Fechar Status" while the main menu is open, and "CARD FOLDERS" and "GUIDE" are untranslated.
Screenshot was in the chat only. Start at the PT label table (tools/text_override/pt.txt) and the menu's font glyph for the
accented I.

### All 16 recorded and reviewed (2026-11-21, evening)
`.dumps/dri/r<challenge>*.mp4` (one clip per stage), `r<c>.out`, `r<c>.battle.log`; the video reviews (agy, Gemini 4
Hydrogen) in `.dumps/dri/review/`.
- Clean (model, HUD name, the signature cast by name with its own animation and camera, no black or garbled frame):
  Dinohumon (Lizard Dance), Marsmon (Mugenhadou: puts to Sleep, no damage), Cannondramon (DynamoCannon), Hookmon
  (Captain Cannon), GuardiAngemon (Golden Ripper), ExVeemon (Veelaser), Angemon (Hand of Fate), Kyubimon (Dragon Wheel).
- Grizzmon's Maul Attack is kind 10, a counter stance: the name shows, no animation, and it strikes only when the party
  attacks it. As the technique is; not a bug.
- Instant KO: the disc's own MagnaAngemon (Gate of Destiny) and GranKuwagamon's Dimension Scissors knock the party's
  member out; the game then waits on the switch menu (CROSS). The recorder now presses CROSS after 10 s with no action.
- Recorder fixes: a stage left at 1 HP is put back to 1 if it heals, and the party gets +20000 to the hit stat
  (modifiers[2]) once the stage's turns are seen (the party's hit roll has no floor, so a level-6 party never hit a
  level-40 stage); `--god`'s 30000 HP overflows the HUD's digits ("3000020000"): a harness artefact.

## Owner's roadmap (2026-11-21, evening; left for the night)
In his words, translated: the DRI was meant to be **4 duels, starting at the Rookie**. Later, an **item** that forces a run
of battles in the region, about 10 in sequence, the last one the region's rare Digimon, to speed the hunt up. So a
sequence system past 3 helps both.
- **Option A (his wish, try it seriously before anything else):** in the battle itself, after the 3rd enemy falls, the
  next 3 come in, in sequence.
- **Option B (only if A is truly blocked):** instead of the XP screen, a screen like it (copy it) that says another fight
  follows; in the DRI the Mega would then come alone in a 2nd battle, and the transition can say the Digimon is unleashing
  a super power.
- **The item:** sold in the common item shops for about 1000 Bits; it starts 10 duels and the 10th is surely the region's
  rare (its Rare Target) if it has one; with none, a text box says there is no rare Digimon in the region.

Status: a research agent is reading the battle code and prototyping A in a worktree (report: `.dumps/tasks/dri-waves.md`).

### Proposal for the Rookie stage of each line (for the owner's review)
P = the player's own Rookie form on an enemy base (as the Champions), E = the disc's enemy species.

| # | line (agent) | Rookie | why |
|---|---|---|---|
| 0 | Terriermon / Guardromon (Yang) | E Hagurumon | no Terriermon anywhere; Hagurumon -> Guardromon is the classic line |
| 1 | Veemon (Bob) | P Veemon | |
| 2 | Patamon (Kazuki) | P Patamon | |
| 3 | Agumon (Johann) | P Agumon | |
| 4 | Guilmon (Jimmy) | P Guilmon | |
| 5 | Devimon (Kazuki) | E DemiDevimon | Myotismon's servant |
| 6 | Renamon (Michael) | P Renamon | |
| 7 | Angewomon (Michael) | P Patamon? | no Salamon/Gatomon; **to choose** |
| 8 | Wormmon / Stingmon (Bob) | E Kunemon | no Wormmon; the larva Rookie |
| 9 | Agumon, dark (Johann) | P Agumon | same as line 3 (a dark recolour later, as BlueGreymon) |
| 10 | Impmon / Beelzemon (Jimmy) | E Goburimon? | no Impmon; an imp-like Rookie; **to choose** (or DemiDevimon) |
| 11 | Kotemon (Ken) | P Kotemon | |
| 12 | Musyamon (Ken) | P Kotemon? | the swordsman's Rookie; **to choose** |
| 13 | Kumamon (Yuji) | P Kumamon | |
| 14 | Kuwagamon (Yuji) | E Kunemon? | no Tentomon; **to choose** |
| 15 | Monmon (Yang) | P Monmon | |
