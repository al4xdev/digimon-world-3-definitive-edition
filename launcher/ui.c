#include "ui.h"

#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "assets/assets.h"

uint32_t *fb;
int ui_scale = 1, ui_w = W, ui_h = H;

uint32_t mix(uint32_t a, uint32_t b, int t) {
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return RGB(ar + (br - ar) * t / 256, ag + (bg - ag) * t / 256, ab + (bb - ab) * t / 256);
}

void px_raw(int px, int py, uint32_t c, int a) {
    if (px < 0 || py < 0 || px >= ui_w || py >= ui_h || a <= 0) {
        return;
    }
    fb[py * ui_w + px] = a >= 256 ? c : mix(fb[py * ui_w + px], c, a);
}

static void rect_raw(int x, int y, int w, int h, uint32_t c, int a) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > ui_w ? ui_w : x + w, y1 = y + h > ui_h ? ui_h : y + h;
    if (a >= 256) {
        for (int j = y0; j < y1; j++) {
            uint32_t *row = &fb[j * ui_w];
            for (int i = x0; i < x1; i++) row[i] = c;
        }
    } else if (a > 0) {
        for (int j = y0; j < y1; j++) {
            uint32_t *row = &fb[j * ui_w];
            for (int i = x0; i < x1; i++) row[i] = mix(row[i], c, a);
        }
    }
}

void rect(int x, int y, int w, int h, uint32_t c, int a) {
    rect_raw(x * ui_scale, y * ui_scale, w * ui_scale, h * ui_scale, c, a);
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
    int sx = x * ui_scale, sy = y * ui_scale, sw = w * ui_scale, sh = h * ui_scale, sr = r * ui_scale;
    if (sr <= 0) {
        rect_raw(sx, sy, sw, sh, c, a);
        return;
    }
    if (sh > sr * 2) {
        rect_raw(sx, sy + sr, sw, sh - sr * 2, c, a);
    }
    for (int j = 0; j < sh; j++) {
        if (j >= sr && j < sh - sr) continue;
        for (int i = 0; i < sw; i++) {
            int cv = (i < sr || i >= sw - sr) ? rr_cover(i, j, sw, sh, sr) : 256;
            px_raw(sx + i, sy + j, c, cv * a / 256);
        }
    }
}

void rrect_line(int x, int y, int w, int h, int r, uint32_t c) {
    int sx = x * ui_scale, sy = y * ui_scale, sw = w * ui_scale, sh = h * ui_scale, sr = r * ui_scale;
    int t = ui_scale;
    for (int j = 0; j < sh; j++) {
        for (int i = 0; i < sw; i++) {
            if (i >= sr && i < sw - sr && j >= t && j < sh - t) continue;
            if (j >= sr && j < sh - sr && i >= t && i < sw - t) continue;
            int o = rr_cover(i, j, sw, sh, sr);
            if (o == 0) continue;
            int inside = (i >= t && j >= t && i < sw - t && j < sh - t)
                             ? rr_cover(i - t, j - t, sw - 2 * t, sh - 2 * t, sr > t ? sr - t : 0)
                             : 0;
            int edge = o - inside * o / 256;
            if (edge > 0) px_raw(sx + i, sy + j, c, edge);
        }
    }
}

void circle(int cx, int cy, int r, uint32_t c, int a) {
    int scx = cx * ui_scale, scy = cy * ui_scale, sr = r * ui_scale;
    for (int j = -sr - 1; j <= sr + 1; j++) {
        for (int i = -sr - 1; i <= sr + 1; i++) {
            int d2 = (i * i + j * j) * 16, r2 = sr * sr * 16;
            int cv = d2 <= r2 - 8 * sr ? 256 : d2 >= r2 + 8 * sr ? 0 : 128 - (d2 - r2) * 128 / (8 * sr + 1);
            px_raw(scx + i, scy + j, c, cv * a / 256);
        }
    }
}

void ring(int cx, int cy, int r, int thick, uint32_t c) {
    int scx = cx * ui_scale, scy = cy * ui_scale, sr = r * ui_scale, st = thick * ui_scale;
    for (int j = -sr - 1; j <= sr + 1; j++) {
        for (int i = -sr - 1; i <= sr + 1; i++) {
            int d = i * i + j * j;
            if (d <= sr * sr && d >= (sr - st) * (sr - st)) {
                px_raw(scx + i, scy + j, c, 256);
            }
        }
    }
}

void line(int x0, int y0, int x1, int y1, uint32_t c) {
    int sx0 = x0 * ui_scale, sy0 = y0 * ui_scale, sx1 = x1 * ui_scale, sy1 = y1 * ui_scale;
    int dx = abs(sx1 - sx0), sx = sx0 < sx1 ? 1 : -1, dy = -abs(sy1 - sy0), sy = sy0 < sy1 ? 1 : -1, e = dx + dy;
    for (;;) {
        rect_raw(sx0, sy0, ui_scale, ui_scale, c, 256);
        if (sx0 == sx1 && sy0 == sy1) {
            break;
        }
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; sx0 += sx; }
        if (e2 <= dx) { e += dx; sy0 += sy; }
    }
}

void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
    x0 *= ui_scale; y0 *= ui_scale; x1 *= ui_scale; y1 *= ui_scale; x2 *= ui_scale; y2 *= ui_scale;
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            int w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
            int w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
            int w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                px_raw(x, y, c, 256);
            }
        }
    }
}

static uint32_t *bg_cache;
static int bg_ready;

/* Vertical gradient + a soft vignette + a subtle digital grid, overlaid with moving data pulses and floating nodes. */
void background(int tick) {
    if (!bg_ready) {
        bg_cache = malloc((size_t)ui_w * ui_h * sizeof(uint32_t));
        int step = 32 * ui_scale;
        for (int y = 0; y < ui_h; y++) {
            uint32_t rowc = mix(C_BG0, C_BG1, y * 256 / ui_h);
            int dy = (y - ui_h / 2) / ui_scale;
            int ygrid = (y % step) < ui_scale;
            for (int x = 0; x < ui_w; x++) {
                int dx = (x - ui_w / 2) / ui_scale;
                int v = (dx * dx / 64 + dy * dy / 40) / 12; /* darker at the corners */
                uint32_t c = mix(rowc, 0, v > 90 ? 90 : v);
                if (ygrid || (x % step) < ui_scale) {
                    c = mix(c, C_GRID, 140);
                }
                bg_cache[y * ui_w + x] = c;
            }
        }
        bg_ready = 1;
    }
    memcpy(fb, bg_cache, (size_t)ui_w * ui_h * sizeof(uint32_t));

    /* Traveling data pulses along grid lines */
    for (int p = 0; p < 4; p++) {
        int horiz = (p & 1) == 0;
        int speed = 90 + p * 35;
        int pos = ((tick * speed / 1000) + p * 173) % (horiz ? (W + 120) : (H + 120)) - 60;
        uint32_t col = p == 1 ? C_GREEN : C_CYAN;
        if (horiz) {
            int y = (2 + p * 3) * 32;
            for (int k = 0; k < 44; k++) {
                int x = pos - k;
                if (x >= 0 && x < W) rect_raw(x * ui_scale, y * ui_scale, ui_scale, ui_scale, col, 180 - k * 4);
            }
        } else {
            int x = (3 + p * 4) * 32;
            for (int k = 0; k < 44; k++) {
                int y = pos - k;
                if (y >= 0 && y < H) rect_raw(x * ui_scale, y * ui_scale, ui_scale, ui_scale, col, 180 - k * 4);
            }
        }
    }

    /* Gently drifting digital particles */
    for (int n = 0; n < 22; n++) {
        int sp = 14 + (n * 7) % 24;
        int x = ((n * 97 + 23) + (tick * ((n % 3) - 1) * 6 / 1000)) % W;
        if (x < 0) x += W;
        int y = H - 1 - (((n * 53 + tick * sp / 1000)) % H);
        int sz = (n % 4 == 0) ? 3 : 2;
        int phase = (tick / 12 + n * 41) % 200;
        int a = 45 + (phase < 100 ? phase : 200 - phase);
        uint32_t col = (n % 5 == 0) ? C_GREEN : C_CYAN;
        rect(x, y, sz, sz, col, a);
    }
}

/* ---- text ---- */
#define FONT_SIZES 16
static stbtt_fontinfo finfo;
static int font_ok;
typedef struct { unsigned char *bm; int w, h, xo, yo, adv; } Glyph;
static struct { int size; float scale; int ascent; Glyph g[96]; } fcache[FONT_SIZES];
static int fcount;

void ui_init(int scale) {
    ui_scale = scale < 1 ? 1 : scale > 3 ? 3 : scale;
    ui_w = W * ui_scale;
    ui_h = H * ui_scale;
    free(fb);
    fb = calloc((size_t)ui_w * ui_h, sizeof(uint32_t));
    if (bg_cache) { free(bg_cache); bg_cache = NULL; bg_ready = 0; }
    font_ok = stbtt_InitFont(&finfo, font_data, 0);
}

static int fslot(int px_size) {
    for (int i = 0; i < fcount; i++) {
        if (fcache[i].size == px_size) {
            return i;
        }
    }
    int i = fcount < FONT_SIZES ? fcount++ : 0;
    memset(&fcache[i], 0, sizeof(fcache[i]));
    fcache[i].size = px_size;
    fcache[i].scale = stbtt_ScaleForPixelHeight(&finfo, (float)px_size);
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

static int text_w_px(const char *s, int size) {
    if (!font_ok) return 0;
    int slot = fslot(size * ui_scale), w = 0;
    for (; *s; s++) {
        w += glyph(slot, (unsigned char)*s)->adv;
    }
    return w;
}

int text_w(const char *s, int size) {
    return (text_w_px(s, size) + ui_scale / 2) / ui_scale;
}

static int text_raw(int px, int py, const char *s, int size, uint32_t c) {
    if (!font_ok) return px;
    int slot = fslot(size * ui_scale);
    int base = py + fcache[slot].ascent;
    for (; *s; s++) {
        Glyph *g = glyph(slot, (unsigned char)*s);
        for (int j = 0; j < g->h; j++)
            for (int i = 0; i < g->w; i++)
                px_raw(px + g->xo + i, base + g->yo + j, c, g->bm[j * g->w + i] * 256 / 255);
        px += g->adv;
    }
    return px;
}

int text(int x, int y, const char *s, int size, uint32_t c) {
    return text_raw(x * ui_scale, y * ui_scale, s, size, c) / ui_scale;
}

int text_c(int cx, int y, const char *s, int size, uint32_t c) {
    return text_raw(cx * ui_scale - text_w_px(s, size) / 2, y * ui_scale, s, size, c) / ui_scale;
}

int text_r(int rx, int y, const char *s, int size, uint32_t c) {
    return text_raw(rx * ui_scale - text_w_px(s, size), y * ui_scale, s, size, c) / ui_scale;
}

int keycap(int x, int cy, const char *label, uint32_t c) {
    int tw = text_w(label, 15), w = tw + 14 > 24 ? tw + 14 : 24;
    rrect(x, cy - 12, w, 24, 12, c, 256);
    text_raw((x + w / 2) * ui_scale - text_w_px(label, 15) / 2, (cy - 9) * ui_scale, label, 15, C_BG0);
    return w;
}
