#ifndef DW3_UI_H
#define DW3_UI_H
/* A logical 640x480 canvas (0x00RRGGBB) rendered at ui_scale (1x on handheld fbdev, 2x/3x on desktop monitors). */
#include <stdint.h>

#define W 640
#define H 480

extern uint32_t *fb;
extern int ui_scale, ui_w, ui_h;

#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

/* The palette (docs: plan 13 "Paleta"). */
#define C_BG0 RGB(7, 16, 25)
#define C_BG1 RGB(12, 27, 42)
#define C_GRID RGB(16, 40, 58)
#define C_PANEL RGB(16, 36, 54)
#define C_EDGE RGB(31, 67, 94)
#define C_FOCUS RGB(21, 57, 74)
#define C_CYAN RGB(0, 229, 195)
#define C_GREEN RGB(64, 255, 128)
#define C_GOLD RGB(255, 204, 51)
#define C_ORANGE RGB(255, 140, 64)
#define C_RED RGB(255, 77, 77)
#define C_TEXT RGB(230, 241, 245)
#define C_NOTE RGB(139, 163, 181)
#define C_OFF RGB(110, 132, 148)

uint32_t mix(uint32_t a, uint32_t b, int t256);                  /* a + (b-a)*t/256 */
void px_raw(int px, int py, uint32_t c, int a256);
void rect(int x, int y, int w, int h, uint32_t c, int a256);
void rrect(int x, int y, int w, int h, int r, uint32_t c, int a256);
void rrect_line(int x, int y, int w, int h, int r, uint32_t c);
void circle(int cx, int cy, int r, uint32_t c, int a256);
void ring(int cx, int cy, int r, int thick, uint32_t c);
void line(int x0, int y0, int x1, int y1, uint32_t c);
void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c);
void background(int tick);                                       /* the Digital World backdrop */

/* Text: size in logical pixels; returns the logical width. Drawn with stb_truetype from the embedded font. */
int text_w(const char *s, int size);
int text(int x, int y, const char *s, int size, uint32_t c);      /* y = top of the line */
int text_c(int cx, int y, const char *s, int size, uint32_t c);   /* centred on cx */
int text_r(int rx, int y, const char *s, int size, uint32_t c);   /* right-aligned to rx */

/* A button as the pad prints it: a disc for one character, a pill for "L1" or "START". Returns its logical width. */
int keycap(int x, int cy, const char *label, uint32_t c);
void ui_init(int scale);
void ui_set_lang(int lang);
#endif
