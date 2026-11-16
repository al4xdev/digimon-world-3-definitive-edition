/* Ergonomic, fluid multi-touch virtual PlayStation gamepad overlay for Android.
 *
 * Layout:
 *   When the device is wider than 16:9 (e.g. 20:9 / 19.5:9 / 21:9 phones), all controls live
 *   in the left/right side margins OUTSIDE the 16:9 game area so zero gameplay pixels are covered,
 *   and anchors stay fixed even on 4:3 menu/video screens so thumb muscle memory never shifts.
 *   On narrower 16:9 displays, controls tuck cleanly onto the outer screen edges with subtle alpha.
 *
 * Fluidity:
 *   - Physical-pixel hitboxes (zero aspect-ratio distortion on wide screens).
 *   - Per-finger role locking: D-pad captures a finger down to release (no dropped slides)
 *     with circular deadzone, angular 8-way sectors, and live glowing thumb puck feedback.
 *   - Face diamond uses closest-button Voronoi selection + smooth thumb-slide switching.
 *   - Fast-tap latching in psxstack/runtime/input.c ensures brief taps never drop between frames.
 *   - Renders anti-aliased BGRA8888 pixels only when dirty (0 CPU/GPU upload cost on idle frames). */
#include "touch_pad.h"

#include <SDL3/SDL.h>
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

enum {
    ROLE_NONE = 0,
    ROLE_DPAD,
    ROLE_FACE,
    ROLE_PILL,
    ROLE_TOGGLE
};

enum {
    BTN_CROSS = 0,
    BTN_CIRCLE,
    BTN_SQUARE,
    BTN_TRIANGLE,
    BTN_L1,
    BTN_L2,
    BTN_SELECT,
    BTN_R1,
    BTN_R2,
    BTN_START,
    N_BTNS
};

typedef struct {
    int active;
    SDL_FingerID id;
    int role;
    int locked_btn;
    float x, y; /* normalized [0..1] */
} TouchFinger;

typedef struct {
    uint16_t bit;
    float cx, cy;
    float hw, hh; /* half-width, half-height (for face: hw == hh == radius) */
    int is_circle;
    uint8_t cr, cg, cb;
    const char *label;
} TpBtn;

typedef struct {
    int w, h;
    int left_zone, right_zone;
    int in_bezel;
    float dpad_cx, dpad_cy, dpad_r;
    float face_cx, face_cy, face_sp, face_r;
    TpBtn btns[N_BTNS];
    float tog_cx, tog_cy, tog_hw, tog_hh;
} TpLayout;

static int tp_enabled = -1;
static int tp_collapsed = 0;
static TouchFinger tp_fingers[MAX_TOUCH_FINGERS];
static uint16_t tp_bits;
static int tp_dpad_held;
static float tp_thumb_dx, tp_thumb_dy; /* pixel offset inside D-pad ring */

static int tp_dirty = 1;
static int tp_last_w, tp_last_h;
static int tp_last_rect[4] = {-1, -1, -1, -1};
static int tp_view_w = 1280, tp_view_h = 576;

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
        tp_dpad_held = 0;
        tp_thumb_dx = tp_thumb_dy = 0.0f;
        port_input_set_touch_bits(0);
    }
    tp_dirty = 1;
}

int dw3_touch_pad_enabled(void) {
    tp_ensure_init();
    return tp_enabled;
}

static void tp_compute_layout(int w, int h, TpLayout *L) {
    if (w <= 0) w = 1280;
    if (h <= 0) h = 576;
    memset(L, 0, sizeof(*L));
    L->w = w;
    L->h = h;

    /* Anchor relative to the 16:9 safe box so controls never jump when screens switch to 4:3. */
    int w16_9 = (int)((long long)h * 16 / 9);
    int bezel = (w - w16_9) / 2;
    int min_zone = (int)(h * 0.21f);
    if (bezel < 0) bezel = 0;

    L->in_bezel = bezel >= (int)(h * 0.16f);
    /* Use the real bezel when there is one: forcing min_zone on 18:9-19.5:9 pushed controls onto the image. */
    L->left_zone = L->in_bezel ? bezel : min_zone;
    L->right_zone = L->left_zone;
    if (L->left_zone > w * 40 / 100) {
        L->left_zone = L->right_zone = w * 40 / 100;
    }

    float lz = (float)L->left_zone;
    float rz = (float)L->right_zone;
    float fh = (float)h;
    float fw = (float)w;

    float lcx = lz * 0.50f;
    float rcx = fw - rz * 0.50f;

    /* D-pad (left bottom) */
    L->dpad_cx = lcx;
    L->dpad_cy = fh * 0.70f;
    L->dpad_r = lz * 0.43f;
    if (L->dpad_r > fh * 0.22f) L->dpad_r = fh * 0.22f;
    if (L->dpad_r < 24.0f) L->dpad_r = 24.0f;

    /* Face diamond (right bottom) */
    L->face_cx = rcx;
    L->face_cy = fh * 0.70f;
    L->face_sp = rz * 0.305f;
    if (L->face_sp > fh * 0.145f) L->face_sp = fh * 0.145f;
    L->face_r = rz * 0.165f;
    if (L->face_r > fh * 0.078f) L->face_r = fh * 0.078f;
    if (L->face_r < 14.0f) L->face_r = 14.0f;

    L->btns[BTN_CROSS] = (TpBtn){PAD_CROSS, L->face_cx, L->face_cy + L->face_sp, L->face_r, L->face_r, 1, 90, 175, 255, "X"};
    L->btns[BTN_CIRCLE] = (TpBtn){PAD_CIRCLE, L->face_cx + L->face_sp, L->face_cy, L->face_r, L->face_r, 1, 255, 100, 105, "O"};
    L->btns[BTN_SQUARE] = (TpBtn){PAD_SQUARE, L->face_cx - L->face_sp, L->face_cy, L->face_r, L->face_r, 1, 225, 125, 255, "[]"};
    L->btns[BTN_TRIANGLE] = (TpBtn){PAD_TRIANGLE, L->face_cx, L->face_cy - L->face_sp, L->face_r, L->face_r, 1, 70, 235, 165, "/\\"};

    /* Left pills: L1, L2, SELECT */
    float pw = lz * 0.39f;
    float ph = fh * 0.052f;
    if (ph < 14.0f) ph = 14.0f;
    L->btns[BTN_L1] = (TpBtn){PAD_L1, lcx, fh * 0.115f, pw, ph, 0, 0, 220, 195, "L1"};
    L->btns[BTN_L2] = (TpBtn){PAD_L2, lcx, fh * 0.240f, pw * 0.90f, ph * 0.90f, 0, 0, 185, 175, "L2"};
    L->btns[BTN_SELECT] = (TpBtn){PAD_SELECT, lcx, fh * 0.365f, pw * 0.84f, ph * 0.82f, 0, 170, 195, 220, "SEL"};

    /* Right pills: R1, R2, START */
    L->btns[BTN_R1] = (TpBtn){PAD_R1, rcx, fh * 0.115f, pw, ph, 0, 0, 220, 195, "R1"};
    L->btns[BTN_R2] = (TpBtn){PAD_R2, rcx, fh * 0.240f, pw * 0.90f, ph * 0.90f, 0, 0, 185, 175, "R2"};
    L->btns[BTN_START] = (TpBtn){PAD_START, rcx, fh * 0.365f, pw * 0.84f, ph * 0.82f, 0, 170, 195, 220, "STA"};

    /* Top-center hide/show toggle pill */
    L->tog_cx = fw * 0.50f;
    L->tog_cy = fh * 0.032f;
    if (L->tog_cy < 12.0f) L->tog_cy = 12.0f;
    L->tog_hw = fh * 0.058f;
    L->tog_hh = fh * 0.022f;
    if (L->tog_hw < 26.0f) L->tog_hw = 26.0f;
    if (L->tog_hh < 9.0f) L->tog_hh = 9.0f;
}

static int tp_hit_pill(const TpLayout *L, int idx, float px, float py, float pad) {
    float dx = fabsf(px - L->btns[idx].cx);
    float dy = fabsf(py - L->btns[idx].cy);
    return dx <= L->btns[idx].hw + pad && dy <= L->btns[idx].hh + pad;
}

static int tp_closest_face(const TpLayout *L, float px, float py, float max_dist) {
    int best = -1;
    float best_d2 = max_dist * max_dist;
    for (int b = BTN_CROSS; b <= BTN_TRIANGLE; b++) {
        float dx = px - L->btns[b].cx;
        float dy = py - L->btns[b].cy;
        float d2 = dx * dx + dy * dy;
        if (d2 <= best_d2) {
            best_d2 = d2;
            best = b;
        }
    }
    return best;
}

static void tp_recompute(void) {
    uint16_t bits = 0;
    int dpad_held = 0;
    float tdx = 0.0f, tdy = 0.0f;

    if (!tp_enabled || tp_collapsed) {
        if (tp_bits != 0 || tp_dpad_held) {
            tp_dirty = 1;
        }
        tp_bits = 0;
        tp_dpad_held = 0;
        tp_thumb_dx = tp_thumb_dy = 0.0f;
        port_input_set_touch_bits(0);
        return;
    }

    TpLayout L;
    tp_compute_layout(tp_view_w, tp_view_h, &L);

    for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
        TouchFinger *f = &tp_fingers[i];
        if (!f->active) continue;
        float px = f->x * (float)L.w;
        float py = f->y * (float)L.h;

        if (f->role == ROLE_DPAD) {
            float dx = px - L.dpad_cx;
            float dy = py - L.dpad_cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float dead = L.dpad_r * 0.16f;
            float max_puck = L.dpad_r * 0.66f;

            dpad_held = 1;
            if (dist > 0.001f) {
                float clamp = dist > max_puck ? max_puck / dist : 1.0f;
                tdx = dx * clamp;
                tdy = dy * clamp;
            }
            if (dist >= dead) {
                /* 8-way angular sectors: ~49 deg pure cardinal, ~41 deg diagonal overlap. */
                float thresh = dist * 0.42f;
                if (dx < -thresh) bits |= PAD_LEFT;
                if (dx > thresh) bits |= PAD_RIGHT;
                if (dy < -thresh) bits |= PAD_UP;
                if (dy > thresh) bits |= PAD_DOWN;
            }
        } else if (f->role == ROLE_FACE) {
            int b = tp_closest_face(&L, px, py, L.face_sp * 2.1f);
            f->locked_btn = b;
            if (b >= 0) {
                bits |= L.btns[b].bit;
            }
        } else if (f->role == ROLE_PILL) {
            int b = f->locked_btn;
            if (b >= BTN_L1 && b < N_BTNS && tp_hit_pill(&L, b, px, py, L.btns[b].hh * 1.4f)) {
                bits |= L.btns[b].bit;
            }
        }
    }

    if (bits != tp_bits || dpad_held != tp_dpad_held ||
        fabsf(tdx - tp_thumb_dx) > 1.0f || fabsf(tdy - tp_thumb_dy) > 1.0f) {
        tp_dirty = 1;
    }
    tp_bits = bits;
    tp_dpad_held = dpad_held;
    tp_thumb_dx = tdx;
    tp_thumb_dy = tdy;
    port_input_set_touch_bits(bits);
}

void port_touch_event(const void *sdl_event) {
    const SDL_Event *e = (const SDL_Event *)sdl_event;
    tp_ensure_init();
    if (!tp_enabled) return;

    if (e->type == SDL_EVENT_WINDOW_FOCUS_LOST || e->type == SDL_EVENT_WILL_ENTER_BACKGROUND) {
        memset(tp_fingers, 0, sizeof(tp_fingers));
        tp_recompute();
        return;
    }

    if (e->type != SDL_EVENT_FINGER_DOWN && e->type != SDL_EVENT_FINGER_MOTION &&
        e->type != SDL_EVENT_FINGER_UP && e->type != SDL_EVENT_FINGER_CANCELED) {
        return;
    }

    float fx = e->tfinger.x;
    float fy = e->tfinger.y;
    SDL_FingerID fid = e->tfinger.fingerID;

    TpLayout L;
    tp_compute_layout(tp_view_w, tp_view_h, &L);
    float px = fx * (float)L.w;
    float py = fy * (float)L.h;

    if (e->type == SDL_EVENT_FINGER_DOWN) {
        int slot = -1;
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (tp_fingers[i].active && tp_fingers[i].id == fid) {
                slot = i;
                break;
            }
            if (!tp_fingers[i].active && slot < 0) {
                slot = i;
            }
        }
        if (slot < 0) return;

        TouchFinger *f = &tp_fingers[slot];
        memset(f, 0, sizeof(*f));
        f->active = 1;
        f->id = fid;
        f->x = fx;
        f->y = fy;
        f->locked_btn = -1;

        /* Top-center toggle pill */
        if (fabsf(px - L.tog_cx) <= L.tog_hw + 6.0f && py <= L.tog_cy + L.tog_hh + 8.0f) {
            f->role = ROLE_TOGGLE;
            tp_recompute();
            return;
        }
        if (tp_collapsed) {
            f->role = ROLE_NONE;
            return;
        }

        /* Left D-pad capture */
        float ddx = px - L.dpad_cx;
        float ddy = py - L.dpad_cy;
        if (ddx * ddx + ddy * ddy <= (L.dpad_r * 1.45f) * (L.dpad_r * 1.45f) ||
            (px <= (float)L.left_zone * 1.15f && py >= (float)L.h * 0.45f)) {
            f->role = ROLE_DPAD;
            tp_recompute();
            return;
        }

        /* Shoulders / Select / Start pills */
        for (int b = BTN_L1; b < N_BTNS; b++) {
            if (tp_hit_pill(&L, b, px, py, 10.0f)) {
                f->role = ROLE_PILL;
                f->locked_btn = b;
                tp_recompute();
                return;
            }
        }

        /* Face diamond (closest button inside capture radius) */
        int fb = tp_closest_face(&L, px, py, L.face_r * 1.65f);
        if (fb < 0 && px >= (float)L.w - (float)L.right_zone * 1.15f && py >= (float)L.h * 0.45f) {
            fb = tp_closest_face(&L, px, py, L.face_sp * 2.0f);
        }
        if (fb >= 0) {
            f->role = ROLE_FACE;
            f->locked_btn = fb;
        } else {
            f->role = ROLE_NONE;
        }
    } else if (e->type == SDL_EVENT_FINGER_MOTION) {
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            if (tp_fingers[i].active && tp_fingers[i].id == fid) {
                tp_fingers[i].x = fx;
                tp_fingers[i].y = fy;
                break;
            }
        }
    } else { /* SDL_EVENT_FINGER_UP or SDL_EVENT_FINGER_CANCELED */
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            TouchFinger *f = &tp_fingers[i];
            if (f->active && f->id == fid) {
                if (e->type == SDL_EVENT_FINGER_UP && f->role == ROLE_TOGGLE &&
                    fabsf(px - L.tog_cx) <= L.tog_hw + 22.0f && py <= L.tog_cy + L.tog_hh + 26.0f) {
                    tp_collapsed = !tp_collapsed;
                    tp_dirty = 1;
                }
                f->active = 0;
                f->role = ROLE_NONE;
                f->locked_btn = -1;
                break;
            }
        }
    }
    tp_recompute();
}

/* ---- Anti-aliased BGRA8888 software drawing helpers ---- */

static inline void blend_px(uint32_t *buf, int w, int h, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h || a == 0) return;
    uint32_t *dst = &buf[(size_t)y * (size_t)w + (size_t)x];
    if (a == 255) {
        *dst = ((uint32_t)255u << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        return;
    }
    uint32_t d = *dst;
    uint32_t da = (d >> 24) & 255u;
    if (da == 0) {
        *dst = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        return;
    }
    uint32_t inv = 255u - a;
    uint32_t nr = (r * a + ((d >> 16) & 255u) * inv) / 255u;
    uint32_t ng = (g * a + ((d >> 8) & 255u) * inv) / 255u;
    uint32_t nb = (b * a + (d & 255u) * inv) / 255u;
    uint32_t na = a + (da * inv) / 255u;
    *dst = (na << 24) | (nr << 16) | (ng << 8) | nb;
}

static void aa_circle_panel(uint32_t *buf, int w, int h, float cx, float cy, float rad,
                            uint8_t cr, uint8_t cg, uint8_t cb, int lit, float alpha_scale) {
    int x0 = (int)floorf(cx - rad - 2.0f), x1 = (int)ceilf(cx + rad + 2.0f);
    int y0 = (int)floorf(cy - rad - 2.0f), y1 = (int)ceilf(cy + rad + 2.0f);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= w) x1 = w - 1;
    if (y1 >= h) y1 = h - 1;

    float fill_a = (lit ? 185.0f : 92.0f) * alpha_scale;
    float edge_a = (lit ? 248.0f : 185.0f) * alpha_scale;
    uint8_t fr = (uint8_t)(lit ? cr / 2 : cr / 5);
    uint8_t fg = (uint8_t)(lit ? cg / 2 : cg / 5);
    uint8_t fb = (uint8_t)(lit ? cb / 2 : cb / 5);

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx;
            float dy = (float)y + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy);
            float cov_outer = rad + 0.75f - d;
            if (cov_outer <= 0.0f) continue;
            if (cov_outer > 1.0f) cov_outer = 1.0f;

            float rim = d - (rad - 1.8f);
            if (rim > 0.0f) {
                float mix_t = rim / 1.8f;
                if (mix_t > 1.0f) mix_t = 1.0f;
                uint8_t rr = (uint8_t)(fr + (cr - fr) * mix_t);
                uint8_t rg = (uint8_t)(fg + (cg - fg) * mix_t);
                uint8_t rb = (uint8_t)(fb + (cb - fb) * mix_t);
                uint8_t ra = (uint8_t)((fill_a + (edge_a - fill_a) * mix_t) * cov_outer);
                blend_px(buf, w, h, x, y, rr, rg, rb, ra);
            } else {
                blend_px(buf, w, h, x, y, fr, fg, fb, (uint8_t)(fill_a * cov_outer));
            }
        }
    }
}

static void aa_rrect_panel(uint32_t *buf, int w, int h, float cx, float cy, float hw, float hh, float rr,
                           uint8_t cr, uint8_t cg, uint8_t cb, int lit, float alpha_scale) {
    if (rr > hw) rr = hw;
    if (rr > hh) rr = hh;
    int x0 = (int)floorf(cx - hw - 2.0f), x1 = (int)ceilf(cx + hw + 2.0f);
    int y0 = (int)floorf(cy - hh - 2.0f), y1 = (int)ceilf(cy + hh + 2.0f);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= w) x1 = w - 1;
    if (y1 >= h) y1 = h - 1;

    float fill_a = (lit ? 185.0f : 92.0f) * alpha_scale;
    float edge_a = (lit ? 248.0f : 185.0f) * alpha_scale;
    uint8_t fr = (uint8_t)(lit ? cr / 2 : cr / 5);
    uint8_t fg = (uint8_t)(lit ? cg / 2 : cg / 5);
    uint8_t fb = (uint8_t)(lit ? cb / 2 : cb / 5);
    float ihw = hw - rr;
    float ihh = hh - rr;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float qx = fabsf((float)x + 0.5f - cx) - ihw;
            float qy = fabsf((float)y + 0.5f - cy) - ihh;
            float ox = qx > 0.0f ? qx : 0.0f;
            float oy = qy > 0.0f ? qy : 0.0f;
            float in = qx > qy ? qx : qy;
            if (in > 0.0f) in = 0.0f;
            float sdf = sqrtf(ox * ox + oy * oy) + in - rr;
            float cov = 0.75f - sdf;
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;

            if (sdf > -1.7f) {
                float mix_t = (sdf + 1.7f) / 1.7f;
                if (mix_t > 1.0f) mix_t = 1.0f;
                uint8_t mr = (uint8_t)(fr + (cr - fr) * mix_t);
                uint8_t mg = (uint8_t)(fg + (cg - fg) * mix_t);
                uint8_t mb = (uint8_t)(fb + (cb - fb) * mix_t);
                uint8_t ma = (uint8_t)((fill_a + (edge_a - fill_a) * mix_t) * cov);
                blend_px(buf, w, h, x, y, mr, mg, mb, ma);
            } else {
                blend_px(buf, w, h, x, y, fr, fg, fb, (uint8_t)(fill_a * cov));
            }
        }
    }
}

static void aa_line(uint32_t *buf, int w, int h, float x0, float y0, float x1, float y1, float thick,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    float minx = (x0 < x1 ? x0 : x1) - thick - 1.5f;
    float maxx = (x0 > x1 ? x0 : x1) + thick + 1.5f;
    float miny = (y0 < y1 ? y0 : y1) - thick - 1.5f;
    float maxy = (y0 > y1 ? y0 : y1) + thick + 1.5f;
    int ix0 = (int)floorf(minx), ix1 = (int)ceilf(maxx);
    int iy0 = (int)floorf(miny), iy1 = (int)ceilf(maxy);
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 >= w) ix1 = w - 1;
    if (iy1 >= h) iy1 = h - 1;

    float vx = x1 - x0, vy = y1 - y0;
    float len2 = vx * vx + vy * vy;
    float half = thick * 0.5f;

    for (int y = iy0; y <= iy1; y++) {
        for (int x = ix0; x <= ix1; x++) {
            float px = (float)x + 0.5f - x0;
            float py = (float)y + 0.5f - y0;
            float t = len2 > 0.001f ? (px * vx + py * vy) / len2 : 0.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float dx = px - vx * t, dy = py - vy * t;
            float d = sqrtf(dx * dx + dy * dy);
            float cov = half + 0.7f - d;
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;
            blend_px(buf, w, h, x, y, r, g, b, (uint8_t)((float)a * cov));
        }
    }
}

static void draw_glyph(uint32_t *buf, int w, int h, float cx, float cy, float s, char ch,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    float hs = s * 0.5f;
    float th = s * 0.16f;
    if (th < 1.5f) th = 1.5f;
    switch (ch) {
    case 'X':
        aa_line(buf, w, h, cx - hs, cy - hs, cx + hs, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs, cy - hs, cx - hs, cy + hs, th, r, g, b, a);
        break;
    case 'O': {
        int x0 = (int)floorf(cx - hs - 2.0f), x1 = (int)ceilf(cx + hs + 2.0f);
        int y0 = (int)floorf(cy - hs - 2.0f), y1 = (int)ceilf(cy + hs + 2.0f);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= w) x1 = w - 1;
        if (y1 >= h) y1 = h - 1;
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                float d = fabsf(sqrtf(((float)x + 0.5f - cx) * ((float)x + 0.5f - cx) +
                                      ((float)y + 0.5f - cy) * ((float)y + 0.5f - cy)) - hs);
                float cov = th * 0.6f + 0.7f - d;
                if (cov > 0.0f) {
                    if (cov > 1.0f) cov = 1.0f;
                    blend_px(buf, w, h, x, y, r, g, b, (uint8_t)((float)a * cov));
                }
            }
        }
        break;
    }
    case '[': /* square */
        aa_line(buf, w, h, cx - hs, cy - hs, cx + hs, cy - hs, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs, cy - hs, cx + hs, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs, cy + hs, cx - hs, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs, cy + hs, cx - hs, cy - hs, th, r, g, b, a);
        break;
    case '^': /* triangle */
        aa_line(buf, w, h, cx, cy - hs, cx - hs, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx, cy - hs, cx + hs, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs, cy + hs, cx + hs, cy + hs, th, r, g, b, a);
        break;
    case 'L':
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx - hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy + hs, cx + hs * 0.7f, cy + hs, th, r, g, b, a);
        break;
    case 'R':
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx - hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx + hs * 0.6f, cy - hs * 0.5f, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.6f, cy - hs * 0.5f, cx - hs * 0.6f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.4f, cy, cx + hs * 0.6f, cy + hs, th, r, g, b, a);
        break;
    case '1':
        aa_line(buf, w, h, cx, cy - hs, cx, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.4f, cy - hs * 0.6f, cx, cy - hs, th, r, g, b, a);
        break;
    case '2':
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx + hs * 0.6f, cy - hs, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.6f, cy - hs, cx + hs * 0.6f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.6f, cy, cx - hs * 0.6f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy, cx - hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy + hs, cx + hs * 0.6f, cy + hs, th, r, g, b, a);
        break;
    case 'S':
        aa_line(buf, w, h, cx + hs * 0.6f, cy - hs, cx - hs * 0.6f, cy - hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx - hs * 0.6f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy, cx + hs * 0.6f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.6f, cy, cx + hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.6f, cy + hs, cx - hs * 0.6f, cy + hs, th, r, g, b, a);
        break;
    case 'E':
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx - hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx + hs * 0.6f, cy - hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy, cx + hs * 0.4f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.6f, cy + hs, cx + hs * 0.6f, cy + hs, th, r, g, b, a);
        break;
    case 'T':
        aa_line(buf, w, h, cx - hs * 0.7f, cy - hs, cx + hs * 0.7f, cy - hs, th, r, g, b, a);
        aa_line(buf, w, h, cx, cy - hs, cx, cy + hs, th, r, g, b, a);
        break;
    case 'A':
        aa_line(buf, w, h, cx, cy - hs, cx - hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx, cy - hs, cx + hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.35f, cy + hs * 0.25f, cx + hs * 0.35f, cy + hs * 0.25f, th, r, g, b, a);
        break;
    }
}

int port_touch_draw_rgba(uint32_t *rgba, int w, int h, const int game_rect[4]) {
    tp_ensure_init();
    if (!tp_enabled || w <= 0 || h <= 0) {
        return -1;
    }
    tp_view_w = w;
    tp_view_h = h;
    if (rgba == NULL) {
        return 0; /* probe: touch overlay enabled */
    }
    if (!tp_dirty && w == tp_last_w && h == tp_last_h &&
        game_rect[0] == tp_last_rect[0] && game_rect[1] == tp_last_rect[1] &&
        game_rect[2] == tp_last_rect[2] && game_rect[3] == tp_last_rect[3]) {
        return 0;
    }

    tp_dirty = 0;
    tp_last_w = w;
    tp_last_h = h;
    memcpy(tp_last_rect, game_rect, sizeof(tp_last_rect));
    memset(rgba, 0, (size_t)w * (size_t)h * sizeof(uint32_t));

    TpLayout L;
    tp_compute_layout(w, h, &L);
    float alpha_scale = L.in_bezel ? 1.0f : 0.72f;

    /* Top-center subtle collapse pill */
    aa_rrect_panel(rgba, w, h, L.tog_cx, L.tog_cy, L.tog_hw, L.tog_hh, L.tog_hh,
                   0, 215, 190, !tp_collapsed, 0.50f);
    if (tp_collapsed) {
        aa_line(rgba, w, h, L.tog_cx - L.tog_hw * 0.35f, L.tog_cy, L.tog_cx + L.tog_hw * 0.35f, L.tog_cy,
                2.0f, 220, 245, 240, 175);
        return 1;
    }
    aa_line(rgba, w, h, L.tog_cx - L.tog_hw * 0.3f, L.tog_cy - 2.0f, L.tog_cx, L.tog_cy + 2.5f,
            1.8f, 220, 245, 240, 190);
    aa_line(rgba, w, h, L.tog_cx, L.tog_cy + 2.5f, L.tog_cx + L.tog_hw * 0.3f, L.tog_cy - 2.0f,
            1.8f, 220, 245, 240, 190);

    /* D-pad base ring + cross arms + glowing thumb puck */
    aa_circle_panel(rgba, w, h, L.dpad_cx, L.dpad_cy, L.dpad_r, 0, 215, 195, tp_dpad_held, alpha_scale * 0.85f);
    {
        float arm_dist = L.dpad_r * 0.56f;
        float aw = L.dpad_r * 0.24f;
        float ah = L.dpad_r * 0.30f;
        aa_rrect_panel(rgba, w, h, L.dpad_cx, L.dpad_cy - arm_dist, aw, ah, 4.0f,
                       0, 235, 205, (tp_bits & PAD_UP) != 0, alpha_scale);
        aa_rrect_panel(rgba, w, h, L.dpad_cx, L.dpad_cy + arm_dist, aw, ah, 4.0f,
                       0, 235, 205, (tp_bits & PAD_DOWN) != 0, alpha_scale);
        aa_rrect_panel(rgba, w, h, L.dpad_cx - arm_dist, L.dpad_cy, ah, aw, 4.0f,
                       0, 235, 205, (tp_bits & PAD_LEFT) != 0, alpha_scale);
        aa_rrect_panel(rgba, w, h, L.dpad_cx + arm_dist, L.dpad_cy, ah, aw, 4.0f,
                       0, 235, 205, (tp_bits & PAD_RIGHT) != 0, alpha_scale);
        aa_circle_panel(rgba, w, h, L.dpad_cx + tp_thumb_dx, L.dpad_cy + tp_thumb_dy, L.dpad_r * 0.26f,
                        180, 250, 240, tp_dpad_held, alpha_scale);
    }

    /* Face buttons, shoulders, Select/Start */
    for (int b = 0; b < N_BTNS; b++) {
        int lit = (tp_bits & L.btns[b].bit) != 0;
        if (L.btns[b].is_circle) {
            aa_circle_panel(rgba, w, h, L.btns[b].cx, L.btns[b].cy, L.btns[b].hw,
                            L.btns[b].cr, L.btns[b].cg, L.btns[b].cb, lit, alpha_scale);
        } else {
            aa_rrect_panel(rgba, w, h, L.btns[b].cx, L.btns[b].cy, L.btns[b].hw, L.btns[b].hh, L.btns[b].hh * 0.85f,
                           L.btns[b].cr, L.btns[b].cg, L.btns[b].cb, lit, alpha_scale);
        }
        uint8_t la = (uint8_t)((lit ? 255.0f : 215.0f) * alpha_scale);
        const char *lbl = L.btns[b].label;
        float gs = (L.btns[b].is_circle ? L.btns[b].hw : L.btns[b].hh) * 0.92f;
        if (strcmp(lbl, "/\\") == 0) {
            draw_glyph(rgba, w, h, L.btns[b].cx, L.btns[b].cy, gs, '^', 240, 250, 255, la);
        } else if (strcmp(lbl, "[]") == 0) {
            draw_glyph(rgba, w, h, L.btns[b].cx, L.btns[b].cy, gs, '[', 240, 250, 255, la);
        } else if (strlen(lbl) == 1) {
            draw_glyph(rgba, w, h, L.btns[b].cx, L.btns[b].cy, gs, lbl[0], 240, 250, 255, la);
        } else if (strlen(lbl) == 2) {
            draw_glyph(rgba, w, h, L.btns[b].cx - gs * 0.55f, L.btns[b].cy, gs * 0.9f, lbl[0], 240, 250, 255, la);
            draw_glyph(rgba, w, h, L.btns[b].cx + gs * 0.55f, L.btns[b].cy, gs * 0.9f, lbl[1], 240, 250, 255, la);
        } else if (strlen(lbl) == 3) {
            draw_glyph(rgba, w, h, L.btns[b].cx - gs * 1.0f, L.btns[b].cy, gs * 0.8f, lbl[0], 240, 250, 255, la);
            draw_glyph(rgba, w, h, L.btns[b].cx, L.btns[b].cy, gs * 0.8f, lbl[1], 240, 250, 255, la);
            draw_glyph(rgba, w, h, L.btns[b].cx + gs * 1.0f, L.btns[b].cy, gs * 0.8f, lbl[2], 240, 250, 255, la);
        }
    }

    return 1;
}
