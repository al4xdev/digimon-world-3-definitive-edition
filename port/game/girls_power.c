/* Girls Power (.plan/tasks/girls-power-kaili-prd.md): Kaili is the hero and Junior her friend, with no text changed.
 *
 * The field draws people from 4-bit TIM frames in FIELD/SPRT P-files (docs/FORMATS.md "Actor sprites"). Junior's
 * are P001PLAY (actor 1, the scenes), P002PLAY (actor 2, the one the player walks) and the action files P002PLYD/F/K/L
 * that fieldstg_actor_update_sprite switches to for the kick, fishing and event actions; Kaili's are P011MINO
 * (actor 11) and P012MINT (actor 178). She has 9 animations where he has 27 plus the actions, so pointing his actor
 * at her file would leave most of his moves on her idle frame. Instead every frame of the loaded file is repainted in
 * place, when cdload has just read it: the head found in the frame is cut out and the other's head of the same
 * direction is put on the same neck, and the clothes take the other's colours (each frame carries its own CLUT, and
 * the two palettes are 16 colours each, so a table maps one's indices onto the other's). His files become her and
 * hers become him, so a scene with both still shows two people. Every animation, its timing and its script stay the
 * game's own.
 *
 * Finding the head: the cap (Junior) or the hair (Kaili) is the connected blob of its colours holding the most
 * pixels of one colour no other part has (his lavender, her pink). The head is that blob's columns, from its top
 * down to the chin (the last row with skin), plus stray hair beside it (the pigtails). The face's skin picks the
 * direction: little skin, the back; skin centred, the front; a little off centre, three quarters; far off, the
 * profile; the side it is on, the mirror. The heads put back are the other's idle frames (animation 1, directions
 * 0-4), read from the disc itself, so the repainted file never feeds its own references.
 *
 * Prototyped in Python over the disc's frames (.dumps/tasks/girls-power/swap4.py); this is the same algorithm.
 * Off under a replay script (shared with the emulator) like the definitive rules; DW3_GIRLS=0 or 1 forces it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "cdload.h"
#include "filetable.h"
#include "port_harness.h"
#include "port_runtime.h"

#include "definitive.h"

#define GP_MAX 64 /* the biggest frame side handled (the field's people are at most 32x32) */

typedef struct GpWho {
    s32 file;    /* the file the reference heads come from */
    s32 table;   /* its animation table (sub-file) */
    u16 cap;     /* colour masks (bit = CLUT index): the cap or hair */
    u16 key;     /* the colour only the cap/hair has */
    u16 skin;
    u16 outline; /* the outline around stray hair */
    s32 neck;    /* the most rows a head takes */
} GpWho;

static const GpWho gp_junior = { 0x187, 0x70, (1 << 2) | (1 << 3) | (1 << 7), 1 << 7, (1 << 8) | (1 << 9), 0, 16 };
static const GpWho gp_kaili = { 0x190, 0xA8, (1 << 4) | (1 << 6) | (1 << 14), 1 << 4, (1 << 1) | (1 << 2), 1 << 7, 15 };

/* The clothes: his index -> hers by role (skin, shirt, jacket, trousers, outlines), and back. */
static const u8 gp_j2k[16] = { 0, 4, 7, 12, 6, 7, 5, 3, 1, 2, 10, 13, 14, 8, 9, 15 };
static const u8 gp_k2j[16] = { 0, 8, 9, 7, 3, 6, 2, 13, 13, 14, 10, 12, 3, 11, 2, 15 };

typedef struct GpPix {
    s8 y, x;
    u8 v;
} GpPix;

typedef struct GpHead {
    GpPix px[GP_MAX * GP_MAX / 4];
    s32 n;
    s32 bottom; /* the chin's row */
    s32 x2;     /* x0 + x1 of the cap/hair blob: twice its centre */
} GpHead;

typedef struct GpRefs {
    u16 clut[16];
    GpHead head[5];
} GpRefs;

static GpRefs gp_ref_junior, gp_ref_kaili;

/* The two screens that show Junior standing (the name entry, NAMEENT/STNMETTM; the card screens, SCREEN/NAMEDTTM) keep
 * his front frame, P002PLAY's sub-file 85 (animation 7, direction 0), in an RLEN-packed VRAM page with his CLUT beside
 * it; port_girls_tim puts Kaili's own front idle frame and CLUT there. Only his frame is touched in the page, and only
 * his CLUT (an exact 16-colour match: no other image on the disc has it). */
#define GP_UI_FRAME 85
static u8 gp_ui_junior[GP_MAX][GP_MAX], gp_ui_kaili[GP_MAX][GP_MAX];
static s32 gp_ui_jw, gp_ui_jh, gp_ui_kw, gp_ui_kh, gp_ui_dx, gp_ui_dy;
static u16 gp_ui_jclut[16];
static void gp_dump(const char *dir);
static int gp_refs_state; /* 0 not read, 1 ready, -1 failed */

static u8 gp_img[GP_MAX][GP_MAX], gp_out[GP_MAX][GP_MAX];
static s32 gp_w, gp_h;

/* Kaili is the hero: the save's choice (DEF_SAVE_KAILI, New Game's question after the difficulty), never under a replay
 * script; DW3_GIRLS=0/1 overrides both. */
static int gp_on(void) {
    static int forced = -2;
    const char *e;

    if (forced == -2) {
        e = getenv("DW3_GIRLS");
        forced = e != NULL ? atoi(e) != 0 : -1;
    }
    if (forced >= 0) {
        return forced;
    }
    return !port_script_active && def_save_flag(DEF_SAVE_KAILI);
}

/* What the cached sprite files were painted for (0: as on the disc). */
static int gp_painted;
static const s32 gp_file_ids[] = { 0x186, 0x187, 0x3C9, 0x3CA, 0x3CB, 0x3CC, 0x190, 0x2F8 };

void port_girls_refresh(void) {
    int on = gp_on();
    u32 i;

    if (on == gp_painted) {
        return;
    }
    /* Called off the field (the title, New Game, a card's save read): no actor holds these files. A file still loading
     * is left to finish; it is painted by what holds when it lands. */
    for (i = 0; i < sizeof(gp_file_ids) / sizeof(gp_file_ids[0]); i++) {
        cdload_module.files.free_file(gp_file_ids[i]);
    }
    gp_painted = on;
    port_log("girls power: the hero is %s", on ? "Kaili" : "Junior");
}

/* ---- the containers (docs/FORMATS.md "Containers"; FieldstgActor scripts) ---- */

static s32 gp_count(const u8 *data) {
    return *(const s32 *)data / 4;
}

static const u8 *gp_sub(const u8 *data, s32 i) {
    return data + ((const s32 *)data)[i];
}

/* The TIM of frame sub-file `i`: its CLUT and pixel words, or 0 when it is not a 4-bit TIM with a CLUT. */
static int gp_tim(u8 *data, s32 i, u16 **clut, u8 **pixels, s32 *w, s32 *h) {
    u8 *p = (u8 *)gp_sub(data, i);
    u8 *q;

    if (((u32 *)p)[0] != 0x10 || ((u32 *)p)[1] != 8) {
        return 0;
    }
    q = p + 8;
    *clut = (u16 *)(q + 12);
    if (((u16 *)(q + 8))[0] < 16) {
        return 0;
    }
    q += *(u32 *)q;
    *w = ((u16 *)(q + 8))[0] * 4;
    *h = ((u16 *)(q + 10))[0];
    *pixels = q + 12;
    return *w > 0 && *h > 0 && *w <= GP_MAX && *h <= GP_MAX;
}

static void gp_unpack(const u8 *pixels) {
    s32 y, x;

    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            u8 b = pixels[y * (gp_w / 2) + x / 2];
            gp_img[y][x] = (x & 1) ? (b >> 4) : (b & 15);
        }
    }
}

static void gp_pack(u8 *pixels) {
    s32 y, x;

    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x += 2) {
            pixels[y * (gp_w / 2) + x / 2] = (u8)(gp_out[y][x] | (gp_out[y][x + 1] << 4));
        }
    }
}

/* ---- the head ---- */

#define GP_IN(m, v) (((m) >> (v)) & 1)

static u8 gp_mark[GP_MAX][GP_MAX];
static s16 gp_stack[GP_MAX * GP_MAX][2];

typedef struct GpFound {
    s32 dir, flip;
    s32 bottom, x2;
} GpFound;

/* Marks the head of gp_img in gp_mark (1) and says where it is and which way it looks; 0 when there is none. */
static int gp_find_head(const GpWho *who, GpFound *f) {
    static u8 seen[GP_MAX][GP_MAX];
    s32 y, x, best_key = 0, best_label = 0, label = 0;
    s32 y0 = GP_MAX, x0 = GP_MAX, y1 = -1, x1 = -1, size = 0;
    s32 bottom, nsk = 0, sum = 0, rows_end, cols0, cols1, d;
    double rel;

    memset(seen, 0, sizeof(seen));
    memset(gp_mark, 0, sizeof(gp_mark));
    /* the blobs of cap colours, labelled in scan order; keep the one with the most key pixels (the first on a tie) */
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            s32 sp = 0, keys = 0;

            if (seen[y][x] || !GP_IN(who->cap, gp_img[y][x])) {
                continue;
            }
            label++;
            seen[y][x] = (u8)label;
            gp_stack[sp][0] = (s16)y;
            gp_stack[sp][1] = (s16)x;
            sp++;
            while (sp > 0) {
                s32 a, b, k;
                static const s32 dy[4] = { 1, -1, 0, 0 }, dx[4] = { 0, 0, 1, -1 };

                sp--;
                a = gp_stack[sp][0];
                b = gp_stack[sp][1];
                keys += GP_IN(who->key, gp_img[a][b]);
                for (k = 0; k < 4; k++) {
                    s32 c = a + dy[k], e = b + dx[k];

                    if (c >= 0 && c < gp_h && e >= 0 && e < gp_w && !seen[c][e] && GP_IN(who->cap, gp_img[c][e])) {
                        seen[c][e] = (u8)label;
                        gp_stack[sp][0] = (s16)c;
                        gp_stack[sp][1] = (s16)e;
                        sp++;
                    }
                }
            }
            if (keys > best_key) {
                best_key = keys;
                best_label = label;
            }
        }
    }
    if (best_label == 0) {
        return 0;
    }
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            if (seen[y][x] == best_label) {
                gp_mark[y][x] = 1;
                size++;
                y0 = y < y0 ? y : y0;
                y1 = y > y1 ? y : y1;
                x0 = x < x0 ? x : x0;
                x1 = x > x1 ? x : x1;
            }
        }
    }
    if (size < 6) {
        return 0;
    }
    rows_end = y0 + who->neck < gp_h ? y0 + who->neck : gp_h;
    cols0 = x0 - 1 > 0 ? x0 - 1 : 0;
    cols1 = x1 + 2 < gp_w ? x1 + 2 : gp_w;
    bottom = -1;
    for (y = y0; y < rows_end; y++) {
        for (x = cols0; x < cols1; x++) {
            if (GP_IN(who->skin, gp_img[y][x])) {
                bottom = y;
            }
        }
    }
    if (bottom < 0) {
        bottom = y0 + who->neck - 2;
    }
    for (y = y0; y < gp_h && y <= bottom; y++) {
        for (x = cols0; x < cols1; x++) {
            if (gp_img[y][x]) {
                gp_mark[y][x] = 1;
            }
        }
    }
    /* strays: hair outside the blob's columns (pigtails), then the outline touching them */
    for (y = y0 - 2 > 0 ? y0 - 2 : 0; y < gp_h && y < bottom + 2; y++) {
        for (x = 0; x < gp_w; x++) {
            if (GP_IN(who->cap, gp_img[y][x])) {
                gp_mark[y][x] |= 2;
            }
        }
    }
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            static const s32 dy[4] = { 1, -1, 0, 0 }, dx[4] = { 0, 0, 1, -1 };
            s32 k;

            if (!(gp_mark[y][x] & 2) || !GP_IN(who->cap, gp_img[y][x])) {
                continue;
            }
            for (k = 0; k < 4; k++) {
                s32 c = y + dy[k], e = x + dx[k];

                if (c >= 0 && c < gp_h && e >= 0 && e < gp_w && GP_IN(who->outline, gp_img[c][e])) {
                    gp_mark[c][e] |= 4;
                }
            }
        }
    }
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            if (gp_mark[y][x] && GP_IN(who->skin, gp_img[y][x])) {
                nsk++;
                sum += x;
            }
        }
    }
    rel = nsk ? (double)sum / nsk - (x0 + x1) / 2.0 : 0.0;
    if (nsk < 4) {
        d = 4;
    } else if (rel > -1.0 && rel < 1.0) {
        d = 0;
    } else if (rel > -2.5 && rel < 2.5) {
        d = nsk >= 12 ? 1 : 3;
    } else {
        d = 2;
    }
    f->dir = d;
    f->flip = rel > 0;
    f->bottom = bottom;
    f->x2 = x0 + x1;
    return 1;
}

/* ---- the references ---- */

/* The first frame of animation 1's script for each direction. */
static s32 gp_idle_frame(const u8 *data, s32 table, s32 dir) {
    const s16 *e = (const s16 *)gp_sub(data, table);

    for (; e[0] != 0; e += 6) {
        if (e[0] == 1) {
            return ((const s32 *)gp_sub(data, e[1 + dir]))[1];
        }
    }
    return -1;
}

static u8 *gp_read_file(s32 id) {
    s32 sectors = filetable_funcs.get_sectors(id), i;
    u32 lba = filetable_funcs.get_lba(id);
    u8 *buf = malloc((size_t)sectors * 2048);

    if (buf == NULL) {
        return NULL;
    }
    for (i = 0; i < sectors; i++) {
        if (!port_disc_read_user(lba + (u32)i, buf + i * 2048)) {
            free(buf);
            return NULL;
        }
    }
    return buf;
}

static int gp_read_refs(const GpWho *who, GpRefs *r) {
    u8 *data = gp_read_file(who->file);
    s32 d, y, x;
    u16 *clut;
    u8 *pixels;
    GpFound f;

    if (data == NULL) {
        return 0;
    }
    for (d = 0; d < 5; d++) {
        GpHead *h = &r->head[d];
        s32 frame = gp_idle_frame(data, who->table, d);

        if (frame < 0 || !gp_tim(data, frame, &clut, &pixels, &gp_w, &gp_h)) {
            free(data);
            return 0;
        }
        memcpy(r->clut, clut, sizeof(r->clut)); /* every frame of a file has the same CLUT; the last one is kept */
        gp_unpack(pixels);
        if (!gp_find_head(who, &f)) {
            free(data);
            return 0;
        }
        h->n = 0;
        for (y = 0; y < gp_h; y++) {
            for (x = 0; x < gp_w; x++) {
                if (gp_mark[y][x] && h->n < (s32)(sizeof(h->px) / sizeof(h->px[0]))) {
                    h->px[h->n].y = (s8)y;
                    h->px[h->n].x = (s8)x;
                    h->px[h->n].v = gp_img[y][x];
                    h->n++;
                }
            }
        }
        h->bottom = f.bottom;
        h->x2 = f.x2;
        if (who == &gp_kaili && d == 0) {
            const s32 *sc = (const s32 *)gp_sub(data, ((const s16 *)gp_sub(data, who->table))[1]);

            memcpy(gp_ui_kaili, gp_img, sizeof(gp_img));
            gp_ui_kw = gp_w;
            gp_ui_kh = gp_h;
            gp_ui_dx = sc[2];
            gp_ui_dy = sc[3];
        }
    }
    if (who == &gp_junior && gp_tim(data, GP_UI_FRAME, &clut, &pixels, &gp_w, &gp_h)) {
        gp_unpack(pixels);
        memcpy(gp_ui_junior, gp_img, sizeof(gp_img));
        memcpy(gp_ui_jclut, clut, sizeof(gp_ui_jclut));
        gp_ui_jw = gp_w;
        gp_ui_jh = gp_h;
    }
    free(data);
    return 1;
}

static int gp_refs(void) {
    if (gp_refs_state == 0) {
        gp_refs_state = gp_read_refs(&gp_junior, &gp_ref_junior) && gp_read_refs(&gp_kaili, &gp_ref_kaili) ? 1 : -1;
        if (gp_refs_state < 0) {
            port_log("girls power: could not read the reference heads from the disc; sprites left as they are");
        } else if (getenv("DW3_GIRLS_DUMP") != NULL) {
            gp_dump(getenv("DW3_GIRLS_DUMP"));
        }
    }
    return gp_refs_state > 0;
}

/* Python's round(): halves to even, as the prototype placed the heads. */
static s32 gp_round_half_even(s32 twice) {
    s32 q = twice / 2, r = twice % 2;

    if (r == 0) {
        return q;
    }
    if (twice > 0) {
        return (q & 1) ? q + 1 : q;
    }
    return (q & 1) ? q - 1 : q;
}

/* ---- one frame, one file ---- */

static void gp_frame(u8 *data, s32 i, const GpWho *src, const GpRefs *dst, const u8 *map) {
    u16 *clut;
    u8 *pixels;
    s32 y, x, k;
    GpFound f;

    if (!gp_tim(data, i, &clut, &pixels, &gp_w, &gp_h)) {
        return;
    }
    gp_unpack(pixels);
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            gp_out[y][x] = gp_img[y][x] ? map[gp_img[y][x]] : 0;
        }
    }
    if (gp_find_head(src, &f)) {
        const GpHead *h = &dst->head[f.dir];
        s32 sh = gp_round_half_even(f.x2 - h->x2); /* (hx - khx), both centres being halves of x0 + x1 */

        for (y = 0; y < gp_h; y++) {
            for (x = 0; x < gp_w; x++) {
                if (gp_mark[y][x]) {
                    gp_out[y][x] = 0;
                }
            }
        }
        for (k = 0; k < h->n; k++) {
            s32 hx = f.flip ? h->x2 - h->px[k].x : h->px[k].x;
            s32 yy = h->px[k].y - h->bottom + f.bottom, xx = hx + sh;

            if (yy >= 0 && yy < gp_h && xx >= 0 && xx < gp_w) {
                gp_out[yy][xx] = h->px[k].v;
            }
        }
    }
    gp_pack(pixels);
    memcpy(clut, dst->clut, sizeof(dst->clut));
}

/* Repaints every frame the animation table's scripts name, once each. */
static void gp_file(u8 *data, s32 table, const GpWho *src, const GpRefs *dst, const u8 *map) {
    static u8 done[1024];
    s32 n = gp_count(data), k, e;
    const s16 *a;

    if (n <= 0 || n > (s32)sizeof(done) || table >= n) {
        return;
    }
    memset(done, 0, sizeof(done));
    for (a = (const s16 *)gp_sub(data, table); a[0] != 0; a += 6) {
        for (k = 0; k < 5; k++) {
            const s32 *s;

            if (a[1 + k] < 0 || a[1 + k] >= n) {
                continue;
            }
            s = (const s32 *)gp_sub(data, a[1 + k]);
            for (e = 0; e < 256 && s[0] > 0; e++, s += 4) {
                if (s[1] >= 0 && s[1] < n && !done[s[1]]) {
                    done[s[1]] = 1;
                    gp_frame(data, s[1], src, dst, map);
                }
            }
        }
    }
}

static const struct {
    s32 id, table, to_kaili;
} gp_files[] = {
        { 0x186, 0xCA, 1 }, /* P001PLAY: Junior in the scenes */
        { 0x187, 0x70, 1 }, /* P002PLAY: Junior walking */
        { 0x3C9, 0x0B, 1 }, /* P002PLYD */
        { 0x3CA, 0x64, 1 }, /* P002PLYF: fishing */
        { 0x3CB, 0x1E, 1 }, /* P002PLYK: the kick */
        { 0x3CC, 0x17, 1 }, /* P002PLYL */
        { 0x190, 0xA8, 0 }, /* P011MINO: Kaili */
    { 0x2F8, 0x26, 0 }, /* P012MINT: Kaili, the short set */
};

static void gp_apply(u32 i, u8 *data) {
    if (gp_files[i].to_kaili) {
        gp_file(data, gp_files[i].table, &gp_junior, &gp_ref_kaili, gp_j2k);
    } else {
        gp_file(data, gp_files[i].table, &gp_kaili, &gp_ref_junior, gp_k2j);
    }
}

/* DW3_GIRLS_DUMP=DIR: every file of the table read from the disc, repainted and written to DIR/<id>.bin (a check of
 * this code against the prototype, tests/port/girls_power.py); once, with the references. */
static void gp_dump(const char *dir) {
    u32 i;

    for (i = 0; i < sizeof(gp_files) / sizeof(gp_files[0]); i++) {
        u8 *data = gp_read_file(gp_files[i].id);
        char path[1024];
        FILE *f;

        if (data == NULL) {
            continue;
        }
        gp_apply(i, data);
        snprintf(path, sizeof(path), "%s/%03X.bin", dir, (unsigned)gp_files[i].id);
        f = fopen(path, "wb");
        if (f != NULL) {
            fwrite(data, 2048, (size_t)filetable_funcs.get_sectors(gp_files[i].id), f);
            fclose(f);
        }
        free(data);
    }
}

void port_girls_loaded(s32 id, u8 *data) {
    u32 i;

    for (i = 0; i < sizeof(gp_files) / sizeof(gp_files[0]); i++) {
        if (gp_files[i].id == id) {
            break;
        }
    }
    if (i == sizeof(gp_files) / sizeof(gp_files[0]) || data == NULL) {
        return;
    }
    gp_painted = gp_on();
    if (!gp_painted || !gp_refs()) {
        return;
    }
    gp_apply(i, data);
}

/* tim_load_all, before a TIM goes to VRAM (unpacked, or the file's own bytes): the VRAM page holding Junior's front
 * frame and his CLUT gets Kaili's. Found by content (the exact CLUT and the frame's every pixel), so a page that was
 * already changed is left alone. */
void port_girls_tim(u8 *tim) {
    u32 flags;
    u8 *q, *px;
    s32 w2, h, wb, x, y, ox, oy, k, found = 0, clut_at = -1;

    if (!gp_on() || !gp_refs() || gp_ui_jw == 0 || gp_ui_kw == 0) {
        return;
    }
    flags = ((u32 *)tim)[1];
    if (flags != 2) { /* the pages are plain 16-bit images holding 4-bit pixels and CLUT rows */
        return;
    }
    q = tim + 8;
    w2 = ((u16 *)(q + 8))[0];
    h = ((u16 *)(q + 10))[0];
    px = q + 12;
    wb = w2 * 2; /* bytes per row; 4-bit pixels: wb * 2 per row */
    /* The CLUT and the frame may be in different pages of the list (NAMEENT: the CLUT rows in the first, the frame in
     * the second), so each is changed wherever it is found. */
    for (k = 0; k + 32 <= wb * h; k += 2) {
        if (memcmp(px + k, gp_ui_jclut, 32) == 0) {
            clut_at = k;
            memcpy(px + k, gp_ref_kaili.clut, 32);
        }
    }
    (void)clut_at;
#define GP_PX(yy, xx) ((px[(yy) * wb + (xx) / 2] >> (((xx) & 1) ? 4 : 0)) & 15)
    for (oy = 0; oy + gp_ui_jh <= h && !found; oy++) {
        for (ox = 0; ox + gp_ui_jw <= wb * 2 && !found; ox++) {
            found = 1;
            for (y = 0; y < gp_ui_jh && found; y++) {
                for (x = 0; x < gp_ui_jw; x++) {
                    if (gp_ui_junior[y][x] && GP_PX(oy + y, ox + x) != gp_ui_junior[y][x]) {
                        found = 0;
                        break;
                    }
                }
            }
            if (found) {
                for (y = 0; y < gp_ui_jh; y++) {
                    for (x = 0; x < gp_ui_jw; x++) {
                        /* her frame's script offsets against frame 85's (-16, -28): where it sits in his slot */
                        s32 ky = y - (gp_ui_dy + 28), kx = x - (gp_ui_dx + 16);
                        u8 v = (ky >= 0 && ky < gp_ui_kh && kx >= 0 && kx < gp_ui_kw) ? gp_ui_kaili[ky][kx] : 0;
                        u8 *b = &px[(oy + y) * wb + (ox + x) / 2];

                        *b = (x + ox) & 1 ? (u8)((*b & 0x0F) | (v << 4)) : (u8)((*b & 0xF0) | v);
                    }
                }
            }
        }
    }
#undef GP_PX
}
