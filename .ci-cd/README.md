# CI/CD

Everything the two workflows run, by target. The workflows themselves stay in
[`.github/workflows/`](../.github/workflows) (GitHub reads them only there) and call the scripts below; every script
also runs by hand from the repository root, with the same result. The release procedure is
[`docs/RELEASE.md`](../docs/RELEASE.md).

| folder | what is in it |
|---|---|
| [`common/`](common) | `ci_areas.sh` (which CI jobs a change runs), `check_paths.sh` (no path from somebody's machine), `check_release_contents.py` (every package held to the list of files a release has), `release_local.sh` (the release's packages built and drafted from this machine) |
| [`linux/`](linux) | `package_appimage.sh` and the AppImage's own files in `appimage/` (AppRun, the `.desktop` file, the icon, the licences) |
| [`windows/`](windows) | `build_windows.sh` (the cross-build with llvm-mingw, tested under Wine), `package_windows.sh` (the release zip) and the zip's own files in `package/` |
| [`android/`](android) | `build_android.sh` and the `Dockerfile` of its builder image; the Gradle project itself is [`android/`](../android) |
| [`portmaster/`](portmaster) | `build_arm.sh` (armhf + aarch64 in Debian 10 images: `Dockerfile.armhf`, `Dockerfile.aarch64`, toolchains in `cmake/`), `make_release.sh` (`dist/dw3.zip`), the package's metadata and art, the player page (`README.md`), the publishing checklist (`PUBLISHING.md`) and `handheld/` (the launch script, the LED daemon, the probe) |
| [`recomplauncher/`](recomplauncher) | how a release is set up for RecompLauncher; the manifest itself is [`recomplauncher.json`](../recomplauncher.json) at the root |

Not here: the setup, build and test scripts every developer runs (`scripts/setup.sh`, `scripts/build.sh`,
`scripts/test.sh`), which CI calls too, and the console launcher's source, [`launcher/`](../launcher).

## Which job runs what

`ci.yml` runs on a push to `main`, a pull request, by hand (`gh workflow run ci.yml --ref <branch>`) and inside a
release. `areas` decides which of the other jobs run (`common/ci_areas.sh`); `check` always runs.

| ci.yml job | runs |
|---|---|
| `areas` | `common/ci_areas.sh` |
| `check` | the scripts parse (`bash -n`, `py_compile`), `common/check_paths.sh`, `tools/hacks.py --check` |
| `game` | `scripts/check_toolchain.sh`, `scripts/build.sh --check`, `scripts/test.sh --layer 2`, the port's replays |
| `port`, `port-mods` | `scripts/test.sh` (layer 1 and port; layer 3 and mods) |
| `launcher` | the SDL game and the launcher, their self-tests, `tests/port/render_*.py` |
| `windows` | `windows/build_windows.sh --launcher --test`, the crash report and the layer-2 replays under Wine |
| `portmaster` | `portmaster/build_arm.sh`, `portmaster/make_release.sh`, `common/check_release_contents.py portmaster` |
| `android` | `android/build_android.sh`, `common/check_release_contents.py android`, `apksigner` |

`release.yml` runs on a pushed tag `vX.Y.Z` (or by hand with a `tag`; without one it builds the packages and
publishes nothing).

| release.yml job | runs |
|---|---|
| Tests (ci.yml) | the whole of `ci.yml`, every area |
| AppImage | `linux/package_appimage.sh --test`, `common/check_release_contents.py appimage` |
| Windows package | `windows/package_windows.sh --test`, `common/check_release_contents.py windows` |
| PortMaster | `portmaster/build_arm.sh`, `portmaster/make_release.sh`, `common/check_release_contents.py portmaster` |
| Android APK | `android/build_android.sh`, `common/check_release_contents.py android` |
| Publish the release | `common/check_release_contents.py dist`, then the GitHub release (a pre-release for a tag with a suffix) |
| Sync to PortMaster-MV-New | `ports/dw3/` in the PortMaster fork, when `PORTMASTER_REPO_TOKEN` is set |

## Which change runs which job

`common/ci_areas.sh` maps each changed path to an area: a change under `portmaster/` runs `portmaster`, under
`android/` runs `android`, `windows/build_windows.sh` runs `launcher` and `windows`; `common/ci_areas.sh` itself and
the workflow run everything. The release-only files (`linux/`, the rest of `windows/`,
`common/check_release_contents.py`, `recomplauncher/`) run nothing in `ci.yml`: `release.yml` tests them, so a change
to them is checked with a manual run of it on the branch.
