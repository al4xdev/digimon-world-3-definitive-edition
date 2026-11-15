#!/usr/bin/env python3
"""Hand-drawing Girls Power's frames (port/game/girls_power_art.h) one at a time.

  tools/venv/bin/python tools/girls_power_draw.py edit FILE FRAME     # the drawing file, from the header as it is
  tools/venv/bin/python tools/girls_power_draw.py base FILE FRAME     # redo its pixels: automatic frame + its pastes
  uv run --no-project --with pillow tools/girls_power_draw.py show FILE FRAME [FILE:FRAME ...]
  tools/venv/bin/python tools/girls_power_draw.py build               # the header from every drawing file
  tools/venv/bin/python tools/girls_power_draw.py check               # the built port's frames == the prototype's

FILE is the sprite file's id in hex (187 for P002PLAY), FRAME the frame's sub-file number.

A drawing file (.dumps/tasks/girls-power/draw/<FILE>_<FRAME>.txt, local: it holds disc pixels) is the whole frame as
it should end, one character per pixel: '.' empty, 1-9 A-F an index of the palette the frame ends with (Kaili's in
Junior's files, Junior's in hers). Above the pixels: '#' lines, the comment the header gets; 'paste' lines, pieces of
the other's own frames read from the disc at run time (GP_PASTE: src file, src frame, x, y, w, h, to x, to y, how:
1 mirrored, 2 through the file's colour table); '%' lines, ignored (the column ruler). `build` writes for each frame
its pastes and, as GP_ART, only the pixels that differ from the automatic frame with the pastes on it: the header
holds what was drawn by hand and where pieces of the disc go, never the disc's pixels.

`show` writes .dumps/tasks/girls-power/draw/<FILE>_<FRAME>.png: big, the disc's frame (his own drawing), the
automatic one, the drawing; then the references named (FILE:FRAME, hers in 190 by default); then all of it near the
game's size, the drawing mirrored too (the game mirrors the side directions); then the frame's animation as the
scripts place it, the disc's and the drawn one, also mirrored."""
import re
import struct
import sys
from functools import lru_cache
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import disc_files  # noqa: E402
import girls_power_preview as gp  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
DRAW = ROOT / ".dumps/tasks/girls-power/draw"
HEX = ".123456789ABCDEF"
HEADER = """/* Girls Power's hand-drawn retouches (port/game/girls_power.c, tools/girls_power_preview.py), written by
 * tools/girls_power_draw.py build from its drawing files: what is drawn over a frame after the automatic swap, where
 * the swap leaves something no rule fixes. Each frame's steps go in this order:
 *
 *   GP_PASTE(file id, frame, source file id, source frame, x, y, w, h, to x, to y, how)
 *     the w x h piece at (x, y) of a frame of a disc file (the other's own drawing: a head turned or tilted the way
 *     the pose asks, an arm) put at (to x, to y), its empty pixels left out; how: 1 mirrored, 2 its colours through
 *     the file's table (a piece of his own frames in her colours)
 *   GP_ART(file id, frame, rows)
 *     one string per row of the frame, each as wide as the frame, top to bottom: '.' keeps the pixel, '0' empties
 *     it, 1-9 A-F set its colour, an index of the palette the frame ends with (Kaili's in Junior's files, Junior's in
 *     hers)
 *
 * The pixels here are drawn by hand for this port (the clothes' patterns redrawn in new poses); the disc's own frames
 * are only referenced, by GP_PASTE, and read at run time. */
"""


def path(fid, f, ext="txt"):
    return DRAW / f"{fid:03X}_{f:03d}.{ext}"


@lru_cache(None)
def disc(fid):
    return disc_files.read(fid)


@lru_cache(None)
def final(fid):
    return gp.repaint(fid)[0]


@lru_cache(None)
def auto(fid):
    return gp.repaint(fid, False)[0]


def rows_of(d, f):
    return gp.tim(d, f)[1]


def base(fid, f, pastes):
    o = [r[:] for r in rows_of(auto(fid), f)]
    gp.draw_steps(o, [("paste", *p) for p in pastes], gp.FILES[fid][2])
    return o


def read(fid, f):
    """(comment lines, pastes, rows) of the drawing file"""
    note, pastes, rows = [], [], []
    for line in path(fid, f).read_text().splitlines():
        if line.startswith("#"):
            note.append(line[1:].strip())
        elif line.startswith("paste"):
            pastes.append(tuple(int(v, 0) for v in line.split()[1:]))
        elif line.startswith("%") or not line.strip():
            continue
        else:
            rows.append([HEX.index(c) for c in line.split("%")[0].strip()])
    return note, pastes, rows


def write(fid, f, note, pastes, rows):
    DRAW.mkdir(parents=True, exist_ok=True)
    w = len(rows[0])
    out = ["# " + n for n in note]
    out += ["paste " + " ".join(("0x%X" % v if i == 0 else str(v)) for i, v in enumerate(p)) for p in pastes]
    out += ["%" + "".join(str(x // 10) if x % 10 == 0 else " " for x in range(w)),
            "%" + "".join(str(x % 10) for x in range(w))]
    out += [" " + "".join(HEX[v] for v in r) + "  %%%d" % y for y, r in enumerate(rows)]
    path(fid, f).write_text("\n".join(out) + "\n")


def header_steps():
    """(fid, f) -> (comment lines, pastes) of the header as it is"""
    out = {}
    if not gp.ART.exists():
        return out
    text = gp.ART.read_text()
    for m in re.finditer(r"(?:/\*((?:(?!\*/).)*)\*/\s*)?GP_(PASTE|ART)\(\s*(0x[0-9A-Fa-f]+)\s*,\s*(\d+)\s*,([^\"]*?)"
                         r"((?:\s*\"[^\"]*\")*)\s*\)", text, flags=re.S):
        key = (int(m.group(3), 16), int(m.group(4)))
        note, pastes = out.setdefault(key, ([], []))
        if m.group(1) and not note and "GP_PASTE(file id" not in m.group(1):
            note += [" ".join(m.group(1).split())]
        if m.group(2) == "PASTE":
            pastes.append(tuple(int(v, 0) for v in m.group(5).replace(",", " ").split()))
    return out


def edit(fid, f):
    if path(fid, f).exists():
        print(path(fid, f))
        return
    note, pastes = header_steps().get((fid, f), ([gp.FILES[fid][0] + " %d: " % f], []))
    write(fid, f, note, pastes, rows_of(final(fid), f))
    print(path(fid, f))


def rebase(fid, f):
    note, pastes, _ = read(fid, f)
    write(fid, f, note, pastes, base(fid, f, pastes))


def wrap(text, width=114):
    words, lines, cur = text.split(), [], ""
    for w in words:
        if cur and len(cur) + 1 + len(w) > width:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + " " + w).strip()
    return lines + [cur] if cur else lines


def build():
    out = [HEADER.rstrip("\n")]
    keys = set(header_steps())
    keys |= {(int(p.stem[:3], 16), int(p.stem[4:])) for p in DRAW.glob("*.txt")} if DRAW.exists() else set()
    for fid, f in sorted(keys):
        if not path(fid, f).exists():
            edit(fid, f)  # a header entry with no drawing file yet: its drawing is the header's
        note, pastes, rows = read(fid, f)
        b = base(fid, f, pastes)
        if len(rows) != len(b) or any(len(r) != len(b[0]) for r in rows):
            sys.exit("%s: %dx%d pixels, the frame is %dx%d" % (path(fid, f), len(rows[0]), len(rows), len(b[0]),
                                                               len(b)))
        diff = ["".join("." if v == w else HEX[v] if v else "0" for v, w in zip(r, q)) for r, q in zip(rows, b)]
        if not pastes and all(c == "." for r in diff for c in r):
            continue  # opened and left as it was
        text = " ".join(note)
        out.append("")
        if text:
            lines = wrap(text)
            out.append("/* " + lines[0] + ("" if len(lines) > 1 else " */"))
            out += [" * " + ln for ln in lines[1:]]
            if len(lines) > 1:
                out[-1] += " */"
        for p in pastes:
            out.append("GP_PASTE(0x%X, %d, 0x%X, %s)" % (fid, f, p[0], ", ".join(str(v) for v in p[1:])))
        if any(c != "." for r in diff for c in r):
            out.append("GP_ART(0x%X, %d," % (fid, f))
            out += ['       "%s"' % r for r in diff]
            out[-1] += ")"
    gp.ART.write_text("\n".join(out) + "\n")
    print(gp.ART, len(keys), "frames")


# ---- show ----

BG, CHECK = (36, 36, 52), (44, 44, 62)


def picture(clut, rows, scale, grid=False, mirror=False):
    from PIL import Image, ImageDraw
    H, W = len(rows), len(rows[0])
    im = Image.new("RGB", (W * scale, H * scale), BG)
    dr = ImageDraw.Draw(im)
    for y, r in enumerate(rows):
        for x, v in enumerate(r[::-1] if mirror else r):
            box = [x * scale, y * scale, x * scale + scale - 1, y * scale + scale - 1]
            if v:
                dr.rectangle(box, fill=gp.rgb(clut[v]))
            elif grid and (x + y) % 2:
                dr.rectangle(box, fill=CHECK)
    if grid:
        for x in range(0, W + 1, 4):
            dr.line([x * scale, 0, x * scale, H * scale], fill=(140, 140, 170) if x % 8 == 0 else (80, 80, 110))
        for y in range(0, H + 1, 4):
            dr.line([0, y * scale, W * scale, y * scale], fill=(140, 140, 170) if y % 8 == 0 else (80, 80, 110))
    return im


def labelled(im, text):
    from PIL import Image, ImageDraw
    out = Image.new("RGB", (max(im.width, 8 * len(text)), im.height + 14), BG)
    out.paste(im, (0, 0))
    ImageDraw.Draw(out).text((2, im.height + 1), text, fill=(220, 220, 220))
    return out


def lay(ims, horizontal=True, gap=10):
    from PIL import Image
    ims = [i for i in ims if i is not None]
    if horizontal:
        out = Image.new("RGB", (sum(i.width for i in ims) + gap * (len(ims) - 1), max(i.height for i in ims)), BG)
    else:
        out = Image.new("RGB", (max(i.width for i in ims), sum(i.height for i in ims) + gap * (len(ims) - 1)), BG)
    at = 0
    for i in ims:
        out.paste(i, (at, 0) if horizontal else (0, at))
        at += (i.width if horizontal else i.height) + gap
    return out


def drawn(fid, g):
    """frame g of the file as it will end: its drawing file when it has one, else the header's"""
    return read(fid, g)[2] if path(fid, g).exists() else rows_of(final(fid), g)


def strip(fid, f, d, rows_for, clut, mirror, scale=3):
    """the frame's first animation script (one only its slot uses, when there is one) as the game places it: each
    step in a cell of the same size, at the script's offset"""
    from PIL import Image
    table = gp.FILES[fid][1]
    best = None
    for _, sc in gp.anims(d, table):
        for si in sc:
            if any(g == f for _, g, _, _ in gp.steps(d, si)):
                if best is None or (sc.count(si) == 1 and best[1] > 1):
                    best = (si, sc.count(si))
    if best is None:
        return None
    st = gp.steps(d, best[0])
    boxes = []
    for _, g, x, y in st:
        r = rows_for(g)
        w = len(r[0])
        boxes.append((g, -(x + w) if mirror else x, y, r))
    x0 = min(b[1] for b in boxes)
    y0 = min(b[2] for b in boxes)
    x1 = max(b[1] + len(b[3][0]) for b in boxes)
    y1 = max(b[2] + len(b[3]) for b in boxes)
    cells = []
    for g, x, y, r in boxes:
        cell = Image.new("RGB", ((x1 - x0) * scale, (y1 - y0) * scale), BG)
        cell.paste(picture(clut, r, scale, mirror=mirror), ((x - x0) * scale, (y - y0) * scale))
        if g == f:
            from PIL import ImageDraw
            ImageDraw.Draw(cell).rectangle([0, 0, cell.width - 1, cell.height - 1], outline=(200, 200, 90))
        cells.append(cell)
    return lay(cells, gap=4)


def show(fid, f, refs):
    note, pastes, rows = read(fid, f) if path(fid, f).exists() else ([], [], rows_of(final(fid), f))
    dclut, drows = gp.tim(disc(fid), f)
    nclut = gp.tim(final(fid), f)[0]
    big = lay([labelled(picture(dclut, drows, 14, True), "disc %X:%d" % (fid, f)),
               labelled(picture(nclut, rows_of(auto(fid), f), 14, True), "auto"),
               labelled(picture(nclut, rows, 14, True), "drawing")])
    refs_big, refs_small = [], []
    for r in refs:
        rf, rg = (int(r.split(":")[0], 16), int(r.split(":")[1])) if ":" in r else (0x190, int(r))
        c, px = gp.tim(disc(rf), rg)
        refs_big.append(labelled(picture(c, px, 7, True), "%X:%d" % (rf, rg)))
        refs_small.append(picture(c, px, 3))
    small = lay([picture(dclut, drows, 3), picture(nclut, rows_of(auto(fid), f), 3), picture(nclut, rows, 3),
                 picture(nclut, rows, 3, mirror=True), picture(dclut, drows, 3, mirror=True)] + refs_small, gap=24)
    own = {f: rows}
    parts = [big]
    if refs_big:
        parts.append(lay(refs_big))
    parts.append(small)
    for mirror in (False, True):
        parts.append(strip(fid, f, disc(fid), lambda g: rows_of(disc(fid), g), dclut, mirror))
        parts.append(strip(fid, f, final(fid), lambda g: own.get(g) or drawn(fid, g), nclut, mirror))
    out = path(fid, f, "png")
    DRAW.mkdir(parents=True, exist_ok=True)
    lay(parts, horizontal=False, gap=16).save(out)
    print(out)


def check():
    """build/port/dw2003 run with DW3_GIRLS_DUMP: its eight repainted files must equal the prototype's byte for byte
    (rebuild the port after `build`: the header is compiled in)"""
    import os
    import subprocess
    import tempfile
    bad = 0
    with tempfile.TemporaryDirectory() as out:
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", DW3_GIRLS="1", DW3_GIRLS_DUMP=out)
        subprocess.run([str(ROOT / "build/port/dw2003"), "--disc", str(ROOT / "iso/dw2003.cue"), "--script",
                        str(ROOT / "tests/replay/scripts/first_battle_save.json"), "--max-frames", "300"], env=env,
                       capture_output=True, cwd=str(ROOT), timeout=300)
        for fid in gp.FILES:
            p = Path(out) / f"{fid:03X}.bin"
            if not p.exists():
                print("missing", hex(fid))
                bad += 1
                continue
            c, py = p.read_bytes(), gp.repaint(fid)[0]
            n = sum(1 for a, b in zip(c, py) if a != b)
            if n or len(c) < len(py):
                print("differs", hex(fid), n, "bytes")
                bad += 1
    print("parity:", "ok" if not bad else "FAIL")
    return bad == 0


def main():
    cmd = sys.argv[1]
    if cmd == "build":
        build()
        return
    if cmd == "check":
        sys.exit(0 if check() else 1)
    fid, f = int(sys.argv[2], 16), int(sys.argv[3])
    if cmd == "edit":
        edit(fid, f)
    elif cmd == "base":
        rebase(fid, f)
    elif cmd == "show":
        show(fid, f, sys.argv[4:])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
