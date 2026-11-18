#!/usr/bin/env python3
"""Check the story rewrite's sources (tools/text_override/en_story.txt, en_kaili.txt, en_choices.txt, en_echoes.txt,
pt_kaili.txt) against the disc's English text: the same speaker plate, the same control codes (besides line and page breaks), the box's size
(22 letters a line in balloons, 38 in unplated narration; 3 lines a page), the page structure (each page ends with
<2,2>, every later page starts with <3>), and no gendered word about the hero.

  tools/venv/bin/python tools/check_story_text.py            # every source; exit 1 on an error
  tools/venv/bin/python tools/check_story_text.py FILE.txt   # one file (same formats)
"""
import re
import subprocess
import sys
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRCS = [ROOT / "tools/text_override" / n for n in ("en_story.txt", "en_kaili.txt", "en_choices.txt", "en_echoes.txt",
                                                  "pt_kaili.txt")]
BREAKS = {"<1>", "<2,2>", "<3>"}
GENDERED = re.compile(r"\b(he|him|his|himself|boy|boys|guy|lad|son|young man|gentleman|mister|mr\.)\b", re.I)


@lru_cache(maxsize=None)
def disc_table(name, sub):
    args = [sys.executable, str(ROOT / "tools/dump_text.py"), name] + (["-s", str(sub)] if sub is not None else [])
    out = subprocess.run(args, capture_output=True, text=True, check=True).stdout
    entries, cur = {}, None
    for line in out.splitlines():
        m = re.match(r"\s*(\d+): (.*)$", line)
        if m and not line.startswith("    "):
            cur = int(m[1])
            entries[cur] = m[2]
        elif cur is not None and line.startswith("    "):
            entries[cur] += line.strip()
    return entries


def plate(t):
    m = re.match(r"(<7>.*?<7>)", t)
    return m[1] if m else ""


def codes(t):
    return sorted(c for c in re.findall(r"<[\d,]+>", re.sub(r"^<7>.*?<7>", "", t)) if c not in BREAKS)


def check(text, orig, where, kaili=False):
    errs = []
    if orig is None:
        return [f"{where}: no such entry on the disc"]
    if text.startswith(">hero "):
        text = text[6:]
    elif plate(text) != plate(orig) and not kaili:
        errs.append(f"{where}: plate {plate(text)!r} (the disc's {plate(orig)!r})")
    if codes(text) != codes(orig):
        errs.append(f"{where}: codes {codes(text)} (the disc's {codes(orig)})")
    body = text[len(plate(text)):]
    width = 22 if plate(orig) else 38
    if not body.endswith("<2,2>"):
        errs.append(f"{where}: does not end with <2,2>")
    pages = body.split("<2,2>")[:-1]
    for k, page in enumerate(pages):
        if k > 0 and not page.startswith("<3>"):
            errs.append(f"{where}: page {k + 1} does not start with <3>")
        segs = page.removeprefix("<3>").split("<1>")
        if len(segs) > 3:
            errs.append(f"{where}: page {k + 1} has {len(segs)} lines")
        for s in segs:
            n = len(re.sub(r"<[\d,]+>", "", s))
            if n > width:
                errs.append(f"{where}: line {s!r} has {n} letters (at most {width})")
    if not kaili:
        for m in GENDERED.finditer(re.sub(r"<[\d,]+>", " ", body)):
            errs.append(f"{where}: check the hero is not gendered: {m[0]!r}")
    return errs


def key(name):
    base, _, sub = name.partition(":")
    return base, (int(sub) if sub else None)


def main():
    files = [Path(a) for a in sys.argv[1:]] or SRCS
    errs, warns, n = [], [], 0
    for path in files:
        if not path.exists():
            continue
        for ln, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            where = f"{path.name}:{ln}"
            kaili = "kaili" in path.name
            e = re.match(r"Q\d+=[123](?:,[123])*\s+(kaili\s+)?(\S+)\s+(\d+):\s*(.*)$", line)
            if e:
                name, idx, text, kaili = e[2], int(e[3]), e[4], bool(e[1])
                n += 1
                base, sub = key(name)
                for err in check(text, disc_table(base, sub).get(idx), where, kaili=kaili):
                    (warns if "check the hero" in err else errs).append(err)
                continue
            m = re.match(r"Q(\d+)\s+(\S+)\s+(\d+)@([123]):\s*(.*?)\s*\|\s*(.*)$", line)
            if m:
                name, idx, label, text = m[2], int(m[3]), m[5], m[6]
                if len(label) > 26:
                    errs.append(f"{where}: label longer than 26 letters")
            else:
                m = re.match(r"(\S+)\s+(\d+):\s*(.*)$", line)
                if not m:
                    errs.append(f"{where}: not `FILE index: text`")
                    continue
                name, idx, text = m[1], int(m[2]), m[3]
            n += 1
            base, sub = key(name)
            for err in check(text, disc_table(base, sub).get(idx), where, kaili=kaili):
                (warns if "check the hero" in err else errs).append(err)
    print("\n".join(errs + warns))
    print(f"{n} lines: {len(errs)} errors, {len(warns)} gender warnings")
    sys.exit(1 if errs else 0)


if __name__ == "__main__":
    main()
