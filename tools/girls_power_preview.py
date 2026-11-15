#!/usr/bin/env python3
"""Girls Power's sprite swap in Python (the prototype port/game/girls_power.c was written from; its frames are equal):
before/after sheets of the field sprite files, to tune the algorithm by eye before touching the C.

  tools/venv/bin/python tools/girls_power_preview.py OUT_DIR [SCALE]
  tools/venv/bin/python tools/girls_power_preview.py zoom OUT.png FILE_ID FRAME,FRAME,... [SCALE]

Writes OUT_DIR/<file>.ppm: rows of 16 frames, each row of originals followed by the same frames repainted. Reads the
user's disc (tools/disc_files.py); embeds no game data."""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import disc_files  # noqa: E402

# file id: (name, animation table, becomes Kaili)
FILES = {0x186: ("P001PLAY", 0xCA, True), 0x187: ("P002PLAY", 0x70, True), 0x3C9: ("P002PLYD", 0x0B, True),
         0x3CA: ("P002PLYF", 0x64, True), 0x3CB: ("P002PLYK", 0x1E, True), 0x3CC: ("P002PLYL", 0x17, True),
         0x190: ("P011MINO", 0xA8, False), 0x2F8: ("P012MINT", 0x26, False)}
J2K = [0, 4, 7, 12, 6, 7, 5, 3, 1, 2, 10, 13, 14, 8, 9, 15]
K2J = [0, 8, 9, 7, 3, 6, 2, 13, 13, 14, 10, 12, 3, 11, 2, 15]
# The animations both have for the same thing (standing, walking, talking, the start, the nod, the hop): there the
# other's own frames are used whole. 57 and 58 they share by number only (another gesture, another way faced), and
# the rest only one has: those keep the head swap.
WHOLE = (1, 4, 7, 9, 12, 51)
# His animations she has no drawing for (running, its start and stop) -> the animation of hers (walking) whose frames
# give the head and trunk, step by step, put on his legs.
BUST = {5: 4, 6: 4}
JUNIOR_WAIST, KAILI_WAIST = 7, 8  # rows from the chin down to the belt: his cut there, hers kept down to there
# His legs in her colours: the head swap's table, but his grey trousers are her violet ones (his greys elsewhere are
# shadows on white, which she has as a brown grey)
LEGS = [0, 4, 5, 13, 6, 7, 5, 12, 1, 2, 10, 13, 14, 7, 12, 15]
ART = Path(__file__).resolve().parents[1] / "port/game/girls_power_art.h"
# file id: the file the whole frames come from (Junior's scene file has his frames 24 wide, as most of hers are)
SOURCE = {0x186: 0x190, 0x187: 0x190, 0x190: 0x186, 0x2F8: 0x186}


def sub(d, i):
    return struct.unpack_from("<i", d, i * 4)[0]


def anims(d, table):
    out, p = [], sub(d, table)
    while True:
        a, *s = struct.unpack_from("<6h", d, p)
        if a == 0:
            return out
        out.append((a, s))
        p += 12


def script(d, idx):
    p, fr = sub(d, idx), []
    while len(fr) < 256:
        t, f, x, y = struct.unpack_from("<4i", d, p)
        p += 16
        fr.append((t, f, x, y))
        if t in (0, -1):
            break
    return fr


def frames_of(d, table):
    seen = []
    for _, sc in anims(d, table):
        for si in sc:
            for t, f, _, _ in script(d, si):
                if t > 0 and f not in seen:
                    seen.append(f)
    return sorted(seen)


def hints(d, table):
    """frame -> the direction slot (0-4) of the first animation that has it in a script only one of its slots uses"""
    out = {}
    for _, sc in anims(d, table):
        for k, si in enumerate(sc):
            if sc.count(si) == 1:
                for t, f, _, _ in script(d, si):
                    if t > 0:
                        out.setdefault(f, k)
    return out


def tim(d, idx):
    p = sub(d, idx)
    q = p + 8
    ln, _, _, cw, ch = struct.unpack_from("<IHHHH", d, q)
    clut = list(struct.unpack_from("<%dH" % (cw * ch), d, q + 12))[:16]
    q += ln
    _, _, _, iw, ih = struct.unpack_from("<IHHHH", d, q)
    px = d[q + 12:q + 12 + iw * 2 * ih]
    return clut, [[(px[y * iw * 2 + x // 2] >> (4 if x & 1 else 0)) & 15 for x in range(iw * 4)] for y in range(ih)]


def rgb(c):
    return ((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3)


class Who:
    def __init__(self, fid, cap, key, skin, neck, outline=(), top=None, hang=None):
        self.fid, self.table = fid, FILES[fid][1]
        # top: the cap/hair seen from above, its colours -> the other's (in the other's palette), for a head that
        # covers the body; hang: hair hanging below the chin and the outline inside it -> the other's clothes
        self.top, self.hang = top or {}, hang or {}
        self.cap, self.key, self.skin, self.neck, self.outline = set(cap), set(key), set(skin), neck, set(outline)

    def comp(self, img, over=None, colours=None):
        """The cap/hair: the blobs of cap colours with at least half their pixels in `over` (where the matched head
        is; a band can split a cap in two), or else the one with the most there. With no `over`, as for the idle
        frames the reference heads are cut from, the blob with the most key pixels. None when there is none."""
        H, W = len(img), len(img[0])
        seen, best, inside = set(), (0, None), []
        colours = colours or self.cap
        for y in range(H):
            for x in range(W):
                if img[y][x] in colours and (y, x) not in seen:
                    st, pts = [(y, x)], []
                    seen.add((y, x))
                    while st:
                        a, b = st.pop()
                        pts.append((a, b))
                        for c, e in ((a + 1, b), (a - 1, b), (a, b + 1), (a, b - 1)):
                            if 0 <= c < H and 0 <= e < W and (c, e) not in seen and img[c][e] in colours:
                                seen.add((c, e))
                                st.append((c, e))
                    k = sum(1 for a, b in pts if (((a, b) in over) if over is not None else img[a][b] in self.key))
                    if k > best[0]:
                        best = (k, pts)
                    if over is not None and k * 2 >= len(pts):
                        inside += pts
        return inside or best[1]

    def head(self, img, over=None):
        """(the head's pixels, chin row, twice the x of its axis) or None. The axis is the face's (the mean x of its
        skin), so that one face lands on the other; a head showing no face has the cap/hair blob's centre."""
        c = self.comp(img, over)
        if not c or len(c) < 6:
            return None
        H, W = len(img), len(img[0])
        y0, x0 = min(p[0] for p in c), min(p[1] for p in c)
        x1 = max(p[1] for p in c)
        cols = range(max(0, x0 - 1), min(W, x1 + 2))
        skin_rows = [y for y in range(y0, min(H, y0 + self.neck)) for x in cols if img[y][x] in self.skin]
        bottom = max(skin_rows) if skin_rows else y0 + self.neck - 2
        pts = {(y, x) for y in range(y0, min(H, bottom + 1)) for x in cols if img[y][x]} | set(c)
        stray = {(y, x) for y in range(max(0, y0 - 2), min(H, bottom + 2)) for x in range(W) if img[y][x] in self.cap}
        for y, x in list(stray):
            for a, b in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= a < H and 0 <= b < W and img[a][b] in self.outline:
                    stray.add((a, b))
        pts |= stray
        sk = [x for y, x in pts if img[y][x] in self.skin]
        x2 = (4 * sum(sk) + len(sk)) // (2 * len(sk)) if len(sk) >= 4 else x0 + x1
        return pts, bottom, x2

    def match(self, img, heads, hint):
        """Where one of this person's own idle heads sits in the frame: (pixels equal, direction, mirrored, dy, dx, the
        head's pixel count) of the best placement, the head whole inside the frame; None when none has an equal pixel.
        Only direction `hint` is tried when it is given. The first best in (direction, mirror, dy, dx) order wins."""
        H, W = len(img), len(img[0])
        best = None
        for d in (range(5) if hint is None else (hint,)):
            px, _, x2 = heads[d]
            for flip in (False, True):
                pts = [(y, x2 - x if flip else x, v) for (y, x), v in px.items()]
                y0, y1 = min(p[0] for p in pts), max(p[0] for p in pts)
                x0, x1 = min(p[1] for p in pts), max(p[1] for p in pts)
                for dy in range(-y0, H - y1):
                    for dx in range(-x0, W - x1):
                        n = sum(1 for y, x, v in pts if img[y + dy][x + dx] == v)
                        if n > (best[0] if best else 0):
                            best = (n, d, flip, dy, dx, len(pts))
        return best

    def refs(self):
        if hasattr(self, "_refs"):
            return self._refs
        self._refs = self._read_refs()
        return self._refs

    def _read_refs(self):
        data = disc_files.read(self.fid)
        s = dict(anims(data, self.table))[1]
        heads = []
        for d in range(5):
            clut, idx = tim(data, script(data, s[d])[0][1])
            pts, bottom, x2 = self.head(idx)
            heads.append(({(y, x): idx[y][x] for y, x in pts}, bottom, x2))
        return clut, heads


JUNIOR = Who(0x187, cap={2, 3, 7}, key={7}, skin={8, 9}, neck=16, top={3: 4, 7: 6, 2: 14, 6: 14})
KAILI = Who(0x190, cap={4, 6, 14}, key={4}, skin={1, 2}, neck=15, outline={7}, top={4: 3, 6: 7, 14: 2},
            hang={4: 3, 6: 2, 14: 2, 7: 2})


LEFTOVER = 8  # the biggest piece cut loose by the head's removal that is dropped with it


def loose(img, pts):
    """what removing `pts` cuts loose from the body (a pigtail's tip, a cap's corner, a foot whose leg went): the
    pieces of at most LEFTOVER pixels touching it, corners included. They go with it."""
    H, W = len(img), len(img[0])
    pts, seen, out = set(pts), set(), set()
    for y in range(H):
        for x in range(W):
            if not img[y][x] or (y, x) in pts or (y, x) in seen:
                continue
            st, part, touch = [(y, x)], [], False
            seen.add((y, x))
            while st:
                a, b = st.pop()
                part.append((a, b))
                for c in (-1, 0, 1):
                    for e in (-1, 0, 1):
                        q = (a + c, b + e)
                        if q in pts:
                            touch = True
                        elif (c == 0) != (e == 0) and 0 <= q[0] < H and 0 <= q[1] < W and img[q[0]][q[1]] \
                                and q not in seen:
                            seen.add(q)
                            st.append(q)
            if touch and len(part) <= LEFTOVER:
                pts |= set(part)
                out |= set(part)
    return out


def swap(img, src, dst_heads, body, hint=None):
    """The frame with the other's head and colours. The direction is the frame's slot in the animation table (`hint`)
    when it has one, else the best match of the source's own idle heads. The head goes where that match is when at
    least half of the matched head's pixels are equal; else (tilted and lying heads) on the cap/hair blob's chin."""
    H, W = len(img), len(img[0])
    out = [[body[v] if v else 0 for v in r] for r in img]
    own = src.refs()[1]
    m = src.match(img, own, hint)
    if not m:
        return out
    _, d, flip, dy, dx, n = m
    px, rb, rx2 = own[d]
    placed = {(y + dy, (rx2 - x if flip else x) + dx) for y, x in px}
    h = src.head(img, placed)
    if m[0] * 2 >= n:
        pts = (h[0] if h else set()) | placed
        bottom, x2 = rb + dy, rx2 + 2 * dx
    else:
        if not h:
            return out
        pts, bottom, x2 = h
        if hint is not None:
            flip = False
    # A head with no body left under it (three quarters of the frame) is one seen from above, lying: no head of the
    # five fits, so the cap/hair is dyed the other's instead.
    if 4 * sum(1 for y, x in pts if img[y][x]) >= 3 * sum(1 for r in img for v in r if v):
        for y, x in src.comp(img, None, set(src.top)) or ():
            out[y][x] = src.top[img[y][x]]
        return out
    pts = pts | loose(img, pts)
    # Hair hanging below the chin over the back (body on both sides of it in its row) is not removed, which would
    # leave a hole: it becomes the other's clothes, with the outline pixels inside it (no neighbour of the four empty,
    # two or more of them that hair).
    if src.hang:
        def body(y, xs):
            return any(img[y][x] and (y, x) not in pts for x in xs)
        low = {(y, x) for y, x in pts if y > bottom and img[y][x] in src.cap and body(y, range(x)) and
               body(y, range(x + 1, W))}
        pts = pts - low
        for y in range(1, H - 1):
            for x in range(1, W - 1):
                near = ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1))
                if img[y][x] in src.outline and (y, x) not in pts and all(img[a][b] for a, b in near) \
                        and sum(1 for q in near if q in low) >= 2:
                    out[y][x] = src.hang[img[y][x]]
        for y, x in low:
            out[y][x] = src.hang[img[y][x]]
    K, kb, kx2 = dst_heads[d]
    for y, x in pts:
        if 0 <= y < H and 0 <= x < W:
            out[y][x] = 0
    sh = round((x2 - kx2) / 2)
    for (y, x), v in K.items():
        if flip:
            x = kx2 - x
        yy, xx = y - kb + bottom, x + sh
        if 0 <= yy < H and 0 <= xx < W:
            out[yy][xx] = v
    return out


def sheet(imgs, path, scale):
    cw, chh, cols = max(len(i[0]) for i in imgs), max(len(i) for i in imgs), 16
    rows = (len(imgs) + cols - 1) // cols
    W, H = cols * cw * scale, rows * chh * scale
    buf = bytearray([40, 40, 60] * W * H)
    for n, im in enumerate(imgs):
        ox, oy = (n % cols) * cw * scale, (n // cols) * chh * scale
        for y, r in enumerate(im):
            for x, c in enumerate(r):
                if c is None:
                    continue
                for dy in range(scale):
                    o = ((oy + y * scale + dy) * W + ox + x * scale) * 3
                    buf[o:o + 3 * scale] = bytes(c) * scale
    Path(path).write_bytes(b"P6 %d %d 255\n" % (W, H) + bytes(buf))


def steps(d, idx):
    """a script's entries up to its end: (offset of the entry, frame, x, y)"""
    p, out = sub(d, idx), []
    while len(out) < 256:
        t, f, x, y = struct.unpack_from("<4i", d, p)
        if t <= 0:
            break
        out.append((p, f, x, y))
        p += 16
    return out


def whole(d, table, sd, stable, who):
    """dst frame -> (src frame, tx, ty, ddx, ddy): the frames of the WHOLE animations that take the other's own frame,
    drawn at (tx, ty) in the frame, every script entry naming the frame moved by (ddx, ddy) so it lands where the
    other's own script draws it. A script takes the other's script of the same animation and direction: its frames
    in order of first use take the other's in order of first use (the last one again when the other has fewer). The
    direction is the script's slot, or, for a script several slots share (drawn one way whatever the slot), the way
    its first frame's head faces: the other may have that way drawn in its own slot. A script is taken whole or not
    at all: every frame must have one source (also against the scripts taken before) that fits it."""
    got, hint, own = {}, hints(d, table), who.refs()[1]
    for a, sc in anims(d, table):
        src = next((s for b, s in anims(sd, stable) if b == a), None)
        if a not in WHOLE or src is None:
            continue
        for k in range(5):
            ds = steps(d, sc[k])
            if sc[k] in sc[:k] or not ds:
                continue
            way = k
            if sc.count(sc[k]) > 1:
                m = who.match(tim(d, ds[0][1])[1], own, hint.get(ds[0][1]))
                if not m:
                    continue
                way = m[1]
            first, sfirst = {}, {}
            for _, f, x, y in ds:
                first.setdefault(f, (x, y))
            for _, sf, sx, sy in steps(sd, src[way]):
                sfirst.setdefault(sf, (sx, sy))
            if not sfirst:
                continue
            sl, trial = list(sfirst.items()), {}
            for j, (f, (x, y)) in enumerate(first.items()):
                sf, (sx, sy) = sl[min(j, len(sl) - 1)]
                if f in got:
                    if got[f][0] != sf:
                        break
                    continue
                img, dst = tim(sd, sf)[1], tim(d, f)[1]
                xs = [i for r in img for i, v in enumerate(r) if v]
                ys = [j2 for j2, r in enumerate(img) if any(r)]
                H, W = len(dst), len(dst[0])
                if not xs or max(xs) - min(xs) >= W or max(ys) - min(ys) >= H:
                    break
                # where the script's own offset would put it, moved only as far as the frame's sides ask
                tx = min(max(sx - x, -min(xs)), W - 1 - max(xs))
                ty = min(max(sy - y, -min(ys)), H - 1 - max(ys))
                trial[f] = (sf, tx, ty, sx - tx - x, sy - ty - y)
            else:
                got.update(trial)
    return got


def spot(who, img, hint):
    """(chin row, twice the axis' x, the chin row of the idle head of that direction) of the head in the frame, as
    swap() places it; None when there is none"""
    own = who.refs()[1]
    m = who.match(img, own, hint)
    if not m:
        return None
    _, d, flip, dy, dx, n = m
    px, rb, rx2 = own[d]
    if m[0] * 2 >= n:
        return rb + dy, rx2 + 2 * dx, rb
    h = who.head(img, {(y + dy, (rx2 - x if flip else x) + dx) for y, x in px})
    return (h[1], h[2], rb) if h else None


def bust(img, kimg, way):
    """His frame with her head and trunk (her frame `kimg`, facing `way`) on his legs; None when a head is not found.
    Everything of his down to the belt goes; hers down to her belt comes, face on face. He runs crouched, his head
    as much as 4 rows under where he holds it standing, and her trunk is longer than his: she drops only half as far
    (rounded down), or no legs would be left under her, and takes as many more rows of her own hips to reach his."""
    a, b = spot(JUNIOR, img, way), spot(KAILI, kimg, way)
    if not a or not b:
        return None
    H, W = len(img), len(img[0])
    drop = max(0, a[0] - a[2])
    lift = drop - drop // 2
    gone = {(y, x) for y, r in enumerate(img) for x, v in enumerate(r) if v and y <= a[0] + JUNIOR_WAIST}
    gone |= loose(img, gone)
    out = [[LEGS[v] if (y, x) not in gone else 0 for x, v in enumerate(r)] for y, r in enumerate(img)]
    sh = round((a[1] - b[1]) / 2)
    for y, r in enumerate(kimg[:b[0] + KAILI_WAIST + lift + 1]):
        for x, v in enumerate(r):
            yy, xx = y - b[0] + a[0] - lift, x + sh
            if v and 0 <= yy < H and 0 <= xx < W:
                out[yy][xx] = v
    return out


def busts(d, table, sd, stable, full):
    """dst frame -> (src frame, direction) for the BUST animations: the same step of the same direction, around again
    when hers is shorter; the first use of a frame decides"""
    got = {}
    for a, sc in anims(d, table):
        src = next((s for b, s in anims(sd, stable) if b == BUST.get(a)), None)
        if src is None:
            continue
        for k in range(5):
            ss = steps(sd, src[k])
            for i, (_, f, _, _) in enumerate(steps(d, sc[k])):
                if ss and f not in full:
                    got.setdefault(f, (ss[i % len(ss)][1], k))
    return got


def art():
    """(file id, frame) -> the hand-drawn retouch steps of port/game/girls_power_art.h, in its order: ("paste", src
    file, src frame, x, y, w, h, to x, to y, how) puts that piece of a disc frame on it (how: 1 mirrored, 2 through
    the file's colour table), ("art", rows) draws over it ('.' keeps the pixel, '0' empties it, 1-9 A-F set its
    colour, the other's palette)"""
    import re
    out = {}
    if not ART.exists():
        return out
    text = re.sub(r"/\*.*?\*/", "", ART.read_text(), flags=re.S)
    for m in re.finditer(r"GP_(PASTE|ART)\(([^\"]*?)((?:\s*\"[^\"]*\")*)\s*\)", text):
        nums = [int(v, 0) for v in m.group(2).replace(",", " ").split()]
        step = ("art", re.findall(r'"([^"]*)"', m.group(3))) if m.group(1) == "ART" else ("paste", *nums[2:])
        out.setdefault((nums[0], nums[1]), []).append(step)
    return out


def draw_steps(o, steps, to_kaili):
    """the frame `o` (rows of indices) with the steps of art() drawn on it, in place"""
    H, W = len(o), len(o[0])
    for st in steps:
        if st[0] == "art":
            for n, c in enumerate("".join(st[1])[:H * W]):
                if c in "0123456789ABCDEF":
                    o[n // W][n % W] = int(c, 16)
            continue
        src, sf, x0, y0, w, h, tx, ty, how = st[1:]
        piece = tim(disc_files.read(src), sf)[1]
        for y in range(h):
            for x in range(w):
                sy, sx, yy, xx = y0 + y, x0 + x, ty + y, tx + (w - 1 - x if how & 1 else x)
                if 0 <= sy < len(piece) and 0 <= sx < len(piece[0]) and 0 <= yy < H and 0 <= xx < W and piece[sy][sx]:
                    v = piece[sy][sx]
                    o[yy][xx] = ((J2K if to_kaili else K2J)[v]) if how & 2 else v


def repaint(fid, retouch=True):
    """the file as Girls Power leaves it (what port/game/girls_power.c writes, byte for byte); (bytes, the whole
    frames' plan, the bust frames' plan)"""
    name, table, to_kaili = FILES[fid]
    d0 = disc_files.read(fid)
    d, hint = bytearray(d0), hints(d0, table)
    jclut, jheads = JUNIOR.refs()
    kclut, kheads = KAILI.refs()
    sd = disc_files.read(SOURCE[fid]) if fid in SOURCE else None
    full = whole(d0, table, sd, FILES[SOURCE[fid]][1], JUNIOR if to_kaili else KAILI) if sd else {}
    half = busts(d0, table, sd, FILES[SOURCE[fid]][1], full) if sd and to_kaili else {}
    drawn = art() if retouch else {}
    for f in frames_of(d0, table):
        _, idx = tim(d0, f)
        o = None
        if f in full:
            sf, tx, ty, _, _ = full[f]
            o = [[0] * len(idx[0]) for _ in idx]
            for y, r in enumerate(tim(sd, sf)[1]):
                for x, v in enumerate(r):
                    if v:
                        o[y + ty][x + tx] = v
        elif f in half:
            o = bust(idx, tim(sd, half[f][0])[1], half[f][1])
        if o is None:
            o = swap(idx, JUNIOR, kheads, J2K, hint.get(f)) if to_kaili else swap(idx, KAILI, jheads, K2J, hint.get(f))
        draw_steps(o, drawn.get((fid, f), ()), to_kaili)
        q = sub(d0, f) + 8
        struct.pack_into("<16H", d, q + 12, *(kclut if to_kaili else jclut))
        q += struct.unpack_from("<I", d0, q)[0] + 12
        for y, r in enumerate(o):
            for x in range(0, len(r), 2):
                d[q + y * (len(r) // 2) + x // 2] = r[x] | (r[x + 1] << 4)
    done = set()
    for _, sc in anims(d0, table):
        for si in sc:
            if si in done:
                continue
            done.add(si)
            for p, f, x, y in steps(d0, si):
                if f in full:
                    struct.pack_into("<2i", d, p + 8, x + full[f][3], y + full[f][4])
    return bytes(d), full, half


def zoom(out, fid, frames, scale):
    """OUT.png: for each frame, a column of the disc's frame, the automatic result, the retouched result and the
    other's frame it was made from; and each automatic frame's rows on stdout, to start a retouch from"""
    import subprocess
    data, (auto, full, half), final = disc_files.read(fid), repaint(fid, False), repaint(fid)[0]
    sd = disc_files.read(SOURCE[fid]) if fid in SOURCE else None
    rows = [[], [], [], []]
    for f in frames:
        src = full[f][0] if f in full else half[f][0] if f in half else None
        for to, (d, g) in zip(rows, ((data, f), (auto, f), (final, f), (sd, src))):
            if g is None:
                to.append([[None] * 32] * 32)
                continue
            clut, idx = tim(d, g)
            to.append([[rgb(clut[v]) if v else None for v in r] for r in idx])
        print("frame", f)
        for r in tim(auto, f)[1]:
            print('        "' + "".join(".123456789ABCDEF"[v] for v in r) + '"')
    imgs = []
    for r in rows:
        imgs += r + [[[None]]] * (16 - len(r))
    sheet(imgs, str(out) + ".ppm", scale)
    subprocess.run(["magick", str(out) + ".ppm", "-crop", "%dx+0+0" % (32 * scale * len(frames)), "+repage", str(out)])
    Path(str(out) + ".ppm").unlink()


def main():
    if sys.argv[1] == "zoom":  # zoom OUT.png FILE_ID(hex) FRAME,FRAME,... [SCALE]
        zoom(Path(sys.argv[2]), int(sys.argv[3], 16), [int(v) for v in sys.argv[4].split(",")],
             int(sys.argv[5]) if len(sys.argv) > 5 else 8)
        return
    out = Path(sys.argv[1])
    scale = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    out.mkdir(parents=True, exist_ok=True)
    for fid, (name, table, to_kaili) in FILES.items():
        data = disc_files.read(fid)
        new, full, half = repaint(fid)
        before, after = [], []
        for f in frames_of(data, table):
            for d, to in ((data, before), (new, after)):
                clut, idx = tim(d, f)
                to.append([[rgb(clut[v]) if v else None for v in r] for r in idx])
        imgs, blank = [], [[None]]
        for k in range(0, len(before), 16):
            a, b = before[k:k + 16], after[k:k + 16]
            imgs += a + [blank] * (16 - len(a)) + b + [blank] * (16 - len(b))
        sheet(imgs, out / f"{name}.ppm", scale)
        moved = sorted((f, v[3], v[4]) for f, v in full.items() if v[3] or v[4])
        print(name, len(before), "frames,", len(full), "whole,", len(half), "bust; script offsets moved:", moved)


if __name__ == "__main__":
    main()
