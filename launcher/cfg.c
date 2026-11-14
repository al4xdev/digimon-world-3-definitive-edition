#include "cfg.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "plat.h"

int is_pc, is_portmaster;
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

/* pad.map names the places in the game's SDL vocabulary, a quarter turn from the kernel's (dw3-menu.py PLACE_TO_GAME). */
static const char *game_name(int code) {
    switch (code) {
    case K_SOUTH: return "west";
    case K_NORTH: return "north";
    case K_WEST: return "east";
    case K_EAST: return "south";
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
    c->btext = 1; /* skip: the definitive HUD has no need of battle messages */
    c->bmem = 1;
    c->bbuf = 1;
    c->led = !is_pc;
    c->fullscreen = !is_pc;
    c->scale = is_pc ? 0 : 2;
    c->renderer_pc = 0; /* gpu */
    c->filter = 1;      /* sharp */
    c->backup_max = 10;
    c->sync_enabled = 1;
    c->sync_interval_min = 10;
    int h[H_COUNT] = {K_EAST, K_L1, K_NORTH, K_SOUTH, K_R1, K_WEST, K_L2};
    memcpy(c->hud, h, sizeof(h));
    /* The face rows rotate circle and triangle against the console's own order: back is the right-hand button and
     * the menu the top one, as in any modern game; the letters follow the places, so the text never says triangle. */
    c->pad[A_CROSS].code = K_SOUTH;
    c->pad[A_SQUARE].code = K_NORTH;
    c->pad[A_CIRCLE].code = K_WEST;
    c->pad[A_TRIANGLE].code = K_EAST;
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
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
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

static const char *ff_names[] = {"off", "2x", "4x", "8x", "unlimited"};
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
        else if (!strcmp(line, "ff")) ffon = n;
        else if (!strcmp(line, "ff_speed")) snprintf(ffspeed, sizeof(ffspeed), "%s", v);
        else if (!strcmp(line, "skip")) c->skip = n != 0;
        else if (!strcmp(line, "battles")) c->battles = n != 0;
        else if (!strcmp(line, "hit")) c->hit = n != 0;
        else if (!strcmp(line, "btext")) c->btext = n < 0 ? 0 : n > 2 ? 2 : n;
        else if (!strcmp(line, "bmem")) c->bmem = n != 0;
        else if (!strcmp(line, "bbuf")) c->bbuf = n < 0 ? 0 : n > 2 ? 2 : n;
        else if (!strcmp(line, "logs")) c->logs = n < 0 ? 0 : n > 2 ? 2 : n;
        else if (!strcmp(line, "widescreen")) c->widescreen = n != 0;
        else if (!strcmp(line, "fullscreen")) c->fullscreen = n != 0;
        else if (!strcmp(line, "scale")) c->scale = n < 0 ? 0 : n > 8 ? 8 : n;
        else if (!strcmp(line, "renderer_pc")) c->renderer_pc = n != 0;
        else if (!strcmp(line, "filter")) c->filter = n < 0 ? 0 : n > 4 ? 4 : n;
        else if (!strcmp(line, "backup_max")) c->backup_max = n < 1 ? 1 : n > 100 ? 100 : n;
        else if (!strcmp(line, "sync_enabled")) c->sync_enabled = n != 0;
        else if (!strcmp(line, "sync_interval")) {
            static const int ivs[] = {0, 5, 10, 15, 20, 30};
            c->sync_interval_min = 10;
            for (int k = 0; k < 6; k++) if (n == ivs[k]) { c->sync_interval_min = n; break; }
        }
        else if (!strcmp(line, "disc") && !disc_path[0] && v[0] && access(v, F_OK) == 0) {
            snprintf(disc_path, sizeof(disc_path), "%s", v);
        }
        else if (!strncmp(line, "bm_", 3)) {
            static const char *const keys[H_COUNT] = {"bm_fight", "bm_tech", "bm_digivolve", "bm_tag", "bm_item", "bm_run", "bm_inspect"};
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
    path_of(path, sizeof(path), "settings.json");
    char *j = slurp(path);
    if (j) {
        int e = json_enabled(j, "led_bar");
        if (e >= 0) c->led = e;
        free(j);
    }
}

static int write_settings(const Cfg *c) {
    char path[600], renderer[16] = "";
    path_of(path, sizeof(path), "settings.json");
    if (is_pc) {
        snprintf(renderer, sizeof(renderer), "%s", c->renderer_pc ? "software" : "gpu");
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
        snprintf(one, sizeof(one), "%s\"%s\": \"%s\"", i ? ", " : "", action_name[i], game_name(c->pad[i].code));
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
    int vw = is_pc ? (c->widescreen && c->renderer_pc == 0) : 0;
    const char *vfilt = is_pc ? filter_names[c->filter] : "none";
    char text[4600];
    snprintf(text, sizeof(text),
        "{\n  \"schema\": 1,\n  \"disc\": { \"path\": \"%s\" },\n"
        "  \"video\": { \"window\": true, \"scale\": %d, \"fullscreen\": %s, \"refresh\": %d, \"renderer\": \"%s\", \"filter\": \"%s\" },\n"
        "  \"audio\": { \"mute\": false },\n  \"memcard1\": \"card1.mcd\",\n  \"memcard2\": \"card2.mcd\",\n"
        "  \"watchdog\": 0,\n  \"debug\": { \"logs\": \"%s\" },\n  \"mods\": {\n"
        "    \"fast_forward\": { \"enabled\": %s, \"speed\": \"%s\", \"mute\": true, \"hold\": \"\", \"toggle\": \"\" },\n"
        "    \"skip_dialogues\": { \"enabled\": %s, \"fast_forward_waits\": false },\n"
        "    \"battle_animations\": { \"enabled\": %s, \"hit_reaction\": %s },\n"
        "    \"battle_text\": { \"enabled\": true, \"mode\": \"%s\" },\n"
        "    \"global_save\": { \"enabled\": true, \"restore_map_state\": true },\n"
        "    \"preset_language\": { \"enabled\": true, \"language\": \"english\" },\n"
        "    \"widescreen\": { \"enabled\": %s },\n"
        "    \"battle_menu\": { \"enabled\": true, \"fight\": \"%s\", \"tech\": \"%s\", \"digivolve\": \"%s\", \"tag\": \"%s\", "
        "\"item\": \"%s\", \"run\": \"%s\", \"inspect\": \"%s\", \"remember_cursor\": %s, \"buffer_input\": \"%s\" },\n"
        "    \"led_bar\": { \"enabled\": %s }\n  },\n  \"input\": { \"gamepad\": %s }\n}\n",
        disc_esc, vscale, vfull ? "true" : "false", c->hz, renderer, vfilt,
        log_names[c->logs], c->ff ? "true" : "false", ff_names[c->ff ? c->ff : 3],
        c->skip ? "true" : "false", c->battles ? "true" : "false", c->battles && c->hit ? "true" : "false",
        btext_names[c->btext], vw ? "true" : "false",
        hud_name(c->hud[H_FIGHT]), hud_name(c->hud[H_TECH]), hud_name(c->hud[H_DV]),
        hud_name(c->hud[H_TAG]), hud_name(c->hud[H_ITEM]), hud_name(c->hud[H_RUN]), hud_name(c->hud[H_INSPECT]),
        c->bmem ? "true" : "false", bbuf_names[c->bbuf], (!is_pc && c->led) ? "true" : "false", pads);
    return write_atomic(path, text);
}

int cfg_save(const Cfg *c) {
    char path[600], text[3600] = "", line[1400];
    int rc = 0;
    snprintf(text, sizeof(text), "hz=%d\nff=%d\nff_speed=%s\nskip=%d\nbattles=%d\nhit=%d\nbtext=%d\nsaveany=1\nbmenu=1\n"
             "bmem=%d\nbbuf=%d\nwidescreen=%d\nfullscreen=%d\nscale=%d\nrenderer_pc=%d\nfilter=%d\n",
             c->hz, c->ff ? 1 : 0, ff_names[c->ff ? c->ff : 3], c->skip, c->battles, c->hit,
             c->btext, c->bmem, c->bbuf, c->widescreen, c->fullscreen, c->scale, c->renderer_pc, c->filter);
    if (disc_path[0]) {
        snprintf(line, sizeof(line), "disc=%s\n", disc_path);
        strcat(text, line);
    }
    static const char *const keys[H_COUNT] = {"bm_fight", "bm_tech", "bm_digivolve", "bm_tag", "bm_item", "bm_run", "bm_inspect"};
    for (int i = 0; i < H_COUNT; i++) {
        snprintf(line, sizeof(line), "%s=%s\n", keys[i], hud_name(c->hud[i]));
        strcat(text, line);
    }
    snprintf(line, sizeof(line), "logs=%d\nlang=0\nlang_on=0\nbackup_max=%d\nsync_enabled=%d\nsync_interval=%d\n",
             c->logs, c->backup_max, c->sync_enabled, c->sync_interval_min);
    strcat(text, line);
    path_of(path, sizeof(path), "tui.state");
    rc |= write_atomic(path, text);
    char pm[1024] = "# game-places x\n";
    for (int i = 0; i < A_COUNT; i++) {
        snprintf(line, sizeof(line), "%s %s %c\n", action_name[i], game_name(c->pad[i].code), c->pad[i].letter);
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
    if (strcmp(dir, "/") != 0 && n < max_items) {
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
        char *sl = strrchr(tmp, '/');
        if (sl) {
            if (sl == tmp) strcpy(tmp, "/");
            else *sl = 0;
            if (access(tmp, R_OK) == 0 && realpath(tmp, out)) return;
        }
    }
    const char *cands[] = {NULL, getenv("HOME"), "/sdcard", "."};
    char iso[700];
    snprintf(iso, sizeof(iso), "%s/iso", game_dir);
    cands[0] = access(iso, R_OK) == 0 ? iso : NULL;
    for (unsigned i = 0; i < 4; i++) {
        if (cands[i] && access(cands[i], R_OK) == 0 && realpath(cands[i], out)) return;
    }
    snprintf(out, n, ".");
}

/* ---- LED bar ---- */
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

int led_compatible(void) {
    if (getenv("DW3_LED_FORCE")) return 1;
    char model[256] = "";
    FILE *f = fopen("/proc/device-tree/model", "r");
    if (!f) f = fopen("/sys/firmware/devicetree/base/model", "r");
    if (f) {
        size_t n = fread(model, 1, sizeof(model) - 1, f);
        model[n] = 0;
        fclose(f);
    }
    for (char *p = model; *p; p++) *p = (char)tolower((unsigned char)*p);
    return strstr(model, "rk3326") && access("/dev/ttyS2", F_OK) == 0;
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

/* ---- logs ---- */
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
