# Battle HUD 2b: the INFO panel comes out of INFO and goes back into it

Branch `feat/battle-hud-2b-anim` (from main 0381c0e, 2026-11-24). Choices and edge cases agreed with the owner:
`battle-hud-part2-info.md`, sections "The owner's pick" and "The panel lives beside the menu". This file is the how.

## Shape: one parameter, run forwards to open and backwards to close

- `obj->panel_t`, 0 (closed) .. `OPEN` (open), in game ticks (`get_frame_ticks()`: vsyncs, so 30 fps draws advance 2,
  fast-forward advances more; never frames). Opening adds 1 per tick; closing subtracts `CLOSE_SPEED` per tick, so
  the close is the open played backwards, faster: the height folds, the bar shrinks, the trace runs back into INFO.
- `obj->inspect` (what the menu means) changes at once, as today. Turning around mid-way is flipping the direction:
  no jump, no restart. **No input waits.**
- Stages of `t` (open ≈ 18 ticks ≈ 0.3 s; close ≈ 9 ticks):
  1. **trace** `[0, T1)`: a 2-pixel line from INFO's word to the panel's top-left corner, its head running out (ease
     out). Origin read every frame from INFO's current place (remap, the menu's slide-in, the widescreen shift).
  2. **width** `[T1, T2)`: the trace is gone; a bar at the panel's top grows from the corner to the full width,
     overshoots by 4 px, settles (settling ≤ 2-3 drawn frames: whole PS1 pixels look like steps at 4K).
  3. **height** `[T2, OPEN]`: the frame grows down; each text window, picture and line appears when the bottom edge
     has passed it (closing: all text off at once, then the frame folds).
  4. At `OPEN` the panel is exactly today's: the static capture must match 2a's pixel for pixel.
- Text windows: `set_visible` (cheap, keeps the text; `message_set_visible` refuses an empty window). Set the text
  once at open as today.
- Closed with `t == 0`: nothing drawn (the trace's last frame lands back on INFO, INFO's plate glows one frame).

## Cut at once (no close animation)

A command chosen (`obj->press`): the menu object ends 7 ticks later and frees its windows. A refusal that puts the
notice in the panel's place. Everything else (INFO / Start again, a refusal without notice) animates.

## Engine

A transition is data: `{ origin (read), destination, stage ends, overshoot }` and small pure helpers
(`hud_anim_ease`, `hud_anim_lerp`) the next transitions (Tech/Item, part 3) reuse. Start them in this file next to
the menu (one user so far), move to a module when part 3 is the second user. Purely visual: nothing the battle reads.

## Check

- Capture rig (`.dumps/tasks/battle-hud-2/bin`): frame-by-frame sheets of open, close, reopen mid-close, close
  mid-open, a command mid-open, at 4:3 and in widescreen (16:10 and 16:9 if the GPU state loads), and with INFO
  remapped (`settings_mixed.json`).
- At rest: the panel open must be identical to 2a's capture.
- `perf.py` with and without.
- Tests: `tests/port/inspect_info.py` and the suite.
