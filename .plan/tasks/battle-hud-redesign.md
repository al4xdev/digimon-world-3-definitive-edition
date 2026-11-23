# Battle HUD redesign (branch `feat/battle-hud`, worktree `../dw2003recomp-hud`)

## The complaint

Every player says the battle HUD is confusing, and none can say why. The current HUD was already the owner's
and Claude's best effort in a joint session: **the easy path (nudge a few pixels, recolor) is not an option**.
If it takes new art drawn from scratch, draw it.

## What we already suspect (2026-11-23, from comparing it with Persona 3 Reload / Persona 5 screenshots)

References: `~/Downloads/persona 5 hud.jpg`, `~/Downloads/persomna 3 hud.webp` (convert the webp with ffmpeg).
Use them to learn *where things go and why*. Copying the **placement** of elements is fine; copying their art,
look or style is not.

1. **The screen position of a command does not match the button's position on the pad.** In P3 the command
   cluster is shaped like the face buttons (Attack bottom, Guard right, Item left). Ours is a cross of 8 icons
   where L1 is top-left, L2 bottom-right, R2 bottom-left: the eye looks for the button in the wrong place.
   Prime suspect for the "it feels weird" nobody can name.
2. **Too many equal-weight commands, no words.** Persona: 4 main commands written out; secondary ones (Analyze,
   Target, Rush...) live in a footer hint bar. Ours: 8 icons of equal weight, no labels.
3. **The menu is not anchored to the acting character**; ours sits in a fixed corner.
4. **No visual hierarchy between enemy and party.** Persona: compact party block bottom-right (portrait + HP/SP),
   the enemy gets a small tag near itself (its weakness shown above it). Ours: two big equal panels on top plus a
   portrait box covering the arena.

These are hypotheses, not the answer: attack them.

## Constraints

- **Input tests are expensive.** First pass changes layout and look only: every button keeps doing exactly what
  it does today. Remapping is a second step, only if the layout alone does not fix it, and only after asking.
- Must work for every face layout the game supports (Xbox, PlayStation, Android touch pad: see `77248df`,
  `c6ddb66`) and at the PS1 resolution.
- **Performance fallback:** if the new HUD is heavy (Android mid-range, handhelds), keep the old one as "HUD Lite"
  and the new one as the default behind a toggle. Only if measured heavy.
- The callouts (WEAK, GUARDED, Last Stand, Blast...) are part of the HUD: they must still read.

## Method

1. Build this worktree (its own `build/`; `iso/`, `extracted/`, `tools/venv/` are symlinks to the main checkout).
2. Capture the **current** HUD in real battles (several states: command select, target, a callout, a boss, the
   Blast gauge, touch layout). Note `docs/images/definitive/shots/battle-hud.png` predates `77248df`.
3. Spawn a **UI/UX critic subagent** (model `sonnet`) that sees only the screenshots (not the code) and the
   Persona references, and critiques like a player-facing UX designer. Ask it *why* it is confusing, ranked.
4. Design, implement, recapture, and send the critic again on the new screenshots. Loop until it holds up.
5. Before/after screenshots in `.dumps/tasks/battle-hud/` and a short write-up in this file.

## Write-up (2026-11-23)

### What the critic found on the old HUD (screenshots only, ranked)

1. **Icon-only commands, and the icons are reused** (sev 5). Eight 13 px pictograms with no word. The bolt, wing and
   gear are also element icons in Inspect, so a player cannot tell a command from a property. Recall instead of
   recognition.
2. **A 3x3 grid, not the pad** (sev 4). The face diamond was right, but L1 sat level with Y, L2 under X, and the
   letters moved around the icon. It read as rows and columns.
3. **Commands far from what they depend on** (sev 4). The Blast gauge was top right, R2 bottom left.
4. **Inverted hierarchy** (sev 4). The portrait box was the largest, brightest thing on screen and repeated the Digimon
   beside it. The commands were the smallest and dimmest. Fight weighed the same as Digivolve.
5. **Unlabelled meters and pips** (sev 4; original art, out of scope).
6. **Buttons change meaning between modes** (sev 3; submenus, out of scope).
7. **Weak disabled and refused states** (sev 3).
8. **Crowded top-right corner** (sev 2).

Our hypotheses: #1 and #2 confirmed and ranked highest. #3 was partly right (proximity, not anchoring). #4 confirmed.
The brief's "L2 bottom-right, R2 bottom-left" was wrong: L2 was bottom-left and R2 bottom-right. The real fault was
that the shoulders were mixed into the face grid at all.

### What changed (layout and look only; every button does what it did)

- **The pad, held in two hands, along the bottom:**
  - L2 INSPECT over L1 ITEM in the bottom-left corner, on their own backing.
  - The face diamond at the bottom centre on a dark rhombus: Y DIGIVOLVE, X RUN, B FIGHT, A TAG.
  - R2 BLAST over R1 TECH in the bottom-right corner, on their own backing.
  - The arena's middle (the enemy) stays clear.
- **Words, new art:** every command is written in the callouts' polygon capitals (`def_poly_text`: no texture, sharp
  at any scale, the house style of the callouts and the flee question, not Persona's). English only (round 6): a
  translated set changed length and wrapped per language. No pictograms, so there is no clash with element icons.
- **Chips:**
  - Opaque and dark, outlined in the real button colour: Xbox A green, B red, X blue, Y yellow.
  - PlayStation symbols when icons are off; shoulder names otherwise.
  - Chip on the outside, word inside.
- **Hierarchy:** FIGHT is the largest word, in a warm colour. The portrait window is no longer opened with the menu.
- **Blast:**
  - Cyan everywhere, so it never sits next to Fight's warm colour and is never red.
  - A 4 px meter with a visible track sits above the tab.
  - A cyan pulse when the gauge is full, brighter and faster when it tempts. "Blast calls..." sits on a plate over the tab.
- **Refusal:** the tab flashes red with a white word for 14 ticks. Red now means only "refused".
- **Panels:** Inspect and the refusal box open in the right middle (the portrait's old room), clear of the enemy.
  "Enemy x2" and LIMIT BREAK sit over the left tabs.
- **Widescreen:** the corners move to the wide screen's corners and the diamond stays centred.
- **PS1 build:** byte-for-byte the same source outside `#ifdef PC_PORT` (checked by stripping both versions).

### Critic loop

| Round | Verdict |
|---|---|
| Round 2 (first cut: worded diamond, tabs above it, bottom-left) | Icons fixed. The diamond read as a ragged list, it covered the enemy, the chips were low contrast and the Blast meter was invisible. |
| Round 3 (split layout) | Holds up for the commands. Blast was too thin, gold sat next to Fight, red was ambiguous, and the rows mixed groups. |
| Round 4 | **Holds up for a first-time player.** One conditional sev-3 item (meter track contrast) is fixed in the final build. |

What remains (sev 2):

- The gaps between groups are small at 320 px.
- Ready and tempting differ in pulse speed and brightness.
- "Blast calls..." can cover the player's Digimon, depending on the camera.

### Measured

- **Cost:** with the menu open, headless and software-rendered, 3000 vsyncs, 3 runs each: old **1.45 ms/frame**, new
  **1.31 ms/frame**. Dropping the portrait's second model view outweighs the ~400 flat quads of the words, so there is
  **no HUD Lite toggle**.
- **Tests:**
  - `tests/port/mods.py --only battle_menu --only widescreen` pass.
  - `tests/port/run.py` passes.
  - `battle_text` fails 3 checks, exactly as on the untouched baseline (pre-existing, not touched here).

### Shots (worktree, `.dumps/tasks/battle-hud/shots/`, gitignored)

- **Before:** `before_*.png`, `sheet_before.png`.
- **After:** `after_*.png` (command, half, blast, tempt, refuse, inspect, notice, callout), `after_ps_command.png`,
  `sheet_after.png`.
- **Critic rounds:** `../critic1..4/`.
- **Scripts:** `../bin/` (`mkstate.py` builds a first-battle state for the current binary in about 6 s; `cap.py`
  captures the HUD in each state).

Owner decisions: `.plan/para-o-dono/battle-hud-redesign.md`.

## Round 6 (the owner's feedback, 2026-11-23)

The owner, on the round-4 cut: *"the HUD came out bad… the v3 style was better in the placement of things; this final
one is too much along the bottom"*. He asked for the v3 cut's placement with every later improvement kept. Round 5
(the mapped button, Fight at the bottom, the grouped plates, the regenerated showcase PNGs) was already committed as
`1a5835f` when the work stopped (a rate limit); this round is the placement.

### What changed (layout only: every button, test and option is round 5's)

- **One compact block in the left middle**, the v3 cut's places (`3445d38`): the shoulders' two rows on top (L2/L1 at
  the block's left, R2/R1 at its right, where the pad has them) and the face diamond below them, all within x 6-157,
  y 136-225. Nothing runs along the screen's bottom edge any more.
- **The block is one piece in widescreen too.** It no longer reaches the screen's edges, so the per-kind shift (the
  corners going to the wide screen's corners) would have torn the tab rows apart: the block keeps the 4:3 places.
- **The rhombus darkens instead of washing out.** Over the bright arena floor the additive fill read as haze and the
  diamond lost its shape; it now uses the plates' subtractive mode, so the four face chips sit on a dark diamond.
- **"Enemy x2" and LIMIT BREAK** go back to the right of the tabs (x 160, y 155/166), where the v3 cut had them, clear
  of the words.
- **The Blast tab is the gauge's bar** (the owner: *"instead of a bar, could the Blast button itself not be the bar?"*).
  The separate 4-px meter is gone: the tab is a dark slot filled from its left end in the Blast's cyan as the gauge
  charges, white-cyan and inked dark (word and chip) when full, so the fill grows towards the button it stands for. The
  ready pulse and the temptation (bright plate, dark word, shaking) are unchanged; red stays for refusals only.
  - The draw order matters and is easy to get wrong: in an ordering-table entry what is queued last is drawn first, so
    the fill is queued before the slot (or the slot covers it) and after the word and the chip (or the fill covers
    them), with the darkening plate last of all.

### Measured

- The gauge reads correctly at every step: captured at 0, 250, 500, 750, 999 and 1000 (`shots/g*_raw.png`): a dark slot,
  five fills, then the lit tab with a dark word.
- Cost is round 5's plus the same one quad per frame (the slot) and minus the three the old meter drew.

### Tests

- `tests/port/mods.py --only battle_menu --only widescreen`: pass.
- `tests/port/run.py`: pass (first_battle_save, new_game).

### Shots

- Round 6: `shots/r9_*.png` (command, half, blast, tempt, inspect, notice, refuse, dv), `shots/g{0,250,500,750,999,1000}*`.
- The showcase PNGs (`docs/images/definitive/shots/battle-hud.png`, `battle-inspect.png`) are recaptured from
  `r9_blast.png` and `r9_inspectknown.png`.
