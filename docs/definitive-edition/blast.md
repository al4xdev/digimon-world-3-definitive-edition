# Blast, Purify and Chosen Evolution

[Technical reference index](README.md) · [Player README](../../README.md)

## Blast, Purify and Chosen Evolution

Code: `port/game/definitive.c` and `definitive_purify.c`.

### The gauge

- Gain per hit: WEAK +150, stagger +100, OVERFINISH +200, an extra action +100; using an item or fleeing -100. A plain hit
  does not fill it, and nothing triggers it automatically.
- Activation is the battle menu's 8th action **Blast**, R2 by default (the launcher has a BLAST row), and the tab's own
  fill is the gauge's bar: a dark slot that fills from its left end as the gauge charges, lit cyan with a dark word when
  full. It needs a full gauge
  (1000), no Blast already active, HP, and no status 8 or 0x20.
- Price: HP and MP at 1 after the battle, and -25% Power/Guard/Speed through `GamestateStats.penalties` (cleared only by an
  inn's `inn_heal_party`).
- The 5 dark forms per partner come from the game's `RecordsDigimon.blast_forms`. They advance in order with step levels
  {1, 10, 25, 35, 40} and repeat the last reached when the level is too low. `DW3_BLAST_STEP` forces a step in tests.
  Forms are painted dark (`DEF_RECOLOR_DARK`).
- **Overheat** (Blast and Chosen alike): when the form ends the Digimon is back in Rookie and sealed from digivolving for 3 of its own turns.
  The turns left are saved with the partner (high nibble of its `REC2_BLAST` byte) and follow it into the next battles, which it starts
  sealed in Rookie; only an inn's rest (Gatomon) clears it, or Gennai for the Chosen Digimon. While it lasts the Digimon also starts the battle as its
  Rookie (not its shown form) and the Tag menu offers it only as its Rookie (`def_blast_heated`).
- **Boost** (`def_blast_boost_pct`, applied to the five stats by `def_blast_boost_stats` in `fightstg_rules_get_stats`'s party
  side): while the quest is DECEIVED the false Gennai (Myotismon) sells the Blast a stat boost in his panel: one step goes on
  sale with each partner+form recorded (up to the quest's ten), the k-th for k x `DEF_BLAST_BOOST_BITS` (1000) Bits, each
  +`DEF_BLAST_BOOST_STEP` (3)% to the Blast form's stats, so +30% at ten (texts `$PfBoostOffer`, `$PfBoost*`). The steps bought
  are saved (`blast_boost`, record 2). The Chosen Evolution has `DEF_CHOSEN_BOOST_PCT` (50)% from the start.
- **The 999 cap:** the Blast's boost stops at 999 (a stat already past it keeps its value); only the Chosen boost
  (+50%) goes up to 9999 (`def_blast_boost_stats`, the bound keeps the damage formulas in range). Not yet seen in a
  real fight.
- **Blast TP cost** (`def_blast_started` / `def_blast_ready`): every non-blessed Blast costs 10 TP (`DEF_BLAST_PLUG_TP`,
  refunded up to 99 at the end of Gennai's quest), so `R2` requires 10 TP (unless blessed).
- **DV Plug** (item 76): fills the gauge and triggers at once (or revives the last fallen partner at 1 HP / 1 MP with
  a full gauge in Last Stand); after the quest it revives to
  Rookie at 1 HP / 1 MP: it goes through the game's revive path (item 0x46, `fightstg_8008D3B4.c`), which revives every fallen
  partner, each as its Rookie with the Blast flag off (`def_blast_plug_revive`; a partner still overheated stays sealed); its
  description is then `$PlugRevive` instead of `ITMINF` 76 ("Revive: Rookie form, 1 HP and 1 MP" / "Revive em Rookie com 1 HP
  e 1 MP"). Before the quest it is still the curse.
- **Temptation:** a full gauge with a knocked-out partner, or the acting Digimon at <= 25% HP, makes the Blast tab
  call: it is lit bright cyan with the word dark, and it shakes sideways in bursts (8 ticks in 24); red is kept for a
  refusal, so the temptation is never read as one. It says nothing else: the tab itself is the call.
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
Seraphimon. "Chosen needs Rookie" shows in the menu. A press on the grey Blast button opens a box in the Inspect panel's
place saying why (`def_chosen_refusal`, checked in this order): another partner is the Chosen one, it was already used this
battle, the Digivice is out of charge (Gennai), the level is under 45, or the Digimon is not in its Rookie form. Gennai's
panel states the rule under the partners: Rookie form only, and the Mega past 999 (Limit Break). Test:
`tests/port/definitive_quest.py`, `definitive_gauges.py`.

### How it hooks into the game

- **The original game's Blast:** `wfightmn_add_gauge` filled `records_state.gauges[Digimon id]` (0..1000) only with
  damage taken; at 1000 event 0x12 ran `wfightmn_blast` (message 0x61 "Blasted!", form `blast_forms[level band]`, full
  HP, `blasted = 1`, end scheduled by event 0x13). **Pitfall:** the `wfightmn_handlers` table is shifted: the step is
  the event type **+ 1** (event 0x12 -> step 0x13; step 0x12 is `wfightmn_seal_end`).
- **The price** is `GamestateStats.penalties` (`def_blast_started` in `wfightmn_blast`; `def_blast_price` after the
  HP/MP write-back at the battle's end). The original never writes that field; only `inn_heal_party` zeroes it and the
  status screens already mark a penalised status. A second Blast before an inn does not deepen it.
- **DV Plug** is `def_blast_plug` (effect 0x4C of `fightstg_8008D3B4.c`): one use fills the gauge and calls event 0x12.
  It costs no TP (the TP is the Blast's). Texts: "Fill Blast; revives last fallen." and the Last Stand notice
  `$PlugLastStand`.
- **Activation:** menu action 8 (choice 7 -> command 7, `DEF_BLAST_COMMAND_ID`, step 0x13) spends the turn like a
  digivolution; the tab is greyed and refused otherwise (`tests/port/mods.py` expects "R2: refused 7" with an empty
  gauge). The launcher (desktop and Android) has the BLAST row (`H_BLAST`, `bm_blast`, default R2); psxstack's reads
  `mod.json`.
- **Gain:** `def_action_resolved` -> `blast_add` (pure: `def_blast_gain`; numbers `DEF_BLAST_*`), summed per blow that
  lands; item and flee cool it (`def_blast_command`); nothing fires by itself (`def_blast_auto`). Temptation threshold
  `DEF_BLAST_TEMPT_HP_PCT` (25); `def_member_knockout` flags a partner fallen in this battle.
- **Forms:** step levels 1, 10, 25, 35, 40; after the fifth form the sequence starts over.
- **Quest data:** each form's first status (stats, HP, MP; the species gives the techniques) and the best status it ever
  had are saved in a third card record. The quest needs 10 distinct partner+form pairs (the same species on two partners
  counts twice, so two partners with all 5 close it, as on Hard Mode). The fee is `DEF_PURIFY_BITS` (10000). A defeat in
  the simulator returns to the field with HP/MP as before and the beaten forms stay purified. The Megas are
  `DEF_CHOSEN_MEGAS`. The dark paint is luminance 40% with a slight purple tone (party model id 1 while in Blast,
  `def_blast_dark`; the simulator's enemies id 2); the Chosen comes out in its normal colours. The story is explained in
  `definitive_purify.c`'s header.
