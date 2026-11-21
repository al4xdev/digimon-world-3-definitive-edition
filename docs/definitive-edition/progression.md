# Progression and difficulty

[Technical reference index](README.md) · [Player README](../../README.md)

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
- **Perfect and Mega wait for the level**: a Perfect form needs the Digimon at level 20, a Mega and its variations at 40, however fast the form levels. The Digi-Lab shows it as a "Level" line.
- **Short animations**: a form's digivolution plays in full the first time, then a short version; hold Circle or Cross
  to skip it, even the first time.

### Hard Mode

Choose it on New Game: your party is two Digimon instead of three. The third partner of the starting pack stays in the
lab, and the lab keeps the third slot closed. The choice is saved with the game.

## Super Hard Mode (the Demon Bell)

- **Super Hard** is the **Demon Bell**, key item 26, held in the bag. Guilmon in the [Velvet Room](velvet-room.md#the-velvet-room)
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
