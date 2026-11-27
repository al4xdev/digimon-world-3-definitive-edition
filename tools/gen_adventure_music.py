#!/usr/bin/env python3
"""Digimon Adventure ("Butter-Fly") PS1 LIBSND music generator and BGM031 bank edits.

  tools/venv/bin/python tools/gen_adventure_music.py            # summary, buffer budget, and smoothing table
  tools/venv/bin/python tools/gen_adventure_music.py --apply    # write port/game/definitive_adventure_music.inc
  tools/venv/bin/python tools/gen_adventure_music.py --apply --smooth off   # unsmoothed raw pass

Reads a local MIDI from $DW3_ADVENTURE_MIDI (default below); never copies or commits the MIDI into the
repo. The generated .inc (SEP score, BOSS02 vag 8 guitar sample, and BGM031 tone edits) is gitignored. Without the
MIDI or the disc nothing is written, an existing .inc is left alone, and port/game/definitive_adventure_music.c reports
unavailable cleanly so the build works either way.
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

OUT = ROOT / "port/game/definitive_adventure_music.inc"
MIDI_DEFAULT = Path.home() / "Downloads/Digimon Adventure OP1 - Butterfly (Full) - Octet - Thymm Rayne.mid"

BUFFER_SIZE = 0xA000  # sound_buffer_1/2 capacity (40960 bytes)

# ---- Bank & sample choice ----
BANK_DIR = "BGM031"  # sound bank 33
GUITAR_PROGRAM = 5
GUITAR_VAG = 14  # spare slot in BGM031 (5824 bytes)
GUITAR_BANK_DIR = "BOSS02"
GUITAR_BANK_VAG = 8  # 3872 bytes; copied over vag 14, only program 5 points at it (program 8's flute stays on vag 9)

# Retune program 5's tone to the swapped BOSS02 guitar sample; keep program 8's flute untouched.
RETUNE = {GUITAR_PROGRAM: dict(vol=96, center=64, shift=0, vag=GUITAR_VAG)}

# Track mapping: (MIDI track name, channel, BGM031 program, level 0-100, pan 0-127).
# Raw first-pass balance from handoff_adventure_music.md (--smooth off):
PARTS_RAW = [
    ("Trumpet", 0, 8, 90, 64),
    ("ElectricGuitarOverdriven", 1, 5, 80, 40),
    ("Cymbal", 2, 10, 45, 100),
    ("Doublebass", 3, 4, 85, 64),
    ("BassDrum", 4, 6, 60, 64),
    ("Harp", 5, 1, 70, 32),
    ("Saxophone", 6, 2, 90, 96),
    ("SnareDrum", 7, 3, 50, 80),
]

# Smoothed & balanced mix (--smooth on):
# Melody (Guitar, Saxophone, Trumpet) leads clearly without clipping or piercing; Doublebass anchors the low end;
# Harp softens slightly (its raw VAG has the highest RMS in BGM031); tonal-sampled drums/cymbal sit gently behind.
PARTS_SMOOTH = [
    ("Trumpet", 0, 8, 82, 64),
    ("ElectricGuitarOverdriven", 1, 5, 88, 44),
    ("Cymbal", 2, 10, 24, 96),
    ("Doublebass", 3, 4, 88, 64),
    ("BassDrum", 4, 6, 44, 64),
    ("Harp", 5, 1, 56, 36),
    ("Saxophone", 6, 2, 88, 84),
    ("SnareDrum", 7, 3, 36, 78),
]

# ---- Smoothing (ADSR + reverb; --smooth off drops all of it) ----
# Raw BGM031 tones have 0.09-1.6 ms attacks (digital clicks) and 5.8-46 ms releases (abrupt pops), plus SL=15/15
# wall-of-sound sustain (and a 4039 ms release on Cymbal). At 165 BPM (91 ms 16ths / 182 ms 8ths), moderate linear
# attacks (12-37 ms) remove clicks while preserving groove; gentle exponential decays to balanced sustain levels keep
# voices from blaring; linear releases (93-372 ms) prevent note-off pops and smooth transitions.
ATTACK_MS = {
    "Trumpet": 37,
    "ElectricGuitarOverdriven": 19,
    "Cymbal": 19,
    "Doublebass": 23,
    "BassDrum": 13,
    "Harp": 19,
    "Saxophone": 31,
    "SnareDrum": 12,
}
DECAY_MS = {
    "Trumpet": 130,
    "ElectricGuitarOverdriven": 130,
    "Cymbal": 65,
    "Doublebass": 130,
    "BassDrum": 33,
    "Harp": 130,
    "Saxophone": 130,
    "SnareDrum": 33,
}
SUSTAIN_LEVEL = {  # 0..15 -> (N+1)/16 of peak
    "Trumpet": 11,
    "ElectricGuitarOverdriven": 10,
    "Cymbal": 4,
    "Doublebass": 10,
    "BassDrum": 3,
    "Harp": 7,
    "Saxophone": 11,
    "SnareDrum": 3,
}
SUSTAIN_SHIFT = {  # exponential downward sustain drift (higher = slower fade while held)
    "Trumpet": 18,
    "ElectricGuitarOverdriven": 17,
    "Cymbal": 14,
    "Doublebass": 16,
    "BassDrum": 13,
    "Harp": 15,
    "Saxophone": 18,
    "SnareDrum": 13,
}
RELEASE_MS = {
    "Trumpet": 186,
    "ElectricGuitarOverdriven": 186,
    "Cymbal": 186,
    "Doublebass": 372,
    "BassDrum": 93,
    "Harp": 372,
    "Saxophone": 186,
    "SnareDrum": 93,
}

# LIBSND reverb (NRPN 15 = type, 16 = depth): type 3 ("studio medium") fits the safe SPU work area; depth 48 adds
# cohesive warmth without smearing fast 165 BPM lines. Reset to 0 when the piece stops (definitive_adventure_music.c).
REVERB_TYPE = 3
REVERB_DEPTH = 48
REVERB_PARTS = (
    "Trumpet", "ElectricGuitarOverdriven", "Cymbal", "Doublebass",
    "BassDrum", "Harp", "Saxophone", "SnareDrum",
)
LOOP_GAP_TICKS = 960


# ---- SPU ADSR envelope math (psxstack/runtime/spu_internal.h, psx-spx) ----
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
def decay_ms(adsr1):
    d_shift = (adsr1 >> 4) & 15
    sl = ((adsr1 & 15) + 1) * 0x800
    e, n = [0x7FFF, 0], 0
    while e[0] > sl and n < 2_000_000:
        _tick(e, d_shift << 2, 1, 1)
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


def pick_adsr1(target_attack_ms, target_decay_ms, sustain_level):
    """Build adsr1 with linear attack nearest target_attack_ms, exponential decay nearest target_decay_ms, and SL."""
    _, a_shift, a_step = min(
        (abs(attack_ms(shift << 10 | step << 8) - target_attack_ms), shift, step)
        for shift in range(2, 15)
        for step in range(4)
    )
    sl = max(0, min(15, sustain_level))
    _, d_shift = min(
        (abs(decay_ms((d << 4) | sl) - target_decay_ms), d)
        for d in range(16)
    )
    return (a_shift << 10) | (a_step << 8) | (d_shift << 4) | sl


def pick_adsr2(sustain_shift, target_release_ms):
    """Build adsr2 with exponential downward sustain drift and linear release nearest target_release_ms."""
    _, r_shift = min(
        (abs(release_ms(shift) - target_release_ms), shift)
        for shift in range(6, 16)
    )
    return (1 << 15) | (1 << 14) | ((sustain_shift & 0x1F) << 8) | r_shift


# ---- MIDI ----
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


# ---- SEP ----
def level_cc7(level):
    return max(0, min(127, round(level * 127 / 100)))


def build_sep(div, tempos, tracks, smooth):
    """Build a 16-sequence PS1 LIBSND SEP with safe MIDI running status.

    Running status omits repeated full status bytes (kind | channel) on consecutive channel events, while always
    emitting explicit status bytes after meta events (0xFF) and at loop boundaries (CC99=20, CC99=30) so both
    psxstack's libsnd_seq.c and tests/sound/sound_formats.py parse cleanly and loop jumps stay synchronized.
    """
    parts = PARTS_SMOOTH if smooth else PARTS_RAW
    ev, order = [], 0

    def add(tick, raw, reset_rs=False):
        nonlocal order
        ev.append((tick, order, raw, reset_rs))
        order += 1

    if smooth and REVERB_DEPTH > 0:
        for lsb, value in ((15, REVERB_TYPE), (16, REVERB_DEPTH)):
            add(0, bytes((0xB0, 99, 16)))
            add(0, bytes((0xB0, 98, lsb)))
            add(0, bytes((0xB0, 6, value)))
    for _name, ch, prog, level, pan in parts:
        add(0, bytes((0xC0 | ch, prog)))
        add(0, bytes((0xB0 | ch, 7, level_cc7(level))))
        add(0, bytes((0xB0 | ch, 10, pan)))
    add(0, bytes((0xB0, 99, 20)), reset_rs=True)
    add(1, bytes((0xB0, 6, 127)), reset_rs=True)
    shift = 1
    add(shift, b"\xff\x51" + tempos[0][1].to_bytes(3, "big"), reset_rs=True)
    for tick, us in tempos[1:]:
        add(tick + shift, b"\xff\x51" + us.to_bytes(3, "big"), reset_rs=True)
    last = 0
    for name, ch, _prog, _level, _pan in parts:
        if name not in tracks:
            raise SystemExit(f"the MIDI has no track {name!r} (has {list(tracks)})")
        for tick, note, vel in tracks[name]:
            add(tick + shift, bytes((0x90 | ch, note, vel)))
            last = max(last, tick + shift)
    add(last + LOOP_GAP_TICKS, bytes((0xB0, 99, 30)), reset_rs=True)
    ev.sort(key=lambda e: (e[0], e[1]))

    data, prev, last_status = bytearray(), 0, None
    for tick, _o, raw, reset_rs in ev:
        data += vlq(tick - prev)
        prev = tick
        status = raw[0]
        if reset_rs or status >= 0xF0:
            data += raw
            last_status = None
        elif status == last_status:
            data += raw[1:]
        else:
            data += raw
            last_status = status
    data += b"\x00\xff\x2f\x00"

    def seq(n, tempo, body):
        return struct.pack(">HH", n, div) + tempo.to_bytes(3, "big") + b"\x04\x02" + struct.pack(">I", len(body)) + body

    sep = b"pQES\x00\x00" + seq(0, tempos[0][1], bytes(data))
    for n in range(1, 16):
        sep += seq(n, 500000, b"\x00\xff\x2f\x00")
    return sep


# ---- Bank tones & sample ----
def bgm031_sep_offset_and_tones(smooth):
    """Return (sep_offset, edits, report) for BGM031."""
    import disc_files
    import sound_formats

    exe = disc_files.EXE.read_bytes()
    id_path = {f.id: f.path for f in disc_files.files()}
    bank = next(b for _i, b in sound_formats.banks(exe) if id_path[b["header_file"]].endswith(f"/MP{BANK_DIR}.BIN"))
    hdr_blob = disc_files.read(bank["header_file"])
    sep_offset = struct.unpack_from("<2I", hdr_blob, 0)[1]
    vh = sound_formats.parse_vh(sound_formats.sub(hdr_blob, bank["vab_header"]))
    tones = {}
    used = [i for i, p in enumerate(vh["progs"]) if p[0]]
    for k, prog in enumerate(used[:vh["programs"]]):
        for t in range(vh["progs"][prog][0]):
            tones[(prog, t)] = vh["tone_recs"][k * 16 + t][2]

    parts = PARTS_SMOOTH if smooth else PARTS_RAW
    edits, report = [], []
    for name, _ch, prog, _level, _pan in parts:
        rec = tones[(prog, 0)]
        fields = dict(RETUNE.get(prog, {}))
        a1, a2 = rec[16], rec[17]
        sl0 = a1 & 15
        if smooth:
            a1n = pick_adsr1(ATTACK_MS[name], DECAY_MS[name], SUSTAIN_LEVEL[name])
            a2n = pick_adsr2(SUSTAIN_SHIFT[name], RELEASE_MS[name])
            fields.update(adsr1=a1n, adsr2=a2n)
            report.append((name, prog, attack_ms(a1), attack_ms(a1n), sl0, a1n & 15, release_ms(a2), release_ms(a2n)))
        else:
            report.append((name, prog, attack_ms(a1), attack_ms(a1), sl0, sl0, release_ms(a2), release_ms(a2)))
        if smooth and name in REVERB_PARTS and REVERB_DEPTH > 0:
            fields.update(mode=4)
        if fields:
            edits.append((prog, 0, fields))
    return sep_offset, edits, report


def guitar_sample():
    """BOSS02's vag 8 ADPCM bytes (3872 bytes) for BGM031's vag 14 slot."""
    import disc_files
    import sound_formats

    exe = disc_files.EXE.read_bytes()
    id_path = {f.id: f.path for f in disc_files.files()}
    bank = next(b for _i, b in sound_formats.banks(exe) if id_path[b["header_file"]].endswith(f"/MP{GUITAR_BANK_DIR}.BIN"))
    vh = sound_formats.parse_vh(sound_formats.sub(disc_files.read(bank["header_file"]), bank["vab_header"]))
    body = sound_formats.sub(disc_files.read(bank["body_file"]), bank["vab_body"])
    start = sum(vh["vag_sizes"][:GUITAR_BANK_VAG])
    return bytes(body[start:start + vh["vag_sizes"][GUITAR_BANK_VAG]])


def c_bytes(name, blob):
    rows = [", ".join(f"0x{b:02X}" for b in blob[i:i + 16]) for i in range(0, len(blob), 16)]
    return f"static const unsigned char {name}[{len(blob)}] = {{\n    " + ",\n    ".join(rows) + ",\n};\n"


def build(smooth, midi):
    div, tempos, tracks = parse_midi(midi)
    sep = build_sep(div, tempos, tracks, smooth)
    sep_offset, edits, report = bgm031_sep_offset_and_tones(smooth)
    if sep_offset + len(sep) > BUFFER_SIZE:
        raise SystemExit(
            f"SEP ({len(sep)} bytes) at BGM031 offset {sep_offset:#x} exceeds BUFFER_SIZE {BUFFER_SIZE:#x} "
            f"by {sep_offset + len(sep) - BUFFER_SIZE} bytes"
        )
    return sep_offset, sep, edits, guitar_sample(), report, tracks


def render(smooth, midi):
    sep_offset, sep, edits, guitar, report, tracks = build(smooth, midi)
    out = [
        "/* Generated by tools/gen_adventure_music.py from a local MIDI and the disc: not committed (it is\n"
        " * .gitignore'd), not shipped. Digimon Adventure (\"Butter-Fly\") score and BGM031 bank edits. */\n",
        f"/* smoothing {'on' if smooth else 'off'}; reverb type {REVERB_TYPE} depth {REVERB_DEPTH if smooth else 0}; "
        f"sep_offset {sep_offset} + {len(sep)} = {sep_offset + len(sep)} / {BUFFER_SIZE} */\n",
        c_bytes("adv_score", sep),
        c_bytes("adv_guitar", guitar),
        f"#define ADV_GUITAR_VAG {GUITAR_VAG}\n",
        f"#define ADV_REVERB_DEPTH {REVERB_DEPTH if smooth else 0}\n",
        "/* program, tone, then vol, center, shift, mode, adsr1, adsr2, vag (-1: the bank's own) */\n",
        "static const int adv_tone_edits[][9] = {\n",
    ]
    for prog, tone, f in edits:
        vals = [f.get(k, -1) for k in ("vol", "center", "shift", "mode", "adsr1", "adsr2", "vag")]
        out.append(f"    {{ {prog}, {tone}, " + ", ".join(str(v) for v in vals) + " },\n")
    out.append("};\n")
    return "".join(out), sep_offset, sep, report, tracks


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true", help="write the .inc")
    ap.add_argument("--smooth", choices=("on", "off"), default="on")
    ap.add_argument("--midi", type=Path, default=Path(os.environ.get("DW3_ADVENTURE_MIDI", MIDI_DEFAULT)))
    ap.add_argument("--quiet", action="store_true", help="no output when there is nothing to do (the build's call)")
    args = ap.parse_args()
    if not args.midi.is_file():
        if not args.quiet:
            print(f"no MIDI at {args.midi} (set DW3_ADVENTURE_MIDI): adventure music unavailable")
        return 0
    try:
        text, sep_offset, sep, report, tracks = render(args.smooth == "on", args.midi)
    except (FileNotFoundError, StopIteration, OSError) as e:
        if not args.quiet:
            print(f"no disc to read the bank and guitar sample from ({e}): adventure music unavailable")
        return 0
    if not args.quiet:
        headroom = BUFFER_SIZE - (sep_offset + len(sep))
        notes = sum(1 for t in tracks.values() for n in t if n[2])
        print(f"score: {len(sep)} bytes SEP at +{sep_offset:#x} ({sep_offset + len(sep)}/{BUFFER_SIZE}, "
              f"{headroom} bytes headroom), {notes} notes, smoothing {args.smooth}")
        print(f"{'part':<25}{'prog':>5}{'attack ms':>20}{'SL':>9}{'release ms':>22}")
        for name, prog, a0, a1, sl0, sl1, r0, r1 in report:
            print(f"{name:<25}{prog:>5}{a0:>8.1f} ->{a1:>7.1f}{sl0:>3}->{sl1:<3}{r0:>10.1f} ->{r1:>8.1f}")
    if args.apply:
        if OUT.is_file() and OUT.read_text() == text:
            return 0
        OUT.write_text(text)
        if not args.quiet:
            print(f"wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
