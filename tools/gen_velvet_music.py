#!/usr/bin/env python3
"""The Velvet Room's music: a local MIDI score, encoded as a SEP the port's LIBSND plays, plus the edits the
bank (BGM031) needs to sound like the intended arrangement.

  tools/venv/bin/python tools/gen_velvet_music.py            # summary and the smoothing's before/after table
  tools/venv/bin/python tools/gen_velvet_music.py --apply    # write port/game/definitive_velvet_music.inc
  tools/venv/bin/python tools/gen_velvet_music.py --apply --smooth off   # the unsmoothed mix

The MIDI is a local file, read from $DW3_VELVET_MIDI (default below). It is never copied into the tree: the
generated .inc (the score, and the piano sample read from the disc's BOSS01 bank) is gitignored. Without the MIDI or the
disc nothing is written, an existing .inc is left alone, and port/game/definitive_velvet_music.c falls back to the
room's old music (the build works either way).

Everything one may want to change by ear is a named constant under "Smoothing". --smooth off drops all of it
(hard attacks and cut-off releases of the bank's own tones, no reverb), a dry mix.
"""
import argparse
import functools
import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests/sound"))

OUT = ROOT / "port/game/definitive_velvet_music.inc"
MIDI_DEFAULT = Path.home() / "Downloads/Persona 5 - Aria of the Soul - Octet - Aragi Shota.mid"

# ---- The tuned mix: change it only by ear ----
BANK_DIR = "BGM031"  # the bank whose samples the room plays (sound bank 33)
# part name in the MIDI, channel (one each: horn and bass shared channel 8 in the MIDI), program, level 0-100, pan
PARTS = [
    ("Flute", 0, 8, 100, 32),
    ("Oboe", 1, 5, 76, 96),
    ("Piano", 2, 10, 30, 64),
    ("Violin", 3, 4, 66, 40),
    ("Viola", 4, 6, 66, 88),
    ("Horn", 5, 1, 71, 48),
    ("Cello", 6, 2, 71, 80),
    ("DoubleBass", 7, 3, 82, 64),
]
# Retunes of the bank's tone records (program -> fields). The oboe is the flute's waveform with its centre at 84, which
# played notes 49-74 two octaves flat: 62 is its register. The bank's own piano sample (vag 14) is a noise burst: the
# BOSS01's vag 9 is used, copied over vag 14's slot, at tone volume 96, centre 66.
RETUNE = {5: dict(center=62), 10: dict(vol=96, center=66, shift=0)}
PIANO_PROGRAM = 10
PIANO_VAG = 14
PIANO_BANK_DIR = "BOSS01"
PIANO_BANK_VAG = 9

# ---- Smoothing (--smooth off turns all of it off) ----
# The bank's own tones attack in 0.2-2 ms (a click) and release in 6-46 ms (the note is cut off at its note-off). The
# envelopes below are what the SPU's ADSR can do: a linear attack and a linear release, picked from the nearest
# shift/step to these times. The piano keeps its attack and release.
ATTACK_MS = {"Flute": 60, "Oboe": 40, "Violin": 45, "Viola": 45, "Horn": 50, "Cello": 50, "DoubleBass": 40}
RELEASE_MS = {"Flute": 200, "Oboe": 200, "Violin": 350, "Viola": 200, "Horn": 200, "Cello": 350, "DoubleBass": 350}
# The room's reverb: LIBSND's own (NRPN 15 = type, 16 = depth 0-127), the game's type 3 ("studio medium": types above
# it need a bigger work area at the top of the SPU RAM and could overlap the bank's samples). The init sets depth 0, so
# the tones' reverb flag (mode 4) did nothing until a sequence raises the depth: this one does, and the game puts it
# back to 0 when the room's music stops (port/game/definitive_velvet_music.c). 0 = no reverb.
REVERB_TYPE = 3
REVERB_DEPTH = 64
# The horn's and the piano's tones have the reverb flag off in the bank (the others have it on): the parts named here
# get it on, so the whole octet sits in the one room.
REVERB_PARTS = ("Flute", "Oboe", "Piano", "Violin", "Viola", "Horn", "Cello", "DoubleBass")
# Rest after the last note-off before the piece loops (ticks, 480 per quarter): the release and the reverb tail.
LOOP_GAP_TICKS = 960


# ---- The SPU's envelope (runtime/spu_internal.h spu_envelope_tick, psx-spx), to turn a time into a shift/step ----
def _tick(e, rate, exp, dec):
    shift = (rate >> 2) & 31
    step = 7 - (rate & 3)
    if dec:
        step = ~step
    step = step * (1 << (11 - shift)) if shift < 11 else step
    inc = 0x8000 >> (shift - 11) if shift > 11 else 0x8000
    if exp and not dec and e[0] > 0x6000:
        if shift < 10:
            step >>= 2
        elif shift >= 11:
            inc >>= 2
        else:
            step >>= 1
            inc >>= 1
    elif exp and dec:
        step = (step * e[0]) >> 15
    if (rate & 0x7F) != 0x7F and inc == 0:
        inc = 1
    e[1] += inc
    if not e[1] & 0x8000:
        return
    e[1] -= 0x8000
    e[0] = max(-0x8000, min(0x7FFF, e[0] + step)) if not dec else max(0, e[0] + step)


@functools.lru_cache(maxsize=None)
def attack_ms(adsr1):
    exp, shift, step = adsr1 >> 15, (adsr1 >> 10) & 31, (adsr1 >> 8) & 3
    e, n = [0, 0], 0
    while e[0] < 0x7FFF and n < 2_000_000:
        _tick(e, shift << 2 | step, exp, 0)
        n += 1
    return n / 44.1


@functools.lru_cache(maxsize=None)
def release_ms(adsr2):
    exp, shift = (adsr2 >> 5) & 1, adsr2 & 31
    e, n = [0x7FFF, 0], 0
    while e[0] > 0 and n < 8_000_000:
        _tick(e, shift << 2, exp, 1)
        n += 1
    return n / 44.1


def pick_attack(adsr1, target):
    """adsr1 with a linear attack nearest `target` ms (decay and sustain level untouched)."""
    best = min(((abs(attack_ms(shift << 10 | step << 8) - target), shift, step)
                for shift in range(2, 15) for step in range(4)))
    return (adsr1 & 0x00FF) | best[1] << 10 | best[2] << 8


def pick_release(adsr2, target):
    """adsr2 with a linear release nearest `target` ms (the sustain half untouched)."""
    best = min((abs(release_ms(shift) - target), shift) for shift in range(6, 16))
    return (adsr2 & 0xFFC0) | best[1]


# ---- The MIDI ----
def _vlq(d, o):
    v = 0
    while True:
        b = d[o]
        o += 1
        v = v << 7 | b & 0x7F
        if not b & 0x80:
            return v, o


def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(n & 0x7F | 0x80)
        n >>= 7
    return bytes(reversed(out))


def parse_midi(path):
    """(ticks per quarter, [(tick, tempo us)], {track name: [(tick, note, velocity or 0 for off)]})."""
    data = path.read_bytes()
    if data[:4] != b"MThd":
        raise SystemExit(f"not a MIDI file: {path}")
    hlen, fmt, ntr, div = struct.unpack(">IHHH", data[4:14])
    if fmt != 1 or div & 0x8000:
        raise SystemExit(f"need a format-1 metric MIDI, got format {fmt} division {div}")
    off = 8 + hlen
    tempos, tracks = [], {}
    for _ in range(ntr):
        if data[off:off + 4] != b"MTrk":
            raise SystemExit("truncated track")
        end = off + 8 + struct.unpack(">I", data[off + 4:off + 8])[0]
        o, status, tick, name, notes = off + 8, None, 0, "", []
        while o < end:
            dt, o = _vlq(data, o)
            tick += dt
            b = data[o]
            if b == 0xFF:
                meta, n = data[o + 1], data[o + 2]
                payload = data[o + 3:o + 3 + n]
                o += 3 + n
                if meta == 0x2F:
                    break
                if meta == 0x03:
                    name = payload.decode("latin1")
                if meta == 0x51 and n == 3:
                    tempos.append((tick, int.from_bytes(payload, "big")))
                continue
            if b in (0xF0, 0xF7):
                n, o = _vlq(data, o + 1)
                o += n
                status = None
                continue
            if b & 0x80:
                status = b
                o += 1
            kind = status & 0xF0
            if kind in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                d1, d2 = data[o], data[o + 1]
                o += 2
                if kind == 0x90 and d2:
                    notes.append((tick, d1, d2))
                elif kind in (0x90, 0x80):
                    notes.append((tick, d1, 0))
            elif kind in (0xC0, 0xD0):
                o += 1
            else:
                raise SystemExit(f"status {status:#x} in the MIDI")
        if name:
            tracks[name] = notes
        off = end
    return div, sorted(tempos) or [(0, 500000)], tracks


# ---- The SEP ----
def level_cc7(level):
    return max(0, min(127, round(level * 127 / 100)))


def build_sep(div, tempos, tracks, smooth):
    """The SEP the port's LIBSND plays: sequence 0 is the piece (looping forever the way the game's BGMs do: NRPN 20 /
    30 with count 127), the other 15 an empty end-of-track. Note-off is note-on velocity 0 (this LIBSND skips 8n);
    every event carries its status byte (a meta event leaves the running status alone, but there is no need to rely on
    it); tempo is FF 51 with no length byte."""
    ev, order = [], 0

    def add(tick, raw):
        nonlocal order
        ev.append((tick, order, raw))
        order += 1

    if smooth and REVERB_DEPTH > 0:
        # NRPN: CC99 = 16, CC98 = 15 (type) or 16 (depth), then data entry (CC6). Before the loop, once.
        for lsb, value in ((15, REVERB_TYPE), (16, REVERB_DEPTH)):
            add(0, bytes((0xB0, 99, 16)))
            add(0, bytes((0xB0, 98, lsb)))
            add(0, bytes((0xB0, 6, value)))
    for _name, ch, prog, level, pan in PARTS:
        add(0, bytes((0xC0 | ch, prog)))
        add(0, bytes((0xB0 | ch, 7, level_cc7(level))))
        add(0, bytes((0xB0 | ch, 10, pan)))
    # The loop, as the disc's BGMs write it: CC99 = 20, then (a tick later) CC6 = count 127. The piece starts one tick
    # later than the MIDI's tick 0; the tempo is repeated inside the loop (the second tempo would carry over).
    add(0, bytes((0xB0, 99, 20)))
    add(1, bytes((0xB0, 6, 127)))
    shift = 1
    add(shift, b"\xff\x51" + tempos[0][1].to_bytes(3, "big"))
    for tick, us in tempos[1:]:
        add(tick + shift, b"\xff\x51" + us.to_bytes(3, "big"))
    last = 0
    for name, ch, _prog, _level, _pan in PARTS:
        if name not in tracks:
            raise SystemExit(f"the MIDI has no track {name!r} (has {list(tracks)})")
        for tick, note, vel in tracks[name]:
            add(tick + shift, bytes((0x90 | ch, note, vel)))
            last = max(last, tick + shift)
    add(last + LOOP_GAP_TICKS, bytes((0xB0, 99, 30)))
    ev.sort(key=lambda e: (e[0], e[1]))
    data, prev = bytearray(), 0
    for tick, _o, raw in ev:
        data += vlq(tick - prev) + raw
        prev = tick
    data += b"\x00\xff\x2f\x00"

    def seq(n, tempo, body):
        return struct.pack(">HH", n, div) + tempo.to_bytes(3, "big") + b"\x04\x02" + struct.pack(">I", len(body)) + body

    sep = b"pQES\x00\x00" + seq(0, tempos[0][1], bytes(data))
    for n in range(1, 16):
        sep += seq(n, 500000, b"\x00\xff\x2f\x00")
    return sep


# ---- The bank's tones ----
def tone_edits(smooth):
    """[(program, tone, {field: value})] over BGM031's tone records. adsr1/adsr2 are the SPU words; the rest are the
    VAB tone's mode (reverb flag 4), vol, center, shift."""
    import disc_files
    import sound_formats
    exe = disc_files.EXE.read_bytes()
    id_path = {f.id: f.path for f in disc_files.files()}
    bank = next(b for _i, b in sound_formats.banks(exe) if id_path[b["header_file"]].endswith(f"/MP{BANK_DIR}.BIN"))
    vh = sound_formats.parse_vh(sound_formats.sub(disc_files.read(bank["header_file"]), bank["vab_header"]))
    tones = {}
    used = [i for i, p in enumerate(vh["progs"]) if p[0]]
    for k, prog in enumerate(used[:vh["programs"]]):
        for t in range(vh["progs"][prog][0]):
            tones[(prog, t)] = vh["tone_recs"][k * 16 + t][2]
    edits, report = [], []
    for name, _ch, prog, _level, _pan in PARTS:
        rec = tones[(prog, 0)]
        fields = dict(RETUNE.get(prog, {}))
        a1, a2 = rec[16], rec[17]
        if smooth and prog != PIANO_PROGRAM:
            a1n = pick_attack(a1, ATTACK_MS[name])
            a2n = pick_release(a2, RELEASE_MS[name])
            fields.update(adsr1=a1n, adsr2=a2n)
            report.append((name, prog, attack_ms(a1), attack_ms(a1n), release_ms(a2), release_ms(a2n)))
        else:
            report.append((name, prog, attack_ms(a1), attack_ms(a1), release_ms(a2), release_ms(a2)))
        if smooth and name in REVERB_PARTS and REVERB_DEPTH > 0:
            fields.update(mode=4)
        if fields:
            edits.append((prog, 0, fields))
    return edits, report


def piano_sample():
    """BOSS01's vag 9 as the disc has it: the ADPCM bytes used for the piano."""
    import disc_files
    import sound_formats
    exe = disc_files.EXE.read_bytes()
    id_path = {f.id: f.path for f in disc_files.files()}
    bank = next(b for _i, b in sound_formats.banks(exe) if id_path[b["header_file"]].endswith(f"/MP{PIANO_BANK_DIR}.BIN"))
    vh = sound_formats.parse_vh(sound_formats.sub(disc_files.read(bank["header_file"]), bank["vab_header"]))
    body = sound_formats.sub(disc_files.read(bank["body_file"]), bank["vab_body"])
    start = sum(vh["vag_sizes"][:PIANO_BANK_VAG])
    return bytes(body[start:start + vh["vag_sizes"][PIANO_BANK_VAG]])


def c_bytes(name, blob):
    rows = [", ".join(f"0x{b:02X}" for b in blob[i:i + 16]) for i in range(0, len(blob), 16)]
    return f"static const unsigned char {name}[{len(blob)}] = {{\n    " + ",\n    ".join(rows) + ",\n};\n"


def build(smooth, midi):
    """(sep, tone edits, piano sample, smoothing report, tracks): everything the .inc and the listening tools carry."""
    div, tempos, tracks = parse_midi(midi)
    sep = build_sep(div, tempos, tracks, smooth)
    edits, report = tone_edits(smooth)
    return sep, edits, piano_sample(), report, tracks


def render(smooth, midi):
    sep, edits, piano, report, tracks = build(smooth, midi)
    out = ["/* Generated by tools/gen_velvet_music.py from a local MIDI and the disc: not committed (it is\n"
           " * .gitignore'd), not shipped. The Velvet Room's score and the edits of its bank (docs: FORMATS \"SEP\"). */\n",
           f"/* smoothing {'on' if smooth else 'off'}; reverb type {REVERB_TYPE} depth {REVERB_DEPTH if smooth else 0} */\n",
           c_bytes("vl_score", sep), c_bytes("vl_piano", piano),
           f"#define VL_PIANO_VAG {PIANO_VAG}\n",
           f"#define VL_REVERB_DEPTH {REVERB_DEPTH if smooth else 0}\n",
           "/* program, tone, then vol, center, shift, mode, adsr1, adsr2 (-1: the bank's own) */\n",
           "static const int vl_tone_edits[][8] = {\n"]
    for prog, tone, f in edits:
        vals = [f.get(k, -1) for k in ("vol", "center", "shift", "mode", "adsr1", "adsr2")]
        out.append(f"    {{ {prog}, {tone}, " + ", ".join(str(v) for v in vals) + " },\n")
    out.append("};\n")
    return "".join(out), sep, report, tracks


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true", help="write the .inc")
    ap.add_argument("--smooth", choices=("on", "off"), default="on")
    ap.add_argument("--midi", type=Path, default=Path(os.environ.get("DW3_VELVET_MIDI", MIDI_DEFAULT)))
    ap.add_argument("--quiet", action="store_true", help="no output when there is nothing to do (the build's call)")
    args = ap.parse_args()
    if not args.midi.is_file():
        if not args.quiet:
            print(f"no MIDI at {args.midi} (set DW3_VELVET_MIDI): the room keeps its old music")
        return 0
    try:
        text, sep, report, tracks = render(args.smooth == "on", args.midi)
    except (FileNotFoundError, StopIteration, OSError) as e:
        if not args.quiet:
            print(f"no disc to read the bank and the piano from ({e}): the room keeps its old music")
        return 0
    if not args.quiet:
        print(f"score: {len(sep)} bytes SEP, {sum(1 for t in tracks.values() for n in t if n[2])} notes, "
              f"smoothing {args.smooth}")
        print(f"{'part':<11}{'prog':>5}{'attack ms':>22}{'release ms':>24}")
        for name, prog, a0, a1, r0, r1 in report:
            print(f"{name:<11}{prog:>5}{a0:>10.1f} ->{a1:>7.1f}{r0:>12.1f} ->{r1:>8.1f}")
    if args.apply:
        if OUT.is_file() and OUT.read_text() == text:
            return 0
        OUT.write_text(text)
        if not args.quiet:
            print(f"wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
