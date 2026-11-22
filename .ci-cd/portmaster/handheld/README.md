# The handheld port's launcher and menu

The files that run on the console, versioned with the port they start. This folder is the source of everything in the
package except the binaries (`dw2003` and the `dw3-launcher` menu, built from [`launcher/`](../../../launcher)), the mods, the
licences and what belongs to the player. For running and deploying, see
[`docs/HANDHELD.md`](../../../docs/HANDHELD.md); the player page is [`.ci-cd/portmaster/README.md`](../README.md); the
publishing checklist is [`.ci-cd/portmaster/PUBLISHING.md`](../PUBLISHING.md).

| file | what it is |
|---|---|
| `Digimon World 3.sh` | the one launcher. It sits beside `dw3/` (flow below). |
| `dw3-launcher.<arch>` (not in this folder) | the console-first C menu, built from `launcher/` by `.ci-cd/portmaster/build_arm.sh`: pad only, 640x480 framebuffer (or SDL2 on desktop), writes `settings.json`, `pad.map`, `tui.state`, and runs the game (`$DW3_GAME_BIN`, `dw2003.<arch>`) behind an animated loading screen until `DW3_STATUS_FD` signals start. |
| `ledd.py` | the LED bar's daemon: the only process that writes `/dev/ttyS2`, started by the game's `led_bar` mod ([`docs/LAUNCHER.md`](../../../docs/LAUNCHER.md), "LED bar"). Holds an `flock` so there is one at a time, and turns the bar off when the pid that asked for the animation goes away. Timings: [`docs/XF35H.md`](../../../docs/XF35H.md). |
| `probe.sh` | a passive environment probe (OS, framebuffer, Wayland, audio, CPU part, Mali, `ldd` of the binary, PortMaster variables). Writes `logs/probe.txt` and changes nothing else. |
| `README.txt` | the short file guide that ships on the card, inside `dw3/`. |
| `README.md` | this file. It is not shipped. |

The player's own files are not here: `settings.json`, `pad.map`, `tui.state`, `card1.mcd`/`card2.mcd`, `game/` (the disc)
and `logs/`.

## How the package is assembled

- `.ci-cd/portmaster/build_arm.sh` builds `dw2003` and `dw3-launcher` in `build/armhf-sdl2/` and `build/aarch64-sdl2/` and mirrors
  the package into `../portmaster`, a directory outside the repository (not the in-repo `.ci-cd/portmaster/`), as
  `dw2003.<arch>` and `dw3-launcher.<arch>`.
- `.ci-cd/portmaster/make_release.sh` writes `dist/dw3.zip` from those two build directories, this folder and `.ci-cd/portmaster/`.
- Both copy every file in this folder into `dw3/` by rule, except `README.md` and the launcher; a new helper script
  needs no list entry.

## The launcher's flow

`Digimon World 3.sh` sources PortMaster's `control.txt`, so the platform variables (`CFW_NAME`, `GPTOKEYB2`, the
`pm_*` functions) come from PortMaster. What it adds:

1. **Disc.** Looks in `game/` for the usual names (`dw3`, `dw2003`, `Digimon World 2003`, `Digimon World 3`, `.cue` then
   `.bin`), then for any `.cue` or `.bin`. With none it shows an error with `pm_show_error` and exits 1.
2. **Video and menu.** In order: `DW3_FORCE_VIDEO`; a set `WAYLAND_DISPLAY` (SDL2); a readable and writable
   `/dev/fb0` (`fbdev`, menu on the framebuffer); otherwise SDL2. The menu (`dw3-launcher.<arch>`) runs with `--fb` on
   `fbdev` and `--sdl` otherwise, unless `DW3_SKIP_MENU=1`.
3. **Libraries.** `ldd` of the binary; missing ARM libraries show an error and exit 1. Without `ldd` the check is skipped.
4. **Renderer default.** CPU part `0xd04` (Cortex-A35) sets `DW3_DEFAULT_RENDERER=gles`. The menu writes it as
   `video.renderer` only when `settings.json` has none (it has no renderer row on the handheld); the script passes
   `--renderer gles` only when it starts the game itself and there is no `settings.json`; `DW3_FORCE_RENDERER` always
   wins.
5. **The menu starts the game.** PLAY writes `settings.json` and runs `dw2003 --config ./settings.json` behind the
   loading screen; the launcher exits with the game's status, and a quit from the menu is 0. Exit code `3` means the
   menu found no screen (or was skipped), so the launcher starts the game itself with `settings.json`. The framebuffer is
   blanked and `pm_finish` runs either way. The menu's output is in `log.txt`, with the device, video path and disc it found.
6. **Logs.** `DW3_LOG` is `soft` or `heavy` when `settings.json` has that `debug.logs` value, and `off` otherwise.
   `probe.sh` runs only for `heavy`.
7. **Input.** `DW3_PAD_REMAP=1` makes the SDL2 build apply `pad.map`. `$GPTOKEYB2` runs in the background only as a kill
   switch; the game reads the pad itself and no `.ini` is used.
8. **Start without the menu** (exit code 3). `--disc <path> --window --fullscreen --watchdog 0`, plus
   `--config ./settings.json` when the file exists. Game output goes to `logs/game.log`; the launcher's own to `log.txt`.

The one-off meters (`DW3_PRESENT_LOG`, `DW3_GPU_LOG`, `DW3_PHASE`) are commented out in the launcher: they cost frames.

## What the menu owns

- `settings.json`: the mods and `input.gamepad`; `video.renderer` is `gles` or `software`. The handheld build is the
  `PSXSTACK_SDL2` one, whose input (`psxstack/runtime/input.c`) has a fixed button map and does not read
  `input.gamepad`; the file still carries the mapping for the SDL3 builds. The hotkeys are read on both:
  `mods.fast_forward.toggle` is the chord of the buttons chosen for SELECT and R1 (`launcher/cfg.c`), shown on the
  `FAST-FORWARD` row.
- The menu writes `settings.json` when PLAY is pressed, not only when a console action runs, because the game reads the
  file once, at its start.
- `pad.map`: one line per PS1 button, `<button> <gamepad input> <letter>`. The name is what the game calls the action
  after it rotates the pad for the non-Japanese languages, so the four face rows read `circle` = confirm, `cross` =
  back, `triangle` and `square`; the gamepad input is SDL's button name (`south`, `east`, `west`, `north`, ...). The game
  applies this file (`dw3_pad_permute`, `DW3_PAD_REMAP` in the launcher) because the SDL2 runtime cannot; the letter is
  only what the text shows, and the menu derives it from the layout. **A button belongs to one action**: binding one
  that another action holds swaps the two, and a file with two actions on one button is repaired when it is read (a
  `select` sharing `circle` would otherwise make CIRCLE+START the exit chord).

## Deploying the launcher

The card's `dw3-launcher.<arch>` must be built from the committed `launcher/` (`.ci-cd/portmaster/build_arm.sh`). The EmuELEC
firmware has no `scp`, so copy it over ssh:

```
ssh root@<console> 'cat > /storage/roms/ports/dw3/dw3-launcher.armhf' < build/armhf-sdl2/dw3-launcher
ssh root@<console> chmod +x /storage/roms/ports/dw3/dw3-launcher.armhf
```

On this firmware the launcher lives in `/storage/roms/ports_scripts/` and the package in `/storage/roms/ports/dw3/`.
In a PortMaster layout the launcher sits in `ports/`, beside `dw3/`.