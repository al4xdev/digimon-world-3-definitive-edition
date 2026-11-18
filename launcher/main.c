/* dw3-launcher: the console-first menu. One canvas, one pad, no widgets: every screen is a short list. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "cfg.h"
#include "cloud.h"
#include "loading.h"
#include "plat.h"
#include "sha256.h"
#include "ui.h"

enum {
    S_SPLASH, S_HOME, S_GAME, S_VIDEO, S_HUD, S_CONTROLS, S_SAVES, S_BACKUPS, S_BACKUP_CONFIRM,
    S_CLOUD, S_PAIR_HOST, S_PAIR_ENTER, S_DEVICES, S_REVOKE_CONFIRM, S_CONFLICT, S_CONFLICT_CONFIRM,
    S_DEBUG, S_BROWSE, S_LED, S_LED1, S_LED2, S_RESET, S_CLEAN, S_LOADING, S_ERROR
};
enum { HI_PLAY, HI_GAME, HI_VIDEO, HI_HUD, HI_CONTROLS, HI_SAVES, HI_LED, HI_DEBUG, HI_RESET, HI_COUNT };

static Cfg c;
static CloudState cloud;
static int screen = S_SPLASH, sel, capture, quit_flag;
static char msg[160], last_err[160], status[160], exe_dir[512] = ".";
static int launch_pid = -1, launch_fd = -1, status_fd = -1, launch_pct, launch_started;
static long launch_t0, splash_t0 = -1;
static FILE *game_log;
static char launch_stage[64] = "CHECKING THE DISC";

/* Saves & Cloud UI state */
#define LIST_VIS 7
static SaveBackup bk_items[BACKUP_MAX_LIST];
static int bk_count, bk_scroll;
static CloudDevice dev_items[CLOUD_MAX_DEVICES];
static int dev_count, dev_scroll;
static char pair_code[8];
static int pair_expires_at;
static PairStatus pair_st;
static PairClaim pair_claim;
static int pair_digits[6], pair_cursor, pair_waiting, pair_done;
static long pair_last_poll_ms;
static char pair_done_msg[96];
static char conflict_msg[160], conflict_cloud_info[96], conflict_local_info[96];
static int conflict_return_screen = S_SAVES, conflict_choice, conflict_step;
static float confirm_pos = 0.0f;

/* Built-in D-Pad file browser state (PC / Android / non-PortMaster) */
#define BROWSE_MAX 256
#define BROWSE_VIS 8
static BrowseItem b_items[BROWSE_MAX];
static int b_count, b_scroll;
static char b_dir[1200];

/* Smooth cursor glide + Xbox-style zoom transition state */
static float cur_y = -1.0f;
static int cur_screen = -1;
static uint32_t *trans_from, *trans_to;
static int trans_active, trans_dir, trans_cy = H / 2;
static long trans_t0;
#define SPLASH_MS 1050
#define TRANS_MS 210

static const char *ff_label[] = {"OFF", "2X", "4X", "8X", "UNLIMITED"};
static const char *load_label[] = {"OFF", "2X", "4X", "8X", "INSTANT"};
static const char *lang_label[] = {"ENGLISH", "PORTUGUÊS (BR)", "FRANÇAIS", "DEUTSCH", "ITALIANO", "ESPAÑOL", "ENGLISH (USA)", "JAPANESE"};
static const char *buf_label[] = {"OFF", "SHORT", "LONG"};
static const char *bt_label[] = {"OFF", "SKIP", "ASYNC"};
static const char *log_label[] = {"OFF", "SOFT", "HEAVY"};
static const char *lay_label[] = {"PS1", "XBOX", "NINTENDO", "CUSTOM"};
static const char *filt_label[] = {"NONE", "SHARP", "SCANLINES", "CRT", "SMOOTH"};
static const char *act_label[A_COUNT] = {"CONFIRM", "CARD", "MENU", "BACK", "L1", "R1", "START", "SELECT"};
static const char *hud_label[H_COUNT] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "ITEM", "RUN", "INSPECT", "BLAST"};
/* On-screen order of the pad rows follows the pad: confirm, back, menu, card first. */
static const int act_order[A_COUNT] = {A_CROSS, A_TRIANGLE, A_CIRCLE, A_SQUARE, A_L1, A_R1, A_START, A_SELECT};

static int home_items[HI_COUNT];
static int home_n;

static void build_home_list(void) {
    home_n = 0;
    home_items[home_n++] = HI_PLAY;
    home_items[home_n++] = HI_GAME;
    if (is_pc) home_items[home_n++] = HI_VIDEO;
    home_items[home_n++] = HI_HUD;
    home_items[home_n++] = HI_CONTROLS;
    home_items[home_n++] = HI_SAVES;
    if (!is_pc) home_items[home_n++] = HI_LED;
    home_items[home_n++] = HI_DEBUG;
    home_items[home_n++] = HI_RESET;
}

static int home_step(void) {
    return home_n > 7 ? 38 : 44;
}

static int game_n(void) { return is_portmaster ? 7 : 8; }
static int game_step(void) { return is_portmaster ? 36 : 30; }

static int home_index(int id) {
    for (int i = 0; i < home_n; i++) {
        if (home_items[i] == id) return i;
    }
    return 0;
}

static void glyph(int code, char *out) {
    int lay = cfg_layout(&c);
    if (lay == LAYOUT_PS1) {
        switch (code) {
        case K_SOUTH: strcpy(out, "X"); return;
        case K_EAST: strcpy(out, "O"); return;
        case K_NORTH: strcpy(out, "SQ"); return;
        case K_WEST: strcpy(out, "TRI"); return;
        }
    }
    char l = layout_letter(lay == LAYOUT_PS1 || lay == LAYOUT_CUSTOM ? LAYOUT_XBOX : lay, code);
    if (l != '-') { out[0] = l; out[1] = 0; return; }
    switch (code) {
    case K_L1: strcpy(out, "L1"); break;
    case K_R1: strcpy(out, "R1"); break;
    case K_L2: strcpy(out, "L2"); break;
    case K_R2: strcpy(out, "R2"); break;
    case K_START: strcpy(out, "START"); break;
    case K_SELECT: strcpy(out, "SELECT"); break;
    default: strcpy(out, "?");
    }
}

static void header(const char *a, const char *b) {
    background((int)(plat_now_ms() & 0x7fffffff));
    rect(0, 0, W, 64, C_BG0, 150);
    rect(0, 64, W, 2, C_CYAN, 120);
    ring(40, 32, 18, 3, C_CYAN);
    circle(40, 32, 8, C_GREEN, 256);
    text(72, 14, a, 24, C_TEXT);
    if (b) text(72 + text_w(a, 24) + 10, 20, b, 18, C_CYAN);
}

static int btn_ok(void) { return cfg_confirm_code(&c); }
static int btn_back(void) { return cfg_back_code(&c); }
static int btn_start(void) { return c.pad[A_START].code; }
static int btn_select(void) { return c.pad[A_SELECT].code; }

static void footer(const char *const *keys, const char *const *what, int n) {
    rect(0, H - 52, W, 52, C_BG0, 190);
    int x = 24;
    for (int i = 0; i < n; i++) {
        x += keycap(x, H - 26, keys[i], C_CYAN) + 8;
        x = text(x, H - 36, what[i], 16, C_TEXT) + 22;
    }
}
static void foot_std(const char *a) {
    char ga[16], gb[16], gst[16];
    glyph(btn_ok(), ga); glyph(btn_back(), gb); glyph(btn_start(), gst);
    const char *k[] = {ga, gb, gst}, *w[] = {a, "BACK", "PLAY"};
    footer(k, w, 3);
}
static void foot_2(const char *la, const char *lb) {
    char ga[16], gb[16];
    glyph(btn_ok(), ga); glyph(btn_back(), gb);
    const char *k[] = {ga, gb}, *w[] = {la, lb};
    footer(k, w, 2);
}
static void foot_back(const char *lb) {
    char gb[16];
    glyph(btn_back(), gb);
    const char *k[] = {gb}, *w[] = {lb};
    footer(k, w, 1);
}

static int row_target_y(int s, int idx) {
    switch (s) {
    case S_HOME: return 92 + idx * home_step() - 4;
    case S_GAME: return 90 + idx * game_step() - 4;
    case S_VIDEO: return 90 + idx * 42 - 4;
    case S_DEBUG: return 92 + idx * 48 - 4;
    case S_SAVES: return 86 + idx * 46 - 4;
    case S_CLOUD: return 206 + idx * 44 - 4;
    case S_CONFLICT: return 194 + idx * 52 - 4;
    case S_BACKUPS: return 114 + (idx - bk_scroll) * 38 - 4;
    case S_DEVICES: return 114 + (idx - dev_scroll) * 38 - 4;
    case S_BROWSE: return 114 + (idx - b_scroll) * 36 - 4;
    case S_HUD: return 180 + idx * 25 - 3;
    case S_CONTROLS: return idx == 0 ? 74 : 128 + (idx - 1) * 36 - 4;
    }
    return -1;
}

static void draw_focus(int x, int y, int w, int h, int r, int bad) {
    long t = plat_now_ms();
    int wave = (int)(t % 1200);
    int p = wave < 600 ? wave : 1200 - wave; /* 0..600 triangle wave */
    uint32_t bar = bad ? C_RED : mix(C_GREEN, C_CYAN, p * 120 / 600);
    rrect(x, y, w, h, r, bad ? RGB(70, 22, 28) : C_FOCUS, 256);
    rect(x, y + 6, 4, h - 12, bar, 256);
}

static void row(int y, const char *label, const char *val, int selected, int bad, int on, int draw_bg) {
    if (selected && draw_bg) {
        draw_focus(40, y - 4, W - 80, 36, 8, bad);
    }
    text(64, y + 2, label, 20, bad ? C_RED : selected ? C_GOLD : C_TEXT);
    if (val) text_r(W - 64, y + 2, val, 20, bad ? C_RED : on ? C_GREEN : selected ? C_GOLD : C_NOTE);
}

static void modal(const char *title, const char *l1, const char *l2, const char *l3, const char *a, const char *b, uint32_t accent) {
    rect(0, 0, W, H, C_BG0, 190);
    rrect(80, 120, W - 160, 230, 14, C_PANEL, 256);
    rrect_line(80, 120, W - 160, 230, 14, accent);
    circle(120, 158, 14, accent, 256);
    text_c(120, 144, "!", 24, C_BG0);
    text(148, 142, title, 24, C_TEXT);
    text(112, 192, l1, 17, C_NOTE);
    text(112, 216, l2, 17, C_NOTE);
    text(112, 240, l3, 17, C_NOTE);
    char ga[16], gb[16];
    glyph(btn_ok(), ga); glyph(btn_back(), gb);
    int x = 112;
    x += keycap(x, 318, ga, C_GREEN) + 8;
    x = text(x, 308, a, 18, C_TEXT) + 36;
    x += keycap(x, 318, gb, C_RED) + 8;
    text(x, 308, b, 18, C_TEXT);
}

/* ---- screens ---- */
static const char *home_name[HI_COUNT] = {
    "PLAY", "GAME", "VIDEO", "BATTLE HUD", "CONTROLS", "SAVES & CLOUD", "LED BAR", "DEBUG", "RESET SETTINGS"
};
static const char *home_hint[HI_COUNT] = {
    "Start Digimon World 3 with your saved settings.", "Language, fast-forward, faster loading, skipping and battle text.",
    "Widescreen battles, monitor scale, fullscreen and filter.",
    "Where each battle action sits on your pad.", "Your buttons, and the letters the game shows.",
    "Cloud cross-play sync, 1-100 local save rotation and backup restore.",
    "The handheld's LED bar follows your Digimon's HP.", "Logs for the people who help you fix a crash.",
    "Put every setting back to the tuned defaults."};

static const char *home_value(int id, int *on) {
    static char sv[32];
    *on = 0;
    switch (id) {
    case HI_GAME: return "TUNING";
    case HI_VIDEO: *on = c.widescreen; return c.widescreen ? "16:9 ON" : "SETTINGS";
    case HI_HUD: return "CUSTOMIZE";
    case HI_CONTROLS: return "REMAP";
    case HI_SAVES:
        if (cloud.revoked) return "REVOKED";
        if (cloud.registered && c.sync_enabled) { *on = 1; return "CLOUD ON"; }
        snprintf(sv, sizeof(sv), "LOCAL x%d", c.backup_max);
        return sv;
    case HI_LED: { int s = led_state(&c); *on = s == LED_ON; return s == LED_NONE ? "INSTALL" : s == LED_ON ? "ON" : "OFF"; }
    case HI_DEBUG: return "SETTINGS";
    case HI_RESET: return "ASK";
    }
    return NULL;
}

static void draw_splash(void) {
    if (splash_t0 < 0) splash_t0 = plat_now_ms();
    long dt = plat_now_ms() - splash_t0;
    int p = dt >= SPLASH_MS ? 256 : (int)(dt * 256 / SPLASH_MS);
    /* ease-out cubic approx: 256 - (256-p)^2/256 */
    int inv = 256 - p;
    int ease = 256 - (inv * inv >> 8);

    background((int)(plat_now_ms() & 0x7fffffff));
    int cx = W / 2, cy = H / 2 - 24;
    /* Expanding Digivice / Xbox crest rings */
    int r1 = 16 + (ease * 48 >> 8);
    int r2 = 8 + (ease * 84 >> 8);
    if (p < 220) ring(cx, cy, r2, 2, mix(C_BG0, C_CYAN, (220 - p) * 180 / 220));
    ring(cx, cy, r1, 4, mix(C_BG0, C_CYAN, ease));
    circle(cx, cy, 8 + (ease * 12 >> 8), mix(C_BG0, C_GREEN, ease), 256);

    /* Horizontal energy sweep across the title band */
    int bar_w = ease * 420 >> 8;
    rect(cx - bar_w / 2, cy + 78, bar_w, 2, C_CYAN, 200);
    if (p > 40) {
        int ta = p > 140 ? 256 : (p - 40) * 256 / 100;
        text_c(cx, cy + 92, "DIGIMON WORLD 3", 30, mix(C_BG0, C_TEXT, ta));
        text_c(cx, cy + 128, "DEFINITIVE EDITION", 18, mix(C_BG0, C_CYAN, ta));
    }
    if (p > 120) {
        text_c(cx, H - 38, "DIGITAL LAUNCHER", 13, mix(C_BG0, C_NOTE, (p - 120) * 200 / 136));
    }
}

static void draw_home(void) {
    header("DIGIMON WORLD 3", "DEFINITIVE EDITION");
    int step = home_step();
    int gy = (int)(cur_y >= 0 && cur_screen == S_HOME ? cur_y : row_target_y(S_HOME, sel));
    draw_focus(40, gy, W - 80, step - 4, 8, 0);
    for (int i = 0; i < home_n; i++) {
        int id = home_items[i], on, y = 92 + i * step;
        if ((unsigned)id >= HI_COUNT) continue;
        const char *v = home_value(id, &on);
        int ix = 74, iy = y + (step - 16) / 2;
        uint32_t ic = i == sel ? C_GOLD : C_CYAN;
        switch (id) {
        case HI_PLAY: tri(ix - 7, iy - 9, ix - 7, iy + 9, ix + 9, iy, ic); break;
        case HI_GAME: ring(ix, iy, 9, 2, ic); circle(ix, iy, 3, ic, 256); break;
        case HI_VIDEO: rect(ix - 11, iy - 7, 22, 14, ic, 256); rect(ix - 8, iy - 4, 16, 8, C_BG0, 256); break;
        case HI_HUD: ring(ix, iy, 9, 2, ic); line(ix - 12, iy, ix + 12, iy, ic); line(ix, iy - 12, ix, iy + 12, ic); break;
        case HI_CONTROLS: rect(ix - 10, iy - 3, 20, 6, ic, 256); rect(ix - 3, iy - 10, 6, 20, ic, 256); break;
        case HI_SAVES:
            rrect(ix - 10, iy - 9, 20, 18, 3, ic, 256);
            rect(ix - 6, iy - 7, 12, 6, C_BG0, 256);
            rect(ix - 5, iy + 2, 10, 5, C_BG0, 256);
            break;
        case HI_LED: for (int k = 0; k < 4; k++) rect(ix - 10 + k * 6, iy - 3 - (k % 2) * 5, 4, 10 + (k % 2) * 6, ic, 256); break;
        case HI_DEBUG: tri(ix, iy - 10, ix - 10, iy + 8, ix + 10, iy + 8, ic); break;
        case HI_RESET: ring(ix, iy, 9, 2, ic); tri(ix + 2, iy - 12, ix + 2, iy - 4, ix + 10, iy - 8, ic); break;
        }
        text(104, y + 3, home_name[id], id == HI_PLAY ? 24 : 20, i == sel ? C_GOLD : C_TEXT);
        if (v) text_r(W - 64, y + 5, v, 18, on ? C_GREEN : i == sel ? C_GOLD : C_NOTE);
    }
    rect(40, 408, W - 80, 1, C_EDGE, 256);
    text(44, 412, home_hint[home_items[sel]], 16, C_NOTE);
    text_r(W - 44, 394, disc_path[0] ? "DISC READY" : (is_portmaster ? "NO DISC IN game/" : "SELECT DISC IN GAME"), 14,
           disc_path[0] ? C_GREEN : C_RED);
    char ga[16], gst[16], gsel[16], chord[36];
    glyph(btn_ok(), ga); glyph(btn_start(), gst); glyph(btn_select(), gsel);
    snprintf(chord, sizeof(chord), "%s+%s", gsel, gst);
    const char *k[] = {ga, gst, chord}, *w[] = {"SELECT", "PLAY", "QUIT"};
    footer(k, w, 3);
}

#define VIDEO_N 6
static void draw_video(void) {
    header("DIGIMON WORLD 3", "VIDEO");
    int dw = 1280, dh = 720;
    plat_desktop_size(&dw, &dh);
    int sc = c.scale > 0 ? c.scale : cfg_auto_scale(&c);
    int gw = (c.widescreen && c.renderer_pc == 0 ? 427 : 320) * sc, gh = 240 * sc;
    char sc_val[48], mon[48];
    if (c.scale == 0) snprintf(sc_val, sizeof(sc_val), "AUTO (%dx%d)", gw, gh);
    else snprintf(sc_val, sizeof(sc_val), "%dX (%dx%d)", c.scale, gw, gh);
    const char *v[VIDEO_N] = {
        (c.widescreen && c.renderer_pc == 0) ? "16:9 ON" : "OFF",
        c.fullscreen ? "ON" : "OFF",
        c.fill ? "FILL" : "INTEGER",
        sc_val,
        c.renderer_pc ? "SOFTWARE" : "GPU",
        c.renderer_pc ? "-" : filt_label[c.filter],
    };
    const char *lab[VIDEO_N] = {"WIDESCREEN", "FULLSCREEN", "SCALING", "WINDOW SCALE", "RENDERER", "FILTER"};
    int gy = (int)(cur_y >= 0 && cur_screen == S_VIDEO ? cur_y : row_target_y(S_VIDEO, sel));
    draw_focus(40, gy, W - 80, 36, 8, 0);
    for (int i = 0; i < VIDEO_N; i++) {
        row(90 + i * 42, lab[i], v[i], i == sel, 0, strcmp(v[i], "OFF") && strcmp(v[i], "-"), 0);
    }
    static const char *desc[VIDEO_N] = {
        "Battles and field fill 16:9 (auto-selects GPU). Menus stay 4:3.",
        "Open the game in fullscreen (F11 also toggles).",
        "FILL stretches cleanly to screen height; INTEGER uses exact multiples.",
        "Window resolution scale (AUTO fits the detected monitor).",
        "GPU enables widescreen and shaders; SOFTWARE is exact PS1.",
        "GPU screen filter (SHARP keeps crisp pixels)."
    };
    rect(40, 346, W - 80, 1, C_EDGE, 256);
    text(44, 358, desc[sel], 15, C_NOTE);
    snprintf(mon, sizeof(mon), "MONITOR %dx%d", dw, dh);
    text_r(W - 44, 386, mon, 14, C_CYAN);
    foot_std("CHANGE");
}

static void draw_game(void) {
    header("DIGIMON WORLD 3", "GAME");
    const char *disc_lbl = "BROWSE...";
    if (disc_path[0]) {
        const char *b = strrchr(disc_path, '/');
        disc_lbl = b ? b + 1 : disc_path;
    }
    const char *v[8] = {lang_label[c.lang < 0 || c.lang > 7 ? 0 : c.lang], ff_label[c.ff], load_label[c.load],
                        c.skip ? "ON" : "OFF", c.battles ? "ON" : "OFF", c.battles ? (c.hit ? "ON" : "OFF") : "-",
                        bt_label[c.btext], disc_lbl};
    const char *lab[8] = {"LANGUAGE", "FAST-FORWARD", "FAST LOAD", "SKIP DIALOGUES", "SKIP BATTLE ANIMS",
                          "   HIT REACTION", "BATTLE TEXT", "DISC IMAGE"};
    int n = game_n(), st = game_step();
    int gy = (int)(cur_y >= 0 && cur_screen == S_GAME ? cur_y : row_target_y(S_GAME, sel));
    draw_focus(40, gy, W - 80, 34, 8, 0);
    for (int i = 0; i < n; i++) {
        int on = (i == 7) ? (disc_path[0] != 0) : (strcmp(v[i], "OFF") && strcmp(v[i], "-"));
        row(90 + i * st, lab[i], v[i], i == sel, 0, on, 0);
    }
    static const char *desc[8] = {"Game text language (English, Portuguese BR, French, German, Italian, Spanish).",
        "How fast the fast-forward button runs the game.",
        "Reads the game's files faster. Movies keep normal speed. OFF, 2X, 4X, 8X or INSTANT.",
        "Dialogue boxes close on their own.",
        "Battle moves play without their animation.", "Keep the hit flinch while animations are skipped.",
        "SKIP closes battle message boxes so combat flows.", "Pick or change your .cue / .bin disc file."};
    rect(40, 346, W - 80, 1, C_EDGE, 256);
    text(44, 358, desc[sel], 16, C_NOTE);
    foot_std("CHANGE");
}

static void dup_note(int bad) {
    if (bad) {
        rrect(40, H - 80, W - 80, 26, 8, RGB(70, 22, 28), 256);
        text(56, H - 76, "SAME BUTTON ON TWO ROWS - FIX IT BEFORE LEAVING", 16, C_RED);
    }
}

static void draw_hud(void) {
    header("DIGIMON WORLD 3", "BATTLE HUD");
    unsigned dm = 0;
    int bad = dup_hud(&c, &dm);
    int cx = W / 2, cy = 116;
    static const struct { int code; int dx, dy; } pos[] = {
        {K_WEST, 0, -30}, {K_NORTH, -80, 0}, {K_EAST, 80, 0}, {K_SOUTH, 0, 30},
        {K_L1, -210, -30}, {K_R1, 210, -30}, {K_L2, -210, 20}, {K_R2, 210, 20},
    };
    rrect(50, 70, W - 100, 102, 12, C_PANEL, 220);
    for (int p = 0; p < 8; p++) {
        char g[8], names[64] = "";
        glyph(pos[p].code, g);
        int n = 0, x = cx + pos[p].dx, y = cy + pos[p].dy;
        for (int a = 0; a < H_COUNT; a++)
            if (c.hud[a] == pos[p].code) { if (n++) strcat(names, "/"); strcat(names, hud_label[a]); }
        uint32_t col = n > 1 ? C_RED : n ? C_GOLD : C_OFF;
        keycap(x - 12, y, g, n > 1 ? C_RED : n ? C_CYAN : C_OFF);
        if (n) {
            if (pos[p].dx > 0) text(x + 20, y - 8, names, 13, col);
            else text_r(x - 16, y - 8, names, 13, col);
        }
    }
    const char *rl[H_COUNT + 2] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "ITEM", "RUN", "INSPECT", "BLAST", "REMEMBER CURSOR", "BUFFER INPUT"};
    int gy = (int)(cur_y >= 0 && cur_screen == S_HUD ? cur_y : row_target_y(S_HUD, sel));
    int sbd = sel < H_COUNT && (dm >> sel & 1);
    draw_focus(40, gy, W - 80, 25, 6, sbd);
    for (int i = 0; i < H_COUNT + 2; i++) {
        char g[16], v[48];
        if (i < H_COUNT) {
            glyph(c.hud[i], g);
            if (capture && i == sel) snprintf(v, sizeof(v), "PRESS A BUTTON...");
            else snprintf(v, sizeof(v), "%s  [%s]", place_display(c.hud[i]), g);
        }
        else if (i == H_COUNT) snprintf(v, sizeof(v), "%s", c.bmem ? "ON" : "OFF");
        else snprintf(v, sizeof(v), "%s", buf_label[c.bbuf]);
        int y = 180 + i * 25, bd = i < H_COUNT && (dm >> i & 1);
        text(64, y, rl[i], 17, bd ? C_RED : i == sel ? C_GOLD : C_TEXT);
        text_r(W - 64, y, v, 17, bd ? C_RED : i == sel ? C_GOLD : C_NOTE);
    }
    dup_note(bad);
    char gst[16], gsel[16], gb[16];
    glyph(btn_start(), gst); glyph(btn_select(), gsel); glyph(btn_back(), gb);
    const char *k[] = {gst, "<>", gsel, gb}, *w[] = {"REBIND", "CYCLE", "RESET", "SAVE"};
    footer(k, w, 4);
}

static void draw_controls(void) {
    header("DIGIMON WORLD 3", "CONTROLS");
    unsigned dm = 0;
    int bad = dup_actions(&c, &dm);
    char v[48];
    int gy = (int)(cur_y >= 0 && cur_screen == S_CONTROLS ? cur_y : row_target_y(S_CONTROLS, sel));
    int sbd = sel > 0 && (dm >> act_order[sel - 1] & 1);
    draw_focus(40, gy, W - 80, 36, 8, sbd);
    snprintf(v, sizeof(v), "< %s >", lay_label[cfg_layout(&c)]);
    row(78, "BUTTON GLYPHS", v, sel == 0, 0, 0, 0);
    rect(60, 120, W - 120, 1, C_EDGE, 256);
    for (int i = 0; i < A_COUNT; i++) {
        int a = act_order[i], y = 128 + i * 36, isel = sel == i + 1, bd = dm >> a & 1;
        char g[16];
        glyph(c.pad[a].code, g);
        if (capture && isel) snprintf(v, sizeof(v), "PRESS A BUTTON...");
        else snprintf(v, sizeof(v), "%s  [%s]", place_display(c.pad[a].code), g);
        row(y, act_label[a], v, isel, bd, 0, 0);
    }
    dup_note(bad);
    char gst[16], gsel[16], gb[16];
    glyph(btn_start(), gst); glyph(btn_select(), gsel); glyph(btn_back(), gb);
    const char *k[] = {gst, "<>", gsel, gb}, *w[] = {"REBIND", "GLYPHS", "RESET", "SAVE"};
    footer(k, w, 4);
}

static void draw_saves(void) {
    header("DIGIMON WORLD 3", "SAVES & CLOUD");
    int gy = (int)(cur_y >= 0 && cur_screen == S_SAVES ? cur_y : row_target_y(S_SAVES, sel));
    draw_focus(40, gy, W - 80, 36, 8, 0);

    const char *sync_val = cloud.revoked ? "REVOKED" : !cloud.registered ? "REGISTER FIRST" : c.sync_enabled ? "ON" : "OFF";
    char iv_val[24], bk_val[24], acc_val[32];
    if (c.sync_interval_min == 0) snprintf(iv_val, sizeof(iv_val), "START / EXIT");
    else snprintf(iv_val, sizeof(iv_val), "EVERY %d MIN", c.sync_interval_min);

    int bk_n = save_backup_list(game_dir, bk_items, BACKUP_MAX_LIST);
    snprintf(bk_val, sizeof(bk_val), "%d SAVED", bk_n);
    if (cloud.revoked) snprintf(acc_val, sizeof(acc_val), "REVOKED");
    else if (cloud.registered) snprintf(acc_val, sizeof(acc_val), "REV #%d", cloud.cloud_seq > 0 ? cloud.cloud_seq : cloud.last_seq);
    else snprintf(acc_val, sizeof(acc_val), "NOT LINKED");

    row(86, "CLOUD SYNC", sync_val, sel == 0, cloud.revoked, cloud.registered && !cloud.revoked && c.sync_enabled, 0);
    row(132, "SYNC INTERVAL", iv_val, sel == 1, 0, c.sync_interval_min > 0, 0);

    /* Row 2: Slider for 1..100 rotating local backups */
    int sy = 178;
    text(64, sy + 2, "LOCAL BACKUPS", 20, sel == 2 ? C_GOLD : C_TEXT);
    int sx = 290, sw = 180;
    rrect(sx, sy + 10, sw, 8, 4, C_EDGE, 256);
    int fill = (c.backup_max * sw) / 100;
    if (fill < 8) fill = 8;
    rrect(sx, sy + 10, fill, 8, 4, sel == 2 ? C_CYAN : C_GREEN, 256);
    int kx = sx + ((c.backup_max - 1) * (sw - 10)) / 99 + 5;
    circle(kx, sy + 14, 7, sel == 2 ? C_GOLD : C_TEXT, 256);
    char sl_val[24];
    snprintf(sl_val, sizeof(sl_val), "%d / 100", c.backup_max);
    text_r(W - 64, sy + 2, sl_val, 19, sel == 2 ? C_GOLD : C_GREEN);

    row(224, "SYNC NOW", cloud.registered && !cloud.revoked ? "RUN" : "-", sel == 3, 0, 0, 0);
    row(270, "RESTORE BACKUP", bk_val, sel == 4, 0, bk_n > 0, 0);
    row(316, "CLOUD ACCOUNT", acc_val, sel == 5, cloud.revoked, cloud.registered && !cloud.revoked, 0);

    rect(40, 368, W - 80, 1, C_EDGE, 256);
    if (status[0]) {
        text(44, 376, status, 15, C_CYAN);
    } else {
        static const char *desc[6] = {
            "Automatically pull before play and push when saving.",
            "Periodic cloud + local save check while playing.",
            "Slider (1-100): L1/R1 jumps by 10. Protects against corruption.",
            "Compare local card1.mcd with the cloud and sync right now.",
            "Browse local rotating snapshots and restore an older save.",
            "Register, pair a 2nd console (2FA code) or revoke devices."
        };
        text(44, 376, desc[sel], 15, C_NOTE);
    }
    text(44, 398, "HOT STORAGE: CLOUD SAVES EXPIRE AFTER 40 DAYS INACTIVE.", 13, C_GOLD);
    char ga[16], gb[16], gl[16], gr[16], sh[36];
    glyph(btn_ok(), ga); glyph(btn_back(), gb); glyph(c.pad[A_L1].code, gl); glyph(c.pad[A_R1].code, gr);
    snprintf(sh, sizeof(sh), "%s/%s", gl, gr);
    const char *k[] = {ga, "<>", sh, gb}, *w[] = {"SELECT", "ADJUST", "+/-10", "BACK"};
    footer(k, w, 4);
}

static void draw_backups(void) {
    header("DIGIMON WORLD 3", "LOCAL BACKUPS");
    text(44, 78, "ROTATING card1.mcd SNAPSHOTS (NEWEST FIRST)", 15, C_CYAN);
    rect(40, 102, W - 80, 1, C_EDGE, 256);
    if (bk_count > 0) {
        int gy = (int)(cur_y >= 0 && cur_screen == S_BACKUPS ? cur_y : row_target_y(S_BACKUPS, sel));
        draw_focus(40, gy, W - 80, 34, 8, 0);
        for (int i = bk_scroll; i < bk_count && i < bk_scroll + LIST_VIS; i++) {
            int y = 114 + (i - bk_scroll) * 38;
            row(y, bk_items[i].label, i == 0 ? "LATEST" : "RESTORE", i == sel, 0, i == 0, 0);
        }
    } else {
        text_c(W / 2, 220, "NO LOCAL BACKUPS YET", 18, C_NOTE);
        text_c(W / 2, 250, "A snapshot is saved automatically when you play or sync.", 14, C_OFF);
    }
    if (status[0]) text(44, 394, status, 15, C_GREEN);
    foot_2("RESTORE", "BACK");
}

static int cloud_n(void) { return (cloud.registered && !cloud.revoked) ? 4 : 2; }

static void draw_cloud(void) {
    header("DIGIMON WORLD 3", "CLOUD ACCOUNT");
    rrect(40, 74, W - 80, 114, 10, C_PANEL, 256);
    rrect_line(40, 74, W - 80, 114, 10, cloud.revoked ? C_RED : cloud.registered ? C_CYAN : C_EDGE);

    if (cloud.revoked) {
        text(58, 86, "STATUS: REVOKED / BANNED", 18, C_RED);
        text(58, 112, cloud.revoke_reason[0] ? cloud.revoke_reason : "THIS DEVICE KEY IS NO LONGER VALID.", 15, C_ORANGE);
        text(58, 140, "Use REGISTER THIS DEVICE or LINK CODE to renew access.", 14, C_NOTE);
    } else if (cloud.registered) {
        char l1[96], l2[96], l3[96];
        snprintf(l1, sizeof(l1), "ACCOUNT: %.18s...   DEVICE: %.22s", cloud.account_id, cloud.device_name);
        int days_left = cloud.retention_days > 0 ? cloud.retention_days : 40;
        if (cloud.expires_at > 0) {
            long rem = cloud.expires_at - (long)time(NULL);
            days_left = rem > 0 ? (int)((rem + 86399) / 86400) : 0;
        }
        snprintf(l2, sizeof(l2), "CLOUD REV: #%d   DEVICES: %d   RETENTION: %dD LEFT",
                 cloud.cloud_seq > 0 ? cloud.cloud_seq : cloud.last_seq,
                 cloud.active_devices > 0 ? cloud.active_devices : 1, days_left);
        snprintf(l3, sizeof(l3), "ENDPOINT: %.44s", cloud.worker_url);
        text(58, 86, "STATUS: ACTIVE", 18, C_GREEN);
        text(58, 114, l1, 14, C_TEXT);
        text(58, 136, l2, 14, C_CYAN);
        text(58, 158, l3, 13, C_OFF);
    } else {
        text(58, 86, "STATUS: NOT REGISTERED", 18, C_GOLD);
        text(58, 114, "Register once to get a 256-bit key for free cross-play sync.", 15, C_NOTE);
        text(58, 140, "Saves stay online for 40 days after your last activity.", 15, C_NOTE);
    }

    int n = cloud_n();
    int gy = (int)(cur_y >= 0 && cur_screen == S_CLOUD ? cur_y : row_target_y(S_CLOUD, sel));
    draw_focus(40, gy, W - 80, 36, 8, 0);
    if (!cloud.registered || cloud.revoked) {
        row(206, "REGISTER THIS DEVICE", "NEW KEY", sel == 0, 0, 1, 0);
        row(250, "LINK EXISTING ACCOUNT", "6-DIGIT CODE", sel == 1, 0, 0, 0);
    } else {
        row(206, "ADD NEW DEVICE (2FA)", "GET CODE", sel == 0, 0, 1, 0);
        row(250, "LINK TO ANOTHER ACCOUNT", "ENTER CODE", sel == 1, 0, 0, 0);
        row(294, "LINKED DEVICES", "LIST / REVOKE", sel == 2, 0, 0, 0);
        row(338, "RENEW / NEW ACCOUNT", "ROTATE KEY", sel == 3, 0, 0, 0);
    }
    if (status[0]) text(44, 394, status, 15, C_CYAN);
    (void)n;
    foot_2("SELECT", "BACK");
}

static void draw_pair_host(void) {
    header("DIGIMON WORLD 3", "ADD NEW DEVICE");
    rrect(50, 84, W - 100, 290, 14, C_PANEL, 256);
    rrect_line(50, 84, W - 100, 290, 14, C_CYAN);

    text_c(W / 2, 104, "ENTER THIS 6-DIGIT CODE ON YOUR OTHER DEVICE", 16, C_NOTE);
    int start_x = W / 2 - 170;
    for (int i = 0; i < 6; i++) {
        int bx = start_x + i * 56 + (i >= 3 ? 16 : 0);
        rrect(bx, 138, 44, 58, 8, C_BG1, 256);
        rrect_line(bx, 138, 44, 58, 8, C_CYAN);
        char ch[2] = {pair_code[i] ? pair_code[i] : '-', 0};
        text_c(bx + 22, 150, ch, 28, C_GOLD);
    }

    int rem = pair_expires_at > 0 ? pair_expires_at - (int)time(NULL) : 0;
    if (rem < 0) rem = 0;
    char tm[48];
    snprintf(tm, sizeof(tm), "EXPIRES IN %02d:%02d", rem / 60, rem % 60);
    text_c(W / 2, 212, tm, 15, rem < 30 ? C_ORANGE : C_CYAN);

    if (pair_done) {
        text_c(W / 2, 260, "DEVICE APPROVED AND LINKED!", 20, C_GREEN);
        text_c(W / 2, 294, "The second device can now pull and push your saves.", 15, C_NOTE);
        foot_back("DONE");
    } else if (!strcmp(pair_st.status, "awaiting_approval")) {
        char req[128];
        snprintf(req, sizeof(req), "REQUEST FROM: %s (%s, %s)",
                 pair_st.pending_name[0] ? pair_st.pending_name : "Unknown",
                 pair_st.pending_platform[0] ? pair_st.pending_platform : "?",
                 pair_st.pending_country[0] ? pair_st.pending_country : "XX");
        text_c(W / 2, 254, req, 17, C_GOLD);
        text_c(W / 2, 286, "Approve adding this device to your cloud saves?", 15, C_TEXT);
        char ga[16], gy[16], gb[16];
        glyph(btn_ok(), ga); glyph(c.pad[A_CIRCLE].code, gy); glyph(btn_back(), gb);
        const char *k[] = {ga, gy, gb}, *w[] = {"APPROVE", "DENY", "CANCEL"};
        footer(k, w, 3);
    } else {
        text_c(W / 2, 268, "WAITING FOR SECOND DEVICE TO ENTER CODE...", 16, C_TEXT);
        if (status[0]) text_c(W / 2, 304, status, 14, C_ORANGE);
        foot_back("CANCEL");
    }
}

static void draw_pair_enter(void) {
    header("DIGIMON WORLD 3", "LINK ACCOUNT");
    rrect(50, 84, W - 100, 290, 14, C_PANEL, 256);
    rrect_line(50, 84, W - 100, 290, 14, pair_waiting ? C_GOLD : C_CYAN);

    text_c(W / 2, 108, "ENTER THE 6-DIGIT CODE FROM YOUR HOST DEVICE", 16, C_NOTE);
    int start_x = W / 2 - 170;
    for (int i = 0; i < 6; i++) {
        int bx = start_x + i * 56 + (i >= 3 ? 16 : 0);
        int active = (!pair_waiting && !pair_done && i == pair_cursor);
        rrect(bx, 148, 44, 58, 8, active ? C_FOCUS : C_BG1, 256);
        rrect_line(bx, 148, 44, 58, 8, active ? C_GOLD : C_EDGE);
        char ch[2] = {(char)('0' + pair_digits[i]), 0};
        text_c(bx + 22, 160, ch, 28, active ? C_GOLD : C_TEXT);
    }

    if (pair_done) {
        text_c(W / 2, 248, pair_done_msg[0] ? pair_done_msg : "LINKED TO CLOUD ACCOUNT!", 20, C_GREEN);
        text_c(W / 2, 282, "Your cloud cross-play sync is ready.", 15, C_NOTE);
        foot_back("DONE");
    } else if (pair_waiting) {
        text_c(W / 2, 248, "WAITING FOR APPROVAL ON HOST DEVICE...", 18, C_GOLD);
        text_c(W / 2, 282, "Press APPROVE on your first console/PC.", 15, C_NOTE);
        foot_back("CANCEL");
    } else {
        if (status[0]) text_c(W / 2, 248, status, 15, C_RED);
        else text_c(W / 2, 248, "UP/DOWN changes digit, LEFT/RIGHT moves cursor.", 15, C_NOTE);
        foot_2("SEND CODE", "BACK");
    }
}

static void draw_devices(void) {
    header("DIGIMON WORLD 3", "LINKED DEVICES");
    text(44, 78, "REVOKE LOST OR UNUSED CONSOLES", 15, C_CYAN);
    rect(40, 102, W - 80, 1, C_EDGE, 256);
    if (dev_count > 0) {
        int gy = (int)(cur_y >= 0 && cur_screen == S_DEVICES ? cur_y : row_target_y(S_DEVICES, sel));
        draw_focus(40, gy, W - 80, 34, 8, 0);
        for (int i = dev_scroll; i < dev_count && i < dev_scroll + LIST_VIS; i++) {
            int y = 114 + (i - dev_scroll) * 38;
            char lbl[96], tag[32];
            snprintf(lbl, sizeof(lbl), "%s (%s/%s)", dev_items[i].name, dev_items[i].platform, dev_items[i].country);
            if (dev_items[i].is_current) snprintf(tag, sizeof(tag), "THIS DEVICE");
            else if (!strcmp(dev_items[i].status, "revoked")) snprintf(tag, sizeof(tag), "REVOKED");
            else snprintf(tag, sizeof(tag), "ACTIVE");
            row(y, lbl, tag, i == sel, !strcmp(dev_items[i].status, "revoked"), dev_items[i].is_current, 0);
        }
    } else {
        text_c(W / 2, 220, "NO DEVICES FOUND", 18, C_NOTE);
    }
    if (status[0]) text(44, 394, status, 15, C_CYAN);
    foot_2("REVOKE", "BACK");
}

static void fmt_epoch(long ts, char *out, size_t n) {
    if (ts <= 0) { snprintf(out, n, "unknown time"); return; }
    time_t t = (time_t)ts;
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%d %H:%M", &tm);
}

static void go(int s, int to_sel, int dir, int focus_y);

static void open_conflict_screen(int ret_screen, const char *cmsg) {
    conflict_return_screen = ret_screen;
    snprintf(conflict_msg, sizeof(conflict_msg), "%s", cmsg ? cmsg : "CLOUD AND LOCAL DIFFER");

    char cts[32], lts[32], csha[9] = "--------", lsha[65] = "";
    fmt_epoch(cloud.cloud_updated_at, cts, sizeof(cts));
    if (cloud.cloud_sha256[0]) snprintf(csha, sizeof(csha), "%.8s", cloud.cloud_sha256);
    snprintf(conflict_cloud_info, sizeof(conflict_cloud_info), "CLOUD: REV #%d   %s   [%s]",
             cloud.cloud_seq, cts, csha);

    char card_p[1024];
    snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
    struct stat st;
    if (stat(card_p, &st) == 0 && save_is_valid_mc(card_p, lsha)) {
        fmt_epoch((long)st.st_mtime, lts, sizeof(lts));
        snprintf(conflict_local_info, sizeof(conflict_local_info), "LOCAL: card1.mcd   %s   [%.8s]", lts, lsha);
    } else {
        snprintf(conflict_local_info, sizeof(conflict_local_info), "LOCAL: no valid card1.mcd");
    }

    int fy = ret_screen == S_HOME ? H / 2 : 224 + 14;
    go(S_CONFLICT, 0, 1, fy);
}

static void draw_conflict(void) {
    header("DIGIMON WORLD 3", "CLOUD CONFLICT");
    rrect(40, 74, W - 80, 102, 10, C_PANEL, 256);
    rrect_line(40, 74, W - 80, 102, 10, C_GOLD);
    text(58, 84, "BOTH CLOUD AND LOCAL SAVES CHANGED", 17, C_GOLD);
    text(58, 112, conflict_cloud_info, 14, C_CYAN);
    text(58, 134, conflict_local_info, 14, C_GREEN);
    text(58, 154, "A local backup was saved automatically.", 12, C_NOTE);

    int gy = (int)(cur_y >= 0 && cur_screen == S_CONFLICT ? cur_y : row_target_y(S_CONFLICT, sel));
    draw_focus(40, gy, W - 80, 38, 8, 0);
    char rev_tag[24];
    snprintf(rev_tag, sizeof(rev_tag), "USE REV #%d", cloud.cloud_seq);
    row(194, "DOWNLOAD CLOUD SAVE", rev_tag, sel == 0, 0, 1, 0);
    row(246, "UPLOAD LOCAL TO CLOUD", "OVERWRITE CLOUD", sel == 1, 0, 0, 0);
    row(298, "CANCEL", "DO NOT SYNC", sel == 2, 0, 0, 0);

    static const char *hint[3] = {
        "Replaces card1.mcd on this device with the Cloud save.",
        "Overwrites the Cloud save with this device's card1.mcd.",
        "Leaves both untouched. You can sync later in SAVES & CLOUD."
    };
    rect(40, 368, W - 80, 1, C_EDGE, 256);
    text(44, 380, hint[sel], 15, C_NOTE);
    foot_2("CHOOSE", "BACK");
}

static void draw_conflict_confirm(void) {
    draw_conflict();
    int is_push = (conflict_choice == 1);
    uint32_t accent = is_push ? C_RED : C_ORANGE;
    const char *title = is_push ? "OVERWRITE CLOUD SAVE?" : "REPLACE LOCAL SAVE?";
    char l1[96], l2[96], l3[96], ok_lbl[32];

    if (is_push) {
        snprintf(l1, sizeof(l1), "Cloud REV #%d will be replaced by this device's", cloud.cloud_seq);
        snprintf(l2, sizeof(l2), "local card1.mcd. Other devices will pull this");
        snprintf(l3, sizeof(l3), "save on their next sync.");
    } else {
        snprintf(l1, sizeof(l1), "This device's card1.mcd will be replaced by");
        snprintf(l2, sizeof(l2), "Cloud REV #%d. Your current local save is", cloud.cloud_seq);
        snprintf(l3, sizeof(l3), "safely backed up in saves_backup/ first.");
    }

    rect(0, 0, W, H, C_BG0, 195);
    rrect(70, 108, W - 140, 266, 14, C_PANEL, 256);
    rrect_line(70, 108, W - 140, 266, 14, accent);
    circle(110, 146, 14, accent, 256);
    text_c(110, 132, "!", 24, C_BG0);
    text(138, 130, title, 23, C_TEXT);
    text(102, 176, l1, 16, C_NOTE);
    text(102, 198, l2, 16, C_NOTE);
    text(102, 220, l3, 16, C_NOTE);

    float pos = confirm_pos < 0.0f ? 0.0f : confirm_pos > 2.0f ? 2.0f : confirm_pos;
    uint32_t step_col = pos > 1.4f ? accent : C_GOLD;

    /* 3-step progressive dot confirmation indicator with animated fill */
    int cx = W / 2, dy = 264;
    rect(cx - 72, dy - 2, 144, 4, C_EDGE, 256);
    int fill_w = (int)(pos * 72.0f + 0.5f);
    if (fill_w > 0) rect(cx - 72, dy - 2, fill_w, 4, step_col, 256);
    for (int i = 0; i < 3; i++) {
        int dx = cx + (i - 1) * 72;
        int lit = pos >= (float)i - 0.15f;
        uint32_t col = lit ? (i == 2 ? accent : C_GOLD) : C_BG1;
        circle(dx, dy, 12, col, 256);
        ring(dx, dy, 12, 2, lit ? (i == 2 ? accent : C_GOLD) : C_EDGE);
        char num[2] = {(char)('1' + i), 0};
        text_c(dx, dy - 8, num, 14, lit ? C_BG0 : C_OFF);
    }

    if (conflict_step < 3) {
        snprintf(ok_lbl, sizeof(ok_lbl), "CONFIRM (%d/3)", conflict_step);
    } else {
        snprintf(ok_lbl, sizeof(ok_lbl), is_push ? "OVERWRITE (3/3)" : "REPLACE (3/3)");
    }

    /* Animated sliding [A] pill track: [A] glides left -> right with each confirmation */
    int tx = 92, tw = 344, ty = 304, th = 36;
    rrect(tx, ty, tw, th, 18, C_BG1, 256);
    int ax = tx + 14 + (int)(pos * 74.0f + 0.5f);
    uint32_t track_col = mix(C_GREEN, accent, (int)(pos * 128.0f));
    int trail_w = (ax - tx) + 22;
    if (trail_w > tw) trail_w = tw;
    rrect(tx, ty, trail_w, th, 18, mix(C_BG1, track_col, 72), 256);
    rrect_line(tx, ty, tw, th, 18, conflict_step == 3 ? accent : C_EDGE);

    char ga[16], gb[16];
    glyph(btn_ok(), ga); glyph(btn_back(), gb);
    if (conflict_step == 3) {
        int wave = (int)(plat_now_ms() % 800);
        int pulse = wave < 400 ? wave : 800 - wave;
        ring(ax + 12, ty + th / 2, 16 + pulse * 3 / 400, 2, accent);
    }
    int kx = ax + keycap(ax, ty + th / 2, ga, track_col) + 8;
    text(kx, ty + 9, ok_lbl, 16, C_TEXT);

    int bx = 452;
    bx += keycap(bx, ty + th / 2, gb, C_RED) + 8;
    text(bx, ty + 9, "CANCEL", 16, C_TEXT);
}

static void draw_debug(void) {
    header("DIGIMON WORLD 3", "DEBUG");
    int gy = (int)(cur_y >= 0 && cur_screen == S_DEBUG ? cur_y : row_target_y(S_DEBUG, sel));
    draw_focus(40, gy, W - 80, 36, 8, 0);
    row(92, "LOGS", log_label[c.logs], sel == 0, 0, c.logs > 0, 0);
    row(140, "SAVE LOGS", "ZIP", sel == 1, 0, 0, 0);
    row(188, "CLEAN LOGS", "ASK", sel == 2, 0, 0, 0);
    if (status[0]) text(64, 260, status, 16, C_GREEN);
    text(64, 330, "A bug report is easier with LOGS on HEAVY and the zip attached.", 15, C_NOTE);
    foot_std("RUN");
}

static void draw_led(void) {
    header("DIGIMON WORLD 3", "LED BAR");
    rrect(50, 100, W - 100, 190, 12, C_PANEL, 256);
    rrect_line(50, 100, W - 100, 190, 12, C_EDGE);
    text(76, 118, "XF35H LED BAR", 22, C_GOLD);
    text(76, 156, "One daemon owns the LED line and the regulator;", 17, C_NOTE);
    text(76, 180, "every port asks it. Install it once, then it is a switch.", 17, C_NOTE);
    text(76, 220, "It uses /dev/ttyS2 and installs into /storage/.config/ledd.", 17, C_NOTE);
    if (msg[0]) text(76, 310, msg, 17, C_RED);
    foot_2("INSTALL", "CANCEL");
}

static void draw_loading(void) {
    char stage[80];
    snprintf(stage, sizeof(stage), "%s...", launch_stage);
    load_draw(&c, plat_now_ms() - launch_t0, stage, launch_pct);
}

static void draw_browse(void) {
    header("DIGIMON WORLD 3", "SELECT DISC");
    const char *shown = b_dir;
    size_t len = strlen(b_dir);
    char path_lbl[80];
    if (len > 48) snprintf(path_lbl, sizeof(path_lbl), "...%s", b_dir + len - 45);
    else snprintf(path_lbl, sizeof(path_lbl), "%s", shown);
    text(44, 78, path_lbl, 16, C_CYAN);
    rect(40, 104, W - 80, 1, C_EDGE, 256);
    if (b_count > 0) {
        int gy = (int)(cur_y >= 0 && cur_screen == S_BROWSE ? cur_y : row_target_y(S_BROWSE, sel));
        draw_focus(40, gy, W - 80, 34, 8, 0);
        for (int i = b_scroll; i < b_count && i < b_scroll + BROWSE_VIS; i++) {
            int y = 114 + (i - b_scroll) * 36;
            const char *tag = b_items[i].is_dir ? "[DIR]" : ".CUE/.BIN";
            char nm[48];
            snprintf(nm, sizeof(nm), "%.42s%s", b_items[i].name, strlen(b_items[i].name) > 42 ? "..." : "");
            row(y, nm, tag, i == sel, 0, !b_items[i].is_dir, 0);
        }
    } else {
        text_c(W / 2, 220, "NO .CUE / .BIN OR FOLDERS HERE", 18, C_NOTE);
    }
    char ga[16], gy[16], gb[16];
    glyph(btn_ok(), ga); glyph(c.pad[A_CIRCLE].code, gy); glyph(btn_back(), gb);
    const char *k[] = {ga, gy, gb}, *w[] = {"OPEN / PICK", "SYSTEM DIALOG", "BACK"};
    footer(k, w, 3);
}

static void draw_error(void) {
    header("DIGIMON WORLD 3", "COULD NOT START");
    rrect(50, 110, W - 100, 210, 12, C_PANEL, 256);
    rrect_line(50, 110, W - 100, 210, 12, C_RED);
    text(76, 130, msg, 20, C_RED);
    if (last_err[0]) {
        char el[72];
        snprintf(el, sizeof(el), "%.62s%s", last_err, strlen(last_err) > 62 ? "..." : "");
        text(76, 168, el, 14, C_GOLD);
    }
    text(76, 210, "Turn LOGS to HEAVY in DEBUG, try again, then SAVE LOGS.", 15, C_NOTE);
    foot_std("OK");
}

static void draw_screen(void) {
    switch (screen) {
    case S_SPLASH: draw_splash(); break;
    case S_HOME: draw_home(); break;
    case S_GAME: draw_game(); break;
    case S_VIDEO: draw_video(); break;
    case S_HUD: draw_hud(); break;
    case S_CONTROLS: draw_controls(); break;
    case S_SAVES: draw_saves(); break;
    case S_BACKUPS: draw_backups(); break;
    case S_BACKUP_CONFIRM: draw_backups();
        modal("RESTORE BACKUP?", "Replace card1.mcd with:", bk_count > 0 ? bk_items[sel].label : "",
              "Your current card1.mcd is backed up first.", "YES, RESTORE", "CANCEL", C_ORANGE); break;
    case S_CLOUD: draw_cloud(); break;
    case S_PAIR_HOST: draw_pair_host(); break;
    case S_PAIR_ENTER: draw_pair_enter(); break;
    case S_DEVICES: draw_devices(); break;
    case S_REVOKE_CONFIRM: draw_devices();
        modal("REVOKE DEVICE?", "Block this device from syncing saves:",
              dev_count > 0 ? dev_items[sel].name : "",
              "It will have to re-register or pair again.", "YES, REVOKE", "CANCEL", C_RED); break;
    case S_CONFLICT: draw_conflict(); break;
    case S_CONFLICT_CONFIRM: draw_conflict_confirm(); break;
    case S_DEBUG: draw_debug(); break;
    case S_BROWSE: draw_browse(); break;
    case S_LED: draw_led(); break;
    case S_LED1: draw_home();
        modal("WRONG HARDWARE?", "This does not look like an XF35H. The LED bar would", "do nothing here and could send bytes to another",
              "serial port on /dev/ttyS2.", "CONTINUE ANYWAY (1/2)", "BACK (SAFE)", C_ORANGE); break;
    case S_LED2: draw_home();
        modal("ARE YOU SURE?", "Last confirmation: install the LED daemon on", "hardware that is not an XF35H?", "", "YES, INSTALL", "CANCEL", C_RED); break;
    case S_RESET: draw_home();
        modal("RESET SETTINGS?", "Every setting goes back to the tuned defaults: battle", "text SKIP, the modern buttons, the default battle HUD,",
              "LED bar on. The memory cards are never touched.", "YES, RESET", "NO, KEEP", C_ORANGE); break;
    case S_CLEAN: draw_debug();
        modal("CLEAN LOGS AND DUMPS?", "Delete logs/, crash-*.txt, menu.log, log.txt and the", "old dw3-logs-*.zip? The cards, settings, pad map",
              "and the disc are never touched.", "YES, CLEAN", "NO, KEEP", C_ORANGE); break;
    case S_LOADING: draw_loading(); break;
    case S_ERROR: draw_error(); break;
    }
}

static void start_trans(int dir, int focus_y) {
    if (!trans_from || !trans_to) return;
    memcpy(trans_from, fb, (size_t)ui_w * ui_h * sizeof(uint32_t));
    trans_active = 1;
    trans_dir = dir;
    trans_cy = focus_y;
    trans_t0 = plat_now_ms();
}

/* Xbox-style screen zoom + cross-fade using 16.16 fixed-point sampling (zero float inside the pixel loop). */
static void draw(void) {
    ui_set_lang(c.lang);
    draw_screen();
    if (!trans_active || !trans_to) return;
    long dt = plat_now_ms() - trans_t0;
    if (dt >= TRANS_MS) {
        trans_active = 0;
        return;
    }
    memcpy(trans_to, fb, (size_t)ui_w * ui_h * sizeof(uint32_t));
    int p = (int)(dt * 256 / TRANS_MS);
    int inv = 256 - p;
    int ease = 256 - (inv * inv >> 8); /* 0..256 */

    /* Forward (+1): old screen zooms INTO the selected row (1.0 -> 1.38) while fading out;
     * new screen zooms IN from 0.86 -> 1.00. Back (-1) reverses the motion. */
    int s_from = trans_dir > 0 ? (65536 - (ease * 18000 >> 8)) : (65536 + (ease * 10000 >> 8));
    int s_to = trans_dir > 0 ? (65536 + ((256 - ease) * 10500 >> 8)) : (65536 - ((256 - ease) * 11000 >> 8));
    int ox_from = (trans_dir > 0 ? (-ease * 24 >> 8) : (ease * 24 >> 8)) * ui_scale;
    int ox_to = (trans_dir > 0 ? ((256 - ease) * 24 >> 8) : (-(256 - ease) * 24 >> 8)) * ui_scale;
    int fcy = trans_cy * ui_scale, hcy = ui_h / 2, hcx = ui_w / 2;

    for (int y = 0; y < ui_h; y++) {
        int sy_f = fcy + (((y - fcy) * s_from) >> 16);
        int sy_t = hcy + (((y - hcy) * s_to) >> 16);
        for (int x = 0; x < ui_w; x++) {
            int sx_f = hcx + ox_from + (((x - hcx) * s_from) >> 16);
            int sx_t = hcx + ox_to + (((x - hcx) * s_to) >> 16);
            uint32_t cf = (sx_f >= 0 && sx_f < ui_w && sy_f >= 0 && sy_f < ui_h) ? trans_from[sy_f * ui_w + sx_f] : C_BG0;
            uint32_t ct = (sx_t >= 0 && sx_t < ui_w && sy_t >= 0 && sy_t < ui_h) ? trans_to[sy_t * ui_w + sx_t] : C_BG0;
            fb[y * ui_w + x] = mix(cf, ct, ease);
        }
    }
}

/* ---- launching ---- */
static void find_game_bin(char *out, size_t n) {
    char cand[1024];
    const char *env_bin = getenv("DW3_GAME_BIN");
    if (env_bin && env_bin[0] && access(env_bin, X_OK) == 0) {
        snprintf(out, n, "%s", env_bin);
        return;
    }
    if (is_pc) {
        snprintf(cand, sizeof(cand), "%s/build/port-sdl/dw2003", game_dir);
        if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
        snprintf(cand, sizeof(cand), "%s/build/release/port-sdl/dw2003", game_dir);
        if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
    }
#if defined(__aarch64__)
    const char *arch_name = "dw2003.aarch64";
#elif defined(__arm__)
    const char *arch_name = "dw2003.armhf";
#else
    const char *arch_name = "dw2003";
#endif
    snprintf(cand, sizeof(cand), "%s/%s", game_dir, arch_name);
    if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
    snprintf(cand, sizeof(cand), "%s/%s", exe_dir, arch_name);
    if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
    snprintf(cand, sizeof(cand), "%s/dw2003", game_dir);
    if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
    snprintf(cand, sizeof(cand), "%s/dw2003", exe_dir);
    if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
    if (!is_pc) {
        snprintf(cand, sizeof(cand), "%s/build/port/dw2003", game_dir);
        if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
    }
    snprintf(out, n, "./%s", arch_name);
}

static void browse_load(const char *dir) {
    char resolved[1200];
    if (realpath(dir, resolved)) snprintf(b_dir, sizeof(b_dir), "%s", resolved);
    else snprintf(b_dir, sizeof(b_dir), "%s", dir);
    b_count = browse_read(b_dir, b_items, BROWSE_MAX);
    sel = 0;
    b_scroll = 0;
    cur_y = (float)row_target_y(S_BROWSE, 0);
}

static void start_game(void) {
    if (!disc_path[0] && !is_portmaster) {
        if (browse_disc() != 0) {
            browse_start_dir(b_dir, sizeof(b_dir));
            browse_load(b_dir);
            start_trans(1, 92 + home_step() + 14);
            screen = S_BROWSE;
            cur_screen = S_BROWSE;
            return;
        }
    }
    if (!disc_path[0]) {
        snprintf(msg, sizeof(msg), "NO DISC IN game/");
        last_err[0] = 0;
        screen = S_ERROR;
        return;
    }
    last_err[0] = 0;
    cfg_save(&c);
    {
        char card_p[1024], smsg[128] = "";
        snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
        if (save_is_valid_mc(card_p, NULL)) {
            save_backup_rotate(game_dir, "card1.mcd", c.backup_max);
        }
        if (c.sync_enabled && cloud.registered && !cloud.revoked) {
            int conf = 0;
            int src = cloud_sync_card1(&cloud, game_dir, c.backup_max, 1, &conf, smsg, sizeof(smsg));
            if (src == 1 && conf) {
                open_conflict_screen(S_HOME, smsg);
                return;
            }
        }
    }
    /* What the script used to export after its menu: the game reads the player's pad map itself, and the log level
     * is the DEBUG screen's choice (the game gets "off" unless it was asked for more). */
    setenv("DW3_PAD_REMAP", "1", 1);
    setenv("DW3_LOG", c.logs == 2 ? "heavy" : c.logs == 1 ? "soft" : "off", 1);
    if (getenv("DW3_NOEXEC")) { /* test hook: the loading screen without a game */
        launch_pid = -2;
    } else {
        char bin_path[1024];
        find_game_bin(bin_path, sizeof(bin_path));
        int p[2], sp[2];
        if (pipe(p) != 0) return;
        if (pipe(sp) != 0) { close(p[0]); close(p[1]); return; }
        launch_pid = fork();
        if (launch_pid == 0) {
            char fd_str[16];
            dup2(p[1], 1);
            dup2(p[1], 2);
            close(p[0]);
            close(p[1]);
            close(sp[0]);
            snprintf(fd_str, sizeof(fd_str), "%d", sp[1]);
            setenv("DW3_STATUS_FD", fd_str, 1);
            if (chdir(game_dir) != 0) _exit(126);
            if (c.logs == 2 && access("./probe.sh", R_OK) == 0) {
                (void)system("sh ./probe.sh >/dev/null 2>&1 || true");
            }
            const char *ex = getenv("DW3_EXEC");
            const char *fr = getenv("DW3_FORCE_RENDERER");
            if (ex) execl("/bin/sh", "sh", "-c", ex, (char *)NULL);
            else if (fr && fr[0]) {
                if (is_pc && !c.fullscreen) execl(bin_path, "dw2003", "--config", "./settings.json", "--window", "--watchdog", "0", "--renderer", fr, (char *)NULL);
                else execl(bin_path, "dw2003", "--config", "./settings.json", "--window", "--fullscreen", "--watchdog", "0", "--renderer", fr, (char *)NULL);
            } else if (is_pc && !c.fullscreen) execl(bin_path, "dw2003", "--config", "./settings.json", "--window", "--watchdog", "0", (char *)NULL);
            else execl(bin_path, "dw2003", "--config", "./settings.json", "--window", "--fullscreen", "--watchdog", "0", (char *)NULL);
            _exit(127);
        }
        close(p[1]);
        close(sp[1]);
        char lp[700];
        snprintf(lp, sizeof(lp), "%s/logs", game_dir);
        mkdir(lp, 0755);
        snprintf(lp, sizeof(lp), "%s/logs/game.log", game_dir);
        game_log = fopen(lp, "a");
        launch_fd = p[0];
        status_fd = sp[0];
        fcntl(launch_fd, F_SETFL, O_NONBLOCK);
        fcntl(status_fd, F_SETFL, O_NONBLOCK);
    }
    launch_t0 = plat_now_ms();
    launch_pct = 0;
    launch_started = 0;
    char best[700];
    snprintf(best, sizeof(best), "%s/loading.best", game_dir);
    load_init(best);
    start_trans(1, 92 + 14);
    screen = S_LOADING;
    cur_screen = S_LOADING;
}

/* Reads what the game prints: status_fd tells us even when DW3_LOG=off. */
static void pump_game(void) {
    char buf[512];
    static char line[512], sline[64];
    static int ln, sln;
    if (status_fd >= 0) {
        ssize_t n;
        while ((n = read(status_fd, buf, sizeof(buf))) > 0) {
            for (ssize_t i = 0; i < n; i++) {
                if (buf[i] == '\n' || sln == (int)sizeof(sline) - 1) {
                    sline[sln] = 0;
                    sln = 0;
                    if (!strcmp(sline, "disc")) snprintf(launch_stage, sizeof(launch_stage), "VERIFYING THE DISC");
                    if (!strcmp(sline, "start")) { launch_started = 1; snprintf(launch_stage, sizeof(launch_stage), "STARTING"); }
                } else sline[sln++] = buf[i];
            }
        }
    }
    if (launch_fd >= 0) {
        ssize_t n;
        while ((n = read(launch_fd, buf, sizeof(buf))) > 0) {
            if (game_log) { fwrite(buf, 1, (size_t)n, game_log); fflush(game_log); }
            for (ssize_t i = 0; i < n; i++) {
                if (buf[i] == '\n' || ln == (int)sizeof(line) - 1) {
                    line[ln] = 0;
                    ln = 0;
                    if (line[0]) snprintf(last_err, sizeof(last_err), "%.159s", line);
                    if (strstr(line, "port: disc")) snprintf(launch_stage, sizeof(launch_stage), "VERIFYING THE DISC");
                    if (strstr(line, "port: start:")) { launch_started = 1; snprintf(launch_stage, sizeof(launch_stage), "STARTING"); }
                } else line[ln++] = buf[i];
            }
        }
    }
    long t = plat_now_ms() - launch_t0;
    int target = launch_started ? 100 : (int)(90 - 90 * 20000 / (20000 + t)); /* creeps towards 90 until the game says so */
    if (target > launch_pct) launch_pct = target;
    if (launch_pid > 0) {
        int st;
        if (waitpid(launch_pid, &st, WNOHANG) == launch_pid) {
            launch_pid = -1;
            ln = sln = 0;
            if (status_fd >= 0) { close(status_fd); status_fd = -1; }
            if (launch_fd >= 0) { close(launch_fd); launch_fd = -1; }
            if (game_log) { fclose(game_log); game_log = NULL; }
            if (WIFEXITED(st) && WEXITSTATUS(st) == 0) {
                char card_p[1024], smsg[128] = "";
                snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
                if (save_is_valid_mc(card_p, NULL)) {
                    save_backup_rotate(game_dir, "card1.mcd", c.backup_max);
                    if (c.sync_enabled && cloud.registered && !cloud.revoked) {
                        cloud_push_card1(&cloud, game_dir, c.backup_max, 0, smsg, sizeof(smsg));
                    }
                }
                quit_flag = 1;
                return;
            }
            snprintf(msg, sizeof(msg), WIFSIGNALED(st) ? "THE GAME CRASHED (SIGNAL %d)" : "THE GAME EXITED WITH CODE %d",
                     WIFSIGNALED(st) ? WTERMSIG(st) : WEXITSTATUS(st));
            screen = S_ERROR;
        }
    }
}

/* ---- input ---- */
static int held_select, held_start;

static void go(int s, int to_sel, int dir, int focus_y) {
    start_trans(dir, focus_y);
    screen = s;
    sel = to_sel;
    capture = 0;
    status[0] = 0;
    msg[0] = 0;
    int ty = row_target_y(s, to_sel);
    cur_screen = s;
    cur_y = (float)ty;
}

/* Leaving an edit screen saves it, unless two rows share a button: then it stays (the red rows say why). */
static void leave_edit(int home_id) {
    int back_sel = home_index(home_id);
    if (screen == S_HUD && dup_hud(&c, NULL)) return;
    if (screen == S_CONTROLS && dup_actions(&c, NULL)) return;
    cfg_save(&c);
    go(S_HOME, back_sel, -1, 92 + back_sel * home_step() + 14);
}

static void home_enter(void) {
    int fy = 92 + sel * home_step() + 14;
    switch (home_items[sel]) {
    case HI_PLAY: start_game(); return;
    case HI_GAME: go(S_GAME, 0, 1, fy); return;
    case HI_VIDEO: go(S_VIDEO, 0, 1, fy); return;
    case HI_HUD: go(S_HUD, 0, 1, fy); return;
    case HI_CONTROLS: go(S_CONTROLS, 0, 1, fy); return;
    case HI_SAVES: go(S_SAVES, 0, 1, fy); return;
    case HI_LED:
        if (led_state(&c) == LED_NONE) {
            if (led_compatible()) go(S_LED, 0, 1, fy);
            else { start_trans(1, fy); screen = S_LED1; }
        } else { c.led = !c.led; cfg_save(&c); }
        return;
    case HI_DEBUG: go(S_DEBUG, 0, 1, fy); return;
    case HI_RESET: start_trans(1, fy); screen = S_RESET; return;
    }
}

static void on_press(int code) {
    int d = code == K_LEFT ? -1 : code == K_RIGHT ? 1 : 0;
    int ok = code == btn_ok(), back = code == btn_back();
    int is_start = code == btn_start(), is_select = code == btn_select();
    if (screen == S_SPLASH) { go(S_HOME, 0, 1, H / 2); return; }
    if (screen == S_LOADING) { load_press(code); return; }
    if (is_select) held_select = 1;
    if (is_start) held_start = 1;
    if (held_select && held_start) { quit_flag = 2; return; } /* leaves without saving the edit in progress */
    if (screen == S_HUD && capture) {
        static const int hud_valid[] = {K_SOUTH, K_EAST, K_WEST, K_NORTH, K_L1, K_R1, K_L2, K_R2};
        if (is_start || is_select) { capture = 0; return; }
        for (unsigned i = 0; i < sizeof(hud_valid) / sizeof(hud_valid[0]); i++) {
            if (hud_valid[i] == code) {
                c.hud[sel] = code;
                capture = 0;
                return;
            }
        }
        return;
    }
    if (screen == S_CONTROLS && capture) {
        static const int valid[] = {K_SOUTH, K_EAST, K_WEST, K_NORTH, K_L1, K_R1, K_L2, K_R2, K_START, K_SELECT};
        if (code == K_UP || code == K_DOWN) { capture = 0; return; }
        for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
            if (valid[i] != code) continue;
            int a = act_order[sel - 1];
            int lay = cfg_layout(&c);
            c.pad[a].code = code;
            if (a <= A_TRIANGLE) {
                c.pad[a].letter = layout_letter(lay == LAYOUT_CUSTOM ? LAYOUT_XBOX : lay, code);
            }
            capture = 0;
            return;
        }
        return;
    }
    int n = 0;
    switch (screen) {
    case S_HOME: n = home_n; break;
    case S_GAME: n = game_n(); break;
    case S_VIDEO: n = VIDEO_N; break;
    case S_HUD: n = H_COUNT + 2; break;
    case S_CONTROLS: n = 1 + A_COUNT; break;
    case S_SAVES: n = 6; break;
    case S_BACKUPS: n = bk_count; break;
    case S_CLOUD: n = cloud_n(); break;
    case S_CONFLICT: n = 3; break;
    case S_DEVICES: n = dev_count; break;
    case S_DEBUG: n = 3; break;
    case S_BROWSE: n = b_count; break;
    }
    if (n && code == K_UP) sel = (sel + n - 1) % n;
    if (n && code == K_DOWN) sel = (sel + 1) % n;
    if (screen == S_BROWSE && b_count > 0) {
        if (sel < b_scroll) b_scroll = sel;
        if (sel >= b_scroll + BROWSE_VIS) b_scroll = sel - BROWSE_VIS + 1;
    }
    if (screen == S_BACKUPS && bk_count > 0) {
        if (sel < bk_scroll) bk_scroll = sel;
        if (sel >= bk_scroll + LIST_VIS) bk_scroll = sel - LIST_VIS + 1;
    }
    if (screen == S_DEVICES && dev_count > 0) {
        if (sel < dev_scroll) dev_scroll = sel;
        if (sel >= dev_scroll + LIST_VIS) dev_scroll = sel - LIST_VIS + 1;
    }
    int s = d ? d : 1;
    switch (screen) {
    case S_HOME:
        if (ok) home_enter();
        if (is_start) start_game();
        break;
    case S_GAME:
        if (is_start) start_game();
        if (back) leave_edit(HI_GAME);
        if (ok || d) {
            switch (sel) {
            case 0: c.lang = (c.lang + s + 8) % 8; break;
            case 1: c.ff = (c.ff + s + 5) % 5; break;
            case 2: c.load = (c.load + s + 5) % 5; break;
            case 3: c.skip = !c.skip; break;
            case 4: c.battles = !c.battles; if (!c.battles) c.hit = 0; break;
            case 5: if (c.battles) c.hit = !c.hit; break;
            case 6: c.btext = (c.btext + s + 3) % 3; break;
            case 7:
                if (ok) {
                    browse_start_dir(b_dir, sizeof(b_dir));
                    browse_load(b_dir);
                    go(S_BROWSE, 0, 1, 90 + 7 * game_step() + 14);
                }
                break;
            }
        }
        break;
    case S_VIDEO:
        if (is_start) start_game();
        if (back) leave_edit(HI_VIDEO);
        if (ok || d) {
            switch (sel) {
            case 0: c.widescreen = !c.widescreen; if (c.widescreen) c.renderer_pc = 0; break;
            case 1: c.fullscreen = !c.fullscreen; break;
            case 2: c.fill = !c.fill; break;
            case 3: {
                static const int scales[] = {0, 2, 3, 4, 5, 6};
                int idx = 0;
                for (; idx < 6; idx++) if (scales[idx] == c.scale) break;
                c.scale = scales[((idx < 6 ? idx : 0) + s + 6) % 6];
                break;
            }
            case 4: c.renderer_pc = !c.renderer_pc; if (c.renderer_pc) c.widescreen = 0; break;
            case 5: if (!c.renderer_pc) c.filter = (c.filter + s + 5) % 5; break;
            }
        }
        break;
    case S_HUD:
        if (back) leave_edit(HI_HUD);
        if (is_select) {
            Cfg def;
            cfg_defaults(&def);
            memcpy(c.hud, def.hud, sizeof(c.hud));
            c.bmem = def.bmem;
            c.bbuf = def.bbuf;
            return;
        }
        if (is_start && sel < H_COUNT) { capture = 1; break; }
        if (d && sel < H_COUNT) c.hud[sel] = hud_cycle(c.hud[sel], d);
        else if (sel == H_COUNT && (ok || d)) c.bmem = !c.bmem;
        else if (sel == H_COUNT + 1 && (ok || d)) c.bbuf = (c.bbuf + s + 3) % 3;
        break;
    case S_CONTROLS:
        if (back) leave_edit(HI_CONTROLS);
        if (is_select) {
            Cfg def;
            cfg_defaults(&def);
            memcpy(c.pad, def.pad, sizeof(c.pad));
            return;
        }
        if (sel == 0 && (ok || d)) {
            static const int cyc[] = {LAYOUT_XBOX, LAYOUT_NINTENDO, LAYOUT_PS1};
            int cur = cfg_layout(&c), i = 0;
            for (; i < 3; i++) if (cyc[i] == cur) break;
            cfg_set_layout(&c, cyc[(i + s + 3) % 3]);
        } else if (sel > 0 && is_start) capture = 1;
        break;
    case S_SAVES:
        if (is_start) start_game();
        if (back) leave_edit(HI_SAVES);
        if (sel == 0 && (ok || d)) {
            if (!cloud.registered || cloud.revoked) {
                char err[128] = "";
                if (cloud.registered && !cloud.revoked) cloud_refresh_me(&cloud, game_dir, err, sizeof(err));
                go(S_CLOUD, 0, 1, 86 + 14);
            } else {
                c.sync_enabled = !c.sync_enabled;
            }
        } else if (sel == 1 && (ok || d)) {
            static const int ivs[] = {0, 5, 10, 15, 20, 30};
            int idx = 2;
            for (int i = 0; i < 6; i++) if (ivs[i] == c.sync_interval_min) { idx = i; break; }
            c.sync_interval_min = ivs[(idx + s + 6) % 6];
        } else if (sel == 2) {
            if (d) {
                c.backup_max += d;
                if (c.backup_max < 1) c.backup_max = 1;
                if (c.backup_max > 100) c.backup_max = 100;
            } else if (code == K_L1) {
                c.backup_max -= 10;
                if (c.backup_max < 1) c.backup_max = 1;
            } else if (code == K_R1) {
                c.backup_max += 10;
                if (c.backup_max > 100) c.backup_max = 100;
            }
        } else if (sel == 3 && ok) {
            int conf = 0;
            if (!cloud.registered || cloud.revoked) {
                snprintf(status, sizeof(status), "%s", cloud.revoked ? "DEVICE REVOKED - OPEN CLOUD ACCOUNT" : "REGISTER IN CLOUD ACCOUNT FIRST");
            } else if (cloud_sync_card1(&cloud, game_dir, c.backup_max, 1, &conf, status, sizeof(status)) == 1 && conf) {
                open_conflict_screen(S_SAVES, status);
            }
        } else if (sel == 4 && ok) {
            bk_count = save_backup_list(game_dir, bk_items, BACKUP_MAX_LIST);
            bk_scroll = 0;
            go(S_BACKUPS, 0, 1, 270 + 14);
        } else if (sel == 5 && ok) {
            char err[128] = "";
            if (cloud.registered) cloud_refresh_me(&cloud, game_dir, err, sizeof(err));
            go(S_CLOUD, 0, 1, 316 + 14);
            if (err[0]) snprintf(status, sizeof(status), "%s", err);
        }
        break;
    case S_BACKUPS:
        if (back) go(S_SAVES, 4, -1, 270 + 14);
        if (ok && bk_count > 0) {
            start_trans(1, row_target_y(S_BACKUPS, sel) + 14);
            screen = S_BACKUP_CONFIRM;
        }
        break;
    case S_BACKUP_CONFIRM:
        if (back) { start_trans(-1, row_target_y(S_BACKUPS, sel) + 14); screen = S_BACKUPS; }
        if (ok && bk_count > 0) {
            char err[128] = "", restored_sha[9] = "";
            snprintf(restored_sha, sizeof(restored_sha), "%.8s", bk_items[sel].sha_short);
            if (save_backup_restore(game_dir, bk_items[sel].name, c.backup_max, err, sizeof(err)) == 0) {
                bk_count = save_backup_list(game_dir, bk_items, BACKUP_MAX_LIST);
                if (sel >= bk_count) sel = bk_count > 0 ? bk_count - 1 : 0;
                if (bk_scroll > sel) bk_scroll = sel;
                if (bk_count > LIST_VIS && bk_scroll > bk_count - LIST_VIS) bk_scroll = bk_count - LIST_VIS;
                if (bk_count <= LIST_VIS) bk_scroll = 0;
                start_trans(-1, row_target_y(S_BACKUPS, sel) + 14);
                screen = S_BACKUPS;
                snprintf(status, sizeof(status), "RESTORED %s", restored_sha);
            } else {
                start_trans(-1, row_target_y(S_BACKUPS, sel) + 14);
                screen = S_BACKUPS;
                snprintf(status, sizeof(status), "%s", err);
            }
        }
        break;
    case S_CLOUD:
        if (back) go(S_SAVES, 5, -1, 316 + 14);
        if (ok) {
            char err[128] = "";
            if (!cloud.registered || cloud.revoked) {
                if (sel == 0) {
                    if (cloud_register(&cloud, game_dir, is_portmaster, err, sizeof(err)) == 0) {
                        c.sync_enabled = 1;
                        cfg_save(&c);
                        char card_p[1024], smsg[96] = "";
                        snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
                        if (save_is_valid_mc(card_p, NULL) &&
                            cloud_push_card1(&cloud, game_dir, c.backup_max, 0, smsg, sizeof(smsg)) == 0) {
                            snprintf(status, sizeof(status), "REGISTERED & %s", smsg);
                        } else {
                            snprintf(status, sizeof(status), "REGISTERED! DEVICE KEY SAVED LOCALLY");
                        }
                    } else {
                        snprintf(status, sizeof(status), "%s", err);
                    }
                } else if (sel == 1) {
                    memset(pair_digits, 0, sizeof(pair_digits));
                    pair_cursor = 0; pair_waiting = 0; pair_done = 0; pair_done_msg[0] = 0;
                    go(S_PAIR_ENTER, 0, 1, 250 + 14);
                }
            } else {
                if (sel == 0) {
                    int exp_sec = 300;
                    memset(&pair_st, 0, sizeof(pair_st));
                    pair_done = 0;
                    if (cloud_pair_create(&cloud, pair_code, &exp_sec, err, sizeof(err)) == 0) {
                        pair_expires_at = (int)time(NULL) + exp_sec;
                        pair_last_poll_ms = plat_now_ms();
                        go(S_PAIR_HOST, 0, 1, 206 + 14);
                    } else {
                        snprintf(status, sizeof(status), "%s", err);
                    }
                } else if (sel == 1) {
                    memset(pair_digits, 0, sizeof(pair_digits));
                    pair_cursor = 0; pair_waiting = 0; pair_done = 0; pair_done_msg[0] = 0;
                    go(S_PAIR_ENTER, 0, 1, 250 + 14);
                } else if (sel == 2) {
                    if (cloud_list_devices(&cloud, dev_items, CLOUD_MAX_DEVICES, &dev_count, err, sizeof(err)) == 0) {
                        dev_scroll = 0;
                        go(S_DEVICES, 0, 1, 294 + 14);
                    } else {
                        snprintf(status, sizeof(status), "%s", err);
                    }
                } else if (sel == 3) {
                    if (cloud_register(&cloud, game_dir, is_portmaster, err, sizeof(err)) == 0) {
                        c.sync_enabled = 1;
                        cfg_save(&c);
                        char card_p[1024], smsg[96] = "";
                        snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
                        if (save_is_valid_mc(card_p, NULL) &&
                            cloud_push_card1(&cloud, game_dir, c.backup_max, 0, smsg, sizeof(smsg)) == 0) {
                            snprintf(status, sizeof(status), "NEW ACCOUNT & %s", smsg);
                        } else {
                            snprintf(status, sizeof(status), "NEW ACCOUNT CREATED");
                        }
                    } else {
                        snprintf(status, sizeof(status), "%s", err);
                    }
                }
            }
        }
        break;
    case S_PAIR_HOST:
        if (back) {
            if (!pair_done && !strcmp(pair_st.status, "awaiting_approval")) {
                char err[96];
                cloud_pair_approve(&cloud, pair_code, 0, err, sizeof(err));
            }
            go(S_CLOUD, 0, -1, 206 + 14);
        } else if (!pair_done && !strcmp(pair_st.status, "awaiting_approval")) {
            char err[128] = "";
            if (ok) {
                if (cloud_pair_approve(&cloud, pair_code, 1, err, sizeof(err)) == 0) {
                    pair_done = 1;
                    cloud_refresh_me(&cloud, game_dir, err, sizeof(err));
                } else {
                    snprintf(status, sizeof(status), "%s", err);
                }
            } else if (code == c.pad[A_CIRCLE].code) {
                cloud_pair_approve(&cloud, pair_code, 0, err, sizeof(err));
                go(S_CLOUD, 0, -1, 206 + 14);
            }
        }
        break;
    case S_PAIR_ENTER:
        if (back) {
            go(S_CLOUD, (!cloud.registered || cloud.revoked) ? 1 : 1, -1, 250 + 14);
        } else if (!pair_waiting && !pair_done) {
            if (code == K_LEFT) pair_cursor = (pair_cursor + 5) % 6;
            else if (code == K_RIGHT) pair_cursor = (pair_cursor + 1) % 6;
            else if (code == K_UP) pair_digits[pair_cursor] = (pair_digits[pair_cursor] + 1) % 10;
            else if (code == K_DOWN) pair_digits[pair_cursor] = (pair_digits[pair_cursor] + 9) % 10;
            else if (ok) {
                char cstr[8], err[128] = "";
                for (int i = 0; i < 6; i++) cstr[i] = (char)('0' + pair_digits[i]);
                cstr[6] = 0;
                if (cloud_pair_claim(&cloud, cstr, is_portmaster, &pair_claim, err, sizeof(err)) == 0) {
                    pair_waiting = 1;
                    pair_last_poll_ms = plat_now_ms();
                    status[0] = 0;
                } else {
                    snprintf(status, sizeof(status), "%s", err);
                }
            }
        }
        break;
    case S_DEVICES:
        if (back) go(S_CLOUD, 2, -1, 294 + 14);
        if (ok && dev_count > 0 && !dev_items[sel].is_current && !strcmp(dev_items[sel].status, "active")) {
            start_trans(1, row_target_y(S_DEVICES, sel) + 14);
            screen = S_REVOKE_CONFIRM;
        }
        break;
    case S_REVOKE_CONFIRM:
        if (back) { start_trans(-1, row_target_y(S_DEVICES, sel) + 14); screen = S_DEVICES; }
        if (ok && dev_count > 0) {
            char err[128] = "";
            if (cloud_revoke_device(&cloud, dev_items[sel].device_id, err, sizeof(err)) == 0) {
                cloud_list_devices(&cloud, dev_items, CLOUD_MAX_DEVICES, &dev_count, err, sizeof(err));
                if (sel >= dev_count) sel = dev_count > 0 ? dev_count - 1 : 0;
                if (dev_scroll > sel) dev_scroll = sel;
                if (dev_count > LIST_VIS && dev_scroll > dev_count - LIST_VIS) dev_scroll = dev_count - LIST_VIS;
                if (dev_count <= LIST_VIS) dev_scroll = 0;
                start_trans(-1, row_target_y(S_DEVICES, sel) + 14);
                screen = S_DEVICES;
                snprintf(status, sizeof(status), "DEVICE REVOKED");
            } else {
                start_trans(-1, row_target_y(S_DEVICES, sel) + 14);
                screen = S_DEVICES;
                snprintf(status, sizeof(status), "%s", err);
            }
        }
        break;
    case S_CONFLICT:
        if (back || (ok && sel == 2)) {
            int rsel = conflict_return_screen == S_SAVES ? 3 : conflict_return_screen == S_CLOUD ? 1 : 0;
            go(conflict_return_screen, rsel, -1, H / 2);
        } else if (ok && (sel == 0 || sel == 1)) {
            conflict_choice = sel;
            conflict_step = 1;
            confirm_pos = 0.0f;
            start_trans(1, row_target_y(S_CONFLICT, sel) + 14);
            screen = S_CONFLICT_CONFIRM;
        }
        break;
    case S_CONFLICT_CONFIRM:
        if (back) {
            start_trans(-1, row_target_y(S_CONFLICT, conflict_choice) + 14);
            screen = S_CONFLICT;
        } else if (ok) {
            if (conflict_step < 3) {
                conflict_step++;
            } else {
                char res_msg[160] = "";
                if (conflict_choice == 0) {
                    cloud_pull_card1(&cloud, game_dir, c.backup_max, res_msg, sizeof(res_msg));
                } else {
                    cloud_push_card1(&cloud, game_dir, c.backup_max, 1, res_msg, sizeof(res_msg));
                }
                int rsel = conflict_return_screen == S_SAVES ? 3 : conflict_return_screen == S_CLOUD ? 1 : 0;
                go(conflict_return_screen, rsel, -1, H / 2);
                snprintf(status, sizeof(status), "%s", res_msg);
            }
        }
        break;
    case S_BROWSE:
        if (back) {
            if (strcmp(b_dir, "/") != 0 && b_count > 0 && strcmp(b_items[0].name, "..") == 0) {
                char next[1400];
                snprintf(next, sizeof(next), "%s/..", b_dir);
                browse_load(next);
            } else {
                go(S_GAME, 7, -1, 90 + 7 * game_step() + 14);
            }
        } else if (code == c.pad[A_CIRCLE].code && is_pc) {
            if (browse_disc() == 0) {
                cfg_save(&c);
                go(S_GAME, 7, -1, 90 + 7 * game_step() + 14);
            }
        } else if (ok && b_count > 0) {
            char next[1500];
            snprintf(next, sizeof(next), "%s/%s", strcmp(b_dir, "/") ? b_dir : "", b_items[sel].name);
            if (b_items[sel].is_dir) {
                browse_load(next);
            } else {
                snprintf(disc_path, sizeof(disc_path), "%.1199s", next);
                cfg_save(&c);
                go(S_GAME, 7, -1, 90 + 7 * game_step() + 14);
            }
        }
        break;
    case S_DEBUG:
        if (is_start) start_game();
        if (back) { int hi = home_index(HI_DEBUG); cfg_save(&c); go(S_HOME, hi, -1, 92 + hi * home_step() + 14); }
        if (sel == 0 && (ok || d)) c.logs = (c.logs + s + 3) % 3;
        if (sel == 1 && ok) {
            char nm[80];
            int r = logs_save(nm, sizeof(nm));
            snprintf(status, sizeof(status), r == 0 ? "SAVED %s" : "COULD NOT SAVE THE ZIP%.0s", nm);
        }
        if (sel == 2 && ok) { start_trans(1, 188 + 14); screen = S_CLEAN; }
        break;
    case S_LED:
        if (back) { int hi = home_index(HI_LED); go(S_HOME, hi, -1, 92 + hi * home_step() + 14); }
        if (ok && led_install(msg, sizeof(msg)) == 0) {
            int hi = home_index(HI_LED);
            c.led = 1; cfg_save(&c); go(S_HOME, hi, -1, 92 + hi * home_step() + 14);
        }
        break;
    case S_LED1:
        if (back) { start_trans(-1, 92 + home_index(HI_LED) * home_step() + 14); screen = S_HOME; }
        if (ok) { start_trans(1, H / 2); screen = S_LED2; }
        break;
    case S_LED2:
        if (back) { start_trans(-1, 92 + home_index(HI_LED) * home_step() + 14); screen = S_HOME; }
        if (ok) {
            int hi = home_index(HI_LED);
            if (led_install(msg, sizeof(msg)) == 0) { c.led = 1; cfg_save(&c); start_trans(-1, 92 + hi * home_step() + 14); screen = S_HOME; }
            else { start_trans(1, H / 2); screen = S_ERROR; }
        }
        break;
    case S_RESET:
        if (back) { start_trans(-1, 92 + home_index(HI_RESET) * home_step() + 14); screen = S_HOME; }
        if (ok) { cfg_defaults(&c); cfg_save(&c); go(S_HOME, 0, -1, 92 + 14); }
        break;
    case S_CLEAN:
        if (back) { start_trans(-1, 188 + 14); screen = S_DEBUG; }
        if (ok) { snprintf(status, sizeof(status), "REMOVED %d FILES", logs_clean()); start_trans(-1, 188 + 14); screen = S_DEBUG; }
        break;
    case S_ERROR:
        if (ok || back) go(S_HOME, 0, -1, H / 2);
        break;
    }
}

int main(int argc, char **argv) {
    int kind = PLAT_FB, force_splash = 0;
    const char *script = NULL;
    const char *sl = argc > 0 && argv[0] ? strrchr(argv[0], '/') : NULL;
    if (sl && (size_t)(sl - argv[0]) < sizeof(exe_dir)) {
        size_t n = (size_t)(sl - argv[0]);
        memcpy(exe_dir, argv[0], n);
        exe_dir[n] = 0;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sdl")) kind = PLAT_SDL;
        else if (!strcmp(argv[i], "--fb")) kind = PLAT_FB;
        else if (!strcmp(argv[i], "--pc")) is_pc = 1;
        else if (!strcmp(argv[i], "--splash")) force_splash = 1;
        else if (!strcmp(argv[i], "--null") && i + 1 < argc) { kind = PLAT_NULL; script = argv[++i]; }
        else if (!strcmp(argv[i], "--dir") && i + 1 < argc) snprintf(game_dir, sizeof(game_dir), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) plat_shot_dir = argv[++i];
    }
    if (kind == PLAT_NULL && !force_splash) {
        screen = S_HOME;
        cur_screen = S_HOME;
        cur_y = (float)row_target_y(S_HOME, 0);
    }
    signal(SIGPIPE, SIG_IGN);
#if defined(DW3_TARGET_PORTMASTER)
    is_portmaster = 1;
    is_pc = 0;
#elif defined(DW3_TARGET_PC)
    is_pc = 1;
    is_portmaster = 0;
#else
    if (getenv("DW3_PORTMASTER") || getenv("CFW_NAME")) is_portmaster = 1;
    if (getenv("DW3_PC")) { is_pc = 1; is_portmaster = 0; }
#endif
    if (kind == PLAT_FB) {
        const char *dv = getenv("DW3_VIDEO");
        if (dv && !strcmp(dv, "sdl2")) kind = PLAT_SDL;
    }
    if (plat_open(kind, script) != 0) {
        if (kind == PLAT_FB && plat_open(PLAT_SDL, NULL) == 0) kind = PLAT_SDL;
        else { fprintf(stderr, "dw3-launcher: no screen to draw on\n"); return 3; }
    }
#if !defined(DW3_TARGET_PORTMASTER) && !defined(DW3_TARGET_PC)
    if (!is_portmaster && kind == PLAT_SDL) is_pc = 1;
    if (!is_pc && kind == PLAT_FB) is_portmaster = 1;
#endif
    build_home_list();
    find_disc();
    cfg_load(&c);
    cloud_state_load(&cloud, game_dir);
    trans_from = malloc((size_t)ui_w * ui_h * sizeof(uint32_t));
    trans_to = malloc((size_t)ui_w * ui_h * sizeof(uint32_t));
    long last = plat_now_ms();
    if (screen == S_SPLASH) splash_t0 = last;
    /* Pre-render first frame so any initial transition snapshots a real picture */
    draw();
    plat_present();
    for (;;) {
        int ty = row_target_y(screen, sel);
        int cursor_moving = ty >= 0 && cur_screen == screen && (cur_y - ty > 0.5f || ty - cur_y > 0.5f);
        int animating = trans_active || cursor_moving || screen == S_SPLASH || screen == S_LOADING || screen == S_CONFLICT_CONFIRM;
        Ev ev = {0};
        int r = plat_poll(&ev, animating ? 16 : 33);
        if (r < 0) break;
        if (r > 0 && ev.code) {
            if (ev.down) on_press(ev.code);
            else if (ev.code == btn_select()) held_select = 0;
            else if (ev.code == btn_start()) held_start = 0;
        }
        long now = plat_now_ms();
        long dt = now - last;
        if (dt < 1) dt = 1;
        if (dt > 60) dt = 60;
        if (screen == S_SPLASH && now - splash_t0 >= SPLASH_MS) {
            draw();
            go(S_HOME, 0, 1, H / 2);
        }
        if (screen == S_CONFLICT_CONFIRM) {
            float target = (float)(conflict_step - 1);
            float diff = target - confirm_pos;
            if (diff > -0.01f && diff < 0.01f) confirm_pos = target;
            else confirm_pos += diff * (dt >= 32 ? 0.45f : 0.25f);
        }
        if (screen == S_PAIR_HOST && !pair_done && now - pair_last_poll_ms >= 1800) {
            pair_last_poll_ms = now;
            char perr[128] = "";
            if (cloud_pair_status(&cloud, pair_code, &pair_st, perr, sizeof(perr)) != 0 && perr[0]) {
                snprintf(status, sizeof(status), "%s", perr);
            }
        } else if (screen == S_PAIR_ENTER && pair_waiting && !pair_done && now - pair_last_poll_ms >= 1800) {
            pair_last_poll_ms = now;
            char perr[128] = "";
            int app = 0;
            int prc = cloud_pair_poll(&cloud, game_dir, &pair_claim, &app, perr, sizeof(perr));
            if (prc == 0 && app) {
                pair_waiting = 0;
                c.sync_enabled = 1;
                cfg_save(&c);
                cloud_refresh_me(&cloud, game_dir, perr, sizeof(perr));
                int conf = 0;
                char smsg[96] = "";
                int src = cloud_sync_card1(&cloud, game_dir, c.backup_max, 1, &conf, smsg, sizeof(smsg));
                if (src == 1 && conf) {
                    open_conflict_screen(S_CLOUD, smsg);
                } else {
                    pair_done = 1;
                    if (src == 0 && smsg[0]) snprintf(pair_done_msg, sizeof(pair_done_msg), "LINKED & %s", smsg);
                    else snprintf(pair_done_msg, sizeof(pair_done_msg), "LINKED TO CLOUD ACCOUNT!");
                }
            } else if (prc == -2) {
                pair_waiting = 0;
                snprintf(status, sizeof(status), "%s", perr[0] ? perr : "PAIRING FAILED");
            }
        }
        /* Smooth exponential cursor glide towards selected row */
        ty = row_target_y(screen, sel);
        if (ty >= 0) {
            if (cur_screen != screen || cur_y < 0.0f) {
                cur_screen = screen;
                cur_y = (float)ty;
            } else {
                float diff = (float)ty - cur_y;
                if (diff > -0.5f && diff < 0.5f) cur_y = (float)ty;
                else cur_y += diff * (dt >= 32 ? 0.55f : 0.35f);
            }
        }
        if (screen == S_LOADING) {
            load_step(now - last);
            pump_game();
            if (launch_started && screen == S_LOADING && launch_pid != -2) {
                /* the game's window is next: leave the last picture up, give the screen away, and monitor logs + periodic sync */
                draw();
                plat_present();
                plat_release();
                if (status_fd >= 0) { close(status_fd); status_fd = -1; }

                char card1_p[1024], last_sha[65] = "", cur_sha[65] = "", pending_sha[65] = "", smsg[128] = "";
                snprintf(card1_p, sizeof(card1_p), "%s/card1.mcd", game_dir);
                save_is_valid_mc(card1_p, last_sha);
                time_t last_sync_ts = time(NULL);
                pid_t sync_pid = -1;
                int st = 0, exited = 0;

                while (launch_pid > 0 && !exited) {
                    if (launch_fd >= 0) {
                        struct pollfd pfd = {launch_fd, POLLIN, 0};
                        poll(&pfd, 1, 1000);
                        char buf[1024];
                        ssize_t n;
                        for (;;) {
                            n = read(launch_fd, buf, sizeof(buf));
                            if (n > 0) {
                                if (game_log) { fwrite(buf, 1, (size_t)n, game_log); fflush(game_log); }
                            } else if (n == 0) {
                                close(launch_fd);
                                launch_fd = -1;
                                break;
                            } else {
                                if (errno == EINTR) continue;
                                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                                close(launch_fd);
                                launch_fd = -1;
                                break;
                            }
                        }
                    } else {
                        sleep(1);
                    }

                    if (sync_pid > 0) {
                        int sst = 0;
                        if (waitpid(sync_pid, &sst, WNOHANG) == sync_pid) {
                            sync_pid = -1;
                            if (WIFEXITED(sst) && WEXITSTATUS(sst) == 0) {
                                cloud_state_load(&cloud, game_dir);
                                snprintf(last_sha, sizeof(last_sha), "%s", pending_sha);
                                if (game_log) {
                                    fprintf(game_log, "[cloud-sync] periodic push ok (rev #%d)\n", cloud.last_seq);
                                    fflush(game_log);
                                }
                            }
                        }
                    }

                    if (waitpid(launch_pid, &st, WNOHANG) == launch_pid) {
                        exited = 1;
                        break;
                    }

                    time_t tnow = time(NULL);
                    if (sync_pid < 0 && c.sync_interval_min > 0 &&
                        tnow - last_sync_ts >= (time_t)c.sync_interval_min * 60) {
                        struct stat cst;
                        if (stat(card1_p, &cst) == 0) {
                            if ((tnow - cst.st_mtime) < 2) {
                                /* Wait 2s for active memory card write to finish instead of skipping the whole interval. */
                                continue;
                            }
                            if (save_is_valid_mc(card1_p, cur_sha) && strcmp(cur_sha, last_sha) != 0) {
                                save_backup_rotate(game_dir, "card1.mcd", c.backup_max);
                                if (c.sync_enabled && cloud.registered && !cloud.revoked) {
                                    snprintf(pending_sha, sizeof(pending_sha), "%s", cur_sha);
                                    sync_pid = fork();
                                    if (sync_pid == 0) {
                                        /* Periodic in-game sync ONLY pushes, never overwrites running game's card1.mcd. */
                                        int prc = cloud_push_card1(&cloud, game_dir, c.backup_max, 0, smsg, sizeof(smsg));
                                        _exit(prc == 0 ? 0 : 1);
                                    }
                                } else {
                                    snprintf(last_sha, sizeof(last_sha), "%s", cur_sha);
                                }
                            }
                        }
                        last_sync_ts = tnow;
                    }
                }

                if (sync_pid > 0) {
                    int sst = 0;
                    while (waitpid(sync_pid, &sst, 0) < 0 && errno == EINTR) {}
                    if (WIFEXITED(sst) && WEXITSTATUS(sst) == 0) cloud_state_load(&cloud, game_dir);
                }

                if (launch_fd >= 0) {
                    char buf[1024];
                    ssize_t n;
                    while ((n = read(launch_fd, buf, sizeof(buf))) > 0) {
                        if (game_log) { fwrite(buf, 1, (size_t)n, game_log); fflush(game_log); }
                    }
                    close(launch_fd);
                    launch_fd = -1;
                }

                /* Final local backup rotation + cloud push on game exit */
                if (save_is_valid_mc(card1_p, cur_sha)) {
                    save_backup_rotate(game_dir, "card1.mcd", c.backup_max);
                    if (c.sync_enabled && cloud.registered && !cloud.revoked) {
                        cloud_push_card1(&cloud, game_dir, c.backup_max, 0, smsg, sizeof(smsg));
                        if (game_log && smsg[0]) {
                            fprintf(game_log, "[cloud-sync] exit: %s\n", smsg);
                            fflush(game_log);
                        }
                    }
                }
                if (game_log) { fclose(game_log); game_log = NULL; }
                return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
            }
        }
        last = now;
        if (quit_flag) break;
        draw();
        plat_present();
    }
    plat_release();
    return 0;
}
