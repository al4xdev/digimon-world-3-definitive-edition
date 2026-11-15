# Girls Power: drawing the head-swap frames by hand, one at a time

Companion to `girls-power-kaili-prd.md`. **Read this first when resuming the work in a new session.**

## Goal (owner's words, 2026-11-14)

The owner wants every frame Girls Power still builds with the automatic **head swap** (her head pasted on his body,
recoloured) **hand-drawn as pixel art**, one frame at a time, with care: "faz os 174, um por um, sem economizar, o
custo e tempo não é problema, quero qualidade". Not sprite sheets reviewed in bulk: each frame studied on its own,
**compared with Junior's original frame (the same pose) and with Kaili's own sprites (so her identity does not
change)**, and checked **mirrored** too (the game draws the left-hand directions by mirroring the right-hand ones).

Whole frames (animations both have) and the run bust swap are not part of this list; they already use real sprites.

## Tools

| command | what it does |
|---|---|
| `tools/venv/bin/python tools/girls_power_draw.py edit FILE FRAME` | creates the drawing file `.dumps/tasks/girls-power/draw/<FILE>_<FRAME>.txt` from the header's current state (no-op if it exists) |
| `tools/venv/bin/python tools/girls_power_draw.py base FILE FRAME` | rewrites the drawing's pixels as automatic frame + its `paste` lines (hand pixels are lost: use right after changing pastes) |
| `uv run --no-project --with pillow tools/girls_power_draw.py show FILE FRAME [FILE:FRAME ...]` | `.dumps/.../<FILE>_<FRAME>.png`: big disc / auto / drawing with a 4-px grid, the references (hers in `190` by default), all at 3x incl. the drawing **mirrored**, and the frame's animation script as the game places it (disc and drawn, normal and mirrored; the current frame boxed) |
| `tools/venv/bin/python tools/girls_power_draw.py build` | regenerates `port/game/girls_power_art.h` from all drawing files (pastes + only the pixels that differ from the automatic frame) |
| `tools/venv/bin/python tools/girls_power_draw.py check` | runs `build/port/dw2003` with `DW3_GIRLS_DUMP` and compares the 8 repainted files with the Python prototype byte for byte (rebuild the port first) |

FILE is hex (`187`, `3CB`), FRAME decimal (sub-file number).

Drawing file: `#` lines = the header comment (say what the frame is and what was drawn); `paste SRC SRCFRAME X Y W H
TOX TOY HOW` = a piece of a disc frame put on the frame at run time (HOW 1 mirrored, 2 through the colour table);
`%` lines ignored (column ruler; each pixel row ends with `%row`); pixel rows: `.` empty, `1`-`F` palette index.

**The header must never hold disc pixels.** Her real head/trunk in another angle → `paste` it (read from the disc at
run time); what is drawn by hand goes in the rows. The drawing files are local (`.dumps/`, gitignored) because they
hold disc pixels; the header is the source of truth (`edit` recreates a drawing from it).

After a batch: `build` → `make -C build/port -j8` and `ninja -C build/port-sdl` (the owner plays the SDL one; -Werror)
→ `check` → `tools/venv/bin/python tests/port/girls_power.py` → `tools/venv/bin/python tests/port/run.py`. Commit in
English (`feat(girls-power): ...`), no AI attribution, **no tag, nothing copied to ~/Applications** (the owner pulls
from `build/port-sdl` himself).

## Placement rule (learned on the kick)

Her head moves against **her** standing frame exactly as his head moves against **his** standing frame of the same
direction (template match of his idle head; scratch helper printed it as "his head moved (dx,dy)"). In paste terms:
offset for her idle frame of direction d = (her idle script x − his idle script x + dx, her idle script y − his idle
script y + dy). Her feet stay on **her** idle ground (her idle feet row + her idle script y − this frame's script y).
When his head drops (crouch, wind-up) her body is shortened to fit: drop jacket rows (leaning) and leg rows (bent
knees), never squash the head. Aligning by his chin instead (what the automatic swap does) makes her body too short
and puts her 1–2 px off her idle position (a visible hop at the start/end of the animation).

Scratch helpers used (rewrite if lost; all thin wrappers over `girls_power_draw.py`'s `read`/`write`):
`compose.py FILE FRAME "comment" SRCFRAME:X0-X1:A-B@DST:DX ...` (drawing = her pieces only, pastes written),
`over.py FILE FRAME` (stdin `ROW:pixels`, '.' keep, '0' erase), `cpdraw.py` (copy a drawing to another frame),
`where.py FILE frames` (the placement rule above, printed per frame). Copies kept in
`.dumps/tasks/girls-power/backup/` (with a patch of the paste support in case the tree is reset).

## Procedure for one frame

1. `edit`, then `show` with 2–4 of her references in the same direction/pose (table below).
2. Read Junior's original: what is the action, where are the hands/feet, which way the head turns, what is in front.
3. Head: her idle head of the right direction is what the swap put there; if the pose wants another angle or tilt
   (looking down at the rod, head thrown back on the kick, a 3/4 that should be a profile) `paste` her head from the
   frame of hers that has it, then `base`.
4. Body: redraw it in **her** design, not his recoloured: jacket light blue-grey `D` with the green `F` lapels/collar
   and `C` shading, white shirt `A` in the middle, outline `8` on clothes and `7` darkest, skin `1`/`2` with `B`/`3`
   edges, trousers `C`/`5` with `D`/`A` highlights, green shoes `F` with `8`/`7` outline. Keep his silhouette and
   motion (it is his animation timing and offsets), change the costume.
5. Look at the 3x row and the mirrored copy: no stray pixels, shapes read at game size, the pigtails do not flip
   into something odd when mirrored.
6. Look at the animation strip: the head must not jump between consecutive frames; the frame must match its
   neighbours (draw neighbours from the same key drawing).
7. Write the `#` comment, `build`, mark the frame done below.

## Palettes

Kaili (her files, and Junior's files after the swap): `1` f8c0b0 skin, `2` e88880 skin shadow, `3` 8870a0 purple
shade, `4` e05860 hair, `5` 383880 navy, `6` a03058 hair dark, `7` 301820 outline darkest, `8` 485048 clothes outline,
`9` 907880 mauve grey, `A` f0f0f0 white, `B` 704850 brown edge, `C` 6868b0 violet blue, `D` 98a0c0 light blue grey,
`E` 581850 hair outline, `F` 98c828 green.

Junior (his files, and P011MINO/P012MINT after the swap): `1` f85058 red, `2` 5068a0 blue, `3` 80b0f8 light blue
(cap), `4` d05830 orange-brown, `5` 784078 purple, `6` 6858a0 violet, `7` 9880c0 lavender, `8` f8c098 skin, `9`
f88888 skin shadow, `A` d09038 orange, `B` 507068 dark green, `C` 582050 dark purple, `D` 404858 outline, `E` 788898
grey, `F` f0f0f0 white.

## Her references (P011MINO, `190`)

Idle (anim 1) slots 0-4: 4 / 3 / 2 / 1 / 0 (+ breathing 82-83, 80-81, 78-79, 76-77, 74-75). Walk (anim 4): front
56 57 55 58 59; 3/4 front 20-24; profile 66 67 65 68 69; 3/4 back 10-14; back 61 62 60 63 64. Gestures: 37-40 front
arms out / eyes closed / mouth open, 41-48 3/4 and profile variants, 49-54 profile/back, 19 29 31 34 36 jumps,
back views and a lying pose (view the full sheet with `show` refs or `tools/girls_power_preview.py`).

## Progress (174 frames)

Mark `[x]` with a short note when the frame is drawn and built.

### P002PLYK `3CB` — the kick (anim 8), 25 — done 2026-11-14
- [x] front (slot 0): 20 21 22 23 24 — her front head/jacket pasted; 23 uses her bowed head (37)
- [x] 3/4 front (slot 1): 15 16 17 18 19 — 18 uses her turned-up head (45); 19 is her standing frame
- [x] profile (slot 2): 10 11 12 13 14 — leg back / through / out / high / standing
- [x] 3/4 back (slot 3): 5 6 7 8 9 — the kicking leg as a smear left of the body (as his)
- [x] back (slot 4): 0 1 2 3 4 — kicking leg hidden forward, thigh only; 3 head sunk (bow)

### P002PLYF `3CA` — fishing, 70 (per slot 0..4: frames for slot 4 / 3 / 2 / 1 / 0)
- [ ] cast + reel (anims 17, 21): back 0-6, 3/4 back 14-20, profile 27-33, 3/4 front 41-47, front 55-61
- [ ] anim 17 end: 7 21 34 48 62
- [ ] anim 18 (waiting): 8 22 35 49 63
- [ ] anim 19 (bite/pull): 9 10 23 24 36 37 50 51 64 65
- [ ] anim 19/60: 11 12 25 26 38 39 52 53 66 67
- [ ] anim 20: 13 40 54 68 69

### P002PLYD `3C9`, 7 — done 2026-11-14
- [x] anim 25: 0 1 — anim 22: 2 3 — anims 23/24: 4 — anim 24: 5 6 (all one way, 3/4 front; 0 had her back head)

### P002PLYL `3CC`, 16 — done 2026-11-14 (climbing a ledge, 3/4 from behind)
- [x] anim 29: 0 1 10 11 — anims 28/31: 2 3 — anims 26/27: 4 5 6 7 8 — 26/27/32: 9 — anim 30: 12 13 — anim 32: 14 15
  (11, 0, 1 had her profile head and 13 her back head: his heads there are 3/4 back and 3/4 front)

### P001PLAY `186` — Junior's scene gestures (24 wide), 37
- [ ] anim 41: 92 93 94 95 96 — anim 42: 97-102 — 43/64: 103 — 43/44: 105 — 46: 106 — 46/64: 107 — 47: 108
- [ ] 48: 109 110 — 50: 111 — 49: 112 — 11: 116 — 57: 119 120 121 — 58: 122 123 — 67: 127 128
- [ ] 16/39: 132 — 38/39: 133 — 39: 134-137 — 38: 138 — 37: 139 140

### P011MINO `190` — her file becomes Junior (the NPC), drawn in **his** palette, 19
- [ ] anim 9: 28 29 — anim 51: 36 — anim 57: 37-48 — anim 58: 49 50 — anim 59: 51 52

## Fishing plan (P002PLYF)

Per direction: compose her body (head of that direction — 3/4 head where he turns his head to look at his hands,
55-58 in front), then `paste` his own arms and rod from the same frame with how=2 (recoloured: his white short
sleeves become her green upper sleeves, his skin her skin), then hand-fix: his bare forearms → her light sleeves
(D/C, white cuff A), clean any of his torso pasted along. The rod/line keep his motion exactly.

## Notes for whoever continues

- In-game verification through the MCP `dw2003` tools did not get past the title's Continue with the owner's cards;
  rely on `show` + `check` and ask the owner for screenshots.
- Junior's files share one palette, hers another; a frame's size varies (24/28/32 wide, 24-32 tall).
- `girls_power_preview.py repaint` order per frame: whole → bust → head swap → the header's steps (pastes, then art,
  in header order).
