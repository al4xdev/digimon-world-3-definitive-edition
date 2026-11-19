/* Parallel touch-first launcher for Android (arm64-v8a).
 * Keeps the Digivice aesthetic of launcher/main.c while increasing clickable spacing, adding touch hitboxes
 * (< BACK header pill, full-width rows, < / > value regions, clickable footer pills, tap-to-jump loading),
 * exposing the FLOATING PAD toggle in GAME settings, and supporting native Android controllers simultaneously. */
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cfg.h"
#include "cloud.h"
#include "i18n.h"
#include "loading.h"
#include "plat.h"
#include "ui.h"

#ifdef __ANDROID__
extern int android_pick_disc_files(void);
#else
static int android_pick_disc_files(void) { return -1; }
#endif

enum {
    S_SPLASH, S_HOME, S_GAME, S_HUD, S_CONTROLS, S_SAVES, S_BACKUPS, S_BACKUP_CONFIRM,
    S_DEBUG, S_BROWSE, S_RESET, S_CLEAN, S_LOADING, S_ERROR
};
enum { HI_PLAY, HI_GAME, HI_HUD, HI_CONTROLS, HI_SAVES, HI_DEBUG, HI_RESET, HI_COUNT };
enum { GAME_LANG, GAME_FF, GAME_LOAD, GAME_SKIP, GAME_BATTLES, GAME_HIT, GAME_BTEXT, GAME_TOUCHPAD, GAME_DISC, GAME_N };
#define GAME_ROW_STEP 36
enum { SAVES_SYNC, SAVES_INTERVAL, SAVES_BACKUPS, SAVES_NOW, SAVES_RESTORE, SAVES_N };
enum { DEBUG_LOGS, DEBUG_SAVE_LOGS, DEBUG_CLEAN_LOGS, DEBUG_N };

static Cfg c;
static CloudState cloud;
static int screen = S_SPLASH, sel, capture, return_screen = S_HOME;
static int return_sel;
static char msg[256], status[256], last_err[256];
static long splash_t0 = -1;
#define SPLASH_MS 600

static char b_dir[1024];
static BrowseItem b_items[256];
static int b_count, b_scroll;
#define BROWSE_VIS 6

static SaveBackup bk_items[BACKUP_MAX_LIST];
static int bk_count, bk_scroll;
#define LIST_VIS 6

static int touch_drag_active, touch_drag_y0, touch_drag_scrolled;

static long launch_t0, launch_prev_ms;
static int launch_pct;
static char launch_stage[64] = "STARTING";
static int android_done, android_cancelled;

static float cur_y = -1.0f;
static int cur_screen = -1;
static int trans_active, trans_dir, trans_cy;
static long trans_t0;
static uint32_t *trans_from, *trans_to;
#define TRANS_MS 180

static const char *lang_label[8] = {"ENGLISH", "PORTUGUESE BR", "FRENCH", "GERMAN", "ITALIAN", "SPANISH", "USA", "JAPANESE"};
static const char *ff_label[5] = {"OFF", "2X", "4X", "8X", "UNLIMITED"};
static const char *load_label[5] = {"OFF", "2X", "4X", "8X", "INSTANT"};
static const char *bt_label[3] = {"OFF", "SKIP", "ASYNC"};
static const char *buf_label[3] = {"OFF", "SHORT", "LONG"};
static const char *log_label[3] = {"OFF", "SOFT", "HEAVY"};
static const char *lay_label[4] = {"PS1", "XBOX", "NINTENDO", "CUSTOM"};
static const char *act_label[A_COUNT] = {"CONFIRM / CROSS", "MENU / SQUARE", "BACK / CIRCLE", "CAM / TRIANGLE", "PREV / L1", "NEXT / R1", "START", "SELECT"};
static const int act_order[A_COUNT] = {A_CROSS, A_CIRCLE, A_SQUARE, A_TRIANGLE, A_L1, A_R1, A_START, A_SELECT};
static const char *hud_label[H_COUNT] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "ITEM", "RUN", "INSPECT", "BLAST"};

static const char *S(const char *en) {
    return ui_tr_lookup(en, c.lang);
}

static int btn_ok(void) { return cfg_confirm_code(&c); }
static int btn_back(void) { return cfg_back_code(&c); }
static int btn_start(void) { return c.pad[A_START].code; }

static void glyph(int code, char *out) {
    char l = layout_letter(cfg_layout(&c), code);
    if (l != '-') { out[0] = l; out[1] = 0; return; }
    switch (code) {
    case K_SOUTH: strcpy(out, "X"); break;
    case K_EAST: strcpy(out, "O"); break;
    case K_WEST: strcpy(out, "^"); break;
    case K_NORTH: strcpy(out, "[]"); break;
    case K_L1: strcpy(out, "L1"); break;
    case K_R1: strcpy(out, "R1"); break;
    case K_L2: strcpy(out, "L2"); break;
    case K_R2: strcpy(out, "R2"); break;
    case K_SELECT: strcpy(out, "SEL"); break;
    case K_START: strcpy(out, "STA"); break;
    default: strcpy(out, "?"); break;
    }
}

static int in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void draw_touch_pill(int x, int y, int w, int h, const char *label, uint32_t bg, uint32_t fg) {
    rrect(x, y, w, h, h / 2, bg, 235);
    rrect_line(x, y, w, h, h / 2, fg);
    text_c(x + w / 2, y + (h - 15) / 2, label, 15, fg);
}

static void header_touch(const char *sub, int can_back) {
    rect(0, 0, W, 64, C_BG0, 195);
    rect(0, 64, W, 1, C_CYAN, 200);
    circle(40, 32, 16, C_BG1, 256);
    ring(40, 32, 16, 2, C_CYAN);
    circle(40, 32, 7, C_GREEN, 256);
    text(68, 16, "DIGIMON WORLD 3", 23, C_TEXT);
    text(68, 42, S(sub), 14, C_CYAN);
    if (can_back) {
        draw_touch_pill(520, 14, 98, 36, S("BACK"), C_PANEL, C_CYAN);
    }
}

static void footer_touch(int show_play, int show_back, const char *mid_label) {
    rect(0, H - 56, W, 56, C_BG0, 210);
    rect(0, H - 56, W, 1, C_EDGE, 256);
    if (show_back) {
        draw_touch_pill(28, H - 46, 126, 36, S("BACK"), C_PANEL, C_NOTE);
    }
    if (mid_label) {
        draw_touch_pill(240, H - 46, 160, 36, S(mid_label), C_PANEL, C_GOLD);
    }
    if (show_play) {
        draw_touch_pill(486, H - 46, 126, 36, S("PLAY"), RGB(18, 64, 48), C_GREEN);
    }
}

static int row_target_y(int s, int idx) {
    switch (s) {
    case S_HOME: return 78 + idx * 48;
    case S_GAME: return 76 + idx * GAME_ROW_STEP;
    case S_HUD: return 174 + idx * 25;
    case S_CONTROLS: return idx == 0 ? 74 : 122 + (idx - 1) * 37;
    case S_SAVES: return 86 + idx * 52;
    case S_BACKUPS: return 114 + (idx - bk_scroll) * 44;
    case S_BROWSE: return 116 + (idx - b_scroll) * 44;
    case S_DEBUG: return 86 + idx * 54;
    }
    return -1;
}

static void draw_focus(int x, int y, int w, int h, int r, int bad) {
    long t = plat_now_ms();
    int wave = (int)(t % 1200);
    int p = wave < 600 ? wave : 1200 - wave;
    uint32_t bar = bad ? C_RED : mix(C_GREEN, C_CYAN, p * 120 / 600);
    rrect(x, y, w, h, r, bad ? RGB(70, 22, 28) : C_FOCUS, 256);
    rrect_line(x, y, w, h, r, bad ? C_RED : C_EDGE);
    rect(x, y + 6, 4, h - 12, bar, 256);
}

static void row_touch(int y, int h, const char *label, const char *val, int selected, int bad, int on, int draw_bg, int arrows) {
    if (selected && draw_bg) draw_focus(32, y, W - 64, h, 10, bad);
    else if (!selected) rrect(32, y, W - 64, h, 10, C_PANEL, 120);
    int ty = y + (h - 20) / 2;
    text(54, ty, S(label), 19, bad ? C_RED : selected ? C_GOLD : C_TEXT);
    if (val) {
        char buf[96];
        if (arrows) snprintf(buf, sizeof(buf), "< %s >", S(val));
        else snprintf(buf, sizeof(buf), "%s", S(val));
        text_r(W - 54, ty, buf, 19, bad ? C_RED : on ? C_GREEN : selected ? C_GOLD : C_NOTE);
    }
}

static void modal_touch(const char *title, const char *l1, const char *l2, const char *l3, const char *a, const char *b, uint32_t accent) {
    rect(0, 0, W, H, C_BG0, 200);
    rrect(60, 108, W - 120, 256, 14, C_PANEL, 256);
    rrect_line(60, 108, W - 120, 256, 14, accent);
    circle(100, 148, 14, accent, 256);
    text_c(100, 134, "!", 24, C_BG0);
    text(128, 132, S(title), 22, C_TEXT);
    text(90, 182, S(l1), 16, C_NOTE);
    text(90, 208, S(l2), 16, C_NOTE);
    text(90, 234, S(l3), 16, C_NOTE);
    draw_touch_pill(90, 294, 210, 44, S(a), RGB(22, 68, 48), C_GREEN);
    draw_touch_pill(340, 294, 210, 44, S(b), RGB(68, 24, 30), C_RED);
}

static void start_trans(int dir, int focus_y) {
    if (!trans_from || !trans_to) return;
    memcpy(trans_from, fb, (size_t)ui_w * ui_h * sizeof(uint32_t));
    trans_active = 1;
    trans_dir = dir;
    trans_cy = focus_y;
    trans_t0 = plat_now_ms();
}

static void go(int s, int new_sel, int dir) {
    int fy = row_target_y(screen, sel);
    if (fy < 0) fy = H / 2;
    start_trans(dir, fy);
    screen = s;
    sel = new_sel;
    cur_y = (float)row_target_y(s, new_sel);
    cur_screen = s;
    msg[0] = 0;
}

static void open_browse(void) {
    browse_start_dir(b_dir, sizeof(b_dir));
    b_count = browse_read(b_dir, b_items, 256);
    b_scroll = 0;
    go(S_BROWSE, 0, +1);
}

static void pick_disc_android(void) {
    if (android_pick_disc_files() == 0) {
        find_disc();
        if (disc_path[0]) {
            cfg_save(&c);
            snprintf(status, sizeof(status), "DISC IMPORTED");
            return;
        }
    }
    open_browse();
}

static void start_game_flow(void) {
    if (!disc_path[0]) {
        pick_disc_android();
        if (!disc_path[0]) return;
    }
    if (cfg_save(&c) != 0) {
        snprintf(msg, sizeof(msg), "Cannot write settings.json");
        go(S_ERROR, 0, +1);
        return;
    }
    launch_t0 = plat_now_ms();
    launch_prev_ms = launch_t0;
    launch_pct = 0;
    snprintf(launch_stage, sizeof(launch_stage), "SAVING CONFIG");
    load_init(NULL);
    go(S_LOADING, 0, +1);
}

static void draw_splash(void) {
    if (splash_t0 < 0) splash_t0 = plat_now_ms();
    long dt = plat_now_ms() - splash_t0;
    int p = dt >= SPLASH_MS ? 256 : (int)(dt * 256 / SPLASH_MS);
    int inv = 256 - p;
    int ease = 256 - (inv * inv >> 8);
    background((int)(plat_now_ms() & 0x7fffffff));
    int cx = W / 2, cy = H / 2 - 24;
    ring(cx, cy, 16 + (ease * 48 >> 8), 4, mix(C_BG0, C_CYAN, ease));
    circle(cx, cy, 8 + (ease * 12 >> 8), mix(C_BG0, C_GREEN, ease), 256);
    rect(cx - (ease * 210 >> 8), cy + 78, ease * 420 >> 8, 2, C_CYAN, 200);
    if (p > 40) {
        int ta = p > 140 ? 256 : (p - 40) * 256 / 100;
        text_c(cx, cy + 92, "DIGIMON WORLD 3", 30, mix(C_BG0, C_TEXT, ta));
        text_c(cx, cy + 128, "ANDROID TOUCH EDITION", 17, mix(C_BG0, C_CYAN, ta));
    }
}

static void draw_home(void) {
    header_touch("ANDROID TOUCH", 0);
    static const char *const names[HI_COUNT] = {
        "PLAY", "GAME", "BATTLE HUD", "CONTROLS", "SAVES & CLOUD", "DEBUG", "RESET SETTINGS"
    };
    static const char *const hints[HI_COUNT] = {
        "Start Digimon World 3 with your saved settings.",
        "Language, fast-forward, skipping and battle text.",
        "Where each battle action sits on your pad.",
        "Your buttons, and the letters the game shows.",
        "Cloud cross-play sync, 1-100 local save rotation and backup restore.",
        "Logs for the people who help you fix a crash.",
        "Put every setting back to the tuned defaults."
    };
    int gy = (int)(cur_y >= 0 && cur_screen == S_HOME ? cur_y : row_target_y(S_HOME, sel));
    draw_focus(32, gy, W - 64, 42, 10, 0);
    for (int i = 0; i < HI_COUNT; i++) {
        char sv[32];
        const char *val = NULL;
        int on = 0;
        switch (i) {
        case HI_PLAY: val = disc_path[0] ? "DISC READY" : "BROWSE..."; on = disc_path[0] != 0; break;
        case HI_GAME: val = "TUNING"; break;
        case HI_HUD: val = "CUSTOMIZE"; break;
        case HI_CONTROLS: val = "REMAP"; break;
        case HI_SAVES:
            if (cloud.revoked) val = "REVOKED";
            else if (cloud.registered && c.sync_enabled) { val = "CLOUD ON"; on = 1; }
            else { snprintf(sv, sizeof(sv), "LOCAL x%d", c.backup_max); val = sv; }
            break;
        case HI_DEBUG: val = "SETTINGS"; break;
        case HI_RESET: val = "ASK"; break;
        }
        row_touch(78 + i * 48, 42, names[i], val, i == sel, (i == HI_PLAY && !disc_path[0]), on, 0, 0);
    }
    rect(0, H - 56, W, 56, C_BG0, 210);
    rect(0, H - 56, W, 1, C_EDGE, 256);
    text(36, H - 36, S(hints[sel]), 13, C_NOTE);
    draw_touch_pill(486, H - 46, 126, 36, disc_path[0] ? S("PLAY") : S("CHOOSE"), RGB(18, 64, 48), C_GREEN);
}

static void draw_game(void) {
    header_touch("GAME", 1);
    const char *disc_lbl = "BROWSE...";
    if (disc_path[0]) {
        const char *b = strrchr(disc_path, '/');
        disc_lbl = b ? b + 1 : disc_path;
    }
    const char *lab[GAME_N] = {
        "LANGUAGE", "FAST-FORWARD", "FAST LOAD", "SKIP DIALOGUES", "SKIP BATTLE ANIMS",
        "   HIT REACTION", "BATTLE TEXT", "FLOATING PAD", "DISC IMAGE"
    };
    const char *val[GAME_N] = {
        lang_label[c.lang < 0 || c.lang > 7 ? 0 : c.lang],
        ff_label[c.ff],
        load_label[c.load],
        c.skip ? "ON" : "OFF",
        c.battles ? "ON" : "OFF",
        c.battles ? (c.hit ? "ON" : "OFF") : "-",
        bt_label[c.btext],
        c.touch_pad ? "ON" : "OFF",
        disc_lbl
    };
    int gy = (int)(cur_y >= 0 && cur_screen == S_GAME ? cur_y : row_target_y(S_GAME, sel));
    draw_focus(32, gy, W - 64, 38, 10, 0);
    for (int i = 0; i < GAME_N; i++) {
        int on = (i == GAME_DISC) ? (disc_path[0] != 0) : (strcmp(val[i], "OFF") != 0 && strcmp(val[i], "-") != 0);
        int arrows = (i != GAME_DISC);
        row_touch(76 + i * GAME_ROW_STEP, 34, lab[i], val[i], i == sel, 0, on, 0, arrows);
    }
    footer_touch(1, 1, "CHANGE");
}

static void draw_hud(void) {
    header_touch("BATTLE HUD", 1);
    unsigned dm = 0;
    int bad = dup_hud(&c, &dm);
    int cx = W / 2, cy = 116;
    static const struct { int code; int dx, dy; } pos[] = {
        {K_WEST, 0, -28}, {K_NORTH, -80, 0}, {K_EAST, 80, 0}, {K_SOUTH, 0, 28},
        {K_L1, -210, -28}, {K_R1, 210, -28}, {K_L2, -210, 20}, {K_R2, 210, 20},
    };
    rrect(40, 70, W - 80, 96, 12, C_PANEL, 220);
    for (int p = 0; p < 8; p++) {
        char g[8], names[64] = "";
        glyph(pos[p].code, g);
        int n = 0, x = cx + pos[p].dx, y = cy + pos[p].dy;
        for (int a = 0; a < H_COUNT; a++) {
            if (c.hud[a] == pos[p].code) { if (n++) strcat(names, "/"); strcat(names, hud_label[a]); }
        }
        keycap(x - 12, y, g, n > 1 ? C_RED : n ? C_CYAN : C_OFF);
        if (n) {
            uint32_t col = n > 1 ? C_RED : C_GOLD;
            if (pos[p].dx > 0) text(x + 20, y - 8, names, 13, col);
            else text_r(x - 16, y - 8, names, 13, col);
        }
    }
    const char *rl[H_COUNT + 2] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "ITEM", "RUN", "INSPECT", "BLAST", "REMEMBER CURSOR", "BUFFER INPUT"};
    int gy = (int)(cur_y >= 0 && cur_screen == S_HUD ? cur_y : row_target_y(S_HUD, sel));
    int sbd = sel < H_COUNT && ((dm >> sel) & 1);
    draw_focus(32, gy, W - 64, 24, 6, sbd);
    for (int i = 0; i < H_COUNT + 2; i++) {
        char g[16], v[48];
        if (i < H_COUNT) {
            glyph(c.hud[i], g);
            if (capture && i == sel) snprintf(v, sizeof(v), "PRESS A BUTTON...");
            else snprintf(v, sizeof(v), "< %s [%s] >", place_display(c.hud[i]), g);
        } else if (i == H_COUNT) {
            snprintf(v, sizeof(v), "< %s >", S(c.bmem ? "ON" : "OFF"));
        } else {
            snprintf(v, sizeof(v), "< %s >", buf_label[c.bbuf]);
        }
        int y = 174 + i * 25, bd = i < H_COUNT && ((dm >> i) & 1);
        text(54, y + 3, S(rl[i]), 16, bd ? C_RED : i == sel ? C_GOLD : C_TEXT);
        text_r(W - 54, y + 3, v, 16, bd ? C_RED : i == sel ? C_GOLD : C_NOTE);
    }
    footer_touch(0, 1, bad ? "RESET" : "CYCLE");
}

static void draw_controls(void) {
    header_touch("CONTROLS", 1);
    unsigned dm = 0;
    (void)dup_actions(&c, &dm);
    char v[48];
    int gy = (int)(cur_y >= 0 && cur_screen == S_CONTROLS ? cur_y : row_target_y(S_CONTROLS, sel));
    int sbd = sel > 0 && ((dm >> act_order[sel - 1]) & 1);
    draw_focus(32, gy, W - 64, 34, 8, sbd);
    snprintf(v, sizeof(v), "%s", lay_label[cfg_layout(&c)]);
    row_touch(74, 34, "BUTTON GLYPHS", v, sel == 0, 0, 0, 0, 1);
    for (int i = 0; i < A_COUNT; i++) {
        int a = act_order[i], y = 122 + i * 37, isel = sel == i + 1, bd = (dm >> a) & 1;
        char g[16];
        glyph(c.pad[a].code, g);
        if (capture && isel) snprintf(v, sizeof(v), "PRESS A BUTTON...");
        else snprintf(v, sizeof(v), "%s [%s]", place_display(c.pad[a].code), g);
        row_touch(y, 34, act_label[a], v, isel, bd, 0, 0, 0);
    }
    footer_touch(0, 1, "RESET");
}

static void draw_saves(void) {
    header_touch("SAVES & CLOUD", 1);
    int gy = (int)(cur_y >= 0 && cur_screen == S_SAVES ? cur_y : row_target_y(S_SAVES, sel));
    draw_focus(32, gy, W - 64, 44, 10, 0);
    const char *sync_val = cloud.revoked ? "REVOKED" : !cloud.registered ? "REGISTER FIRST" : c.sync_enabled ? "ON" : "OFF";
    char iv_val[24], bk_val[24], sl_val[24];
    if (c.sync_interval_min == 0) snprintf(iv_val, sizeof(iv_val), "START / EXIT");
    else snprintf(iv_val, sizeof(iv_val), "EVERY %d MIN", c.sync_interval_min);
    int bk_n = save_backup_list(game_dir, bk_items, BACKUP_MAX_LIST);
    snprintf(bk_val, sizeof(bk_val), "%d SAVED", bk_n);
    snprintf(sl_val, sizeof(sl_val), "%d / 100", c.backup_max);
    row_touch(86, 44, "CLOUD SYNC", sync_val, sel == SAVES_SYNC, cloud.revoked, cloud.registered && !cloud.revoked && c.sync_enabled, 0, 0);
    row_touch(138, 44, "SYNC INTERVAL", iv_val, sel == SAVES_INTERVAL, 0, c.sync_interval_min > 0, 0, 1);
    row_touch(190, 44, "LOCAL BACKUPS", sl_val, sel == SAVES_BACKUPS, 0, 1, 0, 1);
    row_touch(242, 44, "SYNC NOW", cloud.registered && !cloud.revoked ? "RUN" : "-", sel == SAVES_NOW, 0, 0, 0, 0);
    row_touch(294, 44, "RESTORE BACKUP", bk_val, sel == SAVES_RESTORE, 0, bk_n > 0, 0, 0);
    if (status[0]) text(40, H - 80, status, 14, C_CYAN);
    footer_touch(0, 1, "SELECT");
}

static void draw_backups(void) {
    header_touch("LOCAL BACKUPS", 1);
    text(40, 76, "ROTATING card1.mcd SNAPSHOTS (NEWEST FIRST)", 15, C_CYAN);
    if (bk_count > 0) {
        int gy = (int)(cur_y >= 0 && cur_screen == S_BACKUPS ? cur_y : row_target_y(S_BACKUPS, sel));
        draw_focus(32, gy, W - 64, 40, 10, 0);
        for (int i = bk_scroll; i < bk_count && i < bk_scroll + LIST_VIS; i++) {
            row_touch(114 + (i - bk_scroll) * 44, 40, bk_items[i].label, i == 0 ? "LATEST" : "RESTORE", i == sel, 0, i == 0, 0, 0);
        }
    } else {
        text_c(W / 2, 220, "NO LOCAL BACKUPS YET", 18, C_NOTE);
    }
    footer_touch(0, 1, bk_count > 0 ? "RESTORE" : NULL);
}

static void draw_debug(void) {
    header_touch("DEBUG", 1);
    int gy = (int)(cur_y >= 0 && cur_screen == S_DEBUG ? cur_y : row_target_y(S_DEBUG, sel));
    draw_focus(32, gy, W - 64, 44, 10, 0);
    row_touch(86, 44, "LOGS", log_label[c.logs], sel == DEBUG_LOGS, 0, c.logs > 0, 0, 1);
    row_touch(140, 44, "SAVE LOGS", "ZIP", sel == DEBUG_SAVE_LOGS, 0, 0, 0, 0);
    row_touch(194, 44, "CLEAN LOGS", "ASK", sel == DEBUG_CLEAN_LOGS, 0, 0, 0, 0);
    if (status[0]) text(40, 268, status, 16, C_GREEN);
    footer_touch(1, 1, "RUN");
}

static void draw_browse(void) {
    header_touch("SELECT DISC", 1);
    text(40, 76, b_dir, 14, C_CYAN);
    if (b_count > 0) {
        int gy = (int)(cur_y >= 0 && cur_screen == S_BROWSE ? cur_y : row_target_y(S_BROWSE, sel));
        draw_focus(32, gy, W - 64, 40, 10, 0);
        for (int i = b_scroll; i < b_count && i < b_scroll + BROWSE_VIS; i++) {
            const char *tag = b_items[i].is_dir ? "[DIR]" : ".CUE/.BIN";
            row_touch(116 + (i - b_scroll) * 44, 40, b_items[i].name, tag, i == sel, 0, !b_items[i].is_dir, 0, 0);
        }
    } else {
        text_c(W / 2, 210, "NO .CUE / .BIN IN game/", 18, C_NOTE);
    }
    footer_touch(0, 1, "SYSTEM DIALOG");
}

static void draw_loading(void) {
    char stage[80];
    snprintf(stage, sizeof(stage), "%s...", launch_stage);
    load_draw(&c, plat_now_ms() - launch_t0, stage, launch_pct);
}

static void draw_error(void) {
    header_touch("COULD NOT START", 1);
    rrect(50, 110, W - 100, 210, 12, C_PANEL, 256);
    rrect_line(50, 110, W - 100, 210, 12, C_RED);
    text(76, 134, msg, 20, C_RED);
    if (last_err[0]) text(76, 174, last_err, 14, C_GOLD);
    footer_touch(0, 1, "OK");
}

static void draw_screen(void) {
    if (screen != S_SPLASH && screen != S_LOADING) {
        background((int)(plat_now_ms() & 0x7fffffff));
    }
    switch (screen) {
    case S_SPLASH: draw_splash(); break;
    case S_HOME: draw_home(); break;
    case S_GAME: draw_game(); break;
    case S_HUD: draw_hud(); break;
    case S_CONTROLS: draw_controls(); break;
    case S_SAVES: draw_saves(); break;
    case S_BACKUPS: draw_backups(); break;
    case S_BACKUP_CONFIRM:
        draw_backups();
        modal_touch("RESTORE BACKUP?", "Replace card1.mcd with:", bk_count > 0 ? bk_items[sel].label : "",
                    "Your current card1.mcd is backed up first.", "RESTORE", "CANCEL", C_ORANGE);
        break;
    case S_DEBUG: draw_debug(); break;
    case S_BROWSE: draw_browse(); break;
    case S_RESET:
        draw_home();
        modal_touch("RESET SETTINGS?", "Every setting goes back to the tuned defaults.", "The memory cards are never touched.",
                    "", "YES, RESET", "NO, KEEP", C_ORANGE);
        break;
    case S_CLEAN:
        draw_debug();
        modal_touch("CLEAN LOGS AND DUMPS?", "Delete logs/, crash-*.txt and old log zips?", "Saves and disc are never touched.",
                    "", "YES, CLEAN", "NO, KEEP", C_ORANGE);
        break;
    case S_LOADING: draw_loading(); break;
    case S_ERROR: draw_error(); break;
    default: draw_home(); break;
    }
}

static void draw(void) {
    ui_set_lang(c.lang);
    draw_screen();
    if (!trans_active || !trans_to) return;
    long dt = plat_now_ms() - trans_t0;
    if (dt >= TRANS_MS) { trans_active = 0; return; }
    memcpy(trans_to, fb, (size_t)ui_w * ui_h * sizeof(uint32_t));
    int p = (int)(dt * 256 / TRANS_MS), inv = 256 - p, ease = 256 - (inv * inv >> 8);
    int s_from = trans_dir > 0 ? (65536 - (ease * 18000 >> 8)) : (65536 + (ease * 10000 >> 8));
    int s_to = trans_dir > 0 ? (65536 + ((256 - ease) * 10500 >> 8)) : (65536 - ((256 - ease) * 11000 >> 8));
    int fcy = trans_cy * ui_scale, hcy = ui_h / 2, hcx = ui_w / 2;
    for (int y = 0; y < ui_h; y++) {
        int sy_f = fcy + (((y - fcy) * s_from) >> 16);
        int sy_t = hcy + (((y - hcy) * s_to) >> 16);
        for (int x = 0; x < ui_w; x++) {
            int sx_f = hcx + (((x - hcx) * s_from) >> 16);
            int sx_t = hcx + (((x - hcx) * s_to) >> 16);
            uint32_t cf = (sx_f >= 0 && sx_f < ui_w && sy_f >= 0 && sy_f < ui_h) ? trans_from[sy_f * ui_w + sx_f] : C_BG0;
            uint32_t ct = (sx_t >= 0 && sx_t < ui_w && sy_t >= 0 && sy_t < ui_h) ? trans_to[sy_t * ui_w + sx_t] : C_BG0;
            fb[y * ui_w + x] = mix(cf, ct, ease);
        }
    }
}

static void activate_game_row(int row, int dir) {
    switch (row) {
    case GAME_LANG: c.lang = (c.lang + dir + 8) % 8; break;
    case GAME_FF: c.ff = (c.ff + dir + 5) % 5; break;
    case GAME_LOAD: c.load = (c.load + dir + 5) % 5; break;
    case GAME_SKIP: c.skip = !c.skip; break;
    case GAME_BATTLES: c.battles = !c.battles; break;
    case GAME_HIT: if (c.battles) c.hit = !c.hit; break;
    case GAME_BTEXT: c.btext = (c.btext + dir + 3) % 3; break;
    case GAME_TOUCHPAD: c.touch_pad = !c.touch_pad; break;
    case GAME_DISC: pick_disc_android(); break;
    }
    cfg_save(&c);
}

static void activate_home_row(int row) {
    switch (row) {
    case HI_PLAY: start_game_flow(); break;
    case HI_GAME: go(S_GAME, 0, +1); break;
    case HI_HUD: go(S_HUD, 0, +1); break;
    case HI_CONTROLS: go(S_CONTROLS, 0, +1); break;
    case HI_SAVES: status[0] = 0; go(S_SAVES, 0, +1); break;
    case HI_DEBUG: status[0] = 0; go(S_DEBUG, 0, +1); break;
    case HI_RESET: go(S_RESET, 0, +1); break;
    }
}

static void on_touch(int code, int x, int y) {
    if (screen == S_SPLASH) {
        if (code == K_TOUCH_DOWN) go(S_HOME, 0, +1);
        return;
    }
    if (screen == S_LOADING) {
        if (code == K_TOUCH_DOWN) load_press(c.pad[A_CROSS].code);
        return;
    }
    if (code == K_TOUCH_MOVE && touch_drag_active && (screen == S_BROWSE || screen == S_BACKUPS)) {
        int dy = y - touch_drag_y0;
        if (dy > 32) {
            if (screen == S_BROWSE && b_scroll > 0) b_scroll--;
            if (screen == S_BACKUPS && bk_scroll > 0) bk_scroll--;
            touch_drag_y0 = y;
            touch_drag_scrolled = 1;
        } else if (dy < -32) {
            if (screen == S_BROWSE && b_scroll + BROWSE_VIS < b_count) b_scroll++;
            if (screen == S_BACKUPS && bk_scroll + LIST_VIS < bk_count) bk_scroll++;
            touch_drag_y0 = y;
            touch_drag_scrolled = 1;
        }
        return;
    }
    if (code == K_TOUCH_DOWN) {
        touch_drag_active = 1;
        touch_drag_y0 = y;
        touch_drag_scrolled = 0;
        return;
    }
    if (code != K_TOUCH_UP) return;
    int was_drag = touch_drag_scrolled;
    touch_drag_active = 0;
    touch_drag_scrolled = 0;
    if (was_drag) return;

    if (screen == S_RESET) {
        if (in_rect(x, y, 90, 294, 210, 44)) {
            cfg_defaults(&c); cfg_save(&c); go(S_HOME, HI_RESET, -1);
        } else if (in_rect(x, y, 340, 294, 210, 44) || in_rect(x, y, 520, 14, 98, 36)) {
            go(S_HOME, HI_RESET, -1);
        }
        return;
    }
    if (screen == S_BACKUP_CONFIRM) {
        if (in_rect(x, y, 90, 294, 210, 44) && bk_count > 0) {
            char err[128] = "";
            if (save_backup_restore(game_dir, bk_items[sel].name, c.backup_max, err, sizeof(err)) == 0) {
                snprintf(status, sizeof(status), "RESTORED %s", bk_items[sel].label);
            } else {
                snprintf(status, sizeof(status), "%s", err[0] ? err : "RESTORE FAILED");
            }
            go(S_SAVES, SAVES_RESTORE, -1);
        } else if (in_rect(x, y, 340, 294, 210, 44)) {
            go(S_BACKUPS, sel, -1);
        }
        return;
    }
    if (screen == S_CLEAN) {
        if (in_rect(x, y, 90, 294, 210, 44)) {
            int rm = logs_clean();
            snprintf(status, sizeof(status), "CLEANED %d FILES", rm);
            go(S_DEBUG, DEBUG_CLEAN_LOGS, -1);
        } else if (in_rect(x, y, 340, 294, 210, 44)) {
            go(S_DEBUG, DEBUG_CLEAN_LOGS, -1);
        }
        return;
    }

    if (screen != S_HOME && (in_rect(x, y, 520, 14, 98, 36) || in_rect(x, y, 28, H - 46, 126, 36))) {
        if (screen == S_BROWSE) go(S_GAME, GAME_DISC, -1);
        else if (screen == S_BACKUPS) go(S_SAVES, SAVES_RESTORE, -1);
        else if (screen == S_ERROR) go(return_screen, return_sel, -1);
        else { cfg_save(&c); go(S_HOME, 0, -1); }
        return;
    }
    if (in_rect(x, y, 486, H - 46, 126, 36)) {
        if (screen == S_HOME || screen == S_GAME || screen == S_DEBUG) {
            start_game_flow();
            return;
        }
    }

    if (screen == S_HOME) {
        for (int i = 0; i < HI_COUNT; i++) {
            if (in_rect(x, y, 32, 78 + i * 48, W - 64, 44)) {
                sel = i;
                activate_home_row(i);
                return;
            }
        }
    } else if (screen == S_GAME) {
        for (int i = 0; i < GAME_N; i++) {
            if (in_rect(x, y, 32, 76 + i * GAME_ROW_STEP, W - 64, GAME_ROW_STEP)) {
                sel = i;
                int dir = (x >= 380 && x < 490) ? -1 : +1;
                activate_game_row(i, dir);
                return;
            }
        }
        if (in_rect(x, y, 240, H - 46, 160, 36)) activate_game_row(sel, +1);
    } else if (screen == S_HUD) {
        if (in_rect(x, y, 240, H - 46, 160, 36)) {
            unsigned dm = 0;
            if (dup_hud(&c, &dm)) {
                Cfg d; cfg_defaults(&d);
                memcpy(c.hud, d.hud, sizeof(c.hud));
            } else if (sel < H_COUNT) {
                c.hud[sel] = hud_cycle(c.hud[sel], +1);
            }
            cfg_save(&c);
            return;
        }
        for (int i = 0; i < H_COUNT + 2; i++) {
            if (in_rect(x, y, 32, 174 + i * 25, W - 64, 25)) {
                sel = i;
                int dir = (x >= 380 && x < 500) ? -1 : +1;
                if (i < H_COUNT) c.hud[i] = hud_cycle(c.hud[i], dir);
                else if (i == H_COUNT) c.bmem = !c.bmem;
                else c.bbuf = (c.bbuf + dir + 3) % 3;
                cfg_save(&c);
                return;
            }
        }
    } else if (screen == S_CONTROLS) {
        if (in_rect(x, y, 240, H - 46, 160, 36)) {
            Cfg d; cfg_defaults(&d);
            memcpy(c.pad, d.pad, sizeof(c.pad));
            cfg_save(&c);
            return;
        }
        if (in_rect(x, y, 32, 74, W - 64, 38)) {
            sel = 0;
            int dir = (x >= 380 && x < 490) ? -1 : +1;
            int l = cfg_layout(&c);
            if (l == LAYOUT_CUSTOM) l = LAYOUT_PS1;
            else l = (l + dir + 3) % 3;
            cfg_set_layout(&c, l);
            cfg_save(&c);
            return;
        }
        for (int i = 0; i < A_COUNT; i++) {
            if (in_rect(x, y, 32, 122 + i * 37, W - 64, 35)) {
                sel = i + 1;
                capture = !capture;
                return;
            }
        }
    } else if (screen == S_SAVES) {
        for (int i = 0; i < SAVES_N; i++) {
            if (in_rect(x, y, 32, 86 + i * 52, W - 64, 46)) {
                sel = i;
                int dir = (x >= 380 && x < 490) ? -1 : +1;
                if (i == SAVES_SYNC && cloud.registered && !cloud.revoked) c.sync_enabled = !c.sync_enabled;
                else if (i == SAVES_INTERVAL) {
                    static const int ivs[] = {0, 5, 10, 15, 20, 30};
                    int k = 0;
                    for (int j = 0; j < 6; j++) if (c.sync_interval_min == ivs[j]) k = j;
                    c.sync_interval_min = ivs[(k + dir + 6) % 6];
                } else if (i == SAVES_BACKUPS) {
                    c.backup_max += dir * 5;
                    if (c.backup_max < 1) c.backup_max = 100;
                    if (c.backup_max > 100) c.backup_max = 1;
                } else if (i == SAVES_NOW && cloud.registered && !cloud.revoked) {
                    int conf = 0;
                    if (cloud_sync_card1(&cloud, game_dir, c.backup_max, 0, &conf, status, sizeof(status)) == 0 && !status[0]) {
                        snprintf(status, sizeof(status), "SYNC COMPLETE");
                    }
                } else if (i == SAVES_RESTORE) {
                    bk_count = save_backup_list(game_dir, bk_items, BACKUP_MAX_LIST);
                    bk_scroll = 0;
                    go(S_BACKUPS, 0, +1);
                    return;
                }
                cfg_save(&c);
                return;
            }
        }
    } else if (screen == S_BACKUPS) {
        for (int i = bk_scroll; i < bk_count && i < bk_scroll + LIST_VIS; i++) {
            if (in_rect(x, y, 32, 114 + (i - bk_scroll) * 44, W - 64, 40)) {
                sel = i;
                go(S_BACKUP_CONFIRM, sel, +1);
                return;
            }
        }
    } else if (screen == S_DEBUG) {
        for (int i = 0; i < DEBUG_N; i++) {
            if (in_rect(x, y, 32, 86 + i * 54, W - 64, 44)) {
                sel = i;
                if (i == DEBUG_LOGS) { c.logs = (c.logs + 1) % 3; cfg_save(&c); }
                else if (i == DEBUG_SAVE_LOGS) {
                    char nm[128] = "";
                    if (logs_save(nm, sizeof(nm)) == 0) snprintf(status, sizeof(status), "SAVED %s", nm);
                    else snprintf(status, sizeof(status), "SAVE LOGS FAILED");
                } else if (i == DEBUG_CLEAN_LOGS) {
                    go(S_CLEAN, 0, +1);
                }
                return;
            }
        }
    } else if (screen == S_BROWSE) {
        if (in_rect(x, y, 240, H - 46, 160, 36)) {
            if (android_pick_disc_files() == 0) {
                find_disc();
                if (disc_path[0]) { cfg_save(&c); go(S_GAME, GAME_DISC, -1); }
            }
            return;
        }
        for (int i = b_scroll; i < b_count && i < b_scroll + BROWSE_VIS; i++) {
            if (in_rect(x, y, 32, 116 + (i - b_scroll) * 44, W - 64, 40)) {
                sel = i;
                if (b_items[i].is_dir) {
                    if (strcmp(b_items[i].name, "..") == 0) {
                        char *sl = strrchr(b_dir, '/');
                        if (sl && sl != b_dir) *sl = 0;
                        else strcpy(b_dir, "/");
                    } else {
                        char nxt[1400];
                        snprintf(nxt, sizeof(nxt), "%s/%s", strcmp(b_dir, "/") ? b_dir : "", b_items[i].name);
                        snprintf(b_dir, sizeof(b_dir), "%.1023s", nxt);
                    }
                    b_count = browse_read(b_dir, b_items, 256);
                    b_scroll = 0;
                    sel = 0;
                } else {
                    snprintf(disc_path, sizeof(disc_path), "%s/%s", strcmp(b_dir, "/") ? b_dir : "", b_items[i].name);
                    cfg_save(&c);
                    go(S_GAME, GAME_DISC, -1);
                }
                return;
            }
        }
    }
}

/* Android's Back: the same step back as the BACK pill or a modal's cancel. On the home screen it does nothing, so a
 * stray edge swipe never leaves the app. */
static void on_back(void) {
    if (capture) { capture = 0; return; }
    switch (screen) {
    case S_SPLASH: case S_HOME: case S_LOADING: return;
    case S_RESET: go(S_HOME, HI_RESET, -1); return;
    case S_BACKUP_CONFIRM: go(S_BACKUPS, sel, -1); return;
    case S_CLEAN: go(S_DEBUG, DEBUG_CLEAN_LOGS, -1); return;
    case S_BROWSE: go(S_GAME, GAME_DISC, -1); return;
    case S_BACKUPS: go(S_SAVES, SAVES_RESTORE, -1); return;
    case S_ERROR: go(return_screen, return_sel, -1); return;
    default: cfg_save(&c); go(S_HOME, 0, -1); return;
    }
}

static void on_key(int code) {
    if (code == K_BACK) { on_back(); return; }
    if (screen == S_SPLASH) { go(S_HOME, 0, +1); return; }
    if (screen == S_LOADING) { load_press(code); return; }
    if (capture) {
        if (code == K_START || code == K_SELECT) { capture = 0; return; }
        if (screen == S_CONTROLS && sel > 0) c.pad[act_order[sel - 1]].code = code;
        capture = 0;
        cfg_save(&c);
        return;
    }
    if (code == K_UP) {
        int n = screen == S_HOME ? HI_COUNT : screen == S_GAME ? GAME_N : screen == S_HUD ? H_COUNT + 2 :
                screen == S_CONTROLS ? (A_COUNT + 1) : screen == S_SAVES ? SAVES_N : screen == S_DEBUG ? DEBUG_N :
                screen == S_BROWSE ? b_count : screen == S_BACKUPS ? bk_count : 1;
        if (n > 0) sel = (sel - 1 + n) % n;
        if (screen == S_BROWSE && sel < b_scroll) b_scroll = sel;
        if (screen == S_BACKUPS && sel < bk_scroll) bk_scroll = sel;
    } else if (code == K_DOWN) {
        int n = screen == S_HOME ? HI_COUNT : screen == S_GAME ? GAME_N : screen == S_HUD ? H_COUNT + 2 :
                screen == S_CONTROLS ? (A_COUNT + 1) : screen == S_SAVES ? SAVES_N : screen == S_DEBUG ? DEBUG_N :
                screen == S_BROWSE ? b_count : screen == S_BACKUPS ? bk_count : 1;
        if (n > 0) sel = (sel + 1) % n;
        if (screen == S_BROWSE && sel >= b_scroll + BROWSE_VIS) b_scroll = sel - BROWSE_VIS + 1;
        if (screen == S_BACKUPS && sel >= bk_scroll + LIST_VIS) bk_scroll = sel - LIST_VIS + 1;
    } else if (code == K_LEFT || code == K_RIGHT) {
        int dir = code == K_LEFT ? -1 : +1;
        if (screen == S_GAME) activate_game_row(sel, dir);
        else if (screen == S_HUD && sel < H_COUNT) { c.hud[sel] = hud_cycle(c.hud[sel], dir); cfg_save(&c); }
        else if (screen == S_CONTROLS && sel == 0) {
            int l = cfg_layout(&c);
            cfg_set_layout(&c, l == LAYOUT_CUSTOM ? LAYOUT_PS1 : (l + dir + 3) % 3);
            cfg_save(&c);
        }
    } else if (code == btn_ok() || code == K_SOUTH) {
        if (screen == S_HOME) activate_home_row(sel);
        else if (screen == S_GAME) activate_game_row(sel, +1);
        else if (screen == S_DEBUG) {
            if (sel == DEBUG_LOGS) { c.logs = (c.logs + 1) % 3; cfg_save(&c); }
            else if (sel == DEBUG_CLEAN_LOGS) go(S_CLEAN, 0, +1);
        }
    } else if (code == btn_back() || code == K_EAST) {
        if (screen != S_HOME) { cfg_save(&c); go(S_HOME, 0, -1); }
    } else if (code == btn_start()) {
        start_game_flow();
    }
}

int dw3_android_launcher_run(int kind, const char *script, int *out_touch_pad, int *out_log_level,
                             char *out_disc, size_t out_disc_sz) {
    is_android = 1;
    is_pc = 0;
    is_portmaster = 0;
    android_done = 0;
    android_cancelled = 0;
    screen = S_SPLASH;
    splash_t0 = -1;
    sel = 0;
    cur_y = -1.0f;

    if (plat_open(kind, script) != 0) return -1;
    trans_from = malloc((size_t)ui_w * ui_h * sizeof(uint32_t));
    trans_to = malloc((size_t)ui_w * ui_h * sizeof(uint32_t));

    cfg_load(&c);
    find_disc();
    if (!disc_path[0]) cfg_load(&c);
    cloud_state_load(&cloud, game_dir);

    while (!android_done) {
        if (screen == S_SPLASH && splash_t0 >= 0 && plat_now_ms() - splash_t0 >= SPLASH_MS) {
            go(S_HOME, 0, +1);
        }
        if (screen == S_LOADING) {
            long now = plat_now_ms();
            load_step(now - launch_prev_ms);
            launch_prev_ms = now;
            long dt = now - launch_t0;
            launch_pct = dt < 450 ? (int)(dt * 70 / 450) : 100;
            snprintf(launch_stage, sizeof(launch_stage), "%s", dt < 450 ? "PREPARING" : "LAUNCHING");
            if (dt >= 650) {
                android_done = 1;
                break;
            }
        }
        int ty = row_target_y(screen, sel);
        if (ty >= 0) {
            if (cur_screen != screen || cur_y < 0.0f) { cur_y = (float)ty; cur_screen = screen; }
            else cur_y += ((float)ty - cur_y) * 0.42f;
        }
        draw();
        plat_present();

        Ev ev;
        int rc = plat_poll(&ev, 16);
        if (rc < 0) {
            android_cancelled = (kind != PLAT_NULL);
            break;
        }
        if (rc > 0 && ev.code) {
            if (ev.code == K_TOUCH_DOWN || ev.code == K_TOUCH_MOVE || ev.code == K_TOUCH_UP) {
                on_touch(ev.code, ev.x, ev.y);
            } else if (ev.down) {
                on_key(ev.code);
            }
        }
    }

    free(trans_from); trans_from = NULL;
    free(trans_to); trans_to = NULL;
    plat_release();

    if (out_touch_pad) *out_touch_pad = c.touch_pad;
    if (out_log_level) *out_log_level = c.logs;
    if (out_disc && out_disc_sz > 0) snprintf(out_disc, out_disc_sz, "%s", disc_path);
    return android_cancelled ? 1 : 0;
}

#ifdef DW3_ANDROID_STANDALONE_TEST
int main(int argc, char **argv) {
    int kind = PLAT_SDL, tp = 1, lg = 0;
    const char *script = NULL;
    char disc[1200] = "";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--null") && i + 1 < argc) { kind = PLAT_NULL; script = argv[++i]; }
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) plat_shot_dir = argv[++i];
        else if (!strcmp(argv[i], "--dir") && i + 1 < argc) snprintf(game_dir, sizeof(game_dir), "%s", argv[++i]);
    }
    int rc = dw3_android_launcher_run(kind, script, &tp, &lg, disc, sizeof(disc));
    printf("android-launcher rc=%d touch_pad=%d logs=%d disc=%s\n", rc, tp, lg, disc);
    return rc;
}
#endif
