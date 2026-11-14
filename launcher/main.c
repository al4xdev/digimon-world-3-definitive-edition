/* dw3-launcher: the console-first menu (plan 13). One canvas, one pad, no widgets: every screen is a short list. */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cfg.h"
#include "loading.h"
#include "plat.h"
#include "ui.h"

enum { S_HOME, S_GAME, S_HUD, S_CONTROLS, S_DEBUG, S_LED, S_LED1, S_LED2, S_RESET, S_CLEAN, S_LOADING, S_ERROR };

static Cfg c;
static int screen = S_HOME, sel, capture, quit_flag;
static char msg[160], status[160];
static int launch_pid = -1, launch_fd = -1, launch_pct, launch_started;
static long launch_t0;
static FILE *game_log;
static char launch_stage[64] = "CHECKING THE DISC";

static const char *ff_label[] = {"OFF", "2X", "4X", "8X", "UNLIMITED"};
static const char *buf_label[] = {"OFF", "SHORT", "LONG"};
static const char *bt_label[] = {"OFF", "SKIP", "ASYNC"};
static const char *log_label[] = {"OFF", "SOFT", "HEAVY"};
static const char *lay_label[] = {"PS1", "XBOX", "NINTENDO", "CUSTOM"};
static const char *act_label[A_COUNT] = {"CONFIRM", "CARD", "MENU", "BACK", "L1", "R1", "START", "SELECT"};
static const char *hud_label[H_COUNT] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "ITEM", "RUN", "INSPECT"};
/* On-screen order of the pad rows follows the pad: confirm, back, menu, card first. */
static const int act_order[A_COUNT] = {A_CROSS, A_TRIANGLE, A_CIRCLE, A_SQUARE, A_L1, A_R1, A_START, A_SELECT};

/* What the player's pad prints for a place, read through the current layout (PS1 shows the Xbox letters here: the
 * canvas has no PlayStation symbols). */
static void glyph(int code, char *out) {
    int lay = cfg_layout(&c);
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
    background(0);
    rect(0, 0, W, 64, C_BG0, 150);
    rect(0, 64, W, 2, C_CYAN, 120);
    ring(40, 32, 18, 3, C_CYAN);
    circle(40, 32, 8, C_GREEN, 256);
    text(72, 14, a, 24, C_TEXT);
    if (b) text(72 + text_w(a, 24) + 10, 20, b, 18, C_CYAN);
}

static void footer(const char *const *keys, const char *const *what, int n) {
    rect(0, H - 52, W, 52, C_BG0, 190);
    int x = 24;
    for (int i = 0; i < n; i++) {
        x += keycap(x, H - 26, keys[i], C_CYAN) + 8;
        x = text(x, H - 36, what[i], 16, C_TEXT) + 22;
    }
}
static void foot_std(const char *a) {
    const char *k[] = {"A", "B", "START"}, *w[] = {a, "BACK", "PLAY"};
    footer(k, w, 3);
}

static void row(int y, const char *label, const char *val, int selected, int bad, int on) {
    if (selected) {
        rrect(40, y - 4, W - 80, 36, 8, bad ? RGB(70, 22, 28) : C_FOCUS, 256);
        rect(40, y + 2, 4, 24, bad ? C_RED : C_GREEN, 256);
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
    int x = 112;
    x += keycap(x, 318, "A", C_GREEN) + 8;
    x = text(x, 308, a, 18, C_TEXT) + 36;
    x += keycap(x, 318, "B", C_RED) + 8;
    text(x, 308, b, 18, C_TEXT);
}

/* ---- screens ---- */
#define HOME_N 7
static const char *home_name[HOME_N] = {"PLAY", "GAME", "BATTLE HUD", "CONTROLS", "LED BAR", "DEBUG", "RESET SETTINGS"};
static const char *home_hint[HOME_N] = {
    "Start Digimon World 3 with your saved settings.", "Fast-forward, skipping and battle text.",
    "Where each battle action sits on your pad.", "Your buttons, and the letters the game shows.",
    "The handheld's LED bar follows your Digimon's HP.", "Logs for the people who help you fix a crash.",
    "Put every setting back to the tuned defaults."};

static const char *home_value(int i, int *on) {
    *on = 0;
    switch (i) {
    case 1: return "SETTINGS";
    case 2: return "CUSTOMIZE";
    case 3: return "REMAP";
    case 4: { int s = led_state(&c); *on = s == LED_ON; return s == LED_NONE ? "INSTALL" : s == LED_ON ? "ON" : "OFF"; }
    case 5: return "SETTINGS";
    case 6: return "ASK";
    }
    return NULL;
}

static void draw_home(void) {
    header("DIGIMON WORLD 3", "DEFINITIVE EDITION");
    for (int i = 0; i < HOME_N; i++) {
        int on, y = 92 + i * 44;
        const char *v = home_value(i, &on);
        if (i == sel) { rrect(40, y - 6, W - 80, 40, 8, C_FOCUS, 256); rect(40, y + 2, 4, 24, C_GREEN, 256); }
        int ix = 74, iy = y + 14;
        uint32_t ic = i == sel ? C_GOLD : C_CYAN;
        switch (i) {
        case 0: tri(ix - 7, iy - 9, ix - 7, iy + 9, ix + 9, iy, ic); break;
        case 1: ring(ix, iy, 9, 2, ic); circle(ix, iy, 3, ic, 256); break;
        case 2: ring(ix, iy, 9, 2, ic); line(ix - 12, iy, ix + 12, iy, ic); line(ix, iy - 12, ix, iy + 12, ic); break;
        case 3: rect(ix - 10, iy - 3, 20, 6, ic, 256); rect(ix - 3, iy - 10, 6, 20, ic, 256); break;
        case 4: for (int k = 0; k < 4; k++) rect(ix - 10 + k * 6, iy - 3 - (k % 2) * 5, 4, 10 + (k % 2) * 6, ic, 256); break;
        case 5: tri(ix, iy - 10, ix - 10, iy + 8, ix + 10, iy + 8, ic); break;
        case 6: ring(ix, iy, 9, 2, ic); tri(ix + 2, iy - 12, ix + 2, iy - 4, ix + 10, iy - 8, ic); break;
        }
        text(104, y + 3, home_name[i], i == 0 ? 24 : 20, i == sel ? C_GOLD : C_TEXT);
        if (v) text_r(W - 64, y + 5, v, 18, on ? C_GREEN : i == sel ? C_GOLD : C_NOTE);
    }
    rect(40, 408, W - 80, 1, C_EDGE, 256);
    text(44, 412, home_hint[sel], 16, C_NOTE);
    text_r(W - 44, 394, disc_path[0] ? "DISC READY" : "NO DISC IN game/", 14, disc_path[0] ? C_GREEN : C_RED);
    const char *k[] = {"A", "START", "SELECT+START"}, *w[] = {"SELECT", "PLAY", "QUIT"};
    footer(k, w, 3);
}

#define GAME_N 5
static void draw_game(void) {
    header("DIGIMON WORLD 3", "GAME");
    const char *v[5] = {ff_label[c.ff], c.skip ? "ON" : "OFF", c.battles ? "ON" : "OFF",
                        c.battles ? (c.hit ? "ON" : "OFF") : "-", bt_label[c.btext]};
    const char *lab[5] = {"FAST-FORWARD", "SKIP DIALOGUES", "SKIP BATTLE ANIMS", "   HIT REACTION", "BATTLE TEXT"};
    for (int i = 0; i < 5; i++) row(92 + i * 48, lab[i], v[i], i == sel, 0, strcmp(v[i], "OFF") && strcmp(v[i], "-"));
    static const char *desc[5] = {"How fast the fast-forward button runs the game.", "Dialogue boxes close on their own.",
        "Battle moves play without their animation.", "Keep the hit flinch while animations are skipped.",
        "SKIP closes battle message boxes so combat flows."};
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
    const char *rl[9] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "ITEM", "RUN", "INSPECT", "REMEMBER CURSOR", "BUFFER INPUT"};
    for (int i = 0; i < 9; i++) {
        char g[16], v[48];
        if (i < H_COUNT) { glyph(c.hud[i], g); snprintf(v, sizeof(v), "%s  [%s]", place_display(c.hud[i]), g); }
        else if (i == 7) snprintf(v, sizeof(v), "%s", c.bmem ? "ON" : "OFF");
        else snprintf(v, sizeof(v), "%s", buf_label[c.bbuf]);
        int y = 180 + i * 25, bd = i < H_COUNT && (dm >> i & 1);
        if (i == sel) { rrect(40, y - 3, W - 80, 25, 6, bd ? RGB(70, 22, 28) : C_FOCUS, 256); rect(40, y + 1, 4, 17, C_GREEN, 256); }
        text(64, y, rl[i], 17, bd ? C_RED : i == sel ? C_GOLD : C_TEXT);
        text_r(W - 64, y, v, 17, bd ? C_RED : i == sel ? C_GOLD : C_NOTE);
    }
    dup_note(bad);
    const char *k[] = {"<>", "B", "START"}, *w[] = {"CHANGE", "SAVE & BACK", "PLAY"};
    footer(k, w, 3);
}

static void draw_controls(void) {
    header("DIGIMON WORLD 3", "CONTROLS");
    unsigned dm = 0;
    int bad = dup_actions(&c, &dm);
    char v[48];
    snprintf(v, sizeof(v), "< %s >", lay_label[cfg_layout(&c)]);
    row(78, "BUTTON GLYPHS", v, sel == 0, 0, 0);
    rect(60, 120, W - 120, 1, C_EDGE, 256);
    for (int i = 0; i < A_COUNT; i++) {
        int a = act_order[i], y = 128 + i * 36, isel = sel == i + 1, bd = dm >> a & 1;
        char g[16];
        glyph(c.pad[a].code, g);
        if (capture && isel) snprintf(v, sizeof(v), "PRESS A BUTTON...");
        else snprintf(v, sizeof(v), "%s  [%s]", place_display(c.pad[a].code), g);
        row(y, act_label[a], v, isel, bd, 0);
    }
    dup_note(bad);
    const char *k[] = {"A", "<>", "B"}, *w[] = {"REBIND", "GLYPHS", "SAVE & BACK"};
    footer(k, w, 3);
}

static void draw_debug(void) {
    header("DIGIMON WORLD 3", "DEBUG");
    row(92, "LOGS", log_label[c.logs], sel == 0, 0, c.logs > 0);
    row(140, "SAVE LOGS", "ZIP", sel == 1, 0, 0);
    row(188, "CLEAN LOGS", "ASK", sel == 2, 0, 0);
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
    const char *k[] = {"A", "B"}, *w[] = {"INSTALL", "CANCEL"};
    footer(k, w, 2);
}

static void draw_loading(void) {
    char stage[80];
    snprintf(stage, sizeof(stage), "%s...", launch_stage);
    load_draw(&c, plat_now_ms() - launch_t0, stage, launch_pct);
}

static void draw_error(void) {
    header("DIGIMON WORLD 3", "COULD NOT START");
    rrect(50, 110, W - 100, 200, 12, C_PANEL, 256);
    rrect_line(50, 110, W - 100, 200, 12, C_RED);
    text(76, 130, msg, 20, C_RED);
    text(76, 180, "Turn LOGS to HEAVY in DEBUG, try again, then SAVE LOGS.", 16, C_NOTE);
    foot_std("OK");
}

static void draw(void) {
    switch (screen) {
    case S_HOME: draw_home(); break;
    case S_GAME: draw_game(); break;
    case S_HUD: draw_hud(); break;
    case S_CONTROLS: draw_controls(); break;
    case S_DEBUG: draw_debug(); break;
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

/* ---- launching ---- */
static void start_game(void) {
    if (!disc_path[0]) {
        snprintf(msg, sizeof(msg), "NO DISC IN game/");
        screen = S_ERROR;
        return;
    }
    cfg_save(&c);
    /* What the script used to export after its menu: the game reads the player's pad map itself, and the log level
     * is the DEBUG screen's choice (the game gets "off" unless it was asked for more). */
    setenv("DW3_PAD_REMAP", "1", 1);
    setenv("DW3_LOG", c.logs == 2 ? "heavy" : c.logs == 1 ? "soft" : "off", 1);
    if (getenv("DW3_NOEXEC")) { /* test hook: the loading screen without a game */
        launch_pid = -2;
    } else {
        int p[2];
        if (pipe(p) != 0) return;
        launch_pid = fork();
        if (launch_pid == 0) {
            dup2(p[1], 1);
            dup2(p[1], 2);
            close(p[0]);
            close(p[1]);
            if (chdir(game_dir) != 0) _exit(126);
            const char *ex = getenv("DW3_EXEC");
            if (ex) execl("/bin/sh", "sh", "-c", ex, (char *)NULL);
            else execl("./dw2003", "dw2003", "--config", "./settings.json", "--window", "--fullscreen", "--watchdog", "0", (char *)NULL);
            _exit(127);
        }
        close(p[1]);
        char lp[700];
        snprintf(lp, sizeof(lp), "%s/logs", game_dir);
        mkdir(lp, 0755);
        snprintf(lp, sizeof(lp), "%s/logs/game.log", game_dir);
        game_log = fopen(lp, "a");
        launch_fd = p[0];
        fcntl(launch_fd, F_SETFL, O_NONBLOCK);
    }
    launch_t0 = plat_now_ms();
    launch_pct = 0;
    launch_started = 0;
    char best[700];
    snprintf(best, sizeof(best), "%s/loading.best", game_dir);
    load_init(best);
    screen = S_LOADING;
}

/* Reads what the game prints: "disc" lines move the stage along, "start" means its window is about to open. */
static void pump_game(void) {
    char buf[512];
    static char line[512];
    static int ln;
    if (launch_fd >= 0) {
        ssize_t n;
        while ((n = read(launch_fd, buf, sizeof(buf))) > 0) {
            if (game_log) { fwrite(buf, 1, (size_t)n, game_log); fflush(game_log); }
            for (ssize_t i = 0; i < n; i++) {
                if (buf[i] == '\n' || ln == (int)sizeof(line) - 1) {
                    line[ln] = 0;
                    ln = 0;
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
            if (WIFEXITED(st) && WEXITSTATUS(st) == 0) { quit_flag = 1; return; }
            snprintf(msg, sizeof(msg), WIFSIGNALED(st) ? "THE GAME CRASHED (SIGNAL %d)" : "THE GAME EXITED WITH CODE %d",
                     WIFSIGNALED(st) ? WTERMSIG(st) : WEXITSTATUS(st));
            if (launch_fd >= 0) { close(launch_fd); launch_fd = -1; }
            screen = S_ERROR;
        }
    }
}

/* ---- input ---- */
static int held_select, held_start;

static void go(int s, int to_sel) { screen = s; sel = to_sel; capture = 0; status[0] = 0; msg[0] = 0; }

/* Leaving an edit screen saves it, unless two rows share a button: then it stays (the red rows say why). */
static void leave_edit(int back_sel) {
    if (screen == S_HUD && dup_hud(&c, NULL)) return;
    if (screen == S_CONTROLS && dup_actions(&c, NULL)) return;
    cfg_save(&c);
    go(S_HOME, back_sel);
}

static void home_enter(void) {
    switch (sel) {
    case 0: start_game(); return;
    case 1: go(S_GAME, 0); return;
    case 2: go(S_HUD, 0); return;
    case 3: go(S_CONTROLS, 0); return;
    case 4:
        if (led_state(&c) == LED_NONE) { if (led_compatible()) go(S_LED, 0); else screen = S_LED1; }
        else { c.led = !c.led; cfg_save(&c); }
        return;
    case 5: go(S_DEBUG, 0); return;
    case 6: screen = S_RESET; return;
    }
}

static void on_press(int code) {
    int d = code == K_LEFT ? -1 : code == K_RIGHT ? 1 : 0, ok = code == K_SOUTH, back = code == K_EAST;
    if (screen == S_LOADING) { load_press(code); return; }
    if (code == K_SELECT) held_select = 1;
    if (code == K_START) held_start = 1;
    if (held_select && held_start) { quit_flag = 2; return; } /* leaves without saving the edit in progress */
    if (screen == S_CONTROLS && capture) {
        static const int valid[] = {K_SOUTH, K_EAST, K_WEST, K_NORTH, K_L1, K_R1, K_L2, K_R2, K_START, K_SELECT};
        for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
            if (valid[i] != code) continue;
            int a = act_order[sel - 1];
            c.pad[a].code = code;
            if (a <= A_TRIANGLE) {
                int lay = cfg_layout(&c);
                c.pad[a].letter = layout_letter(lay == LAYOUT_CUSTOM || lay == LAYOUT_PS1 ? LAYOUT_XBOX : lay, code);
            }
            capture = 0;
            return;
        }
        return;
    }
    int n = 0;
    switch (screen) {
    case S_HOME: n = HOME_N; break;
    case S_GAME: n = GAME_N; break;
    case S_HUD: n = 9; break;
    case S_CONTROLS: n = 1 + A_COUNT; break;
    case S_DEBUG: n = 3; break;
    }
    if (n && code == K_UP) sel = (sel + n - 1) % n;
    if (n && code == K_DOWN) sel = (sel + 1) % n;
    int s = d ? d : 1;
    switch (screen) {
    case S_HOME:
        if (ok) home_enter();
        if (code == K_START) start_game();
        break;
    case S_GAME:
        if (code == K_START) start_game();
        if (back) leave_edit(1);
        if (ok || d) {
            switch (sel) {
            case 0: c.ff = (c.ff + s + 5) % 5; break;
            case 1: c.skip = !c.skip; break;
            case 2: c.battles = !c.battles; if (!c.battles) c.hit = 0; break;
            case 3: if (c.battles) c.hit = !c.hit; break;
            case 4: c.btext = (c.btext + s + 3) % 3; break;
            }
        }
        break;
    case S_HUD:
        if (code == K_START && !dup_hud(&c, NULL)) start_game();
        if (back) leave_edit(2);
        if (ok || d) {
            if (sel < H_COUNT) c.hud[sel] = hud_cycle(c.hud[sel], s);
            else if (sel == 7) c.bmem = !c.bmem;
            else c.bbuf = (c.bbuf + s + 3) % 3;
        }
        break;
    case S_CONTROLS:
        if (code == K_START && !dup_actions(&c, NULL)) start_game();
        if (back) leave_edit(3);
        if (sel == 0 && (ok || d)) {
            static const int cyc[] = {LAYOUT_XBOX, LAYOUT_NINTENDO, LAYOUT_PS1};
            int cur = cfg_layout(&c), i = 0;
            for (; i < 3; i++) if (cyc[i] == cur) break;
            cfg_set_layout(&c, cyc[(i + s + 3) % 3]);
        } else if (sel > 0 && ok) capture = 1;
        break;
    case S_DEBUG:
        if (code == K_START) start_game();
        if (back) { cfg_save(&c); go(S_HOME, 5); }
        if (sel == 0 && (ok || d)) c.logs = (c.logs + s + 3) % 3;
        if (sel == 1 && ok) {
            char nm[80];
            int r = logs_save(nm, sizeof(nm));
            snprintf(status, sizeof(status), r == 0 ? "SAVED %s" : "COULD NOT SAVE THE ZIP%.0s", nm);
        }
        if (sel == 2 && ok) screen = S_CLEAN;
        break;
    case S_LED:
        if (back) go(S_HOME, 4);
        if (ok && led_install(msg, sizeof(msg)) == 0) { c.led = 1; cfg_save(&c); go(S_HOME, 4); }
        break;
    case S_LED1:
        if (back) screen = S_HOME;
        if (ok) screen = S_LED2;
        break;
    case S_LED2:
        if (back) screen = S_HOME;
        if (ok) {
            if (led_install(msg, sizeof(msg)) == 0) { c.led = 1; cfg_save(&c); }
            screen = S_HOME;
        }
        break;
    case S_RESET:
        if (back) screen = S_HOME;
        if (ok) { cfg_defaults(&c); cfg_save(&c); go(S_HOME, 0); }
        break;
    case S_CLEAN:
        if (back) screen = S_DEBUG;
        if (ok) { snprintf(status, sizeof(status), "REMOVED %d FILES", logs_clean()); screen = S_DEBUG; }
        break;
    case S_ERROR:
        if (ok || back) go(S_HOME, 0);
        break;
    }
}

int main(int argc, char **argv) {
    int kind = PLAT_FB;
    const char *script = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sdl")) kind = PLAT_SDL;
        else if (!strcmp(argv[i], "--fb")) kind = PLAT_FB;
        else if (!strcmp(argv[i], "--null") && i + 1 < argc) { kind = PLAT_NULL; script = argv[++i]; }
        else if (!strcmp(argv[i], "--dir") && i + 1 < argc) snprintf(game_dir, sizeof(game_dir), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) plat_shot_dir = argv[++i];
    }
    signal(SIGPIPE, SIG_IGN);
    find_disc();
    cfg_load(&c);
    ui_init();
    if (plat_open(kind, script) != 0) {
        if (kind == PLAT_FB && plat_open(PLAT_SDL, NULL) == 0) kind = PLAT_SDL;
        else { fprintf(stderr, "dw3-launcher: no screen to draw on\n"); return 3; }
    }
    long last = plat_now_ms();
    for (;;) {
        Ev ev = {0};
        int r = plat_poll(&ev, 33);
        if (r < 0) break;
        if (r > 0 && ev.code) {
            if (ev.down) on_press(ev.code);
            else if (ev.code == K_SELECT) held_select = 0;
            else if (ev.code == K_START) held_start = 0;
        }
        long now = plat_now_ms();
        if (screen == S_LOADING) {
            load_step(now - last);
            pump_game();
            if (launch_started && screen == S_LOADING && launch_pid != -2) {
                /* the game's window is next: leave the last picture up, then give the screen away */
                draw();
                plat_present();
                usleep(400000);
                plat_release();
                int st = 0;
                if (launch_pid > 0) waitpid(launch_pid, &st, 0);
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
