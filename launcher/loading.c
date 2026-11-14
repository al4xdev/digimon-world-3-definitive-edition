#include "loading.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/assets.h"
#include "cfg.h"
#include "plat.h"
#include "ui.h"

#define GROUND 292
#define JUMP_MS 720
#define JUMP_H 96
#define OB_W 24
#define OB_H 36

typedef struct { const char *prefix; const char *name; int x, scale; uint32_t color; int code; long jump_t; } Runner;
/* Left, top and right of the pad: the handheld's kernel calls them north, west and east. */
static Runner runners[3] = {
    {"k", "KUMAMON", 170, 3, RGB(0, 195, 175), K_NORTH, -1},
    {"g", "GUILMON", 285, 3, RGB(255, 140, 64), K_WEST, -1},
    {"a", "AGUMON", 400, 4, RGB(255, 204, 51), K_EAST, -1},
};

typedef struct { float x; int w; int hit; int scored; } Ob;
#define MAXOB 4
static Ob obs[MAXOB];
static float speed = 230, to_spawn = 400;
static int score, best, flash;
static char best_path[700];
static unsigned rng = 12345;

static unsigned rnd(void) { rng = rng * 1103515245u + 12345u; return (rng >> 16) & 0x7fff; }

void load_init(const char *best_file) {
    snprintf(best_path, sizeof(best_path), "%s", best_file ? best_file : "");
    FILE *f = best_path[0] ? fopen(best_path, "r") : NULL;
    if (f) {
        if (fscanf(f, "%d", &best) != 1) best = 0;
        fclose(f);
    }
    memset(obs, 0, sizeof(obs));
    for (int i = 0; i < MAXOB; i++) obs[i].w = 0;
    score = 0;
    speed = 230;
    to_spawn = 420;
}

void load_press(int code) {
    for (int i = 0; i < 3; i++) {
        if (runners[i].code == code && runners[i].jump_t < 0) {
            runners[i].jump_t = 0;
        }
    }
}

static int height_of(const Runner *r) {
    if (r->jump_t < 0) return 0;
    float t = (float)r->jump_t / JUMP_MS;
    return (int)(4.0f * JUMP_H * t * (1.0f - t));
}

static void save_best(void) {
    if (!best_path[0]) return;
    FILE *f = fopen(best_path, "w");
    if (f) {
        fprintf(f, "%d\n", best);
        fclose(f);
    }
}

void load_step(long dt) {
    for (int i = 0; i < 3; i++) {
        if (runners[i].jump_t >= 0) {
            runners[i].jump_t += dt;
            if (runners[i].jump_t >= JUMP_MS) runners[i].jump_t = -1;
        }
    }
    float dx = speed * dt / 1000.0f;
    to_spawn -= dx;
    if (to_spawn <= 0) {
        for (int i = 0; i < MAXOB; i++) {
            if (obs[i].w == 0) {
                obs[i].x = W + 20;
                obs[i].w = score >= 4 && rnd() % 4 == 0 ? OB_W * 2 : OB_W;
                obs[i].hit = obs[i].scored = 0;
                break;
            }
        }
        to_spawn = 520 + (float)(rnd() % 330);
    }
    for (int i = 0; i < MAXOB; i++) {
        Ob *o = &obs[i];
        if (!o->w) continue;
        o->x -= dx;
        for (int r = 0; r < 3 && !o->hit; r++) {
            int rx0 = runners[r].x - 16, rx1 = runners[r].x + 16;
            if (o->x < rx1 && o->x + o->w > rx0 && height_of(&runners[r]) < OB_H - 4) {
                o->hit = 1;
                flash = 8;
                if (score > best) { best = score; save_best(); }
                score = 0;
                speed = 230;
            }
        }
        if (!o->hit && !o->scored && o->x + o->w < runners[0].x - 16) {
            o->scored = 1;
            score++;
            if (speed < 430) speed += 9;
        }
        if (o->x + o->w < -10) o->w = 0;
    }
    if (flash > 0) flash--;
}

static const Sprite *spr(const char *name) {
    for (int i = 0; i < sprite_count; i++) {
        if (!strcmp(sprites[i].name, name)) return &sprites[i];
    }
    return NULL;
}

/* Feet at (cx, feet_y), `scale` screen pixels per art pixel, a 1-art-pixel halo in the runner's colour. */
static void draw_sprite(const Sprite *s, int cx, int feet_y, int scale, uint32_t halo) {
    int bottom = 0;
    for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++)
            if (s->px[y * s->w + x]) bottom = y;
    int x0 = cx - s->w * scale / 2, y0 = feet_y - (bottom + 1) * scale;
    for (int y = -1; y <= s->h; y++) {
        for (int x = -1; x <= s->w; x++) {
            int v = (x >= 0 && y >= 0 && x < s->w && y < s->h) ? s->px[y * s->w + x] : 0;
            uint32_t c;
            int a = 256;
            if (v == 1) c = C_BG0;
            else if (v == 2) c = RGB(232, 244, 248);
            else {
                int near = 0;
                static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (int k = 0; k < 4; k++) {
                    int nx = x + d[k][0], ny = y + d[k][1];
                    if (nx >= 0 && ny >= 0 && nx < s->w && ny < s->h && s->px[ny * s->w + nx]) near = 1;
                }
                if (!near) continue;
                c = halo;
                a = 190;
            }
            rect(x0 + x * scale, y0 + y * scale, scale, scale, c, a);
        }
    }
}

static const char *letter_for(const Cfg *c, int code, char *buf) {
    for (int a = 0; a <= A_TRIANGLE; a++) {
        if (c->pad[a].code == code && c->pad[a].letter != '-') {
            buf[0] = c->pad[a].letter;
            buf[1] = 0;
            return buf;
        }
    }
    buf[0] = layout_letter(LAYOUT_XBOX, code);
    buf[1] = 0;
    return buf[0] == '-' ? "?" : buf;
}

void load_draw(const Cfg *c, long tick_ms, const char *stage, int percent) {
    background(0);
    text(28, 20, "DIGIMON WORLD 3", 26, C_TEXT);
    text(28, 50, "LOADING", 16, C_CYAN);
    char sc[64];
    snprintf(sc, sizeof(sc), "SCORE %04d   BEST %04d", score, best);
    text_r(W - 28, 28, sc, 18, C_GOLD);

    /* the track: a lit line, and dashes that slide under the runners at the obstacle's speed */
    rect(0, GROUND + 1, W, 2, C_CYAN, 256);
    rect(0, GROUND + 3, W, 1, RGB(18, 82, 92), 256);
    int off = (int)(tick_ms * speed / 1000) % 64;
    for (int x = -off; x < W; x += 64) {
        rect(x + 8, GROUND + 12, 18, 2, RGB(38, 120, 130), 256);
        rect(x + 40, GROUND + 22, 8, 2, RGB(38, 120, 130), 256);
    }
    for (int i = 0; i < MAXOB; i++) {
        Ob *o = &obs[i];
        if (!o->w) continue;
        uint32_t col = o->hit ? C_RED : C_ORANGE;
        for (int k = 0; k < o->w; k += OB_W) {
            int bx = (int)o->x + k;
            tri(bx, GROUND, bx + OB_W / 2, GROUND - OB_H, bx + OB_W, GROUND, col);
            tri(bx + 5, GROUND, bx + OB_W / 2, GROUND - OB_H + 12, bx + OB_W - 5, GROUND, C_BG0);
        }
    }
    for (int i = 0; i < 3; i++) {
        Runner *r = &runners[i];
        char name[8];
        int air = r->jump_t >= 0;
        int frame = (int)(tick_ms / 140 + i) % 2;
        const char *key;
        char buf[8];
        if (air) key = (snprintf(name, sizeof(name), "%s%d", r->prefix, r->prefix[0] == 'a' ? 4 : r->prefix[0] == 'g' ? 4 : 3), name);
        else key = (snprintf(name, sizeof(name), "%s%d", r->prefix, r->prefix[0] == 'a' ? 2 + frame : 1 + frame), name);
        const Sprite *s = spr(key);
        int h = height_of(r);
        if (h > 6) {
            rect(r->x - 18, GROUND + 5, 36, 3, RGB(10, 56, 68), 256);
        }
        if (s) draw_sprite(s, r->x, GROUND - h, r->scale, r->color);
        /* the button that makes this one jump */
        int cx = r->x - 40;
        cx = r->x - (text_w(r->name, 14) + 30) / 2;
        int w = keycap(cx, GROUND + 52, letter_for(c, r->code, buf), r->color);
        text(cx + w + 6, GROUND + 43, r->name, 14, r->color);
    }
    if (flash > 0) rect(0, 0, W, H, C_RED, flash * 10);

    int bx = 120, bw = W - 240;
    rrect(bx, 398, bw, 12, 6, RGB(18, 38, 54), 256);
    if (percent > 0) {
        int fw = bw * (percent > 100 ? 100 : percent) / 100;
        rrect(bx, 398, fw < 12 ? 12 : fw, 12, 6, C_GREEN, 256);
    }
    text_c(W / 2, 418, stage, 18, C_TEXT);
    text_c(W / 2, 444, "JUMP THE FIREWALL WITH EACH ONE'S BUTTON", 14, C_NOTE);
}
