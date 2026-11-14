/* Floating multi-touch virtual PlayStation gamepad overlay for Android (arm64-v8a).
 * Hooks into psxstack/runtime/input.c (port_touch_event) and psxstack/runtime/video.c (port_touch_draw),
 * toggled from the GAME settings screen in the parallel Android launcher (DW3_TOUCH_PAD=1/0). */
#include "touch_pad.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PAD_SELECT (1u << 0)
#define PAD_START (1u << 3)
#define PAD_UP (1u << 4)
#define PAD_RIGHT (1u << 5)
#define PAD_DOWN (1u << 6)
#define PAD_LEFT (1u << 7)
#define PAD_L2 (1u << 8)
#define PAD_R2 (1u << 9)
#define PAD_L1 (1u << 10)
#define PAD_R1 (1u << 11)
#define PAD_TRIANGLE (1u << 12)
#define PAD_CIRCLE (1u << 13)
#define PAD_CROSS (1u << 14)
#define PAD_SQUARE (1u << 15)

extern void port_input_set_touch_bits(uint16_t bits);

#define MAX_TOUCH_FINGERS 8

typedef struct {
    int active;
    SDL_FingerID id;
    float x, y;
} TouchFinger;

typedef struct {
    uint16_t bit;
    float cx, cy, r;
    const char *label;
    uint8_t cr, cg, cb;
} PadBtn;

static int tp_enabled = -1;
static int tp_collapsed = 0;
static TouchFinger tp_fingers[MAX_TOUCH_FINGERS];
static uint16_t tp_bits;

static const PadBtn tp_buttons[] = {
    /* Face diamond (right side) */
    {PAD_CROSS,    0.87f, 0.83f, 0.055f, "X",   80, 170, 255},
    {PAD_CIRCLE,   0.94f, 0.72f, 0.055f, "O",  255,  96,  96},
    {PAD_SQUARE,   0.80f, 0.72f, 0.055f, "[]", 220, 120, 255},
    {PAD_TRIANGLE, 0.87f, 0.61f, 0.055f, "/\\", 64, 235, 160},
    /* Shoulders */
    {PAD_L1,       0.08f, 0.14f, 0.052f, "L1",   0, 220, 195},
    {PAD_L2,       0.08f, 0.27f, 0.048f, "L2",   0, 180, 165},
    {PAD_R1,       0.92f, 0.14f, 0.052f, "R1",   0, 220, 195},
    {PAD_R2,       0.92f, 0.27f, 0.048f, "R2",   0, 180, 165},
    /* Select / Start */
    {PAD_SELECT,   0.37f, 0.08f, 0.042f, "SEL", 180, 200, 220},
    {PAD_START,    0.63f, 0.08f, 0.042f, "STA", 180, 200, 220},
};
#define N_TP_BTNS ((int)(sizeof(tp_buttons) / sizeof(tp_buttons[0])))

static void tp_ensure_init(void) {
    if (tp_enabled >= 0) return;
    const char *e = getenv("DW3_TOUCH_PAD");
    tp_enabled = (!e || e[0] != '0') ? 1 : 0;
}

void dw3_touch_pad_set_enabled(int enabled) {
    tp_enabled = enabled ? 1 : 0;
    if (!tp_enabled) {
        memset(tp_fingers, 0, sizeof(tp_fingers));
        tp_bits = 0;
        port_input_set_touch_bits(0);
    }
}

int dw3_touch_pad_enabled(void) {
    tp_ensure_init();
    return tp_enabled;
}

static void tp_recompute(void) {
    uint16_t bits = 0;
    if (!tp_enabled || tp_collapsed) {
        tp_bits = 0;
        port_input_set_touch_bits(0);
        return;
    }
    const float dpad_cx = 0.13f, dpad_cy = 0.72f, dpad_r = 0.12f;
    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        if (!tp_fingers[i].active) continue;
        float fx = tp_fingers[i].x, fy = tp_fingers[i].y;

        /* 8-way D-Pad zone */
        float dx = fx - dpad_cx, dy = fy - dpad_cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= dpad_r && dist > 0.015f) {
            if (dx < -0.025f) bits |= PAD_LEFT;
            if (dx > 0.025f) bits |= PAD_RIGHT;
            if (dy < -0.025f) bits |= PAD_UP;
            if (dy > 0.025f) bits |= PAD_DOWN;
        }

        /* Individual buttons */
        for (int b = 0; b < N_TP_BTNS; b++) {
            float bx = fx - tp_buttons[b].cx;
            float by = (fy - tp_buttons[b].cy) * 0.65f; /* aspect compensate normalized coords */
            if (bx * bx + by * by <= tp_buttons[b].r * tp_buttons[b].r * 1.45f) {
                bits |= tp_buttons[b].bit;
            }
        }
    }
    tp_bits = bits;
    port_input_set_touch_bits(bits);
}

void port_touch_event(const void *sdl_event) {
    const SDL_Event *e = (const SDL_Event *)sdl_event;
    tp_ensure_init();
    if (!tp_enabled) return;

    float fx = e->tfinger.x;
    float fy = e->tfinger.y;
    SDL_FingerID fid = e->tfinger.fingerId;

    /* Top-center quick hide/show toggle pill when floating pad is enabled */
    if (e->type == SDL_FINGERDOWN && fx >= 0.45f && fx <= 0.55f && fy >= 0.0f && fy <= 0.09f) {
        tp_collapsed = !tp_collapsed;
        tp_recompute();
        return;
    }

    if (e->type == SDL_FINGERDOWN) {
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (!tp_fingers[i].active) {
                tp_fingers[i].active = 1;
                tp_fingers[i].id = fid;
                tp_fingers[i].x = fx;
                tp_fingers[i].y = fy;
                break;
            }
        }
    } else if (e->type == SDL_FINGERMOTION) {
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (tp_fingers[i].active && tp_fingers[i].id == fid) {
                tp_fingers[i].x = fx;
                tp_fingers[i].y = fy;
                break;
            }
        }
    } else if (e->type == SDL_FINGERUP) {
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (tp_fingers[i].active && tp_fingers[i].id == fid) {
                tp_fingers[i].active = 0;
                break;
            }
        }
    }
    tp_recompute();
}

static void draw_box(SDL_Renderer *r, int x, int y, int w, int h, uint8_t cr, uint8_t cg, uint8_t cb, int lit) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, cr / 4, cg / 4, cb / 4, lit ? 190 : 105);
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, cr, cg, cb, lit ? 245 : 175);
    SDL_RenderDrawRect(r, &rc);
}

static void draw_tiny_char(SDL_Renderer *r, int x, int y, int s, char ch) {
    /* Minimal 5x5 bitmap hints for button labels (drawn with crisp lines/rects, zero font deps). */
    SDL_Rect dot = {x, y, s, s};
    (void)dot;
    switch (ch) {
    case 'X':
        SDL_RenderDrawLine(r, x, y, x + s, y + s);
        SDL_RenderDrawLine(r, x + s, y, x, y + s);
        break;
    case 'O': {
        SDL_Rect rc = {x, y, s, s};
        SDL_RenderDrawRect(r, &rc);
        break;
    }
    case '^':
        SDL_RenderDrawLine(r, x + s / 2, y, x, y + s);
        SDL_RenderDrawLine(r, x + s / 2, y, x + s, y + s);
        SDL_RenderDrawLine(r, x, y + s, x + s, y + s);
        break;
    case 'L':
        SDL_RenderDrawLine(r, x, y, x, y + s);
        SDL_RenderDrawLine(r, x, y + s, x + s * 3 / 4, y + s);
        break;
    case 'R':
        SDL_RenderDrawLine(r, x, y, x, y + s);
        SDL_RenderDrawLine(r, x, y, x + s * 3 / 4, y + s / 3);
        SDL_RenderDrawLine(r, x + s * 3 / 4, y + s / 3, x, y + s / 2);
        SDL_RenderDrawLine(r, x, y + s / 2, x + s * 3 / 4, y + s);
        break;
    case '1':
        SDL_RenderDrawLine(r, x + s / 2, y, x + s / 2, y + s);
        break;
    case '2':
        SDL_RenderDrawLine(r, x, y, x + s, y);
        SDL_RenderDrawLine(r, x + s, y, x, y + s);
        SDL_RenderDrawLine(r, x, y + s, x + s, y + s);
        break;
    default: {
        SDL_Rect rc = {x + 1, y + 1, s - 2, s - 2};
        SDL_RenderDrawRect(r, &rc);
        break;
    }
    }
}

void port_touch_draw(void *sdl_renderer) {
    SDL_Renderer *r = (SDL_Renderer *)sdl_renderer;
    tp_ensure_init();
    if (!tp_enabled || !r) return;

    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(r, &ow, &oh);
    if (ow <= 0 || oh <= 0) return;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    /* Top-center quick toggle pill */
    int pill_w = ow / 14;
    if (pill_w < 54) pill_w = 54;
    int pill_h = oh / 18;
    if (pill_h < 22) pill_h = 22;
    draw_box(r, ow / 2 - pill_w / 2, 6, pill_w, pill_h, 0, 229, 195, !tp_collapsed);
    if (tp_collapsed) return;

    /* D-pad cross on left */
    int dcx = (int)(0.13f * (float)ow);
    int dcy = (int)(0.72f * (float)oh);
    int arm = oh / 11;
    if (arm < 34) arm = 34;
    int th = arm * 7 / 10;
    draw_box(r, dcx - th / 2, dcy - arm - th / 2, th, arm, 0, 229, 195, (tp_bits & PAD_UP) != 0);
    draw_box(r, dcx - th / 2, dcy + th / 2, th, arm, 0, 229, 195, (tp_bits & PAD_DOWN) != 0);
    draw_box(r, dcx - arm - th / 2, dcy - th / 2, arm, th, 0, 229, 195, (tp_bits & PAD_LEFT) != 0);
    draw_box(r, dcx + th / 2, dcy - th / 2, arm, th, 0, 229, 195, (tp_bits & PAD_RIGHT) != 0);

    /* Face, shoulder, and start/select buttons */
    for (int b = 0; b < N_TP_BTNS; b++) {
        int cx = (int)(tp_buttons[b].cx * (float)ow);
        int cy = (int)(tp_buttons[b].cy * (float)oh);
        int sz = (int)(tp_buttons[b].r * (float)oh * 1.45f);
        if (sz < 28) sz = 28;
        int w = (tp_buttons[b].bit == PAD_SELECT || tp_buttons[b].bit == PAD_START) ? sz * 2 : sz;
        int h = (tp_buttons[b].bit == PAD_SELECT || tp_buttons[b].bit == PAD_START) ? sz * 3 / 4 : sz;
        int lit = (tp_bits & tp_buttons[b].bit) != 0;
        draw_box(r, cx - w / 2, cy - h / 2, w, h, tp_buttons[b].cr, tp_buttons[b].cg, tp_buttons[b].cb, lit);

        SDL_SetRenderDrawColor(r, 240, 248, 255, lit ? 255 : 200);
        int gs = h / 2;
        if (gs < 10) gs = 10;
        if (tp_buttons[b].label[0] == '/' && tp_buttons[b].label[1] == '\\') {
            draw_tiny_char(r, cx - gs / 2, cy - gs / 2, gs, '^');
        } else if (tp_buttons[b].label[0] == '[' && tp_buttons[b].label[1] == ']') {
            draw_tiny_char(r, cx - gs / 2, cy - gs / 2, gs, '[');
        } else if (tp_buttons[b].label[0] == 'L' || tp_buttons[b].label[0] == 'R') {
            draw_tiny_char(r, cx - gs, cy - gs / 2, gs, tp_buttons[b].label[0]);
            draw_tiny_char(r, cx + 2, cy - gs / 2, gs, tp_buttons[b].label[1]);
        } else if (tp_buttons[b].label[0] == 'X' || tp_buttons[b].label[0] == 'O') {
            draw_tiny_char(r, cx - gs / 2, cy - gs / 2, gs, tp_buttons[b].label[0]);
        }
    }
}
