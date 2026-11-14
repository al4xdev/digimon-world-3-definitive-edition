#!/usr/bin/env bash
# Which parts of CI a change needs (.github/workflows/ci.yml; DECISIONS "CI per area"). Prints game=, port=,
# launcher=, and portmaster= (true/false) for $GITHUB_OUTPUT, and each file's area on stderr.
#
# Usage: scripts/ci_areas.sh FILE...            the areas of these paths (as `git diff --name-only` prints them)
#        scripts/ci_areas.sh --diff BASE [HEAD]  the areas of what changed from BASE to HEAD (default HEAD)
#        scripts/ci_areas.sh --all               every area (manual runs, release tags, unknown history)
#
# The areas and the jobs each runs (ci.yml has the steps; the jobs run in parallel, each gated by its area):
#   game        `check`'s workaround census, `game`: toolchain smoke test, byte-identical rebuild, layer 2
#   port        `check`'s probes, `port` (layer 1 + port), `port-mods`, `windows`, `portmaster`, and `android`
#   launcher    `launcher` (SDL3 game + psxstack/launcher desktop build & self-test) and `windows`
#   portmaster  `portmaster` (armhf + aarch64 Docker cross-build, dw3.zip packaging & checks)
#   android     `android` (arm64-v8a Android APK build & verification)
# Each implies the downstream areas it affects: game => port => launcher + portmaster + android.
set -euo pipefail

# The area of one path: all, game, port, launcher, portmaster, android, or none (no CI step reads it).
area_of() {
    case "$1" in
        # CI itself (the workflow, its jobs' setup action), and this mapping: everything.
        .github/workflows/ci.yml|.github/actions/*|scripts/ci_areas.sh) echo all ;;
        # Documentation (ci.yml's paths-ignore too): no step reads it.
        docs/*|*.md|LICENSE|.gitignore|.github/ISSUE_TEMPLATE/*) echo none ;;
        # Android-specific Gradle/JNI/touch-pad build.
        android/*|tools/build_android.sh|tools/Dockerfile.android) echo android ;;
        # Standalone C launcher affects both PortMaster and Android touch launcher.
        launcher/*) echo handheld ;;
        # PortMaster-specific packaging, handheld scripts, and armhf/aarch64 toolchain.
        handheld/*|portmaster/*|tools/build_arm.sh|tools/Dockerfile.armhf|tools/Dockerfile.aarch64|cmake/armhf-linux.cmake|cmake/aarch64-linux.cmake) echo portmaster ;;
        # The release's own files: release.yml tests them (a manual run); here only `bash -n`, which always runs.
        .github/workflows/release.yml|scripts/package_appimage.sh|scripts/check_release_contents.py|packaging/*) echo none ;;
        # The port: its game side (port/, psxstack, port_inputs, port tests) => port + launcher + portmaster + android.
        port/*|.gitmodules|psxstack|tools/port_inputs.py|tools/port_inventory.py|tools/mcp_game.py|.mcp.json|tests/port/*|tests/spu/*|tests/xa/*|tests/saves/*|tests/host/*) echo port ;;
        # Everything else: the game's C and headers, config/, configure.py, tools/, the other tests, scripts/.
        *) echo game ;;
    esac
}

game=false port=false launcher=false portmaster=false android=false
mark() {
    case "$1" in
        all|game) game=true; port=true; launcher=true; portmaster=true; android=true ;;
        port) port=true; launcher=true; portmaster=true; android=true ;;
        launcher) launcher=true ;;
        handheld) portmaster=true; android=true ;;
        portmaster) portmaster=true ;;
        android) android=true ;;
    esac
}

files=()
case "${1:-}" in
    --all) mark all; echo "ci_areas: every area" >&2 ;;
    --diff)
        [[ $# -ge 2 ]] || { echo "usage: $0 --diff BASE [HEAD]" >&2; exit 2; }
        mapfile -t files < <(git diff --name-only "$2" "${3:-HEAD}")
        echo "ci_areas: ${#files[@]} file(s) changed from $2 to ${3:-HEAD}" >&2 ;;
    -h|--help) sed -n '2,23p' "$0"; exit 0 ;;
    *) files=("$@") ;;
esac
for f in "${files[@]}"; do
    a="$(area_of "$f")"
    printf '  %-10s %s\n' "$a" "$f" >&2
    mark "$a"
done
echo "game=$game"
echo "port=$port"
echo "launcher=$launcher"
echo "portmaster=$portmaster"
echo "android=$android"
