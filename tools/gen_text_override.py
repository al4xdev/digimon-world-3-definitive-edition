#!/usr/bin/env python3
"""Encode the port's text overrides (definitive combat + PT-BR localization) into port/game/definitive_text.c.

  tools/venv/bin/python tools/gen_text_override.py            # write port/game/definitive_text.c (+ definitive_text_lang.inc)
  tools/venv/bin/python tools/gen_text_override.py --check    # exit 1 if the committed file is stale

Source: tools/text_override/en.txt (and optional tools/text_override/pt.txt), one line per entry,
`FILE index: text` or `FILE:sub index: text` (FILE is the text file's name without the language prefix, e.g. SKLINF;
sub is the sub-table index of a DMG* container; index is the table entry; `#` starts a comment). A line `$NAME: text`
is not an override: it is a string of the port's own (the battle HUD's labels, Guide, Bestiary, DRI, Black Market),
exported as `def_str_NAME` (a glyph-byte array). The game's glyph codes (including European accented letters 0x3B..0x72)
are looked up in the user's EXE (tools/dump_text.py char_tables).
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import disc_files as df  # noqa: E402
import dump_text as dt  # noqa: E402

SRC = ROOT / "tools/text_override/en.txt"
SRC_PT = ROOT / "tools/text_override/pt.txt"
OUT = ROOT / "port/game/definitive_text.c"
OUT_PT = ROOT / "port/game/definitive_text_lang.inc"
# The story rewrite: English lines that replace the game's story text, the Kaili hero
# mode's variants of them, and the hero's choice points (definitive_story.c reads the tables).
SRC_STORY = ROOT / "tools/text_override/en_story.txt"
SRC_KAILI = ROOT / "tools/text_override/en_kaili.txt"
SRC_CHOICES = ROOT / "tools/text_override/en_choices.txt"
# Lines that remember an earlier answer (a choice's or a dream's): said instead of the entry's line while it holds.
SRC_ECHOES = ROOT / "tools/text_override/en_echoes.txt"
# Girls Power's Kaili mode in Portuguese: only the lines the name swap below cannot fix (the brother's).
SRC_KAILI_PT = ROOT / "tools/text_override/pt_kaili.txt"
# The story rewrite in Portuguese (translated from the English files, never from the older pt.txt lines): same formats.
SRC_STORY_PT = ROOT / "tools/text_override/pt_story.txt"
SRC_CHOICES_PT = ROOT / "tools/text_override/pt_choices.txt"
SRC_ECHOES_PT = ROOT / "tools/text_override/pt_echoes.txt"
OUT_STORY = ROOT / "port/game/definitive_story_text.c"
ALIAS = {
    "-": "−", "–": "−", "—": "―",
    "'": "’", "‘": "’", "`": "’",
    '"': "”", "“": "”", "«": "”", "»": "”",
    "~": "〜", "°": "º", "\u00a0": " ",
}


def inverse(tables):
    """char -> its bytes: a main glyph is one byte, an extra glyph (space, punctuation, Latin) is `01 xx`."""
    chars, ext = tables
    inv = {}
    for g, code in sorted(chars.items()):
        inv.setdefault(dt.sjis(code), bytes([g]))
    for g, code in sorted(ext.items()):
        inv.setdefault(dt.sjis(code), bytes([1, g]))
    for g, ch in sorted(dt.LATIN.items()):
        inv[ch] = bytes([1, g])
    return inv


CONTROL = re.compile(r"<(\d+(?:,\d+)*)>")


def encode(text, inv, src_name=SRC.name):
    """Text plus the game's control codes in dump_text's notation: `<5,1>` is bytes 02 05 01 (code 5, argument 1), `<1>` a
    line break (02 01). The length of each code is checked against message_code_lengths."""
    out = bytearray()
    i = 0
    while i < len(text):
        m = CONTROL.match(text, i)
        if m:
            nums = [int(x) for x in m.group(1).split(",")]
            want = dt.CODE_LENGTHS[nums[0]] if nums[0] < len(dt.CODE_LENGTHS) and dt.CODE_LENGTHS[nums[0]] >= 2 else 2
            if len(nums) + 1 != want:
                sys.exit(f"{src_name}: control code <{m.group(1)}> must have {want - 2} argument(s): {text!r}")
            out += bytes([2] + nums)
            i = m.end()
            continue
        ch = ALIAS.get(text[i], text[i])
        if ch not in inv:
            sys.exit(f"{src_name}: no glyph for {ch!r} in {text!r}")
        out += inv[ch]
        i += 1
    return bytes(out)


_JPN_ID_CACHE = {}


def jpn_id(name):
    """The Japanese file's ID (or `(sub + 1) << 16 | id` for `FILE:sub` container sub-tables)."""
    if name in _JPN_ID_CACHE:
        return _JPN_ID_CACHE[name]
    if ":" in name:
        base, sub = name.split(":", 1)
        fid = dt.find_file(base, "JPN").id | ((int(sub) + 1) << 16)
    else:
        fid = dt.find_file(name, "JPN").id
    _JPN_ID_CACHE[name] = fid
    return fid


def load_source(path=SRC):
    rows = []
    if not path.exists():
        return rows
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = line.split("#", 1)[0].rstrip() if not line.lstrip().startswith("#") else ""
        if not line.strip():
            continue
        head, sep, text = line.partition(":")
        if sep and not head.startswith("$") and head.count(" ") == 0:
            # Handle `FILE:sub index: text` where the first `:` is inside `FILE:sub`
            head2, sep2, text2 = text.partition(":")
            if sep2:
                head = head + ":" + head2
                sep = sep2
                text = text2
        parts = head.split()
        if sep and len(parts) == 1 and parts[0].startswith("$"):
            text = text.strip()
            if len(text) >= 2 and text[0] == text[-1] == '"':  # a quoted text keeps its spaces (the port's separators)
                text = text[1:-1]
            rows.append((parts[0], -1, text))
            continue
        if not sep or len(parts) != 2 or not parts[1].isdigit():
            sys.exit(f"{path.name}:{n}: expected `FILE index: text`")
        rows.append((parts[0].upper(), int(parts[1]), text.strip()))
    return rows


def roundtrip(tables, inv):
    """Every entry of the disc's ENG text files named below that is plain glyphs and control codes re-encodes to the same
    bytes (the encoder is checked against the game's own text before anything is written)."""
    bad = 0
    for name in ("SKLINF", "SKLNAM", "STATUS", "TRAINI"):
        d = (df.DISC / dt.find_file(name, "ENG").path).read_bytes()
        (_, offs), = dt.tables_of(d)
        for o in offs:
            raw = d[o:d.index(0, o)]
            text, b = dt.decode(d, o, tables, latin=False)
            text = text.replace("\n    ", "").replace("\u2212", "-").replace("\u2019", "'")
            if b == 0 and all(ord(c) < 128 for c in text.replace("\u2192", "")):
                try:
                    if encode(text, inv) != raw:
                        bad += 1
                except SystemExit:
                    bad += 1
    return bad


# ---- Girls Power's Kaili mode: the roles swap. The hero is the girl (Ivy in the Real World, the player's account name
# in the Digital World) and the friend is the boy, Junior, in both worlds (his account name stands in for Kail). So
# every text with no line of its own in en_kaili.txt / pt_kaili.txt names Junior as Ivy, and Ivy or Kail as Junior;
# Smith Junior, the Black Market's boy, keeps his name. A line that no longer fits its box is rewrapped within its page.
# Portuguese swaps the same names: its rewrite is written without gendered words next to them and without articles before
# them (tools/check_pt_text.py warns), and a line that still needs its own Girls Power wording has one in pt_kaili.txt.
NAMES = re.compile(r"(?<!Smith )(?<![A-Za-z])(Junior|Ivy|Kail)(?![a-z])")
NAMES_PT = NAMES
NAME_AS = {"Junior": "Ivy", "Ivy": "Junior", "Kail": "Junior"}


def names_swap(s, names=NAMES):
    return names.sub(lambda m: NAME_AS[m[1]], s)


CODE = re.compile(r"<[\d,]+>")


def _width(s):
    return len(CODE.sub("", s))


def _wrap(lines, width, max_lines):
    """The page's words greedily in lines of `width` letters (codes not counted); None if they need more lines."""
    words = " ".join(x.strip() for x in lines).split()
    out = [""]
    for w in words:
        cand = (out[-1] + " " + w) if out[-1] else w
        if _width(cand) <= width or not out[-1]:
            out[-1] = cand
        else:
            out.append(w)
    return out if len(out) <= max_lines else None


def kaili_swap(text, width, max_lines=3, names=NAMES):
    """`text` with the names swapped (names_swap): each line that grew past `width` (or past its own old width, when the original was
    already wider) has its page rewrapped. None when a page no longer fits `max_lines` lines."""
    m = re.match(r"(>hero )?(<7>.*?<7>)?", text)
    head, body = text[:m.end()], text[m.end():]
    head = names_swap(head, names)
    pages = body.split("<2,2>")
    out = []
    for page in pages:
        lead = "<3>" if page.startswith("<3>") else ""
        old = page[len(lead):].split("<1>")
        new = [names_swap(x, names) for x in old]
        limit = max([width] + [_width(x) for x in old])
        if any(_width(x) > limit for x in new):
            new = _wrap(new, limit, max(max_lines, len(old)))
            if new is None:
                return None
        out.append(lead + "<1>".join(new))
    return head + "<2,2>".join(out)


def disc_name_entries(tables):
    """{(FILE[:sub], index): text} of every English disc text entry that names Junior, Ivy or Kail (dump_text's
    notation)."""
    found = {}
    for e in df.files():
        p = e.path.split("/")
        if len(p) != 5 or p[2] != "COUNTRY" or p[3] != "ENG":
            continue
        d = (df.DISC / e.path).read_bytes()
        for sub, offs in dt.tables_of(d):
            for i, o in enumerate(offs):
                t = dt.decode(d, o, tables)[0].replace("\n    ", "")
                if NAMES.search(t):
                    found[(p[4][2:-4] + ("" if sub is None else f":{sub}"), i)] = t
    return found


def kaili_dollars(rows, src_name, names=NAMES):
    """{$NAME: text} of the port's own strings that name Junior, Ivy or Kail, swapped (a balloon's lines are up to 22
    letters, the Guide's up to 44)."""
    out = {}
    for name, _, text in rows:
        if name.startswith("$") and names.search(text):
            t = kaili_swap(text + "<2,2>", 22 if text.startswith("<7>") else 44, text.count("<1>") + 1, names)
            if t is None:
                sys.exit(f"{src_name}: {name} does not fit its lines with the names swapped: give it its own")
            out[name] = t[:-len("<2,2>")]
    return out


def dollar_cap(en_b, pt_b=b"", *more):
    need = max([len(en_b), len(pt_b)] + [len(b) for b in more])
    if need <= 96:
        return 128
    return max(448, ((need + 63) // 64) * 64)


def render(en_rows, lang_rows, tables, inv):
    """`lang_rows` is {code: rows} of every translated language: a $string's buffer holds the longest of them."""
    pt_dollars = {}
    for code, path in LANGS:
        for name, _, text in lang_rows[code]:
            if name.startswith("$"):
                b = encode(text, inv, path.name).replace(b"\xe6", b"\x01\x09") + b"\0"
                pt_dollars[name] = max(pt_dollars.get(name, b""), b, key=len)
    k_dollars = [kaili_dollars(en_rows, SRC.name)] + [kaili_dollars(lang_rows[c], p.name, NAMES_PT) for c, p in LANGS]
    lines = ["/* Generated by tools/gen_text_override.py from tools/text_override/en.txt: do not edit. */",
             '#include "definitive.h"', ""]
    over = [r for r in en_rows if not r[0].startswith("$")]
    for i, (name, idx, text) in enumerate(en_rows):
        b = encode(text, inv, SRC.name) + b"\0"
        if name.startswith("$"):  # the port's own labels are drawn in the HUD's 9-pixel font, which has no glyph 0xE6
            b = b.replace(b"\xe6", b"\x01\x09")  # ('?'): the extra glyph draws it
            cap = dollar_cap(b, pt_dollars.get(name, b""),
                             *(encode(k[name], inv) + b"\0" for k in k_dollars if name in k))
            sym = f"def_str_{name[1:]}"
            lines.append(f"unsigned char {sym}[{cap}] = {{ "
                         + ", ".join(f"0x{x:02X}" for x in b) + f" }}; /* {text} */")
        else:
            sym = f"def_text_{i}"
            lines.append(f"static const unsigned char {sym}[] = {{ "
                         + ", ".join(f"0x{x:02X}" for x in b) + " };")
    lines += ["", "const DefTextOverride def_text_overrides[] = {"]
    for i, (name, idx, text) in enumerate(en_rows):
        if not name.startswith("$"):
            lines.append(f"    {{ 0x{jpn_id(name):X}, {idx}, def_text_{i} }}, /* {name} {idx}: {text} */")
    lines += ["};", f"const int def_text_override_count = {len(over)};", "", '#include "definitive_text_lang.inc"', ""]
    return "\n".join(lines)


# The translated languages, in the order of the C side's `def_lang()` (0 is English): the port's own strings and the
# entries the Definitive Edition adds to the disc. Portuguese is a sixth language of the disc's English set; French,
# German, Spanish and Italian already have the original campaign on the EU disc, so their files hold only the delta.
LANGS = [("pt", ROOT / "tools/text_override/pt.txt"), ("fr", ROOT / "tools/text_override/fr.txt"),
         ("de", ROOT / "tools/text_override/de.txt"), ("it", ROOT / "tools/text_override/it.txt"),
         ("es", ROOT / "tools/text_override/es.txt")]


def render_pt(en_rows, lang_rows, tables, inv):
    """The language tables: one `def_<l>_str_*` per string and language, `def_text_overrides_<l>` per language, and the
    switch (`def_language_set`). `lang_rows` is {code: rows}; a language without a file falls back to English."""
    def dollar(text, src):
        return encode(text, inv, src).replace(b"\xe6", b"\x01\x09") + b"\0"

    en_dollars = [(name, dollar(text, SRC.name)) for name, _, text in en_rows if name.startswith("$")]
    k_en = kaili_dollars(en_rows, SRC.name)
    lines = ["/* Generated by tools/gen_text_override.py from tools/text_override/{pt,fr,de,it,es}.txt: do not edit. */",
             "#include <stdlib.h>", "#include <string.h>", ""]
    dmap, kmap = {}, {}
    for code, path in LANGS:
        rows = lang_rows[code]
        dmap[code] = {name: dollar(text, path.name) for name, _, text in rows if name.startswith("$")}
        kmap[code] = kaili_dollars(rows, path.name, NAMES_PT)

    kaili = {}  # $NAME -> {code: bytes} of the Kaili mode, for the strings that name Kail in any language
    for name, en_b in en_dollars:
        sym = name[1:]
        lines.append(f"static const unsigned char def_en_str_{sym}[] = {{ " + ", ".join(f"0x{x:02X}" for x in en_b) + " };")
        named = name in k_en or any(name in kmap[c] for c, _ in LANGS)
        if named:
            kaili[name] = {"en": dollar(k_en[name], SRC.name) if name in k_en else en_b}
            lines.append(f"static const unsigned char def_enk_str_{sym}[] = {{ "
                         + ", ".join(f"0x{x:02X}" for x in kaili[name]['en']) + " };")
        for code, path in LANGS:
            b = dmap[code].get(name, en_b)
            lines.append(f"static const unsigned char def_{code}_str_{sym}[] = {{ " + ", ".join(f"0x{x:02X}" for x in b) + " };")
            if named:
                kb = dollar(kmap[code][name], path.name) if name in kmap[code] else b
                kaili[name][code] = kb
                lines.append(f"static const unsigned char def_{code}k_str_{sym}[] = {{ "
                             + ", ".join(f"0x{x:02X}" for x in kb) + " };")

    lines.append("")
    counts = {}
    for code, path in LANGS:
        over = []
        for name, idx, text in lang_rows[code]:
            if not name.startswith("$"):
                over.append((jpn_id(name), idx, name, text))
        over.sort(key=lambda r: (r[0], r[1]))  # binary search in def_text_override
        counts[code] = len(over)
        for i, (fid, idx, name, text) in enumerate(over):
            b = encode(text, inv, path.name) + b"\0"
            lines.append(f"static const unsigned char def_{code}_text_{i}[] = {{ " + ", ".join(f"0x{x:02X}" for x in b) + " };")
        lines += ["", f"const DefTextOverride def_text_overrides_{code}[] = {{"]
        if over:
            lines += [f"    {{ 0x{fid:X}, {idx}, def_{code}_text_{i} }}, /* {name} {idx} */"
                      for i, (fid, idx, name, _) in enumerate(over)]
        else:
            lines.append("    { 0, 0, NULL },")
        lines += ["};", f"const int def_text_override_{code}_count = {len(over)};", ""]

    lines += ["const DefTextOverride *def_text_overrides_of(int lang, int *count) {", "    switch (lang) {"]
    for n, (code, _) in enumerate(LANGS, 1):
        lines += [f"    case {n}:", f"        *count = def_text_override_{code}_count;", f"        return def_text_overrides_{code};"]
    lines += ["    default:", "        *count = 0;", "        return NULL;", "    }", "}", ""]

    lines += [
        "static int def_lang_now = -1;",
        "static int def_lang_kaili; /* Girls Power's Kaili mode: the strings with Junior, Ivy and Kail swapped */",
        "",
        "/* Language 0 English, 1 Portuguese, 2 French, 3 German, 4 Italian, 5 Spanish: every string of the port's own is copied",
        " * into its def_str_* buffer, the Kaili mode's variant when it has one. */",
        "void def_language_set(int lang) {",
        "    def_lang_now = lang >= 0 && lang <= %d ? lang : 0;" % len(LANGS),
    ]
    names = ["en"] + [c for c, _ in LANGS]
    for name, en_b in en_dollars:
        sym = name[1:]
        lines.append("    switch (def_lang_now) {")
        for n, code in enumerate(names):
            if name in kaili:
                kb, b = kaili[name][code], (en_b if code == "en" else dmap[code].get(name, en_b))
                kb_sym = f"def_{code}k_str_{sym}"
                lines.append(f"    case {n}: if (def_lang_kaili) memcpy(def_str_{sym}, {kb_sym}, {len(kb)}); "
                             f"else memcpy(def_str_{sym}, def_{code}_str_{sym}, {len(b)}); break;")
            else:
                b = en_b if code == "en" else dmap[code].get(name, en_b)
                lines.append(f"    case {n}: memcpy(def_str_{sym}, def_{code}_str_{sym}, {len(b)}); break;")
        lines.append("    }")
    lines += [
        "}",
        "",
        "void def_language_kaili(int on) {",
        "    if ((on != 0) != def_lang_kaili) {",
        "        def_lang_kaili = on != 0;",
        "        def_language_set(def_lang());",
        "    }",
        "}",
        "",
        "/* The language now in use: DW3_LANG (pt, fr, de, it, es) or the preset's; else the disc's own set (records_language",
        " * 3 French, 4 Italian, 5 German, 6 Spanish; 1 and 2 are English). Refreshes the strings when it changes. */",
        "int def_lang(void) {",
        "    extern int records_language;",
        "    int want = 0;",
        '    const char *v = getenv("DW3_LANG");',
        "    if (def_lang_now == 1) {",
        "        return 1; /* Portuguese rides on the English set, and is chosen by the preset or DW3_LANG only */",
        "    }",
        '    if (v != NULL && (strcmp(v, "pt") == 0 || strcmp(v, "ptbr") == 0 || strcmp(v, "portuguese") == 0)) {',
        "        want = 1;",
        "    } else if (records_language == 3) {",
        "        want = 2;",
        "    } else if (records_language == 5) {",
        "        want = 3;",
        "    } else if (records_language == 4) {",
        "        want = 4;",
        "    } else if (records_language == 6) {",
        "        want = 5;",
        "    }",
        "    if (want != def_lang_now) {",
        "        def_language_set(want);",
        "    }",
        "    return def_lang_now;",
        "}",
        "",
        "int def_is_ptbr(void) {",
        "    return def_lang() == 1;",
        "}",
        "",
    ]
    return "\n".join(lines)


ALLY_MAP = {"teddy": 0, "ivy": 1, "lisa": 2, "keith": 3, "kurt": 4}


def parse_choice_fx(label):
    """Extract optional `[teddy:+1,rep:-1]` tag from the end of a choice label."""
    ally, bdelta, rdelta = -1, 0, 0
    m = re.search(r"\s*\[([^\]]+)\]\s*$", label)
    if m:
        label = label[:m.start()].rstrip()
        for part in m.group(1).split(","):
            k, _, v = part.strip().partition(":")
            k = k.strip().lower()
            val = int(v.strip())
            if k == "rep":
                rdelta = val
            elif k in ALLY_MAP:
                ally = ALLY_MAP[k]
                bdelta = val
            else:
                sys.exit(f"unknown choice effect {k!r} in [{m.group(1)}]")
    return label, ally, bdelta, rdelta


def load_choices(path=SRC_CHOICES):
    """`Q<n> FILE[:sub] index@<k>: label [fx] | text` -> {(name, index): (n, [(k, label, text, ally, bdelta, rdelta)])}."""
    out = {}
    if not path.exists():
        return out
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        m = re.match(r"Q(\d+)\s+(\S+)\s+(\d+)@([123]):\s*(.*?)\s*\|\s*(.*)$", line)
        if not m:
            sys.exit(f"{path.name}:{n}: expected `Q<n> FILE index@<k>: label | text`")
        q, name, idx, k, raw_label, text = int(m[1]), m[2].upper(), int(m[3]), int(m[4]), m[5], m[6].strip()
        label, ally, bdelta, rdelta = parse_choice_fx(raw_label)
        if len(label) > 26:
            sys.exit(f"{path.name}:{n}: the label is longer than 26 letters ({label!r})")
        key = (name, idx)
        if key in out and out[key][0] != q:
            sys.exit(f"{path.name}:{n}: {name} {idx} has two question numbers")
        out.setdefault(key, (q, []))[1].append((k, label, text, ally, bdelta, rdelta))
    # a question on several entries is one scene's copies (the partner packs' DMG200:3/4/5...): one plays, the same
    # options in each
    count = {}
    for (name, idx), (q, opts) in out.items():
        if count.setdefault(q, len(opts)) != len(opts):
            sys.exit(f"{path.name}: Q{q}'s copies do not have the same options")
    return out


ECHO = re.compile(r"Q(\d+)=([0123](?:,[0123])*)\s+(?:(kaili)\s+)?(\S+)\s+(\d+):\s*(.*)$")
DREAMS = 8  # story answers 0..7 are Igor's dreams (definitive_story.c), 3 options each
MAX_QUESTIONS = 512


def load_echoes(choices, path=SRC_ECHOES):
    """`Q<n>=<k>[,<k>...] [kaili] FILE[:sub] index: text` -> [(name, index, n, mask, kaili, text)] in the file's order.
    k=0 matches when the player ignored/missed the question. `kaili` marks the Girls Power line."""
    out = []
    if not path.exists():
        return out
    asked = {q: len(opts) for (q, opts) in choices.values()}
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        m = ECHO.match(line)
        if not m:
            sys.exit(f"{path.name}:{n}: expected `Q<n>=<k> [kaili] FILE index: text`")
        q, ks, name, idx, text = int(m[1]), [int(k) for k in m[2].split(",")], m[4].upper(), int(m[5]), m[6].strip()
        if q >= MAX_QUESTIONS or (q >= DREAMS and q not in asked):
            sys.exit(f"{path.name}:{n}: Q{q} is no question (choices, or 0..{DREAMS - 1} for the dreams)")
        if any(k > asked.get(q, 3) for k in ks):
            sys.exit(f"{path.name}:{n}: Q{q} has {asked.get(q, 3)} options")
        if (name, idx) in choices:
            sys.exit(f"{path.name}:{n}: {name} {idx} is itself a choice")
        if text.startswith(">hero "):
            sys.exit(f"{path.name}:{n}: an echo keeps the entry's speaker (en_kaili.txt moves a balloon to the hero)")
        out.append((name, idx, q, sum(1 << k for k in ks), bool(m[3]), text))
    return out


def render_story(tables, inv, en_rows, pt_rows):
    def arr(sym, b, static=True):
        return (("static " if static else "") + f"const unsigned char {sym}[] = {{ "
                + ", ".join(f"0x{x:02X}" for x in b) + " };")

    lines = ["/* Generated by tools/gen_text_override.py from tools/text_override/en_story.txt, en_kaili.txt,",
             " * en_choices.txt and pt_kaili.txt (+ en.txt, pt.txt and the disc for the Kaili mode's name swap): do not",
             " * edit. */", '#include "definitive.h"', ""]
    story = sorted(((jpn_id(n), i, n, t) for n, i, t in load_source(SRC_STORY) if not n.startswith("$")),
                   key=lambda r: (r[0], r[1]))
    for k, (fid, idx, name, text) in enumerate(story):
        lines.append(arr(f"st_line_{k}", encode(text, inv, SRC_STORY.name) + b"\0"))
    lines += ["", "const DefStoryLine def_story_lines[] = {"]
    lines += [f"    {{ 0x{fid:X}, {idx}, 0, st_line_{k} }}, /* {name} {idx} */" for k, (fid, idx, name, _) in enumerate(story)]
    lines += ["    { 0, 0, 0, 0 },", "};", f"const int def_story_line_count = {len(story)};", ""]
    story_pt_rows = [(n, i, t) for n, i, t in load_source(SRC_STORY_PT) if not n.startswith("$")]
    story_pt = sorted(((jpn_id(n), i, n, t) for n, i, t in story_pt_rows), key=lambda r: (r[0], r[1]))
    for k, (fid, idx, name, text) in enumerate(story_pt):
        lines.append(arr(f"st_pt_line_{k}", encode(text, inv, SRC_STORY_PT.name) + b"\0"))
    lines += ["", "const DefStoryLine def_story_lines_pt[] = {"]
    lines += [f"    {{ 0x{fid:X}, {idx}, 0, st_pt_line_{k} }}, /* {name} {idx} */" for k, (fid, idx, name, _) in enumerate(story_pt)]
    lines += ["    { 0, 0, 0, 0 },", "};", f"const int def_story_line_pt_count = {len(story_pt)};", ""]

    def kaili_rows(hand, base, src_name, hero_keys=frozenset(), names=NAMES):
        """The hand-written lines of `hand`, then every text of `base` ({(FILE, index): text}) that names Junior, Ivy or
        Kail (or that
        the English moves to the hero: `hero_keys`) with the name swapped."""
        rows = {}
        for n, i, t in load_source(hand):
            if not n.startswith("$"):
                rows[(n, i)] = t
        for (n, i), t in sorted(base.items()):
            if (n, i) in rows or not (names.search(t) or (n, i) in hero_keys):
                continue
            plated = t.startswith("<7>")
            new = kaili_swap(t, 22 if plated else 38, names=names)
            if new is None:
                sys.exit(f"{src_name}: {n} {i} does not fit its box with the names swapped: give it its own line")
            if (n, i) in hero_keys:
                new = ">hero " + re.sub(r"^<7>.*?<7>", "<7><9><7>", new)
            rows[(n, i)] = new
        out = []
        for (n, i), t in rows.items():
            hero = t.startswith(">hero ")
            out.append((jpn_id(n), i, n, t[len(">hero "):] if hero else t, hero))
        return sorted(out, key=lambda r: (r[0], r[1]))

    def table(sym, count, rows, prefix, src_name):
        for k, (fid, idx, name, text, hero) in enumerate(rows):
            lines.append(arr(f"{prefix}_{k}", encode(text, inv, src_name) + b"\0"))
        lines.extend(["", f"const DefStoryLine {sym}[] = {{"])
        lines.extend(f"    {{ 0x{fid:X}, {idx}, {int(hero)}, {prefix}_{k} }}, /* {name} {idx} */"
                     for k, (fid, idx, name, _, hero) in enumerate(rows))
        lines.extend(["    { 0, 0, 0, 0 },", "};", f"const int {count} = {len(rows)};", ""])

    # English: the rewrite's line, else en.txt's, else the disc's
    base = disc_name_entries(tables)
    base.update({(n, i): t for n, i, t in en_rows if not n.startswith("$") and ":" not in n})
    base.update({(n, i): t for n, i, t in load_source(SRC_STORY) if not n.startswith("$")})
    kaili = kaili_rows(SRC_KAILI, base, SRC_KAILI.name)
    table("def_kaili_lines", "def_kaili_line_count", kaili, "st_kaili", SRC_KAILI.name)
    # Portuguese (pt.txt's lines; the rewrite is English only until it is translated): the lines the English gives the
    # hero go to her too
    hero_keys = frozenset((n, i) for _, i, n, _, hero in kaili if hero)
    pt_base = {(n, i): t for n, i, t in pt_rows if not n.startswith("$")}
    pt_base.update({(n, i): t for n, i, t in story_pt_rows})  # the rewrite's Portuguese wins over the disc-derived lines
    table("def_kaili_lines_pt", "def_kaili_line_pt_count", kaili_rows(SRC_KAILI_PT, pt_base, SRC_KAILI_PT.name,
                                                                      hero_keys, NAMES_PT), "st_kaili_pt", SRC_KAILI_PT.name)

    loaded = load_choices()
    # a choice's lines are said in both heroes' games as written: no name the Kaili mode swaps, and no entry that mode
    # says its own way
    kaili_keys = {(n.upper(), i) for n, i, _ in load_source(SRC_KAILI)}
    for (name, idx), (q, opts) in loaded.items():
        if (name, idx) in kaili_keys:
            sys.exit(f"{SRC_CHOICES.name}: Q{q} {name} {idx} has a line of its own in {SRC_KAILI.name}")
        if any(NAMES.search(text) for _, _, text, _, _, _ in opts):
            sys.exit(f"{SRC_CHOICES.name}: Q{q} {name} {idx} names Junior, Ivy or Kail (the Kaili mode cannot swap it)")
    loaded_pt = load_choices(SRC_CHOICES_PT)
    for (name, idx), (q, opts) in loaded_pt.items():
        if (name, idx) in loaded and loaded[(name, idx)][0] != q:
            sys.exit(f"{SRC_CHOICES_PT.name}: Q{q} {name} {idx} has different question number than {SRC_CHOICES.name}")
        if any(NAMES.search(text) for _, _, text, _, _, _ in opts):
            sys.exit(f"{SRC_CHOICES_PT.name}: Q{q} {name} {idx} names Junior, Ivy or Kail (the Kaili mode cannot swap it)")
    all_choice_keys = set(loaded.keys()) | set(loaded_pt.keys())
    all_choices_union = dict(loaded)
    for key, val in loaded_pt.items():
        all_choices_union.setdefault(key, val)
    choices = sorted(
        (jpn_id(name), idx, name, loaded[k][0] if k in loaded else loaded_pt[k][0],
         sorted(loaded[k][1]) if k in loaded else None,
         sorted(loaded_pt[k][1]) if k in loaded_pt else None)
        for k in all_choice_keys for name, idx in [k]
    )
    for c, (fid, idx, name, q, en_opts, pt_opts_list) in enumerate(choices):
        if en_opts is not None:
            for k, label, text, _, _, _ in en_opts:
                lines.append(arr(f"st_choice_{c}_label_{k}", encode(label, inv, SRC_CHOICES.name)
                                 .replace(b"\xe6", b"\x01\x09") + b"\0"))
                lines.append(arr(f"st_choice_{c}_line_{k}", encode(text, inv, SRC_CHOICES.name) + b"\0"))
        if pt_opts_list is not None and (en_opts is None or [x[0] for x in pt_opts_list] == [x[0] for x in en_opts]):
            for k, label, text, _, _, _ in pt_opts_list:
                lines.append(arr(f"st_choice_{c}_plabel_{k}", encode(label, inv, SRC_CHOICES_PT.name)
                                 .replace(b"\xe6", b"\x01\x09") + b"\0"))
                lines.append(arr(f"st_choice_{c}_pline_{k}", encode(text, inv, SRC_CHOICES_PT.name) + b"\0"))
    lines += ["", "const DefStoryChoice def_story_choices[] = {"]
    for c, (fid, idx, name, q, en_opts, pt_opts_list) in enumerate(choices):
        ref_opts = pt_opts_list if pt_opts_list is not None else en_opts
        n_opts = len(ref_opts)
        if en_opts is not None:
            labels = ", ".join(f"st_choice_{c}_label_{k}" for k, _, _, _, _, _ in en_opts) + ", 0" * (3 - len(en_opts))
            texts = ", ".join(f"st_choice_{c}_line_{k}" for k, _, _, _, _, _ in en_opts) + ", 0" * (3 - len(en_opts))
        else:
            labels = texts = "0, 0, 0"
        if pt_opts_list is not None and (en_opts is None or [x[0] for x in pt_opts_list] == [x[0] for x in en_opts]):
            plabels = ", ".join(f"st_choice_{c}_plabel_{k}" for k, _, _, _, _, _ in pt_opts_list) + ", 0" * (3 - len(pt_opts_list))
            ptexts = ", ".join(f"st_choice_{c}_pline_{k}" for k, _, _, _, _, _ in pt_opts_list) + ", 0" * (3 - len(pt_opts_list))
        else:
            plabels = ptexts = "0, 0, 0"
        allies = ", ".join(str(x[3]) for x in ref_opts) + ", -1" * (3 - n_opts)
        bdeltas = ", ".join(str(x[4]) for x in ref_opts) + ", 0" * (3 - n_opts)
        rdeltas = ", ".join(str(x[5]) for x in ref_opts) + ", 0" * (3 - n_opts)
        lines.append(f"    {{ 0x{fid:X}, {idx}, {q}, {n_opts}, {{ {labels} }}, {{ {texts} }}, {{ {plabels} }}, "
                     f"{{ {ptexts} }}, {{ {allies} }}, {{ {bdeltas} }}, {{ {rdeltas} }} }}, /* {name} {idx} */")
    lines += ["    { 0, 0, 0, 0, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { -1, -1, -1 }, { 0, 0, 0 }, { 0, 0, 0 } },", "};",
              f"const int def_story_choice_count = {len(choices)};", ""]

    # the echoes: mode 0 both heroes, 1 without Girls Power, 2 with it (its own line, or the names swapped)
    en_echo_list = load_echoes(loaded)
    pt_echo_list = load_echoes(all_choices_union, SRC_ECHOES_PT)
    echoes, hand_k = [], {(n, i, q, mask) for n, i, q, mask, k, _ in en_echo_list if k}
    echo_pt = {(n, i, q, mask, k): t for n, i, q, mask, k, t in pt_echo_list}
    en_echo_keys = {(n, i, q, mask) for n, i, q, mask, _, _ in en_echo_list}

    def pt_swapped(n, i, q, mask, mode):
        """The Portuguese text of an echo record in `mode` (2: Girls Power's own, else the names swapped), or None."""
        if mode != 2:
            return echo_pt.get((n, i, q, mask, False))
        if (n, i, q, mask, True) in echo_pt:
            return echo_pt[(n, i, q, mask, True)]
        plain = echo_pt.get((n, i, q, mask, False))
        if plain is None:
            return None
        sw = kaili_swap(plain, 22 if plain.startswith("<7>") else 38, names=NAMES_PT)
        if sw is None:
            sys.exit(f"{SRC_ECHOES_PT.name}: {n} {i} does not fit its box with the names swapped: give it a kaili line")
        return re.sub(r"^<7>.*?<7>", "<7><9><7>", sw) if (n, i) in hero_keys else sw

    for order, (n, i, q, mask, k, text) in enumerate(en_echo_list):
        if k:
            echoes.append((n, i, q, mask, 2, text, order))
            continue
        swapped = kaili_swap(text, 22 if text.startswith("<7>") else 38)
        if swapped is None:
            sys.exit(f"{SRC_ECHOES.name}: {n} {i} does not fit its box with the names swapped: give it a kaili line")
        if (n, i) in hero_keys:
            swapped = re.sub(r"^<7>.*?<7>", "<7><9><7>", swapped)
        if (n.upper(), i) in kaili_keys and (n, i, q, mask) not in hand_k:
            sys.exit(f"{SRC_ECHOES.name}: Q{q} {n} {i} has a line of its own in {SRC_KAILI.name}: give the echo a kaili twin")
        if (n, i, q, mask) in hand_k or swapped != text:
            echoes.append((n, i, q, mask, 1, text, order))
            if (n, i, q, mask) not in hand_k:
                echoes.append((n, i, q, mask, 2, swapped, order))
        else:
            echoes.append((n, i, q, mask, 0, text, order))
    # PT-BR-only echoes (not yet ported to en_echoes.txt)
    pt_only_base = len(en_echo_list)
    pt_hand_k = {(n, i, q, mask) for n, i, q, mask, k, _ in pt_echo_list if k}
    for order, (n, i, q, mask, k, text) in enumerate(pt_echo_list):
        if (n, i, q, mask) in en_echo_keys:
            continue
        if k:
            echoes.append((n, i, q, mask, 2, None, pt_only_base + order))
            continue
        swapped = kaili_swap(text, 22 if text.startswith("<7>") else 38, names=NAMES_PT)
        if swapped is None:
            sys.exit(f"{SRC_ECHOES_PT.name}: {n} {i} does not fit its box with the names swapped: give it a kaili line")
        if (n, i, q, mask) in pt_hand_k or swapped != text:
            echoes.append((n, i, q, mask, 1, None, pt_only_base + order))
            if (n, i, q, mask) not in pt_hand_k:
                echoes.append((n, i, q, mask, 2, None, pt_only_base + order))
        else:
            echoes.append((n, i, q, mask, 0, None, pt_only_base + order))
    echoes.sort(key=lambda r: (jpn_id(r[0]), r[1], r[6]))
    pt_texts = []
    for k, (n, i, q, mask, mode, text, _) in enumerate(echoes):
        if text is not None:
            lines.append(arr(f"st_echo_{k}", encode(text, inv, SRC_ECHOES.name) + b"\0"))
        ptext = pt_swapped(n, i, q, mask, mode)
        pt_texts.append(ptext)
        if ptext is not None:
            lines.append(arr(f"st_echo_pt_{k}", encode(ptext, inv, SRC_ECHOES_PT.name) + b"\0"))
    lines += ["", "const DefStoryEcho def_story_echoes[] = {"]
    lines += [f"    {{ 0x{jpn_id(n):X}, {i}, {q}, 0x{mask:X}, {mode}, "
              f"{f'st_echo_{k}' if text is not None else '0'}, "
              f"{f'st_echo_pt_{k}' if pt_texts[k] is not None else '0'} }}, /* {n} {i} */"
              for k, (n, i, q, mask, mode, text, _) in enumerate(echoes)]
    lines += ["    { 0, 0, 0, 0, 0, 0, 0 },", "};", f"const int def_story_echo_count = {len(echoes)};", ""]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    tables = dt.char_tables()
    inv = inverse(tables)
    bad = roundtrip(tables, inv)
    if bad:
        sys.exit(f"the encoder does not reproduce {bad} entries of the disc's text: the glyph table is wrong")
    en_rows = load_source(SRC)
    lang_rows = {code: load_source(path) for code, path in LANGS}
    pt_rows = lang_rows["pt"]
    text = render(en_rows, lang_rows, tables, inv)
    text_pt = render_pt(en_rows, lang_rows, tables, inv)
    text_story = render_story(tables, inv, en_rows, pt_rows)
    if a.check:
        ok = OUT.exists() and OUT.read_text(encoding="utf-8") == text
        ok_pt = OUT_PT.exists() and OUT_PT.read_text(encoding="utf-8") == text_pt
        ok_story = OUT_STORY.exists() and OUT_STORY.read_text(encoding="utf-8") == text_story
        sys.exit(0 if (ok and ok_pt and ok_story) else 1)
    OUT.write_text(text, encoding="utf-8")
    OUT_PT.write_text(text_pt, encoding="utf-8")
    OUT_STORY.write_text(text_story, encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)}: {len([r for r in en_rows if not r[0].startswith('$')])} EN overrides, "
          f"{len([r for r in pt_rows if not r[0].startswith('$')])} PT overrides")


if __name__ == "__main__":
    main()
