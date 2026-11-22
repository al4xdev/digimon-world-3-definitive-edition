# Blast, Purify and Chosen Evolution

[Technical reference index](README.md) · [Player README](../../README.md)

## Blast, Purify and Chosen Evolution

**Status: done and committed** (`8c0971b`; the overheat that follows the partner in `ee6fd02`). `definitive.c`,
`definitive_purify.c`. The Blast Gauge was redesigned.

### The gauge

- Gain per hit: WEAK +150, stagger +100, OVERFINISH +200, an extra action +100; using an item or fleeing -100. A plain hit
  does not fill it, and nothing triggers it automatically.
- Activation is the battle menu's 8th action **Blast**, R2 by default (the launcher has a BLAST row). It needs a full gauge
  (1000), no Blast already active, HP, and no status 8 or 0x20.
- Price: HP and MP at 1 after the battle, and -25% Power/Guard/Speed through `GamestateStats.penalties` (cleared only by an
  inn's `inn_heal_party`).
- The 5 dark forms per partner come from the game's `RecordsDigimon.blast_forms`. They advance in order with step levels
  {1, 10, 25, 35, 40} and repeat the last reached when the level is too low. `DW3_BLAST_STEP` forces a step in tests
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
- **Blast TP cost** (`def_blast_started` / `def_blast_ready`): every non-blessed Blast costs 10 TP (`DEF_BLAST_PLUG_TP`,
  refunded up to 99 at the end of Gennai's quest), so `R2` requires 10 TP (unless blessed).
- **DV Plug** (item 76): fills the gauge and triggers at once (or revives the last fallen partner at 1 HP / 1 MP with
  a full gauge in Last Stand); after the quest it revives to
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
Seraphimon. "Chosen needs Rookie" shows in the menu. A press on the grey Blast button opens a box in the Inspect panel's
place saying why (`def_chosen_refusal`, checked in this order): another partner is the Chosen one, it was already used this
battle, the Digivice is out of charge (Gennai), the level is under 45, or the Digimon is not in its Rookie form. Gennai's
panel states the rule under the partners: Rookie form only, and the Mega past 999 (Limit Break). Test:
`tests/port/definitive_quest.py`, `definitive_gauges.py`.

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
  A second Blast before an inn does not deepen it. **DV Plug** (`def_blast_plug`, effect 0x4C of `fightstg_8008D3B4.c`): one use fills the gauge and calls the game's event 0x12; it costs no TP
  (the TP is the Blast's, see above); text "Fill Blast; revives last fallen." and its Last Stand notice (`$PlugLastStand`).
- **Activation details:** menu action 8 (the choice 7 -> command 7, `DEF_BLAST_COMMAND_ID`, step 0x13, spends the turn like the DV); dark star icon (`01 36`) under L1;
  greyed and refused otherwise; `tests/port/mods.py` expects "R2: refused 7" with an empty gauge. The launcher (C, desktop and Android) has the BLAST row (`H_BLAST`, `bm_blast`,
  default R2; on Android the screen's rows went from 27 to 25 px to fit); the psxstack one reads `mod.json`.
- **Gain rule** (`def_action_resolved` -> `blast_add`, pure `def_blast_gain`; numbers `DEF_BLAST_*`): per blow that lands, WEAK +150, stagger +100, OVERFINISH +200, a blow in an
  extra action +100, summed; item and flee cool -100 (`def_blast_command`); taking damage does not fill; nothing fires by itself (`def_blast_auto`). Temptation threshold
  `DEF_BLAST_TEMPT_HP_PCT` (25), `def_member_knockout` flags a partner fallen in this battle.
- **The owner's redesign, kept as written (2026-11-17, afternoon):** forms from the game's `blast_forms`, in sequence per partner (form 1 level 1, 2 level 10, 3 level 25, 4 level 35,
  5 level 40; originally 15 and 45, moved to 10 and 40 in 0.8.12, else repeat the last reached; after the 5th back to the start). **Recording:** the **first** status of each form (the Digimon transformed the first time: stats, HP, MP;
  the species gives the techniques) and the **best** status it ever had; it needed a third card record (the second did not fit). **The quest:** 10 = 10 distinct partner+form pairs (the same
  species on two partners counts twice, so two partners with all 5 close it: works on Hard Mode); 1) the old man of the room introduces himself as **Gennai** (really Myotismon in disguise),
  only shows how many forms you released and tells you to master them, no quest; 2) at 10 he **charges the Bits** (the fee), reveals he tricked you and that your Digimon is cursed; black screen
  and a sound; the real old man appears, he is Gennai, the other was Myotismon, and he believes you are a DigiDestined; 3) Gennai opens the **simulator**: the 10 forms in sequence (waves of
  3 chained, no results screen between), each with the **first recorded status - 10%** and the species' techniques; 4) winning: **all TP** the Blast took back; **a permanent buff per
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
