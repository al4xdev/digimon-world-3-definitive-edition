#include "ui.h"

#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "assets/assets.h"

uint32_t fb[W * H];

uint32_t mix(uint32_t a, uint32_t b, int t) {
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return RGB(ar + (br - ar) * t / 256, ag + (bg - ag) * t / 256, ab + (bb - ab) * t / 256);
}

void px_blend(int x, int y, uint32_t c, int a) {
    if (x < 0 || y < 0 || x >= W || y >= H || a <= 0) {
        return;
    }
    fb[y * W + x] = a >= 256 ? c : mix(fb[y * W + x], c, a);
}

void rect(int x, int y, int w, int h, uint32_t c, int a) {
    for (int j = y < 0 ? 0 : y; j < y + h && j < H; j++) {
        for (int i = x < 0 ? 0 : x; i < x + w && i < W; i++) {
            px_blend(i, j, c, a);
        }
    }
}

/* Coverage of a rounded rect at (px,py) inside w x h with corner radius r: 0..256, antialiased on the corners. */
static int rr_cover(int px, int py, int w, int h, int r) {
    int cx = px < r ? r : (px >= w - r ? w - 1 - r : px);
    int cy = py < r ? r : (py >= h - r ? h - 1 - r : py);
    int dx = px - cx, dy = py - cy;
    if (dx == 0 && dy == 0) {
        return 256;
    }
    int d2 = (dx * dx + dy * dy) * 16, r2 = r * r * 16;
    if (d2 <= r2 - 8 * r) {
        return 256;
    }
    if (d2 >= r2 + 8 * r) {
        return 0;
    }
    return 128 - (d2 - r2) * 128 / (8 * r + 1);
}

void rrect(int x, int y, int w, int h, int r, uint32_t c, int a) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int cv = (r > 0) ? rr_cover(i, j, w, h, r) : 256;
            px_blend(x + i, y + j, c, cv * a / 256);
        }
    }
}

void rrect_line(int x, int y, int w, int h, int r, uint32_t c) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int o = rr_cover(i, j, w, h, r);
            if (o == 0) {
                continue;
            }
            int inside = (i > 0 && j > 0 && i < w - 1 && j < h - 1) ? rr_cover(i - 1, j - 1, w - 2, h - 2, r > 0 ? r - 1 : 0) : 0;
            int edge = o - inside * o / 256;
            if (i > 0 && j > 0 && i < w - 1 && j < h - 1 && inside == 256) {
                continue;
            }
            px_blend(x + i, y + j, c, edge > 0 ? edge : 0);
        }
    }
}

void circle(int cx, int cy, int r, uint32_t c, int a) {
    for (int j = -r - 1; j <= r + 1; j++) {
        for (int i = -r - 1; i <= r + 1; i++) {
            int d2 = (i * i + j * j) * 16, r2 = r * r * 16;
            int cv = d2 <= r2 - 8 * r ? 256 : d2 >= r2 + 8 * r ? 0 : 128 - (d2 - r2) * 128 / (8 * r + 1);
            px_blend(cx + i, cy + j, c, cv * a / 256);
        }
    }
}

void ring(int cx, int cy, int r, int t, uint32_t c) {
    for (int j = -r - 1; j <= r + 1; j++) {
        for (int i = -r - 1; i <= r + 1; i++) {
            int d = i * i + j * j;
            if (d <= r * r && d >= (r - t) * (r - t)) {
                px_blend(cx + i, cy + j, c, 256);
            }
        }
    }
}

void line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        px_blend(x0, y0, c, 256);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2), maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2), maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            int w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
            int w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
            int w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                px_blend(x, y, c, 256);
            }
        }
    }
}

static uint32_t bg_cache[W * H];
static int bg_ready;

/* Vertical gradient + a soft vignette + a sparse digital grid with a few lit nodes. Built once, copied each frame. */
void background(int tick) {
    if (!bg_ready) {
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                uint32_t c = mix(C_BG0, C_BG1, y * 256 / H);
                int dx = x - W / 2, dy = y - H / 2;
                int v = (dx * dx / 64 + dy * dy / 40) / 12; /* darker at the corners */
                c = mix(c, 0, v > 90 ? 90 : v);
                if (x % 32 == 0 || y % 32 == 0) {
                    c = mix(c, C_GRID, 150);
                }
                bg_cache[y * W + x] = c;
            }
        }
        for (int n = 0; n < 18; n++) { /* nodes at grid crossings */
            int x = ((n * 7 + 3) % 19) * 32 + 32, y = ((n * 5 + 2) % 13) * 32 + 32;
            if (x < W && y < H) {
                for (int j = -1; j <= 1; j++)
                    for (int i = -1; i <= 1; i++)
                        bg_cache[(y + j) * W + x + i] = mix(bg_cache[(y + j) * W + x + i], C_CYAN, 70);
            }
        }
        bg_ready = 1;
    }
    memcpy(fb, bg_cache, sizeof(fb));
    (void)tick;
}

/* ---- text ---- */
#define FONT_SIZES 6
static stbtt_fontinfo finfo;
static int font_ok;
typedef struct { unsigned char *bm; int w, h, xo, yo, adv; } Glyph;
static struct { int size; float scale; int ascent; Glyph g[96]; } fcache[FONT_SIZES];
static int fcount;

void ui_init(void) {
    font_ok = stbtt_InitFont(&finfo, font_data, 0);
}

static int fslot(int size) {
    for (int i = 0; i < fcount; i++) {
        if (fcache[i].size == size) {
            return i;
        }
    }
    int i = fcount < FONT_SIZES ? fcount++ : 0;
    memset(&fcache[i], 0, sizeof(fcache[i]));
    fcache[i].size = size;
    fcache[i].scale = stbtt_ScaleForPixelHeight(&finfo, (float)size);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&finfo, &asc, &desc, &gap);
    fcache[i].ascent = (int)(asc * fcache[i].scale + 0.5f);
    return i;
}

static Glyph *glyph(int slot, int ch) {
    if (ch < 32 || ch > 127) {
        ch = '?';
    }
    Glyph *g = &fcache[slot].g[ch - 32];
    if (!g->bm && g->adv == 0) {
        float s = fcache[slot].scale;
        int adv, lsb, x0, y0, x1, y1;
        stbtt_GetCodepointHMetrics(&finfo, ch, &adv, &lsb);
        g->adv = (int)(adv * s + 0.5f);
        if (g->adv == 0) {
            g->adv = 1;
        }
        stbtt_GetCodepointBitmapBox(&finfo, ch, s, s, &x0, &y0, &x1, &y1);
        g->w = x1 - x0;
        g->h = y1 - y0;
        g->xo = x0;
        g->yo = y0;
        if (g->w > 0 && g->h > 0) {
            g->bm = malloc((size_t)g->w * g->h);
            stbtt_MakeCodepointBitmap(&finfo, g->bm, g->w, g->h, g->w, s, s, ch);
        }
    }
    return g;
}

int text_w(const char *s, int size) {
    if (!font_ok) {
        return 0;
    }
    int slot = fslot(size), w = 0;
    for (; *s; s++) {
        w += glyph(slot, (unsigned char)*s)->adv;
    }
    return w;
}

int text(int x, int y, const char *s, int size, uint32_t c) {
    if (!font_ok) {
        return x;
    }
    int slot = fslot(size);
    int base = y + fcache[slot].ascent;
    for (; *s; s++) {
        Glyph *g = glyph(slot, (unsigned char)*s);
        for (int j = 0; j < g->h; j++)
            for (int i = 0; i < g->w; i++)
                px_blend(x + g->xo + i, base + g->yo + j, c, g->bm[j * g->w + i] * 256 / 255);
        x += g->adv;
    }
    return x;
}

int text_c(int cx, int y, const char *s, int size, uint32_t c) {
    return text(cx - text_w(s, size) / 2, y, s, size, c);
}

int text_r(int rx, int y, const char *s, int size, uint32_t c) {
    return text(rx - text_w(s, size), y, s, size, c);
}

int keycap(int x, int cy, const char *label, uint32_t c) {
    int tw = text_w(label, 15), w = tw + 14 > 24 ? tw + 14 : 24;
    rrect(x, cy - 12, w, 24, 12, c, 256);
    text(x + (w - tw) / 2, cy - 9, label, 15, C_BG0);
    return w;
}
