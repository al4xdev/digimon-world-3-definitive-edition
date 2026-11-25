---
name: drive-dw2003
description: Drive the Digimon World 2003 PC port from a script or the dw2003 MCP server — boot it, reach any screen fast (warps, test pokes, savestates), press buttons, read/write memory by symbol, take GPU 8x screenshots and record GIF/MP4 clips. Use it to verify a change in game, reproduce a bug, or capture README/site shots.
---

# Driving the game

The port has a **debug channel**: a socket the game listens on while it runs, used by
`psxstack/tools/mcp/game.py` (`Game`) and by the `dw2003` MCP server built on it. Through it you
step frames, hold the pad, peek/poke native memory, take screenshots and save/load states. Every
call happens between two vsyncs, so a run is **deterministic**: the same state + the same inputs
on the same frames give the same pixels. Use that: find your inputs once, then replay them.

`scripts/dw.py` wraps all of this (`seq`, `rec`, `sheet`, `clip`; `--help` lists the options).

## Pick the channel

- **MCP tools** (`mcp__dw2003__*`): interactive poking, one call at a time. `game_start` cannot set
  env vars and always runs `build/port/dw2003` (headless, software renderer only).
- **Python** (`scripts/dw.py`, or `Run` in it): anything with env vars, loops, GPU captures or
  recording. A savestate made by Python can be loaded by MCP only if it is the same binary.

## Rules that cost hours when ignored

1. **A savestate belongs to one binary** (sha1 checked: "saved by another binary"). Rebuilds
   invalidate states. When another session may rebuild (`Permission denied` on the binary while
   it is being linked), **copy the binary** to `.dumps/…/bin/` and use the copy with `--bin`.
2. **Static arrays the game rebuilds on entry are not in the savestate** (example: the Bestiary's
   `catalog[]` is built when the stage opens, so `catalog_count` reads 0 after a state load).
   Reach the screen by inputs from a field state, then read/poke.
3. **Screenshots on the GPU renderer need a real window**: `--window --renderer gpu
   --internal-scale 8 --subpixel on` with `screenshot(renderer="gpu")`. Offscreen SDL silently
   falls back to software (320x240-ish frames); the headless `build/port/dw2003` has no GPU at
   all ("no GPU renderer in this run"). Use `build/port-sdl/dw2003`.
4. **The window is real**: if the user closes it, the run dies with `GameExited: Broken pipe`.
   Just run again. A capture is ~1 s per GPU frame at 8x; tell the user before long ones.
5. **Never write to a player's memory card**. Copy it (`--card1 copy.mcd`) or use a test card.
6. Look at a **contact sheet** (`dw.py sheet DIR STEP out.png`, then Read the PNG) before
   building anything: timings at 60 Hz drift from what you guessed.

## Getting somewhere fast

- **Warp at boot**: `--env DW3_TIME_TEST=0x23b:1736:468:26` (map:x:y:progress, see
  `port/game/definitive_time_room.c` `test_warp`). Then load a save from the title and you stand
  on that map. Save a state right there and start every later run from it.
- **Title → game** (default buttons, a card with a save in slot 1): run ~7700 frames, `CROSS`,
  ~1500 frames, hold `START` 30 frames to skip the movie, then `START`, `CROSS` through the load
  menu. Do it once with `seq`, save the state, never again.
- **Random battle now**: poke `gamestate_data+0x30=1` (encounter timer) and walk a few steps. A
  big value means *later*, not *now*.
- **Test hooks the code already has** (grep `getenv("DW3_` and "the in-game test pokes it"):
  `DW3_RUSH_TEST=1` offers the Overlevel Rush whatever the levels (2 also takes it);
  `--poke def_superhunt_debug=1` opens the Super Rare hunt board on the next idle field frame;
  `--poke bm_ui=1` (its first field is `request`) opens the Black Market anywhere on the field.
- **Field menu**: `START` (not TRIANGLE). The cursor does not wrap: count DOWNs to the item.

## Memory

- Symbols come from `nm` of **the same binary** (`dw.py` resolves `name+off`). Statics work too
  (`b`/`d` in nm), but the same static name can exist in several files: `dw.py` refuses ambiguous
  names; take the address from `nm -n` and pass `0x…+off`.
- Structs: read the C definition and count. Check your layout by the gap to the next symbol
  (`catalog` is 512 × 20-byte entries = 0x2800 before `saved_count`).
- Battle: `fightstg_battle+0x76` enemy max HP, `+0x78` current HP (2 bytes each). Raise HP to
  film several turns; drop it to 40 to film a KO (OVERFINISH).
- To fake progress for a screen (discovered entries, owned items), poke the in-memory table the
  screen reads, then make the screen rebuild (switch tab / re-enter). Prefer this to editing
  saves; the expansion save (`<card>.def.json`) is the alternative when it must persist.

## Battle buttons (default HUD)

CROSS fight · CIRCLE tag · TRIANGLE digivolve · SQUARE run · L2 info · R1 tech · L1 bag · R2 blast.
Radials confirm in two steps (choose, then confirm); the form marked NOW does nothing on confirm.

## Recording

```fish
set D .agents/skills/drive-dw2003
uv run --with pillow python $D/scripts/dw.py rec --bin .dumps/x/bin/dw2003-sdl \
    --config $D/settings-capture.json --card1 .dumps/x/c1.mcd --state .dumps/x/battle.state \
    .dumps/x/raw/weak 300 "R1@10,CROSS@22,CROSS@34"
uv run --with pillow python $D/scripts/dw.py sheet .dumps/x/raw/weak 8 /tmp/s.png
uv run --with pillow python $D/scripts/dw.py clip .dumps/x/raw/weak \
    docs/images/definitive/shots/battle-weak.gif site/assets/shots/battle-weak.mp4 "10-160,200-290" 120
```

Frames are taken every 2 vsyncs (30 per second) and saved at 1280 wide. Budgets that worked:
README GIF 480 wide, 8 fps, no dither, under ~5 MB; site MP4 1280x720 crf 25–29 with a JPG
poster; stills PNG 1280x720. Cut idle stretches with several ranges rather than lowering quality.
`settings-capture.json` is English, default HUD, widescreen, muted: not the player's own mods.
