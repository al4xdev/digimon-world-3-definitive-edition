# Forge variety: every material used, new trade-off equipment

Status (2026-11-17, night):

- **Part 1: done** (`89c46af`): the rule below is in `tools/gen_recipes.py` (`MIN_POOL`, `REACH_MAX`, `WINDOW`,
  `SPREAD`, `SECOND`), with its three checks in `--check`. Seen in game: shop 13's Shine Blade asks Chrome Digizoid,
  shop 28's Tenjinmaru Obsidian Digizoid. The README line waits for the shared README to be free.
- Part 2 needs the texts (`tools/text_override/*.txt`, `tools/gen_text_override.py`, the generated
  `definitive_text*.{c,inc}`). Those belong to the story session until it says its types are done. Part 2 also
  creates items, so the owner approves the list first (PRD rule).

Measured with `tools/gen_recipes.py` (317 recipes, 18 shops) and `tools/gen_equipment.py` (27 variants). The
prototype of Part 1 and the in-game scripts are in `.dumps/tasks/forge/`:

- `proto.py K W`: the new rule's result;
- `dist.py`: where the variants are and how much each material is asked;
- `cand.py`: the free items per tier;
- `boot_entry.py`, `shots.py`, `buy.py`: shop states, shots and a purchase;
- `bmshot2.py`: the Black Market.

The rule is also written out below.

## Where things are today

### The 27 variants sit in the old shops

There is no free item ID, so each variant is an old shop item rebuilt in place (same ID, icon, price, slot,
wearers). There is one per archetype in every weapon/armour goods list (a "tier"), spread over every town:

| tier | shops (town) | Speed (weapon) | Elemental (head) | Tank (body/shield) |
| --- | --- | --- | --- | --- |
| 0 | 0 Amaterasu | Breeze Spear (150) | Ember Band, Fire (40) | Padded Coat (70) |
| 1 | 5 Qing Long | Swift Pike (400) | Frost Visor, Water+Ice (100) | Quilted Suit (160) |
| 2 | 7 Zhu Que | Quick Sword (1000) | Gust Helmet, Wind (550) | Heavy Shield (550) |
| 3 | 10 Mirage Tower, 11 Bai Hu | Gale Saber (2500) | Volt Crown, Thunder+Machine (1100) | Bulwark Suit (1300) |
| 4 | 3 Amaterasu, 20 Qing Long | Zephyr Sword (5000) | Tide Cap, Water+Ice (900) | Rampart Mail (3800) |
| 5 | 13 Xuan Wu, 22 Zhu Que | Rapid Edge (9500) | Circuit Helm, Thunder+Machine (4200) | Tower Shield (4200) |
| 6 | 25 Mirage Tower | Hayatemaru (15000) | Shade Goggles, Dark (2800) | Fortress Vest (8500) |
| 7 | 26 Bai Hu, 28 Xuan Wu | Sonic Blade (22000) | Blaze Crown, Fire (6800) | Aegis Wall (7500) |
| 8 | 15 Amaterasu | Flash Sword (30000) | Tempest Crown, Wind (9000) | Titan Shield (11000) |

Shops with no variant:

- 1, 8, 16 and 23 sell only accessories:
  - 1, 8 and 23 share the Gems list;
  - 16 has the Rings list.
- 18 is the per-Digimon set shop, excluded by design.

### Gaps

1. **Two materials are never asked:** Mega Crystal and Obsidian Digizoid. Three more are rare:
   - Chrome Digizoid: 1 recipe.
   - Giga Crystal: 4.
   - Black Digizoid: 6.

   The others appear in 13 to 40 recipes. The owner wants all materials used.
   - **Mega Crystal (mid level 32)** drops in low regions. It is in the pools of shops 0, 1, 5, 7, 8 and 20, but
     "nearest to the item's level" always prefers the Shards or Chips there.
   - **Obsidian (mid level 52.5)** drops only in Kicking Forest and Kulon Pit. No shop has them within
     `DEF_RECIPE_REACH` (4 exits).
2. **Some shops ask one material for everything.**
   - Shop 13 (Xuan Wu) asks Peta Prism for all 18 items.
   - Shops 3, 10, 11, 22, 23, 25 and 28 have 2 materials in reach and use them unevenly.
3. **Shop 23 (Zhu Que, the accessory shop on the second server) is out of level.**
   - Its cheap rings (300 to 1500 Bits, level 8 to 17 by price) ask Giga Crystal and Tera Prism (mid level about
     36), because no region near it is low.
   - This is acceptable: the player only reaches it late, on the second server.
   - It is the one shop where the price-to-level curve is wrong. Leave it, and exclude it from the level-gap check.
4. **No accessory variants,** so the accessory shops have no trade-offs.
5. **The shop line lost the slot text.** The requirement replaced "Right Hand" / "Both Hands" on the description
   page. The owner keeps it so (2026-11-17): the icon shows the kind, the Triangle page who can wear it.

## Part 1: every material in some recipe (no texts, no new items)

All of this goes in `tools/gen_recipes.py`, then regenerate `port/game/definitive_recipes.c`. The rule below was
measured in the prototype.

### The new pick

1. **Pool.** A shop whose pool has fewer than 3 commons widens its reach one exit at a time, up to 7. It keeps the
   wider reach only where that adds a common (the level filter `DEF_RECIPE_OVER` stays).
   - This changes only shops 3 (reach 5), 11 (5), 13 (6) and 28 (6).
   - The rare items stay dealt from the reach-4 regions, as today.
2. **Pick.** Walk the shop's equipment cheapest first. For each item:
   - The candidates are the pool's commons whose mid level is at most `W` = 8 levels further than the nearest one.
   - Score each candidate as `|mid level - item level| + K * (times this shop already asked it)`, with `K` = 2.
   - A weapon takes the best score.
   - Armour and accessories take the second best when it is within 2 points of the best. This keeps today's split,
     where armour asks the neighbour material.
3. **Safety net (not triggered with K = 2, W = 8).** A common still asked by no recipe goes to the shop of its
   regions' server whose dearest item's level is nearest the material's mid level, as the common of that shop's
   three dearest items.

### Measured result

All 16 commons are asked, and every shop asks at least 2:

| material | today | new |
| --- | --- | --- |
| Bit Shard | 13 | 8 |
| Byte Shard | 40 | 22 |
| Data Chip | 22 | 32 |
| Code Chip | 38 | 44 |
| Core Fragment | 29 | 20 |
| Core Crystal | 17 | 20 |
| Mega Crystal | 0 | 9 |
| Giga Crystal | 4 | 21 |
| Tera Prism | 32 | 20 |
| Peta Prism | 18 | 7 |
| Chrome Digizoid | 1 | 11 |
| Blue Digizoid | 28 | 32 |
| Red Digizoid | 38 | 37 |
| Gold Digizoid | 31 | 23 |
| Black Digizoid | 6 | 7 |
| Obsidian Digizoid | 0 | 4 |

Shops that change most:

- 13: Peta Prism 7, Chrome Digizoid 6, Blue Digizoid 5 (was Peta Prism 18).
- 28: Gold Digizoid 7, Black Digizoid 7, Obsidian Digizoid 4.
- 20: Code Chip, Core Fragment, Mega Crystal, Giga Crystal and Tera Prism.
- 3: Core Crystal 8, Mega Crystal 5.

The worst level gap outside shop 23 is 24.4 (Kung Fu Cap asking Peta Prism). Today it is 24.9, so no item gets a
material further from its level than today.

### Checks and tests

- Add to `gen_recipes.py --check` and `tests/port/definitive_economy.py`:
  - every one of the 16 commons is asked by at least one recipe;
  - no shop asks a single common for all its items;
  - outside shop 23, no recipe's level gap is over 25.
- Tests:
  - `uv run --no-project python tests/port/definitive_economy.py`, and the buffs, market and savestate tests (the
    recipe table feeds the savestate).
- In game:
  - Shots of shops 13, 20 and 28: `boot_entry.py F00 100 100 1 OUT` with `ENTRY=n`, then `shots.py`.
  - A purchase in one of them, with `buy.py`.
- Docs:
  - README, "Shops ask for materials".
  - `.dumps/tasks/equipment/receitas.md`, written by `gen_recipes.py`.

Old saves are not affected: the recipes are a table in code, and the save holds only the materials.

## Part 2: new trade-off equipment (needs texts and the owner's OK)

The mechanism is `gen_equipment.py`, with the same rules as today: in-place conversion, a budget equal to the tier's
best plain item, and `--check` refusing straight upgrades. A new archetype adds:

- a `KINDS` entry;
- a `candidate()` rule;
- a `build()` branch;
- `ITMNAM`/`ITMINF` lines in `en.txt` (description at most 34 characters).

### A. Elemental weapon (recommended)

- **What it is:** a one-handed weapon that gives up power (`DEF_EQUIP_ELEM_COST`, 50% like the heads) for one
  family's resists. In this edition the resists are the player's affinity, which raises WEAK! hits and the hit on a
  staggered target.
- **The trade-off against the heads:** the head version costs guard, the weapon version costs damage on neutral
  hits.
- **Selection:** one per tier, 9 items, plus a second in tier 8. Rotate the wearer group so every rookie gets one,
  and rotate the family (Fire, Water+Ice, Wind, Thunder+Machine, Dark).
- **Candidates:** plain items with no bonus, no ID effect, no strong type, not in a set, not yet a variant.

  | tier | candidate (wearers) | family |
  | --- | --- | --- |
  | 0 | Dagger (all) | Fire |
  | 1 | Shishioumaru (all) | Water+Ice |
  | 2 | Fairy Tale (Renamon, Patamon) | Wind |
  | 3 | Wolf Fang (Agumon, Veemon, Guilmon) | Thunder+Machine |
  | 4 | Shotgun (Monmon) | Dark |
  | 5 | Tiger Glove (Kumamon) | Fire |
  | 6 | Scissor Horn (Agumon, Veemon, Guilmon) | Water+Ice |
  | 7 | Royal Spear (Guilmon, Renamon, Patamon) | Wind |
  | 8 | Raijin Fist (Kumamon), keep name, new description | Thunder+Machine |
  | 8 | Suijin Cannon (Monmon), keep name, new description | Water+Ice |

  Raijin and Suijin are the thunder and water gods, so their names already fit.
- **Kotemon:** his free swords in tiers 7 and 8 are Tenjinmaru ("the highest rank katana") and Fuujin Blade, which
  already has Guard and Boost. Both stay as they are. He gets his element from the all-wearer Dagger and
  Shishioumaru.
- **Stacking with an Elemental head, resolved:** allow it. `def_affinity` caps each resist at 999 before turning it
  into affinity, at most +0.30 for Fire or Wind and +0.60 for the pairs and Dark (`DEF_AFF_*_MAX`). Stacking head and
  weapon only reaches that ceiling sooner, and paying with both guard and power is the trade-off. No new cap is
  needed.
- **Engine work:** none. `RecordsWeapon` has two `bonus_stats`, already written by `def_variants_hook`.

### B. Accessory trade-offs (accessory shops 1/8/23 and 16)

- **The pieces:**

  | piece | gain | penalty | list |
  | --- | --- | --- | --- |
  | Fury Gem | Power | Guard | Gems list |
  | Sage Gem | Wisdom | Boost (Wisdom drives the support buffs, PRD 12) | Gems list |
  | 2 Rings | the same, at higher values | | shop 16's Rings list |

- **Leave out:**
  - the status rings (Antidote, Revive, Awake, Sober, Prayer), which FIGHTSTG knows by ID;
  - the elemental rings, which are plain resists.
- **Engine work, measured:**
  - `RecordsAccessory` has one `bonus_stat`/`bonus_value` and 3 unused bytes (`unk_09`), so the penalty needs a
    port hook.
  - It must go in the two places that add an accessory's bonus:
    - `src/main/gamestate.c:928`, the party's stats in and out of battle;
    - `src/stitshop/stitshop_800859C0.c:972`, the shop's preview.
  - In both, under `PC_PORT`, add `def_accessory_extra(item, &stat, &value)` from a small generated table. The
    item ID is not in scope at :928 today, but `items[j]` is.
  - The FIGHTSTG read at `fightstg_8008D3B4.c:9689` is a crest's MP cost, not a stat, so it needs nothing.
  - Check that the preview's stat page shows the penalty in red, as for the Tank pieces.
- **Without the hook:** a single bigger stat is a straight upgrade, which the rules forbid. So B is
  hook-or-nothing.

### C. Not proposed

- **Two-handed "berserk" weapons:** the game's two-handers already trade Boost for power.
- **Wisdom rods:** rods already carry Spirit.

## Order of work

1. Part 1 (recipes), with its checks, tests and in-game shots. It does not wait for the texts.
2. The owner picks from Part 2 (A recommended; B costs two small engine hooks).
3. Part 2, after the story session says its types are done:
   - `gen_equipment.py`: the new kinds, then `--check`.
   - `en.txt`: names and descriptions.
   - `gen_text_override.py`.
   - `gen_recipes.py`, for the `.md`.
   - The tests `definitive_variants.py`, `definitive_economy.py` and `text_override.py`, then shop shots in game.
4. README "Equipment variants": add the new archetype and its list.

## Other open work of this session (outside this plan)

- **Commit:** the regional forge, the replica Cores, the Black Market UI (`.plan/tasks/black-market-ux.md`) and the
  super rare hunts (`.plan/tasks/super-rare-hunts.md`) are done and tested but not committed.
  - Several files hold the story session's changes too (`definitive.h`, `port.h`, `definitive.c`, `en.txt`, the
    generated texts, `definitive_buffs_driver.c`). The commit waits for that session, or goes together with it.
  - Before committing, run the PRD's definition of done: `tests/port/definitive*.py` (all pass today),
    `tests/port/text_override.py`, `tests/port/run.py` and `scripts/test.sh`.
- **PRD 4, open decision 1, answered by the owner (2026-11-17):** a boss may be the Rare Target of several regions,
  with a different colour in each.
  - Today's table (`definitive_hunt_targets.c`, 71 regions) has 71 distinct bosses: the "Datamon covers 13" note
    is from the old table, so nothing repeats now.
  - If a boss ever repeats, `definitive_recolor.c` must turn the hue per region instead of the fixed 120 degrees of
    every regional Rare: for example the target's index times a step, skipping the Super Rare's 200.
- **Igor's head:** handed to the Velvet session.
- **Not ours:** the menu sound volume (the owner), `.dumps/handoff_adventure_music.md`, and the story session's files
  (`definitive_story*.c`, `en_story.txt`, `en_choices.txt`, `check_story_text.py`).
