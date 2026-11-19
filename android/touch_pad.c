/* Ergonomic, fluid multi-touch virtual PlayStation gamepad overlay for Android.
 *
 * Layout:
 *   Every control is sized from the screen's height (a thumb is the same size on any phone), so the D-pad and the face
 *   diamond reach past a 20:9 phone's side margins onto the picture's edges, drawn translucent there; the shoulders and
 *   Select/Start stay in the margins. The first cut fitted everything inside the margins and came out too small for a
 *   thumb (owner, 2026-11-18). Two small buttons in the top corners: hide the pad (left) and edit it (right).
 *
 * Editing: the right corner button opens the layout editor over the running game (the pad sends nothing while it is
 *   open). Each group (D-pad, face buttons, L1/L2/SELECT, R1/R2/START) is dragged to its place and sized with the
 *   - and + buttons in the middle; RESET puts the defaults back, OK leaves. The layout is kept in touch_layout.cfg in
 *   the app's files directory, as offsets in screen heights, so it survives a rotation or a resolution change.
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
#include <stdio.h>
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
#define TP_LAYOUT_FILE "touch_layout.cfg"

enum {
    ROLE_NONE = 0,
    ROLE_DPAD,
    ROLE_FACE,
    ROLE_PILL,
    ROLE_TOGGLE,
    ROLE_EDIT,     /* the corner button that opens the editor */
    ROLE_DRAG,     /* editor: a finger moving a group */
    ROLE_EDIT_UI   /* editor: a finger on one of its buttons */
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

/* The groups the editor moves and sizes as one. */
enum { G_DPAD = 0, G_FACE, G_LEFT, G_RIGHT, N_GROUPS };

/* The editor's buttons, in the middle of the screen. */
enum { UI_SMALLER = 0, UI_BIGGER, UI_RESET, UI_OK, N_UI };

#define TP_SCALE_MIN 0.6f
#define TP_SCALE_MAX 1.8f
#define TP_SCALE_STEP 0.1f

typedef struct {
    int active;
    SDL_FingerID id;
    int role;
    int locked_btn;
    float x, y; /* normalized [0..1] */
    float sx, sy; /* editor: where the drag started, in pixels */
    float sox, soy; /* editor: the group's offset then */
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
    float ox, oy; /* offset from the default place, in screen heights */
    float s;      /* size, 1 = the default */
} TpGroup;

typedef struct {
    float cx, cy, hw, hh;
    const char *label;
} TpUiBtn;

typedef struct {
    int w, h;
    float dpad_cx, dpad_cy, dpad_r;
    float face_cx, face_cy, face_sp, face_r;
    TpBtn btns[N_BTNS];
    float tog_cx, tog_cy, corner_r; /* hide button, top left */
    float edit_cx, edit_cy;         /* editor button, top right */
    TpUiBtn ui[N_UI];
} TpLayout;

static int tp_enabled = -1;
static int tp_collapsed = 0;
static int tp_editing = 0;
static int tp_sel_group = G_DPAD; /* the editor's group that - and + size */
static TpGroup tp_groups[N_GROUPS];
/* A gamepad being connected hides the pad (the picture is left clean); the next finger on the screen shows it
 * again, so a pad that SDL lists but the player does not use (some phones expose built-in sensors as one) never
 * locks the touch controls away. */
static int tp_pad_hidden;
static int tp_had_gamepad;
static TouchFinger tp_fingers[MAX_TOUCH_FINGERS];
static uint16_t tp_bits;
static int tp_dpad_held;
static float tp_thumb_dx, tp_thumb_dy; /* pixel offset inside D-pad ring */

static int tp_dirty = 1;
static int tp_last_w, tp_last_h;
static int tp_last_rect[4] = {-1, -1, -1, -1};
static int tp_view_w = 1280, tp_view_h = 576;

static void tp_groups_reset(void) {
    for (int g = 0; g < N_GROUPS; g++) {
        tp_groups[g].ox = tp_groups[g].oy = 0.0f;
        tp_groups[g].s = 1.0f;
    }
}

static float tp_clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static void tp_layout_load(void) {
    tp_groups_reset();
    FILE *f = fopen(TP_LAYOUT_FILE, "r");
    if (!f) return;
    int g;
    float ox, oy, s;
    while (fscanf(f, "%d %f %f %f", &g, &ox, &oy, &s) == 4) {
        if (g < 0 || g >= N_GROUPS) continue;
        tp_groups[g].ox = tp_clampf(ox, -4.0f, 4.0f);
        tp_groups[g].oy = tp_clampf(oy, -1.0f, 1.0f);
        tp_groups[g].s = tp_clampf(s, TP_SCALE_MIN, TP_SCALE_MAX);
    }
    fclose(f);
}

static void tp_layout_save(void) {
    FILE *f = fopen(TP_LAYOUT_FILE, "w");
    if (!f) return;
    for (int g = 0; g < N_GROUPS; g++) {
        fprintf(f, "%d %.4f %.4f %.3f\n", g, tp_groups[g].ox, tp_groups[g].oy, tp_groups[g].s);
    }
    fclose(f);
}

static void tp_ensure_init(void) {
    if (tp_enabled >= 0) return;
    const char *e = getenv("DW3_TOUCH_PAD");
    tp_enabled = (!e || e[0] != '0') ? 1 : 0;
    tp_layout_load();
}

void dw3_touch_pad_set_enabled(int enabled) {
    tp_ensure_init();
    tp_enabled = enabled ? 1 : 0;
    if (!tp_enabled) {
        memset(tp_fingers, 0, sizeof(tp_fingers));
        tp_bits = 0;
        tp_dpad_held = 0;
        tp_thumb_dx = tp_thumb_dy = 0.0f;
        tp_editing = 0;
        port_input_set_touch_bits(0);
    }
    tp_dirty = 1;
}

int dw3_touch_pad_enabled(void) {
    tp_ensure_init();
    return tp_enabled;
}

/* A group's point the size is taken about, and the box it covers (for the editor's grab and highlight). */
static void tp_group_box(const TpLayout *L, int g, float *cx, float *cy, float *hw, float *hh) {
    if (g == G_DPAD) {
        *cx = L->dpad_cx;
        *cy = L->dpad_cy;
        *hw = *hh = L->dpad_r;
        return;
    }
    if (g == G_FACE) {
        *cx = L->face_cx;
        *cy = L->face_cy;
        *hw = *hh = L->face_sp + L->face_r;
        return;
    }
    int b0 = g == G_LEFT ? BTN_L1 : BTN_R1;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int b = b0; b < b0 + 3; b++) {
        const TpBtn *t = &L->btns[b];
        x0 = fminf(x0, t->cx - t->hw);
        x1 = fmaxf(x1, t->cx + t->hw);
        y0 = fminf(y0, t->cy - t->hh);
        y1 = fmaxf(y1, t->cy + t->hh);
    }
    *cx = (x0 + x1) * 0.5f;
    *cy = (y0 + y1) * 0.5f;
    *hw = (x1 - x0) * 0.5f;
    *hh = (y1 - y0) * 0.5f;
}

/* A shoulder column (L1, L2, SELECT or R1, R2, START) at column centre cx, sized and moved by its group. */
static void tp_pill_column(TpLayout *L, int b0, float cx, int left, const TpGroup *G) {
    float fh = (float)L->h;
    float pw = fh * 0.105f;
    float ph = fh * 0.048f;
    static const float ys[3] = {0.19f, 0.315f, 0.44f};
    static const float sizes[3][2] = {{1.0f, 1.0f}, {0.92f, 0.92f}, {0.84f, 0.82f}};
    float ay = fh * ys[1]; /* the column's middle: the size is taken about it */
    for (int i = 0; i < 3; i++) {
        float y = ay + (fh * ys[i] - ay) * G->s;
        TpBtn *t = &L->btns[b0 + i];
        t->bit = left ? (i == 0 ? PAD_L1 : i == 1 ? PAD_L2 : PAD_SELECT) : (i == 0 ? PAD_R1 : i == 1 ? PAD_R2 : PAD_START);
        t->cx = cx + G->ox * fh;
        t->cy = y + G->oy * fh;
        t->hw = pw * sizes[i][0] * G->s;
        t->hh = ph * sizes[i][1] * G->s;
        t->is_circle = 0;
        t->cr = i == 2 ? 170 : 0;
        t->cg = i == 0 ? 220 : i == 1 ? 185 : 195;
        t->cb = i == 0 ? 195 : i == 1 ? 175 : 220;
        t->label = left ? (i == 0 ? "L1" : i == 1 ? "L2" : "SEL") : (i == 0 ? "R1" : i == 1 ? "R2" : "STA");
    }
}

static void tp_apply_faces(TpLayout *L);

static void tp_compute_layout(int w, int h, TpLayout *L) {
    if (w <= 0) w = 1280;
    if (h <= 0) h = 576;
    memset(L, 0, sizeof(*L));
    L->w = w;
    L->h = h;

    float fh = (float)h;
    float fw = (float)w;
    float m = fh * 0.035f; /* the margin from the screen's edges */
    const TpGroup *G;

    /* D-pad (left bottom) */
    G = &tp_groups[G_DPAD];
    L->dpad_r = fh * 0.19f * G->s;
    L->dpad_cx = m + fh * 0.19f + G->ox * fh;
    L->dpad_cy = fh - m - fh * 0.19f + G->oy * fh;

    /* Face diamond (right bottom) */
    G = &tp_groups[G_FACE];
    L->face_sp = fh * 0.155f * G->s;
    L->face_r = fh * 0.082f * G->s;
    L->face_cx = fw - m - fh * (0.155f + 0.082f) + G->ox * fh;
    L->face_cy = fh - m - fh * (0.155f + 0.082f) + G->oy * fh;

    L->btns[BTN_CROSS] = (TpBtn){PAD_CROSS, L->face_cx, L->face_cy + L->face_sp, L->face_r, L->face_r, 1, 90, 175, 255, "X"};
    L->btns[BTN_CIRCLE] = (TpBtn){PAD_CIRCLE, L->face_cx + L->face_sp, L->face_cy, L->face_r, L->face_r, 1, 255, 100, 105, "O"};
    L->btns[BTN_SQUARE] = (TpBtn){PAD_SQUARE, L->face_cx - L->face_sp, L->face_cy, L->face_r, L->face_r, 1, 225, 125, 255, "[]"};
    L->btns[BTN_TRIANGLE] = (TpBtn){PAD_TRIANGLE, L->face_cx, L->face_cy - L->face_sp, L->face_r, L->face_r, 1, 70, 235, 165, "/\\"};
    tp_apply_faces(L);

    /* Shoulders and Select/Start, a column on each side */
    float colx = m + fh * 0.105f;
    tp_pill_column(L, BTN_L1, colx, 1, &tp_groups[G_LEFT]);
    tp_pill_column(L, BTN_R1, fw - colx, 0, &tp_groups[G_RIGHT]);

    /* The corner buttons: hide (top left) and edit (top right) */
    L->corner_r = fh * 0.04f;
    if (L->corner_r < 16.0f) L->corner_r = 16.0f;
    L->tog_cx = m + L->corner_r;
    L->tog_cy = m * 0.6f + L->corner_r;
    L->edit_cx = fw - m - L->corner_r;
    L->edit_cy = L->tog_cy;

    /* The editor's buttons, a row in the middle of the screen */
    {
        static const char *labels[N_UI] = {"-", "+", "RESET", "OK"};
        static const float widths[N_UI] = {0.075f, 0.075f, 0.16f, 0.09f};
        float gap = fh * 0.03f, total = 0.0f;
        for (int i = 0; i < N_UI; i++) total += fh * widths[i] * 2.0f + (i ? gap : 0.0f);
        float x = fw * 0.5f - total * 0.5f;
        for (int i = 0; i < N_UI; i++) {
            float hw = fh * widths[i];
            L->ui[i] = (TpUiBtn){x + hw, fh * 0.5f, hw, fh * 0.06f, labels[i]};
            x += hw * 2.0f + gap;
        }
    }
}

static int tp_hit_pill(const TpLayout *L, int idx, float px, float py, float pad) {
    float dx = fabsf(px - L->btns[idx].cx);
    float dy = fabsf(py - L->btns[idx].cy);
    return dx <= L->btns[idx].hw + pad && dy <= L->btns[idx].hh + pad;
}

static int tp_hit_corner(const TpLayout *L, float cx, float cy, float px, float py, float pad) {
    float dx = px - cx, dy = py - cy, r = L->corner_r + pad;
    return dx * dx + dy * dy <= r * r;
}

static int tp_hit_ui(const TpLayout *L, float px, float py) {
    for (int i = 0; i < N_UI; i++) {
        if (fabsf(px - L->ui[i].cx) <= L->ui[i].hw + 6.0f && fabsf(py - L->ui[i].cy) <= L->ui[i].hh + 6.0f) {
            return i;
        }
    }
    return -1;
}

/* The editor's group under a finger: the nearest whose box (a little grown) holds it. */
static int tp_hit_group(const TpLayout *L, float px, float py) {
    int best = -1;
    float best_d = 1e18f;
    for (int g = 0; g < N_GROUPS; g++) {
        float cx, cy, hw, hh;
        tp_group_box(L, g, &cx, &cy, &hw, &hh);
        float pad = L->h * 0.03f;
        if (fabsf(px - cx) <= hw + pad && fabsf(py - cy) <= hh + pad) {
            float d = (px - cx) * (px - cx) + (py - cy) * (py - cy);
            if (d < best_d) {
                best_d = d;
                best = g;
            }
        }
    }
    return best;
}

/* The controller setting's face buttons (dw3_touch_pad_set_faces): each PS1 button moves to the spot of the button it
 * is bound to and shows that button's letter, so the pad reads like the gamepad the game's prompts describe ("press
 * X" is the left button, as on an Xbox pad). */
static int tp_face_place[4] = {TP_PLACE_BOTTOM, TP_PLACE_LEFT, TP_PLACE_RIGHT, TP_PLACE_TOP};
static char tp_face_letter[4] = {'-', '-', '-', '-'};
static char tp_face_label[4][2];

void dw3_touch_pad_set_faces(const int place[4], const char letter[4]) {
    int seen = 0;
    for (int i = 0; i < 4; i++) {
        if (place[i] < TP_PLACE_BOTTOM || place[i] > TP_PLACE_TOP || (seen & (1 << place[i]))) return;
        seen |= 1 << place[i];
    }
    for (int i = 0; i < 4; i++) {
        tp_face_place[i] = place[i];
        tp_face_letter[i] = letter[i];
    }
}

static void tp_apply_faces(TpLayout *L) {
    static const int btn_of[4] = {BTN_CROSS, BTN_SQUARE, BTN_CIRCLE, BTN_TRIANGLE};
    static const float dx[4] = {0.0f, 1.0f, -1.0f, 0.0f}, dy[4] = {1.0f, 0.0f, 0.0f, -1.0f};
    for (int i = 0; i < 4; i++) {
        TpBtn *b = &L->btns[btn_of[i]];
        b->cx = L->face_cx + dx[tp_face_place[i]] * L->face_sp;
        b->cy = L->face_cy + dy[tp_face_place[i]] * L->face_sp;
        char ch = tp_face_letter[i];
        if (ch != 'A' && ch != 'B' && ch != 'X' && ch != 'Y') continue; /* the PS1 shapes and colours stay */
        tp_face_label[i][0] = ch;
        tp_face_label[i][1] = 0;
        b->label = tp_face_label[i];
        /* The colours the letters carry on the pads that print them (green A, red B, blue X, yellow Y). */
        switch (ch) {
        case 'A': b->cr = 80; b->cg = 215; b->cb = 110; break;
        case 'B': b->cr = 255; b->cg = 95; b->cb = 95; break;
        case 'X': b->cr = 80; b->cg = 160; b->cb = 255; break;
        case 'Y': b->cr = 250; b->cg = 210; b->cb = 70; break;
        }
    }
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

    if (!tp_enabled || tp_collapsed || tp_editing) {
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

/* The editor's drag: the group follows the finger, its anchor kept on the screen. */
static void tp_drag(TouchFinger *f, float px, float py) {
    float fh = (float)tp_view_h;
    TpGroup *G = &tp_groups[tp_sel_group];
    float ox = G->ox, oy = G->oy;
    G->ox = f->sox + (px - f->sx) / fh;
    G->oy = f->soy + (py - f->sy) / fh;
    TpLayout L;
    tp_compute_layout(tp_view_w, tp_view_h, &L);
    float cx, cy, hw, hh;
    tp_group_box(&L, tp_sel_group, &cx, &cy, &hw, &hh);
    if (cx < 0.0f || cx > (float)L.w || cy < 0.0f || cy > (float)L.h) {
        G->ox = ox;
        G->oy = oy;
    }
    tp_dirty = 1;
}

static void tp_edit_button(int ui) {
    TpGroup *G = &tp_groups[tp_sel_group];
    if (ui == UI_SMALLER) {
        G->s = tp_clampf(G->s - TP_SCALE_STEP, TP_SCALE_MIN, TP_SCALE_MAX);
    } else if (ui == UI_BIGGER) {
        G->s = tp_clampf(G->s + TP_SCALE_STEP, TP_SCALE_MIN, TP_SCALE_MAX);
    } else if (ui == UI_RESET) {
        tp_groups_reset();
    } else if (ui == UI_OK) {
        tp_editing = 0;
    }
    tp_layout_save();
    tp_dirty = 1;
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
        if (tp_pad_hidden) {
            tp_pad_hidden = 0; /* this tap only brings the pad back; it presses nothing */
            tp_dirty = 1;
            return;
        }
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

        if (tp_editing) {
            int ui = tp_hit_ui(&L, px, py);
            if (ui >= 0) {
                f->role = ROLE_EDIT_UI;
                f->locked_btn = ui;
                return;
            }
            int g = tp_hit_group(&L, px, py);
            if (g >= 0) {
                tp_sel_group = g;
                f->role = ROLE_DRAG;
                f->sx = px;
                f->sy = py;
                f->sox = tp_groups[g].ox;
                f->soy = tp_groups[g].oy;
                tp_dirty = 1;
            }
            return;
        }

        /* The corner buttons: hide (left) and edit (right); the edit one is not there while the pad is hidden */
        if (tp_hit_corner(&L, L.tog_cx, L.tog_cy, px, py, 10.0f)) {
            f->role = ROLE_TOGGLE;
            tp_recompute();
            return;
        }
        if (tp_collapsed) {
            f->role = ROLE_NONE;
            return;
        }
        if (tp_hit_corner(&L, L.edit_cx, L.edit_cy, px, py, 10.0f)) {
            f->role = ROLE_EDIT;
            tp_recompute();
            return;
        }

        /* D-pad capture */
        float ddx = px - L.dpad_cx;
        float ddy = py - L.dpad_cy;
        if (ddx * ddx + ddy * ddy <= (L.dpad_r * 1.45f) * (L.dpad_r * 1.45f)) {
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
        if (fb < 0) {
            float fdx = px - L.face_cx, fdy = py - L.face_cy, reach = (L.face_sp + L.face_r) * 1.3f;
            if (fdx * fdx + fdy * fdy <= reach * reach) {
                fb = tp_closest_face(&L, px, py, L.face_sp * 2.0f);
            }
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
                if (tp_fingers[i].role == ROLE_DRAG && tp_editing) {
                    tp_drag(&tp_fingers[i], px, py);
                }
                break;
            }
        }
    } else { /* SDL_EVENT_FINGER_UP or SDL_EVENT_FINGER_CANCELED */
        for (int i = 0; i < MAX_TOUCH_FINGERS; i++) {
            TouchFinger *f = &tp_fingers[i];
            if (f->active && f->id == fid) {
                int up = e->type == SDL_EVENT_FINGER_UP;
                if (up && f->role == ROLE_TOGGLE && tp_hit_corner(&L, L.tog_cx, L.tog_cy, px, py, 24.0f)) {
                    tp_collapsed = !tp_collapsed;
                    tp_dirty = 1;
                } else if (up && f->role == ROLE_EDIT && tp_hit_corner(&L, L.edit_cx, L.edit_cy, px, py, 24.0f)) {
                    tp_editing = 1;
                    tp_dirty = 1;
                } else if (up && f->role == ROLE_EDIT_UI && tp_hit_ui(&L, px, py) == f->locked_btn) {
                    tp_edit_button(f->locked_btn);
                } else if (f->role == ROLE_DRAG) {
                    tp_layout_save();
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
    case '+':
        aa_line(buf, w, h, cx - hs * 0.7f, cy, cx + hs * 0.7f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx, cy - hs * 0.7f, cx, cy + hs * 0.7f, th, r, g, b, a);
        break;
    case '-':
        aa_line(buf, w, h, cx - hs * 0.7f, cy, cx + hs * 0.7f, cy, th, r, g, b, a);
        break;
    case 'K':
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx - hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.6f, cy - hs, cx - hs * 0.6f, cy + hs * 0.1f, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.2f, cy - hs * 0.2f, cx + hs * 0.6f, cy + hs, th, r, g, b, a);
        break;
    case 'B':
        aa_line(buf, w, h, cx - hs * 0.55f, cy - hs, cx - hs * 0.55f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.55f, cy - hs, cx + hs * 0.3f, cy - hs, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.3f, cy - hs, cx + hs * 0.5f, cy - hs * 0.55f, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.5f, cy - hs * 0.55f, cx + hs * 0.3f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.55f, cy, cx + hs * 0.35f, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.35f, cy, cx + hs * 0.6f, cy + hs * 0.5f, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.6f, cy + hs * 0.5f, cx + hs * 0.35f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.35f, cy + hs, cx - hs * 0.55f, cy + hs, th, r, g, b, a);
        break;
    case 'Y':
        aa_line(buf, w, h, cx - hs * 0.6f, cy - hs, cx, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx + hs * 0.6f, cy - hs, cx, cy, th, r, g, b, a);
        aa_line(buf, w, h, cx, cy, cx, cy + hs, th, r, g, b, a);
        break;
    case 'A':
        aa_line(buf, w, h, cx, cy - hs, cx - hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx, cy - hs, cx + hs * 0.6f, cy + hs, th, r, g, b, a);
        aa_line(buf, w, h, cx - hs * 0.35f, cy + hs * 0.25f, cx + hs * 0.35f, cy + hs * 0.25f, th, r, g, b, a);
        break;
    }
}

/* A label with a dark drop shadow under it: on a 16:9 or 4:3 screen the pad sits on the picture, and a light glyph
 * alone disappears on the game's bright scenes (pavements, snow, menus). */
static void draw_label(uint32_t *buf, int w, int h, float cx, float cy, float s, char ch,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    float off = s * 0.09f;
    if (off < 1.0f) off = 1.0f;
    draw_glyph(buf, w, h, cx + off, cy + off, s, ch, 8, 12, 16, (uint8_t)(a * 0.85f));
    draw_glyph(buf, w, h, cx, cy, s, ch, r, g, b, a);
}

/* A word of the glyphs above, centred on (cx, cy), each letter s tall. */
static void draw_word(uint32_t *buf, int w, int h, float cx, float cy, float s, const char *str, uint8_t a) {
    int n = (int)strlen(str);
    float step = s * 1.05f;
    float x = cx - step * (float)(n - 1) * 0.5f;
    for (int i = 0; i < n; i++, x += step) {
        draw_label(buf, w, h, x, cy, s, str[i], 240, 250, 255, a);
    }
}

/* The hide button: a chevron pointing up (the pad shown: tap to hide) or a bar (hidden: tap to show). */
static void draw_hide_button(uint32_t *buf, int w, int h, const TpLayout *L) {
    float cx = L->tog_cx, cy = L->tog_cy, r = L->corner_r;
    aa_circle_panel(buf, w, h, cx, cy, r, 0, 215, 190, 0, 0.75f);
    if (tp_collapsed) {
        aa_line(buf, w, h, cx - r * 0.4f, cy, cx + r * 0.4f, cy, r * 0.12f, 220, 245, 240, 220);
        aa_line(buf, w, h, cx, cy - r * 0.4f, cx, cy + r * 0.4f, r * 0.12f, 220, 245, 240, 220);
    } else {
        aa_line(buf, w, h, cx - r * 0.4f, cy + r * 0.15f, cx, cy - r * 0.25f, r * 0.12f, 220, 245, 240, 220);
        aa_line(buf, w, h, cx, cy - r * 0.25f, cx + r * 0.4f, cy + r * 0.15f, r * 0.12f, 220, 245, 240, 220);
    }
}

/* The edit button: four arrows out of the centre (move and size the pad). */
static void draw_edit_button(uint32_t *buf, int w, int h, const TpLayout *L) {
    float cx = L->edit_cx, cy = L->edit_cy, r = L->corner_r, t = r * 0.11f, a = r * 0.5f, k = r * 0.17f;
    aa_circle_panel(buf, w, h, cx, cy, r, 0, 215, 190, tp_editing, 0.75f);
    aa_line(buf, w, h, cx - a, cy, cx + a, cy, t, 220, 245, 240, 220);
    aa_line(buf, w, h, cx, cy - a, cx, cy + a, t, 220, 245, 240, 220);
    for (int s = -1; s <= 1; s += 2) {
        aa_line(buf, w, h, cx + s * a, cy, cx + s * (a - k), cy - k, t, 220, 245, 240, 220);
        aa_line(buf, w, h, cx + s * a, cy, cx + s * (a - k), cy + k, t, 220, 245, 240, 220);
        aa_line(buf, w, h, cx, cy + s * a, cx - k, cy + s * (a - k), t, 220, 245, 240, 220);
        aa_line(buf, w, h, cx, cy + s * a, cx + k, cy + s * (a - k), t, 220, 245, 240, 220);
    }
}

int port_touch_draw_rgba(uint32_t *rgba, int w, int h, const int game_rect[4]) {
    tp_ensure_init();
    if (!tp_enabled || w <= 0 || h <= 0) {
        return -1;
    }
    tp_view_w = w;
    tp_view_h = h;
    {
        int has_gamepad = SDL_HasGamepad() ? 1 : 0;
        if (has_gamepad && !tp_had_gamepad) {
            tp_pad_hidden = 1;
            tp_editing = 0;
            memset(tp_fingers, 0, sizeof(tp_fingers));
            tp_recompute();
        } else if (!has_gamepad && tp_pad_hidden) {
            tp_pad_hidden = 0;
        }
        if (has_gamepad != tp_had_gamepad) {
            tp_dirty = 1;
        }
        tp_had_gamepad = has_gamepad;
    }
    if (tp_pad_hidden) {
        return -1; /* the renderers draw no overlay this frame */
    }
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
    /* The bigger pad lies on the picture's edges on most phones: translucent enough to see the game through. */
    float alpha_scale = 0.85f;

    draw_hide_button(rgba, w, h, &L);
    if (tp_collapsed) {
        return 1;
    }
    draw_edit_button(rgba, w, h, &L);

    if (tp_editing) {
        /* The editor: the chosen group lit behind, the others dimmed, and its buttons in the middle. */
        for (int g = 0; g < N_GROUPS; g++) {
            float cx, cy, hw, hh;
            tp_group_box(&L, g, &cx, &cy, &hw, &hh);
            float pad = (float)h * 0.02f;
            aa_rrect_panel(rgba, w, h, cx, cy, hw + pad, hh + pad, (float)h * 0.03f,
                           g == tp_sel_group ? 255 : 120, g == tp_sel_group ? 200 : 140, g == tp_sel_group ? 60 : 160,
                           g == tp_sel_group, g == tp_sel_group ? 0.55f : 0.35f);
        }
        for (int i = 0; i < N_UI; i++) {
            aa_rrect_panel(rgba, w, h, L.ui[i].cx, L.ui[i].cy, L.ui[i].hw, L.ui[i].hh, L.ui[i].hh * 0.8f,
                           i == UI_OK ? 60 : 0, i == UI_OK ? 220 : 200, i == UI_OK ? 120 : 190, 0, 1.0f);
            draw_word(rgba, w, h, L.ui[i].cx, L.ui[i].cy, L.ui[i].hh * 0.75f, L.ui[i].label, 240);
        }
    }

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
            draw_label(rgba, w, h, L.btns[b].cx, L.btns[b].cy, gs, '^', 240, 250, 255, la);
        } else if (strcmp(lbl, "[]") == 0) {
            draw_label(rgba, w, h, L.btns[b].cx, L.btns[b].cy, gs, '[', 240, 250, 255, la);
        } else if (strlen(lbl) == 1) {
            draw_label(rgba, w, h, L.btns[b].cx, L.btns[b].cy, gs, lbl[0], 240, 250, 255, la);
        } else if (strlen(lbl) == 2) {
            draw_label(rgba, w, h, L.btns[b].cx - gs * 0.55f, L.btns[b].cy, gs * 0.9f, lbl[0], 240, 250, 255, la);
            draw_label(rgba, w, h, L.btns[b].cx + gs * 0.55f, L.btns[b].cy, gs * 0.9f, lbl[1], 240, 250, 255, la);
        } else if (strlen(lbl) == 3) {
            draw_label(rgba, w, h, L.btns[b].cx - gs * 1.0f, L.btns[b].cy, gs * 0.8f, lbl[0], 240, 250, 255, la);
            draw_label(rgba, w, h, L.btns[b].cx, L.btns[b].cy, gs * 0.8f, lbl[1], 240, 250, 255, la);
            draw_label(rgba, w, h, L.btns[b].cx + gs * 1.0f, L.btns[b].cy, gs * 0.8f, lbl[2], 240, 250, 255, la);
        }
    }

    return 1;
}
