/* dw3-launcher: the console-first menu. One canvas, one pad, no widgets: every screen is a short list. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifndef _WIN32
#include <poll.h>
#include <sys/wait.h>
#endif

#include "cfg.h"
#include "migrate.h"
#include "cloud.h"
#include "app_core.h"
#include "compat.h"
#include "i18n.h"
#include "loading.h"
#include "plat.h"
#include "sha256.h"
#include "ui.h"
#include "update.h"
#ifdef DW3_TARGET_PC
#include "selfupdate.h"
#endif
#include "win32.h"

enum {
    S_SPLASH, S_HOME, S_GAME, S_VIDEO, S_HUD, S_CONTROLS, S_SAVES, S_BACKUPS, S_BACKUP_CONFIRM,
    S_CLOUD, S_PAIR_HOST, S_PAIR_ENTER, S_DEVICES, S_REVOKE_CONFIRM, S_CONFLICT, S_CONFLICT_CONFIRM,
    S_DEBUG, S_BROWSE, S_LED, S_LED1, S_LED2, S_RESET, S_CLEAN, S_LOADING, S_DISC, S_ERROR, S_UPDATING, S_UPDATE_ASK
};
enum { HI_PLAY, HI_GAME, HI_VIDEO, HI_HUD, HI_CONTROLS, HI_SAVES, HI_LED, HI_DEBUG, HI_RESET, HI_COUNT };

static Cfg c;
static CloudState cloud;
static int screen = S_SPLASH, sel, capture, quit_flag;
static int cap_btns[2], cap_n; /* the TURBO row's capture: the buttons pressed so far (one, or a chord of two) */
static char msg[160], last_err[160], status[160], exe_dir[512] = ".";
static int launch_pid = -1, launch_pct, launch_started;
#ifdef _WIN32
static W32Proc wproc; /* the game's process, its output and its status pipes (launch_pid 1 while it runs) */
#else
static int launch_fd = -1, status_fd = -1; /* the game's output and its status pipes */
#endif
static long launch_t0, splash_t0 = -1;
static FILE *game_log;
static long game_log_start; /* where this run begins in logs/game.log */
static char launch_stage[64] = "CHECKING THE DISC";

/* The game refused the disc (its "port: fatal: disc:" line, psxstack runtime/disc.c): the check doing its job, not a
 * crash, so S_DISC says what it wants instead of the red COULD NOT START. DISC_BAD_IMAGE: another dump or a patched
 * image ("is not an accepted"); DISC_BAD_FILE: the file could not be opened or read. */
enum { DISC_OK, DISC_BAD_IMAGE, DISC_BAD_FILE };
static int disc_bad;
static int fatal_seen; /* the game printed its fatal line: that stays the reason shown, not the crash report's path after it */

static int disc_fatal_kind(const char *line) {
    if (!strstr(line, "port: fatal: disc:")) return DISC_OK;
    return strstr(line, " is not an accepted ") ? DISC_BAD_IMAGE : DISC_BAD_FILE;
}

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
/* The save conflict screen lists the saves that differ: the cloud's, this device's and, when RecompLauncher started the
 * launcher and changed its mirror (cloud.h), RecompLauncher's. conflict_acts[i] is what row i does. */
enum { CA_CLOUD, CA_LOCAL, CA_RECOMP, CA_CANCEL };
#define TR(s) ui_tr_lookup((s), c.lang) /* for a text that is formatted before it is drawn (draw's own text() translates the rest) */
static char conflict_msg[160], conflict_cloud_info[96], conflict_local_info[96], conflict_recomp_info[96], conflict_recomp_sha[9];
static int conflict_acts[4] = {CA_CLOUD, CA_LOCAL, CA_CANCEL, CA_CANCEL}, conflict_n = 3, conflict_has_cloud = 1;
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
static int game_step(void) { return is_portmaster ? 31 : 27; }

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

#define VIDEO_STEP 25 /* ten rows */
#define CTRL_STEP 31 /* the glyph row, eight buttons and TURBO: ten rows above the footer */
static int row_target_y(int s, int idx) {
    switch (s) {
    case S_HOME: return 92 + idx * home_step() - 4;
    case S_GAME: return 90 + idx * game_step() - 4;
    case S_VIDEO: return 90 + idx * VIDEO_STEP - 4;
    case S_DEBUG: return 92 + idx * 48 - 4;
    case S_SAVES: return 86 + idx * 46 - 4;
    case S_CLOUD: return 206 + idx * 44 - 4;
    case S_CONFLICT: return (conflict_n > 3 ? 186 + idx * 44 : 194 + idx * 52) - 4;
    case S_BACKUPS: return 114 + (idx - bk_scroll) * 38 - 4;
    case S_DEVICES: return 114 + (idx - dev_scroll) * 38 - 4;
    case S_BROWSE: return 114 + (idx - b_scroll) * 36 - 4;
    case S_HUD: return 180 + idx * 23 - 3; /* 23, not 25: the eleventh row (Fight button) clears the footer */
    case S_CONTROLS: return idx == 0 ? 74 : 124 + (idx - 1) * CTRL_STEP;
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
    "Start Digimon World 3 with your saved settings.", "Language, fast-forward, faster loading and skipping.",
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
    case HI_VIDEO: *on = c.widescreen; return c.widescreen ? "AUTO" : "SETTINGS";
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

#ifdef DW3_TARGET_PC
/* The update notice (update.h): this launcher asks GitHub for a newer release at most once in 12 hours. PortMaster has
 * its own updates and the Android launcher its own screen for it. */
static UpdateInfo upd;
static int debug_n(void) {
    update_poll(&upd);
    return upd.state == UPDATE_AVAILABLE ? 5 : 4;
}
#else
static int debug_n(void) { return 3; }
#endif

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
#ifdef DW3_TARGET_PC
    update_poll(&upd);
    if (upd.state == UPDATE_AVAILABLE) {
        char nv[64];
        snprintf(nv, sizeof(nv), "NEW VERSION %s: SEE DEBUG", upd.latest);
        text(44, 394, nv, 14, C_GOLD);
    }
#endif
    char ga[16], gst[16], gsel[16], chord[36];
    glyph(btn_ok(), ga); glyph(btn_start(), gst); glyph(btn_select(), gsel);
    snprintf(chord, sizeof(chord), "%s+%s", gsel, gst);
    const char *k[] = {ga, gst, chord}, *w[] = {"SELECT", "PLAY", "QUIT"};
    footer(k, w, 3);
}

#define VIDEO_N 10
static void draw_video(void) {
    header("DIGIMON WORLD 3", "VIDEO");
    int dw = 1280, dh = 720;
    plat_desktop_size(&dw, &dh);
    int sc = c.scale > 0 ? c.scale : cfg_auto_scale(&c);
    int gw = (c.widescreen && c.renderer_pc == 0 ? 427 : 320) * sc, gh = 240 * sc;
    char sc_val[48], mon[48];
    if (c.scale == 0) snprintf(sc_val, sizeof(sc_val), "AUTO (%dx%d)", gw, gh);
    else snprintf(sc_val, sizeof(sc_val), "%dX (%dx%d)", c.scale, gw, gh);
    char res_val[48];
    app_fmt_vram(&c, res_val, sizeof(res_val));
    const char *v[VIDEO_N] = {
        (c.widescreen && c.renderer_pc == 0) ? "AUTO" : "OFF",
        c.fullscreen ? "ON" : "OFF",
        c.fill ? "FILL" : "INTEGER",
        sc_val,
        c.hz == 60 ? "60 HZ" : "50 HZ",
        c.renderer_pc ? "SOFTWARE" : "GPU",
        c.renderer_pc ? "-" : filt_label[c.filter],
        c.renderer_pc ? "-" : res_val,
        (c.renderer_pc || c.iscale == 1) ? "-" : vert_label[c.subpixel],
        c.fmv_autocrop ? "ON" : "OFF",
    };
    const char *lab[VIDEO_N] = {"WIDESCREEN", "FULLSCREEN", "SCALING", "WINDOW SCALE", "REFRESH RATE", "RENDERER", "FILTER",
                                "RESOLUTION", "3D VERTICES", "FMV AUTOCROP"};
    int gy = (int)(cur_y >= 0 && cur_screen == S_VIDEO ? cur_y : row_target_y(S_VIDEO, sel));
    draw_focus(40, gy, W - 80, VIDEO_STEP + 1, 8, 0);
    for (int i = 0; i < VIDEO_N; i++) {
        /* Green means "changed from the PS1's picture": 1X and the PS1 vertices are the console's own. */
        int on = i == 4 ? c.hz == 60 : i == 7 ? c.iscale > 1 : i == 8 ? c.subpixel > 0 : (strcmp(v[i], "OFF") && strcmp(v[i], "-"));
        row(90 + i * VIDEO_STEP, lab[i], v[i], i == sel, 0, on && strcmp(v[i], "-"), 0);
    }
    static const char *desc[VIDEO_N] = {
        "Battles and field follow the screen's shape, up to 16:9 (auto-selects GPU). Menus stay 4:3.",
        "Open the game in fullscreen (F11 also toggles).",
        "FILL stretches cleanly to screen height; INTEGER uses exact multiples.",
        "Window resolution scale (AUTO fits the detected monitor).",
        "60 HZ is the game's own NTSC mode (smoother). 50 HZ is the original PAL speed.",
        "GPU enables widescreen and shaders; SOFTWARE is exact PS1.",
        "GPU screen filter (SHARP keeps crisp pixels).",
        "Scales full PS1 VRAM (1024x512, 1536x512 in 16:9), not just 320x240.",
        "Above 1X: PS1 wobbles; SMOOTH steadies 3D; + TEXTURES fixes warping.",
        "Trims the black bars off videos while they play (off by default)."
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
        const char *b = path_last_sep(disc_path);
        disc_lbl = b ? b + 1 : disc_path;
    }
    const char *v[8] = {lang_label[c.lang < 0 || c.lang > 6 ? 0 : c.lang], ff_label[c.ff], load_label[c.load],
                        c.skip ? "ON" : "OFF", c.battles ? "ON" : "OFF", c.battles ? (c.hit ? "ON" : "OFF") : "-",
                        bt_label[c.btext < 0 || c.btext > 2 ? 1 : c.btext], disc_lbl};
    const char *lab[8] = {"LANGUAGE", "FAST-FORWARD", "FAST LOAD", "SKIP DIALOGUES", "SKIP BATTLE ANIMS",
                          "   HIT REACTION", "BATTLE TEXT", "DISC IMAGE"};
    int n = game_n(), st = game_step();
    int gy = (int)(cur_y >= 0 && cur_screen == S_GAME ? cur_y : row_target_y(S_GAME, sel));
    draw_focus(40, gy, W - 80, st < 30 ? 28 : 34, 8, 0);
    for (int i = 0; i < n; i++) {
        int on = (i == 7) ? (disc_path[0] != 0) : (strcmp(v[i], "OFF") && strcmp(v[i], "-"));
        row(90 + i * st, lab[i], v[i], i == sel, 0, on, 0);
    }
    static const char *desc[8] = {"Game text language (English, Portuguese BR, French, German, Italian, Spanish).",
        NULL, /* the fast-forward row: its chord, below */
        "Reads the game's files faster. Movies keep normal speed. OFF, 2X, 4X, 8X or INSTANT.",
        "Dialogue boxes close on their own.",
        "Battle moves play without their animation.", "Keep the hit flinch while animations are skipped.",
        "SKIP: battle text hidden. ASYNC: queued in the HUD without pausing.", "Pick or change your .cue / .bin disc file."};
    /* The chord that turns fast-forward on and off in the game (cfg.c writes it as the runtime's toggle binding), in the
     * player's buttons and glyphs; the PC's TAB toggles it too. */
    char ff_desc[160], ffb[48];
    cfg_fmt_ff(&c, ffb, sizeof(ffb));
    if (c.ff_n == 0)
        snprintf(ff_desc, sizeof(ff_desc), "%s", TR(is_pc ? "The speed. No pad button: set TURBO in CONTROLS (TAB still works)."
                                                           : "The speed. No pad button: set TURBO in CONTROLS."));
    else
        snprintf(ff_desc, sizeof(ff_desc), TR(is_pc ? "The speed. In the game, %s or TAB turns it on and off."
                                                    : "The speed. In the game, %s turns it on and off."), ffb);
    rect(40, 346, W - 80, 1, C_EDGE, 256);
    text(44, 358, sel == 1 ? ff_desc : desc[sel], 16, C_NOTE);
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
    const char *rl[HUD_ROWS] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "BAG", "RUN", "INFO", "BLAST", "REMEMBER CURSOR", "BUFFER INPUT",
                                "FIGHT BUTTON"};
    int gy = (int)(cur_y >= 0 && cur_screen == S_HUD ? cur_y : row_target_y(S_HUD, sel));
    int sbd = sel < H_COUNT && (dm >> sel & 1);
    draw_focus(40, gy, W - 80, 25, 6, sbd);
    for (int i = 0; i < HUD_ROWS; i++) {
        char g[16], v[48];
        if (i < H_COUNT) {
            glyph(c.hud[i], g);
            if (capture && i == sel) snprintf(v, sizeof(v), "PRESS A BUTTON...");
            else snprintf(v, sizeof(v), "%s  [%s]", place_display(c.hud[i]), g);
        }
        else if (i == H_COUNT) snprintf(v, sizeof(v), "%s", c.bmem ? "ON" : "OFF");
        else if (i == H_COUNT + 1) snprintf(v, sizeof(v), "%s", buf_label[c.bbuf]);
        else snprintf(v, sizeof(v), "%s", fight_label[cfg_fight_place(&c) + 1]);
        int y = 180 + i * 23, bd = i < H_COUNT && (dm >> i & 1);
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
    int sbd = sel > 0 && sel <= A_COUNT && (dm >> act_order[sel - 1] & 1);
    draw_focus(40, gy, W - 80, CTRL_STEP - 2, 8, sbd);
    snprintf(v, sizeof(v), "< %s >", lay_label[cfg_layout(&c)]);
    row(78, "BUTTON GLYPHS", v, sel == 0, 0, 0, 0);
    rect(60, 120, W - 120, 1, C_EDGE, 256);
    for (int i = 0; i < A_COUNT; i++) {
        int a = act_order[i], y = 128 + i * CTRL_STEP, isel = sel == i + 1, bd = dm >> a & 1;
        char g[16];
        glyph(c.pad[a].code, g);
        if (capture && isel) snprintf(v, sizeof(v), "PRESS A BUTTON...");
        else snprintf(v, sizeof(v), "%s  [%s]", place_display(c.pad[a].code), g);
        row(y, act_label[a], v, isel, bd, 0, 0);
    }
    {
        /* TURBO: the fast-forward key, one button or a chord of two (the runtime's toggle binding). */
        char tb[48];
        int tsel = sel == A_COUNT + 1, on = c.ff_n > 0;
        if (capture && tsel) snprintf(tb, sizeof(tb), "%s", cap_n ? "PRESS A 2ND OR LET GO" : "PRESS 1 OR 2 BUTTONS...");
        else cfg_fmt_ff(&c, tb, sizeof(tb));
        row(128 + A_COUNT * CTRL_STEP, "TURBO", tb, tsel, 0, on && !(capture && tsel), 0);
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
    if (rem <= 0) snprintf(tm, sizeof(tm), "%s", "CODE EXPIRED");
    else snprintf(tm, sizeof(tm), "EXPIRES IN %02d:%02d", rem / 60, rem % 60);
    text_c(W / 2, 212, tm, 15, rem <= 0 ? C_RED : rem < 30 ? C_ORANGE : C_CYAN);

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
        glyph(btn_ok(), ga); glyph(c.pad[A_TRIANGLE].code, gy); glyph(btn_back(), gb);
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
    app_fmt_epoch(ts, TR("unknown time"), out, n);
}

static void go(int s, int to_sel, int dir, int focus_y);
static void start_trans(int dir, int focus_y);

static int cloud_usable(void) { return c.sync_enabled && cloud.registered && !cloud.revoked; }

/* The conflict screen. has_cloud: the cloud's save is one of the choices (a sync found it differs from local). The
 * RecompLauncher one is added when RecompLauncher changed its mirror (recomp_mirror_foreign). */
static void open_conflict_ex(int ret_screen, const char *cmsg, int has_cloud) {
    RecompMirror rm;
    int has_recomp = recomp_active() && recomp_mirror_foreign(game_dir, &rm);
    conflict_return_screen = ret_screen;
    conflict_has_cloud = has_cloud;
    snprintf(conflict_msg, sizeof(conflict_msg), "%s", cmsg ? cmsg : "CLOUD AND LOCAL DIFFER");

    conflict_n = 0;
    if (has_cloud) {
        conflict_acts[conflict_n++] = CA_CLOUD;
        conflict_acts[conflict_n++] = CA_LOCAL;
    }
    if (has_recomp) conflict_acts[conflict_n++] = CA_RECOMP;
    conflict_acts[conflict_n++] = CA_CANCEL;

    char cts[32], lts[32], csha[9] = "--------", lsha[65] = "";
    if (has_cloud) {
        fmt_epoch(cloud.cloud_updated_at, cts, sizeof(cts));
        if (cloud.cloud_sha256[0]) snprintf(csha, sizeof(csha), "%.8s", cloud.cloud_sha256);
        snprintf(conflict_cloud_info, sizeof(conflict_cloud_info), "%s: REV #%d   %s   [%s]",
                 TR("CLOUD"), cloud.cloud_seq, cts, csha);
    } else {
        snprintf(conflict_cloud_info, sizeof(conflict_cloud_info), "%s: %s", TR("CLOUD"), TR("SYNC OFF"));
    }

    char card_p[1024];
    snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
    struct stat st;
    if (stat(card_p, &st) == 0 && save_is_valid_mc(card_p, lsha)) {
        fmt_epoch((long)st.st_mtime, lts, sizeof(lts));
        snprintf(conflict_local_info, sizeof(conflict_local_info), "%s: card1.mcd   %s   [%.8s]", TR("LOCAL"), lts, lsha);
    } else {
        snprintf(conflict_local_info, sizeof(conflict_local_info), "%s: %s", TR("LOCAL"), TR("no valid card1.mcd"));
    }

    conflict_recomp_info[0] = 0;
    conflict_recomp_sha[0] = 0;
    if (has_recomp) {
        char rts[32];
        snprintf(conflict_recomp_sha, sizeof(conflict_recomp_sha), "%.8s", rm.sha256);
        fmt_epoch(rm.written_at, rts, sizeof(rts));
        snprintf(conflict_recomp_info, sizeof(conflict_recomp_info), "RECOMPLAUNCHER: %s   %s   [%.8s]",
                 rm.device_name[0] ? rm.device_name : "?", rts, rm.sha256);
    }

    int fy = ret_screen == S_HOME ? H / 2 : 224 + 14;
    go(S_CONFLICT, 0, 1, fy);
}

static void open_conflict_screen(int ret_screen, const char *cmsg) {
    open_conflict_ex(ret_screen, cmsg, 1);
}

static void draw_conflict(void) {
    int rc = conflict_recomp_info[0] != 0;
    header("DIGIMON WORLD 3", conflict_has_cloud ? "CLOUD CONFLICT" : "SAVE CONFLICT");
    rrect(40, 74, W - 80, 102, 10, C_PANEL, 256);
    rrect_line(40, 74, W - 80, 102, 10, C_GOLD);
    text(58, 84, conflict_has_cloud ? "BOTH CLOUD AND LOCAL SAVES CHANGED" : "THE RECOMPLAUNCHER SAVE DIFFERS FROM LOCAL", 17, C_GOLD);
    text(58, rc ? 106 : 112, conflict_cloud_info, 14, C_CYAN);
    text(58, rc ? 124 : 134, conflict_local_info, 14, C_GREEN);
    if (rc) text(58, 142, conflict_recomp_info, 14, C_ORANGE);
    if (conflict_has_cloud) text(58, rc ? 159 : 154, "A local backup was saved automatically.", 12, C_NOTE);

    int gy = (int)(cur_y >= 0 && cur_screen == S_CONFLICT ? cur_y : row_target_y(S_CONFLICT, sel));
    draw_focus(40, gy, W - 80, 38, 8, 0);
    const char *hint = "";
    for (int i = 0; i < conflict_n; i++) {
        int y = row_target_y(S_CONFLICT, i) + 4;
        char tag[40];
        switch (conflict_acts[i]) {
        case CA_CLOUD:
            snprintf(tag, sizeof(tag), "%s #%d", TR("USE REV"), cloud.cloud_seq);
            row(y, "DOWNLOAD CLOUD SAVE", tag, sel == i, 0, 1, 0);
            if (sel == i) hint = "Replaces card1.mcd on this device with the Cloud save.";
            break;
        case CA_LOCAL:
            row(y, "UPLOAD LOCAL TO CLOUD", "OVERWRITE CLOUD", sel == i, 0, 0, 0);
            if (sel == i) hint = "Overwrites the Cloud save with this device's card1.mcd.";
            break;
        case CA_RECOMP:
            snprintf(tag, sizeof(tag), "[%s]", conflict_recomp_sha);
            row(y, "USE RECOMPLAUNCHER SAVE", tag, sel == i, 0, 0, 0);
            if (sel == i) hint = cloud_usable() ? "Replaces card1.mcd with the RecompLauncher save, and the Cloud too."
                                                : "Replaces card1.mcd with the RecompLauncher save.";
            break;
        default:
            row(y, "CANCEL", "DO NOT SYNC", sel == i, 0, 0, 0);
            if (sel == i) hint = conflict_has_cloud ? "Leaves both untouched. You can sync later in SAVES & CLOUD."
                                                    : "Leaves every save untouched. You will be asked again on PLAY.";
            break;
        }
    }
    rect(40, 368, W - 80, 1, C_EDGE, 256);
    text(44, 380, hint, 15, C_NOTE);
    foot_2("CHOOSE", "BACK");
}

static void draw_conflict_confirm(void) {
    draw_conflict();
    int act = conflict_acts[conflict_choice < conflict_n ? conflict_choice : 0];
    int is_push = (act == CA_LOCAL);
    uint32_t accent = is_push ? C_RED : C_ORANGE;
    const char *title = is_push ? "OVERWRITE CLOUD SAVE?" : act == CA_RECOMP ? "USE RECOMPLAUNCHER SAVE?" : "REPLACE LOCAL SAVE?";
    char l1[96], l2[96], l3[96], ok_lbl[32];

    if (is_push) {
        snprintf(l1, sizeof(l1), TR("Cloud REV #%d will be replaced by this device's"), cloud.cloud_seq);
        snprintf(l2, sizeof(l2), "%s", TR("local card1.mcd. Other devices will pull this"));
        snprintf(l3, sizeof(l3), "%s", TR("save on their next sync."));
    } else if (act == CA_RECOMP) {
        snprintf(l1, sizeof(l1), "%s", TR("This device's card1.mcd will be replaced by"));
        snprintf(l2, sizeof(l2), "%s", TR("the RecompLauncher save. Your current local"));
        snprintf(l3, sizeof(l3), "%s", cloud_usable() ? TR("save is backed up first; the Cloud follows.")
                                                      : TR("save is backed up first in saves_backup/."));
    } else {
        snprintf(l1, sizeof(l1), "%s", TR("This device's card1.mcd will be replaced by"));
        snprintf(l2, sizeof(l2), TR("Cloud REV #%d. Your current local save is"), cloud.cloud_seq);
        snprintf(l3, sizeof(l3), "%s", TR("safely backed up in saves_backup/ first."));
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
        snprintf(ok_lbl, sizeof(ok_lbl), TR("CONFIRM (%d/3)"), conflict_step);
    } else {
        snprintf(ok_lbl, sizeof(ok_lbl), "%s", is_push ? TR("OVERWRITE (3/3)") : TR("REPLACE (3/3)"));
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

/* SEND LOGS: the full zip is saved beside the game as ever, then the capped one goes to the server and is dropped. */
static void send_logs(void) {
    /* A report without the game's log cannot be reproduced: the player turns logs on and plays the bug again first. */
    if (c.logs == 0) {
        snprintf(status, sizeof(status), "%s", APP_LOGS_OFF);
        return;
    }
#ifdef DW3_LAUNCHER_VERSION
    const char *ver = DW3_LAUNCHER_VERSION;
#else
    const char *ver = "";
#endif
    app_send_logs(&cloud, ver, is_portmaster, status, sizeof(status));
}

static void draw_debug(void) {
    header("DIGIMON WORLD 3", "DEBUG");
    int gy = (int)(cur_y >= 0 && cur_screen == S_DEBUG ? cur_y : row_target_y(S_DEBUG, sel));
    draw_focus(40, gy, W - 80, 36, 8, 0);
    row(92, "LOGS", log_label[c.logs], sel == 0, 0, c.logs > 0, 0);
    row(140, "SEND LOGS", "UPLOAD", sel == 1, 0, 0, 0);
    row(188, "CLEAN LOGS", "ASK", sel == 2, 0, 0, 0);
#ifdef DW3_TARGET_PC
    row(236, "CHECK FOR UPDATES", c.update_check ? "ON" : "OFF", sel == 3, 0, c.update_check, 0);
    if (debug_n() == 5) {
        row(284, "NEW VERSION", upd.latest, sel == 4, 0, 1, 0);
        if (sel == 4) text(64, 322, selfupd_available(&upd, exe_dir) ? "PRESS A TO UPDATE" : "PRESS A TO OPEN THE PAGE", 15, C_NOTE);
    }
#endif
    if (status[0]) text(64, 350, status, 16, C_GREEN);
    text(64, 380, "SEND LOGS uploads the logs and your save (RPG data) to the developer.", 15, C_NOTE);
    text(64, 400, "Then open an issue on GitHub and say what happened.", 15, C_NOTE);
#ifdef DW3_LAUNCHER_STAMP
    {
        char bld[96];
        snprintf(bld, sizeof(bld), "BUILD: %s", DW3_LAUNCHER_STAMP);
        text(64, 428, bld, 14, strstr(DW3_LAUNCHER_STAMP, "-dirty") ? C_GOLD : C_NOTE);
    }
#endif
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

#ifdef DW3_TARGET_PC
/* The self-update (selfupdate.h): the runner minigame stays up while the updater works, and when it is ready a notice
 * asks the player to restart; nothing closes on its own. */
static SelfUpdate su;
static int su_on; /* an update was started this run and not cancelled or failed */
static int update_asked; /* PLAY already offered the update this run (or never should: a test run, --play): it asks once */

static void draw_updating(void) {
    char stage[120];
    const char *t = su.state == SU_READY ? "UPDATE READY" : su.text[0] ? su.text : "STARTING";
    snprintf(stage, sizeof(stage), "%s...", TR(t));
    load_draw(&c, plat_now_ms() - launch_t0, stage, su.state == SU_READY ? 100 : su.pct);
    if (su.state == SU_READY) {
        rrect(60, 68, W - 120, 74, 12, C_PANEL, 256);
        rrect_line(60, 68, W - 120, 74, 12, C_GOLD);
        text_c(W / 2, 80, "UPDATE READY", 22, C_GOLD);
        text_c(W / 2, 108, "PRESS START TO RESTART", 17, C_TEXT);
        text_c(W / 2, 128, "SELECT: LATER", 13, C_NOTE);
    } else {
        text_c(W / 2, 464, "SELECT: CANCEL", 13, C_NOTE);
    }
}

/* Starts the update and shows its screen; the reason on the DEBUG screen when it cannot. */
static void start_update(void) {
    char err[96] = "";
    if (su_on && su.state == SU_READY) { start_trans(1, H / 2); screen = S_UPDATING; return; } /* staged earlier: back to its notice */
    if (selfupd_start(&upd, exe_dir, err, sizeof(err)) != 0) { snprintf(status, sizeof(status), "%s", err); return; }
    memset(&su, 0, sizeof(su));
    su_on = 1;
    launch_t0 = plat_now_ms();
    char best[700];
    snprintf(best, sizeof(best), "%s/loading.best", game_dir);
    load_init(best);
    start_trans(1, H / 2);
    screen = S_UPDATING;
    cur_screen = S_UPDATING;
}
#endif

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
    text(76, 210, "Turn LOGS to HEAVY in DEBUG, try again, then SEND LOGS.", 15, C_NOTE);
    foot_std("OK");
}

/* One line of S_DISC; *right keeps the rightmost edge drawn, for the self-test's check that every language fits. */
static int disc_line(int x, int y, const char *s, int size, uint32_t col, int *right) {
    int r = x + text_w(s, size);
    if (r > *right) *right = r;
    text(x, y, s, size, col);
    return y + (size >= 20 ? 38 : 22);
}

/* S_DISC: the game refused the disc. What it plays and where to pick it, in gold and cyan rather than the error's red,
 * so a player with another dump of the right game does not read it as a bug. Returns the rightmost edge drawn. */
static int draw_disc(void) {
    static const char *const image[] = {
        "Nothing broke: the game checks your disc before it starts.",
        "It plays one exact image: the Redump dump of",
        "Digimon World 2003 (Europe), SLES-03936, as .bin + .cue.",
        "Patched or differently ripped images don't pass.",
    };
    const int img = disc_bad != DISC_BAD_FILE, rl = recomp_active();
    const char *next = img ? (rl ? "In RecompLauncher, the game's page marks the right one:"
                              : is_portmaster ? "Put that image in game/ and press PLAY again."
                                              : "Choose that image and press PLAY again.")
                           : (rl ? "Choose the disc again on the game's page in RecompLauncher."
                              : is_portmaster ? "Check the image in game/ and press PLAY again."
                                              : "Choose the disc again and press PLAY.");
    int y = 120, right = 0;
    header("DIGIMON WORLD 3", "CHECK YOUR DISC");
    rrect(50, 104, W - 100, 208, 12, C_PANEL, 256);
    rrect_line(50, 104, W - 100, 208, 12, C_GOLD);
    y = disc_line(76, y, img ? "NOT THE VERIFIED DISC IMAGE" : "THE DISC FILE COULD NOT BE READ", 20, C_GOLD, &right);
    if (img) {
        for (size_t i = 0; i < sizeof(image) / sizeof(image[0]); i++) y = disc_line(76, y, image[i], 14, C_TEXT, &right);
    } else {
        y = disc_line(76, y, "It may have been moved or renamed, or its drive unplugged.", 14, C_TEXT, &right);
    }
    y = disc_line(76, y + 12, next, 14, C_CYAN, &right);
    /* RecompLauncher's own badge, in its words (not in the launcher's table, so never translated) */
    if (img && rl) disc_line(96, y, "\"Matches the verified Europe disc\"", 14, C_GREEN, &right);
    if (!img && last_err[0]) {
        char el[72];
        snprintf(el, sizeof(el), "%.62s%s", last_err, strlen(last_err) > 62 ? "..." : "");
        text(76, y + 8, el, 13, C_NOTE);
    }
    foot_2(rl || is_portmaster ? "OK" : "CHOOSE DISC", "BACK");
    return right;
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
#ifdef DW3_TARGET_PC
    case S_UPDATE_ASK: {
        char nv[96];
        snprintf(nv, sizeof(nv), "%s %.40s", TR("NEW VERSION"), upd.latest);
        draw_home();
        modal("UPDATE AVAILABLE", nv, su_on && su.state == SU_READY ? "Already downloaded: restarts to install it." : "Downloads now, then restarts the launcher.",
              "Saves and settings are never touched.", "YES, UPDATE", "NO, PLAY", C_GOLD);
        break;
    }
    case S_UPDATING: draw_updating(); break;
#endif
    case S_DISC: draw_disc(); break;
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
static const char *game_extra[8]; /* more arguments for the game: the self-test's --max-frames */

static void find_game_bin(char *out, size_t n) {
    const char *env_bin = getenv("DW3_GAME_BIN");
    if (env_bin && env_bin[0] && exe_ok(env_bin)) {
        snprintf(out, n, "%s", env_bin);
        return;
    }
#ifdef _WIN32
    /* The release's folder: dw2003.exe beside the launcher. */
    snprintf(out, n, "%s/dw2003.exe", exe_dir);
    return;
#else
    char cand[1024];
    if (is_pc) {
        /* The release (the AppImage's usr/bin/, or any folder): the game beside the launcher. A checkout's build:
         * launcher/dw3-launcher beside build/. */
        static const char *const rel[] = {"dw2003", "../build/port-sdl/dw2003", "../build/release/port-sdl/dw2003"};
        for (unsigned i = 0; i < sizeof(rel) / sizeof(rel[0]); i++) {
            snprintf(cand, sizeof(cand), "%s/%s", exe_dir, rel[i]);
            if (exe_ok(cand)) { snprintf(out, n, "%s", cand); return; }
        }
    }
#if defined(__aarch64__)
    const char *arch_name = "dw2003.aarch64";
#elif defined(__arm__)
    const char *arch_name = "dw2003.armhf";
#else
    const char *arch_name = "dw2003";
#endif
    snprintf(cand, sizeof(cand), "%s/%s", game_dir, arch_name);
    if (exe_ok(cand)) { snprintf(out, n, "%s", cand); return; }
    snprintf(cand, sizeof(cand), "%s/%s", exe_dir, arch_name);
    if (exe_ok(cand)) { snprintf(out, n, "%s", cand); return; }
    snprintf(cand, sizeof(cand), "%s/dw2003", game_dir);
    if (exe_ok(cand)) { snprintf(out, n, "%s", cand); return; }
    snprintf(cand, sizeof(cand), "%s/dw2003", exe_dir);
    if (exe_ok(cand)) { snprintf(out, n, "%s", cand); return; }
    if (!is_pc) {
        snprintf(cand, sizeof(cand), "%s/build/port/dw2003", game_dir);
        if (exe_ok(cand)) { snprintf(out, n, "%s", cand); return; }
    }
    snprintf(out, n, "./%s", arch_name);
#endif
}

/* The game's command line: settings.json in the settings directory, which is its working directory. */
static void game_argv(const char *bin, const char **argv, int max) {
    const char *fr = getenv("DW3_FORCE_RENDERER");
    int ac = 0;
    argv[ac++] = bin;
    argv[ac++] = "--config";
    argv[ac++] = "./settings.json";
    argv[ac++] = "--window";
    if (!(is_pc && !c.fullscreen)) argv[ac++] = "--fullscreen";
    argv[ac++] = "--watchdog";
    argv[ac++] = "0";
    if (fr && fr[0]) {
        argv[ac++] = "--renderer";
        argv[ac++] = fr;
    }
    for (int i = 0; game_extra[i] && ac < max - 1; i++) argv[ac++] = game_extra[i];
    argv[ac] = NULL;
}

/* ---- the running game, as each system has it: its output, its loading status and its end ---- */
/* What a pipe of the game has now, without waiting: >0 bytes, 0 nothing yet, -1 closed. */
static long game_read(int status, char *buf, size_t n) {
#ifdef _WIN32
    return w32_read(status ? &wproc.status : &wproc.out, buf, n);
#else
    int *fd = status ? &status_fd : &launch_fd;
    if (*fd < 0) return -1;
    for (;;) {
        ssize_t r = read(*fd, buf, n);
        if (r > 0) return (long)r;
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        close(*fd);
        *fd = -1;
        return -1;
    }
#endif
}

static void game_close_status(void) {
#ifdef _WIN32
    char buf[64];
    while (w32_read(&wproc.status, buf, sizeof(buf)) > 0) {}
    w32_close_pipe(&wproc.status);
#else
    if (status_fd >= 0) { close(status_fd); status_fd = -1; }
#endif
}

static void game_close(void) {
#ifdef _WIN32
    w32_proc_close(&wproc);
#else
    if (status_fd >= 0) { close(status_fd); status_fd = -1; }
    if (launch_fd >= 0) { close(launch_fd); launch_fd = -1; }
#endif
}

/* Waits up to ms for the game's output or its end. */
static void game_wait(int ms) {
#ifdef _WIN32
    w32_wait(&wproc, ms);
#else
    if (launch_fd >= 0) {
        struct pollfd pfd = {launch_fd, POLLIN, 0};
        poll(&pfd, 1, ms);
    } else {
        sleep(1);
    }
#endif
}

/* 1 once the game has ended: *code is the launcher's own exit status then (0 when the game ended normally), and why
 * says what went wrong otherwise. */
static int game_reap(int *code, char *why, size_t n) {
#ifdef _WIN32
    unsigned long st = 1;
    if (!w32_exited(&wproc, &st)) return 0;
    *code = (int)st;
    /* an exception's code (0xC0000005: an access violation), not a status the game chose */
    if (st != 0) snprintf(why, n, st >= 0xC0000000UL ? "THE GAME CRASHED (0X%08lX)" : "THE GAME EXITED WITH CODE %lu", st);
    return 1;
#else
    int st;
    if (launch_pid <= 0 || waitpid(launch_pid, &st, WNOHANG) != launch_pid) return 0;
    *code = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
    if (*code != 0) {
        snprintf(why, n, WIFSIGNALED(st) ? "THE GAME CRASHED (SIGNAL %d)" : "THE GAME EXITED WITH CODE %d",
                 WIFSIGNALED(st) ? WTERMSIG(st) : WEXITSTATUS(st));
    }
    return 1;
#endif
}

#define CRASH_SUMMARY_FRAMES 16

/* A report line's value: copied into out when the line starts with key and out is still empty. */
static void crash_field(const char *line, const char *key, char *out, size_t n) {
    size_t k = strlen(key);
    if (!out[0] && !strncmp(line, key, k)) snprintf(out, n, "%.*s", (int)(n - 1), line + k);
}

/* The crash report (psxstack runtime/crash.c) as one line for game.log: a player's issue often carries only the log's
 * last lines (RecompLauncher writes it so), which named the report's file and nothing in it. One line, so a short
 * excerpt keeps all of it; scripts/symbolize.py reads it. out is left empty when the report has no reason or address.
 * The report's own copy of the log is indented, so it matches no key here. */
static void crash_summary(const char *report, char *out, size_t n) {
    char kind[16] = "", build[96] = "", why[96] = "", fault[24] = "", access[12] = "", stage[12] = "", map[12] = "",
         pc[32] = "", stack[CRASH_SUMMARY_FRAMES * 24] = "";
    int in_stack = 0, frames = 0;
    out[0] = 0;
    for (const char *p = report; *p;) {
        char line[256];
        size_t len = strcspn(p, "\r\n");
        snprintf(line, sizeof(line), "%.*s", (int)(len < sizeof(line) ? len : sizeof(line) - 1), p);
        p += len;
        p += strspn(p, "\r\n");
        crash_field(line, "kind: ", kind, sizeof(kind));
        crash_field(line, "build: ", build, sizeof(build));
        crash_field(line, "exception: ", why, sizeof(why));
        crash_field(line, "signal: ", why, sizeof(why));
        crash_field(line, "reason: ", why, sizeof(why));
        crash_field(line, "fault address: ", fault, sizeof(fault));
        crash_field(line, "access: ", access, sizeof(access));
        crash_field(line, "stage: ", stage, sizeof(stage));
        crash_field(line, "map: ", map, sizeof(map));
        crash_field(line, "pc: ", pc, sizeof(pc));
        if (!strncmp(line, "stack (", 7)) in_stack = 1;
        else if (in_stack && !strncmp(line, "  #", 3) && frames < CRASH_SUMMARY_FRAMES) {
            const char *a = strchr(line + 3, ' ');
            size_t used = strlen(stack);
            if (!a) continue;
            snprintf(stack + used, sizeof(stack) - used, "%s%.*s", frames ? "," : "", (int)strcspn(a + 1, " "), a + 1);
            frames++;
        }
    }
    if (!why[0] && !pc[0] && !frames) return;
    const struct { const char *name, *value; } part[] = {
        {"kind=", kind}, {"build=", build}, {"", why}, {"access=", access}, {"fault=", fault},
        {"stage=", stage}, {"map=", map}, {"pc=", pc}, {"stack=", stack},
    };
    snprintf(out, n, "[launcher] crash:");
    for (size_t i = 0; i < sizeof(part) / sizeof(part[0]); i++) {
        size_t used = strlen(out);
        const char *sep = out[used - 1] == ':' ? " " : " | ";
        if (part[i].value[0]) snprintf(out + used, n - used, "%s%s%s", sep, part[i].name, part[i].value);
    }
}

/* Appends the summary of the report this run named ("port: crash report: PATH") to the open game.log. */
static void log_crash_summary(void) {
    static const char head[] = "port: crash report: ";
    static char report[32 * 1024];
    char path[1700], tail[8192 + 1], line[1024];
    fflush(game_log);
    snprintf(path, sizeof(path), "%s/logs/game.log", game_dir);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long from = ftell(f) - (long)(sizeof(tail) - 1);
    if (from < game_log_start) from = game_log_start; /* never an earlier run's crash */
    fseek(f, from, SEEK_SET);
    tail[fread(tail, 1, sizeof(tail) - 1, f)] = 0;
    fclose(f);
    const char *hit = NULL;
    for (const char *p = tail; (p = strstr(p, head)) != NULL; p++) hit = p + sizeof(head) - 1;
    if (!hit) return;
    int len = (int)strcspn(hit, "\r\n");
    int absolute = hit[0] == '/' || hit[0] == '\\' || (hit[0] && hit[1] == ':');
    snprintf(path, sizeof(path), "%s%s%.*s", absolute ? "" : game_dir, absolute ? "" : "/", len, hit);
    f = fopen(path, "rb");
    if (!f) return;
    report[fread(report, 1, sizeof(report) - 1, f)] = 0;
    fclose(f);
    crash_summary(report, line, sizeof(line));
    if (line[0]) fprintf(game_log, "%s\n", line);
}

/* The ended game's last output into game.log, then what went wrong; closes the game's channels (the log stays open). */
static void game_log_end(int code, const char *why) {
    char buf[1024];
    long n;
    while ((n = game_read(0, buf, sizeof(buf))) > 0) {
        if (game_log) fwrite(buf, 1, (size_t)n, game_log);
    }
    game_close();
    if (!game_log) return;
    if (code != 0) {
        log_crash_summary();
        fprintf(game_log, "[launcher] %s\n", why);
    }
    fflush(game_log);
}

/* The periodic cloud push while the game runs (it only pushes: the running game's card is never overwritten): a child
 * process on Linux, a thread on Windows (a copy of the cloud state; the launcher reloads the file afterwards). */
#ifdef _WIN32
static CloudState sync_cloud;
static void *sync_thread;

static int sync_push(void *arg) {
    char smsg[128];
    (void)arg;
    return cloud_push_card1(&sync_cloud, game_dir, c.backup_max, 0, smsg, sizeof(smsg)) == 0 ? 0 : 1;
}
#else
static pid_t sync_pid = -1;
#endif

static int sync_start(void) {
#ifdef _WIN32
    sync_cloud = cloud;
    sync_thread = w32_thread(sync_push, NULL);
    return sync_thread ? 0 : -1;
#else
    sync_pid = fork();
    if (sync_pid == 0) {
        char smsg[128];
        int prc = cloud_push_card1(&cloud, game_dir, c.backup_max, 0, smsg, sizeof(smsg));
        _exit(prc == 0 ? 0 : 1);
    }
    return sync_pid > 0 ? 0 : -1;
#endif
}

/* 1 when the push has ended (*ok: it succeeded), 0 while it runs; `wait` waits for it. */
static int sync_end(int wait, int *ok) {
#ifdef _WIN32
    int rc = 1;
    if (!sync_thread || !w32_thread_end(sync_thread, wait, &rc)) return 0;
    sync_thread = NULL;
    *ok = rc == 0;
    return 1;
#else
    int sst = 0;
    pid_t r;
    if (sync_pid <= 0) return 0;
    while ((r = waitpid(sync_pid, &sst, wait ? 0 : WNOHANG)) < 0 && errno == EINTR) {}
    if (r != sync_pid) return 0;
    sync_pid = -1;
    *ok = WIFEXITED(sst) && WEXITSTATUS(sst) == 0;
    return 1;
#endif
}

static void browse_load(const char *dir) {
    char resolved[1200];
    if (!realpath(dir, resolved)) snprintf(resolved, sizeof(resolved), "%s", dir); /* dir may be b_dir itself */
    memcpy(b_dir, resolved, sizeof(b_dir));
    b_count = browse_read(b_dir, b_items, BROWSE_MAX);
    sel = 0;
    b_scroll = 0;
    cur_y = (float)row_target_y(S_BROWSE, 0);
}

static void start_game(void) {
#ifdef DW3_TARGET_PC
    /* First of everything PLAY does, so that the one window at a time holds: the update's question comes before the
     * save sync, whose conflict screen would otherwise be on top of it. NO comes back here and goes on to the sync. */
    if (!update_asked) {
        update_poll(&upd);
        /* a staged update (READY, "later") is offered again as well: the old launcher plays, the update waits for it */
        if ((su_on && su.state == SU_READY) || (upd.state == UPDATE_AVAILABLE && selfupd_available(&upd, exe_dir))) {
            update_asked = 1;
            start_trans(1, H / 2);
            screen = S_UPDATE_ASK;
            return;
        }
    }
#endif
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
    disc_bad = DISC_OK;
    fatal_seen = 0;
    cfg_save(&c);
    {
        char card_p[1024], smsg[128] = "";
        snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
        if (save_is_valid_mc(card_p, NULL)) {
            save_backup_rotate(game_dir, "card1.mcd", c.backup_max);
        }
        int synced = 0;
        if (c.sync_enabled && cloud.registered && !cloud.revoked) {
            int conf = 0;
            int src = cloud_sync_card1(&cloud, game_dir, c.backup_max, 1, &conf, smsg, sizeof(smsg));
            if (src == 1 && conf) {
                open_conflict_screen(S_HOME, smsg);
                return;
            }
            synced = src == 0;
        }
        /* After the sync: a v2 file the pull just brought takes the current schema too, and the sync state follows the
         * migrated bytes (cloud_migrate_saves), so the migration is not a local change to the cloud. */
        cloud_migrate_saves(&cloud, game_dir);
        /* RecompLauncher's mirror (cloud.h): a changed one asks first; else it follows card1.mcd */
        if (recomp_active()) {
            if (recomp_mirror_foreign(game_dir, NULL)) {
                open_conflict_ex(S_HOME, NULL, synced);
                return;
            }
            recomp_mirror_write(game_dir, cloud.device_id, cloud.device_name, cloud.last_seq, 0);
        }
    }
    /* The game reads the player's pad map itself, and the log level
     * is the DEBUG screen's choice (the game gets "off" unless it was asked for more). */
    if (is_portmaster) setenv("DW3_PAD_REMAP", "1", 1);
    plat_close_pad(); /* the game opens the same pad in its own process */
    setenv("DW3_LOG", c.logs == 2 ? "heavy" : c.logs == 1 ? "soft" : "off", 1);
    if (getenv("DW3_NOEXEC")) { /* test hook: the loading screen without a game */
        launch_pid = -2;
    } else {
        char bin_path[1024];
        const char *argv[24];
        find_game_bin(bin_path, sizeof(bin_path));
        game_argv(bin_path, argv, 24);
#ifdef _WIN32
        /* The status descriptor's number in the game: w32_spawn hands it over as the CRT does (lpReserved2). */
        setenv("DW3_STATUS_FD", "3", 1);
        if (w32_spawn(&wproc, argv, game_dir, 3) != 0) {
            snprintf(msg, sizeof(msg), "THE GAME DID NOT START");
            snprintf(last_err, sizeof(last_err), "%.159s", bin_path);
            screen = S_ERROR;
            return;
        }
        launch_pid = 1;
#else
        int p[2], sp[2];
        if (pipe(p) != 0) return;
        if (pipe(sp) != 0) { close(p[0]); close(p[1]); return; }
        argv[0] = "dw2003";
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
            if (ex) execl("/bin/sh", "sh", "-c", ex, (char *)NULL);
            else execv(bin_path, (char *const *)argv);
            _exit(127);
        }
        close(p[1]);
        close(sp[1]);
        launch_fd = p[0];
        status_fd = sp[0];
        fcntl(launch_fd, F_SETFL, O_NONBLOCK);
        fcntl(status_fd, F_SETFL, O_NONBLOCK);
#endif
        char lp[700];
        snprintf(lp, sizeof(lp), "%s/logs", game_dir);
        mkdir(lp, 0755);
        snprintf(lp, sizeof(lp), "%s/logs/game.log", game_dir);
        game_log = fopen(lp, "a");
        if (game_log) {
            fseek(game_log, 0, SEEK_END);
            game_log_start = ftell(game_log);
        }
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

static int status_said_start; /* the runtime's own "start" arrived (the self-test checks the status channel) */

/* One line of the game's output: the loading screen's stage, the error screen's reason and a refused disc. */
static void game_output_line(const char *line) {
    if (line[0] && !fatal_seen) snprintf(last_err, sizeof(last_err), "%.159s", line);
    if (strstr(line, "port: fatal:")) {
        fatal_seen = 1;
        if (disc_fatal_kind(line) != DISC_OK) disc_bad = disc_fatal_kind(line);
    }
    if (strstr(line, "port: disc")) snprintf(launch_stage, sizeof(launch_stage), "VERIFYING THE DISC");
    if (strstr(line, "port: start:")) { launch_started = 1; snprintf(launch_stage, sizeof(launch_stage), "STARTING"); }
}

/* Reads what the game prints: the status channel tells us even when DW3_LOG=off. */
static void pump_game(void) {
    char buf[512];
    static char line[512], sline[64];
    static int ln, sln;
    long n;
    while ((n = game_read(1, buf, sizeof(buf))) > 0) {
        for (long i = 0; i < n; i++) {
            if (buf[i] == '\n' || sln == (int)sizeof(sline) - 1) {
                sline[sln] = 0;
                sln = 0;
                if (!strcmp(sline, "disc")) snprintf(launch_stage, sizeof(launch_stage), "VERIFYING THE DISC");
                if (!strcmp(sline, "start")) {
                    launch_started = status_said_start = 1;
                    snprintf(launch_stage, sizeof(launch_stage), "STARTING");
                }
            } else sline[sln++] = buf[i];
        }
    }
    while ((n = game_read(0, buf, sizeof(buf))) > 0) {
        if (game_log) { fwrite(buf, 1, (size_t)n, game_log); fflush(game_log); }
        for (long i = 0; i < n; i++) {
            if (buf[i] == '\n' || ln == (int)sizeof(line) - 1) {
                line[ln] = 0;
                ln = 0;
                game_output_line(line);
            } else line[ln++] = buf[i];
        }
    }
    long t = plat_now_ms() - launch_t0;
    int target = launch_started ? 100 : (int)(90 - 90 * 20000 / (20000 + t)); /* creeps towards 90 until the game says so */
    if (target > launch_pct) launch_pct = target;
#ifdef _WIN32
    /* the game's window is up: the hand-over, whatever the status channel said */
    if (launch_pid > 0 && !launch_started && w32_window_up(&wproc)) {
        launch_started = 1;
        snprintf(launch_stage, sizeof(launch_stage), "STARTING");
    }
#endif
    int code = 1;
    char why[160] = "";
    if (launch_pid > 0 && game_reap(&code, why, sizeof(why))) {
        launch_pid = -1;
        ln = sln = 0;
        game_log_end(code, why);
        if (game_log) { fclose(game_log); game_log = NULL; }
        if (code == 0) {
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
        snprintf(msg, sizeof(msg), "%s", why);
        screen = disc_bad != DISC_OK ? S_DISC : S_ERROR;
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
#ifdef DW3_TARGET_PC
    if (screen == S_UPDATING) {
        if (su.state == SU_READY && is_start) { selfupd_go(); quit_flag = 1; return; } /* the updater swaps once this ends */
        /* SELECT, not the back button: the minigame's three jump buttons include the east one, which is back's */
        if (is_select) {
            if (su.state != SU_READY) { selfupd_cancel(); su_on = 0; } /* a staged update keeps waiting; SELECT only hides it */
            update_asked = 0; /* later or cancelled: PLAY asks again */
            go(S_DEBUG, 4, -1, 284 + 14);
            return;
        }
        load_press(code);
        return;
    }
#endif
    if (is_select) held_select = 1;
    if (is_start) held_start = 1;
    if (held_select && held_start && !capture) { quit_flag = 2; return; } /* leaves without saving the edit in progress */
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
    if (screen == S_CONTROLS && capture && sel == A_COUNT + 1) {
        /* TURBO: the first button waits for a second (a chord) until it is let go; two make the chord at once. */
        static const int tvalid[] = {K_SOUTH, K_EAST, K_WEST, K_NORTH, K_L1, K_R1, K_L2, K_R2, K_START, K_SELECT, K_L3, K_R3};
        if (code == K_UP || code == K_DOWN) { capture = 0; return; }
        for (unsigned i = 0; i < sizeof(tvalid) / sizeof(tvalid[0]); i++) {
            if (tvalid[i] != code) continue;
            if (cap_n == 1 && cap_btns[0] == code) return;
            cap_btns[cap_n++] = code;
            if (cap_n == 2) {
                c.ff_n = 2;
                c.ff_btns[0] = cap_btns[0];
                c.ff_btns[1] = cap_btns[1];
                capture = 0;
            }
            return;
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
    case S_HUD: n = HUD_ROWS; break;
    case S_CONTROLS: n = 1 + A_COUNT + 1; break;
    case S_SAVES: n = 6; break;
    case S_BACKUPS: n = bk_count; break;
    case S_CLOUD: n = cloud_n(); break;
    case S_CONFLICT: n = conflict_n; break;
    case S_DEVICES: n = dev_count; break;
    case S_DEBUG: n = debug_n(); break;
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
            case 0: c.lang = (c.lang + s + 7) % 7; break;
            case 1: c.ff = (c.ff + s + 5) % 5; break;
            case 2: c.load = (c.load + s + 5) % 5; break;
            case 3: c.skip = !c.skip; break;
            case 4: c.battles = !c.battles; if (!c.battles) c.hit = 0; break;
            case 5: if (c.battles) c.hit = !c.hit; break;
            case 6: c.btext = c.btext == 2 ? 1 : 2; break; /* SKIP <-> ASYNC: the original boxes are gone */
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
            case 4: c.hz = c.hz == 60 ? 50 : 60; break;
            case 5: c.renderer_pc = !c.renderer_pc; if (c.renderer_pc) c.widescreen = 0; break;
            case 6: if (!c.renderer_pc) c.filter = (c.filter + s + 5) % 5; break;
            case 7: if (!c.renderer_pc) c.iscale = (c.iscale - 1 + s + 8) % 8 + 1; break;
            case 8: if (!c.renderer_pc && c.iscale > 1) c.subpixel = (c.subpixel + s + 3) % 3; break;
            case 9: c.fmv_autocrop = !c.fmv_autocrop; break;
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
        else if (sel == HUD_FIGHT_ROW && (ok || d)) cfg_set_fight_place(&c, cfg_fight_place(&c) != 1);
        break;
    case S_CONTROLS:
        if (back) leave_edit(HI_CONTROLS);
        if (is_select) {
            Cfg def;
            cfg_defaults(&def);
            memcpy(c.pad, def.pad, sizeof(c.pad));
            c.ff_n = def.ff_n;
            memcpy(c.ff_btns, def.ff_btns, sizeof(c.ff_btns));
            return;
        }
        if (sel == A_COUNT + 1) {
            if (is_start) { capture = 1; cap_n = 0; }
            else if (ok || d) cfg_ff_cycle(&c, d ? s : 1);
        } else if (sel == 0 && (ok || d)) {
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
            } else if (code == c.pad[A_TRIANGLE].code) {
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
        if (back || (ok && conflict_acts[sel < conflict_n ? sel : conflict_n - 1] == CA_CANCEL)) {
            int rsel = conflict_return_screen == S_SAVES ? 3 : conflict_return_screen == S_CLOUD ? 1 : 0;
            go(conflict_return_screen, rsel, -1, H / 2);
        } else if (ok) {
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
                int act = conflict_acts[conflict_choice < conflict_n ? conflict_choice : 0];
                if (act == CA_CLOUD) {
                    cloud_pull_card1(&cloud, game_dir, c.backup_max, res_msg, sizeof(res_msg));
                } else if (act == CA_LOCAL) {
                    cloud_push_card1(&cloud, game_dir, c.backup_max, 1, res_msg, sizeof(res_msg));
                } else if (act == CA_RECOMP) {
                    /* RecompLauncher's save becomes card1.mcd and, with the cloud on, the cloud's too */
                    if (recomp_mirror_adopt(game_dir, c.backup_max, res_msg, sizeof(res_msg)) == 0 && cloud_usable()) {
                        char pmsg[160] = "";
                        if (cloud_push_card1(&cloud, game_dir, c.backup_max, 1, pmsg, sizeof(pmsg)) != 0) {
                            snprintf(res_msg, sizeof(res_msg), "%s", pmsg);
                        }
                    }
                }
                /* whichever was chosen, the mirror follows card1.mcd now (the changed one has been answered) */
                if (recomp_active()) recomp_mirror_write(game_dir, cloud.device_id, cloud.device_name, cloud.last_seq, 1);
                int rsel = conflict_return_screen == S_SAVES ? 3 : conflict_return_screen == S_CLOUD ? 1 : 0;
                go(conflict_return_screen, rsel, -1, H / 2);
                snprintf(status, sizeof(status), "%s", res_msg);
            }
        }
        break;
    case S_BROWSE:
        if (back) {
            if (!path_is_root(b_dir) && b_count > 0 && strcmp(b_items[0].name, "..") == 0) {
                char next[1400];
                snprintf(next, sizeof(next), "%s/..", b_dir);
                browse_load(next);
            } else {
                go(S_GAME, 7, -1, 90 + 7 * game_step() + 14);
            }
        } else if (code == c.pad[A_TRIANGLE].code && is_pc) {
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
            send_logs();
        }
        if (sel == 2 && ok) { start_trans(1, 188 + 14); screen = S_CLEAN; }
#ifdef DW3_TARGET_PC
        if (sel == 3 && (ok || d)) {
            c.update_check = !c.update_check;
            cfg_save(&c);
            update_check_start(game_dir, DW3_LAUNCHER_VERSION, c.update_check);
        }
        if (sel == 4 && ok) {
            update_poll(&upd);
            if (upd.state == UPDATE_AVAILABLE && selfupd_available(&upd, exe_dir)) start_update();
            else if (upd.state == UPDATE_AVAILABLE && plat_open_url(upd.url) != 0) snprintf(status, sizeof(status), "OPEN %s", upd.url);
        }
#endif
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
#ifdef DW3_TARGET_PC
    case S_UPDATE_ASK:
        if (ok) start_update(); /* on its screen, or (cannot start: the reason is on DEBUG) on to PLAY */
        if (back || (ok && screen == S_UPDATE_ASK)) { screen = S_HOME; cur_screen = S_HOME; start_game(); }
        break;
#endif
    case S_RESET:
        if (back) { start_trans(-1, 92 + home_index(HI_RESET) * home_step() + 14); screen = S_HOME; }
        if (ok) { app_reset_cfg(&c); go(S_HOME, 0, -1, 92 + 14); }
        break;
    case S_CLEAN:
        if (back) { start_trans(-1, 188 + 14); screen = S_DEBUG; }
        if (ok) { snprintf(status, sizeof(status), "REMOVED %d FILES", logs_clean()); start_trans(-1, 188 + 14); screen = S_DEBUG; }
        break;
    case S_DISC:
        /* RecompLauncher writes the disc into settings.json on every start, and PortMaster's is game/: picking one
         * here only on a PC run of its own */
        if (ok && !recomp_active() && !is_portmaster) {
            browse_start_dir(b_dir, sizeof(b_dir));
            browse_load(b_dir);
            start_trans(1, H / 2);
            screen = S_BROWSE;
            cur_screen = S_BROWSE;
        } else if (ok || back) {
            go(S_HOME, 0, -1, H / 2);
        }
        break;
    case S_ERROR:
        if (ok || back) go(S_HOME, 0, -1, H / 2);
        break;
    }
}

#ifdef DW3_TARGET_PC
/* The settings directory on PC, one rule: portable.txt beside the launcher makes it the launcher's own folder; else
 * the per-user directory (%APPDATA%\dw2003, ~/.local/share/dw2003), the one the game and RecompLauncher use. Never
 * the current directory nor the program's (an AppImage is read-only). */
static int settings_dir_for(const char *exe, char *out, size_t n) {
    char p[1200];
    snprintf(p, sizeof(p), "%s/portable.txt", exe);
    if (access(p, F_OK) == 0) {
        snprintf(out, n, "%s", exe);
        return 0;
    }
    return plat_pref_dir(out, n);
}
#endif

/* ---- the self-test (--self-test DIR): the release packages run it, under Wine for Windows ---- */
static int st_checks, st_fails;

static void check(int ok, const char *what) {
    st_checks++;
    if (!ok) st_fails++;
    fprintf(stderr, "self-test: %s %s\n", ok ? "ok    " : "FAILED", what);
}

static int write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fputs(text, f);
    return fclose(f);
}

static char *read_text(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *b = malloc(1 << 16);
    size_t r = b ? fread(b, 1, (1 << 16) - 1, f) : 0;
    fclose(f);
    if (b) b[r] = 0;
    return b;
}

static int replace_in_file(const char *path, const char *from, const char *to) {
    char *t = read_text(path);
    char *at = t ? strstr(t, from) : NULL;
    if (!at) { free(t); return -1; }
    size_t len = strlen(t) + strlen(to) + 1;
    char *o = malloc(len);
    if (!o) { free(t); return -1; }
    snprintf(o, len, "%.*s%s%s", (int)(at - t), t, to, at + strlen(from));
    int rc = write_text(path, o);
    free(t);
    free(o);
    return rc;
}

static int count_mods(const char *dir) {
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    while (d && (e = readdir(d))) {
        char p[1400];
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof(p), "%s/%s/mod.json", dir, e->d_name);
        n += access(p, F_OK) == 0;
    }
    if (d) closedir(d);
    return n;
}

/* Pumps the game started from PLAY until it ends (two minutes at most). */
static void selftest_wait_game(void) {
    for (int i = 0; i < 120000 / 10 && launch_pid > 0; i++) {
        pump_game();
        if (launch_pid > 0) plat_poll(&(Ev){0}, 10);
#ifdef _WIN32
        if (launch_pid > 0) w32_wait(&wproc, 10);
#else
        if (launch_pid > 0) poll(NULL, 0, 10);
#endif
    }
}

static int self_test(const char *root) {
    char p[1200], q[1200], what[1400];
    mkdir(root, 0755);
#ifdef DW3_TARGET_PC
    /* The settings directory's rule. */
    char exe[1200], got[1200], pref[1200] = "";
    snprintf(exe, sizeof(exe), "%s/exe", root);
    mkdir(exe, 0755);
    snprintf(p, sizeof(p), "%s/portable.txt", exe);
    remove(p);
    check(settings_dir_for(exe, got, sizeof(got)) == 0 && plat_pref_dir(pref, sizeof(pref)) == 0 && !strcmp(got, pref),
          "settings directory: the per-user one without portable.txt");
    write_text(p, "");
    check(settings_dir_for(exe, got, sizeof(got)) == 0 && !strcmp(got, exe),
          "settings directory: the launcher's own folder with portable.txt");
    check(update_is_newer("v0.8.2", "0.8.1") && !update_is_newer("v0.8.1", "0.8.1") && !update_is_newer("v0.8.0", "0.8.1") &&
              !update_is_newer("v0.8.2", "0.8.1-3-gabcdef1") && !update_is_newer("v0.8.2", "dev"),
          "update notice: a newer release is told; an equal, older or non-release build is not");
#endif
    /* The files: every setting off its default, saved, read back the same. */
    snprintf(game_dir, sizeof(game_dir), "%s/settings", root);
    mkdir(game_dir, 0755);
    snprintf(p, sizeof(p), "%s/game", game_dir);
    mkdir(p, 0755);
    snprintf(p, sizeof(p), "%s/game/dw2003.cue", game_dir);
    write_text(p, "FILE \"dw2003.bin\" BINARY\n");
    snprintf(disc_path, sizeof(disc_path), "%s", p);
    Cfg a, b;
    cfg_defaults(&a);
    a.lang = 1; a.ff = 2; a.load = 4; a.skip = 1; a.battles = 1; a.hit = 1; a.bmem = 0; a.bbuf = 2;
    a.logs = 1; a.fullscreen = !a.fullscreen; a.fill = !a.fill; a.scale = 4; a.filter = 3; a.backup_max = 42;
    a.sync_enabled = 0; a.sync_interval_min = 30; a.hud[H_FIGHT] = K_R2; a.hud[H_RUN] = K_L1;
    a.pad[A_START].code = K_SELECT; a.pad[A_SELECT].code = K_START;
    check(cfg_save(&a) == 0, "settings: tui.state, pad.map and settings.json written");
    disc_path[0] = 0;
    cfg_load(&b);
    check(!memcmp(&a, &b, sizeof(a)), "settings: read back as written");
    check(!strcmp(disc_path, p), "settings: the disc read back");
    {
        /* SEND LOGS's zip: a crash report beside the settings and one in crashes/ (where the game writes when
         * RecompLauncher started it) both go in. The zip is stored, so the names are in its bytes. */
        char zn[128], zp[1400], cd[1200];
        snprintf(cd, sizeof(cd), "%s/crashes", game_dir);
        mkdir(cd, 0755);
        snprintf(q, sizeof(q), "%s/crash-20260101-000000.txt", game_dir);
        write_text(q, "dw2003 crash report\n");
        snprintf(q, sizeof(q), "%s/crash-20260102-000000.txt", cd);
        write_text(q, "dw2003 crash report\n");
        int found = 0;
        if (logs_save(zn, sizeof(zn)) == 0) {
            snprintf(zp, sizeof(zp), "%s/%s", game_dir, zn);
            FILE *zf = fopen(zp, "rb");
            static char zb[1 << 20];
            size_t zl = zf ? fread(zb, 1, sizeof(zb), zf) : 0;
            if (zf) fclose(zf);
            const char *want[] = {"crashes/crash-20260101-000000.txt", "crashes/crash-20260102-000000.txt"};
            for (int k = 0; k < 2; k++)
                for (size_t o = 0; o + strlen(want[k]) <= zl; o++)
                    if (!memcmp(zb + o, want[k], strlen(want[k]))) { found++; break; }
            remove(zp);
        }
        check(found == 2, "send logs: the crash reports beside the settings and in crashes/ are in the zip");
    }
    {
        /* The Fight button: Fight at the bottom by default; the row swaps it with Tag, and a button another
         * action held moves to the row it left; an old tui.state still on the old defaults takes the new one. */
        Cfg f, g;
        char tp[1200];
        cfg_defaults(&f);
        check(cfg_fight_place(&f) == 0 && f.hud[H_FIGHT] == K_SOUTH && f.hud[H_TAG] == K_EAST,
              "fight button: Fight bottom, Tag right by default");
        cfg_set_fight_place(&f, 1);
        check(cfg_fight_place(&f) == 1 && !dup_hud(&f, NULL), "fight button: RIGHT puts Fight right and Tag at the bottom");
        f.hud[H_RUN] = K_SOUTH; f.hud[H_TAG] = K_WEST;
        cfg_set_fight_place(&f, 0);
        check(cfg_fight_place(&f) == 0 && f.hud[H_RUN] == K_WEST && !dup_hud(&f, NULL),
              "fight button: BOTTOM from a custom layout leaves no button on two rows");
        cfg_defaults(&f);
        cfg_set_fight_place(&f, 1);
        cfg_save(&f);
        cfg_load(&g);
        check(cfg_fight_place(&g) == 1, "fight button: RIGHT, saved, is read back as RIGHT");
        snprintf(tp, sizeof(tp), "%s/tui.state", game_dir);
        write_text(tp, "hz=50\nbm_fight=east\nbm_tag=south\n");
        cfg_load(&g);
        check(cfg_fight_place(&g) == 0, "fight button: a tui.state from before, on the old defaults, takes the bottom");
        write_text(tp, "hz=50\nbm_fight=west\nbm_run=east\nbm_tag=south\n");
        cfg_load(&g);
        check(g.hud[H_FIGHT] == K_WEST && g.hud[H_RUN] == K_EAST, "fight button: a layout of the player's own is kept");
        /* Schema migrations (migrate.h): tui.state's old defaults take the new ones once, a file on the current schema
         * keeps what the player chose, and the expansion save steps from version 2 (0.9.1+) to 3 with its old copy kept. */
        write_text(tp, "hz=50\nbm_fight=east\nbm_tag=south\nbm_v=1\nff=1\n");
        cfg_load(&g);
        check(g.hz == 60 && cfg_fight_place(&g) == 0, "schema: an old tui.state takes 60 Hz and Fight at the bottom");
        {
            char *t = slurp(tp);
            check(t && strstr(t, "schema=2") && !strstr(t, "bm_v") && strstr(t, "hz=60"), "schema: tui.state stamped schema=2");
            free(t);
        }
        snprintf(tp, sizeof(tp), "%s/tui.state.v0.bak", game_dir);
        {
            char *t = slurp(tp);
            check(t && strstr(t, "hz=50"), "schema: the old tui.state kept as .v0.bak");
            free(t);
        }
        snprintf(tp, sizeof(tp), "%s/tui.state", game_dir);
        write_text(tp, "schema=2\nhz=50\nbm_fight=east\nbm_tag=south\n");
        cfg_load(&g);
        {
            char *t = slurp(tp); /* settings.json's refresh wins over tui.state's hz on a read, so the file is what shows */
            check(cfg_fight_place(&g) == 1 && t && strstr(t, "hz=50"), "schema: a tui.state on the current schema keeps the player's Fight right and 50 Hz");
            free(t);
        }
        {
            char *u = strdup("schema=1\nlang=6\n"), *jp = strdup("schema=1\nlang=7\n");
            int fu = -1, fjp = -1;
            check(migrate_tui_text(&u, &fu) == MIGRATE_DONE && fu == 1 && strstr(u, "lang=0") && strstr(u, "schema=2"),
                  "schema: dropped USA lang (6) migrates to english (0)");
            check(migrate_tui_text(&jp, &fjp) == MIGRATE_DONE && fjp == 1 && strstr(jp, "lang=6") && strstr(jp, "schema=2"),
                  "schema: japanese lang (7) shifts to 6");
            free(u); free(jp);
        }
        write_text(tp, "schema=9\nhz=50\n");
        cfg_load(&g);
        {
            char *t = slurp(tp);
            check(t && !strcmp(t, "schema=9\nhz=50\n"), "schema: a newer tui.state is left as it is");
            free(t);
        }
        write_text(tp, "hz=50\nbm_v=2\nff=1\n");
        cfg_load(&g);
        cfg_load(&g); /* no save in between: the 60 Hz is in the file, not only in the load that migrated it */
        check(g.hz == 60, "schema: the migrated 60 Hz is still there on the next load");
        {
            char *crlf = strdup("hz=50\r\nbm_fight=east\r\nbm_tag=south\r\nff=1"); /* CRLF, and no newline at the end */
            int from = -1;
            check(migrate_tui_text(&crlf, &from) == MIGRATE_DONE && from == 0 && strstr(crlf, "hz=60") &&
                      strstr(crlf, "bm_fight=south") && strstr(crlf, "schema=2") && strstr(crlf, "ff=1"),
                  "schema: a tui.state with CRLF lines and no last newline migrates");
            free(crlf);
        }
        write_text(tp, "schema=9\nhz=50\nfuture_key=1\n");
        cfg_save(&g);
        {
            char *t = slurp(tp);
            check(t && !strcmp(t, "schema=9\nhz=50\nfuture_key=1\n"), "schema: a newer tui.state survives a save of this launcher");
            free(t);
        }
        {
            char *a1 = strdup("{\"version\":1.5}"), *a2 = strdup("{\"version\":99999999999}"), *a3 = strdup("{\"version\":-1}");
            int from = 0;
            check(migrate_save_text(&a1, &from) == MIGRATE_NONE && !strcmp(a1, "{\"version\":1.5}"), "schema: a version that is not a whole number is not ours");
            check(migrate_save_text(&a2, &from) == MIGRATE_NEWER, "schema: a huge version reads as newer, with no overflow");
            check(migrate_save_text(&a3, &from) == MIGRATE_NONE && !strcmp(a3, "{\"version\":-1}"), "schema: a negative version is not ours, left alone");
            free(a1); free(a2); free(a3);
        }
        {
            char *v1 = strdup("{\"version\":1,\"slots\":[{\"gen\":3}]}"),
                 *v2_090 = strdup("{\"version\":2,\"slots\":[{\"gen\":3}]}"),
                 *j = strdup("{\"version\":2,\"slots\":[{\"gen\":3,\"story\":{}}]}"),
                 *n = strdup("{\"version\":4,\"slots\":[]}"), *o2 = strdup("[1]");
            int from = 0;
            check(migrate_save_text(&v1, &from) == MIGRATE_NONE && !strcmp(v1, "{\"version\":1,\"slots\":[{\"gen\":3}]}"),
                  "schema: pre-0.9.1 version 1 save is not migrated");
            check(migrate_save_text(&v2_090, &from) == MIGRATE_NONE && !strcmp(v2_090, "{\"version\":2,\"slots\":[{\"gen\":3}]}"),
                  "schema: 0.9.0 version 2 save without story is not migrated");
            check(migrate_save_text(&j, &from) == MIGRATE_DONE && from == 2 &&
                      !strcmp(j, "{\"version\":3,\"slots\":[{\"gen\":3,\"story\":{}}]}"),
                  "schema: 0.9.1+ expansion save steps 2 -> 3 and nothing else changes");
            check(migrate_save_text(&j, &from) == MIGRATE_NONE, "schema: version 3 is current");
            check(migrate_save_text(&n, &from) == MIGRATE_NEWER && strstr(n, "\"version\":4"), "schema: a newer expansion save is left alone");
            check(migrate_save_text(&o2, &from) == MIGRATE_NONE && !strcmp(o2, "[1]"), "schema: a file that is not the game's is left alone");
            free(v1); free(v2_090); free(j); free(n); free(o2);
        }
        snprintf(tp, sizeof(tp), "%s/card1.def.json", game_dir);
        write_text(tp, "{\"version\": 1, \"slots\": [{\"gen\": 3}]}");
        check(migrate_saves() == 0, "schema: pre-0.9.1 card1.def.json v1 is not migrated");
        write_text(tp, "{\"version\": 2, \"slots\": [{\"gen\": 3}]}");
        check(migrate_saves() == 0, "schema: 0.9.0 card1.def.json v2 without story is not migrated");
        write_text(tp, "{\"version\": 2, \"slots\": [{\"gen\": 3, \"story\": {}}]}");
        check(migrate_saves() == 1, "schema: card1.def.json migrated");
        {
            char *t = slurp(tp);
            check(t && strstr(t, "\"version\": 3"), "schema: card1.def.json is on version 3");
            free(t);
            snprintf(tp, sizeof(tp), "%s/card1.def.json.v2.bak", game_dir);
            t = slurp(tp);
            check(t && strstr(t, "\"version\": 2"), "schema: the old expansion save kept as .v2.bak");
            free(t);
            check(migrate_saves() == 0, "schema: nothing left to migrate");
            /* a later old file (a restored backup, a pull) gets its own copy: no .bak is ever replaced */
            snprintf(tp, sizeof(tp), "%s/card1.def.json", game_dir);
            write_text(tp, "{\"version\": 2, \"slots\": [{\"gen\": 8, \"story\": {}}]}");
            check(migrate_saves() == 1, "schema: a second old file migrates too");
            snprintf(tp, sizeof(tp), "%s/card1.def.json.v2.2.bak", game_dir);
            t = slurp(tp);
            check(t && strstr(t, "\"gen\": 8"), "schema: ... with its own copy, .v2.2.bak");
            free(t);
            unlink(tp);
            snprintf(tp, sizeof(tp), "%s/card1.def.json.v2.bak", game_dir);
            unlink(tp);
            snprintf(tp, sizeof(tp), "%s/card1.def.json", game_dir);
            unlink(tp);
            snprintf(tp, sizeof(tp), "%s/tui.state.v0.bak", game_dir);
            unlink(tp);
        }
        snprintf(tp, sizeof(tp), "%s/tui.state", game_dir);
        write_text(tp, "schema=2\nhz=60\nv091_save_ok=1\n");
        cfg_load(&g);
        cfg_save(&g);
        {
            char *t = slurp(tp);
            check(g.hz == 60 && t && !strstr(t, "v091_save_ok"), "schema: old v091_save_ok key in tui.state is ignored and dropped");
            free(t);
        }
        cfg_save(&a);
        disc_path[0] = 0;
        cfg_load(&b);
        snprintf(disc_path, sizeof(disc_path), "%s", p);
    }
    {
        /* RecompLauncher's mirror (cloud.h): written, never offered back, a change from outside asked about (and kept
         * until answered), then adopted into card1.mcd. */
        static char card_a[PS1_MC_SIZE], card_b[PS1_MC_SIZE], got_card[PS1_MC_SIZE];
        char cp[1200], mp[1200], sp[1200], err[96] = "";
        RecompMirror rm;
        FILE *cf;
        memset(card_a, 0, sizeof(card_a));
        card_a[0] = 'M'; card_a[1] = 'C';
        memcpy(card_b, card_a, sizeof(card_b));
        card_b[2] = 7;
        snprintf(cp, sizeof(cp), "%s/card1.mcd", game_dir);
        snprintf(mp, sizeof(mp), "%s/recomp/dw3-save.dw3s", game_dir);
        snprintf(sp, sizeof(sp), "%s/recomp_mirror.state", game_dir);
        cf = fopen(cp, "wb"); if (cf) { fwrite(card_b, 1, sizeof(card_b), cf); fclose(cf); }
        check(recomp_mirror_write(game_dir, "dev1", "TEST", 3, 0) == 0 && access(mp, F_OK) == 0 &&
                  !recomp_mirror_foreign(game_dir, &rm),
              "recomp mirror: written beside the settings and not offered back");
        /* RecompLauncher restored another device's card B into the mirror while this device holds A */
        write_text(sp, "0000000000000000000000000000000000000000000000000000000000000000");
        cf = fopen(cp, "wb"); if (cf) { fwrite(card_a, 1, sizeof(card_a), cf); fclose(cf); }
        check(recomp_mirror_foreign(game_dir, &rm) == 1 && !strcmp(rm.device_id, "dev1") && rm.cloud_seq == 3,
              "recomp mirror: a change from outside is offered, with its header");
        check(recomp_mirror_write(game_dir, "dev1", "TEST", 3, 0) == 1 && recomp_mirror_foreign(game_dir, NULL) == 1,
              "recomp mirror: a changed mirror is not overwritten before the player answers");
        check(recomp_mirror_adopt(game_dir, 5, err, sizeof(err)) == 0 && !recomp_mirror_foreign(game_dir, NULL),
              "recomp mirror: adopted, and no longer offered");
        cf = fopen(cp, "rb");
        check(cf && fread(got_card, 1, sizeof(got_card), cf) == sizeof(got_card) && !memcmp(got_card, card_b, sizeof(card_b)),
              "recomp mirror: card1.mcd is the mirror's card after adopting");
        if (cf) fclose(cf);
        check(save_backup_list(game_dir, (SaveBackup[4]){0}, 4) >= 1, "recomp mirror: the card it replaced was backed up first");
        unlink(mp);
        check(!recomp_mirror_foreign(game_dir, NULL), "recomp mirror: a missing or damaged mirror is never offered");

        /* The expansion save (cloud.h): card1.def.json is part of the save's sha, its backups, restore and the mirror. */
        {
            char ep[1200], sha_card[65], sha_pair[65], got[80] = "";
            SaveBackup bks[8];
            snprintf(ep, sizeof(ep), "%s/card1.def.json", game_dir);
            save_is_valid_mc(cp, sha_card);
            write_text(ep, "{\"version\":2,\"slots\":[{\"gen\":3,\"story\":{}}]}");
            check(save_is_valid_mc(cp, sha_pair) && strcmp(sha_card, sha_pair) != 0,
                  "expansion: its JSON changes the save's sha; with none the card's own sha is kept");
            {
                /* The migration is not a local change to the cloud: the synced pair's sha follows the migrated bytes. */
                CloudState cs;
                char after[65];
                cloud_state_defaults(&cs);
                snprintf(cs.last_sha256, sizeof(cs.last_sha256), "%s", sha_pair);
                check(cloud_migrate_saves(&cs, game_dir) == 1 && save_is_valid_mc(cp, after) && strcmp(after, sha_pair) != 0 &&
                          !strcmp(cs.last_sha256, after),
                      "schema: migrating a synced save moves the cloud state's sha with it (no false conflict)");
                write_text(ep, "{\"version\":2,\"slots\":[{\"gen\":3,\"story\":{}}]}"); /* the checks below use this file */
                snprintf(ep, sizeof(ep), "%s/card1.def.json.v2.bak", game_dir);
                unlink(ep);
                snprintf(ep, sizeof(ep), "%s/cloud_sync.json", game_dir); /* cloud_state_save's file, from the check above */
                unlink(ep);
                snprintf(ep, sizeof(ep), "%s/card1.def.json", game_dir);
            }
            check(recomp_mirror_write(game_dir, "dev1", "TEST", 4, 1) == 0, "expansion: the mirror is written as a pair");
            int nb = save_backup_list(game_dir, bks, 8);
            char old_bk[sizeof(bks[0].name)] = "", new_bk[sizeof(bks[0].name)] = "";
            if (nb > 0) snprintf(old_bk, sizeof(old_bk), "%.95s", bks[0].name);
            save_backup_rotate(game_dir, "card1.mcd", 5);
            int nb2 = save_backup_list(game_dir, bks, 8);
            check(nb2 == nb + 1, "expansion: a JSON change alone is a new backup");
            for (int i = 0; i < nb2; i++) {
                if (strcmp(bks[i].name, old_bk) != 0) { snprintf(new_bk, sizeof(new_bk), "%.95s", bks[i].name); break; }
            }
            char bj[1400];
            snprintf(bj, sizeof(bj), "%s/saves_backup/%.*s.def.json", game_dir, (int)strlen(new_bk) - 4, new_bk);
            check(new_bk[0] && access(bj, F_OK) == 0, "expansion: the backup carries the JSON beside the card");
            write_text(ep, "{\"version\":2,\"slots\":[{\"gen\":9,\"story\":{}}]}");
            check(save_backup_restore(game_dir, new_bk, 5, err, sizeof(err)) == 0, "expansion: a pair restored");
            cf = fopen(ep, "rb");
            if (cf) { got[fread(got, 1, sizeof(got) - 1, cf)] = 0; fclose(cf); }
            check(strstr(got, "\"gen\":3") != NULL, "expansion: restore brings the backup's JSON back with its card");
            /* the mirror holds the gen 3 pair: changed locally to a card with no JSON, then adopted back */
            remove(ep);
            write_text(sp, "0000000000000000000000000000000000000000000000000000000000000000");
            check(recomp_mirror_foreign(game_dir, NULL) == 1 && recomp_mirror_adopt(game_dir, 5, err, sizeof(err)) == 0 &&
                      save_is_valid_mc(cp, got) && !strcmp(got, sha_pair) && access(ep, F_OK) == 0,
                  "expansion: a format 2 mirror adopted writes the card and its JSON");
            char epv[1300];
            snprintf(epv, sizeof(epv), "%s.prev", ep);
            check(old_bk[0] && save_backup_restore(game_dir, old_bk, 5, err, sizeof(err)) == 0 && access(ep, F_OK) != 0 &&
                      access(epv, F_OK) == 0,
                  "expansion: a backup from before the JSON leaves none beside the card (the old one moved aside)");
            remove(epv);
            /* one the launcher cannot carry (not a JSON object) is left alone: the card still counts and syncs */
            save_is_valid_mc(cp, sha_card);
            write_text(ep, "[1]");
            check(save_is_valid_mc(cp, got) && !strcmp(got, sha_card), "expansion: an uncarriable JSON, the card alone");
            remove(ep);
        }
        remove(sp);
        remove(cp); /* the rest of the self-test starts without a card, as before */
        snprintf(mp, sizeof(mp), "%s/recomp", game_dir);
        rmdir(mp);
    }
#ifdef DW3_TARGET_PC
    /* settings.json as RecompLauncher leaves it (recomplauncher.json's options): the launcher shows its values. */
    snprintf(q, sizeof(q), "%s/settings.json", game_dir);
    replace_in_file(q, "\"filter\": \"crt\"", "\"filter\": \"smooth\"");
    replace_in_file(q, "\"skip_dialogues\": { \"enabled\": true", "\"skip_dialogues\": { \"enabled\": false");
    replace_in_file(q, "\"language\": \"portuguese\"", "\"language\": \"german\"");
    cfg_load(&b);
    check(b.filter == 4 && b.skip == 0 && b.lang == 3, "settings: settings.json's own changes win over tui.state");
#endif
#ifdef DW3_TARGET_PC
    /* The update notice, without the network: a fresh update.cache naming a release far ahead of this build. */
    {
        UpdateInfo u;
        snprintf(q, sizeof(q), "%s/update.cache", game_dir);
        snprintf(what, sizeof(what), "%ld v99.0.0 https://github.com/al4xdev/digimon-world-3-definitive-edition/releases/tag/v99.0.0\n",
                 (long)time(NULL));
        write_text(q, what);
        update_check_start(game_dir, "0.8.1", 1);
        update_poll(&u);
        check(u.state == UPDATE_AVAILABLE && !strcmp(u.latest, "v99.0.0") && !strncmp(u.url, "https://github.com/", 19),
              "update notice: a newer release in update.cache is told without a request");
        check(debug_n() == 5, "update notice: the DEBUG screen gets its NEW VERSION row");
    }
#endif
    /* Every screen draws. */
    snprintf(q, sizeof(q), "%s/screens", root);
    mkdir(q, 0755);
    plat_shot_dir = q;
    c = a;
    int drawn = 0;
    for (int s = S_SPLASH; s <= S_ERROR; s++) {
        if (s == S_LOADING) continue;
        screen = s;
        sel = 0;
        draw_screen();
        snprintf(what, sizeof(what), "screen%02d", s);
        plat_shot(what);
        drawn++;
    }
    check(drawn > 20, "screens: every screen drawn");
    {
        /* The conflict screen with RecompLauncher's save as the third choice, in the longest languages. */
        static const int langs[] = {0, 1, 2, 3};
        conflict_has_cloud = 1;
        conflict_n = 4;
        conflict_acts[0] = CA_CLOUD; conflict_acts[1] = CA_LOCAL; conflict_acts[2] = CA_RECOMP; conflict_acts[3] = CA_CANCEL;
        snprintf(conflict_cloud_info, sizeof(conflict_cloud_info), "CLOUD: REV #12   2026-11-20 02:00   [33b81c84]");
        snprintf(conflict_local_info, sizeof(conflict_local_info), "LOCAL: card1.mcd   2026-11-20 02:10   [9a0e17c2]");
        snprintf(conflict_recomp_info, sizeof(conflict_recomp_info), "RECOMPLAUNCHER: ALEX-DESKTOP   2026-11-19 23:41   [5d41402a]");
        snprintf(conflict_recomp_sha, sizeof(conflict_recomp_sha), "5d41402a");
        for (size_t li = 0; li < sizeof(langs) / sizeof(langs[0]); li++) {
            c.lang = langs[li];
            ui_set_lang(c.lang);
            for (int pass = 0; pass < 2; pass++) {
                screen = pass ? S_CONFLICT_CONFIRM : S_CONFLICT;
                sel = conflict_choice = 2;
                conflict_step = 3;
                confirm_pos = 2.0f;
                cur_screen = S_CONFLICT;
                cur_y = (float)row_target_y(S_CONFLICT, 2);
                draw_screen();
                snprintf(what, sizeof(what), "conflict_l%d_%s", langs[li], pass ? "confirm" : "list");
                plat_shot(what);
            }
        }
        c.lang = a.lang;
        ui_set_lang(c.lang);
        conflict_n = 3;
        conflict_recomp_info[0] = 0;
    }
    {
        /* A refused disc, from a player's 0.8.5 run on Windows (another dump of the European disc, picked in
         * RecompLauncher): S_DISC, with the game's reason kept over the crash report's path printed after it. */
        static const char *const run[] = {
            "port: version 0.8.5 (0688217d87ea8e9036bafb530852dd6c42ee1efb)",
            "port: fatal: disc: E:\\Emulator&Rom\\PSX\\ROM\\DMW 2003.iso is not an accepted Digimon World 2003 disc: "
            "SHA-1 0864298f35d7c6088e06786a89898f9ea68b96f8, expected 457cb233349ba841e03b33d8060f8fbcadd45cb3 "
            "(Europe (SLES-03936), unpatched) (--no-disc-check runs it anyway)",
            "port: crash report: C:\\Users\\player\\AppData\\Roaming\\dw2003\\crashes/crash-20261121-013708.txt",
            "port: exit 1 after 0 frame(s): fatal error",
        };
        disc_bad = DISC_OK;
        fatal_seen = 0;
        last_err[0] = 0;
        for (size_t i = 0; i < sizeof(run) / sizeof(run[0]); i++) game_output_line(run[i]);
        check(disc_bad == DISC_BAD_IMAGE && !strncmp(last_err, "port: fatal: disc:", 18),
              "refused disc: another dump goes to the disc screen, with the game's own reason");
        disc_bad = DISC_OK;
        fatal_seen = 0;
        game_output_line("port: fatal: disc: cannot open E:\\DMW 2003.bin: No such file or directory");
        check(disc_bad == DISC_BAD_FILE, "refused disc: a file that cannot be opened goes there too");
        disc_bad = DISC_OK;
        fatal_seen = 0;
        game_output_line("port: fatal: settings: something else");
        check(disc_bad == DISC_OK && fatal_seen, "refused disc: any other fatal error stays on the error screen");

        /* A crash report as the runtime writes it on Windows (CRLF too), its copy of the log included. */
        char sum[1024];
        crash_summary("dw2003 crash report\r\nkind: crash\nstatus: -1073741819\nbuild: 0.9.1 (abc)\n"
                      "stage: 3\nmap: 258\n\nlog (last 1 lines):\n  pc: not this one\n\nexception: "
                      "EXCEPTION_ACCESS_VIOLATION (0xc0000005)\nfault address: 0x10\naccess: read\npc: exe+0x1a2b\n"
                      "\nstack (unwound):\n  #0 exe+0x1a2b\n  #1 0x7ff812340000 ?\n  #2 exe+0x3c4d\n",
                      sum, sizeof(sum));
        check(!strcmp(sum, "[launcher] crash: kind=crash | build=0.9.1 (abc) | EXCEPTION_ACCESS_VIOLATION "
                           "(0xc0000005) | access=read | fault=0x10 | stage=3 | map=258 | pc=exe+0x1a2b | "
                           "stack=exe+0x1a2b,0x7ff812340000,exe+0x3c4d"),
              "crash report: one line with its reason and addresses");
        crash_summary("not a report\n", sum, sizeof(sum));
        check(!sum[0], "crash report: nothing to say, no line");

        /* Into game.log, from the report this run named; an earlier run's line in the same log is not this run's. */
        char lp[1700], rp[1700], got[2048];
        snprintf(lp, sizeof(lp), "%s/logs", game_dir);
        mkdir(lp, 0755);
        snprintf(lp, sizeof(lp), "%s/logs/game.log", game_dir);
        snprintf(rp, sizeof(rp), "%s/selftest-crash.txt", game_dir);
        FILE *rf = fopen(rp, "wb");
        if (rf) { fputs("kind: crash\npc: exe+0x1a2b\n", rf); fclose(rf); }
        for (int old = 0; old < 2; old++) {
            game_log = fopen(lp, "wb");
            if (!game_log) { check(0, "crash report: the self-test's game.log"); break; }
            fputs("port: crash report: selftest-crash.txt\n", game_log);
            game_log_start = old ? ftell(game_log) : 0;
            log_crash_summary();
            fclose(game_log);
            game_log = NULL;
            rf = fopen(lp, "rb");
            got[rf ? fread(got, 1, sizeof(got) - 1, rf) : 0] = 0;
            if (rf) fclose(rf);
            check((strstr(got, "[launcher] crash: kind=crash | pc=exe+0x1a2b") != NULL) == !old,
                  old ? "crash report: an earlier run's report is left alone" : "crash report: the line reaches game.log");
        }
        game_log_start = 0;
        remove(rp);
        remove(lp);

        /* Every language fits the panel (16 px inside its right edge), for both reasons and each way the launcher
         * runs: its own (CHOOSE DISC), under RecompLauncher, on PortMaster. */
        const char *rl_env = getenv("RECOMPLAUNCHER");
        char rl_saved[64];
        snprintf(rl_saved, sizeof(rl_saved), "%s", rl_env ? rl_env : "0");
        const int pm_saved = is_portmaster;
        int fits = 1;
        char worst[96] = "";
        screen = S_DISC;
        snprintf(last_err, sizeof(last_err), "port: fatal: disc: cannot open E:\\DMW 2003.bin: No such file");
        for (int lang = 0; lang < 6; lang++) {
            c.lang = lang;
            ui_set_lang(c.lang);
            for (int kind = DISC_BAD_IMAGE; kind <= DISC_BAD_FILE; kind++) {
                for (int mode = 0; mode < 3; mode++) {
                    static const char *const modes[] = {"pc", "recomp", "portmaster"};
                    disc_bad = kind;
                    setenv("RECOMPLAUNCHER", mode == 1 ? "1" : "0", 1);
                    is_portmaster = mode == 2;
                    int right = draw_disc();
                    snprintf(what, sizeof(what), "disc_l%d_%s_%s", lang, kind == DISC_BAD_IMAGE ? "image" : "file",
                             modes[mode]);
                    plat_shot(what);
                    if (right > W - 50 - 16) {
                        fits = 0;
                        snprintf(worst, sizeof(worst), "%.64s reaches x %d", what, right);
                    }
                }
            }
        }
        setenv("RECOMPLAUNCHER", rl_saved, 1);
        is_portmaster = pm_saved;
        c.lang = a.lang;
        ui_set_lang(c.lang);
        disc_bad = DISC_OK;
        fatal_seen = 0;
        last_err[0] = 0;
        snprintf(what, sizeof(what), "refused disc: every language fits the disc screen%s%s", worst[0] ? ": " : "",
                 worst);
        check(fits, what);
    }
#ifdef DW3_TARGET_PC
    {
        /* The update screen's words in every language: the notice inside its panel, the stage under the bar, the hints. */
        struct { const char *s; int size, max; } words[] = {
            {"UPDATE READY", 22, W - 120 - 24},          {"PRESS START TO RESTART", 17, W - 120 - 24},
            {"SELECT: LATER", 13, W - 120 - 24},              {"DOWNLOADING...", 18, W - 40},
            {"CHECKING...", 18, W - 40},                 {"PREPARING...", 18, W - 40},
            {"SELECT: CANCEL", 13, W - 40}, {"PRESS A TO UPDATE", 15, W - 64 - 24},      {"PRESS A TO OPEN THE PAGE", 15, W - 64 - 24},
            {"NEW VERSION v0.9.0", 17, W - 192},
            {"Downloads now, then restarts the launcher.", 17, W - 192}, {"Already downloaded: restarts to install it.", 17, W - 192},
            {"Saves and settings are never touched.", 17, W - 192}, {"YES, UPDATE", 18, 200}, {"NO, PLAY", 18, 200},
        };
        /* The two states drawn (and saved with --shots), to look at. */
        memset(&su, 0, sizeof(su));
        su.state = SU_RUNNING; su.pct = 43; snprintf(su.text, sizeof(su.text), "DOWNLOADING");
        draw_updating(); plat_shot("update_downloading");
        su.state = SU_READY; su.pct = 100;
        draw_updating(); plat_shot("update_ready");
        memset(&su, 0, sizeof(su));
        int ufits = 1;
        char uworst[96] = "";
        for (int lang = 0; lang < 6; lang++) {
            for (unsigned i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
                char w[160];
                size_t n = strlen(words[i].s);
                int dots = n > 3 && !strcmp(words[i].s + n - 3, "...");
                snprintf(w, sizeof(w), "%.*s", (int)(dots ? n - 3 : n), words[i].s);
                const char *t = ui_tr_lookup(w, lang);
                if (text_w(t, words[i].size) + (dots ? 3 * words[i].size / 2 : 0) > words[i].max) {
                    ufits = 0;
                    snprintf(uworst, sizeof(uworst), "lang %d: \"%.40s\"", lang, t);
                }
            }
        }
        snprintf(what, sizeof(what), "update screen: every language fits%s%s", uworst[0] ? ": " : "", uworst);
        check(ufits, what);
    }
#endif
    {
        /* SEND LOGS refused while LOGS is OFF: nothing is written, and its line fits the DEBUG screen in every language
         * (status at x 64, 16 px). */
        Cfg keep = c;
        status[0] = 0;
        c.logs = 0;
        send_logs();
        int refused = !strcmp(status, APP_LOGS_OFF);
        int lfits = 1;
        for (int lang = 0; lang < 6; lang++) {
            if (text_w(ui_tr_lookup(APP_LOGS_OFF, lang), 16) > W - 64 - 24) lfits = 0;
        }
        c = keep;
        status[0] = 0;
        check(refused && lfits, "send logs: refused with LOGS at OFF, and the reason fits in every language");
    }
    /* The game beside the launcher (the release's layout) and its mods; with a disc, 300 frames from PLAY. */
    const char *real = getenv("DW3_SELFTEST_GAME");
    if (!real) return st_fails;
    char bin[1024];
    find_game_bin(bin, sizeof(bin));
    snprintf(what, sizeof(what), "the game beside the launcher: %s", bin);
    check(exe_ok(bin), what);
    snprintf(q, sizeof(q), "%s/mods", exe_dir);
    snprintf(what, sizeof(what), "the mods beside the game: %s", q);
    check(count_mods(q) > 0, what);
    if (!exe_ok(bin)) return st_fails;
    c = a;
    c.fullscreen = 0;
    c.logs = 2;
    c.sync_enabled = 0;
    static const char *const extra[] = {"--max-frames", "300", "--fps", "0", NULL};
    memcpy(game_extra, extra, sizeof(extra));
    const char *vd = getenv("DW3_SELFTEST_VIDEO_DRIVER"); /* Wine drops SDL_VIDEO_DRIVER from the environment */
    setenv("SDL_VIDEO_DRIVER", vd && vd[0] ? vd : "offscreen", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
    {
        /* PLAY with an image the game refuses (16 zero sectors), no disc needed: the game's own check and output,
         * read back as the disc screen, not the error screen. */
        static const char zero[2352];
        snprintf(p, sizeof(p), "%s/not-the-disc.bin", root);
        FILE *f = fopen(p, "wb");
        for (int i = 0; f && i < 16; i++) fwrite(zero, 1, sizeof(zero), f);
        if (f) fclose(f);
        snprintf(disc_path, sizeof(disc_path), "%s", p);
        screen = S_HOME;
        quit_flag = 0;
        start_game();
        selftest_wait_game();
        snprintf(what, sizeof(what), "PLAY with an image the game refuses ends on the disc screen: %s; %s", msg,
                 last_err);
        check(launch_pid < 0 && screen == S_DISC && disc_bad == DISC_BAD_IMAGE && quit_flag == 0, what);
    }
    const char *disc = getenv("DW3_SELFTEST_DISC");
    if (!disc) return st_fails;
    snprintf(disc_path, sizeof(disc_path), "%s", disc);
    screen = S_HOME;
    quit_flag = 0;
    msg[0] = 0;
    start_game();
    check(screen == S_LOADING && launch_pid > 0, "the game starts from PLAY");
    selftest_wait_game();
    check(status_said_start, "the game's status channel said start (DW3_STATUS_FD)");
    snprintf(what, sizeof(what), "the game ran 300 frames and ended normally: %s%s%s", msg, last_err[0] ? "; " : "",
             last_err);
    check(launch_pid < 0 && quit_flag == 1, what);
    snprintf(p, sizeof(p), "%s/card1.mcd", game_dir);
    check(access(p, F_OK) == 0, "the game made memory card 1 in the settings directory");
    snprintf(p, sizeof(p), "%s/logs/game.log", game_dir);
    check(access(p, F_OK) == 0, "the game's output in logs/game.log");
    return st_fails;
}

int main(int argc, char **argv) {
    int kind = PLAT_FB, force_splash = 0;
#ifdef DW3_TARGET_PC
    kind = PLAT_SDL; /* a desktop: a window, never the console's framebuffer */
#endif
    const char *script = NULL, *self_test_dir = NULL;
    int play_now = 0;
#ifdef _WIN32
    char self[1024];
    if (w32_exe_path(self, sizeof(self)) == 0 && path_last_sep(self)) {
        *path_last_sep(self) = 0;
        snprintf(exe_dir, sizeof(exe_dir), "%s", self);
    }
#else
    char self[1024];
    ssize_t sl_n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (sl_n > 0) {
        self[sl_n] = 0;
        if (path_last_sep(self)) *path_last_sep(self) = 0;
        snprintf(exe_dir, sizeof(exe_dir), "%s", self);
    } else {
        const char *sl = argc > 0 && argv[0] ? strrchr(argv[0], '/') : NULL;
        if (sl && (size_t)(sl - argv[0]) < sizeof(exe_dir)) {
            size_t n = (size_t)(sl - argv[0]);
            memcpy(exe_dir, argv[0], n);
            exe_dir[n] = 0;
        }
    }
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sdl")) kind = PLAT_SDL;
        else if (!strcmp(argv[i], "--fb")) kind = PLAT_FB;
        else if (!strcmp(argv[i], "--pc")) is_pc = 1;
        else if (!strcmp(argv[i], "--splash")) force_splash = 1;
        else if (!strcmp(argv[i], "--version")) {
            /* what the updater runs on a staged launcher before installing it: it starts, and says which one it is */
#ifdef DW3_LAUNCHER_VERSION
            puts(DW3_LAUNCHER_VERSION);
#else
            puts("dev");
#endif
            return 0;
        }
        else if (!strcmp(argv[i], "--play")) play_now = 1;
        else if (!strcmp(argv[i], "--null") && i + 1 < argc) { kind = PLAT_NULL; script = argv[++i]; }
#ifndef DW3_TARGET_PC
        /* the handhelds' folder; a PC launcher has only the rule (settings_dir_for) */
        else if (!strcmp(argv[i], "--dir") && i + 1 < argc) snprintf(game_dir, sizeof(game_dir), "%s", argv[++i]);
#endif
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) plat_shot_dir = argv[++i];
        else if (!strcmp(argv[i], "--self-test") && i + 1 < argc) { self_test_dir = argv[++i]; kind = PLAT_NULL; script = ""; }
    }
    if (kind == PLAT_NULL && !force_splash) {
        screen = S_HOME;
        cur_screen = S_HOME;
        cur_y = (float)row_target_y(S_HOME, 0);
    }
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
#if defined(DW3_TARGET_PORTMASTER)
    is_portmaster = 1;
    is_pc = 0;
#elif defined(DW3_TARGET_PC)
    is_pc = 1;
    is_portmaster = 0;
    if (settings_dir_for(exe_dir, game_dir, sizeof(game_dir)) != 0) {
        fprintf(stderr, "dw3-launcher: no per-user settings directory (SDL_GetPrefPath failed)\n");
        return 3;
    }
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
    if (self_test_dir) {
        int fails = self_test(self_test_dir);
        fprintf(stderr, "self-test: %d of %d checks passed\n", st_checks - st_fails, st_checks);
        plat_release();
        return fails ? 1 : 0;
    }
    find_disc();
    cfg_load(&c);
    cloud_state_load(&cloud, game_dir);
#ifdef DW3_TARGET_PC
    if (kind != PLAT_NULL) update_check_start(game_dir, DW3_LAUNCHER_VERSION, c.update_check); /* never in a test run */
    if (kind == PLAT_NULL) update_asked = 1;
    if (kind != PLAT_NULL) selfupd_cleanup(exe_dir); /* what an earlier update left in its working folder */
#endif
    trans_from = malloc((size_t)ui_w * ui_h * sizeof(uint32_t));
    trans_to = malloc((size_t)ui_w * ui_h * sizeof(uint32_t));
    long last = plat_now_ms();
    if (screen == S_SPLASH) splash_t0 = last;
    /* Pre-render first frame so any initial transition snapshots a real picture */
    draw();
    plat_present();
    /* --play (RecompLauncher's "Launcher menu: Skip"): PLAY at once, from HOME. Everything PLAY does around the game
     * still happens (the backup, the cloud sync, a save conflict's screen); a missing disc or game lands on the
     * menu's own screens, and a normal end of the game ends the launcher, as after PLAY. */
    if (play_now && kind != PLAT_NULL) {
#ifdef DW3_TARGET_PC
        update_asked = 1; /* "Skip" means no menu in between */
#endif
        screen = S_HOME;
        cur_screen = S_HOME;
        cur_y = (float)row_target_y(S_HOME, 0);
        start_game();
    }
    for (;;) {
        int ty = row_target_y(screen, sel);
        int cursor_moving = ty >= 0 && cur_screen == screen && (cur_y - ty > 0.5f || ty - cur_y > 0.5f);
        int animating = trans_active || cursor_moving || screen == S_SPLASH || screen == S_LOADING || screen == S_UPDATING || screen == S_CONFLICT_CONFIRM;
        Ev ev = {0};
        int r = plat_poll(&ev, animating ? 16 : 33);
        if (r < 0) break;
        if (r > 0 && ev.code) {
            if (ev.down) on_press(ev.code);
            else if (capture && screen == S_CONTROLS && sel == A_COUNT + 1 && cap_n == 1 && ev.code == cap_btns[0]) {
                /* let go with only one button down: TURBO is that one button */
                c.ff_n = 1;
                c.ff_btns[0] = cap_btns[0];
                capture = 0;
                if (ev.code == btn_select()) held_select = 0;
                if (ev.code == btn_start()) held_start = 0;
            }
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
#ifdef DW3_TARGET_PC
        if (su_on) {
            selfupd_poll(&su);
            if (su.state == SU_FAILED) {
                su_on = 0;
                if (screen == S_UPDATING) go(S_DEBUG, 4, -1, 284 + 14);
                snprintf(status, sizeof(status), "UPDATE FAILED: %.120s", su.text);
            }
        }
        if (screen == S_UPDATING) load_step(now - last);
#endif
        if (screen == S_LOADING) {
            load_step(now - last);
            pump_game();
            if (launch_started && screen == S_LOADING && launch_pid != -2) {
                /* the game's window is next: leave the last picture up, give the screen away, and monitor logs + periodic sync */
                draw();
                plat_present();
                plat_release();
                game_close_status();

                char card1_p[1024], last_sha[65] = "", cur_sha[65] = "", pending_sha[65] = "", smsg[128] = "";
                char buf[1024], why[160] = "";
                long n;
                snprintf(card1_p, sizeof(card1_p), "%s/card1.mcd", game_dir);
                save_is_valid_mc(card1_p, last_sha);
                time_t last_sync_ts = time(NULL);
                int syncing = 0, sync_ok = 0, code = 1;

                for (;;) {
                    game_wait(1000);
                    while ((n = game_read(0, buf, sizeof(buf))) > 0) {
                        if (game_log) { fwrite(buf, 1, (size_t)n, game_log); fflush(game_log); }
                    }
                    if (syncing && sync_end(0, &sync_ok)) {
                        syncing = 0;
                        if (sync_ok) {
                            cloud_state_load(&cloud, game_dir);
                            snprintf(last_sha, sizeof(last_sha), "%s", pending_sha);
                            if (game_log) {
                                fprintf(game_log, "[cloud-sync] periodic push ok (rev #%d)\n", cloud.last_seq);
                                fflush(game_log);
                            }
                        }
                    }
                    if (game_reap(&code, why, sizeof(why))) break;

                    time_t tnow = time(NULL);
                    if (!syncing && c.sync_interval_min > 0 && tnow - last_sync_ts >= (time_t)c.sync_interval_min * 60) {
                        struct stat cst;
                        if (stat(card1_p, &cst) == 0) {
                            if ((tnow - cst.st_mtime) < 2) {
                                /* Wait 2s for active memory card write to finish instead of skipping the whole interval. */
                                continue;
                            }
                            if (save_is_valid_mc(card1_p, cur_sha) && strcmp(cur_sha, last_sha) != 0) {
                                save_backup_rotate(game_dir, "card1.mcd", c.backup_max);
                                if (recomp_active()) recomp_mirror_write(game_dir, cloud.device_id, cloud.device_name, cloud.last_seq, 0);
                                if (c.sync_enabled && cloud.registered && !cloud.revoked) {
                                    snprintf(pending_sha, sizeof(pending_sha), "%s", cur_sha);
                                    syncing = sync_start() == 0;
                                } else {
                                    snprintf(last_sha, sizeof(last_sha), "%s", cur_sha);
                                }
                            }
                        }
                        last_sync_ts = tnow;
                    }
                }
                launch_pid = -1;

                if (syncing && sync_end(1, &sync_ok) && sync_ok) cloud_state_load(&cloud, game_dir);

                game_log_end(code, why);

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
                    if (recomp_active()) recomp_mirror_write(game_dir, cloud.device_id, cloud.device_name, cloud.last_seq, 0);
                }
                if (game_log) { fclose(game_log); game_log = NULL; }
                return code;
            }
        }
        last = now;
        if (quit_flag) break;
        draw();
        plat_present();
    }
    plat_quit();
    return 0;
}
