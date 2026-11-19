#ifndef DW3_ANDROID_TOUCH_PAD_H
#define DW3_ANDROID_TOUCH_PAD_H

#include <stdint.h>

void dw3_touch_pad_set_enabled(int enabled);
int dw3_touch_pad_enabled(void);
/* The face buttons as the player's controller setting has them, indexed cross, square, circle, triangle (cfg.h's A_*
 * order): place[i] is where that PS1 button sits (TP_PLACE_*), letter[i] the letter the game shows for it ('A', 'B',
 * 'X', 'Y'), or '-' to keep the PS1 shapes. Without a call, or with places that do not cover the four spots once
 * each, the pad keeps the PS1 diamond. */
enum { TP_PLACE_BOTTOM = 0, TP_PLACE_RIGHT, TP_PLACE_LEFT, TP_PLACE_TOP };
void dw3_touch_pad_set_faces(const int place[4], const char letter[4]);
int port_touch_draw_rgba(uint32_t *rgba, int w, int h, const int game_rect[4]);

#endif
