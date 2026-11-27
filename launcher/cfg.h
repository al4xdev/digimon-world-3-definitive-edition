#ifndef DW3_CFG_H
#define DW3_CFG_H
/* What the launcher edits and writes: tui.state, pad.map and settings.json (the same files and formats as the
 * Python menu it replaced, so the game and an older menu read them unchanged). */
#include <stddef.h>

enum { A_CROSS, A_SQUARE, A_CIRCLE, A_TRIANGLE, A_L1, A_R1, A_START, A_SELECT, A_COUNT };
enum { H_FIGHT, H_TECH, H_DV, H_TAG, H_ITEM, H_RUN, H_INSPECT, H_BLAST, H_COUNT };
enum { LAYOUT_PS1, LAYOUT_XBOX, LAYOUT_NINTENDO, LAYOUT_CUSTOM };

typedef struct Bind {
    int code;    /* evdev code of the button (the handheld's vocabulary) */
    char letter; /* what the game's text shows: A B X Y L R S E, or '-' for the button's own symbol */
} Bind;

typedef struct Cfg {
    int hz;                  /* 50 or 60: GAME screen's REFRESH RATE; default 60 */
    int lang;                /* 0 english, 1 portuguese, 2 french, 3 german, 4 italian, 5 spanish, 6 japanese */
    int ff;                  /* 0 off, 1 2x, 2 4x, 3 8x, 4 unlimited */
    int load;                /* fast_loading: 0 off, 1 2x, 2 4x, 3 8x, 4 instant */
    int skip, battles, hit;  /* skip dialogues, skip battle animations, hit reaction */
    int bmem, bbuf;          /* remember cursor; buffer input 0 off 1 short 2 long */
    int logs;                /* 0 off 1 soft 2 heavy */
    int led;                 /* the led_bar mod's switch in settings.json */
    int widescreen;          /* 0 off, 1 16:9 (needs GPU renderer on PC) */
    int fullscreen;          /* 1 on handheld, 0/1 on PC */
    int touch_pad;           /* 1 show floating virtual gamepad on Android, 0 hide */
    int fill;                /* 1 fill screen preserving aspect, 0 exact integer scale (PC) */
    int scale;               /* PC window scale: 0 = auto from monitor, 2..6 manual */
    int renderer_pc;         /* 0 gpu, 1 software (PC only) */
    int filter;              /* 0 none, 1 sharp, 2 scanlines, 3 crt, 4 smooth (PC GPU) */
    int iscale;              /* video.internal_scale: 1..8 times the PS1's resolution (PC GPU) */
    int subpixel;            /* video.subpixel: 0 off, 1 on, 2 perspective (PC GPU, above iscale 1) */
    int backup_max;          /* 1..100 local rotating card1.mcd snapshots */
    int sync_enabled;        /* 0 off, 1 cloud cross-play sync active */
    int sync_interval_min;   /* 0 = start/exit only, else 5, 10, 15, 20, 30 min */
    int update_check;        /* 1: the Android launcher asks GitHub for a newer release (update.h) */
    int v091_save_ok;        /* 1: player answered the v0.9.1 fresh-save recommendation */
    int hud[H_COUNT];        /* evdev code of each battle action's button */
    Bind pad[A_COUNT];
} Cfg;

extern int is_pc, is_portmaster, is_android;
extern char game_dir[512];
extern char disc_path[1200];  /* "" when there is none */

/* Whole-file helpers (cfg.c): the text of a file (malloc'd; NULL when unreadable or past 4 MB), and a write that replaces it
 * atomically. */
char *slurp(const char *path);
int write_atomic(const char *path, const char *text);

typedef struct {
    char name[256];
    int is_dir;
} BrowseItem;

void cfg_defaults(Cfg *c);
void cfg_load(Cfg *c);
int cfg_save(const Cfg *c);          /* tui.state, pad.map and (with a disc) settings.json; 0 on success */
int cfg_auto_scale(const Cfg *c);    /* integer game scale fitting the detected monitor */
int browse_disc(void);               /* opens OS file picker on PC; 0 when disc_path updated */
int browse_read(const char *dir, BrowseItem *out, int max_items);
void browse_start_dir(char *out, size_t n);
void cfg_set_layout(Cfg *c, int layout);
int cfg_layout(const Cfg *c);
char layout_letter(int layout, int code);
const char *place_display(int code);  /* "SOUTH", "EAST", ... as the handheld's pad is printed */
/* The "Fight button" row: Fight and Tag on the face buttons, Fight at the bottom (the default) or on
 * the right (the layout before). cfg_fight_place: 0 bottom (Fight south, Tag east), 1 right (Fight east, Tag south),
 * -1 neither (the player put them elsewhere on the per-action rows). cfg_set_fight_place moves the two, and whatever
 * action held a button they take gets the one its taker left, so no button ends up on two rows. */
int cfg_fight_place(const Cfg *c);
void cfg_set_fight_place(Cfg *c, int right);
const char *code_name(int code);      /* the kernel's name for it (settings.json's) */
int dup_actions(const Cfg *c, unsigned *mask_out);   /* pad rows sharing a button: count; mask of the rows */
int dup_hud(const Cfg *c, unsigned *mask_out);       /* battle actions sharing a button */
int hud_cycle(int code, int dir);     /* the next place in the HUD's cycle */
void find_disc(void);
int cfg_confirm_code(const Cfg *c);
int cfg_back_code(const Cfg *c);

/* the LED bar */
enum { LED_NONE, LED_OFF, LED_ON };
int led_state(const Cfg *c);          /* LED_NONE until a daemon answers or the shared copy is on the card */
int led_compatible(void);             /* an RK3326 with the LED UART */
int led_install(char *why, size_t n); /* 0 on success */

/* debug */
int logs_save(char *name, size_t n);
/* The same zip; upload: the one SEND LOGS posts (dw3-send-*.zip, each log cut to its last 200 KB, a crash report to 100 KB,
 * the whole under 1.3 MB so it fits the server's 1.5 MB; the home folder's name taken out of the text). Both carry the player's card and system.txt. */
int logs_pack(char *name, size_t n, int upload);
int logs_clean(void);
#endif
