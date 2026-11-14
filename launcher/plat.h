#ifndef DW3_PLAT_H
#define DW3_PLAT_H
/* The platform layer: where the canvas goes and where the buttons come from. Buttons are Linux evdev codes in the
 * handheld's own vocabulary (305 east ... see names in app.c); every backend translates to them. */
typedef struct Ev {
    int code; /* evdev code; 0 = nothing happened (a tick) */
    int down; /* 1 press, 0 release */
} Ev;

enum { PLAT_FB, PLAT_SDL, PLAT_NULL };

int plat_open(int kind, const char *script);   /* 0 on success; PLAT_NULL runs a script and writes PPM shots */
int plat_poll(Ev *ev, int timeout_ms);         /* 1 when an event was filled in, 0 on timeout; -1 when the window closed */
void plat_present(void);
void plat_release(void);                       /* stop using the screen and the pad, leaving the last picture up */
long plat_now_ms(void);                        /* monotonic; virtual (33 ms a poll) under PLAT_NULL */
void plat_desktop_size(int *w, int *h);        /* detected monitor size (1280x720 fallback if headless) */
void plat_toggle_fullscreen(void);
extern const char *plat_shot_dir;

#define K_SOUTH 304
#define K_EAST 305
#define K_WEST 307
#define K_NORTH 308
#define K_L1 310
#define K_R1 311
#define K_L2 312
#define K_R2 313
#define K_SELECT 314
#define K_START 315
#define K_UP 544
#define K_DOWN 545
#define K_LEFT 546
#define K_RIGHT 547
#endif
