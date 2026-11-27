#!/usr/bin/env python3
"""The story layer in the other languages: check one against its source, the Portuguese, and move entries between the
language's files and the chunks a translator works on.

A language's story layer is tools/text_override/<lang>_story.txt, _choices.txt, _echoes.txt and _kaili.txt (lang: fr de
it es; tools/gen_text_override.py STORY_LANGS), the same formats as pt_*.txt: every entry the Portuguese has, with
- the same speakers, each plate being the disc's own name in that language for the speaker of the English entry's plate
  (<lang>_plates.txt, `--plates` makes it from the disc);
- the same control codes (but line and page breaks), the same names the Kaili mode swaps (Junior, Ivy, Kail), the same
  `>hero ` prefix, and a choice's effect tag (`[teddy:+1]`) as the source's;
- the box's limits (tools/check_pt_text.py's: 22 letters a line in a balloon, 38 in narration, 3 lines a page, a pace limit
  per page; every page ends with <2,2>, every later page starts with <3>).
Beyond the source's entries a language may need Kaili-mode lines of its own (a line whose words take the hero's or a
friend's gender, which the names swap cannot fix): a story entry in <lang>_kaili.txt, an echo's `kaili` twin in
<lang>_echoes.txt; written as the Kaili mode says them (Junior and Ivy swapped, Kail as Junior). A choice has none: its
lines are said as written in both heroes' games, so they are written without gendered words about the hero.

  tools/venv/bin/python tools/check_story_lang.py fr               # check fr_*.txt; exit 1 on an error
  tools/venv/bin/python tools/check_story_lang.py --all            # fr de it es
  tools/venv/bin/python tools/check_story_lang.py fr --todo        # entries missing or stale (the Portuguese changed)
  tools/venv/bin/python tools/check_story_lang.py fr --export DIR [--words 6000] [--all-entries]
        # the missing/stale entries as chunks to translate: DIR/NN.src.txt (the Portuguese, the plates already in the
        # language) and DIR/NN.en.txt (the English, for reference); a chunk keeps a scene's entries together
  tools/venv/bin/python tools/check_story_lang.py fr --chunk DIR/NN.src.txt DIR/NN.out.txt   # a translated chunk
  tools/venv/bin/python tools/check_story_lang.py fr --merge DIR/NN.out.txt ...   # chunks into fr_*.txt (+ story_source.json)
  tools/venv/bin/python tools/check_story_lang.py --plates         # write <lang>_plates.txt from the disc

A chunk's `## kaili_extra` section holds the Kaili-mode lines of its own a translation needs (a story entry's key, or an
echo's key with `kaili`, e.g. `Q18=2 kaili DMG200:32 3: ...`).
"""
import argparse
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import check_pt_text as cp  # noqa: E402
import gen_text_override as g  # noqa: E402

TXT = ROOT / "tools/text_override"
SOURCE = g.STORY_SOURCE
LANGS = [c for c in g.STORY_LANGS if c not in ("en", "pt")]
KINDS = ["story", "choices", "echoes", "kaili"]
SNAP = TXT / "story_source.json"  # {lang: {kind: {key: hash of the Portuguese it was translated from}}}
PLATE = re.compile(r"<7>.*?<7>")
NAME_WORDS = cp.NAME_WORDS
EFFECT = re.compile(r"\s*(\[[^\]]+\])\s*$")
# A word that takes the hero's gender in a line the hero says (Kaili mode: the hero is a girl). Warnings only: the
# critics read the rest (a line said to the hero, a friend named).
HERO_GENDER = {
    "fr": r"\b(prêt|prête|seul|seule|fatigué|fatiguée|désolé|désolée|sûr|sûre|content|contente|heureux|heureuse|"
          r"nerveux|nerveuse|surpris|surprise|perdu|perdue|ravi|ravie|inquiet|inquiète|fou|folle|venu|venue|allé|allée)\b",
    "es": r"\b(listo|lista|cansado|cansada|seguro|segura|contento|contenta|nervioso|nerviosa|preocupado|preocupada|"
          r"sorprendido|sorprendida|perdido|perdida|loco|loca|agradecido|agradecida|solito|solita|tranquilo|tranquila)\b",
    "it": r"\b(pronto|pronta|stanco|stanca|sicuro|sicura|contento|contenta|nervoso|nervosa|preoccupato|preoccupata|"
          r"sorpreso|sorpresa|perso|persa|pazzo|pazza|grato|grata|venuto|venuta|andato|andata|arrivato|arrivata)\b",
    "de": r"\b(Junge|Mädchen|Bursche|Kerl|Typ)\b",
}


def h(text):
    return hashlib.sha1(text.encode("utf-8")).hexdigest()[:10]


def path(lang, kind):
    return TXT / f"{lang}_{kind}.txt"


def entries(p):
    """[(key, text)] of a file in the story formats (`## sections` ignored: a plain file)."""
    if not p.exists():
        return []
    out = []
    for rows in cp.read_sections(p).values():
        for k, t, n in rows:
            if k is None:
                sys.exit(f"{p.name}:{n}: not an entry line: {t[:60]!r}")
            out.append((k, t))
    return out


def group_of(kind, key):
    """The FILE[:sub] an entry is in (a chunk keeps a scene's entries together)."""
    toks = key.split()
    return toks[1] if kind == "choices" else toks[-2] if kind == "echoes" else toks[0]


def entry_of(kind, key):
    """The (FILE[:sub], index) of an entry: a choice's and an echo's entry too."""
    toks = key.split()
    if kind == "choices":
        return toks[1], int(toks[2].split("@")[0])
    if kind == "echoes":
        return toks[-2], int(toks[-1])
    return toks[0], int(toks[1])


def plain_echo(key):
    return key.replace(" kaili ", " ", 1)


def kaili_echo(key):
    q, rest = key.split(" ", 1)
    return f"{q} kaili {rest}"


def load_plates(lang):
    p = TXT / f"{lang}_plates.txt"
    out = {}
    if p.exists():
        for line in p.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.startswith("#") and " => " in line:
                a, b = line.split(" => ", 1)
                out[a] = b
    return out


def make_plates():
    """<lang>_plates.txt: every speaker plate of the English story files => the plate the language's disc gives the same
    speaker (the entries that have the English plate on the English disc, the same entries on the language's)."""
    import dump_text as dt
    import disc_files as df
    tables = dt.char_tables()
    discs = {c: g.STORY_DISC[c] for c in LANGS}
    files = {}
    for e in df.files():
        p = e.path.split("/")
        if len(p) == 5 and p[2] == "COUNTRY" and p[3] in ["ENG"] + list(discs.values()):
            files.setdefault(p[4][2:-4], {})[p[3]] = e.path
    votes = {c: {} for c in LANGS}
    for name, by in files.items():
        if len(by) != 1 + len(discs):
            continue
        dec = {}
        for d_lang, fp in by.items():
            d = (df.DISC / fp).read_bytes()
            dec[d_lang] = {(sub, i): dt.decode(d, o, tables)[0] for sub, offs in dt.tables_of(d) for i, o in enumerate(offs)}
        for k, t in dec["ENG"].items():
            m = re.match(r"<7>.*?<7>", t)
            if not m or m[0] == "<7><9><7>":
                continue
            for c, d_lang in discs.items():
                m2 = re.match(r"<7>.*?<7>", dec[d_lang].get(k, ""))
                if m2:
                    votes[c].setdefault(m[0], Counter())[m2[0]] += 1
    used = set()
    for kind in KINDS:
        for k, t in entries(path("en", kind)):
            used.update(p for p in PLATE.findall(t) if p != "<7><9><7>")
    for c in LANGS:
        lines = [f"# Speaker plates: the English plate => the plate the {g.STORY_DISC[c]} disc gives the same speaker",
                 "# (tools/check_story_lang.py --plates; a plate the disc does not have keeps the English)."]
        missing = 0
        for p in sorted(used):
            v = votes[c].get(p)
            if v is None:
                missing += 1
            lines.append(f"{p} => {v.most_common(1)[0][0] if v else p}")
        (TXT / f"{c}_plates.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"{c}_plates.txt: {len(used)} plates, {missing} not on the disc")


def want_plates(lang, kind, key, src_text, en, plates):
    """The plates the language's entry must have: the English entry's, as the language's disc names those speakers (the
    source's own when the English entry has a different count)."""
    en_text = en.get((kind, key))
    src_p = PLATE.findall(src_text)
    if en_text is not None:
        en_p = PLATE.findall(en_text)
        if len(en_p) == len(src_p):
            return [plates.get(p, p) for p in en_p]
    return src_p


def split_choice(text):
    label, _, body = text.partition(" | ")
    m = EFFECT.search(label)
    return label[:m.start()].strip() if m else label.strip(), m[1] if m else "", body.strip()


def check_dialogue(where, lang, src, out, plates, errs, warns, kaili=False):
    """`out` against its source `src` (a Kaili-mode line of the language's own: against the source with the names
    swapped)."""
    if src.startswith(">hero ") != out.startswith(">hero "):
        errs.append(f"{where}: the `>hero ` prefix must be as the source's")
    s, o = src.removeprefix(">hero "), out.removeprefix(">hero ")
    got = PLATE.findall(o)
    if got != plates:
        errs.append(f"{where}: speaker plates {got} must be {plates}")
    s_body, o_body = PLATE.sub("", s), PLATE.sub("", o)
    s_codes = sorted(c for c in cp.CODE.findall(s_body) if c not in cp.BREAKS)
    o_codes = sorted(c for c in cp.CODE.findall(o_body) if c not in cp.BREAKS)
    if s_codes != o_codes:
        errs.append(f"{where}: control codes {o_codes} (the source's {s_codes})")
    s_names = Counter(NAME_WORDS.findall(g.names_swap(s) if kaili else s))
    o_names = Counter(NAME_WORDS.findall(o))
    if s_names != o_names:
        errs.append(f"{where}: names {dict(o_names)} (the source's {dict(s_names)}): keep Junior, Ivy and Kail as they"
                    " are, the Kaili mode swaps them")
    if s.endswith("<2,2>") != o.endswith("<2,2>"):
        errs.append(f"{where}: {'must' if s.endswith('<2,2>') else 'must not'} end with <2,2> like the source")
    plated = bool(PLATE.match(o))
    letters, pixels, per_page = cp.LIMITS[plated]
    pages = o.split("<2,2>")
    if o.endswith("<2,2>"):
        pages = pages[:-1]
    for k, page in enumerate(pages):
        page = PLATE.sub("", page) if not page.startswith("<3>") else "<3>" + PLATE.sub("", page[3:])
        if k > 0 and not page.startswith("<3>"):
            errs.append(f"{where}: page {k + 1} must start with <3>")
        if k == 0 and page.startswith("<3>") and not s.removeprefix(PLATE.match(s)[0] if PLATE.match(s) else "").startswith("<3>"):
            warns.append(f"{where}: the first page starts with <3>")
        segs = page.removeprefix("<3>").split("<1>")
        if len(segs) > 3:
            errs.append(f"{where}: page {k + 1} has {len(segs)} lines (at most 3): split it with <2,2><3>")
        total = 0
        for seg in segs:
            n = cp.vis(seg)
            total += n
            # the box's real limit is its width in pixels; a line a letter or two over the letters fits when they are
            # narrow (the Portuguese has some)
            if cp.px(seg) > pixels:
                errs.append(f"{where}: line {seg!r} is {cp.px(seg)} px wide (at most {pixels}; {n} letters)")
            elif n > letters + 2:
                errs.append(f"{where}: line {seg!r} has {n} letters (at most {letters})")
            elif n > letters:
                warns.append(f"{where}: line {seg!r} has {n} letters (at most {letters}; its pixels fit)")
        if total > per_page:
            errs.append(f"{where}: page {k + 1} has {total} letters (at most {per_page}): split it with <2,2><3>")
        elif total > cp.HEAVY[plated]:
            warns.append(f"{where}: page {k + 1} is heavy ({total} letters)")
        if len(pages) > 1 and 0 < total < 6:
            warns.append(f"{where}: page {k + 1} is tiny ({total} letters): merge it")
    if (got[:1] == ["<7><9><7>"] or out.startswith(">hero ")) and lang in HERO_GENDER:
        m = re.search(HERO_GENDER[lang], cp.CODE.sub(" ", o_body))
        if m:
            warns.append(f"{where}: the hero speaks: {m[0]!r} takes a gender (Kaili mode: the hero is a girl)")
    for ch in cp.glyph_errors(out):
        errs.append(f"{where}: no glyph for {ch!r}")


def check_entry(where, lang, kind, src, out, plates, errs, warns, kaili=False):
    if kind == "choices":
        s_label, s_fx, s_text = split_choice(src)
        if " | " not in out:
            errs.append(f"{where}: a choice is `label [effect] | balloon text`")
            return
        o_label, o_fx, o_text = split_choice(out)
        if o_fx != s_fx:
            errs.append(f"{where}: effect tag {o_fx!r} must be the source's {s_fx!r}")
        if not o_label or cp.vis(o_label) > 26:
            errs.append(f"{where}: label {o_label!r} has {cp.vis(o_label)} letters (1 to 26)")
        if NAME_WORDS.search(o_text):
            errs.append(f"{where}: a choice names Junior, Ivy or Kail (the Kaili mode cannot swap it)")
        for ch in cp.glyph_errors(o_label):
            errs.append(f"{where}: no glyph for {ch!r} in the label")
        check_dialogue(where, lang, s_text, o_text, plates, errs, warns)
    else:
        check_dialogue(where, lang, src, out, plates, errs, warns, kaili)


def hero_keys():
    """The entries the English Kaili mode gives the hero (`>hero ` in en_kaili.txt): a line of a language's own there
    is the hero's too."""
    return {k for k, t in entries(path("en", "kaili")) if t.startswith(">hero ")}


def check_extra(where, lang, key, text, src_story, src_echoes, plates_of, errs, warns):
    """A Kaili-mode line of the language's own: a story entry's or an echo's `kaili` twin."""
    if " kaili " in key:
        base = plain_echo(key)
        if base not in src_echoes:
            errs.append(f"{where}: {key!r}: no echo {base!r} in the source")
            return
        src, kind = src_echoes[base], "echoes"
    else:
        if key not in src_story:
            errs.append(f"{where}: {key!r} is not a story entry of the source")
            return
        src, kind = src_story[key], "story"
    plates = [g.names_swap(p) for p in plates_of(kind, key if kind == "story" else base, src)]
    if kind == "story" and key in hero_keys():
        if not text.startswith(">hero "):
            errs.append(f"{where}: the English Kaili mode gives this line to the hero: start it with `>hero <7><9><7>`")
        src, plates = ">hero " + PLATE.sub("<7><9><7>", src, count=1), ["<7><9><7>"] + plates[1:]
    check_dialogue(where, lang, src, text, plates, errs, warns, kaili=True)


def check_echo_twins(where, kaili_story, echo_keys, twins, errs):
    """An echo on an entry the Kaili mode says its own way needs a `kaili` twin (gen_text_override.py's rule)."""
    for k in echo_keys:
        if " kaili " in k:
            continue
        n, i = entry_of("echoes", k)
        if f"{n} {i}" in kaili_story and kaili_echo(k) not in twins:
            errs.append(f"{where}: echo {k!r} is on an entry with a Kaili line of its own: give it a twin "
                        f"`{kaili_echo(k)}: ...` in ## kaili_extra")


def source_sets():
    src = {kind: entries(path(SOURCE, kind)) for kind in KINDS}
    en = {(kind, k): t for kind in KINDS for k, t in entries(path("en", kind))}
    return src, en


def check_lang(lang, partial=False):
    errs, warns = [], []
    src, en = source_sets()
    plates = load_plates(lang)
    if not plates:
        errs.append(f"{lang}_plates.txt is missing: run --plates")
    src_story = dict(src["story"])
    src_echoes = {k: t for k, t in src["echoes"] if " kaili " not in k}
    n = 0
    out_all = {}
    for kind in KINDS:
        s = dict(src[kind])
        out = entries(path(lang, kind))
        out_all[kind] = dict(out)
        seen = Counter(k for k, _ in out)
        for k, c in seen.items():
            if c > 1:
                errs.append(f"{lang}_{kind}.txt: {k!r} twice")
        for k, t in out:
            where = f"{lang}_{kind}.txt {k}"
            if k in s:
                check_entry(where, lang, kind, s[k], t, want_plates(lang, kind, k, s[k], en, plates), errs, warns)
                n += 1
            elif kind in ("kaili", "echoes") and (kind == "kaili" or " kaili " in k):
                check_extra(where, lang, k, t, src_story, src_echoes,
                            lambda kd, kk, st: want_plates(lang, kd, kk, st, en, plates), errs, warns)
                n += 1
            else:
                errs.append(f"{where}: not an entry of {SOURCE}_{kind}.txt")
        if not partial:
            missing = [k for k in s if k not in out_all[kind]]
            if missing:
                errs.append(f"{lang}_{kind}.txt: {len(missing)} entries of {SOURCE}_{kind}.txt missing (first: {missing[0]!r})")
    check_echo_twins(f"{lang}_echoes.txt", set(out_all["kaili"]), list(out_all["echoes"]), set(out_all["echoes"]), errs)
    return n, errs, warns


def todo(lang):
    snap = json.loads(SNAP.read_text()) if SNAP.exists() else {}
    src, _ = source_sets()
    out = []
    for kind in KINDS:
        have = dict(entries(path(lang, kind)))
        rec = snap.get(lang, {}).get(kind, {})
        for k, t in src[kind]:
            if k not in have:
                out.append((kind, k, "new"))
            elif k in rec and rec[k] != h(t):
                out.append((kind, k, "stale"))
    return out


def export(lang, outdir, words, everything):
    src, en = source_sets()
    plates = load_plates(lang)
    want = None if everything else {(kind, k) for kind, k, _ in todo(lang)}
    groups = {}  # FILE[:sub] -> {kind: [(key, text, en_text)]}, in the source's order
    for kind in KINDS:
        for k, t in src[kind]:
            if want is not None and (kind, k) not in want:
                continue
            wp = want_plates(lang, kind, k, t, en, plates)
            it = iter(wp)
            t2 = PLATE.sub(lambda m: next(it, m[0]), t)
            e = en.get((kind, k), "")
            it = iter(wp)
            e2 = PLATE.sub(lambda m: next(it, m[0]), e) if len(PLATE.findall(e)) == len(wp) else e
            groups.setdefault(group_of(kind, k), {}).setdefault(kind, []).append((k, t2, e2))
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    chunks, cur, size = [], [], 0
    for grp, kinds in groups.items():
        w = sum(len(t.split()) for rows in kinds.values() for _, t, _ in rows)
        if cur and size + w > words:
            chunks.append(cur)
            cur, size = [], 0
        cur.append(kinds)
        size += w
    if cur:
        chunks.append(cur)
    for n, chunk in enumerate(chunks, 1):
        for which, col in (("src", 1), ("en", 2)):
            lines = [f"# {lang} chunk {n}: {'the Portuguese source' if which == 'src' else 'the English, for reference only'}"]
            for kind in KINDS:
                rows = [r for kinds in chunk for r in kinds.get(kind, [])]
                if rows:
                    lines.append(f"## {kind}")
                    lines += [f"{r[0]}: {r[col]}" for r in rows]
            (outdir / f"{n:02}.{which}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{len(chunks)} chunks in {outdir} ({sum(len(r) for c in chunks for k in c for r in k.values())} entries)")


def read_chunk(p):
    out = {}
    for sec, rows in cp.read_sections(p).items():
        for k, t, n in rows:
            if k is None:
                sys.exit(f"{p}:{n}: not an entry line: {t[:60]!r}")
            out.setdefault(sec, []).append((k, t))
    return out


def check_chunk(lang, src_p, out_p):
    errs, warns = [], []
    src, out = read_chunk(src_p), read_chunk(out_p)
    for sec in out:
        if sec not in src and sec != "kaili_extra":
            errs.append(f"section '## {sec}' is not in the source chunk")
    for sec, rows in src.items():
        got = out.get(sec, [])
        if [k for k, _ in got] != [k for k, _ in rows]:
            miss = [k for k, _ in rows if k not in dict(got)]
            extra = [k for k, _ in got if k not in dict(rows)]
            errs.append(f"## {sec}: the keys must be the source's, in its order (missing {miss[:5]}, extra {extra[:5]})")
        g_ = dict(got)
        for k, t in rows:
            if k in g_:
                check_entry(f"{sec} {k}", lang, sec, t, g_[k], PLATE.findall(t.split(' | ', 1)[-1] if sec == 'choices' else t),
                            errs, warns)
    src_story = dict(src.get("story", []))
    src_echoes = {k: t for k, t in src.get("echoes", []) if " kaili " not in k}
    extra = out.get("kaili_extra", [])
    for k, t in extra:
        check_extra(f"kaili_extra {k}", lang, k, t, src_story, src_echoes, lambda kd, kk, st: PLATE.findall(st), errs, warns)
    kaili_story = {k for k, _ in src.get("kaili", [])} | {k for k, _ in extra if " kaili " not in k}
    twins = {k for k, _ in src.get("echoes", []) if " kaili " in k} | {k for k, _ in extra if " kaili " in k}
    check_echo_twins("echoes", kaili_story, list(src_echoes), twins, errs)
    n = sum(len(r) for r in src.values()) + len(extra)
    return n, errs, warns


def merge(lang, outs):
    src, _ = source_sets()
    have = {kind: dict(entries(path(lang, kind))) for kind in KINDS}
    extras = {}
    got = {kind: {} for kind in KINDS}
    for p in outs:
        for sec, rows in read_chunk(p).items():
            for k, t in rows:
                if sec == "kaili_extra":
                    (got["echoes"] if " kaili " in k else extras)[k] = t
                else:
                    got[sec][k] = t
    snap = json.loads(SNAP.read_text()) if SNAP.exists() else {}
    rec = snap.setdefault(lang, {})
    src_text = {kind: dict(src[kind]) for kind in KINDS}
    for kind in KINDS:
        cur = dict(have[kind])
        cur.update(got[kind])
        if kind == "kaili":
            cur.update(extras)
        for k in got[kind]:
            if k in src_text[kind]:
                rec.setdefault(kind, {})[k] = h(src_text[kind][k])
        # the source's order; a Kaili line of the language's own after its story entry, an echo's twin after it
        order = [k for k, _ in src[kind]]
        if kind == "kaili":
            pos = {k: n for n, (k, _) in enumerate(src["story"])}
            order = sorted(set(order) | set(cur), key=lambda k: (pos.get(k, 1 << 30), k))
        elif kind == "echoes":
            order = [x for k in order if " kaili " not in k for x in (k, kaili_echo(k))] + [k for k in order if " kaili " in k]
            order = list(dict.fromkeys(order))
        lines = [f"# {lang}_{kind}.txt: translated from {SOURCE}_{kind}.txt (tools/check_story_lang.py; same format)."]
        lines += [f"{k}: {cur[k]}" for k in order if k in cur]
        lines += [f"{k}: {t}" for k, t in cur.items() if k not in set(order)]
        path(lang, kind).write_text("\n".join(lines) + "\n", encoding="utf-8")
    SNAP.write_text(json.dumps(snap, indent=0, sort_keys=True, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"merged {sum(len(v) for v in got.values()) + len(extras)} entries into {lang}_*.txt")


def report(n, errs, warns, show_warns=60):
    print("\n".join(errs[:300]))
    if warns:
        print("\n".join(sorted(set(warns))[:show_warns]))
    print(f"{n} entries: {len(errs)} errors, {len(set(warns))} warnings")
    return 1 if errs else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("lang", nargs="?", choices=LANGS)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--partial", action="store_true", help="do not count missing entries as errors")
    ap.add_argument("--todo", action="store_true")
    ap.add_argument("--export", metavar="DIR")
    ap.add_argument("--words", type=int, default=6000)
    ap.add_argument("--all-entries", action="store_true", help="export every entry, not only the missing/stale ones")
    ap.add_argument("--chunk", nargs=2, metavar=("SRC", "OUT"))
    ap.add_argument("--merge", nargs="+", metavar="OUT")
    ap.add_argument("--plates", action="store_true")
    a = ap.parse_args()
    if a.plates:
        return make_plates()
    langs = LANGS if a.all else [a.lang] if a.lang else ap.error("give a language or --all")
    if a.todo:
        for lang in langs:
            t = todo(lang)
            print(f"{lang}: {sum(1 for x in t if x[2] == 'new')} new, {sum(1 for x in t if x[2] == 'stale')} stale")
            for kind, k, why in t[:10]:
                print(f"   {why:5} {kind:8} {k}")
        return 0
    if a.export:
        return export(langs[0], a.export, a.words, a.all_entries)
    if a.chunk:
        return sys.exit(report(*check_chunk(langs[0], a.chunk[0], a.chunk[1])))
    if a.merge:
        return merge(langs[0], [Path(p) for p in a.merge])
    rc = 0
    for lang in langs:
        print(f"== {lang}")
        rc |= report(*check_lang(lang, a.partial))
    sys.exit(rc)


if __name__ == "__main__":
    main()
