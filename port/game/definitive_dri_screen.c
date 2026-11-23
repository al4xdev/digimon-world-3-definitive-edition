/* The DRI Agents' Gauntlet, the game's side (the rules are definitive_dri.c).
 *
 * A DRI Agent (def_dri_agents; FIELDSTG's talk hook tells def_dri_talk) talks as in the game until his DDNA quest is
 * done (one per agent for the whole game). From then on his talk is the network's own: a line of his and three answers,
 * the simulations, his talk (the game's, run again from the hook with the network out of the way) or leaving. Nothing
 * waits for a talk to end: the list opens from the question only. (It used to open once the field had been idle a while
 * after his talk; walking off and stopping later then opened it out of nowhere.) The list of simulations is in the
 * Guardromon network's layout: the agent's own challenges (DefDriAgent.challenges, two each: no agent holds them all),
 * each line by its Rookie and its Mega, its best stage (or locked), and for the one under the cursor its four stages (level,
 * name, reward) and what unlocking it takes. The confirm button chooses once and acts on a second press:
 * a locked challenge is unlocked (its materials handed over), an open one shows its Tamer's page (def_dri_rival: the
 * classic Tamer whose battle data it replays, and their line), whose confirm starts it.
 * The screen is a field overlay run from
 * FIELDSTG's idle step (def_dri_frame, next to dw3_travel_frame); while it is up fieldstg_stage.event_running stays set,
 * which freezes the player, the map's events and the encounters.
 *
 * Starting a simulation sets up an existing boss battle (fieldstg_start_battle: its kind, immunities and, on a win, its
 * XP and Bits) and then gives records_state the line's first three stages as its three enemies, on the boss battles'
 * scenery and music. It never goes through fieldstg_encounter_start, so no Overlevel Rush, no Rare Target and no hunt
 * chain; escape stays blocked, as in any boss battle. In the battle, the native next-member entrance brings each stage in,
 * and when the third falls the Mega comes in as a wave in a fallen member's slot (def_dri_next_wave); a "DIGIVOLVE!"
 * callout marks each (def_dri_stage_enters). At the battle's end (def_log_end) the stages felled give their
 * tiers' accessories; a lost simulation does not end the game: wfightmn sends it back to the field (def_dri_defeat), with
 * the party's HP and MP as they were before it (a simulation hurts nobody; the items it spent stay spent). Back on the
 * field, a panel tells what the simulation gave.
 *
 * Off in a replay script (def_on): no agent answers and nothing starts. */
#include <string.h>

#include "common.h"

#include "cdload.h"
#include "fieldstg.h"
#include "fightstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "pad.h"
#include "psyq/libgpu.h"
#include "records.h"
#include "sound.h"

#include "psxstack/savestate.h"

#include "definitive.h"
#include "port_runtime.h"

extern int def_on(void);
extern void fieldstg_start_battle(s32 battle);
extern s32 fieldstg_actor_talk_to(FieldstgActor *obj, FieldstgPos *pos);
extern void *font_get_entry(void *data, s32 i);
extern const unsigned char def_str_DriTitle[], def_str_DriSep[], def_str_DriDash[], def_str_DriReward[],
    def_str_DriBest[], def_str_DriClear[], def_str_DriKeysUnlock[], def_str_DriKeysStart[], def_str_DriKeysConfirm[],
    def_str_DriStages[], def_str_DriNewReward[], def_str_DriNoReward[], def_str_DriOk[], def_str_EvoLv[],
    def_str_HuntSpace[], def_str_DriOf[], def_str_DriSep[], def_str_Unknown[], def_str_DriKeysFight[], def_str_DriAgent[],
    def_str_DriGreet[], def_str_DriGreetDone[], def_str_DriAskSim[], def_str_DriAskTalk[], def_str_DriAskBye[],
    def_str_DriThrough[];

/* The layout (320x240), the Guardromon network's colours and panels: a title bar, the list, the chosen challenge's
 * table and the keys; the debrief is one panel over the field. Everything that is read is in font 1 (the 16-pixel
 * dialogue font): font 2's 11 pixels were too small to read on a TV, and its blue on the panels' navy too dark. */
#define DRI_LAYER 0x1002 /* the field's window layer */
#define DRI_LINE 17      /* a row's pitch */
#define DRI_SMALL 15     /* a line of the info panel and the debrief */
#define DRI_TITLE_H 23
#define DRI_ROWS 3       /* challenges listed at once (an agent offers two); the tests' list of all scrolls */
#define DRI_TOP 32       /* the list's first row */
#define DRI_LIST_H (DRI_ROWS * DRI_LINE + 8)
#define DRI_PIP_X 16     /* a row's four stage pips: cleared, open or locked */
#define DRI_PIP 6
#define DRI_NAME_X 52    /* its Rookie and Mega */
#define DRI_HINT_Y 206
#define DRI_INFO_Y (DRI_TOP - 5 + DRI_LIST_H + 3)
#define DRI_INFO_H (DRI_HINT_Y - 2 - DRI_INFO_Y)
/* The info table: a row per stage (the four names do not fit side by side), its level, name and reward. */
#define DRI_COL_LV 10
#define DRI_COL_NAME 56
#define DRI_COL_RW 184
#define DRI_DEB_Y 100    /* the debrief's panel, and the Tamer's page */
#define DRI_DEB_H 136
#define DRI_ASK_Y 128    /* the agent's question */
#define DRI_ASK_H 108
#define DRI_ASK_ROW (DRI_ASK_Y + 58) /* its first answer; the others DRI_SMALL under it */
#define DRI_ASKS 3
#define DRI_TEXT 96      /* bytes of a composed line */
#define DRI_WINDOWS 32
#define DRI_PAL_TEXT 0
#define DRI_PAL_HOT 3    /* chosen once, a reward still to win (the bestiary's orange) */
#define DRI_PAL_DONE 4   /* cleared (its green) */
#define DRI_PAL_DIM 7    /* locked */
#define DRI_KEYS ((1 << 12) | (1 << 13) | (1 << 14)) /* the screens' buttons (PAD_PRESSED's bits) */
#define DRI_SND_MOVE 0x8004513E
#define DRI_SND_OK 0x8004503C
#define DRI_SND_BACK 0x800450BD
/* gamestate_data.progress at which fieldstg_start_battle sends every battle to 0xE0A instead of the battle stage: no
 * simulation then. */
#define DRI_PROGRESS_SPECIAL 0x2B

enum { DRI_LIST, DRI_DEBRIEF, DRI_RIVAL, DRI_ASK };
enum { DRI_IDLE, DRI_CLOSE, DRI_START, DRI_TO_RIVAL, DRI_TO_LIST, DRI_TALK };
enum { DRI_ASK_SIM, DRI_ASK_TALK, DRI_ASK_BYE };

int def_dri_debug = -1; /* tests: 0.. starts that challenge from the idle field, DEF_DRI_DEBUG_MENU opens the list */

static struct {
    s32 request; /* the screen opens on the field's next idle frame with its texts loaded */
    s32 open;
    s32 mode;    /* DRI_LIST, DRI_RIVAL, DRI_DEBRIEF or DRI_ASK */
    s32 ask;     /* the agent's question: the answer under the cursor */
    s32 talk;    /* "Talk" was answered: the agent's own talk once the buttons are up */
    s32 bypass;  /* that talk is under way: def_dri_talk leaves it to the game */
    s32 agent;   /* the DRI Agent talked to (def_dri_agents), -1: every challenge (the tests' list) */
    s32 list[DEF_DRI_CHALLENGES]; /* the challenges the list shows, in its order */
    s32 list_n;
    s32 sel;     /* the row under the cursor (an index of list) */
    s32 top;     /* the list's first row */
    s32 armed;   /* the row the confirm button chose once, or -1 */
    s32 fresh;   /* the frame it opened: the pad is not read (the press that talked may still be down) */
    s32 action;
    s32 releasing; /* closed: the field goes back once the screen's buttons are up (dri_release) */
    s32 count;
    MessageWindow *wins[DRI_WINDOWS];
    MessageWindow *answers[DRI_ASKS];
    MessageWindow *title, *hint, *names[DRI_ROWS], *lv[DEF_DRI_STAGES], *nm[DEF_DRI_STAGES], *rw[DEF_DRI_STAGES], *cost;
    MessageCursor *cursor;
} dri;

/* A simulation, from its start to the field's debrief. No pointers: a save state carries it (a battle loaded from one
 * is still a simulation). */
static struct {
    signed char pending;  /* its battle was set up: the next battle to begin is it */
    signed char fighting; /* that battle runs */
    signed char lost;     /* it was lost: wfightmn sends it back to the field (def_dri_defeat) */
    signed char debrief;  /* its result waits for the field */
    signed char challenge;
    signed char fallen;   /* stages felled */
    signed char result;   /* fightstg_events.result: 1 won, 2 lost */
    signed char count;    /* accessories it gave */
    signed char wave;     /* stages past the battle's members that came in (def_dri_next_wave) */
    short items[DEF_DRI_TIERS];
    short map;            /* the field it left from */
    short party[3];       /* the party's Digimon then, and their HP and MP (restored after a defeat) */
    short hp[3], mp[3];
} dri_sim;

static unsigned char row_name[DRI_ROWS][DRI_TEXT], info_lv[DEF_DRI_STAGES][12], info_best[DRI_TEXT], deb_name[DRI_TEXT],
    deb_stages[DRI_TEXT], deb_reward[2 * DRI_TEXT];

static void list_request(void);

static int idle(void) {
    return !fieldstg_stage.event_running && !fieldstg_stage.battle_starting && !fieldstg_stage.menu_open &&
           !fieldstg_stage.actor_busy && !fieldstg_stage.title_shown;
}

/* ---- Texts. */

static const unsigned char *item_name(int item) {
    void *names = cdload_module.files.get_file(records_language + 0x6A);
    const unsigned char *s = names != NULL ? (const unsigned char *)font_get_entry(names, item) : NULL;

    return s != NULL ? s : def_str_Unknown;
}

static const unsigned char *stage_name(const DefDriChallenge *ch, int i) {
    const unsigned char *name = def_species_name(ch->stages[i].digimon);

    return name != NULL ? name : def_str_Unknown;
}

/* "Agumon ... BK WarGreymon": the line by its Rookie and its Mega (all four names do not fit a row). */
static void put_line(unsigned char *buf, int size, const DefDriChallenge *ch) {
    int p = 0;

    buf[0] = 0;
    def_glyph_put(buf, &p, size, stage_name(ch, 0));
    def_glyph_put(buf, &p, size, def_str_DriThrough);
    def_glyph_put(buf, &p, size, stage_name(ch, DEF_DRI_STAGES - 1));
}

static void put_items(unsigned char *buf, int *p, int size, const short *items, int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (i > 0) {
            def_glyph_put(buf, p, size, def_str_DriSep);
        }
        def_glyph_put(buf, p, size, item_name(items[i]));
    }
}

/* "2 of 3" (the 11-pixel font has no slash). */
static void put_stage(unsigned char *buf, int *p, int size, int n) {
    def_glyph_count(buf, p, size, n);
    def_glyph_put(buf, p, size, def_str_DriOf);
    def_glyph_count(buf, p, size, DEF_DRI_STAGES);
}

/* The three texts the screen reads: the species' records (0x1CF), their names (0x4E) and the items' (0x6A).
 * cdload's is_loading queues a file that is not there, so asking is loading. */
static int texts_loaded(void) {
    s32 a = cdload_module.is_loading(0x1CF);
    s32 b = cdload_module.is_loading(records_language + 0x4E);
    s32 c = cdload_module.is_loading(records_language + 0x6A);

    return a == 0 && b == 0 && c == 0;
}

/* ---- The windows and panels (definitive_travel.c's). */

static MessageWindow *dri_window(s32 font, s32 x, s32 y) {
    MessageWindow *w = message_create_window(DRI_LAYER, font, x, y);

    w->set_ot_depth(w, 1);
    if (dri.count < DRI_WINDOWS) {
        dri.wins[dri.count++] = w;
    }
    return w;
}

/* An opaque rectangle; the layer draws what is added last first, so frames go before fills. */
static void dri_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(DRI_LAYER);
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

static void dri_panel(s32 x, s32 y, s32 w, s32 h, s32 band) {
    if (band) {
        dri_rect(x + 2, y + 2, w - 4, DRI_LINE + 2, 0, 0, 74);
    }
    dri_rect(x + 1, y + 1, w - 2, h - 2, 0, 0, 33);
    dri_rect(x, y, w, h, 107, 107, 214);
}

/* ---- The list. */

/* The challenge on row `row` of the list, -1 past it. */
static int row_challenge(int row) {
    return row >= 0 && row < dri.list_n ? dri.list[row] : -1;
}

static void list_fill(void) {
    const DefDriChallenge *ch;
    int r, c, p, best, pal, open, sel = row_challenge(dri.sel);

    for (r = 0; r < DRI_ROWS; r++) {
        c = row_challenge(dri.top + r);
        ch = def_dri_challenge(c);
        dri.names[r]->set_visible(dri.names[r], ch != NULL);
        if (ch == NULL) {
            continue;
        }
        put_line(row_name[r], DRI_TEXT, ch);
        dri.names[r]->set_text(dri.names[r], row_name[r], -1);
        /* the chosen row is marked by its orange bar: white reads on it, orange would not */
        dri.names[r]->set_palette(dri.names[r],
                                  dri.top + r != dri.armed && !def_record_dri_unlocked(c) ? DRI_PAL_DIM : DRI_PAL_TEXT);
    }
    ch = def_dri_challenge(sel);
    if (ch == NULL) {
        return;
    }
    open = def_record_dri_unlocked(sel);
    best = open ? def_record_dri_best(sel) : 0; /* the best tier */
    /* the table: each stage's level, name and reward (the Rookie's none), green once that stage has been felled */
    for (r = 0; r < DEF_DRI_STAGES; r++) {
        int tier = r - (DEF_DRI_STAGES - DEF_DRI_TIERS), felled = def_dri_tier_stages(best) > r;

        p = 0;
        info_lv[r][0] = 0;
        def_glyph_put(info_lv[r], &p, sizeof(info_lv[r]), def_str_EvoLv);
        def_glyph_put(info_lv[r], &p, sizeof(info_lv[r]), def_str_HuntSpace);
        def_glyph_count(info_lv[r], &p, sizeof(info_lv[r]), ch->stages[r].level);
        dri.lv[r]->set_text(dri.lv[r], info_lv[r], -1);
        dri.nm[r]->set_text(dri.nm[r], (u8 *)stage_name(ch, r), -1);
        dri.rw[r]->set_text(dri.rw[r], (u8 *)(tier >= 0 ? item_name(ch->rewards[tier]) : def_str_DriDash), -1);
        pal = !open ? DRI_PAL_DIM : felled ? DRI_PAL_DONE : DRI_PAL_TEXT;
        dri.lv[r]->set_palette(dri.lv[r], pal);
        dri.nm[r]->set_palette(dri.nm[r], pal);
        dri.rw[r]->set_palette(dri.rw[r], !open || tier < 0 ? DRI_PAL_DIM : felled ? DRI_PAL_DONE : DRI_PAL_HOT);
    }
    if (!open) {
        const unsigned char *cost = def_dri_cost_label(sel, &pal);

        dri.cost->set_text(dri.cost, (u8 *)cost, -1);
        dri.cost->set_palette(dri.cost, pal);
    } else {
        p = 0;
        info_best[0] = 0;
        if (best >= DEF_DRI_TIERS) {
            def_glyph_put(info_best, &p, DRI_TEXT, def_str_DriClear);
        } else {
            def_glyph_put(info_best, &p, DRI_TEXT, def_str_DriBest);
            put_stage(info_best, &p, DRI_TEXT, def_dri_tier_stages(best));
        }
        dri.cost->set_text(dri.cost, info_best, -1);
        dri.cost->set_palette(dri.cost, best >= DEF_DRI_TIERS ? DRI_PAL_DONE : DRI_PAL_TEXT);
    }
    dri.hint->set_text(dri.hint, (u8 *)(dri.armed >= 0 ? def_str_DriKeysConfirm
                                        : open         ? def_str_DriKeysStart
                                                       : def_str_DriKeysUnlock),
                       -1);
    dri.cursor->set_pos(dri.cursor, 4, DRI_TOP + (dri.sel - dri.top) * DRI_LINE);
}

static void list_open(void) {
    s32 r;

    dri.title = dri_window(1, 8, 5);
    dri.title->set_text(dri.title, (u8 *)def_str_DriTitle, -1);
    for (r = 0; r < DRI_ROWS; r++) {
        dri.names[r] = dri_window(1, DRI_NAME_X, DRI_TOP + r * DRI_LINE);
    }
    for (r = 0; r < DEF_DRI_STAGES; r++) {
        dri.lv[r] = dri_window(1, DRI_COL_LV, DRI_INFO_Y + 6 + r * DRI_SMALL);
        dri.nm[r] = dri_window(1, DRI_COL_NAME, DRI_INFO_Y + 6 + r * DRI_SMALL);
        dri.rw[r] = dri_window(1, DRI_COL_RW, DRI_INFO_Y + 6 + r * DRI_SMALL);
    }
    dri.cost = dri_window(1, 10, DRI_INFO_Y + 12 + DEF_DRI_STAGES * DRI_SMALL);
    dri.cost->set_page_lines(dri.cost, 2);
    dri.hint = dri_window(1, 8, DRI_HINT_Y + 3);
    dri.hint->set_page_lines(dri.hint, 2);
    dri.cursor = message_create_cursor(DRI_LAYER, 1, 4, DRI_TOP);
    dri.cursor->show(dri.cursor, 1);
    list_fill();
}

/* A row's four pips, left to right the stages: green felled, blue open, grey while the challenge is locked. */
static void list_pips(s32 r, s32 c) {
    s32 open = def_record_dri_unlocked(c), best = open ? def_dri_tier_stages(def_record_dri_best(c)) : 0, i;
    s32 y = DRI_TOP + r * DRI_LINE + 4;

    for (i = 0; i < DEF_DRI_STAGES; i++) {
        s32 x = DRI_PIP_X + i * (DRI_PIP + 2);

        if (!open) {
            dri_rect(x + 1, y + 1, DRI_PIP - 2, DRI_PIP - 2, 30, 30, 48);
            dri_rect(x, y, DRI_PIP, DRI_PIP, 70, 70, 96);
        } else if (best > i) {
            dri_rect(x, y, DRI_PIP, DRI_PIP, 72, 224, 112);
        } else {
            dri_rect(x + 1, y + 1, DRI_PIP - 2, DRI_PIP - 2, 0, 0, 33);
            dri_rect(x, y, DRI_PIP, DRI_PIP, 130, 150, 255);
        }
    }
}

static void list_draw(void) {
    s32 r;

    /* first queued, drawn last: the pips and the bars over the panels' fill */
    for (r = 0; r < DRI_ROWS; r++) {
        if (def_dri_challenge(row_challenge(dri.top + r)) != NULL) {
            list_pips(r, row_challenge(dri.top + r));
        }
    }
    if (dri.armed >= dri.top && dri.armed < dri.top + DRI_ROWS) {
        dri_rect(5, DRI_TOP + (dri.armed - dri.top) * DRI_LINE - 3, 310, DRI_LINE - 1, 96, 48, 0);
    } else if (dri.sel >= dri.top && dri.sel < dri.top + DRI_ROWS) {
        dri_rect(5, DRI_TOP + (dri.sel - dri.top) * DRI_LINE - 3, 310, DRI_LINE - 1, 16, 32, 120);
    }
    /* the table's lines: under its stage rows, and between the levels and the names */
    dri_rect(8, DRI_INFO_Y + 7 + DEF_DRI_STAGES * DRI_SMALL, 304, 1, 60, 60, 140);
    dri_rect(DRI_COL_NAME - 6, DRI_INFO_Y + 5, 1, DEF_DRI_STAGES * DRI_SMALL, 60, 60, 140);
    dri_panel(0, 0, 320, DRI_TITLE_H, 1);
    dri_panel(2, DRI_TOP - 6, 316, DRI_LIST_H, 0);
    dri_panel(2, DRI_INFO_Y, 316, DRI_INFO_H, 0);
    dri_panel(0, DRI_HINT_Y, 320, 240 - DRI_HINT_Y, 0);
}

static void list_move(int sel) {
    if (sel == dri.sel) {
        return;
    }
    dri.sel = sel;
    if (dri.sel < dri.top) {
        dri.top = dri.sel;
    } else if (dri.sel >= dri.top + DRI_ROWS) {
        dri.top = dri.sel - DRI_ROWS + 1;
    }
    dri.armed = -1;
    sound_module.play(DRI_SND_MOVE);
    list_fill();
}

static void list_input(void) {
    int n = dri.list_n;
    int c = row_challenge(dri.sel);

    if (n <= 0) { /* an empty table: only Back */
        if (PAD_PRESSED(12) || PAD_PRESSED(13) || PAD_PRESSED(14)) {
            dri.action = DRI_CLOSE;
        }
        return;
    }
    if (PAD_PRESSED(4) || PAD_REPEAT(4)) {
        list_move((dri.sel + n - 1) % n);
        return;
    }
    if (PAD_PRESSED(6) || PAD_REPEAT(6)) {
        list_move((dri.sel + 1) % n);
        return;
    }
    if (PAD_PRESSED(13)) {
        if (!def_record_dri_unlocked(c) && !def_dri_can_unlock(c)) { /* short of its materials */
            sound_module.play(DRI_SND_BACK);
        } else if (dri.armed != dri.sel) {
            dri.armed = dri.sel;
            sound_module.play(DRI_SND_OK);
            list_fill();
        } else if (!def_record_dri_unlocked(c)) {
            def_dri_unlock(c);
            dri.armed = -1;
            sound_module.play(DRI_SND_OK);
            list_fill();
        } else {
            sound_module.play(DRI_SND_OK);
            dri.action = def_dri_rival(c, DEF_DRI_RIVAL_HELLO) != NULL ? DRI_TO_RIVAL : DRI_START;
        }
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        sound_module.play(DRI_SND_BACK);
        if (dri.armed >= 0) {
            dri.armed = -1;
            list_fill();
        } else {
            dri.action = DRI_CLOSE;
        }
    }
}

/* ---- The debrief: what the simulation just fought gave. */

static void debrief_open(void) {
    const DefDriChallenge *ch = def_dri_challenge(dri_sim.challenge);
    const unsigned char *say;
    MessageWindow *w;
    int p = 0;

    dri_sim.debrief = 0; /* shown once */
    if (ch == NULL) {
        return;
    }
    put_line(deb_name, DRI_TEXT, ch);
    w = dri_window(1, 18, DRI_DEB_Y + 5);
    w->set_text(w, deb_name, -1);
    deb_stages[0] = 0;
    def_glyph_put(deb_stages, &p, DRI_TEXT, def_str_DriStages);
    put_stage(deb_stages, &p, DRI_TEXT, dri_sim.fallen);
    w = dri_window(1, 18, DRI_DEB_Y + 25);
    w->set_text(w, deb_stages, -1);
    w->set_palette(w, dri_sim.fallen >= DEF_DRI_STAGES ? DRI_PAL_DONE : DRI_PAL_TEXT);
    say = def_dri_rival(dri_sim.challenge, dri_sim.fallen >= DEF_DRI_STAGES ? DEF_DRI_RIVAL_WON : DEF_DRI_RIVAL_LOST);
    if (say != NULL) { /* the Tamer's word on it: a full clear, or anything less */
        w = dri_window(1, 18, DRI_DEB_Y + 45);
        w->set_page_lines(w, 2);
        w->set_text(w, (u8 *)say, -1);
    }
    p = 0;
    deb_reward[0] = 0;
    if (dri_sim.count > 0) {
        static const unsigned char line_break[] = { 2, 1, 0 };

        def_glyph_put(deb_reward, &p, sizeof(deb_reward), def_str_DriNewReward);
        def_glyph_put(deb_reward, &p, sizeof(deb_reward), line_break);
        if (dri_sim.count > 2) {
            put_items(deb_reward, &p, sizeof(deb_reward), dri_sim.items, 2);
            def_glyph_put(deb_reward, &p, sizeof(deb_reward), line_break);
            put_items(deb_reward, &p, sizeof(deb_reward), dri_sim.items + 2, dri_sim.count - 2);
        } else {
            put_items(deb_reward, &p, sizeof(deb_reward), dri_sim.items, dri_sim.count);
        }
    } else {
        def_glyph_put(deb_reward, &p, sizeof(deb_reward), def_str_DriNoReward);
    }
    w = dri_window(1, 18, DRI_DEB_Y + 77);
    w->set_page_lines(w, 3);
    w->set_text(w, deb_reward, -1);
    w->set_palette(w, dri_sim.count > 0 ? DRI_PAL_HOT : DRI_PAL_TEXT);
    w = dri_window(1, 18, DRI_DEB_Y + 119);
    w->set_text(w, (u8 *)def_str_DriOk, -1);
}

/* ---- The Tamer's page: before an open challenge starts, whose battle data it replays and their line. */

static void rival_open(void) {
    const DefDriChallenge *ch = def_dri_challenge(row_challenge(dri.sel));
    const unsigned char *name = def_dri_rival(row_challenge(dri.sel), DEF_DRI_RIVAL_NAME);
    const unsigned char *hello = def_dri_rival(row_challenge(dri.sel), DEF_DRI_RIVAL_HELLO);
    MessageWindow *w;

    if (ch == NULL || name == NULL || hello == NULL) {
        return;
    }
    w = dri_window(1, 18, DRI_DEB_Y + 5);
    w->set_text(w, (u8 *)name, -1);
    w->set_palette(w, DRI_PAL_HOT);
    put_line(deb_name, DRI_TEXT, ch);
    w = dri_window(1, 18, DRI_DEB_Y + 23);
    w->set_text(w, deb_name, -1);
    w = dri_window(1, 18, DRI_DEB_Y + 47);
    w->set_page_lines(w, 2);
    w->set_text(w, (u8 *)hello, -1);
    w = dri_window(1, 18, DRI_DEB_Y + 96);
    w->set_page_lines(w, 2);
    w->set_text(w, (u8 *)def_str_DriKeysFight, -1);
}

static void rival_input(void) {
    if (PAD_PRESSED(13)) {
        sound_module.play(DRI_SND_OK);
        dri.action = DRI_START;
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        sound_module.play(DRI_SND_BACK);
        dri.action = DRI_TO_LIST;
    }
}

/* ---- The agent's question: his line, then the simulations, his own talk or leaving. */

static void list_build(void);

/* Every challenge the agent offers is cleared (his greeting says so). */
static int all_cleared(void) {
    int r;

    list_build();
    for (r = 0; r < dri.list_n; r++) {
        if (def_record_dri_best(dri.list[r]) < DEF_DRI_TIERS) {
            return 0;
        }
    }
    return dri.list_n > 0;
}

static void ask_fill(void) {
    s32 i;

    for (i = 0; i < DRI_ASKS; i++) {
        dri.answers[i]->set_palette(dri.answers[i], i == dri.ask ? DRI_PAL_HOT : DRI_PAL_TEXT);
    }
    dri.cursor->set_pos(dri.cursor, 26, DRI_ASK_ROW + dri.ask * DRI_SMALL);
}

static void ask_open(void) {
    static const unsigned char *const answers[DRI_ASKS] = { def_str_DriAskSim, def_str_DriAskTalk, def_str_DriAskBye };
    MessageWindow *w;
    s32 i;

    w = dri_window(1, 18, DRI_ASK_Y + 5);
    w->set_text(w, (u8 *)def_str_DriAgent, -1);
    w->set_palette(w, DRI_PAL_HOT);
    w = dri_window(1, 18, DRI_ASK_Y + 25);
    w->set_page_lines(w, 2);
    w->set_text(w, (u8 *)(all_cleared() ? def_str_DriGreetDone : def_str_DriGreet), -1);
    for (i = 0; i < DRI_ASKS; i++) {
        dri.answers[i] = dri_window(1, 42, DRI_ASK_ROW + i * DRI_SMALL);
        dri.answers[i]->set_text(dri.answers[i], (u8 *)answers[i], -1);
    }
    dri.cursor = message_create_cursor(DRI_LAYER, 1, 26, DRI_ASK_ROW);
    dri.cursor->show(dri.cursor, 1);
    ask_fill();
}

static void ask_input(void) {
    if (PAD_PRESSED(4) || PAD_REPEAT(4) || PAD_PRESSED(6) || PAD_REPEAT(6)) {
        dri.ask = (dri.ask + (PAD_PRESSED(4) || PAD_REPEAT(4) ? DRI_ASKS - 1 : 1)) % DRI_ASKS;
        sound_module.play(DRI_SND_MOVE);
        ask_fill();
    } else if (PAD_PRESSED(13)) {
        sound_module.play(dri.ask == DRI_ASK_BYE ? DRI_SND_BACK : DRI_SND_OK);
        dri.action = dri.ask == DRI_ASK_SIM ? DRI_TO_LIST : dri.ask == DRI_ASK_TALK ? DRI_TALK : DRI_CLOSE;
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        sound_module.play(DRI_SND_BACK);
        dri.action = DRI_CLOSE;
    }
}

static void ask_draw(void) {
    dri_rect(14, DRI_ASK_ROW + dri.ask * DRI_SMALL - 3, 292, DRI_SMALL, 16, 32, 120); /* over the panel's fill */
    dri_panel(8, DRI_ASK_Y, 304, DRI_ASK_H, 1);
}

/* ---- The screen. */

static void dri_open(void) {
    dri.count = 0;
    dri.cursor = NULL;
    dri.armed = -1;
    dri.action = DRI_IDLE;
    if (dri.mode == DRI_LIST) {
        list_open();
    } else if (dri.mode == DRI_RIVAL) {
        rival_open();
    } else if (dri.mode == DRI_ASK) {
        ask_open();
    } else {
        debrief_open();
    }
    dri.open = 1;
    dri.request = 0;
    dri.fresh = 1;
    port_log("dri: frame %ld: the %s of map 0x%X", port_frames,
             dri.mode == DRI_LIST ? "list" : dri.mode == DRI_RIVAL ? "Tamer page" : dri.mode == DRI_ASK ? "agent's question" : "debrief", gamestate_data.funcs.get_map());
}

static void dri_close(void) {
    s32 i;

    for (i = 0; i < dri.count; i++) {
        heap_objects.stop((Object *)dri.wins[i]);
        dri.wins[i] = NULL;
    }
    dri.count = 0;
    if (dri.cursor != NULL) {
        heap_objects.stop((Object *)dri.cursor);
        dri.cursor = NULL;
    }
    dri.open = 0;
    dri.request = 0;
}

/* The windows belong to no object's data block, so nothing runs them but this (as the network's screen does). */
static void dri_run(void) {
    s32 i;

    for (i = 0; i < dri.count; i++) {
        heap_objects.try_run((Object *)dri.wins[i]);
    }
    if (dri.cursor != NULL) {
        heap_objects.try_run((Object *)dri.cursor);
    }
}

/* The simulation's battle: the template (an existing boss battle) as fieldstg_start_battle sets it up, then the line's
 * stages as its three enemies. 1 when the field is leaving for it. */
static int sim_start(int c) {
    const DefDriChallenge *ch = def_dri_challenge(c);
    int i, id;

    if (ch == NULL || fieldstg_stage.battle_starting || gamestate_data.progress == DRI_PROGRESS_SPECIAL) {
        return 0;
    }
    fieldstg_start_battle(ch->battle);
    if (!fieldstg_stage.battle_starting) { /* no field manager: nothing was set up */
        port_log("dri: challenge %d not started (no field manager)", c);
        return 0;
    }
    records_state.stage = DEF_DRI_STAGE;
    records_state.music = DEF_DRI_MUSIC;
    for (i = 0; i < DEF_DRI_MEMBERS; i++) { /* the rest come in as waves (def_dri_next_wave) */
        RecordsEnemy *e = &records_state.enemies[i];

        e->digimon = ch->stages[i].digimon;
        e->level = ch->stages[i].level;
        e->hp = ch->stages[i].hp;
        e->mp = ch->stages[i].mp;
        e->stat_scale = ch->stages[i].stat_scale;
    }
    records_state.blocked[11] = 1; /* no escape, as a boss's: a defeat is the way out, and it costs nothing */
    records_state.has_prize = 0;
    memset(&dri_sim, 0, sizeof(dri_sim));
    dri_sim.pending = 1;
    dri_sim.challenge = (signed char)c;
    dri_sim.map = (short)gamestate_data.funcs.get_map();
    for (i = 0; i < 3; i++) {
        id = gamestate_data.funcs.get_party_member(i);
        dri_sim.party[i] = (short)id;
        if (id >= 0) {
            dri_sim.hp[i] = gamestate_data.digimon[id].record.stats.values[2];
            dri_sim.mp[i] = gamestate_data.digimon[id].record.stats.values[4];
        }
    }
    port_log("dri: challenge %d started from map 0x%X (battle %d: %d, %d, %d, then %d at %d, %d, %d, %d)", c, dri_sim.map,
             ch->battle, ch->stages[0].digimon, ch->stages[1].digimon, ch->stages[2].digimon, ch->stages[3].digimon,
             ch->stages[0].level, ch->stages[1].level, ch->stages[2].level, ch->stages[3].level);
    return 1;
}

/* Closes the screen and carries out the choice: the simulation's battle, or the field handed back. */
static void dri_finish(void) {
    s32 action = dri.action;

    dri.action = DRI_IDLE;
    dri_close();
    if (action == DRI_TO_RIVAL) { /* the other page, on the next frame: the field stays held */
        dri.mode = DRI_RIVAL;
        dri.request = 1;
        return;
    }
    if (action == DRI_TO_LIST) {
        list_request();
        return;
    }
    dri.talk = action == DRI_TALK;
    if (action == DRI_START && sim_start(row_challenge(dri.sel))) {
        return; /* fieldstg_start_battle keeps the field frozen until the battle */
    }
    dri.releasing = 1;
}

/* The field back to the player once none of the screens' buttons is down. Handed back in the frame of the press that
 * closed the screen, the press reached the player's control too, and CIRCLE there examines what the player faces: after
 * the debrief (closed by CIRCLE) that left the player stuck (fieldstg_stage.actor_busy). */
static int dri_release(void) {
    FieldstgActor *player;

    if (pad_state.get_held(0) & DRI_KEYS) {
        fieldstg_stage.event_running = 1;
        return 1;
    }
    dri.releasing = 0;
    fieldstg_stage.event_running = 0;
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player == NULL) {
        dri.talk = 0;
        return 1;
    }
    if (dri.talk) {
        /* "Talk": the agent's own talk, as the player's press would start it (he has not moved since) */
        FieldstgPos front;

        dri.talk = 0;
        dri.bypass = 1;
        player->get_front(player, &front);
        if (fieldstg_actor_talk_to(player, &front)) {
            dri.bypass = 0;
            return 1;
        }
        dri.bypass = 0;
    }
    player->reset_control(player);
    return 1;
}

/* The agent's challenges (def_dri_agents' own), or all of them for dri.agent -1. */
static void list_build(void) {
    int i, c;

    dri.list_n = 0;
    if (dri.agent >= 0 && dri.agent < def_dri_agent_count) {
        for (i = 0; i < DEF_DRI_PER_AGENT; i++) {
            c = def_dri_agents[dri.agent].challenges[i];
            if (def_dri_challenge(c) != NULL) {
                dri.list[dri.list_n++] = c;
            }
        }
        return;
    }
    for (c = 0; c < DEF_DRI_CHALLENGES && def_dri_challenge(c) != NULL; c++) {
        dri.list[dri.list_n++] = c;
    }
}

static void list_request(void) {
    dri.request = 1;
    dri.mode = DRI_LIST;
    list_build();
    if (dri.sel < 0 || dri.sel >= dri.list_n) {
        dri.sel = 0;
    }
    dri.top = dri.sel >= DRI_ROWS ? dri.sel - DRI_ROWS + 1 : 0;
    fieldstg_stage.event_running = 1; /* the player stands still from the talk on */
}

/* ---- The agents' own DDNA quests (the owner's, 2026-11-17).
 *
 * In the game an agent gives his quest only inside a window of the party's level sum (below it he is "too busy", over
 * it the party "doesn't need" his Digimon), so a strong party could never get it. Each talk with an agent rewrites his
 * talk entries' conditions in the stage's data before the game picks the entry: the quest line (the entry that sets
 * the quest flag) loses its level conditions, and the entries that hold the player off by level never match. The
 * rewrite writes absolute values, so doing it on every talk is the same as doing it once, and a replay script (def_on
 * 0) never gets here. The Gauntlet's list opens only once the agent's own Rookie has joined: his quest is done, or the
 * Rookie was a starter, so the canonical quest and the simulator never share a talk. */
typedef struct DriCondition { /* gamestate_conditions' entry (gamestate.c's GamestateCondition) */
    u8 id;
    u8 type;
    u8 arg;
} DriCondition;
extern DriCondition gamestate_conditions[];

#define DRI_FLAG_COND 0x70     /* gamestate_get_flag's type of a gamestate_conditions entry */
#define DRI_COND_JOINED 0x11   /* cond_digimon 1: Rookie `arg` has joined */
#define DRI_COND_JOIN 0x13     /* cond_digimon 3: Rookie `arg` joins (the hand-in's flag) */
#define DRI_COND_LEVEL 0x15    /* cond_digimon 5: the party's level sum is at least arg * 15 + 30 */
#define DRI_COND_UNJOINED 0x16 /* cond_digimon 6: Rookie `arg` has not joined */
#define DRI_QUEST_FLAG_LO 0x600 /* the quest flags the agents' quest lines set (one per agent, 0x600..0x607) */
#define DRI_QUEST_FLAG_HI 0x607
#define DRI_ALWAYS 0x5000 /* a flag type gamestate_get_flag does not know: it holds whatever the value */
#define DRI_NEVER 0x61FF  /* with value 1: progress == 511, never */

/* The gamestate_conditions entry a (flag, value) pair checks, NULL when it is not a condition. */
static const DriCondition *dri_condition(u16 flag) {
    const DriCondition *c;

    if (((flag >> 8) & ~1) != DRI_FLAG_COND) {
        return NULL;
    }
    for (c = gamestate_conditions; c->id != 0xFF; c++) {
        if (c->id == (flag & 0x1FF)) {
            return c;
        }
    }
    return NULL;
}

static int dri_sets_quest(const u16 *list) {
    for (; list != NULL && *list != 0xFFFF; list += 2) {
        if (*list >= DRI_QUEST_FLAG_LO && *list <= DRI_QUEST_FLAG_HI) {
            return 1;
        }
    }
    return 0;
}

/* The agent's talk with its level window lifted (see above). */
static void dri_quest_open(const FieldstgPlacedActor *placed) {
    FieldstgTalk *t;
    u16 *p;
    int quest;

    for (t = placed->talks; t != NULL && (t->flags_required != NULL || t->flags_set != NULL || t->message != 0); t++) {
        quest = dri_sets_quest(t->flags_set);
        for (p = t->flags_required; p != NULL && *p != 0xFFFF; p += 2) {
            const DriCondition *c = dri_condition(*p);

            if (c == NULL || c->type != DRI_COND_LEVEL) {
                continue;
            }
            if (quest) {
                p[0] = DRI_ALWAYS;
                p[1] = 1;
            } else {
                p[0] = DRI_NEVER; /* one never-pair is enough: the entry is out */
                p[1] = 1;
                break;
            }
        }
    }
}

/* The agent's Rookie, read from his own talk (the joined / not joined / join conditions); -1: none (not an agent). */
static int dri_rookie(const FieldstgPlacedActor *placed) {
    const FieldstgTalk *t;
    const u16 *p;

    for (t = placed->talks; t != NULL && (t->flags_required != NULL || t->flags_set != NULL || t->message != 0); t++) {
        const u16 *lists[2] = { t->flags_required, t->flags_set };
        int k;

        for (k = 0; k < 2; k++) {
            for (p = lists[k]; p != NULL && *p != 0xFFFF; p += 2) {
                const DriCondition *c = dri_condition(*p);

                if (c != NULL && (c->type == DRI_COND_JOINED || c->type == DRI_COND_JOIN || c->type == DRI_COND_UNJOINED) &&
                    c->arg < 8) {
                    return c->arg;
                }
            }
        }
    }
    return -1;
}

int def_dri_talk(s32 map, s32 actor) {
    FieldstgActor *agent;
    int rookie = -1, who = def_dri_agent_index(map, actor);

    if (who < 0) {
        return 0;
    }
    agent = (FieldstgActor *)heap_objects.find(5, actor, 1);
    if (agent != NULL && agent->placed != NULL) {
        dri_quest_open(agent->placed);
        rookie = dri_rookie(agent->placed);
    }
    if (dri.bypass) { /* "Talk" was answered: this is his own talk, the game's */
        return 0;
    }
    if (dri.open || dri.request || dri.releasing || dri_sim.pending || gamestate_data.progress == DRI_PROGRESS_SPECIAL) {
        return 0;
    }
    if (rookie < 0 || gamestate_data.digimon[rookie].joined == 0) {
        port_log("dri: the DRI Agent %d of map 0x%X: his DDNA quest first (Rookie %d not joined), his own talk", actor,
                 map, rookie);
        return 0;
    }
    port_log("dri: the DRI Agent %d of map 0x%X: the network's question", actor, map);
    if (who != dri.agent) { /* another agent's list: from its top */
        dri.sel = 0;
    }
    dri.agent = who;
    dri.request = 1;
    dri.mode = DRI_ASK;
    dri.ask = DRI_ASK_SIM;
    fieldstg_stage.event_running = 1; /* frozen from this frame on, as the screen will hold it */
    return 1;
}

/* Tests: the player 24 pixels under the first DRI Agent of the map, so UP then the confirm button talks to him. */
static void dri_debug_agent(void) {
    FieldstgActor *player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    s32 map = gamestate_data.funcs.get_map(), i;

    for (i = 0; i < def_dri_agent_count && player != NULL; i++) {
        FieldstgActor *agent;

        if (def_dri_agents[i].map != map || (agent = (FieldstgActor *)heap_objects.find(5, def_dri_agents[i].actor, 1)) == NULL) {
            continue;
        }
        player->pos.x = agent->pos.x;
        player->pos.y = agent->pos.y + (24 << 8);
        port_log("dri: debug: the player under the DRI Agent %d of map 0x%X", def_dri_agents[i].actor, map);
        return;
    }
    /* none here (or not placed at this point of the story): to Asuka's Kazuki (wstag330: 43 at 737, 954), then ask again */
    port_log("dri: debug: no DRI Agent on map 0x%X: to map 0x21D", map);
    fieldstg_goto_map(0x21D, -1, 737 << 8, (954 + 24) << 8, 0);
}

int def_dri_frame(void) {
    if (!def_on()) {
        return 0;
    }
    if (dri.releasing) {
        return dri_release();
    }
    if (!dri.open && !dri.request) {
        if (def_dri_debug >= 0 && idle()) {
            int d = def_dri_debug;

            def_dri_debug = -1;
            if (d == DEF_DRI_DEBUG_MENU) {
                dri.agent = -1;
                list_request();
            } else if (d == DEF_DRI_DEBUG_AGENT) {
                dri_debug_agent();
            } else if (sim_start(d)) {
                return 1;
            }
        } else if (dri_sim.debrief && idle()) {
            if (dri_sim.map != gamestate_data.funcs.get_map()) { /* not the field it left from: a load, a reset */
                dri_sim.debrief = 0;
                return 0;
            }
            dri.request = 1;
            dri.mode = DRI_DEBRIEF;
            dri_sim.lost = 0; /* back on the field: the defeat's redirect is done */
        }
        if (!dri.request) {
            return 0;
        }
    }
    fieldstg_stage.event_running = 1;
    if (!dri.open) {
        if (!texts_loaded()) {
            return 1;
        }
        dri_open();
    }
    dri.action = DRI_IDLE;
    if (!dri.fresh) {
        if (dri.mode == DRI_LIST) {
            list_input();
        } else if (dri.mode == DRI_RIVAL) {
            rival_input();
        } else if (dri.mode == DRI_ASK) {
            ask_input();
        } else if (PAD_PRESSED(13) || PAD_PRESSED(12) || PAD_PRESSED(14)) {
            sound_module.play(DRI_SND_OK);
            dri.action = DRI_CLOSE;
        }
    }
    dri.fresh = 0;
    if (dri.action != DRI_IDLE) {
        dri_finish();
        return 1;
    }
    dri_run();
    if (dri.mode == DRI_LIST) {
        list_draw();
    } else if (dri.mode == DRI_ASK) {
        ask_draw();
    } else {
        dri_panel(8, DRI_DEB_Y, 304, DRI_DEB_H, 1);
    }
    return 1;
}

/* ---- The battle's hooks. */

void def_dri_battle_begin(void) {
    const DefDriChallenge *ch;

    dri_sim.fighting = 0;
    dri_sim.lost = 0;
    if (!dri_sim.pending) {
        return;
    }
    dri_sim.pending = 0;
    ch = def_dri_challenge(dri_sim.challenge);
    /* only the battle sim_start set up: anything else that began meanwhile is not a simulation */
    if (ch != NULL && records_state.battle == ch->battle && records_state.enemies[0].digimon == ch->stages[0].digimon) {
        dri_sim.fighting = 1;
        port_log("dri: challenge %d: the battle begins", dri_sim.challenge);
    }
}

int def_dri_fighting(void) {
    return dri_sim.fighting;
}

/* The DRI challenge being fought (the enemy personas' DRI), -1 none. */
int def_dri_challenge_fighting(void) {
    return dri_sim.fighting ? dri_sim.challenge : -1;
}

/* A stage that is a player form (DefDriStage.base): the base species' disc record (with the port's table on it, so the
 * line's balance is the generator's) under the form's name, type and techniques, and a plan of its own (below). Asked by
 * fightstg_enemy_get_record only for an ID with no disc record, while a simulation is set up or runs. */
/* The battle reads an enemy's cameras from its model's type B record (fightstg_camera_get_preset, _get_default); a
 * player form's is type A (12 cameras, laid out otherwise), so read as B it puts the camera nowhere and the 3D goes black.
 * The form's model stays; its cameras are the base species' (a body of the same kind). */
s32 def_enemy_camera_model(s32 model_id) {
    const DefDriChallenge *ch;
    int i, c;

    if ((dri_sim.pending || dri_sim.fighting) && (ch = def_dri_challenge(dri_sim.challenge)) != NULL) {
        for (i = 0; i < DEF_DRI_STAGES; i++) {
            if (ch->stages[i].digimon == model_id && ch->stages[i].base != 0) {
                return ch->stages[i].base;
            }
        }
    }
    if (def_purify_fighting()) {
        for (c = 0; c < def_dri_challenge_count; c++) {
            ch = def_dri_challenge(c);
            for (i = 0; ch != NULL && i < DEF_DRI_STAGES; i++) {
                if (ch->stages[i].digimon == model_id && ch->stages[i].base != 0) {
                    return ch->stages[i].base;
                }
            }
        }
        return 448; /* BK WarGreymon: Type B bipedal Mega with valid battle cameras */
    }
    return model_id;
}

extern s32 fightstg_enemy_find_record(s32 id);

/* The form's techniques as an enemy's. techniques[0] is its attack and techniques[6] its signature (Nova Blast, Terra Force,
 * Seven Heavens...): the one a player's form had to itself until now. The base's own plan is not reused: the disc's plans
 * for plain species (Tyrannomon's is three plain attacks) never reach tech_3, so the signature would never be cast.
 * The plan is BK MegaGargomon's (a boss of the same rank): the signature every third turn, else tech_2 about 3 times in 8,
 * else the attack. tech_2 is the form's strongest single-hit damaging technique of the middle slots, not slot 4 as the
 * purification takes it (that one is often a buff, a heal or an antidote). */
/* The DNA results have no techniques of their own (stubs, def_form_record): an attack of one of their pairs' forms (the
 * forms' plain attacks are all one record, 100 power and the same animation) and their DNA technique as the signature. */
static const struct {
    short form, attack, signature;
} dri_dna[] = {
    { 468, 0x0C, 0x1B0 }, /* Agunimon: Greymon's attack, Agunimon's DNA */
    { 469, 0x0B, 0x1B1 }, /* BladeGarurumon: Grizzmon's, BladeGarurumon's DNA */
    { 470, 0x32, 0x1B2 }, /* AncientGreymon: Imperialdramon-P's, AncientGreymon's DNA */
};

static void dri_form_techniques(FightstgEnemyRecord *r, const RecordsDigimon *form) {
    static const FightstgEnemyAction plan[3] = { { 3, 18, 3 }, { 2, 1, 48 }, { 1, 0, 0 } };
    int j, best = 0, best_score = 0;

    r->tech = (s16)form->techniques[0];
    r->tech_3 = (s16)form->techniques[6];
    for (j = 0; r->tech == 0 && j < (int)(sizeof(dri_dna) / sizeof(dri_dna[0])); j++) {
        if (dri_dna[j].form == form->id) {
            r->tech = dri_dna[j].attack;
            r->tech_3 = dri_dna[j].signature;
        }
    }
    for (j = 1; j <= 5; j++) {
        int id = form->techniques[j];
        const RecordsTechnique *t = id > 0 ? &records_techniques[id - 1] : NULL;

        /* target 2: one enemy of the caster; kind 1: plain damage (kind 0 and 2+ are items' and special effects') */
        if (t != NULL && t->target == 2 && t->kind == 1 && t->power * t->hit_count > best_score) {
            best = id;
            best_score = t->power * t->hit_count;
        }
    }
    r->tech_2 = (s16)best;
    if (r->tech_2 == 0 && r->tech_3 != 0) { /* a form with no such technique: the signature stands for it */
        r->tech_2 = r->tech_3;
    }
    if (r->tech_3 != 0) {
        memcpy(r->actions, plan, sizeof(plan));
    }
}

FightstgEnemyRecord *def_dri_record(s32 id) {
    static FightstgEnemyRecord made[DEF_DRI_STAGES];
    const DefDriChallenge *ch;
    FightstgEnemyRecord *recs = (FightstgEnemyRecord *)cdload_module.files.get_file(0x1CF);
    int i, b;

    if (!(dri_sim.pending || dri_sim.fighting) || recs == NULL ||
        (ch = def_dri_challenge(dri_sim.challenge)) == NULL) {
        return NULL;
    }
    for (i = 0; i < DEF_DRI_STAGES; i++) {
        const DefDriStage *st = &ch->stages[i];
        RecordsDigimon *form;
        FightstgEnemyRecord *r = &made[i];

        if (st->digimon != id || st->base == 0 || (form = def_form_record(id)) == NULL ||
            (b = fightstg_enemy_find_record(st->base)) < 0) {
            continue;
        }
        if (form->techniques[0] == 0 && form->techniques[6] == 0) {
            int j, known = 0;

            for (j = 0; j < (int)(sizeof(dri_dna) / sizeof(dri_dna[0])); j++) {
                known |= dri_dna[j].form == form->id;
            }
            if (!known) { /* a form with no attack at all would read records_techniques[-1] */
                continue;
            }
        }
        *r = recs[b];
        def_enemy_override(r);
        r->id = (s16)id;
        r->name = form->name_id;
        r->type = form->type;
        r->item = 0; /* a simulation drops nothing */
        dri_form_techniques(r, form);
        return r;
    }
    return NULL;
}

/* The battle's last enemy member fell (def_enemy_next_wave, from wfightmn_knockout): the next stage past the members (the
 * Mega), set up in a fallen enemy slot as sim_start and wfightmn_init_members set a member up at the start (its record,
 * stats and cameras are found by its species, so nothing of the one that fell there is kept), or -1 when none is left
 * (the battle ends). The entrance that follows loads its model. The slot is the one after the member that just fell,
 * never its own: the HUD renames the enemy and resets its HP bar only when current[1] changes (fightstg_hud_update_names,
 * _update_hp), so a wave in the same slot kept the fallen one's name and empty bar. */
int def_dri_next_wave(void) {
    extern int def_super_hp(int hp);
    const DefDriChallenge *ch;
    const DefDriStage *st;
    int slot = (fightstg_battle.state.current[1] + 1) % 3, k = DEF_DRI_MEMBERS + dri_sim.wave;
    FightstgMember *m = &fightstg_battle.state.members[1][slot];
    RecordsEnemy *e = &records_state.enemies[slot];

    if (!dri_sim.fighting || (ch = def_dri_challenge(dri_sim.challenge)) == NULL || k >= DEF_DRI_STAGES) {
        return -1;
    }
    st = &ch->stages[k];
    memset(m, 0, sizeof(*m));
    m->digimon = st->digimon;
    m->hp = m->max_hp = (s16)def_super_hp(st->hp);
    m->mp = m->max_mp = st->mp;
    m->item = 0; /* a simulation drops nothing (and the results' drop draw reads every slot's) */
    e->digimon = st->digimon;
    e->level = st->level;
    e->hp = st->hp;
    e->mp = st->mp;
    e->stat_scale = st->stat_scale;
    dri_sim.wave++;
    port_log("dri: challenge %d: stage %d (species %d, level %d, HP %d) comes in as a wave in enemy slot %d",
             dri_sim.challenge, k, st->digimon, st->level, st->hp, slot);
    return slot;
}

void def_dri_stage_enters(int member) {
    if (!dri_sim.fighting) {
        return;
    }
    def_callout_show(DEF_CALLOUT_EVOLVE, 1);
    port_log("dri: challenge %d: stage %d enters (species %d)", dri_sim.challenge, member,
             fightstg_battle.state.members[1][member % 3].digimon);
}

void def_dri_battle_end(int result) {
    short items[DEF_DRI_TIERS];
    int i, n, fallen = 0;

    if (!dri_sim.fighting) {
        return;
    }
    dri_sim.fighting = 0;
    for (i = 0; i < 3; i++) {
        FightstgMember *m = &fightstg_battle.state.members[1][i];

        if (m->digimon != 0 && m->hp <= 0) {
            fallen++;
        }
    }
    fallen += dri_sim.wave; /* each wave took a fallen member's slot: the stage that fell there counts too */
    n = def_dri_award(dri_sim.challenge, fallen, items);
    for (i = 0; i < n; i++) { /* counted at once: the results screen and the field come after */
        if (items[i] > 0 && items[i] < 0x193 && gamestate_data.items[items[i]] < 99) {
            gamestate_data.items[items[i]]++;
        }
        dri_sim.items[i] = items[i];
    }
    dri_sim.count = (signed char)n;
    dri_sim.fallen = (signed char)fallen;
    dri_sim.result = (signed char)result;
    dri_sim.lost = result == 2;
    dri_sim.debrief = 1;
    port_log("dri: challenge %d ended: result %d, %d stages felled, best %d, %d new rewards (%d %d %d)", dri_sim.challenge,
             result, fallen, def_record_dri_best(dri_sim.challenge), n, n > 0 ? items[0] : 0, n > 1 ? items[1] : 0,
             n > 2 ? items[2] : 0);
}

int def_dri_defeat(void) {
    int i, id;

    if (dri_sim.fighting) { /* WFIGHTMN picks the next map before the battle's log ends it (def_log_end): end it here */
        def_dri_battle_end(2);
    }
    /* WFIGHTMN asks every frame until the map changes, so this stays 1 (and the party restored) until the next battle or
     * the field's debrief clears it: a 0 on a later frame would let the game over's 0xE00 overwrite the field */
    if (!dri_sim.lost) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        id = dri_sim.party[i];
        if (id >= 0 && id == gamestate_data.funcs.get_party_member(i)) {
            gamestate_data.digimon[id].record.stats.values[2] = dri_sim.hp[i];
            gamestate_data.digimon[id].record.stats.values[4] = dri_sim.mp[i];
        }
    }
    port_log("dri: challenge %d lost: back to map 0x%X, the party as it went in", dri_sim.challenge,
             gamestate_data.field_map);
    return 1;
}

void def_dri_reset(void) {
    memset(&dri_sim, 0, sizeof(dri_sim));
    memset(&dri, 0, sizeof(dri));
}

void def_dri_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, dri_sim);
    /* The list's screen points into the field's heap and at the texts below: all of it goes with the heap. */
    PORT_STATE_VAR(s, dri);
    PORT_STATE_VAR(s, row_name);
    PORT_STATE_VAR(s, info_lv);
    PORT_STATE_VAR(s, info_best);
    PORT_STATE_VAR(s, deb_name);
    PORT_STATE_VAR(s, deb_stages);
    PORT_STATE_VAR(s, deb_reward);
}
