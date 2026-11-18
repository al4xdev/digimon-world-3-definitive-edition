#!/usr/bin/env python3
"""Cycle piano timbres on the running title. Each option is copied into the
spare slot (the old noise sample) and program 10 is pointed at it, so the
cello keeps its own sample. The piano stays soloed so the choice is audible.

  tools/venv/bin/python tools/music_lab/velvet/piano.py          # which one is on
  tools/venv/bin/python tools/music_lab/velvet/piano.py next
  tools/venv/bin/python tools/music_lab/velvet/piano.py prev
  tools/venv/bin/python tools/music_lab/velvet/piano.py 4        # jump to option 4
  tools/venv/bin/python tools/music_lab/velvet/piano.py test      # that option, in the full piece
  tools/venv/bin/python tools/music_lab/velvet/piano.py list

Center is held at 66 for every option, so the difference is the timbre, not
the tuning. Samples are the game's own, deduped, 80-250 ms, not a drone and
not noise, and small enough for the spare slot (5824 bytes).
"""
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tests/sound"))
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "psxstack/tools/mcp"))
from game import Game, GameError  # noqa: E402
from play import OFFSETS, SOCK  # noqa: E402
import disc_files  # noqa: E402
import sound_formats  # noqa: E402

HERE = Path(__file__).resolve().parent
CATALOG = HERE / "piano_options.json"
INDEX = HERE / "piano.index"
SLOT_VAG = 14
CENTER = 66
VOL = 96
# vag_end[n] is the SPU address / 8 where vag n ends. Confirmed on this process.
VAG_END_OFF = 172


def pid_of():
    text = (HERE / "session.txt").read_text()
    for line in text.splitlines():
        if line.startswith("pid "):
            return int(line.split()[1])
    raise SystemExit("session.txt sem pid")


def snd_of():
    text = (HERE / "session.txt").read_text()
    for line in text.splitlines():
        if line.startswith("snd "):
            return int(line.split()[1], 16)
    raise SystemExit("session.txt sem snd")


def spu_of(pid):
    exe = Path(f"/proc/{pid}/exe")
    if not exe.exists():
        raise SystemExit("o jogo não está mais aberto")
    for line in subprocess.check_output(["nm", str(exe)], text=True).splitlines():
        parts = line.split()
        if parts and parts[-1] == "spu":
            return int(parts[0], 16)
    raise SystemExit("spu não está no binário que está rodando")


def catalog():
    if CATALOG.is_file():
        return json.loads(CATALOG.read_text())
    exe = disc_files.EXE.read_bytes()
    id_path = {f.id: f.path for f in disc_files.files()}
    seen = {}
    for bid, bank in sound_formats.banks(exe):
        name = id_path[bank["header_file"]].split("/")[-2]
        vh = sound_formats.parse_vh(sound_formats.sub(disc_files.read(bank["header_file"]), bank["vab_header"]))
        body = sound_formats.sub(disc_files.read(bank["body_file"]), bank["vab_body"])
        off = 0
        raw = {}
        for i, sz in enumerate(vh["vag_sizes"]):
            if i and sz:
                raw[i] = body[off:off + sz]
            off += sz
        used = set()
        for prog, tone, fields in vh["tone_recs"]:
            if tone >= vh["progs"][prog][0]:
                continue
            vag = fields[19]
            if vag in used or vag not in raw:
                continue
            used.add(vag)
            if not (fields[6] <= 60 and fields[7] >= 72):
                continue
            blob = raw[vag]
            if not 80 <= len(blob) / 16 * 28 / 44.1 <= 250 or len(blob) > 5824:
                continue
            digest = hashlib.sha1(blob).hexdigest()
            if digest in seen:
                continue
            # A few blocks are enough to drop drones and noise. The full sample is stored.
            zc = _zc(blob)
            if not 20 <= zc <= 160:
                continue
            seen[digest] = {
                "bank": name, "prog": prog, "vag": vag, "bytes": len(blob),
                "ms": round(len(blob) / 16 * 28 / 44.1), "zc": round(zc, 1),
                "file": bank["body_file"], "sub": bank["vab_body"],
            }
    # Darker plucks first; the title bank's own samples stay in the list.
    options = sorted(seen.values(), key=lambda r: (r["zc"], r["bank"]))
    CATALOG.write_text(json.dumps(options))
    # The bytes are not in the json: re-read from the disc on apply. The catalog
    # is only the index, so a rebuild of the list does not copy game audio into git.
    return options


def _zc(vag):
    pos, neg = (0, 60, 115, 98, 122), (0, 0, -52, -55, -60)
    s, h1, h2, n = [], 0, 0, 0
    for o in range(0, len(vag) - 15, 16):
        shift, filt, fl = vag[o] & 0xF, (vag[o] >> 4) & 7, vag[o + 1]
        if shift > 12:
            shift = 9
        f = min(filt, 4)
        for i in range(2, 16):
            b = vag[o + i]
            for nib in (b & 0xF, b >> 4):
                if nib >= 8:
                    nib -= 16
                sample = max(-32768, min(32767, ((nib << 12) >> shift) + ((h1 * pos[f] + h2 * neg[f] + 32) >> 6)))
                s.append(sample)
                h2, h1 = h1, sample
        n += 1
        if fl & 1 or n >= 6:
            break
    loud = s[:max(1, len(s) // 3)]
    return sum(1 for a, b in zip(loud, loud[1:]) if (a >= 0) != (b >= 0)) / max(1, len(loud)) * 1000


def sample_bytes(opt):
    exe = disc_files.EXE.read_bytes()
    banks = dict(sound_formats.banks(exe))
    # file/sub identify the body; find the bank by those ids.
    for _bid, bank in banks.items():
        if bank["body_file"] == opt["file"] and bank["vab_body"] == opt["sub"]:
            vh = sound_formats.parse_vh(sound_formats.sub(disc_files.read(bank["header_file"]), bank["vab_header"]))
            body = sound_formats.sub(disc_files.read(bank["body_file"]), bank["vab_body"])
            off = 0
            for i, sz in enumerate(vh["vag_sizes"]):
                if i == opt["vag"]:
                    return body[off:off + sz]
                off += sz
            break
    raise SystemExit(f"sample sumiu: {opt['bank']} vag {opt['vag']}")


def apply(n, solo=True):
    options = catalog()
    if not options:
        raise SystemExit("nenhum sample coube no lugar do piano")
    n = n % len(options)
    opt = options[n]
    pid = pid_of()
    snd = snd_of()
    spu = spu_of(pid)
    game = Game.attach(str(SOCK))
    try:
        game.pause()
        vab = snd + OFFSETS["SndState.vab"] + OFFSETS["SndVab"]
        head = struct.unpack("<Q", game.peek(vab + OFFSETS["SndVab.head"], 8))[0]
        vag_end = struct.unpack("<256H", game.peek(vab + VAG_END_OFF, 512))
        slot = vag_end[SLOT_VAG - 1] * 8
        slot_end = vag_end[SLOT_VAG] * 8
        blob = sample_bytes(opt)
        if len(blob) > slot_end - slot:
            raise SystemExit(f"{opt['bank']} não cabe no vão ({len(blob)} > {slot_end - slot})")
        game.poke(spu + slot, blob)
        # Program 10's tone: vol, center, vag. The score keeps saying program 10.
        programs = struct.unpack("<H", game.peek(head + 0x12, 2))[0]
        hdr = game.peek(head, 0x20 + 0x800 + programs * 0x200)
        used = [i for i in range(128) if hdr[0x20 + 16 * i]]
        tone = None
        for k, prog in enumerate(used[:programs]):
            if prog == 10:
                tone = head + 0x20 + 0x800 + 0x200 * k
                break
        if tone is None:
            raise SystemExit("programa 10 não está no banco carregado")
        game.poke(tone + 2, bytes([VOL]))
        game.poke(tone + 4, bytes([CENTER]))
        game.poke(tone + 5, bytes([0]))  # no fine tune, so the options compare fairly
        game.poke(tone + 0x16, struct.pack("<h", SLOT_VAG))
        entry = snd + OFFSETS["SndState.seq"] + 34 * OFFSETS["SndSeq"]
        # Solo keeps the other seven quiet. test clears the mask: the score is
        # already running, so the other parts come in on their next notes.
        mask = (0xFF ^ (1 << 2)) if solo else 0
        game.poke(entry + OFFSETS["SndSeq.mute"], struct.pack("<H", mask))
        game.step(2)
        game.resume()
    except GameError as e:
        raise SystemExit(f"debug: {e}") from e
    finally:
        game.close()
    INDEX.write_text(str(n))
    (HERE / "mute.mask").write_text(f"0x{mask:02x}\n")
    print(f"{n + 1}/{len(options)}  {opt['bank']}  {opt['ms']} ms  brilho {opt['zc']}")
    print("piano solo. next, prev, ou um número." if solo else "música completa, com esse piano.")


def main(argv):
    options = catalog()
    cur = int(INDEX.read_text()) if INDEX.is_file() else 0
    cmd = argv[1] if len(argv) > 1 else "status"
    if cmd == "list":
        for i, opt in enumerate(options):
            mark = ">" if i == cur else " "
            print(f"{mark} {i + 1:3} {opt['bank']:8} {opt['ms']:3} ms  brilho {opt['zc']}")
        return
    if cmd == "status":
        opt = options[cur % len(options)]
        print(f"{cur + 1}/{len(options)}  {opt['bank']}  {opt['ms']} ms  brilho {opt['zc']}")
        print(f"{len(options)} opções. next para ouvir a seguinte.")
        return
    if cmd == "test":
        apply(cur, solo=False)
        return
    if cmd == "next":
        apply(cur + 1 if INDEX.is_file() else 0)
        return
    if cmd == "prev":
        apply(cur - 1)
        return
    if cmd.isdigit():
        n = int(cmd)
        if not 1 <= n <= len(options):
            raise SystemExit(f"número de 1 a {len(options)}")
        apply(n - 1)
        return
    raise SystemExit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
