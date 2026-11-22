# Texts and translation

[Technical reference index](README.md) · [Player README](../../README.md)

## Texts and translation (EN / PT-BR)

- Overrides live in `tools/text_override/en.txt` and `pt.txt`: lines `FILE index: text`, `FILE:sub index: text`, and
  `$NAME: text` for the port's own strings. `tools/gen_text_override.py` (with `--check`) generates
  `port/game/definitive_text.c` and `definitive_text_lang.inc`. Technique descriptions are limited to 34 characters
  (`tools/gen_tech_text.py`, `gen_affinity_text.py`).
- Fonts: the 11px font has no `/ : , '` and no accents (the 14px one is similar). A speech box holds 3 lines (split with
  `<2,2><3>`).
- **PT-BR is a 6th language**, falling back to English per entry (`port_language_ptbr`; `records_language` stays 2). The
  other languages (FR/IT/DE/ES) show the disc's text for the original entries, their own translation (`fr.txt`, `de.txt`,
  `it.txt`, `es.txt`, 854 entries each, 2026-11-20) for the Definitive delta and English for what is not translated
  (the story rewrite, `dw3_velvet_text_own` entries without a line). The language is `def_lang()` (0 en, 1 pt, 2 fr, 3 de,
  4 it, 5 es: from the preset or `DW3_LANG=pt`, else `records_language`); `def_text_overrides_of(lang)` gives the table
  and `def_pick()` picks among six C literals (title questions, Rush and flee prompts, compass). The callout font
  (`def_poly_text`) draws capitals only: no apostrophe, `¿ ¡` or `ß` (write SS). The EU disc already has the original campaign in FR/DE/IT/ES; only the definitive
  delta (the guide, bestiary, hunt, forge, Black Market, DRI, `SKLINF`, Velvet Room, Super Rare screens) is missing there.
- **The story rewrite** (`en_story.txt`, `en_kaili.txt`, `pt_kaili.txt`, `en_choices.txt`, `en_echoes.txt`, generated into
  `port/game/definitive_story_text.c`, checked by `tools/check_story_text.py`) is English only; formats, counts and the
  lookup order are in [The story layer](story.md#the-story-layer-velvet-room-in-the-main-story).
- PT pipeline: `tools/translate_ptbr.py` talks to a local Gemma server (no sampling parameters sent; `--benchmark` compares
  reasoning low vs medium on 20 real strings; `--tier p0|all`; `--export`). Glossary: keep in English `WEAK`, `RESIST`,
  `OVERFINISH`, `Baton Pass`, `Relay`, `Libra`, `Digimon`, `Digi-Lab`, `Load Tech`, `Blast Gauge`, `Bits`, `TP`, `MP`, `HP`
  and proper names; use Fogo, Agua, Gelo, Vento, Trovao, Maquina, Trevas for elements. A cache with tag/width validation lives
  under `.dumps/tasks/translation/`. The other four languages were translated by Haiku subagents straight from `en.txt` and checked with
  `tools/check_pt_text.py` (0 errors; heavy-page and width warnings remain, pixel widths are unverified in game).
- Glyph fix (plan 9): despite the name, it is the **button-glyph/place translation bug** of the battle menu (`PLACE_TO_GAME`), not a font fix; the cause, the fix and the 11 simplification items (done status
  unverified) are in [Battle HUD: design record](interface.md#battle-hud-design-record-plan-8-the-opt-in-mod-that-became-always-on-and-the-glyph-fix-plan-9).
- **PT-BR pipeline spec (plan 15).** Goal: PT-BR as a 6th selectable language in the launcher and the handheld menu (loads the disc's English base and applies the `pt.txt` override with automatic English fallback);
  `portuguese` is a value of `preset_language` (`mod.json`, `game_mods.c`) and of the language selector of the C launcher (`launcher/cfg.c`, `launcher/ui.c`); with it active `records_language` stays `2` (the `ES*.BIN`
  English files) and `port_language_ptbr = 1`; `def_text_override(data, i)` and the `def_str_*` strings return the PT entry of `definitive_text_pt` (binary search by `(file_id, subtable, index)`) with fallback to English.
  Accents: the European font has the `01 xx` slots (`0x3B..0x72`: a with acute, e acute, i acute, o acute, u acute, a circumflex, e circumflex, o circumflex, a grave, c cedilla, u diaeresis, capital E/I/O/U acute); the plan said to
  check `ã`/`õ` and, if the texture lacked them, inject the glyphs in RAM under `PC_PORT` so the accent is never replaced by a plain `a` (whether this was done: unverified; the current texts avoid accents in the small fonts).
  `tools/translate_ptbr.py` calls the local Gemma 4 server directly (about 150 tok/s) **without sending temperature, top_p, top_k or repetition_penalty**; a mini-benchmark on 20 real strings (short 34-char descriptions, menus,
  Guide, `TALK*` dialogue with tags `<1>`, `<5,1>`) compares `reasoning_effort` low vs medium and picks the one with 100% respect for tags and size at the best throughput (`--benchmark`). Fixed glossary: keep in English WEAK,
  RESIST, OVERFINISH, Baton Pass, Relay, Libra, Digimon, Digi-Lab, Load Tech, Blast Gauge, Bits, TP, MP, HP, species and NPC names; standardise elements (Fogo, Agua, Gelo, Vento, Trovao, Maquina, Trevas), stats (Forca, Defesa,
  Espirito, Sabedoria, Velocidade) and the lean SKLINF pattern with turns ("Atq+2 no usuario, 3 turnos.", "Def-2 no alvo, 3 turnos.", "Magia de Fogo. Causa Veneno."). Incremental cache `.dumps/tasks/translation/cache_ptbr.jsonl`
  deduplicating strings repeated across maps; automatic per-line validation: exact preservation of every control code (`<1>`, `<2,...>`, `<5,1>`, `\n`, variables, colour codes), strict character/pixel limit per window (34 chars in
  SKLINF, pixel width from the font metrics for `TALK*`/`DMG*` and menus), auto-retry with size feedback on overflow. Execution order planned: the buff/debuff SKLINF batch was held until the tier work finished.
  **FR/DE/IT/ES (done 2026-11-20):** the EU disc has the original campaign in French (`records_language` 3), Italian (4), German (5) and
  Spanish (6); the Definitive delta is `tools/text_override/{fr,de,it,es}.txt`, one line per key of `en.txt` (854 each), read by
  `tools/gen_text_override.py` into `definitive_text_lang.inc`. The `--lang` and `--only-definitive-delta` options of the old plan were not
  needed. Open: a native read in game (width of the 14 px labels, the heavy dialogue pages, plates kept in English), and the story rewrite
  (`en_story`, `en_choices`, `en_echoes`, `en_kaili`), which exists only in English and Portuguese.
