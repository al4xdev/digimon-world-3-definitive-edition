/* The battle's escape asks first (one press on Escape fled at once, an easy slip). Choosing Escape
 * in the command menu opens this question instead of acting (fightstg_command_update's substep 10): the confirm button
 * flees, the cancel button (or triangle) goes back to the menu. It is the Overlevel Rush question's panel
 * (definitive_rush_panel.c): a heading in the callouts' polygon letters and, under it, the two answers after their
 * buttons' symbols in the game's font, on a dark frame centred on the screen. No timer: nothing happens until the
 * player answers.
 *
 * The answers' window lives on the battle's heap; the question closes it whichever way it ends, and a save state keeps
 * it with the heap it points into (def_flee_ask_savestate). Not asked in a replay script (def_on): the menu's choice
 * acts as in the game. */
#include "common.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "pad.h"
#include "psyq/libgpu.h"
#include "sound.h"

#include "psxstack/savestate.h"

#include "definitive.h"

extern int def_on(void);
extern void port_log(const char *fmt, ...);
extern u8 port_face_text_code(int bit);
extern const unsigned char def_str_FleeAskYes[], def_str_FleeAskNo[];

#define FA_LAYER 0x1005  /* the battle HUD's layer (the names and HP windows): offset (0, 0), over the 3D */
#define FA_FONT 1        /* the dialogues' font: its button symbols, its accents */
#define FA_CENTRE 160
#define FA_Y 150         /* the panel's top: under the portrait window (layer 0x1009, drawn over this one), where the command menu was */
#define FA_TITLE 208     /* the heading: 8.8 of the callouts' 16-pixel letters (13 pixels) */
#define FA_TITLE_Y 9     /* its top, from the panel's: room above for its accents */
#define FA_KEYS_Y 30     /* the answers' line */
#define FA_H 50
#define FA_PAD 16
#define FA_MIN_W 200
#define FA_SND_OK 0x8004503C
#define FA_SND_BACK 0x800450BD

static struct {
    MessageWindow *keys;
    s32 keys_w;
    s32 shown; /* frames up: the press that chose Escape is not read as the answer */
    unsigned char keys_text[64];
} flee_ask;

int def_flee_ask_on(void) {
    return def_on() && port_mod_battle_menu == 0;
}

static const char *fa_title(void) {
    return def_pick("RUN FROM THE BATTLE?", "FUGIR DA BATALHA?", "FUIR LE COMBAT?", "VOM KAMPF FLIEHEN?",
                    "FUGGIRE DALLO SCONTRO?", "HUIR DEL COMBATE?");
}

/* "(confirm) Run     (cancel) Back": each answer's button as the symbol of the bit read (the player's own letter
 * when pad.map has one), as the Rush question writes its answers. */
static void fa_keys_open(void) {
    static const unsigned char space[] = { 0x01, 0x01, 0x00 };
    unsigned char sym[3] = { 0x01, 0x00, 0x00 };
    unsigned char *t = flee_ask.keys_text;
    int p = 0, i;
    MessageWindow *w;

    t[0] = 0;
    sym[1] = port_face_text_code(pad_state.get_button_map(0, 13));
    if (sym[1] != 0) {
        def_glyph_put(t, &p, sizeof(flee_ask.keys_text), sym);
        def_glyph_put(t, &p, sizeof(flee_ask.keys_text), space);
    }
    def_glyph_put(t, &p, sizeof(flee_ask.keys_text), def_str_FleeAskYes);
    for (i = 0; i < 5; i++) {
        def_glyph_put(t, &p, sizeof(flee_ask.keys_text), space);
    }
    sym[1] = port_face_text_code(pad_state.get_button_map(0, 14));
    if (sym[1] != 0) {
        def_glyph_put(t, &p, sizeof(flee_ask.keys_text), sym);
        def_glyph_put(t, &p, sizeof(flee_ask.keys_text), space);
    }
    def_glyph_put(t, &p, sizeof(flee_ask.keys_text), def_str_FleeAskNo);
    w = message_create_window(FA_LAYER, FA_FONT, 0, 0);
    w->set_ot_depth(w, 0);
    w->set_text(w, (u8 *)t, -1);
    w->measure(w, 1); /* the lines start at x less their width: the window is placed by its right end */
    flee_ask.keys = w;
    flee_ask.keys_w = w->measured_width;
}

static void fa_quad(GfxLayer *layer, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
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

/* Queued front to back (in one ordering-table entry what is added last is drawn first): the heading and its dark edge,
 * the answers, the frame's lines, then the darkened fill. */
static void fa_draw(void) {
    static const u8 red[3] = { 255, 120, 88 }, edge[3] = { 8, 12, 40 };
    GfxLayer *layer = gfx_module.funcs.get_layer(FA_LAYER);
    const char *title = fa_title();
    s32 tw = def_poly_text_width(FA_TITLE, title), w, x;

    if (layer == NULL) {
        return;
    }
    w = tw + FA_PAD * 2;
    if (w < flee_ask.keys_w + FA_PAD * 2) {
        w = flee_ask.keys_w + FA_PAD * 2;
    }
    if (w < FA_MIN_W) {
        w = FA_MIN_W;
    }
    x = FA_CENTRE - w / 2;
    def_poly_text(layer, 0, FA_CENTRE - tw / 2, FA_Y + FA_TITLE_Y, FA_TITLE, title, red);
    def_poly_text(layer, 0, FA_CENTRE - tw / 2 + 1, FA_Y + FA_TITLE_Y + 1, FA_TITLE, title, edge);
    if (flee_ask.keys != NULL) {
        flee_ask.keys->set_pos(flee_ask.keys, FA_CENTRE + flee_ask.keys_w / 2, FA_Y + FA_KEYS_Y);
        flee_ask.keys = (MessageWindow *)heap_objects.try_run((Object *)flee_ask.keys);
    }
    fa_quad(layer, x, FA_Y, w, 1, 1, 60, 130, 230);
    fa_quad(layer, x, FA_Y + FA_H - 1, w, 1, 1, 60, 130, 230);
    fa_quad(layer, x, FA_Y, 1, FA_H, 1, 60, 130, 230);
    fa_quad(layer, x + w - 1, FA_Y, 1, FA_H, 1, 60, 130, 230);
    fa_quad(layer, x, FA_Y, w, FA_H, 2, 225, 215, 160);
}

void def_flee_ask_close(void) {
    if (flee_ask.keys != NULL) {
        heap_objects.stop((Object *)flee_ask.keys);
    }
    flee_ask.keys = NULL;
    flee_ask.keys_w = 0;
    flee_ask.shown = 0;
}

int def_flee_ask_frame(void) {
    int answer = 0;

    if (flee_ask.keys == NULL && flee_ask.shown == 0) {
        fa_keys_open();
        port_log("flee: the escape asks first");
    }
    if (flee_ask.shown > 0) {
        if (PAD_PRESSED(13)) {
            sound_module.play(FA_SND_OK);
            answer = 1;
        } else if (PAD_PRESSED(14) || PAD_PRESSED(12)) {
            sound_module.play(FA_SND_BACK);
            answer = -1;
        }
    }
    flee_ask.shown++;
    if (answer != 0) {
        port_log("flee: %s", answer > 0 ? "run" : "back to the menu");
        def_flee_ask_close();
        return answer;
    }
    fa_draw();
    return 0;
}

/* A battle's load forgets the window (that heap went with the last stage). */
void def_flee_ask_reset(void) {
    flee_ask.keys = NULL;
    flee_ask.keys_w = 0;
    flee_ask.shown = 0;
}

void def_flee_ask_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, flee_ask);
}
