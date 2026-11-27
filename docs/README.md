# Documentation

The technical side of the project: how the port is built, how it runs on a handheld, what the decompilation knows of
the game, and how the Definitive Edition is put together. The player's overview is the [top-level README](../README.md).

## The Definitive Edition

- [`definitive-edition/README.md`](definitive-edition/README.md) is the long form of the edition: rules, numbers, files,
  save bytes, tests and pitfalls, one page per subject (its "Pages" table lists them), and how the definitive layers
  work. Read its "How the definitive layers work" first.
- Design and numbers: combat and buffs in [`combat.md`](definitive-edition/combat.md); progression and comfort in
  [`progression.md`](definitive-edition/progression.md) and [`interface.md`](definitive-edition/interface.md).
- Every tunable number is a constant in [`port/game/definitive.h`](../port/game/definitive.h). The edition is always on:
  there is no option to turn it off.
- Old saves: `tools/convert_save_mp.py card1.mcd` converts a card's MP and TP to the new tables (it keeps a backup and
  has `--dry-run`; [`save-format.md`](definitive-edition/save-format.md)).
- The figures the Definitive pages show: [`images/definitive/README.md`](images/definitive/README.md).

## Playing and running

| Document | Contents |
|---|---|
| [`HANDHELD.md`](HANDHELD.md) | running on a handheld, building and deploying the ARM package, the two renderers, environment variables, limitations |
| [`../.ci-cd/portmaster/README.md`](../.ci-cd/portmaster/README.md) | the page a PortMaster player reads: install, disc, the menu, controls, tested devices, known issues |
| [`../.ci-cd/portmaster/handheld/README.md`](../.ci-cd/portmaster/handheld/README.md) | the files that run on the console and the launcher's flow, for developers |
| [`LAUNCHER.md`](LAUNCHER.md) | the launchers, the built-in mods, the settings file, the LED bar and the 50/60 Hz mode |
| [`XF35H.md`](XF35H.md) | the XF35H's panel, LED bar and rumble |
| [`PERFORMANCE.md`](PERFORMANCE.md) | measured frame costs on the handheld, its limits and how to measure them |
| [`../psxstack/docs/HANDHELD.md`](../psxstack/docs/HANDHELD.md) | the stack's own notes on its handheld renderer |
| [`RELEASE.md`](RELEASE.md) | desktop releases (AppImage, Windows) |
| [`BUILDING.md`](BUILDING.md) | building and testing the PC port |

## The port and the project

| Document | Contents |
|---|---|
| [`PORT.md`](PORT.md) | the design of the PC port |
| [`../port/README.md`](../port/README.md) | the port's game side: build commands, `port/game/`, the mods' manifests |
| [`STATUS.md`](STATUS.md) | where the project stands |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | the rules, naming, CI checks and how to report issues |
| [`THIRD_PARTY.md`](THIRD_PARTY.md) | what the project borrows, and under which licences |
| [`../tests/README.md`](../tests/README.md) | the reference tests; the port's in [`../tests/port/README.md`](../tests/port/README.md), the saves' in [`../tests/saves/README.md`](../tests/saves/README.md), the host replay's mismatches in [`../tests/host/FINDINGS.md`](../tests/host/FINDINGS.md) |
| [`../.ci-cd/portmaster/PUBLISHING.md`](../.ci-cd/portmaster/PUBLISHING.md) | publishing the PortMaster package |

## The game and the decompilation

| Document | Contents |
|---|---|
| [`MATCHING.md`](MATCHING.md) | matching functions to the original build |
| [`TOOLCHAIN.md`](TOOLCHAIN.md) | what built the original game |
| [`MECHANICS.md`](MECHANICS.md) | game mechanics reference for the tests |
| [`FORMATS.md`](FORMATS.md) | file formats |
| [`DISC_LAYOUT.md`](DISC_LAYOUT.md) | the disc's layout |
| [`SOUND.md`](SOUND.md) | what the game asks of the sound library and how the port reproduces it |

## Repository layout

| Path | Contents |
|---|---|
| `src/`, `include/` | the game's C, decompiled and matching the original build |
| `port/` | the PC port's adapter for the game ([`port/README.md`](../port/README.md)) |
| `psxstack/` | the runtime stack, a git submodule ([fork](https://github.com/al4xdev/dw3-psxstack)) |
| `launcher/` | the C99 console launcher `dw3-launcher` (PortMaster, Android, a standalone PC build): its screens and the `settings.json` it writes ([`LAUNCHER.md`](LAUNCHER.md)) |
| `.ci-cd/portmaster/handheld/` | the shell launcher and the LED daemon, the source of what runs on the card besides the binaries ([`.ci-cd/portmaster/handheld/README.md`](../.ci-cd/portmaster/handheld/README.md)) |
| `android/` | the Android APK: the entry point, the floating touch pad and the Gradle project (`.ci-cd/android/build_android.sh`) |
| `cloud-sync/` | the Cloudflare Worker behind the launcher's cloud save sync |
| `.ci-cd/` | CI/CD by target: what each workflow job runs, the build and package scripts, the Docker images ([`.ci-cd/README.md`](../.ci-cd/README.md)) |
| `.ci-cd/portmaster/` | PortMaster metadata, `make_release.sh` and the publishing checklist; the ARM builds and their toolchains |
| `tools/`, `scripts/` | build, analysis and check scripts |
| `tests/` | the reference tests ([`tests/README.md`](../tests/README.md)) |
| `docs/` | the documentation, indexed above; the Definitive Edition's in `docs/definitive-edition/` |
