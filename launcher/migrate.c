/* Schema migrations (migrate.h): one table of steps per versioned file, applied in sequence. A step is a plain function
 * on the file's text that does what changes from N to N+1 and nothing else; the engine below stamps the new number. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cfg.h"
#include "compat.h"
#include "migrate.h"

typedef int (*MigrateStep)(char **text); /* 0 ok */

/* ---- key=value text (tui.state) ---- */

/* The value of `key` in a line `key=value`, or NULL when the file has no such line (points into text). */
static const char *kv_find(const char *text, const char *key) {
    size_t kl = strlen(key);
    for (const char *p = text; *p;) {
        if (!strncmp(p, key, kl) && p[kl] == '=') return p + kl + 1;
        p = strchr(p, '\n');
        if (!p) break;
        p++;
    }
    return NULL;
}

/* A whole number from the start of `s`, for a version: digits only, then the end of the value. -1 when it is not one
 * (a newer or foreign file reads as one that is too new, never as 0); huge numbers cap, so nothing overflows. */
static int parse_version(const char *s) {
    long v = 0;
    int neg = 0, digits = 0;
    if (*s == '-') { neg = 1; s++; }
    for (; *s >= '0' && *s <= '9'; s++, digits++) {
        if (v < 1000000) v = v * 10 + (*s - '0');
    }
    if (!digits || (*s && *s != '\n' && *s != '\r' && *s != ' ' && *s != '\t' && *s != ',' && *s != '}')) return -1;
    return neg ? -1 : (int)v;
}

static int kv_int(const char *text, const char *key, int def) {
    const char *v = kv_find(text, key);
    return v ? parse_version(v) : def;
}

static int kv_is(const char *text, const char *key, const char *value) {
    const char *v = kv_find(text, key);
    size_t n = strlen(value);
    return v && !strncmp(v, value, n) && (v[n] == '\n' || v[n] == '\r' || v[n] == 0);
}

/* Sets `key=value` (replacing the line, or appending it); value NULL removes the line. */
static int kv_set(char **text, const char *key, const char *value) {
    const char *old = kv_find(*text, key);
    size_t kl = strlen(key), len = strlen(*text);
    size_t start = old ? (size_t)(old - *text) - kl - 1 : len;
    size_t end = start;
    if (old) {
        end = start + kl + 1;
        while ((*text)[end] && (*text)[end] != '\n') end++;
        if ((*text)[end] == '\n') end++;
    }
    size_t add = value ? kl + strlen(value) + 2 : 0;
    char *out = malloc(len - (end - start) + add + 2);
    if (!out) return -1;
    memcpy(out, *text, start);
    size_t n = start;
    if (!old && n && out[n - 1] != '\n') out[n++] = '\n'; /* a file that did not end its last line */
    if (value) n += (size_t)sprintf(out + n, "%s=%s\n", key, value);
    strcpy(out + n, *text + end);
    free(*text);
    *text = out;
    return 0;
}

/* ---- tui.state ---- */

/* 0 -> 1. Both are defaults a tui.state written before this never chose, so a file still on them takes the new ones;
 * anything else was the player's and stays:
 *  - The Fight button (2026-11-23, was bm_v=2): Fight moved to the bottom face button. A file with Fight right and Tag
 *    bottom had the old defaults (every save writes all eight buttons): the two swap, as cfg_set_fight_place does.
 *  - 60 Hz (2026-11-24): the launcher had no screen for the refresh rate, so its 50 was never a choice. */
static int tui_0_to_1(char **text) {
    if (kv_int(*text, "bm_v", 0) < 2 && kv_is(*text, "bm_fight", "east") && kv_is(*text, "bm_tag", "south")) {
        if (kv_set(text, "bm_fight", "south") || kv_set(text, "bm_tag", "east")) return -1;
    }
    if (kv_int(*text, "hz", 60) == 50 && kv_set(text, "hz", "60")) return -1;
    return kv_set(text, "bm_v", NULL);
}

static int tui_1_to_2(char **text) {
    int lang = kv_int(*text, "lang", -1);
    if (lang == 6) return kv_set(text, "lang", "0");
    if (lang == 7) return kv_set(text, "lang", "6");
    return 0;
}

static const MigrateStep tui_steps[] = {tui_0_to_1, tui_1_to_2}; /* tui_steps[N] takes version N to N + 1 */

int migrate_tui_text(char **text, int *from_out) {
    int v = kv_int(*text, "schema", 0), from = v;
    if (from_out) *from_out = from;
    if (v == SCHEMA_TUI_STATE) return MIGRATE_NONE;
    if (v > SCHEMA_TUI_STATE || v < 0) return MIGRATE_NEWER; /* too new, or not a number: not ours to change */
    char *t = strdup(*text);
    if (!t) return MIGRATE_FAILED;
    for (; v < SCHEMA_TUI_STATE; v++) {
        char num[16];
        if (tui_steps[v](&t)) { free(t); return MIGRATE_FAILED; }
        snprintf(num, sizeof(num), "%d", v + 1);
        if (kv_set(&t, "schema", num)) { free(t); return MIGRATE_FAILED; }
    }
    free(*text);
    *text = t;
    return MIGRATE_DONE;
}

/* ---- the expansion save (card1.def.json) ---- */

/* The number after the top-level "version" key: where it starts, its length, and its value; 0 when the text is not
 * the game's file ({"version": N, ...}: the key comes first, the game writes it first). */
static int save_version(const char *text, size_t *at, size_t *len, int *value) {
    const char *p = text;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p++ != '{') return 0;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (strncmp(p, "\"version\"", 9)) return 0;
    p += 9;
    while (*p == ' ' || *p == '\t') p++;
    if (*p++ != ':') return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return 0;
    const char *s = p;
    while (*p >= '0' && *p <= '9') p++;
    *at = (size_t)(s - text);
    *len = (size_t)(p - s);
    *value = parse_version(s);
    /* a whole number only: 1.5 or 1e0 is not a version the game wrote */
    return *p == ',' || *p == '}' || *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t';
}

static int save_set_version(char **text, int version) {
    size_t at, len;
    int v;
    if (!save_version(*text, &at, &len, &v)) return -1;
    char num[16], *out = malloc(strlen(*text) + 16);
    if (!out) return -1;
    snprintf(num, sizeof(num), "%d", version);
    memcpy(out, *text, at);
    strcpy(out + at, num);
    strcat(out, *text + at + len);
    free(*text);
    *text = out;
    return 0;
}

/* 1 -> 2: the "respec" section is new; a slot without it reads as no swaps, so nothing in the data changes. */
static int save_1_to_2(char **text) {
    (void)text;
    return 0;
}

static const MigrateStep save_steps[] = {NULL, save_1_to_2}; /* save_steps[N] takes version N to N + 1 */

int migrate_save_text(char **text, int *from_out) {
    size_t at, len;
    int v;
    if (!save_version(*text, &at, &len, &v)) return MIGRATE_NONE; /* not the game's file: not ours to touch */
    if (from_out) *from_out = v;
    if (v == SCHEMA_SAVE) return MIGRATE_NONE;
    if (v > SCHEMA_SAVE || v < 0) return MIGRATE_NEWER; /* the game leaves it as it is too */
    if (v < 1) return MIGRATE_NONE;
    char *t = strdup(*text);
    if (!t) return MIGRATE_FAILED;
    for (; v < SCHEMA_SAVE; v++) {
        if (save_steps[v](&t) || save_set_version(&t, v + 1)) { free(t); return MIGRATE_FAILED; }
    }
    free(*text);
    *text = t;
    return MIGRATE_DONE;
}

/* ---- the files ---- */

/* One file: read, migrate, keep the old as <file>.v<N>.bak (the first one stays), write the new atomically. */
static int migrate_file(const char *path, int (*fn)(char **, int *)) {
    char *text = slurp(path);
    if (!text) return 0;
    int from = 0, r = fn(&text, &from);
    if (r == MIGRATE_DONE) {
        char bak[700];
        char *old = slurp(path);
        /* <file>.v<N>.bak, or .v<N>.2.bak ... when an older copy of that version is already there (a later restore
         * or pull brought another one): no copy is ever replaced. */
        snprintf(bak, sizeof(bak), "%s.v%d.bak", path, from);
        for (int k = 2; access(bak, F_OK) == 0 && k < 100; k++) snprintf(bak, sizeof(bak), "%s.v%d.%d.bak", path, from, k);
        if (!old || access(bak, F_OK) == 0 || write_atomic(bak, old) != 0 || write_atomic(path, text) != 0) r = MIGRATE_FAILED;
        free(old);
    }
    free(text);
    return r == MIGRATE_DONE ? 1 : r == MIGRATE_FAILED ? -1 : 0;
}

int migrate_config_files(void) {
    char p[600];
    snprintf(p, sizeof(p), "%s/tui.state", game_dir);
    return migrate_file(p, migrate_tui_text);
}

int migrate_tui_newer(void) {
    char p[600];
    snprintf(p, sizeof(p), "%s/tui.state", game_dir);
    char *t = slurp(p);
    if (!t) return 0;
    int v = kv_int(t, "schema", 0);
    free(t);
    return v > SCHEMA_TUI_STATE || v < 0;
}

int migrate_saves(void) {
    int n = 0, bad = 0;
    for (int card = 1; card <= 2; card++) {
        char p[600];
        snprintf(p, sizeof(p), "%s/card%d.def.json", game_dir, card);
        int r = migrate_file(p, migrate_save_text);
        if (r > 0) n += r;
        if (r < 0) bad = 1;
    }
    return bad ? -1 : n;
}
