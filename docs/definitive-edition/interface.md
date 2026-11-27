# Comfort features, the battle HUD and launcher options

[Technical reference index](README.md) · [Player README](../../README.md)

## Comfort, Cross-Play and PC Features

- **Playable Kail (Girls Power).** On New Game, choose **Kail** or **Junior** right after the difficulty prompt: Kail
  and Junior swap roles across field sprites, cutscenes, field actions (tree kick, fishing) and menus (`port/game/girls_power.c`),
  saved per slot without altering old saves.
- **In-game Guide (`GUIDE`).** Open `GUIDE` from the field menu for two tabs (`L1`/`R1`): your current main quest step
  and open side missions (El Dorado ID, Folder Bag, Tree Boots, Fishing Pole), plus all 16 Asuka City Cargo Tower
  auctions (confirm twice on an open auction to travel straight to El Dorado). Live banners also slide in when quest
  steps advance or auctions open.
- **Cross-Play Cloud Sync & Local Backups (`SAVES & CLOUD`).** Sync `card1.mcd` across handhelds and PC through a
  zero-cost Cloudflare Worker (`cloud-sync/`): register a device, pair a second device via a **6-digit 2FA code** with
  explicit host approval, resolve conflicts cleanly, revoke lost devices, and keep **1–100 automatic local rotating
  snapshots** with one-button restore.
- **PC Graphics, Widescreen & Fullscreen `FILL`.** On PC (`--sdl`), the GPU renderer enables **16:9 Widescreen** in
  both **battles and the field** (menus and movies stay 4:3), **HighDPI** Wayland/X11/Windows windowing, shader filters
  (`NONE`, `SHARP`, `SCANLINES`, `CRT`, `SMOOTH`) and **`SCALING: FILL / INTEGER`** so fullscreen fills your monitor
  cleanly instead of stopping at integer 240p multiples.
- **Console-First C Launcher (`dw3-launcher`).** A lightweight C99 dashboard (`launcher/`) with zero Python runtime
  dependency on the menu: animated splash screen, sliding cursor, Xbox-style zoom transitions, live Digital World
  backdrop, strict duplicate-button protection on `CONTROLS` and `BATTLE HUD` (`START` to rebind, `SELECT` to reset),
  live remapped navigation, built-in D-Pad `.cue`/`.bin` browser (outside PortMaster), and the **Trio Runner** Chrome-Dino-style
  loading minigame (Kumamon, Guilmon and Agumon) while the disc verifies.
- **The Blast Gauge is saved** (eight s16 values in the slot's tail) and is now the game's Blast button; see
  [Blast, Purify and Chosen Evolution](blast.md#blast-purify-and-chosen-evolution).
- **Battle text that does not stop the fight**: message boxes always skip without waiting for a button (the constant
  `BT_SKIP`).
- **Running away asks first**: choosing RUN opens a question (confirm flees, back returns to the menu; no timer,
  `definitive_flee_ask.c`), so one slip of the button no longer ends a fight.
- **Overlevel Rush**: when your strongest Digimon is 3 or more levels above every enemy of a random battle, the game
  offers to skip it, at the top of the screen while you keep walking (confirm takes it, back or five seconds fights).
  Take it and the rewards a win by your lead Digimon would give (experience, Bits, the drop and Hunt Materials) are
  yours at once: a panel slides in at the top of the screen and lists them for three seconds while you keep walking
  (+EXP and LV UP! for each member, NEW TECH!, TP FULL!, the Bits, the item and the materials). A battle you fight
  still ends on the results screen.
- **A Guardromon network**: at any Guardromon, `SAVE` or `TELEPORT` to the others you have met, a **map objective
  marker**, **fast-forward**, button glyphs that match your pad, and full remapping.
- **60 Hz by default**: the European disc runs at 50 Hz; the launcher's `GAME > REFRESH RATE` row (PC and Android) starts
  at 60, the game's own speed-compensated 60 Hz mode (`video.refresh: 60`, `--refresh 60`, `records_60hz`). An old
  launcher state takes 60 Hz once when it migrates.
- **Video on PC**: `RESOLUTION` (internal resolution, default 3X: 720p in 16:9) and `3D VERTICES` (smooth by default) on
  the VIDEO screen; **Widescreen AUTO**: the wide picture follows the window's shape from 4:3 to 16:9, so a 16:10 screen
  (a Steam Deck, a laptop) is filled; wider than 16:9 keeps 16:9 with side bars.
- **The launcher updates itself** on Windows and in the Linux AppImage: `NEW VERSION` (DEBUG) downloads the release,
  checks its size and SHA-256, and swaps the files after you confirm. PortMaster, RecompLauncher and a source checkout
  still open the release page; Android tells you a newer APK is out.
- **Fast-forward anywhere**: Select + R1 on a pad, Tab on a PC keyboard, and a two-chevron button in the corner of the
  Android touch pad.

## Battle HUD and launcher options

Buttons (default, Xbox names): Bag (items) L1, Tech R1, Fight A (south), Run X (west), Digivolve Y (north), Tag B
(east), Info L2, Blast R2 (`bm_default_places` in `port/game/game_mods.c`, `launcher/cfg.c`; a launcher option keeps
the older layout with Fight and Tag swapped). The `battle_menu` options are `fight`, `tech`, `digivolve`, `tag`,
`item`, `run`, `inspect`, `blast` (a place on the pad: `south` `west` `north` `east` `l1` `r1` `l2` `r2`),
`remember_cursor` (default true) and `buffer_input` (`off`, `short` 1.0 s, `long` 1.5 s). The options name a place, not
a PS1 button; `pad.map` (`dw3_pad_permute`) decides which bit that place delivers, so remapping moves the action with
the button. Two actions on one button fall back to the defaults with a log line. Details:
[`docs/LAUNCHER.md`](../LAUNCHER.md) "Battle menu".

### How the battle menu hooks in

- The main list is built in one place only, `fightstg_command_menu_create(obj->last, &obj->choice)` from
  `fightstg_command_update` step 2; nothing else creates or reads a `FightstgCommandMenu`. The contract: write `0..5`
  into `*result` (0 Fight, 1 Tech, 2 DV, 3 Tag, 4 Item, 5 Run) and end; WFIGHTMN reads only
  `FightstgCommand->command/arg/arg2`, which `command_update` fills afterwards. Phases
  (`fightstg_command_set_phase`): 0 idle, 1 HUD only, 2 main menu, 3 confusion menu (a separate object,
  `fightstg_confused_menu_*`, left as the original: the shuffle is the whole effect), 4 Tag-only member menu;
  `wfightts` (debug battle) only sets 0.
- Rules of the original that the HUD copies: with `member->status & 8`, Fight, Tech and DV are greyed (palette 7) and
  refuse input; with `status & 0x20`, DV is greyed and refuses; a refused press does nothing (the original still plays
  the confirm sound first). Labels are localised data (`records_language + 0x7F`, strings 1..6).
- Pad bits (`src/main/pad.c`): `get_pressed(0)` is the PS1 layout after the port's pad map (`dw3_pad_permute`): 12
  triangle, 13 circle (confirm `0x2000`), 14 cross, 15 square, 10 L1, 11 R1, 8 L2, 9 R2, 4-7 d-pad. **The left analog
  stick also sets the d-pad bits**, so the d-pad is never offered for actions.
- The new code lives in a PC_PORT-only file with one dispatch line in the original (the matching C's `LOOP_BLOCK` is
  register-allocation evidence, `docs/MATCHING.md`). Input is ignored on the creation frame, so a shortcut equal to
  confirm does not also confirm inside the submenu it opens. The menu consumes no RNG, and it draws only with what the
  game already uses (`message_create_window(0x1005, 1, x, y)`, button glyphs from `CMFONTCS` or the HUD bank, layer
  0x1005).
- **Place names:** the battle menu's places are the physical buttons (the launcher's names) and `pad.map` uses the
  game's names. `bm_place`, `dw3_pad_permute` and `port_battle_menu_symbol_bit`
  (`port/game/game_mods.c`) translate; `fightstg_battle_menu_icon_text` shows the `pad.map` button in the action's
  place.

## Launcher design rules

- Palette (`launcher/ui.h`, 640x480 base): background `#071019` -> `#0C1B2A` gradient with a subtle grid `#10283A`,
  panels `#102436` with edge `#1F435E` and 8 px radius, focus band `#15394A` with a green/cyan side bar `#40FF80` /
  `#00E5C3` and gold text `#FFCC33`, ON `#40FF80`, off `#6E8494`, conflict/duplicate/incompatible `#FF4D4D`, text
  `#E6F1F5`, notes `#8BA3B5`; one geometric TTF through `stb_truetype.h`.
- `BATTLE HUD` / `CONTROLS`: no silent auto-swap (a duplicate is allowed while editing); conflicting rows turn red with
  an alert icon and `B`/`START` neither leave nor save while a duplicate exists; a forced exit (`Select+Start` or closing
  the window) discards the edits without writing `settings.json`.
- `pad.map` defaults to Circle and Triangle rotated together with the glyphs (`CONFIRM` = game action `cross` ->
  physical SOUTH, `BACK` = `triangle` -> EAST, so every in-game "back" shows `B` or circle; `MENU` = `circle` -> NORTH,
  `CARD` = `square` -> WEST). `BUTTON GLYPHS` cycles XBOX, NINTENDO, PS1.
- `LED BAR`: detects the XF35H (`/proc/device-tree/model`, `/sys/firmware/devicetree/base/model` and `/dev/ttyS2`);
  other hardware gets a warning and two confirmations.
- Loading screen (**Trio Runner**): 1-bit sprites with coloured outline, Kumamon (cyan), Guilmon (orange) and Agumon
  (gold), one jump button each; a collision resets the SCORE (keeps the BEST) and never slows the real loading. The
  launcher hands the screen to the game when `port: start:` arrives and shows the error if `dw2003` fails at boot.
- The sprites and the font in `launcher/assets/` come from `launcher/tools/gen_assets.py`.
