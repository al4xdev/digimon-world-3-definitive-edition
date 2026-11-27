#!/usr/bin/env python3
"""Hand-drawing Girls Power's frames (port/game/girls_power_art.h): the one tool for the whole flow.

Looking (FILE is the sprite file's id in hex, 187 for P002PLAY; FRAME the frame's sub-file number):
  anims FILE                      every animation: per direction slot, its script's frames, their times and offsets
  text SPEC ...                   frames side by side as text; SPEC is FILE:FRAME, with d/ (the disc's frame, the
                                  default), a/ (the automatic repaint) or f/ (the drawing) before it
  refs NAME SPEC ...              the same frames big with a grid -> review/NAME.png
  show FILE FRAME [FILE:FRAME ...]  one frame's review picture (disc, automatic, drawing, references, its animation)
  moves FILE FRAMES [--base FILE:FRAME] [--rows N]
                                  what a pose does to the head, measured on the disc against the standing frame of
                                  the same direction (or --base): how far the head moved, whether it tilts (rows
                                  shifting apart), which of its pixels change (hair bouncing, a blink), and where
                                  the figure ends; FRAMES like 42-47 or 42,45

Drawing (a set is sets/NAME.txt: the frames of one animation and direction, typed pixel by pixel; see SET below):
  set NAME [--open] [--slow N]    review/NAME.png (Junior's frames, the drawn ones big, both small and mirrored) and
                                  review/NAME.gif (Junior, Kaili as she was, Kaili drawn, at the game's pace; --slow N
                                  times slower); --open shows a numbered copy of the GIF. Nothing else is written:
                                  the one to review.
  lean NAME                       per frame, where his and her head, torso and legs sit across (the centre of each
                                  band: top 40%, next 30%, the rest), without her pigtails: a body running upright
                                  or on tiptoe shows as legs under the torso where his are behind it
  far OUT NAME... [--open] [--scale N] [--slow N] [--no-game] [--mp4]
                                  review/OUT.gif seen from afar (N times the game's size, 3 by default), one column
                                  per set: Junior, Kaili as the game has her now, Kaili as the set draws her, each
                                  cycling at the game's pace. Small is how pixel art is judged: the lean of a body,
                                  a foot that reads as a tiptoe, show at a glance there and hide up close.
                                  --slow N holds each pose N times longer: for a model that watches video, which
                                  samples a second or two at a time and would miss most of a fast cycle;
                                  --no-game leaves out the row of Kaili as the game has her (only the reference
                                  and the new drawing, nothing to mix up); --mp4 also writes review/OUT.mp4 for a
                                  model that watches video: 60 fps, each pose held its vsyncs as in the game, three
                                  cycles at the game's speed and then two at a quarter of it, scaled up without
                                  smoothing (nearest neighbour), the speed written in the corner
  save NAME                       the set's drawing files (draw/FILE_FRAME.txt, the old ones kept in
                                  backup/), with the recipe: what equals a frame of the disc becomes a `paste` line
  edit FILE FRAME                 a drawing file from the header as it is (to touch one frame by hand)
  base FILE FRAME                 redo a drawing's pixels: the automatic frame + its pastes
  build                           the header from every drawing file
  check                           the built port's frames == the prototype's
  make                            build, compile both ports, check, tests/port/girls_power.py, tests/port/run.py

Everything local is under .dumps/tasks/girls-power/ (gitignored: it holds disc pixels): sets/, review/, draw/,
backup/. Pictures need Pillow: the tool runs itself again under `uv run --with pillow` when it is missing.

SET: lines of a set file. `#` a comment. `file 187`. `also 186`: `save` writes the frames to that file too, cut
to its frames (the same picture at the same place on screen). `part NAME` and then its pixel rows. `frame 42 the note the
header gets` and then, in order, `use PART X Y [m]` (a part put at X, Y; m mirrored, the rows as wide as the widest)
and `draw X Y` with pixel rows after it. A pixel row is `.` (leaves the pixel), `0` (empties it) and 1-9 A-F (an
index of the palette the frame ends with: Kaili's in Junior's files, Junior's in hers). A frame starts empty: the set
is the whole frame, nothing of the automatic one.

The recipe (`save`): the header must never hold the disc's pixels, and a head that the pose does not change is drawn
exactly as the disc has it. So where a piece of the drawing equals a piece of one of the other's own frames, the
drawing file gets a `paste` line (the piece is read from the player's disc when the game loads the file) and the
header only the pixels that are new. The frame in the game is the drawing, pixel for pixel, either way.

A drawing file is the whole frame as it should end, one character per pixel: '.' empty, 1-9 A-F an index. Above the
pixels: '#' lines, the comment the header gets; 'paste' lines (GP_PASTE: src file, src frame, x, y, w, h, to x, to y,
how: 1 mirrored, 2 through the file's colour table); '%' lines, ignored (the column ruler). `build` writes for each
frame its pastes and, as GP_ART, only the pixels that differ from the automatic frame with the pastes on it."""
import os
import re
import struct
import subprocess
import sys
from functools import lru_cache
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import disc_files  # noqa: E402
import girls_power_preview as gp  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / ".dumps/tasks/girls-power"
DRAW = WORK / "draw"
SETS = WORK / "sets"
REVIEW = WORK / "review"
BACKUP = WORK / "backup"
FIELD = (96, 120, 88)  # a field-like green behind the game-size pictures
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


# ---- looking ----

def need_pillow():
    """pictures need Pillow; without it the tool runs itself again under uv (once)"""
    try:
        import PIL  # noqa: F401
    except ImportError:
        if os.environ.get("GP_DRAW_UV"):
            sys.exit("Pillow is missing and `uv run --with pillow` did not provide it")
        os.execvpe("uv", ["uv", "run", "--no-project", "--with", "pillow", str(Path(__file__).resolve())]
                   + sys.argv[1:], dict(os.environ, GP_DRAW_UV="1"))


def cmd_anims(fid):
    d = disc(fid)
    for a, sc in gp.anims(d, gp.FILES[fid][1]):
        print("anim", a)
        for k, si in enumerate(sc):
            if k and si == sc[k - 1]:
                print("  slot %d = slot %d" % (k, k - 1))
                continue
            st = [e for e in gp.script(d, si) if e[0] > 0]
            end = gp.script(d, si)[-1][0]
            print("  slot %d script %d (%s): %s" % (k, si, "loops" if end == 0 else "ends",
                                                  " ".join("%d(%dt)@%d,%d" % (f, t, x, y) for t, f, x, y in st)))


def where(d, table, f):
    """(slot, its script's x, y) of the first script that draws frame f"""
    for _, sc in gp.anims(d, table):
        for k, si in enumerate(sc):
            for t, g, x, y in gp.script(d, si):
                if g == f and t > 0:
                    return k, x, y
    return None


def standing(d, table, slot):
    """the first frame of the standing animation (1) in that direction slot, with its script's x, y"""
    for a, sc in gp.anims(d, table):
        if a == 1 and slot < len(sc):
            t, f, x, y = next(e for e in gp.script(d, sc[slot]) if e[0] > 0)
            return f, x, y
    return None


def extent(rows):
    xs = [x for r in rows for x, v in enumerate(r) if v]
    ys = [y for y, r in enumerate(rows) if any(r)]
    return min(xs), max(xs), min(ys), max(ys)


def cmd_moves(fid, frames, base=None, nrows=13):
    """the head of each frame against the standing head: moved by (dx, dy) in screen terms (script offsets
    counted), tilted or not (each row's own best shift: a lean shows as the top rows shifting more than the bottom),
    the pixels that change, and the figure's extent"""
    d, table = disc(fid), gp.FILES[fid][1]
    for f in frames:
        loc = where(d, table, f) or (None, 0, 0)
        if base:
            bfid, bf = base
            bloc = where(disc(bfid), gp.FILES[bfid][1], bf) or (None, 0, 0)
            B, bx, by = rows_of(disc(bfid), bf), bloc[1], bloc[2]
        else:
            st = standing(d, table, loc[0]) if loc[0] is not None else None
            if not st:
                sys.exit("frame %d: no standing frame in its direction; give one with --base FILE:FRAME" % f)
            bf, bx, by = st
            B = rows_of(d, bf)
        R = rows_of(d, f)
        top = extent(B)[2]
        head = [y for y in range(top, min(top + nrows, len(B)))]
        best = None
        for dy in range(-6, 13):
            for dx in range(-8, 9):
                m = sum(1 for y in head for x, v in enumerate(B[y]) if v and 0 <= y + dy < len(R)
                        and 0 <= x + dx < len(R[0]) and R[y + dy][x + dx] == v)
                if best is None or m > best[0]:
                    best = (m, dx, dy)
        m, dx, dy = best
        n = sum(1 for y in head for v in B[y] if v)
        shifts = []
        for y in head:
            sc = {k: sum(1 for x, v in enumerate(B[y]) if v and 0 <= y + dy < len(R) and 0 <= x + dx + k < len(R[0])
                         and R[y + dy][x + dx + k] == v) for k in range(-3, 4)}
            shifts.append(max(sc, key=lambda k: (sc[k], -abs(k))))
        sx, sy = dx + loc[1] - bx, dy + loc[2] - by
        print("frame %d (standing %d): head moved %+d,%+d on screen%s; %d of its %d pixels the same; %s" % (
            f, bf, sx, sy, "" if (sx, sy) == (dx, dy) else " (%+d,%+d in the frame)" % (dx, dy), m, n,
            "no tilt (every row moves together)" if not any(shifts) else
            "rows shift apart (tilt?): " + " ".join("%+d" % k for k in shifts)))
        for y in head:
            a = "".join(HEX[v] if v else "." for v in B[y])
            b = "".join(HEX[R[y + dy][x + dx]] if 0 <= y + dy < len(R) and 0 <= x + dx < len(R[0])
                        and R[y + dy][x + dx] else "." for x in range(len(B[y])))
            if a != b:
                print("  row %2d standing %s\n         posed    %s\n                  %s" % (
                    y + dy, a, b, "".join("^" if p != q else " " for p, q in zip(a, b))))
        e, eb = extent(R), extent(B)
        print("  figure: columns %d-%d, rows %d-%d (standing %d-%d, %d-%d; frame rows, scripts at y %d and %d)" % (
            e[0], e[1], e[2], e[3], eb[0], eb[1], eb[2], eb[3], loc[2], by))


def frame_list(s):
    out = []
    for part in s.split(","):
        a, _, b = part.partition("-")
        out += list(range(int(a), int(b or a) + 1))
    return out


def spec(s):
    """'d/187:42' -> (label, clut, rows): the disc's frame, a/ the automatic one, f/ the drawing"""
    kind, rest = s.split("/") if "/" in s else ("d", s)
    fid, f = int(rest.split(":")[0], 16), int(rest.split(":")[1])
    if kind == "d":
        clut, rows = gp.tim(disc(fid), f)
    elif kind == "a":
        clut, rows = gp.tim(auto(fid), f)
    else:
        clut, rows = gp.tim(final(fid), f)[0], drawn(fid, f)
    return "%s/%X:%d" % (kind, fid, f), clut, rows


def cmd_text(specs):
    cols = [(n, ["".join(HEX[v] for v in r) for r in rows]) for n, _, rows in map(spec, specs)]
    h = max(len(c[1]) for c in cols)
    print("   " + "  ".join(c[0].ljust(len(c[1][0])) for c in cols))
    print("   " + "  ".join("".join(str(x % 10) for x in range(len(c[1][0]))) for c in cols))
    for y in range(h):
        print("%2d " % y + "  ".join(c[1][y] if y < len(c[1]) else " " * len(c[1][0]) for c in cols))


def cmd_refs(name, specs, scale=12):
    need_pillow()
    REVIEW.mkdir(parents=True, exist_ok=True)
    out = REVIEW / (name + ".png")
    lay([labelled(picture(c, rows, scale, True), n) for n, c, rows in map(spec, specs)]).save(out)
    print(out)


# ---- sets ----

PIXELS = re.compile(r"^[.0-9A-F]+$")


def read_set(name, skip=None):
    """(file id, [(frame, note, rows)]) of sets/NAME.txt; skip: leave out the parts whose name starts with it"""
    p = SETS / (name + ".txt")
    fid, parts, frames, rows, cur = None, {}, [], None, None

    def fail(n, why):
        sys.exit("%s:%d: %s" % (p, n, why))

    for n, line in enumerate(p.read_text().splitlines(), 1):
        line = line.split("#")[0].rstrip()
        if not line.strip():
            continue
        w = line.split()
        if PIXELS.match(line.strip()):
            if rows is None:
                fail(n, "pixels before a `part` or a `draw`")
            rows.append(line.strip())
        elif w[0] == "file":
            fid = int(w[1], 16)
        elif w[0] == "also":
            pass  # the other files the frames go to: also_of()
        elif w[0] == "part":
            rows = parts.setdefault(w[1], [])
        elif w[0] == "frame":
            cur = (int(w[1]), " ".join(w[2:]), [])
            frames.append(cur)
            rows = None
        elif w[0] in ("use", "draw"):
            if cur is None:
                fail(n, "`%s` before a `frame`" % w[0])
            if w[0] == "use":
                if w[1] not in parts:
                    fail(n, "no part named " + w[1])
                if skip and w[1].startswith(skip):
                    rows = None
                    continue
                rs = parts[w[1]]
                if w[4:5] == ["m"]:  # mirrored: one side's pose drawn once, the other side's taken from it
                    rs = [r.ljust(max(map(len, rs)), ".")[::-1] for r in rs]
                cur[2].append((int(w[2]), int(w[3]), rs))
                rows = None
            else:
                rows = []
                cur[2].append((int(w[1]), int(w[2]), rows))
        else:
            fail(n, "not a pixel row nor a known word: " + w[0])
    if fid is None:
        sys.exit("%s: no `file` line" % p)
    out = []
    for f, note, stamps in frames:
        b = rows_of(disc(fid), f)
        o = [[0] * len(b[0]) for _ in b]
        for x0, y0, rs in stamps:
            for dy, r in enumerate(rs):
                for dx, c in enumerate(r):
                    if c == ".":
                        continue
                    if not (0 <= x0 + dx < len(o[0]) and 0 <= y0 + dy < len(o)):
                        sys.exit("%s: frame %d: a pixel at %d,%d, outside the %dx%d frame"
                                 % (p, f, x0 + dx, y0 + dy, len(o[0]), len(o)))
                    o[y0 + dy][x0 + dx] = HEX.index(c) if c != "0" else 0
        out.append((f, note, o))
    return fid, out


def times(fid, frames):
    """each frame's time in its first script, in vsyncs (3 when no script has it)"""
    d, out = disc(fid), {}
    for _, sc in gp.anims(d, gp.FILES[fid][1]):
        for si in sc:
            for t, f, _, _ in gp.script(d, si):
                if t > 0:
                    out.setdefault(f, t)
    return [out.get(f, 3) for f in frames]


def blit(px, rows, clut, ox, oy, mirror=False):
    for y, r in enumerate(rows):
        for x, v in enumerate(r[::-1] if mirror else r):
            if v:
                px[ox + x, oy + y] = gp.rgb(clut[v])


def cmd_set(name, show=False, slow=1):
    need_pillow()
    from PIL import Image, ImageDraw
    fid, frames = read_set(name)
    d, a = disc(fid), auto(fid)
    kclut = gp.tim(final(fid), frames[0][0])[0]
    order = [f for f, _, _ in frames]
    mine = {f: rows for f, _, rows in frames}
    REVIEW.mkdir(parents=True, exist_ok=True)
    # the sheet: to study the frames one by one
    parts = [lay([labelled(picture(gp.tim(d, f)[0], rows_of(d, f), 6, True), "disc %d" % f) for f in order], gap=6),
             lay([labelled(picture(kclut, mine[f], 10, True), "drawn %d" % f) for f in order], gap=6)]
    for mirror in (False, True):
        parts.append(lay([picture(gp.tim(d, f)[0], rows_of(d, f), 3, mirror=mirror) for f in order]
                         + [picture(kclut, mine[f], 3, mirror=mirror) for f in order], gap=8))
    w, h = len(mine[order[0]][0]), len(mine[order[0]])
    strip = Image.new("RGB", (len(order) * 2 * (w + 2) + 4, 2 * (h + 4)), FIELD)
    for k, f in enumerate(order * 2):
        blit(strip.load(), rows_of(d, f), gp.tim(d, f)[0], 4 + k * (w + 2), 2)
        blit(strip.load(), mine[f], kclut, 4 + k * (w + 2), h + 6)
    parts += [strip.resize((strip.width * 2, strip.height * 2), Image.NEAREST), strip]
    sheet = REVIEW / (name + ".png")
    lay(parts, horizontal=False, gap=14).save(sheet)
    # the GIF: the one to review
    scale, cw, ims = 6, w + 8, []
    for f in order:
        im = Image.new("RGB", (cw * 3, (h + 4) * 2), FIELD)
        for k, (rows, clut) in enumerate(((rows_of(d, f), gp.tim(d, f)[0]), (rows_of(a, f), kclut),
                                          (mine[f], kclut))):
            for mirror in (0, 1):
                blit(im.load(), rows, clut, k * cw + 4, mirror * (h + 4) + 2, bool(mirror))
        big = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
        full = Image.new("RGB", (big.width, big.height + 22), (30, 30, 40))
        full.paste(big, (0, 22))
        for k, t in enumerate(("Junior original", "Kaili antes", "Kaili nova (quadro %d)" % f)):
            ImageDraw.Draw(full).text((k * cw * scale + 8, 5), t, fill=(235, 235, 235))
        ims.append(full)
    gif = REVIEW / (name + ".gif")
    ims[0].save(gif, save_all=True, append_images=ims[1:], loop=0,
                duration=[max(20, round(t * slow * 1000 / 60)) for t in times(fid, order)])
    print(sheet)
    print(gif)
    if show:
        n = 1 + len(list(REVIEW.glob(name + "_v*.gif")))
        copy = REVIEW / ("%s_v%d.gif" % (name, n))  # a new name each time: the viewer keeps the old one cached
        copy.write_bytes(gif.read_bytes())
        subprocess.Popen(["xdg-open", str(copy)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         start_new_session=True)
        print(copy)


def cmd_lean(name):
    fid, frames = read_set(name, skip="pig")
    d = disc(fid)

    def bands(rows):
        ys = [y for y, r in enumerate(rows) if any(r)]
        top, n = ys[0], ys[-1] - ys[0] + 1

        def centre(y0, y1):
            xs = [x for y in range(y0, y1) for x, v in enumerate(rows[y]) if v]
            return sum(xs) / len(xs)
        return centre(top, top + n * 4 // 10), centre(top + n * 4 // 10, top + n * 7 // 10), centre(top + n * 7 // 10,
                                                                                                     ys[-1] + 1)
    print("frame  Junior: torso-head legs-torso   Kaili: torso-head legs-torso   (columns; + is screen right)")
    for f, _, rows in frames:
        j, k = bands(rows_of(d, f)), bands(rows)
        print("%5d          %+5.1f      %+5.1f            %+5.1f      %+5.1f" % (f, j[1] - j[0], j[2] - j[1],
                                                                             k[1] - k[0], k[2] - k[1]))


def cmd_far(out, names, show=False, scale=3, slow=1, game=True, mp4=False):
    need_pillow()
    from PIL import Image, ImageDraw, ImageFont
    sets = [read_set(n) for n in names]
    n = max(len(fr) for _, fr in sets)
    try:  # the labels grow with the picture (Pillow 10.1+ scales its own font)
        font = ImageFont.load_default(size=max(11, 3 * scale))
    except TypeError:
        font = None
    panel = max(110, 26 * scale)
    cells = []
    for fid, frames in sets:
        d, k = disc(fid), gp.tim(final(fid), frames[0][0])[0]
        cells.append((fid, frames, d, k, len(frames[0][2][0]), len(frames[0][2])))
    W = sum(c[4] + 6 for c in cells) + 6
    H = max(c[5] for c in cells)
    label = 12
    ims, times_ = [], []
    shown = ("Junior", "Kaili no jogo", "Kaili nova") if game else ("Junior", "Kaili nova")
    for i in range(n):
        im = Image.new("RGB", (W, len(shown) * (H + 4) + 4), FIELD)
        x = 6
        for fid, frames, d, k, w, h in cells:
            f, _, rows = frames[i % len(frames)]
            layers = [(rows_of(d, f), gp.tim(d, f)[0])] + ([(drawn(fid, f), k)] if game else []) + [(rows, k)]
            for row, (pix, clut) in enumerate(layers):
                blit(im.load(), pix, clut, x, 4 + row * (H + 4))
            x += w + 6
        big = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
        full = Image.new("RGB", (big.width + panel, big.height), (30, 30, 40))
        full.paste(big, (panel, 0))
        for row, t in enumerate(shown):
            ImageDraw.Draw(full).text((6, row * (H + 4) * scale + (H + 4) * scale // 2 - label // 2), t,
                                      fill=(235, 235, 235), font=font)
        ImageDraw.Draw(full).text((6, 4), "pose %d de %d" % (i + 1, n), fill=(235, 220, 120), font=font)
        ims.append(full)
        times_.append(times(cells[0][0], [cells[0][1][i % len(cells[0][1])][0]])[0])
    REVIEW.mkdir(parents=True, exist_ok=True)
    gif = REVIEW / (out + ".gif")
    ims[0].save(gif, save_all=True, append_images=ims[1:], loop=0,
                duration=[max(20, round(t * slow * 1000 / 60)) for t in times_])
    print(gif)
    if mp4:
        import shutil
        import tempfile
        tmp = Path(tempfile.mkdtemp(prefix="far-", dir=str(WORK)))
        k = 0
        for loops, factor, label in ((3, 1, "velocidade real do jogo"), (2, 4, "0,25x (4 vezes mais lento)")):
            for _ in range(loops):
                for im, t in zip(ims, times_):
                    fr = im.copy()
                    ImageDraw.Draw(fr).text((6, fr.height - 6 - max(11, 3 * scale)), label, fill=(120, 220, 235),
                                            font=font)
                    for _ in range(t * factor):
                        fr.save(tmp / ("%05d.png" % k))
                        k += 1
        video = REVIEW / (out + ".mp4")
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-framerate", "60", "-i", str(tmp / "%05d.png"),
                        "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2", "-c:v", "libx264", "-crf", "12",
                        "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(video)], check=True)
        shutil.rmtree(tmp)
        print(video)
    if show:
        k = 1 + len(list(REVIEW.glob(out + "_v*.gif")))
        copy = REVIEW / ("%s_v%d.gif" % (out, k))
        copy.write_bytes(gif.read_bytes())
        subprocess.Popen(["xdg-open", str(copy)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         start_new_session=True)
        print(copy)


# ---- the recipe ----

RECIPE_MIN = 16  # the fewest header pixels a paste must save to be worth a line


def votes(want, sd):
    """(source frame, dx, dy) -> how many wanted pixels the frame has at that offset; want: {(x, y): index}"""
    out = {}
    for sf in gp.frames_of(sd, gp.FILES[SRC_OF[0]][1]):
        by = {}
        for sy, r in enumerate(gp.tim(sd, sf)[1]):
            for sx, v in enumerate(r):
                if v:
                    by.setdefault(v, []).append((sx, sy))
        for (x, y), v in want.items():
            for sx, sy in by.get(v, ()):
                k = (sf, x - sx, y - sy)
                out[k] = out.get(k, 0) + 1
    return out


SRC_OF = [0]  # the file the pieces come from, set by recipe()


def biggest(pts):
    """the largest 8-connected group of the points"""
    left, best = set(pts), set()
    while left:
        grp, todo = set(), [left.pop()]
        while todo:
            x, y = todo.pop()
            grp.add((x, y))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    q = (x + dx, y + dy)
                    if q in left:
                        left.remove(q)
                        todo.append(q)
        if len(grp) > len(best):
            best = grp
    return best


def recipe(fid, f, rows):
    """the paste lines that give the drawing's pieces equal to the other's own frames, and how the header's pixels
    went: (pastes, pixels the header holds without them, with them, the most of them that still fall on the same
    colour of one disc frame at one offset: scattered, since every connected piece of RECIPE_MIN became a paste)"""
    SRC_OF[0] = src = 0x190 if gp.FILES[fid][2] else 0x187
    sd = disc(src)
    H, W = len(rows), len(rows[0])

    def cost(b):
        return sum(1 for y in range(H) for x in range(W) if rows[y][x] != b[y][x])

    pastes, b = [], base(fid, f, [])
    first = cost(b)
    left = 0
    for _ in range(8):
        want = {(x, y): rows[y][x] for y in range(H) for x in range(W) if rows[y][x] and rows[y][x] != b[y][x]}
        v = votes(want, sd)
        best, left = None, max(v.values(), default=0)
        for (sf, dx, dy), n in sorted(v.items(), key=lambda kv: -kv[1])[:12]:
            piece = gp.tim(sd, sf)[1]
            hit = biggest([(x, y) for (x, y), val in want.items()
                           if 0 <= y - dy < len(piece) and 0 <= x - dx < len(piece[0]) and piece[y - dy][x - dx] == val])
            if len(hit) < RECIPE_MIN:
                continue
            x0, x1 = min(x for x, _ in hit), max(x for x, _ in hit)
            y0, y1 = min(y for _, y in hit), max(y for _, y in hit)
            p = (src, sf, x0 - dx, y0 - dy, x1 - x0 + 1, y1 - y0 + 1, x0, y0, 0)
            gain = cost(b) - cost(base(fid, f, pastes + [p]))
            if gain >= RECIPE_MIN and (best is None or gain > best[0]):
                best = (gain, p)
        if best is None:
            break
        pastes.append(best[1])
        b = base(fid, f, pastes)
    return pastes, first, cost(b), left


def also_of(name):
    """the files of a set's `also` lines: another file whose frame of the same number is the same picture at the same
    place on screen, cut to its own size (the scene file 186's run is the playable 187's, 24 wide)"""
    return [int(x, 16) for line in (SETS / (name + ".txt")).read_text().splitlines()
            if line.split("#")[0].split()[:1] == ["also"] for x in line.split("#")[0].split()[1:]]


def recut(fid, f, rows, to):
    """the drawing of frame f of file fid moved into the frame f of file `to`: each pixel kept where it is on screen
    (the two scripts' offsets), the frame the size `to` has; a pixel that falls outside it is left out, and said"""
    a, b = where(disc(fid), gp.FILES[fid][1], f), where(disc(to), gp.FILES[to][1], f)
    if not a or not b:
        sys.exit("frame %d: no script draws it in %X or %X" % (f, fid, to))
    dx, dy = a[1] - b[1], a[2] - b[2]
    size = rows_of(disc(to), f)
    out, cut = [[0] * len(size[0]) for _ in size], []
    for y, r in enumerate(rows):
        for x, v in enumerate(r):
            if not v:
                continue
            if 0 <= x + dx < len(out[0]) and 0 <= y + dy < len(out):
                out[y + dy][x + dx] = v
            else:
                cut.append("%d,%d" % (x, y))
    if cut:
        print("frame %d: %d pixel%s outside %X's %dx%d frame, left out there: %s"
              % (f, len(cut), "" if len(cut) == 1 else "s", to, len(out[0]), len(out), " ".join(cut)))
    return out


def cmd_save(name):
    import datetime
    fid, frames = read_set(name)
    keep = BACKUP / ("draw-" + datetime.date.today().strftime("%Y%m%d"))
    jobs = [(fid, f, note, rows) for f, note, rows in frames]
    jobs += [(to, f, note, recut(fid, f, rows, to)) for to in also_of(name) for f, note, rows in frames]
    for fid, f, note, rows in jobs:
        if path(fid, f).exists() and not (keep / path(fid, f).name).exists():
            keep.mkdir(parents=True, exist_ok=True)
            (keep / path(fid, f).name).write_bytes(path(fid, f).read_bytes())
        pastes, before, after, left = recipe(fid, f, rows)
        write(fid, f, ["%s %d: %s (drawn whole by hand)" % (gp.FILES[fid][0], f, note)], pastes, rows)
        if base(fid, f, pastes) and drawn(fid, f) != rows:
            sys.exit("%s: the drawing file does not read back as the set" % path(fid, f))
        print("%s: %d paste%s; the header holds %d pixels (%d without the recipe); at most %d of them fall on the "
              "same colour in one disc frame at one offset, scattered (no piece of %d left)"
              % (path(fid, f).name, len(pastes), "" if len(pastes) == 1 else "s", after, before, left, RECIPE_MIN))


def cmd_make():
    def run(what, cmd, **kw):
        r = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True, **kw)
        print("%-34s %s" % (what, "ok" if r.returncode == 0 else "FAIL"))
        if r.returncode:
            print((r.stdout + r.stderr)[-3000:])
            sys.exit(1)

    build()
    run("build/port (headless)", ["make", "-C", "build/port", "-j8"])
    run("build/port-sdl (the one played)", ["ninja", "-C", "build/port-sdl"])
    print("%-34s %s" % ("parity with the prototype", "ok" if check() else "FAIL"))
    py = str(ROOT / "tools/venv/bin/python")
    run("tests/port/girls_power.py", [py, "tests/port/girls_power.py"])
    run("tests/port/run.py", [py, "tests/port/run.py"])



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
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == "build":
        build()
    elif cmd == "check":
        sys.exit(0 if check() else 1)
    elif cmd == "make":
        cmd_make()
    elif cmd == "anims":
        cmd_anims(int(args[0], 16))
    elif cmd == "moves":
        base = args[args.index("--base") + 1].split(":") if "--base" in args else None
        cmd_moves(int(args[0], 16), frame_list(args[1]),
                  (int(base[0], 16), int(base[1])) if base else None,
                  int(args[args.index("--rows") + 1]) if "--rows" in args else 13)
    elif cmd == "text":
        cmd_text(args)
    elif cmd == "refs":
        cmd_refs(args[0], args[1:])
    elif cmd == "set":
        cmd_set(args[0], "--open" in args, int(args[args.index("--slow") + 1]) if "--slow" in args else 1)
    elif cmd == "lean":
        cmd_lean(args[0])
    elif cmd == "far":
        names = [a for a in args[1:] if not a.startswith("--") and not a.isdigit()]
        cmd_far(args[0], names, "--open" in args,
                int(args[args.index("--scale") + 1]) if "--scale" in args else 3,
                int(args[args.index("--slow") + 1]) if "--slow" in args else 1, "--no-game" not in args,
                "--mp4" in args)
    elif cmd == "save":
        cmd_save(args[0])
    elif cmd in ("edit", "base", "show"):
        fid, f = int(args[0], 16), int(args[1])
        if cmd == "edit":
            edit(fid, f)
        elif cmd == "base":
            rebase(fid, f)
        else:
            need_pillow()
            show(fid, f, args[2:])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
