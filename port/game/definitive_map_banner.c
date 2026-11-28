#include "common.h"
#include "cdload.h"
#include "fieldstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "port.h"
#include "psyq/libgpu.h"
#include "records.h"

#include "psxstack/savestate.h"

#include "definitive.h"

extern void *font_get_entry(void *data, s32 i);

typedef struct MapBannerTitle {
    u8 text_0;
    u8 text_1;
    s16 map;
} MapBannerTitle;

extern MapBannerTitle fieldstg_map_titles[];

#define MB_LAYER 0x1004
#define MB_FONT 1
#define MB_PAD 8
#define MB_MIN_W 96
#define MB_MAX_W 224
#define MB_H 36
#define MB_X 8
#define MB_Y 8
#define MB_SLIDE 8
#define MB_HOLD 110
#define MB_TOTAL (MB_SLIDE + MB_HOLD + MB_SLIDE)
#define MB_SECTORS 0xA9
#define MB_MAPS 0xB7
#define MB_PAL_SECTOR 1
#define MB_PAL_MAP 0

static struct {
    s32 pending;
    s16 map;
    s32 left;
    s32 w;
    MessageWindow *win0, *win1;
    s32 w0, w1;
} mb;

static MessageWindow *mb_window(const u8 *text, s32 palette, s32 *width) {
    MessageWindow *w;

    *width = 0;
    if (text == NULL || text[0] == 0) {
        return NULL;
    }
    w = message_create_window(MB_LAYER, MB_FONT, 0, 0);
    w->set_ot_depth(w, 0);
    w->set_text(w, (u8 *)text, -1);
    w->set_palette(w, (u8)palette);
    w->measure(w, 1);
    *width = w->measured_width;
    return w;
}

static void mb_close(void) {
    heap_objects.stop((Object *)mb.win0);
    heap_objects.stop((Object *)mb.win1);
    mb.win0 = NULL;
    mb.win1 = NULL;
    mb.w0 = 0;
    mb.w1 = 0;
    mb.left = 0;
    mb.pending = 0;
}

static s32 mb_make(void) {
    void *sectors;
    void *maps;
    const u8 *t0 = NULL;
    const u8 *t1 = NULL;
    s32 i;

    if (cdload_module.is_loading(records_language + MB_SECTORS) != 0 ||
        cdload_module.is_loading(records_language + MB_MAPS) != 0) {
        return 0;
    }
    sectors = cdload_module.files.get_file(records_language + MB_SECTORS);
    maps = cdload_module.files.get_file(records_language + MB_MAPS);
    if (sectors == NULL || maps == NULL) {
        mb.pending = 0;
        return 0;
    }
    for (i = 0; fieldstg_map_titles[i].map != 0; i++) {
        if (fieldstg_map_titles[i].map == mb.map) {
            t0 = (const u8 *)font_get_entry(sectors, fieldstg_map_titles[i].text_0);
            t1 = (const u8 *)font_get_entry(maps, fieldstg_map_titles[i].text_1);
            break;
        }
    }
    if (t0 == NULL && t1 == NULL) {
        mb.pending = 0;
        return 0;
    }
    mb.win0 = mb_window(t0, MB_PAL_SECTOR, &mb.w0);
    mb.win1 = mb_window(t1, MB_PAL_MAP, &mb.w1);
    dw3_velvet_title_window(mb.win0, mb.win1);
    if (mb.win0 != NULL) {
        mb.win0->measure(mb.win0, 1);
        mb.w0 = mb.win0->measured_width;
    }
    if (mb.win1 != NULL) {
        mb.win1->measure(mb.win1, 1);
        mb.w1 = mb.win1->measured_width;
    }
    mb.w = (mb.w0 > mb.w1 ? mb.w0 : mb.w1) + 2 * MB_PAD;
    if (mb.w < MB_MIN_W) {
        mb.w = MB_MIN_W;
    }
    if (mb.w > MB_MAX_W) {
        mb.w = MB_MAX_W;
    }
    mb.pending = 0;
    mb.left = MB_TOTAL;
    return 1;
}

static void mb_quad(GfxLayer *layer, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
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

static void mb_slanted_quad(GfxLayer *layer, s32 cx, s32 cy, s32 hw, s32 hh, s32 skew, s32 abr, u8 r, u8 g, u8 b) {
    POLY_F4 *poly = gfx_module.funcs.get_packet();
    u32 *ot = layer->get_ot_entry(layer, 0);
    const s16 vx[4] = {
        (s16)(cx - hw + skew), (s16)(cx + hw + skew + 2),
        (s16)(cx - hw - skew - 1), (s16)(cx + hw - skew)
    };
    const s16 vy[4] = {
        (s16)(cy - hh), (s16)(cy - hh - 1),
        (s16)(cy + hh + 1), (s16)(cy + hh)
    };

    setPolyF4(poly);
    setSemiTrans(poly, abr >= 0);
    setRGB0(poly, r, g, b);
    setXY4(poly, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2], vx[3], vy[3]);
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

static void mb_box(GfxLayer *layer, s32 x, s32 y, s32 w, s32 h) {
    s32 skew = 4;

    mb_quad(layer, x + skew, y, w, 2, -1, 0, 196, 255);
    mb_quad(layer, x + 2, y + 18, w - 4, 1, -1, 0, 144, 224);
    mb_quad(layer, x - skew, y + h - 1, w, 1, 1, 40, 118, 240);
    mb_slanted_quad(layer, x + w / 2, y + h / 2, w / 2, h / 2, skew, 2, 210, 200, 145);
    mb_slanted_quad(layer, x + w / 2, y + h / 2, w / 2 + 1, h / 2 + 1, skew, 1, 36, 112, 232);
}

void def_map_banner_open(short map) {
    if (!def_on()) {
        return;
    }
    mb_close();
    mb.pending = 1;
    mb.map = map;
}

void def_map_banner_frame(void) {
    Object *mgr;
    GfxLayer *layer;
    s32 x, y, shown;

    if (!def_on() || (!mb.pending && mb.left <= 0)) {
        return;
    }
    mgr = heap_objects.find(7, -1, -1);
    layer = gfx_module.funcs.get_layer(MB_LAYER);
    if (fieldstg_stage.menu_open != 0 || fieldstg_stage.event_running != 0 || fieldstg_stage.battle_starting != 0 ||
        def_rush_panel_up() || mgr == NULL || mgr->state != OBJECT_STATE_RUN || layer == NULL) {
        mb_close();
        return;
    }
    if (mb.pending && !mb_make()) {
        return;
    }
    if (mb.left <= 0) {
        mb_close();
        return;
    }
    shown = MB_TOTAL - mb.left < mb.left ? MB_TOTAL - mb.left : mb.left;
    y = shown >= MB_SLIDE ? MB_Y : MB_Y - (MB_H + MB_Y) * (MB_SLIDE - shown) / MB_SLIDE;
    x = MB_X - port_field_wide;
    if (mb.win0 != NULL) {
        mb.win0->set_pos(mb.win0, x + MB_PAD + mb.w0, y + 4);
        mb.win0 = (MessageWindow *)heap_objects.try_run((Object *)mb.win0);
    }
    if (mb.win1 != NULL) {
        mb.win1->set_pos(mb.win1, x + MB_PAD + mb.w1, y + 20);
        mb.win1 = (MessageWindow *)heap_objects.try_run((Object *)mb.win1);
    }
    mb_box(layer, x, y, mb.w, MB_H);
    mb.left -= gfx_module.funcs.get_frame_ticks();
    if (mb.left <= 0) {
        mb_close();
    }
}

int def_map_banner_up(void) {
    return mb.pending || mb.left > 0;
}

void def_map_banner_reset(void) {
    mb.win0 = NULL;
    mb.win1 = NULL;
    mb.w0 = 0;
    mb.w1 = 0;
    mb.left = 0;
    mb.pending = 0;
}

void def_map_banner_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, mb);
}
