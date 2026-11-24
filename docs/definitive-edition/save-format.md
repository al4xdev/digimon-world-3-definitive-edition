# Save format

[Technical reference index](README.md) · [Player README](../../README.md)

## Save format (memory card records)

The card file is 0x8000 bytes.

| What | Where | Notes |
|---|---|---|
| Record 1 (BEST) | 0x7800 + slot*0x100, part 5, "BEST", version 1 | XOR checksum over the 193-byte species table; `REC_EXTRA` at 0xD0, 48 bytes, marker "X1" |
| Record 2 (HUNT) | 0x7B00, part 6, "HUNT" | rare items, DRI, Blast data, Blast boost, story flags and answers, skin, Super Rare state |
| Record 3 (BLST) | 0x7E00 + slot*0x80, part 7, "BLST" | up to 10 partner+form entries of 11 bytes: partner, step, 5 stats, hp, best-stat index, best value |
| Slot tail | Blast gauges (8 x s16) at slot+0x26F0; MP/TP converter marker at 0x26EC | "MPV1" is MP only, "DFV2" is MP and TP |

STGMCARD steps: 0x48 read and 0x36 write for record 1; 0x49 read and 0x37 write for record 2; 0x4A read and 0x38 write
for record 3. The slot size is 0x2700 (`memcard_init`, `src/main/memcard.c`), so three slots fit in 0x7800 and the card's
last 0x800 bytes hold eight 0x100 records the game never writes.

`REC_EXTRA` (record 1, offsets): 0-1 "X1"; 2 flags (`DEF_SAVE_HARD` 1, `DEF_SAVE_KAILI` 2); 4-7 Guardromon network mask; 8
region_area; 9 region_chain; 10-25 sixteen common materials (u8, cap 99); 26-27 replica bits (16); 28-29 and 30-33 DRI
unlocked/best stage per one PRD (see the DRI note above).

Record 2: 128 rare-item slots (cap 99) plus "ever held" bits; DRI bytes 152-153 unlocked and 154-157 best stage (2 bits
each); the Blast data (next step per partner `REC2_BLAST`, owed TP, quest stage, purified bits, the chosen partner and
charge); then, still in the Blast block (`REC2_BLAST_QUEST` + 5..8), `blast_boost` (the boost steps bought), `story_flags`
(u16: the story hooks' bits 0..7, `DEF_STORY_*`, and the Velvet Room's level last greeted in bits 12..14) and `story_gate`
(the Time Room's trials won in the run); at `REC2_ANSWERS` (= `REC2_BLAST_END`) the 40 bytes of story answers
(`DEF_STORY_ANSWERS` 160 x 2 bits, low bits first; see [The story layer](story.md#the-story-layer-velvet-room-in-the-main-story)); the
skin byte (`REC2_SKIN`); and the last 24 bytes, `DEF_SUPER_RECORD`. Compile-time checks keep the Blast block before the Super
Rare bytes and the answers before the skin byte; an older record reads all of these as 0.

Rules for any new saved state: write only when something is non-zero; zero it in `def_bestiary_reset`; register it in
`def_bestiary_savestate` (`PORT_STATE_VAR`); validate on read (the extras have no checksum). In detail (PRD 4's
checklist): `def_bestiary_record_write` writes record 1, and its "X1" part, only when the table or `extra_any()` has
something, so a new `REC_EXTRA` field must enter `extra_any()`, or a save holding only that state loses it; the record is
zeroed before writing and read only when "X1" is present, so an old save reads zero for every new field and a save with
nothing gained stays byte for byte the same; `out[5]` (the XOR checksum) covers only the species table, so clamp on
read (a material count past `DEF_HUNT_MATERIAL_CAP` reads as the cap); never grow `REC_EXTRA`: add a record per slot
instead (record 2 "HUNT" and record 3 "BLST" did exactly that). Converter:
`tools/convert_save_mp.py card1.mcd` (`--dry-run`; writes "DFV2"). Tests: `tests/saves`, `tests/port/definitive_bestiary.py`.

## The expansion save (`card1.def.json`)

What has no room on the card lives in a JSON beside it (`card1.mcd` -> `card1.def.json`), read whole when a slot is
loaded and written whole, before the card, when one is saved (`port/game/definitive_expansion.h`):

```json
{"version": 2, "slots": [{"gen": 7, "personas": {"3": {"known": [5, 0], "solved": false}}}, {"gen": 0}, ...]}
```

- **Pairing.** `REC_EXTRA` + 30 (the last 18 bytes of record 1) hold "EXP1" and the slot's generation (u32, low byte
  first). An entry whose `gen` is not the card's (a card restored or synced without its JSON) reads as empty, logged.
  Those bytes held the old persona solved bits (144), imported into the file on the first load.
- **Sections.** One named object per slot each (`definitive_expansion_sections.c`: `"personas"` and `"respec"`, the Velvet Room pig's swaps paid). Keys the
  build does not know are kept when the file is written again; a file of a newer `version` is read as empty and never
  overwritten. Adding a section: `.plan/tasks/expansion-save.md`.
- **Schema versions** (owner, 2026-11-24): `"version"` is 2 (1: the personas alone; 2: the `"respec"` section). Any schema change, a new
  section included, bumps it, and the launcher migrates the file before the game starts (`launcher/migrate.c`: one step per
  version, applied in sequence, the old file kept as `card1.def.json.v<N>.bak`). The game reads its own version or older.
- **A read that fails.** When the file cannot be read as the slot loads (a lock, an antivirus), the slot's sections are empty
  only because of that, so the slot's next save does not write the file (the card keeps its generation) until the slot is
  loaded again; that session's progress in the expansion is lost, the file's data is not.
- **The launcher** carries the two files as one save (backups, restore, cloud, the RecompLauncher mirror;
  `launcher/cloud.h`). Tests: `tests/port/definitive_expansion.py`.
