# Launcher: carry and migrate the expansion save

Owner, 2026-11-23: not now. The next update that touches the expansion save (`card1.def.json`) does both.

1. **Pair the files.** DONE 2026-11-23 (launcher/cloud.c, cloud-sync worker deployed, D1 migrated). Was: Backup, restore, cloud sync and the RecompLauncher mirror are keyed only on `card1.mcd`. They
   must move `card1.mcd` and `card1.def.json` together (a restored card without its JSON reads that slot's expansion
   data as empty, by the generation check, so nothing is wrong, only lost).
2. **Schema migration.** DONE 2026-11-24 (launcher/migrate.c, no screen: it migrates silently before the game starts,
   the old file kept as .v<N>.bak; docs/LAUNCHER.md "Schema versions"). Was: the JSON carries `"version"`. When a release changes the schema, the launcher shows a
   screen that migrates the file (backup the old one first, then rewrite it to the new version) before the game runs.
   The game itself only reads its own version or older; a newer file is read as empty and not overwritten.

Also listed in the READMEs' "What's left" and in `.dumps/release-0.8.14.md`. Related:
`.plan/backlog/expansion-items.md`.

## Found 2026-11-23 while doing step 1

`cloud-sync/test/e2e_launcher.test.mjs` is stale and unsafe: its menu script predates the current menus (the confirm
button is B now, "a" does nothing) and fails at `tui.state` even on the HEAD launcher; worse, `--pc --dir <tmp>`
does not move the settings, so a run rewrites the developer's real `~/.local/share/dw2003/{settings.json,tui.state,
pad.map}` (same values back, this time). Fix the isolation (an env or flag for the settings dir) before running it
again, then the script. The pair is covered by the launcher's `--self-test` (backups, restore, mirror) and the
worker's own test (push/pull, old clients), not by this e2e. Also: `make` without `SDL=1` fails on `SDL_OpenURL`.

## Left from the 0.8.14 release review (2026-11-23, minor)

- The game derives `card1.def.json` from `port_settings.memcard[port]`; `--memcard1 X.mcd` on the command line opens
  another card but still pairs with the config card's JSON. The launcher passes only `--config`, so players are not
  hit. Fix: derive the path from the card `port_memcard_open` really used.
- A load that fails transiently (the JSON unreadable) leaves the sections empty; the next save of that slot writes
  them empty with a higher generation over the real entry. DONE 2026-11-24: `load_failed` in definitive_expansion.c
  makes the save leave the file and the card's generation alone until the slot is loaded again (a new game, a card read
  or a load that worked clears it); tests/port/definitive_expansion_driver.c "unreadable load".
- A crash between the JSON write and the card write loses that save's persona progress (by design: the entry's gen
  is newer than the card's). Softer rule possible: accept gen == card gen + 1.
- The mirror's format 2 JSON is not validated on adopt beyond its sha.
- Unrelated, found running the port layer: `tests/port/field.py` swamp_flies fails since at least 2026-11-17: its
  script uses a `write_mem` step the pinned psxstack runtime does not have ("unknown step type"). CI's `port` job is
  skipped without the disc, so it never showed there. Also `scripts/test.sh --layer port` stops at `vram.py` (exit 2)
  on a machine without pcsx-redux instead of skipping it.
