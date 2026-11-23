# Launcher: carry and migrate the expansion save

Owner, 2026-11-23: not now. The next update that touches the expansion save (`card1.def.json`) does both.

1. **Pair the files.** DONE 2026-11-23 (launcher/cloud.c, cloud-sync worker deployed, D1 migrated). Was: Backup, restore, cloud sync and the RecompLauncher mirror are keyed only on `card1.mcd`. They
   must move `card1.mcd` and `card1.def.json` together (a restored card without its JSON reads that slot's expansion
   data as empty, by the generation check, so nothing is wrong, only lost).
2. **Schema migration screen.** The JSON carries `"version"`. When a release changes the schema, the launcher shows a
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
