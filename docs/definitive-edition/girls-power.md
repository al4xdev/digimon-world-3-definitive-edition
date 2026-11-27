# Girls Power (playable Kail)

[Technical reference index](README.md) · [Player README](../../README.md)

## Girls Power (playable Kail)

`port/game/girls_power.c`, `girls_power_art.h`. Kail is the hero and Junior the friend. The story's English text follows
the swap (`tools/text_override/en_kaili.txt`, `tools/gen_text_override.py`): the hero is Ivy in the Real World and the
player's account name in the Digital World, Kurt is her brother, and the friend is Junior in both worlds (Junior becomes Ivy;
Ivy and Kail become Junior; Smith Junior keeps his name). Portuguese swaps the same names (`pt_kaili.txt`). The choice is
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

### How it works

The feature lives in the game, not on the memory card: only the choice is saved. Everything is under `PC_PORT`.

**Disc census.** Field characters are 2D 4-bit sprites (`FIELD/SPRT/P###xxxx.BIN`, `docs/FORMATS.md` "Actor sprites").
Junior's exact 16-colour palette was searched in all 2382 disc files and appears only in: `P001PLAY` (0x186, Junior in
cutscenes, actor 1, 27 animations) and `P002PLAY` (0x187, controllable, actor 2) -> become Kail; `P002PLYD/F/K/L`
(0x3C9-0x3CC: actions `PLYD`, fishing, kick, `PLYL`) -> become Kail; `P011MINO` (0x190) and `P012MINT` (0x2F8), the
Kail NPCs (actors 11 and 178) -> become Junior; the VRAM pages of the name screens (`NAMEENT/STNMETTM` 0x28B,
`SCREEN/NAMEDTTM` 0x771: name entry, confirmation, Digimon name and deck, with Junior's front frame 85) receive Kail's
own front standing frame and her CLUT. She has 9 animations against his 27 plus the actions, so the port **repaints his
frames** instead of pointing the actor at her file.

**Method.** As soon as `cdload` finishes reading one of the 8 files (`src/main/cdload.c`, transition 2->3) every frame the animation tables cite is repainted in its own buffer.
- **Whole frame of the other** in animations both have for the same thing (1 idle, 4 walk, 7 talk, 9 startle, 12 wave, 51 hop): the frame gets the other's original frame (same animation and direction)
  read from the disc: hers from `P011MINO`, his from `P001PLAY`. A script shared by several slots (drawn one way) uses the head direction of its first frame to pick the other's slot; script entries
  that cite the frame are shifted so it lands where the other's script draws it. Result: 65 of the 84 frames of Junior (ex-Kail) and 47 of the 87 of the walkable Kail are original. 57 and 58 only coincide in number
  (another gesture, another side); running, kick, fishing and the cutscenes that only one has keep the head swap.
- **Running (his animations 5 and 6):** she has no such drawing: head and trunk come from her walk frame (same step and direction) down to the belt; of Junior only the legs stay, in her colours. He runs
  crouched (head up to 4 rows lower) and her trunk is longer, so she drops only half. With this the walkable file has no frame with a glued head: 47 originals + 40 running.
- **The head:** the character's own idle heads (animation 1, 5 directions) are matched to the frame pixel by pixel,
  also mirrored; direction = the frame's slot in the animation table (5 slots = 5 directions) when a single-slot script
  cites it, else the matched head's. With at least half the pixels equal the other's head goes exactly where the
  original is (it follows the original motion); otherwise (tilted or lying head) it goes on the chin found under the
  cap/hair. The two heads are aligned face to face (mean x of the skin). Pieces of up to 8 pixels that the removal
  leaves loose (a pigtail tip, a cap corner) go with it. A head that covers the body (lying, seen from above: 3/4 of
  the frame would be removed): none of the 5 heads fits, so the cap/hair is only tinted with the other's colours, no
  paste. Her hair falling on her back (below the chin with body on both sides) is not removed (it would leave a hole):
  it becomes Junior's clothes.
- **Clothes:** recoloured with a 16-index table and the CLUT becomes the other's. **Unchanged:** animations, timings and scripts stay the game's, so kick, fishing and all cutscenes work without animation translation.
- **References:** heads come from the original standing frames read straight from the disc (`port_disc_read_user` in
  `psxstack/runtime/disc.c`). **Menu screens:** `tim_load_all` (`src/main/tim.c`) calls `port_girls_tim` before each
  upload; the page with Junior's frame 85 and CLUT receives Kail's (animation 1, direction 0) and her CLUT, recognised
  by content. **Switch:** the question "CHOOSE YOUR HERO!" on New Game right after the difficulty, same panel
  (`src/stdwtitl/stdwtitl_800852F0.c`, step 11; X goes back to difficulty), Kail first; `port_girls_refresh` (New Game,
  save read in `stgmcard`) drops the 8 files from the cache if the heroine changed; off under a replay script;
  `DW3_GIRLS=0/1` forces it. The polygon font's J and P are in `definitive_callout.c`.
- **Checks:** the C must equal the Python prototype (`tools/girls_power_preview.py`) in every frame of the 8 files
  (`DW3_GIRLS_DUMP=<dir>` writes the repainted files); `tests/port/girls_power.py` checks every frame's CLUT and head,
  and that nothing changes under a script. To tune the algorithm: generate before/after sheets with the preview tool,
  change the Python, compare by eye, then replicate in C.

### The hand-drawn frames

Every frame the automatic head swap would otherwise build is hand-drawn pixel art, compared with Junior's frame of the
same pose and with Kail's own sprites (so her identity does not change), and checked **mirrored** (the game draws the
left directions by mirroring the right ones). Whole frames and the run bust swap use real sprites. **A head that leans is
never slid ahead of the shoulders: it goes down, or it is another head (turned), as his.**

**Where each drawn frame is** (from the frame comments in `girls_power_art.h`): kick `3CB` (`P002PLYK`): back 0-4, 3/4
back 5-9, profile 10-14, 3/4 front 15-19, front 20-24. Fishing `3CA` (`P002PLYF`): back 0-13, 3/4 back 14-26 and 69,
profile 27-40, 3/4 front 41-54, front 55-68. `3C9` (`P002PLYD`) 0-6: the actions, all 3/4 from the front. `3CC`
(`P002PLYL`) 0-15: the ladder (up over the edge, at its foot, climbing 3/4 from behind, the top, jumping down and
landing, holding on). `186` (`P001PLAY`): the run 30-69 and the scene gestures (arms crossed 92-96, tapping 97-102,
103-112, 116, 119-123, 127, 128, lying 132-138, hop 139-140). `190` (`P011MINO`, Junior in his palette): 28, 29, 36-52.
`187` (`P002PLAY`): the run 30-69.

Tools (`FILE` is hex, `FRAME` decimal sub-file number): `tools/girls_power_draw.py edit FILE FRAME` (creates
`.dumps/tasks/girls-power/draw/<FILE>_<FRAME>.txt` from the header; no-op if it exists), `base FILE FRAME` (rewrites
the drawing as automatic frame + its `paste` lines; hand pixels are lost: use right after changing pastes), `show FILE
FRAME [FILE:FRAME ...]` (run with `uv run --no-project --with pillow`; writes a PNG with disc / auto / drawing on a
4-px grid, the references (hers in `190` by default), all 3x including the drawing **mirrored**, and the animation
script as the game places it, normal and mirrored, current frame boxed), `build` (regenerates
`port/game/girls_power_art.h` from all drawing files: pastes + only the pixels that differ from the automatic frame),
`check` (runs `build/port/dw2003` with `DW3_GIRLS_DUMP` and compares the 8 repainted files with the Python prototype
byte for byte; rebuild the port first). Drawing file: `#` lines = the header comment; `paste SRC SRCFRAME X Y W H TOX
TOY HOW` (a piece of a disc frame put on the frame at run time; HOW 1 mirrored, 2 through the colour table); `%` lines
ignored (column ruler, each pixel row ends with `%row`); pixel rows: `.` empty, `1`-`F` palette index. **The header
must never hold disc pixels**: her real head/trunk at another angle is `paste`d (read from the disc at run time); what
is hand-drawn goes in the rows. The drawing files are local (`.dumps/`, gitignored) because they hold disc pixels; the
header is the source of truth (`edit` recreates a drawing from it). After a batch: `build` -> `make -C build/port -j8`
and `ninja -C build/port-sdl` (-Werror) -> `check` -> `tools/venv/bin/python tests/port/girls_power.py` ->
`tests/port/run.py`.

**Placement rule.** Her head moves against **her** standing frame exactly as his head moves against **his** standing
frame of the same direction (template match of his idle head; "his head moved (dx,dy)"). In paste terms: offset for her
idle frame of direction d = (her idle script x
- his idle script x + dx, her idle script y - his idle script y + dy); her feet stay on **her** idle ground (her idle feet row + her idle script y - this frame's script y). When his head drops (crouch, wind-up) her body is shortened to fit: drop jacket rows (leaning) and leg rows (bent knees), never squash the head.
Aligning by his chin (what the automatic swap does) makes her body too short and puts her 1-2 px off her idle position (a visible hop at the start/end of the animation).

**Procedure for one frame:** `edit`, then `show` with 2-4 of her references in the same direction/pose; read Junior's
original (the action, hands/feet, where the head turns, what is in front); head: her idle head of the right direction
is what the swap put there, if the pose wants another angle or tilt `paste` her head from the frame that has it, then
`base`; body: redraw in **her** design, not his recoloured: jacket light blue-grey `D` with green `F` lapels/collar and
`C` shading, white shirt `A` in the middle, outline `8` on clothes and `7` darkest, skin `1`/`2` with `B`/`3` edges,
trousers `C`/`5` with `D`/`A` highlights, green shoes `F` with `8`/`7` outline; keep his silhouette and motion (his
timing and offsets), change the costume; look at the 3x row and the mirrored copy (no stray pixels, shapes read at game
size, pigtails do not flip into something odd); look at the animation strip (the head must not jump between consecutive
frames, match neighbours, draw neighbours from the same key drawing); write the `#` comment, `build`. **Palettes.**
Kail (her files, and Junior's files after the swap): `1` f8c0b0 skin, `2` e88880 skin shadow, `3` 8870a0 purple shade,
`4` e05860 hair, `5` 383880 navy, `6` a03058 hair dark, `7` 301820 outline darkest, `8` 485048 clothes outline, `9`
907880 mauve grey, `A` f0f0f0 white, `B` 704850 brown edge, `C` 6868b0 violet blue, `D` 98a0c0 light blue grey, `E`
581850 hair outline, `F` 98c828 green. Junior (his files, and P011MINO/P012MINT after the swap): `1` f85058 red, `2`
5068a0 blue, `3` 80b0f8 light blue (cap), `4` d05830 orange-brown, `5` 784078 purple, `6` 6858a0 violet, `7` 9880c0
lavender, `8` f8c098 skin, `9` f88888 skin shadow, `A` d09038 orange, `B` 507068 dark green, `C` 582050 dark purple,
`D` 404858 outline, `E` 788898 grey, `F` f0f0f0 white. **Her references (`P011MINO`, `190`):** idle (anim 1) slots 0-4
= frames 4 / 3 / 2 / 1 / 0 (+ breathing 82-83, 80-81, 78-79, 76-77, 74-75); walk (anim 4): front 56 57 55 58 59, 3/4
front 20-24, profile 66 67 65 68 69, 3/4 back 10-14, back 61 62 60 63 64; gestures 37-40 front arms out / eyes closed /
mouth open, 41-48 3/4 and profile variants, 49-54 profile/back, 19 29 31 34 36 jumps, back views and a lying pose.
**Review in the game:** `DW3_GIRLS_NUMBERS=1` writes each repainted frame's sub-file number in its top-left corner (3x5
digits, mirrored when the game mirrors the frame); on a screenshot the number names the frame, the file is known from
the action (fishing `3CA`, kick `3CB`, climbing `3CC`, scene gestures `186`, walking `187`, the Junior NPC `190`).
Known weak spots: the rod is one pixel wide in her dark hair colour (index 6); 3/4 frames in 24-wide files lose 1-2 px
of the pigtail tip at the frame edge (95, 97-99, 108-110, 119-123, 93, 100-103, 107); the lying frames keep the
automatic body; 122/123 differ from standing only by head height and elbow. Fishing plan used (`3CA`): per direction
compose her body (the head of that direction; the 3/4 head where he turns his head to look at his hands, 55-58 in
front), then `paste` his own arms and rod from the same frame with how=2 (recoloured: his white short sleeves become
her green upper sleeves, his skin hers), then hand-fix (his bare forearms -> her light sleeves D/C with a white A cuff,
clean any of his torso pasted along); the rod/line keep his motion exactly. Junior's files share one palette, hers
another, frame sizes vary (24/28/32 wide, 24-32 tall); `girls_power_preview.py repaint` order per frame: whole -> bust
-> head swap -> the header's steps (pastes, then art, in header order).
