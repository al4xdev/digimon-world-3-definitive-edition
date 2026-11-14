#!/bin/bash
# build_android.sh: build the Android (arm64-v8a) APK using tools/Dockerfile.android (or host SDK/NDK if
# DW3_ANDROID_LOCAL=1). Outputs dist/dw2003-<version>-android-arm64-v8a.apk.
set -euo pipefail

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$TOOLS_DIR/.." && pwd)"
IMAGE="dw3-android-builder:latest"

if [ -n "${DW3_VERSION:-}" ]; then
    VER=${DW3_VERSION#v}
else
    stamp=$(git -C "$REPO" describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --always --dirty)
    case "$stamp" in
        v[0-9]*) VER=${stamp#v} ;;
        *) VER=dev-$stamp ;;
    esac
fi
export DW3_VERSION="$VER"

if [ "${DW3_ANDROID_LOCAL:-0}" = "1" ]; then
    echo "=== Building Android APK locally ($VER, arm64-v8a) ==="
    gradle -p "$REPO/android" --no-daemon assembleRelease
else
    if ! command -v docker >/dev/null 2>&1; then
        echo "Error: docker is required (or set DW3_ANDROID_LOCAL=1 with ANDROID_HOME/NDK/SDL2)." >&2
        exit 1
    fi
    if [ "${DW3_REBUILD_IMAGE:-0}" = "1" ] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
        echo "Building Android toolchain image $IMAGE..."
        docker build -t "$IMAGE" -f "$TOOLS_DIR/Dockerfile.android" "$TOOLS_DIR"
    fi
    echo "=== Building Android APK in Docker ($VER, arm64-v8a) ==="
    mkdir -p "$REPO/.dumps/gradle-home" "$REPO/.dumps/android-home"
    docker run --rm -u "$(id -u):$(id -g)" \
        -e DW3_VERSION \
        -e HOME=/work/.dumps/android-home \
        -e ANDROID_USER_HOME=/work/.dumps/android-home/.android \
        -e GRADLE_USER_HOME=/work/.dumps/gradle-home \
        -v "$REPO:/work" \
        -w /work/android \
        "$IMAGE" \
        gradle --no-daemon assembleRelease
fi

APK_SRC="$REPO/android/app/build/outputs/apk/release/app-release.apk"
[ -f "$APK_SRC" ] || { echo "Error: APK not produced at $APK_SRC" >&2; exit 1; }
mkdir -p "$REPO/dist"
APK_OUT="$REPO/dist/dw2003-${VER}-android-arm64-v8a.apk"
cp "$APK_SRC" "$APK_OUT"
(cd "$REPO/dist" && sha256sum "$(basename "$APK_OUT")" > SHA256SUMS-android)
echo "=== Android APK ready: $APK_OUT ==="
ls -lh "$APK_OUT"
