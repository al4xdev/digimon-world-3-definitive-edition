# Launcher, cloud sync, handhelds, Android and PortMaster

[Technical reference index](README.md) · [Player README](../../README.md)

## Launcher, cloud sync, handhelds, Android, PortMaster

- **C launcher** (`launcher/`, `dw3-launcher`, C99): screens Home, GAME, BATTLE HUD, CONTROLS, DEBUG, LED BAR (with two
  confirmations on incompatible hardware), RESET/CLEAN confirmations, and the Trio Runner loading game (jump: Left =
  Kumamon, Up = Guilmon, Right = Agumon). The game reports `port: start:` when it takes the screen.
- **Cloud sync** (`cloud-sync/`, a Cloudflare Worker, TypeScript): routes `/v1/health`, `/v1/register`, `/v1/pair/*`
  (create, claim, poll, status, approve), `/v1/me`, `/v1/devices` and `/v1/devices/revoke`, saves, and admin ban/cleanup;
  PS1 card validation (0x20000 bytes) and rate limits; tests in `cloud-sync/test/`; schema `cloud-sync/schema.sql`.
  `DW3_SYNC_URL`, `DW3_CLOUD_CURL`.
- **Android** (`android/`, `.ci-cd/android/build_android.sh`): `arm64-v8a`, SDL3 GPU, a touch pad
  (`android/touch_pad.c`), SAF disc picker, `FLOATING PAD` toggle. The touch pad hides while a gamepad is connected and
  comes back on the next tap. The SDL3 runtime maps the gamepad from `settings.json` (`input.gamepad`, real SDL places
  on Android: `launcher/cfg.c` `sdl3_name`), so the Android build does not set `DW3_PAD_REMAP` (that is the handheld
  SDL2 runtime's; on Android it permuted the touch pad's buttons and the gamepad a second time). System Back steps back
  in the launcher and does nothing in the game. `versionCode` comes from the tag (MAJOR*10000 + MINOR*100 + PATCH); the
  release key from `DW3_ANDROID_KEYSTORE*` (`release.yml`: the secrets `ANDROID_KEYSTORE_B64`,
  `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD`), else a debug key.
- **Handhelds / PortMaster:** entry `.ci-cd/portmaster/handheld/Digimon World 3.sh` (it runs the C launcher),
  `.ci-cd/portmaster/` (`port.json` arch `["armhf","aarch64"]`), docs `docs/HANDHELD.md`, `docs/XF35H.md`. Probe with
  `Debug logs: heavy` and `logs/probe.txt`.
- **CI and release:** `.github/workflows/ci.yml` and `release.yml`; the flow, secrets and variables are in
  `docs/RELEASE.md`.
- **Performance and renderers:** `docs/HANDHELD.md` and `docs/PERFORMANCE.md`. Widescreen: `docs/LAUNCHER.md`.
