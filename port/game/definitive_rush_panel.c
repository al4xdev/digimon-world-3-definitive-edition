/* The Overlevel Rush on the field, its question and its results. With DEF_RUSH_FLOAT the Rush's leave
 * (fieldstg_rush_leave) keeps the field, gives the report's rewards at once (def_rush_apply_quiet: STFGTREP's own
 * routines in its order, so the game's state is the report's) and opens the results' panel; fieldstg_rush_frame runs
 * the question and the panel once a frame from the camera's update, whatever the player does. Neither sets
 * event_running: that is what would hold the player, who walks on under both. definitive_rush.c gives the panel's rows
 * and clock.
 *
 * Both sit at the top of the screen, centred, sliding in from above: the camera keeps the player in the middle, so a
 * panel there would cover the one walking under it (and one that followed the player would shake with every step).
 * The headings are in the callouts' polygon letters (def_poly_text: no texture, so the accents Portuguese needs and
 * any internal resolution are theirs); the rest is the game's font, a window per text (windows of no object's data
 * block: only this runs them, as on the Guardromon screen), on the Rush's own layer 0x1004 over the field's sprites,
 * with the Rush question's dark panel. In one ordering-table entry what is added last is drawn first, so the texts go
 * in before the frame and the fill.
 *
 * The windows live on the field's heap: the panel closes and frees them as soon as the field holds the player or the map
 * ends, a map's load forgets them (def_rush_panel_reset: that heap went with the last stage), and a save state keeps
 * the panel with the heap it points into (def_rush_savestate). */
#include <stdlib.h>

#include "common.h"
#include "cdload.h"
#include "fieldstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "pad.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "records.h"

#include "psxstack/savestate.h"

#include "definitive.h"

extern void *font_get_entry(void *data, s32 i);
extern void port_log(const char *fmt, ...);
extern const unsigned char def_str_RushAskYes[], def_str_RushAskNo[];

#define RP_LAYER 0x1004 /* the Rush's layer (its question and RUSH!), over the field's sprites; offset (0, 0) on the field */
#define RP_FONT 1       /* the dialogues' 14-pixel font: its button symbols, its accents, white on palette 0 */
#define RP_LINE 14      /* a row's pitch in it */
#define RP_PAD 8        /* the texts' inset from the panel's sides */
#define RP_BOTTOM 3     /* under the last row */
#define RP_GAP 12       /* the least room between a row's two texts */
#define RP_MIN_W 140
#define RP_MAX_W 312
#define RP_Y 8          /* the panels' top once slid in */
#define RP_SLIDE 10     /* ticks (1/60 s) a panel takes to slide in from above, and the results' to slide back out */
#define RP_CENTRE 160   /* the screen's middle: in a widescreen field too, which widens both sides alike */
#define RP_TITLE 192    /* the headings: 8.8 of the callouts' 16-pixel letters (12 pixels) */
#define RP_TITLE_Y 8    /* a heading's top, from its panel's: room above for its accents */
#define RP_RULE_Y 23    /* the results' line under it */
#define RP_HEAD 26      /* their first row's top */
#define RP_TEXT 96      /* bytes of a row's text */
#define RP_PAL_TEXT 0   /* names and numbers */
#define RP_PAL_UP 3     /* LV UP! and TP FULL! (the bestiary's orange) */
#define RP_PAL_NEW 4    /* NEW TECH! (its green) */
#define RP_ITEM_NAMES 0x6A /* the item names' text file (records_language + it), STFGTREP's own */

#define RA_KEYS_Y 24    /* the question: the answers' line, in the game's font */
#define RA_BAR_Y 42     /* the time left */
#define RA_BAR_H 3
#define RA_H 49
#define RA_PAD 14
#define RA_MIN_W 208

#define RP_STAMP_FRAMES 5   /* slam from 2.2x to 1.0x */
#define RP_HOLD_FRAMES 13   /* brief beat after impact before expanding */
#define RP_EXPAND_FRAMES 22 /* downward ease-out expansion finished */

/* The tests read rush_panel.clock (left, opened) first. */
static struct {
    DefRushClock clock;
    DefRushSummary summary;
    DefRushRow rows[DEF_RUSH_ROWS];
    s32 count;
    s32 w, h;
    s32 item_row; /* the item's row, still waiting for the item names' file; -1: none */
    MessageWindow *left[DEF_RUSH_ROWS], *right[DEF_RUSH_ROWS];
    s32 left_w[DEF_RUSH_ROWS], right_w[DEF_RUSH_ROWS];
    /* the question's answers: split into Yes (pulsing A chip) and No (steady B chip) */
    MessageWindow *keys;
    s32 keys_w;
    unsigned char keys_text[64];
    MessageWindow *key_yes, *key_no, *lbl_yes, *lbl_no;
    s32 lbl_yes_w, lbl_no_w;
    unsigned char sym_yes[4], sym_no[4];
    s32 ask_w;        /* width of the ask plate at confirmation, for seamless transition */
    s32 ask_bar_perm; /* 0..1000 frozen timer bar ratio at confirmation */
    s32 ask_ticks;    /* animation clock for pulsing confirm badge */
} rush_panel;

int def_rush_float_mode = -1; /* -1: DW3_RUSH_FLOAT not read yet (a debug session may set it directly) */

int def_rush_float(void) {
    if (def_rush_float_mode < 0) {
        const char *v = getenv("DW3_RUSH_FLOAT");

        def_rush_float_mode = v != NULL ? atoi(v) != 0 : DEF_RUSH_FLOAT;
        if (v != NULL) {
            port_log("rush: DW3_RUSH_FLOAT=%s: the %s", v, def_rush_float_mode ? "panel" : "results screen");
        }
    }
    return def_rush_float_mode != 0 && def_on();
}

/* The item names' file once it is in, else NULL. cdload_is_loading queues a file it does not have, and get_file would
 * read one synchronously (a stall in the middle of the field's frame): so is_loading first, get_file once it says 0. */
static void *rp_item_names(void) {
    if (cdload_module.is_loading(records_language + RP_ITEM_NAMES)) {
        return NULL;
    }
    return cdload_module.files.get_file(records_language + RP_ITEM_NAMES);
}

static void rp_stop_row(s32 i) {
    heap_objects.stop((Object *)rush_panel.left[i]);
    heap_objects.stop((Object *)rush_panel.right[i]);
    rush_panel.left[i] = rush_panel.right[i] = NULL;
    rush_panel.left_w[i] = rush_panel.right_w[i] = 0;
}

static void rp_close(void) {
    s32 i;

    for (i = 0; i < DEF_RUSH_ROWS; i++) {
        rp_stop_row(i);
    }
    rush_panel.count = 0;
    rush_panel.item_row = -1;
    rush_panel.clock.left = 0;
}

/* A text's window, placed by its right end (message_measure: the lines start at x less their width), so that a left
 * text goes at its row's start plus its width and a right one at the panel's right edge. */
static MessageWindow *rp_window(const unsigned char *text, s32 palette, s32 *width) {
    MessageWindow *w;

    *width = 0;
    if (text[0] == 0) {
        return NULL;
    }
    w = message_create_window(RP_LAYER, RP_FONT, 0, 0);
    w->set_ot_depth(w, 0);
    w->set_text(w, (u8 *)text, -1);
    w->set_palette(w, (u8)palette);
    w->measure(w, 1);
    *width = w->measured_width;
    return w;
}

static void rp_fill_row(s32 i) {
    unsigned char left[RP_TEXT], right[RP_TEXT];
    const unsigned char *item_name = NULL;
    DefRushRow row = rush_panel.rows[i];
    s32 pal = RP_PAL_TEXT;
    void *names;

    if (row.kind == DEF_RUSH_ROW_ITEM && (names = rp_item_names()) != NULL) {
        item_name = (const unsigned char *)font_get_entry(names, rush_panel.summary.item);
    }
    def_rush_row_text(&rush_panel.summary, row, item_name, left, right, RP_TEXT);
    if (row.kind == DEF_RUSH_ROW_TECH) {
        pal = RP_PAL_NEW;
    } else if (row.kind == DEF_RUSH_ROW_TP || (row.kind == DEF_RUSH_ROW_MEMBER && rush_panel.summary.members[row.index].levels)) {
        pal = RP_PAL_UP;
    }
    rp_stop_row(i);
    rush_panel.left[i] = rp_window(left, RP_PAL_TEXT, &rush_panel.left_w[i]);
    rush_panel.right[i] = rp_window(right, pal, &rush_panel.right_w[i]);
}

/* The results' heading, in the game's language. */
static const char *rp_title(void) {
    return def_pick("EASY WIN!", "VITÓRIA FÁCIL!", "VICTOIRE FACILE!", "LEICHTER SIEG!", "VITTORIA FACILE!",
                    "VICTORIA FÁCIL!");
}

static void rp_size(void) {
    s32 i, w;

    rush_panel.w = RP_MIN_W;
    w = def_poly_text_width(RP_TITLE, rp_title()) + RP_PAD * 2;
    if (w > rush_panel.w) {
        rush_panel.w = w;
    }
    for (i = 0; i < rush_panel.count; i++) {
        w = RP_PAD * 2 + rush_panel.left_w[i] + (rush_panel.right_w[i] > 0 ? RP_GAP + rush_panel.right_w[i] : 0);
        if (w > rush_panel.w) {
            rush_panel.w = w;
        }
    }
    if (rush_panel.w > RP_MAX_W) {
        rush_panel.w = RP_MAX_W;
    }
    rush_panel.h = RP_HEAD + rush_panel.count * RP_LINE + RP_BOTTOM;
}

/* A flat quad with its blend: `abr` 1 adds, 2 subtracts (the panels' frame and fill); -1 is
 * opaque. */
static void rp_quad(GfxLayer *layer, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
    POLY_F4 *poly = gfx_module.funcs.get_packet();
    DR_TPAGE *tpage = (DR_TPAGE *)(poly + 1);
    u32 *ot = layer->get_ot_entry(layer, 0);

    setPolyF4(poly);
    setSemiTrans(poly, abr >= 0);
    setRGB0(poly, r, g, b);
    setXY4(poly, x, y, x + w, y, x, y + h, x + w, y + h);
    addPrim(ot, poly);
    SetDrawTPage(tpage, 0, 1, getTPage(1, abr >= 0 ? abr : 0, 0, 0));
    addPrim(ot, tpage);
    gfx_module.funcs.set_packet(tpage + 1);
}

static void rp_slanted_quad(GfxLayer *layer, s32 cx, s32 cy, s32 hw, s32 hh, s32 skew, s32 angle, s32 abr, u8 r, u8 g,
                            u8 b) {
    POLY_F4 *poly = gfx_module.funcs.get_packet();
    u32 *ot = layer->get_ot_entry(layer, 0);
    s32 cs = rsin(angle + 1024), sn = rsin(angle);
    const s32 lx[4] = { -hw + skew, hw + skew + 2, -hw - skew - 1, hw - skew };
    const s32 ly[4] = { -hh, -hh - 1, hh + 1, hh };
    s16 v[8];
    s32 k;

    for (k = 0; k < 4; k++) {
        v[k * 2] = (s16)(cx + ((lx[k] * cs - ly[k] * sn) >> 12));
        v[k * 2 + 1] = (s16)(cy + ((lx[k] * sn + ly[k] * cs) >> 12));
    }
    setPolyF4(poly);
    setSemiTrans(poly, abr >= 0);
    setRGB0(poly, r, g, b);
    setXY4(poly, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
    addPrim(ot, poly);
    if (abr >= 0) {
        DR_TPAGE *tpage = (DR_TPAGE *)(poly + 1);
        SetDrawTPage(tpage, 0, 1, getTPage(1, abr, 0, 0));
        addPrim(ot, tpage);
        gfx_module.funcs.set_packet(tpage + 1);
    } else {
        gfx_module.funcs.set_packet(poly + 1);
    }
}

/* The P3R-styled slanted panel frame and fill (queued after what goes on it: under it). */
static void rp_box(GfxLayer *layer, s32 x, s32 y, s32 w, s32 h, s32 head_h) {
    s32 skew = 5;

    /* Bright electric cyan top edge + P3R cobalt header strip */
    rp_quad(layer, x + skew, y, w, 2, -1, 0, 196, 255);
    rp_quad(layer, x - skew, y + h - 1, w, 1, 1, 40, 118, 240);
    if (head_h > 0) {
        rp_quad(layer, x + 2, y + head_h, w - 4, 1, -1, 0, 176, 255);
        rp_quad(layer, x + 2, y + 2, w - 4, head_h - 2, -1, 8, 18, 44);
    }
    /* Main slanted dark plate + crisp outer blue trim + subtle cyan slash backing */
    rp_slanted_quad(layer, x + w / 2, y + h / 2, w / 2, h / 2, skew, 0, 2, 210, 200, 145);
    rp_slanted_quad(layer, x + w / 2, y + h / 2, w / 2 + 1, h / 2 + 1, skew, 0, 1, 36, 112, 232);
    rp_slanted_quad(layer, x + w / 2 - 4, y + h / 2 + 3, w / 2 + 2, h / 2, skew + 1, 0, 1, 0, 128, 210);
}

/* The "RUSH!" stamp slammed into the top-right corner of the panel. */
static void rp_stamp(GfxLayer *layer, s32 cx, s32 cy, s32 scale_8_8) {
    static const u8 white[3] = { 250, 252, 255 };
    static const u8 gold[3] = { 255, 216, 64 };
    static const u8 shadow[3] = { 16, 8, 24 };
    s32 angle = -168;
    s32 tw = def_poly_text_rot(NULL, 0, 0, 0, scale_8_8, angle, 0, "RUSH!", gold, NULL);
    s32 hw = tw / 2 + ((7 * scale_8_8) >> 8);
    s32 hh = ((16 * scale_8_8) >> 9) + ((4 * scale_8_8) >> 8);

    def_poly_text_rot(layer, 0, cx, cy, scale_8_8, angle, 0, "RUSH!", gold, NULL);
    def_poly_text_rot(layer, 0, cx + 1, cy + 1, scale_8_8, angle, 0, "RUSH!", shadow, NULL);
    rp_slanted_quad(layer, cx, cy, hw, hh, 3, angle, -1, 14, 24, 56);
    rp_slanted_quad(layer, cx, cy, hw + 2, hh + 2, 3, angle, -1, 0, 196, 255);
    rp_slanted_quad(layer, cx, cy, hw + 3, hh + 3, 3, angle, -1, white[0], white[1], white[2]);
}

/* A heading in the callouts' letters with the dark edge the title screen's questions have, centred on the screen. */
static void rp_heading(GfxLayer *layer, s32 y, s32 size, const char *text, const u8 *rgb) {
    static const u8 edge[3] = { 8, 12, 40 };
    s32 x = RP_CENTRE - def_poly_text_width(size, text) / 2;

    def_poly_text(layer, 0, x, y, size, text, rgb);
    def_poly_text(layer, 0, x + 1, y + 1, size, text, edge);
}

/* How far a panel `h` tall that has been up `shown` ticks has come down: from above the screen to RP_Y. */
static s32 rp_slide_y(s32 shown, s32 h) {
    if (shown >= RP_SLIDE) {
        return RP_Y;
    }
    if (shown < 0) {
        shown = 0;
    }
    return RP_Y - (h + RP_Y) * (RP_SLIDE - shown) / RP_SLIDE;
}

void def_rush_panel_open(const DefRushSummary *s) {
    s32 replaced = def_rush_clock_open(&rush_panel.clock);
    s32 i;

    for (i = 0; i < DEF_RUSH_ROWS; i++) { /* a second Rush's panel takes the place of the first */
        rp_stop_row(i);
    }
    rush_panel.summary = *s;
    rush_panel.count = def_rush_rows(&rush_panel.summary, rush_panel.rows, DEF_RUSH_ROWS);
    rush_panel.item_row = -1;
    for (i = 0; i < rush_panel.count; i++) {
        if (rush_panel.rows[i].kind == DEF_RUSH_ROW_ITEM && rp_item_names() == NULL) {
            rush_panel.item_row = i; /* the field does not keep the item names: "???" until they are in */
        }
        rp_fill_row(i);
    }
    rp_size();
    port_log("rush: floating results (random %d): battle %d, exp %d/%d/%d, levels +%d/+%d/+%d, learnt %d/%d/%d, "
             "bits %d, item %d x%d, %d material line(s); the panel, %d rows%s",
             s->random, (int)records_battle_results.battle, s->members[0].exp, s->members[1].exp, s->members[2].exp,
             s->members[0].levels, s->members[1].levels, s->members[2].levels, s->members[0].learnt,
             s->members[1].learnt, s->members[2].learnt, s->bits, s->item, s->item_count, s->materials,
             (int)rush_panel.count, replaced ? " (in place of the last one)" : "");
}

int def_rush_panel_up(void) {
    return rush_panel.clock.left > 0;
}

void def_rush_panel_frame(void) {
    static const u8 gold[3] = { 255, 208, 64 };
    Object *mgr;
    GfxLayer *layer;
    s32 held, x, y, i, ty, shown, cur_w, cur_h, jolt_y = 0, flash = 0, stamp_scale;
    s32 expand_t;

    if (rush_panel.clock.left <= 0) {
        return;
    }
    mgr = heap_objects.find(7, -1, -1);
    layer = gfx_module.funcs.get_layer(RP_LAYER);
    held = fieldstg_stage.menu_open != 0 || fieldstg_stage.event_running != 0 || fieldstg_stage.battle_starting != 0 ||
           mgr == NULL || mgr->state != OBJECT_STATE_RUN || layer == NULL;
    if (!def_rush_clock_tick(&rush_panel.clock, gfx_module.funcs.get_frame_ticks(), held)) {
        rp_close();
        return;
    }
    if (rush_panel.item_row >= 0 && rp_item_names() != NULL) {
        rp_fill_row(rush_panel.item_row);
        rush_panel.item_row = -1;
        rp_size();
    }
    shown = DEF_RUSH_FLOAT_FRAMES - rush_panel.clock.left;

    /* Fast dismiss on Confirm press once expanded, so the player keeps their field rhythm. */
    if (!held && shown >= RP_EXPAND_FRAMES && rush_panel.clock.left > RP_SLIDE) {
        u32 confirm_mask = 1u << pad_state.get_button_map(0, 13);
        if ((pad_state.get_pressed(0) & confirm_mask) != 0) {
            rush_panel.clock.left = RP_SLIDE;
        }
    }

    /* Height & width animate from the ask plate (RA_H) down into the full results plate. */
    cur_w = rush_panel.w;
    if (rush_panel.ask_w > cur_w) {
        cur_w = rush_panel.ask_w;
    }
    if (shown < RP_HOLD_FRAMES) {
        cur_h = RA_H;
        expand_t = 0;
    } else if (shown < RP_EXPAND_FRAMES) {
        s32 u = shown - RP_HOLD_FRAMES;
        s32 span = RP_EXPAND_FRAMES - RP_HOLD_FRAMES;
        expand_t = 256 - (span - u) * (span - u) * 256 / (span * span);
        cur_h = RA_H + (((rush_panel.h - RA_H) * expand_t) >> 8);
    } else {
        cur_h = rush_panel.h;
        expand_t = 256;
    }

    /* Micro-jolt & 2-frame white flash on the exact frame the stamp slams in. */
    if (shown == RP_STAMP_FRAMES) {
        jolt_y = 3;
        flash = 1;
    } else if (shown == RP_STAMP_FRAMES + 1) {
        jolt_y = 2;
        flash = 1;
    } else if (shown == RP_STAMP_FRAMES + 2) {
        jolt_y = 1;
    }

    x = RP_CENTRE - cur_w / 2;
    if (rush_panel.clock.left < RP_SLIDE) {
        y = rp_slide_y(rush_panel.clock.left, cur_h);
    } else {
        y = RP_Y + jolt_y;
    }

    /* 2-frame impact white flash over the whole plate */
    if (flash) {
        rp_slanted_quad(layer, x + cur_w / 2, y + cur_h / 2, cur_w / 2, cur_h / 2, 5, 0, 1, 200, 224, 255);
    }

    /* "RUSH!" stamp slamming from 2.2x -> 1.0x and staying pinned at the top-right corner */
    if (shown < RP_STAMP_FRAMES) {
        stamp_scale = 380 - (380 - 156) * shown / RP_STAMP_FRAMES;
    } else {
        stamp_scale = 156;
    }
    rp_stamp(layer, x + cur_w - 22, y + 11, stamp_scale);

    rp_heading(layer, y + RP_TITLE_Y, RP_TITLE, rp_title(), gold);

    if (shown < RP_HOLD_FRAMES && rush_panel.ask_bar_perm > 0) {
        /* Frozen timer bar during stamp slam + short breath */
        s32 bar = (cur_w - RA_PAD * 2) * rush_panel.ask_bar_perm / 1000;
        rp_quad(layer, x + RA_PAD, y + RA_BAR_Y, bar, RA_BAR_H, -1, 255, 208, 64);
        rp_quad(layer, x + RA_PAD, y + RA_BAR_Y, cur_w - RA_PAD * 2, RA_BAR_H, 1, 40, 48, 72);
    } else {
        for (i = 0; i < rush_panel.count; i++) {
            ty = y + RP_HEAD + i * RP_LINE;
            if (ty + RP_LINE > y + cur_h - 1) {
                continue;
            }
            if (rush_panel.left[i] != NULL) {
                rush_panel.left[i]->set_pos(rush_panel.left[i], x + RP_PAD + rush_panel.left_w[i], ty);
                rush_panel.left[i] = (MessageWindow *)heap_objects.try_run((Object *)rush_panel.left[i]);
            }
            if (rush_panel.right[i] != NULL) {
                rush_panel.right[i]->set_pos(rush_panel.right[i], x + cur_w - RP_PAD, ty);
                rush_panel.right[i] = (MessageWindow *)heap_objects.try_run((Object *)rush_panel.right[i]);
            }
        }
    }
    rp_box(layer, x, y, cur_w, cur_h, RP_RULE_Y);
}

/* The question's answers split into separate windows so the green Confirm badge pulses independently while the rest
 * stays rock-steady for instant reading. */
static void ra_keys_open(void) {
    s32 dummy;

    if (dw3_feat_icons) {
        rush_panel.sym_yes[0] = 0x0E; /* A */
        rush_panel.sym_yes[1] = 0;
        rush_panel.sym_no[0] = 0x0F; /* B */
        rush_panel.sym_no[1] = 0;
    } else {
        u8 cy = port_face_text_code(pad_state.get_button_map(0, 13));
        u8 cn = port_face_text_code(pad_state.get_button_map(0, 14));
        rush_panel.sym_yes[0] = 1;
        rush_panel.sym_yes[1] = cy != 0 ? cy : 0x32;
        rush_panel.sym_yes[2] = 0;
        rush_panel.sym_no[0] = 1;
        rush_panel.sym_no[1] = cn != 0 ? cn : 0x34;
        rush_panel.sym_no[2] = 0;
    }
    rush_panel.key_yes = rp_window(rush_panel.sym_yes, RP_PAL_TEXT, &dummy);
    rush_panel.key_no = rp_window(rush_panel.sym_no, RP_PAL_TEXT, &dummy);
    rush_panel.lbl_yes = rp_window(def_str_RushAskYes, RP_PAL_TEXT, &rush_panel.lbl_yes_w);
    rush_panel.lbl_no = rp_window(def_str_RushAskNo, RP_PAL_TEXT, &rush_panel.lbl_no_w);
    rush_panel.keys_w = 18 + rush_panel.lbl_yes_w + 26 + 18 + rush_panel.lbl_no_w;
    rush_panel.ask_ticks = 0;
}

void def_rush_ask_frame(int waited, int total) {
    static const u8 gold[3] = { 255, 208, 64 };
    GfxLayer *layer = gfx_module.funcs.get_layer(RP_LAYER);
    const char *title = def_pick("EASY FIGHT! RUSH IT?", "LUTA FÁCIL! ATROPELAR?", "COMBAT FACILE! FONCER?",
                                 "LEICHTER KAMPF! RUSH?", "SCONTRO FACILE! TRAVOLGERLO?", "COMBATE FÁCIL! ARROLLAR?");
    s32 w, x, y, left, bar, row_x, yes_cx, yes_cy, no_cx, no_cy, pulse, shake;

    if (layer == NULL) {
        return;
    }
    if (rush_panel.lbl_yes == NULL) {
        ra_keys_open();
    }
    rush_panel.ask_ticks += gfx_module.funcs.get_frame_ticks();
    w = def_poly_text_width(RP_TITLE, title) + RA_PAD * 2;
    if (w < rush_panel.keys_w + RA_PAD * 2) {
        w = rush_panel.keys_w + RA_PAD * 2;
    }
    if (w < RA_MIN_W) {
        w = RA_MIN_W;
    }
    x = RP_CENTRE - w / 2;
    y = rp_slide_y(waited, RA_H);
    left = total - waited;
    left = left < 0 ? 0 : left;
    bar = (w - RA_PAD * 2) * left / total;
    rush_panel.ask_w = w;
    rush_panel.ask_bar_perm = total > 0 ? (left * 1000 / total) : 0;

    rp_heading(layer, y + RP_TITLE_Y, RP_TITLE, title, gold);

    row_x = RP_CENTRE - rush_panel.keys_w / 2;
    pulse = (rsin((rush_panel.ask_ticks * 120) & 0xFFF) + 4096) >> 12; /* 0..1 gentle pop */
    shake = ((rush_panel.ask_ticks % 28) < 4) ? ((rush_panel.ask_ticks & 1) ? 1 : -1) : 0;
    yes_cx = row_x + 7 + shake;
    yes_cy = y + RA_KEYS_Y + 6;
    no_cx = row_x + 18 + rush_panel.lbl_yes_w + 26 + 7;
    no_cy = y + RA_KEYS_Y + 6;

    if (rush_panel.key_yes != NULL) {
        rush_panel.key_yes->set_pos(rush_panel.key_yes, yes_cx + (dw3_feat_icons ? 4 : 6), yes_cy - 6);
        rush_panel.key_yes = (MessageWindow *)heap_objects.try_run((Object *)rush_panel.key_yes);
    }
    if (rush_panel.lbl_yes != NULL) {
        rush_panel.lbl_yes->set_pos(rush_panel.lbl_yes, row_x + 18 + rush_panel.lbl_yes_w, y + RA_KEYS_Y);
        rush_panel.lbl_yes = (MessageWindow *)heap_objects.try_run((Object *)rush_panel.lbl_yes);
    }
    if (rush_panel.key_no != NULL) {
        rush_panel.key_no->set_pos(rush_panel.key_no, no_cx + (dw3_feat_icons ? 4 : 6), no_cy - 6);
        rush_panel.key_no = (MessageWindow *)heap_objects.try_run((Object *)rush_panel.key_no);
    }
    if (rush_panel.lbl_no != NULL) {
        rush_panel.lbl_no->set_pos(rush_panel.lbl_no, row_x + 18 + rush_panel.lbl_yes_w + 26 + 18 + rush_panel.lbl_no_w,
                                   y + RA_KEYS_Y);
        rush_panel.lbl_no = (MessageWindow *)heap_objects.try_run((Object *)rush_panel.lbl_no);
    }

    /* Pulsing/shivering green A (or blue X) badge calling the click + steady red B badge */
    rp_slanted_quad(layer, yes_cx, yes_cy, 6 + pulse, 6 + pulse, 2, -64, -1, (u8)(dw3_feat_icons ? 28 : 44),
                    (u8)(dw3_feat_icons ? 196 : 144), (u8)(dw3_feat_icons ? 76 : 255));
    rp_slanted_quad(layer, yes_cx, yes_cy, 7 + pulse, 7 + pulse, 2, -64, -1, 246, 252, 255);
    rp_slanted_quad(layer, no_cx, no_cy, 6, 6, 2, -48, -1, 224, 46, 52);
    rp_slanted_quad(layer, no_cx, no_cy, 7, 7, 2, -48, -1, 246, 252, 255);

    /* the time left, gold turning red in its last third, over its empty track */
    if (left * 3 > total) {
        rp_quad(layer, x + RA_PAD, y + RA_BAR_Y, bar, RA_BAR_H, -1, 255, 208, 64);
    } else {
        rp_quad(layer, x + RA_PAD, y + RA_BAR_Y, bar, RA_BAR_H, -1, 255, 88, 56);
    }
    rp_quad(layer, x + RA_PAD, y + RA_BAR_Y, w - RA_PAD * 2, RA_BAR_H, 1, 40, 48, 72);
    rp_box(layer, x, y, w, RA_H, RP_RULE_Y - 2);
}

void def_rush_ask_close(void) {
    heap_objects.stop((Object *)rush_panel.keys);
    heap_objects.stop((Object *)rush_panel.key_yes);
    heap_objects.stop((Object *)rush_panel.key_no);
    heap_objects.stop((Object *)rush_panel.lbl_yes);
    heap_objects.stop((Object *)rush_panel.lbl_no);
    rush_panel.keys = NULL;
    rush_panel.key_yes = rush_panel.key_no = NULL;
    rush_panel.lbl_yes = rush_panel.lbl_no = NULL;
    rush_panel.keys_w = 0;
}

void def_rush_panel_reset(void) {
    s32 i;

    for (i = 0; i < DEF_RUSH_ROWS; i++) {
        rush_panel.left[i] = rush_panel.right[i] = NULL;
    }
    rush_panel.count = 0;
    rush_panel.item_row = -1;
    rush_panel.clock.left = 0;
    rush_panel.keys = NULL;
    rush_panel.key_yes = rush_panel.key_no = NULL;
    rush_panel.lbl_yes = rush_panel.lbl_no = NULL;
    rush_panel.ask_w = 0;
    rush_panel.ask_bar_perm = 0;
    rush_panel.ask_ticks = 0;
}

void def_rush_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, rush_panel);
}
