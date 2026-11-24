# The launcher and the mods: this game's part

This page is mostly the reference for this game's built-in mods (including the LED bar) and its 60 Hz mode. There is
one launcher, the C99 `launcher/`, with the same screens and options everywhere; each release line compiles its sources:

- **Linux x86_64 (AppImage) and Windows x86_64 (zip) releases**: `launcher/CMakeLists.txt`, `-DDW3_TARGET_PC`, SDL3
  linked statically (`cmake -S launcher -B build/launcher -G Ninja -DSDL3_DIR=$PWD/tools/sdl3-desktop/lib/cmake/SDL3`;
  `build/launcher/dw2003-launcher`). Windows is cross-built by `.ci-cd/windows/build_windows.sh --launcher`, with
  `launcher/win32.c` for the game's process, the file dialog and WinHTTP and `launcher/compat.h` for the POSIX calls
  UCRT spells differently. The game (`dw2003`, `dw2003.exe`) and its `mods/` sit beside the launcher.
  The settings directory has one rule, the game's too (`psxstack/runtime/main.c`) and `recomplauncher.json`'s: a
  `portable.txt` beside the launcher keeps everything in the launcher's folder, else the per-user folder
  (`~/.local/share/dw2003/`, `%APPDATA%\dw2003\`). In it: `tui.state` (the launcher's settings), `pad.map` (its
  controls), `settings.json` (the game's, written by the launcher; its VIDEO, GAME and DEBUG LOGS values are read
  back, so what RecompLauncher changed shows in the launcher), the memory cards, `logs/`. `--self-test DIR` checks the
  rule, the files' round trip, settings.json's read-back and every screen, headless; with `DW3_SELFTEST_GAME=beside`
  also PLAY with an image the game refuses (the disc screen), and with `DW3_SELFTEST_DISC` the game started from
  PLAY for 300 frames (its status channel, card and log).
  The update notice: once in 12 hours (cached in `update.cache` beside the settings) the launcher asks GitHub for the latest
  full release and, when it is newer than this build, says so on HOME and offers its page in DEBUG (`CHECK FOR UPDATES`
  turns it off; a build that is not a release, `dev`, never asks; nothing is downloaded or installed).
  RecompLauncher starts this launcher (`.ci-cd/recomplauncher/README.md`); `--play` presses PLAY at once (its
  "Launcher menu: Skip"), with the same backup, cloud sync and conflict screen around the game.
- **PortMaster (armhf) releases** use the shell entry `.ci-cd/portmaster/handheld/Digimon World 3.sh` together with the C99 console-first
  launcher `launcher/` (`dw3-launcher`, compiled with `-DDW3_TARGET_PORTMASTER -DHAVE_SDL2` by `.ci-cd/portmaster/build_arm.sh` in
  the Debian 10 `buster` armhf container for glibc >= 2.28). It runs on `/dev/fb0` + evdev by default and falls back to
  SDL2 on Wayland/KMSDRM/portrait panels.
- **Android (`arm64-v8a`) APK releases** (`.ci-cd/android/build_android.sh`, `android/`) link the parallel touch-first C99
  launcher (`launcher/main_android.c`, with larger clickable rows, `< BACK` header/footer pills, SAF disc picker, and
  the `FLOATING PAD` toggle in `GAME`) directly with the `PSXSTACK_SDL2` game and floating virtual gamepad
  (`android/touch_pad.c`), using Android's native controller detection via SDL2.
- **A desktop checkout's quick build** (`launcher/Makefile`, `-DDW3_TARGET_PC`, SDL2): `make -C launcher SDL=1`, then
  `./launcher/dw3-launcher --sdl` finds `build/port-sdl/dw2003`.

The decisions behind the design: DECISIONS "Launcher and mods".

Code cites this file as `docs/LAUNCHER.md "<heading>"`; keep the headings stable.

## The game's side of the contract
- `dw2003 --config FILE` and `--print-settings` are psxstack's runtime (`psxstack/runtime/settings.c`, `input.c`,
  `mods.c`); `tests/port/settings.py` checks the round trip, the defaults, the overrides and the errors against this
  game's build, and that every manifest under `psxstack/mods/` and `port/mods/` equals the game's registry
  (`--print-mods`). `--game PATH` is the same option as `--disc PATH`. When `RECOMPLAUNCHER=1` and `--config` was not
  passed, the runtime loads the launcher's usual `settings.json` (portable.txt beside the executable, else the
  per-user folder) and puts crash reports and data mods beside it. A start without that variable is unchanged.
- The disc check is the game's description's: `port/game/game.json` names the unpatched EU disc (SLES-03936) by its
  SHA-1 and size, and the game checks it when it starts. The launcher does not hash the disc itself: it reads a
  refusal from the game's output (`port: fatal: disc:`) and shows the disc screen (`S_DISC`: which image the game
  plays and where to pick it, in gold rather than the error screen's red) instead of COULD NOT START. A refused
  disc is the check doing its job, not a crash. RecompLauncher only marks the Redump dump as verified
  (`recomplauncher.json` `dumps`, its "Matches the verified Europe disc"); another dump of the European disc passes
  its own check (`check`: the serial) and is refused by the game, so its label and its exit code 1 text name that
  badge.
- The environment variables keep their names (`DW3_SELFTEST_*`, `DW3_PORT_*`): the description's `env_prefix` is
  `DW3`.
- Texture packs are data mods in the settings directory's `mods/` (psxstack `docs/LAUNCHER.md` "Data mods"; the
  launcher passes `--mods-dir`); `tests/port/settings.py` checks `mod_order` and the data mods' settings, and
  `tests/port/textures.py` the packs found that way.
- The self-test with the real disc and game: `DW3_SELFTEST_DISC=$PWD/iso/dw2003.cue
  DW3_SELFTEST_GAME=$PWD/build/port-sdl/dw2003 SDL_VIDEO_DRIVER=offscreen build/launcher/dw2003-launcher --self-test DIR`.

## Built-in mods

### Fast-forward

`fast_forward` (runtime only, no game C). Options: `hold` (binding, default `Tab`), `toggle` (binding, unbound),
`speed` (enum `2x 3x 4x 6x 8x unlimited`, default `4x`), `mute` (bool, default true). The console-first launcher
(`launcher/cfg.c`) writes `hold` `Tab` on a PC (none on a handheld) and `toggle` the chord of the buttons chosen for
SELECT and R1 (`[["pad:back", "pad:rightshoulder"]]` by default), which its `FAST-FORWARD` row names; Select comes
first, since the game has no use for it and R1 alone pages its menus. The handheld's SDL2 build reads it too.

While it is on (the hold binding held, or after the toggle) the pace is the nominal rate times the speed (`4x`: 200
vsyncs a second at 50 Hz, 240 at 60; `unlimited`: no pace), the window presents at most 60 images a second (every vsync
is still drawn), and with `mute` the audio device's queue is cleared and nothing is queued (the SPU keeps rendering,
since LIBSND reads its envelopes; a `--wav` is unchanged). The window's title says it is on. The game, its log and its
record are those of an unthrottled run, which are byte-identical to a paced run's.

Test hook: `DW3_PORT_FAST_FORWARD=ON:OFF` (only while the mod is enabled) turns it on for ON vsyncs and off for OFF,
repeating (`tests/port/settings.py`).

### Faster loading

`fast_loading` (runtime only, no game C). Option: `speed` (enum `2x 4x 8x instant`, default `4x`). The launcher's
GAME screen writes it on at `8x` (`FAST LOAD`); `OFF` leaves the drive as it is.

While it is enabled, cdload's data reads (overlays, maps, sound banks: the install and the loads between screens) run
at that multiple of the drive's rate, with the seek shortened as much, or at once. The movie stream and any read with
the XA bit keep the realistic drive, so the title demo's video and its audio stay paced. A load ends in fewer vsyncs,
so a generator the game steps once a frame can come out differently later. With the mod off the frame log is unchanged.

### Skip dialogues

`skip_dialogues`. Options: `toggle` (binding, default `F2`), `hold` (binding, unbound), `fast_forward_waits` (bool,
default false). While it is on, `port_mod_skip_dialogues` is 1 and three hooks act:

- `message_window_update` (`src/main/message.c`), RUN case: a revealing window (`speed > 0`, not done) shows its whole
  page at once (`message_find_page_end`). This covers every message window, the overlays' too.
- Its DONE step 1: a wait for button 13 (confirm) goes on by itself. Every wait in the disc's text tables uses that
  button; any other wait is left to the player.
- `fightstg_message_step` step 3 (`src/fightstg/fightstg_8008D3B4.c`): the battle's message wait goes on as a press
  would (without the press's sound).

No pad press is synthesised, so choices, menus, name entry, shops, the inn and the save prompts still wait, and a
closed dialogue does not talk to the NPC again. Not covered: the code-driven prompts of the card game, the status
screen and other overlays.

With `fast_forward_waits`, the mod also asks for fast-forward (with `fast_forward`'s speed and mute, or their defaults
when that mod is off) while FIELDSTG is the current overlay and `fieldstg_stage.event_running` is set: a cutscene's
scripted waits, walks and bubble animations. Free walking is not sped up.

Test hook: `DW3_PORT_SKIP_DIALOGUES=1` (only while the mod is enabled) starts it on. `tests/port/mods.py` runs
`first_battle_save`'s route with no press through the scenes.

### Disable battle animations

`battle_animations`. Option: `hit_reaction` (bool, default true). No hotkey: it acts whenever enabled
(`port_mod_battle_animations`).

The battle's rules (every roll, damage, HP) run before an action's animation script starts; the scripts only present
them. The hook sits in `fightstg_script_update`'s INIT step 0 (`src/fightstg/fightstg_8008B630.c`), after the script's
stream is found and before script 12's sound-bank load. For scripts 5 and up (attacks, techniques, items) it asks
`port_battle_cut` (`port/game/game_mods.c`), which scans the stream (`port/game/battle_scan.c`) and returns:

- **-1, run it:** a stream that does not scan to its end (none on the disc).
- **1-4, the target's reaction** (`results[3] + 1`: flinch, heavy hit, KO, dodge) for a script with a child command
  when `hit_reaction` is on. The hook restarts the same object as that reaction (side flipped; results, stage, effect
  and hit sound zeroed; step 0 again), as the child command would have.
- **0, end it at once:** a script with no child command, or every script when `hit_reaction` is off, except that **a
  knock-out keeps its KO reaction** so the model ends on its KO pose.

In each case it plays the script's first hit sound (`fightstg_script_run_sound`'s choice). Reaction scripts 1-4 created
directly (the defeat camera, `fightstg_player_reaction`) and the scenes outside the scripts (entrances,
digivolutions, the intro and defeat cameras) are left alone: they carry functional writes. `tests/port/battle.py`
checks the scanner against every script on the disc; `tests/port/mods.py` runs the first story battle with the mod on.

### Battle text

**Not a mod any more: the battle text is always skipped** (the owner, 2026-11-23 — "from now on it is all skip"). The
launcher has no option for it and the settings file no longer carries one; `port_mod_battle_text` stays as a code
constant (`BT_SKIP`, 1, in `port/game/game_mods.c`) for the FIGHTSTG side and for a future change. 0 (as the disc) and
2 (async) remain in the flag's vocabulary, unused.

The battle's message box (`FightstgMessage`, `src/fightstg/fightstg_8008D3B4.c`) is presentation only: the rules ran
before it is shown, but every action waits for it to be dismissed with a confirm press. The hook sits in
`fightstg_message_update`'s RUN, the frame a message is ready (`fightstg_message_port_take`). Each message has a class,
set by `fightstg_message_show` from its type and text ID (`ESMFIGHT`):

- **keep:** the victory (0x52) and defeat (0x53) texts. They are the game's pacing (`wfightmn_victory_wait` closes the
  victory text after 100 frames), so they run as on the disc in every mode.
- **drop:** what the animation already says: a plain "Attack" (8), "Evade" (0x1D), "Focus on attack/guard" (0x3C, 0x3D),
  "Special effect up" (0x3E), "Blast gauge up" (0x3F), "Excited" (0x40). Ended at once in `skip` and `async`.
- **normal** (technique and item names, damage, healing, statuses, switches...): `skip` ends it at once; `async` hands
  its two windows to a **toast** and ends it, so the action goes on (the animation starts with the technique's name
  still up, and the damage replaces it when it arrives). A chain (a queued follow-up: regeneration, the counter's
  damage) shows only its last page. The toast stays 90 frames unless a newer text replaces it.

The toast is owned by no object: the battle's slots object (`fightstg_slots_update`) runs and draws the windows each
frame, and `fightstg_slots_create` clears it for a new battle. Its state (`port_battle_text_windows`,
`port_battle_text_frames`, in `port/game/game_mods.c`) is named in the save state. No random number is drawn and no
rule changes, but the frame count of a battle does, so the scripted replays of the disc's own battles do not apply with
the mode on.

Being fixed, it also changes the frame count of a battle for everyone, so the scripted replays of the disc's own
battles are compared on the game state, not on frames (`tests/port/mods.py`'s `battle_menu` runs the first battle this
way).

### Preset language

`preset_language`. Option: `language` (enum `english portuguese french german italian spanish usa japanese`, default
`english`, `applies: restart`). No hotkey: it acts at boot whenever enabled (`port_mod_preset_language`). `portuguese`
is the English text set (code 2) with the port's PT-BR texts over it
([`definitive-edition/texts-and-translation.md`](definitive-edition/texts-and-translation.md)).

The game boots into CNTY_SEL (map 0x1600), whose menu sets `records_language` (the offset of every localized text
file ID: 0 JPN, 1 USA, 2 ENG, 3 FRA, 4 ITA, 5 GER, 6 SPN) and goes on to the opening, map 0xE02 (0xE01 for Japanese).
With the mod on, CNTY_SEL's root object (`src/cnty_sel/cnty_sel_80082CE8.c`), once the screen's sound bank is in, sets
`records_language` to the option's code and makes that map change itself: the screen's image, menu, music and fade
are never loaded. Nothing else on the EU disc reads the choice; the title's New Game builds the new game's records
(`gamestate_init_records`) in that language. A reset goes through CNTY_SEL again, so the language stays. The screen
offers only the five EU languages, but the USA and Japanese text sets are on the disc too (`docs/DISC_LAYOUT.md`) and
the EU code handles them: Japanese takes the opening's 0xE01 path, with the Japanese release's intro art (both
play-tested to the memory card screen).

`tests/port/mods.py` boots each language (the opening's map and `records_language`, and the mod off still at the
screen) and runs `new_game`'s route from the opening in French, whose new game has the French deck names.

### Logs (the DEBUG group)

The game's commentary is one switch, `debug.logs` in `settings.json`: `off` (the default) prints nothing, `soft` prints
the startup lines and the events, `heavy` adds the periodic measurements (the per-second `present:` line among them).
`.ci-cd/portmaster/handheld/Digimon World 3.sh` reads the value and passes it as `DW3_LOG`; the runtime's `port_log` honours it (`runtime/main.c`,
`port_log_verbose()` for the periodic lines). **With the logs off the game log stays empty** - the file is still created,
because the launcher appends to it - and the crash report keeps its last lines either way, so a report is possible
without them. The separate one-off meters (`DW3_PRESENT_LOG`, `DW3_GPU_LOG`, `DW3_PHASE`) are commented in that launcher and
are not the same thing: they write their own files and some cost real frames.

`SEND LOGS`, in the same group, saves `dw3-logs-<date>.zip` beside the port: `logs/*`, `crash-*.txt`, `settings.json`,
`pad.map`, `tui.state`, the player's `card1.mcd` (and `card1.def.json` up to 512 KB, 256 KB in the upload: an RPG's progress, nothing
personal, and what reproduces a battle or quest bug) and a `system.txt` (OS, CPU, cores, memory, launcher version). The
disc and the cloud token are left out. It then uploads a second zip (`dw3-send-<date>.zip`, deleted after, each log cut
to its last 200 KB, a crash report to 100 KB, the whole under 1.3 MB so it fits the server's 1.5 MB, and the home folder's name
taken out of the text) to the cloud worker's `POST /v1/logs` (`cloud-sync/src/index.ts`), which needs no
account, allows 5 uploads an hour and 20 a day per IP and keeps them `RETENTION_DAYS` days. The launcher shows the
report's code (`XXXXX-XXXXX`); the bug report form requires it (`docs/CONTRIBUTING.md` "Reporting issues"). The
developer reads a report with `GET /v1/admin/logs` and `/v1/admin/logs/<code>` (`X-Admin-Secret`). Setup on a database
from before it: `npm run db:migrate:logs:remote` in `cloud-sync/`.

`NEW VERSION` (DEBUG, when GitHub has a newer release and `CHECK FOR UPDATES` is on) **installs it** on Windows and in the
Linux AppImage; everywhere else (a checkout, a folder that cannot be written, PortMaster, under RecompLauncher, Android's own
way) A still opens the release's page. `launcher/update.c` picks the platform's asset and its sha256 from the release's
API answer (the `digest` of each asset); `launcher/selfupdate.c` copies `dw2003-updater` to `.update/run/` (Linux:
`.dw2003-update/run/`, beside the AppImage, outside the mount) and runs the copy; `launcher/updater.c` is that program, with no
window and no SDL, the only code that touches installed files:

1. downloads the asset (WinHTTP on Windows, curl on Linux, as the cloud sync talks) and checks its size and sha256;
2. unpacks a zip with miniz into `.update/new/`, refusing any entry that is absolute, has `..`, a backslash or a drive, and
   runs the staged launcher's `--version` once (it must end with 0);
3. writes `READY` to `update.status` and waits for `update.go` (the launcher writes it when the player presses START) **and** for
   the launcher's process to be gone;
4. moves each file the package has from the install into `dw2003-backup/` (Linux: `.dw2003-backup/`, beside the AppImage; not a bare `backup/`, which may be the player's own folder) and the new one into its
   place, by rename on the same volume: Windows lets a running executable be renamed, not deleted or overwritten. The
   previous update's backup folder is deleted first (links are removed, never entered); files the package does not have, and the player's own, are never touched;
5. any failure puts every moved file back, newest first, and starts the old launcher; `dw2003-updater --rollback --dir D
   --backup B` does the same by hand. `update.log` (in the install folder) says what happened;
6. starts the new launcher. The launcher removes `.update/` and `update.status` at its next start.

While the updater works the launcher shows the runner minigame with the stage and the percent; when it is staged a notice
says to press START to restart. SELECT hides the notice (the staged update waits) or, while it still downloads, cancels it.
Nothing closes by itself. The hash comes from the same GitHub release as the file: it catches a broken or altered
download, not a compromised release. `tests/launcher_updater_test.py` runs the updater and the launcher's side of it
against a local fake release (a good update, a wrong hash or size, a 404, zip-slip, a launcher that does not start, a
failure in the middle of the swap, no go-ahead, a single-file package, the cancel).

### Widescreen

`widescreen`. One option, `field` (on by default; applies at restart), no hotkey: while enabled, the battle (FIGHTSTG)
and, with `field`, the field (FIELDSTG) are shown at the screen's shape, up to 16:9 (a 16:10 Steam Deck gets 16:10),
through the GPU renderer's wide canvas (psxstack `runtime/render_gpu_wide.c`; issues #71, #78), which draws what the
game sends past the 320-wide screen's edges. The mod's frame callback (`port/game/game_mods.c`) calls
`port_video_widescreen(1)` while FIGHTSTG (or FIELDSTG) is the tier-1 overlay; its start asks for the canvas and a
16:9 window (`port_video_widescreen_enable`) and for the larger VRAM (`port_video_vram_extend`).

- **The battle:** the game is unchanged. FIGHTSTG already sends the 3D a 16:9 frame needs (its only screen-space cull,
  `fightstg_model_mesh_is_visible`, keeps a mesh with a bounding point within the clip plus 64 pixels: 224 from the
  centre, against the 213.5 of a 16:9 frame), and the GPU clipped it to the 320-wide drawing area. More of the arena
  shows at both sides; the background clear, fades and flashes cover the whole width. The HUD, the menus and the
  portrait window stay as drawn, in the middle 4:3.
- **The field:** the map is streamed 128x128 tiles, and the 30 tile objects in RAM already cover a 426-wide view, but
  FIELDSTG draws only the 4 x 3 a 320-wide screen shows, from 12 VRAM slots. With `field` on, `port_field_wide`
  (`include/port.h`) is the canvas's columns past each side (53 at 16:9, 32 at 16:10) and the field's hooks draw that
  far: the camera stops with the map's edge at the picture's (`fieldstg_camera_apply`; a map narrower than the picture
  is centred, black at the sides), the background draws up to 5 x 3 tiles from 15 slots (`fieldstg_background_draw`;
  slots 12-14 and their CLUTs in the extended VRAM's rows 512-1023), its fills cover the width (the map's fades and
  closing window), the map title's bars reach the picture's edge, and `gfx_get_view_rect` is that much wider (the
  culling of FIELDSTG's sprites and of the stages'). The HUD, the message windows and the menus stay in the middle.
  Unlike the battle this changes the game's state (the camera's clamp, what is culled): a run's record differs from
  the 4:3 one. With the software renderer `port_field_wide` is 0 and the field is the original's.
- **What stays 4:3:** the menus and every other overlay (320-wide 2D whose art ends at the screen's edge;
  pillarboxed), the movies, and everything with the software renderer (logged once).
- **The window** keeps its size; the picture switches between 4:3 and 16:9 inside it. A new window opens 16:9
  (`video.scale` x 427 x 240) when the mod is on and `video.renderer` is `gpu`.
- **Known limits:** the digivolution's 2D effects are 320-wide art and may show their edges at the sides; a stage
  modelled only at the front (stage kind 4) may show its back at the sides. In the field, something an event keeps just
  off the 320-wide screen now shows, a stage's own backdrop (WSTAG375 and WSTAG949 have parallax layers) may not cover
  the sides, and the closing window's first step drops the sides at once. All are for the play-test.

`tests/port/render_gpu.py` (a GPU device): with `field` off, a battle frame's middle 320 columns equal the software
image byte for byte, its sides are drawn, the present is 16:9, the field stays 4:3, the frame log equals the run
without the mod; with `field` on, field frames are 426 wide, their middle the run's software image and their sides
drawn. `tests/port/mods.py` (headless, `field` on): the frame log unchanged (the field's hooks do nothing without the
canvas) and the software path's 4:3 logged once.

### LED bar

`led_bar`. No options: the on/off switch is the option. **Only the RK3326 handhelds have the hardware** (R36S, XF35H
and their clones: an MCU on `/dev/ttyS2`, `docs/XF35H.md` "The LED bar is on a UART, not in sysfs"), so everywhere
else the mod is a no-op that logs one line. While enabled and in a battle (FIGHTSTG is the tier-1 overlay) its frame
callback (`port/game/led.c`) reads the acting member's health and sends **one message per change** to `ledd`, the
package's daemon: the Digimon in front picks one of six readings, so a switch updates the bar at once.

| the acting member | the bar |
|---|---|
| 100-81% | green |
| 80-61% | green, breathing |
| 60-41% | lilac, breathing |
| 40-21% | red, breathing |
| 20-1% | red, blinking (200 ms on, 200 ms off) |
| 0 | the bar's own rotating mode, every colour |

Outside a battle the bar goes dark and its power rail drops: nothing is being watched there.

- **Why a daemon and not the game writing the bar:** what costs in that path is not the byte (1 ms on the wire at 9600
  8N1) but paying the setup every time. Measured on the console: the vendor script 66 ms a change, its binary 18 ms,
  opening and closing the tty with no fork at all ~14 ms, the same fd held open 0.15-0.9 ms. One process owns the line,
  takes an `flock` so there is only ever one, coalesces the requests of callers that queue up behind each other, and
  runs the animation's own clock. A change then costs the game ~2 ms, a handful of times a fight.
- **The animation does not outlive the game:** every reading is sent with the game's pid, and the daemon turns the bar
  off within a second of that process going away - a clean exit, a crash and a `kill` alike.
- **What it does not do:** it never changes the console's own LED setting (`ee_led.mode`); EmulationStation applies
  that at boot and when its LED screen is used, and a writer a lock cannot reach is left alone. There is no brightness
  in the hardware - a mode byte and nothing else - so the reading is carried by colour and rhythm.
- **Known limits:** the palette is the measured one, not the names of the vendor script, and the breathing modes were
  judged by eye on one console (`docs/XF35H.md`). Mode 25 and the `times` argument are untested.

The daemon's own tests are the device (`/.dumps/led-test/`): the palette map, the cost of each path and the singleton
lock. On the host there is nothing to see: `tests/port/mods.py` keeps the mod's silent no-op honest.

### Battle menu

`battle_menu`. **Always on** (the definitive combat:
[`definitive-edition/combat.md`](definitive-edition/combat.md)); the mod's record only carries its options (the
buttons, `remember_cursor`, `buffer_input`), which apply while it is enabled and otherwise hold their defaults. The one
exception is a replay script (`--script`), which keeps the original list. A seventh button, **Inspect** (L2, or R2 and
then the free places when the player put an action there), opens a panel with the enemy's five element families (weak,
neutral or resisted once found out, `???` otherwise; combat.md "Knowing your enemy") and spends no turn; what kind of enemy it is (its traits and their legend) and the next turns (enemy twice before us: two red chips before the blue one). An eighth button, **Blast** (R2, the right tab
above Tech; the free places when it clashes), sets the acting Digimon's Blast off once its gauge is full and spends the
turn (see [`definitive-edition/blast.md`](definitive-edition/blast.md#the-gauge) "The gauge"); the tab **is** the
gauge's bar (a dark slot that fills from its left end as the gauge charges), so with the gauge not full it is grey and
refused; full, it is lit and the acting Digimon may Blast; at a critical moment (a partner fell, or the acting Digimon
is at a quarter of its HP) the lit tab shines and shakes: it calls, with no line of its own. Both launchers list it: the
C one's BATTLE HUD screen (`bm_blast`), the psxstack one from the manifest.
Redesigns **only the battle's main command menu** (Fight, Tech, Digivolve, Tag, Item,
Run): the list and its cursor are gone, each action has its own button, and the menu is one compact block in the left
middle of the screen, clear of the enemy and of the fighter the player watches: the shoulders' two rows on top (L2
Inspect over L1 Item at the block's left, R2 Blast over R1 Tech at its right, as on the pad) and the four face buttons
below them as a diamond on a dark rhombus (top Digivolve, left Run, right Tag, bottom Fight; each action sits on the
button the player mapped it to). Each action is a chip with its button's glyph (opaque, outlined in the Xbox letter's
colour, or the PlayStation symbol) and the command written out in the callouts' polygon capitals, the chip where the
button is (on the diamond, on the block's edge for a shoulder) and the word beside it. Fight has the largest word and a
warm edge; a refused press flashes its tab red. The Inspect panel and the refusal's box open in the right middle, and
the battle's portrait window (the acting Digimon's close-up) is not opened with the menu.
A short slide-in and press feedback (`fightstg_battle_menu_update`,
`src/fightstg/fightstg_8008D3B4.c`; the 2026-11-23 redesign: `.plan/tasks/battle-hud-redesign.md`).
The submenus (techniques, items, Digivolve, Tag), the confusion menu and the rest of the battle are the game's own.
With the mod off nothing runs: the dispatch is one `#ifdef PC_PORT` test where the menu is created.

| option | default | |
|---|---|---|
| `fight` | bottom button | the place of each action on the pad: `south`, `west`, `north`, `east` (the face buttons by position), `l1`, `r1`, `l2`, `r2` |
| `tech` | R1 | |
| `digivolve` | top button | |
| `tag` | right button | |
| `item` | L1 | |
| `run` | left button | |
| `inspect` | L2 | |
| `blast` | R2 | |
| `remember_cursor` | `true` | the technique and item menus open on what that Digimon used last (by member ID and kind); the command menu keeps its cursor between turns |
| `buffer_input` | `short` | `off`, `short` (1.0 s) or `long` (1.5 s): presses made in the window right before the command menu opens are given to it as it opens (up to 2). An action's press is given without the command menu being drawn, and with a confirm behind it the remembered technique or item (see `remember_cursor`) is executed at once, without drawing that menu either; a refused press still just leaves the menu open |

- **It follows the button mapping.** The options name a *place* on the player's pad, not a PS1 button. `pad.map`
  (`dw3_pad_permute`) says which PS1 bit the player's button at that place delivers (`port_battle_menu_bit`), so
  remapping the pad moves the action with the button, and the icon beside each label is that button's glyph, which
  `dw3_button_glyph` already turns into the player's own letter (A/B/X/Y on the handhelds). L1/R1/L2/R2 are drawn as text.
  The symbol uses the 9-pixel font, the pictures the 14-pixel one.
- **Same contract as the list:** the menu's `result` is -1, then 0-5 (Fight, Tech, Digivolve, Tag, Item, Run); a Digimon
  that cannot Fight, Tech or Digivolve (status 8) or Digivolve (0x20) gets those labels greyed and the press refused, as
  the list does. The confirm sound plays on a press, nothing else in the battle changes, and no random number is drawn.
- **Two actions on one button** cannot both work: the default buttons are used and one line is logged.
- **Known limits:** the D-pad is not offered as a shortcut (the left stick also sets its bits). Two actions on the left
  or right shoulder row stack one under the other. The confusion menu keeps its list.

`tests/port/mods.py` (headless): the mod on, each button opens its submenu and nothing else does, a refused action does
nothing, the mod off gives the original's frame log.

## 50/60 Hz

A setting, not a mod: `video.refresh: 60` or `--refresh 60` (the engine's own default is 50, PAL; the launcher writes 60 by default and its GAME screen has a REFRESH RATE row, PC and Android). It sets, before `port_overlay_init()`
(so the reset's snapshot holds it):

- `records_60hz = 1`, the flag the NTSC patch sets (`0x8005CCAC`). The game then runs its own 60 Hz mode: the video
  mode, the time step per vsync, actor speeds, the sound tick and the fade rate are compensated, as on a patched PS1.
- `port_rate` and the pace 60 (735 audio samples a vsync).
- `psyq_cd_set_vsync_hz(60)`: the drive's rate stays per second (2.5 sectors and 735 XA frames a tick; seeks keep their
  milliseconds).

`main_screen_pos` (the patch's second word, `0x8005CCB0`) stays 1 (PAL): it only adds the PAL screen offset, which the
port's video ignores, and picks the card game's PAL layout. `--fps 60` alone is not the 60 Hz mode: it speeds
everything up, music and the play-time clock included.

`tests/port/hz60.py` compares the port at 60 Hz with the patched game in the emulator (`tests/port/ntsc_patch.lua`
writes the two words at `main`'s entry; references in `tests/port/hz60/`).

## Schema versions and migrations (2026-11-24)

Every file that is kept between releases carries a schema number, and a release that changes one adds a step to
`launcher/migrate.c` (from N to N+1) instead of the code reading the old shape. The launcher applies the steps in
sequence, from the file's own number up to the current one, and keeps the old file beside it as `<file>.v<N>.bak` (the
first copy is never overwritten).

| File | Number | Current | Migrated |
|---|---|---|---|
| `tui.state` | `schema=N` (none: 0) | 1 | by `cfg_load`, before it reads the file. 0 -> 1: Fight at the bottom (was `bm_v=2`) and 60 Hz, for a file still on the old defaults |
| `card1.def.json`, `card2.def.json` | `"version": N` | 2 | by `migrate_saves`, before the game starts (PC and Android). 1 -> 2: the `"respec"` section |
| `settings.json` | `"schema": N` (the game's contract) | 1 | rewritten whole on every save: no step |

A newer file than the launcher knows is left alone (the game does the same with the save). Files that come from outside
(a restored backup, a cloud pull, the RecompLauncher mirror) keep their old number until the next game start; the
migration then changes the JSON's bytes, so the cloud sees one local change and pushes it. To add a migration: raise
`SCHEMA_*` in `migrate.h`, append the step function to the file's table in `migrate.c`, raise the game's own number when
it writes the file (`DEF_EXPANSION_VERSION`), and add the case to the launcher's `--self-test`.
