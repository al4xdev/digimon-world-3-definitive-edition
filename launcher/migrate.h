/* Versioned files and their migrations (owner, 2026-11-24). Every file the launcher or the game keeps carries a schema
 * number; a release that changes a schema adds ONE step to the file's table in migrate.c (from N to N+1) and raises the
 * file's current number. The launcher applies the steps in sequence from the file's own number up to the current one,
 * keeping the old file beside it as <file>.v<N>.bak first (never overwritten, so the oldest copy stays).
 *
 *   tui.state        "schema=N" (none: 0). The launcher's own: cfg_load migrates it before it reads it.
 *   cardN.def.json   "version": N, the expansion save the game writes (definitive_expansion.h): migrate_saves, before
 *                    the game starts. The game reads its own version or older, and leaves a newer file alone.
 *   settings.json    "schema": N is the game's contract (docs/LAUNCHER.md "Settings file"); the launcher rewrites it
 *                    whole on every save, so it needs no step.
 *
 * A file that arrives from outside (a restored backup, a cloud pull, the mirror) keeps its old number until the next
 * migrate_saves, which runs before every game start; the cloud then sees one local change and pushes it. */
#ifndef MIGRATE_H
#define MIGRATE_H

#include <stddef.h>

#define SCHEMA_TUI_STATE 2 /* 0 -> 1: Fight/60Hz defaults; 1 -> 2: drop USA lang (6 -> 0, 7 -> 6) */
#define SCHEMA_SAVE 2      /* 1 -> 2: the "respec" section (the pig's swaps); the data of version 1 is unchanged */
/* TODO, next SCHEMA_SAVE bump, 3 (owner, 2026-11-26): a clean cut, saves from before 0.9.1 are not migrated (players
 * were on a near-alpha), and the fresh-save notice (Cfg.v091_save_ok, main.c and main_android.c) goes with it.
 * 0.9.1 added the "story" section without bumping: 0.9.0 and 0.9.1 files are both version 2, told apart only by
 * "story", which an older save continued in 0.9.1 has too. The README's "Coming from an older version?" promises the
 * opposite: change it with the cut. */

enum { MIGRATE_NONE = 0, MIGRATE_DONE = 1, MIGRATE_NEWER = 2, MIGRATE_FAILED = -1 };

/* On the text of one file (the host tests call these). *text is malloc'd and replaced when a step ran; *from_out gets
 * the version the file had. Returns MIGRATE_*: NONE (current, or not a file of ours), DONE, NEWER (a newer file: left
 * as it is), FAILED (a step failed: *text is untouched). */
int migrate_tui_text(char **text, int *from_out);
int migrate_save_text(char **text, int *from_out);

/* On the files of the launcher's game_dir: the old file is kept as <file>.v<N>.bak, then replaced atomically.
 * Return the number of files migrated (0: none), or -1 when one failed (the file stays as it was). */
int migrate_config_files(void);
int migrate_tui_newer(void); /* 1 when tui.state is of a newer schema than this launcher's: cfg_save leaves it alone */
int migrate_saves(void);

#endif
