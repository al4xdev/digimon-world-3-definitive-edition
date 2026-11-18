#!/usr/bin/env python3
"""Cycle guitar timbres on the adventure window. Same idea as piano.py: each
option is copied into the spare slot and the guitar's program (5, channel 1)
is pointed at it. The other parts keep their samples.

  tools/venv/bin/python tools/music_lab/adventure/guitar.py next
  tools/venv/bin/python tools/music_lab/adventure/guitar.py 4
  tools/venv/bin/python tools/music_lab/adventure/guitar.py test
  tools/venv/bin/python tools/music_lab/adventure/guitar.py list

Center stays 64, near the riff (notes 52-80). The 51 options are the same
tonal samples piano.py uses.
"""
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
VELVET = HERE.parent / "velvet"
sys.path.insert(0, str(VELVET))
sys.path.insert(0, str(HERE.parents[2] / "psxstack/tools/mcp"))
from game import Game, GameError  # noqa: E402
from piano import CENTER, SLOT_VAG, VAG_END_OFF, VOL, catalog, sample_bytes, spu_of  # noqa: E402
from play import OFFSETS  # noqa: E402

SOCK = Path("/tmp/adventure-music.sock")
INDEX = HERE / "guitar.index"
PROG = 5
CHANNEL = 1
# The riff sits a little lower than the Velvet piano. 64, not 66.
GUITAR_CENTER = 64


def session():
    text = (HERE / "session.txt").read_text()
    pid = snd = None
    for line in text.splitlines():
        if line.startswith("pid "):
            pid = int(line.split()[1])
        elif line.startswith("snd "):
            snd = int(line.split()[1], 16)
    if pid is None or snd is None:
        raise SystemExit("session.txt sem pid/snd; rode play.py adventure")
    return pid, snd


def rewind(game, snd, entry):
    """Back to the score start. Only the pointer play.py stored, never a mid-stream seek."""
    start = struct.unpack("<Q", game.peek(entry + OFFSETS["SndSeq.start"], 8))[0]
    if game.peek(start, 2) != b"\xff\x51":
        raise SystemExit("o começo da partitura não está onde deveria; não vou saltar")
    game.poke(entry + OFFSETS["SndSeq.pos"], struct.pack("<Q", start))
    game.poke(entry + OFFSETS["SndSeq.loop"], struct.pack("<Q", start))
    game.poke(entry + OFFSETS["SndSeq.flags"], struct.pack("<I", 1))
    game.poke(entry + OFFSETS["SndSeq.play_mode"], b"\x01")
    game.poke(entry + OFFSETS["SndSeq.status"], b"\x00")
    game.poke(entry + OFFSETS["SndSeq.play_count"], b"\x00")
    game.poke(entry + OFFSETS["SndSeq.played"], b"\x00")
    game.poke(entry + OFFSETS["SndSeq.frame_wait"], struct.pack("<h", -1))
    game.poke(entry + OFFSETS["SndSeq.step"], struct.pack("<H", 264))
    game.poke(entry + OFFSETS["SndSeq.bpm"], struct.pack("<i", 165))
    game.poke(entry + OFFSETS["SndSeq.delta"], struct.pack("<i", 0))
    voice = snd + OFFSETS["SndState.voice"]
    lo = hi = 0
    for v in range(24):
        base = voice + v * OFFSETS["SndVoice"]
        if game.peek(base + OFFSETS["SndVoice.status"], 1)[0] == 1:
            game.poke(base + OFFSETS["SndVoice.status"], b"\x00")
            if v < 16:
                lo |= 1 << v
            else:
                hi |= 1 << (v - 16)
    if lo or hi:
        game.poke(snd + OFFSETS["SndState.koff"], struct.pack("<HH", lo, hi))


def apply(n, solo=True):
    options = catalog()
    n = n % len(options)
    opt = options[n]
    pid, snd = session()
    spu = spu_of(pid)
    game = Game.attach(str(SOCK))
    try:
        game.pause()
        vab = snd + OFFSETS["SndState.vab"] + OFFSETS["SndVab"]
        head = struct.unpack("<Q", game.peek(vab + OFFSETS["SndVab.head"], 8))[0]
        vag_end = struct.unpack("<256H", game.peek(vab + VAG_END_OFF, 512))
        slot, slot_end = vag_end[SLOT_VAG - 1] * 8, vag_end[SLOT_VAG] * 8
        blob = sample_bytes(opt)
        if len(blob) > slot_end - slot:
            raise SystemExit(f"{opt['bank']} não cabe ({len(blob)} > {slot_end - slot})")
        game.poke(spu + slot, blob)
        programs = struct.unpack("<H", game.peek(head + 0x12, 2))[0]
        hdr = game.peek(head, 0x20 + 0x800 + programs * 0x200)
        used = [i for i in range(128) if hdr[0x20 + 16 * i]]
        tone = None
        for k, prog in enumerate(used[:programs]):
            if prog == PROG:
                tone = head + 0x20 + 0x800 + 0x200 * k
                break
        if tone is None:
            raise SystemExit(f"programa {PROG} não está no banco carregado")
        game.poke(tone + 2, bytes([VOL]))
        game.poke(tone + 4, bytes([GUITAR_CENTER]))
        game.poke(tone + 5, bytes([0]))
        game.poke(tone + 0x16, struct.pack("<h", SLOT_VAG))
        entry = snd + OFFSETS["SndState.seq"] + 34 * OFFSETS["SndSeq"]
        mask = (0xFF ^ (1 << CHANNEL)) if solo else 0
        game.poke(entry + OFFSETS["SndSeq.mute"], struct.pack("<H", mask))
        rewind(game, snd, entry)
        game.step(2)
        game.resume()
    except GameError as e:
        raise SystemExit(f"debug: {e}") from e
    finally:
        game.close()
    INDEX.write_text(str(n))
    print(f"{n + 1}/{len(options)}  {opt['bank']}  {opt['ms']} ms  brilho {opt['zc']}")
    print("do começo. guitarra solo. next, prev, ou um número." if solo else "do começo, música completa, com essa guitarra.")


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
