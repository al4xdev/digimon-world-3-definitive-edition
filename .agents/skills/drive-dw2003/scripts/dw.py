"""Drive the PC port through its debug channel: run, press, poke, screenshot, record, make clips.

Run from the repo root:  uv run --with pillow python .agents/skills/drive-dw2003/scripts/dw.py <command> ...

  seq   [--state S] [--env K=V]... [--poke SYM=V]... [--save S] OUTPREFIX "BTN:wait,BTN+BTN:wait,_:wait"
        press each button (or "_" for none), run `wait` frames, take one screenshot per step
        (OUTPREFIX_00.png, _01...). Use it to find your way through menus.
  rec   [--state S] [--env K=V]... [--poke SYM=V]... [--pre N] [--every 2] [--save S] OUTDIR N "BTN@i,..."
        record N frames (one every `--every` vsyncs) into OUTDIR/f0000.png...; BTN@i presses BTN when frame i is taken.
  sheet DIR STEP DST         contact sheet of every STEP-th frame (6x6, 320x180 tiles): look at it before anything else.
  clip  DIR GIF MP4 RANGES POSTER [--fps 8] [--gif-width 480] [--crf 25]
        GIF (README) + h264 MP4 at 1280x720/30 fps and a .jpg poster (site) from frame ranges "a-b,c-d".

Common options: --bin (default build/port-sdl/dw2003), --config (settings JSON), --soft (software renderer, no window),
--card1/--card2 (memory cards; work on COPIES, never on a player's own card).
SYM is a symbol name from `nm` of the same binary, with an optional +offset and :size (bytes, default 4):
  --poke gamestate_data+0x30=1     --poke fightstg_battle+0x78=40:2
"""
import argparse
import glob
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "psxstack/tools/mcp"))
from game import Game, button_mask  # noqa: E402

from PIL import Image  # noqa: E402

GPU_ARGS = ["--window", "--scale", "2", "--renderer", "gpu", "--internal-scale", "8", "--subpixel", "on"]


class Run:
    def __init__(self, binary, config=None, env=None, state=None, gpu=True, card1=None, card2=None):
        self.binary = str(binary)
        self.gpu = gpu
        e = dict(os.environ, SDL_AUDIODRIVER="dummy")
        if not gpu:
            e.setdefault("SDL_VIDEODRIVER", "offscreen")
        e.update(env or {})
        cmd = [self.binary, "--disc", str(ROOT / "iso/dw2003.cue"), "--cd-speed", "instant", "--fps", "0"]
        if card1:
            cmd += ["--memcard1", str(card1)]
        if card2:
            cmd += ["--memcard2", str(card2)]
        if config:
            cmd += ["--config", str(config), "--script-mods"]
        if gpu:
            cmd += GPU_ARGS
        self.g = Game.spawn(cmd, cwd=str(ROOT), env=e, hold=True, timeout=60)
        self._syms = None
        if state:
            self.g.load_state(str(Path(state).resolve()))

    def sym(self, name):
        if self._syms is None:
            out = subprocess.run(["nm", self.binary], capture_output=True, text=True).stdout
            self._syms = {}
            for line in out.splitlines():
                p = line.split()
                if len(p) == 3:
                    self._syms.setdefault(p[2], []).append(int(p[0], 16))
        hits = self._syms.get(name)
        if not hits:
            raise KeyError(f"no symbol {name}")
        if len(hits) > 1:
            raise KeyError(f"{name} is ambiguous ({len(hits)} statics): use its address from nm -n")
        return hits[0]

    def addr(self, target):
        name, _, off = target.partition("+")
        base = int(name, 0) if name.startswith("0x") else self.sym(name)
        return base + (int(off, 0) if off else 0)

    def rd(self, target, size=4):
        return int.from_bytes(self.g.peek(self.addr(target), size), "little")

    def wr(self, target, value, size=4):
        self.g.poke(self.addr(target), int(value).to_bytes(size, "little", signed=value < 0))

    def poke_spec(self, spec):
        target, _, v = spec.partition("=")
        v, _, size = v.partition(":")
        self.wr(target, int(v or "1", 0), int(size or 4))

    def step(self, n=1):
        return self.g.step(n)

    def press(self, buttons, frames=2, release=2):
        self.g.pad(button_mask(buttons), frames=frames, release=release, sync=True)

    def shot(self, path, width=None):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        ppm = path.with_suffix(".ppm")
        self.g.screenshot(str(ppm), renderer="gpu" if self.gpu else None)
        im = Image.open(ppm).convert("RGB")
        ppm.unlink()
        if width and im.width != width:
            im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
        im.save(path)
        return im

    def save(self, path):
        self.g.save_state(str(Path(path).resolve()))

    def quit(self):
        self.g.stop()


def open_run(a):
    env = dict(kv.split("=", 1) for kv in a.env)
    r = Run(a.bin, a.config, env, a.state, gpu=not a.soft, card1=a.card1, card2=a.card2)
    return r


def cmd_seq(a):
    r = open_run(a)
    for spec in a.poke:
        r.poke_spec(spec)
    for k, it in enumerate(x for x in a.steps.split(",") if x):
        b, _, w = it.partition(":")
        if b not in ("_", "-"):
            r.press(b.split("+"))
        r.step(int(w or 30))
        r.shot(f"{a.out}_{k:02d}.png", width=1280)
    if a.save:
        r.save(a.save)
    r.quit()


def cmd_rec(a):
    r = open_run(a)
    r.step(a.pre)
    for spec in a.poke:
        r.poke_spec(spec)
    plan = {}
    for it in (x for x in a.presses.split(",") if x):
        b, i = it.split("@")
        plan.setdefault(int(i), []).append(b.split("+"))
    for i in range(a.n):
        for b in plan.get(i, []):
            r.g.pad(button_mask(b), frames=2, release=0)
        r.shot(Path(a.out) / f"f{i:04d}.png", width=1280)
        r.step(a.every)
    if a.save:
        r.save(a.save)
    r.quit()


def cmd_sheet(a):
    fs = sorted(glob.glob(a.dir + "/f*.png"))[:: a.step][:36]
    w, h = 320, 180
    W = Image.new("RGB", (w * 6, h * 6))
    for k, f in enumerate(fs):
        W.paste(Image.open(f).resize((w, h)), ((k % 6) * w, (k // 6) * h))
    W.save(a.dst)


def cmd_clip(a):
    fs = sorted(glob.glob(a.dir + "/f*.png"))
    keep = []
    for rg in a.ranges.split(","):
        x, y = map(int, rg.split("-"))
        keep += fs[x:y]

    def load(f, w):
        im = Image.open(f).convert("RGB")
        im = im.crop((0, 0, im.width, im.width * 9 // 16))  # 1280x721 frames happen: keep exact 16:9
        return im if w == im.width else im.resize((w, w * 9 // 16), Image.LANCZOS)

    # frames were taken every 2 vsyncs = 30 per second; the GIF samples them down to --fps and holds the last 1.5 s
    n = int(len(keep) * a.fps / 30)
    sel = [keep[min(len(keep) - 1, round(k * 30 / a.fps))] for k in range(n)] + [keep[-1]] * int(a.fps * 1.5)
    t = tempfile.mkdtemp()
    for i, f in enumerate(sel):
        load(f, a.gif_width).save(f"{t}/{i:05d}.png")
    ff = ["ffmpeg", "-loglevel", "error", "-y", "-framerate", str(a.fps), "-i", f"{t}/%05d.png"]
    subprocess.run(ff + ["-vf", "palettegen=stats_mode=full", f"{t}/p.png"], check=True)
    # dither none: dithered GIFs of 3D scenes come out 2-3x bigger
    subprocess.run(ff + ["-i", f"{t}/p.png", "-lavfi", "paletteuse=dither=none:diff_mode=rectangle", "-loop", "0", a.gif],
                   check=True)
    t2 = tempfile.mkdtemp()
    for i, f in enumerate(keep + [keep[-1]] * 45):
        load(f, 1280).save(f"{t2}/{i:05d}.png")
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-framerate", "30", "-i", f"{t2}/%05d.png", "-c:v", "libx264",
                    "-pix_fmt", "yuv420p", "-crf", str(a.crf), "-preset", "slow", "-movflags", "+faststart", a.mp4],
                   check=True)
    load(fs[a.poster], 1280).save(a.mp4.rsplit(".", 1)[0] + ".jpg", quality=85)
    print(a.gif, os.path.getsize(a.gif), a.mp4, os.path.getsize(a.mp4))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    def game_opts(s):
        s.add_argument("--bin", default=str(ROOT / "build/port-sdl/dw2003"))
        s.add_argument("--config")
        s.add_argument("--state")
        s.add_argument("--env", action="append", default=[])
        s.add_argument("--poke", action="append", default=[])
        s.add_argument("--save")
        s.add_argument("--soft", action="store_true")
        s.add_argument("--card1")
        s.add_argument("--card2")

    s = sub.add_parser("seq")
    game_opts(s)
    s.add_argument("out")
    s.add_argument("steps")
    s.set_defaults(fn=cmd_seq)
    s = sub.add_parser("rec")
    game_opts(s)
    s.add_argument("--pre", type=int, default=0)
    s.add_argument("--every", type=int, default=2)
    s.add_argument("out")
    s.add_argument("n", type=int)
    s.add_argument("presses", nargs="?", default="")
    s.set_defaults(fn=cmd_rec)
    s = sub.add_parser("sheet")
    s.add_argument("dir")
    s.add_argument("step", type=int)
    s.add_argument("dst")
    s.set_defaults(fn=cmd_sheet)
    s = sub.add_parser("clip")
    s.add_argument("dir")
    s.add_argument("gif")
    s.add_argument("mp4")
    s.add_argument("ranges")
    s.add_argument("poster", type=int)
    s.add_argument("--fps", type=float, default=8)
    s.add_argument("--gif-width", type=int, default=480)
    s.add_argument("--crf", type=int, default=25)
    s.set_defaults(fn=cmd_clip)
    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
