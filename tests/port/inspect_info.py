#!/usr/bin/env python3
"""The battle menu's INFO panel (src/fightstg/fightstg_8008D3B4.c): every legend
line, in every language and with the table's largest values, fits the legend's room in the 14-pixel font; no entry
has more than two lines; with each persona row's traits the panel ends inside the 240-line screen; and the traits
row's pictures never reach the turn queue at the row's right end. The widths are the font's own
(font_widths, from the EXE's glyph metrics): needs the disc.

The layout numbers below mirror the FIGHTSTG_INSPECT_* defines; change both together."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import dump_text as dt  # noqa: E402

LANGS = ["en", "pt", "fr", "de", "it", "es"]
X = 174                         # FIGHTSTG_INSPECT_X: the panel's text starts here
RIGHT = X - 4 + 138 - 4         # FIGHTSTG_INSPECT_RIGHT: and ends here
LEGEND_PX = RIGHT - (X + 14)    # FIGHTSTG_INSPECT_LEGEND_DX: a legend line's room after its picture
PICT, STEP, PIP_STEP = 11, 15, 4  # FIGHTSTG_INSPECT_PICT, _TRAIT_STEP, _PIP_STEP
LINE = 14                       # FIGHTSTG_INSPECT_LINE
LEGEND_Y = 92 + LINE + LINE * 3 + 4  # under a traits row and the grid's three lines
SCREEN_H = 240
BADGE_GAP = 3                   # pictures and queue never touch
QUEUE_W = 12 * 2 + 7            # the three chips: FIGHTSTG_INSPECT_QUEUE_STEP, _CHIP


def font_widths():
    """{character: pixels} of Font 1 (14 px), from the disc's character tables and src/main/font.c's glyphs."""
    chars, ext = dt.char_tables()
    src = (ROOT / "src/main/font.c").read_text()

    def parse_adv(name):
        block = src[src.index(f"FontGlyph {name}["):]
        block = block[:block.index("};")]
        out = []
        for line in block.splitlines()[1:]:
            nums = [int(x, 0) for x in re.findall(r"-?0x[0-9A-Fa-f]+|-?\d+", line)]
            if len(nums) == 11:
                out.append(nums[7] + nums[9])
        return out

    g14 = parse_adv("font_glyphs_14")
    g14_ext = parse_adv("font_glyphs_14_ext")
    px = {}
    for g, code in sorted(chars.items()):
        if 0 <= g - 4 < len(g14):
            px.setdefault(dt.sjis(code), g14[g - 4])
    for g, code in sorted(ext.items()):
        if 0 <= g - 1 < len(g14_ext):
            px.setdefault(dt.sjis(code), g14_ext[g - 1])
    for g, ch in dt.LATIN.items():
        if 0 <= g - 1 < len(g14_ext):
            px[ch] = g14_ext[g - 1]
    return px


def strings(lang):
    out = {}
    for line in (ROOT / f"tools/text_override/{lang}.txt").read_text(encoding="utf-8").splitlines():
        m = re.match(r"\$(Info\w+):\s*(.*)$", line)
        if m:
            t = m.group(2).strip()
            out[m.group(1)] = t[1:-1] if len(t) >= 2 and t[0] == t[-1] == '"' else t
    return out


def rows():
    """The persona table's rows as the traits they show (definitive_persona_table.c)."""
    src = (ROOT / "port/game/definitive_persona_table.c").read_text()
    for m in re.finditer(r"^\s*\{ ((?:SUPER|RARE|DRI|CAMPAIGN|SPECIES)\(.*?) \},(.*)$", src, re.M):
        r, note = m.group(1), m.group(2).strip()
        phases = re.search(r"PHASES\((\d+)\)", r)
        yield {
            "name": note or r,
            "shift": phases is not None or "ON_STAGGER" in r,
            "turns": int(phases.group(1)) if phases else 0,
            "guard": bool(re.search(r"\b(GUARD|RED)_(A|B|ALWAYS)\b", r)),
            "speed": int(m2.group(1)) if (m2 := re.search(r"SPEED_X\((\d+)\)", r)) else 0,
            "def": int(m2.group(1)) if (m2 := re.search(r"DEF_PCT\((\d+)\)", r)) else 0,
        }


def main():
    guard_pct = int(re.search(r"#define DEF_GUARD_PCT (\d+)", (ROOT / "port/game/definitive.h").read_text()).group(1))
    table = list(rows())
    px = font_widths()
    turns = max([r["turns"] for r in table] + [1])
    speed = max([r["speed"] for r in table] + [1])
    bad = 0
    for lang in LANGS:
        s = strings(lang)
        missing = [k for k in ("InfoShiftTurnsA", "InfoShiftTurnsB", "InfoShiftStagger", "InfoShiftWeak",
                               "InfoShiftGuard", "InfoShiftBoth", "InfoShiftRed", "InfoGuardA", "InfoGuardB", "InfoGuardUntil",
                               "InfoGuardRed",
                               "InfoSpeedA", "InfoSpeedB", "InfoDef") if k not in s]
        if missing:
            print(f"inspect_info: {lang}.txt has no {', '.join(missing)}")
            bad += 1
            continue
        entries = {  # what def_inspect_legend writes, with the largest numbers the table and the guard give
            "shift": [s["InfoShiftTurnsA"] + str(turns) + s["InfoShiftTurnsB"], s["InfoShiftStagger"],
                      s["InfoShiftWeak"], s["InfoShiftGuard"], s["InfoShiftBoth"], s["InfoShiftRed"]],
            "guard": [s["InfoGuardA"] + str(guard_pct) + s["InfoGuardB"], s["InfoGuardUntil"], s["InfoGuardRed"]],
            "speed": [s["InfoSpeedA"] + str(speed) + s["InfoSpeedB"]],
            "def": [s["InfoDef"]],
        }
        for kind, lines in entries.items():
            for text in lines:
                if "<" in text:
                    print(f"inspect_info: {lang}: {kind}: a line of its own may not break: {text!r}")
                    bad += 1
                w = sum(px.get(ch, 6) for ch in text)
                if w > LEGEND_PX:
                    print(f"inspect_info: {lang}: {kind}: {text!r} is {w} px, the legend has {LEGEND_PX}")
                    bad += 1
    for r in table:
        lines = 2 * r["shift"] + 2 * r["guard"] + (r["speed"] > 0) + (r["def"] > 0)
        bottom = LEGEND_Y + LINE * lines + 2
        if bottom > SCREEN_H:
            print(f"inspect_info: {r['name']}: the panel ends at line {bottom}, past {SCREEN_H}")
            bad += 1
        px = X
        if r["shift"]:
            px += STEP + (PIP_STEP * r["turns"] + 2 if r["turns"] else 0)
        px += STEP * (r["guard"] + (r["speed"] > 0) + (r["def"] > 0))
        end = px - (STEP - PICT) if px > X else X
        if end + BADGE_GAP > RIGHT - QUEUE_W:
            print(f"inspect_info: {r['name']}: the traits row ends at {end}, the turn queue starts at {RIGHT - QUEUE_W}")
            bad += 1
    print(f"inspect_info: {len(LANGS)} languages, {len(table)} rows, legend {LEGEND_PX} px: "
          + ("FAIL" if bad else "pass"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
