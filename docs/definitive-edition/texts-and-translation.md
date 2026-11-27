# Texts and translation

[Technical reference index](README.md) · [Player README](../../README.md)

## Texts and translation (EN / PT-BR)

- Overrides live in `tools/text_override/en.txt` and `pt.txt`: lines `FILE index: text`, `FILE:sub index: text`, and
  `$NAME: text` for the port's own strings. `tools/gen_text_override.py` (with `--check`) generates
  `port/game/definitive_text.c` and `definitive_text_lang.inc`. Technique descriptions are limited to 34
  characters.
- Fonts: the 11px font has no `/ : , '` and no accents (the 14px one is similar). A speech box holds 3 lines (split with
  `<2,2><3>`).
- **PT-BR is a 6th language**, falling back to English per entry (`def_is_ptbr()`; `records_language` stays 2). The
  other languages (FR/IT/DE/ES) show the disc's text for the original entries, their own translation (`fr.txt`, `de.txt`,
  `it.txt`, `es.txt`) for the Definitive delta, their own story rewrite (see below) and English for what is not
  translated. The language is `def_lang()` (0 en, 1 pt, 2 fr, 3 de,
  4 it, 5 es: from the preset or `DW3_LANG=pt`, else `records_language`); `def_text_overrides_of(lang)` gives the table
  and `def_pick()` picks among six C literals (title questions, Rush and flee prompts, compass). The callout font
  (`def_poly_text`) draws capitals only: no apostrophe, `¿ ¡` or `ß` (write SS).
- **The story rewrite** (`<lang>_story.txt`, `<lang>_kaili.txt`, `<lang>_choices.txt`, `<lang>_echoes.txt` for `en`,
  `pt`, `fr`, `de`, `it` and `es`) is generated into `port/game/definitive_story_text.c`. Formats, counts and the lookup
  order are in [The story layer](story.md#the-story-layer-velvet-room-in-the-main-story).
- **PT-BR is the story rewrite's source language**, in the tone of the classic 80s/90s Brazilian anime dubs; the
  English files are translated from it.
- Glossary (PT-BR): keep in English `WEAK`, `RESIST`, `OVERFINISH`, `Baton Pass`, `Relay`, `Libra`, `Digimon`,
  `Digi-Lab`, `Load Tech`, `Blast Gauge`, `Bits`, `TP`, `MP`, `HP` and proper names; use Fogo, Agua, Gelo, Vento,
  Trovao, Maquina, Trevas for elements.
- **PT-BR in the game:** `portuguese` is a value of `preset_language`; with it active `records_language` stays `2` (the
  `ES*.BIN` English files) and `def_is_ptbr()` is true; `def_text_override(data, i)` and the `def_str_*` strings return
  the entry of `def_text_overrides_pt` (binary search by `(file_id, subtable, index)`), falling back to English. The European
  font has accented slots under `01 xx` (`0x3B..0x72`); whether `ã`/`õ` exist is unverified, and the current texts avoid
  accents in the small fonts.
- **Translation checks:** every control code (`<1>`, `<2,...>`, `<5,1>`, `\n`, variables, colour codes) is preserved
  exactly, and each window keeps its limit (34 characters in `SKLINF`, pixel width from the font metrics for dialogue and
  menus). Stats are Forca, Defesa, Espirito, Sabedoria, Velocidade; `SKLINF` uses the lean pattern with turns
  ("Atq+2 no usuario, 3 turnos.").
- **FR/DE/IT/ES:** the EU disc has the original campaign in French (`records_language` 3), Italian (4), German (5) and
  Spanish (6); the Definitive delta is `tools/text_override/{fr,de,it,es}.txt`, one line per key of `en.txt`, read by
  `tools/gen_text_override.py` into `definitive_text_lang.inc`. Plates stay in English.
