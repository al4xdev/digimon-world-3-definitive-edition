# Battle HUD, part 3: the Tech and Item screens, a sheet of paper that unfolds out of the menu

Owner, 2026-11-24 (after 2b shipped in 0.8.17). Tag (and Digivolve, Run) stay as they are: Tech and Item are alike,
and the scope stays small.

## What the owner chose

- **Redo the two screens** in the new HUD's design (the battle menu and INFO): the same dark subtractive backdrop and
  blue frame, the flat-pixel pictures, the polygon words. The original's sprite frame (bank 0x4550000) goes.
- **The menu itself is the paper.** On Tech / Item the battle menu's block becomes a sheet that unfolds like paper
  into the screen (an accordion, panel by panel, each turning on a hinge), and folds back into the menu on cancel.
- **Two halves, independent.** The new screens use the left half of the 4:3 screen; INFO keeps the right half and can
  be opened or closed inside them at any time, without stopping anything. In widescreen the sheet follows the block's
  shift (`FIGHTSTG_BMENU_WIDE_SHIFT`), so it sits where it does at 4:3.

## Consequences

- The game's descriptions are pre-wrapped for ~230-290 px: in the left column (~150 px) they are **re-wrapped at run
  time** (join the line breaks, break again at spaces by measuring each glyph), in all six languages. Measure the
  longest result before fixing the column's line count.
- MP moves from the partner's box (right half, INFO's place) into the sheet's header.
- Input never waits (2b's rule): the cursor and the pages work from the first tick; a choice cuts at once.

## Steps

1. Static screens (Tech, then Item) in the left half; the re-wrap; captures in the six languages.
2. The paper: the sheet object that outlives the hand-over battle menu -> Tech/Item -> battle menu; the fold (quads
   whose far edge moves with the angle, shade by angle, crease line; text squashed with the panel through the
   message window's scale, `message_set_scale` / `message_set_center`).
3. INFO inside Tech/Item (the INFO button's mapping), independent of the sheet.
4. Item mirrored from Tech.

## Check

Capture rig `.dumps/tasks/battle-hud-2/bin` (`screens.py` for the static screens, `anim.py` for frame sheets); 4:3,
16:9 and 16:10; a remapped INFO; the six languages; `buffer_input`'s skip (no sheet at all).
