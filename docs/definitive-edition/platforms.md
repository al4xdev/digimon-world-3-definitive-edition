# Launcher, cloud sync, handhelds, Android and PortMaster

[Technical reference index](README.md) · [Player README](../../README.md)

## Launcher, cloud sync, handhelds, Android, PortMaster

- **C launcher** (`launcher/`, `dw3-launcher`, C99): 9 screens: Home, GAME, BATTLE HUD, CONTROLS, DEBUG, LED BAR (with two
  confirmations on incompatible hardware), RESET/CLEAN confirmations, and the Trio Runner loading game (jump: Left =
  Kumamon, Up = Guilmon, Right = Agumon). Colour palette and duplicate-button blocking are documented in the closed plan 13
  (now here). It starts the game with a `port: start:` handoff.
- **Cloud sync** (`cloud-sync/`, a Cloudflare Worker, TypeScript): routes `/v1/health`, `/v1/register`, `/v1/pair/*`
  (create, claim, poll, status, approve), `/v1/me`, `/v1/devices` and `/v1/devices/revoke`, saves, and admin ban/cleanup;
  PS1 card validation (0x20000 bytes) and rate limits; tests in `cloud-sync/test/`; schema `cloud-sync/schema.sql`.
  `DW3_SYNC_URL`, `DW3_CLOUD_CURL`.
- **Android** (`android/`, `tools/build_android.sh`): `arm64-v8a`, SDL3 GPU, a touch pad (`android/touch_pad.c`), SAF disc
  picker, `FLOATING PAD` toggle. The original design (a Kotlin overlay, Fase 0 audit of `psxstack` for the NDK) is in the
  backlog list below.
- **Handhelds / PortMaster:** entry `handheld/Digimon World 3.sh` (the old `dw3-menu.py` menu was removed in `cbf34ce`; the C launcher `dw3-launcher` replaced it), `portmaster/` (`port.json`
  arch `["armhf","aarch64"]`), docs `docs/HANDHELD.md`, `docs/XF35H.md`. Probe with `Debug logs: heavy` and `logs/probe.txt`.
- **Console deploy handoff (ssh to a handheld, procedure of 2026-11-12; it predates the multi-arch package: the binaries are now `dw2003.armhf` / `dw2003.aarch64` and `dw3-launcher.*`, see `docs/HANDHELD.md`):** the owner supplies `<USER>@<IP>`, the password or key and the PortMaster folder
  (usually `/roms/ports`, or `/roms2/ports`, `/storage/roms/ports`; find it with `ssh <console> 'ls -d /roms*/ports /storage/roms/ports 2>/dev/null'`); `PORTS` below.
  Source is `../portmaster/` (a sibling of the checkout) (the device mirror `tools/build_arm.sh` writes the new build into): copy `dw3/dw2003` (the 32-bit ARM binary; check `file` says
  `ELF 32-bit LSB ... ARM` and the date is current, else run `tools/build_arm.sh`, Docker image `dw3-armhf-builder`), `dw3/mods/`, and only if changed `ledd.py`, `probe.sh`,
  `README.txt` and `Digimon World 3.sh`. **Never overwrite or delete:** `PORTS/dw3/game/` (the player's disc), `card1.mcd`, `card2.mcd`, `settings.json`, `pad.map`, `tui.state`, `logs/`,
  `log.txt`, `menu.log`; no `rsync --delete`, no `scp -r` of the whole folder. **Step 1, mandatory backup:** create `PORTS/dw3-backup-<date>` with `card1.mcd`, `card2.mcd`, `settings.json`,
  `pad.map`, `tui.state`, `dw2003.old`, `mods.old`, the `.sh`; then bring a copy of the saves to `../backups-console/` and check they exist and are not empty (`card1.mcd`
  should be 131072 bytes; if the player never saved, tell the owner and go on). **Step 2:** `scp` the binary and the mods, `chmod +x`, compare `sha1sum` local and remote; the mods `party_xp`, `xp_boost`
  and `evo_buff` no longer exist (rules of the game or removed): **move** the old folders into the backup, do not delete; the launcher also changed (no XP screen, no EVO BUFF); an old
  `settings.json` with those keys keeps working. **Step 3, old saves and MP (the owner's decision, never alone):** the game does not migrate old saves (they open with the old MP); to convert,
  run `tools/convert_save_mp.py --dry-run` then the real one **always on the local copy** in `backups-console/`, only if the owner says so, and only then `scp` the card back after the step-1
  backup. The card gained a bestiary record at the file's tail (last 0x800 bytes, never used by the game): an old save opens with everything "???". **Step 4, verify:** `ls -la PORTS/dw3/`, ask the owner to
  open the game, read `PORTS/dw3/log.txt` and `logs/` (`tail -n 40`) and report an ARM library error, `fatal` or `crash`; to roll back restore `dw2003.old`, `mods.old` and the `.mcd`s from the backup.
  What the owner would see different at the time (to tell bug from intent): the new battle HUD always (7th button Inspect on L2, Start opens the enemy's memory), the player always opens the fight, a
  weakness with an elemental technique staggers and gives an extra action, and enemies kept the disc's resistance values until the owner approved `.dumps/tasks/persona/fraquezas.md`.
- **PortMaster CFW research (plan 11):** muOS 32-bit and TrimUI are **unverified**; see "PortMaster CFW research" under [History](history.md#history--decisions-log) for the plan and the partial results.
- **CI and release (GitHub Actions, state 2026-11-13).** Workflows `.github/workflows/ci.yml` (Linux and Windows; ignores pushes that only touch `docs/**`, `**/*.md`, `LICENSE`, `.gitignore`) and
  `release.yml` (release by tag); `docs/RELEASE.md` has the flow: `git tag v0.1.0; and git push origin v0.1.0` (a suffix such as `v0.1.0-rc1` is a pre-release). At the time of the handoff the push of
  `2d0f647` had triggered **no workflow** (0 runs in total although Actions was enabled; maybe GitHub delay) and both were dispatched by hand (`workflow_dispatch`; the release without a tag only builds
  artifacts, the AppImage and the Windows zip); to check: `gh run watch <id> -R al4xdev/dw3 --exit-status`, `gh run view <id> -R al4xdev/dw3 --log-failed`, `gh run list -R al4xdev/dw3` (the next push to `main`
  touching code should start CI by itself, event `push`; if not, Settings -> Actions -> General). No secrets or variables were configured: `GAMEDATA_DEPLOY_KEY` (optional secret: with it CI and release run
  the tests with the disc; without it only the smoke tests without a disc), `PSXSTACK_TOKEN` not needed (`al4xdev/dw3-psxstack` is public), `RELEASE_DRAFT=true` (variable) before the first tag if the release
  should be a draft. (`gh` is not installed on this machine, so those commands are for wherever it is.) The `main` of GitHub had received a force-push (`841521a` -> `ca5867d`) before `2d0f647`; `ca5867d` was
  already in the local history so the push was a fast-forward that deleted nothing; if `841521a` held something, it is not on `main`.
- **The psxstack pin fix (still pending).** The pin (`5e88c08` of `dw3-handheld` in `al4xdev/dw3-psxstack`) does not build the PC targets as is, hence the workaround block in `port/CMakeLists.txt`
  (three things: `-Wno-error=unused-variable` in `runtime/video.c` without EGL, `fbdev_present.c` out of the build without SDL2, an empty `sys/syscall.h` on Windows); the lab's build (SDL2 and EGL off)
  passes through it and the tests pass. The real fix is this patch **in the psxstack repo** (`dw3-handheld`), then update the pin and remove the block. Before applying, check what the patch assumes: it takes
  `fbdev_present.c` and `render_gles.c` out of the runtime glob and says they come back with `PSXSTACK_SDL2`/`PSXSTACK_EGL` further down in `psxstack.cmake`; the patch does not show that part and it was **not verified**;
  if those options do not add the files back the handheld build loses its presenter. The patch (a copy was in `.dumps/tasks/ci/psxstack-pc-build.patch`):
  ```diff
  --- a/cmake/psxstack.cmake
  +++ b/cmake/psxstack.cmake
  @@ -293,6 +293,9 @@
   
       # ---- The binary: the runtime, the adapter, the tables, the shim, the units (and on PE the bracket symbols).
       file(GLOB RUNTIME_SRCS CONFIGURE_DEPENDS "${PSXSTACK_ROOT}/runtime/*.c")
  +    # The handheld's presenter and renderer join the build with their options (PSXSTACK_SDL2, PSXSTACK_EGL, below):
  +    # fbdev_present.c includes <linux/fb.h> and the Linux ioctls, which break the Windows build.
  +    list(FILTER RUNTIME_SRCS EXCLUDE REGEX "/runtime/(fbdev_present|render_gles)\\.c$")
       set(PLATFORM_SRCS "")
       if(WIN32)
           _psxstack_run("${PSXSTACK_PORT_GEN}" markers --id "${ID}" --overlays "${G_OVERLAYS}" --out "${GEN}/markers.c")
  --- a/runtime/main.c
  +++ b/runtime/main.c
  @@ -4,7 +4,9 @@
   #include <errno.h>
   #include <stdlib.h>
   #include <string.h>
  +#ifdef __linux__ /* SYS_sched_setaffinity (below); Windows has no such header */
   #include <sys/syscall.h>
  +#endif
   #include <unistd.h>
   
   #include "platform.h"
  --- a/runtime/video.c
  +++ b/runtime/video.c
  @@ -76,7 +76,9 @@
   static int video_converted_frame = -1; /* the frame video_pixels was converted at (the debug channel's screenshot) */
   static int video_gpu_wanted;            /* --renderer gpu / video.renderer "gpu" */
   static int video_gles_wanted;           /* --renderer gles: the offscreen GLES renderer (render_gles.c) */
  +#ifdef PSXSTACK_EGL /* only the GLES paths read it: unused (-Werror) in the other builds */
   static int video_gles;                  /* the offscreen GLES renderer draws; the present still comes out through fbdev */
  +#endif
   static int video_internal_scale = 1;    /* --internal-scale / video.internal_scale: the rasteriser's, 1..8 */
   static PortFilter video_filter;         /* --filter / video.filter: the hardware renderer's present (render_gpu_present.c) */
   static struct {
  ```
  (hunk line counts and context are as in the original; whitespace-only context lines may be off by trailing spaces.)
- **Widescreen field (handoff note, since superseded on PC):** the battle mod existed (`port/mods/widescreen` 0.1) and shipped in both packages; the field mod 0.2 of the original fork (`7c47270`)
  needed runtime changes the pin did not have, which first had to enter `dw3-psxstack`. The README above says PC now has 16:9 in battles and the field; whether the handheld package carries the
  field part is **unverified**.

### Performance and what failed (closed plans 1-4, summary)

The R36S/XF35H port, the GLES backend (41.85 fps against software 37.70 in battle; 23.9 ms a frame, present about 3.2 ms,
0.7% VRAM difference), SPU/NEON audio (4.9 ms down to 3.0-3.3 ms), environment detection and the single PortMaster launcher
were done in the closed plans; their conclusions are in `docs/HANDHELD.md`, and the plans' own account of what failed or was left
undone is in the [History](history.md#history--decisions-log) section below. Widescreen field: see `docs/LAUNCHER.md`.
Handoff notes: GitHub Actions / CI and the psxstack pin fix are under "CI and release" above; the first release goes out with a `v0.1.0` tag.
