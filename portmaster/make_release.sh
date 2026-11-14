#!/bin/bash
# make_release.sh: assemble the multi-arch (armhf + aarch64) PortMaster release of this port, in the layout
# https://portmaster.games/packaging.html asks for, and check it before it is offered:
#
#   dist/dw3.zip
#     Digimon World 3.sh        the launcher (handheld/, versioned with the port)
#     port.json                 the metadata (this directory)
#     README.md                 the wiki page's notes (this directory)
#     gameinfo.xml              EmulationStation's entry (this directory)
#     screenshot.png            640x480, gameplay (this directory; capture it on the device)
#     cover.png                 the box art (this directory; optional)
#     dw3/                      the port: dw2003.{armhf,aarch64}, dw3-launcher.{armhf,aarch64} and handheld/ extras
#       licenses/               what the port borrows and its licences (portmaster/licenses/)
#
# It refuses to build when one of those is missing, when the port directory's name differs from the port's, or when
# the binaries are not valid 32-bit ARM / 64-bit aarch64 ELFs. Nothing of the game's data is ever included.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
NAME="dw3"                             # the port's name: dist/<name>.zip, ports/<name>/, ports/<name>/<name>/
SCRIPT="Digimon World 3.sh"
BIN_ARMHF="$REPO/build/armhf-sdl2/dw2003"
LAUNCHER_ARMHF="$REPO/build/armhf-sdl2/dw3-launcher"
BIN_AARCH64="$REPO/build/aarch64-sdl2/dw2003"
LAUNCHER_AARCH64="$REPO/build/aarch64-sdl2/dw3-launcher"
DIST="$REPO/dist"
OUT="$DIST/$NAME"

fail() { echo "make_release: $*" >&2; exit 1; }

[ -f "$BIN_ARMHF" ] || fail "armhf binary not built: $BIN_ARMHF (run tools/build_arm.sh)"
[ -x "$LAUNCHER_ARMHF" ] || fail "armhf launcher not built: $LAUNCHER_ARMHF (run tools/build_arm.sh)"
[ -f "$BIN_AARCH64" ] || fail "aarch64 binary not built: $BIN_AARCH64 (run tools/build_arm.sh)"
[ -x "$LAUNCHER_AARCH64" ] || fail "aarch64 launcher not built: $LAUNCHER_AARCH64 (run tools/build_arm.sh)"

file "$BIN_ARMHF" | grep -q "ELF 32-bit LSB.*ARM" || fail "$BIN_ARMHF is not a 32-bit ARM ELF"
file "$LAUNCHER_ARMHF" | grep -q "ELF 32-bit LSB.*ARM" || fail "$LAUNCHER_ARMHF is not a 32-bit ARM ELF"
file "$BIN_AARCH64" | grep -q "ELF 64-bit LSB.*ARM aarch64" || fail "$BIN_AARCH64 is not a 64-bit aarch64 ELF"
file "$LAUNCHER_AARCH64" | grep -q "ELF 64-bit LSB.*ARM aarch64" || fail "$LAUNCHER_AARCH64 is not a 64-bit aarch64 ELF"

for f in port.json README.md gameinfo.xml screenshot.png; do
    [ -f "$HERE/$f" ] || fail "missing $HERE/$f (the screenshot is captured on the device, 640x480 of gameplay)"
done
[ -f "$REPO/handheld/$SCRIPT" ] || fail "missing $REPO/handheld/$SCRIPT"

echo "=== $NAME: assembling $OUT (armhf + aarch64) ==="
rm -rf "$OUT"
mkdir -p "$OUT/$NAME/licenses"
cp "$HERE/port.json" "$HERE/README.md" "$HERE/gameinfo.xml" "$HERE/screenshot.png" "$OUT/"
[ -f "$HERE/cover.png" ] && cp "$HERE/cover.png" "$OUT/"
cp "$REPO/handheld/$SCRIPT" "$OUT/$SCRIPT"
chmod +x "$OUT/$SCRIPT"

for f in "$REPO"/handheld/*; do
    case "$(basename "$f")" in
        README.md|"$SCRIPT"|__pycache__) continue ;;
    esac
    cp "$f" "$OUT/$NAME/"
done
chmod -x "$OUT/$NAME/README.txt" 2>/dev/null || true
[ -f "$OUT/$NAME/ledd.py" ] && chmod +x "$OUT/$NAME/ledd.py"
[ -f "$OUT/$NAME/probe.sh" ] && chmod +x "$OUT/$NAME/probe.sh"

cp "$BIN_ARMHF" "$OUT/$NAME/dw2003.armhf"
cp "$LAUNCHER_ARMHF" "$OUT/$NAME/dw3-launcher.armhf"
cp "$BIN_AARCH64" "$OUT/$NAME/dw2003.aarch64"
cp "$LAUNCHER_AARCH64" "$OUT/$NAME/dw3-launcher.aarch64"
chmod +x "$OUT/$NAME/dw2003.armhf" "$OUT/$NAME/dw3-launcher.armhf" \
         "$OUT/$NAME/dw2003.aarch64" "$OUT/$NAME/dw3-launcher.aarch64"

mods_src=""
if [ -d "$REPO/build/armhf-sdl2/mods" ]; then
    mods_src="$REPO/build/armhf-sdl2/mods"
elif [ -d "$REPO/build/aarch64-sdl2/mods" ]; then
    mods_src="$REPO/build/aarch64-sdl2/mods"
fi
if [ -n "$mods_src" ]; then
    mkdir -p "$OUT/$NAME/mods"
    cp -r "$mods_src/"* "$OUT/$NAME/mods/"
fi

cp "$HERE"/licenses/* "$OUT/$NAME/licenses/"

cd "$DIST"
rm -f "$NAME.zip"
python3 - "$NAME" <<'PY'
import os
import sys
import zipfile

name = sys.argv[1]
with zipfile.ZipFile(name + ".zip", "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, _dirs, files in os.walk(name):
        for f in sorted(files):
            path = os.path.join(root, f)
            arcname = os.path.relpath(path, name)
            info = zipfile.ZipInfo.from_file(path, arcname)
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as src:
                z.writestr(info, src.read())
print("zip ok:", name + ".zip")
PY

echo "=== checks"
python3 - "$DIST/$NAME.zip" <<'PY'
import json
import sys
import zipfile

z = zipfile.ZipFile(sys.argv[1])
names = z.namelist()
with z.open("port.json") as f:
    port = json.load(f)
missing = [n for n in ("Digimon World 3.sh", "port.json", "README.md", "gameinfo.xml",
                       "screenshot.png", "dw3/dw2003.armhf", "dw3/dw3-launcher.armhf",
                       "dw3/dw2003.aarch64", "dw3/dw3-launcher.aarch64")
           if n not in names]
if missing:
    sys.exit("missing from the zip: " + ", ".join(missing))
if port["name"] != "dw3.zip" or port["items"] != ["Digimon World 3.sh", "dw3"]:
    sys.exit("port.json does not describe this layout: %r / %r" % (port["name"], port["items"]))
if set(port["attr"]["arch"]) != {"armhf", "aarch64"}:
    sys.exit("port.json arch does not match multi-arch build: %r" % (port["attr"]["arch"],))
banned = [n for n in names if n.lower().endswith((".bin", ".cue", ".iso", ".chd", ".mcd", ".img"))]
if banned:
    sys.exit("the package carries the game's data: " + ", ".join(banned))
print("zip ok: %d files, port.json %s, arch %s" % (len(names), port["name"], port["attr"]["arch"]))
PY

sha256sum "$NAME.zip" > SHA256SUMS-portmaster
md5sum "$DIST/$NAME.zip"
du -h "$DIST/$NAME.zip"
echo "=== $NAME ready: publish it as ports/$NAME/ in PortMaster-MV-New (a decomp port) — see portmaster/PUBLISHING.md"
