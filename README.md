# Digimon World 3: Definitive Edition

*Digimon World 3* as it could have been: battles rebuilt around elemental weaknesses in the style of *Persona*, a
progression that respects your time, and the comforts a 2002 game never had. It runs natively, with no emulator, on
RK3326 handhelds (the R36S, the XF35H and their clones) and on PC.

- **Fights you think through.** Find the weakness, stagger the enemy, earn one more action, pass the baton.
- **Less grinding, same journey.** The late game arrives around level 40-50, where most players used to stop.
- **A reason to roam.** Every battle can drop Hunt Materials, a long winning streak in one region brings out its Rare
  Target, and what you collect pays for forged gear, Black Market replicas and the DRI Gauntlet.
- **Nothing hidden.** Enemy weaknesses, digivolution requirements, quest/auction progress (`GUIDE`) and what each
  technique really does are all on screen.
- **New Game choices.** Play as **Kail** or Junior (**Girls Power**), and optionally turn on **Hard Mode** (two-Digimon
  party).
- **Cross-play saves.** Free **Cloud Sync** (`SAVES & CLOUD`) with 6-digit device pairing keeps `card1.mcd` synced across
  handhelds and PC, backed by up to 100 local rotating snapshots.
- **Play anywhere cleanly.** Console-first C launcher (`dw3-launcher`), modern controls, and on PC: GPU hardware
  rendering, shaders, **16:9 Widescreen** (battles and field) and edge-to-edge **Fullscreen `FILL`** scaling.
- **Your old save still works.**

It is built on the [Digimon World 2003 decompilation](https://github.com/gascarcella/dw2003recomp) and its PC port,
which are the upstream project's work; this fork adds the handheld path and the Definitive Edition.

> [!IMPORTANT]
> **This repository contains no game data.** You need your own copy of *Digimon World 2003 (Europe) (En,Fr,De,Es,It)*,
> raw `.bin` + `.cue`, MODE2/2352, 692,146,560 bytes, SHA-1 `457cb233349ba841e03b33d8060f8fbcadd45cb3`.
> This project is not affiliated with or endorsed by Bandai, Bandai Namco or Sony.

<a href="https://recomplauncher.com"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

<p align="center"><img src="docs/images/definitive/banner.svg" width="800" alt="Digimon World 3: Definitive Edition. Find the weakness. Stagger. One more. Pass the baton."></p>

## Contents

- [Quick start](#quick-start) · [Where it runs](#where-it-runs)
- Player guide: [Combat](#combat) · [Progression](#progression) · [The hunt](#the-hunt) · [Comfort, Cross-Play and PC Features](#comfort-cross-play-and-pc-features)
- [Reference](#reference)
  - [How the definitive layers work](#how-the-definitive-layers-work-read-this-first) · [Buff and debuff tiers](#buff-and-debuff-tiers-no-stacking) · [Status, Poison and revive](#status-poison-and-revive-rework) · [Super Hard Mode](#super-hard-mode-the-demon-bell)
  - [Hunt constants, prices, forge, Black Market, DRI, Guide](#the-hunt-constants-and-hooks)
  - [The Velvet Room](#the-velvet-room) · [The story layer](#the-story-layer-velvet-room-in-the-main-story) · [Super Rare hunts](#super-rare-hunts) · [Recolour](#recolour-palette-swap-of-rare-foes) · [Hero skins](#hero-skins)
  - [Blast, Purify and Chosen Evolution](#blast-purify-and-chosen-evolution) · [Girls Power](#girls-power-playable-kaili) · [Battle HUD](#battle-hud-and-launcher-options)
  - [Texts and translation](#texts-and-translation-en--pt-br) · [Save format](#save-format-memory-card-records)
  - [Building and running](#building-and-running-the-port) · [Mods](#mods-portmods) · [Tools](#tools-tools) · [Tests](#tests) · [Debugging and the MCP](#debugging-and-the-mcp)
  - [Launcher, cloud sync, handhelds, Android, PortMaster](#launcher-cloud-sync-handhelds-android-portmaster)
  - [Planned / backlog](#planned--backlog) · [Pending work / open decisions](#pending-work--open-decisions)
  - [History / decisions log](#history--decisions-log) (closed plans, PortMaster CFW research, muOS, Android, Persona brief, telemetry PRD) · design records: [Combat](#combat-design-record-details-from-the-prds-that-the-sections-above-summarise), [Status balance](#balance-measurements-behind-the-status-rework-status-balance-reportmd-simulated-from-the-disc-tables-and-the-ports-formulas), [Hunt/forge/DRI](#hunt-forge-black-market-dri-and-rush-design-record-prd-4-layers-a-i-done-2026-11-13), [Velvet details](#velvet-room-details-decisions-and-history), [Blast design](#blast-how-it-was-designed-the-owners-text-and-the-decisions), [Girls Power build](#girls-power-how-it-was-built-decisions-and-the-hand-drawing-workflow), [Battle HUD record](#battle-hud-design-record-plan-8-the-opt-in-mod-that-became-always-on-and-the-glyph-fix-plan-9)
- [Building and documentation](#building-and-documentation) · [Contributing](#contributing-and-support) · [Credits](#credits-and-license)

## Quick start

### Handhelds (PortMaster)
1. Download `dw3.zip` from the [releases](https://github.com/al4xdev/digimon-world-3-definitive-edition/releases) and unpack it into your PortMaster
   `ports` directory.
2. Copy your disc image into `ports/dw3/game/`.
3. Start *Digimon World 3* from the ports menu.

Install details, tested devices and known issues are on the player page, [`portmaster/README.md`](portmaster/README.md).

### PC (Linux & Windows)
1. Build or unpack the PC release (AppImage on Linux, `.zip` on Windows) or run `./launcher/dw3-launcher --sdl` from a built checkout.
2. Pick your `.cue` / `.bin` disc image on first launch (or change it anytime under `GAME > DISC IMAGE` — hidden on PortMaster).
3. Open `VIDEO` to toggle **Widescreen (16:9)**, **Fullscreen**, **`FILL` vs `INTEGER`** scaling, **GPU / Software** renderer and screen **Filters**.

**Coming from an older version?** Your save opens and plays. To give it the new MP and TP tables, run
`tools/convert_save_mp.py card1.mcd` once (it keeps a backup).

## Where it runs

- **RK3326 handhelds** (XF35H, R36S and clones, 640×480 `/dev/fb0` + GLES): tuned end-to-end on real hardware.
- **Linux & Windows PC** (Wayland, X11, Windows x86_64): runs with an SDL3 **GPU hardware renderer** (Vulkan/D3D12/Metal),
  16:9 Widescreen in battles and the field, HighDPI fullscreen `FILL` scaling and shader filters (`SHARP`, `SCANLINES`,
  `CRT`, `SMOOTH`).

If you try it on another device, a report in the [issue tracker](https://github.com/al4xdev/digimon-world-3-definitive-edition/issues) helps a lot.

## Combat

### Weaknesses

Seven elements in five families: **Fire**, **Water + Ice**, **Wind**, **Thunder + Machine** and **Dark**. The two
elements of a pair always share one value on an enemy (both weak, both neutral or both resisted), so a pair counts as
one weakness or one resistance. Every enemy has its own, counted in families:

- most enemies are weak to two families and resist one;
- about one in five enemies up to level 30 is a "wall", weak to a single family and resisting two: find the weakness
  or struggle;
- from level 45 on, enemies are weak to two families and resist two, and so are bosses;
- the final boss is weak to one family and to Dark, and resists three;
- Dark is rare on both sides: only a handful of enemies are weak to it, and as few resist it. The four other families
  come out about even: of the 193 species, 93 are weak to Fire, 87 to Water + Ice, 94 to Wind and 93 to Thunder +
  Machine.

Each family has one resistance value: 80 or less is a weakness, 250 or more is resisted, anything between is neutral
(the enemy table uses 60 for a weakness and 100 to 115 for neutral). There is no cap on weaknesses, and they are read
at the moment of the hit, so a debuff that takes a neutral family to 80 or less adds one in the middle of a fight (see
[Buffs and debuffs](#buffs-and-debuffs)).

<p align="center"><img src="docs/images/definitive/weakness.svg" width="800" alt="The five element families. An example enemy resists Fire and is weak to Water and Ice; after a Fire hit and an Ice hit, Inspect shows Fire resisted, Water + Ice weak and the rest ???."></p>

### Stagger and One More

Hitting a weakness with an elemental technique **staggers** the enemy (it takes its tired pose) and gives you **one
more** action. Each weak family gives its extra action once per round (a round starts with each normal action; an
extra one does not start a new round), so two weaknesses can chain into several actions in a row, but Ice after Water
gives none. Hitting a weakness again keeps the enemy staggered. Any other blow ends it, and still gets the stagger
bonus unless it hits a resistance; the enemy's own action ends it too. A blow with no element, or the element a crest
lends a basic attack, never staggers.

- **Baton Pass**: while the enemy is staggered, the Tag menu offers a free switch that keeps your turn (once per round).
  A plain switch still costs the turn, unless Relay made it free.
- **OVERFINISH**: a blow that knocks the enemy out and earns one more action leaves it lying there for that action.
  Every hit on it then shows **OVERFINISH!** and pays Bits worth 10% of its damage, added to the battle's.
- **You hear and see how a technique landed**: a slanted **WEAK!** or **RESIST!** over the enemy, and a distinct
  sound for a weakness, a neutral hit and a resisted one.
- **You open the fight** unless you are ambushed. Speed no longer decides who starts; a much faster side acts twice,
  and the HUD shows "Enemy x2" when it is the enemy.

<p align="center"><img src="docs/images/definitive/stagger.svg" width="800" alt="Hit a weakness: WEAK!, the enemy staggers, ONE MORE! Then hit a weakness again, Baton Pass, or any other blow, which gets the bonus and ends the stagger."></p>

### Damage

Damage is still the game's own formula. The Definitive Edition replaces the one step where the original read
elemental resistances, and adds a stagger bonus for blows with no element.

**The multiplier.** When your elemental technique hits an enemy:

| The hit | Multiplier |
|---|---|
| Neutral element | 1.0 |
| Weakness | 1.0 + 0.5 + affinity |
| Neutral element, staggered target | 1.0 + 0.2 + affinity |
| Weakness, staggered target | 1.0 + 0.5 + 0.2 + 2 × affinity |
| Resisted element | 0.5, no bonus, staggered or not |
| A blow with no element, staggered target | 1.0 + 0.2, no affinity |

**Affinity** is what your Digimon's old elemental "tolerance" stats became: they no longer defend, they strengthen your
attacks of that element. It is added once for a weakness and once for a staggered target, and never otherwise: on a
neutral hit against an enemy that is not staggered it does nothing. Each family has a ceiling, scaled by the stat's
share of its maximum, 999: Fire and Wind count on their own, up to 0.3; Water with Ice and Thunder with Machine count as
pairs, on the two stats' sum out of 1998, up to 0.6 (a Water and an Ice technique get the same affinity); Dark counts on
its own, up to 0.6. The strongest hit there is, a weakness on a staggered target with full affinity, is 2.3 for Fire or
Wind and 2.9 for the pairs and Dark.

One stat defends against everything: enemy magic is resisted by your **Defense**, and Spirit is purely your magic
attack. Your elemental stats no longer cut the damage enemy elements do to you.

<p align="center"><img src="docs/images/definitive/damage.svg" width="800" alt="The multiplier: resisted 0.5, neutral 1.0, weakness 1.5 to 2.1, neutral element on a staggered target 1.2 to 1.8, weakness on a staggered target 1.7 to 2.9, no element on a staggered target 1.2. Affinity ceilings: Fire 0.3, Water + Ice 0.6, Wind 0.3, Thunder + Machine 0.6, Dark 0.6."></p>

#### The whole calculation

In the order the game runs it, all integer maths (every original step in full is in
[`docs/MECHANICS.md`](docs/MECHANICS.md), section 2):

1. **Base.**
   - Physical: `power × Strength ÷ Defense`.
   - Magic: `power × (Spirit × 50 ÷ D + 50) ÷ 100`, kept between half and twice the power. D is the enemy's Spirit
     when you attack, and your Defense when it attacks.
   - An enemy's power is first scaled by its battle's stat scale (`× stat_scale ÷ 16`).
   - Buffs and debuffs: physical ones act on Strength and Defense, as in the original. For magic, the attacker's Spirit
     is scaled by its Strength modifier and a defending enemy's Spirit by its Defense modifier (the final boss keeps
     its own); your Defense already carries yours.
2. **Field, type, boosts.** A field of the technique's element adds `field power ÷ 128` of the damage, the opposite
   element loses `field power ÷ 256`. A type match adds 50% (in magic, after steps 3 and 4). Physical blows add the
   attacker's power boost (`× power_up ÷ 64`).
3. **Element: the Definitive step.**
   - Your elemental technique on an enemy: `× the multiplier` above.
   - An enemy's elemental technique on you: physical `+ damage × element power × 2 ÷ 100`, magic unchanged. Both are
     the original formulas with your resistance fixed at the neutral 100, so your elemental stats no longer count.
   - A crest's element on your basic attack keeps the original bonus, read against the enemy's resistance.
4. **No element, staggered**: a blow with no element on a staggered enemy, `× 1.2`.
5. **Critical and crests**: a multi-hit crest `× 0.4` (physical), a critical hit adds about 50% to 150%, a Guard crest
   takes off a flat amount.
6. **Caps**: never more than 5 × the base, never more than 9999.

<p align="center"><img src="docs/images/definitive/damage-pipeline.svg" width="800" alt="The damage pipeline: 1 base, 2 field, type and boosts, 3 the element step (the Definitive multiplier), 4 times 1.2 for a blow with no element on a staggered enemy, 5 critical and crests, 6 caps."></p>

**A worked example**, checked against the port's code. Your Digimon has Spirit 180 and a Fire stat of 333. It casts a
Fire technique of power 100 at an enemy that fights with Spirit 120 and is weak to Fire; no field, no type match, no
critical hit.

| Step | Calculation | Result |
|---|---|---|
| Base | 100 × (180 × 50 ÷ 120 + 50) ÷ 100 = 100 × (75 + 50) ÷ 100 | 125 |
| Affinity | 0.3 × 333 ÷ 999 | 0.1 |
| First hit, a weakness | 125 × (1.0 + 0.5 + 0.1) = 125 × 1.6 | **200** |
| The One More: the same technique on the staggered enemy | 125 × (1.0 + 0.5 + 0.1 + 0.2 + 0.1) = 125 × 1.9 = 237.5 | **237** |
| The same hit if Fire were neutral, or resisted | 125 × 1.0, or 125 × 0.5 = 62.5 | 125, or 62 |

The second hit gives no further action (Fire already gave this round's), but it keeps the enemy staggered. The other
way round, an enemy's magic of power 100 (stat scale 16, so × 1) with Spirit 150 against your Defense 120 does
100 × (150 × 50 ÷ 120 + 50) ÷ 100 = 100 × (62 + 50) ÷ 100 = **112**, whatever your elemental stats.

### Buffs and debuffs

- An **attack buff** raises physical and magic damage alike; a **defense buff or debuff** counts against both kinds of
  hit. A magic party gets as much from them as a physical one.
- **Bosses are immune to status** (poison, sleep and the like) **but not to debuffs**: lowering a boss's defense or
  elemental resistance is part of the plan.
- **Resist Down** lowers the enemy's resistance to your strongest family (the one with your highest affinity, a pair
  counting both its elements) by 80, and **Mega Break** your two strongest families by 100 each, for three of the
  enemy's turns. On a neutral family that opens a weakness that was not there; a resisted one (300) only drops to
  neutral. In an enemy's hands 202, 203 and 207 all act as Resist Down (80, your best family) and 208 as Mega Break
  (100, your two best), for 3 turns; Libra and Relay mean nothing to an enemy (owner's choice; `def_tech_effect`).
- The HUD shows every active buff and debuff as an up or down arrow beside the Digimon, on both sides; a lowered
  elemental resistance shows as a down arrow in the element's colour, one per family.

### Knowing your enemy

- **Inspect** (a seventh button on the battle HUD, L2 by default) and **Start** show the enemy's five element families,
  but only what you have found out: hitting a weakness or a resistance with an elemental technique teaches both
  elements of its family, everything else shows `???`. In Inspect a family you know shows its current state (weak,
  neutral or resisted, with a Resist Down or Mega Break already counted), not the state it had when you learnt it.
- **Libra** reveals all of an enemy's elements at once.
- The **Bestiary** (field menu) keeps what you learnt of every species, saved with your game, and lists its Hunt
  Materials: the common one with how many you hold, and its rare items as a collection (see [The hunt](#the-hunt)).
- Every list of a Digimon's techniques shows each technique's element picture after its name: the battle's technique menu, the battle's Tag and digivolve status pages, the field status screen's digivolve page and the Digi-Lab's three technique screens (`def_tech_icon`). A technique with no element (a physical blow, a support) shows none. It and the status screens keep the seven element pictures.

![Bestiary, the weakness panel in battle and the technique menu's element](docs/images/definitive/definitive_ui.png)

### Techniques and MP

- Remade techniques: **Libra** (was Speed Up), **Relay** (was Mega Boost: the next switch is free, 50 MP), **Resist
  Down** (was Slow Down) and **Mega Break**.
- More techniques carry a secondary effect, so each form has its own role in a fight.
- Every technique description was rewritten from what the technique actually does, effect first: "Fire magic. Sleep
  chance."
- MP matters now, so the rookies that were starved of it are not any more: Kotemon, Kumamon, Monmon and Guilmon start
  with 140 MP and grow at the same pace as the others.

### Battle HUD

A direct-button HUD replaces the battle's list menu: each action has its own button in a pad-shaped layout (**Item** on `L1` left, **Tech** on `R1` right, **Fight** on `B`, **Run** on `X`, **Digivolve** on `Y`, **Tag** on `A`, **Inspect** on `L2`, **Blast** on `R2`), rebindable from `BATTLE HUD` in the launcher. It remembers the last technique and item, buffers presses made just before the menu opens, and skips battle message boxes by default (`BATTLE TEXT: SKIP`). Controls use the modern **Circle ↔ Triangle** rotation by default (`Confirm` at the bottom, `Back` on the right), with selectable **Xbox / Nintendo / PS1** button glyphs.

![Persona-style Direct Battle HUD](docs/images/battle_hud.png)

## Progression

The original is known for ending at level 40-50 after exhausting grinding. The Definitive Edition brings the late game
to that same range without the grind.

### Training points

| Level | TP per level | Original |
|---|---|---|
| 2-20 | 7 | 5 |
| 21-40 | 9 | 5 |
| 41-50 | 30 | 5 |
| 51 and up | 0 | 5 |

By level 50 a Digimon has earned more TP than the original gives by level 99 (613 against 490). Levels 41 to 50 are
where you shape its stats the way you want. TP still caps at 99 in hand, so you train along the way instead of hoarding
for the last gym; the results screen tells you when a level-up lost TP to the cap.

### Training

- **Guaranteed training**: after three sessions without a perfect result, the fourth succeeds on every round. A perfect
  session resets the count, and the training screen shows where you stand.
- **Resistance training no longer costs a stat.**

### Experience

- A rookie levels at the original pace; **digivolution forms level 14 times faster**, so trying a new form is fun
  instead of a chore.
- **The party grows together**: a party member that stayed out of the fight earns the same experience as one that
  fought (one knocked out at the end earns none), and each Digimon's three chosen forms earn form experience whether
  they fought or not.
- **Card boosters drop 15 at a time**, so building a deck for the card game does not take a second playthrough.
- These were launcher mods (party experience, XP boost); they are the game's own rules now, with no switch. The Evo
  Buff mod is gone: weakness, stagger and the chain do its job.

### Digivolution

- **Requirements on screen**: the Digi-Lab shows what each next form needs and how far you are, for example
  "Greymon Lv 5 (3/5)" or "Strength 180 (142/180)". No guide needed.
- **Short animations**: a form's digivolution plays in full the first time, then a short version; hold Circle or Cross
  to skip it, even the first time.

### Hard Mode

Choose it on New Game: your party is two Digimon instead of three. The third partner of the starting pack stays in the
lab, and the lab keeps the third slot closed. The choice is saved with the game.

## The hunt

Optional, and open all game long: every battle can drop Hunt Materials, and a winning streak in one region brings out
its rarest prey. The materials then pay for the forge, the Black Market and the DRI Gauntlet.

<p align="center"><img src="docs/images/definitive/hunt.svg" width="800" alt="The Rare Target loop: 1 the regional chain, past 5 wins a chance rising to certain at 10; 2 the Rare Target, a boss battle at the region's level shown with its region's numeral, such as Datamon IV; 3 its rewards, the region's numbered rare item, an R-Booster pack and far more experience; 4 the Bestiary keeps the collection."></p>

### The regional chain and the Rare Target

- Every random battle you win adds one to the **hunt chain** of the region you are in (an area of the world map; the
  server counts as its own). Moving to another region, a Guardromon teleport and fighting the Rare Target reset it.
  Battles you do not win do not count; a battle the Overlevel Rush skips **does** count and leaves materials, like a
  won one (`def_hunt_skipped`, called by `def_rush_results`, commit `3fb236d`, 2026-11-14; the first PRD said the
  opposite, to avoid a Rare Target while backtracking). The chain is saved with your game.
- Past **5** wins, each random encounter in that region may be its **Rare Target** instead: a 20% chance at 6 wins,
  20% more per win, and **certain** at 10.
- The Rare Target is **one Digimon, alone** (no trios): one enemy of an existing battle of the region (a fixed battle
  and slot per region), fought on the region's own stage and music. Its level is the region's average encounter level
  **+ 5** (it can lower a strong boss in a weak place) and its HP is **x4** (`DEF_HUNT_RARE_HP_MULT`, capped at 30000).
  A **RARE!** callout announces it, its colours are turned 120 degrees on the colour wheel (see
  [Recolour](#recolour-palette-swap-of-rare-foes)), the Overlevel Rush never offers to skip it, and you can always run
  from it (except inside an active Super Rare territory, see [Super Rare hunts](#super-rare-hunts)).
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
- Rares: 71, one per region with a Rare Target, up to 99 of each. 149 species carry a common material. (The numbers
  were regenerated by `tools/gen_hunt_targets.py`; the old "Datamon Core I-XIII" example no longer applies.)
- The results screen lists each one after the item: "Hunt material: Bit Shard x2" for commons, "Rare material:
  <species> Core" for a rare.
- A Rare Target pays ten times the experience of the encounter it replaced. Its R-Booster is 01 below level 25, 02
  from 25, 03 from 40, 04 from 55 and 05 from 70.

### Spending them

What you collect pays for better gear: every shop is also a forge, a trader in the first town runs a Black Market,
and the DRI Agents open the Gauntlet.

<p align="center"><img src="docs/images/definitive/economy.svg" width="800" alt="The economy loop: common materials from random battles and rare items from Rare Targets feed the forge in every shop, the Black Market (one rare for 12 commons, replicas for two rares of different bosses plus Bits) and the DRI Gauntlet, which give shop equipment and its 27 variants, 16 replicas that resell for 0 and the Power accessories. Shop gear from 10000 Bits costs 150% and resells for 5000 plus 8% of its old price above 10000."></p>

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
see [The Velvet Room](#the-velvet-room)) runs the Black Market. Talking
to them opens two tabs (L1/R1):

- **Exchange**: one rare item for 12 of any common material you pick, as long as all 12 fit under the 99 cap.
- **Replicas**: 16 pieces of equipment no shop sells, as good as the original: the seven Power S accessories,
  Machinegun, Magician Rod, Ghost Fang, Pulse Guard, Drill Horn, Chain Saw, Naginata, Recover Crest and MP Mega Proxy.
  Each costs one Core of each of its own two bosses, chosen by its theme and strength (the Fire Power S takes Deltamon
  and BK WarGrowlmon, the Machinegun Megadramon and Puppetmon), shown with what you hold ("Takes: Deltamon 1/1,
  BK WarGrowlmon 0/1", orange while one is missing), plus 20 times the original's selling price in Bits: 20,000 to
  40,000. No boss serves two replicas.
  A replica resells for 0 Bits, so the shop does not even offer to buy it.

### The DRI Gauntlet

Talk to a DRI Agent: until their Rookie is in your party they give their DDNA quest, at any party level (the game's
"too busy" and "you don't need it" level window is gone), and tell where the Digimon with the DDNA is found. Once the
Rookie has joined (at once if it was one of your starters), their lines play and then the list of simulations opens. Each replays a classic Tamer's battle data, who has a word for you before the fight and after it. Each is one battle against an evolution line's three stages,
Champion, Ultimate and Mega: when one falls the next comes in at full HP with a **DIGIVOLVE!** callout, while your
party keeps the HP, MP and items it has spent. There is no running away and no game over: lose and you are back on the
field with your HP and MP as they were before (spent items stay spent). A simulation is unlocked once, for good, by
handing in its materials.

| Line | Stages and levels | Unlock | Rewards |
|---|---|---|---|
| Terriermon (Guardromon) | Guardromon 24, Andromon 27, BK MegaGargomon 30 | Code Chip ×15, Guardromon Core | Metal Power 1, 2, S |
| Veemon | Devidramon 32, Paildramon 35, BK Imperialdramon 38 | Core Crystal ×20, Paildramon Core | Bolt Power 1, 2, S |
| Patamon | Airdramon 40, MagnaAngemon 43, BK Seraphimon 46 | Tera Prism ×25, MagnaAngemon Core, Raremon Core | Wind Power 1, 2, S |
| Agumon | Tyrannomon 48, MetalGreymon 51, BK WarGreymon 54 | Gold Digizoid ×35, MetalGreymon Core, BK WarGreymon Core | Fire Power 1, 2, S |
| Guilmon | Flarerizamon 56, WarGrowlmon 59, BK WarGrowlmon 62 | Obsidian Digizoid ×40, BK WarGrowlmon Core, WarGrowlmon Core | Dark Power 1, 2, S |

Every stage you fell reaches a tier of the line's Power accessory (one stage: 1, two: 2, all three: S), even if you
lose afterwards. Each tier is given once, the first time you reach it, so a first full clear gives all three; the list
keeps your best stage. A replay pays the battle's experience and Bits, but no accessory.

## Comfort, Cross-Play and PC Features

- **Playable Kail (Girls Power).** On New Game, choose **Kail** or **Junior** right after the difficulty prompt: Kail
  and Junior swap roles across field sprites, cutscenes, field actions (tree kick, fishing) and menus (`port/game/girls_power.c`),
  saved per slot without altering old saves.
- **In-game Guide (`GUIDE`).** Open `GUIDE` from the field menu for two tabs (`L1`/`R1`): your current main quest step
  and open side missions (El Dorado ID, Folder Bag, Tree Boots, Fishing Pole), plus all 16 Asuka City Cargo Tower
  auctions (confirm twice on an open auction to travel straight to El Dorado). Live banners also slide in when quest
  steps advance or auctions open.
- **Cross-Play Cloud Sync & Local Backups (`SAVES & CLOUD`).** Sync `card1.mcd` across handhelds and PC through a
  zero-cost Cloudflare Worker (`cloud-sync/`): register a device, pair a second device via a **6-digit 2FA code** with
  explicit host approval, resolve conflicts cleanly, revoke lost devices, and keep **1–100 automatic local rotating
  snapshots** with one-button restore.
- **PC Graphics, Widescreen & Fullscreen `FILL`.** On PC (`--sdl`), the GPU renderer enables **16:9 Widescreen** in
  both **battles and the field** (menus and movies stay 4:3), **HighDPI** Wayland/X11/Windows windowing, shader filters
  (`NONE`, `SHARP`, `SCANLINES`, `CRT`, `SMOOTH`) and **`SCALING: FILL / INTEGER`** so fullscreen fills your monitor
  cleanly instead of stopping at integer 240p multiples.
- **Console-First C Launcher (`dw3-launcher`).** A lightweight C99 dashboard (`launcher/`) with zero Python runtime
  dependency on the menu: animated splash screen, sliding cursor, Xbox-style zoom transitions, live Digital World
  backdrop, strict duplicate-button protection on `CONTROLS` and `BATTLE HUD` (`START` to rebind, `SELECT` to reset),
  live remapped navigation, built-in D-Pad `.cue`/`.bin` browser (outside PortMaster), and the **Trio Runner** Chrome-Dino-style
  loading minigame (Kumamon, Guilmon and Agumon) while the disc verifies.
- **The Blast Gauge is saved** (eight s16 values in the slot's tail) and is now the game's Blast button; see
  [Blast, Purify and Chosen Evolution](#blast-purify-and-chosen-evolution).
- **Battle text that does not stop the fight**: message boxes skip by default (`BATTLE TEXT: SKIP`) without waiting for a button.
- **Overlevel Rush**: when your strongest Digimon is 8 or more levels above every enemy of a random battle, the game
  offers to skip it, at the top of the screen while you keep walking (confirm takes it, back or five seconds fights).
  Take it and the rewards a win by your lead Digimon would give (experience, Bits, the drop and Hunt Materials) are
  yours at once: a panel slides in at the top of the screen and lists them for three seconds while you keep walking
  (+EXP and LV UP! for each member, NEW TECH!, TP FULL!, the Bits, the item and the materials). A battle you fight
  still ends on the results screen.
- **A Guardromon network**: at any Guardromon, `SAVE` or `TELEPORT` to the others you have met, a **map objective
  marker**, **fast-forward**, button glyphs that match your pad, and full remapping.
- **50/60 Hz support**: `--refresh 60` / `video.refresh: 60` engages the game's compensated 60 Hz mode (`records_60hz`).

---

# Reference

Everything below is the long form: rules, numbers, files, save bytes, tests and pitfalls, so nothing depends on
remembering it. Where a point could not be verified while writing, it says so ("unverified").

## How the definitive layers work (read this first)

- The "definitive" layers are **always on** (there is no mod switch). They are gated by `def_on()`, which is off under a
  `--script` replay, so scripted replays and the golden tests keep the original game.
- **Every number is a constant in `port/game/definitive.h`.** Tuning the balance means editing that file (the balance
  itself is the owner's call).
- The game's C calls hooks under `#ifdef PC_PORT`; the prototypes are in `include/port.h`. The PS1 matching build and the
  goldens must stay unchanged, so no hook may change behaviour outside `PC_PORT`.
- Main files: `port/game/definitive.c` (combat, training, XP, status), `definitive_bestiary.c` (card records),
  `definitive_hunt.c`, `definitive_superhunt*.c`, `definitive_materials.c`, `definitive_market*.c`, `definitive_dri*.c`,
  `definitive_economy.c`, `definitive_prices.c`, `definitive_recipes.c`, `definitive_velvet.c`, `definitive_purify.c`,
  `definitive_recolor.c`, `definitive_skins.c`, `definitive_travel.c`, `definitive_evohint.c`, `definitive_callout.c`,
  `definitive_text.c` / `definitive_text_pt.inc`, `girls_power.c`, `game_mods.c`.
- Rules every layer keeps: an old save opens and plays; the player's MP stays limited (Relay and technique costs are
  real); a boss is immune to status (poison, sleep and the like) but not to stat debuffs nor to elemental damage. New
  code never assumes three party members: ask `gamestate_data.funcs.get_party_member` (Hard Mode runs with two).

### The combat layers, with the owner's decisions

The combat PRD was built in layers 0-10 (the older PRDs 2 and 3 added layers A-J and the mod-to-native move):

- **0** closed without a baseline table; the battle log (`DW3_BATTLE_LOG`) is enough to measure.
- **1** text override: hooks `font_get_entry` (`src/main/font.c`), so any text can be replaced from
  `tools/text_override/`.
- **2-3** weakness table and stagger/chain (the multiplier and the Baton Pass above).
- **4** turn order: the player opens unless ambushed; the enemy's first turn comes after its full delay.
- **5** the Overfinish (10% of the damage in Bits, `DEF_OVERFINISH_BITS_PCT`).
- **6** the four remade techniques: Libra (202), Relay (203), Resist Down (207), Mega Break (208).
- **7** Defense defends against magic; the old "Tolerance" texts became "Affinity".
- **8** the MP table: Kotemon, Kumamon, Monmon and Guilmon 140 MP, gain 57; Veemon 150; Guilmon gain 60.
- **9** the bestiary save.
- **10** balance: the owner tunes it.
- PRD 3: the launcher mods `evo_buff`, `party_xp` and `xp_boost` were removed and became the game's own rules.

### Constants worth knowing

`DEF_WEAK_MAX` 80, `DEF_RESIST_MIN` 250 (a family's resistance: weak at 80 or less, resisted at 250 or more). Resisted
hits are a fixed 0.5. Affinity ceilings: single 0.3, pair 0.6, Dark 0.6. Weak +0.5, stagger +0.2. The player's reference
resistance is 100. Relay costs 50 MP. Resist Down is a delta of 80 and Mega Break 100, for 3 enemy turns. The Baton Pass
is free (`DEF_BATON_PASS_MP` 0) and offered only while the enemy is staggered. **Overlevel gap 8** (`DEF_OVERLEVEL_GAP`,
it was 15). The Rush question lasts 300 ticks, the Rush panel 175 ticks and 10 rows.

Their names: multipliers and affinities are fixed point in thousandths (`DEF_MULT_ONE` 1000): `DEF_MULT_WEAK` 500,
`DEF_MULT_STAGGER` 200, `DEF_MULT_RESISTED` 500, `DEF_AFF_SINGLE_MAX` 300 (Fire, Wind; resist 999), `DEF_AFF_PAIR_MAX`
600 (Water+Ice, Thunder+Machine; the pair's sum 1998), `DEF_AFF_DARK_MAX` 600. `DEF_PLAYER_REF_RESIST` 100 is what an
enemy's element is measured against when it hits the player. Resist Down / Mega Break: `DEF_BREAK_SLOW_DELTA` 80,
`DEF_BREAK_MEGA_DELTA` 100, `DEF_BREAK_TURNS` 3; on a boss the original made immune, a stat debuff is scaled by
`DEF_BOSS_DEBUFF_PCT` (100). `DEF_RELAY_MP` 50. TP: `DEF_TP_GAIN_LV_2_20` 7, `DEF_TP_GAIN_LV_21_40` 9,
`DEF_TP_GAIN_LV_41_50` 30, `DEF_TP_GAIN_LV_51_UP` 0, `DEF_TP_CAP` 99. Short digivolution: hold Circle or Cross
`DEF_EVO_SKIP_HOLD_FRAMES` (4) frames (the PRD's switch `DEF_EVO_ANIM_SKIP` never existed in code).

### Combat design record (details from the PRDs that the sections above summarise)

Working rules of the PRDs (kept because they explain why the code looks as it does):

- **Not a mod.** Always on, no `mod.json` option, no `port_mod_*` flag. Game C is touched only under `#ifdef PC_PORT`
  with short hooks; new battle state goes into `game_savestate` (`port/game/game_mods.c`). Disc data is never edited or
  versioned: enemies (`SDIGIEDT.PRO`) and texts (`ES*.BIN`) change through port override tables, techniques and Digimon
  (`src/main/records.c`, matching source) through a patch applied at port boot. Fractions are fixed point (the game uses
  no float). Removing a file: move it to `/tmp`, never `rm`.
- **Vocabulary.** Elements, fixed order: Fire, Water, Ice, Wind, Thunder, Machine, Dark = `resists[0..6]`. In a technique
  `RecordsTechnique.defense_stat` 2..8 is the element (`resists[defense_stat - 2]`), 1 = none; `RecordsTechnique.element`
  is the class (2 physical, 3 magic, 4 heal, 5 buff/debuff, 6 field). Stats: `stats[0]` Strength, `[1]` Defense, `[2]`
  Spirit, `[3]` Wisdom, `[4]` Speed. `side == 0` is the player attacking; `fightstg_rules.stats[0]` is the attacker and
  `[1]` the target. Only the final boss has `state.type == 6`; other bosses are recognised by `records_state.blocked[]`.
- **Layer 0 decision (2026-11-12):** closed without a baseline table, no autoplay and no `DW3_FORCE_BATTLE`; the owner
  balances by playing. Rule checks are host tests with calculation, not play. Layer 10 (balance) is the owner's, done on the
  constants of `definitive.h`. The owner's guidance for it: if a boss falls in 2-3 turns, **raise boss HP first** (a
  constant multiplier on `RecordsEnemy.hp` of boss fights) and only then touch magic; only if the magic team still loses
  badly to the physical one revisit the `[power/2, 2 x power]` clamp of `get_special_damage`.
- **The battle log (`DW3_BATTLE_LOG=<file>`, layer 0's measuring tool).** One plain-text line per event (`definitive.c`
  `LOG`): `battle` (id, kind, type) with a line per party member and per enemy (digimon, level, HP, MP, scale); `turn`
  (side, number, member); `action` (side, kind, technique, class, element, power, MP); `damage` (value, the target's HP
  before and after) with a `mult` line (the multiplier and its terms: weak, open, resisted, affinity); `end battle`
  (result, party turns, enemy turns); plus the definitive events: `hit target N weak W stagger S -> S' extra_pending E`,
  `extra action granted`, `baton pass`, `relay swap`, `stagger closed by the enemy's turn`, `libra`, `resist down`,
  `overfinish damage`, `twice E Pe P` (the "Enemy x2" test), `blast gauge`, `mod` lines. The battle code calls
  `def_log_begin`, `def_log_turn`, `def_log_action` (`fightstg_80086A00.c`, `wfightmn_800A6440.c`) and `def_log_end`.
  The PRD's acceptance checks of layers 2-4 (chain, window, close, free swap) were read from this log. The first plan
  of measuring 4-6 bosses with a physical and a magic team (`baseline.md`) was dropped.
- **Layer 2 (HUD, Inspect, sound, "Enemy x2").** The HUD became mandatory (the list menu is unreachable). Three distinct
  sounds (weak, neutral, resisted) are picked from effects the battle already loads. The "Enemy x2" indicator shows when
  `E + Pe < P`, where E is the enemy's remaining turn delay in the queue, Pe = `get_delay(0x10, 0)` and P the player's
  remaining delay after the current action (`get_delay(0, 0)`); `get_delay` of type 0 draws nothing but **overwrites
  `fightstg_rules.stats[]`**, so call it only with the command menu open, never in the middle of a damage calculation.
- **Layer 3 (enemy table), the generator's rules** (`tools/gen_enemy_weaknesses.py`, fixed seed 20261112, `--apply` writes
  `port/game/definitive_enemies.c`, a readable proposal goes to `.dumps/tasks/persona/fraquezas.md`):
  - archetypes by family: plain 2 weak + 1 strong, wall 1 + 2 (about 20% of the mobs of level <= 30), late 2 + 2 (level >=
    45), boss 2 + 2 and final boss 1 + 3 (plus its Dark mark); the original asked 2 + 3 for bosses but only four
    families exist outside Dark, so it did not fit. Weak = 60, strong = 300 (or the disc's own value from 250), neutral
    100..115 (the current value when already there), the same for both elements of a pair;
  - the "house" family of a species (the family of its techniques' commonest element, else of its one high resist, else
    of its type) is never a weakness and is the natural strong resistance; a plain species that already has exactly two
    weak families keeps them, otherwise they are filled by opposition to the house element, then by balance, so the
    count per family evens out and two species of one battle do not share a pair;
  - **Dark:** weak only to Dark: BK Seraphimon, MagnaAngemon, Knightmon, Taomon (ids 447, 452, 178, 453); Dark as an extra
    weakness: Valkyrimon and the three Galacticmon phases (269, 442, 466, 467); the only eight that resist Dark:
    Armageddemon, VenomMyotismon, LadyDevimon, Phantomon, Mummymon, Ghoulmon, Gargoylemon, Apokarimon. The rule: resisting
    Dark is as rare as being weak to it so Dark stays a useful element;
  - `def_weakness_mask` marks the whole family when one of its values is at `DEF_WEAK_MAX` (80) or less, with **no median
    and no cap**: a Break that creates a third weakness hides no natural one. (The first design used a median and
    `DEF_WEAK_GAP` / `DEF_WEAK_COUNT`; they are gone.)
  - where the table enters: `fightstg_enemy_get_record` (`fightstg_8008D3B4.c`, found by `fightstg_enemy_find_record` in
    file 0x1CF) calls `def_enemy_override` on the record in memory, so the disc's SDIGIEDT is never edited; the field's
    own SDIGIEDT reads call it too (`definitive.c`), and a purification's dark form (no disc record) comes from
    `def_purify_record`. A row of `def_enemy_rows[]` is `{species id, 7 resists, dark}`; `dark` is the Dark mark, kept
    in the port's table because the disc record has no such field (`dark_mark`), and `def_weakness_mask(resists, dark)`
    adds Dark as an extra weakness outside any count. Stagger and chain state is a `DefChain` in `definitive_mask.c`
    (`staggered[3]`, `extra_pending`, `next_extra` / `cur_extra`, `window_used`, `swap_used`, `relay`); a Baton Pass or
    a Relay swap sets `extra_pending`, so the free swap keeps the turn.
- **Layer 4 (stagger).** One flag per enemy (`def_staggered[3]`), unrelated to HP. It opens when the player hits a weakness
  with an elemental technique (`defense_stat >= 2`); a physical blow with no element and a crest's element (`attack_element`)
  never open it. It closes when the staggered enemy takes the next player blow (which still gets the bonus) or acts. A
  multi-hit technique counts as one blow. The next player turn is rescheduled at command time (`wfightmn_party_turn`, end of
  step 2), before the blow resolves, so on a hit set `extra_pending` and when the action ends find the player's turn in the
  queue (`fightstg_events.find(2)`) and zero its delay (check the tie rule of `fightstg_events_take_next`). With 2 actions
  from speed and 2 weaknesses the chain is up to 6 actions: that is design, not a bug. The chain is the player's only; the
  PRD flagged "does the enemy get an extra action" as to confirm with the owner; the code answers no (checked 2026-11-18:
  `def_action_resolved` returns at `side != 0`, `def_chain_enemy_acts` only closes the stagger, and `extra_pending` is set
  only by the player's hits and free swaps in `definitive_mask.c`). The staggered pose comes from the flag on the enemy side (never from HP;
  the low-HP pose is removed on the player side), and the final boss shows its tired pose when `final_phase || staggered`.
  The Tag button shows a different icon while a free swap is available. A save state in the middle of a chain restores equal.
- **Layer 5 acceptance cases** used by the host test: neutral 1.0; weakness 1.5; weakness + stagger with full Fire affinity
  1.0 + 0.5 + 0.2 + 0.3 + 0.3 = 2.3; the Water + Ice pair with 999 + 0 gives 0.3 of affinity. The magic clamp
  `[power/2, 2 x power]` was deliberately not changed. Status rolls still read the player's resists as before.
- **Layer 5 hook points.** With `def_on` the multiplier replaces the element step of `fightstg_rules_scale_damage`
  (physical, `def_scale_physical`) and the resist bands of `fightstg_rules_get_special_damage` (magic, `def_scale_magic`)
  when the player attacks. When an enemy's element hits the player the old scale stays, with the player counted at
  `DEF_PLAYER_REF_RESIST` (100): physical `value + value * power * 2 / 100`, magic unchanged (the old bands at resist 100
  are 100%). Layer 7's Defense against magic is `def_magic_defense` in the same function. The staggered pose is set
  through `wfightmn_update_idle_anim`; the Baton Pass is a row of the swap menu (`fightstg_switch_menu_update`).
- **Layer 6 (techniques).** `records_techniques[]` is global: re-aiming a technique changes every form and enemy that uses
  it, so list who uses an ID first (`.dumps/tasks/persona/techs.txt`, regenerate with `tools/venv/bin/python
  .dumps/tasks/persona/dump.py`). Remade: 202 Speed Up -> Libra, 203 Mega Boost -> Relay (50 MP), 207 Slow Down -> elemental
  resist debuff, 208 Mega Break -> stronger or wider. Inheritance stays as it was (Digi-Lab mark, +20% MP, no new limit).
  Candidates the PRD listed to become elemental techniques (the owner approves before any is applied): 129, 130 (empty);
  194, 195 (no form learns them); 146, 148, 204, 211, 212 (enemy only: do not change the record); 131-136 (Busters per
  type); 213-219 (Fields); 190, 192; 127, 128. Goal: every evolution line reaches at least 2 elements and none all 6
  (unverified how far this went; see `definitive.c` `patch_techniques` and `def_mod_tech_spec`).
- **Layer 7 texts.** The status menu's "Tolerance" of the 7 elements became affinity; text that described Spirit as defence
  was corrected (`ESSTATUS` 92-98, `ESTRAINI` 29+).
- **Layer 8 originals** (start MP -> new, gain per level -> new): Kotemon 100 -> 140 and 55 -> 57; Kumamon 10 -> 140 and 30 ->
  57; Monmon 80 -> 140 and 45 -> 57; Veemon 150 (gain 50 -> 57); Guilmon 130 -> 140 (gain 60). Forms have no MP of their own:
  MP is the rookie's (`RecordsDigimon.start_mp`, `hp_mp_gains[1]`). It is applied by a patch at port boot (`def_mp_hook`);
  `tools/convert_save_mp.py` uses the same table, keep both in sync.
- **Layer 9 (bestiary save), original reasoning:** the card file has 4 blocks and the game used up to 0x7800, the last 0x800
  were never written, so one 0x100 record per slot went there; absent, unknown version or bad checksum reads as all zero.
  The slot tail is not usable: 0x26C4..0x26EB is the `global_save`, 0x26EC..0x26EF the MP converter marker. Start (bit 0x8,
  free in battle) opens the battle memory; Inspect respects the same table. Wiring: part 5 of `memcard.c`
  (`DEF_BESTIARY_PART`), read by STGMCARD step 0x48 (`def_bestiary_record_read`; any failure reads as an empty table,
  never an error screen) and written by step 0x36 after the slot (`def_bestiary_record_write`); the record is "BEST",
  version, XOR checksum and the 193-byte table (bits 0..6 = element learnt).
- **Boss immunity (PRD 2 layer A).** `records_state.blocked[12]` per fight: indexes 0-4 and 7 are status and still count;
  8, 9, 10 are Strength, Defense and Speed reductions, which the definitive edition ignores (the effect enters, scaled by
  `DEF_BOSS_DEBUFF_PCT / 100`, see `def_stat_blocked`); do not touch 5, 6 and 11 (11 is "no escape"). The hunt's boss-fight
  Rare Targets may carry `blocked[11]`, which is why the flee roll is forced before checking it.
- **Buff reaching magic (PRD 2 layer B).** The engine has 3 modifiers per member (`modifiers[0]` Strength, `[1]` Defense,
  `[2]` Speed); Spirit has none. The fix is in `get_special_damage`, not `get_stats` (for an enemy Spirit is magic attack
  and defence at once): the attacker's Spirit is scaled by the Strength modifier ratio, the target's magic-defence stat
  (Defense on the player, Spirit on an enemy) by the Defense modifier ratio; the player's Defense already comes modified, so
  it is not applied twice; the final boss (`type == 6`) keeps its own Defense/Spirit adjustments.
- **Training facts (PRD 2 layers E and I).** A round succeeds with chance 75 (+3 or +6 with the training item). Trainings
  below id 13 have 3 rounds and an automatic bonus round if all 3 hit; from 13 on, 5 rounds with an optional bonus of chance
  50 (0 if the previous bonus was won). The pity counter is RAM only (not saved: it resets when the game closes), one for all
  Digimon, "perfect" = every normal round hit (resets the counter, anything else adds 1), at `DEF_TRAIN_PITY_AFTER` the next
  session hits every round (and the bonus round, `DEF_TRAIN_PITY_BONUS`) and the counter resets; it only applies to a
  session in which TP was spent; the screen shows filled marks (e.g. 2/3) and a clear notice when the next is guaranteed.
  Resistance trainings (types 8..14) have a second stat (`entry->stat_2`); when it is 1..5 each round had a 50% chance of
  taking points from it (`stgtrain_session_lower_stat`, `stgtrain_stat_losses` = {1, 2+r3, 4+r3},
  `src/stgtrain/stgtrain_800861BC.c`). `DEF_TRAIN_NO_STAT_LOSS` zeroes the loss and hides the line; `stat_2` 15/16 raise max
  HP/MP and stay. The original draws one random number for the loss (plus one for the amount); with the switch on,
  `stgtrain_session_lower_stat` returns before any `pad_random.next()`, so a definitive session simply draws fewer
  numbers, and a scripted replay (`def_on()` off) draws as before.
- **TP retroactive for old saves** (`tools/convert_save_mp.py`, `retroactive_tp`): old TP = (new total up to the current
  level, capped at level 50) - (5 x levels gained up to 50), never negative, respecting the 99 cap; what does not fit is
  lost and `--dry-run` says so. Marker at slot + 0x26EC: `MPV1` is MP only, `DFV2` is MP and TP; the converter is idempotent
  and upgrades `MPV1` to `DFV2` keeping the MP. Original TP: +5 per level (`stfgtrep_add_exp`, `stfgtrep_80082E70.c`).
- **Blast gauge was a bug of the original:** `records_state.gauges[8]` (s16, 0..1000 per Digimon) lived only in RAM and was
  cleared at the title (`records_clear_gauges`, `stdwtitl_80085CD4.c`); the port saves it in the slot tail at +0x26F0 (an
  out-of-range value reads as 0, an old save loads with 0, a new game still zeroes it; also in `game_savestate`).
- **Digivolution requirements (PRD 2 layer J).** Each partner has a list of 44 forms (`stfgtrep_learn_lists[digimon]`,
  `StfgtrepLearn`): up to two required forms with a minimum level (`requires[i] = {form, level}`) and one minimum stat
  (`stat` 1-6 the six stats, 7 the Digimon's level, 8-14 resistances; `min`), checked in `stfgtrep_learn_technique`. The
  Digi-Lab panel is read only, shows one step ahead (`DEF_EVO_HINT_DEPTH` 1; 0 off, 99 all), and the form is still learned
  at the next level-up as in the original; the numbers were checked against `stfgtrep_learn_lists` for Kotemon and Kumamon.
  The panel lists the forms in reach with their requirements met / total ("Ready" in green when all are met), and for
  the chosen form one block per requirement with its current value against the goal, green when met (`EH_PAL_MET`).
- **Second pass on techniques (PRD 2 layer H, now `def_mod_tech_spec`).** The player had only 13 buff/debuff techniques in 55
  forms and no attack reduction (technique 204 existed only for enemies); the table "form, technique, current effect,
  proposed effect, MP" went to `.dumps/tasks/persona/tecnicas-v2.md` for the owner, with the goals one identity per form, at
  least one attack reduction for the player, and nothing that gives an extra turn (the chain already does). Damage techniques
  with a side debuff are tier 1 for 2 turns; dedicated single-target ones tier 2 for 3-4 turns;
  strong or group ones tier 2 for the group or tier 3 on a single target.
- **Native XP rules (PRD 3), what each launcher mod became.** `party_xp` share 100 (`knocked_out` and `catch_up` removed:
  a knocked-out member gets no shared XP, no level-gap bonus); `xp_boost` EXP and Bits 1x (path removed), form EXP 14x
  (disables the original form-EXP cap), all forms on, card boosters 15 per drop (`DEF_XP_PARTY_SHARE`, `DEF_XP_FORM_MULT`,
  `DEF_XP_ALL_FORMS`, `DEF_CARD_BOOST_PACKS`); `evo_buff` removed with no leftover
  (`grep -ri "evo_buff\|evo buff\|evo_shield"` outside `.plan/` and `.dumps/` must stay empty). An old `settings.json`
  with those keys still works (unknown keys are ignored). `all_forms` credits the three chosen forms of each member
  (the old mod text said "every digivolution the member owns", `LAUNCHER.md` said "the three chosen forms"; the code and
  the README line are the three chosen). Rules moved to `docs/MECHANICS.md` section 6.
- **OVERFINISH Bits** were "maybe 0.1" in the PRD, the owner will lower it if it turns out high; the red callout reuses the
  WEAK!/RESIST! creation flow.
- **Teleport fix** (PRD 3 roadmap, done): the Guardromon teleport used to spawn the player outside the map with inverted
  controls; the destination now uses each map's standard start coordinates.

## Buff and debuff tiers (no stacking)

A stat buff or debuff is one of three **fixed tiers**, a percentage of the stat's base value (kinds: attack, defense,
speed), not an accumulating sum:

| Tier | Buff | Debuff |
|---|---|---|
| 1 | +20% | -15% |
| 2 | +35% | -25% |
| 3 | +50% | -35% |

- Duration is in **turns of the affected Digimon**: 2 (short), 3 (base) or 4 (long); a damage technique with a side
  debuff is tier 1 for 2 turns.
- **No stacking.** Same sign: the stronger tier wins and the same or a weaker tier only refreshes the turns. Opposite
  sign: the new effect overwrites the old.
- Per-technique specs live in `def_mod_tech_spec` (`definitive.c`): 198 and 200 are +2 for 4 turns, 199 and 201 are +3,
  194, 205, 134, 81 and 136 are -2, 195, 204 and 206 are -3, and 132, 108 and 131 are -1 for 2 turns. Only technique IDs
  with the enemy bit clear are changed, so enemy techniques keep their roles.
- Bosses are immune to status but not to debuffs (`DEF_BOSS_DEBUFF_PCT` 100).
- Every description in `SKLINF` states effect, tier and turns (34 characters at most), e.g. "User Atk+2 (phys/mag), 3
  turns.".
- Tests: `tests/port/definitive_buffs.py` (+ driver) checks the tiers, the no-stack rule and the expiry.

## Status, Poison and revive rework

Measured in `.plan/tasks/status-balance-report.md` (a level-50 Digimon has median Wisdom 338, Spirit 316 and max HP 3383;
training gives about 4.1 points per TP).

- **Chance** = value + (attacker Wisdom - target Wisdom) / 8 - resist_sum / 8, out of 128. On a boss x0.6
  (`DEF_BOSS_STATUS_PCT` 60); on a staggered target x1.5 + 24; a boss cap of 100/128; the result is clamped to 1..127.
- **Poison** damage per turn = power / 2 + Wind affinity / 6 + Wisdom / 4 + Spirit / 4, capped at max HP / 4 and, on
  enemies, at 8% of max HP.
- **Sleep and Confusion** hold an enemy for 2 turns and end on a hit (`def_wake_on_hit`). A sleeping enemy regenerates 6%
  per turn. After confusion ends the enemy gains Attack +2 for 3 turns.

### Revive items

| Item | Chance | HP returned | Rated max HP | Price |
|---|---|---|---|---|
| Revive Disk (item 67) | 60% | 30% | 1500 | 400 buy, 200 sell |
| Life Disk (item 70) | 85% | 60% | 4000 | 2500 buy, 1250 sell |
| Max Charge (item 46) | 100% | 100% | 9999 | 3000 sell |

The chance scales down by rated max HP / the target's max HP when its max HP is above the rating (never under 20%,
`DEF_REVIVE_MIN_CHANCE`; `def_revive_chance`). By the fallen ally's max HP (computed from that rule, not measured):

| Max HP | Revive Disk | Life Disk |
|---|---|---|
| 300 | 60%, 90 HP | 85%, 180 HP |
| 800 | 60%, 240 | 85%, 480 |
| 1500 | 60%, 450 | 85%, 900 |
| 2000 | 45%, 600 | 85%, 1200 |
| 3000 | 30%, 900 | 85%, 1800 |
| 4000 | 22%, 1200 | 85%, 2400 |
| 6000 | 20% (floor), 1800 | 56%, 3600 |

Max Charge is 100% at any max HP. On a 3000 max HP ally a success costs about 1330 Bits in Revive Disks (3.3 of them)
or 2940 in Life Disks (1.2).

### Balance measurements behind the status rework (`status-balance-report.md`, simulated from the disc tables and the port's formulas)

Names: Power, Guard, Spirit (`stats[2]`), Wisdom (`stats[3]`, the game's "precision" and the heal's stat), Boost (speed).
Poison reads Wisdom + Spirit + Wind affinity.

- **A level-50 Digimon by level-ups alone** (8 party species x 40 rolls, no gear, no training): Wisdom p10/median/p90/max
  305 / 338 / 371 / 401; Spirit 204 / 316 / 382 / 409; max HP 2644 / 3383 / 4629 / 4693.
- **Training:** pure stat trainings (ids 1-5, below 13: 3 rounds, bonus after 3 wins, `stat_2` = 0 so nothing is lost) with
  round chance 75 + item (0/3/6) succeed 76/79/82 in 100; cost 1 / 5 / 10 TP per session at trainer level 0 / 1 / 2. About
  **4.1 points per TP whatever the trainer** (the three tiers are balanced on purpose): 4.09 at 76%, 4.33 at 79%, 4.59 at 82%.
  The report assumed the original +5 TP per level (245 TP, about 1001 points); with the Definitive TP table (613 by level 50)
  there is more to spend. Reaching 999 Wisdom from the median 338 costs 661 points = 161 TP of the original 245 (66%), the rest
  buys about 340 Spirit or Wind resist (153 TP for the best-growth species): 999 Wisdom is reachable at 50 but eats most TP.
  Profiles used: casual (Wis 250 / Spirit 250), focused (600 / 400), pure (999 / 656, Wind resist about 300 -> affinity 90).
- **Bosses** (kind 2 with at least 9 of 12 `blocked` flags): 48 battles, 47 at level >= 30, 41 at >= 45. Level >= 30: HP p10/
  median/p90 1776 / 3120 / 5040, precision (Wisdom x scale) 400 / 738 / 1053, median poison resist sum 163. Level >= 45: HP
  2052 / 3360 / 5040, precision 552 / 750 / 1053, resist sum 164.
- **Poison damage** (technique powers [80, 88, 96, 127]; tick = power/2 + Wind affinity/6 + Wisdom/4 + Spirit/4, capped at
  max HP/4 by `def_poison_dot_calc` / `def_poison_cap`; a boss ticks 10 times): casual 164-187 per tick (5.3-6.0% of a median
  3120 HP boss, 53-60% over 10 ticks), focused 300-323 (96-100%), pure 468-491 (100%); on the p90-HP boss (5040) 10 ticks do
  33-37% / 60-64% / 93-97%.
- **Chance to land a status on a boss**, medians over bosses of level >= 30 (chance = effect_chance + (my Wisdom - boss
  precision)/8 - boss resist sum/8, x0.6 on a boss, then x1.5 + 24 if staggered, boss cap 100/128 = 78%): Poison with the
  best technique (127): casual 19% normal / 47% staggered, focused 39% / 77%, pure 62% / 78%; with the 72 technique casual 1% /
  8%. Sleep best (127): casual 20% / 48%, focused 40% / 78%, pure 63% / 78%. Confusion best (127): 19% / 47%, 39% / 77%,
  62% / 78%. Worst boss with the best technique: casual 1%, focused 5% poison (26% staggered), pure 28% (61% staggered).
- **The report's full cells** (chance %, normal / staggered, medians over bosses of level >= 30; base chances: Poison
  [72, 80, 96, 112, 127], Confusion [80, 127], Sleep [80, 96, 127]):

  | Profile | Poison 72 / 96 / 127 | Confusion 80 / 127 | Sleep 80 / 96 / 127 | Worst boss, best tech (P / C / S) |
  |---|---|---|---|---|
  | casual, normal | 1 / 4 / 19 | 1 / 19 | 1 / 5 / 20 | 1 / 1 / 1 |
  | casual, staggered | 8 / 24 / 47 | 14 / 47 | 15 / 26 / 48 | 1 / 1 / 1 |
  | focused, normal | 13 / 24 / 39 | 17 / 39 | 18 / 26 / 40 | 5 / 1 / 1 |
  | focused, staggered | 38 / 55 / 77 | 45 / 77 | 45 / 57 / 78 | 26 / 19 / 19 |
  | pure, normal | 37 / 48 / 62 | 41 / 62 | 41 / 49 / 63 | 28 / 23 / 23 |
  | pure, staggered | 73 / 78 / 78 | 78 / 78 | 78 / 78 / 78 | 61 / 54 / 54 |

  Training per session (ids 1-5): trainer level 0 / 1 / 2 costs 1 / 5 / 10 TP and gives 4.1 / 20.4 / 40.9 points at a
  76% round chance, 4.3 / 21.7 / 43.3 at 79%, 4.6 / 23.0 / 45.9 at 82%; the 5-round trainings from id 13 pay more at
  the same cost and were left out, so these numbers are the safe side. The boss with the lowest HP of the set (912) dies
  to 10 poison ticks at every profile. Generator: `.dumps/tasks/status_balance_sim.py` (local, gitignored), from the
  disc tables and the port's formulas (`def_poison_dot_calc`, `def_poison_cap`, `def_revive_hp`,
  `fightstg_rules_get_heal`); it assumed the original +5 TP per level.
- **Would it be annoying?** Sleep and Confusion last at most 2 boss turns (`DEF_STATUS_BOSS_TURNS`) and end on any direct hit
  (`def_wake_on_hit`), so control is for healing and setup, not the damage turn. A sleeping boss regains 6% max HP a turn
  (12% over 2 turns). Expected attempts for the best Sleep: casual 5.1 normal / 2.1 staggered, focused 2.5 / 1.3, pure 1.6 / 1.3.
- **Revive for a pure healer:** the report's table used rated HP 400 (Revive Disk) and 1500 (Life Disk); **the code now rates
  them at 1500 and 4000** (`DEF_REVIVE_T1_RATED_HP`, `DEF_REVIVE_T2_RATED_HP`), so the tables under "Revive items" above are
  the current ones and the report's numbers are older. Expected cost per success on a 3000 max HP ally in the report: Revive Disk 20% (about 5 items,
  2000 Bits), Life Disk 42% (about 5952 Bits). Heal = power x 64 + Wisdom x power / 8 (`fightstg_rules_get_heal`, cap 9999),
  so Wisdom is the healer's stat too: technique 0xB8 (power 8, 16 MP) heals 762 / 1112 / 1511 at Wisdom 250 / 600 / 999; 0xB9
  (32, 74 MP) 3048 / 4448 / 6044; 0xBA (127, 120 MP) 9999 at any of them; 0xBB (32, 188 MP) like 0xB9; 0xBC (64, 240 MP) 6096 /
  8896 / 9999; 0x190 (8, 0 MP) like 0xB8.

## Super Hard Mode (the Demon Bell)

- **Super Hard** is the **Demon Bell**, key item 26, held in the bag. Guilmon in the [Velvet Room](#the-velvet-room)
  hands it over and takes it back ("Take the Demon Bell. Foes grow stronger and faster... but they pay more Bits and more
  materials.").
- While held: enemy HP x1.5 and Speed x1.5 (Speed may pass 999 and give "Enemy x2"), **Bits x2 and Hunt Materials x2**.
- Hooks: `def_super_on` (`port/game/definitive_super.c`; the mode is the item in the bag, which the card already saves, so
  there is no flag of ours); HP in `wfightmn_init_members` (`def_super_hp`); Speed in `fightstg_rules_get_enemy_stats`
  (`def_super_speed`); Bits in `stfgtrep_main_run` and the Rush's `def_rush_apply_quiet` (`def_super_reward`, both paths);
  Hunt Materials in `common_drops` (`definitive_hunt.c`); numbers `DEF_SUPER_HP_PCT` 150, `DEF_SUPER_SPEED_PCT` 150,
  `DEF_SUPER_REWARD_PCT` 200 (`definitive.h`). Guilmon's talk toggles the item (`def_super_toggle`); `TALK05` 77 handed, 78
  taken back. Off in a replay script.
- The balance of these numbers is the owner's, and **a real fight with the Bell is untested** (only the host tests).
- Hard Mode (two Digimon) is the New Game choice described above and is separate from this.

## Turn order, stagger, chain, XP, training, digivolution: the numbers

- The player opens unless ambushed; the enemy's first turn comes after its full delay.
- Stagger and chain: one extra action per family per window.
- TP per level: 7 for levels 2-20, 9 for 21-40, 30 for 41-50, 0 from 51 (cap 99 in hand). Totals 313 by level 40 and 613
  by level 50 (original 195, 245 and 490).
- **Training pity**: 3 non-perfect trainings, then the 4th is guaranteed (the bonus round included). The counter lives in
  RAM and is shared by all Digimon (`tests/port/definitive_pity.py`, `definitive_train_loss.py`, `definitive_tp.py`).
- XP: party share 100%, form XP x14 (cap skipped), every chosen form earns XP, card boosters x15.
- Rare Booster: 1 pack, items 0x186..0x18A; 01 below level 25, then one more per 15 levels (05 from 70).
- Short digivolution animation: a "seen" bitset in RAM; hold Circle or Cross for `DEF_EVO_SKIP_HOLD_FRAMES` (4) frames to
  skip (`tests/port/definitive_evo_anim.py`); the hold works even on a form's first digivolution (`def_evo_skip_update`
  in `fightstg_80086A00.c` checks no "seen" bit); the seen set is RAM only.
- Digi-Lab "Next Forms" panel (`DEF_EVO_HINT_DEPTH` 1), opened with Square (`definitive_evohint.c`).

## The hunt: constants and hooks

- Chain: `DEF_HUNT_CHAIN_MIN` 5, sure at 10, cap 255; 20% at 6 wins, +20% per win.
- Regions are the `ststatus_map_areas` byte: area 0..0x2D (`DEF_HUNT_AREAS` is 0x2E) plus bit 7 for the second server
  (Amaterasu).
- Rare Target: battle + slot of an existing battle, alone, level = region average + 5, HP x4, XP x10. Flee always works,
  except in an active Super Rare territory.
- 25% common drop per enemy; the 16 commons are listed above.
- Hooks: `fieldstg_encounter_start` (the Rare Target swap happens **before** the Rush); `def_log_end` ->
  `def_hunt_battle_end`; `def_hunt_flee`. The chain resets on a map region change and on a Guardromon teleport.
- Generator: `tools/gen_hunt_targets.py --apply` writes `port/game/definitive_hunt_targets.c` (it also writes a readable
  proposal to `.dumps/tasks/hunt/alvos.md`; `tools/gen_recipes.py --check` for the forge). It reads the story's per-route
  encounter lists too (35 stages used to be missed).
- Tests: `tests/port/definitive_hunt.py` (+ driver), the market, economy and DRI tests below.
- The Rare Target's battle (`def_hunt_scale`): the target's slot of the drawn battle is moved to the first position and
  the three members are zeroed around it (one Digimon, owner 2026-11-17); its HP follows the level up or down (a strong
  Digimon is lowered in a weak place) times `DEF_HUNT_RARE_HP_MULT`, capped at `DEF_HUNT_HP_MAX`; `stat_scale` is left
  as it is.
- Economy constants (`definitive.h`): `DEF_HUNT_MATERIALS` 16 and `DEF_HUNT_MATERIAL_CAP` 99 (one u8 each,
  `REC_EXTRA + 10..25`), `DEF_HUNT_RARES` 128 slots and `DEF_HUNT_RARE_CAP` 99 (record 2), `DEF_REPLICAS` 16 (bits at
  `REC_EXTRA + 26..27`), `DEF_DRI_CHALLENGES` 16 (an unlocked bit and a 2-bit best stage each, record 2); prices
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
- In-game recipe for the chain (from PRD 4): load the map 0x21D field state (see "Making a battle state for tests"
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
- Forge, the result of the 2026-11-17 spread (commit `89c46af`; constants `MIN_POOL, REACH_MAX, WINDOW, SPREAD, SECOND =
  3, 7, 8, 2, 2` in `tools/gen_recipes.py`): asks per common over the 317 recipes: Bit Shard 8, Byte Shard 22, Data Chip
  32, Code Chip 44, Core Fragment 20, Core Crystal 20, Mega Crystal 9, Giga Crystal 21, Tera Prism 20, Peta Prism 7,
  Chrome Digizoid 11, Blue 32, Red 37, Gold 23, Black 7, Obsidian 4. The shops that changed most: 13 (it asked Peta
  Prism for every item; now Peta 7, Chrome 6, Blue 5), 28 (Gold 7, Black 7, Obsidian 4), 20 (Code Chip, Core Fragment,
  Mega, Giga, Tera), 3 (Core Crystal 8, Mega 5). Why: Mega Crystal was always outbid by the Shards and Chips of the
  shops that had it in reach, and Obsidian drops only beyond `DEF_RECIPE_REACH` of every shop. The rares stay dealt from
  the reach-4 regions. A safety net (a common no recipe asks goes to the shop of its server whose dearest item's level
  is nearest the common's mid level) is not triggered today. Shop 23 (the Gems list's second-server shop, map 0x2AB) is
  outside the level-gap check: its cheap rings and gems (300-1500 Bits) ask Giga Crystal and Tera Prism because no
  region near it is low; accepted since the player reaches it late. Old saves are unaffected (the recipes are a code
  table). Seen in game 2026-11-17: shop 13's Shine Blade asks Chrome Digizoid, shop 28's Tenjinmaru Obsidian Digizoid,
  shop 15's Suijin Cannon Chrome Digizoid. The checks (`gen_recipes.py --check`, `tests/port/definitive_economy.py`):
  every common asked, no shop on one common, no level gap over 25 outside shop 23; the buffs, market and savestate
  tests run too (the recipe table feeds the savestate). Local prototype scripts in `.dumps/tasks/forge/`.
- The 27 base variants, by shop tier (from `tools/gen_equipment.py`'s proposal; original price in brackets):

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
- Elemental weapons (`ELEMENT_WEAPONS`, commit `11d9e54`; `ELEMENT_WEAPON_COST` 25% of power, half the heads' 50,
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
- Trade-off pieces (`TRADES`, commit `188618a`): Power Gem -> Fury Gem (292) and Wisdom Gem -> Sage Gem (298) in the
  Gems list; Guard Ring -> Fury Ring (295) and Spirit Ring -> Sage Ring (297) in the Rings list. Left out on purpose: the
  status rings (Antidote, Revive, Awake, Sober, Prayer) and the elemental rings (plain resists). Boost is the Sage
  pieces' price because Wisdom drives the support buffs. FIGHTSTG's own read of an accessory's bonus
  (`fightstg_rules_get_tech_cost`: the MP Proxy and MP Mega Proxy cut a technique's MP) is not a stat, so it needs no
  penalty hook. Without the penalty a bigger single stat would be a straight
  upgrade, which `gen_equipment.py --check` refuses. Seen in game at shops 1 and 16: the preview shows Power up in blue
  and Guard or Boost down in red.
- Owner decisions on the forge screen (2026-11-17): the shop's description page lost the slot text ("Right Hand" /
  "Both Hands") to the recipe line, and stays so (the icon shows the kind, the Triangle page who can wear it). The
  Portuguese names and texts of the Elemental weapons and of items 292/295/297/298 are the owner's to translate:
  `pt.txt` still has the plain pieces' lines.
- `tests/port/definitive_economy.py` (driver): an item the price table has (past the u16), one it does not, no item, a
  replica's resale 0; recipes with no shop open, another shop's recipe for the same item, an item without a recipe, a
  shortfall (orange label), buying several units, the scarcer of two materials deciding, payment; in a replay script
  the game's prices and no recipe. `gen_item_prices.py --check` requires the table to be the rule's and a dearer item
  never to resell for less. In game, Asuka's shop (`shop_open` is a checkpoint of `first_battle_save`) shows the prices.

### The Black Market, in detail

- Trader: actor 42 on maps 0x20C and 0x27B, and now also `{0x20F, 42}` in the Velvet Room
  (`port/game/definitive_market_screen.c`, `traders[]`). Two tabs (L1/R1).
- Exchange: 1 rare for 12 commons, only if all 12 fit under the 99 cap.
- Replicas: 16 items, one Core of each of its own two Rare Targets (`DefReplica.cores`, hand-picked in the generator's
  `CORES`, none shared) plus 20x the original sell price in Bits (20-40k); resale 0, using one
  bit per replica (record 1, REC_EXTRA 26-27). List: Power S x7, Machinegun, Magician Rod, Ghost Fang, Pulse Guard, Drill
  Horn, Chain Saw, Naginata, Recover Crest, MP Mega Proxy. Generator: `tools/gen_replicas.py --check`.
- **UX pass:** everything uses the dialogue font 1; white text, orange cursor and shortfall, green success, grey
  explanation lines; rows show "have/99"; the detail lines give description, type/stats, who can equip and which Cores.
- Pitfall: a **colon renders as "0"** in the 14px font (30 other port strings are affected; the 11px font has no
  `/ : , '` or accents either).
- UX pass in detail (owner's request 2026-11-16: "a UI/UX on the black market screen, as game design, to tell the player what the item does, and fix legibility"; code
  `definitive_market_screen.c`, the rules of `definitive_market.c` unchanged; done and tested 2026-11-17, committed in `def4d30`; built in `build/port-ux` not to fight another session
  over `build/port`). What was bad: the small orange font has no `/`, `,`, `:` or accents ("30/99" came out "30099", a cost "Datamon II0 BK..."); normal and highlight colour were the same orange; an unaffordable
  price was blue with no explanation; choosing a material showed one at a time and Bits only appeared in a replica's detail; nothing explained what a Core is, what a replica does or who can equip it. Now: the dialogue font
  (font 1) everywhere; colours with a fixed meaning (white text, orange = cursor row or what is missing, green = success or already the player's, grey = what does not apply now and explanations); list with a header ("Your Rare
  Cores", "Get 12 of which material?", "Legendary Replicas"), position ("3/16"), 6 rows with a fixed band on the chosen row (blue in the list, brown when swapping or buying); Exchange: the Core list's detail says how much a Core is worth
  here, what materials are for and that replicas also spend Cores (with no Cores it says where they come from), x opens the 16-material list with "have/99" (grey what does not fit +12), the detail shows "Give: ... Held: 3", "Get: ... x12
  Held: 30/99" and the warning when it does not fit; Replicas: a 4-line detail: 1) the shop description (ITMINF), 2) type and stats read from the item as the shop reads them (weapon, two-handed, armour, accessory; Strength, Defense..., elements), 3) **who in the team can equip it**
  (most weapons fit 1 to 3 of the 8 Digimon; orange when nobody in the team can), 4) which Cores the purchase spends; the confirmation shows the cost and the resale-0 warning; if nobody can use it the warning stays on screen but the purchase is **not blocked** (a Digimon that
  joins later may use it); footer: the player's Bits always visible, orange when the chosen replica costs more. Texts: new strings in `en.txt`/`pt.txt` and the glossary of `tools/translate_ptbr.py`; `$BMCost` and `$BMPrice` removed; elements reuse `$Fire`...`$Dark` of Inspect; a detail line wider than the
  panel goes to the log ("market: detail line ... pixels wide"); PT: ITMNAM 356-359 translated (Poder Maquina S, Poder de Trevas 1/2/S). Tests: `definitive_market.py` pass; `run.py --exe build/port-ux/dw2003` pass; EN and PT screenshots via `.dumps/tasks/market/bmshot.py` (list, swap, full materials, every replica, purchase and confirmation,
  no Cores, no Bits, no bosses; no line overflowed). The Black Market strings avoid colons ("Price 40000 Bits   Held 0", "x Buy   triangle Back").
- Pending small items: PT names "Crest de Recuper" (ITMNAM 320, cut), "Pulse Guard" (untranslated), "Shard de Bits/Bytes" (half English); **the same colon-as-"0" defect hits about 30 other port strings**
  (`x button: ...`, `L1/R1: ...`, `Pity:`, etc.): needs its own pass or a fix of the glyph in the font.

### The DRI Gauntlet, in detail

- Agents are the eight DDNA researchers, by map and actor (`AGENTS`); an actor with their sprite elsewhere is an
  ordinary NPC. Each talk rewrites the agent's talk conditions before the game picks the entry (`dri_quest_open`): the
  quest line (the entry that sets quest flag 0x600..0x607) gets an always-true pair (`0x5000`) for its party level sum
  conditions, the other level-gated entries a never pair (`0x61FF`, progress 511). The list is armed only once the
  agent's Rookie has joined (`dri_rookie` reads it from his own "joined" conditions), so the hand-in talk never opens
  it; then it opens after the talk, once the field has been idle for 30 frames (max 3600). Bob's and Johann's quest
  lines name the place in `en.txt`, the other six in the story rewrite (`en_story.txt`).
- A challenge is one boss battle whose 3 enemy members are an evolution line; `wfightmn_knockout` brings in the next at
  full HP with a DIGIVOLVE! callout. Stage 19, music 0x60880000. Defeat returns to the field with HP/MP restored
  (`def_dri_defeat`). Levels start at 24, +8 per challenge, +3 per stage.
- Tamer titles: The Leader of Courage, The Child of Hope, The Heir of Miracles, The Dreamer Tamer, The Calm Tamer.
- Generator: `tools/gen_dri_challenges.py --check`; tests: `tests/port/definitive_dri.py` (+ driver).
- Save: unlocked bits (2 bytes, low first) and best stage per challenge (2 bits each, 4 bytes) in record 2 at
  `REC2_DRI` (right after the rare items' "ever held" bits; `port/game/definitive_bestiary.c`, checked 2026-11-17). An
  older PRD placed them in record 1's `REC_EXTRA` area; the code does not.
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
  `wstag635` (0x258); the level window no longer applies (2026-11-17). Places are named as each map's own title shows
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
  line said he had no idea where MetalGreymon was) are in `en.txt` (commit `11d9e54`); the other six are pages in
  `en_story.txt`, which wins over `en.txt` (commit `c5e58e4`): Kazuki `TALK02 633`, Michael `TALK02 647`, Jimmy
  `TALK01 277`, Ken `TALK02 831`, Yuji `TALK02 839`, Yang `TALK02 847`. `pt.txt` still has the disc's lines without the
  hints (the Portuguese is the owner's). The quest opening and the Rookie rule are commit `bb15d51`. A starter's agent
  counts as done from the start: his own line plays, then the list. `def_dri_debug` (tests: start a challenge or open
  the list from the idle field) is unchanged.
- Host test (`definitive_dri_driver.c`): the tier per stage; the table's bounds; an agent on his map, a map-any row, and
  nobody else (an actor of an agent's sprite on another map); the wait (nothing while the talk runs, the list once, an
  idle gap inside the talk, a battle or another map drops it, a talk that never ends gives up); the Tamers' lines;
  unlock (locked with nothing, the common alone is not enough, exact payment, never paid again, a rare short shows
  orange, a rares-only challenge); rewards (each tier once, the best stage only rises, capped at 3); record 2 (DRI state
  alone writes it at bytes 152..157, read back, a damaged record reads nothing unlocked, a rares-only record, record 1's
  read resets them before record 2 brings them back, out-of-range challenges); nothing in a replay script.
- Seen in game (2026-11-17): Johann's quest line at map 0x22B with a party level sum of 87 (over his old bound), the
  hand-in talk without the list and the next talk with it. Known small bug: the debrief reward line overflows its box.

### Guide, Guardromon network, Rush

- **Guide** (`def_quests`): one entry per story step 0..45, keyed on key-item flags (0x8000|item); 4 missions; 16 Cargo
  Tower auctions (flag 0x400+N; travel goes to map 0x213, none from map 0x2D7 on). The notice panel lasts 300 ticks.
  Env `DW3_GUIDE=0/1`, `DW3_GUIDE_CHECK`. Plan: `.plan/tasks/definitive-guide.md` (absorbed here).
  - Origin and decisions (owner, 2026-11-13, roadmap "QoL and menus"): an in-game guide that says what to do with triggers from the game itself (event-like
    missions, the main quest), without a whole walkthrough, plus a notice/menu of the Asuka auctions with near-direct teleport. (The owner could have brought a
    famous site's walkthrough as Markdown; the shipped texts are summaries written from the walkthrough the owner brought, not copies.) Started in a Fable session in the lab that ran out of quota before
    testing, finished in another; the backup of that WIP is in `.dumps/fable-guide-wip/` (all of it is in commit `3d3f2e8`, may go to `/tmp`).
  - Trigger: the game's own story counter `gamestate_data.progress` (0..45, written by the stage events) plus a flag when the line asks (a key item held, `0x8000 | item`).
    Nothing is saved, so an old save opens on the right step. Main quest (`def_quests`, `definitive_quest.c`): one line per step with title, what to do and where to go (the map
    name is the game's own title); key-item variants come before the plain line (the fake "8lue Card", the Blue Card, the Sepik Mask, the Agumon Suit, the TNT Chip); texts in `en.txt`
    as `$Q<step>` and `$Q<step>T`. Missions (`def_missions`): from step 4 (when the field menu opens) while the item is missing: El Dorado ID (opens the auctions), Folder Bag (Card Battles),
    Tree Boots, Fishing Pole. Auctions (`def_auctions`): the 16 of the Cargo Tower are the game's `fieldstg_flag_events`; auction N runs with the story step exactly its own, flag `0x400 + N` clear, and money; any answer
    sets the flag; states: not yet, OPEN NOW, attended, lost (the step passed without going; Amaterasu's hidden shop sells the lost items); price from the game's own money table.
  - Screen GUIDE: a new field-menu entry (between SAVE and the Bestiary) in the Guardromon-network layout; Quest tab, Auctions tab (L1/R1) coloured by state; on an open auction confirming twice travels to El Dorado
    (`0x213`, the Cargo Tower entrance) like the Guardromon teleport (resets the hunt chain); **no travel** from the real world, from Gunslinger, from El Dorado and from Byakko's trap.
  - Notices: when the main quest changes step or an auction opens, a panel drops at the top of the field for 5 s ("NEW OBJECTIVE", "AUCTION OPEN"); it does not hold the player and waits while an event, the menu or a
    battle start holds the field. Off in replay scripts (shared with the emulator); `DW3_GUIDE=1` turns it on in scripts too.
  - Test: `tests/port/definitive_quest.py` (a line per step, item variants, missions, auction state/price/travel, notices once per change, replay; tables against the game's: the 16 auctions against `fieldstg_flag_events`,
    every step 0..45 has a plain line after the variants, every text in `en.txt` fits the window's lines); in game `guide_ingame.py` (arrival notice, GUIDE entry, tabs, step 5 with objective and auction notices, travel to El Dorado).
- **Guardromon network**: 25 inns (13 plus 12 copies of the second half); `nodes[]` in `definitive_travel.c` and
  `gs_inns[]` in `game_mods.c`; SAVE and TELEPORT; the network mask is REC_EXTRA 4-7.
- **Overlevel Rush floating panel**: `def_rush_apply_quiet` (in STFGTREP) applies the report's rewards in the same order
  without the screen; the panel draws on layer 0x1004, does not set `event_running` and closes when the field holds the
  player. `DW3_RUSH_FLOAT`, `DW3_RUSH_TEST`. Level-up stat rolls differ in timing from the report. Test:
  `tests/port/definitive_rush.py`.

### Hunt, forge, Black Market, DRI and Rush: design record (PRD 4, layers A-I, done 2026-11-13)

The loop, as the PRD put it: exploration and hunting (chaining battles in one region summons Rare Encounters that drop
high-tier Hunt Materials, documented in the Bestiary); economy and forge (shops ask materials plus Bits for advanced
gear; the Black Market drains surplus materials and Bits for missing pieces or Legendary Replicas, with 0 resale); DRI
challenges (the same materials are permanent keys that unlock chained-evolution simulations that pay tiered
accessories). Working rules of that PRD: small layers with a test between them; every constant in `definitive.h` with
a line saying what it controls; game code only calls `def_*`/`dw3_*` hooks under `#ifdef PC_PORT`; an old save opens
and plays (with nothing gained, the slot's record stays byte for byte the same); nothing runs inside a replay script
(`def_on()` / `def_bestiary_active()`); a short report to the owner after each layer; screenshots in
`.dumps/tasks/hunt/`, `.../ingame/`. Definition of done for a layer: its tests plus `tests/port/definitive*.py`,
`text_override.py`, `run.py` and `scripts/test.sh` pass, the PS1 matching build and `tests/golden` unchanged, every new
text enters through `tools/text_override/en.txt`.

**Engine facts the layers rely on**

- Battle: `fieldstg_start_battle(battle)` copies `fieldstg_battles[battle]` (335 battles, `src/fieldstg/fieldstg_80083784.c`)
  into `records_state` (enemies with `digimon`, `level`, `hp`, `mp`, `stat_scale`) and sends the field to map `0x600`; a random
  encounter is `fieldstg_encounter_start`, a scripted one `fieldstg_start_listed_battle` / `fieldstg_start_battle_5` (never
  touch those). `fieldstg_stage.battle_lists->encounters[area]` holds the random lists; that `area` is attribute layer 4
  **inside the stage** (0..2), not the world-map area, and the battle drawn is `events[pad_random.next() & 7]`.
- Reward: `stfgtrep_rewards[records_battle_results.battle]` has `tech_exp`, `exp`, `money`; the item comes from
  `records_battle_results.item`. Species record: `FightstgEnemyRecord` (file `0x1CF`, 193 species) with `id`, `item`,
  `drop_rate`, `resists`, `name`.
- Flee: `fightstg_rules_roll_escape` returns 0 before rolling if `records_state.blocked[11]` is set or the status forbids,
  else the chance is `(battle->escapes + 1) * 8`. `fightstg_battle.state.type`: 0 normal, 1 the enemy may flee, 5 script, 6 boss.
- World area: `ststatus_map_areas[map & 0xFF] & 0x7F` (0..46), bit 7 = second server (Amaterasu).
- A stray `object_new` in the field **does not run** (nobody updates it): use static state plus a per-frame hook (like
  `fieldstg_rush_frame`, called from `fieldstg_camera_update`). In one ordering-table entry what is added later is drawn
  **below**: text first, dark panel last. `def_poly_text` (`definitive_callout.c`) draws text in polygons for screens without
  the game font: capitals, `2`, `3`, `!`, `?`, plus `J` and `P`; a new letter is one row of the `glyphs` table.
- Hook points (names, not lines, which drift): battle end `def_log_end(fightstg_events.result)` in
  `wfightmn_800A6440.c`; map loaded (`get_map() + 0x1E00` flag, just before `def_hard_party_fix()`) for the regional reset;
  `fieldstg_encounter_start` (Rare swap before the Rush block); `fightstg_rules_roll_escape`; Bits at
  `stfgtrep_80082E70.c` (the OVERFINISH adds there, the model for "a function that returns and clears"); XP
  (`exp = stfgtrep_rewards[...].exp`); Cardmon test `(enemies[0].digimon - 0x1C9) >= 8` (Cardmons are species `0x1C9..0x1D0`);
  Bestiary rows `BS_ROW_Y 0x40`, `BS_HINT_Y 0xA5` in `fieldmenu.c`; shop in `stitshop_8008321C.c` (selection, max buy, debit);
  `records_items[]` (about 402 entries; about 124 weapons, 78 armours, 69 accessories); defeat to map `0xE00`
  (`wfightmn` `set_next_map(0xE00, 0)`).
- Making a battle state for tests: `env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/port/dw2003 --disc iso/dw2003.cue
  --script tests/replay/scripts/first_battle_save.json --save-state battle_start:/tmp/battle.state --save-state-exit` (first
  battle), and `--save-state 41824:/tmp/field.state` for the field with random encounters (map `0x21D`, two frames before
  the script ends). Then `game_start` with `extra_args ["--load-state", ...]`, `mem_write` on static symbols
  (`gamestate_data+0x30` = `encounter_timer`, 1 forces an encounter when walking) and `screenshot`. Confirm is `CIRCLE`,
  cancel `CROSS`, the field menu opens with `START`. In-game lab scripts (outside the repo, in `.dumps/tasks/ingame/`):
  `dri_talk_ingame.py`, `guide_ingame.py`, `market_ingame.py`, `dri_ingame.py`, `equip_ingame.py`, `rush_float_ingame.py`;
  they need `tools/venv/bin/python` and the lab's disc. Only `dri_talk_ingame.py` and `guide_ingame.py` (plus
  `capture_definitive_ui.py`, `probe_guide_and_dri.py`, `test_kazuki.py`, `verify_three.py`) are in that folder on this
  machine (checked 2026-11-18); the others stayed in the lab. What they did, per PRD 4: `market_ingame.py` opens the Black Market from the map
  0x21D field state, swaps, buys, is refused, closes and runs the sweep; `equip_ingame.py` fights four battles with the same draws
  (own gear, then a variant of each archetype on the leader) and compares the stats, the battle stat block, the `twice` line and
  how many player turns come before the enemy's first; `dri_ingame.py` covers list, unlock, start from the list, defeat after one
  stage, victory and repeat; `dri_talk_ingame.py` the wait after an agent's talk.

**Layer A (chain and Rare Target), rules and reasons.** The region is the world area **plus server** (all 8 bits of
`ststatus_map_areas`), not the `area` of `encounters[area]`. Use `pad_random` only after deciding the feature is on, so
replay sequences are untouched. Only random encounters count: a script or story-boss battle does not (remember at encounter
start that the fight came from `fieldstg_encounter_start`). The chain curve was the owner's amendment of 2026-11-13: nothing
at 5, rising evenly per win to certain at `DEF_HUNT_CHAIN_SURE` (10): 20% at 6, 40% at 7, ... 100% at 10 (before it was 5% at
5, +5% per win, cap 40%). The swap changes `event->battle` only; `stage` and `music` stay the drawn encounter's, so the
fight happens on the region's scenery. `def_hunt_active()` holds from the swap to the end of the fight and clears in
`def_log_end` whether won or lost; the flee is forced to succeed (before the `blocked[11]` check) because a boss battle used
as a Rare Target may forbid fleeing. The first target rule ("the boss the region itself starts") failed: only 13 of 67 regions
with encounters have one and many are optional superbosses far above the encounters. The second rule (approved 2026-11-13,
66 of 67 regions): a game boss with its own level between T and T + 15 (T = the region's highest random encounter), preferably
one the region starts, no repeats while another exists, fought at T + 3; Galacticmon's battles never enter. Both were
replaced by the owner's later rule (one Digimon alone, region average + 5, HP x4, 71 regions, regional "Core" items) written
in the hunt sections above. At the time of PRD 4 the target also was the boss battle's own reward until layer B.

**Layer B (rewards, materials).** Materials are not disc items (no free item IDs, the item screen reads the disc table): they
are counters in the save (`REC_EXTRA + 10..25`, cap 99; reads clip a value above the cap because no checksum covers it).
XP: `stfgtrep_rewards[b].exp * DEF_HUNT_XP_MULT` where `b` is the random encounter the Rare Target **replaced** (the area's
standard encounter, not the boss battle), split among members like the game (6/10 with two, 1/3 with three). The card reward
became an **R-Booster** because the results screen shows one item: when the battle left no item the Rare Target leaves one
R-Booster (items `0x186..0x18A`), 01 below level 25, +1 per `DEF_HUNT_PACK_LEVELS` (15) levels, 05 from 70; it is **one**
pack (`DEF_HUNT_PACKS`, the owner, 2026-11-13) because the x15 of PRD 3 (`DEF_CARD_BOOST_PACKS`) is for plain Boosters and
would make the rare too plentiful; the screen marks the Booster it gave (`def_hunt_take_pack`) and only it counts as one
(`def_hunt_pack_count`). The Cardmon drop table was not used. First version: a material per target battle ordered by the
battle's lowest level (9 of 16 materials had no source); superseded by the commons/rares split (owner's amendment, same day):
the 16 approved names are commons of any Digimon; commons come from `def_hunt_commons` (the species that appear in random
battles, ordered by the highest level they appear at, cut into 16 equal groups, weakest first; list in
`.dumps/tasks/hunt/materiais.md`); rares are one per region, "<species> Core <numeral>"; the numeral came from the target's
level order when the lead species repeats (`variant` field), and the Rare Target showed the numeral in the HUD and Inspect
("Datamon IV") so it did not look like the same Digimon with a higher level. Rares also have an "ever held" bit each, in the
**second card record** (part 6 of `memcard.c`, `0x7B00 + slot * 0x100`, "HUNT", version, checksum, read and written after the
first by STGMCARD steps `0x49` and `0x37`). The material is awarded at victory (`def_hunt_battle_end`), not on the screen, so
closing the game on the results screen loses nothing. Results screen: one line per material after the item, "Hunt material:
Bit Shard x2" for each common and "Rare material: <name>" for the rare, from step 22 of `stfgtrep_main_run` (free) in a loop;
the pending reward clears at the end of the screen (step 50) and at the start of the next battle (`def_log_begin`). Composed
names at run time: the species name from the disc (text `0x4E`), the rest from `$HuntRareWord`, `$HuntRoman*`, `$HuntSpace`
(quoted, which the loader now preserves), so no game text is embedded. Common drop chance `DEF_HUNT_COMMON_PCT` 25%.

**Layer C (Bestiary rows).** Two lines under the five family rows with a rule above (the free space was 31 px, `BS_ROW_Y + 5*14
= 0x86` to `BS_HINT_Y 0xA5`): the label "Hunt" (`$HuntDrops`, the same name as the results line; "Drops" invaded the names
column), each material's name in the families' name column (x 42) and the amount held ("x12", grey at zero) at `BS_HELD_X`
(136). The longest name, "Obsidian Digizoid", is 89 px in the game font (ends at x 131). Unknown species show `???`; known
but without any target material show `-`. Species to materials: `def_hunt_species_materials` walks the target battles
(`fieldstg_battles[...].enemies`) and marks each battle's material; at most 2 materials are shown (Bulbmon had two, battles 323
and 315). Windows used: `bs_info[6..7]`, `bs_states[5..6]` and a new `bs_drops`. The second line (rares as a collection, "Rare
Cores 4/13") turns orange when complete. `fieldmenu.c` preprocesses the same without `PC_PORT`.

**Layer D (prices), the survey.** Prices are the compiled game's (`records_items` in `src/main/records.c`), not the disc's; every
resale is half of the buy price and nothing sells for more than it costs. The 60000 pieces (Ronin Blade, Brave Sword, Dramon
Horn...) are shop 18 and resell for 30000; found or won cheaply (the owner's example "bought for 2,000, sold for 30,000") they
become 30000 Bits. There is no auction in the code. The rule leaves items no shop sells as they are (resale up to 2000). Full list:
`.dumps/tasks/prices/precos.md`. The shop code reads every price through `STITSHOP_BUY` / `STITSHOP_SELL` under `PC_PORT` and its
local `u16 price` became `s32` (a new price passes 65535); `records_items` is not altered; in a replay script the prices are
the game's. The owner's decision (2026-11-13) was to apply the proposal directly and review later; the PRD's first idea (a resale
cap as a % of the buy price plus an exception table of absolute values) was replaced by the knee rule.

**Layer E (forge), the recipes.** *Redone 2026-11-17 (the owner: every shop item asks for its vendor's own region's
materials): see "The forge" above; the rule is in `tools/gen_recipes.py`'s docstring.* The first version: by the old price of a weapon/armour/accessory sold in a shop: under 5000 Bits only; 5000-20000
(intermediate) one common (from the 3rd to the 9th by price order), 10 to 25 of it; 20000 and up (top) one of the seven strongest
commons, 30 to 50, plus a region's rare, the rares in order of target level spread over the top items by price order, one (two
from 50000). The owner's model: commons in large amounts, rares for the best gear. Shop behaviour (`stitshop_8008321C.c`, under
`PC_PORT`): on selection without the materials the refusal is the no-money one with "Not enough materials."; max
buy is capped by what the materials allow (the scarcer of two); purchase takes the materials with the Bits; the item panel's slot
line shows the requirement ("Peta Prism 0/30, Bulbmon V 0/1" for Tenjinmaru), orange while short. `definitive_glyph.c` holds the
assembled texts (string, count, roman numeral) shared by the hunt and the shop. List: `.dumps/tasks/forge/receitas.md`. **Not
verified in the real shop screen**: in game, changing `stitshop_goods_N` did not change the shop list (probably the overlay data is
reloaded on open), so the refusal and the max-buy limit have only the unit test.

**Layer F (variants), the survey and the rules.** No free item ID exists (`records_items` runs 1..402, the save inventory is
`items[0x193]`, no empty or duplicated item), so each variant is a shop item **converted in place**: same ID, icon (`type`), slot,
group, users, charisma, price and forge recipe; power or guard, hit or evasion, both bonuses, name and description change. Only
two items were strictly dominated in their tier (Long Sword, Kulon Sword). Stats are recomputed from the item data every time
(`gamestate_get_stats`, the shop preview, `fightstg_rules_get_stats`), nothing is saved, so an old save wearing one just gets the
variant's stats. One of each archetype per shop tier (the 9 lists of weapons and armours; shop 18, set pieces only, is left out),
so in every equipment shop of the campaign. Budget: each stat is worth Power points (`DEF_EQUIP_W_*`: Guard, Spirit, Boost 1; Wisdom
0.5; hit and evasion 0.25; a resistance 0.05 because in the definitive combat it only feeds affinity); a variant never exceeds the
best score of the tier's ordinary items of the same slot that share a user. Speed: one-handed weapons (not staves or arm guards),
power of the tier's best weapon minus 50%, Boost completes the budget (+5 in tier 1 to +85 in tier 9, 15% to 25% of the rookie's
expected Boost at the shop level), calibrated by the turn rule: the player opens and the enemy's first turn comes after its whole
delay (`1000 * sqrt(our Boost / its Boost)`, 707 to 1414), so more Boost than the enemy gives two actions before its first, and in
the long run `Boost / enemy Boost` actions per enemy action (up to 2). Elemental (head): guard of the tier's best minus 50%, one
family's resistances complete the budget (Water+Ice and Thunder+Machine split across the pair, which `def_affinity` sums; Dark
weighs double), at most 400 per element: +85 Fire in tier 1 to +355 Wind in tier 9 (affinity +0.025 to +0.107 on a WEAK! hit and
again on a staggered target); the family is the item's existing element, else the least used so far, and all five appear. Tank
(body or shield): guard of the tier's best +50%, Spirit 25% of it, negative Boost that returns to the budget (-6 to -30). Choice
order: dominated first; then the trait (an item already leaning to the archetype, Wing Sword and Fenrir Sword for Speed, Wool Cap and
Night Vision for Elemental; one with another archetype's bonus last, so the Kung Fu Cap, Spirit and Wisdom items stay); then
redundancy (users keep an ordinary option of the slot at least as strong); then the cheapest. Never: items with an effect by ID in
FIGHTSTG (poison, critical...), with a strong type, or set pieces; accessories stay intact (including the Fire Power of layer H).
Written to `RecordsWeapon`/`RecordsArmor` by `def_variants_hook` every time (called inside `def_mp_hook`: new game and battle start,
and at every field map so a loaded save has them before the first shop or status screen); the values are absolute, the reset
restores the disc table and the next hook writes again. In a replay nothing is written and the names and descriptions (`ITMNAM` /
`ITMINF`, entry = item ID) are hidden: `def_text_override` returns the disc text when `def_variant_text_hidden` says the text is a
variant's (`DEF_EQUIP_ON` 0 turns it all off). Prices and recipes are inherited from the item (the layer D and E tables are by ID),
which is why changing a price would touch both rules. Tests: `tests/port/definitive_variants.py` (driver with `records.c` data) and
`gen_equipment.py --check` (table = rule, budget, no variant is a direct upgrade of another item of the tier, names up to 15
letters, descriptions up to 34). List: `.dumps/tasks/equipment/variantes.md`. `tools/gen_equipment.py` also has `OTHER_TEXTS` (items
outside the variants with port text: 26, 46, 67, 70, 76).

**Layer G (Black Market), how the choices were made.** The trader is a town extra spoken through the hook `def_bm_talk(map, key1)`
in `fieldstg_actor_talk_to` (after the redirect loop); the game's talk does not happen and the screen opens on the next idle frame
(`def_bm_frame`, right after `dw3_travel_frame` in step 0 of `fieldstg_manager_update`). The extra: map 0x20C actor 42 (WSTAG240, in
the town of inn 0x20A: a single line, always present, no flag, no event) and the same house in the second half (0x27B, WSTAG930);
`tools/bm_npc_candidates.py` lists each town's candidates with the decoded line (needs the disc). The owner was to confirm the extra.
Screen: Guardromon-network model (polygon panels, game font, `event_running` while open, the player held like the field menu).
Exchange: rares in hand one per line ("Datamon IV x2"), confirm one and pick the common (up/down or left/right over the 16, "Get:
Data Chip x12   Held: 30/99"), each confirmation swaps **one** rare for `DEF_BM_COMMONS` (12), only if all 12 fit under the cap, so no
trade creates or loses anything outside the rate. Replicas: `tools/gen_replicas.py` takes equipment no shop sells, with a sell
price, that no event cites (item flag 0x80..0x8E in a flag list or in `set_flag`/`get_flag`, and `gamestate_cond_items`; the
blacksmith quest's weapons are out), the 16 with the highest sale value, ties by ID: the seven Power S, Machinegun, Magician Rod, Ghost
Fang, Pulse Guard, Drill Horn, Chain Saw, Naginata, Recover Crest, MP Mega Proxy. Price `DEF_BM_REPLICA_BITS` (20) x the sell price
(40000, 30000, 25000, 20000) and `DEF_BM_REPLICA_RARES` (2) rares of **distinct** bosses (the species leading the Rare Target:
"Datamon IV" and "Datamon VII" are the same boss); the screen picked the two most plentiful (tie: the weaker target) and showed them
("Cost: Datamon IV, Bulbmon III"), a rule since replaced by a fixed pair per replica (`DefReplica.cores`, see "The Black Market, in
detail"); purchase takes two confirmations, the second warning that the replica resells for 0; you may buy
again (one per Digimon). Resale 0 (the PRD's simplification, accepted): one bit per replica in record 1 `REC_EXTRA + 26..27`
(`def_record_replica`); with the bit on, `def_item_sell` pays 0 for that ID even with an original beside it, and since the shop's
sell list skips what is worth 0 the replica is not even offered. The bit clears when the player has none of that ID left, in the bag or
equipped (`def_bm_sweep`, every field frame): the points where the count changes (sale, event, discard) have no hook of their own, and
until the sweep the shop only pays 0 for an ID no shop sells. Tests: `tests/port/definitive_market.py` (rate and cap, 400 trades with
no value created or lost, distinct bosses, purchase and refusals, resale 0 and the bit's end, equipped counts, record round trip, old
record, new game all zero, replay) and `gen_replicas.py --check`; in game `market_ingame.py`.

**Layer H (DRI), the implementation record.** There was no species-swap proof of concept: a simulation is an ordinary battle whose
three enemies are the line's three stages in order, and the engine already does "the battle does not end, the next form enters":
when the current enemy falls `wfightmn_knockout` (step 1) finds the first enemy member still standing, plays its entrance
(`fightstg_entrance_create`: loads the model, a 60 frame flash with sound, the new model takes slot `0x10`) and shows the name; it
enters with the full HP it got at the start (`wfightmn_init_members`), the party stays as it was. Models are **not** all loaded at the
start, only the first (`slots->add(0x10, enemies[0])`); each next one is loaded on entry, like every multi-enemy battle (battles
305-308 chain three bosses so). A new `DIGIVOLVE!` callout (`DEF_CALLOUT_EVOLVE`) shows over each new model
(`def_dri_stage_enters`, `wfightmn_knockout` step 3). "Mirrored enemies" exist only as species of file `0x1CF` (193): the rivals'
Ultimates (MetalGreymon, Paildramon, WarGrowlmon, MagnaAngemon, Taomon, Kyukimon, Armormon, GrapLeomon: 449-456) and the BK Megas
(WarGreymon, Imperialdramon, Seraphimon, MegaGargomon, WarGrowlmon) are there, Greymon, Garurumon and line, Growlmon, ExVeemon, Angemon
and Gallantmon are not; so each line's Champion is an ISO equivalent and the **Gabumon/MetalGarurumon and the Beelzemon-solo lines do not
exist** (the PRD's plan B, "equivalent bosses from the ISO"; the original proposal also named Gallantmon). Assembly
(`definitive_dri_screen.c`, `sim_start`): `fieldstg_start_battle` on an existing boss battle (the model: type, `blocked[]` immunities
and, on a win, its XP and Bits), then `records_state.enemies` = the three stages, `stage` 19 and music `0x60880000`
(`DEF_DRI_STAGE`, `DEF_DRI_MUSIC`, those of boss battles), `blocked[11]` on (no flee), `has_prize` 0; it does not go through
`fieldstg_encounter_start`: no Rush, no Rare Target, no hunt chain, no commons. Table (`tools/gen_dri_challenges.py`,
`definitive_dri_table.c`, list in `.dumps/tasks/dri/desafios.md`): 5 challenges ordered by the story battle of the Mega, Champion
level `DEF_DRI_LEVEL_FIRST` (24) + 8 per challenge, each stage +3; HP and `stat_scale` from the species' own rows in the battle table
(interpolation between two of its levels; outside them the nearest row times (level ratio)^0.8, the growth of the game's own rows); MP
from the line; model battle = the type-2 battle with none of the line's species, level nearest the Mega. Unlock: the strongest common
whose band ends at or below the Champion's level, 15 to 40 of it (`DEF_DRI_COMMON_*`); one rare (two from the 3rd challenge,
`DEF_DRI_TWO_RARES`) of Rare Targets up to `DEF_DRI_RARE_OVER` (5) levels above the Champion, the line's own first. Rewards are the
game's own elemental "Power" accessories (`records.c` 339-359), no new item; items enter the inventory at the end of the battle
(`def_log_end`), win or lose; a replay gives the model battle's XP and Bits if won. Defeat (`fightstg_events.result` 2) does not go to
the title (`0xE00`): `def_dri_defeat` (in `wfightmn_battle_end`) returns to `gamestate_data.field_map` like fleeing, with the party's HP
and MP **as before the simulation**; on victory the result is any battle's (a fallen member at 1 HP). Because WFIGHTMN picks the next
map before the battle log ends, `def_dri_defeat` ends the simulation itself, and WFIGHTMN asks every frame until the map changes, so the
answer stays 1 until the next battle or the field summary (a 0 on a later frame let the game-over `0xE00` overwrite the field). After
the simulation a panel counts the stages felled and the new accessory ("No new reward" on a repeat), closed with one press. The screen
follows the Guardromon-network style: title, list (best stage "2 of 3" or "--", the three species names composed at run time, grey while
locked, green when complete), challenge panel (rewards by item name, levels, the cost "Unlock Code Chip 12 of 15" / "Bulbmon I 0 of 1",
orange while short) and keys; confirm picks, a second press unlocks (pays) or starts. Fixed in the lab: the 11 px font has no `/`, `:`
or `,` (they print as `0`), so separators are " - " and counts "12 of 15". DRI Agents: the actor ids are shared sprite types (21 is the
Guardromon of the 25 inns, 33 is in 58 maps), so `AGENTS` in `tools/gen_dri_challenges.py` (map, actor id) was filled
with `tools/find_dri_agents.py`, which decodes each `wstag*.c` actor's lines; the owner's amendment (2026-11-13) removed the story's pig
forms (actors 157-161, "Oink...": a quest state, not agents), and the table is now the eight explicit (map, actor) pairs (an actor of
the same sprite on another map is an ordinary NPC; the "map -1 = any" rows of the first version are gone): Kazuki/Patamon
(43, `0x21D`) and Michael/Renamon (44, `0x21D`), Bob/Veemon (43, `0x226`), Johann/Agumon (43, `0x22B`), Jimmy/Guilmon (43, `0x22E`), Ken/
Kotemon (43, `0x258`), Yuji/Kumamon (44, `0x258`), Yang/Monmon (163, `0x258`), the DDNA researchers, one quest per agent in the whole
game. The agent's own talk is **never replaced**: `def_dri_talk` only arms a wait and returns 0, so talk and DDNA quest run as in
the game, and the list opens after `DEF_DRI_WAIT_IDLE` (30) idle frames on the same map (`def_dri_wait_step`; one idle frame between two
parts of the talk does not count; a battle or another map cancels; after `DEF_DRI_WAIT_MAX` it gives up). Each simulation replays the
battle data of a classic Tamer by title, no name: "The Leader of Courage" (Agumon line), "The Child of Hope" (Patamon), "The Heir of
Miracles" (Veemon), "The Dreamer Tamer" (Guilmon), "The Calm Tamer" (Terriermon); choosing an open challenge shows the Tamer's page
(title, the three stages, the line; x fights, triangle returns) and the summary shows the victory line (all three) or the defeat line;
texts `$DriRival<n>*` in the order of `LINES` of `gen_dri_challenges.py`, which checks they exist; the 11 px font has no comma, colon,
slash or apostrophe, so the lines avoid them. Fixed together: closing the summary with CIRCLE returned the field in the same frame and
the player control saw the same press and examined what was in front (stuck, `actor_busy`); the DRI screens now return the field only when
their buttons are released (`dri_release`). Rules are per (map, sprite) and the same sprite may be another NPC in the same map; since
2026-11-17 the list is also armed only when `dri_rookie` finds a Rookie in the actor's own talk conditions, so such an NPC should not open
it (not checked in game).

**Layer I (floating Rush panel), why and how.** The results screen is a stage of its own (`STFGTREP` loads its files, objects and layers
and runs as map `0x1400`); drawing it over the field would mean loading that stage inside FIELDSTG with a shared heap and ordering table,
which is the expensive and risky part; the logic of it is reused without the screen, plus the dark panel look the field already draws.
The swap is in `fieldstg_rush_leave` (`fieldstg_rush_float`): with `def_rush_float()` (`DEF_RUSH_FLOAT`; `DW3_RUSH_FLOAT=0/1` or
`def_rush_float_mode` for the test) the manager keeps state, steps, `next_map` and `map_entry`, `fieldstg_start_battle` builds the fight as
before (including the Cardmon prize), everything is restored, `event_running` and `battle_starting` go to 0, then `def_rush_results`,
`def_rush_apply_quiet` and `def_rush_panel_open`. `def_rush_apply_quiet` (in STFGTREP, under `PC_PORT`) makes the same calls in the same
order as the screen (split XP, item 0x141, share to the party, `DEF_XP_ALL_FORMS`, techniques, `add_exp`/`raise_stats`/TP per band/lost-TP
note, new forms, Bits with 0x142 and OVERFINISH, item with Boosters, material lines, `def_hunt_reward_done`). Panel (`definitive_rush.c` pure,
`definitive_rush_panel.c`): 11 px font on layer 0x1004, above the player and pinned to the screen at the time (now the code puts it at
the top of the screen, centred, sliding in from above, because the camera keeps the player in the middle and a panel there would cover
the one walking under it; headings in the polygon letters of `def_poly_text`, the rest in the game's 14 px dialogue font, `RP_FONT` 1,
a window per text, the dark panel added last so it draws below; `event_running` is never set, which is what keeps the player free):
`+N EXP` and `LV UP!` per member, `NEW
TECH!`, `TP FULL!`, Bits, item (`???` until the names file arrives), materials; 120 ticks at the time (now 175 and 10 rows); it closes when the
field holds the player; a second Rush closes it on start and opens its own. Decisions: the panel is enough for a new technique or form (no
full screen); the screen's "more than 3 forms" hint does not appear; the level-up stat gain comes from the same draw as the screen but at a
different moment, so the numbers may differ (the rule is the same). Equivalence test: the same skipped battle by today's Rush and the floating
one leaves `gamestate_data` identical (party Digimon block, `money`, items, cards); any difference was a blocker. Tests:
`tests/port/definitive_rush.py` (31 cases) and `rush_float_ingame.py` in the lab.

**Risks the PRD tracked:** save space (a second record per slot if short, never grow `REC_EXTRA`); replay change (`pad_random` only after
`def_on()`); a boss-fight Rare Target stuck without flee (forced flee before `blocked[11]`); no free item IDs (materials never items);
a playable model that does not animate in the enemy slot (plan B above); a screenless reward diverging from the screen (equivalence test);
Bits inflation (resale knee, replicas resell 0). Save budget at the time of the PRD: of the 48 bytes of `REC_EXTRA`, 8 were used; 3 free, 4-7
Guardromon mask, 8 `region_area`, 9 `region_chain`, 10-25 materials, 26-27 replicas, 28-29 and 30-33 DRI (later moved to record 2), 34-47 reserve.
The unit of a "replica bit per item ID" does not fit (about 402 items = 51 bytes), hence the index in `def_replicas[]`.
**Decisions the PRD left open and their outcome:** Rare Targets per region and scale (superseded); materials (superseded); price table
(applied directly); recipes (applied); new items (variants in place, no new IDs); Black Market list and prices (applied, owner may change);
Gauntlet lines (the first idea was Agumon, Gabumon, Guilmon; shipped Terriermon, Veemon, Patamon, Agumon, Guilmon); Rush panel (applied).
**Not tested in game at the time:** the HUNT record save and load on a real card at a Guardromon save point (the field menu has no SAVE:
save, reload, check the Bestiary "Hunt" and "Rare Cores" rows and the DRI "Best"), the forge line on the shop screen (above), the real DRI
agent talk, the Guide's arrival notice (the log says `notice 1 (4) at progress 4` but a capture 30 frames later shows only the map title
"Central Sector"; the step-5 notices "NEW OBJECTIVE" and "AUCTION OPEN" do appear; maybe it appears after the title fades, or is lost).

## The Velvet Room

**Status: working and committed (`d927826` .. `46424ef`), with open items (below).** `port/game/definitive_velvet.c`. It runs
the bar map 0x20F (WSTAG255) as a room. **Ways in (rewritten 2026-11-18, `46424ef`; the owner vetoed drawn doors): no door
sprite.** Each inn has a spot (`doors[]`, one per inn map, the second half's copies sharing the first half's layouts) in front
of a piece of its own scenery that a faint blue light marks (`looks[]`). Standing within `VL_RADIUS` (12 px) of the spot shows
the map exits' own prompt (the balloon with the up arrow: `fieldstg_icon_create` type 10, id 6) and only **CIRCLE** enters,
as a map exit is taken; stepping off closes the icon, and a way that opened under the player (Repeating Tom's news, see the
story layer) prompts only once they stepped off and back on (`vl_armed`). The prompt belongs to no game object, so
`vl_icons_run` runs it every idle frame and `dw3_velvet_field_loaded` drops it at each stage setup. `vl_from` remembers the
inn. The ways stay closed until the story opens the room (`def_story_velvet_level() > 0`, see
[The story layer](#the-story-layer-velvet-room-in-the-main-story)). Spots (map x,y: the light): 0x20A 339,192 the wall
niche's dark pane lit from inside; 0x223 205,269 the lantern on the deck's logs burns blue; 0x230 210,300 light under the
painted stage's lip; 0x238 504,296 light through the red double door's crack; 0x23F 628,392 light under the bedside chest's
lid; 0x247 540,304 light from the gap under the metal ramp's foot, on the sand; 0x249 403,187 the fallen pipe's open end;
0x25B 355,313 the floating platform's two pins run blue; 0x25D 1074,604 a mortar joint of the brick wall; 0x263 383,235
light from behind the cabin's barrels, on their rim and the post; 0x269 553,262 a slit vent in the machine's dark base;
0x26D 156,500 the last capsule lit inside its hood; 0x26F 1120,830 the alcove capsule by the hatch. The second half (0x279,
0x292, 0x29E, 0x2A5, 0x2AC, 0x2B1, 0x2B3, 0x2C4, 0x2C6, 0x2CB, 0x2D1, 0x2D6; 0x26D has no copy) uses the same spots and
lights over the digital-world palette: 25 maps, plus the Time Room's door on map 0x288 (160,440, a circuit trace of the
floor carries the light; `def_time_door`).

- **The lights** (`VelvetHint`, up to `VL_HINTS` 4 per map): `VL_HINT_GLOW` (a Gouraud fan ellipse, blue at its heart,
  nothing at its rim), `VL_HINT_SEAM` (a 1 px crack with a halo, fading in over its first quarter and out over its last),
  `VL_HINT_PIXELS` (lights the scenery's own pixels: `vl_runs`, rows read off a screenshot taken without the lights). Each
  has `power`, `front` (drawn over the foreground tiles, depth 2, instead of on the floor and walls, depth 5) and `dim`
  (first subtracts red/green, abr 2, because blue added to a light surface only whitens it). The lights are added (abr 1)
  through `DR_TPAGE` steps without dithering (dithered, a faint light is a checkerboard); one breathing pulse, a triangle of
  `VL_PULSE` 120 frames from `VL_PULSE_LOW` 150/256 to full. `DW3_VELVET_NOHINT=1` leaves the lights out (the bare
  screenshot the tuning compares against). The table and `vl_runs` were written by a tuning script from screenshots
  (`.dumps/tasks/velvet/looks/looks.py`, `runs/*.json` from `mask.py`; scratch). Each light was judged from the image alone
  by Sonnet agents (before/after, whole screen and zoom, up to 5 rounds); for 0x238, 0x25B, 0x25D, 0x263, 0x269 and 0x26F the
  judges stayed divided (too weak against too strong), so the middle was kept and the owner decides in game.
- Music: the pair `music` 0x3B with `sound` 0x60EC0000 (**set `sound`, not only `music`**), unless the room's own
  arrangement was generated: then bank 72 and key `0x61200000` (`dw3_velvet_music`, see "The room's own music" below).
  Map title "VELVET / SERVER" (`$VelvetTitle`/`$VelvetTitle2`; "TIME / ROOM" while the Time Room runs). The player comes in
  at (215,335) facing in; the exit is the front door, back to the Guardromon of the inn the way was taken in
  (`dw3_travel_node`). The upper door (the bar's map event type 1 to 0x20A) is closed, except in the Time Room before its
  gate is passed, when it leads to the trials (`def_time_side_door_open`).
- Actors (`FieldstgPlacedActor` + `FieldstgTalk`; who is present depends on the room's story level, `vl_guests[]`):
  - **Igor**, the host (actor 109, the game's suited man `P055GMGD` with a hand-drawn head, VRAM place 8, at (268,318) on the
    lower floor, dir 7 towards the door, `no_turn`); TALK05 18/19/20. See the story layer.
  - **Oikawa**, the Black Market trader (hand-drawn, actor 42 at (275,255), dir 1, `no_turn`); TALK05 8/9/12/13, and 99 the
    first time after the Bug Maze Pit.
  - **The mentor**, Gennai (the old man, actor 0x39, at (352,194) dir 1, in front of the upper door, the Time Room's way;
    owner 2026-11-18, `f8f0982`; before that on the organ stage at (318,165)); TALK05 81/82.
  - **Etemon** (actor 0x61 at (352,284), dir 5, facing the piano).
  - **The pig** (actor 318 at (226,296), dir 0, on the floor by the carpet's edge, left of the counter's front; before
    2026-11-18 at (245,292)); TALK05 79. It is the only truly unseen art of the 54 never-used actor IDs (P182BOMP, ID 318).
  - **Guilmon** (actor 0x172, upstairs by the stairs' head at (126,200), dir 0) with the Demon Bell; TALK05 77/78.
  - **Sepikmon** (actor 61, VRAM place 11, at (86,214), dir 7 towards the stairs' head: upstairs in the corner left of the
    Demon Bell's keeper; owner 2026-11-18, `e854782`/`f8f0982`; before that (176,306)), the hunts' master; TALK05 14/15
    (16/17 in the story's Seiryu hook).
  - VRAM places: of the bar's patron places the room leaves free, 8, 9 and 10 draw a figure; 7, 12 and 13 show garbage for
    any figure (tried 2026-11-17).
- `dw3_velvet_text_own()` serves the English text in every language for the port-owned TALK05 entries.
- State is RAM only (`vl_room`, `vl_from`, `vl_shop_*`): a slot saved inside the room loads as the normal bar. Savestate
  hooks: `dw3_velvet_savestate`, `def_bm_savestate`, `def_travel_savestate`, `def_evohint_savestate`,
  `def_callout_savestate`, `def_dri_savestate`.
- Talk reach: `dw3_velvet_talk_find` (the trader 30 px sideways / 16 up / 28 down; others 24x16).
- Debug env: `DW3_FEAT_VELVET`.
- **Pitfalls:** a `POLY_F4` with semi-transparency dirties the void; the layer draws what was added last, first; the
  wstag handlers table is shifted by +1.
- **Pending:** the owner's look in game at the lights (the six "divided" ones above); FR/IT/DE/ES translations of port
  texts; a save inside the room. The doors' sprites were dropped (the owner vetoed drawn doors, 2026-11-18) and the room
  was arranged on 2026-11-18 (positions above).

### Velvet Room: details, decisions and history

**Owner decisions, in order.** (1) First the Velvet Server, a room where the new NPCs live. (2) NPCs: the Black Market trader (moves
into the room) and a Gennai-like mentor (Blast Gauge / Chosen Evolution; the hook and the place only, the content is another task);
later the owner asked for a joke Etemon at the piano with the line "I heard this music in another game", through the same path
(translatable texts). (3) Scenery: Asuka City's bar, map `0x20F` (`src/wstag/wstag255.c`, background `S255PACK` = file `0x1AA`), the void
painted blue (a 16-band gradient on layer 0x1000, measured from the owner's mock-up `bar_mock_azul.png`) and the room in a blue tone.
(4) Entry: a blue door in each of the 13 inns of the Guardromon network (`nodes[]` of `definitive_travel.c`, `gs_inns[]` of
`game_mods.c`), the owner approves or moves each. (5) The front door (exit) returns to the inn; the top door stays closed. (6) Music: random
among rarely used tracks for now, the owner composes later. (7) A test build is fine, the owner adjusts later. (8) 2026-11-16: the area's
name is **Velvet Server** (the map card shows VELVET / SERVER in both languages; place names stay English in PT-BR). (9) The trader is
named **Oikawa** (Yukio Oikawa, after the Digimon character); the name is in the lines; the art is original, hand-drawn: two attempts
resembling the character and a hat seller were rejected for clashing with the game's style. (10) The mentor is, for now, an old NPC of the
game, swappable for a better one in a good corner; drawing a new NPC comes later, the focus is the mechanics. The owner's answer on the doors
(2026-11-16): redraw them: a better hand-drawn door sprite in the game's style (nothing cut from the disc) and place each beside the wardrobes
and cupboards of the maps, as in the original text of "section 6" below, instead of the inn Guardromon; "any point of the map is allowed".
(11) 2026-11-17: the room grows with the story (guests arrive by story step; see the story layer). (12) 2026-11-18: **no drawn
door at all** (vetoed): each way in is a spot before the inn's own scenery with a faint blue light, entered with CIRCLE like a
map exit; this supersedes (4) and the 2026-11-16 answer on the doors. The room's arrangement (Sepikmon, Gennai, the pig) was
redone the same day. (6) is superseded by the owner's own arrangement (below), and (5) by the Time Room (the upper door).

**How it works.** The way in is a spot of the inn (see the lights above; until 2026-11-18 it was a drawn blue door); standing on it (radius
12 px) shows the prompt, and CIRCLE (`dw3_velvet_frame`) calls `fieldstg_goto_map(0x20F, -1, 215, 335, dir 1)` with the warp sound (0x40004); `vl_from` remembers the inn. In room mode (`vl_room`,
RAM) `wstag255_setup` ends by calling `dw3_velvet_stage_setup`, which swaps `fieldstg_stage.actors` for the NPCs, the music, and paints the void.
**The room's own music** (commit `8042491`, 2026-11-17; the owner listened and accepted it, smoothing included):
an octet arrangement played by the game's own sequencer (LIBSND, `SsSeqCalledTbyT` every vsync), not a WAV.
`tools/gen_velvet_music.py` runs at CMake configure: it reads the owner's local MIDI (never committed nor shipped; path
in the script, `DW3_VELVET_MIDI` overrides) and the disc, and writes the gitignored
`port/game/definitive_velvet_music.inc`; without the MIDI or the disc nothing is written and the room keeps bank 59
(`ENV019`). `port/game/definitive_velvet_music.c` serves bank 72, a copy of BGM031's files with its own SEP score
(sequence 0 the piece, 15 empty ones) and patched tones, through `PC_PORT` hooks in `src/main/sound.c`
(`dw3_velvet_sound_bank/header/body/music/stopped/silenced`). The mix: eight parts on their own channels (horn and
double bass shared channel 8 in the MIDI and cut each other off), CC7 levels 100/76/30/66/66/71/71/82 (flute, oboe,
piano, violin, viola, horn, cello, bass) written into the score so a loop keeps them; programs 8, 5, 10, 4, 6, 1, 2, 3
of BGM031; the oboe's tone centred on 62 (at 84 it played two octaves flat); the piano is BOSS01's vag 9 copied into a
spare slot (tone volume 96, centre 66). Smoothing (why it sounded coarse: attacks of 0.2-2 ms clicked, releases of
6-46 ms cut notes at note-off, and the game zeroes the reverb depth so the tones' reverb never sounded): attacks
~37-62 ms and releases ~186-372 ms on the sustained parts (piano untouched), reverb type 3 depth 64 raised by the score
and dropped when its music stops; horn and piano get the reverb flag. Knobs at the top of the generator (`ATTACK_MS`,
`RELEASE_MS`, `REVERB_DEPTH`, `REVERB_PARTS`, `LOOP_GAP_TICKS`); `--smooth off` gives the unsmoothed mix; audition
tools in `tools/music_lab/velvet/` (`play.py --smooth on|off`, `--reverb N`; below). Test: `tests/port/definitive_velvet_music.py`
(skips without the MIDI). A second tune (Adventure) is described below; it is not in the game.

**The room's music: the handoff's details (kept from the deleted `handoff_velvet_music.md`).** Score: the owner's local MIDI
`~/Downloads/Persona 5 - Aria of the Soul - Octet - Aragi Shota.mid` (`MIDI_DEFAULT` in the generator; format 1, 480 ticks
per quarter, eight named tracks; the owner supplied it and accepted the fan-project risk; read at build time, never committed
nor shipped). The SEP is written for the port's LIBSND (`psxstack/psyq/libsnd_seq.c`, `docs/FORMATS.md` "SEP"): `pQES`,
version 0, 16 sequences when `SsSepOpen` is called with 16 (sequence 0 the piece, the other 15 an empty end-of-track);
note-off is a note-on with velocity 0 (`9n`): `8n` is **skipped and its data bytes are not consumed**; tempo is
`FF 51 tt tt tt` with **no length byte**; the end is `FF 2F 00`; after a meta event the running status is `FF`, so the next
event needs its own status byte. Tempo 600000 (100 bpm), then 659340 (~91 bpm) from tick 19200 (the MIDI's values; the
generator reads them from the file). Parts (channel, first note, note range): flute 0, tick 19200, 64-81; oboe 1, tick 28800
(~36 s: the silence before is the score, not a mute), 49-74; piano 2, tick 0, 49-83; violin 3, tick 720; viola 4, tick 0;
horn 5, tick 19200; cello 6, tick 0; double bass 7, tick 3840. 24 voices: the octet steals voices (accepted). Mix (`PARTS`:
level / CC7 = round(level x 127 / 100) / pan): flute 100/127/32, oboe 76/97/96, piano 30/38/64, violin 66/84/40, viola
66/84/88, horn 71/90/48, cello 71/90/80, bass 82/104/64 (the pans are the staging that was playing; the owner did not ask to
change them; the levels are also in `tools/music_lab/velvet/volumes.json`). An extra bank's header buffer is `0xA000`
(`sound_buffer_1`/`_2` in `src/main/sound.c`). Instruments: the flute (program 8) is not retuned (the oboe's waveform, BGM031
vag 9, whose centre 84 sits on the flute's register: leave it); the oboe is program 5 with centre 62; the piano is program 10:
BGM031's own vag 14 is a noise burst, the owner rejected it, then a darker pluck, and picked **option 19 of 51**, BOSS01's
vag 9 (program 3 of that bank, 5376 bytes, ~213 ms), copied over the spare slot of BGM031's vag 14 (5824 bytes at SPU
`vag_end[13]*8`), tone volume 96, centre 66, shift 0; the tone volume (96) and the channel level (30) are different knobs:
keep both. **What not to repeat:** writing the score over the VAB header (on the listen the header lived in `sound_buffer_1`,
the score went in `sound_buffer_2`); seeking by poking `SndSeq.pos` (a bad landing ran the parser off the stream, a zero
tempo divided by zero and the score table was garbage); trusting `nm` of a binary another build replaced for `snd`'s address
in a running process (`play.py` writes the live one to `session.txt` next to itself).

**Audition tools (`tools/music_lab/`, moved from `.dumps/velvet-live/` and `.dumps/adventure-music/`).** None patches the
disc or the source; they start their own windowed port (`build/port-sdl/dw2003 --window`) on a private debug socket and
point the playing title sequence at the score (stay on the title screen: starting a game reloads the bank). Runtime files
next to them (`game.log`, `session.txt`, `mute.mask`, `*.index`, `piano_options.json`, `render/`) are gitignored
(`tools/music_lab/.gitignore`). `tools/venv/bin/python tools/music_lab/velvet/play.py` installs the room's tune from
`gen_velvet_music` (`--smooth on|off`, `--reverb N`; offline, for numbers only: `--wav render/x.wav --seconds 50`, `--probe`
one 1 s note per part; `analyze.py` and `analyze_probe.py` compare two renders, run with `uv run --with numpy python ...`);
`... play.py adventure` plays the Adventure score instead (socket `/tmp/adventure-music.sock`, without the Velvet retune).
`velvet/solo.py` (`next`, `mute N`, `solo N`, `rem N...`, `add N`, `vol`, `vol N 0-100`, `reset`; parts 1..8 in play.py's
order; a level is written into the score so a loop keeps it); `velvet/piano.py` (`next`, `prev`, `N`, `list`, `test`:
auditions one of 51 deduplicated game samples, 80-250 ms, in the spare slot at centre 66 and solos the piano; `test` puts
it back in the full piece); `adventure/guitar.py` (`next`, `prev`, `N`, `test`, `list`: the same 51 samples on the Adventure
window's program 5, centre 64; `next`/`test` rewind to the score start, `SndSeq.start`, which must still be `FF 51`).
`velvet/offsets.c` prints LIBSND's sequencer layout on the host and `offsets.json` holds the offsets the tools poke.

**Adventure music (a second tune; first pass, not signed off, not tied to any cue).** Same method as the room's, through the
game's sequencer, no WAV, no source change: `play.py adventure`. Score: the owner's local
`~/Downloads/Digimon Adventure OP1 - Butterfly (Full) - Octet - Thymm Rayne.mid` (not in the repo, never committed): format 1,
480 ticks per quarter, one tempo 363636 (~165 bpm) at tick 0, about 335650 ticks (~4.2 minutes), 7812 note events; the built
SEP is 32009 bytes and fits `sound_buffer_2` (40960). Each named track gets its own channel (the file's shared channels do not
matter). Nothing plays at tick 0: the guitar comes in at tick 1920 (~1.4 s), so a check of the first 40 vsyncs hears
silence. Parts (`ADVENTURE_LEVELS` in `play.py`; channel, BGM031 program, first note, level, CC7, pan): Trumpet 0, 8, 74640,
90, 114, 64; ElectricGuitarOverdriven 1, 5 (sample swapped), 1920, 80, 102, 40; Cymbal 2, 10, 1950, 45, 57, 100; Doublebass 3,
4, 24960, 85, 108, 64; BassDrum 4, 6, 1728, 60, 76, 64; Harp 5, 1, 24960, 70, 89, 32; Saxophone 6, 2, 7680, 90, 114, 96;
SnareDrum 7, 3, 1950, 50, 64, 80. The programs are the loaded title bank's, longest usable sample first (8, 5, 10, 4, 6, 1,
2, 3); program 10 is BGM031's noise burst and landed on the cymbal; kick, snare and cymbal are tonal samples played at drum
notes, not a drum kit (replacing them is the same spare-slot copy, not done). The levels are a starting balance so the drums
do not bury the melody; the owner has not tuned them by ear. Guitar: program 5 of BGM031 is the flute-like waveform, so the
owner picked **option 29 of 51** in `guitar.py`: BOSS02's vag 8 (program 3 of that bank, 3872 bytes, ~154 ms) copied into the
spare slot (BGM031 vag 14, the slot the Velvet piano uses, 5824 bytes at `vag_end[13]*8`), program 5's tone pointed at it,
tone volume 96, centre 64, shift 0; channel level stays 80 (CC7 102), pan 40; only program 5's tone record changes (on the
Velvet piece program 8 shares a waveform with program 5's original vag: do not retarget the flute). The bank on that boot was
BGM031 as for the Velvet listen; `ENV019` is too small; if the tune is ever given to the room or another cue, both
`fieldstg_stage.music` and `fieldstg_stage.sound` must change. Same "what not to repeat" rules as above.

**Music pitfall (already made once):** the field plays `fieldstg_stage.sound` (the track) *after* loading the bank of `fieldstg_stage.music`;
changing only `music` changes the bank and the same tune keeps playing. The room uses the pair of map `0x25F`: `music = 0x3B`, `sound = 0x60EC0000`
(only map `0x25F` used it). Hooks: field idle step (`fieldstg_manager_update`, RUN step 0: door and drawing), `fieldstg_goto_map_delayed` ->
`dw3_velvet_leave`, `fieldstg_map_events_check` -> `dw3_velvet_event_closed(type, param)` (it takes `type, param`, not the `FieldstgMapEvent`, because
`include/port.h` is included by `common.h` without game types), `wstag255_setup`, `dw3_travel_node` (the Guardromon node getter),
`fieldstg_map_title_create_windows` -> `dw3_velvet_title_window`. Outside the room map `0x20F` is still the game's bar: entering by the normal doors
changes nothing; the bar's other door (the right one, to inn `0x20A`) is **closed** while the room runs, except in the Time Room before
its gate is passed, when it starts the trials (`def_time_side_door_open`). `dw3_velvet_event_closed` also holds shut the Warp Gate on map 0x288
(`def_time_warp_closed`), event 1080's trigger on map 0x2DF (`def_story_event_closed`) and a trial area's own events. Idle-step order
(`src/fieldstg/fieldstg_80087DB0.c`): `def_story_frame`, then `dw3_velvet_frame` (the prompt and CIRCLE), then `dw3_velvet_draw` (the
room's blue, the Time Room's frame, the call-overs, or the inn's light).

**Oikawa (the trader).** Behind the counter at (275,255), **dir 1** (front-left, the direction the counter runs, the owner's choice), `no_turn = 1`; still
**actor 42** (it gives the speech box and opens the market screen). `fieldstg_actor_draw` calls `dw3_velvet_actor_art`, which draws the hand-made art **in
place of** the actor's sprite (same ordering-table entry: the player passes in front of and behind him; the game's shadow stays). The art is hand-placed
pixels (black shoulder-length hair, plain dark coat, a gold coin in hand; one 3/4 drawing facing down-left, he does not turn) in
`.dumps/tasks/velvet/art/v2/hand.txt` (one character per pixel, 16 palette; the NPC `P068SARM` only appears faded underneath as a pose and size guide, no
disc pixel enters); `hand.py show OUT.png` renders the review (8x, over the guide, beside game NPCs, in the bar); `hand.py header
port/game/definitive_velvet_art.h` generates the header. He breathes like town NPCs (2 frames of 23 vsyncs). The port draws **one quad per colour run**:
no VRAM, no CLUT, exact colours; the 4-bit TIM route was tried and **discarded** because the upload (via `tim.load`) dirtied the scenery's VRAM. Tried and
vetoed: giving him the hero's file (Girls Power repaints it for Kail, so there were two identical heroes). Reach: the game's interaction box
(`fieldstg_actor_find_at`) is 16 x 8 around each figure and the counter's front row left the player's point 12 to 23 px below him (nobody reached him;
at (275,244) he was out of reach), so when the game finds nobody `fieldstg_actor_talk_to` asks `dw3_velvet_talk_find`: the trader 30 px sideways, 16 up and 28
down (the counter row x 256 to 300); the others 24 x 16; the nearest wins. Tested (`.dumps/tasks/velvet/talktest4.py`, **one process per point**: it
does not reload the state between points, the previous balloon stays open and the circle advances it): trader, mentor, Demon Bell Guilmon, Etemon and
the pig answer. His lines (EN + PT-BR, `TALK05` entries 8, 9, 12, 13): the two greetings **alternate on each visit**; the "bought something" (12) and "left without
buying" (13) lines appear on the **next visit** (the market screen has nowhere to show text after itself) and **once per sequence**, then the greeting alternates
again. Flow: talk -> Oikawa's line in the game dialogue -> the Black Market screen opens -> leave -> the next visit comments how the previous ended. State is RAM
(`vl_shop_*`). The same hand-drawn art is drawn over the Time Room's first challenger and the hooded man of the Bug Maze Pit
(`vl_dealer`, `def_story_oikawa_figure`). Since the story layer he is behind the counter only from the room's level 5 (the Oikawa
beaten), and his first line there is `TALK05` 99 (`VL_SHOP_OIKAWA`).

**The other figures.** (Positions below are the 2026-11-17 ones; the 2026-11-18 arrangement is in the Actors list above.) Mentor = actor `0x39` (`P074JIJI`, the body of the "Grandpa ..." and the Tamer Genji: bald, white beard, 24x32, 5 directions; fits VRAM
slot 5) on the stage between the organ pipes at (318,165) dir 1 (2026-11-17; before that the Kotemon `0x178`, and `376`'s Kotemon `P189PKEL.BIN`/`0x19`, 32x32,
1 animation, in a previous try). The stage is a dead end joined to the upper floor by a strip (y 160..176): it blocks no way, the entry camera already shows
its red lights, and he can be talked to from any side. Alternatives seen: Piximon (the Adventure trainer, 48x40: does not fit the slot), Wizardmon (32x40), Master
John. The slot `vram_places[5]`: the neighbour uses u=32, so the frame must be at most 32 px wide. Etemon: actor `0x61` (97, vram 4) **back to the room, facing the
piano** at (352,284) **dir 5** (the keyboard is up and to his right, so 5 points to the keys); he turns when the player talks. The pig: actor 318 (`0x13E`, vram 6) at
(245,292), the only art in the game that no map uses; the line is the owner's joke ("somebody found me in a pile of toys nobody used"), `TALK05` 79. Guilmon (actor
`0x172`, vram place 3) lives on the upper floor; his talk toggles the key item **26** (a Japanese-tag slot the US disc never gives, renamed "Demon Bell" /
"Sineta Demoniaca" in `en.txt`/`pt.txt`); lines `TALK05` 77 (handed over) and 78 (taken back). `TALK05` 81 and 82 (the mentor) and the others are entries of the bar's
talk file (`records_language + 0xE8`) that on the disc hold only the letter "a" and nobody uses; the port overrides them in EN and PT-BR (`tools/gen_text_override.py`),
and `dw3_velvet_text_own()` (list in `definitive_velvet.c`) marks the entries that are **port text**, so `def_text_override` serves the English text in **any** language
(proved with `records_language = 3`, French: the balloon shows the English line instead of "a"). Translating those lines to FR/IT/DE/ES is the general
translation task; PT-BR has its own. The mentor's content was a placeholder until the Blast task wrote the real one (now the Gennai quest).

**Doors (the 13 positions, done properly). History: superseded on 2026-11-18** by the lights on the scenery (no door sprite; the spots moved, see
the lead of this section); kept for the method. The first attempt was an algorithm plus a contact sheet (the owner caught a door in front of a Digimon and in the middle
of the scenery); the second was by eye over 13 small screens and misjudged what was behind several. Now each was checked in its own map at 2x with the actors where the game
puts them (the search excludes 56 px around every actor). The door is a **door-shaped portal**: a dark frame (two jambs, a lintel and a sill) with light in 6
bands (dim at the lintel, bright at the floor), so it stands anywhere and does not look like a plaque; depth **3** (the layer draws back to front): in front of floor and
walls and **behind the player** (at depth 2 the player vanished behind the door). Positions: 0x20A (339,192) right wall by the stairs; 0x223 (205,269) end of the deck's
logs; 0x230 (516,380) stair landing, stained glass; 0x238 (300,440) shop floor (no wall; nothing in front); 0x23F (580,392) white counter wall; 0x247 (540,304) foot of the
metal ramp (no wall); 0x249 (403,187) stone step of the trail; 0x25B (355,313) platform edge (no wall); 0x25D (1074,604) brick low wall; 0x263 (383,235) log wall by the
armchair; 0x269 (553,262) face of the machine block; 0x26D (396,520) tower wall; 0x26F (1156,833) metal wall far from the Digimon. Second half: the same coordinates in 0x279,
0x292, 0x29E, 0x2A5, 0x2AC, 0x2B1, 0x2B3, 0x2C4, 0x2C6, 0x2CB, 0x2D1, 0x2D6 (`0x26D` has no copy): 25 maps in all. Compromise: 0x223, 0x238, 0x247 and 0x25B have **no wall** where a
door fits without covering an actor or prop, so there it stands on open floor held up by its own frame. The `doors` table is not `const` on purpose (`doors_try.py MAP:x:y ...`
moves doors in RAM and takes a screenshot of each without rebuilding). Proofs: `.dumps/tasks/velvet/doors/*.png` (the 25 screens), `doors_final_a.png`, `doors_final_b.png`.

**Unused-actor study (2026-11-16)**, `.dumps/tasks/velvet/unused/REPORT.md` (63 preview sheets, `_shortlist.png`, `unused_ids.txt`; scratch tools `tim.py`, `render.py`, `contact.py`,
`sprites_fixed.py` decode 4-bit TIM from the disc: the old `sprites.py` read the TIM wrongly, CLUT in bytes and image `w`/`h` in words, frame `w*4` x `h`, and is broken: do not use). 406 actor ids
in `fieldstg_actor_sprite_files`; 342 are placed by some of the 293 stages; of the remaining 64, 9 the engine uses without placing (id 2 = spare player, 3..10 = the 8 partners), leaving
**54 never-used ids**. "Unused id" is not unseen art: 54 of those 64 draw the same sub-file as an id the game places; the only art nobody sees is `P182BOMP` (id 318), **a pig**. Identified by
twin sprite and the `<7>Name<7>` tag of lines: 29 Divermon; 188-191/387/388 Etemon; 207 MetalEtemon; 233/234 Patamon; 236 Renamon; 237/238 Kotemon; 240 Monmon; 248-250 Knightmon;
286 WaruMonzaemon; 312/313 soldier; 366 Agumon; 368 Veemon; 370 Guilmon; 372 Patamon; 374 Renamon; 376 Kotemon; 378/380 Kumamon; 17/18/19 generic bodies; 177/251 pigs; 275-279 "Guard
A/B/C"; 353-360 are the BLOK objects (0x148/0x15F/0x160), not NPCs.

**Savestate fixes.** The state of the Black Market screen (`bm_ui`: open, cursor, windows in the field heap) and the room's (`vl_room`, `vl_from`, `vl_shop_*`) were not in
the savestate (loading a state from before the shop opened left the screen "open" over windows the heap no longer had: the player froze, the third consecutive load
segfaulted); now `def_bm_savestate` and `dw3_velvet_savestate` are in `def_savestate`, tested with 7 consecutive loads alternating trader (shop opening), Etemon and pig
(`talktest3.py`). On 2026-11-17 the same was done for `def_travel_savestate` (the Guardromon screen's `nt`), `def_evohint_savestate` (`eh`), `def_callout_savestate` (`callout`) and
`def_dri_savestate` (which now also carries `dri`, `dri_wait` and the list's text buffers), all registered in `def_savestate` (`definitive.c`); proof `ss/travel_ss.py BIN OUT` (opens
the Guardromon screen at Asuka's inn, saves before and with it open, loads alternating 4 times and checks open/closed, cursor, triangle and floor): the old binary **crashes on the first
load**, the new one passes the 25 checks.

**Verification done.** `trigger.py` (the 25 doors, at the final positions, lead to map `0x20F`, 0 failures, reads the table directly from `definitive_velvet.c` via `doors_of.py`);
`e2e.py` (inn `0x20A` -> door -> room -> talk to Etemon (dialogue with the port's line) -> front door -> back at `0x20A` on the Guardromon); `gen_text_override.py --check`; `tests/port/run.py`
(the room is off under `--script`); `tests/port/field.py --only lobby_lab_exit items_list` ok (the `swamp_dive`/`swamp_flies` cases **already failed before** on a `write_mem` step that this
checkout's `psxstack/runtime/script.c` does not implement, "unknown step type", unrelated); the SDL3 build (`build/port-sdl`) compiles and the room looks the same. Scratch tools in
`.dumps/tasks/velvet/`: `cycle.sh COMMAND...` (rebuild, regenerate `field.state`, run), `shot.py MAP X Y DIR out [--poke sym=value] [--player X:Y:DIR]`, `doors_try.py`, `walk.py`, `montage.py`,
`trigger.py`, `e2e.py`, `attr.py` (collision map as text; `--around X Y`), `vrun.py` (runs the scratch tools against another binary, `DW3_BIN=...`); a private build `build/port-velvet/dw2003`
was used so as not to fight other sessions over `build/port` (a worktree via `wtsync.sh` while the Black Market was under construction).
The 2026-11-18 prompt (scratch `.dumps/tasks/story/time/prompt.py`): no icon off the spot; on the spot the icon and **no entry in 180 frames without
CIRCLE**; stepping off closes it, coming back reopens it; CIRCLE enters (map 0x20F); passed on 0x23F, 0x25D (x > 960) and 0x288 (opens the Time Room).
`def_story_savestate` and `def_time_savestate` are registered in `def_savestate` too (the story's talk, dream, choice panel and Oikawa state; the Time
Room's run).

**Code map.** Stage `0x20F`: `src/wstag/wstag255.c` (setup, `actors[]`, `sprites[]`, `map_events[]`); stage code per map `fieldstg_stages[240]` (`{id, file, entry}`); talk `fieldstg_actor_talk_to` (`def_bm_talk` /
`def_dri_talk` before the redirect); traders `traders[]` / `def_bm_talk` in `definitive_market_screen.c`; door/exit between maps `FieldstgMapEvent` (flags, type 1 = move, x, y, dir, route, room,
`include/fieldstg.h`); map spots (layer 7) `fieldstg_map_events_check`: `attr & 0x1F` = index, `attr >> 5` = direction; music `fieldstg_stage.music` (wstag255 uses 8; the room 0x3B); background
`fieldstg_stage.background_file`.

**The original text of the next tasks (owner, 2026-11-16), "section 6: the world, organic onboarding and the Velvet Room"**: hidden room reached through mysterious doors beside the wardrobes and
cupboards scattered through the game's maps (inspired by the Persona Velvet Room and Gennai); inhabitants: the Black Market merchant (rare materials and replicas) and a Gennai-inspired mentor
who explains the risk of the Cursed Evolution, runs the Purification Gauntlet and grants the Chosen Evolution. **Organic onboarding without boring tutorials (out of scope for now, the owner is changing
the main quest in another session and will create guide quests later):** small mandatory hooks in the main progression: make the player interact with a Guardromon right after unlocking the second
teleport point (teaches the fast-travel network in practice), and steer the player through one of the wardrobe doors for the first time when rare Hunt Materials start to appear. This is the open
"onboarding hooks" item.

**Pending / questions to the owner (2026-11-17, updated 2026-11-18):** test in game (enter an inn, find the light, take it with CIRCLE, talk to every figure, leave by the front); look at each
inn's light (the six "divided" ones first) or say "in that inn move it / make it stronger or weaker"; approve or replace the mentor's figure (the old man `0x39` until one is drawn); the room's music
is done (the owner's arrangement, above); the door sprites and the 13 door positions were dropped with the doors; FR/IT/DE/ES lines (the port override
covers English and PT-BR only, `dw3_velvet_text_own()` serves English elsewhere); **saving on the memory card inside the room**: the save falls on the generic `0xC00` and loading opens `0x20F` as the
normal bar (`vl_room` is RAM, not on the card; the port's savestate carries it since 2026-11-16): acceptable? if not, it is the next decision. The owner's rules (summary): pt-BR with the owner,
no commit without being asked, no AI attribution in repo files, scratch in `.dumps/`, plans in `.plan/tasks/`, move files to `/tmp` instead of `rm`, "no 'I don't know, wait': measure and go; ask only
what is the owner's decision".

## The story layer (Velvet Room in the main story)

**Status: implemented and committed (`c5e58e4` dreams, choices and echoes; `9dba2cf` the Time Room gate, the Oikawa's arc and
Act III; `91f12d2` the Oinkmon dream); only partly seen in a real game (see Pending).** `port/game/definitive_story.c`
(the hooks, Igor, the dreams, the rewrite's lookup), `definitive_time_room.c` and its generated `definitive_time_room_table.c`
(`tools/gen_time_room.py`), `definitive_velvet_igor.c/.h` (Igor's head), generated `definitive_story_text.c`
(`tools/gen_text_override.py` from `tools/text_override/en_story.txt`, `en_kaili.txt`, `pt_kaili.txt`, `en_choices.txt`,
`en_echoes.txt`). Everything is keyed on the story step `gamestate_data.progress` and is off in a replay script (`def_on`).
A story talk is the game's own talk balloon on the player (or on a given actor) with no actor talked to: the hook asks for
an entry of the stage's talk file whose disc text is the unused letter "a", and the port's override fills it; the field is
held (`event_running`) until the last balloon closes and the confirm button is let go. Plans of origin:
`velvet-historia.md` (the arcs) and `velvet-sonhos.md` (dreams, rewrite).

**The room grows with the story** (`def_story_velvet_level`; a save from before the port's story is placed by its step alone):

| Level | When | Who comes in (`vl_guests[]`) |
|---|---|---|
| 0 | until Repeating Tom's news at step 4 | nobody: every inn's way in is closed |
| 1 | Tom's news (`DEF_STORY_SEIRYU_CALL`), or any step past 4 | the Sepikmon |
| 2 | step 10, "The Suzaku Badge" | Gennai (the old man, the Blast quest) and Igor |
| 3 | step 16, "The real Leader" | Guilmon, the Demon Bell's keeper |
| 4 | step 22, "Free Asuka City" | Etemon and the pig |
| 5 (`DEF_VELVET_LEVEL_MAX`) | the Oikawa beaten (`DEF_STORY_OIKAWA_BEATEN`), or a save past step 27 | Oikawa behind the counter: the Black Market |

Steps 10/16/22 were picked to spread the guests over the campaign (`steps[]` in `def_story_velvet_level`); owner order of
2026-11-17: Gennai comes with Igor at step 10 ("the start drags"), Guilmon at 16. The first visit after the level rose,
the guest of the highest level that greets calls the player over (`vl_newcomer`; codes `DW3_VELVET_PULL_SEPIK`, `_MENTOR`,
`_IGOR`, `_ETEMON`, `_MERCHANT`, plus `_TRIAL` for the Time Room in `include/port.h`); the Bell's keeper and the pig never call,
so at level 3 it is still Gennai. The level last greeted is saved in bits 12..14 of the story flags (`def_record_velvet_seen`).
The room is entered by portal (`dw3_velvet_pull_by`) for the scenes; `dw3_velvet_cut(who)` re-enters the map in place "as its
level has it" (a screen cut). `dw3_velvet_pull` (the old Seiryu pull, the Sepikmon's) is still defined but has no caller.

**Step 0, "Digimon Online": the opening cut in two.** At New Game the scene (WSTAG785, map 0x2D8, event 1; text of event text
file 0x14A, table 1) is cut where Junior cannot pick a starter pack, after event text 12 (Ivy's "choose, and let's go"):
`opening_cut` rebuilds the overlay's script at runtime into two buffers (`st_first`, `st_second`, 1400 words each; cut again
after every load). The first half ends with the port's event command **`0x7F00 1`** (`ST_CMD_DREAM`, handled in
`fieldstg_event.c` case 0x7F): Junior is pulled into the room alone with Igor, who gives him the key (`TALK05` 18,
`DEF_STORY_IGOR`); the room sends him back to the spot he stood on (`st_junior`) and the stage replays the scene from the
cut: actors put back (PLACE, idle facing) and the camera snapped, then Ivy and Teddy ask why he froze (event texts 1..4
replaced by `$OpenIvy1`, `$OpenTeddy`, `$OpenJunior`, `$OpenIvy2`; the Kaili mode swaps the names), then the game's own scene
from text 13 ("You still haven't made up your mind?"). If the script is not the expected one the cut is skipped (logged).

**Step 4, "The Seiryu Badge": Repeating Tom** (replaces the first tutorial's direct pull at Seiryu City). The Guide's row asks
the player to ask around Seiryu City for the Leader (`DEF_STORY_COND_SEIRYU_ASK`, map 0x230). Talked to on the stairs of the
city's inn, Repeating Tom's own line (`TALK01` 0xCC entry 79, "Seiryu Leader is training at the Protocol Ruins") is said as
`$TomNews` (the Leader is gone), then the player's feeling (`TALK01` 178) with the portal sound, and `DEF_STORY_SEIRYU_CALL`
raises the room to level 1: every inn's way in opens, Seiryu's inn's at (210,300). Through any of them Igor alone shows the
room again (`TALK05` 20, `DEF_STORY_SEIRYU_ROOM`), the screen cuts (`dw3_velvet_cut`) and the Sepikmon calls the player
over (`TALK05` 17, his first words while the Leader is missing) with the Ogremon tutorial hunt (Super Rare hunts). Until the
hunt is reported Tom says `$TomGone` and the Guide's main quest is the missing Leader (`DEF_STORY_COND_SEIRYU`); reporting
the won hunt (`def_super_done() > 0`) gives `TALK05` 16 (his trackers saw the Leader at the Protocol Ruins), sets
`DEF_STORY_SEIRYU_DONE`, and the story goes on as the game's. `DEF_SUPER_FLAG_MET` still marks the Sepikmon's first meeting.

**Igor** (`VL_IGOR` 109, the game's man in a black suit `P055GMGD`, file 0x356, 24 px frames; actor 53 `P068SARM` is the
game's other suit). **His head** is hand-drawn for this port (`definitive_velvet_igor.c/.h`, 92 runs of one palette colour,
one quad each, from `.dumps/tasks/velvet/art/v2/igor.txt` via `igor.py header`): over frames 9, 10 and 11 of his down-left idle
(11 is 9 a row lower; 10 keeps 9's place), mirrored when he faces right; any other frame keeps the game's head; body, idle
and shadow stay the game's. `dw3_velvet_actor_art` calls `dw3_velvet_igor_head` before FIELDSTG adds the figure, so the quads
draw over it in the same ordering-table entry; he keeps his facing (`no_turn`). In his scenes (the dreams, Seiryu's showing:
`vl_scene`, `vl_dream_actors`) he is alone, facing down-left (dir 1), and the player comes in two steps in front of him at
(246,340) dir 5. **His line** is the story's (`def_story_igor_line`, closed by `def_story_igor_done`): the key at the opening
(`TALK05` 18), the showing at Seiryu (20), otherwise **his reading of the party's DNA forms** (`TALK05` 19, built into
`st_igor_text`, 1024 bytes, by `igor_read`): a greeting (`$IgorMeet` to a guest who found the room without the opening's
dream, i.e. a save from before the story layer, else `$IgorBack`), then for each party member with one, a page "<partner>
could become <form>, born of two forms:" with the two forms, the level each needs and how many are done (`$IgorBoth`,
`$IgorOne`, `$IgorNone`), and a farewell (`$IgorBye`, or `$IgorBlank` when nobody has a DNA form). `dna_pick` takes, from the
partner's learn list (`stfgtrep_learn_lists`, as the Digi-Lab's Next Forms panel reads it), a form not owned yet that needs
two other forms, with the most of its two bonds done, then the highest levels. Nothing is built until the Digimon names are
loaded (the room queues file `records_language + 0x4E` at setup); the disc's line shows then.

**Igor's dreams** (`dreams[]`): at a story moment Igor pulls the player into the room alone (`dw3_velvet_pull_by(map,
DW3_VELVET_PULL_IGOR)`: the portal sound, the room with only him), speaks, asks with the **answers' panel** (up to three rows
at the screen's foot over the room's blue; up/down move with sound 0x8004513E, CIRCLE takes with 0x8004503C; it arms only once
the button that closed the last balloon is let go), answers the choice, and sends the player back to the exact spot
(`dream_leave`). The answer is saved and changes no event; later dreams recall earlier answers. A row: answer slot, first and
last step, a game flag that must be set (or 0), Igor's ask, 3 options, 3 replies, the slot of an earlier answer to recall
and its 3 recall pages. A new dream is a row plus its `$Dr*` lines in `tools/text_override/en.txt`. Rows, in table order:

| `DW3_DREAM` | Slot | Step | Trigger | Theme (owner's text) |
|---|---|---|---|---|
| 1 | 0 | 12..18 | game flag 0x1C0E (set by WSTAG305's event 351, the Game Master blames Lucky Mouse, DMG305:1) | Act I, the Game Master's lie: the eyes see the shadow on the wall, not every mask is the culprit's; every answer goes on hunting Lucky Mouse (at 19 his side is heard) |
| 2 | 1 | 27 | the first free frame once the Oikawa no longer bars the Bug Maze (maps 0x244..0x246, `oikawa_ahead`) | Act II, the burden: past this threshold there is no easy way back; do you carry it by your own will? |
| 3 | 3 | 0x26 | WSTAG226's event 935 (DMG200:41 "Wait here. I'll look into it.") ends at 0x26 leaving the player free on map 0x270; the step lasts until the Master Room's events (0x27), Qing Long's restoration machine (flag 0x1A0A) inside it | the Oinkmon: the Destromon stopped, yet Teddy and Kurt are still Oinkmon; recalls the Act I answer and asks what carries the hero, alone now (`$Dr4*`) |
| 4 | 2 | 0x2B | the end of WSTAG820's event 1080 on map 0x2DF (Snatchmon fuses with the Gunslinger, DMG800:17) | Act III, the abyss: accept defeat, or forge a miracle at the edge of the abyss (the canonical defeat becomes the Chosen's awakening); recalls the Act II answer |

Strings: `$DrIgor` (the plate) and 34 `$Dr1*`..`$Dr4*` keys (`Dr1` Act I, `Dr2` Act II, `Dr3` Act III, `Dr4` the Oinkmon; `Ask`,
`Op1..3`, `Re1..3`, `Rc1..3` for the recalls). `DW3_DREAM=<n>` (1..4, the table's order above) dreams row n at the next free
field frame whatever the step and answers (a test shortcut). **Act III and Galacticmon:** in the game event 1080 ends
(`wstag820_event_1080_end`) by setting flag 0x7401, whose type starts the stage's listed battle 1 (Galacticmon) at once, so no
free frame was left and the dream never came. `def_story_hold_galacticmon` (called there under `PC_PORT`) lets the event end
without it; `galactic_hook` sets 0x7401 itself on the first free frame back on map 0x2DF once answer 2 is given; the event's
own trigger (map event type 8, param 0x438) stays shut afterwards (`def_story_event_closed`), since nothing in the game marks
it done (its flag 0x40A5 is set by no one).

**The hero's choices and the echoes.** The scenes' choices (`en_choices.txt`, `Q<n> FILE:sub index@<k>: label | full balloon
text`): before entry `index` the event's dialog holds (`def_story_event_dialog`, asked by `fieldstg_event.c` before each event
dialog) while the same panel shows up to 3 labels (at most 26 letters); the balloon then says the line picked
(`def_story_line`) and the scene goes on as the game's. An echo (`en_echoes.txt`, `Q<n>=<k>[,<k>...] [kaili] FILE[:sub] index:
text`) is said in place of an entry while answer n is one of the options listed (`kaili`: only in Girls Power's mode, a plain
echo in both); an entry's echoes are tried in file order, the first that holds wins; an echo keeps the entry's speaker, and a
choice entry cannot have one. `def_story_line(file, index)` answers in this order: the hero's choice, an echo, the Kaili
line, the rewritten line, then the disc's. Counts today: 111 scene questions (numbered between Q8 and Q150, never
renumbered) with 336 options; 600 echoes (30 of them Kaili-only).

**Story answers in the save** (`def_record_story_answer` / `_set_`, `definitive_bestiary.c`): `DEF_STORY_ANSWERS` = 160 answers
at 2 bits each (`DEF_STORY_ANSWER_BYTES` = 40 bytes at `REC2_ANSWERS`, right after the Blast data in card record 2; answer q
is byte q >> 2, bits (q & 3) x 2; 0 not answered, 1..3 the option); slots 0..7 are Igor's dreams, 8..159 the scenes' choices
(Q8..Q39 existed first, at one byte each, until the bytes ran out). A compile-time check keeps them clear of the skin byte;
an older record reads 0. See [Save format](#save-format-memory-card-records).

**The story rewrite (English only).** `en_story.txt` (2724 entries, `FILE:sub index: text` for an event scene's table,
`FILE index: text` for a talk file): childish or simple dialogue made more serious without changing order or quests, and
gender-neutral wherever the hero is concerned. `en_kaili.txt` (111 entries): Girls Power's mode, the lines the automatic
name swap gets wrong; a text starting `>hero ` is said by the hero (the balloon moves to the player). `pt_kaili.txt` (3
entries): the brother's lines in Portuguese. Portuguese shows the disc's/`pt.txt` text with only the Kaili names (the rewrite
and its choices are English); FR/IT/DE/ES show the disc's. `DW3_STORY=0` turns the layer off (the game's own text, to
compare). Logs: `story: echo ...`, `story: kaili line ...`, `story: ... choice N answered M`. Checker
`tools/check_story_text.py [FILE]` (every source by default; exit 1 on an error): the same speaker plate and control codes as
the disc's line, 22 letters a line in balloons (38 in unplated narration), 3 lines a page, the page structure (each page ends
with `<2,2>`, every later page starts with `<3>`), and no gendered word about the hero. Girls Power's swap
(`tools/gen_text_override.py`, `NAMES`): every line without its own entry names Junior as Ivy, and Ivy or Kail as Junior
(Smith Junior keeps his name), rewrapped within its page; written by hand in `en_kaili.txt`: the brother's lines, the "he/him"
ones (DMG700:0 2, DMG200:32 3, DMG600:15 18/20/23) and the account-name joke (DMG200:3/4/5 7-8); `$OpenIvy1K`/`$OpenJuniorK`
were removed (the swap makes the `$Open*` lines). Portuguese swaps only Kail for Junior: its Real World lines need their
articles and endings turned too ("a Ivy" -> "o Junior", "sozinho" -> "sozinha", the captured Kail as "ele"), which waits for
the rewrite's translation. How the rewrite was made (scratch in `.dumps/tasks/story/rewrite/`): a Sonnet "bible" per chapter
(`bible_events.md`, `bible_talk.md`), a rewrite per chapter with verification, then an adversarial review (`review.md`)
applied straight into `en_*.txt` by `apply_review.py` (its `merge.py` rewrites `en_*.txt` from `out/`: never rerun it alone).

**The Oikawa's arc (step 26 -> 27, `oikawa_hook`).** After Bulbmon, event 711 sets step 27 and leaves the player in the Bug
Maze Pit's hall (map 0x245, WSTAG540) at (1800,524). While he is unbeaten the hooded dealer is added to that map's figures
(`def_story_stage_setup`): actor 42 with the Black Market trader's hand-drawn art over it, in VRAM place 6 (the map's third
figure, actor 78, is left out while he waits), at (1778,502) dir 7, two steps up-left. He calls the player over once per
visit (the player turns to him) and, after his line (`TALK06` 0xEF entry 5, or 6 after a lost try), fights:
**Arukenimon Lv28, Mummymon Lv30, VenomMyotismon Lv32** (`def_oikawa_fight`, boss battle template 7, Bulbmon's scenery and
music: stage 10, `0x60880000`; no escape). The plan's Dinohyumon and MaloMyotismon are not on the disc: Mummymon and
VenomMyotismon stand in. A defeat is no game over (`def_story_defeat` from WFIGHTMN: the party back as it went in, he
calls again); beaten, he says `TALK06` 7 (his name is Oikawa) and is gone, the room reaches level 5, and the Guide sends the
player to the Velvet Room (`DEF_STORY_COND_MARKET`) until he first opens the market behind the counter (`TALK05` 99,
`def_story_market_opened`, `DEF_STORY_MARKET_OPEN`). The Velvet Room's Black Market is closed before that (he is not in the
room); the houses' traders of maps 0x20C/0x27B are unchanged.

**The Time Room (step 40, the gatekeeper before the point of no return).** In the Master Room of Asuka (map 0x288, WSTAG311,
talk file `TALK03` 0xDA) Megadeath's voice opens the Warp Gate to the Gunslinger (a map event of type 14 to map 0x2DD), and
nothing there leads back. While the step is 0x28..0x2A and `DEF_STORY_GATE_PASSED` is unset, the port holds the gate shut
(`def_time_warp_closed`); stepping onto it, Igor's voice stops the player, once per step onto it (`TALK03` 311 while the
Blast quest is not done: finish Gennai's road or prove your own strength; 312 with it; `DEF_STORY_GATE_WARNED`), and the
Velvet Room's way in stands in the nook left of the gate's platform at (160,440); the room's front door brings the player
back beside it at (184,448). Through it the room is the Time Room (`def_time_on`, title "TIME / ROOM"): Igor and the three
challengers (Gennai or, while the quest is still DECEIVED, Myotismon in his shape; the Sepikmon; Oikawa), time stands still.
**No prize** (owner): it is mandatory and measures the player's level instead of throwing them into a no-return zone. Igor
opens by recalling the Act II answer (`TALK05` 47..49 by that answer, 50 if never asked; 51 talked to again, 52 after a
lost or abandoned run, 53 all won); the guests 54 (Gennai), 55 (Myotismon), 56 (Sepikmon), 57 (Oikawa). The upper door
(open only here) starts a run of three trial areas, each with the room's music, no random battles, the area's own events
shut and its exits leading back to the room, and the challenger at his spot in VRAM place 4, the player arriving two steps in
front of him:

| Round | Area | Challenger (actor, spot) | Team (`def_time_trials`, battle template) |
|---|---|---|---|
| 1 | Asuka Bridge, mid-span (0x202, WSTAG205) | Oikawa (42, 440,392) | Mummymon Lv56, Arukenimon Lv58, VenomMyotismon Lv60 (301) |
| 2 | Genbu City's west square (0x26F, WSTAG760) | the Sepikmon (61, 300,480) | Gryphonmon Lv60, SkullMammothmon Lv62, Kimeramon Lv64 (303) |
| 3 | the middle of the Mobius Desert (0x258, WSTAG635) | the old man (57 = 0x39, 570,470) | Gennai: Valkyrimon Lv63, Knightmon Lv65, Zanbamon Lv67; Myotismon (quest DECEIVED): Puppetmon Lv63, Machinedramon Lv65, Piedmon Lv67 (303) |

A trial is one battle of the three brought in one after the other (a boss battle's own entrance, as the DRI Gauntlet's),
Megadeath's scenery and music (stage 17, `0x608C0000`), no escape; his line before it is `TALK05` 58..61 and after a win
62..65 (by row). A won trial skips the results screen (`def_time_won`: no XP, no Bits) and the next area follows; a lost one
is no game over (`def_time_defeat`, the party as it went in) and, like walking out of an area, sends the player back to the
room with the run reset (`story_gate` 0). The third win sets `DEF_STORY_GATE_PASSED` and the gate opens. The run is RAM (a
save state carries it); the save keeps the gate bits and `story_gate` (0..3 trials won in the current run). `tools/gen_time_room.py
[--apply]` builds the table: each species' stats at its level from its own rows in the game's battles
(`gen_dri_challenges.stage_of`) and, as the template, an existing boss battle near the trial's top level (not one of the
special ones, none with the final bosses); the levels climb towards Galacticmon's. Spots were measured on the attribute map:
`DW3_TIME_TEST="map:x:y:step"` (hex or decimal) warps to the map at the first idle frame, then sets the step and reloads it
(0x2B also holds Galacticmon, as event 1080's end); it logs the player's position every 30 frames and the landed map's size,
and with `DW3_TIME_SURVEY` also each map event's cells (attribute layer 7) and a free-ground grid. **A set battle** (a trial,
the Oikawa's) goes through `def_time_set_battle`, which sets the scenery and music itself: `fieldstg_start_battle` leaves
them as the last battle had them, garbage right after a load, which crashed FIGHTSTG (`stage < 0` keeps them for a caller
that set them).

**The Guide's story conditions** (`definitive_quest.c`): a quest row whose flag is `STORY(cond)` (`DEF_STORY_FLAG` 0xF000 |
cond) asks `def_story_cond` instead of a game flag: `DEF_STORY_COND_SEIRYU_ASK` (step 4, map 0x230, "The Seiryu Leader": ask
Repeating Tom), `_SEIRYU` (step 4, map 0x230, "The missing Leader"), `_OIKAWA` (step 27, map 0x245, "The hooded man" bars the
way), `_MARKET` (steps 27..45, map 0x20F, mission "The Black Market"). `tests/port/definitive_quest.py` accepts the
`STORY(...)` rows (its driver stubs `def_story_cond` to 0).

**Saved state** (card record 2, after the Blast bytes): `story_flags` (u16: bit 0 `DEF_STORY_IGOR`, 1 `_SEIRYU_CALL`, 2
`_SEIRYU_DONE`, 3 `_OIKAWA_BEATEN`, 4 `_MARKET_OPEN`, 5 `_GATE_WARNED`, 6 `_GATE_PASSED`, 7 `_SEIRYU_ROOM`; bits 12..14 the
room's level last greeted), `story_gate`, then the 40 answer bytes. The port-owned talk entries (served in every language,
English until translated: `def_story_text_own`, `def_time_text_own`, `dw3_velvet_text_own`): `TALK01` 178; `TALK05` 16..20,
47..65, 99; `TALK06` 5..7; `TALK03` 311/312; Tom's `TALK01` 79 and the opening's event texts 1..4 are replaced only while
their hook holds (`def_story_text`).

**Hook points in the game's code** (all under `PC_PORT`): `src/fieldstg/fieldstg_event.c` (`def_story_event_dialog` before an
event dialog, `def_story_event_command` for command 0x7F, `def_story_event_script` when an event starts);
`src/fieldstg/fieldstg_80087DB0.c` (stage setup: `dw3_velvet_field_loaded`, `def_time_stage_setup`, `def_story_stage_setup`;
idle step: `def_story_frame`, `dw3_velvet_frame`, `dw3_velvet_draw`); `src/fieldstg/fieldstg_80085590.c`
(`dw3_velvet_title_window`: VELVET / SERVER or TIME / ROOM); `src/wstag/wstag820.c` (`def_story_hold_galacticmon`);
`src/wfightmn/wfightmn_800A6440.c` (`def_time_won` skips the results; `def_time_defeat`, `def_story_defeat`: no game over);
`port/game/definitive.c` (`def_time_battle_begin/_end`, `def_story_battle_begin/_end` with the other battle hooks;
`def_story_savestate`, `def_time_savestate` in `def_savestate`); declarations in `include/port.h` and `definitive.h`.

## Super Rare hunts

**Status: done, tested in game (tutorial end to end, 2026-11-17) and committed** (with the Blast work in `8c0971b`, since
`definitive.c/.h`, `definitive_bestiary.c`, `include/port.h` and the texts are shared and `definitive_recolor.c` calls
`def_blast_dark` and `def_purify_fighting`; the skins page in `eb2fa36`). Old saves did not matter while it was built
("nobody is playing yet, it is all debug").

### The owner's decisions

- Regional Rare Targets: one Digimon per battle, no trios, any thematic species (preferably one absent from the region),
  level = region average + 5, HP x4 capped at 30000, **never Galacticmon**. With a hunt active and the player in the
  target's territory, the regional Rare "gets in the way" (Monster Hunter style): the chain gives a chance from 2 wins
  instead of 6 (the Rush counts) and the party's flee against it drops to 50%.
- 12 Super Rares, one Digimon each, no repeats, **unlocked in sequence**: all listed with the recommended level, the next
  opens when the previous is finished, finished ones can be **repeated**, the active one can be **cancelled** (the target
  heals). Levels 29 to 61 ("the levels are great"), HP 15000 to 30000.
- A fixed territory per Digimon: a sector (North/East/Central/West/South, a hand-made table) of one server.
- The target sits on a **random map** of the territory at a reachable **point**; a **temperature compass** guides between
  maps (graph distance) and inside a map (distance to the point). Stepping on the point starts the battle.
- Battle: the target alone; losing **25% of its bar** in a fight makes it flee to another random map, **keeping the HP
  lost**; each re-meeting gives **+20% to all stats**, stacking. Fleeing or losing: the hunt continues. The Rush never
  skips a Super Rare. Leaving the territory leaves the target where it was.
- Reward: **1 crystal per hunt**, which sells high; no big Bits or XP bonus ("I am afraid of breaking something").
- Colour: a real palette swap on Super Rares **and** regional Rares.
- **Tutorial** (asked later): a level-3 hunt the Sepikmon gives in the Velvet Room. The game intercepts the first arrival
  at **Seiryu City** (the first city after Asuka) and pulls the player into the Velvet Room for the first time. Target:
  **Ogremon in Central Park**, near Asuka. (Since the story layer, 2026-11-17, the first visit comes through Repeating
  Tom's news at step 4 instead: see [The story layer](#the-story-layer-velvet-room-in-the-main-story).)
- **Compass around the territory** (asked later): it also shows on maps up to 3 exits outside the territory, to lead the
  player there (in the tutorial the territory is only Central Park). One rule for all hunts.

### Data

- `def_super_rares[13]` in `definitive_hunt_targets.c` (battle, slot, level, HP, sector, server, area, species, price).
  Row 0 is the tutorial `{204, 1, 3, 480, 2, 0, 0x1D, Ogremon, 1500}`; then the 12 in level order. The `area` field >= 0
  restricts a hunt to one area (the tutorial); -1 uses the whole sector. Crystal price 8000 to 16000 (the owner asked for "about 10000 Bits"); crystals are sold at
  the Sepikmon's board (Square), not at the Black Market. Super Rares by order: MasterTyrannomon (29), Pharaohmon (37), Cherrymon (40), Knightmon (46),
  Whamon (46), WaruMonzaemon (47), BK Seraphimon (47), Kimeramon (52), Gryphonmon (56), GrapLeomon (57), HiAndromon (59),
  Kyukimon (61).
- Sectors (`SECTORS` in `tools/gen_hunt_targets.py`): North, East, Center, West, South, each a list of world map areas,
  per server (Asuka, Amaterasu).
- **Map graph:** `tools/gen_map_graph.py --apply` -> `port/game/definitive_map_graph.c` (map event types 1 and 14 are
  exits): 239 maps, 415 exits, 123 with encounters (`def_map_wild`); `def_map_distance` is a BFS.

### Logic (`definitive_superhunt.c`, pure)

- Order open/locked/done/active, repeat, cancel (heals), the draw of a map in the territory (`draw_map`).
- Territory: `hunt_region` / `hunt_map`. Compass zone: `def_super_territory_distance` <= `DEF_SUPER_COMPASS_REACH` (3).
- Temperature: COLD (>= 4 maps), COOL (2-3), WARM (1), HOT (on the target's map), BURNING (<= 120 px from the point).
  The battle starts within `DEF_SUPER_POINT_RADIUS` (24 px).
- The point: the field sends candidates (`fieldstg_super_points` in `src/fieldstg/fieldstg_80087DB0.c`: a flood fill of 8
  neighbours over the collision layer in 16 px cells, seeded at the walkable floor nearest the player, encounter tiles
  only); the chosen point is the one at 2/3 of the distance.
- Battle: one enemy, the HP that is left, `stat_scale` +20% per re-meeting (`def_super_scale`, capped at 10 meetings), flee
  after losing 25% of the bar in one fight (counted in battle units, Super Hard included); the party's flee always works.
- Win: a crystal (cap `DEF_SUPER_CRYSTAL_CAP` 99), the hunt ends, `def_super_won_take` feeds the panel.
  `def_super_crystal_sell` returns the row's price.
- Rare chain inside the territory starts at 2 wins, sure at 6; a party flee against a regional Rare there is 50%.
- **Toughness** (2026-11-18): the battle's HP is an `s16`, so a bar cannot be doubled; every hit of the player on a Super Rare
  (not the tutorial's Ogremon) does `DEF_SUPER_DAMAGE_PCT` 50% (`def_super_damage`, hooked after `wfightmn_cap_damage` in
  `fightstg_8008D3B4.c`): each 25% before the flee takes twice as long.
- **The Blast opens at the Ogremon** (`blast_unlocked` in `definitive.c`): the button and the DV Plug only work in the tutorial
  hunt's battle and for good once it is won. Igor gives the Plug on his Seiryu visit (`TALK05` 20, `def_story_igor_done`).
  **The blessed Blast** (the tutorial's battle, or the first Blast of the game: story bit `DEF_STORY_BLAST_FIRST`): the Plug
  costs no TP, the form carries `DEF_SUPER_TUTORIAL_BOOST_PCT` 50% more and a Super Rare target no longer flees from that battle.
- **The Hunter's Mark** (key item 27, a Japanese tag slot the disc never gives; `ITMNAM/ITMINF 27`): the Black Market's replicas
  list ends with it, `DEF_HUNT_MARK_BITS` 50000 and `DEF_HUNT_MARK_RARES` 2 different rare items (the two held most). Holding
  it a Rare Target leaves 2x its item (`rare_won`) and a Super Rare 2x its crystals (`def_hunt_mark_mult`). Tests:
  `tests/port/definitive_market.py`.

### Screen, compass, call (`definitive_superhunt_screen.c`, `def_superhunt_frame` in the field's idle step)

- **Seiryu City call:** now the story layer's step-4 hook (Repeating Tom's news on the inn's stairs opens the ways in; Igor
  shows the room, the screen cuts, the Sepikmon calls the player over with `TALK05` 17; see
  [The story layer](#the-story-layer-velvet-room-in-the-main-story)). The old direct pull (area 0x0E, story step >= 4, once,
  through `dw3_velvet_pull`) is gone: `dw3_velvet_pull` has no caller. `DEF_SUPER_FLAG_MET` (1) still marks the Sepikmon's
  first meeting.
- **The board:** 13 hunts with level, state (NEW/ACTIVE/DONE/--), where (sector and server, or "Central Park near Asuka
  City"), HP left, crystals and price. Confirm starts; confirming twice cancels the active one; Square sells a crystal;
  Circle/Triangle exits. On the first opening the tutorial starts by itself (`DEF_SUPER_FLAG_TUTORIAL` = 2).
- **Reward panel** after a win: "Hunt reward: <species> Crystal".
- **Compass:** polygons on layer 0x1004 (no heap window), a word and 5 coloured bars; in Portuguese in the PT build.
- **MATERIALS screen** (`definitive_materials.c`): crystals after the Cores, one per hunt, "???" until won; names over 21
  letters cut the species with a dot (`MAT_FIT`, `$MatCut`: "MasterTyrann. Crystal", "Cristal de MasterTyr.").
- Texts: `$Sup*`, `$MatCrystal`, `$MatCut` and TALK05 14/15 in `tools/text_override/en.txt` and `pt.txt`.

### Testing

- Host: `tests/port/definitive_superhunt.py` (+ driver), `definitive_hunt.py`, and the tests that link
  `tests/port/definitive_super_stub.c` (bestiary, dri, market, buffs). All in `scripts/test.sh`.
- In game: `.dumps/tasks/hunt/play/boot.py [MAP X Y DIR OUT]` boots from the console save (`card1.mcd`, Zhu Que Inn, party
  level 29), loads, and saves a savestate at the requested map (default Phoenix Bay -> `phoenix.state`). **A savestate is
  valid only for the binary that wrote it**: after building, run the boot again.
- Through the MCP (`mcp__dw2003__*`): `mem_write` on `st` (hunt state; `st+1` = hunts done), `run` (the point), `hunt+1`
  (forces the regional Rare on the next battle), `skin_hero`. In Phoenix Bay: right for 200 frames, down for 250 gives an
  encounter (the Rush question; Confirm fights).
- Link failures on `__start_dw2003_data_wstagNNN` come from objects cut by two builds at once: move the `wstagNNN` `.o`
  files aside and rebuild.
- To play in Portuguese through the MCP: write a settings file with `{"schema": 1, "video": {"window": false}, "mods":
  {"preset_language": {"enabled": true, "language": "portuguese"}}}`, pass its path as `game_start`'s `config`, then
  `state_load` an English state. To see the Super Rare colour without hunting: with a hunt active, `mem_write` `run+6`
  (`run.battle`) = 1 before an ordinary battle, so `def_super_battle_ready` holds for it (shot
  `.dumps/tasks/hunt/play/super_recolor.png`).
- Seen in game: a Super Rare in Phoenix Bay (fires on the point, comes alone, flees at 25%, changes map, keeps HP, returns
  buffed, win gives the crystal); the whole tutorial (Seiryu call, Sepikmon, board, compass COLD..BURNING in Central Park,
  Ogremon, reward panel, crystal in MATERIALS); the regional colour (RedVegiemon).
- The owner also played the tutorial to the end. The generator's readable proposal (`.dumps/tasks/hunt/alvos.md`) has a "Compass" section: per hunt, the outside areas where
  the compass shows. The tutorial hunt, Seiryu call and everything else of this front hang from the hero skins (unlocked by hunts won), see [Hero skins](#hero-skins).
- Checked in game on 2026-11-17: the colour of a Super Rare (200 degrees, RedVegiemon), the compass WARM outside the
  territory (Shell Beach), the cut crystal names in MATERIALS in EN ("MasterTyrann. Crystal") and PT ("Cristal de
  MasterTyr.", "Cristal de WaruMonza."), the PT texts of the board, the reward panel and the menu. The 50% flee against
  a regional Rare in an active hunt's territory is covered by the host test (`definitive_hunt_driver.c`: a draw of 49
  flees, 50 does not; the call is in `fightstg_8008D3B4.c` before the game's escape rule), not played.

### Save

The last 24 bytes of record 2 (`DEF_SUPER_RECORD`, `definitive_bestiary.c`; far from the Blast data that grows from the
start): 0 active+1, 1 hunts done, 2-3 map, 4-5 HP lost, 6 meets, 7 flags, 8-20 crystals. The hero skin uses the byte
just before them (`REC2_SKIN`).

## Recolour (palette swap of rare foes)

`port/game/definitive_recolor.c`: regional Rare 120 degrees, Super Rare 200 degrees (`rc_degrees`, adjustable), and the
Blast dark forms through the same path (`DEF_RECOLOR_DARK`: luminance 40%, slightly purple).

- An enemy enters `fightstg_slots_add` with model id >= 0x10 (0x10 in the opening script, 0x11 on an entrance); the party
  is 0 (1 on an entrance or a Blast form).
- **Why it is done on the CLUT, not at upload:** a battle Digimon's texture is a 16-bit VRAM page holding 4/8-bit texels and
  CLUT rows together, so recolouring at upload turns into noise. So when the texture loads (`fightstg_model_update`) the
  page is remembered (`def_recolor_texture_loaded`), and each CLUT the mesh uses (the texture command,
  `fightstg_model_mesh_set_texture`) is read from VRAM (`psyq_gpu_vram`), rotated and written back once
  (`def_recolor_mesh_clut`).
- Each model has its own VRAM page (`fightstg_slots_vram`), so the party does not change even when it is the same species.
- Owner answer (2026-11-17, PRD 4 open decision 1): a boss may be the Rare Target of several regions, with a different
  colour in each. Today the 71 targets are 71 distinct species, so nothing repeats; if one ever does, `rc_degrees` must
  turn the hue per region (for example the target's index times a step, skipping the Super Rare's 200 degrees) instead
  of the fixed 120 degrees every regional Rare uses now.

## Hero skins

**Status: done, tested in game (2026-11-17) and committed (`8c0971b`, the choice page in `eb2fa36`); the choice screen
is a page of the Sepikmon's board, there is no NPC of its own.**
`port/game/definitive_skins.c` with the hook `port_girls_reskin` in `girls_power.c`.

- Owner decisions: skins are **for the hero only** (Kail, or Junior without Girls Power); the player's Digimon keep their
  own colours (painting the team was dropped). Only rare enemies change colour. The first plan was an NPC in the
  **existing Velvet Room**; it became the Sepikmon board's "Hero Skins" page (see Pending (1) below). **Each hunt won unlocks one skin**
  (a reward that does not touch Bits or XP).
- 13 recipes, one per hunt (`DEF_SKIN_COUNT` = `DEF_SUPER_COUNT`): skin n opens when `def_super_done() >= n`, so the Ogremon
  tutorial gives the first. Skin 0 is the hero's own colours, always open. Each recipe has two parts, **hair** (the cap on
  Junior) and **clothes** (everything but skin and transparency). A part is either `ROT` (a hue turn from the hero's own
  colours, with saturation % and light %: used for black, silver and white, which are greys) or `TINT` (every colour of
  the part takes one hue and keeps its lightness, so the shading stays; a near-grey colour gets `TINT_MIN_SAT` first).
  `TINT` was added on 2026-11-17 after the first set (all hue turns) was shot on both heroes: one turn gave Kail's pink
  hair and Junior's light blue cap different colours (violet and mint), and the clothes-only turns barely showed on
  Kail. Skin colours never change.
- The set (chosen by Claude on 2026-11-17 from shots of all 14 on both heroes, `.dumps/tasks/hunt/play/skins_sheet.png`;
  every skin changes the hair or the clothes visibly, hair hues kept away from pink and light blue): 1 Black Hair (the
  tutorial's), 2 Green Hair, 3 White Outfit, 4 Gold Hair, 5 Red Outfit, 6 All Black, 7 Purple Hair, 8 Navy Outfit, 9
  Silver Hair, 10 Ocean (hair and clothes), 11 Sunset (orange hair, purple clothes), 12 Wild Colors (lime hair, magenta
  clothes), 13 Silver and Black. PT: Cabelo Preto, Cabelo Verde, Roupa Branca, Cabelo Dourado, Roupa Vermelha, Todo
  Preto, Cabelo Roxo, Roupa Marinho, Cabelo Prateado, Oceano, Pôr do Sol, Cores Malucas, Prata e Preto. Names are
  `$Skin0..$Skin13`; recipes are in `skin_recipes`, owner-tunable.
- `def_skin_set_hero` accepts only an open skin; `def_skin_worn` is the one the sprites use (0 in a replay script or when
  the chosen skin is locked, for instance a card with fewer hunts).
- Painting (`girls_power.c`): the hero's field sprites are 4-bit frames, each with its own 16-colour CLUT. The hero files are
  the ones Junior's actors use (0x186, 0x187, 0x3C9..0x3CC); with Girls Power they become Kail when cdload reads them. The
  hair and skin indices come from each person's `GpWho` masks (Kail hair 4/6/14, skin 1/2; Junior cap 2/3/7, skin 8/9).
  `gp_skin` always starts from the hero's original CLUT (`gp_ref_kaili.clut` or `gp_ref_junior.clut`), so repainting never
  accumulates and a frame with another CLUT is left alone. `port_girls_reskin` (every field frame, from
  `def_superhunt_frame`) repaints files already in memory when the worn skin changes; no map change is needed (it shows on
  the next animation frame; a standing sprite does not return to VRAM). `port_girls_refresh` (New Game, card read) also
  drops the files when the skin changes.
- Save: 1 byte in record 2 (`REC2_SKIN`), out of range reads as 0; savestate `def_skin_savestate`.
- Test: `tests/port/definitive_skins.py` (+ driver): unlock, worn skin, CLUT (hair and clothes change; skin, transparency
  and STP bit stay; no colour becomes transparent) and save.
- Seen in game with Junior (console save, Phoenix Bay; `mem_write` on `st+1` = 13 and `skin_hero`): skin 4 olive cap, skin
  6 black cap and clothes with the same face, skin 0 back to the original.
- **Pending:** (1) done 2026-11-17, as a page of the Sepikmon's board instead of an NPC (the owner: the hunts' master
  hands out the skins the hunts unlock): R1 on the board opens "Hero Skins", L1 goes back; the 14 listed, WORN on the
  one worn (palette 3, the active hunt's colour: the small font's green draws its O oddly), "--" and a dim name on locked ones, the info line names
  the hunt that unlocks a locked one; the confirm button wears an unlocked one (`skins_fill`, `skins_input` in
  `definitive_superhunt_screen.c`; texts `$SupSkinsTitle`, `$SupSkinWorn`, `$SupSkinFrom`, `$SupSkinOwn`,
  `$SupSkinOriginal`, `$SupSkinSet`, `$SupSkinLocked`, `$SupKeysSkins`); seen in game; (2) done
  2026-11-17: the hunt reward panel shows "New skin: <name>" under the crystal on a hunt's first win only
  (`def_super_won_take(&first)`, `$SupSkin`; a hunt won again unlocks none), seen on screen; (3) done 2026-11-17: all 14 seen on Kail and Junior;
  a state loaded in a fresh process used to lose what the hero's files were painted for, so Kail was not repainted
  (`port_girls_savestate` now keeps `gp_painted` and `gp_skinned`); a new skin shows on the next animation frame, so
  after under a second of walking; (4) dropped by
  the owner (2026-11-17, "not worth it"): portraits, menus (status, Digi-Lab) and the name/card screen keep the
  unskinned hero (other images; `port_girls_tim` swaps Junior's for Kail's, without a skin). Backlog ideas: a skin price (free or one crystal), a 3D
  preview later; Digimon skins were dropped.

## Blast, Purify and Chosen Evolution

**Status: done and committed** (`8c0971b`; the overheat that follows the partner in `ee6fd02`). `definitive.c`,
`definitive_purify.c`. The Blast Gauge was redesigned. Not yet seen in a real game: a full-gauge battle (see Pending).

### The gauge

- Gain per hit: WEAK +150, stagger +100, OVERFINISH +200, an extra action +100; using an item or fleeing -100. A plain hit
  does not fill it, and nothing triggers it automatically.
- Activation is the battle menu's 8th action **Blast**, R2 by default (the launcher has a BLAST row). It needs a full gauge
  (1000), no Blast already active, HP, and no status 8 or 0x20.
- Price: HP and MP at 1 after the battle, and -25% Power/Guard/Speed through `GamestateStats.penalties` (cleared only by an
  inn's `inn_heal_party`).
- The 5 dark forms per partner come from the game's `RecordsDigimon.blast_forms`. They advance in order with step levels
  {1, 15, 25, 35, 45} and repeat the last reached when the level is too low. `DW3_BLAST_STEP` forces a step in tests
  (`DW3_BLAST_FORM` forced a form; old). Forms are painted dark (see Recolour).
- **Overheat** (Blast and Chosen alike): when the form ends the Digimon is back in Rookie and sealed from digivolving for 3 of its own turns.
  The turns left are saved with the partner (high nibble of its `REC2_BLAST` byte) and follow it into the next battles, which it starts
  sealed in Rookie; only an inn's rest (Gatomon) clears it, or Gennai for the Chosen Digimon. While it lasts the Digimon also starts the battle as its
  Rookie (not its shown form) and the Tag menu offers it only as its Rookie (`def_blast_heated`).
- **Boost** (`def_blast_boost_pct`, applied to the five stats by `def_blast_boost_stats` in `fightstg_rules_get_stats`'s party
  side): while the quest is DECEIVED the false Gennai (Myotismon) sells the Blast a stat boost in his panel: one step goes on
  sale with each partner+form recorded (up to the quest's ten), the k-th for k x `DEF_BLAST_BOOST_BITS` (1000) Bits, each
  +`DEF_BLAST_BOOST_STEP` (3)% to the Blast form's stats, so +30% at ten (texts `$PfBoostOffer`, `$PfBoost*`). The steps bought
  are saved (`blast_boost`, record 2). The Chosen Evolution has `DEF_CHOSEN_BOOST_PCT` (50)% from the start.
- **The 999 cap** (done): the Blast's boost stops at 999 (a stat already past it keeps its value); only the Chosen boost
  (+50%) goes up to 9999 (`def_blast_boost_stats`, the bound keeps the damage formulas in range). Not yet seen in a real fight.
- **DV Plug** (item 76): fills the gauge and triggers at once, costs 10 TP (`DEF_BLAST_PLUG_TP`); after the quest it revives to
  Rookie at 1 HP / 1 MP: it goes through the game's revive path (item 0x46, `fightstg_8008D3B4.c`), which revives every fallen
  partner, each as its Rookie with the Blast flag off (`def_blast_plug_revive`; a partner still overheated stays sealed); its
  description is then `$PlugRevive` instead of `ITMINF` 76 ("Revive: Rookie form, 1 HP and 1 MP" / "Revive em Rookie com 1 HP
  e 1 MP"). Before the quest it is still the curse.
- **Temptation:** a full gauge with a knocked-out partner, or the acting Digimon at <= 25% HP, makes the Blast button pulse
  red (8 ticks on, 8 off) with "Blast calls...".
- Save: eight s16 gauges at slot+0x26F0.

### The quest (`definitive_purify.c`)

1. **DECEIVED:** "Gennai" (really Myotismon) shows how many of the 10 partner+form pairs you have collected. At 10 he asks
   for 10000 Bits, a black-screen story plays, and the real Gennai appears.
2. **REVEALED:** the simulator runs the 10 forms in waves of 3 (no repeated species in a wave) at 90% of first-time stats,
   with no results screen and no XP or Bits. Defeat returns to the field.
3. **DONE:** TP refunded (to 99); a permanent +10% of the best stat per purified form (overflow past 999 goes to the next
   strongest stat); Blast becomes the **Chosen Evolution**.

### Chosen Evolution

One partner at a time, chosen with Gennai: level 45, Rookie only, one per battle, one charge (Gennai reloads it), full HP/MP,
no curse. Megas: GuardiAngemon, Beelzemon (Kumamon), Cannondramon, WarGreymon, Imperialdramon, Gallantmon, Sakuyamon,
Seraphimon. "Chosen needs Rookie" shows in the menu. Test: `tests/port/definitive_quest.py`, `definitive_gauges.py`.

### Pending

The main-story onboarding hooks (out of scope, the owner is changing the main quest on their own);
`PF_SND_CURSE` is a provisional sound; the Chosen's break of the 999 cap is done but never seen in a real fight; a real playtest of a full-gauge battle (a normal battle with WEAK/stagger filling the
gauge to 1000 was never seen in a real game; the rule is in the `DW3_BATTLE_LOG` lines "blast gauge hit").

### Blast: how it was designed (the owner's text and the decisions)

- **The original game's Blast:** `wfightmn_add_gauge` filled `records_state.gauges[Digimon id]` (0..1000) only with **damage taken**; at 1000 event
  0x12 -> `wfightmn_blast` (message 0x61 "Blasted!", form `blast_forms[level band]`, full HP, `blasted = 1`, end scheduled by event 0x13). **Pitfall
  (already made once):** the `wfightmn_handlers` table is shifted: the step is the event type **+ 1** (event 0x12 -> step 0x13; step 0x12 is
  `wfightmn_seal_end`).
- **The owner's three-phase text ("Chosen Evolution", 2026-11-16).** Phase 1, the cursed evolution (campaign): charge by aggression only (WEAK!, chained One
  More, hitting staggered targets, OVERFINISH; defending, healing, fleeing do not charge or cool it); optional activation (a dedicated button, or the temptation
  at critical moments such as a partner falling on Hard Mode: the temptation is implemented in any mode); a hand-picked pool of dark/aggressive forms scaled by
  level instead of the original random evolutions; the price (after the battle HP and MP are 1 and a persistent debuff stays until an inn); the old useless Blast
  charging item (it needed 10 uses) now fills the gauge at once with 1 use but costs the Digimon TP, warning in its description. Phase 2, purification and trial
  (late game): the save records each unique dark form the player invoked; for a fee in Bits/materials the player faces, in continuous sequence without recovering
  between waves (like the DRI Gauntlet), all the forms they forced; winning returns the TP sacrificed. Phase 3, total mastery: summon and purify every form on the
  Blast list; reward: permanent stat buffs, and the Blast Gauge is purified into the definitive evolution (DigiChosen / Gennai concept): no after-battle debuffs;
  **Rookie rule** (start in the base form, accepting less speed and stats, then warp-digivolve straight to the canonical/chosen Mega); and it is the only mechanic
  able to exceed the 999 stat cap in combat, in a controlled way (done: the Chosen's boost goes up to 9999, see The gauge).
- **The first implementation (2026-11-17 morning)**, later replaced: a pool of 10 forms by level band (<4 Devimon; <19 Devimon/SkullGreymon/Digitamamon; <39
  SkullGreymon/Myotismon/Cannondramon; <70 Myotismon/BK WarGreymon/GranKuwagamon; 70+ BK WarGreymon/MaloMyotismon/Beelzemon/Diaboromon; the partner took
  `pool[partner % size]`; `DW3_BLAST_FORM=line` forced one), a fixed list of 10 forms, a fee per form and a group mirror. The owner's redesign (afternoon, answer
  "A") replaced it with the game's own `blast_forms` (5 per partner, 29 distinct), painted black when they come by Blast, in sequence per partner, and the
  quest below. The first-implementation tests were `blast_test.py`, `price_test.py` (the log says "digimon 0 pays: HP and MP 1, penalties 12 11 8"), `form_shot.py`,
  `plug_test.py`, `.dumps/tasks/blast/tempt_test.py` (full + low HP tempts; full + full HP and empty + low HP do not).
- **The price** is `GamestateStats.penalties` = 25% of Power, Guard and Speed (`def_blast_started` in `wfightmn_blast`; `def_blast_price` after the HP/MP write-back at
  the battle's end). The original never writes that field; only `inn_heal_party` zeroes it and the status screens already mark a penalised status; no new save field.
  A second Blast before an inn does not deepen it. **DV Plug** (`def_blast_plug`, effect 0x4C of `fightstg_8008D3B4.c`): one use fills the gauge, takes
  `DEF_BLAST_PLUG_TP` = 10 TP (it takes whatever the Digimon has if it has less) and calls the game's event 0x12; text "Blast past its limit. Costs 10 TP" / "Blast além do limite. Custa 10 TP".
- **Activation details:** menu action 8 (the choice 7 -> command 7, `DEF_BLAST_COMMAND_ID`, step 0x13, spends the turn like the DV); dark star icon (`01 36`) under L1;
  greyed and refused otherwise; `tests/port/mods.py` expects "R2: refused 7" with an empty gauge. The launcher (C, desktop and Android) has the BLAST row (`H_BLAST`, `bm_blast`,
  default R2; on Android the screen's rows went from 27 to 25 px to fit); the psxstack one reads `mod.json`.
- **Gain rule** (`def_action_resolved` -> `blast_add`, pure `def_blast_gain`; numbers `DEF_BLAST_*`): per blow that lands, WEAK +150, stagger +100, OVERFINISH +200, a blow in an
  extra action +100, summed; item and flee cool -100 (`def_blast_command`); taking damage does not fill; nothing fires by itself (`def_blast_auto`). Temptation threshold
  `DEF_BLAST_TEMPT_HP_PCT` (25), `def_member_knockout` flags a partner fallen in this battle.
- **The owner's redesign, kept as written (2026-11-17, afternoon):** forms from the game's `blast_forms`, in sequence per partner (form 1 level 1, 2 level 15, 3 level 25, 4 level 35,
  5 level 45, else repeat the last reached; after the 5th back to the start). **Recording:** the **first** status of each form (the Digimon transformed the first time: stats, HP, MP;
  the species gives the techniques) and the **best** status it ever had; it needed a third card record (the second did not fit). **The quest:** 10 = 10 distinct partner+form pairs (the same
  species on two partners counts twice, so two partners with all 5 close it: works on Hard Mode); 1) the old man of the room introduces himself as **Gennai** (really Myotismon in disguise),
  only shows how many forms you released and tells you to master them, no quest; 2) at 10 he **charges the Bits** (the fee), reveals he tricked you and that your Digimon is cursed; black screen
  and a sound; the real old man appears, he is Gennai, the other was Myotismon, and he believes you are a DigiDestined; 3) Gennai opens the **simulator**: the 10 forms in sequence (waves of
  3 chained, no results screen between), each with the **first recorded status - 10%** and the species' techniques; 4) winning: **all TP** from the DV Plug back; **a permanent buff per
  purified form: +10% of the best status that form had** (in the attribute where it was strongest, for the partner that took it; what passes 999 goes to the next strongest attribute,
  and so on); Blast **disappears**, replaced by the **Chosen Evolution**. **Chosen:** one Digimon at a time chosen with Gennai (can change with him anytime); needs the quest done **and level 45**;
  the form is the partner's standard Mega, **Kumamon -> Beelzemon**; only from the **Rookie** form (a warning in any other evolution); **one per battle**; **one charge** (after use only
  talking to Gennai recharges); on use it restores everything (HP and MP); the **DV Plug** then revives the Digimon as Rookie with 1 HP and 1 MP. Implemented as described in the quest
  section above (`definitive_purify.c` header explains it; `DEF_PURIFY_BITS` 10000, a 2-page story, black screen with sound `PF_SND_CURSE` provisional, the real Gennai in 2 pages, then the
  simulator; waves of 3 with no repeated species, status x 90%, `wfightmn` sends straight to the field between waves, no XP/Bits; defeat: back to the field with HP/MP as before and the defeated
  forms stay purified; the end: TP returned up to 99, the +10% buff, Blast turns Chosen; `DEF_CHOSEN_MEGAS`). The dark paint is `DEF_RECOLOR_DARK` (luminance 40% with a slight purple tone; the
  party model id 1 when the member is in Blast, `def_blast_dark`, and the simulator's enemies id 2); the Chosen comes out in its normal colours. Tests (private binary
  `.dumps/tasks/blast/v2/dw2003`, because the other session recompiles the shared one): `blast2_test.py` (sequence step 0/3/2, recording, black paint, Chosen and refusals), `dark_shots.py`
  (contact sheets: dark GranKuwagamon, normal GuardiAngemon), `quest_test.py` (the whole quest). **Out of scope (owner, 2026-11-17):** the main-story hooks (the "organic onboarding" text above).

## Girls Power (playable Kail)

`port/game/girls_power.c`, `girls_power_art.h`. Kail is the hero and Junior the friend. The story's English text follows
the swap (`tools/text_override/en_kaili.txt`, `tools/gen_text_override.py`): the hero is Ivy in the Real World and the
player's account name in the Digital World, Kurt is her brother, and the friend is Junior in both worlds (Junior becomes Ivy;
Ivy and Kail become Junior; Smith Junior keeps his name). Portuguese swaps only Kail until the rewrite is translated. The choice is
"CHOOSE YOUR HERO!" on New Game after the difficulty, saved as `DEF_SAVE_KAILI` (old saves stay Junior). `DW3_GIRLS=0/1`.

- Sprite files repainted at cdload time: 0x186, 0x187, 0x3C9-0x3CC, 0x190, 0x2F8, and the name-entry VRAM pages 0x28B and
  0x771 through `port_girls_tim`. `port_girls_refresh` drops the cache.
- Method: head match against idle heads including mirrored; a whole-frame swap for animations 1, 4, 7, 9, 12 and 51; the run
  bust from her walk frame; hand-drawn retouches in `girls_power_art.h` (174 frames, all drawn).
- Tools: `tools/girls_power_preview.py` (`OUT_DIR [SCALE]` writes before/after sheets; `zoom OUT.png FILE_ID FRAME,FRAME,...
  [SCALE]` shows per frame the disc original, the automatic repaint, the retouched result and the other's frame it was
  made from, and prints the automatic frame's rows to start a retouch from) and `tools/girls_power_draw.py` (subcommands `anims`, `text`, `refs`, `show`, `moves`,
  `edit`, `base`, `build`, `check`). Debug: `DW3_GIRLS_DUMP`, `DW3_GIRLS_NUMBERS`. Test: `tests/port/girls_power.py`.
  `girls_power_art.h` holds `GP_PASTE` (a piece of a disc frame read at run time; how 1 mirrored, 2 through the colour
  table) and `GP_ART` rows (`.` keeps the pixel, `0` empties it, `1`-`9`/`A`-`F` paint it); no disc pixel is stored there.
- Pending: the owner's full playtest; weak spots (a one-pixel rod, a 24-wide pigtail clipping, lying frames).

### Girls Power: how it was built, decisions, and the hand-drawing workflow

**Owner decisions (2026-11-14):** swap the roles (the old Kail becomes Junior); the new art, technically, was left to the implementer; deliver everything at once; **no text changes**
(Kail is called "Junior" and Junior "Kail" in the game's strings); it lives in the game itself, not on the memory card (only the choice is saved, `DEF_SAVE_KAILI`).

**Disc census (2026-11-14).** Field characters are 2D 4-bit sprites (`FIELD/SPRT/P###xxxx.BIN`, `docs/FORMATS.md` "Actor sprites"). Junior's exact 16-colour palette was searched in all
2382 disc files and appears only in: `P001PLAY` (0x186, Junior in cutscenes, actor 1, 27 animations) and `P002PLAY` (0x187, controllable, actor 2) -> become Kail; `P002PLYD/F/K/L`
(0x3C9-0x3CC: actions `PLYD`, fishing, kick, `PLYL`) -> become Kail; `P011MINO` (0x190) and `P012MINT` (0x2F8), the Kail NPCs (actors 11 and 178) -> become Junior; the VRAM pages of the name
screens (`NAMEENT/STNMETTM` 0x28B, `SCREEN/NAMEDTTM` 0x771: name entry, confirmation, Digimon name and deck, with Junior's front frame 85) receive Kail's own front standing frame and her CLUT. The
original premise ("Kail has almost all the sprites") was false: she has 9 animations against his 27 plus the actions; so the solution **repaints his frames** instead of pointing the actor at her file.

**Method.** As soon as `cdload` finishes reading one of the 8 files (`src/main/cdload.c`, transition 2->3) every frame the animation tables cite is repainted in its own buffer.
- **Whole frame of the other** in animations both have for the same thing (1 idle, 4 walk, 7 talk, 9 startle, 12 wave, 51 hop): the frame gets the other's original frame (same animation and direction)
  read from the disc: hers from `P011MINO`, his from `P001PLAY`. A script shared by several slots (drawn one way) uses the head direction of its first frame to pick the other's slot; script entries
  that cite the frame are shifted so it lands where the other's script draws it. Result: 65 of the 84 frames of Junior (ex-Kail) and 47 of the 87 of the walkable Kail are original. 57 and 58 only coincide in number
  (another gesture, another side); running, kick, fishing and the cutscenes that only one has keep the head swap.
- **Running (his animations 5 and 6):** she has no such drawing: head and trunk come from her walk frame (same step and direction) down to the belt; of Junior only the legs stay, in her colours. He runs
  crouched (head up to 4 rows lower) and her trunk is longer, so she drops only half. With this the walkable file has no frame with a glued head: 47 originals + 40 running.
- **The head:** the character's own idle heads (animation 1, 5 directions) are matched to the frame pixel by pixel, also mirrored; direction = the frame's slot in the animation table (5 slots = 5 directions) when a
  single-slot script cites it, else the matched head's. With at least half the pixels equal the other's head goes exactly where the original is (it follows the original motion); otherwise (tilted or lying head) it goes
  on the chin found under the cap/hair. The two heads are aligned face to face (mean x of the skin). Pieces of up to 8 pixels that the removal leaves loose (a pigtail tip, a cap corner) go with it. A head that
  covers the body (lying, seen from above: 3/4 of the frame would be removed): none of the 5 heads fits, so the cap/hair is only tinted with the other's colours, no paste. Her hair falling on her back (below the
  chin with body on both sides) is not removed (it would leave a hole): it becomes Junior's clothes. (Before 2026-11-14 the direction was guessed from the face skin frame by frame: it was wrong in almost every 3/4 and
  back view and turned the head inside the idle, "head spinning".)
- **Clothes:** recoloured with a 16-index table and the CLUT becomes the other's. **Unchanged:** animations, timings and scripts stay the game's, so kick, fishing and all cutscenes work without animation translation.
- **References:** heads come from the original standing frames read straight from the disc (`port_disc_read_user`, new in `psxstack/runtime/disc.c`). **Menu screens:** `tim_load_all` (`src/main/tim.c`) calls
  `port_girls_tim` before each upload; the page with Junior's frame 85 and CLUT receives Kail's (animation 1, direction 0) and her CLUT, recognised by content. **Switch:** the question "CHOOSE YOUR HERO!" on New
  Game right after the difficulty, same panel (`src/stdwtitl/stdwtitl_800852F0.c`, step 11; X goes back to difficulty), Kail first; `port_girls_refresh` (New Game, save read in `stgmcard`) drops the 8 files from the cache if the heroine
  changed; off under a replay script; `DW3_GIRLS=0/1` forces it; letters J and P were added to the polygon font (`definitive_callout.c`). The matching build is untouched (all under `PC_PORT`).
- **Verified:** the C equals the Python prototype (`tools/girls_power_preview.py`) in the 465 frames of the 8 files (`DW3_GIRLS_DUMP=<dir>` writes the repainted files); `tests/port/girls_power.py` (every frame's CLUT is the
  other's, the frame changed and has the other's head; off under a script); `tests/port/run.py` passes; in game (captures `.dumps/tasks/girls-power/st/`): Kail walks in several directions; the Kusanagi street shows Kail, Junior (the ex-Kail) and Takuya
  together; the Digimon Online confirmation screen shows Kail.
- **Playtest checklist for the owner:** lying poses (anims 16, 38, 39 of the cutscenes: cap becomes hair seen from above, no pigtail); tree kick, fishing and the `PLYD`/`PLYL` actions (checked frame by frame on sheets, not in play); the deck
  and Digimon name screens (same page `0x771`, never seen running); anywhere else Junior appears (the exact-palette census found none, but an image with another palette would slip through). To tune the algorithm:
  `tools/venv/bin/python tools/girls_power_preview.py <dir>` generates before/after sheets; change the Python, compare by eye, then replicate in C; `DW3_GIRLS_DUMP` plus the test confirm both match.

**The hand-drawn frames (owner, 2026-11-14): "do all 174, one by one, no shortcuts, cost and time are not a problem, I want quality"**: every frame the automatic head swap still built was drawn by hand as pixel art, each studied alone,
compared with Junior's original frame of the same pose and with Kail's own sprites (so her identity does not change), and checked **mirrored** (the game draws the left directions by mirroring the right ones). Whole frames and the run bust swap are not
in that list (they already use real sprites). **All 174 are drawn**; what is left is the owner's in-game review and redoing what they point at. Numbers: kick `3CB` 25 frames (done 2026-11-14), fishing `3CA` 70 (2026-11-15), `3C9` 7, `3CC` 16
(2026-11-14), `P001PLAY` `186` scene gestures 37 (2026-11-15: arms crossed 92-96, tapping 97-99/100-102, 103 105 107, 108-110, 106, 111, 112, 116, 119-121, 122 123, 127 128, lying 132-138 with her own head pasted, 139 140 her own hop frame with sparks in white),
`P011MINO` `190` (her file becomes Junior in **his** palette) 19 (2026-11-15). Redrawn after the owner's review of 2026-11-16 (sets `ladder`, `fish_profile`, `fish_34front`, `fish_34back`): `3CC` 2-10, 14, 15 whole (one bent arm from her shoulder, legs alternating
as his; 0, 1, 11-13 as first done); `3CA` 27-33 (her 3/4 front head, bowed, instead of a profile head pushed 5 px ahead of her shoulders), 37-39 (striking arm bent, far foot forward), 41-48 and 14-19 (head back over her shoulders, down instead of forward);
front 55-68 and back 0-13 looked at, not changed. **A head that leans is never slid ahead of the shoulders: it goes down, or it is another head (turned), as his.**

**Where each drawn frame is** (from the frame comments in `girls_power_art.h`): kick `3CB` (`P002PLYK`): back 0-4, 3/4 back 5-9, profile
10-14, 3/4 front 15-19, front 20-24. Fishing `3CA` (`P002PLYF`): back 0-13, 3/4 back 14-26 and 69, profile 27-40, 3/4 front 41-54, front
55-68. `3C9` (`P002PLYD`) 0-6: the actions, all 3/4 from the front. `3CC` (`P002PLYL`) 0-15: the ladder (up over the edge, at its foot,
climbing 3/4 from behind, the top, jumping down and landing, holding on). `186` (`P001PLAY`): the run 30-69 and the scene gestures listed
above. `190` (`P011MINO`, Junior in his palette): 28, 29, 36-52. `187` (`P002PLAY`): the run 30-69. The run frames of `186` and
`187` (80) are on top of the 174: the first hand retouches of `187` came with the run bust (`5214165`), the rest were drawn later
(`685214b`, `86c6d0c`, `9f21e31`).

Tools (`FILE` is hex, `FRAME` decimal sub-file number): `tools/girls_power_draw.py edit FILE FRAME` (creates `.dumps/tasks/girls-power/draw/<FILE>_<FRAME>.txt` from the header; no-op if it exists), `base FILE FRAME` (rewrites the drawing as automatic frame + its `paste`
lines; hand pixels are lost: use right after changing pastes), `show FILE FRAME [FILE:FRAME ...]` (run with `uv run --no-project --with pillow`; writes a PNG with disc / auto / drawing on a 4-px grid, the references (hers in `190` by default), all 3x including the
drawing **mirrored**, and the animation script as the game places it, normal and mirrored, current frame boxed), `build` (regenerates `port/game/girls_power_art.h` from all drawing files: pastes + only the pixels that differ from the automatic frame), `check` (runs
`build/port/dw2003` with `DW3_GIRLS_DUMP` and compares the 8 repainted files with the Python prototype byte for byte; rebuild the port first). Drawing file: `#` lines = the header comment; `paste SRC SRCFRAME X Y W H TOX TOY HOW` (a piece of a disc frame put on the frame at run
time; HOW 1 mirrored, 2 through the colour table); `%` lines ignored (column ruler, each pixel row ends with `%row`); pixel rows: `.` empty, `1`-`F` palette index. **The header must never hold disc pixels**: her real head/trunk at another angle is `paste`d (read from the disc at run time);
what is hand-drawn goes in the rows. The drawing files are local (`.dumps/`, gitignored) because they hold disc pixels; the header is the source of truth (`edit` recreates a drawing from it). After a batch: `build` -> `make -C build/port -j8` and `ninja -C build/port-sdl` (the owner plays the SDL one;
-Werror) -> `check` -> `tools/venv/bin/python tests/port/girls_power.py` -> `tests/port/run.py`. Commit in English (`feat(girls-power): ...`), no AI attribution in the repo's files, **no tag, nothing copied to ~/Applications** (the owner pulls from `build/port-sdl`).
Local helpers in `.dumps/tasks/girls-power/helpers/` (rewrite if lost; thin wrappers over `read`/`write` of `girls_power_draw.py`): `compose.py FILE FRAME "comment" SRCFRAME:X0-X1:A-B@DST:DX ...`, `over.py FILE FRAME` (stdin `ROW:pixels`, `.` keep, `0` erase), `cpdraw.py`, `where.py FILE frames`, `stamp.py` + `st/*` (arm stamps
per direction), `rev.py` (disc / drawn / mirrored strip), `addpaste.py`, `mv.py`; copies in `.dumps/tasks/girls-power/backup/` with a patch of the paste support.

**Placement rule (learned on the kick).** Her head moves against **her** standing frame exactly as his head moves against **his** standing frame of the same direction (template match of his idle head; "his head moved (dx,dy)"). In paste terms: offset for her idle frame of direction d = (her idle script x
- his idle script x + dx, her idle script y - his idle script y + dy); her feet stay on **her** idle ground (her idle feet row + her idle script y - this frame's script y). When his head drops (crouch, wind-up) her body is shortened to fit: drop jacket rows (leaning) and leg rows (bent knees), never squash the head.
Aligning by his chin (what the automatic swap does) makes her body too short and puts her 1-2 px off her idle position (a visible hop at the start/end of the animation).

**Procedure for one frame:** `edit`, then `show` with 2-4 of her references in the same direction/pose; read Junior's original (the action, hands/feet, where the head turns, what is in front); head: her idle head of the right direction is what the swap put there, if the pose wants another angle or tilt `paste` her head from the frame that has it, then `base`;
body: redraw in **her** design, not his recoloured: jacket light blue-grey `D` with green `F` lapels/collar and `C` shading, white shirt `A` in the middle, outline `8` on clothes and `7` darkest, skin `1`/`2` with `B`/`3` edges, trousers `C`/`5` with `D`/`A` highlights, green shoes `F` with `8`/`7` outline; keep his silhouette and motion
(his timing and offsets), change the costume; look at the 3x row and the mirrored copy (no stray pixels, shapes read at game size, pigtails do not flip into something odd); look at the animation strip (the head must not jump between consecutive frames, match neighbours, draw neighbours from the same key drawing); write the `#` comment, `build`.
**Palettes.** Kail (her files, and Junior's files after the swap): `1` f8c0b0 skin, `2` e88880 skin shadow, `3` 8870a0 purple shade, `4` e05860 hair, `5` 383880 navy, `6` a03058 hair dark, `7` 301820 outline darkest, `8` 485048 clothes outline, `9` 907880 mauve grey, `A` f0f0f0 white, `B` 704850 brown edge, `C` 6868b0 violet blue, `D` 98a0c0 light blue grey, `E` 581850 hair outline, `F` 98c828 green.
Junior (his files, and P011MINO/P012MINT after the swap): `1` f85058 red, `2` 5068a0 blue, `3` 80b0f8 light blue (cap), `4` d05830 orange-brown, `5` 784078 purple, `6` 6858a0 violet, `7` 9880c0 lavender, `8` f8c098 skin, `9` f88888 skin shadow, `A` d09038 orange, `B` 507068 dark green, `C` 582050 dark purple, `D` 404858 outline, `E` 788898 grey, `F` f0f0f0 white.
**Her references (`P011MINO`, `190`):** idle (anim 1) slots 0-4 = frames 4 / 3 / 2 / 1 / 0 (+ breathing 82-83, 80-81, 78-79, 76-77, 74-75); walk (anim 4): front 56 57 55 58 59, 3/4 front 20-24, profile 66 67 65 68 69, 3/4 back 10-14, back 61 62 60 63 64; gestures 37-40 front arms out / eyes closed / mouth open, 41-48 3/4 and profile variants, 49-54 profile/back, 19 29 31 34 36 jumps, back views and a lying pose.
**Review in the game:** `DW3_GIRLS_NUMBERS=1` writes each repainted frame's sub-file number in its top-left corner (3x5 digits, mirrored when the game mirrors the frame); the owner sends a screenshot, the number names the frame, the file is known from the action (fishing `3CA`, kick `3CB`, climbing `3CC`, scene gestures `186`, walking `187`, the Junior NPC `190`).
Known weak spots, first candidates if the owner complains: the rod is one pixel wide in her dark hair colour (index 6); 3/4 frames in 24-wide files lose 1-2 px of the pigtail tip at the frame edge (95, 97-99, 108-110, 119-123, 93, 100-103, 107); the lying frames keep the automatic body; 122/123 differ from standing only by head height and elbow. Fishing plan used (`3CA`): per direction compose her body (the head of that
direction; the 3/4 head where he turns his head to look at his hands, 55-58 in front), then `paste` his own arms and rod from the same frame with how=2 (recoloured: his white short sleeves become her green upper sleeves, his skin hers), then hand-fix (his bare forearms -> her light sleeves D/C with a white A cuff, clean any of his torso pasted along); the rod/line keep his motion exactly.
Notes for whoever continues: in-game verification through the MCP `dw2003` tools did not get past the title's Continue with the owner's memory cards, so rely on `show` + `check` and ask for screenshots; Junior's files share one palette, hers another, frame sizes vary (24/28/32 wide, 24-32 tall); `girls_power_preview.py repaint` order per frame: whole -> bust -> head swap -> the header's steps (pastes, then art, in header order).

## Battle HUD and launcher options

Buttons: Item L1, Tech R1, Fight B (east), Run X, Digivolve Y, Tag A, Inspect L2, Blast R2. The `battle_menu` options are
`fight`, `tech`, `digivolve`, `tag`, `item`, `run`, `inspect`, `blast` (a place on the pad: `south` `west` `north` `east`
`l1` `r1` `l2` `r2`), `remember_cursor` (default true) and `buffer_input` (`off`, `short` 1.0 s, `long` 1.5 s). The options
name a place, not a PS1 button; `pad.map` (`dw3_pad_permute`) decides which bit that place delivers, so remapping moves the
action with the button. Two actions on one button fall back to the defaults with a log line. `PORT_MOD_MAX_OPTIONS` was
raised from 8 to 12 for the `inspect` option. Details: [`docs/LAUNCHER.md`](docs/LAUNCHER.md) "Battle menu".

### Battle HUD: design record (plan 8, the opt-in mod that became always-on) and the glyph fix (plan 9)

- **Plan 8 findings** (verified in the source, which made it small): the main list is built in one place only, `fightstg_command_menu_create(obj->last, &obj->choice)` called from
  `fightstg_command_update` step 2; nothing else creates or reads a `FightstgCommandMenu`. The contract is tiny: write `0..5` into `*result` (0 Fight, 1 Tech, 2 DV, 3 Tag, 4 Item,
  5 Run) and end; WFIGHTMN reads only `FightstgCommand->command/arg/arg2`, which `command_update` fills afterwards. Phases (`fightstg_command_set_phase`): 0 idle, 1 HUD only, 2 main menu (ours),
  3 confusion menu (a separate object, `fightstg_confused_menu_*`), 4 Tag-only member menu; `wfightts` (debug battle) only sets 0. Rules the original enforces and the HUD copies: with
  `member->status & 8` entries 0-2 (Fight, Tech, DV) are greyed (palette 7) and refuse input; with `status & 0x20` entry 2 (DV) is greyed and refuses; a refused press does nothing (the original
  still plays the confirm sound first). Labels are localised data, not literals (`records_language + 0x7F`, strings 1..6), so it works in every language. Pad bits (`src/main/pad.c`):
  `get_pressed(0)` are the PS1 layout after the port's own pad map (`dw3_pad_permute`): 12 triangle, 13 circle (confirm `0x2000`), 14 cross, 15 square, 10 L1, 11 R1, 8 L2, 9 R2, 4-7 d-pad;
  **the left analog stick also sets the d-pad bits**, so the d-pad is never offered for actions. Mod plumbing copied: a `PortMod` record, a `port_mod_<name>` int set in the mod's `start`,
  read in game C only inside `#ifdef PC_PORT`, a `port/mods/<id>/mod.json` manifest, `tests/port/settings.py` checks every manifest equals the registry (`--print-mods`).
- **Design:** original default buttons were Fight Cross, Tech Square, Item Triangle, DV Circle, Tag L1, Run R1 (named by PS1 position so the player's own map still applies); the shipped HUD
  is the pad-shaped layout in the section above. Options were enums of `cross circle square triangle l1 r1 l2 r2` (duplicates are rejected with a log line and the defaults are used). The new code
  lives in a PC_PORT-only file with one dispatch line in the original (the matching C's `LOOP_BLOCK` is register-allocation evidence, `docs/MATCHING.md`); input is ignored on the creation frame
  (hand-over guard: a shortcut equal to confirm `0x2000` must not also confirm inside the submenu it opens; tested); the menu consumes no RNG, so mod-off replays stay byte-identical; text and
  sprites are drawn with what the game already uses (`message_create_window(0x1005, 1, x, y)`, button glyphs from `CMFONTCS` or the HUD bank, layer 0x1005); no new art; an optional press flash must
  not change frame counts or RNG.
- **Confusion menu (phase D of plan 8, optional, last):** the confused menu is a different object with the same six-row list but shuffled labels (`fightstg_confused_menu_order`), a `roll_confused`
  check and a message queue. Decision: **leave it as it is** (the shuffle is the whole effect; a button-per-action UI would defeat it). A `confusion_style` option (`original` default, `symbols`: same
  behaviour and RNG usage but each row drawn with a confusion marker) was proposed, with a first step of rendering a **test page** of candidate glyphs (Shift-JIS symbols such as `？！～★☆◎○●×△□` and the
  extra glyph range `01 xx`) for the owner to screenshot, and "if nothing usable shows up, `symbols` is dropped, not faked". `confusion_style` does not exist in the code today: not done.
- **Plan 9, the glyph bug (the owner confirmed the fix on the console, binary MD5 `9ff7e35c62b01aa55689f76799b1de61`; inputs were not changed).** Cause: the battle menu's places use the **launcher's names**
  (physical button) and `pad.map` uses the **game's names**; the launcher translated with `PLACE_TO_GAME` when writing (south -> west, west -> east, east -> south, north stays north) but the code looked up the
  letter and symbol without translating, so only `north` matched. Fix: `dw3_pad_bit_by_place`, `port_battle_menu_symbol_bit`, `bm_place_to_game` (in `port/game/game_mods.c`), and `fightstg_battle_menu_icon_text`
  (`src/fightstg/fightstg_8008D3B4.c`) uses the `pad.map` button in the action's place; declaration in `include/port.h`. The C launcher still has the same translation (`launcher/cfg.c` comment "dw3-menu.py PLACE_TO_GAME").
  **The simplification list (plan 9 step 2) was never confirmed done** (unverified; baseline `tests/port/mods.py` before and after each item): 1) unify the place tables into one structure per position (`bm_places`/`bm_place_bits`,
  `dw3_pad_places`, `dw3_place_index`, `bm_place_to_game`: the bug came from three mixed place vocabularies); 2) one source for the launcher-game translation (today `PLACE_TO_GAME` in `dw3-menu.py`/launcher and
  `bm_place_to_game` in C: keep one, e.g. the launcher writes the battle menu already in the game's names, or document both side by side); 3) store `pad.map` once, letter and PS1 button per position (today
  `letter[bit]`/`perm[bit]` and the per-place arrays `dw3_pad_letters_by_place`, `dw3_pad_bit_by_place` hold the same data); 4) `fightstg_battle_menu_icon_text` should use `dw3_button_glyph`, not repeat `0x1B/0x2F/0x32/0x34`;
  5) `dw3_feat_icons` is checked in `dw3_button_glyph` and in `src/main/message.c`, but the battle menu ignores it (draws the letter even with `DW3_FEAT_ICONS=0`): pick one behaviour and apply to both; 6) the pad is
  read by two paths in the battle menu (`port_battle_menu_pressed` with `dw3_raw_pressed`, and `pad_state.get_pressed`): keep one without changing input behaviour; 7) `dw3_raw_buttons` is only written (`src/main/pad.c`):
  confirm with grep and remove if dead; 8) review the long comments of the `pad.map` and battle_menu blocks; 9) `handheld/dw3-menu.py`: read it whole and simplify the double place translation (`PLACE_TO_GAME`,
  `PLACE_FROM_GAME`, `PLACE_DISPLAY`); 10) sweep the other mods of `game_mods.c` (`skip_dialogues`, `global_save`, `led_bar`, fast travel, objective) and the tests and list what is redundant before touching it;
  11) cross-build ARM, install on the console and ask the owner to retest before committing. Out of scope: the inputs themselves (the owner considers them perfect) and upstream code.
- **C launcher design record (plan 13, built; the wireframes are not reproduced).** Goal: replace `handheld/dw3-menu.py` and unify entry for handhelds, Linux PC and Windows with a C99 launcher (< 2 MB, < 20 MB RAM,
  instant) with an Xbox-Classic x PortMaster x Digital World look: one stylised Home (no `cover.png`), no `VIDEO`, `SAVE ANYWHERE`, `CONTROL STYLE`, `BATTLE MENU: ENABLED ALWAYS` rows; everything of combat in `BATTLE HUD`
  (including the 7th button INSPECT), `BATTLE TEXT` fixed at SKIP, the modern Circle <-> Triangle rotation as the single default in `CONTROLS`. Palette (`launcher/ui.h`, 640x480 base): background `#071019` -> `#0C1B2A`
  gradient with a subtle grid `#10283A`, panels `#102436` with edge `#1F435E` and 8 px radius, focus band `#15394A` with a green/cyan side bar `#40FF80` / `#00E5C3` and gold text `#FFCC33`, ON `#40FF80`, off `#6E8494`,
  conflict/duplicate/incompatible `#FF4D4D`, text `#E6F1F5`, notes `#8BA3B5`; one geometric TTF through `stb_truetype.h` (no SDL_ttf). Rules of `BATTLE HUD`/`CONTROLS`: `inspect` is a real option (`PORT_MOD_MAX_OPTIONS` 8 -> 12, `mod.json`,
  `game_mods.c`, fallback `L2 -> R2 -> ...` for an old `settings.json`); no silent auto-swap (a duplicate is allowed while editing), conflicting rows and cross cells turn red with an alert icon and `B`/`START` neither
  leave nor save while a duplicate exists (footer warning), a forced exit (`Select+Start` or closing the window) discards the edits without writing `settings.json`; `CONTROLS`: `pad.map` is born with Circle <-> Triangle
  rotated together with the glyphs (`CONFIRM` = game action `cross` -> physical SOUTH, `BACK` = `triangle` -> EAST so every in-game "back" shows `B` or circle, never triangle, `MENU` = `circle` -> NORTH, `CARD` = `square` -> WEST),
  `BUTTON GLYPHS` cycles XBOX (A/B/X/Y), NINTENDO (B/A/Y/X), PS1, CUSTOM. `LED BAR`: detects XF35H (`/proc/device-tree/model`, `/sys/firmware/devicetree/base/model` and `/dev/ttyS2`); compatible: first-time install
  notice then ON/off; incompatible: a hardware warning and **two sequential confirmations** before continuing. Modals `RESET SETTINGS?` and `CLEAN LOGS AND DUMPS?`; `DEBUG`: LOGS off/soft/heavy, SAVE LOGS (zip), CLEAN LOGS.
  Loading screen: the **Trio Runner**, D-Tector-style 1-bit sprites with coloured outline: Kumamon (24x24 at 3x, cyan `#00C3AF`; idle, step, jump), Guilmon (24x24 at 3x, orange `#FF8C40`), Agumon (16x16 at 4x so it
  matches the others' height, gold `#FFCC33`); one jump button each (Left/West = Kumamon, Up/North = Guilmon, Right/East = Agumon), digital obstacles coming right to left at growing speed, a collision flashes red and
  resets the SCORE (keeps the BEST) without ever slowing the real loading; the launcher hands the screen to the game when `port: start:` arrives and shows the error instead of closing mute if `dw2003` fails
  at boot. Architecture suggested: `ui_draw` (RGBA buffer, rounded rectangles, gradients, alpha, stb_truetype, stb_image, 1-bit blitter), `loading_game`, `platform_fbdev.c` (handhelds: `/dev/fb0` + `/dev/input/event*`),
  `platform_sdl2.c` (Linux PC, Windows, Wayland/KMS CFWs), `config.c` (atomic `settings.json`/`pad.map` via `psxstack/runtime/json.c`, drafts in memory until Save & Back or Play), `runner.c` (spawns `./dw2003` with a pipe);
  `Digimon World 3.sh` still detects the environment (`control.txt`, `fbdev` vs `sdl2`, cpu_part 0xd04 -> gles) and calls `dw3-launcher`. The ARM test console used was an XF35H on EmuELEC (address in the owner's notes).
  Icons planned (`assets/icons/`): digi_crest, play, gamepad_gear, crosshair_hud, dpad, led_wave, bug_wrench, reset_arrow; face_a/b/x/y, ps_cross/circle/square/triangle, l1/r1/l2/r2, start, select, dpad_lr;
  warn_triangle, check_on, fast_forward, eye_inspect, sword_fight, bolt_tech, dna_dv, arrows_tag, potion_item, wing_run, obstacle_spike. `tools/gen_assets.py` makes `launcher/assets/assets.c`.

## Texts and translation (EN / PT-BR)

- Overrides live in `tools/text_override/en.txt` and `pt.txt`: lines `FILE index: text`, `FILE:sub index: text`, and
  `$NAME: text` for the port's own strings. `tools/gen_text_override.py` (with `--check`) generates
  `port/game/definitive_text.c` and `definitive_text_pt.inc`. Technique descriptions are limited to 34 characters
  (`tools/gen_tech_text.py`, `gen_affinity_text.py`).
- Fonts: the 11px font has no `/ : , '` and no accents (the 14px one is similar). A speech box holds 3 lines (split with
  `<2,2><3>`).
- **PT-BR is a 6th language**, falling back to English per entry (`port_language_ptbr`; `records_language` stays 2). The
  other languages (FR/IT/DE/ES) show the disc's text for the original entries and English for port-owned ones (the
  entries in `dw3_velvet_text_own`). The EU disc already has the original campaign in FR/DE/IT/ES; only the definitive
  delta (the guide, bestiary, hunt, forge, Black Market, DRI, `SKLINF`, Velvet Room, Super Rare screens) is missing there.
- **The story rewrite** (`en_story.txt`, `en_kaili.txt`, `pt_kaili.txt`, `en_choices.txt`, `en_echoes.txt`, generated into
  `port/game/definitive_story_text.c`, checked by `tools/check_story_text.py`) is English only; formats, counts and the
  lookup order are in [The story layer](#the-story-layer-velvet-room-in-the-main-story).
- PT pipeline: `tools/translate_ptbr.py` talks to a local Gemma server (no sampling parameters sent; `--benchmark` compares
  reasoning low vs medium on 20 real strings; `--tier p0|all`; `--export`). Glossary: keep in English `WEAK`, `RESIST`,
  `OVERFINISH`, `Baton Pass`, `Relay`, `Libra`, `Digimon`, `Digi-Lab`, `Load Tech`, `Blast Gauge`, `Bits`, `TP`, `MP`, `HP`
  and proper names; use Fogo, Agua, Gelo, Vento, Trovao, Maquina, Trevas for elements. A cache with tag/width validation lives
  under `.dumps/tasks/translation/`. Plan for the other four languages: the same tool over the ~250 lines of `en.txt`.
- Glyph fix (plan 9): despite the name, it is the **button-glyph/place translation bug** of the battle menu (`PLACE_TO_GAME`), not a font fix; the cause, the fix and the 11 simplification items (done status
  unverified) are in [Battle HUD: design record](#battle-hud-design-record-plan-8-the-opt-in-mod-that-became-always-on-and-the-glyph-fix-plan-9).
- **PT-BR pipeline spec (plan 15).** Goal: PT-BR as a 6th selectable language in the launcher and the handheld menu (loads the disc's English base and applies the `pt.txt` override with automatic English fallback);
  `portuguese` is a value of `preset_language` (`mod.json`, `game_mods.c`) and of the language selector of the C launcher (`launcher/cfg.c`, `launcher/ui.c`); with it active `records_language` stays `2` (the `ES*.BIN`
  English files) and `port_language_ptbr = 1`; `def_text_override(data, i)` and the `def_str_*` strings return the PT entry of `definitive_text_pt` (binary search by `(file_id, subtable, index)`) with fallback to English.
  Accents: the European font has the `01 xx` slots (`0x3B..0x72`: a with acute, e acute, i acute, o acute, u acute, a circumflex, e circumflex, o circumflex, a grave, c cedilla, u diaeresis, capital E/I/O/U acute); the plan said to
  check `ã`/`õ` and, if the texture lacked them, inject the glyphs in RAM under `PC_PORT` so the accent is never replaced by a plain `a` (whether this was done: unverified; the current texts avoid accents in the small fonts).
  `tools/translate_ptbr.py` calls the local Gemma 4 server directly (about 150 tok/s) **without sending temperature, top_p, top_k or repetition_penalty**; a mini-benchmark on 20 real strings (short 34-char descriptions, menus,
  Guide, `TALK*` dialogue with tags `<1>`, `<5,1>`) compares `reasoning_effort` low vs medium and picks the one with 100% respect for tags and size at the best throughput (`--benchmark`). Fixed glossary: keep in English WEAK,
  RESIST, OVERFINISH, Baton Pass, Relay, Libra, Digimon, Digi-Lab, Load Tech, Blast Gauge, Bits, TP, MP, HP, species and NPC names; standardise elements (Fogo, Agua, Gelo, Vento, Trovao, Maquina, Trevas), stats (Forca, Defesa,
  Espirito, Sabedoria, Velocidade) and the lean SKLINF pattern with turns ("Atq+2 no usuario, 3 turnos.", "Def-2 no alvo, 3 turnos.", "Magia de Fogo. Causa Veneno."). Incremental cache `.dumps/tasks/translation/cache_ptbr.jsonl`
  deduplicating strings repeated across maps; automatic per-line validation: exact preservation of every control code (`<1>`, `<2,...>`, `<5,1>`, `\n`, variables, colour codes), strict character/pixel limit per window (34 chars in
  SKLINF, pixel width from the font metrics for `TALK*`/`DMG*` and menus), auto-retry with size feedback on overflow. Execution order planned: the buff/debuff SKLINF batch was held until the tier work finished.
  **FR/DE/ES/IT:** the EU disc already has the original campaign in French (`records_language` 3), Italian (4), German (5) and Spanish (6); only the Definitive delta is missing (the guide, quests, Asuka auctions, Bestiary, hunt,
  forge, Black Market, DRI, new SKLINF, about 250 lines of `en.txt` plus the `def_str_*`); the hook planned: `def_text_override` and `tools/gen_text_override.py` accepting `tools/text_override/{fr,de,es,it}.txt` and per-language
  `def_str_*` tables, then `tools/translate_ptbr.py --lang fr,de,es,it --only-definitive-delta` (extract only the strings the Definitive Edition added or changed, via git diff; **the `--lang` and `--only-definitive-delta` options do not exist yet**: `tools/translate_ptbr.py` today has `--url`, `--tier`, `--batch-size`, `--export`, `--benchmark`, and `tools/text_override/` holds only `en.txt` and `pt.txt`). The plan said PT-BR first so the owner can play and validate, then the delta for the other four.

## Save format (memory card records)

The card file is 0x8000 bytes.

| What | Where | Notes |
|---|---|---|
| Record 1 (BEST) | 0x7800 + slot*0x100, part 5, "BEST", version 1 | XOR checksum over the 193-byte species table; `REC_EXTRA` at 0xD0, 48 bytes, marker "X1" |
| Record 2 (HUNT) | 0x7B00, part 6, "HUNT" | rare items, DRI, Blast data, Blast boost, story flags and answers, skin, Super Rare state |
| Record 3 (BLST) | 0x7E00 + slot*0x80, part 7, "BLST" | up to 10 partner+form entries of 11 bytes: partner, step, 5 stats, hp, best-stat index, best value |
| Slot tail | Blast gauges (8 x s16) at slot+0x26F0; MP/TP converter marker at 0x26EC | "MPV1" is MP only, "DFV2" is MP and TP |

STGMCARD steps: 0x48 read and 0x36 write for record 1; 0x49 read and 0x37 write for record 2; 0x4A read and 0x38 write
for record 3. The slot size is 0x2700 (`memcard_init`, `src/main/memcard.c`), so three slots fit in 0x7800 and the card's
last 0x800 bytes hold eight 0x100 records the game never writes.

`REC_EXTRA` (record 1, offsets): 0-1 "X1"; 2 flags (`DEF_SAVE_HARD` 1, `DEF_SAVE_KAILI` 2); 4-7 Guardromon network mask; 8
region_area; 9 region_chain; 10-25 sixteen common materials (u8, cap 99); 26-27 replica bits (16); 28-29 and 30-33 DRI
unlocked/best stage per one PRD (see the DRI note above).

Record 2: 128 rare-item slots (cap 99) plus "ever held" bits; DRI bytes 152-153 unlocked and 154-157 best stage (2 bits
each); the Blast data (next step per partner `REC2_BLAST`, owed TP, quest stage, purified bits, the chosen partner and
charge); then, still in the Blast block (`REC2_BLAST_QUEST` + 5..8), `blast_boost` (the boost steps bought), `story_flags`
(u16: the story hooks' bits 0..7, `DEF_STORY_*`, and the Velvet Room's level last greeted in bits 12..14) and `story_gate`
(the Time Room's trials won in the run); at `REC2_ANSWERS` (= `REC2_BLAST_END`) the 40 bytes of story answers
(`DEF_STORY_ANSWERS` 160 x 2 bits, low bits first; see [The story layer](#the-story-layer-velvet-room-in-the-main-story)); the
skin byte (`REC2_SKIN`); and the last 24 bytes, `DEF_SUPER_RECORD`. Compile-time checks keep the Blast block before the Super
Rare bytes and the answers before the skin byte; an older record reads all of these as 0.

Rules for any new saved state: write only when something is non-zero; zero it in `def_bestiary_reset`; register it in
`def_bestiary_savestate` (`PORT_STATE_VAR`); validate on read (the extras have no checksum). In detail (PRD 4's
checklist): `def_bestiary_record_write` writes record 1, and its "X1" part, only when the table or `extra_any()` has
something, so a new `REC_EXTRA` field must enter `extra_any()`, or a save holding only that state loses it; the record is
zeroed before writing and read only when "X1" is present, so an old save reads zero for every new field and a save with
nothing gained stays byte for byte the same; `out[5]` (the XOR checksum) covers only the species table, so clamp on
read (a material count past `DEF_HUNT_MATERIAL_CAP` reads as the cap); never grow `REC_EXTRA`: add a record per slot
instead (record 2 "HUNT" and record 3 "BLST" did exactly that). Converter:
`tools/convert_save_mp.py card1.mcd` (`--dry-run`; writes "DFV2"). Tests: `tests/saves`, `tests/port/definitive_bestiary.py`.

## Building and running (the port)

- Host build: `cmake -S port -B build/port -G Ninja`, then `cmake --build build/port`; run
  `build/port/dw2003 --disc iso/dw2003.cue`. The SDL3 build: `scripts/setup.sh sdl3 dxc` plus `-DPSXSTACK_SDL=ON`.
- Other builds: `tools/build_arm.sh` (since commit `8373ea6`, 2026-11-14, it builds **both armhf and aarch64**: `DW3_ARCH=armhf|aarch64` for one; Docker images from `tools/Dockerfile.armhf`
  and `tools/Dockerfile.aarch64`, both Debian 10 against glibc 2.28; toolchains `cmake/armhf-linux.cmake` (`-mcpu=cortex-a35`) and `cmake/aarch64-linux.cmake`; trees `build/armhf-sdl2`, `build/aarch64-sdl2`, each with
  `dw2003` + `dw3-launcher`; it assembles a multi-arch package directory `../portmaster` with `dw2003.armhf/.aarch64`, `dw3-launcher.armhf/.aarch64`, `handheld/` and the mods, no symlinks so FAT32/exFAT cards work, override
  with `DW3_PACKAGE_DIR`; do not pipe the script through `head`, it stops on SIGPIPE before copying the binary and the next deploy would silently ship the previous one), `portmaster/make_release.sh` (needs both the 32-bit ARM and the 64-bit
  aarch64 ELFs, `build/armhf-sdl2/` and `build/aarch64-sdl2/`), `scripts/package_appimage.sh`, `scripts/build_windows.sh`, `tools/build_android.sh`.
- `port/CMakeLists.txt` keeps the game's C at `-O0`: `-O2` crashes at boot (`sound_init` -> `sound_update_loading` ->
  `cdload_get_subfile`, SIGSEGV). It also holds the psxstack workaround block.
- Link failures on `__start_dw2003_data_wstagNNN`: see the Testing note under Super Rare hunts.

### Command line options

`--config`, `--print-settings`, `--print-mods`, `--script-mods`, `--refresh 50|60`, `--disc` / `--game`, `--no-disc-check`,
`--cd-speed`, `--memcard1|2`, `--script`, `--log`, `--record`, `--max-frames`, `--watchdog`, `--trace`, `--window`, `--scale`,
`--fullscreen`, `--fps`, `--input-test`, `--screenshot`, `--renderer software|gpu|gles`, `--internal-scale`,
`--gpu-screenshot`, `--subpixel`, `--filter`, `--dump-textures`, `--texture-pack`, `--mods-dir`, `--spu-trace`, `--wav`,
`--mute`, `--debug SOCKET`, `--debug-hold`, `--save-state W:F`, `--save-state-exit`, `--load-state`, `--crash-dir`,
`--version`. The window's keys are in `--help`.

### Environment variables

Definitive layers: `DW3_DEFINITIVE`, `DW3_BESTIARY`, `DW3_GUIDE`, `DW3_GUIDE_CHECK`, `DW3_GIRLS`, `DW3_GIRLS_DUMP`,
`DW3_GIRLS_NUMBERS`, `DW3_RUSH_FLOAT`, `DW3_RUSH_TEST`, `DW3_BLAST_STEP`, `DW3_BLAST_FORM`, `DW3_BATTLE_LOG`,
`DW3_FEAT_VELVET`, `DW3_FEAT_TRAVEL`, `DW3_FEAT_ICONS`, `DW3_FEAT_OBJECTIVE`.
Video and input: `DW3_VIDEO`, `DW3_FORCE_VIDEO`, `DW3_FORCE_RENDERER`, `DW3_DEFAULT_RENDERER`, `DW3_UI_SCALE`, `DW3_PAD`,
`DW3_PAD_REMAP`, `DW3_FB`, `DW3_NO_NEON_FLAT`, `DW3_NO_NEON_GOURAUD`.
Logging and debug: `DW3_LOG`, `DW3_PHASE`, `DW3_PRESENT_LOG`, `DW3_GPU_LOG`, `DW3_STATE_ANY`, `DW3_STATUS_FD`,
`DW3_PORT_CRASH_AT`, `DW3_PC`, `DW3_EXEC`, `DW3_NOEXEC`, `DW3_GAME_BIN`, and `DW3_GLES_CHECK`, `_DUMP`, `_DUMP_FRAME`, `_LOG`,
`_NOFLAT`, `_NOTEX`, `_REPLACE`, `_TRACE`.
Other: `DW3_LANG`, `DW3_SYNC_URL`, `DW3_CLOUD_CURL`, `DW3_LED_DIR`, `DW3_LED_FORCE`, `DW3_PORT_SKIP_DIALOGUES`,
`DW3_PORT_FAST_FORWARD`, `DW3_PORTMASTER`, `CFW_NAME`, `RECOMPLAUNCHER` / `RECOMPLAUNCHER_MODE`, `SDL_VIDEODRIVER`.

## Mods (`port/mods/`)

`battle_animations`, `battle_menu` (always on), `battle_text` (default skip), `led_bar` (RK3326 handhelds only),
`preset_language`, `skip_dialogues` (F2), `widescreen` (0.2, `field` option); `fast_forward` is the runtime's. Full
reference in [`docs/LAUNCHER.md`](docs/LAUNCHER.md). The `battle_menu`, 50/60 Hz and logs notes there are the source for
the HUD section above.

## Tools (`tools/`)

- Text: `gen_text_override.py` (`--check`), `gen_tech_text.py`, `gen_affinity_text.py`, `translate_ptbr.py`, `dump_text.py`.
- Data generators (all `--check`, some `--apply`): `gen_enemy_weaknesses.py` (`--apply`), `gen_hunt_targets.py` (`--apply`),
  `gen_map_graph.py` (`--apply`), `gen_recipes.py`, `gen_dri_challenges.py`, `gen_replicas.py`, `gen_item_prices.py`,
  `gen_equipment.py`.
- Saves: `convert_save_mp.py`. Disc and finding things: `disc_files.py`, `disc_survey.py`, `find_dri_agents.py`,
  `bm_npc_candidates.py`.
- Girls Power: `girls_power_draw.py`, `girls_power_preview.py`.
- Port and decompilation: `hacks.py`, `port_inventory.py`, `port_gen.py`, `port_inputs.py`, `flag_census.py`,
  `progress.py`, `psyq_match.py`, `unit_diff.py`, `game_xref.py`, `data_owners.py`, `data_to_c.py`, `coverage.py`,
  `overlay_layout.py`, `overlay_xref.py`, `ref_names.py`, `renames.py`, `split_case_sizes.py`, `wstag_data.py`,
  `wstag_groups.py`, `compiler_id.py`, `mcp_game.py`.

## Tests

`scripts/test.sh` runs layers: `census` (`tools/hacks.py --check`), `probe` (`tools/port_inventory.py probe/link`), `1`
(`tests/golden`, `tests/host` replay, `tests/spu`, `tests/xa`), `2` (`tests/replay` emulator, `tests/sound`), `3`
(`tests/formats`, `tests/saves`), `port` and `mods` (`tests/port/mods.py`). Flags: `--build`, `--layer`, `--m32`,
`--no-probe`; `DW3_JOBS`.

The `port` layer runs, in order: `run.py`, `settings.py`, `render_gpu.py`, `render_filters.py`, `textures.py`,
`tests/host/gpu_hw_replay.py`, `hz60.py`, `battle.py`, `text_override.py`, `definitive.py`, `definitive_buffs.py`,
`definitive_mp.py`, `definitive_bestiary.py`, `definitive_hunt.py`, `definitive_superhunt.py`, `definitive_skins.py`,
`definitive_tp.py`, `definitive_pity.py`, `definitive_gauges.py`, `definitive_evo_anim.py`, `definitive_train_loss.py`,
`girls_power.py`, `vram.py`, `subpixel_jitter.py`, `debug.py`, `savestate.py`, `crash.py`, `field.py`.
`definitive_dri.py`, `definitive_market.py`, `definitive_economy.py`, `definitive_variants.py`, `definitive_quest.py` and
`definitive_rush.py` also exist but are **not registered in `scripts/test.sh`** (checked 2026-11-17): run them by hand
(`uv run python tests/port/<name>.py`); `definitive_dri.py` and `definitive_market.py` passed on 2026-11-17.
Known small fix: `docs/MECHANICS.md` has a wrong crit range at +148%; `definitive_ui.png` needs a new capture.

## Debugging and the MCP

`--debug SOCKET` opens a JSON channel; the MCP server `mcp__dw2003__*` (`game_start`, `pad_press`, `mem_read`/`mem_write` by
symbol or `ps1:` address, `screenshot`, `state_save`/`state_load`, `wait_map`, ...) drives it (`tools/mcp_game.py`;
`tests/port/debug.py`). `DW3_BATTLE_LOG` logs every battle roll. A crash writes a report (`--crash-dir`, `tests/port/crash.py`).
Savestates are valid only for the binary that saved them.

## Launcher, cloud sync, handhelds, Android, PortMaster

- **C launcher** (`launcher/`, `dw3-launcher`, C99): 9 screens: Home, GAME, BATTLE HUD, CONTROLS, DEBUG, LED BAR (with two
  confirmations on incompatible hardware), RESET/CLEAN confirmations, and the Trio Runner loading game (jump: Left =
  Kumamon, Up = Guilmon, Right = Agumon). Colour palette and duplicate-button blocking are documented in the closed plan 13
  (now here). It starts the game with a `port: start:` handoff.
- **Cloud sync** (`cloud-sync/`, a Cloudflare Worker, TypeScript): routes `/v1/health`, `/v1/register`, `/v1/pair/*`
  (create, claim, poll, status, approve), `/v1/me`, `/v1/devices` and `/v1/devices/revoke`, saves, and admin ban/cleanup;
  PS1 card validation (0x20000 bytes) and rate limits; tests in `cloud-sync/test/`; schema `cloud-sync/schema.sql`.
  `DW3_SYNC_URL`, `DW3_CLOUD_CURL`.
- **Android** (`android/`, `tools/build_android.sh`): `arm64-v8a`, SDL3 GPU, a touch pad (`android/touch_pad.c`), SAF disc
  picker, `FLOATING PAD` toggle. The original design (a Kotlin overlay, Fase 0 audit of `psxstack` for the NDK) is in the
  backlog list below.
- **Handhelds / PortMaster:** entry `handheld/Digimon World 3.sh` (the old `dw3-menu.py` menu was removed in `cbf34ce`; the C launcher `dw3-launcher` replaced it), `portmaster/` (`port.json`
  arch `["armhf","aarch64"]`), docs `docs/HANDHELD.md`, `docs/XF35H.md`. Probe with `Debug logs: heavy` and `logs/probe.txt`.
- **Console deploy handoff (ssh to a handheld, procedure of 2026-11-12; it predates the multi-arch package: the binaries are now `dw2003.armhf` / `dw2003.aarch64` and `dw3-launcher.*`, see `docs/HANDHELD.md`):** the owner supplies `<USER>@<IP>`, the password or key and the PortMaster folder
  (usually `/roms/ports`, or `/roms2/ports`, `/storage/roms/ports`; find it with `ssh <console> 'ls -d /roms*/ports /storage/roms/ports 2>/dev/null'`); `PORTS` below.
  Source is `/home/alex/git/my/dw3/portmaster/` (the device mirror `tools/build_arm.sh` writes the new build into): copy `dw3/dw2003` (the 32-bit ARM binary; check `file` says
  `ELF 32-bit LSB ... ARM` and the date is current, else run `tools/build_arm.sh`, Docker image `dw3-armhf-builder`), `dw3/mods/`, and only if changed `ledd.py`, `probe.sh`,
  `README.txt` and `Digimon World 3.sh`. **Never overwrite or delete:** `PORTS/dw3/game/` (the player's disc), `card1.mcd`, `card2.mcd`, `settings.json`, `pad.map`, `tui.state`, `logs/`,
  `log.txt`, `menu.log`; no `rsync --delete`, no `scp -r` of the whole folder. **Step 1, mandatory backup:** create `PORTS/dw3-backup-<date>` with `card1.mcd`, `card2.mcd`, `settings.json`,
  `pad.map`, `tui.state`, `dw2003.old`, `mods.old`, the `.sh`; then bring a copy of the saves to `/home/alex/git/my/dw3/backups-console/` and check they exist and are not empty (`card1.mcd`
  should be 131072 bytes; if the player never saved, tell the owner and go on). **Step 2:** `scp` the binary and the mods, `chmod +x`, compare `sha1sum` local and remote; the mods `party_xp`, `xp_boost`
  and `evo_buff` no longer exist (rules of the game or removed): **move** the old folders into the backup, do not delete; the launcher also changed (no XP screen, no EVO BUFF); an old
  `settings.json` with those keys keeps working. **Step 3, old saves and MP (the owner's decision, never alone):** the game does not migrate old saves (they open with the old MP); to convert,
  run `tools/convert_save_mp.py --dry-run` then the real one **always on the local copy** in `backups-console/`, only if the owner says so, and only then `scp` the card back after the step-1
  backup. The card gained a bestiary record at the file's tail (last 0x800 bytes, never used by the game): an old save opens with everything "???". **Step 4, verify:** `ls -la PORTS/dw3/`, ask the owner to
  open the game, read `PORTS/dw3/log.txt` and `logs/` (`tail -n 40`) and report an ARM library error, `fatal` or `crash`; to roll back restore `dw2003.old`, `mods.old` and the `.mcd`s from the backup.
  What the owner would see different at the time (to tell bug from intent): the new battle HUD always (7th button Inspect on L2, Start opens the enemy's memory), the player always opens the fight, a
  weakness with an elemental technique staggers and gives an extra action, and enemies kept the disc's resistance values until the owner approved `.dumps/tasks/persona/fraquezas.md`.
- **PortMaster CFW research (plan 11):** muOS 32-bit and TrimUI are **unverified**; see "PortMaster CFW research" under [History](#history--decisions-log) for the plan and the partial results.
- **CI and release (GitHub Actions, state 2026-11-13).** Workflows `.github/workflows/ci.yml` (Linux and Windows; ignores pushes that only touch `docs/**`, `**/*.md`, `LICENSE`, `.gitignore`) and
  `release.yml` (release by tag); `docs/RELEASE.md` has the flow: `git tag v0.1.0; and git push origin v0.1.0` (a suffix such as `v0.1.0-rc1` is a pre-release). At the time of the handoff the push of
  `2d0f647` had triggered **no workflow** (0 runs in total although Actions was enabled; maybe GitHub delay) and both were dispatched by hand (`workflow_dispatch`; the release without a tag only builds
  artifacts, the AppImage and the Windows zip); to check: `gh run watch <id> -R al4xdev/dw3 --exit-status`, `gh run view <id> -R al4xdev/dw3 --log-failed`, `gh run list -R al4xdev/dw3` (the next push to `main`
  touching code should start CI by itself, event `push`; if not, Settings -> Actions -> General). No secrets or variables were configured: `GAMEDATA_DEPLOY_KEY` (optional secret: with it CI and release run
  the tests with the disc; without it only the smoke tests without a disc), `PSXSTACK_TOKEN` not needed (`al4xdev/dw3-psxstack` is public), `RELEASE_DRAFT=true` (variable) before the first tag if the release
  should be a draft. (`gh` is not installed on this machine, so those commands are for wherever it is.) The `main` of GitHub had received a force-push (`841521a` -> `ca5867d`) before `2d0f647`; `ca5867d` was
  already in the local history so the push was a fast-forward that deleted nothing; if `841521a` held something, it is not on `main`.
- **The psxstack pin fix (still pending).** The pin (`5e88c08` of `dw3-handheld` in `al4xdev/dw3-psxstack`) does not build the PC targets as is, hence the workaround block in `port/CMakeLists.txt`
  (three things: `-Wno-error=unused-variable` in `runtime/video.c` without EGL, `fbdev_present.c` out of the build without SDL2, an empty `sys/syscall.h` on Windows); the lab's build (SDL2 and EGL off)
  passes through it and the tests pass. The real fix is this patch **in the psxstack repo** (`dw3-handheld`), then update the pin and remove the block. Before applying, check what the patch assumes: it takes
  `fbdev_present.c` and `render_gles.c` out of the runtime glob and says they come back with `PSXSTACK_SDL2`/`PSXSTACK_EGL` further down in `psxstack.cmake`; the patch does not show that part and it was **not verified**;
  if those options do not add the files back the handheld build loses its presenter. The patch (a copy was in `.dumps/tasks/ci/psxstack-pc-build.patch`):
  ```diff
  --- a/cmake/psxstack.cmake
  +++ b/cmake/psxstack.cmake
  @@ -293,6 +293,9 @@
   
       # ---- The binary: the runtime, the adapter, the tables, the shim, the units (and on PE the bracket symbols).
       file(GLOB RUNTIME_SRCS CONFIGURE_DEPENDS "${PSXSTACK_ROOT}/runtime/*.c")
  +    # The handheld's presenter and renderer join the build with their options (PSXSTACK_SDL2, PSXSTACK_EGL, below):
  +    # fbdev_present.c includes <linux/fb.h> and the Linux ioctls, which break the Windows build.
  +    list(FILTER RUNTIME_SRCS EXCLUDE REGEX "/runtime/(fbdev_present|render_gles)\\.c$")
       set(PLATFORM_SRCS "")
       if(WIN32)
           _psxstack_run("${PSXSTACK_PORT_GEN}" markers --id "${ID}" --overlays "${G_OVERLAYS}" --out "${GEN}/markers.c")
  --- a/runtime/main.c
  +++ b/runtime/main.c
  @@ -4,7 +4,9 @@
   #include <errno.h>
   #include <stdlib.h>
   #include <string.h>
  +#ifdef __linux__ /* SYS_sched_setaffinity (below); Windows has no such header */
   #include <sys/syscall.h>
  +#endif
   #include <unistd.h>
   
   #include "platform.h"
  --- a/runtime/video.c
  +++ b/runtime/video.c
  @@ -76,7 +76,9 @@
   static int video_converted_frame = -1; /* the frame video_pixels was converted at (the debug channel's screenshot) */
   static int video_gpu_wanted;            /* --renderer gpu / video.renderer "gpu" */
   static int video_gles_wanted;           /* --renderer gles: the offscreen GLES renderer (render_gles.c) */
  +#ifdef PSXSTACK_EGL /* only the GLES paths read it: unused (-Werror) in the other builds */
   static int video_gles;                  /* the offscreen GLES renderer draws; the present still comes out through fbdev */
  +#endif
   static int video_internal_scale = 1;    /* --internal-scale / video.internal_scale: the rasteriser's, 1..8 */
   static PortFilter video_filter;         /* --filter / video.filter: the hardware renderer's present (render_gpu_present.c) */
   static struct {
  ```
  (hunk line counts and context are as in the original; whitespace-only context lines may be off by trailing spaces.)
- **Widescreen field (handoff note, since superseded on PC):** the battle mod existed (`port/mods/widescreen` 0.1) and shipped in both packages; the field mod 0.2 of the original fork (`7c47270`)
  needed runtime changes the pin did not have, which first had to enter `dw3-psxstack`. The README above says PC now has 16:9 in battles and the field; whether the handheld package carries the
  field part is **unverified**.

### Performance and what failed (closed plans 1-4, summary)

The R36S/XF35H port, the GLES backend (41.85 fps against software 37.70 in battle; 23.9 ms a frame, present about 3.2 ms,
0.7% VRAM difference), SPU/NEON audio (4.9 ms down to 3.0-3.3 ms), environment detection and the single PortMaster launcher
were done in the closed plans; their conclusions are in `docs/HANDHELD.md`, and the plans' own account of what failed or was left
undone is in the [History](#history--decisions-log) section below. Widescreen field: see `docs/LAUNCHER.md`.
Handoff notes: GitHub Actions / CI and the psxstack pin fix are under "CI and release" above; the first release goes out with a `v0.1.0` tag.

## Planned / backlog

Not started or only staged unless the entry says otherwise. Each entry says where its detail lives.

- **muOS aarch64 (staging 10):** the build exists since `8373ea6` (see the History entry for the whole plan and the tester's data); on-device result unverified.
- **Android (staging 14):** the code exists (`android/`, `tools/build_android.sh`, `android/touch_pad.c`; the README line above), the plan's phase gates are the open part: the Phase 0 audit of `psxstack` for the NDK/Bionic,
  the "Show Touch Controls" toggle only in the Android build, SAF disc import, and **a test on a real Android device** (`adb install`, `adb logcat`: boot, disc selection, entering the game, battle with Vulkan + widescreen,
  the toggle). Whole plan in History.
- **FR/DE/ES/IT delta of the Definitive text (staging 15):** about 250 lines of `en.txt` plus the `def_str_*` strings; needs per-language override files and the tool options that do not exist yet (Texts above).
- **PortMaster submission:** the package is checked (`portmaster/make_release.sh`, `dist/dw3.zip`) but **never submitted**: it needs a Discord testing thread and tests on CFWs this machine is not; publishing is the owner's call (plan 1).
- **Telemetry Harness and Plugin Gatekeeper (staging 16):** a separate PRD v0.1 draft, unrelated to the game; the whole PRD is condensed in History.
- **Combat ideas never done:** the Layer 6 technique candidates (Combat design record); the Super Hard rebalance by the owner.
- **Onboarding hooks in the main story** (the Velvet Room "section 6": a forced Guardromon interaction after the second teleport point; steering the player through a wardrobe door when rare materials start to appear): out of scope, the owner is rewriting the main quest elsewhere and will make guide quests.
- **Skin ideas:** a price (free or one crystal), a 3D preview; Digimon skins were dropped. (The Velvet Room's hand-drawn door sprites were dropped: the owner vetoed drawn doors on 2026-11-18; the ways in are lights on each inn's scenery.)
- **Velvet Room / story ideas:** Igor's own figure (his head is drawn; the body is still the game's actor 109, "until his own art is drawn" in `definitive_velvet.c`); a drawn figure for the mentor (the old man `0x39` "until one is drawn"); the Adventure tune (`tools/music_lab/`, a first pass) is not tied to any cue.

## Pending work / open decisions

Every item the owner wanted or planned that is not done yet, with its source plan (the plan files may be deleted; the names are kept for reference). Nothing here is in the game unless it says "partly".

**Hunt, Super Rare, skins**
- **Commit**: done. The Super Rare front, the skins, the recolour, the purify and the map graph are in `8c0971b`; the
  regional forge, the replica Cores and the skins page in `eb2fa36` and `89c46af`; the Black Market UI pass in `def4d30`;
  the Elemental weapons and Trade-off pieces in `11d9e54` and `188618a` (git checked 2026-11-18: nothing of these is
  untracked).
- **A boss repeated across regions** (the owner's 2026-11-17 answer): if the target table ever gives two regions the same
  species, the recolour must turn the hue per region (see Recolour); not needed with today's 71 distinct species.
- **Check in game**: done 2026-11-17 (Super Rare colour, compass outside the territory, MATERIALS cut names EN/PT, PT texts); the 50% flee is host-tested only.
- **Sepikmon's position** in the room: done; moved on 2026-11-18 (owner) upstairs to (86,214), dir 7, left of the Demon Bell's keeper (it was (176,306)).
- **Skins choice screen** (`skins-npc.md`): done 2026-11-17 as the Sepikmon board's R1 page (no NPC of its own); the reward panel's "New skin: <name>" line is done (2026-11-17, seen on screen); tested with Kail (2026-11-17); skins on portraits, menus and the name/card screen dropped by the owner.

**Velvet Room** (`velvet-room-plan.md`)
- Done since the plan: the door sprite was dropped (the owner vetoed drawn doors, 2026-11-18: the ways in are lights on each inn's scenery, entered with CIRCLE); the room's music is the owner's arrangement (bank 72, `8042491`); the room was arranged on 2026-11-18 (Sepikmon, Gennai, the pig).
- Open: the owner's look in game at each inn's light, first the six the judges left "divided" (0x238, 0x25B, 0x25D, 0x263, 0x269, 0x26F; the middle was kept); the mentor's approval or replacement (old man `0x39`, line provisional);
  FR/IT/DE/ES lines for `TALK05` 8, 9, 12, 13, 14, 15, 16..20, 47..65, 77, 78, 79, 81, 82, 99, `TALK06` 5..7, `TALK03` 311/312, `TALK01` 178 and the other port-owned texts (the story rewrite itself is English only); **a memory-card save inside the room** loads as the normal bar (`vl_room` is RAM; the Time Room's run is RAM too): acceptable or not is an open question;
  testing in game of the whole entry-talk-exit flow by the owner (the CIRCLE prompt was checked by script on 0x23F, 0x25D and 0x288).
- Texts that still speak of a "blue door" while the way in is now a light at Seiryu's inn's stage (210,300), not on the stairs: `$TomGone` ("that blue door on the stairs"), the Guide's `$Q4sT` ("a blue door opened on the stairs") and `$M4T` ("by an inn's blue door").
- **Not tested in a real game (story layer; the owner's tests are one English run):** the opening's cut and Tom's hook were tested by scripts (`.dumps/tasks/story/opening/`, `tom/tom_test.py`), not yet in that run; the Oikawa's arc (the hooded figure's sprite in VRAM place 6 of map 0x245 while actor 78 is left out, his spot (1778,502), the fight, a defeat and the market's opening); the Time Room (the way in at (160,440), the warnings, the trial areas' positions and sprites on 0x202, 0x26F and 0x258, the Myotismon/Gennai switch, a lost run's reset); Igor's dreams by their real triggers (Act I's flag 0x1C0E, Act II after the Bug Maze, the Oinkmon at step 0x26, Act III and the held Galacticmon battle; all were seen only through `DW3_DREAM` or `DW3_TIME_TEST`); the scenes' choices and echoes in a full English run.

**Blast / Purify / Chosen** (`velvet-room-plan.md`): the Chosen's break of the 999 stat cap is done (up to 9999) but never seen in a real fight; a real playtest of a full-gauge battle (gauge to 1000 by WEAK/stagger in a real fight was never seen); a final sound for the black screen (`PF_SND_CURSE` is provisional); the main-story onboarding
(out of scope).
**Super Hard** (`velvet-room-plan.md`): a real fight with the Bell was never run; balance of the x1.5 HP / x1.5 Speed / x2 Bits / x2 materials is the owner's.
**Combat balance** (`definitive-combat-prd.md`): layer 10 is the owner's, by playing; the guidance is in the Combat design record (boss HP first, then the magic clamp). Open design questions never answered: the Layer 6 technique candidates, the OVERFINISH 10% (may be lowered). (The PRD's "does the enemy get an extra action" is answered by the code: no, the chain is the player's only.)
**Girls Power** (`girls-power-kaili-prd.md`, `girls-power-pixel-art.md`): the owner's full start-to-end playtest and redoing frames they point at (kick, fishing, ladder, lying poses, deck and Digimon name screens, anywhere else Junior might appear); weak spots listed in the Girls Power section.
**Black Market / texts** (`black-market-ux.md`): PT names "Crest de Recuper" (ITMNAM 320, cut), "Pulse Guard", "Shard de Bits/Bytes"; the colon-as-"0" glyph defect of the 14 px font hits about 30 other port strings (a pass or a font glyph fix); the 11 px font lacks `/ : , '` and accents. The small font's green palette (4, "the bestiary's green") draws an `O` oddly (the Sepikmon board's DONE reads like "D3NE"); the owner likes the look (2026-11-17): kept, not a defect. Owner confirmations PRD 4 left open ("applied, the owner may change"): the trader (actor 42 on maps 0x20C / 0x27B, now also the Velvet Room), the 12-commons-per-rare rate, and the replica list and prices. Known limit: the replica bit clears only on the per-frame sweep (`def_bm_sweep`), so between a sale or discard and the sweep the shop pays 0 only for an ID no shop sells. The Portuguese of the forge's renamed pieces (Elemental weapons, Trade-off items 292/295/297/298) and of the eight DRI hint pages is the owner's: `pt.txt` still has the plain lines.
**Translation** (`staging/15`): FR/IT/DE/ES delta; `ã`/`õ` glyph injection check; the `--lang` and `--only-definitive-delta` tool options.
**Not tested in a real game** (`handoff-definitive-pendencias.md`, `definitive-hunt-economy-dri-prd.md`): (the DRI agent talk: seen 2026-11-17 at Johann, the quest at a party level sum of 87, the hand-in talk without the list and the next one with it; still open: the list's own screen after a real agent talk); a HUNT record
save and load on a card at a Guardromon save point; the forge requirement line on the real shop screen (and the refusal and max-buy limit, which have only unit tests); the Guide's arrival notice (the log shows it but a capture 30 frames later shows only the map title). Also from the PRDs and the DRI
task: of the five planned DRI agent checks only Johann's (quest line at sum 87, hand-in, list on the next talk) was seen; a party level sum
under 30, sum 60+, the DDNA poked into the bag, Agumon poked as joined, and a starter's agent (Ken on 0x258 room 1 with Kotemon joined)
are untested; that an ordinary NPC with an agent's sprite (43/44/163) on an agent's own map does not open the list (`dri_rookie` should
refuse it); the floating Rush's level-up stat numbers against the full report screen (same rule, different moment of the draw); combat
layers 2-4 and progression layers C, E, G, J were accepted by host tests and the owner's play, with no measured timing of the short
digivolution and no kept screenshot set (the "Enemy x2" indicator against an enemy that really acts twice included).
**Small fixes** (`handoff-definitive-pendencias.md`): the DRI victory debrief's reward line overflows the panel's right edge ("Metal Power 1 - Metal Power 2 - Metal Pow...", `definitive_dri_screen.c`, reward at +70: split in two lines or shorten); (done, checked 2026-11-17: `def_hunt_battle_end` now counts only result 1; `fightstg_events_end_battle` gives 1 when the enemy side falls, 2 when the party falls, 0 on flight); the floating Rush level-up stat rolls happen at a different moment from the original screen; `docs/images/definitive/definitive_ui.png` predates the per-family screens and needs a new capture; `docs/MECHANICS.md` has a wrong crit range (+148%), fix from the code.
**Tests not registered** (`scripts/test.sh`): `definitive_dri.py`, `definitive_market.py`, `definitive_economy.py`, `definitive_variants.py`, `definitive_quest.py`, `definitive_rush.py` (run by hand; see Tests).
**Launcher / battle menu** (`9-Commit...`, `8-Battle...`): the 11 simplification items of plan 9 (done status unverified; History); the `confusion_style` option never existed.
**CI / release / psxstack** (`handoff-definitive-pendencias.md`): confirm that a push to `main` starts CI by itself; set the optional secrets/variables if wanted (`GAMEDATA_DEPLOY_KEY`, `RELEASE_DRAFT`); push the psxstack patch to `dw3-handheld` then update the pin and drop the workaround block of `port/CMakeLists.txt`
(after verifying the `PSXSTACK_SDL2`/`PSXSTACK_EGL` assumption); the first release `v0.1.0` tag; the widescreen field mod for the handheld package.
**Platforms** (`staging/10`, `11`, `14`): see Planned / backlog; muOS/TrimUI/Android on-device results; the PortMaster CFW research matrix (History) is incomplete.
**Performance leftovers** (closed plans 2-4): the GLES present (about 3.2 ms a frame), the 0.7% VRAM difference and the 2D check at frame 1700, the decoded-texture cache, the SPU stages 2 and 5 (`aud` stayed 3.0-3.3 ms against a 2.5 ms target), the `--input-test` hang on the device (History).
**Housekeeping:** the owner will delete `.plan/`; everything in the plan files is meant to be in this README. `.plan/.state` was not read (internal state), and `.plan/para-o-dono/` does not exist.

## History / decisions log

Plans that are closed or only history, kept so the reasons survive. Portuguese originals are summarised in English; dates are the plans'.

### Closed plans 1-4 and 5-7: what was done, what failed (the plans' own post-mortems)

- **Plan 1, port plan for the R36S (written at the project's start, closed).** Guidelines: do not assume `/dev/fb0` is the best output (the Mali-G31 MP2 and the 640x480 panel; compare SDL2 + GLES through KMSDRM/EGL against a software rasteriser with direct presentation, decided by measurement);
  validate the memory architecture separately (keep the game's pointers 32-bit for the PS1 code and the 24-bit ordering-table tags; 16 MB is not a universal host limit: inspect `memory.c` and `fill.c`; keep armhf and aarch64-with-multilib in mind); postpone any SIGSEGV handler (fail loudly); audit `gpu_draw.c` against `gl.c`
  (does it produce the whole picture or only feed VRAM?). Milestones: M1 clean 32-bit Linux ARM build (Docker, `arm-linux-gnueabihf`, `build_arm.sh`, no `windows.h`), M2 boot and data loading (`extract.c`/`files.c` paths, checksum of the original executable, `fill.c` with `mprotect`), M3 first correct picture
  (4:3 on 640x480), M4 audio (SPU 44.1 kHz on an SDL2 stream) and controls (SDL Gamepad/evdev), M5 gameplay, battles, FMV (MDEC + XA) and `.mcd` saves, M6 handheld features (quick menu `L3 + R3`, exit hotkey `Select + Start`, fast-forward with `clock_nanosleep(CLOCK_MONOTONIC)`, NEON only where telemetry shows contention), M7 PortMaster/ArkOS
  packaging (`dw3.sh`, `dw3.json`, install docs). **What failed or did not happen:** the machine is an **XF35H** (same RK3326/Mali-G31), not an R36S, so every "R36S" line is technically wrong; the Option A vs B comparison happened after M7 (the port shipped on Option B, software + `/dev/fb0`; GLES came with plan 4); the premise that rendering on the Mali
  breaks the byte-identical picture was **wrong** (in `--screenshot` frames the GLES picture is byte-identical, VRAM differs in 0.2-0.7% of texels); M5's movie path was never verified end to end on the GLES renderer (the 24-bit display path was fixed by reading code, not by watching an FMV); M6's NEON work only happened where the meter pointed (flat span 2D fast paths yes, the decoded-texture cache never, audio NEON stopped at
  3.1 ms of a 2.5 ms target); **M7's real deliverable, a published port, did not happen** (a Discord testing thread and tests on other CFWs are needed, the owner's call); nothing was ever tested on a second device or CFW: every number comes from one handheld.
- **Plan 2, battle margin (closed).** Measured on the device in the heaviest battle: 19.7 s CPU in 20 s wall = 19.7 ms per frame at 50 Hz against a 20 ms budget (before: 21.5 ms, 1.2x slow motion). What produced the difference: meters off the game's path in `dw3.sh` (1-2 ms; the A35 `mrrc` costs 6-15 cycles and the meter read the counter ~10 times per span),
  `-mcpu=cortex-a35` (17 `__aeabi_idiv` calls became 0, 15 hardware `sdiv`), NEON for the flat span on `GPU_COPY` and semi-transparent primitives (98% of spans fell in the scalar loop because the vector path only accepted `GPU_OPAQUE`), `P2` (cross product per row), `R7` (meter without a double), the walk sharing the counter.
  **Ruled out with a source:** P1 divide by reciprocal (~0.3 ms total), P3 direct packet dispatch (`walk - tri` = 0.6 ms of 19.3), P4 CLUT per triangle (~0.06 ms), 2x scale in the VOP (the RK3326's Win0 scales only bilinear/bicubic, through DRM: it would blur), 2 MB huge pages for the micro-TLB (THP is off in this kernel), CPU/DMC governor (already `performance`,
  DMC 528 MHz is the DTS ceiling), the rasteriser on the Mali (breaks the byte-for-byte match). **Why 55 ns/px:** the Cortex-A35's micro-TLB has 10 entries and a VRAM row is 2 KB, so every pixel crosses a 4 KB page every two rows; the prefetcher only follows strides inside one 4 KB page, so the manual `PLD` bought nothing; benchmark 14.6 ns/px with an L1-sized working set,
  54.8 with 1 MB: latency, not bandwidth (DDR about 4.2 GB/s theoretical, the game uses about 75 MB/s). Planned: close the measurement (read `window: N frames presented in X s` in `game.log`: 50/s = no slow motion), audio on NEON (about 3 ms), a decoded texture cache in tiled 8x8 blocks (128 B; about 4 ms; medium-high risk, validate by golden replay + a byte-identical save state), delivery cleanup
  (meters commented out, README with the battle number, `DW3_NO_NEON_FLAT` / `DW3_NO_NEON_GOURAUD` documented). **What failed:** the "0.3 ms headroom" was wrong (a state-based measurement later gave 23.9 ms = 41.85 fps, 4 ms over, the 19.7 figure came from one instrumented window); the premise died when GLES became the default; step 2 went to plan 3 and stopped at 4.72 -> 3.11 ms per
  vsync; step 3 never happened; the README's battle number had to be corrected twice; the per-part profile divided by a hardcoded 882 samples a vsync while the machine ran its 60 Hz mode (735), so every figure came out 20% high until found.
- **Plan 3, audio (closed with what it had; "we can close with what we have, it is better than nothing").** The SPU mix costs 4.9 ms a frame in battle (`DW3_NO_AUDIO=1`: 4.9 -> 0.0); the right instrument is the `aud` field of `DW3_GPU_LOG` (only `spu_render`, cycle counter); `DW3_PHASE` is not usable (in a battle it costs 23 ms); per frame 24 voices x 882 samples at 44.1 kHz plus noise, capture buffers and the reverb (two 39-tap FIRs per channel on half the
  samples). Rules: byte-identical sound (`--wav` before/after, `cmp`), no rate change, no voice cut, no quantising, no rounding-changing reordering, one commit per stage with `aud` before/after, `DW3_NO_AUDIO=1` stays as a measurement switch, measure on the device with `emustation` stopped. Stages: 0 measure inside; 1 idle voice (level 0 in RELEASE contributes nothing, but pitch counter, `spu_voice_block_end`, `ENDX` and loops are side effects that must stay);
  2 global early out when no voice is audible, reverb off and no CD audio (zero fill is exact); 3 NEON in the 4-tap Gaussian interpolation and volume (ADPCM is serial and does not vectorise); 4 NEON in the reverb FIR (64-bit products, integer addition is associative); 5 another core (not recommended: register writes mid-frame, glitches, L2/DDR contention). Criterion `aud` <= 2.5 ms, ideal <= 1.5 ms, WAV identical.
  **Result:** stages 0, 1 and 4 were done (in the order the profile asked), WAV byte-identical (md5 `39757ffd...`), battle `aud` 4.9 -> 3.0-3.3 ms, field/menu 4.72 -> 3.11 ms per vsync; criterion **not met** (0.6 ms short). Stage 3 aimed at the wrong thing (interpolation was 0.3-0.4 ms of 4.7); stages 2 and 5 not done (2 is cheap and still on the table); the biggest win was not in the plan: `spu_envelope_tick` was a function
  call in `spu_dsp.c` called three times per voice per sample; making it `static inline` in the header plus skipping the tick of a released voice at level 0 is most of the 1.6 ms. Detail: `docs/SOUND.md` "What it costs".
- **Plan 4, GPU backend `gles` (done to being the default).** Battle (600 frames of one state): software 37.70 fps, `gles` first 18.75, today **41.85**; the scripted replay's battle window runs at the 50 Hz cap (49.90 against software 49.67); 2D is level (48.99); VRAM check exact in 17 of 18 comparisons; against software 0.2% before models, 0.7% with them (was 4.5%), zero undrawn primitives. Stages: 0 measure (about 1060-1080 primitives
  a frame, about 110 batches, 0 `LoadImage`, about 2 VRAM copies, about 88k textured pixels; criterion <= 150 batches passed); M1 EGL on GBM offscreen (no window, no DRM master; RGBA8 1024x512 target + a read copy; no swap (17 ms), no mode change; picture leaves through the existing fb0 present: a `glReadPixels` of the RGBA8 target plus an 8-to-5-bit conversion through a pair of PBOs, since
  reading the target as RGB565 cost 28 ms); M2 rectangles and sprites; M3 triangles with the CLUT in the shader (4/8/15-bit, texture window, `texelFetch`, integer interpolation like the shim's); M4 semi-transparency (4 modes), mask and dithering through `gl_LastFragColorARM` and flat varyings (DuckStation's batch key became per-vertex state: the four modes, the mask, Gouraud, dither and the drawing
  area were batch state and each cost a draw); M5 VRAM copy and checkpoint (GP0 80h as a blit, 6.5 ms -> 0.06 ms a copy; a save state reads the target back before writing, so a `gles` state's `gpu_vram` block is byte-identical to a software one). Decisions: `gles` is the device default (`settings.json` `"renderer": "gles"`, the handheld menu writes the same; revert with `"software"` or `--renderer software`; the software
  renderer's source is the `software-closed` tag; software **stays the oracle**); selection by `--renderer`/`video.renderer` (the file's `video.renderer` was **ignored**, `"gpu"` ran software, found on the way); one VRAM read per vsync not per batch (18.75 -> 31.5 fps); `glBlitFramebuffer` obeys the scissor (blit with scissor off; the drawing area became a fragment test). **Open at close:** the present (about 3.2 ms: a 320x240 RGBA transfer from the PBO +
  `memcpy` (mapped memory is uncached: converting straight out of it cost 7.1 ms) + conversion + fb0; ideas: let fb0 take the target's format, or deliver through KMS, which is the 17 ms swap the design avoids); the 0.7% difference (55% of pixels with a (-1,-1,-1) delta in 5-bit units, a minority with displaced geometry; next: the **single-triangle** experiment to copy `gpu_triangle`'s edge rule, a half-pixel clip-space bias); the 2D check at
  frame 1700 (1,918 pixels, 0.4%, region 269,195..312,492, predates the round); `DW3_GLES_LOG`/`DW3_GLES_CHECK`/`DW3_STATE_ANY` are instruments, never left on in a session; audio stays in plan 3. Evidence: `docs/gpu/01`-`16` (`16-frame-budget.md` has the dead ends: `glBufferSubData` at 105 us a draw, RGB565 readback 28 ms), commits `b0d17d3`, `494e60a`, `c2ce59d`, `ec234a3`, `2968e84`, `876749c`, `0d5a41b`, `1796e2c`, `b28ff2b`.
  **What failed:** the completion criterion (<= 20 ms) is not met (23.9 ms; the replay window holds the cap but is the lighter half of the same battle); in light 2D scenes (menus, boot) this renderer is **slower** than software (43.5 fps against 57.7 on the scripted run: the present surfaces the frame's GPU work, the boot's texture uploads, once per vsync); the present's 3.2 ms was not attacked; the 0.7% is still there and the single-triangle experiment never run;
  no FMV was ever run on this renderer; **`--input-test` hangs on this device** (no output, immune to `timeout`; the `dw2003` processes it left were killed by hand); measurement discipline failed twice (50 Hz vs the machine's 60 Hz compared; one "no regression" needed an A/B against an older build).
- **Plan 5 (environment detection, closed; plan 6 was a mistaken duplicate rejected).** A tester on a Retroid Pocket 5 (Snapdragon 865, ROCKNIX, Wayland) ran the same 32-bit armhf binary by switching the video backend by hand; the plan automated that with zero regression on the reference RK3326. Invariants: branch `feature/env-detect`, no C or runtime changes; reference behaviour (`/dev/fb0` writable, no Wayland) byte for byte (fbdev, framebuffer menu, GLES,
  alsa); renderer by strict allowlist: software unless all of no Wayland, `/dev/fb0` writable, CPU part Cortex-A35 (`0xd04`), Mali node under `/sys/class/misc/mali*` or `/dev/mali0` hold; manual overrides (`settings.json`, `DW3_FORCE_RENDERER`/`DW3_FORCE_VIDEO`) win; unverified assumptions marked `TODO: validate`. Work items: passive probe `handheld/probe.sh` (to `ports/dw3/logs/probe.txt`: kernel/OS, `/dev/fb0`, `/dev/dri`, Wayland socket,
  `timeout 2 pactl info`, `aplay -l`, CPU part, Mali, devfreq, `ldd` "not found" check or "ldd unavailable" without failing, the armhf loader, PortMaster context); `handheld/detect-env.sh` decision order (override, Wayland with a real socket -> `sdl2` + `SDL_VIDEODRIVER=wayland` + menu skip, `/dev/fb0` writable -> `fbdev` + `fb` menu, else `sdl2` with `SDL_VIDEODRIVER` unset); audio pulse if `pactl` answers else alsa;
  safe menu fallback (historic: written for `dw3-menu.py`, removed in `cbf34ce`): opening `/dev/fb0` in `try/except`, exit code **3** reserved for `FB_UNAVAILABLE` (the script then skips the menu and runs with `settings.json`), `trap pm_finish`, a 32-bit dependency preflight only if `ldd` exists (missing libs -> blocker + `pm_show_error`); the README says headless/Wayland platforms (`DW3_MENU=skip`) configure mods through `settings.json`. Tests planned in 4 simulated states: reference
  (fbdev, fb menu, gles, alsa), Wayland (sdl2, skip, software), unrecognised (sdl2, skip, software, unset `SDL_VIDEODRIVER`), override (`DW3_FORCE_RENDERER=software` wins); a 1:1 regression diff of the variables against the legacy script; a hardware gate on the real XF35H; delivery by pushing the branch to `al4xdev/PortMaster-MV-New` so the tester downloads the zip from GitHub (no local zip). Detection commit `00c4082`.
- **Plan 7, one idiomatic PortMaster launcher (closed; commit/branch `feature/env-detect`).** Consolidate `handheld/Digimon World 3.sh` and `handheld/dw3.sh` into one script that sources `control.txt` and delegates environment, controls and error dialogs to PortMaster, keeping only game-specific logic (disc detection, the framebuffer settings menu, the targeted GLES enablement); target "about 75% gone" by line count. Process: Stage G a read-only gate note
  `.dumps/tasks/portmaster-launcher-gate.md` tied to the local references (`.dumps/tasks/portmaster-ref/`: the 5058-line `portmaster-ai-complete-reference.md`, `packaging.html`, `porting.html`; provenance in `SOURCE.txt`; never refetch the site, never read `~/Downloads`); Stage C two independent critics attacking the gate note (routes `google-antigravity/gemini-3.8-flash-low` and `xai/grok-4.7`, no shared transcript, no repo edits, no new framework);
  Stage R reconcile then cut (act only on findings pointing at a line in the reference or our scripts; where critics disagree keep the behaviour; a behaviour with no reference equivalent and no measured need is listed, not deleted in the same turn). Not to be done: a new detection framework, C/renderer/audio changes, inventing a PortMaster helper, SSH during the gate or the critic stages, pushing or opening a PortMaster PR without the owner. The result is the single `Digimon World 3.sh`.

### PortMaster CFW research (plan 11) and the muOS aarch64 plan (staging 10)

**Plan 11 (read-only research):** find each CFW's GitHub repo, `git clone --depth 1` into its own empty `/tmp/cfw-<name>/`, never run or build the cloned code, and fill per CFW: devices/SoCs/GPU stack, userspace arch (32-bit only, 64-bit only, multiarch, compat layer), where armhf libs come from and what `PORT_32BIT` does in `mod_*.txt`, glibc, SDL2 source and GLES/EGL libs by arch, video path (fbdev with a real or dummy `/dev/fb0`, kmsdrm,
Wayland/sway, X), display and scaling, pad (`gptokeyb2`), how a known 32-bit and a known aarch64 port launch, a verdict (does our armhf binary run: yes / no / needs fix X; would aarch64 be needed). Checklist rows in order: AmberELEC, dArkOS, muOS, ROCKNIX in three GPU variants (Libmali, Panfrost, Adreno optional), Knulli, then any other CFW on the official list (EmuELEC, ArkOS, JELOS, Batocera, TrimUI stock/CrossMix...). Resolutions: 480x320 (optional), 640x480,
720x720 (RGB30, optional), higher (1280x720). Outputs `.dumps/research/portmaster-devices/<cfw>.md` then `matrix.md` (cross-check key claims against a second source; recommendation (a) armhf-only fix such as not hard-linking `libGLESv2` and loading it only when present, (b) aarch64 build, (c) both; the exact commands to ask testers). Done when `matrix.md` exists and every checklist CFW has a source-backed row.
**State (from the files in `.dumps/research/portmaster-devices/`, local and gitignored): `00-official-list.md`, `amberelec.md`, `darkos.md`, `muos.md` exist (2026-11-11); ROCKNIX, Knulli, the other rows and `matrix.md` were not produced.** Results so far: (1) **No `mod_*.txt` sets `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER` or `LD_LIBRARY_PATH`**; `PORT_32BIT=Y` only changes `SPA_PLUGIN_DIR=/usr/lib32/spa-0.2` and
`PIPEWIRE_MODULE_DIR=/usr/lib32/pipewire-0.3/` in Batocera, REGLinux, ROCKNIX, UnofficialOS, JELOS and knulli; `funcs.txt` appends `$controlfolder/libs.armhf:libs.aarch64` to `LD_LIBRARY_PATH`; `control.txt` adds `/usr/lib32` only for JELOS/UnofficialOS and the ROCKNIX/AmberELEC/EmuELEC style (`/storage/.config/.OS_ARCH`); muOS prepends `/opt/muos/extra/lib`. PortMaster GUI commit read: `091aed9fa2c8`. 40 devices on the
supported-handhelds page. (2) **dArkOS: YES, the armhf binary can run** (Debian arm64 rootfs with armhf multiarch, 32-bit Mali and a custom 32-bit SDL2 shipped by default, `BUILD_ARMHF ?= y`); caveats: the video path is KMSDRM/GBM, not fbdev (SDL2 built `--enable-video-kmsdrm --disable-video-x11 --disable-video-wayland`, Mali `*-gbm.so`), `libGLESv2.so.2`/`libEGL.so.1` resolve to the vendor `libMali.so` in
`/usr/lib/arm-linux-gnueabihf/`, a prebuilt image may have been built with `BUILD_ARMHF=n` (check `ls /usr/lib/arm-linux-gnueabihf/libMali.so /usr/lib/ld-linux-armhf.so.3`), and do not rely on `mod_dArkOS.txt` (its libGL symlink logic only touches `aarch64-linux-gnu`). (3) **AmberELEC: YES in principle**: aarch64 userspace with a purpose-built 32-bit bundle in `/usr/lib32` (loader, glibc, SDL2 2.32.10, libmali EGL/GLES/gbm, alsa/pulse) and
`CONFIG_COMPAT`; `/usr/lib32` is searched only because PortMaster's `control.txt` appends it when `/storage/.config/.OS_ARCH` exists (so the script must source `control.txt`, which every PM port does); KMSDRM/GBM only (use the SDL GLES renderer; direct `/dev/fb0` real scanout unverified); 32-bit tarballs exist only for RG351P/V/MP, RG552, RG353 (RG503 reuses RG353); confirm `libGLESv2.so.2`/`libEGL.so.1` exist in `/usr/lib32` on the target.
(4) **muOS: the `libGLESv2.so.2: wrong ELF class: ELFCLASS64` error is consistent** with the armhf loader running (the symlink works) but only a 64-bit `libGLESv2.so.2` being found (system Mali/`libmali` in `/usr/lib` or `/usr/lib64`); muOS adds no armhf GL dir in `mod_muOS.txt`, `muos/control.txt` or `ext-general.sh`; PortMaster-GUI ships no `libGLESv2` in `libs.armhf`; the only armhf GL in the MustardOS repos is `mugl`'s `lib32/libGLESv2.so.2` + `libEGL.so.1` (TrimUI-documented, no launcher wiring found).
**Unverified premise:** that no armhf `libGLESv2.so.2` exists in `/usr/lib32` on that image; if one does, adding `/usr/lib32` to `LD_LIBRARY_PATH` (as the JELOS/ROCKNIX mods do) would fix it; nothing in muOS source does that. The tester's "PORT_32BIT=Y works on muOS" is only **partly supported** (loader symlink, `/lib32`, `/usr/lib32`, PipeWire dirs are real; no working armhf GLES2/EGL stack shown for a non-TrimUI device; known-working 32-bit ports there are probably SDL2 software-render or bring their own GL (gl4es armhf, bundled `libs.armhf`)). `CLAUDE.md:82` of PortMaster-GUI says muOS's SDL has only the `mali` video driver
and `:156` that SDL quit order matters on muOS/Mali. **Do not inherit the old claim "muOS is 64-bit only"; it came from an old log.** Pending from testers: for NotYerAvgPorter a fresh `game.log` and `LD_DEBUG=libs ./dw2003 --help 2>&1 | head -40`; for Pengon (TrimUI Smart Pro S, TG5050, Allwinner A523, 8x Cortex-A55, Mali-G57, 1280x720, "Modified Stock", PortMaster installed, Fallout 1 and Heroes 3 work; believes TSP/TSPS support 64-bit only: unverified) `uname -m`,
`ls /lib /usr/lib | head -50`, `ldd --version`.
**Staging 10, the data of the muOS tester (NotYerAvgPorter, MST, 2026-11-05/10).** muOS 2601.0 (JACARANDA, MustardOS build `bc38efa0`), aarch64, kernel 4.9.170, CPU part `0xd03` (Cortex-A53, x4), Allwinner sunxi codec, `/dev/fb0` present and writable (crw-rw---- root video 29,0), no `/dev/dri`, no Wayland (`XDG_RUNTIME_DIR=/run`), `/dev/mali0`, 640x480, PortMaster `directory=mnt/mmc` (port at `/mnt/mmc/ports/dw3/`), `CFW_NAME=muOS`;
`/lib/ld-linux-armhf.so.3` present, `/lib/arm-linux-gnueabihf` absent. Launcher detect lines: `DW3_VIDEO=fbdev`, `DW3_MENU=fb`, `DW3_RENDERER=software` (CPU part != 0xd04), `SDL_AUDIODRIVER=alsa`, `SDL_VIDEODRIVER=kmsdrm`. `DW3_FORCE_VIDEO=sdl2` does not help (the binary fails to load, not the video path; a ready zip `.dumps/dw3_sdl2_test.zip` is not useful for this case). The error log was from early versions; a newer `log.txt` showed
the right version but the freeze after PLAY was not covered by `log.txt` (launcher only). Asked of them: `logs/game.log`, `menu.log`, the device model (unknown; A53 + Mali + sunxi suggests an H700/A133-class device); with Debug logs heavy, PLAY, wait a couple of minutes, then `logs/game.log`, `menu.log`, `logs/probe.txt`. The developer offered a live session (Tuesday 2026-11-10, 20:00 BRT) and only has an RK3326 to test on. The reference device: EmuELEC 4.7 on an RK3326
handheld (aarch64 kernel with 32-bit userspace, `DEVICE_HAS_ARMHF=Y`), Mali-G31 GLES 3.2, `/dev/fb0` 640x480, CPU part `0xd04`; the only tested target. The Trimui Smart Pro S owner (TG5050/A523) asked whether the port can run and offered to test: open questions to them: firmware (stock, CrossMix, Knulli, muOS?), PortMaster installed?, and the same `probe.txt`; an OS-porting briefing a research agent produced (mainline Linux, U-Boot, device tree, repo `MidG971/trimui_smart_pro_s`) is irrelevant: only a port that runs on the existing firmware is needed.
**Staging 10 plan (phases, now mostly implemented in code):** Phase 0 investigation (the MustardOS GitHub organisation: how the image is built (Buildroot? Debian?), glibc, where SDL2, video libs and ALSA come from; whether the rootfs can be extracted; `mod_muOS.txt` variables `DEVICE_ARCH`, `PORT_32BIT`; kmsdrm vs fbdev; the exact device; ask the tester for `ldd --version`, `ls /usr/lib`, the PortMaster runtimes folder; output `.dumps/tasks/muos-research.md`); Phase 1 aarch64 toolchain on a `feature/aarch64`
branch (`cmake/aarch64-linux.cmake` from the armhf one without `-mfpu`/`-mfloat-abi`/`-marm`, `-mcpu=cortex-a53`; a low-glibc Docker image; first build `-DPSXSTACK_SDL2=ON -DPSXSTACK_EGL=OFF`, software renderer, game C at `-O0`; read the two `__arm__` blocks in `psxstack/psyq/gpu.c` (about 1386 and 2309) to see if aarch64 loses performance); Phase 2 qemu-user on the PC (`--platform linux/arm64` or binfmt, `SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`, `--renderer software`,
`--max-frames 600`, own disc; check the binfmt first; if glibc is in doubt mount the muOS rootfs as a sysroot; it does not cover kmsdrm, real ALSA or A53 speed); Phase 3 launcher picks the binary by `DEVICE_ARCH` or `uname -m` with no `PORT_32BIT` on the aarch64 path and skips the 32-bit `ldd` blocker, `port.json` `["armhf","aarch64"]`, `make_release.sh` accepting both, video choice per Phase 0 (the launcher picks fbdev when `/dev/fb0` is writable with no Wayland, which is what the tester's machine did); Phase 4 an experimental
test zip on Discord. Repo facts then: `port/` has no ARM-specific code; in `psxstack/` `runtime/fiber.c` (`__aarch64__` about 200, `__arm__` about 236), `runtime/savestate.c`, `runtime/crash.c`, NEON guarded by `__ARM_NEON` in `psyq/gpu.c`, `runtime/spu.c`, `runtime/fbdev_present.c`; `port/CMakeLists.txt` says `-O2` on the game C crashes at boot (`sound_init` -> `sound_update_loading` -> `cdload_get_subfile`, SIGSEGV), so the game objects stay at `-O0` and this must be retested on aarch64;
`port/game/state.c` already handles 8-byte pointers for `-m64` with `_Static_assert`s; `psxstack/` is a submodule (fork `al4xdev/dw3-psxstack`): changes need a commit there and a pin update here. Remotes at the time: `public` = `https://github.com/al4xdev/dw3.git`, `origin` = upstream `gascarcella/dw2003recomp`; local branch `upstream-pc-base` mirrors upstream (upstream was only 3 test commits newer than the cherry-picked base `61d06ba`, we were 155 commits ahead); the PortMaster fork `/home/alex/git/my/PortMaster-MV-New` (`al4xdev/PortMaster-MV-New`, `ports/dw3/`, commit `940aa63`); the release `gh release upload dw3-port-1 dist/dw3.zip --clobber -R al4xdev/dw3`.

### Android minimal port (staging 14), the plan

Target only `arm64-v8a`, API 30+ (Android 11+), SDL3 + Vulkan (`psxstack/runtime/render_gpu.c`) and SDL3 audio; no armeabi-v7a/x86, no custom physical-controller remapping (the standard `GameController` pass-through only if free). Controls: a simple virtual touchpad as an external layer in Kotlin/Views on the Android/Gradle wrapper (or an SDL overlay if simpler) sending standard key/button events (`SDL_SendKeyboardKey` / JNI to the input queue) without touching game logic;
layout: D-pad left, Cross/Circle/Square/Triangle right, L1/R1/L2/R2 at the upper corners, Start/Select top/bottom, visibility flag read from the launcher settings; **an Android-only launcher option (`#ifdef __ANDROID__`) "Show Touch Controls"** to hide the overlay (Bluetooth/USB-C pad or recording). **Manual disc path outside PortMaster** (Linux standalone and Android): when the launcher detects it did not come from PortMaster (no `control.txt` or no disc in `game/`), offer choosing the `.cue`/`.bin` path
and persist it in `settings.json` (`disc.path`); on Android (scoped storage) pick through SAF (`ACTION_OPEN_DOCUMENT`/`OPEN_DOCUMENT_TREE`), copying or referencing into the app's private folder (`SDL_GetAndroidExternalStoragePath()`/`GetPrefPath`) so native `fopen()` in `psxstack/runtime/disc.c` reads `.bin` + `.cue` (the `.cue` names the `.bin` in the same folder). **Phase 0 (mandatory before any code):** the state of both launchers (`launcher/` `dw3-launcher` C99 with `is_pc`/`disc_path`, and `psxstack/launcher/` `dw2003-launcher` C++17 ImGui) after `feat/girls-power` and `widescreen-field`
close, and which is packaged on Android; an NDK `arm64-v8a` (Bionic) audit of `psxstack`: `runtime/fiber.c` `port_fiber_swap_` (`__aarch64__`) compiles and runs on Clang/NDK (stack alignment, no `ucontext`), `runtime/crash.c` (`ucontext_t`/`dladdr`/`backtrace` glibc vs Bionic: disable or adapt `crash_write_registers` under `__ANDROID__`), `overlay.c`, `platform.c`, `memcard.c`, `settings.c`, `disc.c` (POSIX/Linux calls that differ: `HOME`/`XDG_DATA_HOME` paths, `argv`/`SDL_main` in a shared library,
packed SPIR-V shaders in assets/memory), `cmake/psxstack.cmake` (build `dw2003` as `SHARED` `libmain.so` under `CMAKE_SYSTEM_NAME=Android`, offline `.hlsl` compilation with `dxc` on the host before the `.apk`); disc/SAF flow and non-PortMaster fallback in `launcher/cfg.c` (or `psxstack/launcher/src/disc.cpp`); the touchpad mapping (Kotlin `TouchOverlayView` over `SDLActivity`, JNI to SDL3 or standard `KeyEvent`/`MotionEvent` consumed by `runtime/input.c`).
**Implementation phases:** 1 build `libmain.so` (`arm64-v8a`) with the NDK + `psxstack` (`#ifdef __ANDROID__` in `psxstack/runtime/` `platform.c`, `crash.c`, `main.c` `SDL_main`, asset/shader paths; a minimal `android/` Gradle + Kotlin `DW3Activity : SDLActivity`; gate: `libmain.so` builds and `tests/port/run.py`/hello show no Linux/Windows regression); 2 manual disc selection + SAF import (`disc.path` in `settings.json`); 3 Kotlin touch layer + Android-only toggle, tested on a real device (`adb install`, `adb logcat`: boot, disc selection, entering the game, a Vulkan + widescreen battle, the toggle).
(The repository has `android/` with `touch_pad.c`, `android_entry.c`, Gradle files and `tools/build_android.sh`; the README's Android line describes it. Which of the phase gates were met is unverified.)

### Rebalance "Persona-like" brief (closed plan 12, a design brief for a frontier model)

Problem then: only one Digimon viable as a magic tank (Guard + Spirit), no support roles, weak magic (class 3). Goal: several clear Persona-like roles (tank, healer, support, physical, magic, all-out attack) **without creating new techniques** (re-aim existing ones) and transparent (a mini-guide and descriptive technique texts). Pillars: **Wisdom as the universal buff/debuff scaler** (the support's identity: heal + status + scales every
buff; buffs, debuffs and heals work against bosses, only status inflicts are resisted); current buffs/debuffs also buff Spirit (magic) plus dedicated magic buff/debuff techniques (full set {Power, Guard, Boost, Spirit, Wisdom}); a **shared shield** of 1/2/3 turns reusing the Evo Buff's guard-at-ceiling; a **"dark buff" triggers the Digivolution** (the big form) as the **All-Out Attack**, since without multi-attack the payoff is the transition to the blast form; reuse (re-aim/re-point) techniques, at most 1-2 dedicated magic ones;
transparency. Technical foundations (validated): stats Power `stats[0]`, Guard `[1]`, Spirit `[2]`, Wisdom `[3]`, Boost `[4]`; normal damage `get_damage` (element 2) `power x Power / Guard`; magic `get_special_damage` (element 3) `power x (own Spirit x 50 / target Spirit + 50) / 100` clamped `[power/2, 2 x power]` (Spirit on both sides); heal `get_heal` `power*64 + Wisdom x power / 8` (scales with Wisdom); the status roll uses `rec->effect_chance + stats[3]/8` against `target->stats[3]/2`;
the gap in the original buff tables `fightstg_tech_stat_effects` (IDs 198-208) and `_party_stat_effects` (199/201/203): only {Power, Guard, Boost}, none touches Spirit or Wisdom; `change_modifier` `modifiers[kind] += base x (factor x effect_power)/128` clamped `[-base/2, base]`; `fightstg_rules_modifier_stats[4]={0,1,4,0}` gave kind 3 -> Power (double) with no slot for Spirit and the party path of `get_stats` did not apply `modifiers[3]`; recommended: hook directly in `get_stats` adding/subtracting (the Evo Buff style); the shield (`dw3_evo_shield_member`, guard at ceiling, reset each party turn in `wfightmn_party_turn`)
generalised to N turns with a counter; blast forms `RecordsDigimon.blast_forms[5]` (value = `records_digimon` index + 1) by level band, the transition fires at gauge 1000 until event `0x13`; the "dark buff" would call the same code as `wfightmn_blast` (watch cooldown/limit, blast turns, gauge reset; validate against the `wfightmn` golden); rotation: command 5 is the switch, `RecordsDigimon.pair_technique` (`include/records.h`) is the team technique offered when the partner enters; `techniques[7]`: `[0]` and `[6]` read by FIGHTSTG (base/signature, flag `0x8000` in the menu), `[1..5]` learned
(STFGTREP); **key risk:** `records_techniques[]` is global, so list who uses a technique ID before re-aiming it. Role table: physical tank = high Guard, magic tank = high Spirit, healer = heal + status clean (Wisdom), buff/debuff (Spirit, x Wisdom), resist / resist-down (Wisdom), A-Attack physical (element 2, Power), Skill magic (element 3, Spirit), All-Out Attack = dark buff -> Digivolution. Example kits: Patamon -> Angemon (id 0x1F, `records_digimon[7]`, Dark, base technique 60, magic) healer/support (heal + status, team buff, shield); Veemon
(id 0x91, `records_digimon[4]`, Ice, base 57, physical) debuff + dark buff (digivolve = AoA) + big hit. Deliverables asked: rebalanced data (Digimon stats, `records_techniques[]` re-aimed, `pair_technique`, `techniques[6]`), more descriptive technique text (`records_language + 0xA2`), a mini-guide, validation (golden tests, PC and handheld battle). **Open decisions it left:** Wisdom scale (linear? cap? which of buff, debuff, heal, shield); shield levels (2 and 3, or 1 and 3 for a mega); dark-buff -> digivolve cooldown/limit (1 per battle? gauge reset or advance?); scope (key roles first or every line); re-aim versus 1-2 dedicated magic techniques. **Outcome:** the later PRDs implemented a different path (fixed-tier buffs, status by Wisdom, Blast redesign, Chosen Evolution); Wisdom-as-universal-scaler and the shared shield were not built as described (unverified: the code's `def_*` buff tiers do not scale by Wisdom).

### Telemetry Harness and Plugin Gatekeeper (staging 16, PRD v0.1 draft for owner review, unrelated to the game)

Owner Alex; evidence base `deepseek-harness` @ `39163dd3` (cloned read-only at `/home/alex/git/my/deepseek-harness`, 2026-11-13, 251 MB shallow; `python3 3.14.10`, `uv 0.13.8` (project pins 3.12+), `bwrap` functional with `--unshare-net`, `docker` 29.9.3 rootless with no compose plugin so MinIO via `docker run` + a Makefile target). **Problem:** agents execute real actions on real machines and three things are missing: per-caller authorization (a shared plugin is a confused deputy by
construction), unified telemetry (a common event envelope, tamper-evident ordering, durable store surviving the container), a control tower (see all teams, approve access, set limits, revoke a running capability). **Positioning against DeepSeek Harness (DSH)**, verified in the clone: DSH is "everything is a plugin" on Cordis (`README.md:7`); it has a permission model but per session, tool-level, human-in-the-loop (`--permission-mode`, `--approval-policy ask|never`, `docs/cli-help.md:119-122`, an `approval/request` waterfall event, a `ctx.sandbox` seam
`docs/capability-seams.md:250-251`); it disclaims isolation strength (`SAFETY.md`); its `packages/guard` is loop hygiene (`repeat-tool-reminder`, `timeout-policy`), not authorization; it uses newline-delimited JSON-RPC over stdio for its Python SDK (`python/sdk/README.md`), a convention mirrored here. The gap filled: per-caller authorization on shared plugin instances, static plugin admission control, kernel-enforced scoped space, hash-chained unified telemetry that survives the container, and a tower that can revoke mid-run; implemented in
Python. Personas: plugin developer, team lead/operator, control-tower admin, external consumer. **Goals G1-G6:** default-deny per `(client, plugin, grant, resource)` on every mediated call; a plugin cannot obtain undeclared authority nor reach outside its scoped space (kernel-enforced, tested against a hostile fixture bypassing our API); one append-only JSONL journal, unified envelope, hash-chained, broker-assigned sequence, outside plugin reach; sealed segments uploaded as immutable new objects and a crashed run reads as incomplete; a tower that
registers, grants, limits and revokes at runtime with revocation reaching the data plane; a gatekeeper suite judged by an **external oracle** (bytes on disk, processes alive, objects in the bucket), never our own log. **Non-goals v0.1:** production hardening, multi-host federation or HA, rewriting arbitrary third-party plugins, bucket authenticity, a tower UI (registry + policy API and CLI only), model quality. **Terms:** *Grant* (`read`, `write`, `spawn`, `handoff`, `ctx`, always resource-scoped; "capability" is avoided because DSH uses it for swappable service seams;
model input/output is the floor, not a grant), *Client* (authenticated external consumer), *Plugin instance* (one supervised subprocess serving N clients), *Effective grant* = `manifest_grants ∩ client_allow − client_deny`, default deny, canonicalised prefixes, *Seal/segment*, *Broker* (the only path from a client to mediated effects).
**Requirements.** A admission (static): A1 manifest YAML (name, version, entrypoint, artifact sha256, grants with resource prefixes, declared model-face capabilities); A2 validation rejects unknown grants, resources escaping the plugin root after `realpath`, hash mismatch, missing entrypoint, schema violations; A3 failure = no process spawned, a reason code recorded; A4 declarative YAML tools compile to a manifest + fixed call plan with the same validation. B supervision: B1 the watcher spawns each plugin in its own process group under `bwrap` (read-only `/`, private `/tmp`,
network unshared, only the scoped dir bind-mounted read-write); B2 start/stop/health/quarantine (health is a mediated RPC and not evidence of correct behaviour); B3 global quarantine kills the process group including descendants; B4 the runtime never spawns plugins directly. C authorization: C1 `client_id` derived from the authenticated transport (per-client socket + HMAC token), never the message body; C2 every call checked against the effective grant set, denial journaled with a reason; C3 per-client rules on a shared instance (revoking B denies B's next call while A continues);
C4 plugins perform no authorization and get only a request-scoped handle (no authority cached across calls or clients); C5 per-client resource scopes (B with `write:regions/b` cannot reach `regions/a` in the same instance); C6 monotonic `policy_version` checked before each mediated call. D telemetry: D1 envelope `team, agent, run_id, seq, ts, trace_id, event, actor, grant, resource, verdict, reason`; D2 broker-assigned monotonic `seq` (a plugin-supplied one is rejected and journaled as a violation); D3 `prev_hash` hash chain; D4 journal outside every plugin-reachable path; D5 segments seal on size, time or
`run_end` and a run ends with an explicit `run_complete`; D6 reads distinguish complete from incomplete. E sync: E1 upload only sealed segments keyed `{team}/{plugin|agent}/{date}/{run_id}/{seq_start}-{sha256}.jsonl`; E2 `If-None-Match: *` (a `412` is success-by-identity after verifying the stored bytes' hash); E3 idempotent retries with backoff, a checksum mismatch withholds the "synced" mark and alerts; E4 never upload while the runtime may still append (seal is a handshake); E5 a per-run manifest of segments uploaded last. F tower: F1 registration API (agents, plugins, manifests, grants, quotas); F2 policy API (per-client allow/deny, quotas, global kill switch,
publishes `policy_version`); F3 heartbeats and low-volume summaries so the tower stays off the hot path; F4 a CLI usable in a demo without a browser; F5 tower unreachable degrades **closed** for new grants and **open** for already-granted calls within a stated grace window (owner decision, Q3). G demo: G1 agent config declares plugins + grants and runs a scripted sequence; G2 two agents, two plugins, three clients (A allowed, B restricted, C operator read-only); G3 one declarative YAML tool runs without code.
**Architecture:** `external client --(own socket + HMAC)--> broker/gatekeeper --(mediated JSON-RPC)--> plugin subprocess`; the control tower (HTTP) publishes `policy_version`; the journal (hash-chained, outside plugin reach) feeds seal + sidecar `--If-None-Match:*--> MinIO`; the watcher spawns/owns plugins under bwrap; processes: `harness`, `control-plane` (tower API + gatekeeper + watcher), `plugin` (N), `sync-sidecar`, `minio`; wire = newline-delimited JSON-RPC.
**Test matrix (the primary deliverable; oracle = judged independently of our logs):** GK-01 undeclared/unknown grant refuses to load (no process, no socket); GK-02 artifact hash mismatch rejected pre-exec (exec never happens, audit hook); GK-03 default deny (denied and no bytes in the scoped dir); GK-04 identity from transport (B sends `client_id=A`: denied, A's resources untouched); GK-05 A writes, read-only B cannot read A's region (bytes only under `regions/a`, B's result lacks the secret); GK-06 hostile fixture (`open(/etc/passwd)`, `../`, symlink out, `/proc/self/fd`: all fail inside bwrap, nothing created outside, no other region visible);
GK-07 kill plugin leaves no descendant (process-group/cgroup probe, not `ppid`); GK-08 a plugin cannot append/truncate/forge the journal and a plugin-supplied `seq` is rejected; GK-09 tamper a sealed segment -> chain break detected, upload refused (`chain_broken`, bucket unchanged); GK-10 `FAIL_AFTER_SEQ=N` crash -> the uploaded run reads incomplete (no `run_complete`); GK-11 same-key re-upload -> `412`, never overwritten, idempotent retry; GK-12 a mediated write with a known payload hash appears in the uploaded object (hash + broker `seq` in bucket bytes); GK-13 revoke B only -> B denied, A still served, same PID alive;
GK-14 global quarantine -> in-flight denied within a bound, group dead, no mutation after the bound. **Anti-theater rule:** any test whose pass condition is "the broker logged denied" scores **fail**; tests emit through the channel they audit, so completeness claims need an outside effect (file, process, bucket object). **What the demo will NOT prove:** journal completeness (only ordering, chaining, immutability of what the broker records; GK-06 covers our plugin shape only), bucket authenticity (integrity and non-overwrite, not authorship), universal containment (only shipped fixtures, under bwrap on this host), multi-host
correctness. **Milestones:** M0 scaffold (uv project, schemas, DSH reference notes; `uv run pytest` green on an empty suite, schema round trip); M1 broker + one plugin under bwrap + validator (GK-01..07); M2 journal, hash chain, sealing, sidecar, MinIO via `docker run` (GK-08..12); M3 tower API, policy versions, per-client rules, revoke (GK-13..14); M4 demo script, declarative tool, README, diagram refresh (end-to-end twice from clean). **Risks and mitigations:** confused deputy (no cached authority; GK-04/05 with interleaved concurrent clients), theater sandboxing (kernel enforcement via bwrap, hostile fixture GK-06),
kill switch not reaching the data plane (`policy_version` per call, bound stated in GK-14, process-group kill), watcher cannot express per-client rules (enforcement lives in the broker; GK-13 asserts the PID stays alive), sidecar fabricating confidence (oracle-based GK-12, hash of uploaded bytes), a crashed run looking complete (explicit `run_complete`, `FAIL_AFTER_SEQ`, GK-10), control-plane sprawl (M1-M2 need no tower; M3 uses a CLI), no compose plugin (MinIO via `docker run`). **Open questions for the owner:** Q1 are agents a client class with their own rules or does agent traffic bypass client rules (changes the matrix most); Q2 repo name/location (proposed `/home/alex/git/my/telemetry-harness`); Q3 when the tower is unreachable: N seconds of grace or strict fail-closed; Q4 does validation include running the plugin's self-test or only static checks; Q5 is `spawn`/`handoff` in v0.1 (a critic would cut it: owning a child process is the most expensive thing to supervise). **Status: draft, nothing implemented, no repo created (unverified).**

### Other history worth keeping

- **Worked in layers, one session each**, each with a short report to the owner; the older PRDs (`definitive-combat-prd.md`, `-progression-prd.md`, `-rebalance-prd.md`, `-hunt-economy-dri-prd.md`) were written "for a smaller model to execute", which is why each layer lists where to touch, what exists and how to test; numbers of lines in them drift, confirm with `grep`.
- **PRD 3's own roadmap** (checked items): Hard Mode (done), WEAK/RESIST animations (done), Ice/Water and Thunder/Machine icon merge in battle HUD and Bestiary (done), OVERFINISH (done), teleport-out-of-map fix (done); element icon in the other menus (done: `def_tech_icon`; seen in game on the battle status page and the field status's digivolve page, the Digi-Lab's screens only by their shared layout); open: elemental fields, Numemon (the owner left both out of 1.0); in-game guide and Asuka auctions (done as the Guide), the translation pipeline (PT-BR done), Girls Power (done; Plan A analyse C/Assembly to build missing frames, Plan B reuse her existing animations for kick/fishing: Plan A was effectively taken by repainting frames).
- **Handoff to console of the definitive build (2026-11-12) and the owner's Layer 10 note**: enemies kept the disc's resistance values until the owner approved the weakness table; since approved and applied (`definitive_enemies.c`).
- **Plan 8's "owner does the final on-device test (RP5 / handheld)"**: battle HUD button feel, overlap, Run on R1, Tag on L1, greyed states while paralysed/confused/sealed were the things to check on device.

## Building and documentation

| For | Command | Details |
|---|---|---|
| The ARM handheld binary and package | `tools/build_arm.sh` | [`docs/HANDHELD.md`](docs/HANDHELD.md#building) |
| The PortMaster zip | `portmaster/make_release.sh` | [`portmaster/PUBLISHING.md`](portmaster/PUBLISHING.md) |
| The PC port and the tests | see the guide | [`docs/BUILDING.md`](docs/BUILDING.md) |

The ARM build runs in Docker, so nothing is installed on the host. Everything technical (how the handheld port reaches
full speed, the repository layout, the design documents and every other guide) is indexed in
[`docs/README.md`](docs/README.md).

## Contributing and support

Read [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md) first. Report bugs and ask questions in the
[issue tracker](https://github.com/al4xdev/digimon-world-3-definitive-edition/issues).

## Credits and license

The decompilation and the PC port are the work of the [upstream project](https://github.com/gascarcella/dw2003recomp).
This fork adds the Definitive Edition and the handheld path: the direct framebuffer present, the Cortex-A35 code generation, the vector span work,
the VRAM row layout, the frame meter and the handheld extras, the GLES renderer, the audio work and the package. The
project's own work is under the [MIT licence](LICENSE); the game remains the property of its publisher.
