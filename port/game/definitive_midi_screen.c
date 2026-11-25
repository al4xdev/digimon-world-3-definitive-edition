/* Etemon's jukebox: the Velvet Room's Etemon, once his line is over, opens the list of the player's MIDI files
 * (definitive_midi.c: the folder, the choice, the conversion) and the game's own music as the first row. The choice is
 * for the random battles. The board is a field overlay run from FIELDSTG's idle step, as the pig's stat swap
 * (definitive_respec_screen.c, whose helpers and look it shares): the cursor moves with up and down, x chooses, the
 * triangle leaves. Off in a replay script. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "fieldstg.h"
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
extern int def_midi_volume(int index);
extern void def_midi_set_volume(int index, int pct);
extern int def_midi_preview_play(int index);
extern const unsigned char def_str_MidiTitle[], def_str_MidiKeys[], def_str_MidiOwn[], def_str_MidiFolder[],
    def_str_MidiNone[], def_str_MidiShuffle[], def_str_MidiOrder[], def_str_MidiSet[], def_str_MidiOwnSet[], def_str_MidiCut[], def_str_MidiMore[];

#define MS_LAYER 0x1002 /* the field's window layer */
#define MS_LINE 16
#define MS_TITLE_H 23
#define MS_TOP 34
#define MS_ROWS 8
#define MS_LIST_H (MS_ROWS * MS_LINE + 6)
#define MS_INFO_Y (MS_TOP - 5 + MS_LIST_H + 3)
#define MS_INFO_H 40
#define MS_HINT_Y 208
#define MS_ROW_X 22
#define MS_NAME_SHOWN 34 /* characters of a file's name on its row */
#define MS_TEXT 96
#define MS_WINDOWS 24
#define MS_PAL_TEXT 0
#define MS_PAL_DONE 4
#define MS_KEYS ((1 << 10) | (1 << 11) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15))
#define MS_SND_MOVE 0x8004513E
#define MS_SND_OK 0x8004503C
#define MS_SND_BACK 0x800450BD

static struct {
    s32 request;
    s32 open;
    s32 fresh;
    s32 releasing;
    s32 close;
    s32 sel;   /* the cursor's row: 0 the game's own music, 1.. the files */
    s32 top;   /* the first row on screen */
    s32 count; /* the files */
    s32 count_windows;
    s32 open_anim;
    s32 tick;
    s32 preview_idx; /* -1: none, else 0..count-1 currently playing */
    s32 saved_bgm;   /* sound_module.current before preview started */
    const unsigned char *msg;
    MessageWindow *wins[MS_WINDOWS];
    MessageWindow *title, *rows[MS_ROWS], *line1, *line2, *vol_win, *hint;
    MessageCursor *cursor;
} ms;

static unsigned char row_text[MS_ROWS][MS_NAME_SHOWN * 2 + 8], info1[MS_TEXT], info2[MS_TEXT], vol_text[48],
    hint_text[96];

/* A file's name in the game's glyphs (docs: the font's table, tools/gen_text_override.py's): letters and digits are one
 * byte, the punctuation the names carry is `01 xx`; anything else shows as an underscore. */
static void ascii_glyphs(unsigned char *buf, int *p, int size, const char *s, int shown) {
    int n = 0;

    for (; *s != 0 && n < shown && *p < size - 3; s++, n++) {
        unsigned char c = (unsigned char)*s;
        int ext = 0x0C; /* the underscore */

        if (c >= '0' && c <= '9') {
            buf[(*p)++] = (unsigned char)(4 + c - '0');
            continue;
        }
        if (c >= 'A' && c <= 'Z') {
            buf[(*p)++] = (unsigned char)(0x0E + c - 'A');
            continue;
        }
        if (c >= 'a' && c <= 'z') {
            buf[(*p)++] = (unsigned char)(0x28 + c - 'a');
            continue;
        }
        switch (c) {
        case ' ': ext = 0x01; break;
        case '/': ext = 0x0F; break;
        case ':': ext = 0x07; break;
        case '.': ext = 0x05; break;
        case ',': ext = 0x04; break;
        case '-': ext = 0x1A; break;
        case '+': ext = 0x19; break;
        case '(': ext = 0x15; break;
        case ')': ext = 0x16; break;
        case '&': ext = 0x2B; break;
        case '\'': ext = 0x13; break;
        }
        buf[(*p)++] = 1;
        buf[(*p)++] = (unsigned char)ext;
    }
    buf[*p] = 0;
}

static MessageWindow *ms_window(s32 font, s32 x, s32 y) {
    MessageWindow *w = message_create_window(MS_LAYER, font, x, y);

    w->set_ot_depth(w, 1);
    if (ms.count_windows < MS_WINDOWS) {
        ms.wins[ms.count_windows++] = w;
    }
    return w;
}


static void ms_rect_ot(s32 depth, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(MS_LAYER);
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

static void ms_slant_ot(s32 depth, s32 x, s32 y, s32 w, s32 h, s32 skew, s32 abr, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(MS_LAYER);
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

static void ms_panel(s32 x, s32 y, s32 w, s32 h, s32 band) {
    if (band) {
        ms_rect_ot(2, x + 2, y + 2, w - 4, MS_LINE + 2, -1, 28, 32, 54);
    }
    ms_rect_ot(2, x, y, 2, h, -1, 240, 196, 86);
    ms_rect_ot(2, x, y, 2, 12, -1, 255, 248, 220);
    ms_rect_ot(2, x + 2, y, w - 2, 1, -1, 152, 124, 58);
    ms_rect_ot(2, x + 2, y + h - 1, w - 2, 1, -1, 88, 72, 36);
    ms_rect_ot(2, x + 2, y + 1, w - 2, h - 2, -1, 12, 15, 26);
}

static void preview_stop(void) {
    if (ms.preview_idx < 0) {
        return;
    }
    ms.preview_idx = -1;
    if (ms.saved_bgm != 0) {
        sound_module.play(ms.saved_bgm);
    } else {
        sound_module.stop_all();
    }
}

static void preview_toggle(void) {
    int idx = ms.sel - 3;
    int key;

    if (idx < 0 || idx >= ms.count) {
        sound_module.play(MS_SND_BACK);
        return;
    }
    if (ms.preview_idx == idx) {
        sound_module.play(MS_SND_BACK);
        preview_stop();
        return;
    }
    if (ms.preview_idx < 0) {
        ms.saved_bgm = sound_module.current;
    }
    key = def_midi_preview_play(idx);
    if (key != 0) {
        ms.preview_idx = idx;
        sound_module.play(key);
    } else {
        sound_module.play(MS_SND_BACK);
    }
}

static void row_name(int k, unsigned char *buf, int size) {
    int p = 0;

    buf[0] = 0;
    if (k < 3) {
        def_glyph_put(buf, &p, size, k == 0 ? def_str_MidiOwn : k == 1 ? def_str_MidiShuffle : def_str_MidiOrder);
        return;
    }
    {
        const char *name = def_midi_name(k - 3);
        char shown[80];
        char *dot;

        snprintf(shown, sizeof(shown), "%s", name);
        dot = strrchr(shown, '.');
        if (dot != NULL) {
            *dot = 0; /* the extension says nothing */
        }
        ascii_glyphs(buf, &p, size, shown, MS_NAME_SHOWN);
    }
}

static void board_fill(void) {
    int i, p = 0, chosen = def_midi_chosen();
    char dir[512];
    const char *tail;

    for (i = 0; i < MS_ROWS; i++) {
        int k = ms.top + i;

        ms.rows[i]->set_visible(ms.rows[i], k <= ms.count + 2);
        if (k <= ms.count + 2) {
            row_name(k, row_text[i], sizeof(row_text[i]));
            ms.rows[i]->set_text(ms.rows[i], row_text[i], -1);
            ms.rows[i]->set_palette(ms.rows[i], k == chosen ? MS_PAL_DONE : MS_PAL_TEXT);
        }
    }
    def_midi_dir(dir, sizeof(dir));
    tail = strlen(dir) > 30 ? dir + strlen(dir) - 30 : dir;
    info1[0] = 0;
    def_glyph_put(info1, &p, MS_TEXT, def_str_MidiFolder);
    ascii_glyphs(info1, &p, MS_TEXT, tail, 31);
    ms.line1->set_text(ms.line1, info1, -1);
    ms.line2->set_text(ms.line2,
                       (u8 *)(ms.msg != NULL ? ms.msg : ms.count == 0 ? def_str_MidiNone : def_str_MidiMore), -1);
    ms.line2->set_palette(ms.line2, ms.msg != NULL ? MS_PAL_DONE : MS_PAL_TEXT);

    if (ms.vol_win != NULL) {
        if (ms.sel >= 3 && ms.sel - 3 < ms.count) {
            char vbuf[32];
            int vp = 0;
            int vol = def_midi_volume(ms.sel - 3);
            snprintf(vbuf, sizeof(vbuf), "VOL %d%%", vol);
            vol_text[0] = 0;
            ascii_glyphs(vol_text, &vp, sizeof(vol_text), vbuf, 16);
            ms.vol_win->set_visible(ms.vol_win, 1);
            ms.vol_win->set_palette(ms.vol_win, 3);
            ms.vol_win->set_text(ms.vol_win, vol_text, -1);
            ms.vol_win->measure(ms.vol_win, 1);
        } else {
            ms.vol_win->set_visible(ms.vol_win, 0);
        }
    }
    if (ms.hint != NULL) {
        int hp = 0;
        const char *hstr = def_pick("A:Pick  X:Play/Stop  L/R:Vol  B:Back",
                                    "A:Escolher  X:Tocar  Esq/Dir:Vol  B:Sair",
                                    "A:Choisir  X:Jouer  G/D:Vol  B:Retour",
                                    "A:Wahl  X:Test  L/R:Vol  B:Zurueck",
                                    "A:Scegli  X:Prova  S/D:Vol  B:Esci",
                                    "A:Elegir  X:Probar  I/D:Vol  B:Salir");
        hint_text[0] = 0;
        ascii_glyphs(hint_text, &hp, sizeof(hint_text), hstr, 46);
        ms.hint->set_text(ms.hint, hint_text, -1);
    }
    ms.cursor->set_pos(ms.cursor, MS_ROW_X - 14, MS_TOP + (ms.sel - ms.top) * MS_LINE);
    ms.cursor->show(ms.cursor, 0);
}

static void board_open(void) {
    int i;

    ms.open_anim = 0;
    ms.tick = 0;
    ms.preview_idx = -1;
    ms.saved_bgm = sound_module.current;

    ms.title = ms_window(1, 10, 5);
    ms.title->set_text(ms.title, (u8 *)def_str_MidiTitle, -1);
    for (i = 0; i < MS_ROWS; i++) {
        ms.rows[i] = ms_window(1, MS_ROW_X, MS_TOP + i * MS_LINE);
        ms.rows[i]->set_palette(ms.rows[i], MS_PAL_TEXT);
    }
    ms.line1 = ms_window(1, 10, MS_INFO_Y + 4);
    ms.line1->set_palette(ms.line1, MS_PAL_TEXT);
    ms.line2 = ms_window(1, 10, MS_INFO_Y + 4 + 16);
    ms.vol_win = ms_window(1, 308, MS_INFO_Y + 4);
    ms.hint = ms_window(1, 8, MS_HINT_Y + 6);
    ms.cursor = message_create_cursor(MS_LAYER, 1, MS_ROW_X - 14, MS_TOP);
    ms.cursor->show(ms.cursor, 0);
    board_fill();
}

static void board_draw(void) {
    GfxLayer *layer = gfx_module.funcs.get_layer(MS_LAYER);
    s32 wide = port_video_widescreen_pad();
    int chosen = def_midi_chosen();
    s32 op = ms.open_anim < 10 ? ms.open_anim : 10;
    s32 title_dx = -((10 - op) * (10 - op));
    s32 list_h = MS_LIST_H * op / 10;
    s32 info_h = MS_INFO_H * op / 10;
    s32 sel_row = ms.sel - ms.top;

    ms.title->set_pos(ms.title, 10 + title_dx, 5);

    /* Selected row highlight + slanted [A] chip */
    if (op >= 10 && sel_row >= 0 && sel_row < MS_ROWS) {
        s32 ry = MS_TOP + sel_row * MS_LINE - 1;
        ms_slant_ot(2, 5, ry, 10, MS_LINE - 1, 2, -1, 44, 192, 96);
        ms_rect_ot(2, 13, ry, 301, MS_LINE - 1, 1, 74, 58, 18);
        ms_rect_ot(2, 13, ry, 301, 1, -1, 248, 206, 96);
    }

    if (op >= 10 && chosen >= ms.top && chosen < ms.top + MS_ROWS) {
        ms_rect_ot(2, MS_ROW_X - 4, MS_TOP + (chosen - ms.top) * MS_LINE - 1, 294, MS_LINE - 1, 1, 22, 68, 38);
    }

    /* Live PLAY badge on the track currently being previewed */
    if (op >= 10 && ms.preview_idx >= 0 && layer != NULL) {
        s32 prow = (ms.preview_idx + 3) - ms.top;
        if (prow >= 0 && prow < MS_ROWS) {
            s32 py = MS_TOP + prow * MS_LINE;
            u8 col[3] = { 255, 244, 180 };
            if ((ms.tick / 12) & 1) {
                col[0] = 120;
                col[1] = 248;
                col[2] = 176;
            }
            def_poly_text(layer, 0, 268, py + 2, 128, "PLAY", col);
            ms_slant_ot(2, 263, py - 1, 44, MS_LINE - 1, -2, -1, 26, 96, 56);
        }
    }

    /* Volume bar in the bottom info panel when hovering a MIDI file */
    if (op >= 10 && ms.sel >= 3 && ms.sel - 3 < ms.count) {
        s32 vol = def_midi_volume(ms.sel - 3);
        s32 bw = 88;
        s32 fill = vol * bw / 100;
        ms_rect_ot(2, 220, MS_INFO_Y + 22, fill, 6, -1, 244, 196, 82);
        ms_rect_ot(2, 220, MS_INFO_Y + 22, bw, 6, -1, 28, 32, 48);
        ms_rect_ot(2, 219, MS_INFO_Y + 21, bw + 2, 8, -1, 152, 124, 58);
    }

    ms_panel(title_dx, 0, 320, MS_TITLE_H, 1);
    if (list_h > 4) {
        ms_panel(2, MS_TOP - 5, 316, list_h, 0);
        if (op < 10) {
            ms_rect_ot(2, 2, MS_TOP - 5 + list_h - 2, 316, 2, -1, 255, 228, 130);
        }
    }
    if (info_h > 4) {
        ms_panel(2, MS_INFO_Y, 316, info_h, 0);
    }
    ms_panel(0, MS_HINT_Y, 320, 240 - MS_HINT_Y, 0);

    /* Full-screen opaque obsidian-navy backdrop with subtle diagonal Persona bands */
    ms_slant_ot(2, 216, 0, 72, 240, -38, -1, 16, 22, 42);
    ms_slant_ot(2, 74, 0, 44, 240, -38, -1, 12, 17, 34);
    ms_rect_ot(2, -wide, 0, 320 + wide * 2, 240, -1, 7, 10, 18);
}

static void move_to(int k) {
    ms.sel = k;
    ms.msg = NULL;
    if (k < ms.top) {
        ms.top = k;
    } else if (k >= ms.top + MS_ROWS) {
        ms.top = k - MS_ROWS + 1;
    }
    sound_module.play(MS_SND_MOVE);
    board_fill();
}

static void confirm(void) {
    def_midi_choose(ms.sel);
    sound_module.play(MS_SND_OK);
    ms.msg = ms.sel == 0 ? def_str_MidiOwnSet : ms.sel >= 3 && def_midi_was_cut(ms.sel - 3) ? def_str_MidiCut : def_str_MidiSet;
    board_fill();
}

static void board_input(void) {
    int k = ms.sel, n = ms.count + 3;

    if (PAD_PRESSED(4) || PAD_REPEAT(4)) {
        move_to((k + n - 1) % n);
    } else if (PAD_PRESSED(6) || PAD_REPEAT(6)) {
        move_to((k + 1) % n);
    } else if (PAD_PRESSED(7) || PAD_REPEAT(7)) {
        if (k >= 3 && k - 3 < ms.count) {
            def_midi_set_volume(k - 3, def_midi_volume(k - 3) - 5);
            sound_module.play(MS_SND_MOVE);
            board_fill();
        }
    } else if (PAD_PRESSED(5) || PAD_REPEAT(5)) {
        if (k >= 3 && k - 3 < ms.count) {
            def_midi_set_volume(k - 3, def_midi_volume(k - 3) + 5);
            sound_module.play(MS_SND_MOVE);
            board_fill();
        }
    } else if (PAD_PRESSED(15)) {
        preview_toggle();
        board_fill();
    } else if (PAD_PRESSED(10)) {
        move_to(k - MS_ROWS < 0 ? 0 : k - MS_ROWS);
    } else if (PAD_PRESSED(11)) {
        move_to(k + MS_ROWS >= n ? n - 1 : k + MS_ROWS);
    } else if (PAD_PRESSED(13)) {
        confirm();
    } else if (PAD_PRESSED(12) || PAD_PRESSED(14)) {
        preview_stop();
        sound_module.play(MS_SND_BACK);
        ms.close = 1;
    }
}

static void ms_close(void) {
    s32 i;

    for (i = 0; i < ms.count_windows; i++) {
        heap_objects.stop((Object *)ms.wins[i]);
        ms.wins[i] = NULL;
    }
    ms.count_windows = 0;
    if (ms.cursor != NULL) {
        heap_objects.stop((Object *)ms.cursor);
        ms.cursor = NULL;
    }
    ms.open = 0;
    ms.request = 0;
}

static void ms_run(void) {
    s32 i;

    for (i = 0; i < ms.count_windows; i++) {
        heap_objects.try_run((Object *)ms.wins[i]);
    }
    if (ms.cursor != NULL) {
        heap_objects.try_run((Object *)ms.cursor);
    }
}

/* The field back to the player once none of the screen's buttons is down (the Sepikmon board's sh_release). */
static int ms_release(void) {
    FieldstgActor *player;

    if (pad_state.get_held(0) & MS_KEYS) {
        fieldstg_stage.event_running = 1;
        return 1;
    }
    ms.releasing = 0;
    fieldstg_stage.event_running = 0;
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player != NULL) {
        player->reset_control(player);
    }
    return 1;
}

/* Etemon's line is over: the list opens on the field's next frame. */
void def_midi_offer(void) {
    if (!def_on() || ms.open || ms.request) {
        return;
    }
    ms.request = 1;
    fieldstg_stage.event_running = 1;
    port_log("midi: Etemon's jukebox");
}

int def_midi_frame(void) {
    if (!def_on()) {
        return 0;
    }
    if (ms.releasing) {
        return ms_release();
    }
    if (!ms.open && !ms.request) {
        return 0;
    }
    fieldstg_stage.event_running = 1;
    if (!ms.open) {
        ms.count_windows = 0;
        ms.cursor = NULL;
        ms.close = 0;
        ms.count = def_midi_scan();
        ms.sel = def_midi_chosen();
        ms.top = ms.sel < MS_ROWS ? 0 : ms.sel - MS_ROWS + 1;
        ms.msg = NULL;
        board_open();
        ms.open = 1;
        ms.request = 0;
        ms.fresh = 1;
    }
    if (!ms.fresh) {
        board_input();
    }
    ms.fresh = 0;
    if (ms.close) {
        ms_close();
        ms.releasing = 1;
        return 1;
    }
    ms.tick++;
    if (ms.open_anim < 10) {
        ms.open_anim++;
    }
    ms_run();
    board_draw();
    return 1;
}

void def_midi_savestate(struct PortState *s) {
    /* the board points into the field's heap and at the texts below: all of it goes with the heap */
    PORT_STATE_VAR(s, ms);
    PORT_STATE_VAR(s, row_text);
    PORT_STATE_VAR(s, info1);
    PORT_STATE_VAR(s, info2);
    PORT_STATE_VAR(s, vol_text);
    PORT_STATE_VAR(s, hint_text);
}
