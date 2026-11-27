/* The Black Market's trader and its screen (the rules are definitive_market.c).
 *
 * The trader is one placed actor of the field (traders below: a map and the actor's FieldstgPlacedActor.id, which is
 * its object's key1), a figure of a secondary area chosen with tools/bm_npc_candidates.py. Talking to it does not
 * start its own talk: FIELDSTG's talk asks def_bm_talk first, and when that takes it the screen opens on the field's
 * next idle frame (def_bm_frame, from fieldstg_manager_update's idle step, after the Guardromon's dw3_travel_frame).
 *
 * The screen is a field overlay drawn as the Guardromon network's (definitive_travel.c): the bestiary's panels in
 * polygons, the game's font in message windows that only this file runs, fieldstg_stage.event_running held while it is
 * up (that freezes the player, the map's events and the encounters). Two tabs, L1/R1 (or left/right) to switch:
 *  - Exchange: the rare items held, one row each ("Datamon IV x2"); the confirm button on one turns the list into the
 *    DEF_HUNT_MATERIALS with what is held of each, the cursor picks the common to receive, and each further press
 *    trades one rare item;
 *  - Replicas: def_replicas with their Bits; the detail says what the item does (its description, its stats), which of
 *    the player's Digimon can wear it and the rare items the purchase would take, and the confirm button pressed twice
 *    buys.
 * The detail is written for a player who meets the screen for the first time: what a Core is worth here, what the
 * materials are for, that the replicas spend Cores too. A result or a refusal shows in its last line until the cursor
 * moves.
 *
 * Off in a replay script (def_on): the trader talks as the game has it. */
#include "common.h"

#include "cdload.h"
#include "fieldstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "pad.h"
#include "psyq/libgpu.h"
#include "records.h"
#include "sound.h"

#include "definitive.h"
#include "port.h"
#include "port_runtime.h"
#include "psxstack/savestate.h"

extern const unsigned char def_str_BMTitle[], def_str_BMTabTrade[], def_str_BMTabReplica[], def_str_BMGive[],
    def_str_BMGet[], def_str_BMHeld[], def_str_BMBits[], def_str_BMNone[], def_str_BMNoneWhere[], def_str_BMTraded[],
    def_str_BMBought[], def_str_BMArmed[], def_str_BMNoRoom[], def_str_BMNoBits[],
    def_str_BMBagFull[], def_str_BMOwned[], def_str_BMKeysTrade[], def_str_BMKeysPick[], def_str_BMKeysReplica[], def_str_BMKeysArmed[],
    def_str_BMHeadCores[], def_str_BMHeadMats[], def_str_BMHeadReplicas[], def_str_BMCoreTrade[], def_str_BMCoreForge[],
    def_str_BMCoreKeep[], def_str_BMKindWeapon[], def_str_BMKindTwo[], def_str_BMKindArmor[], def_str_BMKindAcc[],
    def_str_BMWears[], def_str_BMWearsAll[], def_str_BMWearsNone[], def_str_BMTakes[], def_str_BMPlus[], def_str_BMMinus[],
    def_str_BMStat1[], def_str_BMStat2[], def_str_BMStat3[], def_str_BMStat4[], def_str_BMStat5[], def_str_BMStat6[],
    def_str_Fire[], def_str_Water[], def_str_Ice[], def_str_Wind[], def_str_Thunder[], def_str_Machine[], def_str_Dark[],
    def_str_HuntSpace[], def_str_HuntHeld[], def_str_RecipeSep[], def_str_Slash[];

/* The trader: a figure whose own talk sets no flag and that no event script moves, so taking its talk over loses
 * nothing (tools/bm_npc_candidates.py lists the towns' figures with their lines). The pick is to confirm on the lab with
 * the lines decoded; a map's copy in the second half of the game keeps the trader there too. */
typedef struct BmTrader {
    s16 map;   /* the field map */
    s16 actor; /* FieldstgPlacedActor.id: its actor's key1 */
} BmTrader;
static const BmTrader traders[] = {
    { 0x20F, 42 }, /* WSTAG255, the Velvet Room (port/game/definitive_velvet.c): the trader lives there now, and the
                    * room is reachable from every inn's Guardromon, so the screen is never out of reach */
};
#define BM_TRADERS ((s32)(sizeof(traders) / sizeof(traders[0])))

/* The layout (320x240), the Guardromon screen's frame: the title bar and its tabs; the list, a heading over its rows;
 * the detail, four lines on what the row under the cursor is, does and costs; the keys and the Bits held. All of it in
 * the dialogues' 14-pixel font: the small one has no ',', '/' or ':' (they came out as a '0') nor accents, and its
 * plain colour is the orange of its highlight. */
#define BM_LAYER 0x1002 /* the field's window layer (the field menu's) */
#define BM_FONT 1
#define BM_LINE 14      /* a row's pitch: the font's height */
#define BM_TITLE_H 23
#define BM_LIST_Y 25    /* the list's panel */
#define BM_HEAD_Y 28    /* its heading */
#define BM_RULE_Y 44    /* the line under the heading */
#define BM_ROWS 6       /* rows the list shows; it scrolls */
#define BM_TOP 48       /* the first row */
#define BM_LIST_X 2
#define BM_LIST_W 316
#define BM_LIST_H (BM_TOP - BM_LIST_Y + BM_ROWS * BM_LINE + 3)
#define BM_NAME_X 18    /* a row's name, clear of the cursor */
#define BM_RIGHT_X 304  /* the right column (a count, a price, the list's place) ends here */
#define BM_DETAIL_Y (BM_LIST_Y + BM_LIST_H + 3)
#define BM_DETAIL_LINES 4
#define BM_DETAIL_H (BM_DETAIL_LINES * BM_LINE + 8)
#define BM_TEXT_W (BM_RIGHT_X - BM_NAME_X) /* the room a detail line has */
#define BM_HINT_Y 208   /* the keys, two lines */
#define BM_TAB_X 150
#define BM_TAB_DX 86
/* The colours mean one thing each: white the plain text, orange the row under the cursor or what is short, green
 * what went through or is the player's already, grey what does not apply now (the other tab, a price above the Bits
 * held, a material that is full) and the explanations. */
#define BM_PAL_TEXT 0
#define BM_PAL_HOT 3
#define BM_PAL_GOOD 4
#define BM_PAL_DIM 7

#define BM_SND_MOVE 0x8004513E
#define BM_SND_OK 0x8004503C
#define BM_SND_BACK 0x800450BD

#define BM_FILE_ITEMS (records_language + DEF_TEXT_ITMNAM) /* the items' names (?SITMNAM): an entry per item ID */
#define BM_FILE_INFO (records_language + DEF_TEXT_ITMINF)  /* and their descriptions (?SITMINF), the shops' */

enum { BM_TRADE, BM_REPLICA };       /* the tabs */
enum { BM_LIST, BM_PICK, BM_ARMED }; /* the list; the Exchange's common being chosen; a replica chosen once */
enum { BM_NOTE_NONE = -1 };

static struct {
    s32 request; /* the trader was talked to: the field's next idle frame opens the screen (the in-game test pokes it) */
    s32 open;
    s32 fresh; /* the frame it opened: the press that talked is not read again */
    s32 close;
    s32 tab;
    s32 mode;
    s32 sel; /* the row under the cursor */
    s32 top; /* the first row shown */
    s32 rows;
    s32 common; /* the common the Exchange gives (kept between visits): the materials' list's cursor */
    s32 mtop;   /* that list's first row shown */
    s32 note;   /* a DEF_BM_* result shown in a detail line, or BM_NOTE_NONE */
    s32 bought; /* a trade or a purchase happened since the screen opened (the room's shopkeeper asks) */
    s32 open_anim;
    s32 tab_anim;
    s32 stamp_anim;
    s32 stamp_kind; /* 0: DEAL!, 1: SOLD! */
    s32 layer;
    s16 rares[DEF_HUNT_RARES]; /* the Exchange's rows: the rare items held, by index of def_hunt_targets */
    MessageWindow *title, *tabs[2], *head, *where, *names[BM_ROWS], *counts[BM_ROWS], *lines[BM_DETAIL_LINES], *hint,
        *bits;
    MessageCursor *cursor;
} bm_ui;

/* The composed texts, one buffer per window (a window copies its text when it is set, message_copy_line_text, so
 * these are only scratch). */
static unsigned char bm_row_text[BM_ROWS][DEF_HUNT_LINE_SIZE];
static unsigned char bm_count_text[BM_ROWS][16];
static unsigned char bm_line_text[BM_DETAIL_LINES][DEF_HUNT_LINE_SIZE * 2];
static unsigned char bm_where_text[16], bm_bits_text[24];
static const unsigned char bm_empty[1] = { 0 };

/* A number in the game's digits (glyph 4 + d), any size (def_glyph_count stops at 99). */
static void bm_put_number(unsigned char *buf, int *p, int size, s32 n) {
    unsigned char digits[12];
    int k = 0;

    if (n < 0) {
        n = 0;
    }
    do {
        digits[k++] = (unsigned char)(4 + n % 10);
        n /= 10;
    } while (n > 0 && k < (int)sizeof(digits));
    while (k > 0 && *p < size - 1) {
        buf[(*p)++] = digits[--k];
    }
    buf[*p] = 0;
}

static MessageWindow *bm_window(s32 x, s32 y) {
    MessageWindow *w = message_create_window(bm_ui.layer, BM_FONT, x, y);

    w->set_ot_depth(w, 1);
    return w;
}

static void bm_rect_ot(s32 depth, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(bm_ui.layer);
    POLY_F4 *poly;

    if (layer == NULL || w <= 0 || h <= 0) {
        return;
    }
    poly = gfx_module.funcs.get_packet();
    setPolyF4(poly);
    setRGB0(poly, r, g, b);
    setXY4(poly, (s16)x, (s16)y, (s16)(x + w), (s16)y, (s16)x, (s16)(y + h), (s16)(x + w), (s16)(y + h));
    if (abr >= 0) {
        DR_TPAGE *tp = (DR_TPAGE *)(poly + 1);
        setSemiTrans(poly, 1);
        addPrim(layer->get_ot_entry(layer, depth), poly);
        SetDrawTPage(tp, 0, 1, getTPage(1, abr, 0, 0));
        addPrim(layer->get_ot_entry(layer, depth), tp);
        gfx_module.funcs.set_packet(tp + 1);
    } else {
        addPrim(layer->get_ot_entry(layer, depth), poly);
        gfx_module.funcs.set_packet(poly + 1);
    }
}

static void bm_slant_ot(s32 depth, s32 x, s32 y, s32 w, s32 h, s32 skew, s32 abr, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(bm_ui.layer);
    POLY_F4 *poly;

    if (layer == NULL || w <= 0 || h <= 0) {
        return;
    }
    poly = gfx_module.funcs.get_packet();
    setPolyF4(poly);
    setRGB0(poly, r, g, b);
    setXY4(poly, (s16)(x + skew), (s16)y, (s16)(x + w + skew), (s16)y, (s16)x, (s16)(y + h), (s16)(x + w),
           (s16)(y + h));
    if (abr >= 0) {
        DR_TPAGE *tp = (DR_TPAGE *)(poly + 1);
        setSemiTrans(poly, 1);
        addPrim(layer->get_ot_entry(layer, depth), poly);
        SetDrawTPage(tp, 0, 1, getTPage(1, abr, 0, 0));
        addPrim(layer->get_ot_entry(layer, depth), tp);
        gfx_module.funcs.set_packet(tp + 1);
    } else {
        addPrim(layer->get_ot_entry(layer, depth), poly);
        gfx_module.funcs.set_packet(poly + 1);
    }
}

static void bm_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b) {
    bm_rect_ot(2, x, y, w, h, -1, r, g, b);
}

static void bm_panel(s32 x, s32 y, s32 w, s32 h, s32 band) {
    if (band) {
        bm_rect_ot(2, x + 2, y + 2, w - 4, BM_LINE + 3, -1, 32, 22, 56);
    }
    bm_rect_ot(2, x, y, 2, h, -1, 188, 128, 255);
    bm_rect_ot(2, x, y, 2, 12, -1, 248, 236, 255);
    bm_rect_ot(2, x + 2, y, w - 2, 1, -1, 128, 86, 188);
    bm_rect_ot(2, x + 2, y + h - 1, w - 2, 1, -1, 76, 52, 120);
    bm_rect_ot(2, x + 2, y + 1, w - 2, h - 2, -1, 14, 12, 26);
}

static void bm_draw_stamp(GfxLayer *layer, s32 cx, s32 cy, const char *word, u8 r, u8 g, u8 b, s32 scale_pct) {
    s32 size = 148 * scale_pct / 100;
    s32 tw = def_poly_text_width(size, word);
    s32 pad_x = 8 * scale_pct / 100;
    s32 pad_y = 4 * scale_pct / 100;
    s32 bw = tw + pad_x * 2;
    s32 bh = (10 * scale_pct / 100) + pad_y * 2;
    s32 x = cx - bw / 2;
    s32 y = cy - bh / 2;
    u8 txt[3] = { 255, 252, 240 };
    u8 shd[3] = { 16, 24, 18 };

    def_poly_text(layer, 0, x + pad_x - 2, y + pad_y - 1, size, word, txt);
    def_poly_text(layer, 0, x + pad_x - 1, y + pad_y, size, word, shd);
    bm_slant_ot(0, x + 2, y + 2, bw - 4, bh - 4, -3, -1, r, g, b);
    bm_slant_ot(0, x, y, bw, bh, -3, -1, 244, 255, 232);
}

/* A window's text (NULL: none) and colour. */
static void bm_set(MessageWindow *w, const unsigned char *text, s32 palette) {
    w->set_text(w, (u8 *)(text != NULL ? text : bm_empty), -1);
    w->set_palette(w, (u8)palette);
}

/* The same in the right column: the window is placed by the text's right end (message_measure). */
static void bm_set_right(MessageWindow *w, const unsigned char *text, s32 palette) {
    bm_set(w, text, palette);
    w->measure(w, 1);
}

/* A detail line that came out wider than the panel is logged: every line is written to fit, in both languages. */
static void bm_check_width(s32 i) {
    MessageWindow *w = bm_ui.lines[i];

    w->measure(w, 1);
    if (w->measured_width > BM_TEXT_W) {
        port_log("market: detail line %d is %d pixels wide, the panel holds %d", (int)i, (int)w->measured_width,
                 BM_TEXT_W);
    }
    w->measure(w, 0); /* back to starting at its x */
}

static void bm_line(s32 i, const unsigned char *text, s32 palette) {
    bm_set(bm_ui.lines[i], text, palette);
    bm_check_width(i);
}

/* The first row to show for row `sel` to be in view, the list's first shown being `top`. */
static s32 bm_scrolled(s32 sel, s32 top) {
    if (sel < top) {
        return sel;
    }
    if (sel >= top + BM_ROWS) {
        return sel - BM_ROWS + 1;
    }
    return top;
}

/* The list on screen: the materials while the Exchange's is being picked, else the tab's rows. */
static void bm_view(s32 *rows, s32 *sel, s32 *top) {
    if (bm_ui.mode == BM_PICK) {
        *rows = DEF_HUNT_MATERIALS;
        *sel = bm_ui.common;
        *top = bm_ui.mtop;
    } else {
        *rows = bm_ui.rows;
        *sel = bm_ui.sel;
        *top = bm_ui.top;
    }
}

/* The tab's rows: the replicas, or the rare items held (`keep`: the cursor stays on the rare it was on, while that is
 * still there). */
static void bm_collect(s32 keep) {
    s32 was = keep && bm_ui.tab == BM_TRADE && bm_ui.sel < bm_ui.rows ? bm_ui.rares[bm_ui.sel] : -1;
    s32 t;

    if (bm_ui.tab == BM_REPLICA) {
        bm_ui.rows = def_bm_replica_rows();
    } else {
        bm_ui.rows = 0;
        for (t = 0; t < def_hunt_target_count && t < DEF_HUNT_RARES; t++) {
            if (def_record_rare(t) > 0) {
                if (t == was) {
                    bm_ui.sel = bm_ui.rows;
                }
                bm_ui.rares[bm_ui.rows++] = (s16)t;
            }
        }
    }
    if (bm_ui.sel >= bm_ui.rows) {
        bm_ui.sel = bm_ui.rows > 0 ? bm_ui.rows - 1 : 0;
    }
    bm_ui.top = bm_scrolled(bm_ui.sel, bm_ui.top);
}

/* A result's text for the last detail line; NULL for what the lines show by themselves (a replica's missing Cores
 * are already its cost line, in orange). */
static const unsigned char *bm_note_text(void) {
    switch (bm_ui.note) {
    case DEF_BM_OK:
        return bm_ui.tab == BM_TRADE ? def_str_BMTraded : def_str_BMBought;
    case DEF_BM_NO_RARE:
        return def_str_BMNone;
    case DEF_BM_NO_ROOM:
        return def_str_BMNoRoom;
    case DEF_BM_NO_BITS:
        return def_str_BMNoBits;
    case DEF_BM_BAG_FULL:
        return def_str_BMBagFull;
    case DEF_BM_OWNED:
        return def_str_BMOwned;
    }
    return NULL;
}

/* A stat's name (RecordsBonusStat), NULL for one the line leaves out: 7 (all of them) is no item's, and 17-21 are
 * the rings' resistances, which their description names. The elements' are the Inspect panel's. */
static const unsigned char *bm_stat_name(s32 stat) {
    static const unsigned char *const names[] = {
        NULL,          def_str_BMStat1, def_str_BMStat2, def_str_BMStat3,   def_str_BMStat4,
        def_str_BMStat5, def_str_BMStat6, NULL,          def_str_Fire,      def_str_Water,
        def_str_Ice,   def_str_Wind,    def_str_Thunder, def_str_Machine,   def_str_Dark,
    };

    return stat > 0 && stat < (s32)(sizeof(names) / sizeof(names[0])) ? names[stat] : NULL;
}

/* "   Strength +114" after the line so far; nothing for a stat without a name, or a 0. */
static void bm_put_stat(unsigned char *buf, int *p, int size, s32 stat, s32 value) {
    const unsigned char *name = bm_stat_name(stat);

    if (name == NULL || value == 0) {
        return;
    }
    def_glyph_put(buf, p, size, def_str_HuntSpace);
    def_glyph_put(buf, p, size, def_str_HuntSpace);
    def_glyph_put(buf, p, size, def_str_HuntSpace);
    def_glyph_put(buf, p, size, name);
    def_glyph_put(buf, p, size, def_str_HuntSpace);
    def_glyph_put(buf, p, size, value > 0 ? def_str_BMPlus : def_str_BMMinus);
    bm_put_number(buf, p, size, value > 0 ? value : -value);
}

/* What kind of equipment `item` is and what it adds, read from its data as the shops do (a variant's are its own):
 * "Weapon   Strength +114   Spirit +5". A crest adds no stat: its description says what it does. */
static void bm_stats_line(unsigned char *buf, int size, s32 item) {
    RecordsItem *it = records_funcs.get_item(item);
    int p = 0, k;

    buf[0] = 0;
    if (it == NULL || it->data == NULL) {
        return;
    }
    if (it->category == RECORDS_ITEM_WEAPON) {
        const RecordsWeapon *w = it->data;

        def_glyph_put(buf, &p, size, w->slot == 7 ? def_str_BMKindTwo : def_str_BMKindWeapon);
        bm_put_stat(buf, &p, size, RECORDS_BONUS_POWER, w->power);
        for (k = 0; k < 2; k++) {
            bm_put_stat(buf, &p, size, w->bonus_stats[k], w->bonus_values[k]);
        }
    } else if (it->category == RECORDS_ITEM_ARMOR) {
        const RecordsArmor *a = it->data;

        def_glyph_put(buf, &p, size, def_str_BMKindArmor);
        bm_put_stat(buf, &p, size, RECORDS_BONUS_GUARD, a->guard);
        for (k = 0; k < 2; k++) {
            bm_put_stat(buf, &p, size, a->bonus_stats[k], a->bonus_values[k]);
        }
    } else if (it->category == RECORDS_ITEM_ACCESSORY) {
        const RecordsAccessory *a = it->data;

        def_glyph_put(buf, &p, size, def_str_BMKindAcc);
        bm_put_stat(buf, &p, size, a->bonus_stat, a->bonus_value);
    }
}

/* Which of the player's Digimon (those that joined) can wear `item`: RecordsEquip.members, bit i for
 * gamestate_data.digimon[i] (stitshop_can_equip). Most weapons fit one to three of the eight, so this is what keeps
 * a player from paying for one nobody in the party can hold; the line is orange then. Its colour is returned. */
static s32 bm_wear_line(unsigned char *buf, int size, s32 item) {
    RecordsItem *it = records_funcs.get_item(item);
    const RecordsEquip *eq;
    int p = 0;
    s32 i, joined = 0, can = 0, n = (s32)(sizeof(gamestate_data.digimon) / sizeof(gamestate_data.digimon[0]));

    buf[0] = 0;
    if (it == NULL || it->data == NULL || it->category < RECORDS_ITEM_WEAPON) {
        return BM_PAL_TEXT;
    }
    eq = it->data;
    for (i = 0; i < n; i++) {
        if (gamestate_data.digimon[i].joined != 0) {
            joined++;
            can += (eq->members >> i) & 1;
        }
    }
    if (can == 0) {
        def_glyph_put(buf, &p, size, def_str_BMWearsNone);
        return BM_PAL_HOT;
    }
    if (can == joined && joined > 1) {
        def_glyph_put(buf, &p, size, def_str_BMWearsAll);
        return BM_PAL_TEXT;
    }
    def_glyph_put(buf, &p, size, def_str_BMWears);
    for (i = 0, can = 0; i < n; i++) {
        if (gamestate_data.digimon[i].joined != 0 && ((eq->members >> i) & 1)) {
            if (can++ > 0) {
                def_glyph_put(buf, &p, size, def_str_RecipeSep);
            }
            def_glyph_put(buf, &p, size, (const unsigned char *)gamestate_data.digimon[i].record.name);
        }
    }
    return BM_PAL_TEXT;
}

/* The rare items replica `r` takes, with what is held of each: "Takes: Datamon 3/1, Fujinmon 0/1", orange while one is
 * short (the forge's line). Its colour is returned. */
static s32 bm_cost_line(int r, unsigned char *buf, int size) {
    int picks[DEF_BM_REPLICA_RARES], p = 0, k, all = def_bm_replica_pick(r, picks);

    buf[0] = 0;
    def_glyph_put(buf, &p, size, def_str_BMTakes);
    for (k = 0; k < DEF_BM_REPLICA_RARES && picks[k] >= 0; k++) {
        if (k > 0) {
            def_glyph_put(buf, &p, size, def_str_RecipeSep);
        }
        def_hunt_target_name(buf, &p, size, picks[k]);
        def_glyph_put(buf, &p, size, def_str_HuntSpace);
        def_glyph_count(buf, &p, size, def_record_rare(picks[k]));
        def_glyph_put(buf, &p, size, def_str_Slash);
        def_glyph_count(buf, &p, size, 1);
    }
    return all ? BM_PAL_TEXT : BM_PAL_HOT;
}

/* The Exchange's detail. On the list: what a Core is worth here, what the materials are for, and the replicas'
 * claim on the same Cores (or, with none held, where they come from). Picking: "Give: Datamon IV x1   Held: 3" and
 * "Get: Data Chip x12   Held: 30/99", that one orange with a warning under it while the lot would not fit. */
static void bm_fill_trade(void) {
    unsigned char *a = bm_line_text[0], *b = bm_line_text[1];
    int pa = 0, pb = 0, size = sizeof(bm_line_text[0]);
    s32 rare = bm_ui.sel < bm_ui.rows ? bm_ui.rares[bm_ui.sel] : -1, fits;

    if (bm_ui.mode != BM_PICK || rare < 0) {
        bm_line(0, rare < 0 ? def_str_BMNone : def_str_BMCoreTrade, BM_PAL_TEXT);
        bm_line(1, rare < 0 ? def_str_BMNoneWhere : def_str_BMCoreForge, rare < 0 ? BM_PAL_DIM : BM_PAL_TEXT);
        bm_line(2, rare < 0 ? NULL : def_str_BMCoreKeep, BM_PAL_DIM);
        bm_line(3, NULL, BM_PAL_TEXT);
        return;
    }
    a[0] = b[0] = 0;
    def_glyph_put(a, &pa, size, def_str_BMGive);
    def_hunt_target_name(a, &pa, size, rare);
    def_glyph_put(a, &pa, size, def_str_HuntSpace);
    def_glyph_put(a, &pa, size, def_str_HuntHeld);
    bm_put_number(a, &pa, size, 1);
    def_glyph_put(a, &pa, size, def_str_BMHeld);
    bm_put_number(a, &pa, size, def_record_rare(rare));
    def_glyph_put(b, &pb, size, def_str_BMGet);
    def_glyph_put(b, &pb, size, def_hunt_material_name(bm_ui.common));
    def_glyph_put(b, &pb, size, def_str_HuntSpace);
    def_glyph_put(b, &pb, size, def_str_HuntHeld);
    bm_put_number(b, &pb, size, DEF_BM_COMMONS);
    def_glyph_put(b, &pb, size, def_str_BMHeld);
    bm_put_number(b, &pb, size, def_record_material(bm_ui.common));
    def_glyph_put(b, &pb, size, def_str_Slash);
    bm_put_number(b, &pb, size, DEF_HUNT_MATERIAL_CAP);
    fits = def_bm_exchange_check(rare, bm_ui.common) != DEF_BM_NO_ROOM;
    bm_line(0, a, BM_PAL_TEXT);
    bm_line(1, b, fits ? BM_PAL_TEXT : BM_PAL_HOT);
    bm_line(2, fits ? NULL : def_str_BMNoRoom, BM_PAL_HOT);
    bm_line(3, NULL, BM_PAL_TEXT);
}

/* A replica's detail: what it does (the shops' description of the item), its kind and stats, who can wear it, and the
 * rare items it would take. Chosen once (armed), the last two are the cost and the warning that a replica resells for
 * nothing; a replica none of the player's Digimon can wear keeps saying so then, in the stats' place (one that joins
 * later may wear it, so the purchase is not refused). */
static void bm_fill_replica(void) {
    int size = sizeof(bm_line_text[0]);
    s32 r = bm_ui.sel, item, wear;

    if (r >= bm_ui.rows) {
        for (r = 0; r < BM_DETAIL_LINES; r++) {
            bm_line(r, NULL, BM_PAL_TEXT);
        }
        return;
    }
    item = def_bm_replica_item(r);
    bm_ui.lines[0]->set_text(bm_ui.lines[0], cdload_module.files.get_file(BM_FILE_INFO), item);
    bm_ui.lines[0]->set_palette(bm_ui.lines[0], BM_PAL_TEXT);
    bm_check_width(0);
    wear = bm_wear_line(bm_line_text[2], size, item);
    if (bm_ui.mode == BM_ARMED) {
        if (wear == BM_PAL_HOT) {
            bm_line(1, bm_line_text[2], wear);
        } else {
            bm_stats_line(bm_line_text[1], size, item);
            bm_line(1, bm_line_text[1], BM_PAL_TEXT);
        }
        bm_line(2, bm_line_text[3], bm_cost_line(r, bm_line_text[3], size));
        bm_line(3, def_str_BMArmed, BM_PAL_HOT);
        return;
    }
    bm_stats_line(bm_line_text[1], size, item);
    bm_line(1, bm_line_text[1], BM_PAL_TEXT);
    bm_line(2, bm_line_text[2], wear);
    bm_line(3, bm_line_text[3], bm_cost_line(r, bm_line_text[3], size));
}

/* Fills the windows from the state. */
static void bm_fill(void) {
    const unsigned char *note;
    s32 picking = bm_ui.mode == BM_PICK, rows, sel, top, i, k, palette, right;
    int p;

    bm_view(&rows, &sel, &top);
    bm_ui.tabs[0]->set_palette(bm_ui.tabs[0], bm_ui.tab == BM_TRADE ? BM_PAL_HOT : BM_PAL_DIM);
    bm_ui.tabs[1]->set_palette(bm_ui.tabs[1], bm_ui.tab == BM_REPLICA ? BM_PAL_HOT : BM_PAL_DIM);
    bm_set(bm_ui.head,
           picking ? def_str_BMHeadMats : bm_ui.tab == BM_TRADE ? def_str_BMHeadCores : def_str_BMHeadReplicas,
           BM_PAL_DIM);
    /* where the cursor is, in a list longer than the panel: "3/16" */
    p = 0;
    bm_where_text[0] = 0;
    if (rows > BM_ROWS) {
        bm_put_number(bm_where_text, &p, sizeof(bm_where_text), sel + 1);
        def_glyph_put(bm_where_text, &p, sizeof(bm_where_text), def_str_Slash);
        bm_put_number(bm_where_text, &p, sizeof(bm_where_text), rows);
    }
    bm_set_right(bm_ui.where, bm_where_text, BM_PAL_DIM);
    for (i = 0; i < BM_ROWS; i++) {
        k = top + i;
        p = 0;
        bm_row_text[i][0] = bm_count_text[i][0] = 0;
        if (k >= rows) {
            bm_set(bm_ui.names[i], NULL, BM_PAL_TEXT);
            bm_set_right(bm_ui.counts[i], NULL, BM_PAL_TEXT);
            continue;
        }
        palette = k == sel ? BM_PAL_HOT : BM_PAL_TEXT;
        right = BM_PAL_TEXT;
        if (picking) {
            /* a material and what is held of it against the cap, grey when the lot would not fit */
            def_glyph_put(bm_row_text[i], &p, sizeof(bm_row_text[i]), def_hunt_material_name(k));
            p = 0;
            bm_put_number(bm_count_text[i], &p, sizeof(bm_count_text[i]), def_record_material(k));
            def_glyph_put(bm_count_text[i], &p, sizeof(bm_count_text[i]), def_str_Slash);
            bm_put_number(bm_count_text[i], &p, sizeof(bm_count_text[i]), DEF_HUNT_MATERIAL_CAP);
            if (def_record_material(k) + DEF_BM_COMMONS > DEF_HUNT_MATERIAL_CAP) {
                right = BM_PAL_DIM;
                palette = palette == BM_PAL_TEXT ? BM_PAL_DIM : palette;
            }
            bm_set(bm_ui.names[i], bm_row_text[i], palette);
        } else if (bm_ui.tab == BM_TRADE) {
            def_hunt_target_name(bm_row_text[i], &p, sizeof(bm_row_text[i]), bm_ui.rares[k]);
            p = 0;
            def_glyph_put(bm_count_text[i], &p, sizeof(bm_count_text[i]), def_str_HuntHeld);
            bm_put_number(bm_count_text[i], &p, sizeof(bm_count_text[i]), def_record_rare(bm_ui.rares[k]));
            bm_set(bm_ui.names[i], bm_row_text[i], palette);
        } else {
            /* a replica, green once the player has one; its price grey above the Bits held */
            if (palette == BM_PAL_TEXT && def_bm_replica_owned(k)) {
                palette = BM_PAL_GOOD;
            }
            bm_ui.names[i]->set_text(bm_ui.names[i], cdload_module.files.get_file(BM_FILE_ITEMS), def_bm_replica_item(k));
            bm_ui.names[i]->set_palette(bm_ui.names[i], palette);
            bm_put_number(bm_count_text[i], &p, sizeof(bm_count_text[i]), def_bm_replica_bits(k));
            right = gamestate_data.money < def_bm_replica_bits(k) ? BM_PAL_DIM : BM_PAL_TEXT;
        }
        bm_set_right(bm_ui.counts[i], bm_count_text[i], k == sel && right == BM_PAL_TEXT ? BM_PAL_HOT : right);
    }
    if (bm_ui.tab == BM_TRADE) {
        bm_fill_trade();
    } else {
        bm_fill_replica();
    }
    /* a result or a refusal takes the last line (a full material's warning is already the line above it) */
    note = bm_note_text();
    if (note != NULL && !(picking && bm_ui.note == DEF_BM_NO_ROOM)) {
        bm_line(3, note, bm_ui.note == DEF_BM_OK ? BM_PAL_GOOD : BM_PAL_HOT);
    }
    if (bm_ui.tab == BM_TRADE) {
        bm_ui.hint->set_text(bm_ui.hint, (u8 *)(picking ? def_str_BMKeysPick : def_str_BMKeysTrade), -1);
    } else {
        bm_ui.hint->set_text(bm_ui.hint, (u8 *)(bm_ui.mode == BM_ARMED ? def_str_BMKeysArmed : def_str_BMKeysReplica),
                             -1);
    }
    /* the Bits held, orange under a replica they do not pay for (its price is grey in the list) */
    p = 0;
    bm_put_number(bm_bits_text, &p, sizeof(bm_bits_text), gamestate_data.money);
    def_glyph_put(bm_bits_text, &p, sizeof(bm_bits_text), def_str_BMBits);
    bm_set_right(bm_ui.bits, bm_bits_text,
                 bm_ui.tab == BM_REPLICA && bm_ui.sel < bm_ui.rows && gamestate_data.money < def_bm_replica_bits(bm_ui.sel)
                     ? BM_PAL_HOT
                     : BM_PAL_TEXT);
    bm_ui.cursor->set_pos(bm_ui.cursor, BM_LIST_X + 4, BM_TOP + (sel - top) * BM_LINE);
    bm_ui.cursor->show(bm_ui.cursor, 0);
}

static void bm_open(void) {
    FieldstgActor *player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    s32 i;

    /* the talk that was skipped would have stopped the player: a press while walking leaves its walking step on, so it
     * is set to stand as the field menu does when it opens (fieldstg_manager_open_menu) */
    if (player != NULL) {
        ((void (*)(void *, s32))player->base.set_step)(player, 1);
    }
    bm_ui.layer = BM_LAYER;
    bm_ui.title = bm_window(8, 5);
    bm_ui.title->set_text(bm_ui.title, (u8 *)def_str_BMTitle, -1);
    bm_ui.tabs[0] = bm_window(BM_TAB_X, 5);
    bm_ui.tabs[0]->set_text(bm_ui.tabs[0], (u8 *)def_str_BMTabTrade, -1);
    bm_ui.tabs[1] = bm_window(BM_TAB_X + BM_TAB_DX, 5);
    bm_ui.tabs[1]->set_text(bm_ui.tabs[1], (u8 *)def_str_BMTabReplica, -1);
    bm_ui.head = bm_window(BM_NAME_X, BM_HEAD_Y);
    bm_ui.where = bm_window(BM_RIGHT_X, BM_HEAD_Y);
    for (i = 0; i < BM_ROWS; i++) {
        bm_ui.names[i] = bm_window(BM_NAME_X, BM_TOP + i * BM_LINE);
        bm_ui.counts[i] = bm_window(BM_RIGHT_X, BM_TOP + i * BM_LINE);
    }
    for (i = 0; i < BM_DETAIL_LINES; i++) {
        bm_ui.lines[i] = bm_window(BM_NAME_X, BM_DETAIL_Y + 4 + i * BM_LINE);
    }
    bm_ui.hint = bm_window(8, BM_HINT_Y + 2);
    bm_ui.hint->set_page_lines(bm_ui.hint, 2);
    bm_ui.bits = bm_window(BM_RIGHT_X + 8, BM_HINT_Y + 2);
    bm_ui.cursor = message_create_cursor(bm_ui.layer, 1, BM_LIST_X + 4, BM_TOP);
    bm_ui.open = 1;
    bm_ui.fresh = 1;
    bm_ui.request = 0;
    bm_ui.close = 0;
    bm_ui.open_anim = 0;
    bm_ui.tab_anim = 0;
    bm_ui.stamp_anim = 0;
    bm_ui.stamp_kind = 0;
    bm_ui.tab = BM_TRADE;
    bm_ui.mode = BM_LIST;
    bm_ui.sel = bm_ui.top = 0;
    bm_ui.note = BM_NOTE_NONE;
    if (bm_ui.common < 0 || bm_ui.common >= DEF_HUNT_MATERIALS) {
        bm_ui.common = 0;
    }
    bm_ui.mtop = bm_scrolled(bm_ui.common, 0);
    bm_collect(0);
    bm_fill();
    port_log("market: frame %ld: the Black Market on map 0x%X (%d rare items held)", port_frames,
             gamestate_data.funcs.get_map(), bm_ui.rows);
}

static void bm_close(void) {
    MessageWindow **all[] = { &bm_ui.title, &bm_ui.tabs[0], &bm_ui.tabs[1], &bm_ui.head, &bm_ui.where, &bm_ui.hint,
                              &bm_ui.bits };
    s32 i;

    for (i = 0; i < (s32)(sizeof(all) / sizeof(all[0])); i++) {
        heap_objects.stop((Object *)*all[i]);
        *all[i] = NULL;
    }
    for (i = 0; i < BM_ROWS; i++) {
        heap_objects.stop((Object *)bm_ui.names[i]);
        heap_objects.stop((Object *)bm_ui.counts[i]);
        bm_ui.names[i] = bm_ui.counts[i] = NULL;
    }
    for (i = 0; i < BM_DETAIL_LINES; i++) {
        heap_objects.stop((Object *)bm_ui.lines[i]);
        bm_ui.lines[i] = NULL;
    }
    heap_objects.stop((Object *)bm_ui.cursor);
    bm_ui.cursor = NULL;
    bm_ui.open = 0;
    bm_ui.request = 0;
}

static void bm_run_windows(void) {
    s32 i;

    heap_objects.try_run((Object *)bm_ui.title);
    heap_objects.try_run((Object *)bm_ui.tabs[0]);
    heap_objects.try_run((Object *)bm_ui.tabs[1]);
    heap_objects.try_run((Object *)bm_ui.head);
    heap_objects.try_run((Object *)bm_ui.where);
    for (i = 0; i < BM_ROWS; i++) {
        heap_objects.try_run((Object *)bm_ui.names[i]);
        heap_objects.try_run((Object *)bm_ui.counts[i]);
    }
    for (i = 0; i < BM_DETAIL_LINES; i++) {
        heap_objects.try_run((Object *)bm_ui.lines[i]);
    }
    heap_objects.try_run((Object *)bm_ui.hint);
    heap_objects.try_run((Object *)bm_ui.bits);
    heap_objects.try_run((Object *)bm_ui.cursor);
}

static void bm_draw(void) {
    GfxLayer *layer = gfx_module.funcs.get_layer(bm_ui.layer);
    s32 wide = port_video_widescreen_pad();
    s32 rows, sel, top, i;
    s32 op = bm_ui.open_anim < 10 ? bm_ui.open_anim : 10;
    s32 title_dx = -((10 - op) * (10 - op));
    s32 tab_dx = bm_ui.tab_anim * 3;
    s32 list_h = BM_LIST_H * op / 10;
    s32 det_h = BM_DETAIL_H * op / 10;

    bm_ui.title->set_pos(bm_ui.title, 8 + title_dx, 5);
    bm_ui.tabs[0]->set_pos(bm_ui.tabs[0], BM_TAB_X + title_dx, 5);
    bm_ui.tabs[1]->set_pos(bm_ui.tabs[1], BM_TAB_X + BM_TAB_DX + title_dx, 5);
    bm_ui.head->set_pos(bm_ui.head, BM_NAME_X + tab_dx, BM_HEAD_Y);
    for (i = 0; i < BM_ROWS; i++) {
        s32 ry = BM_TOP + i * BM_LINE;
        bm_ui.names[i]->set_pos(bm_ui.names[i], BM_NAME_X + tab_dx, ry);
        bm_ui.counts[i]->set_pos(bm_ui.counts[i], BM_RIGHT_X + tab_dx, ry);
    }

    /* Active tab underline */
    if (op >= 10) {
        s32 tx = bm_ui.tab == BM_TRADE ? BM_TAB_X : BM_TAB_X + BM_TAB_DX;
        bm_rect_ot(2, tx - 2, 20, 72, 2, -1, 214, 156, 255);
    }

    /* Selected row highlight + slanted [A] chip */
    bm_view(&rows, &sel, &top);
    if (op >= 10 && rows > 0 && sel >= top && sel < top + BM_ROWS) {
        s32 ry = BM_TOP + (sel - top) * BM_LINE - 1;
        if (bm_ui.mode != BM_LIST) {
            bm_slant_ot(2, BM_LIST_X + 3 + tab_dx, ry, 10, BM_LINE, 2, -1, 224, 152, 42);
            bm_rect_ot(2, BM_LIST_X + 11 + tab_dx, ry, BM_LIST_W - 16, BM_LINE, 1, 92, 54, 14);
            bm_rect_ot(2, BM_LIST_X + 11 + tab_dx, ry, BM_LIST_W - 16, 1, -1, 248, 198, 92);
        } else {
            bm_slant_ot(2, BM_LIST_X + 3 + tab_dx, ry, 10, BM_LINE, 2, -1, 44, 192, 96);
            bm_rect_ot(2, BM_LIST_X + 11 + tab_dx, ry, BM_LIST_W - 16, BM_LINE, 1, 48, 30, 88);
            bm_rect_ot(2, BM_LIST_X + 11 + tab_dx, ry, BM_LIST_W - 16, 1, -1, 196, 140, 255);
        }
    }

    /* Scrollbar on right edge of list */
    if (op >= 10 && rows > BM_ROWS) {
        s32 track_h = BM_ROWS * BM_LINE - 4;
        s32 thumb_h = track_h * BM_ROWS / rows;
        s32 thumb_y;
        if (thumb_h < 12) {
            thumb_h = 12;
        }
        thumb_y = BM_TOP + (track_h - thumb_h) * top / (rows - BM_ROWS);
        bm_rect_ot(2, BM_LIST_X + BM_LIST_W - 5, BM_TOP, 2, track_h, -1, 34, 24, 58);
        bm_rect_ot(2, BM_LIST_X + BM_LIST_W - 5, thumb_y, 2, thumb_h, -1, 196, 140, 255);
    }

    /* Stamp-Slam DEAL! / SOLD! on completed trade or purchase */
    if (bm_ui.stamp_anim > 0 && layer != NULL) {
        s32 sc = 100;
        if (bm_ui.stamp_anim > 12) {
            sc = 100 + (bm_ui.stamp_anim - 12) * 24;
        } else if (bm_ui.stamp_anim > 8) {
            sc = 92;
        }
        bm_draw_stamp(layer, 266, BM_DETAIL_Y + 22, bm_ui.stamp_kind ? "SOLD!" : "DEAL!", 36, 158, 86, sc);
        if (bm_ui.stamp_anim >= 10 && bm_ui.stamp_anim <= 13) {
            bm_rect_ot(0, BM_LIST_X, BM_DETAIL_Y, BM_LIST_W, BM_DETAIL_H, 1, 90, 120, 72);
        }
    }

    if (op >= 10) {
        bm_rect(BM_LIST_X + 8, BM_RULE_Y, BM_LIST_W - 16, 1, 128, 86, 188);
    }
    bm_panel(title_dx, 0, 320, BM_TITLE_H, 1);
    if (list_h > 4) {
        bm_panel(BM_LIST_X, BM_LIST_Y, BM_LIST_W, list_h, 0);
        if (op < 10) {
            bm_rect_ot(2, BM_LIST_X, BM_LIST_Y + list_h - 2, BM_LIST_W, 2, -1, 224, 176, 255);
        }
    }
    if (det_h > 4) {
        bm_panel(BM_LIST_X, BM_DETAIL_Y, BM_LIST_W, det_h, 0);
    }
    bm_panel(0, BM_HINT_Y, 320, 240 - BM_HINT_Y, 0);

    /* Full-screen opaque dark obsidian-violet backdrop with diagonal Persona bands */
    bm_slant_ot(2, 214, 0, 72, 240, -38, -1, 20, 14, 38);
    bm_slant_ot(2, 72, 0, 44, 240, -38, -1, 14, 10, 28);
    bm_rect_ot(2, -wide, 0, 320 + wide * 2, 240, -1, 8, 7, 16);
}

static s32 bm_pressed(s32 button) {
    return PAD_PRESSED(button) || PAD_REPEAT(button);
}

/* Moves the cursor by `d` rows, wrapping; scrolls to keep it shown. */
static void bm_move(s32 d) {
    if (bm_ui.rows <= 0) {
        return;
    }
    bm_ui.sel = (bm_ui.sel + d + bm_ui.rows) % bm_ui.rows;
    bm_ui.top = bm_scrolled(bm_ui.sel, bm_ui.top);
    bm_ui.note = BM_NOTE_NONE;
    sound_module.play(BM_SND_MOVE);
    bm_fill();
}

static void bm_pick_input(void) {
    s32 d = (bm_pressed(4) || bm_pressed(7)) ? -1 : (bm_pressed(6) || bm_pressed(5)) ? 1 : 0;

    if (d != 0) {
        bm_ui.common = (bm_ui.common + d + DEF_HUNT_MATERIALS) % DEF_HUNT_MATERIALS;
        bm_ui.mtop = bm_scrolled(bm_ui.common, bm_ui.mtop);
        bm_ui.note = BM_NOTE_NONE;
        sound_module.play(BM_SND_MOVE);
        bm_fill();
    } else if (PAD_PRESSED(13)) {
        bm_ui.note = def_bm_exchange(bm_ui.rares[bm_ui.sel], bm_ui.common);
        sound_module.play(bm_ui.note == DEF_BM_OK ? BM_SND_OK : BM_SND_BACK);
        if (bm_ui.note == DEF_BM_OK) {
            bm_ui.stamp_anim = 20;
            bm_ui.stamp_kind = 0;
            if (def_record_rare(bm_ui.rares[bm_ui.sel]) <= 0) {
                bm_ui.mode = BM_LIST; /* that was the last of it: its row goes */
            }
        }
        bm_collect(1);
        bm_fill();
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        sound_module.play(BM_SND_BACK);
        bm_ui.mode = BM_LIST;
        bm_ui.tab_anim = -6;
        bm_ui.note = BM_NOTE_NONE;
        bm_fill();
    }
}

static void bm_list_input(void) {
    int picks[DEF_BM_REPLICA_RARES];

    if (PAD_PRESSED(10) || PAD_PRESSED(11) || PAD_PRESSED(5) || PAD_PRESSED(7)) {
        bm_ui.tab ^= 1;
        bm_ui.tab_anim = bm_ui.tab == BM_REPLICA ? 8 : -8;
        bm_ui.mode = BM_LIST;
        bm_ui.sel = bm_ui.top = 0;
        bm_ui.note = BM_NOTE_NONE;
        sound_module.play(BM_SND_MOVE);
        bm_collect(0);
        bm_fill();
    } else if (bm_pressed(4)) {
        bm_ui.mode = BM_LIST;
        bm_move(-1);
    } else if (bm_pressed(6)) {
        bm_ui.mode = BM_LIST;
        bm_move(1);
    } else if (PAD_PRESSED(13)) {
        if (bm_ui.rows <= 0) {
            sound_module.play(BM_SND_BACK); /* nothing to trade: the first line says so */
        } else if (bm_ui.tab == BM_TRADE) {
            sound_module.play(BM_SND_OK);
            bm_ui.mode = BM_PICK; /* the list becomes the materials, the cursor on the one picked last time */
            bm_ui.tab_anim = 6;
            bm_ui.mtop = bm_scrolled(bm_ui.common, bm_ui.mtop);
            bm_ui.note = BM_NOTE_NONE;
        } else if (bm_ui.mode == BM_ARMED) {
            bm_ui.note = def_bm_replica_buy(bm_ui.sel);
            sound_module.play(bm_ui.note == DEF_BM_OK ? BM_SND_OK : BM_SND_BACK);
            if (bm_ui.note == DEF_BM_OK) {
                bm_ui.stamp_anim = 20;
                bm_ui.stamp_kind = 1;
            }
            bm_ui.mode = BM_LIST;
        } else {
            bm_ui.note = def_bm_replica_check(bm_ui.sel, picks);
            if (bm_ui.note == DEF_BM_OK) {
                bm_ui.mode = BM_ARMED;
                bm_ui.note = BM_NOTE_NONE;
                sound_module.play(BM_SND_OK);
            } else {
                sound_module.play(BM_SND_BACK);
            }
        }
        bm_fill();
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        sound_module.play(BM_SND_BACK);
        if (bm_ui.mode == BM_ARMED) {
            bm_ui.mode = BM_LIST;
            bm_fill();
        } else {
            bm_ui.close = 1;
        }
    }
}

/* Closes the screen and hands the field back. The talk never started, so the player's control was never taken: it is
 * put back only if something else took it (as the Guardromon screen does after its event). */
static void bm_finish(void) {
    FieldstgActor *player;

    bm_close();
    bm_ui.close = 0;
    fieldstg_stage.event_running = 0;
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player != NULL && player->control == NULL) {
        player->reset_control(player);
    }
    port_log("market: frame %ld: closed", port_frames);
}

/* FIELDSTG's talk: the player talks to the actor `actor_key1` on `map`. 1 when it is the trader: the game's talk is
 * skipped and the screen opens on the next idle frame. */
int def_bm_talk(s32 map, s32 actor_key1) {
    s32 i;

    if (!def_on() || bm_ui.open || bm_ui.request) {
        return 0;
    }
    if (def_time_on()) {
        return 0; /* the Time Room's dealer only talks (dw3_velvet_shop_talk's early return): no market behind the counter */
    }
    if (dw3_velvet_shop_greeting()) {
        return 0; /* the Velvet Room says its line first and asks for the screen itself (dw3_velvet_frame) */
    }
    for (i = 0; i < BM_TRADERS; i++) {
        if (traders[i].map == map && traders[i].actor == actor_key1) {
            break;
        }
    }
    if (i >= BM_TRADERS) {
        return 0;
    }
    cdload_module.queue_file(BM_FILE_ITEMS); /* the replicas' names, read by the list, */
    cdload_module.queue_file(BM_FILE_INFO);  /* and their descriptions, by the detail */
    bm_ui.request = 1;
    fieldstg_stage.event_running = 1; /* frozen from this frame on, as the screen will hold it */
    port_log("market: the trader of map 0x%X (actor %d) talked to", map, actor_key1);
    return 1;
}

/* 1 while the screen is up (or asked for): the Velvet Room closes its books when it goes away. */
int def_bm_screen_open(void) {
    return bm_ui.open || bm_ui.request;
}

/* 1 when the visit traded or bought something: its shopkeeper has a line for each way a visit ends. */
int def_bm_bought(void) {
    return bm_ui.bought;
}

/* One frame of FIELDSTG's idle step: 1 while the screen owns the field. Every field frame also lets a replica the
 * player no longer holds give its ID's resale back (def_bm_sweep): where the counts change (the shop's sale, an
 * event's take, a discard) has no hook of its own, and the shop pays 0 until then anyway. */
int def_bm_frame(void) {
    if (!def_on()) {
        return 0;
    }
    def_bm_sweep();
    if (!bm_ui.open) {
        if (!bm_ui.request) {
            return 0;
        }
        bm_open();
        bm_ui.bought = 0;
    }
    fieldstg_stage.event_running = 1;
    if (bm_ui.fresh) {
        bm_ui.fresh = 0;
    } else if (bm_ui.mode == BM_PICK) {
        bm_pick_input();
    } else {
        bm_list_input();
    }
    if (bm_ui.note == DEF_BM_OK) {
        bm_ui.bought = 1;
    }
    if (bm_ui.close) {
        bm_finish();
        return 1;
    }
    if (bm_ui.open_anim < 10) {
        bm_ui.open_anim++;
    }
    if (bm_ui.tab_anim > 0) {
        bm_ui.tab_anim--;
    } else if (bm_ui.tab_anim < 0) {
        bm_ui.tab_anim++;
    }
    if (bm_ui.stamp_anim > 0) {
        bm_ui.stamp_anim--;
    }
    bm_run_windows();
    bm_draw();
    return 1;
}

/* A save state carries the screen with the field's heap its windows live in: without it a state taken before the screen
 * opened loaded the screen still "open" over windows that heap no longer has (the player stood frozen, and the next
 * load crashed). */
void def_bm_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, bm_ui);
}
