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
 * Finding the head: each person's own idle heads (animation 1, directions 0-4, read from the disc itself, so the
 * repainted file never feeds its own references) are matched against the frame, mirrored too, the head whole inside
 * it; the placement with the most equal pixels says where the head is. Its direction is the frame's slot in the
 * animation table when a script only one slot uses names the frame (the table's five slots are the five directions),
 * else the matched head's. When at least half of the matched head's pixels are equal the other's head of that
 * direction goes exactly there, so it moves as the original did (guessing the direction from the face's skin, as this
 * first did, turned the head from frame to frame of one animation); else (tilted and lying heads) it goes on the chin
 * found under the cap (Junior) or hair (Kaili). What is removed: the matched head's pixels, the cap/hair blobs under
 * it down to the chin (the last row with skin), stray hair beside it (the pigtails), and the small pieces that
 * removal cuts loose from the body. The two heads are put face on face: a head's axis is the mean x of its skin.
 *
 * The animations both have for the same thing (1 standing, 4 walking, 7 talking, 9 the start, 12 the nod, 51 the hop)
 * get no head swap: the frame takes the other's own frame of the same animation and direction, read from the disc
 * (hers from P011MINO, his from P001PLAY, whose frames are 24 wide as most of hers), and the script entries naming it
 * move so it lands where the other's own script draws it. 57 and 58 they share by number only (another gesture,
 * another way faced), and the rest only one has: those keep the head swap.
 *
 * Prototyped in Python over the disc's frames (tools/girls_power_preview.py); this is the same algorithm.
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
    /* the cap/hair seen from above (a head covering the body, lying): its colours, and what each becomes in the
     * other's palette */
    u16 top;
    u8 top_to[16];
    /* hair hanging below the chin, and the outline inside it: what each becomes (the other's clothes); 0, none */
    u8 hang_to[16];
} GpWho;

static const GpWho gp_junior = { 0x187, 0x70, (1 << 2) | (1 << 3) | (1 << 7), 1 << 7, (1 << 8) | (1 << 9), 0, 16,
                                 (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7), { [3] = 4, [7] = 6, [2] = 14, [6] = 14 },
                                 { 0 } };
static const GpWho gp_kaili = { 0x190, 0xA8, (1 << 4) | (1 << 6) | (1 << 14), 1 << 4, (1 << 1) | (1 << 2), 1 << 7, 15,
                                (1 << 4) | (1 << 6) | (1 << 14), { [4] = 3, [6] = 7, [14] = 2 },
                                { [4] = 3, [6] = 2, [14] = 2, [7] = 2 } };

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
    s32 x2;     /* twice the x of its axis: the face's skin, or the cap/hair blob's centre when no face shows */
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
static u8 gp_over[GP_MAX][GP_MAX]; /* where the matched head is */
static s16 gp_stack[GP_MAX * GP_MAX][2];
static s16 gp_part[GP_MAX * GP_MAX][2];

typedef struct GpFound {
    s32 bottom, x2;
} GpFound;

/* Marks in gp_mark (1) the blobs of `colours` in gp_img with at least half their pixels in gp_over (a band can split a
 * cap in two), or else the one with the most there; with no `over`, the blob with the most `key` pixels. The first
 * wins a tie. Returns how many pixels were marked, and their top row and columns. */
static s32 gp_blob(u16 colours, u16 key, int over, s32 *top, s32 *left, s32 *right) {
    static u16 seen[GP_MAX][GP_MAX];
    static u8 keep[GP_MAX * GP_MAX + 1];
    s32 y, x, best_k = 0, best_label = 0, label = 0, any = 0, size = 0;

    *top = GP_MAX;
    *left = GP_MAX;
    *right = -1;
    memset(seen, 0, sizeof(seen));
    memset(gp_mark, 0, sizeof(gp_mark));
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            s32 sp = 0, k = 0, n = 0;

            if (seen[y][x] || !GP_IN(colours, gp_img[y][x])) {
                continue;
            }
            label++;
            seen[y][x] = (u16)label;
            gp_stack[sp][0] = (s16)y;
            gp_stack[sp][1] = (s16)x;
            sp++;
            while (sp > 0) {
                s32 a, b, j;
                static const s32 dy[4] = { 1, -1, 0, 0 }, dx[4] = { 0, 0, 1, -1 };

                sp--;
                a = gp_stack[sp][0];
                b = gp_stack[sp][1];
                n++;
                k += over ? gp_over[a][b] : GP_IN(key, gp_img[a][b]);
                for (j = 0; j < 4; j++) {
                    s32 c = a + dy[j], e = b + dx[j];

                    if (c >= 0 && c < gp_h && e >= 0 && e < gp_w && !seen[c][e] && GP_IN(colours, gp_img[c][e])) {
                        seen[c][e] = (u16)label;
                        gp_stack[sp][0] = (s16)c;
                        gp_stack[sp][1] = (s16)e;
                        sp++;
                    }
                }
            }
            keep[label] = (u8)(over && k * 2 >= n);
            any |= keep[label];
            if (k > best_k) {
                best_k = k;
                best_label = label;
            }
        }
    }
    if (!any) {
        if (best_label == 0) {
            return 0;
        }
        keep[best_label] = 1;
    }
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            if (seen[y][x] && keep[seen[y][x]]) {
                gp_mark[y][x] = 1;
                size++;
                *top = y < *top ? y : *top;
                *left = x < *left ? x : *left;
                *right = x > *right ? x : *right;
            }
        }
    }
    return size;
}

/* Marks the head of gp_img in gp_mark (non-zero; bit 0: the cap/hair blobs, gp_blob's) and says where its chin and
 * axis are; 0 when there is none. `over`: the cap/hair is looked for where the matched head is (gp_over); without
 * it (the idle frames the reference heads are cut from), by its key colour. */
static int gp_find_head(const GpWho *who, int over, GpFound *f) {
    s32 y, x, y0, x0, x1;
    s32 bottom, nsk = 0, sum = 0, rows_end, cols0, cols1;

    if (gp_blob(who->cap, who->key, over, &y0, &x0, &x1) < 6) {
        memset(gp_mark, 0, sizeof(gp_mark));
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
                gp_mark[y][x] |= 8;
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
    f->bottom = bottom;
    f->x2 = nsk >= 4 ? (4 * sum + nsk) / (2 * nsk) : x0 + x1; /* twice the mean, rounded */
    return 1;
}

typedef struct GpMatch {
    s32 n, total; /* pixels equal, of how many */
    s32 dir, flip, dy, dx;
} GpMatch;

/* The best placement in gp_img of one of the person's own idle heads (only direction `hint` when it is not negative),
 * mirrored about its axis too, the head whole inside the frame: the first with the most equal pixels in (direction,
 * mirror, dy, dx) order. 0 when no placement has an equal pixel. */
static int gp_match(const GpRefs *own, s32 hint, GpMatch *m) {
    s32 d, flip, k, dy, dx;

    memset(m, 0, sizeof(*m));
    for (d = hint < 0 ? 0 : hint; d < (hint < 0 ? 5 : hint + 1); d++) {
        const GpHead *h = &own->head[d];

        for (flip = 0; flip < 2; flip++) {
            s32 y0 = GP_MAX, y1 = -GP_MAX, x0 = 4 * GP_MAX, x1 = -4 * GP_MAX;

            for (k = 0; k < h->n; k++) {
                s32 x = flip ? h->x2 - h->px[k].x : h->px[k].x, y = h->px[k].y;

                y0 = y < y0 ? y : y0;
                y1 = y > y1 ? y : y1;
                x0 = x < x0 ? x : x0;
                x1 = x > x1 ? x : x1;
            }
            for (dy = -y0; dy < gp_h - y1; dy++) {
                for (dx = -x0; dx < gp_w - x1; dx++) {
                    s32 n = 0;

                    for (k = 0; k < h->n; k++) {
                        s32 x = flip ? h->x2 - h->px[k].x : h->px[k].x;

                        n += gp_img[h->px[k].y + dy][x + dx] == h->px[k].v;
                    }
                    if (n > m->n) {
                        m->n = n;
                        m->total = h->n;
                        m->dir = d;
                        m->flip = flip;
                        m->dy = dy;
                        m->dx = dx;
                    }
                }
            }
        }
    }
    return m->n > 0;
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
        if (!gp_find_head(who, 0, &f)) {
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

/* The files the whole frames come from, as on the disc (kept: every load of a file repaints from them). */
#define GP_SRC_KAILI 0x190  /* P011MINO, table 0xA8 */
#define GP_SRC_JUNIOR 0x186 /* P001PLAY, table 0xCA */
static u8 *gp_src_kaili, *gp_src_junior;

static int gp_refs(void) {
    if (gp_refs_state == 0) {
        gp_refs_state = gp_read_refs(&gp_junior, &gp_ref_junior) && gp_read_refs(&gp_kaili, &gp_ref_kaili) ? 1 : -1;
        if (gp_refs_state > 0) {
            gp_src_kaili = gp_read_file(GP_SRC_KAILI);
            gp_src_junior = gp_read_file(GP_SRC_JUNIOR);
            if (gp_src_kaili == NULL || gp_src_junior == NULL) {
                gp_refs_state = -1;
            }
        }
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

#define GP_LEFTOVER 8 /* the biggest piece cut loose by the head's removal that is dropped with it */

/* What the removal cut loose from the body (a pigtail's tip, a cap's corner) goes with the head: every piece of at
 * most GP_LEFTOVER pixels touching it, corners included. */
static void gp_mark_leftovers(void) {
    static u8 seen[GP_MAX][GP_MAX];
    s32 y, x;

    memset(seen, 0, sizeof(seen));
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            s32 sp = 0, n = 0, touch = 0, k;

            if (!gp_img[y][x] || gp_mark[y][x] || seen[y][x]) {
                continue;
            }
            seen[y][x] = 1;
            gp_stack[sp][0] = (s16)y;
            gp_stack[sp][1] = (s16)x;
            sp++;
            while (sp > 0) {
                s32 a, b, c, e;

                sp--;
                a = gp_stack[sp][0];
                b = gp_stack[sp][1];
                gp_part[n][0] = (s16)a;
                gp_part[n][1] = (s16)b;
                n++;
                for (c = a - 1; c <= a + 1; c++) {
                    for (e = b - 1; e <= b + 1; e++) {
                        if (c < 0 || c >= gp_h || e < 0 || e >= gp_w) {
                            continue;
                        }
                        if (gp_mark[c][e]) {
                            touch = 1;
                        } else if ((c == a) != (e == b) && gp_img[c][e] && !seen[c][e]) {
                            seen[c][e] = 1;
                            gp_stack[sp][0] = (s16)c;
                            gp_stack[sp][1] = (s16)e;
                            sp++;
                        }
                    }
                }
            }
            if (touch && n <= GP_LEFTOVER) {
                for (k = 0; k < n; k++) {
                    gp_mark[gp_part[k][0]][gp_part[k][1]] = 1;
                }
            }
        }
    }
}

/* Hair hanging below the chin over the back (body on both sides of it in its row) is not removed, which would leave a
 * hole: it becomes the other's clothes, with the outline pixels inside it (no neighbour of the four empty, two or
 * more of them that hair). */
static void gp_hang(const GpWho *src, s32 bottom) {
    static u8 low[GP_MAX][GP_MAX];
    static const s32 dy[4] = { 1, -1, 0, 0 }, dx[4] = { 0, 0, 1, -1 };
    s32 y, x, k;

    memset(low, 0, sizeof(low));
    for (y = bottom + 1; y < gp_h; y++) {
        s32 first = -1, last = -1; /* the row's body: what is not the head */

        for (x = 0; x < gp_w; x++) {
            if (gp_img[y][x] && !gp_mark[y][x]) {
                first = first < 0 ? x : first;
                last = x;
            }
        }
        for (x = first + 1; first >= 0 && x < last; x++) {
            low[y][x] = gp_mark[y][x] && GP_IN(src->cap, gp_img[y][x]);
        }
    }
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            if (low[y][x]) {
                gp_mark[y][x] = 0;
                gp_out[y][x] = src->hang_to[gp_img[y][x]];
            }
        }
    }
    for (y = 1; y < gp_h - 1; y++) {
        for (x = 1; x < gp_w - 1; x++) {
            s32 full = 1, hair = 0;

            if (!GP_IN(src->outline, gp_img[y][x]) || gp_mark[y][x] || low[y][x]) {
                continue;
            }
            for (k = 0; k < 4; k++) {
                full &= gp_img[y + dy[k]][x + dx[k]] != 0;
                hair += low[y + dy[k]][x + dx[k]];
            }
            if (full && hair >= 2) {
                gp_out[y][x] = src->hang_to[gp_img[y][x]];
            }
        }
    }
}

/* `hint`: the frame's direction by the animation table, or -1. */
static void gp_frame(u8 *data, s32 i, const GpWho *src, const GpRefs *own, const GpRefs *dst, const u8 *map,
                     s32 hint) {
    u16 *clut;
    u8 *pixels;
    s32 y, x, k;
    GpFound f;
    GpMatch m;

    if (!gp_tim(data, i, &clut, &pixels, &gp_w, &gp_h)) {
        return;
    }
    gp_unpack(pixels);
    for (y = 0; y < gp_h; y++) {
        for (x = 0; x < gp_w; x++) {
            gp_out[y][x] = gp_img[y][x] ? map[gp_img[y][x]] : 0;
        }
    }
    if (gp_match(own, hint, &m)) {
        const GpHead *o = &own->head[m.dir], *h = &dst->head[m.dir];
        s32 found, sh;

        memset(gp_over, 0, sizeof(gp_over));
        for (k = 0; k < o->n; k++) {
            gp_over[o->px[k].y + m.dy][(m.flip ? o->x2 - o->px[k].x : o->px[k].x) + m.dx] = 1;
        }
        found = gp_find_head(src, 1, &f);
        if (m.n * 2 >= m.total) {
            /* the matched head is the head: the other's goes exactly where it is */
            for (y = 0; y < gp_h; y++) {
                for (x = 0; x < gp_w; x++) {
                    gp_mark[y][x] |= gp_over[y][x];
                }
            }
            f.bottom = o->bottom + m.dy;
            f.x2 = o->x2 + 2 * m.dx;
            found = 1;
        } else if (hint >= 0) {
            m.flip = 0; /* too poor a match to say it is mirrored; the table's frames are not */
        }
        if (found) {
            s32 head = 0, all = 0;

            for (y = 0; y < gp_h; y++) {
                for (x = 0; x < gp_w; x++) {
                    all += gp_img[y][x] != 0;
                    head += gp_img[y][x] && gp_mark[y][x];
                }
            }
            if (4 * head >= 3 * all) {
                /* A head with no body left under it (three quarters of the frame) is one seen from above, lying: no
                 * head of the five fits, so the cap/hair is dyed the other's instead. */
                s32 t, l, r;

                gp_blob(src->top, src->key, 0, &t, &l, &r);
                for (y = 0; y < gp_h; y++) {
                    for (x = 0; x < gp_w; x++) {
                        if (gp_mark[y][x]) {
                            gp_out[y][x] = src->top_to[gp_img[y][x]];
                        }
                    }
                }
                found = 0;
            }
        }
        if (found) {
            gp_mark_leftovers();
            if (src->hang_to[7]) { /* only her hair hangs; his cap does not */
                gp_hang(src, f.bottom);
            }
            sh = gp_round_half_even(f.x2 - h->x2); /* half the difference of the two axes, both kept doubled */
            for (y = 0; y < gp_h; y++) {
                for (x = 0; x < gp_w; x++) {
                    if (gp_mark[y][x]) {
                        gp_out[y][x] = 0;
                    }
                }
            }
            for (k = 0; k < h->n; k++) {
                s32 hx = m.flip ? h->x2 - h->px[k].x : h->px[k].x;
                s32 yy = h->px[k].y - h->bottom + f.bottom, xx = hx + sh;

                if (yy >= 0 && yy < gp_h && xx >= 0 && xx < gp_w) {
                    gp_out[yy][xx] = h->px[k].v;
                }
            }
        }
    }
    gp_pack(pixels);
    memcpy(clut, dst->clut, sizeof(dst->clut));
}

/* ---- whole frames ---- */

#define GP_FRAMES 1024 /* the most sub-files a sprite file may have */

/* The frames that take the other's own frame: its sub-file in the source (-1: none, the head is swapped), where it is
 * drawn in the frame, and how far the script entries naming the frame move. */
static s16 gp_whole_src[GP_FRAMES];
static s8 gp_whole_tx[GP_FRAMES], gp_whole_ty[GP_FRAMES], gp_whole_dx[GP_FRAMES], gp_whole_dy[GP_FRAMES];

static const s16 *gp_anim(const u8 *data, s32 table, s32 id) {
    const s16 *a;

    for (a = (const s16 *)gp_sub(data, table); a[0] != 0; a += 6) {
        if (a[0] == id) {
            return a;
        }
    }
    return NULL;
}

/* Plans gp_whole_*. A script of `data` takes the source's script of the same animation and direction: its frames in
 * order of first use take the source's in order of first use (the last one again when the source has fewer). The
 * direction is the script's slot, or, for a script several slots share (drawn one way whatever the slot), the way its
 * first frame's head faces: the source may have that way drawn in its own slot. A script is taken whole or not at
 * all: every frame must have one source frame (also against the scripts taken before) whose drawing fits it.
 * `hint`: the frames' directions by the table (gp_file's). */
static void gp_whole_plan(u8 *data, s32 table, u8 *sd, s32 stable, const GpRefs *own, const s8 *hint) {
    static const s32 ids[] = { 1, 4, 7, 9, 12, 51 };
    static struct {
        s32 f, x, y;
    } first[256], sfirst[256];
    static struct {
        s32 f, sf, tx, ty, dx, dy;
    } trial[256];
    s32 n = gp_count(data), sn, k, e, j;
    u32 i;
    const s16 *a;

    memset(gp_whole_src, -1, sizeof(gp_whole_src));
    if (sd == NULL) {
        return;
    }
    sn = gp_count(sd);
    if (stable >= sn) {
        return;
    }
    for (a = (const s16 *)gp_sub(data, table); a[0] != 0; a += 6) {
        const s16 *sa = NULL;

        for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
            if (a[0] == ids[i]) {
                sa = gp_anim(sd, stable, a[0]);
            }
        }
        if (sa == NULL) {
            continue;
        }
        for (k = 0; k < 5; k++) {
            const s32 *d, *s;
            s32 nf = 0, ns = 0, nt = 0, ok = 1, uses = 0, before = 0, way = k, w, h;
            u16 *clut;
            u8 *px;

            if (a[1 + k] < 0 || a[1 + k] >= n) {
                continue;
            }
            for (j = 0; j < 5; j++) {
                uses += a[1 + j] == a[1 + k];
                before += j < k && a[1 + j] == a[1 + k];
            }
            if (before) {
                continue;
            }
            d = (const s32 *)gp_sub(data, a[1 + k]);
            for (e = 0; e < 256 && d[0] > 0; e++, d += 4) {
                for (j = 0; j < nf && first[j].f != d[1]; j++) {
                }
                if (j == nf) {
                    first[nf].f = d[1];
                    first[nf].x = d[2];
                    first[nf].y = d[3];
                    nf++;
                }
            }
            if (nf == 0 || first[0].f < 0 || first[0].f >= n) {
                continue;
            }
            if (uses > 1) {
                GpMatch m;

                if (!gp_tim(data, first[0].f, &clut, &px, &gp_w, &gp_h)) {
                    continue;
                }
                gp_unpack(px);
                if (!gp_match(own, hint[first[0].f], &m)) {
                    continue;
                }
                way = m.dir;
            }
            if (sa[1 + way] < 0 || sa[1 + way] >= sn) {
                continue;
            }
            s = (const s32 *)gp_sub(sd, sa[1 + way]);
            for (e = 0; e < 256 && s[0] > 0; e++, s += 4) {
                for (j = 0; j < ns && sfirst[j].f != s[1]; j++) {
                }
                if (j == ns) {
                    sfirst[ns].f = s[1];
                    sfirst[ns].x = s[2];
                    sfirst[ns].y = s[3];
                    ns++;
                }
            }
            if (ns == 0) {
                continue;
            }
            for (j = 0; ok && j < nf; j++) {
                s32 f = first[j].f, q = j < ns ? j : ns - 1, sf = sfirst[q].f;
                s32 sw, sh, y, x, x0, x1, y0, y1, tx, ty;

                if (f < 0 || f >= n || sf < 0 || sf >= sn) {
                    ok = 0;
                    break;
                }
                if (gp_whole_src[f] >= 0) {
                    ok = gp_whole_src[f] == sf;
                    continue;
                }
                if (!gp_tim(data, f, &clut, &px, &w, &h) || !gp_tim(sd, sf, &clut, &px, &sw, &sh)) {
                    ok = 0;
                    break;
                }
                x0 = y0 = GP_MAX;
                x1 = y1 = -1;
                for (y = 0; y < sh; y++) {
                    for (x = 0; x < sw; x++) {
                        if ((px[y * (sw / 2) + x / 2] >> ((x & 1) ? 4 : 0)) & 15) {
                            x0 = x < x0 ? x : x0;
                            x1 = x > x1 ? x : x1;
                            y0 = y < y0 ? y : y0;
                            y1 = y > y1 ? y : y1;
                        }
                    }
                }
                if (x1 < 0 || x1 - x0 >= w || y1 - y0 >= h) {
                    ok = 0;
                    break;
                }
                /* where the frame's own offset would put it, moved only as far as the frame's sides ask */
                tx = sfirst[q].x - first[j].x;
                tx = tx < -x0 ? -x0 : tx;
                tx = tx > w - 1 - x1 ? w - 1 - x1 : tx;
                ty = sfirst[q].y - first[j].y;
                ty = ty < -y0 ? -y0 : ty;
                ty = ty > h - 1 - y1 ? h - 1 - y1 : ty;
                trial[nt].f = f;
                trial[nt].sf = sf;
                trial[nt].tx = tx;
                trial[nt].ty = ty;
                trial[nt].dx = sfirst[q].x - tx - first[j].x;
                trial[nt].dy = sfirst[q].y - ty - first[j].y;
                nt++;
            }
            if (!ok) {
                continue;
            }
            for (j = 0; j < nt; j++) {
                gp_whole_src[trial[j].f] = (s16)trial[j].sf;
                gp_whole_tx[trial[j].f] = (s8)trial[j].tx;
                gp_whole_ty[trial[j].f] = (s8)trial[j].ty;
                gp_whole_dx[trial[j].f] = (s8)trial[j].dx;
                gp_whole_dy[trial[j].f] = (s8)trial[j].dy;
            }
        }
    }
}

/* Frame `i` becomes the source's frame gp_whole_src[i]. */
static void gp_whole_frame(u8 *data, s32 i, u8 *sd, const GpRefs *dst) {
    u16 *clut;
    u8 *pixels, *spx;
    s32 y, x, sw, sh;

    if (!gp_tim(sd, gp_whole_src[i], &clut, &spx, &gp_w, &gp_h)) {
        return;
    }
    gp_unpack(spx);
    sw = gp_w;
    sh = gp_h;
    if (!gp_tim(data, i, &clut, &pixels, &gp_w, &gp_h)) {
        return;
    }
    memset(gp_out, 0, sizeof(gp_out));
    for (y = 0; y < sh; y++) {
        for (x = 0; x < sw; x++) {
            if (gp_img[y][x]) { /* the plan made it fit */
                gp_out[y + gp_whole_ty[i]][x + gp_whole_tx[i]] = gp_img[y][x];
            }
        }
    }
    gp_pack(pixels);
    memcpy(clut, dst->clut, sizeof(dst->clut));
}

/* Repaints every frame the animation table's scripts name, once each. `sd`, `stable`: the other's file and table,
 * for the whole frames (NULL: none, as for the action files only he has). */
static void gp_file(u8 *data, s32 table, const GpWho *src, const GpRefs *own, const GpRefs *dst, const u8 *map,
                    u8 *sd, s32 stable) {
    static u8 done[GP_FRAMES], moved[GP_FRAMES];
    static s8 hint[GP_FRAMES];
    s32 n = gp_count(data), k, e, j, pass;
    const s16 *a;

    if (n <= 0 || n > GP_FRAMES || table >= n) {
        return;
    }
    memset(done, 0, sizeof(done));
    memset(moved, 0, sizeof(moved));
    memset(hint, -1, sizeof(hint));
    memset(gp_whole_src, -1, sizeof(gp_whole_src));
    /* pass 0: a frame's direction is the slot of the first script naming it that only one slot of its animation uses
     * (a scene's animation often has one script in several slots, whatever way it faces); pass 1: the repaint, and
     * each script's entries moved for the whole frames (once: slots and animations share scripts) */
    for (pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            gp_whole_plan(data, table, sd, stable, own, hint); /* with the directions, before any frame changes */
        }
        for (a = (const s16 *)gp_sub(data, table); a[0] != 0; a += 6) {
            for (k = 0; k < 5; k++) {
                s32 *s;
                s32 uses = 0, move;

                if (a[1 + k] < 0 || a[1 + k] >= n) {
                    continue;
                }
                for (j = 0; j < 5; j++) {
                    uses += a[1 + j] == a[1 + k];
                }
                move = pass == 1 && !moved[a[1 + k]];
                moved[a[1 + k]] |= (u8)(pass == 1);
                s = (s32 *)gp_sub(data, a[1 + k]);
                for (e = 0; e < 256 && s[0] > 0; e++, s += 4) {
                    if (s[1] < 0 || s[1] >= n) {
                        continue;
                    }
                    if (pass == 0) {
                        if (uses == 1 && hint[s[1]] < 0) {
                            hint[s[1]] = (s8)k;
                        }
                        continue;
                    }
                    if (move && gp_whole_src[s[1]] >= 0) {
                        s[2] += gp_whole_dx[s[1]];
                        s[3] += gp_whole_dy[s[1]];
                    }
                    if (!done[s[1]]) {
                        done[s[1]] = 1;
                        if (gp_whole_src[s[1]] >= 0) {
                            gp_whole_frame(data, s[1], sd, dst);
                        } else {
                            gp_frame(data, s[1], src, own, dst, map, hint[s[1]]);
                        }
                    }
                }
            }
        }
    }
}

static const struct {
    s32 id, table, to_kaili;
    s32 whole; /* has the animations whose frames the other's file gives whole */
} gp_files[] = {
        { 0x186, 0xCA, 1, 1 }, /* P001PLAY: Junior in the scenes */
        { 0x187, 0x70, 1, 1 }, /* P002PLAY: Junior walking */
        { 0x3C9, 0x0B, 1, 0 }, /* P002PLYD */
        { 0x3CA, 0x64, 1, 0 }, /* P002PLYF: fishing */
        { 0x3CB, 0x1E, 1, 0 }, /* P002PLYK: the kick */
        { 0x3CC, 0x17, 1, 0 }, /* P002PLYL */
        { 0x190, 0xA8, 0, 1 }, /* P011MINO: Kaili */
        { 0x2F8, 0x26, 0, 1 }, /* P012MINT: Kaili, the short set */
};

static void gp_apply(u32 i, u8 *data) {
    if (gp_files[i].to_kaili) {
        gp_file(data, gp_files[i].table, &gp_junior, &gp_ref_junior, &gp_ref_kaili, gp_j2k,
                gp_files[i].whole ? gp_src_kaili : NULL, 0xA8);
    } else {
        gp_file(data, gp_files[i].table, &gp_kaili, &gp_ref_kaili, &gp_ref_junior, gp_k2j,
                gp_files[i].whole ? gp_src_junior : NULL, 0xCA);
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
