/* The stat swap: the Velvet Room's pig (definitive_velvet.c) moves DEF_RESPEC_POINTS of one raw
 * stat of a partner into another of the same partner, for a price that climbs with each swap (def_respec_price).
 *
 * The raw stats are the partner's, not a form's: gamestate_data.digimon[i].record.stats.values[6..18] (Strength,
 * Defense, Spirit, Wisdom, Speed, Charisma, then the seven elemental Affinities), which the memory card already saves.
 * A battle adds the equipment and the form's fixed bonus on top (fightstg_rules_get_stats) and writes nothing back, so
 * no digivolution undoes a swap; level-ups and training keep adding to the raw value as before.
 *
 * Limits: the giving stat keeps at least 1 (every formula divides by a battle stat the game floors at 1, so 1 breaks
 * nothing), the receiving one stays within the raw cap of 999 (gamestate_add_stat's), which also keeps the swap from
 * being a way past 999 for the Chosen Evolution (its boost is the battle's, on top of the raw value). HP, MP and TP are
 * not offered.
 *
 * The board is a field overlay run from FIELDSTG's idle step, as the Sepikmon's (definitive_superhunt_screen.c, whose
 * windows, panels and keys it shares): the partner's 13 stats in two columns; × picks the stat that gives, × the one
 * that takes, × again pays; △ steps back; L1/R1 change the partner. Off in a replay script. */
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "fieldstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "pad.h"
#include "psyq/libgpu.h"
#include "sound.h"

#include "psxstack/savestate.h"

#include "definitive.h"
#include "port.h"
#include "port_runtime.h"

extern int def_on(void);
extern const unsigned char def_str_EvoStr[], def_str_EvoDef[], def_str_EvoSpi[], def_str_EvoWis[], def_str_EvoSpd[],
    def_str_EvoCha[], def_str_Fire[], def_str_Water[], def_str_Ice[], def_str_Wind[], def_str_Thunder[],
    def_str_Machine[], def_str_Dark[], def_str_RsTitle[], def_str_RsKeysFrom[], def_str_RsKeysTo[], def_str_RsKeysPay[],
    def_str_RsPrice[], def_str_RsPriceTail[], def_str_RsBits[], def_str_RsArrow[], def_str_RsDone[], def_str_RsNoBits[], def_str_RsLow[],
    def_str_RsFull[], def_str_RsSame[], def_str_RsSp[], def_str_RsGap[];

#define RS_LAYER 0x1002 /* the field's window layer */
#define RS_STATS 13     /* record.stats.values[6..18] */
#define RS_FIRST 6
#define RS_LEFT 6       /* the left column: the six stats; the right one: the seven Affinities */
#define RS_LINE 16
#define RS_TITLE_H 23
#define RS_TOP 34
#define RS_LIST_H (7 * RS_LINE + 6)
#define RS_INFO_Y (RS_TOP - 5 + RS_LIST_H + 3)
#define RS_INFO_H 56
#define RS_HINT_Y 208
#define RS_COL_X 18     /* a column's name; its value RS_VAL_DX further */
#define RS_COL_DX 152
#define RS_VAL_DX 92
#define RS_TEXT 96
#define RS_WINDOWS 40
#define RS_PAL_TEXT 0
#define RS_PAL_HOT 3    /* a refusal (the bestiary's orange) */
#define RS_PAL_DONE 4
#define RS_KEYS ((1 << 10) | (1 << 11) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15))
#define RS_SND_MOVE 0x8004513E
#define RS_SND_OK 0x8004503C
#define RS_SND_BACK 0x800450BD

enum { RS_PICK, RS_DEST, RS_PAY };

static struct {
    s32 request;
    s32 open;
    s32 fresh;
    s32 releasing;
    s32 close;
    s32 who;   /* the partner (gamestate_data.digimon) */
    s32 sel;   /* the cursor's stat, 0..RS_STATS-1 */
    s32 phase; /* RS_PICK, RS_DEST (src chosen) or RS_PAY (dest chosen too) */
    s32 src, dest;
    const unsigned char *msg;
    s32 msg_pal;
    s32 count;
    MessageWindow *wins[RS_WINDOWS];
    MessageWindow *title, *name, *names[RS_STATS], *values[RS_STATS], *line1, *line2, *note, *hint;
    MessageCursor *cursor;
} rs;

static unsigned char val_text[RS_STATS][8], info1[RS_TEXT], info2[RS_TEXT];

static const unsigned char *stat_name(int k) {
    static const unsigned char *const names[RS_STATS] = {
        def_str_EvoStr, def_str_EvoDef, def_str_EvoSpi,  def_str_EvoWis,  def_str_EvoSpd, def_str_EvoCha, def_str_Fire,
        def_str_Water,  def_str_Ice,    def_str_Wind,    def_str_Thunder, def_str_Machine, def_str_Dark,
    };

    return names[k];
}

static s16 *raw(int k) {
    GamestateStats *st = &gamestate_data.digimon[rs.who].record.stats;

    return k < RS_LEFT ? &st->stats[k] : &st->resists[k - RS_LEFT];
}

static int joined(int i) {
    return i >= 0 && i < DEF_BLAST_PARTNERS && gamestate_data.digimon[i].joined != 0;
}

static MessageWindow *rs_window(s32 font, s32 x, s32 y) {
    MessageWindow *w = message_create_window(RS_LAYER, font, x, y);

    w->set_ot_depth(w, 1);
    if (rs.count < RS_WINDOWS) {
        rs.wins[rs.count++] = w;
    }
    return w;
}

static void rs_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(RS_LAYER);
    POLY_F4 *poly = gfx_module.funcs.get_packet();

    setPolyF4(poly);
    setRGB0(poly, r, g, b);
    poly->x0 = poly->x2 = x;
    poly->x1 = poly->x3 = x + w;
    poly->y0 = poly->y1 = y;
    poly->y2 = poly->y3 = y + h;
    addPrim(layer->get_ot_entry(layer, 2), poly);
    gfx_module.funcs.set_packet(poly + 1);
}

/* The pig's panels: the Sepikmon's shapes in a pink. */
static void rs_panel(s32 x, s32 y, s32 w, s32 h, s32 band) {
    if (band) {
        rs_rect(x + 2, y + 2, w - 4, RS_LINE + 2, 80, 24, 44);
    }
    rs_rect(x + 1, y + 1, w - 2, h - 2, 24, 8, 16);
    rs_rect(x, y, w, h, 230, 140, 170);
}

static s32 row_x(int k) {
    return RS_COL_X + (k < RS_LEFT ? 0 : RS_COL_DX);
}

static s32 row_y(int k) {
    return RS_TOP + (k < RS_LEFT ? k : k - RS_LEFT) * RS_LINE;
}

/* "Strength 450 -> 350" into buf: the stat, its raw value and, with `delta`, what it becomes. */
static void put_change(unsigned char *buf, int *p, int k, int delta) {
    def_glyph_put(buf, p, RS_TEXT, stat_name(k));
    def_glyph_put(buf, p, RS_TEXT, def_str_RsSp);
    def_glyph_number(buf, p, RS_TEXT, *raw(k));
    if (delta != 0) {
        def_glyph_put(buf, p, RS_TEXT, def_str_RsArrow);
        def_glyph_number(buf, p, RS_TEXT, *raw(k) + delta);
    }
}

static void board_fill(void) {
    int k, p;
    int dest = rs.phase == RS_PAY ? rs.dest : rs.sel;

    rs.name->set_text(rs.name, (u8 *)gamestate_data.digimon[rs.who].record.name, -1);
    for (k = 0; k < RS_STATS; k++) {
        p = 0;
        val_text[k][0] = 0;
        def_glyph_number(val_text[k], &p, sizeof(val_text[k]), *raw(k));
        rs.values[k]->set_text(rs.values[k], val_text[k], -1);
    }
    p = 0;
    info1[0] = 0;
    if (rs.phase == RS_PICK) {
        put_change(info1, &p, rs.sel, 0);
    } else {
        put_change(info1, &p, rs.src, -DEF_RESPEC_POINTS);
        if (dest != rs.src) {
            def_glyph_put(info1, &p, RS_TEXT, def_str_RsGap);
            put_change(info1, &p, dest, DEF_RESPEC_POINTS);
        }
    }
    rs.line1->set_text(rs.line1, info1, -1);
    p = 0;
    info2[0] = 0;
    def_glyph_put(info2, &p, RS_TEXT, def_str_RsPrice);
    def_glyph_number(info2, &p, RS_TEXT, def_respec_price());
    def_glyph_put(info2, &p, RS_TEXT, def_str_RsPriceTail);
    def_glyph_number(info2, &p, RS_TEXT, gamestate_data.money);
    def_glyph_put(info2, &p, RS_TEXT, def_str_RsBits);
    rs.line2->set_text(rs.line2, info2, -1);
    rs.note->set_visible(rs.note, rs.msg != NULL);
    if (rs.msg != NULL) {
        rs.note->set_text(rs.note, (u8 *)rs.msg, -1);
        rs.note->set_palette(rs.note, rs.msg_pal);
    }
    rs.hint->set_text(rs.hint,
                      (u8 *)(rs.phase == RS_PICK ? def_str_RsKeysFrom
                             : rs.phase == RS_DEST ? def_str_RsKeysTo
                                                   : def_str_RsKeysPay),
                      -1);
    rs.cursor->set_pos(rs.cursor, row_x(rs.sel) - 12, row_y(rs.sel));
}

static void say(const unsigned char *msg, int pal) {
    rs.msg = msg;
    rs.msg_pal = pal;
}

static void board_open(void) {
    int k;

    rs.title = rs_window(1, 8, 5);
    rs.title->set_text(rs.title, (u8 *)def_str_RsTitle, -1);
    rs.name = rs_window(1, 180, 5);
    for (k = 0; k < RS_STATS; k++) {
        rs.names[k] = rs_window(2, row_x(k), row_y(k));
        rs.names[k]->set_text(rs.names[k], (u8 *)stat_name(k), -1);
        rs.names[k]->set_palette(rs.names[k], RS_PAL_TEXT);
        rs.values[k] = rs_window(2, row_x(k) + RS_VAL_DX, row_y(k));
        rs.values[k]->set_palette(rs.values[k], RS_PAL_TEXT);
    }
    rs.line1 = rs_window(1, 10, RS_INFO_Y + 4);
    rs.line1->set_palette(rs.line1, RS_PAL_TEXT);
    rs.line2 = rs_window(1, 10, RS_INFO_Y + 4 + 14);
    rs.line2->set_palette(rs.line2, RS_PAL_TEXT);
    rs.note = rs_window(1, 10, RS_INFO_Y + 4 + 28); /* the dialogues' font: the small one's coloured palettes have no
                                                      * lowercase */
    rs.hint = rs_window(1, 8, RS_HINT_Y + 2);
    rs.hint->set_page_lines(rs.hint, 2);
    rs.cursor = message_create_cursor(RS_LAYER, 1, row_x(0) - 12, row_y(0));
    rs.cursor->show(rs.cursor, 1);
    board_fill();
}

static void board_draw(void) {
    if (rs.phase != RS_PICK) {
        rs_rect(row_x(rs.src) - 3, row_y(rs.src) - 2, RS_COL_DX - 10, RS_LINE - 2, 96, 30, 50);
    }
    if (rs.phase == RS_PAY) {
        rs_rect(row_x(rs.dest) - 3, row_y(rs.dest) - 2, RS_COL_DX - 10, RS_LINE - 2, 30, 70, 40);
    }
    rs_panel(0, 0, 320, RS_TITLE_H, 1);
    rs_panel(2, RS_TOP - 5, 316, RS_LIST_H, 0);
    rs_panel(2, RS_INFO_Y, 316, RS_INFO_H, 0);
    rs_panel(0, RS_HINT_Y, 320, 240 - RS_HINT_Y, 0);
}

static void move_to(int k) {
    rs.sel = k;
    rs.msg = NULL;
    sound_module.play(RS_SND_MOVE);
    board_fill();
}

/* The next joined partner after (step 1) or before (step -1) the current one. */
static void next_partner(int step) {
    int i, id = rs.who;

    for (i = 0; i < DEF_BLAST_PARTNERS; i++) {
        id = (id + step + DEF_BLAST_PARTNERS) % DEF_BLAST_PARTNERS;
        if (joined(id)) {
            break;
        }
    }
    if (id == rs.who) {
        return;
    }
    rs.who = id;
    rs.phase = RS_PICK;
    rs.msg = NULL;
    sound_module.play(RS_SND_MOVE);
    board_fill();
}

static void refuse(const unsigned char *msg) {
    sound_module.play(RS_SND_BACK);
    say(msg, RS_PAL_HOT);
}

static void confirm(void) {
    int k = rs.sel;

    if (rs.phase == RS_PICK) {
        if (gamestate_data.money < def_respec_price()) {
            refuse(def_str_RsNoBits);
        } else if (*raw(k) < DEF_RESPEC_POINTS + 1) { /* the giver keeps at least 1 */
            refuse(def_str_RsLow);
        } else {
            rs.src = k;
            rs.phase = RS_DEST;
            rs.msg = NULL;
            sound_module.play(RS_SND_OK);
        }
    } else if (rs.phase == RS_DEST) {
        if (k == rs.src) {
            refuse(def_str_RsSame);
        } else if (*raw(k) + DEF_RESPEC_POINTS > 999) { /* the raw cap (gamestate_add_stat's) */
            refuse(def_str_RsFull);
        } else {
            rs.dest = k;
            rs.phase = RS_PAY;
            rs.msg = NULL;
            sound_module.play(RS_SND_OK);
        }
    } else if (gamestate_data.money < def_respec_price()) {
        rs.phase = RS_PICK;
        refuse(def_str_RsNoBits);
    } else {
        gamestate_data.money -= def_respec_price();
        def_respec_swaps_add();
        *raw(rs.src) -= DEF_RESPEC_POINTS;
        *raw(rs.dest) += DEF_RESPEC_POINTS;
        port_log("respec: partner %d, %d points from stat %d (now %d) to stat %d (now %d), %d Bits left", rs.who,
                 DEF_RESPEC_POINTS, RS_FIRST + rs.src, *raw(rs.src), RS_FIRST + rs.dest, *raw(rs.dest),
                 (int)gamestate_data.money);
        rs.phase = RS_PICK;
        rs.sel = rs.dest;
        sound_module.play(RS_SND_OK);
        say(def_str_RsDone, RS_PAL_DONE);
    }
    board_fill();
}

static void board_input(void) {
    int k = rs.sel, row = k < RS_LEFT ? k : k - RS_LEFT;

    if (PAD_PRESSED(4) || PAD_REPEAT(4)) {
        move_to((k + RS_STATS - 1) % RS_STATS);
    } else if (PAD_PRESSED(6) || PAD_REPEAT(6)) {
        move_to((k + 1) % RS_STATS);
    } else if ((PAD_PRESSED(5) || PAD_REPEAT(5)) && k < RS_LEFT) {
        move_to(RS_LEFT + row);
    } else if ((PAD_PRESSED(7) || PAD_REPEAT(7)) && k >= RS_LEFT) {
        move_to(row < RS_LEFT ? row : RS_LEFT - 1);
    } else if (PAD_PRESSED(10) && rs.phase == RS_PICK) {
        next_partner(-1);
    } else if (PAD_PRESSED(11) && rs.phase == RS_PICK) {
        next_partner(1);
    } else if (PAD_PRESSED(13)) {
        confirm();
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        sound_module.play(RS_SND_BACK);
        rs.msg = NULL;
        if (rs.phase == RS_PICK) {
            rs.close = 1;
            return;
        }
        rs.phase = rs.phase == RS_PAY ? RS_DEST : RS_PICK;
        board_fill();
    }
}

static void rs_close(void) {
    s32 i;

    for (i = 0; i < rs.count; i++) {
        heap_objects.stop((Object *)rs.wins[i]);
        rs.wins[i] = NULL;
    }
    rs.count = 0;
    if (rs.cursor != NULL) {
        heap_objects.stop((Object *)rs.cursor);
        rs.cursor = NULL;
    }
    rs.open = 0;
    rs.request = 0;
}

static void rs_run(void) {
    s32 i;

    for (i = 0; i < rs.count; i++) {
        heap_objects.try_run((Object *)rs.wins[i]);
    }
    if (rs.cursor != NULL) {
        heap_objects.try_run((Object *)rs.cursor);
    }
}

/* The field back to the player once none of the screen's buttons is down (the Sepikmon board's sh_release). */
static int rs_release(void) {
    FieldstgActor *player;

    if (pad_state.get_held(0) & RS_KEYS) {
        fieldstg_stage.event_running = 1;
        return 1;
    }
    rs.releasing = 0;
    fieldstg_stage.event_running = 0;
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player != NULL) {
        player->reset_control(player);
    }
    return 1;
}

void def_respec_offer(void) {
    int id;

    if (!def_on() || rs.open || rs.request) {
        return;
    }
    id = gamestate_data.funcs.get_party_member(0);
    if (!joined(id)) {
        for (id = 0; id < DEF_BLAST_PARTNERS && !joined(id); id++) {
        }
        if (id >= DEF_BLAST_PARTNERS) {
            return;
        }
    }
    rs.who = id;
    rs.request = 1;
    fieldstg_stage.event_running = 1;
    port_log("respec: the pig's board for partner %d, %d Bits", id, (int)gamestate_data.money);
}

int def_respec_frame(void) {
    if (!def_on()) {
        return 0;
    }
    if (rs.releasing) {
        return rs_release();
    }
    if (!rs.open && !rs.request) {
        return 0;
    }
    fieldstg_stage.event_running = 1;
    if (!rs.open) {
        rs.count = 0;
        rs.cursor = NULL;
        rs.close = 0;
        rs.sel = 0;
        rs.phase = RS_PICK;
        rs.msg = NULL;
        board_open();
        rs.open = 1;
        rs.request = 0;
        rs.fresh = 1;
    }
    if (!rs.fresh) {
        board_input();
    }
    rs.fresh = 0;
    if (rs.close) {
        rs_close();
        rs.releasing = 1;
        return 1;
    }
    rs_run();
    board_draw();
    return 1;
}

void def_respec_savestate(struct PortState *s) {
    /* the board points into the field's heap and at the texts below: all of it goes with the heap */
    PORT_STATE_VAR(s, rs);
    PORT_STATE_VAR(s, val_text);
    PORT_STATE_VAR(s, info1);
    PORT_STATE_VAR(s, info2);
}
