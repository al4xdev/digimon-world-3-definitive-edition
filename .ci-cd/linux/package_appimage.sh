#!/usr/bin/env bash
# Builds the release AppImage (DECISIONS "Releases: tagged drafts, published by hand"; docs/RELEASE.md): the game (the SDL build) and the launcher
# (launcher/) in Release, an AppDir with both in usr/bin/ (the launcher finds the game beside itself) and the mods' manifests in
# usr/bin/mods/, AppRun (the launcher, with its arguments), a .desktop file, the icon, recomplauncher.json and LICENSES/, then appimagetool with the pinned
# static runtime (no libfuse2 needed to run it). Players supply their own disc in the launcher.
#
# Usage: .ci-cd/linux/package_appimage.sh [--version V] [--out DIR] [--sdl3 DIR] [--test]
#   --version V  the version in the file name (default: the release tag being built, vX.Y.Z..., else the last such
#                tag by `git describe`, else dev-<commit>; a leading v dropped)
#   --out DIR    where the AppImage and SHA256SUMS go (default build/release/)
#   --sdl3 DIR   the SDL3 install to link (default $DW3_SDL3_DIR, else tools/sdl3-desktop, else tools/sdl3); it must
#                have the desktop backends (X11, Wayland, PipeWire, PulseAudio, ALSA): `scripts/setup.sh sdl3-desktop`
#   --test       then run the AppImage's smoke test: the launcher's --self-test inside the AppImage (extract-and-run)
#                with DW3_SELFTEST_GAME=beside (the bundled game and mods, found by the launcher itself) and, when
#                iso/dw2003.cue exists, the disc (the bundled game runs 300 frames from PLAY, offscreen)
# Needs: scripts/setup.sh sdl3-desktop appimage dxc; cmake, ninja, gcc, binutils, python3.
# Outputs: <out>/dw2003-<version>-x86_64.AppImage, <out>/dw2003-<version>-x86_64.debug (the game's debug info, which
# symbolizes crash reports: scripts/symbolize.py, docs/PORT.md "Crash report"), <out>/SHA256SUMS; build trees and logs
# under build/release/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MAIN="$(dirname "$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir)")"
JOBS="${DW3_JOBS:-$(nproc)}"
log() { printf '\033[1;34m[package]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[package]\033[0m %s\n' "$*" >&2; exit 1; }

version="${DW3_VERSION:-}" out="$ROOT/build/release" sdl="${DW3_SDL3_DIR:-}" test=0
export PSXSTACK_VERSION="$version"   # psxstack/cmake/version.cmake reads it (DW3_VERSION stays the release's input)
while [[ $# -gt 0 ]]; do
    case "$1" in
        --version) version="$2"; shift 2 ;;
        --out) out="$2"; shift 2 ;;
        --sdl3) sdl="$2"; shift 2 ;;
        --test) test=1; shift ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) die "unknown argument: $1" ;;
    esac
done

# A built tool: this checkout's tools/<name>, else the main checkout's (scripts/setup.sh installs there).
tool() {
    if [[ -e "$ROOT/tools/$1" ]]; then echo "$ROOT/tools/$1"; else echo "$MAIN/tools/$1"; fi
}

# ---- The version: the release tag being built (CI), else git describe over the release tags (vX.Y.Z...; not the
# decomp's milestone tags such as v0.1-matching-closed).
if [[ -z "$version" ]]; then
    if [[ "${GITHUB_REF:-}" == refs/tags/v* ]]; then
        version="${GITHUB_REF#refs/tags/}"
    else
        # --always: the commit alone when no release tag precedes it
        version="$(git -C "$ROOT" describe --tags --match 'v[0-9]*.[0-9]*.[0-9]*' --always --dirty)"
        [[ "$version" == v* ]] || version="dev-$version"
    fi
fi
version="${version#v}"
export DW3_VERSION="$version"   # the launcher's CMake reads it (launcher/CMakeLists.txt): it is the X.Y.Z the update check compares
[[ "$version" =~ ^[A-Za-z0-9._+-]+$ ]] || die "a version of letters, digits and ._+- only: '$version'"
name="dw2003-$version-x86_64.AppImage"
debug="dw2003-$version-x86_64.debug"

# ---- The tools.
[[ -n "$sdl" ]] || { sdl="$(tool sdl3-desktop)"; [[ -d "$sdl" ]] || sdl="$(tool sdl3)"; }
[[ -f "$sdl/lib/libSDL3.a" && -f "$sdl/lib/cmake/SDL3/SDL3Config.cmake" ]] ||
    die "no SDL3 at $sdl: scripts/setup.sh sdl3-desktop (or --sdl3 DIR)"
[[ -x "$(tool dxc)/bin/dxc" ]] || die "no DXC for the hardware renderer's shaders: scripts/setup.sh dxc"
PATH="$PATH:$(tool venv)/bin"   # cmake/ninja from tools/venv when the system has none (scripts/setup.sh cmake)
for t in cmake ninja gcc strip ldd nm objdump objcopy; do
    command -v "$t" >/dev/null || die "no $t on PATH"
done
# The desktop drivers (the same list as setup.sh's SDL3_DESKTOP_DRIVERS): without them the AppImage could only open
# an offscreen window.
syms="$(nm -g --defined-only "$sdl/lib/libSDL3.a" 2>/dev/null)" || die "nm cannot read $sdl/lib/libSDL3.a"
for d in X11_bootstrap Wayland_bootstrap PIPEWIRE_bootstrap PULSEAUDIO_bootstrap ALSA_bootstrap; do
    grep -q " $d\$" <<<"$syms" || die "$sdl has no ${d%_bootstrap} driver: build the release's SDL with" \
        "scripts/setup.sh sdl3-desktop (it lists the -dev packages it needs)"
done
appimage="$(tool appimage)"
[[ -x "$appimage/appimagetool/AppRun" && -f "$appimage/runtime-x86_64" && -f "$appimage/runtime-LICENSE" ]] ||
    die "no appimagetool/runtime: scripts/setup.sh appimage"
log "dw2003 $version: SDL3 from $sdl"

# ---- The two programs, in Release. The game's -O3 build gives the same logs, records, SPU traces and checkpoint dumps
# as the tests' default build (tests/port/run.py; release.yml runs it on a Release build before packaging).
work="$ROOT/build/release"
mkdir -p "$work" "$out"
quiet() { # quiet LOG CMD...: the command's output into LOG, its end shown when it fails
    local logf="$1"; shift
    "$@" >>"$logf" 2>&1 || { tail -n 40 "$logf" >&2; die "failed: $* (the whole output: $logf)"; }
}
rm -f "$work/port-sdl.log" "$work/launcher.log"
log "building the game (SDL)"
quiet "$work/port-sdl.log" cmake -S "$ROOT/port" -B "$work/port-sdl" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DPSXSTACK_SDL=ON -DSDL3_DIR="$sdl/lib/cmake/SDL3" -DPSXSTACK_TOOLS_DIR="$ROOT/tools"
quiet "$work/port-sdl.log" cmake --build "$work/port-sdl" -j "$JOBS"
log "building the launcher"
quiet "$work/launcher.log" cmake -S "$ROOT/launcher" -B "$work/launcher" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSDL3_DIR="$sdl/lib/cmake/SDL3"
quiet "$work/launcher.log" cmake --build "$work/launcher" -j "$JOBS"

# ---- The AppDir.
appdir="$work/AppDir"
pkg="$ROOT/.ci-cd/linux/appimage"
rm -rf "$appdir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/scalable/apps" \
    "$appdir/LICENSES"
strip -o "$appdir/usr/bin/dw2003" "$work/port-sdl/dw2003"
# The game's debug info (the Release build keeps -g; port/CMakeLists.txt) as the release's .debug file: a crash report's
# exe+offset addresses resolve against it (scripts/symbolize.py --binary).
rm -f "$out/$debug"
objcopy --only-keep-debug "$work/port-sdl/dw2003" "$out/$debug"
strip -o "$appdir/usr/bin/dw2003-launcher" "$work/launcher/dw2003-launcher"
strip -o "$appdir/usr/bin/dw2003-updater" "$work/launcher/dw2003-updater" # the launcher runs a copy of it to update the AppImage
[[ -d "$work/port-sdl/mods" ]] || die "the game's build has no mods/ (psxstack copies its mods/ and port/mods there)"
cp -r "$work/port-sdl/mods" "$appdir/usr/bin/mods"
# A script, not a link: the launcher at its path inside the mount, for a double-click and RecompLauncher alike.
cp "$pkg/AppRun" "$appdir/AppRun"
chmod +x "$appdir/AppRun"
cp "$ROOT/recomplauncher.json" "$appdir/recomplauncher.json"
cp "$pkg/dw2003recomp.desktop" "$appdir/dw2003recomp.desktop"
cp "$pkg/dw2003recomp.desktop" "$appdir/usr/share/applications/"
cp "$pkg/dw2003recomp.svg" "$appdir/dw2003recomp.svg"
cp "$pkg/dw2003recomp.svg" "$appdir/usr/share/icons/hicolor/scalable/apps/"
cp "$ROOT/LICENSE" "$appdir/LICENSES/dw2003recomp.txt"
cp "$sdl/share/licenses/SDL3/LICENSE.txt" "$appdir/LICENSES/SDL3.txt"
cp "$ROOT/launcher/LICENSES/"*.txt "$appdir/LICENSES/"   # the launcher's font and stb_truetype
cp "$ROOT/port/third_party/cjson/LICENSE" "$appdir/LICENSES/cJSON.txt"   # the game's JSON (the expansion save)
cp "$appimage/runtime-LICENSE" "$appdir/LICENSES/appimage-runtime.txt"
cp "$pkg/LICENSES/"*.txt "$appdir/LICENSES/"

# ---- Only the C library's own libraries (SDL dlopens X11, Wayland, the audio servers, udev, ... at run time).
allowed='linux-vdso\.so\.1|/lib64/ld-linux-x86-64\.so\.2|ld-linux-x86-64\.so\.2|lib(c|m|dl|pthread|rt)\.so\.[0-9]+'
for bin in dw2003 dw2003-launcher dw2003-updater; do
    libs="$(ldd "$appdir/usr/bin/$bin" | awk '{print $1}')"
    bad="$(grep -vE "^($allowed)\$" <<<"$libs" || true)"
    [[ -z "$bad" ]] || die "$bin needs libraries a desktop may not have: $(echo $bad)"
    glibc="$(objdump -T "$appdir/usr/bin/$bin" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -n 1)"
    log "$bin needs: $(echo $libs) (newest glibc symbol: $glibc)"
done

# ---- The AppImage.
rm -f "$out/$name"
log "appimagetool -> $out/$name"
rm -f "$work/appimagetool.log"
quiet "$work/appimagetool.log" env ARCH=x86_64 "$appimage/appimagetool/AppRun" --no-appstream \
    --runtime-file "$appimage/runtime-x86_64" "$appdir" "$out/$name"
(cd "$out" && sha256sum "$name" "$debug" > SHA256SUMS)
log "$(head -n 1 "$out/SHA256SUMS") ($(du -h "$out/$name" | cut -f1))"
log "debug info: $out/$debug ($(du -h "$out/$debug" | cut -f1); scripts/symbolize.py REPORT --binary $debug)"

# ---- The smoke test: the launcher inside the AppImage finds the game and the mods beside itself.
if [[ $test -eq 1 ]]; then
    scratch="$work/self-test"
    rm -rf "$scratch"
    mkdir -p "$scratch"
    vars=(APPIMAGE_EXTRACT_AND_RUN=1 TMPDIR="$scratch" DW3_SELFTEST_GAME=beside)
    if [[ -f "$ROOT/iso/dw2003.cue" ]]; then
        vars+=(DW3_SELFTEST_DISC="$ROOT/iso/dw2003.cue")
        log "smoke test: the AppImage's self-test with the bundled game and the disc"
    else
        log "smoke test: the AppImage's self-test with the bundled game (no iso/dw2003.cue: no disc run)"
    fi
    env "${vars[@]}" "$out/$name" --self-test "$scratch" ||
        die "the AppImage's self-test failed"
    log "smoke test passed"
fi
