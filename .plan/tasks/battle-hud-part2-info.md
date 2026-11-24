# Battle HUD, part 2 of 3: the INFO (weakness) panel

Part 1 (command HUD) shipped. Part 2 is the INFO panel: same look as part 1, the enemy personas shown for what they
are, explained by a legend in the panel itself (no separate help screen). Part 3 (player side, Limit Break, Tech/Item
menus) may never happen: **do not design part 2 around it.** Out of scope: the bestiary screen (its own weakness
view, `def_bestiary_state`) and the `SHIFT!` callout itself, which stays as it is.

The game is stable and in production: every step here is drawing and reading state, never battle logic.

## Read before implementing: the traps

1. **`def_enemy_acts_twice()` has a side effect.** It calls `fightstg_events.get_delay`, which rewrites
   `fightstg_rules.stats`. It is only safe where it is called today: once, at the battle menu's creation
   (`data->twice`, fightstg_8008D3B4.c). Store that value and reuse it. Never call it per frame, from the panel's
   draw, or on opening INFO. Calling it at the wrong time corrupts the stats of the damage being calculated.
2. **Never call `def_persona_enemy_turn` / `def_persona_staggered` to read state.** They advance the phase. Read
   `def_persona_phase` and the slot's turns only.
3. **Guard: read `enemy_guarded`'s rule, do not rewrite it** (Time Room trials, the final boss, stagger all live
   there).
4. **Draw and read only.** No change to battle logic, the persona table, or the SHIFT callout.
5. **No `hud2_try_*` global may reach a commit** (grep before committing).

## Why

Even the owner was surprised by a `SHIFT!` on a Maré enemy (GUARD_A + PHASES(2)): the phase flips its guard, not its
weakness, at the start of the enemy's turn, so the callout pops as it attacks. Not a bug (`persona_phase_changed`
fires on every phase change, definitive.c), but the panel never says what kind of persona the enemy is, which phase it
is in, or when it changes. Today a persona is only a `!` before the name.

## Delivery 2a: the panel (layout + information) — tested together, owner + Claude

```
┌─ ! KIMERAMON ────────────── FASE A  x2 ─┐  title + live state (phase, "Enemy x2")
│  [◐ ⟳2]  [⏩3x]  [DEF]                    │  traits row: mold, SPEED_X, DEF_PCT
├──────────────────────────────────────────┤
│  🔥     RESIST     │  ⚡ ⚙    ???         │
│  💧 ❄   WEAK       │  ✳       ???         │  today's grid, restyled
│  🪽     ???        │                      │
├──────────────────────────────────────────┤
│  ◐  SHIFT: a fraqueza muda                │
│     a cada 2 turnos                       │  legend: only THIS enemy's traits,
│  ⏩  velocidade 3x a sua                   │  same icon as in the traits row
│  DEF ...                                  │
└──────────────────────────────────────────┘
```
(Kimeramon's real row: Camaleão + Relâmpago, `PHASES(2)`, `SPEED_X(3)`, `DEF_PCT(60)`. The wording is illustrative.)

- **One icon, one meaning:** a trait's icon in the row is exactly the one that opens its legend entry. A mold's
  sentence carries its own phase rule (⟳n, on stagger) and its guard, so those get no line of their own; a trait
  outside any mold (SPEED_X, DEF_PCT, a guard with no mold) gets one entry.
- **SPEED_X and "Enemy x2" are different things, both shown, not merged** (checked in definitive.c): SPEED_X is a row
  trait (its Speed is the party's times n, `def_turn_speeds`), permanent; "Enemy x2" (`def_enemy_acts_twice`) is a
  live prediction for *this* turn (E + Pe < P) and fires for any fast enemy, persona or not. So: SPEED_X is a trait
  with a legend entry, "x2" is a state badge in the title line next to the phase, with its own glyph (not ⏩).
  **Risk:** `def_enemy_acts_twice` calls `get_delay`, which rewrites `fightstg_rules.stats`: it is safe only with the
  command menu open and nothing being calculated. Read it once where it is read today (the menu's creation,
  `data->twice`) and reuse that value; never call it per frame or from the panel's draw.
- **Traits row:** the mold (Camaleão, Reflexo, Maré, Fechadura, ...), guard, SPEED_X, DEF_PCT, and enemy-side states
  that float on screen today (**"Enemy x2" moves into the panel**, as the title-line badge above). Any other enemy-side label found later
  goes here too. **LIMIT BREAK stays where it is** (both sides use it; part 3's business).
- **Phase A/B + turns left (⟳n)** from the slot's phase/turns (definitive_persona.c). This is what makes SHIFT expected.
  Only two-phase rows show it; ⟳n only for `PHASES(n)` (an `ON_STAGGER` row has no countdown: its legend says
  "on stagger" instead).
- **Guard icon follows the live state**, not just the row: lit when guarded now (`GUARD_A` only in phase A,
  `GUARD_ALWAYS` in both), off while the enemy is staggered (`enemy_guarded` in definitive.c is the rule to read; do
  not duplicate it).
- **"Enemy x2" leaves the middle of the screen** (owner, 2026-11-24): it is shown only in the panel, accepted that a
  player who never opens INFO does not see it.
- **Legend (the only explanation: no SELECT help screen, owner 2026-11-24):** one entry per trait the enemy has, each
  saying what SHIFT means for that mold. Values come from the row (`PHASES(n)`), never hard-coded. The mock's wording
  is illustrative: the real strings are written against the pixel budget below.
- **Spoilers:** the mechanic is shown, the values are not. Already how it works: knowledge is per phase
  (`slots[].known[map]`, `def_persona_known`), so phase B reads `???` until learnt. No new logic.
- **Size:** the panel is optional (INFO toggles it), so it may grow **down** over the player's Digimon (bottom edge
  inside the 240-line screen with the most traits + legend lines a row can have: measure on the worst row). Width stays
  about today's (138 px, `FIGHTSTG_INSPECT_W`). Do not widen it into the widescreen pad: there is a known widescreen
  seam / double-buffer line problem there.
- **Text:** translated (EN/PT/FR/DE). Budget in **pixels** (proportional font), measured, not guessed: the panel is
  138 px; minus frame and padding and the 13 px icon + gap in front of a legend line, the text gets about **110 px**
  (measure the net width on a capture before fixing `max_px_limit`). FR/DE run 20-30 % longer than EN/PT: write them
  first. At most 2 lines per entry.
- **Title line:** the longest enemy name in the game (with the `!` mark) + "FASE A" + "x2" must fit in 138 px without
  touching: find the longest by pixel width across all species and languages, capture it with `cap.py`. If it does not
  fit, the state badges move to the traits row, never a cut name.
- **Height, worst row:** title + traits row + grid (3 lines) + legend. The worst rows have a mold + SPEED_X + DEF_PCT
  (Impmon r15, Kimeramon): 3 entries x 2 lines. At 14 px a line that is about 160 px from y 75: it reaches ~235 of
  240. Measure on that row; if it does not fit, the legend's line height goes down to 11 px (the "Enemy x2" label's)
  or a non-mold trait gets one line. A host test measures every string in every language and fails over budget (pixel widths:
  `tools/translate_ptbr.py` already measures, `max_px_limit`).
- **Icons:** new 13 px glyphs, shapes clearly unlike the element icons (part 1's critic, issue 1: reused icons read as
  properties).

### 2a status (2026-11-24, branch `feat/battle-hud-2-info`, not committed: waiting for the owner's test)

Built and captured (`.dumps/tasks/battle-hud-2/shots/`, `final_sheet.png`, `sheet_lang.png`). What was measured and
decided while building it:

- **Title line:** the widest title is `! BK Imperialdramon XIV` (132 px, `bin/names_px.py`), so phase + "x2" cannot
  share it: per the rule above the badges moved to the traits row. "Enemy x2" (its translated label, Weak's colour) is
  right-aligned there; the phase is not text at all but the SHIFT picture's lit half (A left, B right).
- **Icons:** the font's extra glyphs are the disc's (`01 01..3A`: arrows, heart, note, the element and stat icons), no
  shield or phase among them and no way to add a glyph, so the four pictures are 11x11 pixel bitmaps drawn as flat quads
  on the panel's layer (`fightstg_inspect_picts`): SHIFT two halves, guard shield (dark while open), speed chevrons,
  lowered Defense a cracked block. ~25 quads each, no texture.
- **Countdown:** one pip per enemy turn left (`def_persona_turns_left`, a read-only accessor added to the engine).
- **Legend:** 14-px font kept (11 px not needed); the text's room is 116 px. SHIFT's entry is "SHIFT every n turns /
  on stagger" + what changes (new weakness, guard on/off, weakness + guard); the guard always has its own entry
  ("Takes only 30% / until staggered", DEF_GUARD_PCT), speed and Defense one line each. Worst row (Impmon r15: 6
  lines) ends at line 238. FR "après n tours" (the only short form that fits) and DE "Deckung" for the guard
  ("Abwehr" is the Defense stat there) are wording calls for the owner.
- **Host test:** `tests/port/inspect_info.py` (registered in `scripts/test.sh`): every language's lines with the
  table's largest values, lines per entry, panel bottom and traits row vs the widest badge, per persona row.
- **Read-only:** `def_inspect_info` reads `persona_at`, `def_persona_phase`, `def_persona_turns_left` and calls
  `enemy_guarded()` itself; `def_enemy_acts_twice` is still called once, at the menu's creation (`obj->twice`).
- **Not captured:** widescreen (a replay-made state has no `gpu_vram_ext`, a widescreen process refuses it); the code
  adds the pad to every window and quad as before.
- **Rig additions:** `bin/cap2.py` (persona row forced into `slots[0]`, `POKE_AT`, `STAGGER`, `FIGHTS`, `LANG2` copies
  a language's `def_<l>_str_Info*` into the live buffers right before INFO opens: a state load brings English back),
  `bin/panels.py` (panel contact sheet), `bin/legend_px.py` / `legend3.py` (widths).

## Delivery 2b (later): animations / style polish

Only after 2a is in players' hands and the information is right, **and only if the spike below says it is viable and
cheap.**

### Animation spike (parallel to 2a, opus subagent, throwaway worktree: nothing reaches main)

Reading and measuring only, while the owner and Claude build 2a. Questions:

1. Can the whole panel move frame by frame? Quads (frame, backdrop) surely: part 1 already animates the slide-in
   (`FIGHTSTG_BMENU_SLIDE` / `obj->age`) and the Blast shake. The doubt is the text: the panel is several
   `message_create_window` windows placed at creation; can they be moved every frame without being recreated?
2. Does a "pop" (comes out from inside, overshoots, settles fast) hold at 320x240 without jitter or blur?
3. Cost: `perf.py` (3000 vsyncs) with and without the animation. Matters on Android and handhelds.
4. Widescreen: does the motion cross the known widescreen seam line?

Method: the capture rig (frame sequences like `fight`/`techuse`, `sheet.py` contact sheets), curve variants
(duration, overshoot) as temporary globals captured in parallel; a `sonnet` critic judges the captures only.
Output: a short report (one verdict per question, cost in ms, and the engine's proposed shape) with sheets in `.dumps/tasks/battle-hud-2/spike/`.

**The goal behind it: one animation engine, not hand-made animations** (owner, 2026-11-24; the same reason the
persona table worked: one motor, rows of data, bugs in one place). The HUD will get about three new transitions: this
panel (2b), and the Tech/Item menus (part 3, if it happens). Today the only one is part 1's slide-in (`obj->age` /
`FIGHTSTG_BMENU_SLIDE`), written by hand. The spike also proposes the engine's shape:

- **A transition is data:** origin, destination, curve (out, overshoot, back), duration. The weakness panel declares
  "comes out of the INFO word, goes to its place, goes back into INFO when closed".
- **The origin is read, never fixed:** every action, INFO included, can be mapped to any of 8 buttons (launcher HUD
  screen, `"inspect"` in `port/mods/battle_menu/mod.json`), and its word is drawn at that button's spot
  (`fightstg_bmenu_spot`). Use INFO's current place (`obj->x[FIGHTSTG_BMENU_INSPECT]` / `obj->y[...]`). Capture with
  a remapped INFO (`HUD_CFG` + a settings file, like part 1's `settings_mixed.json`).
- **One clock:** the battle's ticks (`gfx_module.funcs.get_frame_ticks()`), as the callouts use, so battle speed and
  the animation mods apply to every transition for free.
- **Purely visual:** nothing the battle reads (as `definitive_callout.c`). A broken animation looks bad, never breaks
  a battle.
- **Out of this spike: moving the battle camera** (Persona-style focus). It touches the 3D camera that effects and the
  callouts' projection read: its own spike later, starting with who reads the camera.
- **Not the message queue** either: the panel is opened and closed by the player, it never competes with messages.

### The owner's pick (2026-11-24, storyboard in chat): variant A, "the frame moves, the text lights up"

The game's text windows only appear, vanish or move; they never stretch or fade. So the frame (quads) carries the
motion and each line of text appears when the frame's edge reaches it.

- **Open (~10 frames at 30 fps):** INFO's word blinks; a trace in the frame's colour runs from INFO to the panel's
  corner; the frame unfolds in width (overshoots ~4 px, settles), then in height; title, traits row, grid, legend light
  up as the edge passes them.
- **Close (faster, ~6 frames):** all text vanishes at once; the height folds into a bar; the bar shrinks and runs back
  along the trace into INFO; INFO blinks. (Open questions for the owner: keep the trace? close back into INFO or just
  fold away?)
- Rejected for now, variant B (the finished panel slides out whole): it crosses both Digimon, and text moving in
  whole pixels may jitter (spike question 2).
- **Internal resolution up to 8x (4K, `video.internal_scale`):** everything is placed in whole PS1 pixels (OT
  primitives and text windows take integers), so at 4K one step is ~9 screen pixels. Fast moves hide it; the slow
  part (the overshoot settling) will show steps: keep it to 2-3 frames, and look at it at 1x and at 8x.
- **Widescreen is any aspect from 4:3 to 16:9** (the picture follows the window: 16:10 gets 16:10), not just 16:9.
  The button block moves left by `pad * FIGHTSTG_BMENU_WIDE_SHIFT / 100` and the panel right by `pad`, so the trace's
  length changes with the aspect: origin and destination are read every frame, never a fixed distance. The trace
  crosses the seam between the 4:3 buffer and the wide canvas (spike question 4): capture at 4:3, 16:10 and 16:9.
- **Owner's answers (2026-11-24):** the trace stays (digital look, links the left button to the right panel) but it
  is gone as soon as the frame starts unfolding, so it never cuts across the enemy. Closing runs back along the trace
  into INFO (~5-6 frames); if the spike's contact sheet shows that as cluttered, simplify to folding in place.

### The panel lives beside the menu, not in its way: edge cases

`obj->inspect` (what is open) keeps changing at once, exactly as today; the animation is only a picture of it
(`anim_t` moving toward 0 or 1, in `get_frame_ticks()` ticks, not frames: fast-forward and frame skip stay right).
**No input ever waits for an animation.** The cases, from the menu's code (`fightstg_battle_menu_update`):

| case | today | with the animation |
|---|---|---|
| INFO (or Start, the battle memory: the same panel) again mid-open / mid-close | toggles | turns around from where it is (no jump, no restart) |
| a command chosen with the panel open or opening (`obj->press`) | closes it, the 7-tick nudge, then the menu object ENDs and its windows are freed | cut at once: a 5-6 frame close cannot outlive the object (the nudge is 7 ticks) and must not compete with the next screen |
| a refused (grey) command | closes it, shakes, and Blast's refusal puts the notice in the same place | cut at once when a notice comes (two things in one place); without a notice, the normal close |
| a buffered action (`port_bm_take_action`: pressed before the menu showed) | taken on the first RUN frame | INFO can open during the menu's own slide-in: the trace starts from where INFO is *now* (read every frame) |
| the menu comes back (a new turn, a Tech/Bag submenu cancelled) | a new object, panel closed | starts closed, no animation left over |
| INFO remapped to another button | panel opens | trace starts from that button (`fightstg_bmenu_spot`) |
| script / auto play (`port_script_active`) | presses at script speed | same as a player: never waits |

The spike drives each row with the capture rig (presses on chosen frames) and checks one frame after each press that
the menu did what it does today.

## Method

Same as part 1: own branch + worktree, UI critic (`sonnet`) on screenshots only, loop.

### Step 0: the capture rig (done 2026-11-24, works on the current build)

`.dumps/tasks/battle-hud-2/` (gitignored), part 1's scripts recovered from that session's transcript (its worktree is
gone). Run from that folder with `uv run --no-project --with pillow python bin/X.py`:

- `mkstate.py states/NAME.state`: the first battle's start state for the current binary (~6 s, replay
  `first_battle_save.json`). Rebuild the state after every build.
- `cap.py states/NAME.state TAG [scenario ...]`: one fresh process per scenario, shots in `shots/TAG_<scenario>.png`
  (3x) and `_raw` (1x). `inspect` = the INFO panel. `HUD_BIN=...` points it at another binary (a baseline build).
- `hud.py` (the `Run` helper: `wr`/`rd` by symbol via `nm`, `press`, `step`, `shot`), `sheet.py` (contact sheet),
  `perf.py` (wall time of 3000 vsyncs: the HUD Lite question).
- Before: `shots/before_inspect.png`.

Still to add: a scenario that puts a **persona** enemy in the panel (the first battle's Kuwagamon has none): poke the
slot's species / persona row, one per mold (Camaleão, Reflexo, Maré, Fechadura, SPEED_X, Enemy x2).

### Variants in parallel (part 1's trick)

While designing, the layout numbers live in **temporary globals** (`hud2_try_*`), not `#define`s. `cap.py` pokes them
per run (`r.wr("hud2_try_w", 150)`), so one build captures many versions side by side, each in its own process, all in
parallel. Once one is chosen: delete the globals, write the chosen values as `#define`s, recapture to confirm.
Nothing `hud2_try_*` may reach a commit (grep before committing).
