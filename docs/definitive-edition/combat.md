# Combat

[Technical reference index](README.md) · [Player README](../../README.md)

## Combat

### Weaknesses

Seven elements in five families: **Fire**, **Water + Ice**, **Wind**, **Thunder + Machine** and **Dark**. The two
elements of a pair always share one value on an enemy (both weak, both neutral or both resisted), so a pair counts as
one weakness or one resistance. Every enemy has its own, counted in families:

- most enemies are weak to two families and resist one;
- about one in five enemies up to level 30 is a "wall", weak to a single family and resisting two: find the weakness
  or struggle;
- from level 45 on, enemies are weak to two families and resist two, and so are bosses;
- the final boss is weak to one family and to Dark, and resists three; its second phase (Galacticmon 466) resists Dark
  instead of being weak to it, so a Dark build cannot burst it down;
  the three phases also carry 150% of the disc's HP (5040, 3600 and 5400 become 7560, 5400 and 8100: `def_boss_hp`,
  `DEF_FINAL_HP_PCT`);
- Dark is rare on both sides: only a handful of enemies are weak to it (eight common species, from Numemon to
  Clockmon, carry it as a third weakness), and as few resist it. The four other families
  come out about even: of the 193 species, 93 are weak to Fire, 87 to Water + Ice, 94 to Wind and 93 to Thunder +
  Machine.

Each family has one resistance value: 80 or less is a weakness, 250 or more is resisted, anything between is neutral
(the enemy table uses 60 for a weakness and 100 to 115 for neutral). There is no cap on weaknesses, and they are read
at the moment of the hit, so a debuff that takes a neutral family to 80 or less adds one in the middle of a fight (see
[Buffs and debuffs](#buffs-and-debuffs)).

<p align="center"><img src="../images/definitive/weakness.svg" width="800" alt="The five element families. An example enemy resists Fire and is weak to Water and Ice; after a Fire hit and an Ice hit, INFO shows Fire resisted, Water + Ice weak and the rest ???."></p>

### Stagger and One More

**Only magic carries an element.** Every elemental physical technique the player can learn became magic, its power
recalibrated to the magic table (MP costs unchanged), so a physical technique or a basic attack is purely physical. The
technique lists show it where the game drew its red (physical) and blue (magic) class marks: a fist for a physical
technique, the element's picture for elemental magic, the Spirit picture for magic with no element; healing and support
keep the game's mark. An enemy using one of these techniques casts it as magic too. Thirteen player signature
techniques that had no element as physical blows became magic, with elements that fit each one: Fire:
WarGreymon's Terra Force, Taomon, Marsmon, Seraphimon; Machine: Armormon, Paildramon, Cannondramon, Imperialdramon-F;
Dark: Beelzemon, Devimon, Imperialdramon, GranKuwagamon; and no element: Omnimon's T-Sword and Imperialdramon-P's Omega
Blade. Each one's power (times its hits) keeps the rank its physical blow had among the signatures of about the same
MP, carried to the same rank among the player's magic of that MP (ratio kept within 0.7..1.4, a hit never past 800).
Left physical on purpose: the counter stance of Gallantmon's Final Purification and Grizzmon's Maul Attack (a counter
does not go through the technique's damage), Hookmon's Captain Cannon (its critical chance is a physical blow's) and
the ones that stay pure physical blows (Kumamon, Monmon, Veemon, Dinohumon, Stingmon, GrapLeomon, Rosemon,
GuardiAngemon).

Hitting a weakness with elemental magic **staggers** the enemy (it takes its tired pose) and gives you **one
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
- **You open the fight** unless you are ambushed. Speed no longer decides who starts. An enemy twice as fast as your
  active Digimon acts twice (the disc's rule), and the INFO panel's turn queue shows it; a faster party counts only the
  square root of its lead, so it needs four times the enemy's speed to act twice every time (`def_turn_speeds`, in
  `fightstg_events_get_delay`): the party's extra actions are One More's and Baton Pass's.

<p align="center"><img src="../images/definitive/stagger.svg" width="800" alt="Hit a weakness: WEAK!, the enemy staggers, ONE MORE! Then hit a weakness again, Baton Pass, or any other blow, which gets the bonus and ends the stagger."></p>

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
neutral hit against an enemy that is not staggered it does nothing. Affinity = the family's ceiling × (stat ÷ 999)²,
the stat counting up to 999. Fire and Wind count on their own stat, up to 0.4; Water with Ice and Thunder with Machine
count as pairs, on the mean of the two stats' squares, up to 0.8 (a Water and an Ice technique get the same affinity;
999 + 0 is half the ceiling, 999 + 999 all of it); Dark counts on its own stat, up to 0.8. The strongest hit there is,
a weakness on a staggered target with full affinity, is 2.5 for Fire or Wind and 3.3 for the pairs and Dark.

Because it grows with the square, the ceiling only shows near 999: the middle of the road pays about what it did
before, and the last 100 points are worth almost a fifth of the ceiling.

| Stat | Fire, Wind | A pair with both stats at this value, Dark |
|---|---|---|
| 300 | 0.036 | 0.072 |
| 555 | 0.123 | 0.246 |
| 700 | 0.196 | 0.392 |
| 800 | 0.256 | 0.513 |
| 900 | 0.324 | 0.649 |
| 999 | 0.4 | 0.8 |

(`def_affinity`, thousandths rounded down: 800 gives 513 and 900 gives 649.)

One stat defends against everything: enemy magic is resisted by your **Defense**, and Spirit is purely your magic
attack. Your elemental stats no longer cut the damage enemy elements do to you.

<p align="center"><img src="../images/definitive/damage.svg" width="800" alt="The multiplier: resisted 0.5, neutral 1.0, weakness 1.5 to 2.3, neutral element on a staggered target 1.2 to 2.0, weakness on a staggered target 1.7 to 3.3, no element on a staggered target 1.2. Affinity is the ceiling times (stat / 999) squared; ceilings: Fire 0.4, Water + Ice 0.8, Wind 0.4, Thunder + Machine 0.8, Dark 0.8."></p>

#### The whole calculation

In the order the game runs it, all integer maths (every original step in full is in
[`docs/MECHANICS.md`](../MECHANICS.md), section 2):

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

<p align="center"><img src="../images/definitive/damage-pipeline.svg" width="800" alt="The damage pipeline: 1 base, 2 field, type and boosts, 3 the element step (the Definitive multiplier), 4 times 1.2 for a blow with no element on a staggered enemy, 5 critical and crests, 6 caps."></p>

**A worked example**, checked against the port's code. Your Digimon has Spirit 180 and a Fire stat of 333. It casts a
Fire technique of power 100 at an enemy that fights with Spirit 120 and is weak to Fire; no field, no type match, no
critical hit.

| Step | Calculation | Result |
|---|---|---|
| Base | 100 × (180 × 50 ÷ 120 + 50) ÷ 100 = 100 × (75 + 50) ÷ 100 | 125 |
| Affinity | 0.4 × (333 ÷ 999)² = 0.4 × 0.111 | 0.044 |
| First hit, a weakness | 125 × (1.0 + 0.5 + 0.044) = 125 × 1.544 = 193 | **193** |
| The One More: the same technique on the staggered enemy | 125 × (1.0 + 0.5 + 0.044 + 0.2 + 0.044) = 125 × 1.788 = 223.5 | **223** |
| The same hit if Fire were neutral, or resisted | 125 × 1.0, or 125 × 0.5 = 62.5 | 125, or 62 |

With the Fire stat at 999 the same two hits would be 125 × 1.9 = **237** and 125 × 2.5 = **312**.
The second hit gives no further action (Fire already gave this round's), but it keeps the enemy staggered. The other
way round, an enemy's magic of power 100 (stat scale 16, so × 1) with Spirit 150 against your Defense 120 does
100 × (150 × 50 ÷ 120 + 50) ÷ 100 = 100 × (62 + 50) ÷ 100 = **112**, whatever your elemental stats.

### Buffs and debuffs

- An **attack buff** raises physical and magic damage alike; a **defense buff or debuff** counts against both kinds of
  hit. A magic party gets as much from them as a physical one.
- **Bosses resist status but are not immune to it**: Poison, Sleep and Confusion land on a boss at 60% of their
  usual chance, never above 78% (Paralysis still never lands on a boss; see [Status and poison](#status-and-poison-for-the-player)).
  **Debuffs** land in full: lowering a boss's defense or elemental resistance is part of the plan.
- **Resist Down** lowers the enemy's resistance to your strongest family (the one with your highest affinity, a pair
  counting both its elements) by 80, and **Mega Break** your two strongest families by 100 each, for three of the
  enemy's turns. On a neutral family that opens a weakness that was not there; a resisted one (300) only drops to
  neutral. In an enemy's hands 202, 203 and 207 all act as Resist Down (80, your best family) and 208 as Mega Break
  (100, your two best), for 3 turns; Libra and Relay mean nothing to an enemy (`def_tech_effect`).
- The HUD shows every active buff and debuff as an up or down arrow beside the Digimon, on both sides; a lowered
  elemental resistance shows as a down arrow in the element's colour, one per family.

### Enemies with a rule of their own (personas)

<p align="center"><img src="../images/definitive/shots/battle-persona.png" width="640" alt="INFO on a Woodmon marked with !: its weaknesses still ???, and two rules explained at the foot: SHIFT every 2 turns, guard on/off; takes only 30% until staggered."></p>

Some enemies are a puzzle instead of a bigger HP bar. Every Super Rare, every DRI Gauntlet line, every region's Rare
Target, a few story bosses and, from level 10 on, about half of the common species carry one or more of these rules:

| Rule | What you see | What it means for you |
|---|---|---|
| **Guard** | an orange shield; `GUARDED` over the enemy when a blow is cut | it takes only 30% of your damage until you stagger it. Some are guarded all fight, some only in one phase |
| **Red guard** | the shield on a red field; `GUARDED` in red | 30% of every blow, except a blow on its weakness, which goes through whole. No stagger ever opens it, and physical blows always take 30%: find the weakness with magic |
| **SHIFT** | `SHIFT!` over the enemy, and its colours change | every few of its turns (or each time you stagger it) it changes weakness, guard or both. Each colour keeps its own weakness for the whole fight |
| **Speed** | two chevrons | its Speed is two or three times your party's, so it acts more than once per turn of yours |
| **A wall that is soft** | resists every family | elemental magic does half, but its Defense is lowered (INFO says it "takes more damage"): hit it hard and physical |
| **One way in** | a single weakness, sometimes Dark | only one family staggers it |

**How to read it in battle.** An enemy with a rule of its own has a `!` before its name in INFO (`L2`). Under the name,
one picture per rule, explained by a legend at the panel's foot with that enemy's own numbers ("SHIFT every 2 turns",
"Takes only 30% until staggered"); for a SHIFT, the phase it is in is lit, with one pip per enemy turn left before the
next. At the row's right end, the **next turns** as chips (red diamond the enemy, blue disc you): two diamonds before
the disc mean it acts twice. Weaknesses you have not found are `???`, phase by phase.

**It remembers.** What you learn of a special enemy's map is saved with your game, for that encounter, and shown again
the next time you meet it, until you have found every weakness: then the whole map shows from the start. What you learn
of a common species goes to its Bestiary entry.

The first red guard is a gatekeeper: the Vemmon of the Admin Center's story fight (Lv28), twice as fast as your party,
draining MP with every blow, to teach you the rule.

### Knowing your enemy

- **INFO** (the old Inspect: a seventh button on the battle HUD, L2 by default) and **Start** show the enemy's five element families,
  but only what you have found out: hitting a weakness or a resistance with an elemental technique teaches both
  elements of its family, everything else shows `???`. In INFO a family you know shows its current state (weak,
  neutral or resisted, with a Resist Down or Mega Break already counted), not the state it had when you learnt it.
- INFO also says **what kind of enemy** it is, under its name: one picture per trait of its persona, each explained
  by a legend at the panel's foot with the row's own values (nothing else explains them: no help screen). **SHIFT**
  (two halves, the phase it is in lit, and one pip per enemy turn left before the next SHIFT when it counts turns):
  every n turns or at each stagger its weakness, its guard or both change, which is when the `SHIFT!` callout pops; the
  **guard** (a shield, lit while it is guarded now, dark while it is open: staggered, or in its unguarded phase): it
  takes only 30% until staggered; the **red guard** (the shield on a red field, `GUARDED` in red) takes 30% of every
  blow except one on a weakness, which goes through whole, and no stagger ever opens it (a weakness still staggers and
  still gives its extra action): physical blows and counters always take 30%; **speed** (two chevrons): its Speed is the party's times n; a **lowered Defense** (a
  cracked block). At the traits row's right end, for every enemy, the **next turns** as a queue of chips (red diamond
  the enemy, blue disc you): two diamonds before the disc is an enemy that acts twice (the old "Enemy x2" label). The values of a phase not learnt yet stay `???`.
- **Where the red guard is**: against a party that plays only physical, which no guard ever stops for long, and kept
  rare, since magic finds it easy. The first is its gatekeeper, the Vemmon of the Admin Center's story fight (Lv28):
  the weakest Digimon of its levels raised to the Lv28 bosses (HP 1584, twice its own; Power x1.8) and acting twice per
  action of yours, each blow draining 6.25% of the target's max MP (its own technique, 431), its two weaknesses kept,
  so the fight teaches the guard. Campaign commons that always have it: Blossomon (34), Kurisarimon (40), Tuskmon (42),
  Shadramon (47), Machinedramon (51), Apokarimon (56); Creepymon (55) has the orange guard in one phase and the red one
  in the other. The story's BK WarGreymon (Lv53) is weak to Dark alone, resists every other family and has the red
  guard, its HP untouched.
- **Libra** reveals all of an enemy's elements at once.
- The **Bestiary** (field menu) keeps what you learnt of every species, saved with your game, and lists its Hunt
  Materials: the common one with how many you hold, and its rare items as a collection (see [The hunt](hunt-and-economy.md#the-hunt)).
- Every list of a Digimon's techniques shows each technique's element picture after its name: the battle's technique menu, the battle's Tag and digivolve status pages, the field status screen's digivolve page and the Digi-Lab's three technique screens (`def_tech_icon`). A technique with no element (a physical blow, a support) shows none. It and the status screens keep the seven element pictures.

![Bestiary, the weakness panel in battle and the technique menu's element](../images/definitive/definitive_ui.png)

### Techniques and MP

- Remade techniques: **Libra** (was Speed Up), **Relay** (was Mega Boost: the next switch is free, 50 MP), **Resist
  Down** (was Slow Down) and **Mega Break**.
- More techniques carry a secondary effect, so each form has its own role in a fight.
- Every technique description was rewritten from what the technique actually does, effect first: "Fire magic. Sleep
  chance."
- MP matters now, so the rookies that were starved of it are not any more: Kotemon, Kumamon, Monmon and Guilmon start
  with 140 MP and grow at the same pace as the others.

### Battle HUD

A direct-button HUD replaces the battle's list menu: each action has its own button, drawn as one compact block shaped
like the pad in the left middle of the screen, each command written on the spot of the button it is on (defaults with
Xbox glyphs: **FIGHT** on `A`, **TECH** on `R1`, **DIGIVOLVE** on `Y`, **TAG** on `B`, **BAG** on `L1`, **RUN** on `X`,
**INFO** on `L2`, **BLAST** on `R2`), rebindable from `BATTLE HUD` in the launcher. The BLAST tab is the gauge itself:
it fills as the gauge charges and flashes amber when it is full and ready.

- **TECH and BAG** unfold out of the menu as a sheet on the left half of the screen (`L1` / `R1` turn its pages), with
  each technique's or item's picture, cost or count, and its description; INFO can stay open beside it.
- **TAG and DIGIVOLVE** open a close-up of your Digimon with a ring of choices around it and a live 3D preview of the
  partner or form under the cursor.
- **INFO** opens the enemy panel: its families as you learnt them, its traits and the next turns (see
  [Knowing your enemy](#knowing-your-enemy)). It spends no turn.
- **RUN asks first**: a slip of the button no longer ends the fight.
- It remembers the last technique and item, buffers presses made just before the menu opens, and battle messages never
  stop the fight (they are always skipped).

Controls use the modern **Circle ↔ Triangle** rotation by default (`Confirm` at the bottom, `Back` on the right), with
selectable **Xbox / Nintendo / PS1** button glyphs.

![The direct-button battle HUD and the INFO panel](../images/definitive/shots/battle-inspect.png)

### Constants worth knowing

`DEF_WEAK_MAX` 80, `DEF_RESIST_MIN` 250 (a family's resistance: weak at 80 or less, resisted at 250 or more). Resisted
hits are a fixed 0.5. Affinity ceilings: single 0.4, pair 0.8, Dark 0.8 (convex in the stat). Weak +0.5, stagger +0.2. The player's reference
resistance is 100. Relay costs 50 MP. Resist Down is a delta of 80 and Mega Break 100, for 3 enemy turns. The Baton Pass
is free (`DEF_BATON_PASS_MP` 0) and offered only while the enemy is staggered. **Overlevel gap 3** (`DEF_OVERLEVEL_GAP`). The Rush question lasts 300 ticks, the Rush panel 175 ticks and 10 rows.

Their names: multipliers and affinities are fixed point in thousandths (`DEF_MULT_ONE` 1000): `DEF_MULT_WEAK` 500,
`DEF_MULT_STAGGER` 200, `DEF_MULT_RESISTED` 500, `DEF_AFF_SINGLE_MAX` 400 (Fire, Wind; resist 999), `DEF_AFF_PAIR_MAX`
800 (Water+Ice, Thunder+Machine; both 999), `DEF_AFF_DARK_MAX` 800. `DEF_PLAYER_REF_RESIST` 100 is what an
enemy's element is measured against when it hits the player. Resist Down / Mega Break: `DEF_BREAK_SLOW_DELTA` 80,
`DEF_BREAK_MEGA_DELTA` 100, `DEF_BREAK_TURNS` 3; on a boss the original made immune, a stat debuff is scaled by
`DEF_BOSS_DEBUFF_PCT` (100). `DEF_RELAY_MP` 50. TP: `DEF_TP_GAIN_LV_2_20` 7, `DEF_TP_GAIN_LV_21_40` 9,
`DEF_TP_GAIN_LV_41_50` 30, `DEF_TP_GAIN_LV_51_UP` 0, `DEF_TP_CAP` 99. Short digivolution: hold Circle or Cross
`DEF_EVO_SKIP_HOLD_FRAMES` (4) frames.

### How combat hooks into the game

- **Not a mod.** Always on, no `mod.json` option, no `port_mod_*` flag. Game C is touched only under `#ifdef PC_PORT`
  with short hooks; new battle state goes into `game_savestate` (`port/game/game_mods.c`). Disc data is never edited or
  versioned: enemies (`SDIGIEDT.PRO`) and texts (`ES*.BIN`) change through port override tables, techniques and Digimon
  (`src/main/records.c`, matching source) through a patch applied at port boot. Fractions are fixed point (the game uses
  no float).
- **Vocabulary.** Elements, fixed order: Fire, Water, Ice, Wind, Thunder, Machine, Dark = `resists[0..6]`. In a technique
  `RecordsTechnique.defense_stat` 2..8 is the element (`resists[defense_stat - 2]`), 1 = none; `RecordsTechnique.element`
  is the class (2 physical, 3 magic, 4 heal, 5 buff/debuff, 6 field). Stats: `stats[0]` Strength, `[1]` Defense, `[2]`
  Spirit, `[3]` Wisdom, `[4]` Speed. `side == 0` is the player attacking; `fightstg_rules.stats[0]` is the attacker and
  `[1]` the target. Only the final boss has `state.type == 6`; other bosses are recognised by `records_state.blocked[]`.
- **Balance:** the numbers are constants of `definitive.h`. If a boss falls in 2-3 turns, **raise boss HP first** (a
  constant multiplier on `RecordsEnemy.hp` of boss fights) and only then touch magic; the `[power/2, 2 x power]` clamp of
  `get_special_damage` is the last resort. Rule checks are host tests with calculation, not play.
- **The battle log (`DW3_BATTLE_LOG=<file>`).** One plain-text line per event (`definitive.c`
  `LOG`): `battle` (id, kind, type) with a line per party member and per enemy (digimon, level, HP, MP, scale); `turn`
  (side, number, member); `action` (side, kind, technique, class, element, power, MP); `damage` (value, the target's HP
  before and after) with a `mult` line (the multiplier and its terms: weak, open, resisted, affinity); `end battle`
  (result, party turns, enemy turns); plus the definitive events: `hit target N weak W stagger S -> S' extra_pending E`,
  `extra action granted`, `baton pass`, `relay swap`, `stagger closed by the enemy's turn`, `libra`, `resist down`,
  `overfinish damage`, `twice E Pe P` (the "Enemy x2" test), `blast gauge`, `mod` lines. The battle code calls
  `def_log_begin`, `def_log_turn`, `def_log_action` (`fightstg_80086A00.c`, `wfightmn_800A6440.c`) and `def_log_end`.
  Chain, window, close and free swap can be
  checked from this log.
- **Sound and turn queue.** The list menu is unreachable. Three distinct sounds (weak, neutral, resisted) are picked
  from effects the battle already loads. The INFO panel's turn queue (`def_turn_queue`, read once per menu) shows two
  enemy turns first when `E + Pe < P`, where E is the enemy's remaining turn delay in the queue, Pe = `get_delay(0x10,
  0)` and P the player's remaining delay after the current action (`get_delay(0, 0)`); `get_delay` of type 0 draws
  nothing but **overwrites `fightstg_rules.stats[]`**, so call it only with the command menu open, never in the middle
  of a damage calculation.
- **Enemy table** (`port/game/definitive_enemies.c`, hand-maintained). The rules it follows:
  - archetypes by family: plain 2 weak + 1 strong, wall 1 + 2 (about 20% of the mobs of level <= 30), late 2 + 2 (level
    >= 45), boss 2 + 2 and final boss 1 + 3 (plus its Dark mark); Weak = 60, strong = 300 (or the disc's own value from
    250), neutral 100..115 (the current value when already there), the same for both elements of a pair;
  - the "house" family of a species (the family of its techniques' commonest element, else of its one high resist, else
    of its type) is never a weakness and is the natural strong resistance; a plain species that already has exactly two
    weak families keeps them, otherwise they are filled by opposition to the house element, then by balance, so the
    count per family evens out and two species of one battle do not share a pair;
  - **Dark:** weak only to Dark: BK Seraphimon, MagnaAngemon, Knightmon, Taomon (ids 447, 452, 178, 453); Dark as an extra
    weakness: Valkyrimon and two of the three Galacticmon phases (269, 442, 467; the second one, 466, resists it); the only nine that resist Dark: Galacticmon's second phase and
    Armageddemon, VenomMyotismon, LadyDevimon, Phantomon, Mummymon, Ghoulmon, Gargoylemon, Apokarimon. The rule: resisting
    Dark is as rare as being weak to it so Dark stays a useful element;
  - `def_weakness_mask` marks the whole family when one of its values is at `DEF_WEAK_MAX` (80) or less, with **no median
    and no cap**: a Break that creates a third weakness hides no natural one.
  - where the table enters: `fightstg_enemy_get_record` (`fightstg_8008D3B4.c`, found by `fightstg_enemy_find_record` in
    file 0x1CF) calls `def_enemy_override` on the record in memory, so the disc's SDIGIEDT is never edited; the field's
    own SDIGIEDT reads call it too (`definitive.c`), and a purification's dark form (no disc record) comes from
    `def_purify_record`. A row of `def_enemy_rows[]` is `{species id, 7 resists, dark}`; `dark` is the Dark mark, kept
    in the port's table because the disc record has no such field (`dark_mark`), and `def_weakness_mask(resists, dark)`
    adds Dark as an extra weakness outside any count. Stagger and chain state is a `DefChain` in `definitive_mask.c`
    (`staggered[3]`, `extra_pending`, `next_extra` / `cur_extra`, `window_used`, `swap_used`, `relay`); a Baton Pass or
    a Relay swap sets `extra_pending`, so the free swap keeps the turn.
- **Stagger.** One flag per enemy (`DefChain.staggered[3]`), unrelated to HP. It opens when the player hits a weakness with
  an elemental technique (`defense_stat >= 2`); a physical blow with no element and a crest's element
  (`attack_element`) never open it. It closes when the staggered enemy takes the next player blow (which still gets the
  bonus) or acts. A multi-hit technique counts as one blow. The next player turn is rescheduled at command time
  (`wfightmn_party_turn`, end of step 2), before the blow resolves, so on a hit set `extra_pending` and when the action
  ends find the player's turn in the queue (`fightstg_events.find(2)`) and zero its delay (check the tie rule of
  `fightstg_events_take_next`). With 2 actions from speed and 2 weaknesses the chain is up to 6 actions: that is
  design, not a bug. The chain is the player's only; the enemy gets no extra action (`def_action_resolved` returns at
  `side != 0`, `def_chain_enemy_acts` only closes the stagger, and `extra_pending` is set only by the player's hits and
  free swaps in `definitive_mask.c`). The staggered pose comes from the flag on the enemy side (never from HP; the
  low-HP pose is removed on the player side), and the final boss shows its tired pose when `final_phase || staggered`.
  The Tag button shows a different icon while a free swap is available. A save state in the middle of a chain restores
  equal.
- **Multiplier cases** checked by the host test (`tests/port/definitive_driver.c`): neutral 1.0; weakness 1.5;
  weakness + stagger with full Fire affinity 1.0 + 0.5 + 0.2 + 0.4 + 0.4 = 2.5; the Water + Ice pair with 999 + 0 gives
  0.4 of affinity (half the pair's 0.8), 999 + 999 gives 0.8. The magic clamp
  `[power/2, 2 x power]` is the original's. Status rolls still read the player's resists as before.
- **Damage hook points.** With `def_on` the multiplier replaces the element step of `fightstg_rules_scale_damage`
  (physical, `def_scale_physical`) and the resist bands of `fightstg_rules_get_special_damage` (magic, `def_scale_magic`)
  when the player attacks. When an enemy's element hits the player the old scale stays, with the player counted at
  `DEF_PLAYER_REF_RESIST` (100): physical `value + value * power * 2 / 100`, magic unchanged (the old bands at resist 100
  are 100%). Defense against magic is `def_magic_defense` in the same function. The staggered pose is set
  through `wfightmn_update_idle_anim`; the Baton Pass is a row of the swap menu (`fightstg_switch_menu_update`).
- **Techniques.** `records_techniques[]` is global: re-aiming a technique changes every form and enemy that uses it, so
  list who uses an ID first. Remade: 202 Speed Up -> Libra, 203 Mega Boost -> Relay (50 MP), 207 Slow Down -> elemental
  resist debuff, 208 Mega Break -> stronger or wider. Inheritance stays as it was (Digi-Lab mark, +20% MP, no new
  limit). See `patch_techniques` (`definitive.c`) and `def_mod_tech_spec`.
- **Texts.** The status menu's "Tolerance" of the 7 elements became affinity; text that described Spirit as defence
  was corrected (`ESSTATUS` 92-98, `ESTRAINI` 29+).
- **MP originals** (start MP -> new, gain per level -> new): Kotemon 100 -> 140 and 55 -> 57; Kumamon 10 -> 140 and 30 ->
  57; Monmon 80 -> 140 and 45 -> 57; Veemon 150 (gain 50 -> 57); Guilmon 130 -> 140 (gain 60). Forms have no MP of their own:
  MP is the rookie's (`RecordsDigimon.start_mp`, `hp_mp_gains[1]`). It is applied by a patch at port boot (`def_mp_hook`);
  `tools/convert_save_mp.py` uses the same table, keep both in sync.
- **Bestiary save:** the card file has 4 blocks and the game used up to 0x7800, the last 0x800
  were never written, so one 0x100 record per slot went there; absent, unknown version or bad checksum reads as all zero.
  The slot tail is not usable: 0x26C4..0x26EB holds the map state (`include/gamestate.h`), 0x26EC..0x26EF the MP converter marker. Start (bit 0x8,
  free in battle) opens the battle memory; Inspect respects the same table. Wiring: part 5 of `memcard.c`
  (`DEF_BESTIARY_PART`), read by STGMCARD step 0x48 (`def_bestiary_record_read`; any failure reads as an empty table,
  never an error screen) and written by step 0x36 after the slot (`def_bestiary_record_write`); the record is "BEST",
  version, XOR checksum and the 193-byte table (bits 0..6 = element learnt).
- **Boss immunity.** `records_state.blocked[12]` per fight: indexes 0-4 and 7 are status and still count;
  8, 9, 10 are Strength, Defense and Speed reductions, which the definitive edition ignores (the effect enters, scaled by
  `DEF_BOSS_DEBUFF_PCT / 100`, see `def_stat_blocked`); do not touch 5, 6 and 11 (11 is "no escape"). The hunt's boss-fight
  Rare Targets may carry `blocked[11]`, which is why the flee roll is forced before checking it.
- **Buff reaching magic.** The engine has 3 modifiers per member (`modifiers[0]` Strength, `[1]` Defense,
  `[2]` Speed); Spirit has none. The fix is in `get_special_damage`, not `get_stats` (for an enemy Spirit is magic attack
  and defence at once): the attacker's Spirit is scaled by the Strength modifier ratio, the target's magic-defence stat
  (Defense on the player, Spirit on an enemy) by the Defense modifier ratio; the player's Defense already comes modified, so
  it is not applied twice; the final boss (`type == 6`) keeps its own Defense/Spirit adjustments.
- **Training facts.** A round succeeds with chance 75 (+3 or +6 with the training item). Trainings
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
- **Blast gauge:** in the original `records_state.gauges[8]` (s16, 0..1000 per Digimon) lived only in RAM and was
  cleared at the title (`records_clear_gauges`, `stdwtitl_80085CD4.c`); the port saves it in the slot tail at +0x26F0 (an
  out-of-range value reads as 0, an old save loads with 0, a new game still zeroes it; also in `game_savestate`).
- **Digivolution requirements.** Each partner has a list of 44 forms (`stfgtrep_learn_lists[digimon]`,
  `StfgtrepLearn`): up to two required forms with a minimum level (`requires[i] = {form, level}`) and one minimum stat
  (`stat` 1-6 the six stats, 7 the Digimon's level, 8-14 resistances; `min`), checked in `stfgtrep_learn_technique`. The
  Digi-Lab panel is read only, shows one step ahead (`DEF_EVO_HINT_DEPTH` 1; 0 off, 99 all), and the form is still learned
  at the next level-up as in the original.
  The panel lists the forms in reach with their requirements met / total ("Ready" in green when all are met), and for
  the chosen form one block per requirement with its current value against the goal, green when met (`EH_PAL_MET`).
- **Technique tiers** (`def_mod_tech_spec`): damage techniques with a side debuff are tier 1 for 2 turns; dedicated
  single-target ones tier 2 for 3-4 turns; strong or group ones tier 2 for the group or tier 3 on a single target.
  Nothing gives an extra turn (the chain already does).
- **XP rules:** party share 100 (a knocked-out member gets no shared XP, no level-gap bonus); EXP and Bits 1x, form EXP
  14x (disables the original form-EXP cap), all forms on, card boosters 15 per drop (`DEF_XP_PARTY_SHARE`,
  `DEF_XP_FORM_MULT`, `DEF_XP_ALL_FORMS`, `DEF_CARD_BOOST_PACKS`). `all_forms` credits the three chosen forms of each
  member. An old `settings.json` with the former mods' keys still works (unknown keys are ignored). Rules:
  `docs/MECHANICS.md` section 6.
- **Teleport:** the Guardromon destination uses each map's standard start coordinates.

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
- Bosses take debuffs in full (`DEF_BOSS_DEBUFF_PCT` 100); status lands on them at a reduced chance (see below).
- Every description in `SKLINF` states effect, tier and turns (34 characters at most), e.g. "User Atk+2 (phys/mag), 3
  turns.".
- Tests: `tests/port/definitive_buffs.py` (+ driver) checks the tiers, the no-stack rule and the expiry.

## Status, Poison and revive rework

### Status and poison, for the player

**Will it land?** Every Poison, Sleep or Confusion technique has a base chance. Then:

- **Wisdom against Wisdom.** For every 8 points of Wisdom you have over the target, the chance goes up by about 0.8%
  (1 in 128); for every 8 points under, it goes down as much.
- **The target's resistance.** Its resistance to that status, plus its value in one element family, takes the chance
  down by 0.8% per 8 points. Poison and Sleep read the target's **Water + Ice** family and Confusion its **Wind** family,
  so an enemy weak to Water + Ice (60) is much easier to poison or put to sleep than one that resists it (300), and a
  Resist Down on that family helps your status too.
- **Stagger first.** On a staggered target the chance is multiplied by 1.5 and then gets about 19% more (24 in 128).
- **Bosses** used to be immune. Now Poison, Sleep and Confusion land on them at 60% of the chance, and never above 78%,
  stagger or not. Paralysis still never lands on a boss.
- Nothing is ever certain or impossible: the chance stays between 1 and 127 in 128.

**Poison** is now damage that bites every turn, fixed when it lands:

> damage per turn = the technique's power ÷ 2 + Wind affinity × 1000 ÷ 6 + your Wisdom ÷ 4 + your Spirit ÷ 4

(each 0.1 of Wind affinity is about 17 more a turn). It never takes more than a quarter of the target's max HP in one
turn, and on an enemy never more than 8% of its max HP, so a poison build wears a boss down instead of melting it.

**Sleep and Confusion** hold an enemy for two of its turns, and any hit wakes it. A sleeping enemy gets back 6% of its
max HP each turn, and a confused one comes out of it angry: Attack +2 (+35%) for three turns. They are for buying a
heal or setting up the stagger, not for the damage turn.

**What feeds what:** Wisdom decides whether a status lands, adds to poison damage and powers your heals; Spirit adds to
poison damage (and is your magic attack); your Wind stat adds to poison damage through its affinity (and to your Wind
magic).

**A worked example**, checked against the port's code (`def_status_chance_calc`, `def_poison_dot_calc`,
`def_poison_tick`). Your Digimon has Wisdom 400, Spirit 300 and a Wind stat of 600 (Wind affinity 0.4 × (600 ÷ 999)² =
0.144). It casts a poison spell of power 127 and base chance 112 in 128.

| | Calculation | Result |
|---|---|---|
| Poison per turn | 127 ÷ 2 + 144 ÷ 6 + 400 ÷ 4 + 300 ÷ 4 = 63 + 24 + 100 + 75 | **262** |
| ... on an enemy with 3000 max HP | capped at 8% of 3000 | **240** |
| ... on an enemy with 5000 max HP | under its cap of 400 | **262** |
| Chance on a common enemy (Wisdom 300, poison resistance + Water value 150) | 112 + (400 − 300) ÷ 8 − 150 ÷ 8 = 112 + 12 − 18 = 106 in 128 | **83%** |
| ... staggered | 106 × 1.5 + 24, capped at 127 | **99%** |
| Chance on a boss (Wisdom 738, resistance sum 163) | (112 − 42 − 20) × 0.6 = 30 in 128 | **23%** |
| ... staggered | 30 × 1.5 + 24 = 69 in 128 (under the boss cap of 100) | **54%** |

Stagger the boss first and the same spell lands more than twice as often.

### The formulas (reference)

A level-50 Digimon by level-ups alone has median Wisdom 338, Spirit 316 and max HP 3383; training gives about 4.1 points
per TP.

- **Chance** = value + (attacker Wisdom - target Wisdom) / 8 - resist_sum / 8, out of 128. On a boss x0.6
  (`DEF_BOSS_STATUS_PCT` 60); on a staggered target x1.5 + 24; a boss cap of 100/128; the result is clamped to 1..127.
- **Poison** damage per turn = power / 2 + Wind affinity / 6 + Wisdom / 4 + Spirit / 4, capped at max HP / 4 and, on
  enemies, at 8% of max HP.
- **Sleep and Confusion** hold an enemy for 2 turns and end on a hit (`def_wake_on_hit`). A sleeping enemy regenerates 6%
  per turn. After confusion ends the enemy gains Attack +2 for 3 turns.

### Revive items

| Item | Chance | HP returned | Rated max HP | Price |
|---|---|---|---|---|
| Revive Disk (item 67) | 40% | 30% | 1500 | 2000 buy, 1000 sell |
| Life Disk (item 70) | 65% | 60% | 4000 | 10000 buy, 5000 sell |
| Max Charge (item 46) | 100% | 100% | 9999 | 3000 sell |

The chance scales down by rated max HP / the target's max HP when its max HP is above the rating (never under 15%,
`DEF_REVIVE_MIN_CHANCE`; `def_revive_chance`). By the fallen ally's max HP
(computed from that rule):

| Max HP | Revive Disk | Life Disk |
|---|---|---|
| 300 | 40%, 90 HP | 65%, 180 HP |
| 800 | 40%, 240 | 65%, 480 |
| 1500 | 40%, 450 | 65%, 900 |
| 2000 | 30%, 600 | 65%, 1200 |
| 3000 | 20%, 900 | 65%, 1800 |
| 4000 | 15%, 1200 | 65%, 2400 |
| 6000 | 15% (floor), 1800 | 43%, 3600 |

Max Charge is 100% at any max HP. On a 3000 max HP ally a success costs about 10,000 Bits in Revive Disks (5 of them)
or 15,400 in Life Disks (1.5).

**Healing:** heal = power x 64 + Wisdom x power / 8 (`fightstg_rules_get_heal`, cap 9999), so Wisdom is the healer's
stat too: technique 0xB8 (power 8, 16 MP) heals 762 / 1112 / 1511 at Wisdom 250 / 600 / 999; 0xB9 (power 32, 74 MP)
3048 / 4448 / 6044; 0xBA (power 127, 120 MP) 9999 at any of them.

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
