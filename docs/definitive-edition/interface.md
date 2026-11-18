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
- **Battle text that does not stop the fight**: message boxes skip by default (`BATTLE TEXT: SKIP`) without waiting for a button.
- **Overlevel Rush**: when your strongest Digimon is 8 or more levels above every enemy of a random battle, the game
  offers to skip it, at the top of the screen while you keep walking (confirm takes it, back or five seconds fights).
  Take it and the rewards a win by your lead Digimon would give (experience, Bits, the drop and Hunt Materials) are
  yours at once: a panel slides in at the top of the screen and lists them for three seconds while you keep walking
  (+EXP and LV UP! for each member, NEW TECH!, TP FULL!, the Bits, the item and the materials). A battle you fight
  still ends on the results screen.
- **A Guardromon network**: at any Guardromon, `SAVE` or `TELEPORT` to the others you have met, a **map objective
  marker**, **fast-forward**, button glyphs that match your pad, and full remapping.
- **50/60 Hz support**: `--refresh 60` / `video.refresh: 60` engages the game's compensated 60 Hz mode (`records_60hz`).

## Battle HUD and launcher options

Buttons: Item L1, Tech R1, Fight B (east), Run X, Digivolve Y, Tag A, Inspect L2, Blast R2. The `battle_menu` options are
`fight`, `tech`, `digivolve`, `tag`, `item`, `run`, `inspect`, `blast` (a place on the pad: `south` `west` `north` `east`
`l1` `r1` `l2` `r2`), `remember_cursor` (default true) and `buffer_input` (`off`, `short` 1.0 s, `long` 1.5 s). The options
name a place, not a PS1 button; `pad.map` (`dw3_pad_permute`) decides which bit that place delivers, so remapping moves the
action with the button. Two actions on one button fall back to the defaults with a log line. `PORT_MOD_MAX_OPTIONS` was
raised from 8 to 12 for the `inspect` option. Details: [`docs/LAUNCHER.md`](../LAUNCHER.md) "Battle menu".

### Battle HUD: design record (plan 8, the opt-in mod that became always-on) and the glyph fix (plan 9)

- **Plan 8 findings** (verified in the source, which made it small): the main list is built in one place only, `fightstg_command_menu_create(obj->last, &obj->choice)` called from
  `fightstg_command_update` step 2; nothing else creates or reads a `FightstgCommandMenu`. The contract is tiny: write `0..5` into `*result` (0 Fight, 1 Tech, 2 DV, 3 Tag, 4 Item,
  5 Run) and end; WFIGHTMN reads only `FightstgCommand->command/arg/arg2`, which `command_update` fills afterwards. Phases (`fightstg_command_set_phase`): 0 idle, 1 HUD only, 2 main menu (ours),
  3 confusion menu (a separate object, `fightstg_confused_menu_*`), 4 Tag-only member menu; `wfightts` (debug battle) only sets 0. Rules the original enforces and the HUD copies: with
  `member->status & 8` entries 0-2 (Fight, Tech, DV) are greyed (palette 7) and refuse input; with `status & 0x20` entry 2 (DV) is greyed and refuses; a refused press does nothing (the original
  still plays the confirm sound first). Labels are localised data, not literals (`records_language + 0x7F`, strings 1..6), so it works in every language. Pad bits (`src/main/pad.c`):
  `get_pressed(0)` are the PS1 layout after the port's own pad map (`dw3_pad_permute`): 12 triangle, 13 circle (confirm `0x2000`), 14 cross, 15 square, 10 L1, 11 R1, 8 L2, 9 R2, 4-7 d-pad;
  **the left analog stick also sets the d-pad bits**, so the d-pad is never offered for actions. Mod plumbing copied: a `PortMod` record, a `port_mod_<name>` int set in the mod's `start`,
  read in game C only inside `#ifdef PC_PORT`, a `port/mods/<id>/mod.json` manifest, `tests/port/settings.py` checks every manifest equals the registry (`--print-mods`).
- **Design:** original default buttons were Fight Cross, Tech Square, Item Triangle, DV Circle, Tag L1, Run R1 (named by PS1 position so the player's own map still applies); the shipped HUD
  is the pad-shaped layout in the section above. Options were enums of `cross circle square triangle l1 r1 l2 r2` (duplicates are rejected with a log line and the defaults are used). The new code
  lives in a PC_PORT-only file with one dispatch line in the original (the matching C's `LOOP_BLOCK` is register-allocation evidence, `docs/MATCHING.md`); input is ignored on the creation frame
  (hand-over guard: a shortcut equal to confirm `0x2000` must not also confirm inside the submenu it opens; tested); the menu consumes no RNG, so mod-off replays stay byte-identical; text and
  sprites are drawn with what the game already uses (`message_create_window(0x1005, 1, x, y)`, button glyphs from `CMFONTCS` or the HUD bank, layer 0x1005); no new art; an optional press flash must
  not change frame counts or RNG.
- **Confusion menu (phase D of plan 8, optional, last):** the confused menu is a different object with the same six-row list but shuffled labels (`fightstg_confused_menu_order`), a `roll_confused`
  check and a message queue. Decision: **leave it as it is** (the shuffle is the whole effect; a button-per-action UI would defeat it). A `confusion_style` option (`original` default, `symbols`: same
  behaviour and RNG usage but each row drawn with a confusion marker) was proposed, with a first step of rendering a **test page** of candidate glyphs (Shift-JIS symbols such as `？！～★☆◎○●×△□` and the
  extra glyph range `01 xx`) for the owner to screenshot, and "if nothing usable shows up, `symbols` is dropped, not faked". `confusion_style` does not exist in the code today: not done.
- **Plan 9, the glyph bug (the owner confirmed the fix on the console, binary MD5 `9ff7e35c62b01aa55689f76799b1de61`; inputs were not changed).** Cause: the battle menu's places use the **launcher's names**
  (physical button) and `pad.map` uses the **game's names**; the launcher translated with `PLACE_TO_GAME` when writing (south -> west, west -> east, east -> south, north stays north) but the code looked up the
  letter and symbol without translating, so only `north` matched. Fix: `dw3_pad_bit_by_place`, `port_battle_menu_symbol_bit`, `bm_place_to_game` (in `port/game/game_mods.c`), and `fightstg_battle_menu_icon_text`
  (`src/fightstg/fightstg_8008D3B4.c`) uses the `pad.map` button in the action's place; declaration in `include/port.h`. The C launcher still has the same translation (`launcher/cfg.c` comment "dw3-menu.py PLACE_TO_GAME").
  **The simplification list (plan 9 step 2) was never confirmed done** (unverified; baseline `tests/port/mods.py` before and after each item): 1) unify the place tables into one structure per position (`bm_places`/`bm_place_bits`,
  `dw3_pad_places`, `dw3_place_index`, `bm_place_to_game`: the bug came from three mixed place vocabularies); 2) one source for the launcher-game translation (today `PLACE_TO_GAME` in `dw3-menu.py`/launcher and
  `bm_place_to_game` in C: keep one, e.g. the launcher writes the battle menu already in the game's names, or document both side by side); 3) store `pad.map` once, letter and PS1 button per position (today
  `letter[bit]`/`perm[bit]` and the per-place arrays `dw3_pad_letters_by_place`, `dw3_pad_bit_by_place` hold the same data); 4) `fightstg_battle_menu_icon_text` should use `dw3_button_glyph`, not repeat `0x1B/0x2F/0x32/0x34`;
  5) `dw3_feat_icons` is checked in `dw3_button_glyph` and in `src/main/message.c`, but the battle menu ignores it (draws the letter even with `DW3_FEAT_ICONS=0`): pick one behaviour and apply to both; 6) the pad is
  read by two paths in the battle menu (`port_battle_menu_pressed` with `dw3_raw_pressed`, and `pad_state.get_pressed`): keep one without changing input behaviour; 7) `dw3_raw_buttons` is only written (`src/main/pad.c`):
  confirm with grep and remove if dead; 8) review the long comments of the `pad.map` and battle_menu blocks; 9) `handheld/dw3-menu.py`: read it whole and simplify the double place translation (`PLACE_TO_GAME`,
  `PLACE_FROM_GAME`, `PLACE_DISPLAY`); 10) sweep the other mods of `game_mods.c` (`skip_dialogues`, `global_save`, `led_bar`, fast travel, objective) and the tests and list what is redundant before touching it;
  11) cross-build ARM, install on the console and ask the owner to retest before committing. Out of scope: the inputs themselves (the owner considers them perfect) and upstream code.
- **C launcher design record (plan 13, built; the wireframes are not reproduced).** Goal: replace `handheld/dw3-menu.py` and unify entry for handhelds, Linux PC and Windows with a C99 launcher (< 2 MB, < 20 MB RAM,
  instant) with an Xbox-Classic x PortMaster x Digital World look: one stylised Home (no `cover.png`), no `VIDEO`, `SAVE ANYWHERE`, `CONTROL STYLE`, `BATTLE MENU: ENABLED ALWAYS` rows; everything of combat in `BATTLE HUD`
  (including the 7th button INSPECT), `BATTLE TEXT` fixed at SKIP, the modern Circle <-> Triangle rotation as the single default in `CONTROLS`. Palette (`launcher/ui.h`, 640x480 base): background `#071019` -> `#0C1B2A`
  gradient with a subtle grid `#10283A`, panels `#102436` with edge `#1F435E` and 8 px radius, focus band `#15394A` with a green/cyan side bar `#40FF80` / `#00E5C3` and gold text `#FFCC33`, ON `#40FF80`, off `#6E8494`,
  conflict/duplicate/incompatible `#FF4D4D`, text `#E6F1F5`, notes `#8BA3B5`; one geometric TTF through `stb_truetype.h` (no SDL_ttf). Rules of `BATTLE HUD`/`CONTROLS`: `inspect` is a real option (`PORT_MOD_MAX_OPTIONS` 8 -> 12, `mod.json`,
  `game_mods.c`, fallback `L2 -> R2 -> ...` for an old `settings.json`); no silent auto-swap (a duplicate is allowed while editing), conflicting rows and cross cells turn red with an alert icon and `B`/`START` neither
  leave nor save while a duplicate exists (footer warning), a forced exit (`Select+Start` or closing the window) discards the edits without writing `settings.json`; `CONTROLS`: `pad.map` is born with Circle <-> Triangle
  rotated together with the glyphs (`CONFIRM` = game action `cross` -> physical SOUTH, `BACK` = `triangle` -> EAST so every in-game "back" shows `B` or circle, never triangle, `MENU` = `circle` -> NORTH, `CARD` = `square` -> WEST),
  `BUTTON GLYPHS` cycles XBOX (A/B/X/Y), NINTENDO (B/A/Y/X), PS1, CUSTOM. `LED BAR`: detects XF35H (`/proc/device-tree/model`, `/sys/firmware/devicetree/base/model` and `/dev/ttyS2`); compatible: first-time install
  notice then ON/off; incompatible: a hardware warning and **two sequential confirmations** before continuing. Modals `RESET SETTINGS?` and `CLEAN LOGS AND DUMPS?`; `DEBUG`: LOGS off/soft/heavy, SAVE LOGS (zip), CLEAN LOGS.
  Loading screen: the **Trio Runner**, D-Tector-style 1-bit sprites with coloured outline: Kumamon (24x24 at 3x, cyan `#00C3AF`; idle, step, jump), Guilmon (24x24 at 3x, orange `#FF8C40`), Agumon (16x16 at 4x so it
  matches the others' height, gold `#FFCC33`); one jump button each (Left/West = Kumamon, Up/North = Guilmon, Right/East = Agumon), digital obstacles coming right to left at growing speed, a collision flashes red and
  resets the SCORE (keeps the BEST) without ever slowing the real loading; the launcher hands the screen to the game when `port: start:` arrives and shows the error instead of closing mute if `dw2003` fails
  at boot. Architecture suggested: `ui_draw` (RGBA buffer, rounded rectangles, gradients, alpha, stb_truetype, stb_image, 1-bit blitter), `loading_game`, `platform_fbdev.c` (handhelds: `/dev/fb0` + `/dev/input/event*`),
  `platform_sdl2.c` (Linux PC, Windows, Wayland/KMS CFWs), `config.c` (atomic `settings.json`/`pad.map` via `psxstack/runtime/json.c`, drafts in memory until Save & Back or Play), `runner.c` (spawns `./dw2003` with a pipe);
  `Digimon World 3.sh` still detects the environment (`control.txt`, `fbdev` vs `sdl2`, cpu_part 0xd04 -> gles) and calls `dw3-launcher`. The ARM test console used was an XF35H on EmuELEC (address in the owner's notes).
  Icons planned (`assets/icons/`): digi_crest, play, gamepad_gear, crosshair_hud, dpad, led_wave, bug_wrench, reset_arrow; face_a/b/x/y, ps_cross/circle/square/triangle, l1/r1/l2/r2, start, select, dpad_lr;
  warn_triangle, check_on, fast_forward, eye_inspect, sword_fight, bolt_tech, dna_dv, arrows_tag, potion_item, wing_run, obstacle_spike. `tools/gen_assets.py` makes `launcher/assets/assets.c`.
