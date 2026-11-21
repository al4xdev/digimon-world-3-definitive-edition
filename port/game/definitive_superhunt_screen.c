/* The Super Rare hunts, the game's side (the rules are definitive_superhunt.c).
 *
 * The Sepikmon of the Velvet Room (definitive_velvet.c) says his line and his board opens: every hunt with its level,
 * its state and its target's name (locked ones too: the owner wants the levels shown from the start, "whoever wants to
 * try, tries"), and for the one under the cursor where it roams, the HP it has left or the crystals held and their
 * price. The confirm button starts an open or finished hunt (one at a time) or, pressed twice, cancels the active one
 * (its target heals); the square sells one of the chosen hunt's crystals for its price. The first time the board opens
 * it gives the tutorial hunt (Ogremon in Central Park) at once. R1 turns the board to the hero's skins (definitive_skins.c:
 * each hunt won unlocks one, so the hunts' master hands them out; the owner's of 2026-11-17, no NPC of their own): the
 * confirm button wears an unlocked one, a locked one names the hunt that opens it; L1 turns back.
 *
 * The story sends the player to him (definitive_story.c, owner's of 2026-11-17): in Seiryu City the Leader is gone
 * and the Guide asks for the Sepikmon, whose tutorial is the hunt that finds him. Meeting him earlier through any inn's
 * door counts the same.
 *
 * The compass: in the active hunt's ground, a small thermometer at the top left of the field, in the callouts' polygon
 * letters (no window: nothing lives on the field's heap between frames), five steps from cold (4 maps or more away) to
 * burning (close to the point). After a hunt won, a panel on the field names the crystal it gave.
 *
 * The board is a field overlay run from FIELDSTG's idle step, as the DRI Agents' list (definitive_dri_screen.c, whose
 * layout and colours it shares); while it is up fieldstg_stage.event_running stays set. Off in a replay script. */
#include <string.h>

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

#include "psxstack/savestate.h"

#include "definitive.h"
#include "port.h"
#include "port_runtime.h"

extern int def_on(void);
extern unsigned char ststatus_map_areas[];
extern const unsigned char def_str_SupTitle[], def_str_SupNew[], def_str_SupOn[], def_str_SupDone[], def_str_SupLocked[],
    def_str_SupLv[], def_str_SupWhere[], def_str_SupOf[], def_str_SupSec0[], def_str_SupSec1[], def_str_SupSec2[],
    def_str_SupSec3[], def_str_SupSec4[], def_str_SupSrv0[], def_str_SupSrv1[], def_str_SupTutWhere[], def_str_SupHp[],
    def_str_SupSlash[], def_str_SupFled[], def_str_SupHeld[], def_str_SupPrice[], def_str_SupBits[],
    def_str_SupKeysStart[], def_str_SupKeysCancel[], def_str_SupKeysConfirm[], def_str_SupKeysLocked[], def_str_SupBusy[],
    def_str_SupStarted[], def_str_SupTutGiven[], def_str_SupCancelled[], def_str_SupSold[], def_str_SupNoCrystal[],
    def_str_SupLockedMsg[], def_str_SupCrystalPre[], def_str_SupCrystalPost[], def_str_SupGot[], def_str_DriOk[],
    def_str_Unknown[], def_str_SupSkin[], def_str_Skin1[], def_str_Skin2[], def_str_Skin3[], def_str_Skin4[],
    def_str_Skin5[], def_str_Skin6[], def_str_Skin7[], def_str_Skin8[], def_str_Skin9[], def_str_Skin10[], def_str_Skin11[],
    def_str_Skin12[], def_str_Skin13[], def_str_Skin0[], def_str_SupSkinsTitle[], def_str_SupSkinWorn[],
    def_str_SupSkinFrom[], def_str_SupSkinOwn[], def_str_SupSkinOriginal[], def_str_SupSkinSet[], def_str_SupSkinLocked[],
    def_str_SupKeysSkins[], def_str_SupFledTarget[], def_str_SupFledPlayer[];

extern void *fieldstg_dialog_create_talk(FieldstgActor *actor, s32 message);

#define SH_LAYER 0x1002 /* the field's window layer */
#define SH_LINE 14      /* a row's pitch */
#define SH_SMALL 13
#define SH_TITLE_H 23
#define SH_ROWS 8       /* hunts listed at once; the list scrolls past them */
#define SH_TOP 30
#define SH_LIST_H (SH_ROWS * SH_LINE + 6)
#define SH_STATE_X 18   /* a row's state: NEW, HUNT, DONE or -- */
#define SH_LV_X 68      /* its level */
#define SH_NAME_X 116   /* its target */
#define SH_INFO_Y (SH_TOP - 5 + SH_LIST_H + 3)
#define SH_INFO_H 58
#define SH_HINT_Y 208
#define SH_DEB_Y 150    /* the reward's panel */
#define SH_DEB_H 60
#define SH_DEB_SKIN_H 82 /* with the skin's line */
#define SH_TEXT 96
#define SH_WINDOWS 40
#define SH_PAL_TEXT 0
#define SH_PAL_HOT 3    /* the active hunt, a refusal (the bestiary's orange) */
#define SH_PAL_DONE 4   /* finished, sold (its green) */
#define SH_PAL_DIM 7    /* locked */
#define SH_KEYS ((1 << 10) | (1 << 11) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15))
#define SH_KEY_SELL 15  /* the square */
#define SH_KEY_HUNTS 10 /* L1: the hunts' page */
#define SH_KEY_SKINS 11 /* R1: the skins' page */
#define SH_SKIN_ROWS (DEF_SKIN_COUNT + 1) /* the own colours, then a skin per hunt */
#define SH_SND_MOVE 0x8004513E
#define SH_SND_OK 0x8004503C
#define SH_SND_BACK 0x800450BD
#define SH_MONEY_MAX 9999999

/* The compass: top left, clear of the Rush's panels (top centre). */
#define SH_CMP_LAYER 0x1004 /* the Rush's layer, over the field's sprites */
#define SH_CMP_X 8
#define SH_CMP_Y 8
#define SH_CMP_W 92
#define SH_CMP_H 30
#define SH_CMP_SIZE 160     /* 8.8 of the callouts' 16-pixel letters: 10 pixels */

enum { SH_BOARD = 1, SH_DEBRIEF };

int def_superhunt_debug;

static struct {
    s32 request; /* opens on the field's next idle frame with its texts loaded */
    s32 open;
    s32 mode;    /* SH_BOARD or SH_DEBRIEF */
    s32 sel;
    s32 top;
    s32 armed;   /* the active hunt the confirm button chose once (to cancel it), -1: none */
    s32 fresh;
    s32 releasing;
    s32 close;
    s32 won;     /* the debrief's hunt */
    s32 skin;    /* the skin its first win unlocked, 0: none (a hunt won again) */
    s32 skins;   /* the board shows the skins' page */
    s32 ssel, stop; /* its cursor and first row */
    const unsigned char *msg; /* the board's last result line, NULL: none */
    s32 msg_pal;
    s32 count;
    MessageWindow *wins[SH_WINDOWS];
    MessageWindow *title, *states[SH_ROWS], *levels[SH_ROWS], *names[SH_ROWS], *where, *detail, *note, *hint;
    MessageCursor *cursor;
    void *flee_dialog; /* Junior's field talk balloon after a Super Rare hunt escape */
    s32 flee_map;
} sh;

static const unsigned char *flee_talk_text;

const unsigned char *def_super_talk_text(void) {
    const unsigned char *t = flee_talk_text;

    flee_talk_text = NULL;
    return t;
}

static unsigned char row_lv[SH_ROWS][16], info_where[SH_TEXT], info_detail[SH_TEXT], info_note[SH_TEXT],
    deb_line[SH_TEXT], deb_skin[SH_TEXT];

static int idle(void) {
    return !fieldstg_stage.event_running && !fieldstg_stage.battle_starting && !fieldstg_stage.menu_open &&
           !fieldstg_stage.actor_busy && !fieldstg_stage.title_shown;
}

/* The species' records (0x1CF) and their names (0x4E): cdload's is_loading queues a file that is not there. */
static int texts_loaded(void) {
    s32 a = cdload_module.is_loading(0x1CF);
    s32 b = cdload_module.is_loading(records_language + 0x4E);

    return a == 0 && b == 0;
}

static const unsigned char *target_name(int hunt) {
    const unsigned char *s = def_species_name(def_super_rares[hunt].species);

    return s != NULL ? s : def_str_Unknown;
}

/* "Ogremon Crystal" / "Cristal de Ogremon". */
static void put_crystal(unsigned char *buf, int *p, int size, int hunt) {
    def_glyph_put(buf, p, size, def_str_SupCrystalPre);
    def_glyph_put(buf, p, size, target_name(hunt));
    def_glyph_put(buf, p, size, def_str_SupCrystalPost);
}

/* Skin n's name (1..DEF_SKIN_COUNT; 0 the own colours). */
static const unsigned char *skin_name(int skin) {
    static const unsigned char *const names[SH_SKIN_ROWS] = {
        def_str_Skin0, def_str_Skin1, def_str_Skin2,  def_str_Skin3,  def_str_Skin4,  def_str_Skin5,  def_str_Skin6,
        def_str_Skin7, def_str_Skin8, def_str_Skin9, def_str_Skin10, def_str_Skin11, def_str_Skin12, def_str_Skin13,
    };

    return names[skin < 0 || skin > DEF_SKIN_COUNT ? 0 : skin];
}

/* ---- The windows and panels (the DRI list's). */

static MessageWindow *sh_window(s32 font, s32 x, s32 y) {
    MessageWindow *w = message_create_window(SH_LAYER, font, x, y);

    w->set_ot_depth(w, 1);
    if (sh.count < SH_WINDOWS) {
        sh.wins[sh.count++] = w;
    }
    return w;
}

static void sh_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(SH_LAYER);
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

/* The Sepikmon's panels: the DRI list's shapes in the shaman's ochre. */
static void sh_panel(s32 x, s32 y, s32 w, s32 h, s32 band) {
    if (band) {
        sh_rect(x + 2, y + 2, w - 4, SH_LINE + 2, 70, 40, 0);
    }
    sh_rect(x + 1, y + 1, w - 2, h - 2, 26, 14, 4);
    sh_rect(x, y, w, h, 214, 160, 80);
}

/* ---- The board. */

/* The skins' page in the board's windows: the level column is left empty, the state says which is worn, the info
 * panel what a locked one needs (skin n opens with hunt n - 1 won, def_skin_unlocked). */
static void skins_fill(void) {
    int r, k, p, open, worn = def_skin_hero();

    sh.title->set_text(sh.title, (u8 *)def_str_SupSkinsTitle, -1);
    for (r = 0; r < SH_ROWS; r++) {
        k = sh.stop + r;
        sh.levels[r]->set_visible(sh.levels[r], 0);
        sh.states[r]->set_visible(sh.states[r], k < SH_SKIN_ROWS);
        sh.names[r]->set_visible(sh.names[r], k < SH_SKIN_ROWS);
        if (k >= SH_SKIN_ROWS) {
            continue;
        }
        open = def_skin_unlocked(k);
        sh.states[r]->set_text(sh.states[r], (u8 *)(k == worn ? def_str_SupSkinWorn : def_str_SupLocked), -1);
        sh.states[r]->set_visible(sh.states[r], k == worn || !open); /* an open one not worn: no state */
        /* worn: the active hunt's colour (the small font's green garbles WORN) */
        sh.states[r]->set_palette(sh.states[r], k == worn ? SH_PAL_HOT : SH_PAL_DIM);
        sh.names[r]->set_text(sh.names[r], (u8 *)skin_name(k), -1);
        sh.names[r]->set_palette(sh.names[r], open ? SH_PAL_TEXT : SH_PAL_DIM);
    }
    k = sh.ssel;
    p = 0;
    info_where[0] = 0;
    if (k == 0) {
        def_glyph_put(info_where, &p, SH_TEXT, def_str_SupSkinOriginal);
    } else if (def_skin_unlocked(k)) {
        def_glyph_put(info_where, &p, SH_TEXT, def_str_SupSkinOwn);
    } else {
        def_glyph_put(info_where, &p, SH_TEXT, def_str_SupSkinFrom);
        def_glyph_put(info_where, &p, SH_TEXT, target_name(k - 1));
    }
    sh.where->set_text(sh.where, info_where, -1);
    sh.detail->set_visible(sh.detail, 0);
    sh.note->set_visible(sh.note, sh.msg != NULL);
    if (sh.msg != NULL) {
        sh.note->set_text(sh.note, (u8 *)sh.msg, -1);
        sh.note->set_palette(sh.note, sh.msg_pal);
    }
    sh.hint->set_text(sh.hint, (u8 *)def_str_SupKeysSkins, -1);
    sh.cursor->set_pos(sh.cursor, 6, SH_TOP + (sh.ssel - sh.stop) * SH_LINE);
}

static void board_fill(void) {
    int r, h, p, state, pal;

    if (sh.skins) {
        skins_fill();
        return;
    }
    sh.title->set_text(sh.title, (u8 *)def_str_SupTitle, -1);
    sh.detail->set_visible(sh.detail, 1);

    for (r = 0; r < SH_ROWS; r++) {
        h = sh.top + r;
        sh.states[r]->set_visible(sh.states[r], h < DEF_SUPER_COUNT);
        sh.levels[r]->set_visible(sh.levels[r], h < DEF_SUPER_COUNT);
        sh.names[r]->set_visible(sh.names[r], h < DEF_SUPER_COUNT);
        if (h >= DEF_SUPER_COUNT) {
            continue;
        }
        state = def_super_state(h);
        sh.states[r]->set_text(sh.states[r],
                               (u8 *)(state == DEF_SUPER_ACTIVE ? def_str_SupOn
                                      : state == DEF_SUPER_DONE ? def_str_SupDone
                                      : state == DEF_SUPER_OPEN ? def_str_SupNew
                                                                : def_str_SupLocked),
                               -1);
        pal = state == DEF_SUPER_ACTIVE ? SH_PAL_HOT
              : state == DEF_SUPER_DONE ? SH_PAL_DONE
              : state == DEF_SUPER_OPEN ? SH_PAL_TEXT
                                        : SH_PAL_DIM;
        sh.states[r]->set_palette(sh.states[r], pal);
        p = 0;
        row_lv[r][0] = 0;
        def_glyph_put(row_lv[r], &p, sizeof(row_lv[r]), def_str_SupLv);
        def_glyph_count(row_lv[r], &p, sizeof(row_lv[r]), def_super_rares[h].level);
        sh.levels[r]->set_text(sh.levels[r], row_lv[r], -1);
        sh.levels[r]->set_palette(sh.levels[r], state == DEF_SUPER_LOCKED ? SH_PAL_DIM : SH_PAL_TEXT);
        sh.names[r]->set_text(sh.names[r], (u8 *)target_name(h), -1);
        sh.names[r]->set_palette(sh.names[r], h == sh.armed ? SH_PAL_HOT : state == DEF_SUPER_LOCKED ? SH_PAL_DIM : SH_PAL_TEXT);
    }

    h = sh.sel;
    p = 0;
    info_where[0] = 0;
    if (def_super_rares[h].area >= 0) {
        def_glyph_put(info_where, &p, SH_TEXT, def_str_SupTutWhere);
    } else {
        static const unsigned char *const sectors[DEF_HUNT_SECTORS] = { def_str_SupSec0, def_str_SupSec1, def_str_SupSec2,
                                                                         def_str_SupSec3, def_str_SupSec4 };
        int s = def_super_rares[h].sector;

        def_glyph_put(info_where, &p, SH_TEXT, def_str_SupWhere);
        def_glyph_put(info_where, &p, SH_TEXT, s >= 0 && s < DEF_HUNT_SECTORS ? sectors[s] : def_str_Unknown);
        def_glyph_put(info_where, &p, SH_TEXT, def_str_SupOf);
        def_glyph_put(info_where, &p, SH_TEXT, def_super_rares[h].server ? def_str_SupSrv1 : def_str_SupSrv0);
    }
    sh.where->set_text(sh.where, info_where, -1);

    p = 0;
    info_detail[0] = 0;
    if (def_super_state(h) == DEF_SUPER_ACTIVE) {
        def_glyph_put(info_detail, &p, SH_TEXT, def_str_SupHp);
        def_glyph_number(info_detail, &p, SH_TEXT, def_super_hp_left());
        def_glyph_put(info_detail, &p, SH_TEXT, def_str_SupSlash);
        def_glyph_number(info_detail, &p, SH_TEXT, def_super_rares[h].hp);
        def_glyph_put(info_detail, &p, SH_TEXT, def_str_SupFled);
        def_glyph_count(info_detail, &p, SH_TEXT, def_super_meets());
    } else {
        def_glyph_put(info_detail, &p, SH_TEXT, def_str_SupHeld);
        def_glyph_count(info_detail, &p, SH_TEXT, def_super_crystals(h));
        def_glyph_put(info_detail, &p, SH_TEXT, def_str_SupPrice);
        def_glyph_number(info_detail, &p, SH_TEXT, def_super_rares[h].price);
        def_glyph_put(info_detail, &p, SH_TEXT, def_str_SupBits);
    }
    sh.detail->set_text(sh.detail, info_detail, -1);

    sh.note->set_visible(sh.note, sh.msg != NULL);
    if (sh.msg != NULL) {
        sh.note->set_text(sh.note, (u8 *)sh.msg, -1);
        sh.note->set_palette(sh.note, sh.msg_pal);
    }
    state = def_super_state(h);
    sh.hint->set_text(sh.hint, (u8 *)(sh.armed == h               ? def_str_SupKeysConfirm
                                      : state == DEF_SUPER_ACTIVE ? def_str_SupKeysCancel
                                      : state == DEF_SUPER_LOCKED ? def_str_SupKeysLocked
                                                                  : def_str_SupKeysStart),
                      -1);
    sh.cursor->set_pos(sh.cursor, 6, SH_TOP + (sh.sel - sh.top) * SH_LINE);
}

static void say(const unsigned char *msg, int pal) {
    sh.msg = msg;
    sh.msg_pal = pal;
}

static void board_open(void) {
    s32 r;

    /* The first board gives the tutorial at once (the Sepikmon's first line sends the player to it). */
    if (!(def_super_flags() & DEF_SUPER_FLAG_TUTORIAL)) {
        def_super_flag_set(DEF_SUPER_FLAG_TUTORIAL);
        if (def_super_active() < 0 && def_super_start(DEF_SUPER_TUTORIAL)) {
            say(def_str_SupTutGiven, SH_PAL_DONE);
        }
    }
    if (def_super_active() >= 0) {
        sh.sel = def_super_active();
    } else if (def_super_done() < DEF_SUPER_COUNT) {
        sh.sel = def_super_done(); /* the next one to open */
    }
    if (sh.sel < 0 || sh.sel >= DEF_SUPER_COUNT) {
        sh.sel = 0;
    }
    sh.top = sh.sel >= SH_ROWS ? sh.sel - SH_ROWS + 1 : 0;
    sh.skins = 0;
    sh.ssel = def_skin_hero();
    sh.stop = sh.ssel >= SH_ROWS ? sh.ssel - SH_ROWS + 1 : 0;

    sh.title = sh_window(1, 8, 5);
    for (r = 0; r < SH_ROWS; r++) {
        sh.states[r] = sh_window(2, SH_STATE_X, SH_TOP + r * SH_LINE);
        sh.levels[r] = sh_window(2, SH_LV_X, SH_TOP + r * SH_LINE);
        sh.names[r] = sh_window(2, SH_NAME_X, SH_TOP + r * SH_LINE);
    }
    sh.where = sh_window(2, 10, SH_INFO_Y + 5);
    sh.where->set_palette(sh.where, SH_PAL_TEXT);
    sh.detail = sh_window(2, 10, SH_INFO_Y + 5 + SH_SMALL);
    sh.detail->set_palette(sh.detail, SH_PAL_TEXT);
    sh.note = sh_window(1, 10, SH_INFO_Y + 5 + 2 * SH_SMALL); /* the dialogues' font: the small one's coloured palettes
                                                                * have no lowercase */
    sh.hint = sh_window(1, 8, SH_HINT_Y + 2);
    sh.hint->set_page_lines(sh.hint, 2);
    sh.cursor = message_create_cursor(SH_LAYER, 1, 6, SH_TOP);
    sh.cursor->show(sh.cursor, 1);
    board_fill();
}

static void board_draw(void) {
    if (!sh.skins && sh.armed >= sh.top && sh.armed < sh.top + SH_ROWS) {
        sh_rect(5, SH_TOP + (sh.armed - sh.top) * SH_LINE - 2, 310, SH_LINE, 74, 41, 0);
    }
    sh_panel(0, 0, 320, SH_TITLE_H, 1);
    sh_panel(2, SH_TOP - 5, 316, SH_LIST_H, 0);
    sh_panel(2, SH_INFO_Y, 316, SH_INFO_H, 0);
    sh_panel(0, SH_HINT_Y, 320, 240 - SH_HINT_Y, 0);
}

static void board_move(int sel) {
    sh.sel = sel;
    if (sh.sel < sh.top) {
        sh.top = sh.sel;
    } else if (sh.sel >= sh.top + SH_ROWS) {
        sh.top = sh.sel - SH_ROWS + 1;
    }
    sh.armed = -1;
    sh.msg = NULL;
    sound_module.play(SH_SND_MOVE);
    board_fill();
}

static void skins_move(int k) {
    sh.ssel = k;
    if (sh.ssel < sh.stop) {
        sh.stop = sh.ssel;
    } else if (sh.ssel >= sh.stop + SH_ROWS) {
        sh.stop = sh.ssel - SH_ROWS + 1;
    }
    sh.msg = NULL;
    sound_module.play(SH_SND_MOVE);
    board_fill();
}

static void skins_input(void) {
    int k = sh.ssel;

    if (PAD_PRESSED(4) || PAD_REPEAT(4)) {
        skins_move((k + SH_SKIN_ROWS - 1) % SH_SKIN_ROWS);
    } else if (PAD_PRESSED(6) || PAD_REPEAT(6)) {
        skins_move((k + 1) % SH_SKIN_ROWS);
    } else if (PAD_PRESSED(13)) {
        if (def_skin_set_hero(k)) { /* the sprites follow on the field (port_girls_reskin) */
            sound_module.play(SH_SND_OK);
            say(def_str_SupSkinSet, SH_PAL_DONE);
            port_log("superhunt: the hero's skin %d chosen", k);
        } else {
            sound_module.play(SH_SND_BACK);
            say(def_str_SupSkinLocked, SH_PAL_HOT);
        }
        board_fill();
    } else if (PAD_PRESSED(SH_KEY_HUNTS)) {
        sh.skins = 0;
        sh.msg = NULL;
        sound_module.play(SH_SND_MOVE);
        board_fill();
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        sound_module.play(SH_SND_BACK);
        sh.close = 1;
    }
}

static void board_input(void) {
    int h = sh.sel, state = def_super_state(h), price;

    if (sh.skins) {
        skins_input();
        return;
    }
    if (PAD_PRESSED(SH_KEY_SKINS)) {
        sh.skins = 1;
        sh.armed = -1;
        sh.msg = NULL;
        sound_module.play(SH_SND_MOVE);
        board_fill();
        return;
    }

    if (PAD_PRESSED(4) || PAD_REPEAT(4)) {
        board_move((h + DEF_SUPER_COUNT - 1) % DEF_SUPER_COUNT);
        return;
    }
    if (PAD_PRESSED(6) || PAD_REPEAT(6)) {
        board_move((h + 1) % DEF_SUPER_COUNT);
        return;
    }
    if (PAD_PRESSED(13)) {
        if (state == DEF_SUPER_LOCKED) {
            sound_module.play(SH_SND_BACK);
            say(def_str_SupLockedMsg, SH_PAL_HOT);
        } else if (state == DEF_SUPER_ACTIVE) {
            if (sh.armed != h) {
                sh.armed = h; /* a cancel heals the target: asked twice */
                sound_module.play(SH_SND_OK);
            } else {
                def_super_cancel();
                sh.armed = -1;
                sound_module.play(SH_SND_OK);
                say(def_str_SupCancelled, SH_PAL_TEXT);
            }
        } else if (def_super_active() >= 0) {
            sound_module.play(SH_SND_BACK);
            say(def_str_SupBusy, SH_PAL_HOT);
        } else if (def_super_start(h)) {
            sound_module.play(SH_SND_OK);
            say(def_str_SupStarted, SH_PAL_DONE);
        } else {
            sound_module.play(SH_SND_BACK);
        }
        board_fill();
    } else if (PAD_PRESSED(SH_KEY_SELL)) {
        price = def_super_crystal_sell(h);
        if (price > 0) {
            int p = 0;

            gamestate_data.money = gamestate_data.money + price > SH_MONEY_MAX ? SH_MONEY_MAX : gamestate_data.money + price;
            info_note[0] = 0;
            def_glyph_put(info_note, &p, SH_TEXT, def_str_SupSold);
            def_glyph_number(info_note, &p, SH_TEXT, price);
            def_glyph_put(info_note, &p, SH_TEXT, def_str_SupBits);
            sound_module.play(SH_SND_OK);
            say(info_note, SH_PAL_DONE);
        } else {
            sound_module.play(SH_SND_BACK);
            say(def_str_SupNoCrystal, SH_PAL_HOT);
        }
        board_fill();
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        sound_module.play(SH_SND_BACK);
        if (sh.armed >= 0) {
            sh.armed = -1;
            board_fill();
        } else {
            sh.close = 1;
        }
    }
}

/* ---- The reward's panel: the hunt won and its crystal. */

static void debrief_open(void) {
    MessageWindow *w;
    int p = 0;

    deb_line[0] = 0;
    def_glyph_put(deb_line, &p, SH_TEXT, def_str_SupGot);
    put_crystal(deb_line, &p, SH_TEXT, sh.won);
    w = sh_window(1, 18, SH_DEB_Y + 8); /* the dialogues' font: the small one's green has no lowercase */
    w->set_text(w, deb_line, -1);
    if (sh.skin > 0) {
        p = 0;
        deb_skin[0] = 0;
        def_glyph_put(deb_skin, &p, SH_TEXT, def_str_SupSkin);
        def_glyph_put(deb_skin, &p, SH_TEXT, skin_name(sh.skin));
        w = sh_window(1, 18, SH_DEB_Y + 30);
        w->set_text(w, deb_skin, -1);
    }
    w = sh_window(1, 18, SH_DEB_Y + (sh.skin > 0 ? 56 : 36));
    w->set_text(w, (u8 *)def_str_DriOk, -1);
}

/* ---- The screen. */

static void sh_close(void) {
    s32 i;

    for (i = 0; i < sh.count; i++) {
        heap_objects.stop((Object *)sh.wins[i]);
        sh.wins[i] = NULL;
    }
    sh.count = 0;
    if (sh.cursor != NULL) {
        heap_objects.stop((Object *)sh.cursor);
        sh.cursor = NULL;
    }
    sh.open = 0;
    sh.request = 0;
}

static void sh_run(void) {
    s32 i;

    for (i = 0; i < sh.count; i++) {
        heap_objects.try_run((Object *)sh.wins[i]);
    }
    if (sh.cursor != NULL) {
        heap_objects.try_run((Object *)sh.cursor);
    }
}

/* The field back to the player once none of the screen's buttons is down (as the DRI list's dri_release). */
static int sh_release(void) {
    FieldstgActor *player;

    if (pad_state.get_held(0) & SH_KEYS) {
        fieldstg_stage.event_running = 1;
        return 1;
    }
    sh.releasing = 0;
    fieldstg_stage.event_running = 0;
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player != NULL) {
        player->reset_control(player);
    }
    return 1;
}

void def_superhunt_board_offer(void) {
    if (!def_on() || sh.open || sh.request) {
        return;
    }
    sh.request = 1;
    sh.mode = SH_BOARD;
    sh.msg = NULL;
    fieldstg_stage.event_running = 1;
    port_log("superhunt: the Sepikmon's board: active %d, done %d, flags %d", def_super_active(), def_super_done(),
             def_super_flags());
}

/* ---- The compass. */

static void cmp_quad(GfxLayer *layer, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
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

static void compass_draw(void) {
    /* the reading's word and its colour, cold blue to burning red; the segments lit up to the reading */
    static const char *const en[DEF_TEMP_COUNT] = { "", "COLD", "COOL", "WARM", "HOT", "BURNING!" };
    static const char *const pt[DEF_TEMP_COUNT] = { "", "FRIO", "MORNO", "QUENTE", "MUITO QUENTE", "FERVENDO!" };
    static const char *const fr[DEF_TEMP_COUNT] = { "", "FROID", "FRAIS", "TIÈDE", "CHAUD", "BRÛLANT!" };
    static const char *const de[DEF_TEMP_COUNT] = { "", "KALT", "KÜHL", "WARM", "HEISS", "GLÜHEND!" };
    static const char *const it[DEF_TEMP_COUNT] = { "", "FREDDO", "FRESCO", "TIEPIDO", "CALDO", "BOLLENTE!" };
    static const char *const es[DEF_TEMP_COUNT] = { "", "FRÍO", "FRESCO", "TIBIO", "CALIENTE", "ARDIENDO!" };
    static const char *const *const words[6] = { en, pt, fr, de, it, es };
    static const u8 rgb[DEF_TEMP_COUNT][3] = {
        { 0, 0, 0 }, { 80, 140, 255 }, { 120, 220, 230 }, { 250, 220, 90 }, { 255, 140, 40 }, { 255, 50, 40 },
    };
    static const u8 edge[3] = { 8, 12, 40 };
    FieldstgActor *player;
    GfxLayer *layer;
    const char *word;
    char prox_text[16];
    int map, t, p, i, w, lit;

    if (def_super_active() < 0 || fieldstg_stage.menu_open || fieldstg_stage.title_shown || dw3_velvet_in_room()) {
        return;
    }
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player == NULL) {
        return;
    }
    map = gamestate_data.funcs.get_map();
    t = def_super_temperature(map, player->pixel_pos.x, player->pixel_pos.y);
    if (t <= DEF_TEMP_NONE || t >= DEF_TEMP_COUNT) {
        return;
    }
    layer = gfx_module.funcs.get_layer(SH_CMP_LAYER);
    word = words[def_lang() < 6 ? def_lang() : 0][t];
    w = def_poly_text_width(SH_CMP_SIZE, word) + 12;
    if (w < SH_CMP_W) {
        w = SH_CMP_W;
    }
    /* what is queued first is drawn last: the word, the segments, then the panel under them */
    def_poly_text(layer, 0, SH_CMP_X + 6, SH_CMP_Y + 4, SH_CMP_SIZE, word, rgb[t]);
    def_poly_text(layer, 0, SH_CMP_X + 7, SH_CMP_Y + 5, SH_CMP_SIZE, word, edge);
    for (i = 1; i < DEF_TEMP_COUNT; i++) {
        s32 x = SH_CMP_X + 6 + (i - 1) * 16;

        if (i <= t) {
            cmp_quad(layer, x, SH_CMP_Y + 19, 14, 6, -1, rgb[i][0], rgb[i][1], rgb[i][2]);
        } else {
            cmp_quad(layer, x, SH_CMP_Y + 19, 14, 6, -1, 30, 34, 60);
        }
    }
    cmp_quad(layer, SH_CMP_X, SH_CMP_Y, w, 1, 1, 214, 160, 80);
    cmp_quad(layer, SH_CMP_X, SH_CMP_Y + SH_CMP_H - 1, w, 1, 1, 214, 160, 80);
    cmp_quad(layer, SH_CMP_X, SH_CMP_Y, w, SH_CMP_H, 2, 60, 40, 20);

    /* Second high-precision thermometer on the target's map: shows exact proximity 0..100% to the spawn point. */
    p = def_super_proximity(map, player->pixel_pos.x, player->pixel_pos.y);
    if (p >= 0) {
        s32 y2 = SH_CMP_Y + SH_CMP_H + 2;
        s32 h2 = 30;
        s32 bar_w = p * 79 / 100;
        const char *pfx = def_pick("POS ", "ALVO ", "CIBLE ", "ZIEL ", "META ", "META ");
        int k = 0, c = p >= 90 ? 5 : p >= 75 ? 4 : p >= 50 ? 3 : p >= 25 ? 2 : 1;

        while (*pfx) {
            prox_text[k++] = *pfx++;
        }
        if (p == 100) {
            prox_text[k++] = '1';
            prox_text[k++] = '0';
            prox_text[k++] = '0';
        } else {
            if (p >= 10) {
                prox_text[k++] = (char)('0' + p / 10);
            }
            prox_text[k++] = (char)('0' + p % 10);
        }
        prox_text[k++] = '%';
        prox_text[k] = 0;

        lit = (p + 9) / 10;
        if (p == 0) {
            lit = 0;
        }
        def_poly_text(layer, 0, SH_CMP_X + 6, y2 + 4, SH_CMP_SIZE, prox_text, rgb[c]);
        def_poly_text(layer, 0, SH_CMP_X + 7, y2 + 5, SH_CMP_SIZE, prox_text, edge);
        for (i = 0; i < 10; i++) {
            s32 x = SH_CMP_X + 6 + i * 8;
            int sc = i >= 8 ? 5 : i >= 6 ? 4 : i >= 4 ? 3 : i >= 2 ? 2 : 1;

            if (i < lit) {
                cmp_quad(layer, x, y2 + 18, 7, 5, -1, rgb[sc][0], rgb[sc][1], rgb[sc][2]);
            } else {
                cmp_quad(layer, x, y2 + 18, 7, 5, -1, 30, 34, 60);
            }
        }
        if (bar_w > 0) {
            cmp_quad(layer, SH_CMP_X + 6, y2 + 25, bar_w, 2, -1, rgb[c][0], rgb[c][1], rgb[c][2]);
        }
        cmp_quad(layer, SH_CMP_X + 6, y2 + 25, 79, 2, -1, 30, 34, 60);
        cmp_quad(layer, SH_CMP_X, y2, w, 1, 1, 214, 160, 80);
        cmp_quad(layer, SH_CMP_X, y2 + h2 - 1, w, 1, 1, 214, 160, 80);
        cmp_quad(layer, SH_CMP_X, y2, w, h2, 2, 60, 40, 20);
    }
}

int def_superhunt_frame(void) {
    port_girls_reskin(); /* the hero's skin (definitive_skins.c): chosen, unlocked or read from a card */
    if (!def_on()) {
        return 0;
    }
    if (sh.flee_dialog != NULL) {
        if (sh.flee_map != gamestate_data.funcs.get_map()) {
            sh.flee_dialog = NULL;
        } else {
            fieldstg_stage.event_running = 1;
            sh.flee_dialog = heap_objects.try_run((Object *)sh.flee_dialog);
            if (sh.flee_dialog == NULL) {
                sh.releasing = 1;
            }
            return 1;
        }
    }
    if (sh.releasing) {
        return sh_release();
    }
    if (!sh.open && !sh.request) {
        int won, first, fled;

        if (def_superhunt_debug && idle()) {
            def_superhunt_debug = 0;
            def_superhunt_board_offer();
        } else if (idle() && (won = def_super_won_take(&first)) >= 0) {
            sh.won = won;
            /* skin n opens with the nth hunt won (def_skin_unlocked); a hunt won again opens none */
            sh.skin = first && won + 1 <= DEF_SKIN_COUNT ? won + 1 : 0;
            sh.request = 1;
            sh.mode = SH_DEBRIEF;
        } else if (idle() && fieldstg_stage.talk_file > 0 && cdload_module.is_loading(fieldstg_stage.talk_file) == 0 &&
                   (fled = def_super_fled_take()) != DEF_SUPER_FLED_NONE) {
            FieldstgActor *player = (FieldstgActor *)heap_objects.find(5, -1, 0);

            if (player != NULL) {
                flee_talk_text = fled == DEF_SUPER_FLED_PLAYER ? def_str_SupFledPlayer : def_str_SupFledTarget;
                sh.flee_map = gamestate_data.funcs.get_map();
                sh.flee_dialog = fieldstg_dialog_create_talk(player, 0);
                fieldstg_stage.event_running = 1;
                sh.flee_dialog = heap_objects.try_run((Object *)sh.flee_dialog);
                flee_talk_text = NULL;
                if (sh.flee_dialog == NULL) {
                    sh.releasing = 1;
                }
                return 1;
            }
        } else {
            compass_draw();
            return 0;
        }
    }
    fieldstg_stage.event_running = 1;
    if (!sh.open) {
        if (!texts_loaded()) {
            return 1;
        }
        sh.count = 0;
        sh.cursor = NULL;
        sh.armed = -1;
        sh.close = 0;
        if (sh.mode == SH_BOARD) {
            board_open();
        } else {
            debrief_open();
        }
        sh.open = 1;
        sh.request = 0;
        sh.fresh = 1;
        port_log("superhunt: frame %ld: the %s", port_frames, sh.mode == SH_BOARD ? "board" : "reward panel");
    }
    if (!sh.fresh) {
        if (sh.mode == SH_BOARD) {
            board_input();
        } else if (PAD_PRESSED(13) || PAD_PRESSED(12) || PAD_PRESSED(14)) {
            sound_module.play(SH_SND_OK);
            sh.close = 1;
        }
    }
    sh.fresh = 0;
    if (sh.close) {
        sh_close();
        sh.releasing = 1;
        return 1;
    }
    sh_run();
    if (sh.mode == SH_BOARD) {
        board_draw();
    } else {
        sh_panel(8, SH_DEB_Y, 304, sh.skin > 0 ? SH_DEB_SKIN_H : SH_DEB_H, 0);
    }
    return 1;
}

void def_superhunt_screen_savestate(struct PortState *s) {
    /* the board points into the field's heap and at the texts below: all of it goes with the heap */
    PORT_STATE_VAR(s, sh);
    PORT_STATE_VAR(s, row_lv);
    PORT_STATE_VAR(s, info_where);
    PORT_STATE_VAR(s, info_detail);
    PORT_STATE_VAR(s, info_note);
    PORT_STATE_VAR(s, deb_line);
    PORT_STATE_VAR(s, deb_skin);
}
