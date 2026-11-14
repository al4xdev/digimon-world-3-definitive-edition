#!/usr/bin/env python3
"""Girls Power's head swap in Python (the prototype port/game/girls_power.c was written from; its frames are equal):
before/after sheets of the field sprite files, to tune the algorithm by eye before touching the C.

  tools/venv/bin/python tools/girls_power_preview.py OUT_DIR [SCALE]

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
    def __init__(self, fid, cap, key, skin, neck, outline=()):
        self.fid, self.table = fid, FILES[fid][1]
        self.cap, self.key, self.skin, self.neck, self.outline = set(cap), set(key), set(skin), neck, set(outline)

    def comp(self, img):
        H, W = len(img), len(img[0])
        seen, best = set(), (0, None)
        for y in range(H):
            for x in range(W):
                if img[y][x] in self.cap and (y, x) not in seen:
                    st, pts = [(y, x)], []
                    seen.add((y, x))
                    while st:
                        a, b = st.pop()
                        pts.append((a, b))
                        for c, e in ((a + 1, b), (a - 1, b), (a, b + 1), (a, b - 1)):
                            if 0 <= c < H and 0 <= e < W and (c, e) not in seen and img[c][e] in self.cap:
                                seen.add((c, e))
                                st.append((c, e))
                    k = sum(1 for a, b in pts if img[a][b] in self.key)
                    if k > best[0]:
                        best = (k, pts)
        return best[1]

    def head(self, img):
        """(the head's pixels, chin row, x0 + x1 of the cap/hair, direction 0-4, mirrored) or None"""
        c = self.comp(img)
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
        sk = [(y, x) for y, x in pts if img[y][x] in self.skin]
        rel = (sum(x for _, x in sk) / len(sk) - (x0 + x1) / 2) if sk else 0
        if len(sk) < 4:
            d = 4
        elif abs(rel) < 1.0:
            d = 0
        elif abs(rel) < 2.5:
            d = 1 if len(sk) >= 12 else 3
        else:
            d = 2
        return pts, bottom, x0 + x1, d, rel > 0

    def refs(self):
        data = disc_files.read(self.fid)
        s = dict(anims(data, self.table))[1]
        heads = []
        for d in range(5):
            clut, idx = tim(data, script(data, s[d])[0][1])
            pts, bottom, x2, _, _ = self.head(idx)
            heads.append(({(y, x): idx[y][x] for y, x in pts}, bottom, x2))
        return clut, heads


JUNIOR = Who(0x187, cap={2, 3, 7}, key={7}, skin={8, 9}, neck=16)
KAILI = Who(0x190, cap={4, 6, 14}, key={4}, skin={1, 2}, neck=15, outline={7})


def swap(img, src, dst_heads, body):
    H, W = len(img), len(img[0])
    out = [[body[v] if v else 0 for v in r] for r in img]
    h = src.head(img)
    if h:
        pts, bottom, x2, d, flip = h
        K, kb, kx2 = dst_heads[d]
        for y, x in pts:
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


def main():
    out = Path(sys.argv[1])
    scale = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    out.mkdir(parents=True, exist_ok=True)
    jclut, jheads = JUNIOR.refs()
    kclut, kheads = KAILI.refs()
    for fid, (name, table, to_kaili) in FILES.items():
        data = disc_files.read(fid)
        before, after = [], []
        for f in frames_of(data, table):
            clut, idx = tim(data, f)
            o = swap(idx, JUNIOR, kheads, J2K) if to_kaili else swap(idx, KAILI, jheads, K2J)
            nclut = kclut if to_kaili else jclut
            before.append([[rgb(clut[v]) if v else None for v in r] for r in idx])
            after.append([[rgb(nclut[v]) if v else None for v in r] for r in o])
        imgs, blank = [], [[None]]
        for k in range(0, len(before), 16):
            a, b = before[k:k + 16], after[k:k + 16]
            imgs += a + [blank] * (16 - len(a)) + b + [blank] * (16 - len(b))
        sheet(imgs, out / f"{name}.ppm", scale)
        print(name, len(before), "frames")


if __name__ == "__main__":
    main()
