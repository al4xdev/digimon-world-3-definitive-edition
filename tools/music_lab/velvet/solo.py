#!/usr/bin/env python3
"""Mute the live score one part at a time, on the game that play.py already started.

The sequencer ignores a channel whose bit is set in SndSeq.mute. Notes already
ringing are keyed off, or a mute would only catch the next attack.

  tools/venv/bin/python tools/music_lab/velvet/solo.py            # who is audible
  tools/venv/bin/python tools/music_lab/velvet/solo.py next       # mute the next part (1, then 2, ...)
  tools/venv/bin/python tools/music_lab/velvet/solo.py mute 3     # mute just that one, leave the rest
  tools/venv/bin/python tools/music_lab/velvet/solo.py solo 3     # only that one
  tools/venv/bin/python tools/music_lab/velvet/solo.py reset      # all eight back on
  tools/venv/bin/python tools/music_lab/velvet/solo.py vol          # the eight levels
  tools/venv/bin/python tools/music_lab/velvet/solo.py vol 2 70     # oboe at 70 (0-100)
  tools/venv/bin/python tools/music_lab/velvet/solo.py rem 3        # tira o piano; repete para ir tirando
  tools/venv/bin/python tools/music_lab/velvet/solo.py rem 3 5 6    # tira vários de uma vez
  tools/venv/bin/python tools/music_lab/velvet/solo.py add 3        # devolve o piano

Numbers are 1..8, the order play.py assigned. 100 is full, 0 is silent.
The level is written into the score, so the loop does not put the old one back.
"""
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "psxstack/tools/mcp"))
from game import Game, GameError  # noqa: E402
from play import OFFSETS, PARTS, SOCK, VOLUMES, level_of, midi_of, playing  # noqa: E402

HERE = Path(__file__).resolve().parent
MASK = HERE / "mute.mask"
MUTE_OFF = OFFSETS["SndSeq.mute"]
CH_OFF = OFFSETS["SndVoice.channel"]
STAT_OFF = OFFSETS["SndVoice.status"]


def names():
    return [p[0] for p in PARTS]


def load_mask():
    if not MASK.is_file():
        return 0
    try:
        return int(MASK.read_text().strip(), 0) & 0xFF
    except ValueError:
        return 0


def save_mask(mask):
    MASK.write_text(f"0x{mask:02x}\n")


def describe(mask=None):
    if mask is None:
        mask = load_mask()
    lines = []
    for i, (name, _ch, default, _pan) in enumerate(PARTS):
        state = "MUDO" if mask & (1 << i) else "soa"
        lines.append(f"  {i + 1} {name:12} {state:4} {level_of(i, default):3}")
    return "\n".join(lines)


def music_entry(game, snd):
    class Mem:
        def __init__(self, g):
            self.g = g

        def u8(self, addr):
            return self.g.peek(addr, 1)[0]

        def u32(self, addr):
            return struct.unpack("<I", self.g.peek(addr, 4))[0]

        def u64(self, addr):
            return struct.unpack("<Q", self.g.peek(addr, 8))[0]

    rows = playing(Mem(game), snd)
    music = [r for r in rows if r[2] > 0]
    if not music:
        raise SystemExit("nenhuma sequência de música tocando; o título ainda está de pé?")
    return music[0][1]


def key_off_muted(game, snd, mask):
    """Key off voices whose channel is muted, so the note stops now, not at its release."""
    voice = snd + OFFSETS["SndState.voice"]
    lo = hi = 0
    for v in range(24):
        base = voice + v * OFFSETS["SndVoice"]
        raw = game.peek(base, 32)
        if raw[STAT_OFF] != 1:
            continue
        ch = struct.unpack_from("<h", raw, CH_OFF)[0]
        if ch < 0 or ch > 7 or not (mask & (1 << ch)):
            continue
        game.poke(base + STAT_OFF, b"\x00")
        if v < 16:
            lo |= 1 << v
        else:
            hi |= 1 << (v - 16)
    if lo or hi:
        game.poke(snd + OFFSETS["SndState.koff"], struct.pack("<HH", lo, hi))


def live_snd():
    """The running process, not nm. A rebuild moves snd in the file; the open game does not."""
    text = (HERE / "session.txt").read_text() if (HERE / "session.txt").is_file() else ""
    for line in text.splitlines():
        if line.startswith("snd "):
            return int(line.split()[1], 16)
    raise SystemExit("session.txt sem o endereço do snd; rode play.py de novo")


def apply(mask):
    if not SOCK.exists():
        raise SystemExit(f"jogo não está no socket {SOCK}; rode play.py primeiro")
    game = Game.attach(str(SOCK))
    try:
        snd = live_snd()
        game.pause()
        entry = music_entry(game, snd)
        game.poke(entry + MUTE_OFF, struct.pack("<H", mask & 0xFF))
        key_off_muted(game, snd, mask)
        game.step(2)
        game.resume()
    except GameError as e:
        raise SystemExit(f"debug: {e}") from e
    finally:
        game.close()
    save_mask(mask)
    print(describe())


def set_volume(n, level):
    """Channel volume 0-100. Patches the score's CC7 and the live channel volume."""
    if not 0 <= level <= 100:
        raise SystemExit("nível de 0 a 100")
    if not SOCK.exists():
        raise SystemExit(f"jogo não está no socket {SOCK}; rode play.py primeiro")
    ch = n - 1
    midi = midi_of(level)
    game = Game.attach(str(SOCK))
    try:
        snd = live_snd()
        game.pause()
        entry = music_entry(game, snd)
        start = struct.unpack("<Q", game.peek(entry + OFFSETS["SndSeq.start"], 8))[0]
        window = game.peek(start, 128)
        mark = bytes((0xB0 | ch, 7))
        at = window.find(mark)
        if at < 0:
            raise SystemExit(f"CC7 do canal {ch} não está no começo da partitura")
        game.poke(start + at + 2, bytes([midi]))
        game.poke(entry + OFFSETS["SndSeq.ch_vol"] + 2 * ch, struct.pack("<h", midi))
        # Held notes keep the old level; key them off so the next attack uses this one.
        voice = snd + OFFSETS["SndState.voice"]
        lo = hi = 0
        for v in range(24):
            base = voice + v * OFFSETS["SndVoice"]
            raw = game.peek(base, 32)
            if raw[STAT_OFF] != 1:
                continue
            if struct.unpack_from("<h", raw, CH_OFF)[0] != ch:
                continue
            game.poke(base + STAT_OFF, b"\x00")
            if v < 16:
                lo |= 1 << v
            else:
                hi |= 1 << (v - 16)
        if lo or hi:
            game.poke(snd + OFFSETS["SndState.koff"], struct.pack("<HH", lo, hi))
        game.step(2)
        game.resume()
    except GameError as e:
        raise SystemExit(f"debug: {e}") from e
    finally:
        game.close()
    levels = [level_of(i, PARTS[i][2]) for i in range(8)]
    levels[ch] = level
    VOLUMES.write_text(json.dumps(levels))
    print(f"{n} {names()[ch]} volume {level}")


def show_volumes():
    print(describe())


def part_numbers(args):
    if not args or not all(a.isdigit() for a in args):
        raise SystemExit("números de 1 a 8, por exemplo: rem 3 5")
    nums = [int(a) for a in args]
    if any(n < 1 or n > 8 for n in nums):
        raise SystemExit("instrumento de 1 a 8")
    return nums


def main(argv):
    cmd = argv[1] if len(argv) > 1 else "status"
    mask = load_mask()
    if cmd in ("status", "lista"):
        print(describe())
        return
    if cmd == "rem":
        bits = 0
        for n in part_numbers(argv[2:]):
            bits |= 1 << (n - 1)
            print(f"tirando {n} {names()[n - 1]}")
        apply(mask | bits)
        return
    if cmd == "add":
        bits = 0
        for n in part_numbers(argv[2:]):
            bits |= 1 << (n - 1)
            print(f"devolvendo {n} {names()[n - 1]}")
        apply(mask & ~bits)
        return
    if cmd == "vol" and len(argv) == 2:
        show_volumes()
        return
    if cmd == "vol" and len(argv) == 4 and argv[2].isdigit() and argv[3].isdigit():
        n, level = int(argv[2]), int(argv[3])
        if not 1 <= n <= 8:
            raise SystemExit("instrumento de 1 a 8")
        set_volume(n, level)
        return
    if cmd == "reset":
        apply(0)
        return
    if cmd == "next":
        for i in range(8):
            if not (mask & (1 << i)):
                print(f"calando {i + 1} {names()[i]}")
                apply(mask | (1 << i))
                return
        print("os 8 já estão mudos; reset para voltar")
        return
    if cmd in ("mute", "solo") and len(argv) == 3 and argv[2].isdigit():
        n = int(argv[2])
        if not 1 <= n <= 8:
            raise SystemExit("número de 1 a 8")
        bit = 1 << (n - 1)
        if cmd == "mute":
            apply(mask | bit)
        else:
            apply(0xFF ^ bit)
        return
    raise SystemExit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
