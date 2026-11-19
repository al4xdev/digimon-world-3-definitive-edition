#include "cfg.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <time.h>
#include <unistd.h>

#include "compat.h"
#include "plat.h"
#include "win32.h"

int is_pc, is_portmaster, is_android;
char game_dir[512] = ".";
char disc_path[1200];

static const struct { int code; const char *name; const char *display; } places[] = {
    {K_SOUTH, "south", "SOUTH"}, {K_EAST, "east", "EAST"}, {K_NORTH, "north", "WEST"}, {K_WEST, "west", "NORTH"},
    {K_L1, "leftshoulder", "L1"}, {K_R1, "rightshoulder", "R1"}, {K_L2, "lefttrigger", "L2"},
    {K_R2, "righttrigger", "R2"}, {K_SELECT, "back", "SELECT"}, {K_START, "start", "START"},
};
#define NPLACES ((int)(sizeof(places) / sizeof(places[0])))

const char *code_name(int code) {
    for (int i = 0; i < NPLACES; i++) {
        if (places[i].code == code) {
            return places[i].name;
        }
    }
    return "south";
}

const char *place_display(int code) {
    for (int i = 0; i < NPLACES; i++) {
        if (places[i].code == code) {
            return places[i].display;
        }
    }
    return "?";
}

static int code_by_name(const char *n) {
    for (int i = 0; i < NPLACES; i++) {
        if (strcmp(places[i].name, n) == 0) {
            return places[i].code;
        }
    }
    return 0;
}

/* The battle menu names its places "east", "l1"...: the same buttons, in the mod's own spelling. */
static const struct { const char *name; int code; } hud_places[] = {
    {"east", K_EAST}, {"l1", K_L1}, {"north", K_NORTH}, {"south", K_SOUTH},
    {"r1", K_R1}, {"west", K_WEST}, {"l2", K_L2}, {"r2", K_R2},
};
#define NHUD 8

static const char *hud_name(int code) {
    for (int i = 0; i < NHUD; i++) {
        if (hud_places[i].code == code) {
            return hud_places[i].name;
        }
    }
    return "east";
}

static int hud_code(const char *n) {
    for (int i = 0; i < NHUD; i++) {
        if (strcmp(hud_places[i].name, n) == 0) {
            return hud_places[i].code;
        }
    }
    return 0;
}

int hud_cycle(int code, int dir) {
    int i = 0;
    for (; i < NHUD; i++) {
        if (hud_places[i].code == code) {
            break;
        }
    }
    return hud_places[((i < NHUD ? i : 0) + dir + NHUD) % NHUD].code;
}

/* pad.map names the places in the game's SDL vocabulary, a quarter turn from the kernel's. */
static const char *game_name(int code) {
    switch (code) {
    case K_SOUTH: return "west";
    case K_NORTH: return "north";
    case K_WEST: return "east";
    case K_EAST: return "south";
    }
    return code_name(code);
}

/* settings.json's input.gamepad on Android: SDL3 reports a phone's or a Bluetooth pad's face buttons at their real
 * places (plat.c: SDL's west arrives as K_NORTH, its north as K_WEST), with none of the handheld's quarter turn. */
static const char *sdl3_name(int code) {
    switch (code) {
    case K_SOUTH: return "south";
    case K_EAST: return "east";
    case K_NORTH: return "west";
    case K_WEST: return "north";
    }
    return code_name(code);
}

static int code_from_game(const char *n) {
    if (!strcmp(n, "west")) return K_SOUTH;
    if (!strcmp(n, "north")) return K_NORTH;
    if (!strcmp(n, "east")) return K_WEST;
    if (!strcmp(n, "south")) return K_EAST;
    return code_by_name(n);
}

static const char *action_name[A_COUNT] = {"cross", "square", "circle", "triangle", "l1", "r1", "start", "select"};

char layout_letter(int layout, int code) {
    static const char xbox[4] = {'A', 'B', 'X', 'Y'}, nin[4] = {'B', 'A', 'Y', 'X'};
    int i = code == K_SOUTH ? 0 : code == K_EAST ? 1 : code == K_NORTH ? 2 : code == K_WEST ? 3 : -1;
    if (i < 0 || layout == LAYOUT_PS1 || layout == LAYOUT_CUSTOM) {
        return '-';
    }
    return layout == LAYOUT_XBOX ? xbox[i] : nin[i];
}

void cfg_set_layout(Cfg *c, int layout) {
    for (int a = 0; a <= A_TRIANGLE; a++) {
        c->pad[a].letter = layout_letter(layout, c->pad[a].code);
    }
}

int cfg_layout(const Cfg *c) {
    for (int l = LAYOUT_PS1; l <= LAYOUT_NINTENDO; l++) {
        int ok = 1;
        for (int a = 0; a <= A_TRIANGLE; a++) {
            ok &= c->pad[a].letter == layout_letter(l, c->pad[a].code);
        }
        if (ok) {
            return l;
        }
    }
    return LAYOUT_CUSTOM;
}

void cfg_defaults(Cfg *c) {
    memset(c, 0, sizeof(*c));
    c->hz = 50;
    c->ff = 3;
    c->load = 3; /* faster loading on at 8x (owner, 2026-11-18; it was 4x) */
    c->btext = 1; /* skip: the definitive HUD has no need of battle messages */
    c->bmem = 1;
    c->bbuf = 1;
    c->led = (!is_pc && !is_android);
    c->fullscreen = (is_pc && !is_android) ? 0 : 1;
    c->widescreen = (is_pc || is_android) ? 1 : 0;
    c->touch_pad = is_android ? 1 : 0;
    c->fill = (is_pc || is_android);
    c->scale = (is_pc || is_android) ? 0 : 2;
    c->renderer_pc = 0; /* gpu */
    c->filter = 1;      /* sharp */
    c->backup_max = 10;
    c->update_check = 1;
    c->sync_enabled = 1;
    c->sync_interval_min = 10;
    int h[H_COUNT] = {K_EAST, K_R1, K_NORTH, K_SOUTH, K_L1, K_WEST, K_L2, K_R2};
    if (is_android) {
        /* The touch pad's face buttons send other PS1 buttons than a gamepad's (android/touch_pad.c, main_android.c:
         * circle is its A, triangle its B, cross its Y), so the menu's places are turned to put each action under the
         * touch button at its cell (owner's check on the phone): Fight on B, Tag on A, Digivolve on Y, Run on X. */
        h[H_FIGHT] = K_NORTH;
        h[H_DV] = K_SOUTH;
        h[H_TAG] = K_EAST;
    }
    memcpy(c->hud, h, sizeof(h));
    /* The face rows rotate circle and triangle against the console's own order: back is the right-hand button and
     * the menu the top one, as in any modern game; the letters follow the places, so the text never says triangle. */
    c->pad[A_CROSS].code = K_SOUTH;
    c->pad[A_SQUARE].code = K_NORTH;
    c->pad[A_CIRCLE].code = K_WEST;
    c->pad[A_TRIANGLE].code = K_EAST;
    if (is_android) { /* the PC's layout (the PS1's own places: circle right, triangle top), as on the touch pad */
        c->pad[A_CIRCLE].code = K_EAST;
        c->pad[A_TRIANGLE].code = K_WEST;
    }
    c->pad[A_L1].code = K_L1;
    c->pad[A_R1].code = K_R1;
    c->pad[A_START].code = K_START;
    c->pad[A_SELECT].code = K_SELECT;
    cfg_set_layout(c, LAYOUT_XBOX);
    c->pad[A_L1].letter = 'L';
    c->pad[A_R1].letter = 'R';
    c->pad[A_START].letter = 'S';
    c->pad[A_SELECT].letter = 'E';
}

int cfg_confirm_code(const Cfg *c) { return c->pad[A_CROSS].code; }
int cfg_back_code(const Cfg *c) { return c->pad[A_TRIANGLE].code; }

static unsigned dup_mask(const int *codes, int n) {
    unsigned m = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (codes[i] == codes[j]) {
                m |= 1u << i | 1u << j;
            }
    return m;
}

int dup_actions(const Cfg *c, unsigned *mask_out) {
    int codes[A_COUNT];
    for (int i = 0; i < A_COUNT; i++) codes[i] = c->pad[i].code;
    unsigned m = dup_mask(codes, A_COUNT);
    if (mask_out) *mask_out = m;
    return __builtin_popcount(m);
}

int dup_hud(const Cfg *c, unsigned *mask_out) {
    unsigned m = dup_mask(c->hud, H_COUNT);
    if (mask_out) *mask_out = m;
    return __builtin_popcount(m);
}

/* ---- files ---- */
static void path_of(char *out, size_t n, const char *name) {
    snprintf(out, n, "%s/%s", game_dir, name);
}

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || n > 4 * 1024 * 1024) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)n, f);
    b[r] = 0;
    fclose(f);
    return b;
}

static int write_atomic(const char *path, const char *text) {
    char tmp[800];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path, (long)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    size_t len = strlen(text);
    size_t w = fwrite(text, 1, len, f);
    int bad = (w != len) || (fflush(f) != 0);
    if (!bad) fsync(fileno(f));
    if (fclose(f) != 0) bad = 1;
    if (bad) { unlink(tmp); return -1; }
    if (rename_over(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* The string value of "key" after `from` in a JSON text, copied into out; 0 if absent. Enough for the two values
 * this file's own writer put there; it is not a JSON parser. */
static int json_str(const char *text, const char *from, const char *key, char *out, size_t n) {
    const char *p = text, *s;
    if (from && (s = strstr(text, from))) p = s;
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    s = strstr(p, pat);
    if (!s) return 0;
    s = strchr(s + strlen(pat), ':');
    if (!s) return 0;
    while (*++s == ' ') {}
    if (*s != '"') return 0;
    s++;
    size_t i = 0;
    while (*s && *s != '"' && i < n - 1) out[i++] = *s++;
    out[i] = 0;
    return 1;
}

static int json_enabled(const char *text, const char *mod) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", mod);
    const char *s = strstr(text, pat);
    if (!s) return -1;
    s = strstr(s, "\"enabled\"");
    if (!s) return -1;
    s = strchr(s, ':');
    while (s && *++s == ' ') {}
    return s && !strncmp(s, "true", 4);
}

/* ---- settings.json, read back on PC ----
 * settings.json is the game's file and RecompLauncher edits it in place (recomplauncher.json: the same options as the
 * VIDEO and GAME screens), so on PC its values win over tui.state's for every option it holds. A small reader of
 * object members by path ("video/fullscreen"): enough for the file this launcher writes and the same members
 * rewritten by another program; arrays and numbers other than integers are skipped, never interpreted. */
static const char *j_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static const char *j_skip_str(const char *p) { /* p at the opening quote: past the closing one */
    p++;
    while (*p && *p != '"') p += (*p == '\\' && p[1]) ? 2 : 1;
    return *p ? p + 1 : p;
}

static const char *j_skip_val(const char *p) {
    if (*p == '"') return j_skip_str(p);
    if (*p == '{' || *p == '[') {
        int depth = 0;
        while (*p) {
            if (*p == '"') { p = j_skip_str(p); continue; }
            if (*p == '{' || *p == '[') depth++;
            else if ((*p == '}' || *p == ']') && --depth == 0) return p + 1;
            p++;
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

static const char *j_path(const char *text, const char *path) {
    const char *p = j_ws(text);
    while (*path) {
        if (*p != '{') return NULL;
        const char *slash = strchr(path, '/');
        size_t kl = slash ? (size_t)(slash - path) : strlen(path);
        const char *found = NULL;
        p = j_ws(p + 1);
        while (*p == '"') {
            const char *k = p + 1, *ke = j_skip_str(p) - 1;
            p = j_ws(j_skip_str(p));
            if (*p != ':') return NULL;
            p = j_ws(p + 1);
            if ((size_t)(ke - k) == kl && !strncmp(k, path, kl)) { found = p; break; }
            p = j_ws(j_skip_val(p));
            if (*p == ',') p = j_ws(p + 1);
        }
        if (!found) return NULL;
        p = found;
        path += kl + (slash ? 1 : 0);
    }
    return p;
}

static int j_bool(const char *t, const char *path, int def) {
    const char *v = j_path(t, path);
    if (v && !strncmp(v, "true", 4)) return 1;
    if (v && !strncmp(v, "false", 5)) return 0;
    return def;
}

static int j_int(const char *t, const char *path, int def) {
    const char *v = j_path(t, path);
    return v && (*v == '-' || isdigit((unsigned char)*v)) ? atoi(v) : def;
}

static int j_str(const char *t, const char *path, char *out, size_t n) {
    const char *v = j_path(t, path);
    if (!v || *v != '"' || n == 0) return 0;
    size_t i = 0;
    for (v++; *v && *v != '"' && i + 1 < n; v++) {
        if (*v == '\\' && v[1]) v++;
        out[i++] = *v;
    }
    out[i] = 0;
    return 1;
}

static const char *ff_names[] = {"off", "2x", "4x", "8x", "unlimited"};
static const char *load_names[] = {"off", "2x", "4x", "8x", "instant"};
static const char *lang_names[] = {"english", "portuguese", "french", "german", "italian", "spanish", "usa", "japanese"};
static const char *bbuf_names[] = {"off", "short", "long"};
static const char *btext_names[] = {"off", "skip", "async"};
static const char *log_names[] = {"off", "soft", "heavy"};
static const char *filter_names[] = {"none", "sharp", "scanlines", "crt", "smooth"};

int cfg_auto_scale(const Cfg *c) {
    int dw = 1280, dh = 720;
    plat_desktop_size(&dw, &dh);
    int bw = c->widescreen ? 427 : 320;
    int sx = (dw * 88 / 100) / bw;
    int sy = (dh * 85 / 100) / 240;
    int s = sx < sy ? sx : sy;
    if (s < 2) s = 2;
    if (s > 8) s = 8;
    return s;
}

static int idx_of(const char *const *names, int n, const char *v, int def) {
    for (int i = 0; i < n; i++) if (!strcmp(names[i], v)) return i;
    return def;
}

/* settings.json's values for the launcher's own options (VIDEO, GAME, DEBUG LOGS; recomplauncher.json shows the same
 * ones) over tui.state's (PC): what RecompLauncher changed shows here, and the next save keeps it. Nothing else of
 * settings.json is read: the launcher rewrites the rest. */
static void cfg_read_settings(Cfg *c, const char *j) {
    char s[1200];
    c->fullscreen = j_bool(j, "video/fullscreen", c->fullscreen);
    c->fill = j_bool(j, "video/fill", c->fill);
    if (j_str(j, "video/renderer", s, sizeof(s))) c->renderer_pc = !strcmp(s, "software");
    if (j_str(j, "video/filter", s, sizeof(s))) c->filter = idx_of(filter_names, 5, s, c->filter);
    c->widescreen = j_bool(j, "mods/widescreen/enabled", c->widescreen) && !c->renderer_pc;
    /* the window scale: AUTO stays AUTO while the file holds what AUTO gives on this monitor */
    int sc = j_int(j, "video/scale", 0);
    if (sc >= 1 && !(c->scale == 0 && sc == cfg_auto_scale(c))) c->scale = sc < 2 ? 2 : sc > 8 ? 8 : sc;
    if (!j_bool(j, "mods/fast_forward/enabled", 1)) c->ff = 0;
    else if (j_str(j, "mods/fast_forward/speed", s, sizeof(s))) c->ff = idx_of(ff_names, 5, s, 0) ?: 3; /* on: never "off" */
    if (!j_bool(j, "mods/fast_loading/enabled", 1)) c->load = 0;
    else if (j_str(j, "mods/fast_loading/speed", s, sizeof(s))) c->load = idx_of(load_names, 5, s, 0) ?: 3;
    c->skip = j_bool(j, "mods/skip_dialogues/enabled", c->skip);
    c->battles = j_bool(j, "mods/battle_animations/enabled", c->battles);
    c->hit = c->battles && j_bool(j, "mods/battle_animations/hit_reaction", c->hit);
    if (!j_bool(j, "mods/battle_text/enabled", 1)) c->btext = 0;
    else if (j_str(j, "mods/battle_text/mode", s, sizeof(s))) c->btext = idx_of(btext_names, 3, s, c->btext);
    if (j_bool(j, "mods/preset_language/enabled", 1) && j_str(j, "mods/preset_language/language", s, sizeof(s)))
        c->lang = idx_of(lang_names, 8, s, c->lang);
    if (j_str(j, "debug/logs", s, sizeof(s))) c->logs = idx_of(log_names, 3, s, c->logs);
    /* the disc: RecompLauncher's game_path is disc/path; relative paths are the settings directory's */
    if (j_str(j, "disc/path", s, sizeof(s)) && s[0]) {
        char full[1200];
        int abs = s[0] == '/' || s[0] == '\\' || (s[0] && s[1] == ':');
        if (abs) snprintf(full, sizeof(full), "%s", s);
        else snprintf(full, sizeof(full), "%s/%s", game_dir, s);
        if (access(full, F_OK) == 0) snprintf(disc_path, sizeof(disc_path), "%s", full);
    }
}

void cfg_load(Cfg *c) {
    char path[600], line[1400];
    cfg_defaults(c);
    path_of(path, sizeof(path), "tui.state");
    FILE *f = fopen(path, "r");
    char ffspeed[16] = "";
    int ffon = 1;
    while (f && fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        int n = atoi(v);
        if (!strcmp(line, "hz")) c->hz = n == 60 ? 60 : 50;
        else if (!strcmp(line, "lang")) c->lang = n < 0 ? 0 : n > 7 ? 7 : n;
        else if (!strcmp(line, "ff")) ffon = n;
        else if (!strcmp(line, "ff_speed")) snprintf(ffspeed, sizeof(ffspeed), "%s", v);
        else if (!strcmp(line, "load")) c->load = n < 0 ? 0 : n > 4 ? 4 : n;
        else if (!strcmp(line, "skip")) c->skip = n != 0;
        else if (!strcmp(line, "battles")) c->battles = n != 0;
        else if (!strcmp(line, "hit")) c->hit = n != 0;
        else if (!strcmp(line, "btext")) c->btext = n < 0 ? 0 : n > 2 ? 2 : n;
        else if (!strcmp(line, "bmem")) c->bmem = n != 0;
        else if (!strcmp(line, "bbuf")) c->bbuf = n < 0 ? 0 : n > 2 ? 2 : n;
        else if (!strcmp(line, "logs")) c->logs = n < 0 ? 0 : n > 2 ? 2 : n;
        else if (!strcmp(line, "widescreen")) c->widescreen = n != 0;
        else if (!strcmp(line, "fullscreen")) c->fullscreen = n != 0;
        else if (!strcmp(line, "touch_pad")) c->touch_pad = n != 0;
        else if (!strcmp(line, "fill")) c->fill = n != 0;
        else if (!strcmp(line, "scale")) c->scale = n < 0 ? 0 : n > 8 ? 8 : n;
        else if (!strcmp(line, "renderer_pc")) c->renderer_pc = n != 0;
        else if (!strcmp(line, "filter")) c->filter = n < 0 ? 0 : n > 4 ? 4 : n;
        else if (!strcmp(line, "backup_max")) c->backup_max = n < 1 ? 1 : n > 100 ? 100 : n;
        else if (!strcmp(line, "sync_enabled")) c->sync_enabled = n != 0;
        else if (!strcmp(line, "update_check")) c->update_check = n != 0;
        else if (!strcmp(line, "sync_interval")) {
            static const int ivs[] = {0, 5, 10, 15, 20, 30};
            c->sync_interval_min = 10;
            for (int k = 0; k < 6; k++) if (n == ivs[k]) { c->sync_interval_min = n; break; }
        }
        else if (!strcmp(line, "disc") && !disc_path[0] && v[0] && access(v, F_OK) == 0) {
            snprintf(disc_path, sizeof(disc_path), "%s", v);
        }
        else if (!strncmp(line, "bm_", 3)) {
            static const char *const keys[H_COUNT] = {"bm_fight", "bm_tech", "bm_digivolve", "bm_tag", "bm_item", "bm_run", "bm_inspect", "bm_blast"};
            for (int i = 0; i < H_COUNT; i++)
                if (!strcmp(line, keys[i]) && hud_code(v)) c->hud[i] = hud_code(v);
        }
    }
    if (f) {
        fclose(f);
        c->ff = !ffon ? 0 : ffspeed[0] ? idx_of(ff_names, 5, ffspeed, 3) : 3;
        if (c->ff == 0 && ffon) c->ff = 3;
    }
    path_of(path, sizeof(path), "pad.map");
    f = fopen(path, "r");
    int game_places = 0;
    while (f && fgets(line, sizeof(line), f)) {
        char a[32], p[32], l;
        if (line[0] == '#') { game_places = 1; continue; }
        if (sscanf(line, "%31s %31s %c", a, p, &l) != 3) continue;
        for (int i = 0; i < A_COUNT; i++) {
            if (strcmp(a, action_name[i])) continue;
            int code = game_places ? code_from_game(p) : code_by_name(p);
            if (code && strchr("ABXYLRSE-", l)) {
                c->pad[i].code = code;
                c->pad[i].letter = l;
            }
        }
    }
    if (f) fclose(f);
    if (is_android && c->touch_pad &&
        c->pad[A_CROSS].letter == 'Y' && c->pad[A_SQUARE].letter == 'X' &&
        c->pad[A_CIRCLE].letter == 'A' && c->pad[A_TRIANGLE].letter == 'B') {
        cfg_set_layout(c, LAYOUT_XBOX);
    }
    path_of(path, sizeof(path), "settings.json");
    char *j = slurp(path);
    if (j) {
        int e = json_enabled(j, "led_bar");
        if (e >= 0) c->led = e;
        if (!f) {
            char lstr[32] = "";
            if (json_str(j, "\"preset_language\"", "language", lstr, sizeof(lstr)))
                c->lang = idx_of(lang_names, 8, lstr, 0);
        }
        if (is_pc) cfg_read_settings(c, j);
        free(j);
    }
}

static int write_settings(const Cfg *c) {
    char path[600], renderer[16] = "";
    path_of(path, sizeof(path), "settings.json");
    if (is_pc || is_android) {
        snprintf(renderer, sizeof(renderer), "%s", is_android ? "gpu" : (c->renderer_pc ? "software" : "gpu"));
    } else {
        char *old = slurp(path);
        if (old) {
            json_str(old, "\"video\"", "renderer", renderer, sizeof(renderer));
            free(old);
        }
        if (strcmp(renderer, "gles") && strcmp(renderer, "software") && strcmp(renderer, "gpu")) {
            const char *d = getenv("DW3_DEFAULT_RENDERER");
            snprintf(renderer, sizeof(renderer), "%s", d && (!strcmp(d, "gles") || !strcmp(d, "gpu")) ? d : "software");
        }
    }
    char pads[512] = "{ ";
    for (int i = 0; i < A_COUNT; i++) {
        char one[64];
        snprintf(one, sizeof(one), "%s\"%s\": \"%s\"", i ? ", " : "", action_name[i],
                 is_android ? sdl3_name(c->pad[i].code) : game_name(c->pad[i].code));
        strcat(pads, one);
    }
    strcat(pads, " }");
    char disc_json[1400], disc_esc[2400];
    char gpref[600];
    snprintf(gpref, sizeof(gpref), "%s/game/", game_dir);
    if (!strncmp(disc_path, gpref, strlen(gpref))) {
        snprintf(disc_json, sizeof(disc_json), "game/%s", disc_path + strlen(gpref));
    } else {
        snprintf(disc_json, sizeof(disc_json), "%s", disc_path);
    }
    {
        size_t j = 0;
        for (size_t i = 0; disc_json[i] && j + 2 < sizeof(disc_esc); i++) {
            unsigned char ch = (unsigned char)disc_json[i];
            if (ch == '"' || ch == '\\') { disc_esc[j++] = '\\'; disc_esc[j++] = (char)ch; }
            else if (ch >= 0x20) disc_esc[j++] = (char)ch;
        }
        disc_esc[j] = 0;
    }
    int vscale = is_pc ? (c->scale > 0 ? c->scale : cfg_auto_scale(c)) : 2;
    int vfull = is_pc ? c->fullscreen : 1;
    int vfill = is_android ? 1 : (is_pc && c->fill);
    int vw = is_pc ? (c->widescreen && c->renderer_pc == 0) : (is_android ? 1 : 0);
    const char *vfilt = (is_pc || is_android) ? filter_names[c->filter] : "none";
    char text[6400];
    snprintf(text, sizeof(text),
        "{\n  \"schema\": 1,\n  \"disc\": { \"path\": \"%s\" },\n"
        "  \"video\": { \"window\": true, \"scale\": %d, \"fullscreen\": %s, \"fill\": %s, \"refresh\": %d, \"renderer\": \"%s\", \"filter\": \"%s\" },\n"
        "  \"audio\": { \"mute\": false },\n  \"memcard1\": \"card1.mcd\",\n  \"memcard2\": \"card2.mcd\",\n"
        "  \"watchdog\": 0,\n  \"debug\": { \"logs\": \"%s\" },\n  \"mods\": {\n"
        "    \"fast_forward\": { \"enabled\": %s, \"speed\": \"%s\", \"mute\": true, \"hold\": \"\", \"toggle\": \"\" },\n"
        "    \"fast_loading\": { \"enabled\": %s, \"speed\": \"%s\" },\n"
        "    \"skip_dialogues\": { \"enabled\": %s, \"fast_forward_waits\": false },\n"
        "    \"battle_animations\": { \"enabled\": %s, \"hit_reaction\": %s },\n"
        "    \"battle_text\": { \"enabled\": true, \"mode\": \"%s\" },\n"
        "    \"preset_language\": { \"enabled\": true, \"language\": \"%s\" },\n"
        "    \"widescreen\": { \"enabled\": %s },\n"
        "    \"battle_menu\": { \"enabled\": true, \"fight\": \"%s\", \"tech\": \"%s\", \"digivolve\": \"%s\", \"tag\": \"%s\", "
        "\"item\": \"%s\", \"run\": \"%s\", \"inspect\": \"%s\", \"blast\": \"%s\", \"remember_cursor\": %s, \"buffer_input\": \"%s\" },\n"
        "    \"led_bar\": { \"enabled\": %s }\n  },\n%s  \"input\": { \"gamepad\": %s }\n}\n",
        disc_esc, vscale, vfull ? "true" : "false", vfill ? "true" : "false", c->hz, renderer, vfilt,
        log_names[c->logs], c->ff ? "true" : "false", ff_names[c->ff ? c->ff : 3],
        c->load ? "true" : "false", load_names[c->load ? c->load : 3],
        c->skip ? "true" : "false", c->battles ? "true" : "false", c->battles && c->hit ? "true" : "false",
        btext_names[c->btext], lang_names[c->lang < 0 || c->lang > 7 ? 0 : c->lang], vw ? "true" : "false",
        hud_name(c->hud[H_FIGHT]), hud_name(c->hud[H_TECH]), hud_name(c->hud[H_DV]),
        hud_name(c->hud[H_TAG]), hud_name(c->hud[H_ITEM]), hud_name(c->hud[H_RUN]), hud_name(c->hud[H_INSPECT]), hud_name(c->hud[H_BLAST]),
        c->bmem ? "true" : "false", bbuf_names[c->bbuf], (!is_pc && !is_android && c->led) ? "true" : "false",
        /* The runtime has no touch_pad setting (it warns "unknown key, ignored" on every start): only the Android
         * build, whose launcher reads it back, keeps the key in settings.json. */
        is_android ? (c->touch_pad ? "  \"touch_pad\": true,\n" : "  \"touch_pad\": false,\n") : "", pads);
    return write_atomic(path, text);
}

int cfg_save(const Cfg *c) {
    char path[600], text[3600] = "", line[1400];
    int rc = 0;
    snprintf(text, sizeof(text), "hz=%d\nff=%d\nff_speed=%s\nload=%d\nskip=%d\nbattles=%d\nhit=%d\nbtext=%d\nsaveany=1\nbmenu=1\n"
             "bmem=%d\nbbuf=%d\nwidescreen=%d\nfullscreen=%d\ntouch_pad=%d\nfill=%d\nscale=%d\nrenderer_pc=%d\nfilter=%d\n",
             c->hz, c->ff ? 1 : 0, ff_names[c->ff ? c->ff : 3], c->load, c->skip, c->battles, c->hit,
             c->btext, c->bmem, c->bbuf, c->widescreen, c->fullscreen, c->touch_pad, c->fill, c->scale, c->renderer_pc, c->filter);
    if (disc_path[0]) {
        snprintf(line, sizeof(line), "disc=%s\n", disc_path);
        strcat(text, line);
    }
    static const char *const keys[H_COUNT] = {"bm_fight", "bm_tech", "bm_digivolve", "bm_tag", "bm_item", "bm_run", "bm_inspect", "bm_blast"};
    for (int i = 0; i < H_COUNT; i++) {
        snprintf(line, sizeof(line), "%s=%s\n", keys[i], hud_name(c->hud[i]));
        strcat(text, line);
    }
    snprintf(line, sizeof(line), "logs=%d\nlang=%d\nlang_on=1\nbackup_max=%d\nsync_enabled=%d\nsync_interval=%d\nupdate_check=%d\n",
             c->logs, c->lang, c->backup_max, c->sync_enabled, c->sync_interval_min, c->update_check);
    strcat(text, line);
    path_of(path, sizeof(path), "tui.state");
    rc |= write_atomic(path, text);
    int tp_xbox = is_android && c->touch_pad && cfg_layout(c) == LAYOUT_XBOX;
    static const char tp_xbox_letter[4] = {'Y', 'X', 'A', 'B'};
    char pm[1024] = "# game-places x\n";
    for (int i = 0; i < A_COUNT; i++) {
        char letter = (tp_xbox && i < 4) ? tp_xbox_letter[i] : c->pad[i].letter;
        snprintf(line, sizeof(line), "%s %s %c\n", action_name[i], game_name(c->pad[i].code), letter);
        strcat(pm, line);
    }
    path_of(path, sizeof(path), "pad.map");
    rc |= write_atomic(path, pm);
    if (disc_path[0]) rc |= write_settings(c);
    return rc;
}

void find_disc(void) {
    static const char *names[] = {"dw2003.cue", "dw2003.bin", "dw3.cue", "dw3.bin", "Digimon World 2003.cue",
                                  "Digimon World 2003.bin"};
    static const char *subdirs[] = {"game", ".", "iso"};
    char dir[600], p[1200];
    disc_path[0] = 0;
    for (unsigned d_i = 0; d_i < sizeof(subdirs) / sizeof(subdirs[0]); d_i++) {
        if (!strcmp(subdirs[d_i], ".")) snprintf(dir, sizeof(dir), "%s", game_dir);
        else path_of(dir, sizeof(dir), subdirs[d_i]);
        for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
            snprintf(p, sizeof(p), "%s/%s", dir, names[i]);
            if (access(p, F_OK) == 0) {
                snprintf(disc_path, sizeof(disc_path), "%s", p);
                return;
            }
        }
    }
    path_of(dir, sizeof(dir), "game");
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    char best[512] = "";
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 4 && (!strcasecmp(e->d_name + n - 4, ".cue") || !strcasecmp(e->d_name + n - 4, ".bin")) &&
            (!best[0] || strcmp(e->d_name, best) < 0)) {
            snprintf(best, sizeof(best), "%s", e->d_name);
        }
    }
    closedir(d);
    if (best[0]) snprintf(disc_path, sizeof(disc_path), "%s/%s", dir, best);
}

int browse_disc(void) {
#ifdef _WIN32
    char start[1200] = "", picked[1200];
    browse_start_dir(start, sizeof(start));
    if (w32_pick_disc(start, picked, sizeof(picked)) != 0 || access(picked, F_OK) != 0) return -1;
    snprintf(disc_path, sizeof(disc_path), "%s", picked);
    return 0;
#else
    const char *cmds[] = {
        "zenity --file-selection --title='Select Digimon World 3 (.cue / .bin)' --file-filter='*.cue *.bin' 2>/dev/null",
        "kdialog --getopenfilename . '*.cue *.bin|PS1 Disc Image' 2>/dev/null",
    };
    for (unsigned i = 0; i < 2; i++) {
        FILE *fp = popen(cmds[i], "r");
        if (!fp) continue;
        char buf[1200] = "";
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\r\n")] = 0;
        }
        int rc = pclose(fp);
        if (rc == 0 && buf[0] && access(buf, F_OK) == 0) {
            snprintf(disc_path, sizeof(disc_path), "%s", buf);
            return 0;
        }
    }
    return -1;
#endif
}

static int cmp_browse(const void *a, const void *b) {
    const BrowseItem *x = a, *y = b;
    if (strcmp(x->name, "..") == 0) return -1;
    if (strcmp(y->name, "..") == 0) return 1;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
    return strcasecmp(x->name, y->name);
}

int browse_read(const char *dir, BrowseItem *out, int max_items) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    if (!path_is_root(dir) && n < max_items) {
        snprintf(out[n].name, sizeof(out[n].name), "..");
        out[n].is_dir = 1;
        n++;
    }
    struct dirent *e;
    while ((e = readdir(d)) && n < max_items) {
        if (e->d_name[0] == '.') continue;
        char full[1400];
        snprintf(full, sizeof(full), "%s/%s", strcmp(dir, "/") ? dir : "", e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            snprintf(out[n].name, sizeof(out[n].name), "%s", e->d_name);
            out[n].is_dir = 1;
            n++;
        } else if (S_ISREG(st.st_mode)) {
            size_t len = strlen(e->d_name);
            if (len > 4 && (!strcasecmp(e->d_name + len - 4, ".cue") || !strcasecmp(e->d_name + len - 4, ".bin"))) {
                snprintf(out[n].name, sizeof(out[n].name), "%s", e->d_name);
                out[n].is_dir = 0;
                n++;
            }
        }
    }
    closedir(d);
    if (n > 1) qsort(out, (size_t)n, sizeof(out[0]), cmp_browse);
    return n;
}

void browse_start_dir(char *out, size_t n) {
    if (disc_path[0]) {
        char tmp[1200];
        snprintf(tmp, sizeof(tmp), "%s", disc_path);
        char *sl = path_last_sep(tmp);
        if (sl) {
            if (sl == tmp) strcpy(tmp, "/");
            else if (sl == tmp + 2 && tmp[1] == ':') sl[1] = 0; /* a drive's root keeps its separator: "C:\\" */
            else *sl = 0;
            if (access(tmp, R_OK) == 0 && realpath(tmp, out)) return;
        }
    }
    const char *cands[] = {NULL, getenv("HOME"), getenv("USERPROFILE"), "/sdcard", "."};
    char iso[700];
    snprintf(iso, sizeof(iso), "%s/iso", game_dir);
    cands[0] = access(iso, R_OK) == 0 ? iso : NULL;
    for (unsigned i = 0; i < sizeof(cands) / sizeof(cands[0]); i++) {
        if (cands[i] && access(cands[i], R_OK) == 0 && realpath(cands[i], out)) return;
    }
    snprintf(out, n, ".");
}

/* ---- LED bar (the XF35H handheld's; the home screen never offers it on PC) ---- */
#ifdef _WIN32
int led_state(const Cfg *c) { (void)c; return LED_NONE; }
int led_compatible(void) { return 0; }
int led_install(char *why, size_t n) { snprintf(why, n, "NOT ON THIS SYSTEM"); return -1; }
#else
#define LED_SOCK "/tmp/ledd.sock"

static const char *led_dir(void) {
    const char *d = getenv("DW3_LED_DIR");
    return d ? d : "/storage/.config/ledd";
}

static int led_ask(const char *req) {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return 0;
    struct timeval tv = {0, 400000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_un a = {0};
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof(a.sun_path), "%s", LED_SOCK);
    int ok = 0;
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == 0) {
        char buf[64];
        dprintf(s, "%s\n", req);
        ok = read(s, buf, sizeof(buf)) > 0;
    }
    close(s);
    return ok;
}

int led_state(const Cfg *c) {
    static long seen;
    static int cached;
    long now = plat_now_ms();
    if (now - seen > 1000 || !seen) {
        char p[700];
        snprintf(p, sizeof(p), "%s/ledd.py", led_dir());
        cached = led_ask("status") || access(p, F_OK) == 0;
        seen = now;
    }
    return !cached ? LED_NONE : c->led ? LED_ON : LED_OFF;
}

static int read_dt_lower(const char *path, char *out, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t r = fread(out, 1, n - 1, f);
    fclose(f);
    for (size_t i = 0; i < r; i++) {
        out[i] = out[i] ? (char)tolower((unsigned char)out[i]) : ' ';
    }
    out[r] = 0;
    return r > 0;
}

int led_compatible(void) {
    if (getenv("DW3_LED_FORCE")) return 1;
    if (access("/dev/ttyS2", F_OK) != 0) return 0;
    char buf[512] = "";
    const char *paths[] = {
        "/proc/device-tree/compatible",
        "/sys/firmware/devicetree/base/compatible",
        "/proc/device-tree/model",
        "/sys/firmware/devicetree/base/model",
    };
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if (read_dt_lower(paths[i], buf, sizeof(buf)) && strstr(buf, "rk3326")) {
            return 1;
        }
    }
    return 0;
}

int led_install(char *why, size_t n) {
    char src[700], dst[700], cmd[3200];
    snprintf(src, sizeof(src), "%s/ledd.py", game_dir);
    snprintf(dst, sizeof(dst), "%s/ledd.py", led_dir());
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && cp '%s' '%s.tmp' && mv '%s.tmp' '%s'", led_dir(), src, dst, dst, dst);
    if (system(cmd) != 0) {
        snprintf(why, n, "CANNOT WRITE %s", led_dir());
        return -1;
    }
    snprintf(cmd, sizeof(cmd), "cd '%s' && python3 '%s' --detach </dev/null >/dev/null 2>&1", game_dir, dst);
    if (system(cmd) != 0) {
        snprintf(why, n, "THE DAEMON DID NOT START");
        return -1;
    }
    for (int i = 0; i < 20; i++) {
        if (led_ask("status")) {
            snprintf(cmd, sizeof(cmd), "%s/installed", led_dir());
            FILE *f = fopen(cmd, "w");
            if (f) {
                fprintf(f, "dw3 launcher %ld\n", (long)time(NULL));
                fclose(f);
            }
            return 0;
        }
        usleep(100000);
    }
    snprintf(why, n, "THE DAEMON DID NOT ANSWER");
    return -1;
}
#endif

/* ---- logs ---- */
#ifdef _WIN32
/* Windows has no python3 to write the zip: a stored (uncompressed) one is written here, the same files in it. */
typedef struct {
    char name[200];
    unsigned long crc, size, off;
} ZipEnt;

static unsigned long zip_crc(const unsigned char *b, size_t n) {
    unsigned long crc = 0xffffffffUL;
    for (size_t i = 0; i < n; i++) {
        crc ^= b[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xedb88320UL & (0UL - (crc & 1)));
    }
    return crc ^ 0xffffffffUL;
}

static void zip_u16(FILE *z, unsigned v) { fputc((int)(v & 255), z); fputc((int)((v >> 8) & 255), z); }
static void zip_u32(FILE *z, unsigned long v) { zip_u16(z, (unsigned)(v & 0xffff)); zip_u16(z, (unsigned)(v >> 16)); }

static void zip_put(FILE *z, ZipEnt *e, int *count, int max, const char *arcname, const unsigned char *data, size_t len) {
    if (*count >= max) return;
    ZipEnt *x = &e[(*count)++];
    snprintf(x->name, sizeof(x->name), "%s", arcname);
    x->crc = zip_crc(data, len);
    x->size = (unsigned long)len;
    x->off = (unsigned long)ftell(z);
    size_t nl = strlen(x->name);
    zip_u32(z, 0x04034b50UL); zip_u16(z, 20); zip_u16(z, 0); zip_u16(z, 0); zip_u16(z, 0); zip_u16(z, 0x21);
    zip_u32(z, x->crc); zip_u32(z, x->size); zip_u32(z, x->size); zip_u16(z, (unsigned)nl); zip_u16(z, 0);
    fwrite(x->name, 1, nl, z);
    fwrite(data, 1, len, z);
}

static void zip_file(FILE *z, ZipEnt *e, int *count, int max, const char *arcname, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > 64L * 1024 * 1024) return;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    unsigned char *b = malloc((size_t)st.st_size + 1);
    size_t r = b ? fread(b, 1, (size_t)st.st_size, f) : 0;
    fclose(f);
    if (b) zip_put(z, e, count, max, arcname, b, r);
    free(b);
}

static void zip_dir(FILE *z, ZipEnt *e, int *count, int max, const char *dir, const char *prefix, const char *suffix,
                    const char *arc_dir) {
    DIR *d = opendir(dir);
    struct dirent *de;
    while (d && (de = readdir(d))) {
        size_t l = strlen(de->d_name), sl = strlen(suffix);
        if (de->d_name[0] == '.' || strncmp(de->d_name, prefix, strlen(prefix)) || l < sl || strcmp(de->d_name + l - sl, suffix)) continue;
        char p[1400], arc[400];
        snprintf(p, sizeof(p), "%s/%s", dir, de->d_name);
        snprintf(arc, sizeof(arc), "%s%s", arc_dir, de->d_name);
        zip_file(z, e, count, max, arc, p);
    }
    if (d) closedir(d);
}

int logs_save(char *name, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(name, n, "dw3-logs-%Y%m%d-%H%M%S.zip", &tm);
    char zp[1200], p[1200];
    snprintf(zp, sizeof(zp), "%s/%s", game_dir, name);
    FILE *z = fopen(zp, "wb");
    if (!z) return -1;
    enum { MAXE = 256 };
    ZipEnt *e = calloc(MAXE, sizeof(ZipEnt));
    int count = 0;
    if (!e) { fclose(z); return -1; }
    static const char *const files[] = {"settings.json", "pad.map", "tui.state", "menu.log", "log.txt"};
    for (unsigned i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        path_of(p, sizeof(p), files[i]);
        zip_file(z, e, &count, MAXE, files[i], p);
    }
    zip_dir(z, e, &count, MAXE, game_dir, "crash-", ".txt", "crashes/");
    path_of(p, sizeof(p), "logs");
    zip_dir(z, e, &count, MAXE, p, "", "", "logs/");
    static const char sys[] = "os: windows\n";
    zip_put(z, e, &count, MAXE, "system.txt", (const unsigned char *)sys, sizeof(sys) - 1);
    unsigned long cd = (unsigned long)ftell(z);
    for (int i = 0; i < count; i++) {
        size_t nl = strlen(e[i].name);
        zip_u32(z, 0x02014b50UL); zip_u16(z, 20); zip_u16(z, 20); zip_u16(z, 0); zip_u16(z, 0); zip_u16(z, 0);
        zip_u16(z, 0x21); zip_u32(z, e[i].crc); zip_u32(z, e[i].size); zip_u32(z, e[i].size); zip_u16(z, (unsigned)nl);
        zip_u16(z, 0); zip_u16(z, 0); zip_u16(z, 0); zip_u16(z, 0); zip_u32(z, 0); zip_u32(z, e[i].off);
        fwrite(e[i].name, 1, nl, z);
    }
    unsigned long cd_size = (unsigned long)ftell(z) - cd;
    zip_u32(z, 0x06054b50UL); zip_u16(z, 0); zip_u16(z, 0); zip_u16(z, (unsigned)count); zip_u16(z, (unsigned)count);
    zip_u32(z, cd_size); zip_u32(z, cd); zip_u16(z, 0);
    free(e);
    return fclose(z) == 0 ? 0 : -1;
}
#else
int logs_save(char *name, size_t n) {
    char cmd[2048];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(name, n, "dw3-logs-%Y%m%d-%H%M%S.zip", &tm);
    /* python3 is on every console this runs on (the launcher script needs it too); a zip writer is not worth the code */
    snprintf(cmd, sizeof(cmd),
        "cd '%s' && python3 -c \"import os,sys,glob,zipfile\n"
        "z=zipfile.ZipFile(sys.argv[1],'w',zipfile.ZIP_DEFLATED)\n"
        "for f in ('settings.json','pad.map','tui.state','menu.log','log.txt'):\n"
        "  os.path.isfile(f) and z.write(f)\n"
        "for f in glob.glob('crash-*.txt'): z.write(f,'crashes/'+f)\n"
        "for f in glob.glob('logs/*'):\n"
        "  os.path.isfile(f) and z.write(f)\n"
        "z.writestr('system.txt','uname: '+' '.join(os.uname())+'\\n')\n\" '%s' 2>/dev/null", game_dir, name);
    return system(cmd) == 0 ? 0 : -1;
}
#endif

static int rm_matching(const char *dirpath, const char *prefix, const char *suffix) {
    DIR *d = opendir(dirpath);
    int n = 0;
    struct dirent *e;
    while (d && (e = readdir(d))) {
        size_t l = strlen(e->d_name), sl = strlen(suffix);
        if (strncmp(e->d_name, prefix, strlen(prefix)) || l < sl || strcmp(e->d_name + l - sl, suffix)) continue;
        char p[1200];
        snprintf(p, sizeof(p), "%s/%s", dirpath, e->d_name);
        struct stat st;
        if (stat(p, &st) == 0 && S_ISREG(st.st_mode) && unlink(p) == 0) n++;
    }
    if (d) closedir(d);
    return n;
}

int logs_clean(void) {
    char p[600];
    int n = 0;
    path_of(p, sizeof(p), "logs");
    n += rm_matching(p, "", "");
    n += rm_matching(game_dir, "crash-", ".txt");
    n += rm_matching(game_dir, "dw3-logs-", ".zip");
    path_of(p, sizeof(p), "menu.log");
    n += unlink(p) == 0;
    path_of(p, sizeof(p), "log.txt");
    n += unlink(p) == 0;
    return n;
}
