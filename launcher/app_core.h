#ifndef DW3_APP_CORE_H
#define DW3_APP_CORE_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cfg.h"
#include "cloud.h"
#include "compat.h" /* localtime_r on Windows */

/* Canonical UI label tables shared by desktop (main.c) and Android (main_android.c). */
static const char *const ff_label[5] = {"OFF", "2X", "4X", "8X", "UNLIMITED"};
static const char *const load_label[5] = {"OFF", "2X", "4X", "8X", "INSTANT"};
static const char *const lang_label[7] = {
    "ENGLISH", "PORTUGUÊS (BR)", "FRANÇAIS", "DEUTSCH", "ITALIANO", "ESPAÑOL", "JAPANESE"
};
static const char *const buf_label[3] = {"OFF", "SHORT", "LONG"};
static const char *const bt_label[3] = {"OFF", "SKIP", "ASYNC"};
static const char *const fight_label[3] = {"CUSTOM", "BOTTOM", "RIGHT"};
#define HUD_FIGHT_ROW (H_COUNT + 2)
#define HUD_ROWS (H_COUNT + 3)
static const char *const log_label[3] = {"OFF", "SOFT", "HEAVY"};
/* SEND LOGS with LOGS at OFF (launcher/i18n.h has its translations). */
#define APP_LOGS_OFF "LOGS ARE OFF: SET HEAVY (OR SOFT), REPEAT THE BUG, SEND"
static const char *const lay_label[4] = {"PS1", "XBOX", "NINTENDO", "CUSTOM"};
static const char *const filt_label[5] = {"NONE", "SHARP", "SCANLINES", "CRT", "SMOOTH"};
static const char *const vert_label[3] = {"PS1", "SMOOTH", "SMOOTH + TEXTURES"};
static const char *const act_label[A_COUNT] = {"CONFIRM", "CARD", "BACK", "MENU", "L1", "R1", "START", "SELECT"};
static const char *const hud_label[H_COUNT] = {"FIGHT", "TECH", "DIGIVOLVE", "TAG", "BAG", "RUN", "INFO", "BLAST"};
static const int act_order[A_COUNT] = {A_CROSS, A_CIRCLE, A_TRIANGLE, A_SQUARE, A_L1, A_R1, A_START, A_SELECT};

static inline void app_fmt_vram(const Cfg *c, char *out, size_t n) {
    int vw = (c->widescreen && c->renderer_pc == 0 ? 1536 : 1024) * c->iscale;
    int vh = 512 * c->iscale;
    snprintf(out, n, "%dX (%dx%d VRAM)", c->iscale, vw, vh);
}

static inline void app_fmt_epoch(long ts, const char *unknown_lbl, char *out, size_t n) {
    if (ts <= 0) {
        snprintf(out, n, "%s", unknown_lbl ? unknown_lbl : "?");
        return;
    }
    time_t t = (time_t)ts;
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%d %H:%M", &tm);
}

static inline void app_reset_cfg(Cfg *c) {
    cfg_defaults(c);
    cfg_save(c);
}

static inline void app_step_backups(Cfg *c, int delta) {
    c->backup_max += delta;
    if (c->backup_max < 1) c->backup_max = 1;
    if (c->backup_max > 100) c->backup_max = 100;
}

static inline void app_step_sync_interval(Cfg *c, int dir) {
    static const int ivs[] = {0, 5, 10, 15, 20, 30};
    int k = 0;
    for (int j = 0; j < 6; j++) {
        if (c->sync_interval_min == ivs[j]) k = j;
    }
    c->sync_interval_min = ivs[(k + dir + 6) % 6];
}

static inline void app_send_logs(const CloudState *cloud, const char *version, int portmaster,
                                 char *status_out, size_t status_n) {
    char nm[128], up[128], id[40] = "", err[96] = "";
    /* The upload blocks the screen, so presses made meanwhile arrive after it: each one sent the same zip again and
     * spent the player's 5 uploads an hour. A second press within 30 s keeps the result already shown. */
    static time_t last_send;
    time_t now = time(NULL);
    if (last_send != 0 && now - last_send < 30 && now >= last_send) return;
    last_send = now;
    if (logs_save(nm, sizeof(nm)) != 0) {
        snprintf(status_out, status_n, "COULD NOT SAVE THE ZIP");
        return;
    }
    if (logs_pack(up, sizeof(up), 1) != 0) {
        snprintf(status_out, status_n, "SAVED %s", nm);
        return;
    }
    char zp[1400];
    snprintf(zp, sizeof(zp), "%s/%s", game_dir, up);
    int rc = cloud_send_logs(cloud, zp, version ? version : "", portmaster, id, sizeof(id), err, sizeof(err));
    unlink(zp);
    if (rc == 0) snprintf(status_out, status_n, "SENT. REPORT %s. A COPY IS SAVED", id);
    else snprintf(status_out, status_n, "NOT SENT: %s. ZIP SAVED", err);
}

#endif
