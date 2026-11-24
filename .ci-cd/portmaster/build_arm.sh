#!/bin/bash
# build_arm.sh: cross-compile this port for ARM handhelds (PortMaster multi-arch: armhf + aarch64) and assemble
# the package the card runs. The toolchains are Docker images built from .ci-cd/portmaster/Dockerfile.armhf and
# .ci-cd/portmaster/Dockerfile.aarch64 (Debian 10 buster, glibc 2.28 floor), so nothing host-wide is installed.
#
#   .ci-cd/portmaster/build_arm.sh                 build both armhf + aarch64 and package into ../portmaster
#   DW3_ARCH=armhf .ci-cd/portmaster/build_arm.sh  build only armhf (or aarch64)
#   DW3_PACKAGE_DIR=... .ci-cd/portmaster/build_arm.sh   assemble the package somewhere else
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # .ci-cd/portmaster/: the Dockerfiles and the toolchains
REPO="$(cd "$HERE/../.." && pwd)"
PACKAGE_DIR="${DW3_PACKAGE_DIR:-$REPO/../portmaster}"
TARGET_ARCH="${DW3_ARCH:-all}"

echo "=== Building Digimon World 2003 for PortMaster ($TARGET_ARCH) ==="

if ! command -v docker &> /dev/null; then
    echo "Error: docker is required for the cross-compilation toolchain." >&2
    exit 1
fi

# The image has no git. Prefer DW3_VERSION (for tagged releases) over git describe.
if [ -n "${DW3_VERSION:-}" ]; then
    PSXSTACK_VERSION=${DW3_VERSION#v}
else
    stamp=$(git -C "$REPO" describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --always --dirty)
    case "$stamp" in
        v[0-9]*) PSXSTACK_VERSION=${stamp#v} ;;
        *) PSXSTACK_VERSION=dev-$stamp ;;
    esac
fi
PSXSTACK_COMMIT=$(git -C "$REPO" rev-parse HEAD)
export PSXSTACK_VERSION PSXSTACK_COMMIT
echo "Version stamp: $PSXSTACK_VERSION ($PSXSTACK_COMMIT)"

build_one_arch() {
    local arch="$1"
    local image="dw3-${arch}-builder:latest"
    local dockerfile="$HERE/Dockerfile.${arch}"
    local build_sub="build/${arch}-sdl2"
    local toolchain="/work/.ci-cd/portmaster/cmake/${arch}-linux.cmake"
    local cc
    if [ "$arch" = "armhf" ]; then
        cc="arm-linux-gnueabihf-gcc"
    else
        cc="aarch64-linux-gnu-gcc"
    fi

    if [ "${DW3_REBUILD_IMAGE:-0}" = "1" ] || ! docker image inspect "$image" &> /dev/null; then
        echo "Building toolchain image $image from $dockerfile..."
        docker build -t "$image" -f "$dockerfile" "$HERE"
    fi

    echo "Configuring and compiling ($arch) with CMake + Ninja inside Docker..."
    docker run --rm -u "$(id -u):$(id -g)" \
        -e PSXSTACK_VERSION -e PSXSTACK_COMMIT \
        -v "$REPO:/work" \
        -w /work \
        "$image" \
        bash -c "
            cmake -S port -B ${build_sub} -G Ninja \
                -DCMAKE_TOOLCHAIN_FILE=${toolchain} \
                -DPSXSTACK_PYTHON=/usr/bin/python3 \
                -DPSXSTACK_SDL2=ON -DPSXSTACK_EGL=ON && \
            ninja -C ${build_sub} -j\$(nproc)
        "

    # PortMaster launcher (C99): compiled with -DDW3_TARGET_PORTMASTER and -DHAVE_SDL2 so fbdev is used by default
    # and SDL2 (Wayland/KMSDRM/portrait panels) works as a seamless fallback on ROCKNIX/Knulli/muOS/480x320.
    docker run --rm -u "$(id -u):$(id -g)" -v "$REPO:/work" -w /work "$image" bash -c "
        ${cc} -std=gnu99 -D_GNU_SOURCE -O2 -s -DDW3_TARGET_PORTMASTER -DHAVE_SDL2 \
            -I/usr/include/SDL2 launcher/main.c launcher/ui.c launcher/plat.c launcher/cfg.c launcher/migrate.c \
            launcher/loading.c launcher/cloud.c launcher/sha256.c launcher/assets/assets.c \
            -o ${build_sub}/dw3-launcher -lSDL2 -lm
    "
    echo "Build complete ($arch): $REPO/${build_sub}/dw2003 and dw3-launcher"
}

case "$TARGET_ARCH" in
    all)
        build_one_arch armhf
        build_one_arch aarch64
        ;;
    armhf|aarch64)
        build_one_arch "$TARGET_ARCH"
        ;;
    *)
        echo "Error: DW3_ARCH must be 'all', 'armhf', or 'aarch64' (got '$TARGET_ARCH')" >&2
        exit 1
        ;;
esac

if [ "${DW3_BUILD_ONLY:-0}" = "1" ]; then
    exit 0
fi

# The package is what the card holds: multi-arch binaries plus everything in .ci-cd/portmaster/handheld/ (no symlinks for exFAT/FAT32).
echo "Assembling the package in $PACKAGE_DIR..."
mkdir -p "$PACKAGE_DIR/dw3/game" "$PACKAGE_DIR/dw3/mods"

for arch in armhf aarch64; do
    bdir="$REPO/build/${arch}-sdl2"
    if [ -f "$bdir/dw2003" ] && [ -f "$bdir/dw3-launcher" ]; then
        cp "$bdir/dw2003" "$PACKAGE_DIR/dw3/dw2003.${arch}"
        cp "$bdir/dw3-launcher" "$PACKAGE_DIR/dw3/dw3-launcher.${arch}"
        chmod +x "$PACKAGE_DIR/dw3/dw2003.${arch}" "$PACKAGE_DIR/dw3/dw3-launcher.${arch}"
    fi
done

# Retired files: the package is a mirror, so a file that left .ci-cd/portmaster/handheld/ has to leave it too (the copy loop only adds).
rm -f "$PACKAGE_DIR/dw3/dw3-tui.sh" "$PACKAGE_DIR/dw3/dw3.sh" "$PACKAGE_DIR/dw3/detect-env.sh" "$PACKAGE_DIR/dw3/dw3-menu.py"

cp "$REPO/.ci-cd/portmaster/handheld/Digimon World 3.sh" "$PACKAGE_DIR/Digimon World 3.sh"
chmod +x "$PACKAGE_DIR/Digimon World 3.sh"

for f in "$REPO"/.ci-cd/portmaster/handheld/*; do
    case "$(basename "$f")" in
        README.md|"Digimon World 3.sh"|__pycache__) continue ;;
    esac
    cp "$f" "$PACKAGE_DIR/dw3/"
done
chmod -x "$PACKAGE_DIR/dw3/README.txt" 2>/dev/null || true
chmod +x "$PACKAGE_DIR/dw3/ledd.py" 2>/dev/null || true

mods_src=""
if [ -d "$REPO/build/armhf-sdl2/mods" ]; then
    mods_src="$REPO/build/armhf-sdl2/mods"
elif [ -d "$REPO/build/aarch64-sdl2/mods" ]; then
    mods_src="$REPO/build/aarch64-sdl2/mods"
fi
if [ -n "$mods_src" ]; then
    cp -r "$mods_src/"* "$PACKAGE_DIR/dw3/mods/" || true
fi

echo "=== Done ==="
echo "  package: $PACKAGE_DIR"
