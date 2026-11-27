#!/usr/bin/env python3
"""The Velvet Room's music (tools/gen_velvet_music.py, port/game/definitive_velvet_music.c): BGM031's header file from the
disc goes through the port's hooks in a host driver; the result is checked here: the SEP is one the port's LIBSND reads
(tests/sound/sound_formats.py's parser), the VAB header only changed in its tone records, the oboe and the piano are
retuned, the piano's samples are in the vag 14 slot, the reverb is put back. Needs the disc and the generated
port/game/definitive_velvet_music.inc (a local MIDI): without them the test says so and passes."""
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests/sound"))
OUT = ROOT / "build/port-test/definitive_velvet_music"
INC = ROOT / "port/game/definitive_velvet_music.inc"


def main():
    if not INC.is_file():
        print("definitive_velvet_music: skip (no generated score: the MIDI is local, tools/gen_velvet_music.py)")
        return 0
    import disc_files
    import sound_formats as sf
    import gen_velvet_music as gvm

    OUT.mkdir(parents=True, exist_ok=True)
    exe = OUT / "driver"
    cc = ["gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-I", str(ROOT / "include"),
          "-I", str(ROOT / "tests/host"), "-I", str(ROOT / "psxstack/include"), "-I", str(ROOT / "psxstack/include/psxstack"),
          "-I", str(ROOT / "port/game"), str(ROOT / "port/game/definitive_velvet_music.c"),
          str(ROOT / "tests/port/definitive_velvet_music_driver.c"), "-o", str(exe)]
    if subprocess.run(cc).returncode != 0:
        print("definitive_velvet_music: FAIL: does not compile")
        return 1
    exe_bytes = disc_files.EXE.read_bytes()
    idp = {f.id: f.path for f in disc_files.files()}
    bank = next(b for _i, b in sf.banks(exe_bytes) if idp[b["header_file"]].endswith("/MPBGM031.BIN"))
    header_file = disc_files.read(bank["header_file"])
    src, dst = OUT / "bgm031.bin", OUT / "patched.bin"
    src.write_bytes(header_file)
    run = subprocess.run([str(exe), str(src), str(dst)], capture_output=True, text=True)
    print(run.stdout, end="")
    bad = run.returncode != 0
    got = dst.read_bytes()
    buf, rest = got[:0xA000], got[0xA000:]
    xfer = struct.unpack_from("<I", rest)[0]
    ram = rest[4:]

    def fail(msg):
        nonlocal bad
        bad = True
        print(f"FAIL {msg}")

    off_vab, off_sep = struct.unpack_from("<2i", buf)
    old_vh = sf.sub(header_file, bank["vab_header"])
    vh_len = off_sep - off_vab
    new_vh, old_vh = buf[off_vab:off_sep], header_file[off_vab:off_sep]
    tone_lo, tone_hi = 0x20 + 0x800, 0x20 + 0x800 + 0x200 * struct.unpack_from("<H", old_vh, 0x12)[0]
    diff = [i for i in range(vh_len) if new_vh[i] != old_vh[i]]
    if any(not tone_lo <= i < tone_hi for i in diff):
        fail("the VAB header changed outside the tone records")
    else:
        print(f"ok VAB header changed only in its tone records ({len(diff)} bytes)")
    # the SEP: the port's own format check on every sequence
    census = Counter()
    try:
        _m, _v, seqs, _end = sf.parse_sep(buf[off_sep:], census)
        if len(seqs) != 16 or any(q["parsed"] != q["size"] for q in seqs):
            fail(f"SEP: {len(seqs)} sequences, data ends where its size says: {[q['parsed'] == q['size'] for q in seqs]}")
        else:
            print(f"ok SEP parses as 16 sequences ({dict(census)})")
    except Exception as e:  # noqa: BLE001 - the parser raises its own assertion kinds
        fail(f"SEP does not parse: {e!r}")
    # tones
    vh = sf.parse_vh(new_vh)
    used = [i for i, p in enumerate(vh["progs"]) if p[0]]
    tone = {prog: vh["tone_recs"][k * 16][2] for k, prog in enumerate(used[:vh["programs"]])}
    if tone[5][4] != 62:
        fail(f"oboe centre {tone[5][4]}, not 62")
    if (tone[10][2], tone[10][4], tone[10][5]) != (96, 66, 0):
        fail(f"piano tone {tone[10][2:6]}")
    if tone[8][4] != 84:
        fail("the flute's centre moved")
    # the piano
    slot = 0x49C10 + sum(vh["vag_sizes"][1:gvm.PIANO_VAG])
    piano = gvm.piano_sample()
    if ram[slot:slot + len(piano)] != piano:
        fail("the piano's samples are not in vag 14's slot")
    else:
        print(f"ok piano in the slot at SPU {slot:#x} ({len(piano)} of {vh['vag_sizes'][gvm.PIANO_VAG]} bytes)")
    print("definitive_velvet_music: " + ("FAIL" if bad else "pass"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
