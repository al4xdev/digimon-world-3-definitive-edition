# Expansion save (JSON beside the memory card)

The card's free bytes are spent (record 2 full, record 3 ~10 bytes left). The expansion is a JSON file beside each
card image (`card1.mcd` -> `card1.def.json`, `card2.mcd` -> `card2.def.json`) that holds new port state and, later,
the overflow of any card limit. The legacy card records (BEST/X1, HUNT, BLST) are unchanged; the only card bytes the
expansion took are the 18 at the end of record 1 that held the personas' "solved" bits.

## Built (2026-11-23)

- `port/third_party/cjson/`: cJSON v1.7.19, unmodified (docs/THIRD_PARTY.md; LICENSE copied into the PC and
  PortMaster packages as `LICENSES/cJSON.txt`). `port/CMakeLists.txt` appends it to the adapter, so the desktop,
  Windows, Android and PortMaster builds (all through `port/`) compile it.
- Core: `port/game/definitive_expansion.h/.c`. Owns the file (read whole on load, written whole on save through
  `.tmp` + fsync/_commit + `port_file_replace`), the pairing with the card, the mismatch log, the savestate, and the
  passthrough of every key it or a section does not know (top level, the slot's entry, other builds' sections). A
  file of a newer `version` is read as empty and never overwritten; a file that is not JSON is moved to `.bad`.
  The core knows nothing of what a section holds.
- Sections: `port/game/definitive_expansion_sections.c` lists them (order = savestate order). First consumer:
  `port/game/definitive_persona_save.c`, section `"personas"`.
- Hooks (`src/stgmcard/stgmcard_80082CD0.c`, PC_PORT blocks): load step 0x48 calls `def_expansion_load(port, slot)`
  right after `def_bestiary_record_read` (which takes the generation from the record); save step 0x33 calls
  `def_expansion_save(port, slot)` before `def_bestiary_record_write`, so the file is on disk before step 0x34 starts
  writing the card. Both only under `def_bestiary_active()` (never in replay scripts).
- Path: from `port_settings.memcard[port]` (the `--config` file: the launcher, Android, PortMaster). No settings (a
  bare `--memcard1 FILE` run, tests) -> the JSON is kept in memory for the run only. A run with both `--config` and a
  `--memcard1` override would pair the override card with the config card's JSON: the generation catches it (empty).
  A `port_memcard_path(slot)` accessor in psxstack would close both gaps (not done: it is a stack change).

### File shape

```json
{"version": 1,
 "slots": [{"gen": 7, "personas": {"3": {"known": [5, 0], "solved": false}}},
           {"gen": 0},
           {"gen": 2, "personas": {"0": {"known": [127, 127], "solved": true}}}]}
```

### Pairing

- Card side (record 1, `REC_EXTRA + 30`, 18 bytes): `"EXP1"`, the generation (u32, low first), 10 spare zeros.
  No generation -> all zeros, so a slot with nothing in the expansion keeps a record byte-identical to before.
- Save: the new generation is `max(card's, file entry's) + 1` (never 0), so a new game saved over a slot or an older
  card restored never meets an entry with its number. The file is written first; the card takes the new number only
  if that write succeeded (otherwise it keeps the old one, which the untouched old entry still matches).
- Nothing kept and no generation: the card bytes stay zero; if the file's entry still has section data (a new game
  saved over the slot) those sections are emptied and the entry's gen is kept as the high-water mark.
- Load: entry gen == card gen -> every section loads; anything else (no file, no entry, another number, not JSON,
  newer version) -> every section empty, `port_log("expansion: slot N: ...: the slot's expansion reads as empty")`.
- Migration: record bytes without `"EXP1"` go to each section's `migrate_from_card`. Personas read them as the old
  144 "solved" bits (KEEP id = bit) and import those ids as solved with the whole map. While not yet saved, the card
  write puts the old bytes back unchanged (a failed file write cannot lose them). No shipped row ever had a KEEP, so
  in practice these bytes are zero on every existing card; the magic makes old vs new unambiguous anyway.
- Known limits: the counter does not make cross-device collisions impossible (two devices from the same gen both
  save gen+1 with different data; a card from one with the JSON from the other would pair). Sync will need the pair
  (below). A card write that fails after the JSON's leaves card gen N, file gen N+1 -> that slot reads empty.
  Unknown sections of a mismatched entry are kept (not wiped) and re-stamped with the new gen on the next save.

## How to add a section (for the card-limit tasks)

1. A file `port/game/definitive_<thing>_save.c` with the state in memory (a fixed-size static is fine; the JSON is
   only the disk format) and a `const DefExpansionSection def_<thing>_section = { "<name>", reset, load, save, any,
   migrate_from_card, savestate };` (definitive_expansion.h documents each callback):
   - `reset`: empty. Called on a new game, before every load and migrate, and on a mismatch.
   - `load(const cJSON *obj)`: obj is the slot entry's `"<name>"` (NULL when absent). Missing keys = 0, ignore unknown.
   - `save(cJSON *obj)`: fill a fresh empty object (the core replaces the old one; keys you do not write are gone, so
     a section that wants to keep a newer build's keys inside its own object must copy them itself).
   - `any`: 1 when it has something to keep (drives whether a generation is taken at all).
   - `migrate_from_card`: NULL unless the section's data used to live in the 18 expansion bytes. A limit whose
     overflow comes from another record (HUNT, BLST...) keeps that record as it is and moves only what does not fit:
     its own record read hook decides, not this callback.
   - `savestate`: `PORT_STATE_VAR` its statics.
2. Append `&def_<thing>_section` to `def_expansion_sections[]` (never reorder, rename or remove a shipped name).
3. The consumer's game code talks to its own small API (like `def_persona_record_known/learn/solved/solve`), never to
   the file.
4. Tests: link the section in `tests/port/definitive_expansion.py` (it uses the real section list) and add its cases
   there or in its own driver; tests that link `definitive_bestiary.c` without the core use
   `tests/port/definitive_expansion_stub.c`.

## Launcher: carries the pair (done 2026-11-23)

`launcher/cloud.c` treats `card1.mcd` + `card1.def.json` as one save (see `launcher/cloud.h`):

- The sha is sha256(card then JSON): the card's own sha when there is no JSON, so `cloud_sync.json`, backup names and
  the mirror's state from before keep matching.
- Backups write `card1_<stamp>_<sha>.def.json` beside the card; rotation drops both; restore and adopt write the JSON
  first, then the card (the game's order); a source with no JSON removes the local one (already backed up).
- Cloud: push sends the card then the JSON (`X-Save-Expansion-Size`, `X-Save-Pair-Sha256`); pull asks for it
  (`X-Accept-Expansion: 1`). The worker (`cloud-sync/`) keeps `sha256` as the card's for the old clients and adds
  `pair_sha256` + `ext_blob` (`migrations/0002_save_expansion.sql`, applied to the remote D1). An old client's push (the
  card alone) clears the stored JSON; an old client's pull gets exactly the 128 KB card.
- RecompLauncher mirror: format 1 while there is no JSON (old launchers read it), format 2 (`expansion_size`, the JSON
  after the card) when there is.
- Tests: the launcher's `--self-test` (8 expansion checks) and `cloud-sync` `npm test` (an expansion case). The e2e
  launcher test is stale and unsafe to run (`.plan/backlog/launcher-expansion-save.md`).

Still to do: the schema migration screen (`.plan/backlog/launcher-expansion-save.md`).
## Not verified in game

- A real save/load through the stgmcard screen writing and reading `card1.def.json` (only the host tests drove it).
- No shipped persona row has a KEEP yet, so nothing will be written until the table gets one.
