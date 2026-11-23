#!/usr/bin/env python3
"""The player's MIDI as battle music (port/game/definitive_midi.c, definitive_midi_screen.c, Etemon's jukebox): the module
compiled alone against a driver with the folder in a temp directory. Checks the list (only playable files, sorted), the
choice file (written, read back, a path out of the folder refused), the battle music (the MIDI bank's key, the game's own
when nothing is chosen or the file is gone), a changed file making the loaded bank be forgotten, and the SEP the bank hook
writes: BGM031's real header file from the disc, the SEP parsed by the port's own LIBSND reader (tests/sound/), 16
sequences, every note-on closed, drum notes on the four drum tones, a file too long cut to fit. The disc is only needed for
the hook part (the test says so and passes that part without it). Also: the generated strings exist in every language."""
import os
import random
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests/sound"))
OUT = ROOT / "build/port-test/definitive_midi"


def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(n & 0x7F | 0x80)
        n >>= 7
    return bytes(reversed(out))


def track(events):
    """events: [(tick, bytes)] -> MTrk chunk."""
    data, prev = bytearray(), 0
    for tick, raw in sorted(events, key=lambda e: e[0]):
        data += vlq(tick - prev) + raw
        prev = tick
    data += b"\x00\xff\x2f\x00"
    return b"MTrk" + struct.pack(">I", len(data)) + bytes(data)


def midi(tracks, division=480, fmt=1):
    return b"MThd" + struct.pack(">IHHH", 6, fmt, len(tracks), division) + b"".join(tracks)


def song(notes_per_track=40, tracks=2, with_drums=True):
    """A small format-1 piece: a tempo track, melodic tracks on channels 0..n-1 with program changes (GM 0, 40, 56,
    73...), a drum track on channel 9 with notes inside and outside the four tones the bank has."""
    tr = [track([(0, b"\xff\x51\x03" + (500000).to_bytes(3, "big")), (960, b"\xff\x51\x03" + (400000).to_bytes(3, "big"))])]
    rnd = random.Random(7)
    for t in range(tracks):
        ev = [(0, bytes((0xC0 | t, (0, 40, 56, 73)[t % 4]))), (0, bytes((0xB0 | t, 7, 90)))]
        for k in range(notes_per_track):
            tick = k * 240
            note = rnd.randrange(40, 90)
            ev += [(tick, bytes((0x90 | t, note, 80))), (tick + 200, bytes((0x80 | t, note, 0)))]
        tr.append(track(ev))
    if with_drums:
        ev = []
        for k in range(notes_per_track):
            for note in (36, 38, 42, 49, 56, 70):
                ev += [(k * 240, bytes((0x99, note, 100))), (k * 240 + 100, bytes((0x99, note, 0)))]
        tr.append(track(ev))
    return midi(tr)


def decode(buf, off_sep):
    """Sequence 0 of the SEP at buf[off_sep:], decoded the way LIBSND reads it (running status, delta before an event,
    tempo with no length byte): [(tick, status, data...)] and the sequence's declared size."""
    assert buf[off_sep:off_sep + 4] == b"pQES"
    div, = struct.unpack_from(">H", buf, off_sep + 8)
    size, = struct.unpack_from(">I", buf, off_sep + 6 + 9)
    o, end, tick, status, ev = off_sep + 6 + 13, off_sep + 6 + 13 + size, 0, 0, []
    while o < end:
        d = 0
        while True:
            b = buf[o]
            o += 1
            d = d << 7 | b & 0x7F
            if not b & 0x80:
                break
        tick += d
        if buf[o] & 0x80:
            status = buf[o]
            o += 1
        if status == 0xFF:
            kind = buf[o]
            o += 1
            if kind == 0x2F:
                ev.append((tick, "end"))
                break
            ev.append((tick, "tempo", int.from_bytes(buf[o:o + 3], "big")))
            o += 3
            status = 0
            continue
        n = 1 if status & 0xF0 == 0xC0 else 2
        ev.append((tick, status, *buf[o:o + n]))
        o += n
    return ev, div, size


def main():
    if OUT.exists():
        shutil.move(str(OUT), f"/tmp/definitive_midi_old_{os.getpid()}")
    folder = OUT / "midi"
    folder.mkdir(parents=True)
    (folder / "b_song.mid").write_bytes(song())
    (folder / "A_other.MIDI").write_bytes(song(notes_per_track=10, tracks=1, with_drums=False))
    (folder / "huge.mid").write_bytes(song(notes_per_track=6000, tracks=3))
    (folder / "notmidi.mid").write_bytes(b"this is not a midi file")
    (folder / "smpte.mid").write_bytes(midi([track([(0, b"\x90\x40\x40"), (10, b"\x90\x40\x00")])], division=0xE728))
    (folder / "empty.mid").write_bytes(midi([track([(0, b"\xff\x51\x03\x07\xa1\x20")])]))
    (folder / "readme.txt").write_text("not a midi")
    exe = OUT / "driver"
    cc = ["gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include"), "-I", str(ROOT / "tests/host"),
          "-I", str(ROOT / "psxstack/include"), "-I", str(ROOT / "psxstack/include/psxstack"), "-I", str(ROOT / "port/game"),
          str(ROOT / "port/game/definitive_midi.c"), str(ROOT / "tests/port/definitive_midi_driver.c"), "-o", str(exe)]
    if subprocess.run(cc).returncode != 0:
        print("definitive_midi: FAIL: does not compile")
        return 1
    header = None
    try:
        import disc_files
        import sound_formats as sf
        exe_bytes = disc_files.EXE.read_bytes()
        idp = {f.id: f.path for f in disc_files.files()}
        bank = next(b for _i, b in sf.banks(exe_bytes) if idp[b["header_file"]].endswith("/MPBGM031.BIN"))
        header = disc_files.read(bank["header_file"])
    except (FileNotFoundError, StopIteration, OSError, ImportError) as e:
        print(f"definitive_midi: no disc ({e}): the bank hook is not checked")
    src, dst = OUT / "bgm031.bin", OUT / "patched.bin"
    src.write_bytes(header if header is not None else b"\0" * 0x6000)
    env = dict(os.environ, DW3_MIDI_DIR=str(folder))
    # the choice file first: a path out of the folder must not be taken
    (folder / "battle_music.txt").write_text("../b_song.mid\n")
    p = subprocess.Popen([str(exe), str(folder), str(src), str(dst)], env=env, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, text=True)
    lines = []
    bad = False

    def wait_step():
        while True:
            line = p.stdout.readline()
            if not line:
                return None
            lines.append(line.rstrip())
            print(line, end="")
            if line.startswith("STEP"):
                return line.split()[1]

    step = wait_step()
    while step is not None:
        if step == "change":
            (folder / "A_other.MIDI").write_bytes(song(notes_per_track=11, tracks=1, with_drums=False))
            # the choice is the first file in the sorted list: "A_other.MIDI" (case-insensitive order)
        elif step == "delete":
            (folder / "A_other.MIDI").rename(OUT / "A_other.gone")
        p.stdin.write("go\n")
        p.stdin.flush()
        step = wait_step()
    rest = p.stdout.read()
    print(rest, end="")
    bad |= p.wait() != 0
    names = [ln.split("=", 1)[1].split(" cut=")[0] for ln in lines if ln.startswith("file=")]
    cuts = {ln.split("=", 1)[1].split(" cut=")[0]: ln.endswith("cut=1") for ln in lines if ln.startswith("file=")}

    def fail(msg):
        nonlocal bad
        bad = True
        print(f"FAIL {msg}")

    if names != ["A_other.MIDI", "b_song.mid", "huge.mid"]:
        fail(f"the list is {names}: only the playable files, sorted without regard to case")
    else:
        print("ok the list: playable files only, sorted")
    if not (cuts.get("huge.mid") and not cuts.get("b_song.mid")):
        fail(f"only the huge piece is cut: {cuts}")
    else:
        print("ok the huge piece is cut to fit, the small ones are not")
    # the refused path was never a choice; the real choice file after the run is the game's own music
    if (folder / "battle_music.txt").read_text().strip() != "mode: own":
        fail("the choice file does not say `mode: own` after choosing the game's own music")
    if header is not None:
        got = dst.read_bytes()
        off_vab, off_sep = struct.unpack_from("<2i", got)
        import sound_formats as sf
        from collections import Counter
        census = Counter()
        _m, _v, seqs, _end = sf.parse_sep(got[off_sep:], census)
        if len(seqs) != 16 or any(q["parsed"] != q["size"] for q in seqs):
            fail(f"SEP: {len(seqs)} sequences, parsed == size: {[q['parsed'] == q['size'] for q in seqs]}")
        else:
            print(f"ok the SEP parses as 16 sequences ({dict(census)})")
        old = header[off_vab:off_sep]
        new = got[off_vab:off_sep]
        vh_old = sf.parse_vh(old)
        diff = [i for i in range(len(old)) if old[i] != new[i]]
        lo, hi = 0x20 + 0x800, 0x20 + 0x800 + 0x200 * struct.unpack_from("<H", old, 0x12)[0]
        if any(not lo <= i < hi for i in diff):
            fail(f"the VAB header changed outside the tone records: {len(diff)} bytes")
        else:
            print(f"ok the VAB header changed only in its tone records ({len(diff)} bytes)")
        vh_new = sf.parse_vh(new)
        used = [i for i, p in enumerate(vh_old["progs"]) if p[0]]

        def rec(vh, prog):
            return vh["tone_recs"][used.index(prog) * 16][2]
        for prog in (1, 2, 3, 4, 5, 6, 8):
            o, n = rec(vh_old, prog), rec(vh_new, prog)
            want1 = (o[16] & 0x00FF) | 9 << 10 | 3 << 8
            want2 = (o[17] & 0xFFC0) | 12
            if (n[16], n[17], n[1]) != (want1, want2, 4) or (prog == 5 and n[4] != 62):
                fail(f"program {prog}: envelope/reverb/centre not the shared setting: {n[1]} {n[16]:#x} {n[17]:#x}")
        for prog in (0, 10):
            if rec(vh_old, prog) != rec(vh_new, prog):
                fail(f"program {prog} (drums, piano) changed")
        print("ok melodic tones: shared attack/release and the reverb flag, oboe centred; drums and piano untouched")
        sep_end = off_sep + len(got[off_sep:].rstrip(b"\0"))
        if sep_end > 0xA000:
            fail("the SEP runs past the buffer")
    if header is not None:
        for idx, name in ((1, "b_song.mid"), (2, "huge.mid")):
            (folder / "battle_music.txt").write_text("")
            if not (folder / "A_other.MIDI").exists():
                shutil.move(str(OUT / "A_other.gone"), str(folder / "A_other.MIDI"))
            run = subprocess.run([str(exe), str(folder), str(src), str(dst), str(idx)], env=env, capture_output=True, text=True)
            if run.returncode != 0:
                fail(f"{name}: the driver failed:\n{run.stdout}")
                continue
            got = dst.read_bytes()
            off_sep = struct.unpack_from("<2i", got)[1]
            ev, div, size = decode(got, off_sep)
            ons = [e for e in ev if len(e) == 4 and e[1] & 0xF0 == 0x90 and e[3] > 0]
            offs = [e for e in ev if len(e) == 4 and e[1] & 0xF0 == 0x90 and e[3] == 0]
            drums = {e[2] for e in ons if e[1] & 0xF == 9}
            progs = {(e[1] & 0xF, e[2]) for e in ev if len(e) == 3 and isinstance(e[1], int) and e[1] & 0xF0 == 0xC0}
            tempos = [e[2] for e in ev if e[1:2] == ("tempo",)]
            print(f"{name}: {len(ons)} notes, {size} bytes, drums {sorted(drums)}, programs {sorted(progs)}, tempos {tempos}")
            if len(ons) != len(offs):
                fail(f"{name}: {len(ons)} note-ons and {len(offs)} note-offs: a note is left sounding")
            if not drums <= {36, 38, 44, 49}:
                fail(f"{name}: drum notes {sorted(drums)} are not the bank's tones")
            if div != 480 or tempos[:1] != [500000]:
                fail(f"{name}: division {div}, tempos {tempos}")
            if off_sep + 6 + 13 + size > 0xA000:
                fail(f"{name}: the SEP is past the buffer")
            if name == "b_song.mid":
                if (len(ons), sorted(drums), {(0, 1), (1, 4), (9, 0)} - progs) != (40 * 2 + 40 * 4, [36, 38, 44, 49], set()):
                    fail(f"{name}: the converted notes, drums or programs are wrong")
                if tempos != [500000, 400000]:
                    fail(f"{name}: the tempo map {tempos}")
            if name == "huge.mid" and (2, 2) not in progs:
                fail(f"{name}: GM 56 (trumpet) should be program 2, got {sorted(progs)}")
            if name == "huge.mid" and not (0 < len(ons) < 3 * 6000 + 6000 * 4 and size > 25000):
                fail(f"{name}: expected a cut piece that still fills the room, got {len(ons)} notes, {size} bytes")
        if not bad:
            print("ok the converted notes, drums, programs and tempo map; every note is closed; the long piece is cut")
    # the playlists: "all, in order" and "all, shuffled" over the folder (empty.mid, notmidi.mid and smpte.mid do not convert
    # and are skipped; the three that do are A_other.MIDI, b_song.mid, huge.mid)
    if not (folder / "A_other.MIDI").exists() and (OUT / "A_other.gone").exists():
        shutil.move(str(OUT / "A_other.gone"), str(folder / "A_other.MIDI"))

    def playlist(row, count, seed=1, where=folder):
        out = subprocess.run([str(exe), str(where), str(src), str(dst), "p", str(row), str(count)],
                             env=dict(env, DW3_MIDI_DIR=str(where), DW3_MIDI_SEED=str(seed)), capture_output=True, text=True)
        names = [ln.split("midi: ")[1].split(",")[0] for ln in out.stdout.splitlines() if ln.startswith("log: midi:") and "notes" in ln]
        keys = [int(ln.split("=")[1], 16) for ln in out.stdout.splitlines() if ln.startswith("key=")]
        return out.returncode, names, keys

    KEY = 0x60000000 | 74 << 18
    rc, names, keys = playlist(2, 5)
    want = ["A_other.MIDI", "b_song.mid", "huge.mid", "A_other.MIDI", "b_song.mid"]
    if rc != 0 or names != want or keys != [KEY] * 5:
        fail(f"in order: {names} {keys}, wanted {want}")
    else:
        print("ok all, in order: alphabetical, wraps, unplayable files skipped")
    rc, names, keys = playlist(-1, 2)  # a new process: the position is in battle_music.txt
    if names != ["huge.mid", "A_other.MIDI"]:
        fail(f"in order does not continue across a restart: {names}")
    else:
        print("ok all, in order continues across a restart (the last file is in battle_music.txt)")
    (folder / "battle_music.txt").write_text("mode: order\nlast: b_song.mid\n")
    (folder / "b_song.mid").rename(OUT / "b_song.gone")  # the last file deleted: the next name after it still follows
    rc, names, keys = playlist(-1, 1)
    shutil.move(str(OUT / "b_song.gone"), str(folder / "b_song.mid"))
    if names != ["huge.mid"]:
        fail(f"in order after a deleted last file: {names}")
    rc, a, keys = playlist(1, 30, seed=7)
    rc, b, _ = playlist(1, 30, seed=7)
    if rc != 0 or len(a) != 30 or set(a) != {"A_other.MIDI", "b_song.mid", "huge.mid"} or any(x == y for x, y in zip(a, a[1:])):
        fail(f"shuffled: {a}")
    elif a != b:
        fail("shuffled with the same seed is not repeatable")
    else:
        print("ok all, shuffled: every file comes up, never the same twice in a row, repeatable with a seed")
    only = OUT / "badfolder"
    only.mkdir(exist_ok=True)
    (only / "notmidi.mid").write_bytes(b"nope")
    (only / "battle_music.txt").write_text("mode: shuffle\n")
    rc, names, keys = playlist(-1, 3, where=only)
    if rc != 0 or names or keys != [0x42] * 3:
        fail(f"no file converts: {names} {keys}, wanted the game's own music")
    else:
        print("ok no file converts: the game's own music, no crash")
    (only / "battle_music.txt").write_text("battle.mid\n")  # the first version's file: a name only is a single pick
    (only / "battle.mid").write_bytes(song(5, 1, False))
    rc, names, keys = playlist(-1, 2, where=only)
    if names != ["battle.mid"] * 2:
        fail(f"an old one-line choice file: {names}")
    else:
        print("ok an old one-line battle_music.txt is a single pick")
    # the strings of the screen exist in the generated text for every language
    text = (ROOT / "port/game/definitive_text.c").read_text() + (ROOT / "port/game/definitive_text_lang.inc").read_text()
    for key in ("MidiTitle", "MidiKeys", "MidiOwn", "MidiShuffle", "MidiOrder", "MidiFolder", "MidiNone", "MidiMore", "MidiSet", "MidiOwnSet", "MidiCut"):
        for lang in ("en", "pt", "fr", "de", "it", "es"):
            if f"def_{lang}_str_{key}[]" not in text and not (lang == "en" and f"def_str_{key}[" in text):
                fail(f"the string {key} is missing in {lang}")
    print("definitive_midi: " + ("FAIL" if bad else "pass"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
