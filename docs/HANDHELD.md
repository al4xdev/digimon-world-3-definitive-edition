# Running on a handheld

How the port runs on PortMaster ARM handhelds (`armhf` and `aarch64`: RK3326 R36S/XF35H and clones, H700, RK3566, A133P TrimUI, Mali-G31, PowerVR, Panfrost, across 480x320 to 1280x720 panels), how to build and deploy the multi-arch ARM package, and what is known not to work. Only one handheld with one firmware (XF35H, EmuELEC 4.7-Nexus) has been tested on physical hardware; all other CFWs and resolutions are verified synthetically (source audit + Docker/QEMU simulation; see `.ci-cd/portmaster/README.md`).

Related documents:

- [`../.ci-cd/portmaster/README.md`](../.ci-cd/portmaster/README.md): the page a player reads (install, disc, controls).
- [`../.ci-cd/portmaster/handheld/README.md`](../.ci-cd/portmaster/handheld/README.md): the launcher, the menu and the package flow, for developers.
- [`LAUNCHER.md`](LAUNCHER.md): the mods, the settings file and the LED bar.
- [`XF35H.md`](XF35H.md): the panel, LED bar and rumble of the XF35H.
- [`PERFORMANCE.md`](PERFORMANCE.md): frame costs and how to measure them.
- [`gpu/README.md`](gpu/README.md): how the GLES renderer was designed and measured.

- [Renderers and video](#renderers-and-video)
- [Running](#running)
- [Building](#building)
- [Deploying and rolling back](#deploying-and-rolling-back)
- [Controls](#controls)
- [Mods and menu defaults](#mods-and-menu-defaults)
- [Environment variables](#environment-variables)
- [Known limitations](#known-limitations)

## Renderers and video

The binary has two renderers, and the picture leaves through `/dev/fb0` with either (a window swap costs 17 ms on this
hardware, the framebuffer present under 1 ms).

- `--renderer software`: the CPU rasteriser, byte-identical to the PS1's. It is the binary's default and the reference
  the other renderer is checked against.
- `--renderer gles`: the VRAM is an RGBA8 texture on the Mali-G31, drawn offscreen through EGL on a GBM device (no window,
  no DRM master, no mode change) and read back for the same fb0 present. In the heaviest battle it is faster than
  software (41.85 against 37.70 fps) and in light 2D scenes it is slower (43.5 against 57.7 fps on the scripted boot).
  See [`PERFORMANCE.md`](PERFORMANCE.md).

The launcher selects `gles` by default only when `/proc/cpuinfo` reports CPU part `0xd04` (the Cortex-A35): it exports
`DW3_DEFAULT_RENDERER=gles`. On any other CPU the default is software. The handheld menu has no video screen (its VIDEO
screen is PC-only): it keeps the `video.renderer` already in `ports/dw3/settings.json` and writes that default only
when the file has none. To switch, set `video.renderer` in `settings.json` (`gles` or `software`) or
`DW3_FORCE_RENDERER`, which always wins. The shell launcher passes `--renderer gles` itself only when it starts the
game without the menu and there is no `settings.json` yet.

The VRAM of the `gles` renderer differs from the software one in 0.2-0.7% of its texels (edges and 5-bit rounding). At
the frames compared with `--screenshot` the picture that reaches the screen is byte-identical.

## Running

On a PortMaster install the launcher `Digimon World 3.sh` sits in `ports/`, next to the `dw3/` directory. On the EmuELEC
handheld this port was developed on, EmulationStation runs it from `/storage/roms/ports_scripts/` and the game directory
is `/storage/roms/ports/dw3/`.

The launcher sources PortMaster's `control.txt`, finds the disc image, chooses the video path and runs the menu
(`dw3-launcher`, built from [`launcher/`](../launcher)), which starts the game (`dw2003.<arch>`) itself behind a loading screen. In
outline:

- The disc is the first existing file of `dw3`, `dw2003`, `Digimon World 2003` or `Digimon World 3` with a `.cue` or
  `.bin` extension in `game/`, otherwise any `.cue` or `.bin` there. With none, it shows an error and exits.
- Video: `DW3_FORCE_VIDEO` wins. Otherwise a Wayland session selects SDL2, a readable and writable `/dev/fb0` selects
  fbdev, and anything else selects SDL2. The menu draws on the framebuffer with fbdev and through SDL2 otherwise; when
  it finds no screen at all (exit code 3) the launcher starts the game itself, from `settings.json` as it is.
- It checks with `ldd` that the 32-bit libraries are present before starting.
- Logs go to `logs/game.log`; the launcher's own output is in `log.txt` and the menu's in `menu.log`.

The step-by-step flow (menu exit codes, skip rules, log levels) is in [`../.ci-cd/portmaster/handheld/README.md`](../.ci-cd/portmaster/handheld/README.md).

The device directory:

```
/storage/roms/ports/dw3/
  dw2003.armhf      the port binary (ARM 32-bit; .aarch64 for 64-bit)
  dw3-launcher.armhf  the pad menu (.aarch64 for 64-bit; built from launcher/)
  ledd.py           the LED bar's daemon       (from .ci-cd/portmaster/handheld/)
  probe.sh          diagnostic, run only with heavy logs (from .ci-cd/portmaster/handheld/)
  README.txt        what each file is, on the device (from .ci-cd/portmaster/handheld/)
  settings.json     the player's settings and mods
  pad.map           button letters shown in text
  game/             the disc image (the player's own)
  card1.mcd         memory card 1
  logs/             game.log
```

`.ci-cd/portmaster/handheld/` is the versioned source of everything in that directory except the two binaries (built by
`.ci-cd/portmaster/build_arm.sh`) and `settings.json`, `pad.map`, `game/`, `card1.mcd` and `logs/`, which belong to the player.

`Select + Start`, `SIGTERM`, `SIGINT` and `SIGHUP` end the run at the end of the current vsync, through the normal exit
path, so the log is closed.

## Building

The ARM cross-builds run in Docker (`.ci-cd/portmaster/Dockerfile.armhf` for 32-bit `armhf` and `.ci-cd/portmaster/Dockerfile.aarch64` for 64-bit `aarch64`, both on Debian 10 `buster` against glibc 2.28), so nothing is installed on the host:

```sh
.ci-cd/portmaster/build_arm.sh               # builds both armhf and aarch64 (DW3_ARCH=armhf|aarch64 for one)
```

It configures `build/armhf-sdl2` (`.ci-cd/portmaster/cmake/armhf-linux.cmake`) and `build/aarch64-sdl2` (`.ci-cd/portmaster/cmake/aarch64-linux.cmake`) with Ninja (`dw2003` + `dw3-launcher`).

It then assembles a multi-arch package directory for deploying to a device: `Digimon World 3.sh` and `dw3/` (`dw2003.armhf`, `dw2003.aarch64`, `dw3-launcher.armhf`, `dw3-launcher.aarch64`, everything in `.ci-cd/portmaster/handheld/`, and the mods — no symlinks, so FAT32/exFAT cards work). That directory is `../portmaster`, a sibling of the repository checkout and not the repository's own `.ci-cd/portmaster/`. Set `DW3_PACKAGE_DIR` to put it somewhere else. Do not pipe the script through `head`: the script stops on `SIGPIPE` before it copies the binary, and the next deploy would silently ship the previous one.

To build the zip for PortMaster, run the script after the build:

```sh
.ci-cd/portmaster/make_release.sh
```

It writes `dist/dw3.zip` from `build/armhf-sdl2/` and `build/aarch64-sdl2/`, `.ci-cd/portmaster/handheld/` and the metadata in `.ci-cd/portmaster/`, refuses to build when a required file is missing or the binaries are not valid `armhf` / `aarch64` ELFs, and never includes game data. Publishing steps are in [`../.ci-cd/portmaster/PUBLISHING.md`](../.ci-cd/portmaster/PUBLISHING.md).

## Deploying and rolling back

The device has no `sftp` server, so `scp` fails. Copy with `ssh` and `cat`, over `root@<console-ip>`, keeping the
previous binary as `dw2003.armhf.prev` so that a rollback takes seconds. The target is `dw2003.<arch>` (`dw2003.armhf`
on the XF35H): `Digimon World 3.sh` runs `dw2003.<arch>` whenever it exists and falls back to a plain `dw2003` only
without it, so a binary copied to `dw2003` next to a packaged `dw2003.armhf` is never run.

```sh
ssh root@<console-ip> 'cat > /storage/roms/ports/dw3/dw2003.armhf.new' < build/armhf-sdl2/dw2003
ssh root@<console-ip> 'cd /storage/roms/ports/dw3 && mv -f dw2003.armhf dw2003.armhf.prev && mv dw2003.armhf.new dw2003.armhf && chmod +x dw2003.armhf'
ssh root@<console-ip> md5sum /storage/roms/ports/dw3/dw2003.armhf
md5sum build/armhf-sdl2/dw2003
```

The two checksums must match. The launcher (from `.ci-cd/portmaster/handheld/`) and the menu (the `dw3-launcher` binary of the same
build) are copied the same way:

```sh
ssh root@<console-ip> 'cat > "/storage/roms/ports_scripts/Digimon World 3.sh"' < "handheld/Digimon World 3.sh"
ssh root@<console-ip> 'cat > /storage/roms/ports/dw3/dw3-launcher.armhf' < build/armhf-sdl2/dw3-launcher
ssh root@<console-ip> chmod +x /storage/roms/ports/dw3/dw3-launcher.armhf
```

To go back to an older version, check out the commit, update the `psxstack` submodule to the commit it pins, rebuild and
deploy as above:

```sh
git checkout <commit> && git submodule update
.ci-cd/portmaster/build_arm.sh
```

Two other things to know:

- The card's filesystem refuses Unix sockets, so debug sockets must live in `/tmp`.
- Symbol addresses move between builds. To read a variable out of `/proc/<pid>/mem`, take its address from the binary that
  is actually running.

## Controls

The menu's `CONTROLS` screen can remap the layout. Its rows name the actions, not the PS1 symbols, because the game
rotates the face buttons for every language but Japanese.

| Row in `CONTROLS` | What it does in the game | Default button |
|---|---|---|
| `CONFIRM` (the game's circle) | confirm, talk | A on an Xbox pad, the bottom button on a PlayStation one |
| `BACK` (the game's cross) | cancel, close | B, the right-hand button |
| `MENU`, `CARD` | the other two face buttons: utility buttons | the top and the left-hand button |
| `L1`, `R1` | L1, R1 | L1, R1 |
| `START` | field menu | start |
| `SELECT` | with `START`: quit | select |
| the D-pad and the sticks | move | - |

At a Guardromon, `CONFIRM` offers `SAVE` (the game's own save screen) or `TELEPORT`: a two-column list of every
Guardromon the player has met so far, in the bestiary's layout, where `CONFIRM` chooses a place and a second `CONFIRM`
takes it. `TRIANGLE`/`BACK` goes back. A Guardromon is met by talking to it, and the places met so far travel with the save (the
network: [`definitive-edition/interface.md`](definitive-edition/interface.md),
[`hunt-and-economy.md`](definitive-edition/hunt-and-economy.md)). The `BUTTON GLYPHS` row (`PS1`, `XBOX`,
`NINTENDO`, `CUSTOM`; `XBOX` by default) says what is printed on the pad. The remap is written to `pad.map` and applied
by the game itself (`DW3_PAD_REMAP=1`), because the SDL2 build has a fixed button map of its own. Where each battle
action sits on the pad is the menu's `BATTLE HUD` screen ([`LAUNCHER.md`](LAUNCHER.md) "Battle menu").

## Mods and menu defaults

The mods are described in [`LAUNCHER.md`](LAUNCHER.md). What matters on a handheld is what the menu writes when nobody
opens it. Its defaults are the settings this port was tuned with: 8x fast-forward (audio muted), faster loading at 4x,
battle text in `skip`. The experience rules (the party sharing all the experience, form experience 14x, the three
chosen forms earning) are the Definitive Edition's own, in the game, not settings
([`definitive-edition/progression.md`](definitive-edition/progression.md#experience); the code's view in
[`MECHANICS.md`](MECHANICS.md) section 6).

Fast-forward goes up to 8x where the game has headroom (menus, dialogue, cutscenes). In the field and in a battle the CPU
is the limit, so it only mutes the audio there.

## Environment variables

Set by the launcher; override with the `FORCE` pair:

| Variable | Effect |
|---|---|
| `DW3_FORCE_VIDEO=fbdev\|sdl2` | forces the video path and skips detection; with `sdl2` the menu draws through SDL2 |
| `DW3_FORCE_RENDERER=<name>` | passes `--renderer <name>` to the game |
| `DW3_SKIP_MENU=1` | skips the menu; the launcher starts the game from `settings.json` as it is |
| `DW3_VIDEO` | `fbdev` presents through `/dev/fb0`; set by the launcher. Unset and without `SDL_VIDEODRIVER`, the runtime picks fbdev when `/dev/fb0` is readable and writable |
| `DW3_DEFAULT_RENDERER=gles` | set by the launcher on CPU part `0xd04` |
| `DW3_PAD_REMAP=1` | the game applies `pad.map` itself; set by the launcher |

Read by the game:

| Variable | Effect |
|---|---|
| `DW3_FEAT_ICONS=0` | no button glyph replacement |
| `DW3_FEAT_TRAVEL=0` | no Guardromon network (the Guardromon only saves) |
| `DW3_FEAT_OBJECTIVE=0` | no map objective |
| `DW3_FEAT_VELVET=0` | no Velvet Room ([`definitive-edition/velvet-room.md`](definitive-edition/velvet-room.md)); set to anything else, it also keeps the room under a replay script |
| `DW3_NO_NEON_FLAT=1` | scalar flat textured span, the same binary (to time or to isolate a fault) |
| `DW3_NO_NEON_GOURAUD=1` | scalar Gouraud span |
| `DW3_STATE_ANY=1` | load a save state written by another build; lifts the checksum and image checks, and can hang silently if the state is really incompatible |

Measurement switches (off in the launcher, where they are commented out; see [`PERFORMANCE.md`](PERFORMANCE.md)):

| Variable | Effect |
|---|---|
| `DW3_PRESENT_LOG=path` | one line a second with the present cost |
| `DW3_GPU_LOG=path` | one line a second with the frame's split, GPU and SPU included (1-2 ms a battle frame) |
| `DW3_PHASE=path` | per-phase CPU split; `1` writes to stderr only. Never leave it on to play: it wraps every GTE call and costs about 23 ms of a 20 ms frame |
| `DW3_SKIP_BIG=N` | draw no triangle (by bounding box) or rectangle over N pixels, to weigh a scene's background |
| `DW3_GLES_LOG=1` | `gles` only: one line a second with the renderer's split (listener, flush, present) |
| `DW3_GLES_CHECK=N` | `gles` only: compare the VRAM against the software renderer every N frames |
| `DW3_GLES_REPLACE=0` | `gles` only: the CPU draws as well, so the check above has something to compare; meaningful only with `DW3_GLES_CHECK` |
| `DW3_GLES_DUMP=1` | `gles` only: dump both VRAMs to `/tmp` at the first failed check (`DW3_GLES_DUMP_FRAME=N` picks a frame) |
| `DW3_GLES_NOFLAT=1`, `DW3_GLES_NOTEX=1` | `gles` only: count untextured or textured primitives instead of drawing them, to bisect what paints a region |
| `DW3_GLES_TRACE=N` | `gles` only: log every rectangle of frame N |

## Known limitations

- **The movies show an artefact on `gles`**: a rectangle of wrong pixels in the top-right corner of the film, in some frames
  and not others. The software renderer is clean. No cause is known; [`gpu/17-movie-artefact.md`](gpu/17-movie-artefact.md)
  records what the game does there and every hypothesis already ruled out. No movie has been watched end to end on the
  `gles` renderer.
- **The heaviest battles are over budget**: about 24 ms of work against 20 with `gles` (41.85 fps) and about 27 ms with
  software (37.70 fps). Light battles fit.
- **`gles` is slower than software in light 2D scenes** (43.5 against 57.7 fps on the scripted boot). The handheld
  menu has no video screen: set `video.renderer` in `ports/dw3/settings.json` (`gles` or `software`) or
  `DW3_FORCE_RENDERER`.
- **The VRAM of `gles` differs** from the software one in 0.2-0.7% of its texels. The picture compared at the frames
  tested is identical, which is not a proof for every scene.
- **Widescreen needs the hardware renderer**; it is off on software.
- **Fast-forward mutes the audio**, on purpose: at 8x the sound would be eight times faster and choppy.
- **A teleport** lands on the spot the game itself walks the player to before that Guardromon, and only between
  Guardromon met so far: nothing else travels.
- **The boot and title screen is 320x576** and falls back to the converting present (~16 ms a frame, for under a second).
- **`--input-test` hangs on this device**, with no output and immune to `timeout`. It does not affect playing.
- **The game's own C is built at `-O0`**: `-O2` crashes at boot, because the decompilation has undefined behaviour the
  optimiser exploits.
- **Everything was measured on one handheld with one firmware**, at 60 Hz.