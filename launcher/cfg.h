#ifndef DW3_CFG_H
#define DW3_CFG_H
/* What the launcher edits and writes: tui.state, pad.map and settings.json (the same files and formats as the
 * handheld/dw3-menu.py it replaces, so the game and an older menu read them unchanged). */
#include <stddef.h>

enum { A_CROSS, A_SQUARE, A_CIRCLE, A_TRIANGLE, A_L1, A_R1, A_START, A_SELECT, A_COUNT };
enum { H_FIGHT, H_TECH, H_DV, H_TAG, H_ITEM, H_RUN, H_INSPECT, H_COUNT };
enum { LAYOUT_PS1, LAYOUT_XBOX, LAYOUT_NINTENDO, LAYOUT_CUSTOM };

typedef struct Bind {
    int code;    /* evdev code of the button (the handheld's vocabulary) */
    char letter; /* what the game's text shows: A B X Y L R S E, or '-' for the button's own symbol */
} Bind;

typedef struct Cfg {
    int hz;                  /* 50 or 60: kept from the file, no screen edits it any more */
    int ff;                  /* 0 off, 1 2x, 2 4x, 3 8x, 4 unlimited */
    int skip, battles, hit;  /* skip dialogues, skip battle animations, hit reaction */
    int btext;               /* 0 off, 1 skip, 2 async */
    int bmem, bbuf;          /* remember cursor; buffer input 0 off 1 short 2 long */
    int logs;                /* 0 off 1 soft 2 heavy */
    int led;                 /* the led_bar mod's switch in settings.json */
    int widescreen;          /* 0 off, 1 16:9 (needs GPU renderer on PC) */
    int fullscreen;          /* 1 on handheld, 0/1 on PC */
    int fill;                /* 1 fill screen preserving aspect, 0 exact integer scale (PC) */
    int scale;               /* PC window scale: 0 = auto from monitor, 2..6 manual */
    int renderer_pc;         /* 0 gpu, 1 software (PC only) */
    int filter;              /* 0 none, 1 sharp, 2 scanlines, 3 crt, 4 smooth (PC GPU) */
    int backup_max;          /* 1..100 local rotating card1.mcd snapshots */
    int sync_enabled;        /* 0 off, 1 cloud cross-play sync active */
    int sync_interval_min;   /* 0 = start/exit only, else 5, 10, 15, 20, 30 min */
    int hud[H_COUNT];        /* evdev code of each battle action's button */
    Bind pad[A_COUNT];
} Cfg;

extern int is_pc, is_portmaster;
extern char game_dir[512];
extern char disc_path[1200];  /* "" when there is none */

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
int logs_clean(void);
#endif
