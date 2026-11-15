#ifndef DW3_ANDROID_TOUCH_PAD_H
#define DW3_ANDROID_TOUCH_PAD_H

#include <stdint.h>

void dw3_touch_pad_set_enabled(int enabled);
int dw3_touch_pad_enabled(void);
int port_touch_draw_rgba(uint32_t *rgba, int w, int h, const int game_rect[4]);

#endif
