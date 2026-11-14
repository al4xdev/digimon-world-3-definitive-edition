#!/usr/bin/env python3
"""Encode the port's text overrides (definitive combat PRD, layer 1 + PT-BR localization) into port/game/definitive_text.c.

  tools/venv/bin/python tools/gen_text_override.py            # write port/game/definitive_text.c (+ definitive_text_pt.inc)
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
OUT_PT = ROOT / "port/game/definitive_text_pt.inc"
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


def dollar_cap(en_b, pt_b=b""):
    need = max(len(en_b), len(pt_b))
    return 448 if need > 96 else 128


def render(en_rows, pt_rows, tables, inv):
    pt_dollars = {name: encode(text, inv, SRC_PT.name).replace(b"\xe6", b"\x01\x09") + b"\0"
                  for name, _, text in pt_rows if name.startswith("$")}
    lines = ["/* Generated by tools/gen_text_override.py from tools/text_override/en.txt: do not edit. */",
             '#include "definitive.h"', ""]
    over = [r for r in en_rows if not r[0].startswith("$")]
    for i, (name, idx, text) in enumerate(en_rows):
        b = encode(text, inv, SRC.name) + b"\0"
        if name.startswith("$"):  # the port's own labels are drawn in the HUD's 9-pixel font, which has no glyph 0xE6
            b = b.replace(b"\xe6", b"\x01\x09")  # ('?'): the extra glyph draws it
            cap = dollar_cap(b, pt_dollars.get(name, b""))
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
    lines += ["};", f"const int def_text_override_count = {len(over)};", "", '#include "definitive_text_pt.inc"', ""]
    return "\n".join(lines)


def render_pt(en_rows, pt_rows, tables, inv):
    en_dollars = [(name, encode(text, inv, SRC.name).replace(b"\xe6", b"\x01\x09") + b"\0")
                  for name, _, text in en_rows if name.startswith("$")]
    pt_dollar_map = {name: encode(text, inv, SRC_PT.name).replace(b"\xe6", b"\x01\x09") + b"\0"
                     for name, _, text in pt_rows if name.startswith("$")}

    lines = ["/* Generated by tools/gen_text_override.py from tools/text_override/pt.txt: do not edit. */",
             "#include <stdlib.h>", "#include <string.h>", ""]

    for name, en_b in en_dollars:
        pt_b = pt_dollar_map.get(name, en_b)
        sym = name[1:]
        lines.append(f"static const unsigned char def_en_str_{sym}[] = {{ " + ", ".join(f"0x{x:02X}" for x in en_b) + " };")
        lines.append(f"static const unsigned char def_pt_str_{sym}[] = {{ " + ", ".join(f"0x{x:02X}" for x in pt_b) + " };")

    pt_over = []
    seen_keys = set()
    for name, idx, text in pt_rows:
        if not name.startswith("$"):
            fid = jpn_id(name)
            pt_over.append((fid, idx, name, text))
            seen_keys.add((fid, idx))
    # Sort by (file, index) for binary search in def_text_override
    pt_over.sort(key=lambda r: (r[0], r[1]))

    lines.append("")
    for i, (fid, idx, name, text) in enumerate(pt_over):
        b = encode(text, inv, SRC_PT.name) + b"\0"
        lines.append(f"static const unsigned char def_pt_text_{i}[] = {{ " + ", ".join(f"0x{x:02X}" for x in b) + " };")

    lines += ["", "const DefTextOverride def_text_overrides_pt[] = {"]
    if pt_over:
        for i, (fid, idx, name, _) in enumerate(pt_over):
            lines.append(f"    {{ 0x{fid:X}, {idx}, def_pt_text_{i} }}, /* {name} {idx} */")
    else:
        lines.append("    { 0, 0, NULL },")
    lines += ["};", f"const int def_text_override_pt_count = {len(pt_over)};", ""]

    lines += [
        "static int def_lang_ptbr = -1;",
        "",
        "void def_language_set(int ptbr) {",
        "    def_lang_ptbr = ptbr ? 1 : 0;",
    ]
    for name, en_b in en_dollars:
        pt_b = pt_dollar_map.get(name, en_b)
        sym = name[1:]
        lines.append(
            f"    memcpy(def_str_{sym}, def_lang_ptbr ? def_pt_str_{sym} : def_en_str_{sym}, "
            f"def_lang_ptbr ? {len(pt_b)} : {len(en_b)});"
        )
    lines += [
        "}",
        "",
        "int def_is_ptbr(void) {",
        "    if (def_lang_ptbr < 0) {",
        '        const char *v = getenv("DW3_LANG");',
        '        def_language_set(v != NULL && (strcmp(v, "pt") == 0 || strcmp(v, "ptbr") == 0 || strcmp(v, "portuguese") == 0));',
        "    }",
        "    return def_lang_ptbr;",
        "}",
        "",
    ]
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
    pt_rows = load_source(SRC_PT)
    text = render(en_rows, pt_rows, tables, inv)
    text_pt = render_pt(en_rows, pt_rows, tables, inv)
    if a.check:
        ok = OUT.exists() and OUT.read_text(encoding="utf-8") == text
        ok_pt = OUT_PT.exists() and OUT_PT.read_text(encoding="utf-8") == text_pt
        sys.exit(0 if (ok and ok_pt) else 1)
    OUT.write_text(text, encoding="utf-8")
    OUT_PT.write_text(text_pt, encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)}: {len([r for r in en_rows if not r[0].startswith('$')])} EN overrides, "
          f"{len([r for r in pt_rows if not r[0].startswith('$')])} PT overrides")


if __name__ == "__main__":
    main()
