#!/usr/bin/env python3
"""Play a MIDI score live through the running game's own sequencer.

Does not pre-render a WAV, does not patch the disc and does not touch source.
It starts its own windowed port (a private debug socket — other agents' games
are left alone), waits until a music bank is loaded and a sequence is playing,
then points that sequence at the MIDI's notes. The game's vsync calls
SsSeqCalledTbyT, which keys the already-loaded samples.

The score is a MIDI from Downloads. Note-offs become note-on
velocity 0, because this LIBSND skips 8n. Tempo is FF 51 with no length byte.
Stay on the title screen: leaving it reloads the bank and drops the pointer.
"""
import json
import os
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "psxstack/tools/mcp"))
from game import Game, GameError  # noqa: E402

sys.path.insert(0, str(ROOT / "tools"))
import gen_velvet_music as gvm  # noqa: E402  (the room's score and bank edits: one source for the game and this tool)

HERE = Path(__file__).resolve().parent
# The source MIDIs are not in the repository: each user keeps their own in ~/Downloads.
MIDI = Path.home() / "Downloads" / "Persona 5 - Aria of the Soul - Octet - Aragi Shota.mid"
BIN = ROOT / "build/port-sdl/dw2003"
DISC = ROOT / "iso/dw2003.cue"
SOCK = Path("/tmp/velvet-live.sock")
LOG = HERE / "game.log"
OFFSETS = json.loads((HERE / "offsets.json").read_text())

# Octave staging: each track its own channel, so two parts that shared a MIDI
# channel (horn and bass) do not cut each other off.
# The piano they picked (piano.py option 19/51): BOSS01 vag 9, copied into the
# spare slot on each start. volumes.json is 0-100 per part, written by solo.py vol.
PIANO_BANK = "BOSS01"
PIANO_VAG = 9
VOLUMES = Path(__file__).resolve().parent / "volumes.json"
PARTS = [
    ("Flute", 0, 96, 32),
    ("Oboe", 1, 96, 96),
    ("Piano", 2, 112, 64),
    ("Violin", 3, 84, 40),
    ("Viola", 4, 84, 88),
    ("Horn", 5, 90, 48),
    ("Cello", 6, 90, 80),
    ("DoubleBass", 7, 104, 64),
]


def level_of(index, midi_default):
    """0-100 from volumes.json, else the part's original MIDI volume scaled down."""
    if VOLUMES.is_file():
        try:
            levels = json.loads(VOLUMES.read_text())
            return max(0, min(100, int(levels[index])))
        except (OSError, ValueError, IndexError, TypeError):
            pass
    return round(midi_default * 100 / 127)


def midi_of(level):
    return max(0, min(127, round(level * 127 / 100)))


def vlq(n):
    if n < 0:
        raise ValueError(n)
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(out))


def parse_midi(path):
    data = path.read_bytes()
    if data[:4] != b"MThd":
        raise SystemExit(f"not a MIDI file: {path}")
    hlen, fmt, ntr, div = struct.unpack(">IHHH", data[4:14])
    if fmt != 1 or div & 0x8000:
        raise SystemExit(f"need a format-1 metric MIDI, got format {fmt} division {div}")
    off = 8 + hlen
    tempos = []
    by_name = {}
    for ti in range(ntr):
        if data[off:off + 4] != b"MTrk":
            raise SystemExit("truncated track")
        ln = struct.unpack(">I", data[off + 4:off + 8])[0]
        start, end = off + 8, off + 8 + ln
        o, status, tick = start, None, 0
        name, notes = "", []
        while o < end:
            dt, o = _midi_vlq(data, o)
            tick += dt
            b = data[o]
            if b == 0xFF:
                meta, ln2 = data[o + 1], data[o + 2]
                payload = data[o + 3:o + 3 + ln2]
                o += 3 + ln2
                if meta == 0x2F:
                    break
                if meta == 0x03:
                    name = payload.decode("latin1", "replace")
                if meta == 0x51 and len(payload) == 3:
                    tempos.append((tick, int.from_bytes(payload, "big")))
                continue
            if b == 0xF0 or b == 0xF7:
                ln2, o = _midi_vlq(data, o + 1)
                o += ln2
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
                    notes.append((tick, d1, d2, True))
                elif kind == 0x90 or kind == 0x80:
                    notes.append((tick, d1, 0, False))
            elif kind in (0xC0, 0xD0):
                o += 1
            else:
                raise SystemExit(f"status {status:#x} in track {ti}")
        if name:
            by_name[name] = notes
        off = end
    if not tempos:
        tempos = [(0, 500000)]
    tempos.sort()
    return div, tempos, by_name


def _midi_vlq(d, o):
    v = 0
    while True:
        b = d[o]
        o += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, o


def build_score(div, tempos, by_name, programs):
    """SEP event stream the port's LIBSND reads, plus the header fields it
    already consumed when the game opened the original sequence."""
    events = []  # (tick, order, bytes)
    order = 0
    tempo0 = tempos[0][1]
    events.append((0, order, b"\xff\x51" + tempo0.to_bytes(3, "big")))
    order += 1
    for i, (_name, ch, vol, pan) in enumerate(PARTS):
        prog = programs[i % len(programs)]
        vol = midi_of(level_of(i, vol))
        events.append((0, order, bytes((0xC0 | ch, prog))))
        order += 1
        events.append((0, order, bytes((0xB0 | ch, 7, vol))))
        order += 1
        events.append((0, order, bytes((0xB0 | ch, 10, pan))))
        order += 1
    for tick, us in tempos[1:]:
        events.append((tick, order, b"\xff\x51" + us.to_bytes(3, "big")))
        order += 1
    missing = []
    for name, ch, _vol, _pan in PARTS:
        notes = by_name.get(name)
        if not notes:
            missing.append(name)
            continue
        for tick, note, vel, on in notes:
            # 8n is skipped without its data. Velocity 0 is the off this LIBSND plays.
            events.append((tick, order, bytes((0x90 | ch, note, vel if on else 0))))
            order += 1
    if missing:
        raise SystemExit(f"MIDI is missing tracks: {missing}")
    events.sort()
    last = 0
    blob = bytearray()
    first_delta = events[0][0] if events else 0
    # The open sequence already consumed its first delta; pos points at the event.
    for tick, _ord, raw in events:
        blob += vlq(tick - last)
        blob += raw
        last = tick
    blob += vlq(0) + b"\xff\x2f\x00"
    # Drop the leading delta: it is delivered as SndSeq.delta, not as bytes at pos.
    lead = vlq(first_delta)
    if blob[:len(lead)] != lead:
        raise SystemExit("internal: leading delta mismatch")
    stream = bytes(blob[len(lead):])
    bpm = 60000000 // tempo0
    return stream, first_delta, bpm, div


def step_of(resolution, bpm, rate=50):
    per = resolution * bpm * 10
    rate60 = rate * 60
    if per < rate60:
        wait = rate * 600 // (resolution * bpm)
        return wait, wait
    step = per // rate60
    if rate * 30 < per % rate60:
        step += 1
    return -1, step


def u8(b):
    return b[0]


def u16(b):
    return struct.unpack_from("<H", b)[0]


def u32(b):
    return struct.unpack_from("<I", b)[0]


def u64(b):
    return struct.unpack_from("<Q", b)[0]


def i8(n):
    return n - 256 if n >= 128 else n


class Mem:
    def __init__(self, game):
        self.g = game

    def read(self, addr, n):
        return self.g.peek(addr, n)

    def u8(self, addr):
        return u8(self.read(addr, 1))

    def u16(self, addr):
        return u16(self.read(addr, 2))

    def u32(self, addr):
        return u32(self.read(addr, 4))

    def u64(self, addr):
        return u64(self.read(addr, 8))

    def write(self, addr, data):
        self.g.poke(addr, data)


def symbol(name):
    # ET_EXEC: nm's address is the runtime address. The running image, not the file: another build can replace
    # build/port*/dw2003 while this game lives.
    exe = f"/proc/{OPTS['pid']}/exe" if OPTS.get("pid") else BIN
    line = subprocess.check_output(["nm", "-S", str(exe)], text=True)
    for row in line.splitlines():
        parts = row.split()
        if parts and parts[-1] == name:
            return int(parts[0], 16), int(parts[1], 16) if len(parts) > 3 else 0
    raise SystemExit(f"no symbol {name} in {exe}")


def live_heads(mem, snd):
    """Host addresses of VAB headers the sequencer still reads on every note."""
    heads = []
    base = snd + OFFSETS["SndState.vab"]
    for i in range(16):
        addr = base + i * OFFSETS["SndVab"]
        if mem.u8(addr + OFFSETS["SndVab.state"]) == 0:
            continue
        head = mem.u64(addr + OFFSETS["SndVab.head"])
        if head:
            heads.append(head)
    return heads


def pick_buffer(mem, snd, need):
    """A sound buffer the loaded headers do not occupy. Entry 0 is COMMON; the
    title bank landed in entry 1 on this boot, so entry 2 is the spare."""
    heads = live_heads(mem, snd)
    for name in ("sound_buffer_2", "sound_buffer_1", "sound_buffer_0"):
        addr, size = symbol(name)
        if size < need + 16:
            continue
        if any(addr <= h < addr + size for h in heads):
            print(f"{name} holds a VAB header, skipped")
            continue
        return name, addr, size
    raise SystemExit("no sound buffer is free of a live VAB header")


def retune(mem, head):
    """BGM031's program 10 is a noise burst (the 'zoado' piano) and program 5,
    the oboe, is the flute's waveform with its center note at 84, so the oboe
    part (notes 49-74) was playing about two octaves flat. Both are header
    bytes the sequencer reads on every note; the samples stay the ones already
    in the SPU."""
    hdr = mem.read(head, 0x16)
    programs = u16(hdr[0x12:0x14])
    blob = mem.read(head, 0x20 + 0x800 + programs * 0x200)
    used = [i for i in range(128) if blob[0x20 + 16 * i]]
    for k, prog in enumerate(used[:programs]):
        rel = 0x20 + 0x800 + 0x200 * k
        o = head + rel
        if prog == 5:
            mem.write(o + 4, bytes([62]))  # center: the oboe's register
            print(f"oboe program 5 center {blob[rel + 4]} -> 62")
        elif prog == 10:
            mem.write(o + 2, bytes([96]))  # vol, was 120
            mem.write(o + 4, bytes([66]))  # center, was 72
            mem.write(o + 0x16, struct.pack("<h", 6))  # vag 14 noise -> vag 6 pluck
            print(f"piano program 10 vag {struct.unpack_from('<h', blob, rel + 0x16)[0]} -> 6")


def install_piano(mem, snd, pid):
    """Option 2/51: BGM004's vag 9 in the spare slot, program 10 pointed at it."""
    import disc_files
    import sound_formats
    exe = disc_files.EXE.read_bytes()
    id_path = {f.id: f.path for f in disc_files.files()}
    blob = None
    for _bid, bank in sound_formats.banks(exe):
        if not id_path[bank["header_file"]].endswith(f"/{PIANO_BANK}/MP{PIANO_BANK}.BIN"):
            continue
        vh = sound_formats.parse_vh(sound_formats.sub(disc_files.read(bank["header_file"]), bank["vab_header"]))
        body = sound_formats.sub(disc_files.read(bank["body_file"]), bank["vab_body"])
        off = 0
        for i, sz in enumerate(vh["vag_sizes"]):
            if i == PIANO_VAG:
                blob = body[off:off + sz]
                break
            off += sz
        break
    if not blob:
        raise SystemExit(f"piano sample {PIANO_BANK} vag {PIANO_VAG} not on the disc")
    spu = None
    for line in subprocess.check_output(["nm", f"/proc/{pid}/exe"], text=True).splitlines():
        parts = line.split()
        if parts and parts[-1] == "spu":
            spu = int(parts[0], 16)
    if spu is None:
        raise SystemExit("spu not in the running binary")
    vab = snd + OFFSETS["SndState.vab"] + OFFSETS["SndVab"]
    vag_end = struct.unpack("<256H", mem.read(vab + 172, 512))
    slot, slot_end = vag_end[13] * 8, vag_end[14] * 8
    if len(blob) > slot_end - slot:
        raise SystemExit(f"piano sample {len(blob)} does not fit the spare slot")
    mem.write(spu + slot, blob)
    head = mem.u64(vab + OFFSETS["SndVab.head"])
    programs = mem.u16(head + 0x12)
    hdr = mem.read(head, 0x20 + 0x800 + programs * 0x200)
    used = [i for i in range(128) if hdr[0x20 + 16 * i]]
    for k, prog in enumerate(used[:programs]):
        if prog != 10:
            continue
        tone = head + 0x20 + 0x800 + 0x200 * k
        mem.write(tone + 2, bytes([96]))
        mem.write(tone + 4, bytes([66]))
        mem.write(tone + 5, bytes([0]))
        mem.write(tone + 0x16, struct.pack("<h", 14))
        print(f"piano: {PIANO_BANK} vag {PIANO_VAG} ({len(blob)} bytes) in the spare slot")
        return
    raise SystemExit("program 10 missing; piano not installed")


def usable_programs(mem, head):
    """Programs whose tones cover the score's range (G2..B5), longest sample first."""
    hdr = mem.read(head, 0x20)
    if hdr[:4] != b"pBAV":
        raise SystemExit(f"VAB header at {head:#x} is {hdr[:4]!r}, not pBAV")
    programs = u16(hdr[0x12:0x14])
    # program records + tone blocks + vag size table
    need = 0x20 + 0x800 + programs * 0x200 + 0x200
    body = mem.read(head, need)
    progs = []
    used = []
    for i in range(128):
        tones = body[0x20 + 16 * i]
        if tones:
            used.append(i)
        progs.append(tones)
    sizes = [s * 8 for s in struct.unpack_from("<256H", body, 0x20 + 0x800 + programs * 0x200)]
    ranked = []
    for k, prog in enumerate(used[:programs]):
        cover = 0
        best = 0
        for t in range(progs[prog]):
            o = 0x20 + 0x800 + 0x200 * k + 32 * t
            mn, mx = body[o + 6], body[o + 7]
            vag = struct.unpack_from("<h", body, o + 0x16)[0]
            if mn <= 49 and mx >= 83:
                cover = 1
            if 0 <= vag < len(sizes):
                best = max(best, sizes[vag])
        if cover:
            ranked.append((best, prog))
    ranked.sort(reverse=True)
    return [p for _sz, p in ranked]


def playing(mem, snd):
    s_max = mem.u32(snd + OFFSETS["SndState.s_max"])
    t_max = mem.u32(snd + OFFSETS["SndState.t_max"])
    base = snd + OFFSETS["SndState.seq"]
    found = []
    n = min(s_max * t_max, 96)
    for i in range(n):
        e = base + i * OFFSETS["SndSeq"]
        flags = mem.u32(e + OFFSETS["SndSeq.flags"])
        if flags & 1:
            vab = i8(mem.u8(e + OFFSETS["SndSeq.vab"]))
            found.append((i, e, vab, flags))
    return found


def wait_music(mem, snd, timeout=150, game=None):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if game is not None:  # headless, held at its first vsync: advance the game ourselves, a few frames at a time
            game.step(10)
        try:
            rows = playing(mem, snd)
        except GameError:  # the headless build answers "unmapped" until the game has touched its state
            if game is None:
                time.sleep(0.4)
            continue
        music = [r for r in rows if r[2] > 0]
        if music:
            return music[0], rows
        if game is None:
            time.sleep(0.4)
    raise SystemExit("no music sequence started playing (stayed on a silent screen?)")


HEADLESS_BIN = ROOT / "build/port/dw2003"
OPTS = {"smooth": True, "wav": None, "seconds": 50, "headless": False, "probe": False}


def start_game():
    if SOCK.exists():
        SOCK.unlink()
    log = open(LOG, "ab")
    log.write(f"\n--- spawn {time.strftime('%H:%M:%S')} ---\n".encode())
    log.flush()
    if OPTS["headless"]:
        argv = [str(HEADLESS_BIN), "--disc", str(DISC), "--fps", "0", "--debug", str(SOCK), "--debug-hold"]
    else:
        argv = [str(BIN), "--disc", str(DISC), "--window", "--debug", str(SOCK)]
    if OPTS["wav"]:
        argv += ["--wav", str(OPTS["wav"])]
    proc = subprocess.Popen(argv, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                            start_new_session=True)
    deadline = time.monotonic() + 40
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise SystemExit(f"game exited {proc.returncode}; see {LOG}")
        if SOCK.exists():
            try:
                return Game.attach(str(SOCK)), proc
            except (ConnectionRefusedError, FileNotFoundError, socket.error):
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit(f"no debug socket; see {LOG}")


def sep_stream(sep):
    """Sequence 0 of a SEP as play.py points the title's sequence at it: (stream after its leading delta, that delta,
    resolution, tempo us)."""
    res, tempo = struct.unpack(">H", sep[8:10])[0], int.from_bytes(sep[10:13], "big")
    size = struct.unpack(">I", sep[15:19])[0]
    data = sep[19:19 + size]
    delta, n = 0, 0
    while True:
        b = data[n]
        n += 1
        delta = delta << 7 | b & 0x7F
        if not b & 0x80:
            break
    return data[n:], delta, res, tempo


PROBE_NOTES = {"Flute": 72, "Oboe": 62, "Piano": 60, "Violin": 67, "Viola": 60, "Horn": 55, "Cello": 48, "DoubleBass": 40}
PROBE_SPACING = 4  # seconds between the probe notes; each is held 1 s


def probe_sep(smooth):
    """One 1 s note per part, 4 s apart, at the room's mix: the isolated attack and release of each part's tone, which
    the full score hides under the other seven. Same reverb and levels as the score; no loop."""
    res, tempo = 480, 600000
    ticks = lambda sec: round(sec * 1_000_000 / tempo * res)  # noqa: E731
    ev = []
    if smooth and gvm.REVERB_DEPTH > 0:
        for lsb, value in ((15, gvm.REVERB_TYPE), (16, gvm.REVERB_DEPTH)):
            ev += [(0, bytes((0xB0, 99, 16))), (0, bytes((0xB0, 98, lsb))), (0, bytes((0xB0, 6, value)))]
    for i, (name, ch, prog, level, pan) in enumerate(gvm.PARTS):
        ev += [(0, bytes((0xC0 | ch, prog))), (0, bytes((0xB0 | ch, 7, gvm.level_cc7(level)))),
               (0, bytes((0xB0 | ch, 10, pan)))]
        t = ticks(1 + i * PROBE_SPACING)
        ev += [(t, bytes((0x90 | ch, PROBE_NOTES[name], 80))), (t + ticks(1), bytes((0x90 | ch, PROBE_NOTES[name], 0)))]
    ev.sort(key=lambda e: e[0])
    data, prev = bytearray(), 0
    for t, raw in ev:
        data += gvm.vlq(t - prev) + raw
        prev = t
    data += gvm.vlq(ticks(PROBE_SPACING)) + b"\xff\x2f\x00"
    body = bytes(data)
    return b"pQES\x00\x00" + struct.pack(">HH", 0, res) + tempo.to_bytes(3, "big") + b"\x04\x02" + struct.pack(">I", len(body)) + body


def apply_edits(mem, head, edits):
    """The bank's tone edits (what port/game/definitive_velvet_music.c does to the loaded header): poked into the live
    VAB header the sequencer reads on every note."""
    hdr = mem.read(head, 0x20 + 0x800)
    programs = u16(mem.read(head + 0x12, 2))
    for prog, tone, f in edits:
        block = sum(1 for i in range(prog) if hdr[0x20 + 16 * i])
        o = head + 0x20 + 0x800 + 0x200 * block + 32 * tone
        for key, off, size in (("vol", 2, 1), ("center", 4, 1), ("shift", 5, 1), ("mode", 1, 1), ("adsr1", 0x10, 2),
                               ("adsr2", 0x12, 2)):
            if key in f:
                mem.write(o + off, f[key].to_bytes(size, "little"))
    return programs


def install_piano_bytes(mem, snd, pid, blob):
    """The piano's samples over vag 14's slot, in the live SPU RAM."""
    spu = None
    for line in subprocess.check_output(["nm", f"/proc/{pid}/exe"], text=True).splitlines():
        parts = line.split()
        if parts and parts[-1] == "spu":
            spu = int(parts[0], 16)
    if spu is None:
        raise SystemExit("spu not in the running binary")
    vab = snd + OFFSETS["SndState.vab"] + OFFSETS["SndVab"]
    vag_end = struct.unpack("<256H", mem.read(vab + 172, 512))
    slot, slot_end = vag_end[gvm.PIANO_VAG - 1] * 8, vag_end[gvm.PIANO_VAG] * 8
    if len(blob) > slot_end - slot:
        raise SystemExit(f"piano sample {len(blob)} does not fit the spare slot")
    mem.write(spu + slot, blob)


def render_wav(game, mem, proc, voice, start_frame):
    """Headless: the score runs for --seconds, then the game quits and its --wav is complete. Notes the voices carry
    (how many are busy, sampled every vsync) go beside it: 24 busy at once means LIBSND is stealing notes."""
    frames = OPTS["seconds"] * 50
    busy, done = [], 0
    size = OFFSETS["SndVoice"]
    while done < frames:
        if OPTS["probe"]:  # isolated notes: no voice census, run in big steps
            game.step(100)
            done += 100
            busy.append(0)
            continue
        game.step(1)
        done += 1
        raw = mem.read(voice, 24 * size)
        # LIBSND reuses a voice only when it is released and its envelope has reached 0 (u16 env at +6)
        busy.append(sum(1 for v in range(24)
                        if raw[v * size + OFFSETS["SndVoice.status"]] == 1 or u16(raw[v * size + 6:v * size + 8])))
    game.quit(0)
    proc.wait(timeout=30)
    meta = {"start_frame": start_frame, "frames": frames, "smooth": OPTS["smooth"], "busy_max": max(busy),
            "busy_at_24": sum(1 for b in busy if b >= 24), "busy_mean": sum(busy) / len(busy)}
    Path(str(OPTS["wav"]) + ".json").write_text(json.dumps(meta))
    print(f"rendered {OPTS['wav']}: {meta}")


ADVENTURE_MIDI = Path.home() / "Downloads" / "Digimon Adventure OP1 - Butterfly (Full) - Octet - Thymm Rayne.mid"
# The mix, 0-100. Drums sit under the melody.
ADVENTURE_LEVELS = {
    "Trumpet": (90, 64),
    "ElectricGuitarOverdriven": (80, 40),
    "Saxophone": (90, 96),
    "Harp": (70, 32),
    "Doublebass": (85, 64),
    "Cymbal": (45, 100),
    "BassDrum": (60, 64),
    "SnareDrum": (50, 80),
}


def main():
    global MIDI, SOCK, LOG, VOLUMES, PARTS
    args = sys.argv[1:]
    if "--reverb" in args:  # try another depth (0-127) without editing the generator
        i = args.index("--reverb")
        gvm.REVERB_DEPTH = int(args[i + 1])
        del args[i:i + 2]
    for flag in ("--smooth", "--wav", "--seconds"):
        if flag in args:
            i = args.index(flag)
            value = args[i + 1]
            del args[i:i + 2]
            OPTS[flag[2:]] = {"on": True, "off": False}[value] if flag == "--smooth" else (
                int(value) if flag == "--seconds" else Path(value))
    if "--probe" in args:
        args.remove("--probe")
        OPTS["probe"] = True
    if "--headless" in args:
        args.remove("--headless")
        OPTS["headless"] = True
    if OPTS["wav"]:
        OPTS["headless"] = True
    adventure = bool(args) and args[0] == "adventure"
    if adventure:
        MIDI = ADVENTURE_MIDI
        SOCK = Path("/tmp/adventure-music.sock")
        LOG = Path("tools/music_lab/adventure/game.log")
        LOG.parent.mkdir(exist_ok=True)
        VOLUMES = LOG.parent / "volumes.json"
    if not MIDI.is_file():
        raise SystemExit(f"missing {MIDI}")
    div, tempos, by_name = parse_midi(MIDI)
    if adventure:
        names = [n for n in by_name if n.lower() != "control"]
        PARTS = []
        for i, name in enumerate(names[:8]):
            level, pan = ADVENTURE_LEVELS.get(name, (80, 64))
            # level_of treats the third field as a MIDI default and scales it
            # back to 0-100, so store the MIDI value that round-trips to `level`.
            PARTS.append((name, i, round(level * 127 / 100), pan))
    print(f"midi: division {div}, {sum(len(v) for v in by_name.values())} notes, tracks {list(by_name)}")
    game, proc = start_game()
    OPTS["pid"] = proc.pid
    print(f"game pid {proc.pid} socket {SOCK}")
    mem = Mem(game)
    snd, _ = symbol("snd")
    # nm of the file, taken now: another build can move the symbol while this process lives.
    session = (LOG.parent / "session.txt") if adventure else (HERE / "session.txt")
    session.write_text(f"pid {proc.pid}\nsocket {SOCK}\nsnd {snd:#x}\n")
    print(f"waiting for the title music (snd {snd:#x})")
    (index, entry, vab, flags), rows = wait_music(mem, snd, game=game if OPTS["headless"] else None)
    print(f"playing seq index {index} vab {vab} flags {flags:#x}; also {[(i, v) for i, _e, v, _f in rows]}")
    game.pause()
    vab_addr = snd + OFFSETS["SndState.vab"] + vab * OFFSETS["SndVab"]
    state = mem.u8(vab_addr + OFFSETS["SndVab.state"])
    head = mem.u64(vab_addr + OFFSETS["SndVab.head"])
    print(f"vab state {state} head {head:#x}")
    if state != 1 or head == 0:
        raise SystemExit("the music VAB is not playable yet")
    if adventure:
        programs = usable_programs(mem, head)
        if not programs:
            raise SystemExit("the loaded bank has no program that covers G2..B5")
        print(f"programs (longest sample first): {programs}")
        stream, first_delta, bpm, resolution = build_score(div, tempos, by_name, programs)
    else:
        sep, edits, piano, report, _tracks = gvm.build(OPTS["smooth"], MIDI)
        if OPTS["probe"]:
            sep = probe_sep(OPTS["smooth"])
        stream, first_delta, resolution, tempo_us = sep_stream(sep)
        bpm = 60000000 // tempo_us
        print(f"velvet score from gen_velvet_music: smoothing {'on' if OPTS['smooth'] else 'off'}")
    buf_name, buf, buf_n = pick_buffer(mem, snd, len(stream))
    print(f"score buffer {buf_name} at {buf:#x} ({buf_n} bytes)")
    frame_wait, step = step_of(resolution, bpm)
    print(f"score {len(stream)} bytes, tempo {bpm} bpm, step {step}, first delta {first_delta}")

    # Drop the title theme's ringing notes before the new score keys its own.
    koff = snd + OFFSETS["SndState.koff"]
    mem.write(koff, struct.pack("<HH", 0xFFFF, 0x00FF))
    voice = snd + OFFSETS["SndState.voice"]
    for v in range(24):
        mem.write(voice + v * OFFSETS["SndVoice"] + OFFSETS["SndVoice.status"], b"\x00")
    magic_before = mem.read(head, 4)
    mem.write(buf, stream)
    if mem.read(head, 4) != magic_before:
        raise SystemExit("the score overwrite hit the VAB header; not resuming")
    if not adventure:
        apply_edits(mem, head, edits)
        install_piano_bytes(mem, snd, proc.pid, piano)
    # pos/start/loop: the first event. delta is tenths of a MIDI tick.
    mem.write(entry + OFFSETS["SndSeq.pos"], struct.pack("<Q", buf))
    mem.write(entry + OFFSETS["SndSeq.start"], struct.pack("<Q", buf))
    mem.write(entry + OFFSETS["SndSeq.loop"], struct.pack("<Q", buf))
    mem.write(entry + OFFSETS["SndSeq.flags"], struct.pack("<I", 1))
    mem.write(entry + OFFSETS["SndSeq.play_mode"], b"\x01")
    mem.write(entry + OFFSETS["SndSeq.status"], b"\x00")
    mem.write(entry + OFFSETS["SndSeq.play_count"], b"\x00")  # 0: endless, the title loop
    mem.write(entry + OFFSETS["SndSeq.played"], b"\x00")
    mem.write(entry + OFFSETS["SndSeq.resolution"], struct.pack("<H", resolution))
    mem.write(entry + OFFSETS["SndSeq.frame_wait"], struct.pack("<h", frame_wait))
    mem.write(entry + OFFSETS["SndSeq.step"], struct.pack("<H", step))
    mem.write(entry + OFFSETS["SndSeq.bpm"], struct.pack("<i", bpm))
    mem.write(entry + OFFSETS["SndSeq.delta"], struct.pack("<i", first_delta * 10))
    mem.write(entry + OFFSETS["SndSeq.vol_l"], struct.pack("<HH", 0x7F, 0x7F))
    for ch in range(16):
        mem.write(entry + OFFSETS["SndSeq.ch_vol"] + 2 * ch, struct.pack("<h", 0x7F))
    # One vsync flushes the key-off; a few more let the score key its first notes.
    # Piano, viola and cello have notes at tick 0; 40 vsyncs is under a second.
    game.step(40)
    sounding = []
    for v in range(24):
        base = voice + v * OFFSETS["SndVoice"]
        if mem.u8(base + OFFSETS["SndVoice.status"]) == 1:
            sounding.append((v, mem.u8(base + OFFSETS["SndVoice.note"]),
                             mem.u8(base + OFFSETS["SndVoice.prog"])))
    opening = []
    for name, ch, _vol, _pan in PARTS:
        opening += [(name, n[1]) for n in by_name[name] if n[3] and n[0] == 0]
    print(f"tick-0 notes: {opening}")
    print(f"voices after 40 vsyncs: {sounding}")
    if OPTS["wav"]:
        render_wav(game, mem, proc, voice, game.status().get("frame", 0))
        return
    game.resume()
    game.close()
    if not sounding:
        print("the sequencer took the score but no voice is keyed yet; the window is running, listen")
    else:
        print(f"live: pid {proc.pid} is playing the score through the title bank. Leave the title screen up.")
    print(f"stop it with: kill {proc.pid}")


if __name__ == "__main__":
    main()
