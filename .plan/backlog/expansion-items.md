# New items past the card's room: read from the expansion

Owner, 2026-11-23: future items (more Hunt Materials, variants, new item kinds) go to the expansion save
(`card1.def.json`, see the persona expansion task in `.plan/tasks/`), not the card.

What is on the card today (do not move it):

| kind | count | card room | where |
|---|---|---|---|
| common Hunt Materials | 16 | 16 (full) | X1 record, REC_EXTRA + 10..25, u8 each, cap 99 |
| rare items (one per Rare Target) | 71 | 128 | HUNT record, a count + an "ever held" bit each |
| Super Rare crystals | 13 | 16 | DEF_SUPER_RECORD bytes 8..23 |

The pattern for a new item:

1. Its own table (a new file, the way `definitive_hunt_targets.c` is), with a marker saying the item is stored in
   the expansion, so nothing indexes it into the card's arrays.
2. An expansion section per slot (for example `"items": {"<id>": count}`), registered with the core's
   load/save/reset callbacks. Keyed by a stable id, never by array index.
3. The materials screen and the market read the count through one accessor that picks the card or the expansion by
   that marker.
4. A test: round trip, a slot whose generation does not match (reads 0), and an id the table no longer has (kept in
   the file, ignored in game).

Blocked on: the launcher carrying `card1.mcd` and `card1.def.json` as a pair (backup, restore, cloud, mirror).
Until then, an item in the expansion has no backup and does not sync across devices.
