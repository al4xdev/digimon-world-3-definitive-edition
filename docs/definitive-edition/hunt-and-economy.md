# The hunt, the forge, the Black Market and the DRI Gauntlet

[Technical reference index](README.md) · [Player README](../../README.md)

## The hunt

Optional, and open all game long: every battle can drop Hunt Materials, and a winning streak in one region brings out
its rarest prey. The materials then pay for the forge, the Black Market and the DRI Gauntlet.

<p align="center"><img src="../images/definitive/hunt-chain.svg" width="800" alt="The hunt chain: each random battle won in a region adds one; past 5 wins the Rare Target may come out (20% at 6, certain at 10). It fights alone with its colours turned 120 degrees, and gives its region's rare Core, an R-Booster pack and 10x experience. The chain resets in another region, after a Guardromon teleport and after fighting the target."></p>

### The regional chain and the Rare Target

- Every random battle you win adds one to the **hunt chain** of the region you are in (an area of the world map; the
  server counts as its own). Moving to another region, a Guardromon teleport and fighting the Rare Target reset it.
  Battles you do not win do not count; a battle the Overlevel Rush skips **does** count and leaves materials, like a
  won one (`def_hunt_skipped`, called by `def_rush_results`). The chain is saved with your game.
- Past **5** wins, each random encounter in that region may be its **Rare Target** instead: a 20% chance at 6 wins,
  20% more per win, and **certain** at 10.
- The Rare Target is **one Digimon, alone** (no trios): one enemy of an existing battle of the region (a fixed battle
  and slot per region), fought on the region's own stage and music. Its level is the region's average encounter level
  **+ 5** (it can lower a strong boss in a weak place) and its HP is **x4** (`DEF_HUNT_RARE_HP_MULT`, capped at 30000).
  A **RARE!** callout announces it, its colours are turned 120 degrees on the colour wheel (see
  [Recolour](super-rare-hunts.md#recolour-palette-swap-of-rare-foes)), the Overlevel Rush never offers to skip it, and you can always run
  from it (except inside an active Super Rare territory, see [Super Rare hunts](super-rare-hunts.md#super-rare-hunts)).
- Every region with random encounters has one: **71 regions, 71 species, no repeats** (so the numeral below only
  appears if a species ever repeats). No Cardmon (card duels) and never Galacticmon (the final boss).

### Hunt Materials

There are two kinds, and they do different jobs:

- **Common materials** drop in random battles (and in the ones the Overlevel Rush skips): any common Digimon you
  defeat may leave one, and its level band decides which, from Bit Shard on the weakest early Digimon to the Digizoids
  late in the game. They pile up as counts.
- **Rare items** drop only from Rare Targets, one per region that has a Rare Target. Each is named
  "<species> Core <numeral>", the numeral only when the species repeats across regions. The Bestiary remembers every one you have ever found, so they are a collection to complete,
  region by region, even after you spend them.
- A Rare Target also drops one **R-Booster** pack, by the target's level, and pays far more experience than the
  encounter it replaced.
- **The Bestiary tells you where to hunt**: under each species it lists its common material with how many you hold,
  and its rare items as a collection.
- Rares are meant to be spent: the forge, the Black Market and the DRI Gauntlet below are where they go.

- Commons: a 25% chance per enemy defeated; sixteen of them, from the weakest level band to the strongest: Bit Shard,
  Byte Shard, Data Chip, Code Chip, Core Fragment, Core Crystal, Mega Crystal, Giga Crystal, Tera Prism, Peta Prism,
  then Chrome, Blue, Red, Gold, Black and Obsidian Digizoid. You hold up to 99 of each.
- Rares: 71, one per region with a Rare Target, up to 99 of each. 149 species carry a common material. (The table is
 `port/game/definitive_hunt_targets.c`.)
- The results screen lists each one after the item: "Hunt material: Bit Shard x2" for commons, "Rare material:
  <species> Core" for a rare.
- A Rare Target pays ten times the experience of the encounter it replaced. Its R-Booster is 01 below level 25, 02
  from 25, 03 from 40, 04 from 55 and 05 from 70.

### Spending them

What you collect pays for better gear: every shop is also a forge, a trader in the first town runs a Black Market,
and the DRI Agents open the Gauntlet.

<p align="center"><img src="../images/definitive/economy-flow.svg" width="800" alt="From the hunt to your gear: commons from random battles and rare Cores from Rare Targets go to the forge in every shop, the Black Market (1 Core for 12 commons, replicas for 2 Cores plus Bits, the Hunter's Mark) and the DRI Gauntlet, which give shop equipment, 32 replicas and the Power accessories."></p>

### Prices

Advanced gear costs more and resells for much less, so the Bits from OVERFINISH have somewhere to go and a lucky find
no longer sells for a fortune. A weapon, armour or accessory that a shop sells for 10,000 Bits or more now costs 150%
of that, and resells for 5,000 plus 8% of its old price above 10,000 (never more than half the new price, rounded to
100): a 60,000 sword costs 90,000 and resells for 9,000 instead of 30,000, a 15,000 one costs 22,500 and resells for
5,400. Everything else keeps the game's prices.

### The forge

Every weapon, armour and accessory a shop sells asks for Hunt Materials as well as its Bits, and they come from the
hunting regions around that shop: the first town's armory wants the Byte and Bit Shards of the park and the beach, Xuan
Wu's wants the Peta Prisms and the Cores of the snowy mountains. The same item sold in two towns asks for each town's own.

| Original price | Materials |
|---|---|
| Any | One common material those regions leave, near the level the item is bought at: 2 of it for the cheapest gear, up to 40 for the dearest |
| 9,500 and up | Also the rare item of one of those regions (two from 50,000) |

Every one of the 16 common materials is asked somewhere, and no shop asks the same one for everything: a shelf spreads
over the materials near its level (a shop with too few around it looks a little further out), so Mega Crystals and
Obsidian Digizoid, which no recipe used to ask, are worth picking up.

Select an item and its description shows what it takes against what you have, orange while you are short: "Gold
Digizoid 0/25, Fujinmon 0/1" (a rare is named by the Rare Target that drops it). Without them the shop refuses, as it does when
you lack the Bits ("Not enough materials."), and how many you can buy at once is capped by your materials.

### Equipment variants

The game has no free item slots, so 41 shop items were turned into variants in place: one of each of the first three
kinds in every shop tier, one Elemental weapon per tier (two in the last) and four Trade-off accessories, each built
around a style instead of a bigger number. They keep their icon, price and recipe, and a Digimon
already wearing one simply gets the new stats.

- **Speed** (one-handed weapons): half the power of the tier's best weapon, for Boost, from +5 in the first tier to +85
  in the last. With this edition's turn order, more Boost than the enemy means two actions before its first. Breeze
  Spear, Swift Pike, Quick Sword, Gale Saber, Zephyr Sword, Rapid Edge, Hayatemaru, Sonic Blade, Flash Sword.
- **Elemental** (head pieces): half the guard of the tier's best, for one family's resistances, which in this edition
  are your affinity: from +85 Fire to +355 Wind, about +0.03 to +0.11 on every WEAK! hit and again on a staggered
  target. Ember Band, Frost Visor, Gust Helmet, Volt Crown, Tide Cap, Circuit Helm, Shade Goggles, Blaze Crown, Tempest
  Crown.
- **Tank** (body armour and shields): 50% more guard than the tier's best, plus some Spirit, for a Boost penalty of −6
  to −30. Padded Coat, Quilted Suit, Heavy Shield, Bulwark Suit, Rampart Mail, Tower Shield, Fortress Vest, Aegis Wall,
  Titan Shield.
- **Elemental weapon** (one-handed weapons): a quarter less power than the tier's best weapon, for one family's
  resistances (+80 Fire on the first, up to +400 each on the last), the same affinity the Elemental heads give. Head
  and weapon stack, paying with both guard and power. Every Rookie has one, and the families rotate: Ember Dagger,
  Mizuchimaru (both for everyone), Sylph Tale, Volt Fang, Shade Gun, Blaze Glove, Tidal Horn, Storm Spear, Raijin Fist
  and Suijin Cannon (thunder and water, as their names already said).
- **Trade-off** (accessories): half again as much of one stat as the plain piece, paid for with another. In the Gems
  list (the accessory shops of Amaterasu and Zhu Que) the Fury Gem gives Power +15 for Guard −5 and the Sage Gem
  Wisdom +15 for Boost −3; in the Rings list (Amaterasu's later accessory shop) the Fury Ring gives Power +30 for
  Guard −10 and the Sage Ring Wisdom +30 for Boost −5. Each replaces the plain piece of a stat whose other list keeps
  it (the Power and Wisdom Rings, the Guard and Spirit Gems stay), and the shop's preview shows the cost in red.

### The Black Market

A sleepy townsperson in a house of the first town (the same house later in the game, and now also the Velvet Room:
see [The Velvet Room](velvet-room.md#the-velvet-room)) runs the Black Market. Talking
to them opens two tabs (L1/R1):

- **Exchange**: one rare item for 12 of any common material you pick, as long as all 12 fit under the 99 cap.
- **Replicas**: 32 pieces of equipment no shop sells, as good as the original: the seven Power S accessories,
  Machinegun, Magician Rod, Ghost Fang, Pulse Guard, Drill Horn, Chain Saw, Naginata, Recover Crest and MP Mega Proxy;
  also eleven accessories no shop or chest gives (Multi Crest, Search Crest, HP Mega Proxy, Over Load,
  EXP Adapter, BIT Adapter, Power Brace, Goggles, Dance Wing, Hack System, Runner Shoes) and five ultimate weapons
  (Muramasa, Super Nova, Punishment, Eternally, Invincible).
  Each costs one Core of each of its own two bosses, chosen by its theme and strength (the Fire Power S takes Deltamon
  and BK WarGrowlmon, the Machinegun Megadramon and Puppetmon), shown with what you hold ("Takes: Deltamon 1/1,
  BK WarGrowlmon 0/1", orange while one is missing), plus Bits: 20 times the original's selling price for the first
  sixteen (20,000 to 40,000), 25,000 to 50,000 for the exclusive accessories and 80,000 for each ultimate weapon. No boss
  serves two replicas.
  A replica resells for 0 Bits, so the shop does not even offer to buy it.

### The DRI Gauntlet

Talk to a DRI Agent: until their Rookie is in your party they give their DDNA quest, at any party level (the game's
"too busy" and "you don't need it" level window is gone), and tell where the Digimon with the DDNA is found. Once the
Rookie has joined (at once if it was one of your starters), talking to them asks what you want: **Simulations** (the
list), **Talk** (their own lines, as in the game, with nothing after) or **Leave**. Each replays a classic Tamer's battle data, who has a word for you before the fight and after it. There are sixteen lines, two for each agent. Each is one battle against an evolution line's four stages,
Rookie, Champion, Ultimate and Mega: when one falls the next comes in at full HP with a **DIGIVOLVE!** callout (the
Mega joins the same battle once the Ultimate is down), while your party keeps the HP, MP and items it has spent. There is no running away and no game over: lose and you are back on the
field with your HP and MP as they were before (spent items stay spent). A simulation is unlocked once, for good, by
handing in its materials.

The lines are listed in difficulty order. The Champion fights at level 24 + 4 for each place in that order, each next
stage 3 levels higher, and the Rookie 3 below the Champion. Player forms (Greymon, Growlmon...) appear under their own
names.

| Agent | Line | Stages and levels (Rookie to Mega) | Unlock | Rewards |
|---|---|---|---|---|
| Ken | Kotemon | Kotemon 21, Dinohumon 24, Kyukimon 27, Marsmon 30 | Code Chip ×15, Divermon Core | Wind Power 1, 2, S |
| Yang | Monmon | Monmon 25, Hookmon 28, Armormon 31, GuardiAngemon 34 | Core Fragment ×15, Armormon Core | Water Power 1, 2, S |
| Yuji | Kumamon | Kumamon 29, Grizzmon 32, GrapLeomon 35, Cannondramon 38 | Core Crystal ×20, Yanmamon Core, Phantomon Core | Metal Power 1, 2, S |
| Bob | Veemon | Veemon 33, ExVeemon 36, Paildramon 39, Imperialdramon 42 | Giga Crystal ×20, BK Imperialdramon Core, Paildramon Core | Bolt Power 1, 2, S |
| Kazuki | Patamon | Patamon 37, Angemon 40, MagnaAngemon 43, Seraphimon 46 | Tera Prism ×20, MagnaAngemon Core, Raremon Core | Wind Power 1, 2, S |
| Michael | Renamon | Renamon 41, Kyubimon 44, Taomon 47, Sakuyamon 50 | Blue Digizoid ×25, Lynxmon Core, Roachmon Core | Ice Power 1, 2, S |
| Johann | Agumon | Agumon 45, Greymon 48, MetalGreymon 51, WarGreymon 54 | Gold Digizoid ×25, MetalGreymon Core, BK WarGreymon Core | Fire Power 1, 2, S |
| Jimmy | Guilmon | Guilmon 49, Growlmon 52, WarGrowlmon 55, Gallantmon 58 | Black Digizoid ×25, BK WarGrowlmon Core, WarGrowlmon Core | Dark Power 1, 2, S |
| Ken | Musyamon | Kotemon 53, Musyamon 56, Knightmon 59, MetalGarurumon 62 | Obsidian Digizoid ×30, Divermon Core, Raidenmon Core | Ice Power 1, 2, S |
| Yang | Terriermon | Hagurumon 57, Guardromon 60, Antylamon 63, MegaGargomon 66 | Obsidian Digizoid ×30, BK MegaGargomon Core, Guardromon Core | Metal Power 1, 2, S |
| Yuji | Kuwagamon | Kunemon 61, Kuwagamon 64, Okuwamon 67, GranKuwagamon 70 | Obsidian Digizoid ×30, Okuwamon Core, Kuwagamon Core | Bolt Power 1, 2, S |
| Bob | Wormmon | Kunemon 65, Stingmon 68, Paildramon 71, Imperialdramon-F 74 | Obsidian Digizoid ×35, BK Imperialdramon Core, Paildramon Core | Bolt Power 1, 2, S |
| Kazuki | Devimon | DemiDevimon 69, Devimon 72, Myotismon 75, MaloMyotismon 78 | Obsidian Digizoid ×35, Raidenmon Core, MetalGreymon Core | Dark Power 1, 2, S |
| Michael | Angewomon | Kiwimon 73, Angewomon 76, MagnaAngemon 79, Phoenixmon 82 | Obsidian Digizoid ×35, Persiamon Core, MagnaAngemon Core | Water Power 1, 2, S |
| Johann | Agumon (dark) | Agumon 77, Greymon 80, SkullGreymon 83, BK WarGreymon 86 | Obsidian Digizoid ×40, BK WarGreymon Core, Tyrannomon Core | Fire Power 1, 2, S |
| Jimmy | Impmon | Goburimon 81, Flarerizamon 84, Megadramon 87, Beelzemon 90 | Obsidian Digizoid ×40, Megadramon Core, Raidenmon Core | Dark Power 1, 2, S |

The Rookie gives no reward. The Champion you fell gives the line's Power accessory at tier 1, the Ultimate tier 2 and
the Mega tier S, even if you lose afterwards. Each tier is given once, the first time you reach it, so a first full
clear gives all three; the list keeps your best stage. A replay pays the battle's experience and Bits, but no
accessory.

**What a Power accessory does:** the seven Elemental Powers (Fire, Water, Ice, Wind, Bolt, Metal and
Dark Power, tiers 1, 2 and S) no longer lend your basic attack an element, which would break the rule that only magic
carries one. Each raises its element's stat by **+60, +120 or +200** instead, and that stat is your affinity
([Combat: damage](combat.md#damage)), so a Power accessory makes that element's magic hit harder on weaknesses and
staggered foes. In the same pass the Counter Crest's free counter was cut to 1% of its damage (counter techniques and
enemy counters are untouched).

## The hunt: constants and hooks

- Chain: `DEF_HUNT_CHAIN_MIN` 5, sure at 10, cap 255; 20% at 6 wins, +20% per win.
- Regions are the `ststatus_map_areas` byte: area 0..0x2D (`DEF_HUNT_AREAS` is 0x2E) plus bit 7 for the second server
  (Amaterasu).
- Rare Target: battle + slot of an existing battle, alone, level = region average + 5, HP x4, XP x10. Flee always works,
  except in an active Super Rare territory.
- 25% common drop per enemy; the 16 commons are listed above.
- Hooks: `fieldstg_encounter_start` (the Rare Target swap happens **before** the Rush); `def_log_end` ->
  `def_hunt_battle_end`; `def_hunt_flee`. The chain resets on a map region change and on a Guardromon teleport.
- The table `port/game/definitive_hunt_targets.c` is hand-maintained; `tools/gen_hunt_targets.py` is legacy and refuses to
 overwrite it. The forge's recipes are checked by `tools/gen_recipes.py --check`.
- Tests: `tests/port/definitive_hunt.py` (+ driver), the market, economy and DRI tests below.
- The Rare Target's battle (`def_hunt_scale`): the target's slot of the drawn battle is moved to the first position and
  the three members are zeroed around it (one Digimon); its HP follows the level up or down (a strong
  Digimon is lowered in a weak place) times `DEF_HUNT_RARE_HP_MULT`, capped at `DEF_HUNT_HP_MAX`; `stat_scale` is left
  as it is.
- Economy constants (`definitive.h`): `DEF_HUNT_MATERIALS` 16 and `DEF_HUNT_MATERIAL_CAP` 99 (one u8 each,
  `REC_EXTRA + 10..25`), `DEF_HUNT_RARES` 128 slots and `DEF_HUNT_RARE_CAP` 99 (record 2), `DEF_REPLICAS` 32 (bits at
  `REC_EXTRA + 26..29`), `DEF_DRI_CHALLENGES` 16 (an unlocked bit and a 2-bit best stage each, record 2); prices
  `DEF_PRICE_KNEE` 10000, `DEF_PRICE_DRAIN_PCT` 150, `DEF_SELL_BASE` 5000, `DEF_SELL_ABOVE_PCT` 8; forge
  `DEF_RECIPE_REACH` 4 exits, `DEF_RECIPE_OVER` 6 levels, `DEF_RECIPE_RARE_FROM` 9500, `DEF_RECIPE_TWO_RARES` 50000;
  Rush panel `DEF_RUSH_FLOAT_FRAMES` 175 ticks.
- What the hunt driver (`definitive_hunt_driver.c`) checks: the chance curve (0 up to the minimum, even steps, certain
  from `DEF_HUNT_CHAIN_SURE`); won encounters count and no draw happens under the minimum; a lost or fled one neither
  counts nor resets; a Rush-skipped one counts and leaves materials, a scripted battle does not; another map of the
  same region keeps the chain, another region or the teleport resets it; the swap, the target alone at its level and
  HP, the flee rule (always outside a Super Rare hunt, `DEF_SUPER_RARE_FLEE_PCT` inside its territory, from
  `DEF_SUPER_RARE_CHAIN_MIN` wins), the numeral in its name; rewards (the region's rare, the replaced encounter's XP,
  the Booster by level as one pack, the line shown once, cleared at the screen's end); commons (one draw per enemy,
  nothing on a miss, defeat or script); the caps; record 1 with only the hunt state or only materials written and read
  back (a count past the cap reads as the cap), a new game writes all zeros, an older record reads zero; record 2
  (counts and "ever held", damaged or missing reads empty); nothing changes or draws in a replay script.
  `gen_hunt_targets.py` asserts the 13 Super Rares distinct and the 193-species commons table.
- In-game recipe for the chain: load the map 0x21D field state (see "Making a battle state for tests"
  below), `mem_write` the chain near the threshold and `gamestate_data+0x30` (`encounter_timer`) = 1, walk, then check the
  Rare Target, the RARE! callout, that no Rush question appears and that fleeing succeeds.

### Prices, forge and variants: how they are built

- Prices: the table is in `definitive_prices.c`, read through `def_item_buy` / `def_item_sell`; the shop's `u16 price`
  became `s32`. From 10000 Bits the cost is 150%; the resale is 5000 + 8% above 10000, never more than half the new
  price, rounded to 100. Generator: `tools/gen_item_prices.py --check`.
- Forge: 317 recipes, by shop and item (18 equipment shops); STITSHOP tells the forge which shop is open
  (`def_recipe_set_shop`, savestated) and hooks at selection, max-buy and debit; the refusal message is "Not enough
  materials." Recipes are generated by `tools/gen_recipes.py` (needs the disc; `--check` skips without it). The common
  is picked by level gap plus 2 per earlier use in the shop, among the commons within 8 levels of the nearest; a shop
  with fewer than 3 commons around widens its reach up to 7 exits. `--check` also requires all 16 commons asked, no
  shop on one common, and a worst level gap of 25 outside shop 23 (Zhu Que's rings, reached late: accepted).
- Variants: 41 shop items converted in place, written by `def_variants_hook` into RecordsWeapon/RecordsArmor/
  RecordsAccessory every time (absolute values, so re-running is safe). Budget weights: Guard 100, Spirit 100, Wisdom 50, Boost 100, hit 25,
  element 5. Speed: weapon power minus 50% with Boost +5..+85; Elemental: head guard minus 50% with resists up to 400;
  Tank: guard +50%, Spirit 25%, Boost -6..-30; Elemental weapon (`ELEMENT_WEAPONS`, hand-picked, kind
  `DEF_EQUIP_ELEMENT` with the weapon's fields): power minus 25% with resists up to 400. Trade-off (`TRADES`,
  hand-picked, kind `DEF_EQUIP_TRADE`, `weapon` 2): the stat +50% over the list's plain piece, the extra paid in the
  second stat at the budget's weights; RecordsAccessory has one bonus, so the penalty sits in its unused `unk_09` bytes
  and `def_accessory_penalty` adds it where the game adds the accessory's bonus (`gamestate_get_stats`, the shop's
  `stitshop_info_get_stats`), only with `def_on`. Only two items were strictly dominated (Long Sword and Kulon Sword).
  Generator: `tools/gen_equipment.py --check`.
- Forge spread (constants `MIN_POOL, REACH_MAX, WINDOW, SPREAD, SECOND = 3, 7, 8, 2, 2` in `tools/gen_recipes.py`):
  rares are dealt from the reach-4 regions. A safety net sends a common no recipe asks to the shop of its server whose
  dearest item's level is nearest the common's mid level. Shop 23 (the Gems list's second-server shop, map 0x2AB) is
  outside the level-gap check: its cheap rings and gems ask Giga Crystal and Tera Prism because no region near it is
  low, and the player reaches it late. The recipes are a code table, so old saves are unaffected. Checks:
  `gen_recipes.py --check` and `tests/port/definitive_economy.py`.
- The 27 base variants, by shop tier (from `tools/gen_equipment.py`; original price in brackets):

  | Tier | Shops (maps) | Speed | Elemental (head) | Tank |
  |---|---|---|---|---|
  | 1 | 0 (0x20D) | Breeze Spear (150) | Ember Band, Fire (40) | Padded Coat (70) |
  | 2 | 5 (0x22F) | Swift Pike (400) | Frost Visor, Water+Ice (100) | Quilted Suit (160) |
  | 3 | 7 (0x23E) | Quick Sword (1000) | Gust Helmet, Wind (550) | Heavy Shield (550) |
  | 4 | 10, 11 (0x25B, 0x25D) | Gale Saber (2500) | Volt Crown, Thunder+Machine (1100) | Bulwark Suit (1300) |
  | 5 | 3, 20 (0x20E, 0x29D) | Zephyr Sword (5000) | Tide Cap, Water+Ice (900) | Rampart Mail (3800) |
  | 6 | 13, 22 (0x26F, 0x2AB) | Rapid Edge (9500) | Circuit Helm, Thunder+Machine (4200) | Tower Shield (4200) |
  | 7 | 25 (0x2C4) | Hayatemaru (15000) | Shade Goggles, Dark (2800) | Fortress Vest (8500) |
  | 8 | 26, 28 (0x2C6, 0x2D6) | Sonic Blade (22000) | Blaze Crown, Fire (6800) | Aegis Wall (7500) |
  | 9 | 15 (0x27C) | Flash Sword (30000) | Tempest Crown, Wind (9000) | Titan Shield (11000) |

  Shops 1, 8, 23 (the Gems list) and 16 (the Rings list) sell only accessories, so they carry only Trade-off pieces;
  shop 18 (per-Digimon sets) is left out by design.
- Elemental weapons (`ELEMENT_WEAPONS`; `ELEMENT_WEAPON_COST` 25% of power, half the heads' 50,
  because a weapon's power is the whole damage and at 50 the resists hit their cap from the middle tiers): tier 1 Dagger
  -> Ember Dagger (Fire, everyone), 2 Shishioumaru -> Mizuchimaru (Water+Ice, everyone), 3 Fairy Tale -> Sylph Tale
  (Wind; Renamon, Patamon), 4 Wolf Fang -> Volt Fang (Thunder+Machine; Agumon, Veemon, Guilmon), 5 Shotgun -> Shade
  Gun (Dark; Monmon), 6 Tiger Glove -> Blaze Glove (Fire; Kumamon), 7 Scissor Horn -> Tidal Horn (Water+Ice; Agumon,
  Veemon, Guilmon), 8 Royal Spear -> Storm Spear (Wind; Guilmon, Renamon, Patamon), 9 Raijin Fist (Thunder+Machine;
  Kumamon) and Suijin Cannon (Water+Ice; Monmon) keep their names with new descriptions. Candidates were plain items
  with no bonus, no ID effect, no strong type, not in a set and not yet a variant. Stacking with an Elemental head is
  allowed: `def_affinity` caps each resist at 999 before it becomes affinity (`DEF_AFF_*_MAX`), so stacking only reaches
  the ceiling sooner. Not made: two-handed "berserk" weapons (two-handers already trade Boost for power) and Wisdom
  rods (rods already carry Spirit).
- Trade-off pieces (`TRADES`): Power Gem -> Fury Gem (292) and Wisdom Gem -> Sage Gem (298) in the
  Gems list; Guard Ring -> Fury Ring (295) and Spirit Ring -> Sage Ring (297) in the Rings list. Left out on purpose: the
  status rings (Antidote, Revive, Awake, Sober, Prayer) and the elemental rings (plain resists). Boost is the Sage
  pieces' price because Wisdom drives the support buffs. FIGHTSTG's own read of an accessory's bonus
  (`fightstg_rules_get_tech_cost`: the MP Proxy and MP Mega Proxy cut a technique's MP) is not a stat, so it needs no
  penalty hook. Without the penalty a bigger single stat would be a straight
  upgrade, which `gen_equipment.py --check` refuses. The shop preview shows Power up in blue and Guard or Boost
  down in red.
- Forge screen: the recipe line takes the place of the slot text ("Right Hand" / "Both Hands") on the shop's
  description page; the icon shows the kind and the Triangle page who can wear it. In `pt.txt` the Elemental weapons and
  items 292/295/297/298 still have the plain pieces' lines.
- `tests/port/definitive_economy.py` (driver): an item the price table has (past the u16), one it does not, no item, a
  replica's resale 0; recipes with no shop open, another shop's recipe for the same item, an item without a recipe, a
  shortfall (orange label), buying several units, the scarcer of two materials deciding, payment; in a replay script
  the game's prices and no recipe. `gen_item_prices.py --check` requires the table to be the rule's and a dearer item
  never to resell for less. In game, Asuka's shop (`shop_open` is a checkpoint of `first_battle_save`) shows the prices.

### The Black Market, in detail

- Trader: actor 42 on maps 0x20C and 0x27B, and now also `{0x20F, 42}` in the Velvet Room
  (`port/game/definitive_market_screen.c`, `traders[]`). Two tabs (L1/R1).
- Exchange: 1 rare for 12 commons, only if all 12 fit under the 99 cap.
- Replicas: 32 items, one Core of each of its own two Rare Targets (`DefReplica.cores`, hand-picked in the generator's
  `CORES`, none shared) plus Bits (20-80k); resale 0, using one bit per replica (record 1, REC_EXTRA 26-29; the first 16
  bits keep their place, so old saves still match). List: Power S x7, Machinegun, Magician Rod, Ghost Fang, Pulse Guard,
  Drill Horn, Chain Saw, Naginata, Recover Crest, MP Mega Proxy, then Multi Crest, Search Crest, HP Mega
  Proxy, Over Load, EXP Adapter, BIT Adapter, Power Brace, Goggles, Dance Wing, Hack System, Runner Shoes, Muramasa,
  Super Nova, Punishment, Eternally, Invincible. Generator: `tools/gen_replicas.py --check`.
- **UX pass:** everything uses the dialogue font 1; white text, orange cursor and shortfall, green success, grey
  explanation lines; rows show "have/99"; the detail lines give description, type/stats, who can equip and which Cores.
- Pitfall: a **colon renders as "0"** in the 14px font (30 other port strings are affected; the 11px font has no
  `/ : , '` or accents either).
  Colours have fixed meanings (white text, orange = cursor row or what is missing, green = success or already the
  player's, grey = what does not apply and explanations). A replica's four detail lines: the shop description (ITMINF),
  type and stats, **who in the team can equip it** (a purchase nobody can use warns but is not blocked) and which Cores
  it spends; the confirmation warns that the resale is 0. A detail line wider than the panel goes to the log ("market:
  detail line ... pixels wide"). The strings avoid colons.

### The DRI Gauntlet, in detail

- Agents are the eight DDNA researchers, by map and actor (`AGENTS`); an actor with their sprite elsewhere is an
  ordinary NPC. Each talk rewrites the agent's talk conditions before the game picks the entry (`dri_quest_open`): the
  quest line (the entry that sets quest flag 0x600..0x607) gets an always-true pair (`0x5000`) for its party level sum
  conditions, the other level-gated entries a never pair (`0x61FF`, progress 511). The list is armed only once the
  agent's Rookie has joined (`dri_rookie` reads it from his own "joined" conditions), so the hand-in talk never opens
  it; then it opens after the talk, once the field has been idle for 30 frames (max 3600). Bob's and Johann's quest
  lines name the place in `en.txt`, the other six in the story rewrite (`en_story.txt`).
- A challenge is one boss battle whose 3 enemy members are a line's Rookie, Champion and Ultimate; `wfightmn_knockout`
  brings in the next at full HP with a DIGIVOLVE! callout, and when the third falls `def_dri_next_wave` (called by
  `def_enemy_next_wave`) sets the Mega up in a fallen member's slot, so it is the same battle. Stage 19, music
  0x60880000. Defeat returns to the field with HP/MP restored (`def_dri_defeat`). 16 lines, two per agent. Levels: the
  Champion at `DEF_DRI_LEVEL_FIRST` (24) + `DEF_DRI_LEVEL_GAP` (4) x rank (the line's place in difficulty, 0..15), each
  next stage `DEF_DRI_LEVEL_STEP` (3) higher, the Rookie 3 below the Champion. Rewards: tier 1 for the Champion, 2 for
  the Ultimate, S for the Mega (the Rookie gives none), each tier the first time it is reached.
- Tamer titles, one per line (`$DriRival<index>*`, in the order of `LINES`): the first five lines keep the slots they
  had in 0.8.5, so saves from before still match.
- The table `port/game/definitive_dri_table.c` is hand-maintained (`tools/gen_dri_challenges.py` is legacy, so its `--check` differs); tests: `tests/port/definitive_dri.py` (+ driver).
- Save: unlocked bits (2 bytes, low first) and best stage per challenge (2 bits each, 4 bytes) in record 2 at
  `REC2_DRI` (right after the rare items' "ever held" bits; `port/game/definitive_bestiary.c`).
- The agents (`AGENTS` in `tools/gen_dri_challenges.py`, `def_dri_agents` in `definitive_dri_table.c`; DDNA item and
  quest flag read from each agent's stage file, the DDNA boss's from the stage file that places him):

  | Agent | Map / actor | Rookie | DDNA item | DDNA from (boss, place, stage file) | Quest flag | Old level window (party level sum) |
  |---|---|---|---|---|---|---|
  | Kazuki | 0x21D / 43 | Patamon | 39 | MagnaAngemon, South Badland, West Sector (`wstag550`) | 0x602 | 45-74 |
  | Michael | 0x21D / 44 | Renamon | 38 | Taomon, North Badland W, West Sector (`wstag565`) | 0x603 | 45-74 |
  | Bob | 0x226 / 43 | Veemon | 35 | Paildramon, Bulk Bridge, South Sector (`wstag450`) | 0x607 | under 30 |
  | Johann | 0x22B / 43 | Agumon | 6 | MetalGreymon, Bulk Swamp, South Sector (`wstag445`) | 0x600 | 30-59 |
  | Jimmy | 0x22E / 43 | Guilmon | 34 | WarGrowlmon, Ether Jungle, South Sector (`wstag490`) | 0x601 | 30-59 |
  | Ken | 0x258 / 43 | Kotemon | 19 | Kyukimon, Suzaku UG Lake, South Sector (`wstag525`) | 0x604 | 60-89 |
  | Yuji | 0x258 / 44 | Kumamon | 395 | GrapLeomon, Ether Jungle, South Sector (`wstag490`) | 0x605 | 60-89 |
  | Yang | 0x258 / 163 | Monmon | 360 | Armormon, Duel Island, East Sector (`wstag385`) | 0x606 | 60-89 |

  The agents' stage files are `wstag330` (0x21D), `wstag375` (0x226), `wstag405` (0x22B), `wstag420` (0x22E) and
  `wstag635` (0x258); the level window no longer applies. Places are named as each map's own title shows
  them (Suzaku UG Lake is the underground lake, not the city). `def_dri_agent` still accepts a map of -1 as "any map",
  but no row uses it.
- How the game's agent talk is built: every agent is a placed actor in its `src/wstag/wstagNNN.c`; several
  `FieldstgPlacedActor` variants share his spot, chosen by `flags_required` (one without the DDNA held, one with it),
  and each variant's `FieldstgTalk` list says the first entry whose flags pass: the first meeting (flag `0x1A1F` = 0 on
  0x21D), the Rookie already joined, the party level sum under the lower bound ("too busy"), inside the window (the
  quest line, which sets the quest flag) or at the upper bound ("you don't need it"). The DDNA boss is placed only
  with the quest flag set; the DDNA-held variant says the hand-in line and JOINs the Rookie. Flag decoding
  (`gamestate_get_flag`): `0x70xx` is a `gamestate_conditions` entry, whose type `0x10 + n` is `cond_digimon(n, arg)`:
  1 Rookie joined, 3 Rookie joins (the hand-in), 5 party level sum at least `arg * 15 + 30` (condition IDs 0x25-0x29 =
  sums 30, 45, 60, 75, 90, used only by the five agents' stage files), 6 Rookie not joined; `0x80xx` is an item held.
  `dri_quest_open` writes absolute values into the talk lists and runs only with `def_on`, so replay scripts keep the
  disc's data.
- Hint lines: with the window gone an agent whose Rookie has not joined gives his quest line on every talk, so each
  hint is one extra page before the offer, naming the place. Bob (`TALK06 676`) and Johann (`TALK06 691`, whose disc
  line said he had no idea where MetalGreymon was) are in `en.txt`; the other six are pages in
  `en_story.txt`, which wins over `en.txt`: Kazuki `TALK02 633`, Michael `TALK02 647`, Jimmy
  `TALK01 277`, Ken `TALK02 831`, Yuji `TALK02 839`, Yang `TALK02 847`. `pt.txt` still has the disc's lines without the
  hints. A starter's agent
  counts as done from the start: he asks at once. `def_dri_debug` (tests): a challenge's index starts it, 100
  (`DEF_DRI_DEBUG_MENU`) opens the list from the idle field, 102 (`DEF_DRI_DEBUG_AGENT`) sets the player 24 pixels under
  the map's first DRI Agent, or warps to Asuka's Kazuki (0x21D) when the map has none placed; then UP and CIRCLE talk.
- Host test (`definitive_dri_driver.c`): the tier per stage; the table's bounds; an agent on his map, a map-any row, and
  nobody else (an actor of an agent's sprite on another map); the wait (nothing while the talk runs, the list once, an
  idle gap inside the talk, a battle or another map drops it, a talk that never ends gives up); the Tamers' lines;
  unlock (locked with nothing, the common alone is not enough, exact payment, never paid again, a rare short shows
  orange, a rares-only challenge); rewards (each tier once, the best stage only rises, capped at 3); record 2 (DRI state
  alone writes it at bytes 152..157, read back, a damaged record reads nothing unlocked, a rares-only record, record 1's
  read resets them before record 2 brings them back, out-of-range challenges); nothing in a replay script.
- Known bug: the debrief reward line overflows its box.

### Guide, Guardromon network, Rush

- **Guide** (`def_quests`): one entry per story step 0..45, keyed on key-item flags (0x8000|item); 4 missions; 16 Cargo
  Tower auctions (flag 0x400+N; travel goes to map 0x213, none from map 0x2D7 on). The notice panel lasts 300 ticks.
  Env `DW3_GUIDE=0/1`, `DW3_GUIDE_CHECK`.
  - Trigger: the game's own story counter `gamestate_data.progress` (0..45, written by the stage events) plus a flag when the line asks (a key item held, `0x8000 | item`, or a stage's own flag).
    Nothing is saved, so an old save opens on the right step. Main quest (`def_quests`, `definitive_quest.c`): one line per step with title, what to do and where to go (the map
    name is the game's own title); key-item variants come before the plain line (the Sepik Mask, the Agumon Suit, the TNT Chip); texts in `en.txt`
    as `$Q<step>` and `$Q<step>T`. Step 6 (the Blue Card, which starts at the gondola's terminal, WSTAG410's event 90) follows the Guilmon chase on the
    stages' own flags, latest first: Asuka Inn 2F's Guilmon (`0x1C44`), the Forest Inn's (`0x1C45`), the 8lue Card from Zephyr Tower, the terminal's refusal
    (`0x1C46`), the Guilmon in Seiryu City (`0x1C47`), the Blue Card from the Forest Inn BF, and Bulbmon only once he falls on the ride (`0x4018`): `$Q6b`..`$Q6h`. Missions (`def_missions`): from step 4 (when the field menu opens) while the item is missing: El Dorado ID (opens the auctions), Folder Bag (Card Battles),
    Tree Boots, Fishing Pole. Auctions (`def_auctions`): the 16 of the Cargo Tower are the game's `fieldstg_flag_events`; auction N runs with the story step exactly its own, flag `0x400 + N` clear, and money; any answer
    sets the flag; states: not yet, OPEN NOW, attended, lost (the step passed without going; Amaterasu's hidden shop sells the lost items); price from the game's own money table.
  - Screen GUIDE: a new field-menu entry (between SAVE and the Bestiary) in the Guardromon-network layout; Quest tab, Auctions tab (L1/R1) coloured by state; on an open auction confirming twice travels to El Dorado
    (`0x213`, the Cargo Tower entrance) like the Guardromon teleport (resets the hunt chain); **no travel** from the real world, from Gunslinger, from El Dorado and from Byakko's trap.
  - Notices: when the main quest changes step or an auction opens, a panel drops at the top of the field for 5 s ("NEW OBJECTIVE", "AUCTION OPEN"); it does not hold the player and waits while an event, the menu or a
    battle start holds the field. Off in replay scripts (shared with the emulator); `DW3_GUIDE=1` turns it on in scripts too.
  - Test: `tests/port/definitive_quest.py` (a line per step, item variants, missions, auction state/price/travel,
    notices once per change, replay; tables against the game's: the 16 auctions against `fieldstg_flag_events`, every
    step 0..45 has a plain line after the variants, every text in `en.txt` fits the window's lines).
- **Guardromon network**: 25 inns (13 plus 12 copies of the second half); `nodes[]` in `definitive_travel.c` and
  `gs_inns[]` in `game_mods.c`; SAVE and TELEPORT; the network mask is REC_EXTRA 4-7.
- **Overlevel Rush floating panel**: `def_rush_apply_quiet` (in STFGTREP) applies the report's rewards in the same order
  without the screen; the panel draws on layer 0x1004, does not set `event_running` and closes when the field holds the
  player. `DW3_RUSH_FLOAT`, `DW3_RUSH_TEST`. Level-up stat rolls differ in timing from the report. Test:
  `tests/port/definitive_rush.py`.

## Engine facts these features rely on

- Battle: `fieldstg_start_battle(battle)` copies `fieldstg_battles[battle]` (335 battles,
  `src/fieldstg/fieldstg_80083784.c`) into `records_state` (enemies with `digimon`, `level`, `hp`, `mp`, `stat_scale`)
  and sends the field to map `0x600`; a random encounter is `fieldstg_encounter_start`, a scripted one
  `fieldstg_start_listed_battle` / `fieldstg_start_battle_5` (never touch those).
  `fieldstg_stage.battle_lists->encounters[area]` holds the random lists; that `area` is attribute layer 4 **inside the
  stage** (0..2), not the world-map area, and the battle drawn is `events[pad_random.next() & 7]`.
- Reward: `stfgtrep_rewards[records_battle_results.battle]` has `tech_exp`, `exp`, `money`; the item comes from
  `records_battle_results.item`. Species record: `FightstgEnemyRecord` (file `0x1CF`, 193 species).
- Flee: `fightstg_rules_roll_escape` returns 0 before rolling if `records_state.blocked[11]` is set or the status
  forbids, else the chance is `(battle->escapes + 1) * 8`. `fightstg_battle.state.type`: 0 normal, 1 the enemy may flee,
  5 script, 6 boss.
- World area: `ststatus_map_areas[map & 0xFF] & 0x7F` (0..46), bit 7 = second server (Amaterasu).
- A stray `object_new` in the field **does not run** (nobody updates it): use static state plus a per-frame hook (like
  `fieldstg_rush_frame`, called from `fieldstg_camera_update`). In one ordering-table entry what is added later is drawn
  **below**: text first, dark panel last. `def_poly_text` (`definitive_callout.c`) draws text in polygons for screens
  without the game font; a new letter is one row of the `glyphs` table.
- Hooks: battle end `def_log_end(fightstg_events.result)` in `wfightmn_800A6440.c`; map loaded (`get_map() + 0x1E00`
  flag, just before `def_hard_party_fix()`) for the regional reset; `fieldstg_encounter_start` (Rare swap before the
  Rush block); `fightstg_rules_roll_escape`; Bits at `stfgtrep_80082E70.c`; Cardmon test
  `(enemies[0].digimon - 0x1C9) >= 8` (Cardmons are species `0x1C9..0x1D0`); Bestiary rows `BS_ROW_Y`, `BS_HINT_Y` in
  `fieldmenu.c`; shop in `stitshop_8008321C.c` (selection, max buy, debit); defeat goes to map `0xE00`.
- Making a battle state for tests: `env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/port/dw2003 --disc
  iso/dw2003.cue --script tests/replay/scripts/first_battle_save.json --save-state battle_start:/tmp/battle.state
  --save-state-exit` (first battle), and `--save-state 41824:/tmp/field.state` for the field with random encounters (map
  `0x21D`). Then `game_start` with `extra_args ["--load-state", ...]`, `mem_write` on static symbols
  (`gamestate_data+0x30` = `encounter_timer`, 1 forces an encounter when walking) and `screenshot`. Confirm is `CIRCLE`,
  cancel `CROSS`, the field menu opens with `START`.

## How each feature is built

**Chain and Rare Target.** The region is the world area **plus server** (all 8 bits of `ststatus_map_areas`). Use
`pad_random` only after deciding the feature is on, so replay sequences are untouched. Only random encounters count. The
swap changes `event->battle` only; `stage` and `music` stay the drawn encounter's. `def_hunt_active()` holds from the swap
to `def_log_end`; the flee is forced to succeed before the `blocked[11]` check, because a boss battle used as a Rare
Target may forbid fleeing.

**Rewards and materials.** Materials are not disc items (no free item IDs): they are save counters. XP is the replaced
encounter's `exp * DEF_HUNT_XP_MULT`, split like the game's. The R-Booster is **one** pack (`DEF_HUNT_PACKS`,
`DEF_HUNT_PACK_LEVELS`); the screen marks it (`def_hunt_take_pack`, `def_hunt_pack_count`). The commons come from
`def_hunt_commons` (the species of random battles, ordered by the highest level they appear at, cut into 16 groups).
Rares keep an "ever held" bit in the second card record. The material is awarded at victory (`def_hunt_battle_end`),
not on the screen. The results lines start at step 22 of `stfgtrep_main_run`; the pending reward clears at step 50 and at
`def_log_begin`. Names are composed at run time from the disc's species name and `$HuntRareWord`, `$HuntRoman*`,
`$HuntSpace`, so no game text is embedded.

**Bestiary rows.** Under the five family rows: "Hunt" (`$HuntDrops`), up to two materials of the species
(`def_hunt_species_materials`) with the amount held (`BS_HELD_X`, grey at zero), and the rare collection ("Rare Cores
4/13", orange when complete). Unknown species show `???`, species without a target material `-`. `fieldmenu.c`
preprocesses the same without `PC_PORT`.

**Prices.** Prices are the compiled game's (`records_items` in `src/main/records.c`, not altered); shops read them
through `STITSHOP_BUY` / `STITSHOP_SELL` under `PC_PORT`. A replay script uses the game's prices.

**Forge.** Without the materials the shop refuses with "Not enough materials."; the maximum buy is capped by the scarcer
material; the item panel shows the requirement, orange while short. `definitive_glyph.c` holds the assembled texts.
**The refusal and the max-buy limit are only unit tested**, not verified on the real shop screen.

**Variants.** Each is a shop item **converted in place** (same ID, icon, slot, group, users, charisma, price and
recipe), because there is no free item ID; stats are recomputed from the item data every time, so nothing is saved. The
budget weights are `DEF_EQUIP_W_*`; a variant never exceeds the best score of the tier's ordinary items of the same slot
that share a user. Never converted: items with an effect by ID in FIGHTSTG, with a strong type, or set pieces.
`def_variants_hook` (inside `def_mp_hook`, and at every field map) writes the values; in a replay nothing is written and
`def_variant_text_hidden` keeps the disc's names. `DEF_EQUIP_ON` 0 turns it off. `tools/gen_equipment.py` also has
`OTHER_TEXTS` (port text for items 26, 46, 67, 70, 76).

**Black Market.** `def_bm_talk(map, key1)` in `fieldstg_actor_talk_to` takes the talk and the screen opens on the next
idle frame (`def_bm_frame`); `tools/bm_npc_candidates.py` lists each town's candidate actors. Each exchange swaps one
rare for `DEF_BM_COMMONS` (12), only if all fit under the cap. A bought replica sets its bit (`def_record_replica`);
with it on, `def_item_sell` pays 0 for that ID, and the bit clears when the player has none left (`def_bm_sweep`, every
field frame). Tests: `tests/port/definitive_market.py`, `gen_replicas.py --check`.

**DRI.** A simulation is an ordinary battle whose three enemies are the line's stages: when one falls
`wfightmn_knockout` brings the next in at full HP (`def_dri_stage_enters` shows the `DIGIVOLVE!` callout,
`DEF_CALLOUT_EVOLVE`), and the party keeps its state. `sim_start` (`definitive_dri_screen.c`) starts an existing boss
battle as the model (type, immunities, XP and Bits), replaces the enemies, sets stage and music (`DEF_DRI_STAGE`,
`DEF_DRI_MUSIC`) and `blocked[11]`; no Rush, hunt chain or commons. Some franchise lines have no species on the disc, so
each line uses an equivalent from file `0x1CF`. Unlocking costs the strongest common whose band ends at or below the
Champion's level (`DEF_DRI_COMMON_*`) and rares of nearby Rare Targets (`DEF_DRI_TWO_RARES`, `DEF_DRI_RARE_OVER`).
Rewards are the game's elemental "Power" accessories, added at the end of the battle (`def_log_end`), win or lose.
`def_dri_defeat` (in `wfightmn_battle_end`) returns to the field with HP/MP as before and answers 1 on every frame until
the map changes, or the game over would win. The DRI screens return the field only when their buttons are released
(`dri_release`), so the closing press is not read by the field. The agent's talk: `def_dri_talk` returns 0 until the
Rookie has joined; from then on it opens the question (`DRI_ASK`), and "Talk" calls `fieldstg_actor_talk_to` again with
`dri.bypass` set. Each line replays a classic Tamer by title (`$DriRival<n>*`); the 11 px font has no comma, colon, slash
or apostrophe, so the texts avoid them.

**Floating Rush panel.** The results screen is its own stage, so the field reuses its logic without it
(`fieldstg_rush_float`, `DEF_RUSH_FLOAT`, `DW3_RUSH_FLOAT=0/1`): the battle is built as usual, everything is restored,
then `def_rush_results`, `def_rush_apply_quiet` (the screen's calls in the same order) and `def_rush_panel_open`
(`definitive_rush.c`, `definitive_rush_panel.c`, layer 0x1004, at the top of the screen; `event_running` is never set,
which keeps the player free; `DEF_RUSH_FLOAT_FRAMES`). The level-up stat gain is drawn at a different moment than on the
screen, so the numbers may differ. Test: `tests/port/definitive_rush.py`.

**Not verified in game:** the HUNT record through a real card at a Guardromon save point, the forge line on the shop
screen, and the Guide's arrival notice.
