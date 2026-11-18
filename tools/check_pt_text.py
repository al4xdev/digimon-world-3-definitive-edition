#!/usr/bin/env python3
"""Check a Portuguese (PT-BR) translation against its English source: same entries in the same order, every glyph
encodable, the control codes kept, the speech boxes respected (3 lines a page, letters and pixels a line, and a pace
limit per page so a long speech is split into more pages instead of being crammed into one).

  # one translated chunk against its source chunk (same `## file` sections and lines; the work of one subagent)
  tools/venv/bin/python tools/check_pt_text.py --chunk IN.txt OUT.txt

  # the repository's own pairs: en.txt/pt.txt (only the keys en.txt has), en_story/pt_story, en_choices/pt_choices,
  # en_echoes/pt_echoes, en_kaili/pt_kaili (speaker plates are compared through tools/text_override/pt_plates.txt)
  tools/venv/bin/python tools/check_pt_text.py

Exit 1 on an error; warnings (a heavy page, a gendered word the Kaili mode's name swap would break) only print.
"""
import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import dump_text as dt  # noqa: E402
import gen_text_override as g  # noqa: E402
import translate_ptbr as tp  # noqa: E402  (its font metrics only)

TXT = ROOT / "tools/text_override"
PAIRS = [("en.txt", "pt.txt"), ("en_story.txt", "pt_story.txt"), ("en_choices.txt", "pt_choices.txt"),
         ("en_echoes.txt", "pt_echoes.txt"), ("en_kaili.txt", "pt_kaili.txt")]
PLATES = TXT / "pt_plates.txt"

ENTRY = re.compile(r"^(\$\w+|[A-Za-z0-9_]+(?::\d+)? \d+|Q\d+ \S+ \d+@\d|Q\d+=\S+ (?:kaili )?\S+ \d+):\s?(.*)$")
CODE = re.compile(r"<[\d,]+>")
BREAKS = {"<1>", "<2,2>", "<3>"}
PLATE = re.compile(r"^<7>.*?<7>")
# balloon boxes: a plated balloon holds 22 letters a line (the widest English line is 220 px), narration 38 (296 px)
LIMITS = {True: (22, 222, 66), False: (38, 300, 100)}  # plated?: (letters a line, pixels a line, letters a page)
HEAVY = {True: 58, False: 90}
NAME_WORDS = re.compile(r"(?<![A-Za-z])(Junior|Ivy|Kail)(?![a-z])")
# Girls Power swaps Junior and Ivy; a Portuguese word that marks the gender of someone named would be wrong after it
GENDER_NEAR_NAME = re.compile(r"\b(ele|ela|dele|dela|seu|sua|o|a)\s+(Junior|Ivy|Kail)\b|\b(Junior|Ivy|Kail)\b.{0,40}"
                              r"\b(ele|ela|dele|dela|sozinh[oa]|cansad[oa]|obrigad[oa])\b", re.I)
HERO_GENDER = re.compile(r"\b(sozinh[oa]|obrigad[oa]|cansad[oa]|pronto|pronta|nervos[oa]|preocupad[oa]|surpres[oa]|"
                         r"sortud[oa]|louc[oa]|confus[oa]|sozinh[oa]s?|[a-zà-ú]+(?:ado|ada|ido|ida))\b", re.I)

_INV = None


def inv():
    global _INV
    if _INV is None:
        _INV = g.inverse(dt.char_tables())
    return _INV


def split_entry(line):
    m = ENTRY.match(line)
    return (m[1], m[2]) if m else None


def read_sections(path):
    """{section: [(key, text, line number)]} of a file with `## name` sections (a plain file is one section '')."""
    out, cur = {}, ""
    for n, line in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        if line.startswith("## "):
            cur = line[3:].strip()
            out.setdefault(cur, [])
            continue
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        e = split_entry(line)
        out.setdefault(cur, []).append((e[0], e[1], n) if e else (None, line, n))
    return out


def vis(s):
    return len(CODE.sub("", s))


def px(line):
    w, i = 0, 0
    while i < len(line):
        m = CODE.match(line, i)
        if m:
            if int(m[0][1:-1].split(",")[0]) in (5, 8):
                w += 42
            i = m.end()
            continue
        w += tp.CHAR_PX.get(g.ALIAS.get(line[i], line[i]), 6)
        i += 1
    return w


def glyph_errors(text):
    bad = []
    for part in CODE.split(text):
        for ch in part:
            if g.ALIAS.get(ch, ch) not in inv():
                bad.append(ch)
    return sorted(set(bad))


def kind_of(section, key, en):
    """dialogue | guide | ui | inf | name | choice_label, by where the entry lives."""
    if key.startswith("Q") and "@" in key:
        return "dialogue"
    if key.startswith("$"):
        if en.startswith("<7>") or en.startswith(">hero "):
            return "dialogue"
        return "guide" if re.match(r"\$(Q|M|Auc|Dri)", key) else "ui"
    f = key.split()[0].split(":")[0].upper()
    if f in ("SKLINF", "ITMINF", "TRAINI", "STATUS"):
        return "dialogue" if en.startswith("<7>") or f == "TRAINI" and "<2,2>" in en else "inf"
    if f in ("ITMNAM", "SKLNAM"):
        return "name"
    return "dialogue"


def split_choice(text):
    label, _, body = text.partition(" | ")
    return label.strip(), body.strip()


def check_dialogue(where, en, pt, plate_map, errs, warns, strict_plate=True):
    hero_prefix = en.startswith(">hero ")
    if hero_prefix != pt.startswith(">hero "):
        errs.append(f"{where}: the `>hero ` prefix must be kept exactly as in the English")
    en_b = en[6:] if hero_prefix else en
    pt_b = pt[6:] if pt.startswith(">hero ") else pt
    ep, pp = PLATE.match(en_b), PLATE.match(pt_b)
    plated = bool(ep)
    if bool(ep) != bool(pp):
        errs.append(f"{where}: speaker plate {'missing' if ep else 'added'} (English: {ep[0] if ep else 'none'})")
    elif ep and strict_plate and pp[0] != plate_map.get(ep[0], ep[0]):
        errs.append(f"{where}: plate {pp[0]!r} must stay {plate_map.get(ep[0], ep[0])!r}: copy the English plate untouched")
    en_body = en_b[ep.end():] if ep else en_b
    pt_body = pt_b[pp.end():] if pp else pt_b
    en_codes = [c for c in CODE.findall(en_body) if c not in BREAKS]
    pt_codes = [c for c in CODE.findall(pt_body) if c not in BREAKS]
    if sorted(en_codes) != sorted(pt_codes):
        errs.append(f"{where}: control codes {pt_codes} (the English's {en_codes})")
    if en_body.endswith("<2,2>") and not pt_body.endswith("<2,2>"):
        errs.append(f"{where}: must end with <2,2> like the English")
    if not en_body.endswith("<2,2>") and pt_body.endswith("<2,2>"):
        errs.append(f"{where}: must not end with <2,2> (the English does not)")
    letters, pixels, per_page = LIMITS[plated]
    pages = pt_body.split("<2,2>")
    if pt_body.endswith("<2,2>"):
        pages = pages[:-1]
    for k, page in enumerate(pages):
        if k > 0 and not page.startswith("<3>"):
            errs.append(f"{where}: page {k + 1} must start with <3>")
        if k == 0 and page.startswith("<3>"):
            warns.append(f"{where}: first page starts with <3> (the English's does not unless it does)")
        segs = page.removeprefix("<3>").split("<1>")
        if len(segs) > 3:
            errs.append(f"{where}: page {k + 1} has {len(segs)} lines (at most 3): split it with <2,2><3>")
        total = 0
        for s in segs:
            n = vis(s)
            total += n
            if n > letters:
                errs.append(f"{where}: line {s!r} has {n} letters (at most {letters})")
            elif px(s) > pixels:
                errs.append(f"{where}: line {s!r} is {px(s)} px wide (at most {pixels})")
        if total > per_page:
            errs.append(f"{where}: page {k + 1} has {total} letters (at most {per_page}): it would show too fast, split it "
                        f"with <2,2><3>")
        elif total > HEAVY[plated]:
            warns.append(f"{where}: page {k + 1} is heavy ({total} letters): consider an extra page")
        if len(pages) > 1 and 0 < total < 6:
            warns.append(f"{where}: page {k + 1} is tiny ({total} letters): merge it")
    if NAME_WORDS.search(pt_b):
        if GENDER_NEAR_NAME.search(CODE.sub(" ", pt_body)):
            warns.append(f"{where}: Girls Power swaps Junior/Ivy: a gendered word near a name ({pt_body[:50]!r}...)")
    if pp and pp[0] == "<7><9><7>" or hero_prefix:
        m = HERO_GENDER.search(CODE.sub(" ", pt_body))
        if m:
            warns.append(f"{where}: the hero is neutral: gendered word {m[0]!r}")
    for ch in glyph_errors(pt):
        errs.append(f"{where}: no glyph for {ch!r}")


def check_short(where, kind, key, en, pt, errs, warns):
    if "<2,2>" in pt and "<2,2>" not in en:
        errs.append(f"{where}: stray <2,2>")
    if sorted(c for c in CODE.findall(en) if c not in BREAKS) != sorted(c for c in CODE.findall(pt) if c not in BREAKS):
        errs.append(f"{where}: control codes differ from the English")
    flat = pt.replace("<1>", " ")
    if kind == "inf" and vis(flat) > 34:
        errs.append(f"{where}: {vis(flat)} characters (item/technique descriptions hold 34)")
    if kind == "name":
        lim = 16 if key.startswith("ITMNAM") else max(14, vis(en))
        if vis(flat) > lim:
            errs.append(f"{where}: {vis(flat)} characters (this name holds {lim})")
    if kind in ("guide", "ui"):
        en_lines, pt_lines = en.split("<1>"), pt.split("<1>")
        en_px = max(px(x) for x in en_lines)
        for s in pt_lines:
            if kind == "guide" and vis(s) > 44:
                errs.append(f"{where}: line {s!r} has {vis(s)} characters (at most 44)")
            elif px(s) > en_px * 1.3 + 8:
                errs.append(f"{where}: line {s!r} is {px(s)} px, the English's widest is {en_px}: shorten it")
            elif px(s) > en_px * 1.15:
                warns.append(f"{where}: line {s!r} is wider than the English ({px(s)} vs {en_px} px)")
        if len(pt_lines) > len(en_lines) + 1:
            errs.append(f"{where}: {len(pt_lines)} lines for the English's {len(en_lines)}")
    for ch in glyph_errors(pt):
        errs.append(f"{where}: no glyph for {ch!r}")


def compare(section, en_rows, pt_rows, plate_map, errs, warns, label=""):
    if len(en_rows) != len(pt_rows):
        errs.append(f"{label}{section}: {len(pt_rows)} entries, the English has {len(en_rows)}")
    extra = section == "pt_kaili_extra"  # optional: a Girls Power twin of a story line; its plate may differ
    for (ek, et, _), (pk, pt, pl) in zip(en_rows, pt_rows):
        where = f"{label}{section}:{pl}"
        if pk is None:
            errs.append(f"{where}: not an entry line: {pt[:60]!r}")
            continue
        if ek != pk:
            errs.append(f"{where}: key {pk!r} where the English has {ek!r} (same order, same keys)")
            continue
        if ek.startswith("Q") and "@" in ek:
            e_label, e_text = split_choice(et)
            p_label, p_text = split_choice(pt)
            if " | " not in pt:
                errs.append(f"{where}: a choice is `label | balloon text`")
                continue
            if vis(p_label) > 26:
                errs.append(f"{where}: label {p_label!r} has {vis(p_label)} letters (at most 26)")
            for ch in glyph_errors(p_label):
                errs.append(f"{where}: no glyph for {ch!r} in the label")
            check_dialogue(where, e_text, p_text, plate_map, errs, warns)
            continue
        kind = kind_of(section, ek, et)
        if kind == "dialogue":
            check_dialogue(where, et, pt, plate_map, errs, warns, strict_plate=not extra)
        else:
            check_short(where, kind, ek, et, pt, errs, warns)


def load_plate_map():
    out = {}
    if PLATES.exists():
        for line in PLATES.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.startswith("#") and " => " in line:
                a, b = line.split(" => ", 1)
                out[a.strip("\n")] = b
    return out


SNAP = TXT / "pt_source.json"  # {pt file: {key: hash of the English the translation was made from}}


def h(text):
    return hashlib.sha1(text.encode("utf-8")).hexdigest()[:10]


def todo(snapshot):
    """Per PT file: keys the English has and the translation lacks (new), and keys translated from an English text that has
    changed since (stale: the hash in pt_source.json differs; `--snapshot` records the hashes after a translation)."""
    snap = json.loads(SNAP.read_text()) if SNAP.exists() else {}
    new_snap, total_new, total_stale = {}, 0, 0
    for en_name, pt_name in PAIRS:
        if en_name == "en.txt" or not (TXT / en_name).exists():
            continue
        en = {r[0]: r[1] for r in read_sections(TXT / en_name)[""] if r[0]}
        pt = {r[0] for r in read_sections(TXT / pt_name)[""] if r[0]} if (TXT / pt_name).exists() else set()
        old = snap.get(pt_name, {})
        if snapshot:
            new_snap[pt_name] = {k: h(en[k]) for k in pt if k in en}
            continue
        new = [k for k in en if k not in pt]
        stale = [k for k in pt if k in en and k in old and old[k] != h(en[k])]
        nosnap = [k for k in pt if k in en and k not in old]
        print(f"{pt_name}: {len(new)} new, {len(stale)} stale, {len(nosnap)} translated without a snapshot")
        for k in new[:15] + stale[:15]:
            print(f"   {'STALE' if k in stale else 'NEW  '} {k}")
        total_new += len(new)
        total_stale += len(stale)
    if snapshot:
        SNAP.write_text(json.dumps(new_snap, indent=0, sort_keys=True) + "\n")
        print(f"{SNAP.name}: {sum(len(v) for v in new_snap.values())} translated keys recorded")
    else:
        print(f"{total_new} new, {total_stale} stale: translate those (tools/text_override/en*.txt vs pt_*.txt)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--chunk", nargs=2, metavar=("IN", "OUT"))
    ap.add_argument("--todo", action="store_true", help="list new and stale entries (the English changed since translated)")
    ap.add_argument("--snapshot", action="store_true", help="record the current English of every translated key")
    a = ap.parse_args()
    if a.todo or a.snapshot:
        return todo(a.snapshot)
    errs, warns = [], []
    n = 0
    if a.chunk:
        src, out = read_sections(a.chunk[0]), read_sections(a.chunk[1])
        for sec, rows in src.items():
            if sec not in out:
                errs.append(f"section '## {sec}' is missing from the output")
                continue
            compare(sec, rows, out[sec], {}, errs, warns)
            n += len(rows)
        known = {k: t for rows in src.values() for k, t, _ in rows if k}
        for sec, rows in out.items():
            if sec == "pt_kaili_extra":  # optional twins, checked against the story/en line of the same key
                compare(sec, [(k, known[k], 0) for k, _, _ in rows if k in known],
                        [r for r in rows if r[0] in known], {}, errs, warns)
                errs += [f"pt_kaili_extra:{ln}: {k!r} is not an entry of this chunk" for k, _, ln in rows if k not in known]
            elif sec not in src:
                errs.append(f"section '## {sec}' is not in the source")
    else:
        plate_map = load_plate_map()
        for en_name, pt_name in PAIRS:
            en_p, pt_p = TXT / en_name, TXT / pt_name
            if not pt_p.exists():
                continue
            en_rows = read_sections(en_p)[""]
            pt_rows = read_sections(pt_p)[""]
            # the PT file may list its entries in any order (and pt.txt also holds the disc's entries the English never
            # touched): pair by key, in the English's order, a repeated key taking its rows in turn
            queues = {}
            for r in pt_rows:
                queues.setdefault(r[0], []).append(r)
            paired_en, paired_pt = [], []
            for r in en_rows:
                q = queues.get(r[0])
                if q:
                    paired_en.append(r)
                    paired_pt.append(q.pop(0))
                elif en_name != "en.txt":
                    errs.append(f"{pt_name}: missing {r[0]!r}")
            for k, q in queues.items():
                if q and en_name != "en.txt" and k not in {r[0] for r in en_rows}:
                    errs.append(f"{pt_name}:{q[0][2]}: {k!r} is not in {en_name}")
            en_rows, pt_rows = paired_en, paired_pt
            compare(pt_name, en_rows, pt_rows, plate_map, errs, warns)
            n += len(en_rows)
    print("\n".join(errs))
    if warns:
        print("\n".join(sorted(set(warns))[:400]))
    print(f"{n} entries: {len(errs)} errors, {len(set(warns))} warnings")
    sys.exit(1 if errs else 0)


if __name__ == "__main__":
    main()
