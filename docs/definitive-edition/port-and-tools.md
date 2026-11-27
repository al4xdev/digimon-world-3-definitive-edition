# Building, running, mods, tools, tests and debugging

[Technical reference index](README.md) · [Player README](../../README.md)

## Building and running (the port)

- Host build: `cmake -S port -B build/port -G Ninja`, then `cmake --build build/port`; run
  `build/port/dw2003 --disc iso/dw2003.cue`. The SDL3 build: `scripts/setup.sh sdl3 dxc` plus `-DPSXSTACK_SDL=ON`.
- Other builds: `.ci-cd/portmaster/build_arm.sh` (builds **both armhf and aarch64**: `DW3_ARCH=armhf|aarch64` for one;
  Docker images from `.ci-cd/portmaster/Dockerfile.armhf` and `.ci-cd/portmaster/Dockerfile.aarch64`, both Debian 10
  against glibc 2.28; toolchains `.ci-cd/portmaster/cmake/armhf-linux.cmake` (`-mcpu=cortex-a35`) and
  `.ci-cd/portmaster/cmake/aarch64-linux.cmake`; trees `build/armhf-sdl2`, `build/aarch64-sdl2`, each with `dw2003` +
  `dw3-launcher`; it assembles a multi-arch package directory `../portmaster` with `dw2003.armhf/.aarch64`,
  `dw3-launcher.armhf/.aarch64`, `.ci-cd/portmaster/handheld/` and the mods, no symlinks so FAT32/exFAT cards work,
  override with `DW3_PACKAGE_DIR`; do not pipe the script through `head`, it stops on SIGPIPE before copying the binary
  and the next deploy would silently ship the previous one), `.ci-cd/portmaster/make_release.sh` (needs both the 32-bit
  ARM and the 64-bit aarch64 ELFs, `build/armhf-sdl2/` and `build/aarch64-sdl2/`), `.ci-cd/linux/package_appimage.sh`,
  `.ci-cd/windows/build_windows.sh`, `.ci-cd/android/build_android.sh`.
- `port/CMakeLists.txt` keeps the game's C at `-O0`: `-O2` crashes at boot (`sound_init` -> `sound_update_loading` ->
  `cdload_get_subfile`, SIGSEGV). It also holds the psxstack workaround block.
- Link failures on `__start_dw2003_data_wstagNNN`: see the Testing note under Super Rare hunts.

### Command line options

`--config`, `--print-settings`, `--print-mods`, `--script-mods`, `--refresh 50|60`, `--disc` / `--game`, `--no-disc-check`,
`--cd-speed`, `--memcard1|2`, `--script`, `--log`, `--record`, `--max-frames`, `--watchdog`, `--trace`, `--window`, `--scale`,
`--fullscreen`, `--fps`, `--input-test`, `--screenshot`, `--renderer software|gpu|gles`, `--internal-scale`,
`--gpu-screenshot`, `--subpixel`, `--filter`, `--dump-textures`, `--texture-pack`, `--mods-dir`, `--spu-trace`, `--wav`,
`--mute`, `--debug SOCKET`, `--debug-hold`, `--save-state W:F`, `--save-state-exit`, `--load-state`, `--crash-dir`,
`--version`. The window's keys are in `--help`.

### Environment variables

Definitive layers: `DW3_DEFINITIVE`, `DW3_BESTIARY`, `DW3_GUIDE`, `DW3_GUIDE_CHECK`, `DW3_GIRLS`, `DW3_GIRLS_DUMP`,
`DW3_GIRLS_NUMBERS`, `DW3_RUSH_FLOAT`, `DW3_RUSH_TEST`, `DW3_BLAST_STEP`, `DW3_BLAST_FORM`, `DW3_BATTLE_LOG`,
`DW3_FEAT_VELVET`, `DW3_FEAT_TRAVEL`, `DW3_FEAT_ICONS`, `DW3_FEAT_OBJECTIVE`.
Video and input: `DW3_VIDEO`, `DW3_FORCE_VIDEO`, `DW3_FORCE_RENDERER`, `DW3_DEFAULT_RENDERER`, `DW3_UI_SCALE`, `DW3_PAD`,
`DW3_PAD_REMAP`, `DW3_FB`, `DW3_NO_NEON_FLAT`, `DW3_NO_NEON_GOURAUD`.
Logging and debug: `DW3_LOG`, `DW3_PHASE`, `DW3_PRESENT_LOG`, `DW3_GPU_LOG`, `DW3_STATE_ANY`, `DW3_STATUS_FD`,
`DW3_PORT_CRASH_AT`, `DW3_PC`, `DW3_EXEC`, `DW3_NOEXEC`, `DW3_GAME_BIN`, and `DW3_GLES_CHECK`, `_DUMP`, `_DUMP_FRAME`, `_LOG`,
`_NOFLAT`, `_NOTEX`, `_REPLACE`, `_TRACE`.
Other: `DW3_LANG`, `DW3_SYNC_URL`, `DW3_CLOUD_CURL`, `DW3_LED_DIR`, `DW3_LED_FORCE`, `DW3_PORT_SKIP_DIALOGUES`,
`DW3_PORT_FAST_FORWARD`, `DW3_PORTMASTER`, `CFW_NAME`, `RECOMPLAUNCHER` / `RECOMPLAUNCHER_MODE`, `SDL_VIDEODRIVER`.

## Mods (`port/mods/`)

`battle_animations`, `battle_menu` (always on), `led_bar` (RK3326 handhelds only),
`preset_language`, `skip_dialogues` (F2), `widescreen` (0.2, `field` option); `fast_forward` is the runtime's. Full
reference in [`docs/LAUNCHER.md`](../LAUNCHER.md).

## Tools (`tools/`)

- Text: `gen_text_override.py` (`--check`), `gen_tech_text.py`, `gen_affinity_text.py`, `translate_ptbr.py`, `dump_text.py`.
- Data generators: `gen_map_graph.py` (`--apply`), `gen_recipes.py`, `gen_replicas.py`, `gen_item_prices.py`, `gen_equipment.py`
 (all `--check`). Legacy, their tables now hand-maintained: `gen_enemy_weaknesses.py`, `gen_hunt_targets.py`, `gen_dri_challenges.py`.
- Saves: `convert_save_mp.py`. Disc and finding things: `disc_files.py`, `disc_survey.py`, `find_dri_agents.py`,
  `bm_npc_candidates.py`.
- Girls Power: `girls_power_draw.py`, `girls_power_preview.py`.
- Port and decompilation: `hacks.py`, `port_inventory.py`, `port_gen.py`, `port_inputs.py`, `flag_census.py`,
  `progress.py`, `psyq_match.py`, `unit_diff.py`, `game_xref.py`, `data_owners.py`, `data_to_c.py`, `coverage.py`,
  `overlay_layout.py`, `overlay_xref.py`, `ref_names.py`, `renames.py`, `split_case_sizes.py`, `wstag_data.py`,
  `wstag_groups.py`, `compiler_id.py`, `mcp_game.py`.

## Tests

`scripts/test.sh` runs layers: `census` (`tools/hacks.py --check`), `probe` (`tools/port_inventory.py probe/link`), `1`
(`tests/golden`, `tests/host` replay, `tests/spu`, `tests/xa`), `2` (`tests/replay` emulator, `tests/sound`), `3`
(`tests/formats`, `tests/saves`), `port` and `mods` (`tests/port/mods.py`). Flags: `--build`, `--layer`, `--m32`,
`--no-probe`; `DW3_JOBS`.

The `port` layer runs, in order: `run.py`, `settings.py`, `render_gpu.py`, `render_filters.py`, `textures.py`,
`tests/host/gpu_hw_replay.py`, `hz60.py`, `battle.py`, `text_override.py`, `inspect_info.py`, `definitive.py`,
`definitive_buffs.py`, `definitive_mp.py`, `definitive_bestiary.py`, `definitive_hunt.py`, `definitive_superhunt.py`,
`definitive_skins.py`, `definitive_midi.py`, `definitive_tp.py`, `definitive_pity.py`, `definitive_gauges.py`,
`definitive_evo_anim.py`, `definitive_train_loss.py`, `definitive_form_floor.py`, `girls_power.py`, `vram.py`,
`subpixel_jitter.py`, `debug.py`, `savestate.py`, `crash.py`, `definitive_midi_battle.py`, `field.py`.
`definitive_dri.py`, `definitive_market.py`, `definitive_economy.py`, `definitive_variants.py`, `definitive_quest.py`
and `definitive_rush.py` also exist but are **not registered in `scripts/test.sh`** : run them by hand (`uv run python
tests/port/<name>.py`).

## Debugging and the MCP

`--debug SOCKET` opens a JSON channel; the MCP server `mcp__dw2003__*` (`game_start`, `pad_press`, `mem_read`/`mem_write` by
symbol or `ps1:` address, `screenshot`, `state_save`/`state_load`, `wait_map`, ...) drives it (`tools/mcp_game.py`;
`tests/port/debug.py`). `DW3_BATTLE_LOG` logs every battle roll. A crash writes a report (`--crash-dir`, `tests/port/crash.py`).
Savestates are valid only for the binary that saved them.
