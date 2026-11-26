/* Parallel touch-first launcher for Android (arm64-v8a).
 * Keeps the Digivice aesthetic of launcher/main.c while increasing clickable spacing, adding touch hitboxes
 * (< BACK header pill, full-width rows, < / > value regions, clickable footer pills, tap-to-jump loading),
 * exposing the FLOATING PAD toggle in GAME settings, and supporting native Android controllers simultaneously. */
#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cfg.h"
#include "migrate.h"
#include "cloud.h"
#include "i18n.h"
#include "loading.h"
#include "plat.h"
#include "ui.h"
#include "update.h"
#include "touch_pad.h"

#include <SDL3/SDL.h>

/* psxstack's build stamp (port_version.c): "1.0.0" on a release, a suffix on any other build (update.h). */
extern const char port_version[];

#ifdef __ANDROID__
extern int android_pick_disc_files(void);
#else
static int android_pick_disc_files(void) { return -1; }
#endif

enum {
    S_SPLASH, S_HOME, S_GAME, S_VIDEO, S_HUD, S_CONTROLS, S_SAVES, S_BACKUPS, S_BACKUP_CONFIRM,
    S_DEBUG, S_BROWSE, S_RESET, S_CLEAN, S_LOADING, S_ERROR, S_DISC,
    S_CLOUD, S_PAIR_HOST, S_PAIR_ENTER, S_DEVICES, S_REVOKE_CONFIRM, S_RENEW_CONFIRM, S_CONFLICT, S_CONFLICT_CONFIRM,
    S_SAVE_NOTICE, S_SAVE_NOTICE_CONFIRM
};
enum { DISC_OK, DISC_BAD_IMAGE, DISC_BAD_FILE };
enum { HI_PLAY, HI_GAME, HI_VIDEO, HI_HUD, HI_CONTROLS, HI_SAVES, HI_DEBUG, HI_RESET, HI_COUNT };
#define HOME_ROW_Y 74
#define HOME_ROW_STEP 43
enum { GAME_LANG, GAME_FF, GAME_LOAD, GAME_SKIP, GAME_BATTLES, GAME_HIT, GAME_TOUCHPAD, GAME_DISC, GAME_N };
#define GAME_ROW_STEP 40
enum { VIDEO_WIDESCREEN, VIDEO_SCALING, VIDEO_HZ, VIDEO_RENDERER, VIDEO_FILTER, VIDEO_RESOLUTION, VIDEO_SUBPIXEL, VIDEO_N };
#define VIDEO_ROW_Y 76
#define VIDEO_ROW_STEP 42
enum { SAVES_SYNC, SAVES_INTERVAL, SAVES_BACKUPS, SAVES_NOW, SAVES_RESTORE, SAVES_ACCOUNT, SAVES_N };
/* CLOUD ACCOUNT rows: the first two when this device has no live key, all four once it has one. */
enum { CLOUD_REGISTER_OR_ADD, CLOUD_LINK, CLOUD_DEVICES, CLOUD_RENEW };
#define CLOUD_ROW_Y 182
#define CLOUD_ROW_STEP 50
enum { DEBUG_LOGS, DEBUG_SAVE_LOGS, DEBUG_CLEAN_LOGS, DEBUG_UPDATES, DEBUG_N };

static Cfg c;
static CloudState cloud;
static int screen = S_SPLASH, sel, capture, return_screen = S_HOME;
static int return_sel, disc_bad;
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

/* Cloud pairing (the flow of launcher/main.c's CLOUD ACCOUNT screens). The host shows a 6-digit code and approves the
 * request; the joining device types the code on the keypad and polls until the host approves. */
static CloudDevice dev_items[CLOUD_MAX_DEVICES];
static int dev_count, dev_scroll;
static char pair_code[8];
static long pair_expires_at;
static PairStatus pair_st;
static PairClaim pair_claim;
static char pair_digits[8];
static int pair_len, pair_waiting, pair_done, kp_sel;
static long pair_last_poll_ms;
static char pair_done_msg[160];
#define PAIR_POLL_MS 1800
/* The keypad: two rows of six keys, 1-6 / 7 8 9 0 DEL OK. */
#define KP_COLS 6
#define KP_KEYS 12
#define KP_DEL 10
#define KP_OK 11
#define KP_X 36
#define KP_Y 222
#define KP_W 88
#define KP_H 62
#define KP_GAP 8

/* A sync that found cloud and local both changed waits here for the player's choice. */
static int conflict_ret, conflict_ret_sel, conflict_choice, conflict_sel, conflict_then_play;
static int conflict_step;
static long conflict_tap_ms;
static char conflict_cloud_info[96], conflict_local_info[96];
#define CONFLICT_PILL_Y 296
#define CONFLICT_PILL_W 176
#define CONFLICT_PILL_H 48

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
static const char *buf_label[3] = {"OFF", "SHORT", "LONG"};
static const char *log_label[3] = {"OFF", "SOFT", "HEAVY"};
static const char *lay_label[4] = {"PS1", "XBOX", "NINTENDO", "CUSTOM"};
static const char *filt_label[5] = {"NONE", "SHARP", "SCANLINES", "CRT", "SMOOTH"};
static const char *vert_label[3] = {"PS1", "SMOOTH", "SMOOTH + TEXTURES"};
static const char *act_label[A_COUNT] = {"CONFIRM", "CARD", "BACK", "MENU", "L1", "R1", "START", "SELECT"};
static const int act_order[A_COUNT] = {A_CROSS, A_CIRCLE, A_TRIANGLE, A_SQUARE, A_L1, A_R1, A_START, A_SELECT};
static int notice_step;
static long notice_tap_ms;
static const char *hud_label[H_COUNT] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "BAG", "RUN", "INFO", "BLAST"};
/* the "Fight button" row (cfg_fight_place + 1): neither layout reads CUSTOM, and choosing on it gives the bottom */
static const char *fight_label[] = {"CUSTOM", "BOTTOM", "RIGHT"};
#define HUD_ROWS (H_COUNT + 3)

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
    case S_HOME: return HOME_ROW_Y + idx * HOME_ROW_STEP;
    case S_GAME: return 76 + idx * GAME_ROW_STEP;
    case S_VIDEO: return VIDEO_ROW_Y + idx * VIDEO_ROW_STEP;
    case S_HUD: return 174 + idx * 23; /* 23, not 25: the eleventh row (Fight button) clears the footer */
    case S_CONTROLS: return idx == 0 ? 74 : 122 + (idx - 1) * 37;
    case S_SAVES: return 86 + idx * 52;
    case S_BACKUPS: return 114 + (idx - bk_scroll) * 44;
    case S_BROWSE: return 116 + (idx - b_scroll) * 44;
    case S_DEBUG: return 86 + idx * 54;
    case S_CLOUD: return CLOUD_ROW_Y + idx * CLOUD_ROW_STEP;
    case S_DEVICES: return 114 + (idx - dev_scroll) * 44;
    case S_SAVE_NOTICE: return 226 + idx * 54;
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
    capture = 0;
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

static int pick_newest_game_disc(void) {
    char dir[700], best[512] = "";
    snprintf(dir, sizeof(dir), "%s/game", game_dir);
    DIR *d = opendir(dir);
    if (!d) return 0;
    time_t best_mt = 0;
    int best_cue = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        int is_cue = n > 4 && strcasecmp(e->d_name + n - 4, ".cue") == 0;
        int is_bin = n > 4 && strcasecmp(e->d_name + n - 4, ".bin") == 0;
        if (!is_cue && !is_bin) continue;
        char p[1400];
        snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (!best[0] || st.st_mtime > best_mt || (st.st_mtime == best_mt && is_cue && !best_cue)) {
            best_mt = st.st_mtime;
            best_cue = is_cue;
            snprintf(best, sizeof(best), "%.511s", e->d_name);
        }
    }
    closedir(d);
    if (!best[0]) return 0;
    snprintf(disc_path, sizeof(disc_path), "%s/%s", dir, best);
    return 1;
}

static void pick_disc_android(void) {
    if (android_pick_disc_files() == 0) {
        if (!pick_newest_game_disc()) find_disc();
        if (disc_path[0]) {
            cfg_save(&c);
            snprintf(status, sizeof(status), "DISC IMPORTED");
            return;
        }
    }
    open_browse();
}

static void draw(void);

static int cloud_active(void) { return cloud.registered && !cloud.revoked; }

/* Cloud calls block the loop (a few seconds at worst, offline): put a line on screen before one starts so the tap
 * visibly did something. */
static void busy(const char *what) {
    draw();
    rect(0, H / 2 - 30, W, 60, C_BG0, 225);
    rect(0, H / 2 - 30, W, 1, C_GOLD, 200);
    rect(0, H / 2 + 29, W, 1, C_GOLD, 200);
    text_c(W / 2, H / 2 - 11, S(what), 19, C_GOLD);
    plat_present();
}

static void fmt_epoch(long ts, char *out, size_t n) {
    if (ts <= 0) { snprintf(out, n, "?"); return; }
    time_t t = (time_t)ts;
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%d %H:%M", &tm);
}

/* then_play: the sync ran on the way into the game, which goes on once the conflict is settled (or put off). */
static void open_conflict(int ret, int ret_sel, int then_play) {
    conflict_ret = ret;
    conflict_ret_sel = ret_sel;
    conflict_then_play = then_play;
    conflict_sel = 0;
    char cts[32], lts[32], csha[9] = "--------", lsha[65] = "";
    fmt_epoch(cloud.cloud_updated_at, cts, sizeof(cts));
    if (cloud.cloud_sha256[0]) snprintf(csha, sizeof(csha), "%.8s", cloud.cloud_sha256);
    snprintf(conflict_cloud_info, sizeof(conflict_cloud_info), "%s: REV #%d   %s   [%s]", S("CLOUD"),
             cloud.cloud_seq, cts, csha);
    char card_p[1024];
    snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
    struct stat st;
    if (stat(card_p, &st) == 0 && save_is_valid_mc(card_p, lsha)) {
        fmt_epoch((long)st.st_mtime, lts, sizeof(lts));
        snprintf(conflict_local_info, sizeof(conflict_local_info), "%s: card1.mcd   %s   [%.8s]", S("LOCAL"), lts, lsha);
    } else {
        snprintf(conflict_local_info, sizeof(conflict_local_info), "%s: -", S("LOCAL"));
    }
    go(S_CONFLICT, 0, +1);
}

static void start_game(int sync);

static void start_game_flow(void) { start_game(1); }

static void conflict_finish(const char *res, int proceed) {
    go(conflict_ret, conflict_ret_sel, -1);
    snprintf(status, sizeof(status), "%s", res ? res : "");
    if (proceed && conflict_then_play) start_game(0);
}

/* Minimal SHA-1 for pre-launch disc verification on Android (matches psxstack/runtime/disc.c). */
static void sha1_block(uint32_t h[5], const uint8_t b[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)b[i * 4] << 24) | ((uint32_t)b[i * 4 + 1] << 16) | ((uint32_t)b[i * 4 + 2] << 8) | b[i * 4 + 3];
    for (int i = 16; i < 80; i++) {
        uint32_t t = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
        w[i] = (t << 1) | (t >> 31);
    }
    uint32_t a = h[0], bb = h[1], cc = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (bb & cc) | ((~bb) & d); k = 0x5A827999u; }
        else if (i < 40) { f = bb ^ cc ^ d; k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (bb & cc) | (bb & d) | (cc & d); k = 0x8F1BBCDCu; }
        else { f = bb ^ cc ^ d; k = 0xCA62C1D6u; }
        uint32_t tmp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d; d = cc; cc = (bb << 30) | (bb >> 2); bb = a; a = tmp;
    }
    h[0] += a; h[1] += bb; h[2] += cc; h[3] += d; h[4] += e;
}

static int sha1_file_hex(const char *path, char out[41]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    uint8_t *buf = malloc(65536), tail[128];
    if (!buf) { fclose(f); return -1; }
    uint64_t total = 0;
    size_t rem = 0, r;
    while ((r = fread(buf, 1, 65536, f)) > 0) {
        total += r;
        size_t off = 0;
        if (rem) {
            size_t need = 64 - rem;
            if (r < need) { memcpy(tail + rem, buf, r); rem += r; continue; }
            memcpy(tail + rem, buf, need);
            sha1_block(h, tail);
            off = need; rem = 0;
        }
        while (off + 64 <= r) { sha1_block(h, buf + off); off += 64; }
        if (off < r) { rem = r - off; memcpy(tail, buf + off, rem); }
    }
    int io_err = ferror(f);
    fclose(f);
    free(buf);
    if (io_err) return -1;
    tail[rem++] = 0x80;
    while ((rem % 64) != 56) tail[rem++] = 0;
    uint64_t bits = total * 8;
    for (int i = 7; i >= 0; i--) tail[rem++] = (uint8_t)((bits >> (i * 8)) & 0xFF);
    for (size_t off = 0; off < rem; off += 64) sha1_block(h, tail + off);
    snprintf(out, 41, "%08x%08x%08x%08x%08x", h[0], h[1], h[2], h[3], h[4]);
    return 0;
}

static int disc_sha1_accepted(const char *hex) {
    return strcmp(hex, "457cb233349ba841e03b33d8060f8fbcadd45cb3") == 0 ||
           strcmp(hex, "fbfd1044e4ae0c6128e2e1a679fb6d72dc954b39") == 0;
}

static int disc_stamp_matches(const char *bin_path, const struct stat *st) {
    char sp[1024], line[4600];
    snprintf(sp, sizeof(sp), "%s/dw2003-port/disc-stamps", game_dir);
    FILE *f = fopen(sp, "r");
    if (!f) return 0;
    char *resolved = realpath(bin_path, NULL);
    const char *canon = resolved ? resolved : bin_path;
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        char hex[48], cpath[4096];
        long long sz = 0, sec = 0, nsec = 0;
        unsigned long long ino = 0, dev = 0;
        if (sscanf(line, "%40s %lld %lld.%lld %llu %llu %4095[^\r\n]", hex, &sz, &sec, &nsec, &ino, &dev, cpath) == 7) {
            if (disc_sha1_accepted(hex) &&
                sz == (long long)st->st_size && sec == (long long)st->st_mtime &&
                nsec == (long long)st->st_mtim.tv_nsec &&
                ino == (unsigned long long)st->st_ino && dev == (unsigned long long)st->st_dev &&
                strcmp(cpath, canon) == 0) {
                ok = 1;
                break;
            }
        }
    }
    free(resolved);
    fclose(f);
    return ok;
}

/* Pre-launch disc check on Android: dw3_game_main() runs in-process and port_fatal("disc: ...") calls exit(1).
 * Catching missing/unverified discs here lets Android open S_DISC ("CHECK YOUR DISC") instead of closing the app. */
static int disc_check_android(const char *path) {
    char bin[1400];
    size_t pl = strlen(path);
    last_err[0] = 0;
    if (pl > 4 && strcasecmp(path + pl - 4, ".cue") == 0) {
        FILE *f = fopen(path, "rb");
        if (!f) { snprintf(last_err, sizeof(last_err), "cannot open %s", path); return DISC_BAD_FILE; }
        char line[1024];
        int found = 0;
        while (fgets(line, sizeof(line), f)) {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "FILE", 4) != 0 || (p[4] != ' ' && p[4] != '\t')) continue;
            p += 5;
            while (*p == ' ' || *p == '\t') p++;
            char *name, *end;
            if (*p == '"') { name = p + 1; end = strchr(name, '"'); }
            else { name = p; end = strpbrk(name, " \t\r\n"); }
            if (!end || end == name) break;
            *end = 0;
            if (name[0] == '/') snprintf(bin, sizeof(bin), "%s", name);
            else {
                const char *sl = strrchr(path, '/');
                int dl = sl ? (int)(sl - path) + 1 : 0;
                snprintf(bin, sizeof(bin), "%.*s%s", dl, path, name);
            }
            found = 1;
            break;
        }
        fclose(f);
        if (!found) { snprintf(last_err, sizeof(last_err), "no FILE line in .cue"); return DISC_BAD_FILE; }
    } else {
        snprintf(bin, sizeof(bin), "%s", path);
    }
    struct stat st;
    if (stat(bin, &st) != 0 || !S_ISREG(st.st_mode)) {
        snprintf(last_err, sizeof(last_err), "cannot open %.200s", bin);
        return DISC_BAD_FILE;
    }
    if (st.st_size < 100LL * 1024 * 1024 || (st.st_size % 2352) != 0) return DISC_BAD_IMAGE;
    if (disc_stamp_matches(bin, &st)) return DISC_OK;
    busy("VERIFYING THE DISC");
    char hex[41];
    if (sha1_file_hex(bin, hex) != 0) {
        snprintf(last_err, sizeof(last_err), "cannot read %.200s", bin);
        return DISC_BAD_FILE;
    }
    if (disc_sha1_accepted(hex)) {
        return DISC_OK;
    }
    return DISC_BAD_IMAGE;
}

static void start_game(int sync) {
    if (!disc_path[0]) {
        pick_disc_android();
        if (!disc_path[0]) return;
    }
    disc_bad = disc_check_android(disc_path);
    if (disc_bad != DISC_OK) {
        return_screen = screen;
        return_sel = sel;
        go(S_DISC, 0, +1);
        return;
    }
    {
        char card_p[1024];
        snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
        if (save_is_valid_mc(card_p, NULL)) save_backup_rotate(game_dir, "card1.mcd", c.backup_max);
    }
    /* SAVES & CLOUD's CLOUD SYNC promises a pull before play. The game runs inside this process and Android kills it
     * rather than letting it exit, so there is no push on exit as on desktop: this same sync pushes the last session's
     * save on the next start, and SYNC NOW does it on demand. */
    if (sync && c.sync_enabled && cloud_active()) {
        busy("SYNCING SAVE...");
        int conf = 0;
        char smsg[160] = "";
        if (cloud_sync_card1(&cloud, game_dir, c.backup_max, 1, &conf, smsg, sizeof(smsg)) == 1 && conf) {
            open_conflict(screen, sel, 1);
            return;
        }
    }
    cloud_migrate_saves(&cloud, game_dir); /* after the sync: an old expansion save takes the current schema (migrate.h) */
    if (cfg_save(&c) != 0) {
        return_screen = screen;
        return_sel = sel;
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

/* The notice's page opens in the browser; nothing is downloaded (update.h). 0 when there is no notice to open. */
static int open_update_page(void) {
    UpdateInfo up;
    update_poll(&up);
    if (up.state != UPDATE_AVAILABLE || !up.url[0]) return 0;
    SDL_OpenURL(up.url);
    return 1;
}

static void toggle_update_check(void) {
    c.update_check = !c.update_check;
    cfg_save(&c);
    /* Turned on, it asks in this run already; turned off, a notice already shown stays until the next start. */
    update_check_start(game_dir, port_version, c.update_check);
}

static void draw_home(void) {
    header_touch("ANDROID TOUCH", 0);
    static const char *const names[HI_COUNT] = {
        "PLAY", "GAME", "VIDEO", "BATTLE HUD", "CONTROLS", "SAVES & CLOUD", "DEBUG", "RESET SETTINGS"
    };
    static const char *const hints[HI_COUNT] = {
        "Start Digimon World 3 with your saved settings.",
        "Language, fast-forward, skipping and battle text.",
        "Widescreen, GPU resolution, filters and 3D vertices.",
        "Where each battle action sits on your pad.",
        "Your buttons, and the letters the game shows.",
        "Cloud cross-play sync, 1-100 local save rotation and backup restore.",
        "Logs for the people who help you fix a crash.",
        "Put every setting back to the tuned defaults."
    };
    int gy = (int)(cur_y >= 0 && cur_screen == S_HOME ? cur_y : row_target_y(S_HOME, sel));
    draw_focus(32, gy, W - 64, 38, 10, 0);
    for (int i = 0; i < HI_COUNT; i++) {
        char sv[32];
        const char *val = NULL;
        int on = 0;
        switch (i) {
        case HI_PLAY: val = disc_path[0] ? "DISC READY" : "BROWSE..."; on = disc_path[0] != 0; break;
        case HI_GAME: val = "TUNING"; break;
        case HI_VIDEO:
            if (c.renderer_pc) val = "SOFTWARE";
            else { snprintf(sv, sizeof(sv), "GPU %dX", c.iscale); val = sv; on = 1; }
            break;
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
        row_touch(HOME_ROW_Y + i * HOME_ROW_STEP, 38, names[i], val, i == sel, (i == HI_PLAY && !disc_path[0]), on, 0, 0);
    }
    rect(0, H - 56, W, 56, C_BG0, 210);
    rect(0, H - 56, W, 1, C_EDGE, 256);
    text(36, H - 36, S(hints[sel]), 13, C_NOTE);
    draw_touch_pill(486, H - 46, 126, 36, disc_path[0] ? S("PLAY") : S("CHOOSE"), RGB(18, 64, 48), C_GREEN);
    UpdateInfo up;
    update_poll(&up);
    if (up.state == UPDATE_AVAILABLE) {
        char lbl[48];
        snprintf(lbl, sizeof(lbl), "%s %s", S("NEW"), up.latest);
        draw_touch_pill(452, 14, 166, 36, lbl, RGB(70, 52, 12), C_GOLD);
    }
}

static void draw_video(void) {
    header_touch("VIDEO", 1);
    int vw = (c.widescreen && c.renderer_pc == 0 ? 1536 : 1024) * c.iscale, vh = 512 * c.iscale;
    char res_val[48];
    snprintf(res_val, sizeof(res_val), "%dX (%dx%d VRAM)", c.iscale, vw, vh);
    const char *lab[VIDEO_N] = {
        "WIDESCREEN", "SCALING", "REFRESH RATE", "RENDERER", "FILTER", "RESOLUTION", "3D VERTICES"
    };
    const char *val[VIDEO_N] = {
        (c.widescreen && c.renderer_pc == 0) ? "AUTO" : "OFF",
        c.fill ? "FILL" : "INTEGER",
        c.hz == 60 ? "60 HZ" : "50 HZ",
        c.renderer_pc ? "SOFTWARE" : "GPU",
        c.renderer_pc ? "-" : filt_label[c.filter],
        c.renderer_pc ? "-" : res_val,
        (c.renderer_pc || c.iscale == 1) ? "-" : vert_label[c.subpixel]
    };
    static const char *const desc[VIDEO_N] = {
        "Battles and field follow the screen's shape, up to 16:9 (auto-selects GPU). Menus stay 4:3.",
        "FILL stretches cleanly to screen height; INTEGER uses exact multiples.",
        "60 HZ is the game's own NTSC mode (smoother). 50 HZ is the original PAL speed.",
        "GPU enables widescreen and shaders; SOFTWARE is exact PS1.",
        "GPU screen filter (SHARP keeps crisp pixels).",
        "Scales full PS1 VRAM (1024x512, 1536x512 in 16:9), not just 320x240.",
        "Above 1X: PS1 wobbles; SMOOTH steadies 3D; + TEXTURES fixes warping."
    };
    int gy = (int)(cur_y >= 0 && cur_screen == S_VIDEO ? cur_y : row_target_y(S_VIDEO, sel));
    draw_focus(32, gy, W - 64, 38, 10, 0);
    for (int i = 0; i < VIDEO_N; i++) {
        int on = i == VIDEO_HZ ? c.hz == 60
               : i == VIDEO_RESOLUTION ? c.iscale > 1
               : i == VIDEO_SUBPIXEL ? c.subpixel > 0
               : (strcmp(val[i], "OFF") != 0 && strcmp(val[i], "-") != 0);
        int arrows = strcmp(val[i], "-") != 0;
        row_touch(VIDEO_ROW_Y + i * VIDEO_ROW_STEP, 38, lab[i], val[i], i == sel, 0, on && arrows, 0, arrows);
    }
    text(36, 384, S(desc[sel < VIDEO_N ? sel : 0]), 13, C_NOTE);
    footer_touch(1, 1, "CHANGE");
}

static int has_existing_save(void) {
    char p[1024];
    snprintf(p, sizeof(p), "%s/card1.mcd", game_dir);
    return save_is_valid_mc(p, NULL);
}

static void fresh_save_reset(void) {
    char card_p[1024], ext_p[1024];
    save_backup_rotate(game_dir, "card1.mcd", c.backup_max);
    snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
    snprintf(ext_p, sizeof(ext_p), "%s/card1.def.json", game_dir);
    unlink(card_p);
    unlink(ext_p);
    c.v091_save_ok = 1;
    cfg_save(&c);
}

static void leave_splash(void) {
    if (!c.v091_save_ok) {
        if (has_existing_save()) {
            go(S_SAVE_NOTICE, 0, +1);
            return;
        }
        c.v091_save_ok = 1;
        cfg_save(&c);
    }
    go(S_HOME, 0, +1);
}

static void draw_save_notice(void) {
    header_touch("NEW IN v0.9.1", 0);
    rrect(36, 74, W - 72, 134, 12, C_PANEL, 256);
    rrect_line(36, 74, W - 72, 134, 12, C_GOLD);
    text(56, 88, S("START FRESH RECOMMENDED"), 20, C_GOLD);
    text(56, 120, S("v0.9.1 overhauls the story, NPC mini-arcs, and bonds."), 14, C_TEXT);
    text(56, 144, S("Continuing an old save can negatively affect your experience."), 14, C_TEXT);
    text(56, 170, S("Your current save is safely backed up in saves_backup/ first."), 13, C_GREEN);

    int gy = (int)(cur_y >= 0 && cur_screen == S_SAVE_NOTICE ? cur_y : row_target_y(S_SAVE_NOTICE, sel));
    draw_focus(32, gy, W - 64, 46, 10, sel == 1);
    row_touch(226, 46, "START FRESH (RECOMMENDED)", "BACKUP + NEW SAVE", sel == 0, 0, 1, 0, 0);
    row_touch(280, 46, "KEEP OLD SAVE", "NOT RECOMMENDED", sel == 1, 1, 0, 0, 0);
    footer_touch(0, 0, "SELECT");
}

static void draw_save_notice_confirm(void) {
    draw_save_notice();
    rrect(56, 108, W - 112, 256, 14, C_PANEL, 256);
    rrect_line(56, 108, W - 112, 256, 14, C_RED);
    text_c(W / 2, 128, S("KEEP OLD SAVE ANYWAY?"), 22, C_RED);
    text_c(W / 2, 170, S("Continuing an old save in v0.9.1 may miss new"), 14, C_TEXT);
    text_c(W / 2, 192, S("story choices, NPC mini-arcs, and ally bonds,"), 14, C_TEXT);
    text_c(W / 2, 214, S("negatively affecting your experience."), 14, C_GOLD);
    char pill[64];
    int step = notice_step < 3 ? notice_step + 1 : 3;
    snprintf(pill, sizeof(pill), "%s (%d/3)", S("CONFIRM"), step);
    draw_touch_pill(86, 294, 214, 44, pill, RGB(88, 22, 28), C_RED);
    draw_touch_pill(340, 294, 214, 44, S("BACK"), RGB(18, 52, 42), C_GREEN);
}

static void draw_game(void) {
    header_touch("GAME", 1);
    char disc_short[36] = "BROWSE...";
    if (disc_path[0]) {
        const char *b = strrchr(disc_path, '/');
        const char *n = b ? b + 1 : disc_path;
        if (strlen(n) > 26) snprintf(disc_short, sizeof(disc_short), "%.23s...", n);
        else snprintf(disc_short, sizeof(disc_short), "%s", n);
    }
    const char *lab[GAME_N] = {
        "LANGUAGE", "FAST-FORWARD", "FAST LOAD", "SKIP DIALOGUES", "SKIP BATTLE ANIMS",
        "   HIT REACTION", "FLOATING PAD", "DISC IMAGE"
    };
    const char *val[GAME_N] = {
        lang_label[c.lang < 0 || c.lang > 7 ? 0 : c.lang],
        ff_label[c.ff],
        load_label[c.load],
        c.skip ? "ON" : "OFF",
        c.battles ? "ON" : "OFF",
        c.battles ? (c.hit ? "ON" : "OFF") : "-",
        c.touch_pad ? "ON" : "OFF",
        disc_short
    };
    static const char *const desc[GAME_N] = {
        "Game text language (English, Portuguese BR, French, German, Italian, Spanish).",
        "Speed up gameplay. OFF, 2X, 4X, 8X or UNLIMITED.",
        "Reads the game's files faster. Movies keep normal speed. OFF, 2X, 4X, 8X or INSTANT.",
        "Dialogue boxes close on their own.",
        "Battle moves play without their animation.",
        "Keep the hit flinch while animations are skipped.",
        "On-screen floating touch controls during gameplay.",
        "Pick or change your .cue / .bin disc file."
    };
    int gy = (int)(cur_y >= 0 && cur_screen == S_GAME ? cur_y : row_target_y(S_GAME, sel));
    draw_focus(32, gy, W - 64, 38, 10, 0);
    for (int i = 0; i < GAME_N; i++) {
        int on = (i == GAME_DISC) ? (disc_path[0] != 0) : (strcmp(val[i], "OFF") != 0 && strcmp(val[i], "-") != 0);
        int arrows = (i != GAME_DISC);
        row_touch(76 + i * GAME_ROW_STEP, 36, lab[i], val[i], i == sel, 0, on, 0, arrows);
    }
    text(36, 400, S(desc[sel < GAME_N ? sel : 0]), 13, C_NOTE);
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
    const char *rl[HUD_ROWS] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "BAG", "RUN", "INFO", "BLAST", "REMEMBER CURSOR", "BUFFER INPUT",
                                "FIGHT BUTTON"};
    int gy = (int)(cur_y >= 0 && cur_screen == S_HUD ? cur_y : row_target_y(S_HUD, sel));
    int sbd = sel < H_COUNT && ((dm >> sel) & 1);
    draw_focus(32, gy, W - 64, 24, 6, sbd);
    for (int i = 0; i < HUD_ROWS; i++) {
        char g[16], v[48];
        if (i < H_COUNT) {
            glyph(c.hud[i], g);
            if (capture && i == sel) snprintf(v, sizeof(v), "%s", S("PRESS A BUTTON..."));
            else snprintf(v, sizeof(v), "< %s [%s] >", place_display(c.hud[i]), g);
        } else if (i == H_COUNT) {
            snprintf(v, sizeof(v), "< %s >", S(c.bmem ? "ON" : "OFF"));
        } else if (i == H_COUNT + 1) {
            snprintf(v, sizeof(v), "< %s >", S(buf_label[c.bbuf]));
        } else {
            snprintf(v, sizeof(v), "< %s >", S(fight_label[cfg_fight_place(&c) + 1]));
        }
        int y = 174 + i * 23, bd = i < H_COUNT && ((dm >> i) & 1);
        text(54, y + 3, S(rl[i]), 16, bd ? C_RED : i == sel ? C_GOLD : C_TEXT);
        text_r(W - 54, y + 3, v, 16, bd ? C_RED : i == sel ? C_GOLD : C_NOTE);
    }
    if (msg[0]) text_c(W / 2, H - 72, S(msg), 13, C_RED);
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
        if (capture && isel) snprintf(v, sizeof(v), "%s", S("PRESS A BUTTON..."));
        else snprintf(v, sizeof(v), "%s [%s]", place_display(c.pad[a].code), g);
        row_touch(y, 34, act_label[a], v, isel, bd, 0, 0, 0);
    }
    if (msg[0]) text_c(W / 2, H - 72, S(msg), 13, C_RED);
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
    char acc_val[32];
    if (cloud.revoked) snprintf(acc_val, sizeof(acc_val), "REVOKED");
    else if (cloud.registered) snprintf(acc_val, sizeof(acc_val), "REV #%d", cloud.cloud_seq > 0 ? cloud.cloud_seq : cloud.last_seq);
    else snprintf(acc_val, sizeof(acc_val), "NOT LINKED");
    row_touch(346, 44, "CLOUD ACCOUNT", acc_val, sel == SAVES_ACCOUNT, cloud.revoked, cloud_active(), 0, 0);
    if (status[0]) text(40, 398, status, 14, C_CYAN);
    footer_touch(0, 1, NULL);
}

static int cloud_n(void) { return cloud_active() ? 4 : 2; }

static void draw_cloud(void) {
    header_touch("CLOUD ACCOUNT", 1);
    rrect(32, 74, W - 64, 98, 10, C_PANEL, 256);
    rrect_line(32, 74, W - 64, 98, 10, cloud.revoked ? C_RED : cloud.registered ? C_CYAN : C_EDGE);
    if (cloud.revoked) {
        text(52, 84, "STATUS: REVOKED / BANNED", 18, C_RED);
        text(52, 112, cloud.revoke_reason[0] ? cloud.revoke_reason : "THIS DEVICE KEY IS NO LONGER VALID.", 15, C_ORANGE);
        text(52, 140, "Register again or link with a code to renew access.", 14, C_NOTE);
    } else if (cloud.registered) {
        int days_left = cloud.retention_days > 0 ? cloud.retention_days : 40;
        if (cloud.expires_at > 0) {
            long rem = cloud.expires_at - (long)time(NULL);
            days_left = rem > 0 ? (int)((rem + 86399) / 86400) : 0;
        }
        char l1[128], l2[128], l3[128];
        snprintf(l1, sizeof(l1), "%s: %.24s   %s: %d", S("DEVICE"), cloud.device_name, S("DEVICES"),
                 cloud.active_devices > 0 ? cloud.active_devices : 1);
        snprintf(l2, sizeof(l2), "%s: #%d   %s: %d", S("CLOUD REV"), cloud.cloud_seq > 0 ? cloud.cloud_seq : cloud.last_seq,
                 S("DAYS LEFT"), days_left);
        snprintf(l3, sizeof(l3), "%.60s", cloud.worker_url);
        text(52, 84, "STATUS: ACTIVE", 18, C_GREEN);
        text(52, 110, l1, 15, C_TEXT);
        text(52, 132, l2, 15, C_CYAN);
        text(52, 152, l3, 13, C_OFF);
    } else {
        text(52, 84, "STATUS: NOT REGISTERED", 18, C_GOLD);
        text(52, 112, "Register once to get a key for free cross-play sync.", 14, C_NOTE);
        text(52, 136, "Saves stay online for 40 days after your last activity.", 14, C_NOTE);
    }
    int gy = (int)(cur_y >= 0 && cur_screen == S_CLOUD ? cur_y : row_target_y(S_CLOUD, sel));
    draw_focus(32, gy, W - 64, 44, 10, 0);
    if (!cloud_active()) {
        row_touch(CLOUD_ROW_Y, 44, "REGISTER THIS DEVICE", "NEW KEY", sel == 0, 0, 1, 0, 0);
        row_touch(CLOUD_ROW_Y + CLOUD_ROW_STEP, 44, "LINK EXISTING ACCOUNT", "6-DIGIT CODE", sel == 1, 0, 0, 0, 0);
    } else {
        row_touch(CLOUD_ROW_Y, 44, "ADD NEW DEVICE", "GET CODE", sel == 0, 0, 1, 0, 0);
        row_touch(CLOUD_ROW_Y + CLOUD_ROW_STEP, 44, "LINK TO ANOTHER ACCOUNT", "ENTER CODE", sel == 1, 0, 0, 0, 0);
        row_touch(CLOUD_ROW_Y + 2 * CLOUD_ROW_STEP, 44, "LINKED DEVICES", "LIST / REVOKE", sel == 2, 0, 0, 0, 0);
        row_touch(CLOUD_ROW_Y + 3 * CLOUD_ROW_STEP, 44, "NEW ACCOUNT", "ROTATE KEY", sel == 3, 0, 0, 0, 0);
    }
    if (status[0]) text(40, 398, status, 14, C_CYAN);
    footer_touch(0, 1, NULL);
}

/* The 6 code boxes both pairing screens show: code[i] or a blank, the box at `active` lit. */
static void draw_code_boxes(int y, const char *code, int active) {
    int start_x = W / 2 - 170;
    for (int i = 0; i < 6; i++) {
        int bx = start_x + i * 56 + (i >= 3 ? 16 : 0), on = i == active;
        rrect(bx, y, 44, 58, 8, on ? C_FOCUS : C_BG1, 256);
        rrect_line(bx, y, 44, 58, 8, on ? C_GOLD : C_CYAN);
        char ch[2] = {code[i] ? code[i] : ' ', 0};
        text_c(bx + 22, y + 12, ch, 28, C_GOLD);
    }
}

static long pair_remaining(void) {
    long rem = pair_expires_at - (long)time(NULL);
    return rem > 0 ? rem : 0;
}

static void draw_pair_host(void) {
    header_touch("ADD NEW DEVICE", 1);
    rrect(32, 74, W - 64, 340, 14, C_PANEL, 230);
    rrect_line(32, 74, W - 64, 340, 14, C_CYAN);
    text_c(W / 2, 88, "ENTER THIS CODE ON YOUR OTHER DEVICE", 16, C_NOTE);
    draw_code_boxes(116, pair_code, -1);
    long rem = pair_remaining();
    if (pair_done) {
        text_c(W / 2, 236, "DEVICE APPROVED AND LINKED!", 20, C_GREEN);
        text_c(W / 2, 270, "The second device can now pull and push your saves.", 15, C_NOTE);
    } else if (!strcmp(pair_st.status, "awaiting_approval")) {
        char req[160];
        snprintf(req, sizeof(req), "%s %s (%s, %s)", S("REQUEST FROM:"),
                 pair_st.pending_name[0] ? pair_st.pending_name : "?",
                 pair_st.pending_platform[0] ? pair_st.pending_platform : "?",
                 pair_st.pending_country[0] ? pair_st.pending_country : "XX");
        text_c(W / 2, 206, req, 17, C_GOLD);
        text_c(W / 2, 236, "Approve adding this device to your cloud saves?", 15, C_TEXT);
        if (status[0]) text_c(W / 2, 266, status, 14, C_ORANGE);
        draw_touch_pill(90, 300, 210, 48, S("APPROVE"), RGB(22, 68, 48), C_GREEN);
        draw_touch_pill(340, 300, 210, 48, S("DENY"), RGB(68, 24, 30), C_RED);
    } else if (rem <= 0) {
        text_c(W / 2, 236, "CODE EXPIRED", 20, C_ORANGE);
        text_c(W / 2, 270, "Go back and get a new code.", 15, C_NOTE);
    } else {
        char tm[64];
        snprintf(tm, sizeof(tm), "%s %02ld:%02ld", S("EXPIRES IN"), rem / 60, rem % 60);
        text_c(W / 2, 196, tm, 15, rem < 30 ? C_ORANGE : C_CYAN);
        text_c(W / 2, 240, "WAITING FOR SECOND DEVICE TO ENTER CODE...", 16, C_TEXT);
        if (status[0]) text_c(W / 2, 274, status, 14, C_ORANGE);
    }
    footer_touch(0, 1, NULL);
}

static void kp_rect(int k, int *x, int *y) {
    *x = KP_X + (k % KP_COLS) * (KP_W + KP_GAP);
    *y = KP_Y + (k / KP_COLS) * (KP_H + KP_GAP);
}

static const char *kp_label(int k) {
    static const char *const lab[KP_KEYS] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "DEL", "OK"};
    return lab[k];
}

static void draw_pair_enter(void) {
    header_touch("LINK ACCOUNT", 1);
    text_c(W / 2, 80, "ENTER THE 6-DIGIT CODE FROM YOUR HOST DEVICE", 16, C_NOTE);
    draw_code_boxes(106, pair_digits, (!pair_waiting && !pair_done && pair_len < 6) ? pair_len : -1);
    if (pair_done) {
        text_c(W / 2, 196, "LINKED TO CLOUD ACCOUNT!", 20, C_GREEN);
        text_c(W / 2, 230, "Your cloud cross-play sync is ready.", 15, C_NOTE);
        if (pair_done_msg[0]) text_c(W / 2, 258, pair_done_msg, 14, C_CYAN);
    } else if (pair_waiting) {
        text_c(W / 2, 196, "WAITING FOR APPROVAL ON HOST DEVICE...", 18, C_GOLD);
        text_c(W / 2, 230, "Tap APPROVE on your first device.", 15, C_NOTE);
    } else {
        if (status[0]) text_c(W / 2, 186, status, 15, C_RED);
        else text_c(W / 2, 186, "Type the code, then tap OK.", 15, C_NOTE);
        for (int k = 0; k < KP_KEYS; k++) {
            int x, y;
            kp_rect(k, &x, &y);
            int live = k != KP_OK || pair_len == 6;
            uint32_t fg = k == KP_OK ? (live ? C_GREEN : C_OFF) : k == KP_DEL ? C_ORANGE : C_TEXT;
            rrect(x, y, KP_W, KP_H, 12, k == KP_OK && live ? RGB(18, 64, 48) : C_BG1, 235);
            rrect_line(x, y, KP_W, KP_H, 12, k == kp_sel ? C_GOLD : C_EDGE);
            text_c(x + KP_W / 2, y + (KP_H - 24) / 2, S(kp_label(k)), k < 10 ? 26 : 18, fg);
        }
    }
    footer_touch(0, 1, NULL);
}

static void draw_devices(void) {
    header_touch("LINKED DEVICES", 1);
    text(40, 76, "TAP A DEVICE TO REVOKE IT", 15, C_CYAN);
    if (dev_count > 0) {
        int gy = (int)(cur_y >= 0 && cur_screen == S_DEVICES ? cur_y : row_target_y(S_DEVICES, sel));
        draw_focus(32, gy, W - 64, 40, 10, 0);
        for (int i = dev_scroll; i < dev_count && i < dev_scroll + LIST_VIS; i++) {
            char lbl[96];
            snprintf(lbl, sizeof(lbl), "%.30s (%s/%s)", dev_items[i].name, dev_items[i].platform, dev_items[i].country);
            int revoked = !strcmp(dev_items[i].status, "revoked");
            const char *tag = dev_items[i].is_current ? "THIS DEVICE" : revoked ? "REVOKED" : "ACTIVE";
            row_touch(114 + (i - dev_scroll) * 44, 40, lbl, tag, i == sel, revoked, dev_items[i].is_current, 0, 0);
        }
    } else {
        text_c(W / 2, 220, "NO DEVICES FOUND", 18, C_NOTE);
    }
    if (status[0]) text(40, 398, status, 14, C_CYAN);
    footer_touch(0, 1, NULL);
}

static const char *const conflict_pill[3] = {"DOWNLOAD CLOUD", "UPLOAD LOCAL", "DECIDE LATER"};

static void draw_conflict(void) {
    header_touch("CLOUD CONFLICT", 1);
    rrect(32, 74, W - 64, 340, 14, C_PANEL, 230);
    rrect_line(32, 74, W - 64, 340, 14, C_GOLD);
    text(52, 90, "BOTH CLOUD AND LOCAL SAVES CHANGED", 18, C_GOLD);
    text(52, 128, conflict_cloud_info, 15, C_CYAN);
    text(52, 154, conflict_local_info, 15, C_TEXT);
    text(52, 196, "Pick the save to keep on both sides.", 15, C_NOTE);
    text(52, 220, "A backup of this device's save is kept either way.", 15, C_NOTE);
    for (int i = 0; i < 3; i++) {
        uint32_t fg = i == 0 ? C_GREEN : i == 1 ? C_ORANGE : C_NOTE;
        int x = 44 + i * (CONFLICT_PILL_W + 12);
        draw_touch_pill(x, CONFLICT_PILL_Y, CONFLICT_PILL_W, CONFLICT_PILL_H, S(conflict_pill[i]),
                        i == conflict_sel ? C_FOCUS : C_BG1, fg);
        if (i == conflict_sel) rrect_line(x - 3, CONFLICT_PILL_Y - 3, CONFLICT_PILL_W + 6, CONFLICT_PILL_H + 6, 27, C_GOLD);
    }
    footer_touch(0, 1, NULL);
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

/* SEND LOGS: the full zip is saved as ever, then the capped one goes to the server and is dropped. */
static void send_logs(void) {
    char nm[128], up[128], id[40] = "", err[96] = "";
    busy("UPLOADING...");
    if (logs_save(nm, sizeof(nm)) != 0) { snprintf(status, sizeof(status), "SAVE LOGS FAILED"); return; }
    if (logs_pack(up, sizeof(up), 1) != 0) { snprintf(status, sizeof(status), "SAVED %s", nm); return; }
    char zp[1400];
    snprintf(zp, sizeof(zp), "%s/%s", game_dir, up);
    int rc = cloud_send_logs(&cloud, zp, port_version, 0, id, sizeof(id), err, sizeof(err));
    unlink(zp);
    if (rc == 0) snprintf(status, sizeof(status), "SENT. REPORT %s. A COPY IS SAVED", id);
    else snprintf(status, sizeof(status), "NOT SENT: %s. ZIP SAVED", err);
}

static int debug_n(void) {
    UpdateInfo up;
    update_poll(&up);
    return up.state == UPDATE_AVAILABLE ? DEBUG_N + 1 : DEBUG_N;
}

static void draw_debug(void) {
    header_touch("DEBUG", 1);
    UpdateInfo up;
    update_poll(&up);
    int dn = up.state == UPDATE_AVAILABLE ? DEBUG_N + 1 : DEBUG_N;
    int gy = (int)(cur_y >= 0 && cur_screen == S_DEBUG ? cur_y : row_target_y(S_DEBUG, sel));
    draw_focus(32, gy, W - 64, 44, 10, 0);
    row_touch(86, 44, "LOGS", log_label[c.logs], sel == DEBUG_LOGS, 0, c.logs > 0, 0, 1);
    row_touch(140, 44, "SEND LOGS", "UPLOAD", sel == DEBUG_SAVE_LOGS, 0, 0, 0, 0);
    row_touch(194, 44, "CLEAN LOGS", "ASK", sel == DEBUG_CLEAN_LOGS, 0, 0, 0, 0);
    row_touch(248, 44, "CHECK FOR UPDATES", c.update_check ? "ON" : "OFF", sel == DEBUG_UPDATES, 0, c.update_check, 0, 1);
    if (dn > DEBUG_N) {
        row_touch(302, 44, "NEW VERSION", up.latest, sel == DEBUG_N, 0, 1, 0, 0);
    }
    int sy = dn > DEBUG_N ? 354 : 306;
    if (status[0]) { text(40, sy, status, 14, C_GREEN); sy += 20; }
    text(40, sy, S("SEND LOGS uploads the logs and your save (RPG data) to the developer."), 13, C_NOTE);
    {
        char bld[96];
        snprintf(bld, sizeof(bld), "BUILD: %s", port_version);
        text(40, sy + 20, bld, 13, strstr(port_version, "-dirty") ? C_GOLD : C_NOTE);
    }
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
    snprintf(stage, sizeof(stage), "%s...", S(launch_stage));
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

static void draw_disc(void) {
    const int img = disc_bad != DISC_BAD_FILE;
    header_touch("CHECK YOUR DISC", 1);
    rrect(36, 86, W - 72, 232, 12, C_PANEL, 256);
    rrect_line(36, 86, W - 72, 232, 12, C_GOLD);
    text(56, 102, S(img ? "NOT THE VERIFIED DISC IMAGE" : "THE DISC FILE COULD NOT BE READ"), 19, C_GOLD);
    if (img) {
        text(56, 138, S("Nothing broke: the game checks your disc before it starts."), 14, C_TEXT);
        text(56, 162, S("It plays one exact image: the Redump dump of"), 14, C_TEXT);
        text(56, 186, S("Digimon World 2003 (Europe), SLES-03936, as .bin + .cue."), 14, C_TEXT);
        text(56, 210, S("Patched or differently ripped images don't pass."), 14, C_TEXT);
        text(56, 244, S("Choose that image and press PLAY again."), 14, C_CYAN);
    } else {
        text(56, 146, S("It may have been moved or renamed, or its drive unplugged."), 14, C_TEXT);
        text(56, 182, S("Choose the disc again and press PLAY."), 14, C_CYAN);
        if (last_err[0]) {
            char el[72];
            snprintf(el, sizeof(el), "%.62s%s", last_err, strlen(last_err) > 62 ? "..." : "");
            text(56, 218, el, 13, C_NOTE);
        }
    }
    footer_touch(0, 1, "CHOOSE DISC");
}

static void draw_screen(void) {
    if (screen != S_SPLASH && screen != S_LOADING) {
        background((int)(plat_now_ms() & 0x7fffffff));
    }
    switch (screen) {
    case S_SPLASH: draw_splash(); break;
    case S_HOME: draw_home(); break;
    case S_GAME: draw_game(); break;
    case S_VIDEO: draw_video(); break;
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
    case S_DISC: draw_disc(); break;
    case S_CLOUD: draw_cloud(); break;
    case S_PAIR_HOST: draw_pair_host(); break;
    case S_PAIR_ENTER: draw_pair_enter(); break;
    case S_DEVICES: draw_devices(); break;
    case S_REVOKE_CONFIRM:
        draw_devices();
        modal_touch("REVOKE DEVICE?", "Block this device from syncing saves:", dev_count > 0 ? dev_items[sel].name : "",
                    "It will have to register or pair again.", "YES, REVOKE", "CANCEL", C_RED);
        break;
    case S_RENEW_CONFIRM:
        draw_cloud();
        modal_touch("START A NEW ACCOUNT?", "This device leaves its cloud account.", "A new account starts with its save.",
                    "Other devices stay on the old one.", "YES, NEW ACCOUNT", "NO, KEEP", C_ORANGE);
        break;
    case S_CONFLICT: draw_conflict(); break;
    case S_CONFLICT_CONFIRM: {
        draw_conflict();
        char rev_line[96], ok_btn[64];
        int step = conflict_step < 3 ? conflict_step + 1 : 3;
        snprintf(rev_line, sizeof(rev_line), "REV #%d", cloud.cloud_seq);
        if (conflict_choice == 1) {
            snprintf(ok_btn, sizeof(ok_btn), "%s (%d/3)", S("YES, UPLOAD"), step);
            modal_touch("OVERWRITE CLOUD SAVE?", "This device's save replaces the cloud's.",
                        "Other devices get it on their next sync.", rev_line, ok_btn, "CANCEL", C_RED);
        } else {
            snprintf(ok_btn, sizeof(ok_btn), "%s (%d/3)", S("YES, DOWNLOAD"), step);
            modal_touch("REPLACE LOCAL SAVE?", "The cloud save replaces this device's.",
                        "The current one is backed up first.", rev_line, ok_btn, "CANCEL", C_ORANGE);
        }
        break;
    }
    case S_SAVE_NOTICE: draw_save_notice(); break;
    case S_SAVE_NOTICE_CONFIRM: draw_save_notice_confirm(); break;
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
    case GAME_BATTLES: c.battles = !c.battles; if (!c.battles) c.hit = 0; break;
    case GAME_HIT: if (c.battles) c.hit = !c.hit; break;
    case GAME_TOUCHPAD: c.touch_pad = !c.touch_pad; break;
    case GAME_DISC: pick_disc_android(); break;
    }
    cfg_save(&c);
}

static void activate_video_row(int row, int dir) {
    switch (row) {
    case VIDEO_WIDESCREEN: c.widescreen = !c.widescreen; if (c.widescreen) c.renderer_pc = 0; break;
    case VIDEO_SCALING: c.fill = !c.fill; break;
    case VIDEO_HZ: c.hz = c.hz == 60 ? 50 : 60; break;
    case VIDEO_RENDERER: c.renderer_pc = !c.renderer_pc; if (c.renderer_pc) c.widescreen = 0; break;
    case VIDEO_FILTER: if (!c.renderer_pc) c.filter = (c.filter + dir + 5) % 5; break;
    case VIDEO_RESOLUTION: if (!c.renderer_pc) c.iscale = (c.iscale - 1 + dir + 8) % 8 + 1; break;
    case VIDEO_SUBPIXEL: if (!c.renderer_pc && c.iscale > 1) c.subpixel = (c.subpixel + dir + 3) % 3; break;
    }
    cfg_save(&c);
}

static void notice_confirm_tap(void) {
    long now = plat_now_ms();
    if (notice_step > 0 && now - notice_tap_ms < 220) return;
    notice_tap_ms = now;
    notice_step++;
    if (notice_step >= 3) {
        c.v091_save_ok = 1;
        cfg_save(&c);
        go(S_HOME, 0, +1);
    }
}

static void activate_home_row(int row) {
    switch (row) {
    case HI_PLAY: start_game_flow(); break;
    case HI_GAME: go(S_GAME, 0, +1); break;
    case HI_VIDEO: go(S_VIDEO, 0, +1); break;
    case HI_HUD: go(S_HUD, 0, +1); break;
    case HI_CONTROLS: go(S_CONTROLS, 0, +1); break;
    case HI_SAVES: status[0] = 0; go(S_SAVES, 0, +1); break;
    case HI_DEBUG: status[0] = 0; go(S_DEBUG, 0, +1); break;
    case HI_RESET: go(S_RESET, 0, +1); break;
    }
}

static void open_cloud_account(void) {
    status[0] = 0;
    if (cloud.registered) {
        char err[128] = "";
        busy("CONNECTING...");
        if (cloud_refresh_me(&cloud, game_dir, err, sizeof(err)) != 0) snprintf(status, sizeof(status), "%s", err);
    }
    go(S_CLOUD, 0, +1);
}

static void activate_saves_row(int row, int dir) {
    switch (row) {
    case SAVES_SYNC:
        if (!cloud_active()) { open_cloud_account(); return; }
        c.sync_enabled = !c.sync_enabled;
        break;
    case SAVES_INTERVAL: {
        static const int ivs[] = {0, 5, 10, 15, 20, 30};
        int k = 0;
        for (int j = 0; j < 6; j++) if (c.sync_interval_min == ivs[j]) k = j;
        c.sync_interval_min = ivs[(k + dir + 6) % 6];
        break;
    }
    case SAVES_BACKUPS:
        c.backup_max += dir;
        if (c.backup_max < 1) c.backup_max = 1;
        if (c.backup_max > 100) c.backup_max = 100;
        break;
    case SAVES_NOW:
        if (!cloud_active()) {
            snprintf(status, sizeof(status), "%s",
                     S(cloud.revoked ? "DEVICE REVOKED - OPEN CLOUD ACCOUNT" : "REGISTER IN CLOUD ACCOUNT FIRST"));
        } else {
            int conf = 0;
            busy("SYNCING SAVE...");
            if (cloud_sync_card1(&cloud, game_dir, c.backup_max, 1, &conf, status, sizeof(status)) == 1 && conf) {
                open_conflict(S_SAVES, SAVES_NOW, 0);
                return;
            }
        }
        break;
    case SAVES_RESTORE:
        bk_count = save_backup_list(game_dir, bk_items, BACKUP_MAX_LIST);
        bk_scroll = 0;
        go(S_BACKUPS, 0, +1);
        return;
    case SAVES_ACCOUNT:
        open_cloud_account();
        return;
    }
    cfg_save(&c);
}

/* REGISTER THIS DEVICE and NEW ACCOUNT: a fresh account whose first save is this device's card, as on desktop. */
static void register_device(int new_account) {
    char err[128] = "";
    busy("CONNECTING...");
    if (cloud_register(&cloud, game_dir, is_portmaster, err, sizeof(err)) != 0) {
        snprintf(status, sizeof(status), "%s", err);
        return;
    }
    c.sync_enabled = 1;
    cfg_save(&c);
    const char *done = S(new_account ? "NEW ACCOUNT CREATED" : "REGISTERED");
    char card_p[1024], smsg[96] = "";
    snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
    if (save_is_valid_mc(card_p, NULL) && cloud_push_card1(&cloud, game_dir, c.backup_max, 0, smsg, sizeof(smsg)) == 0) {
        snprintf(status, sizeof(status), "%s - %s", done, smsg);
    } else {
        snprintf(status, sizeof(status), "%s", done);
    }
}

static void open_pair_enter(void) {
    pair_len = 0;
    pair_digits[0] = 0;
    pair_waiting = pair_done = 0;
    pair_done_msg[0] = 0;
    kp_sel = 0;
    status[0] = 0;
    go(S_PAIR_ENTER, 0, +1);
}

static void activate_cloud_row(int row) {
    char err[128] = "";
    status[0] = 0;
    if (!cloud_active()) {
        if (row == CLOUD_REGISTER_OR_ADD) register_device(0);
        else if (row == CLOUD_LINK) open_pair_enter();
        return;
    }
    switch (row) {
    case CLOUD_REGISTER_OR_ADD: {
        int exp_sec = 300;
        memset(&pair_st, 0, sizeof(pair_st));
        memset(pair_code, 0, sizeof(pair_code));
        pair_done = 0;
        busy("CONNECTING...");
        if (cloud_pair_create(&cloud, pair_code, &exp_sec, err, sizeof(err)) == 0) {
            pair_expires_at = (long)time(NULL) + exp_sec;
            pair_last_poll_ms = plat_now_ms();
            go(S_PAIR_HOST, 0, +1);
        } else {
            snprintf(status, sizeof(status), "%s", err[0] ? err : S("PAIRING FAILED"));
        }
        break;
    }
    case CLOUD_LINK: open_pair_enter(); break;
    case CLOUD_DEVICES:
        busy("CONNECTING...");
        if (cloud_list_devices(&cloud, dev_items, CLOUD_MAX_DEVICES, &dev_count, err, sizeof(err)) == 0) {
            dev_scroll = 0;
            go(S_DEVICES, 0, +1);
        } else {
            snprintf(status, sizeof(status), "%s", err);
        }
        break;
    case CLOUD_RENEW: go(S_RENEW_CONFIRM, CLOUD_RENEW, +1); break;
    }
}

static void pair_host_answer(int approve) {
    char err[128] = "";
    busy("CONNECTING...");
    if (cloud_pair_approve(&cloud, pair_code, approve, err, sizeof(err)) != 0) {
        snprintf(status, sizeof(status), "%s", err);
        return;
    }
    if (approve) {
        pair_done = 1;
        status[0] = 0;
        cloud_refresh_me(&cloud, game_dir, err, sizeof(err));
    } else {
        go(S_CLOUD, CLOUD_REGISTER_OR_ADD, -1);
    }
}

static void pair_host_leave(void) {
    /* Leaving with a request on screen answers it: the other device stops waiting instead of timing out. */
    if (!pair_done && !strcmp(pair_st.status, "awaiting_approval")) {
        char err[128];
        cloud_pair_approve(&cloud, pair_code, 0, err, sizeof(err));
    }
    status[0] = 0;
    go(S_CLOUD, CLOUD_REGISTER_OR_ADD, -1);
}

static void pair_key(int k) {
    if (pair_waiting || pair_done || k < 0 || k >= KP_KEYS) return;
    if (k < 10) {
        if (pair_len < 6) pair_digits[pair_len++] = kp_label(k)[0];
        status[0] = 0;
    } else if (k == KP_DEL) {
        if (pair_len > 0) pair_len--;
        status[0] = 0;
    } else if (pair_len == 6) {
        char err[128] = "";
        busy("CONNECTING...");
        if (cloud_pair_claim(&cloud, pair_digits, is_portmaster, &pair_claim, err, sizeof(err)) == 0) {
            pair_waiting = 1;
            pair_last_poll_ms = plat_now_ms();
            status[0] = 0;
        } else {
            snprintf(status, sizeof(status), "%s", err);
        }
    }
    pair_digits[pair_len] = 0;
}

/* The pairing screens' background polling (every PAIR_POLL_MS, as on desktop). */
static void cloud_poll(void) {
    long now = plat_now_ms();
    if (now - pair_last_poll_ms < PAIR_POLL_MS) return;
    char perr[128] = "";
    if (screen == S_PAIR_HOST && !pair_done && pair_remaining() > 0) {
        pair_last_poll_ms = now;
        PairStatus ps;
        /* A failed poll keeps the last answer, so an APPROVE already on screen does not flicker away. */
        if (cloud_pair_status(&cloud, pair_code, &ps, perr, sizeof(perr)) == 0) pair_st = ps;
        else if (perr[0]) snprintf(status, sizeof(status), "%s", perr);
    } else if (screen == S_PAIR_ENTER && pair_waiting && !pair_done) {
        pair_last_poll_ms = now;
        int approved = 0;
        int prc = cloud_pair_poll(&cloud, game_dir, &pair_claim, &approved, perr, sizeof(perr));
        if (prc == 0 && approved) {
            pair_waiting = 0;
            pair_done = 1;
            c.sync_enabled = 1;
            cfg_save(&c);
            cloud_refresh_me(&cloud, game_dir, perr, sizeof(perr));
            int conf = 0;
            char smsg[128] = "";
            busy("SYNCING SAVE...");
            int src = cloud_sync_card1(&cloud, game_dir, c.backup_max, 1, &conf, smsg, sizeof(smsg));
            if (src == 1 && conf) {
                open_conflict(S_CLOUD, 0, 0);
                return;
            }
            snprintf(pair_done_msg, sizeof(pair_done_msg), "%s", smsg);
        } else if (prc == -2) {
            /* Denied, expired or gone: back to the keypad with the reason. */
            pair_waiting = 0;
            pair_len = 0;
            pair_digits[0] = 0;
            snprintf(status, sizeof(status), "%s", perr[0] ? perr : S("PAIRING FAILED"));
        }
    }
}

static void conflict_choose(int i) {
    if (i == 2) {
        conflict_finish(S("SAVES LEFT AS THEY ARE"), 0);
        return;
    }
    conflict_choice = i;
    conflict_step = 0;
    go(S_CONFLICT_CONFIRM, 0, +1);
}

static void conflict_apply(void) {
    char res[160] = "";
    busy("SYNCING SAVE...");
    int rc = conflict_choice == 0
        ? cloud_pull_card1(&cloud, game_dir, c.backup_max, res, sizeof(res))
        : cloud_push_card1(&cloud, game_dir, c.backup_max, 1, res, sizeof(res));
    conflict_finish(res, rc == 0);
}

static void conflict_confirm_tap(void) {
    long now = plat_now_ms();
    if (conflict_step > 0 && now - conflict_tap_ms < 220) return;
    conflict_tap_ms = now;
    conflict_step++;
    if (conflict_step >= 3) {
        conflict_step = 0;
        conflict_apply();
    }
}

static void revoke_selected(void) {
    if (dev_count <= 0 || sel < 0 || sel >= dev_count || dev_items[sel].is_current) {
        go(S_DEVICES, 0, -1);
        return;
    }
    char err[128] = "";
    busy("CONNECTING...");
    if (dev_count > 0 && cloud_revoke_device(&cloud, dev_items[sel].device_id, err, sizeof(err)) == 0) {
        cloud_list_devices(&cloud, dev_items, CLOUD_MAX_DEVICES, &dev_count, err, sizeof(err));
        int ns = sel >= dev_count ? (dev_count > 0 ? dev_count - 1 : 0) : sel;
        if (dev_scroll > ns) dev_scroll = ns;
        if (dev_count <= LIST_VIS) dev_scroll = 0;
        go(S_DEVICES, ns, -1);
        snprintf(status, sizeof(status), "%s", S("DEVICE REVOKED"));
    } else {
        go(S_DEVICES, sel, -1);
        snprintf(status, sizeof(status), "%s", err);
    }
}

static int touch_dir(int x) { return (x >= 260 && x < 470) ? -1 : +1; }

static void clamp_scroll(int *scroll, int vis) {
    if (sel < *scroll) *scroll = sel;
    if (sel >= *scroll + vis) *scroll = sel - vis + 1;
    if (*scroll < 0) *scroll = 0;
}

static void do_reset_confirm(void) {
    int ok91 = c.v091_save_ok;
    cfg_defaults(&c);
    c.v091_save_ok = ok91;
    cfg_save(&c);
    go(S_HOME, HI_RESET, -1);
}

static void do_backup_restore(void) {
    if (bk_count <= 0) return;
    char err[128] = "";
    if (save_backup_restore(game_dir, bk_items[sel].name, c.backup_max, err, sizeof(err)) == 0) {
        snprintf(status, sizeof(status), "RESTORED %s", bk_items[sel].label);
    } else {
        snprintf(status, sizeof(status), "%s", err[0] ? err : "RESTORE FAILED");
    }
    go(S_SAVES, SAVES_RESTORE, -1);
}

static void do_clean_logs(void) {
    int rm = logs_clean();
    snprintf(status, sizeof(status), "CLEANED %d FILES", rm);
    go(S_DEBUG, DEBUG_CLEAN_LOGS, -1);
}

static void do_hud_step(int row, int dir) {
    if (row < H_COUNT) c.hud[row] = hud_cycle(c.hud[row], dir);
    else if (row == H_COUNT) c.bmem = !c.bmem;
    else if (row == H_COUNT + 1) c.bbuf = (c.bbuf + dir + 3) % 3;
    else cfg_set_fight_place(&c, cfg_fight_place(&c) != 1);
    cfg_save(&c);
}

static void do_browse_activate(int i) {
    if (i < 0 || i >= b_count) return;
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
}

static void on_back(void);

static void on_touch(int code, int x, int y) {
    if (screen == S_SPLASH) {
        if (code == K_TOUCH_DOWN) {
            touch_drag_scrolled = 1;
            leave_splash();
        }
        return;
    }
    if (screen == S_LOADING) {
        if (code == K_TOUCH_DOWN) load_press(c.pad[A_CROSS].code);
        return;
    }
    if (code == K_TOUCH_MOVE && touch_drag_active && (screen == S_BROWSE || screen == S_BACKUPS || screen == S_DEVICES)) {
        int dy = y - touch_drag_y0;
        if (dy > 32) {
            if (screen == S_BROWSE && b_scroll > 0) b_scroll--;
            if (screen == S_BACKUPS && bk_scroll > 0) bk_scroll--;
            if (screen == S_DEVICES && dev_scroll > 0) dev_scroll--;
            touch_drag_y0 = y;
            touch_drag_scrolled = 1;
        } else if (dy < -32) {
            if (screen == S_BROWSE && b_scroll + BROWSE_VIS < b_count) b_scroll++;
            if (screen == S_BACKUPS && bk_scroll + LIST_VIS < bk_count) bk_scroll++;
            if (screen == S_DEVICES && dev_scroll + LIST_VIS < dev_count) dev_scroll++;
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
        if (in_rect(x, y, 90, 294, 210, 44)) do_reset_confirm();
        else if (in_rect(x, y, 340, 294, 210, 44) || in_rect(x, y, 520, 14, 98, 36)) go(S_HOME, HI_RESET, -1);
        return;
    }
    if (screen == S_BACKUP_CONFIRM) {
        if (in_rect(x, y, 90, 294, 210, 44) && bk_count > 0) do_backup_restore();
        else if (in_rect(x, y, 340, 294, 210, 44)) go(S_BACKUPS, sel, -1);
        return;
    }
    if (screen == S_CLEAN) {
        if (in_rect(x, y, 90, 294, 210, 44)) do_clean_logs();
        else if (in_rect(x, y, 340, 294, 210, 44)) go(S_DEBUG, DEBUG_CLEAN_LOGS, -1);
        return;
    }

    /* The modals' two buttons; the BACK pill behind them cancels too. */
    if (screen == S_REVOKE_CONFIRM || screen == S_RENEW_CONFIRM || screen == S_CONFLICT_CONFIRM) {
        if (in_rect(x, y, 90, 294, 210, 44)) {
            if (screen == S_REVOKE_CONFIRM) revoke_selected();
            else if (screen == S_RENEW_CONFIRM) { register_device(1); go(S_CLOUD, 0, -1); }
            else conflict_confirm_tap();
        } else if (in_rect(x, y, 340, 294, 210, 44) || in_rect(x, y, 520, 14, 98, 36) || in_rect(x, y, 28, H - 46, 126, 36)) {
            on_back();
        }
        return;
    }

    if (screen == S_SAVE_NOTICE_CONFIRM) {
        if (in_rect(x, y, 86, 294, 214, 44)) {
            notice_confirm_tap();
        } else if (in_rect(x, y, 340, 294, 214, 44)) {
            notice_step = 0;
            go(S_SAVE_NOTICE, 0, -1);
        }
        return;
    }
    if (screen == S_SAVE_NOTICE) {
        if (in_rect(x, y, 32, 226, W - 64, 46) || (in_rect(x, y, 240, H - 46, 160, 36) && sel == 0)) {
            fresh_save_reset();
            go(S_HOME, 0, +1);
        } else if (in_rect(x, y, 32, 280, W - 64, 46) || (in_rect(x, y, 240, H - 46, 160, 36) && sel == 1)) {
            sel = 1;
            notice_step = 0;
            go(S_SAVE_NOTICE_CONFIRM, 0, +1);
        }
        return;
    }

    if (screen == S_ERROR) {
        if (in_rect(x, y, 240, H - 46, 160, 36) || in_rect(x, y, 520, 14, 98, 36) || in_rect(x, y, 28, H - 46, 126, 36)) {
            go(return_screen, return_sel, -1);
        }
        return;
    }
    if (screen == S_DISC) {
        if (in_rect(x, y, 240, H - 46, 160, 36)) {
            pick_disc_android();
            if (screen == S_DISC && disc_path[0]) go(return_screen, return_sel, -1);
        } else if (in_rect(x, y, 520, 14, 98, 36) || in_rect(x, y, 28, H - 46, 126, 36)) {
            go(return_screen, return_sel, -1);
        }
        return;
    }

    if (screen != S_HOME && (in_rect(x, y, 520, 14, 98, 36) || in_rect(x, y, 28, H - 46, 126, 36))) {
        on_back();
        return;
    }
    if (screen == S_HOME && in_rect(x, y, 452, 14, 166, 36) && open_update_page()) return;
    if (in_rect(x, y, 486, H - 46, 126, 36)) {
        if (screen == S_HOME || screen == S_GAME || screen == S_VIDEO || screen == S_DEBUG) {
            start_game_flow();
            return;
        }
    }

    if (screen == S_HOME) {
        for (int i = 0; i < HI_COUNT; i++) {
            if (in_rect(x, y, 32, HOME_ROW_Y + i * HOME_ROW_STEP, W - 64, 40)) {
                sel = i;
                activate_home_row(i);
                return;
            }
        }
    } else if (screen == S_GAME) {
        for (int i = 0; i < GAME_N; i++) {
            if (in_rect(x, y, 32, 76 + i * GAME_ROW_STEP, W - 64, GAME_ROW_STEP)) {
                sel = i;
                activate_game_row(i, touch_dir(x));
                return;
            }
        }
        if (in_rect(x, y, 240, H - 46, 160, 36)) activate_game_row(sel, +1);
    } else if (screen == S_VIDEO) {
        for (int i = 0; i < VIDEO_N; i++) {
            if (in_rect(x, y, 32, VIDEO_ROW_Y + i * VIDEO_ROW_STEP, W - 64, 40)) {
                sel = i;
                activate_video_row(i, touch_dir(x));
                return;
            }
        }
        if (in_rect(x, y, 240, H - 46, 160, 36)) activate_video_row(sel, +1);
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
        for (int i = 0; i < HUD_ROWS; i++) {
            if (in_rect(x, y, 32, 174 + i * 23, W - 64, 23)) {
                sel = i;
                do_hud_step(i, touch_dir(x));
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
            int dir = touch_dir(x);
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
                activate_saves_row(i, touch_dir(x));
                return;
            }
        }
    } else if (screen == S_CLOUD) {
        for (int i = 0; i < cloud_n(); i++) {
            if (in_rect(x, y, 32, CLOUD_ROW_Y + i * CLOUD_ROW_STEP, W - 64, 46)) {
                sel = i;
                activate_cloud_row(i);
                return;
            }
        }
    } else if (screen == S_PAIR_HOST) {
        if (!pair_done && !strcmp(pair_st.status, "awaiting_approval")) {
            if (in_rect(x, y, 90, 300, 210, 48)) pair_host_answer(1);
            else if (in_rect(x, y, 340, 300, 210, 48)) pair_host_answer(0);
        }
    } else if (screen == S_PAIR_ENTER) {
        if (pair_waiting || pair_done) return;
        for (int k = 0; k < KP_KEYS; k++) {
            int kx, ky;
            kp_rect(k, &kx, &ky);
            if (in_rect(x, y, kx, ky, KP_W, KP_H)) {
                kp_sel = k;
                pair_key(k);
                return;
            }
        }
    } else if (screen == S_DEVICES) {
        for (int i = dev_scroll; i < dev_count && i < dev_scroll + LIST_VIS; i++) {
            if (in_rect(x, y, 32, 114 + (i - dev_scroll) * 44, W - 64, 40)) {
                sel = i;
                if (!dev_items[i].is_current && !strcmp(dev_items[i].status, "active")) go(S_REVOKE_CONFIRM, i, +1);
                return;
            }
        }
    } else if (screen == S_CONFLICT) {
        for (int i = 0; i < 3; i++) {
            if (in_rect(x, y, 44 + i * (CONFLICT_PILL_W + 12), CONFLICT_PILL_Y, CONFLICT_PILL_W, CONFLICT_PILL_H)) {
                conflict_sel = i;
                conflict_choose(i);
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
        int dn = debug_n();
        for (int i = 0; i < dn; i++) {
            if (in_rect(x, y, 32, 86 + i * 54, W - 64, 44)) {
                sel = i;
                if (i == DEBUG_LOGS) { c.logs = (c.logs + 1) % 3; cfg_save(&c); }
                else if (i == DEBUG_SAVE_LOGS) {
                    send_logs();
                } else if (i == DEBUG_CLEAN_LOGS) {
                    go(S_CLEAN, 0, +1);
                } else if (i == DEBUG_UPDATES) {
                    toggle_update_check();
                } else if (i == DEBUG_N) {
                    open_update_page();
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
                do_browse_activate(i);
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
    case S_SPLASH: case S_HOME: case S_LOADING: case S_SAVE_NOTICE: return;
    case S_SAVE_NOTICE_CONFIRM: notice_step = 0; go(S_SAVE_NOTICE, 0, -1); return;
    case S_RESET: go(S_HOME, HI_RESET, -1); return;
    case S_BACKUP_CONFIRM: go(S_BACKUPS, sel, -1); return;
    case S_CLEAN: go(S_DEBUG, DEBUG_CLEAN_LOGS, -1); return;
    case S_BROWSE: go(S_GAME, GAME_DISC, -1); return;
    case S_BACKUPS: go(S_SAVES, SAVES_RESTORE, -1); return;
    case S_ERROR: case S_DISC: go(return_screen, return_sel, -1); return;
    case S_CLOUD: status[0] = 0; go(S_SAVES, SAVES_ACCOUNT, -1); return;
    case S_PAIR_HOST: pair_host_leave(); return;
    case S_PAIR_ENTER: status[0] = 0; go(S_CLOUD, CLOUD_LINK, -1); return;
    case S_DEVICES: status[0] = 0; go(S_CLOUD, CLOUD_DEVICES, -1); return;
    case S_REVOKE_CONFIRM: go(S_DEVICES, sel, -1); return;
    case S_RENEW_CONFIRM: go(S_CLOUD, CLOUD_RENEW, -1); return;
    case S_CONFLICT: conflict_finish(S("SAVES LEFT AS THEY ARE"), 0); return;
    case S_CONFLICT_CONFIRM: conflict_step = 0; go(S_CONFLICT, 0, -1); return;
    case S_HUD:
        if (dup_hud(&c, NULL)) { snprintf(msg, sizeof(msg), "SAME BUTTON ON TWO ROWS - FIX IT BEFORE LEAVING"); return; }
        cfg_save(&c); go(S_HOME, HI_HUD, -1); return;
    case S_CONTROLS:
        if (dup_actions(&c, NULL)) { snprintf(msg, sizeof(msg), "SAME BUTTON ON TWO ROWS - FIX IT BEFORE LEAVING"); return; }
        cfg_save(&c); go(S_HOME, HI_CONTROLS, -1); return;
    case S_GAME: cfg_save(&c); go(S_HOME, HI_GAME, -1); return;
    case S_VIDEO: cfg_save(&c); go(S_HOME, HI_VIDEO, -1); return;
    case S_SAVES: cfg_save(&c); go(S_HOME, HI_SAVES, -1); return;
    case S_DEBUG: cfg_save(&c); go(S_HOME, HI_DEBUG, -1); return;
    default: cfg_save(&c); go(S_HOME, 0, -1); return;
    }
}

static void on_key(int code) {
    if (code == K_BACK) { on_back(); return; }
    if (screen == S_SPLASH) { leave_splash(); return; }
    if (screen == S_LOADING) { load_press(code); return; }
    if (capture) {
        /* As the desktop launcher's (main.c, on_press): a face button, a shoulder, Start or Select binds, the d-pad
         * cancels (as Android's Back does, on_back), and the row's letter follows its new button. Without the letter
         * cfg_layout() reads the pad as LAYOUT_CUSTOM, and every row's glyph falls back to the PS1 shapes: one remap
         * looked like it had broken all the others. */
        static const int valid[] = {K_SOUTH, K_EAST, K_WEST, K_NORTH, K_L1, K_R1, K_L2, K_R2, K_START, K_SELECT};
        if (code == K_UP || code == K_DOWN) { capture = 0; return; }
        for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
            if (valid[i] != code) continue;
            if (screen == S_CONTROLS && sel > 0) {
                int a = act_order[sel - 1], lay = cfg_layout(&c);
                c.pad[a].code = code;
                if (a <= A_TRIANGLE) c.pad[a].letter = layout_letter(lay == LAYOUT_CUSTOM ? LAYOUT_XBOX : lay, code);
            }
            capture = 0;
            cfg_save(&c);
            return;
        }
        return;
    }
    if (screen == S_PAIR_ENTER && !pair_waiting && !pair_done) {
        /* A pad walks the keypad; OK presses the lit key. */
        if (code == K_UP || code == K_DOWN) { kp_sel = (kp_sel + KP_COLS) % KP_KEYS; return; }
        if (code == K_LEFT) { kp_sel = (kp_sel + KP_KEYS - 1) % KP_KEYS; return; }
        if (code == K_RIGHT) { kp_sel = (kp_sel + 1) % KP_KEYS; return; }
        if (code == btn_ok()) { pair_key(kp_sel); return; }
    }
    if (screen == S_CONFLICT) {
        if (code == K_LEFT) { conflict_sel = (conflict_sel + 2) % 3; return; }
        if (code == K_RIGHT) { conflict_sel = (conflict_sel + 1) % 3; return; }
        if (code == btn_ok()) { conflict_choose(conflict_sel); return; }
    }
    if (screen == S_PAIR_HOST && !pair_done && !strcmp(pair_st.status, "awaiting_approval") &&
        code == c.pad[A_TRIANGLE].code) {
        pair_host_answer(0);
        return;
    }
    if (screen == S_BROWSE && code == c.pad[A_TRIANGLE].code) {
        if (android_pick_disc_files() == 0) {
            find_disc();
            if (disc_path[0]) { cfg_save(&c); go(S_GAME, GAME_DISC, -1); }
        }
        return;
    }
    if (code == K_UP || code == K_DOWN) {
        if (screen == S_BACKUP_CONFIRM || screen == S_REVOKE_CONFIRM || screen == S_RESET ||
            screen == S_CLEAN || screen == S_RENEW_CONFIRM || screen == S_CONFLICT_CONFIRM ||
            screen == S_SAVE_NOTICE_CONFIRM || screen == S_ERROR || screen == S_DISC) {
            return;
        }
        int dir = code == K_UP ? -1 : +1;
        int n = screen == S_HOME ? HI_COUNT : screen == S_GAME ? GAME_N : screen == S_VIDEO ? VIDEO_N :
                screen == S_HUD ? HUD_ROWS : screen == S_CONTROLS ? (A_COUNT + 1) : screen == S_SAVES ? SAVES_N :
                screen == S_DEBUG ? debug_n() : screen == S_BROWSE ? b_count : screen == S_BACKUPS ? bk_count :
                screen == S_CLOUD ? cloud_n() : screen == S_DEVICES ? dev_count : screen == S_SAVE_NOTICE ? 2 : 1;
        if (n > 0) sel = (sel + dir + n) % n;
        if (screen == S_BROWSE) clamp_scroll(&b_scroll, BROWSE_VIS);
        if (screen == S_BACKUPS) clamp_scroll(&bk_scroll, LIST_VIS);
        if (screen == S_DEVICES) clamp_scroll(&dev_scroll, LIST_VIS);
    } else if ((code == K_L1 || code == K_R1) && screen == S_SAVES && sel == SAVES_BACKUPS) {
        activate_saves_row(SAVES_BACKUPS, code == K_L1 ? -10 : +10);
    } else if (code == K_LEFT || code == K_RIGHT) {
        int dir = code == K_LEFT ? -1 : +1;
        if (screen == S_GAME) activate_game_row(sel, dir);
        else if (screen == S_VIDEO) activate_video_row(sel, dir);
        else if (screen == S_SAVES && (sel == SAVES_INTERVAL || sel == SAVES_BACKUPS)) activate_saves_row(sel, dir);
        else if (screen == S_HUD) do_hud_step(sel, dir);
        else if (screen == S_CONTROLS && sel == 0) {
            int l = cfg_layout(&c);
            cfg_set_layout(&c, l == LAYOUT_CUSTOM ? LAYOUT_PS1 : (l + dir + 3) % 3);
            cfg_save(&c);
        }
    } else if (code == btn_ok()) {
        if (screen == S_HOME) activate_home_row(sel);
        else if (screen == S_GAME) activate_game_row(sel, +1);
        else if (screen == S_VIDEO) activate_video_row(sel, +1);
        else if (screen == S_HUD) do_hud_step(sel, +1);
        else if (screen == S_CONTROLS) {
            if (sel == 0) {
                int l = cfg_layout(&c);
                cfg_set_layout(&c, l == LAYOUT_CUSTOM ? LAYOUT_PS1 : (l + 1) % 3);
                cfg_save(&c);
            } else {
                capture = 1;
            }
        }
        else if (screen == S_SAVE_NOTICE) {
            if (sel == 0) { fresh_save_reset(); go(S_HOME, 0, +1); }
            else { notice_step = 0; go(S_SAVE_NOTICE_CONFIRM, 0, +1); }
        }
        else if (screen == S_SAVE_NOTICE_CONFIRM) notice_confirm_tap();
        else if (screen == S_SAVES) activate_saves_row(sel, +1);
        else if (screen == S_CLOUD) activate_cloud_row(sel);
        else if (screen == S_PAIR_HOST && !pair_done && !strcmp(pair_st.status, "awaiting_approval")) pair_host_answer(1);
        else if (screen == S_DEVICES && dev_count > 0 && !dev_items[sel].is_current && !strcmp(dev_items[sel].status, "active")) {
            go(S_REVOKE_CONFIRM, sel, +1);
        }
        else if (screen == S_BACKUPS && bk_count > 0) go(S_BACKUP_CONFIRM, sel, +1);
        else if (screen == S_BROWSE) do_browse_activate(sel);
        else if (screen == S_RESET) do_reset_confirm();
        else if (screen == S_CLEAN) do_clean_logs();
        else if (screen == S_BACKUP_CONFIRM) do_backup_restore();
        else if (screen == S_REVOKE_CONFIRM) revoke_selected();
        else if (screen == S_RENEW_CONFIRM) { register_device(1); go(S_CLOUD, 0, -1); }
        else if (screen == S_CONFLICT_CONFIRM) conflict_confirm_tap();
        else if (screen == S_ERROR) go(return_screen, return_sel, -1);
        else if (screen == S_DISC) { pick_disc_android(); if (screen == S_DISC && disc_path[0]) go(return_screen, return_sel, -1); }
        else if (screen == S_DEBUG) {
            if (sel == DEBUG_LOGS) { c.logs = (c.logs + 1) % 3; cfg_save(&c); }
            else if (sel == DEBUG_SAVE_LOGS) send_logs();
            else if (sel == DEBUG_CLEAN_LOGS) go(S_CLEAN, 0, +1);
            else if (sel == DEBUG_UPDATES) toggle_update_check();
            else if (sel == DEBUG_N) open_update_page();
        }
    } else if (code == btn_back()) {
        on_back();
    } else if (code == btn_start()) {
        if (screen == S_HOME || screen == S_GAME || screen == S_VIDEO || screen == S_DEBUG) start_game_flow();
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

    find_disc();
    cfg_load(&c);
    cloud_state_load(&cloud, game_dir);
    update_check_start(game_dir, port_version, c.update_check);

    while (!android_done) {
        if (screen == S_SPLASH && splash_t0 >= 0 && plat_now_ms() - splash_t0 >= SPLASH_MS) {
            leave_splash();
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
        cloud_poll();
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
    /* With the Xbox layout the touch pad's face buttons carry the letters the game's prompts print for them (owner's
     * check on the phone: circle is A, triangle B, square X, cross Y) at an Xbox pad's places (A bottom, B right, X
     * left, Y top); each still sends its own PS1 button. Any other layout keeps the PS1 diamond and shapes. */
    {
        int place[4];
        char letter[4];
        for (int i = 0; i < 4; i++) {
            int code = c.pad[i].code;
            place[i] = code == K_SOUTH ? TP_PLACE_BOTTOM
                     : code == K_EAST  ? TP_PLACE_RIGHT
                     : code == K_WEST  ? TP_PLACE_LEFT
                                       : TP_PLACE_TOP;
            letter[i] = c.pad[i].letter;
        }
        dw3_touch_pad_set_faces(place, letter);
    }
    if (out_log_level) *out_log_level = c.logs;
    if (out_disc && out_disc_sz > 0) snprintf(out_disc, out_disc_sz, "%s", disc_path);
    return android_cancelled ? 1 : 0;
}

#ifdef DW3_ANDROID_STANDALONE_TEST
const char port_version[] = "dev";
void dw3_touch_pad_set_faces(const int place[4], const char letter[4]) { (void)place; (void)letter; }

static int android_self_test(const char *tmp) {
    int fails = 0;
    #define acheck(cond, label) do { \
        if (cond) printf("android-self-test: ok     %s\n", (label)); \
        else { printf("android-self-test: FAIL   %s\n", (label)); fails++; } \
    } while (0)

    is_android = 1; is_pc = 0; is_portmaster = 0;
    mkdir(tmp, 0755);
    snprintf(game_dir, sizeof(game_dir), "%s", tmp);
    if (plat_open(PLAT_NULL, "") != 0) { fprintf(stderr, "plat_open failed\n"); return 1; }

    /* 1. Fresh boot without card1.mcd must mark v091_save_ok = 1 and go to S_HOME */
    cfg_defaults(&c);
    screen = S_SPLASH;
    leave_splash();
    acheck(screen == S_HOME && c.v091_save_ok == 1, "fresh boot without save sets v091_save_ok=1 and opens S_HOME");

    /* 2. Reset settings preserves v091_save_ok */
    do_reset_confirm();
    acheck(c.v091_save_ok == 1, "S_RESET preserves v091_save_ok=1");

    /* 3. S_CONFLICT: DECIDE LATER / Back must not launch the game */
    conflict_ret = S_HOME; conflict_ret_sel = HI_PLAY; conflict_then_play = 1;
    screen = S_CONFLICT;
    conflict_choose(2);
    acheck(screen == S_HOME, "S_CONFLICT DECIDE LATER returns to S_HOME without launching");

    /* 3b. Pre-launch disc check catches missing and invalid images */
    acheck(disc_check_android("/nonexistent/dw2003.bin") == DISC_BAD_FILE, "disc_check_android returns DISC_BAD_FILE for missing file");
    {
        char bp[600];
        snprintf(bp, sizeof(bp), "%s/bad.bin", tmp);
        FILE *bf = fopen(bp, "wb");
        if (bf) { fputs("not a ps1 disc", bf); fclose(bf); }
        acheck(disc_check_android(bp) == DISC_BAD_IMAGE, "disc_check_android returns DISC_BAD_IMAGE for invalid disc");
    }

    /* 4. Render every screen in all 6 languages without crashing */
    static const int screens[] = {
        S_SPLASH, S_HOME, S_GAME, S_VIDEO, S_HUD, S_CONTROLS, S_SAVES, S_BACKUPS, S_BACKUP_CONFIRM,
        S_DEBUG, S_BROWSE, S_RESET, S_CLEAN, S_LOADING, S_ERROR, S_DISC, S_CLOUD, S_PAIR_HOST, S_PAIR_ENTER,
        S_DEVICES, S_REVOKE_CONFIRM, S_RENEW_CONFIRM, S_CONFLICT, S_CONFLICT_CONFIRM,
        S_SAVE_NOTICE, S_SAVE_NOTICE_CONFIRM
    };
    for (int lang = 0; lang < 6; lang++) {
        c.lang = lang;
        for (unsigned i = 0; i < sizeof(screens) / sizeof(screens[0]); i++) {
            screen = screens[i];
            sel = 0;
            draw();
        }
    }
    acheck(1, "all 26 Android screens drawn across all 6 languages");

    /* 5. Key i18n strings used by Android screens must resolve to non-English in PT-BR */
    c.lang = 1;
    static const char *const must_tr[] = {
        "NEW IN v0.9.1", "START FRESH RECOMMENDED", "START FRESH (RECOMMENDED)",
        "KEEP OLD SAVE", "KEEP OLD SAVE ANYWAY?",
        "Scales full PS1 VRAM (1024x512, 1536x512 in 16:9), not just 320x240.",
        "On-screen floating touch controls during gameplay.",
        "SEND LOGS uploads the logs and your save (RPG data) to the developer.",
        "CONFIRM", "BACK", "BAG", "FIGHT"
    };
    int tr_bad = 0;
    for (unsigned i = 0; i < sizeof(must_tr) / sizeof(must_tr[0]); i++) {
        if (strcmp(S(must_tr[i]), must_tr[i]) == 0) {
            fprintf(stderr, "missing PT-BR translation: %s\n", must_tr[i]);
            tr_bad++;
        }
    }
    acheck(tr_bad == 0, "all key Android UI strings resolve in i18n.h (PT-BR)");

    plat_release();
    printf("android-self-test: %s (%d failures)\n", fails ? "FAILED" : "all checks passed", fails);
    #undef acheck
    return fails ? 1 : 0;
}

int main(int argc, char **argv) {
    int kind = PLAT_SDL, tp = 1, lg = 0;
    const char *script = NULL;
    char disc[1200] = "";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test") && i + 1 < argc) return android_self_test(argv[i + 1]);
        else if (!strcmp(argv[i], "--null") && i + 1 < argc) { kind = PLAT_NULL; script = argv[++i]; }
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) plat_shot_dir = argv[++i];
        else if (!strcmp(argv[i], "--dir") && i + 1 < argc) snprintf(game_dir, sizeof(game_dir), "%s", argv[++i]);
    }
    int rc = dw3_android_launcher_run(kind, script, &tp, &lg, disc, sizeof(disc));
    printf("android-launcher rc=%d touch_pad=%d logs=%d disc=%s\n", rc, tp, lg, disc);
    return rc;
}
#endif
