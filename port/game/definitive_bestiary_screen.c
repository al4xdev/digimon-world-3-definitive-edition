#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "cdload.h"
#include "fightstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "object.h"
#include "pad.h"
#include "port.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/libgte.h"
#include "records.h"
#include "sound.h"

#include "psxstack/port_harness.h"

#include "definitive.h"
#include "definitive_bestiary_ext.h"
#include "definitive_persona.h"

extern FightstgModel *fightstg_model_create_idle(s32 id, s32 anim_file, FightstgPos pos, FightstgModelParams *params);
extern void fightstg_model_set_color(FightstgModel *obj, s32 use_color, CVECTOR *color);
extern s32 fightstg_enemy_find_record(s32 id);
extern void def_recolor_set_model(const void *model, int kind, int degrees);
extern void def_recolor_enemy_hue(int degrees);
extern void *font_get_entry(void *data, s32 i);
extern int def_hunt_common_of(int index);
extern const unsigned char *def_element_icon(int element);
extern const unsigned char def_str_Unknown[], def_str_Weak[], def_str_Resist[], def_str_Neutral[];

#define BS_LAYER_BG   0x1000
#define BS_LAYER_3D   0x1002
#define BS_LAYER_UI   0x1005

#define BS_LIST_ROWS  11
#define BS_ROW_H      16
#define BS_ROW_Y0     34

#define BS_VP_X       8
#define BS_VP_Y       136
#define BS_VP_W       146
#define BS_VP_H       82

#define BS_LOAD_DEBOUNCE 7

#define BS_SND_MOVE   0x8004513E
#define BS_SND_OK     0x8004503C
#define BS_SND_BACK   0x800450BD
#define BS_SND_TAB    0x40019

typedef struct BestiaryStage {
    Object base;
    s32 step;
    s32 tick;
    s32 open_anim;
    s32 cat;
    s32 sel;
    s32 scroll;
    s32 phase;
    s32 filtered[512];
    s32 filtered_count;
    s32 seen_total;
    s32 total_count;

    s32 load_delay;
    s32 pending_species;
    s32 pending_entry_idx;
    s32 loading_model_file;
    s32 loading_anim_file;
    s32 loaded_model_file;
    s32 loaded_anim_file;
    s32 loaded_entry_idx;
    s32 loaded_phase;
    s32 model_height;
    s32 model_depth;
    s32 cam_yaw;
    s32 cam_pitch;
    s32 cam_zoom;
    s32 auto_rot;
    s32 model_sil;
    s32 model_sil_done;
    FightstgModel *model;
    FightstgModelParams mparams;

    MessageWindow *win_tabs;
    MessageWindow *win_count;
    MessageWindow *win_name;
    MessageWindow *win_tag;
    MessageWindow *win_fam_icon[5];
    MessageWindow *win_fam_state[5];
    MessageWindow *win_drop;
    MessageWindow *win_hint;
    MessageWindow *win_loading;
    MessageWindow *win_rows[BS_LIST_ROWS];
    MessageWindow *win_row_tags[BS_LIST_ROWS];
} BestiaryStage;

static s32 bs_act_inited;
static s32 bs_act_l = -1;
static s32 bs_act_r = -1;
static s32 bs_act_u = -1;
static s32 bs_act_d = -1;

static void bs_init_actions(void) {
    if (bs_act_inited) {
        return;
    }
    bs_act_inited = 1;
    bs_act_l = port_input_action("bestiary_rot_l", NULL, "bestiary", "[\"pad:rightx-\", \"J\"]");
    bs_act_r = port_input_action("bestiary_rot_r", NULL, "bestiary", "[\"pad:rightx+\", \"L\"]");
    bs_act_u = port_input_action("bestiary_rot_u", NULL, "bestiary", "[\"pad:righty-\", \"I\"]");
    bs_act_d = port_input_action("bestiary_rot_d", NULL, "bestiary", "[\"pad:righty+\", \"K\"]");
}

static void bs_ascii_to_glyphs(const char *s, u8 *out, s32 cap) {
    s32 n = 0;

    while (*s != '\0' && n + 3 < cap) {
        char c = *s++;
        if (c == ' ') {
            out[n++] = 0x01;
            out[n++] = 0x01;
        } else if (c >= '0' && c <= '9') {
            out[n++] = (u8)(0x04 + (c - '0'));
        } else if (c >= 'A' && c <= 'Z') {
            out[n++] = (u8)(0x0E + (c - 'A'));
        } else if (c >= 'a' && c <= 'z') {
            out[n++] = (u8)(0x28 + (c - 'a'));
        } else if (c == ',') {
            out[n++] = 0x01;
            out[n++] = 0x04;
        } else if (c == '.') {
            out[n++] = 0x01;
            out[n++] = 0x05;
        } else if (c == ':') {
            out[n++] = 0x01;
            out[n++] = 0x07;
        } else if (c == '?') {
            out[n++] = 0x01;
            out[n++] = 0x09;
        } else if (c == '!') {
            out[n++] = 0xE7;
        } else if (c == '/') {
            out[n++] = 0x01;
            out[n++] = 0x0F;
        } else if (c == '-') {
            out[n++] = 0x01;
            out[n++] = 0x0D;
        } else {
            out[n++] = 0x01;
            out[n++] = 0x01;
        }
    }
    out[n] = 0;
}

static void bs_rect(u32 *ot, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
    POLY_F4 *p;

    if (w <= 0 || h <= 0) {
        return;
    }
    p = gfx_module.funcs.get_packet();
    setPolyF4(p);
    setRGB0(p, r, g, b);
    setXY4(p, (s16)x, (s16)y, (s16)(x + w), (s16)y, (s16)x, (s16)(y + h), (s16)(x + w), (s16)(y + h));
    if (abr >= 0) {
        DR_TPAGE *tp;
        setSemiTrans(p, 1);
        addPrim(ot, p);
        tp = (DR_TPAGE *)(p + 1);
        setDrawTPage(tp, 0, 0, (u16)(abr << 5));
        addPrim(ot, tp);
        gfx_module.funcs.set_packet(tp + 1);
    } else {
        addPrim(ot, p);
        gfx_module.funcs.set_packet(p + 1);
    }
}

static void bs_slant(u32 *ot, s32 x, s32 y, s32 w, s32 h, s32 skew, s32 abr, u8 r, u8 g, u8 b) {
    POLY_F4 *p = gfx_module.funcs.get_packet();

    setPolyF4(p);
    setRGB0(p, r, g, b);
    setXY4(p, (s16)(x + skew), (s16)y, (s16)(x + w + skew), (s16)y, (s16)x, (s16)(y + h), (s16)(x + w), (s16)(y + h));
    if (abr >= 0) {
        DR_TPAGE *tp;
        setSemiTrans(p, 1);
        addPrim(ot, p);
        tp = (DR_TPAGE *)(p + 1);
        setDrawTPage(tp, 0, 0, (u16)(abr << 5));
        addPrim(ot, tp);
        gfx_module.funcs.set_packet(tp + 1);
    } else {
        addPrim(ot, p);
        gfx_module.funcs.set_packet(p + 1);
    }
}

static void bs_draw_mini_glyph(u32 *ot, s32 x, s32 y, char ch, u8 r, u8 g, u8 b) {
    static const struct {
        char ch;
        u8 rows[5];
    } font[] = {
        { 'A', { 0x6, 0x9, 0xF, 0x9, 0x9 } }, { 'B', { 0xE, 0x9, 0xE, 0x9, 0xE } },
        { 'C', { 0x7, 0x8, 0x8, 0x8, 0x7 } }, { 'D', { 0xE, 0x9, 0x9, 0x9, 0xE } },
        { 'E', { 0xF, 0x8, 0xE, 0x8, 0xF } }, { 'I', { 0xE, 0x4, 0x4, 0x4, 0xE } },
        { 'K', { 0x9, 0xA, 0xC, 0xA, 0x9 } }, { 'L', { 0x8, 0x8, 0x8, 0x8, 0xF } },
        { 'N', { 0x9, 0xD, 0xB, 0x9, 0x9 } }, { 'O', { 0x6, 0x9, 0x9, 0x9, 0x6 } },
        { 'R', { 0xE, 0x9, 0xE, 0xA, 0x9 } }, { 'S', { 0x7, 0x8, 0x6, 0x1, 0xE } },
        { 'T', { 0xF, 0x4, 0x4, 0x4, 0x4 } }, { 'Y', { 0x9, 0x9, 0x6, 0x4, 0x4 } },
        { 'G', { 0x7, 0x8, 0xB, 0x9, 0x7 } }, { 'M', { 0x9, 0xF, 0xF, 0x9, 0x9 } },
        { 'P', { 0xE, 0x9, 0xE, 0x8, 0x8 } }, { 'H', { 0x9, 0x9, 0xF, 0x9, 0x9 } },
        { 'Z', { 0xF, 0x2, 0x4, 0x8, 0xF } }, { '2', { 0xE, 0x1, 0x6, 0x8, 0xF } },
        { '1', { 0x4, 0xC, 0x4, 0x4, 0xE } },
    };
    s32 i, row, col;

    for (i = 0; i < (s32)(sizeof(font) / sizeof(font[0])); i++) {
        if (font[i].ch != ch) {
            continue;
        }
        for (row = 0; row < 5; row++) {
            u8 bits = font[i].rows[row];
            for (col = 0; col < 4; col++) {
                if (bits & (8 >> col)) {
                    bs_rect(ot, x + col, y + row, 1, 1, -1, r, g, b);
                }
            }
        }
        return;
    }
}

static void bs_draw_mini_str(u32 *ot, s32 x, s32 y, const char *s, u8 r, u8 g, u8 b) {
    while (*s != '\0') {
        if (*s != ' ') {
            bs_draw_mini_glyph(ot, x, y, *s, r, g, b);
        }
        x += 5;
        s++;
    }
}

static s32 bs_cat_palette(int cat) {
    switch (cat) {
    case DEF_BS_CAT_BOSS:  return 3;
    case DEF_BS_CAT_RARE:  return 1;
    case DEF_BS_CAT_SUPER: return 3;
    case DEF_BS_CAT_DRI:   return 1;
    case DEF_BS_CAT_DARK:  return 4;
    default:               return 0;
    }
}

static void bs_unload_model(BestiaryStage *obj, s32 next_model_file, s32 next_anim_file) {
    if (obj->model != NULL) {
        obj->model->base.set_state(obj->model, OBJECT_STATE_END);
        obj->model = (FightstgModel *)heap_objects.try_run((Object *)obj->model);
    }
    def_recolor_set_model(NULL, DEF_RECOLOR_NONE, 0);
    if (obj->loaded_model_file != 0 && obj->loaded_model_file != next_model_file) {
        cdload_module.files.free_file(obj->loaded_model_file);
        obj->loaded_model_file = 0;
    }
    if (obj->loaded_anim_file != 0 && obj->loaded_anim_file != next_anim_file &&
        obj->loaded_anim_file != obj->loaded_model_file) {
        cdload_module.files.free_file(obj->loaded_anim_file);
        obj->loaded_anim_file = 0;
    }
    obj->loaded_entry_idx = -1;
}

static void bs_request_model(BestiaryStage *obj) {
    if (obj->filtered_count <= 0) {
        bs_unload_model(obj, 0, 0);
        obj->pending_entry_idx = -1;
        return;
    }
    obj->pending_entry_idx = obj->filtered[obj->sel];
    obj->load_delay = BS_LOAD_DEBOUNCE;
    obj->loading_model_file = 0;
    obj->loading_anim_file = 0;
}

static void bs_rebuild_filter(BestiaryStage *obj) {
    s32 i, n = def_bestiary_ext_count();

    obj->filtered_count = 0;
    obj->seen_total = 0;
    obj->total_count = n;
    for (i = 0; i < n; i++) {
        const DefBestiaryEntry *e = def_bestiary_ext_get(i);
        if (e == NULL) {
            continue;
        }
        if (def_bestiary_ext_seen(e)) {
            obj->seen_total++;
        }
        if (obj->cat == DEF_BS_CAT_ALL || e->category == obj->cat) {
            obj->filtered[obj->filtered_count++] = i;
        }
    }
    obj->sel = 0;
    obj->scroll = 0;
    obj->phase = 0;
    bs_request_model(obj);
}

static void bs_set_digimon_name(MessageWindow *win, s32 species_id, s32 known, s32 pal) {
    void *names = cdload_module.files.get_file(records_language + 0x4E);

    win->set_palette(win, pal);
    if (!known || names == NULL) {
        win->set_text(win, (u8 *)def_str_Unknown, -1);
        return;
    }
    {
        FightstgEnemyRecord *recs = (FightstgEnemyRecord *)cdload_module.files.get_file(0x1CF);
        s32 idx = fightstg_enemy_find_record(species_id);
        s32 name_id = species_id;

        if (recs != NULL && idx >= 0 && idx < DEF_BESTIARY_SPECIES) {
            name_id = recs[idx].name;
        } else if (species_id > 0 && species_id < 512) {
            RecordsDigimon *drec = records_get_digimon_func(species_id);
            if (drec != NULL) {
                name_id = drec->name_id;
            }
        }
        win->set_text(win, names, name_id);
    }
}

static void bs_refresh_ui(BestiaryStage *obj) {
    static const char *const tab_names[DEF_BS_CAT_COUNT] = {
        "ALL", "WILD", "BOSS", "RARE", "SUPER", "DRI", "DARK",
    };
    static const int fam_elem[5] = { 0, 1, 3, 4, 6 };
    static const int fam_bits[5] = { 1 << 0, (1 << 1) | (1 << 2), 1 << 3, (1 << 4) | (1 << 5), 1 << 6 };
    u8 buf[96];
    char tmp[64];
    s32 i;

    if (obj->sel < obj->scroll) {
        obj->scroll = obj->sel;
    } else if (obj->sel >= obj->scroll + BS_LIST_ROWS) {
        obj->scroll = obj->sel - BS_LIST_ROWS + 1;
    }

    snprintf(tmp, sizeof(tmp), "L1 %s R1", tab_names[obj->cat]);
    bs_ascii_to_glyphs(tmp, buf, sizeof(buf));
    obj->win_tabs->set_palette(obj->win_tabs, 1);
    obj->win_tabs->set_text(obj->win_tabs, buf, -1);

    snprintf(tmp, sizeof(tmp), "%03d/%03d", obj->seen_total, obj->total_count);
    bs_ascii_to_glyphs(tmp, buf, sizeof(buf));
    obj->win_count->set_palette(obj->win_count, 3);
    obj->win_count->set_text(obj->win_count, buf, -1);
    obj->win_count->measure(obj->win_count, 1);

    for (i = 0; i < BS_LIST_ROWS; i++) {
        s32 idx = obj->scroll + i;
        if (idx >= obj->filtered_count) {
            buf[0] = 0;
            obj->win_rows[i]->set_text(obj->win_rows[i], buf, -1);
            obj->win_row_tags[i]->set_text(obj->win_row_tags[i], buf, -1);
            continue;
        }
        {
            const DefBestiaryEntry *e = def_bestiary_ext_get(obj->filtered[idx]);
            s32 seen = def_bestiary_ext_seen(e);
            s32 chosen = idx == obj->sel;
            s32 pal = !seen ? 7 : chosen ? 3 : 0;

            bs_set_digimon_name(obj->win_rows[i], e->species_id, seen, pal);
            bs_ascii_to_glyphs(def_bestiary_ext_cat_tag(e->category), buf, sizeof(buf));
            obj->win_row_tags[i]->set_palette(obj->win_row_tags[i], seen ? bs_cat_palette(e->category) : 7);
            obj->win_row_tags[i]->set_text(obj->win_row_tags[i], buf, -1);
            obj->win_row_tags[i]->measure(obj->win_row_tags[i], 1);
        }
    }

    if (obj->filtered_count <= 0) {
        buf[0] = 0;
        obj->win_name->set_text(obj->win_name, buf, -1);
        obj->win_tag->set_text(obj->win_tag, buf, -1);
        obj->win_drop->set_text(obj->win_drop, buf, -1);
        for (i = 0; i < 5; i++) {
            obj->win_fam_state[i]->set_text(obj->win_fam_state[i], buf, -1);
        }
        return;
    }

    {
        const DefBestiaryEntry *e = def_bestiary_ext_get(obj->filtered[obj->sel]);
        s32 seen = def_bestiary_ext_seen(e);
        s32 mask = def_bestiary_ext_known(e, obj->phase);
        short resists[7];

        bs_set_digimon_name(obj->win_name, e->species_id, seen, seen ? 0 : 7);
        if (e->phases > 1) {
            snprintf(tmp, sizeof(tmp), "%s P%c", def_bestiary_ext_cat_tag(e->category), obj->phase ? 'B' : 'A');
        } else {
            snprintf(tmp, sizeof(tmp), "%s", def_bestiary_ext_cat_tag(e->category));
        }
        bs_ascii_to_glyphs(tmp, buf, sizeof(buf));
        obj->win_tag->set_palette(obj->win_tag, bs_cat_palette(e->category));
        obj->win_tag->set_text(obj->win_tag, buf, -1);
        obj->win_tag->measure(obj->win_tag, 1);

        def_bestiary_ext_resists(e, obj->phase, resists);
        for (i = 0; i < 5; i++) {
            s32 known_fam = (mask & fam_bits[i]) != 0;
            const unsigned char *lbl = def_str_Unknown;
            s32 pal = 7;

            obj->win_fam_icon[i]->set_text(obj->win_fam_icon[i], (u8 *)def_element_icon(fam_elem[i]), -1);
            if (known_fam) {
                s32 r = resists[fam_elem[i]];
                if (r <= 65) {
                    lbl = def_str_Weak;
                    pal = 3;
                } else if (r >= 200) {
                    lbl = def_str_Resist;
                    pal = 1;
                } else {
                    lbl = def_str_Neutral;
                    pal = 0;
                }
            }
            obj->win_fam_state[i]->set_palette(obj->win_fam_state[i], pal);
            obj->win_fam_state[i]->set_text(obj->win_fam_state[i], (u8 *)lbl, -1);
            obj->win_fam_state[i]->measure(obj->win_fam_state[i], 1);
        }

        if (seen && e->sdigiedt_idx >= 0) {
            s32 mat = def_hunt_common_of(e->sdigiedt_idx);
            if (mat >= 0) {
                obj->win_drop->set_palette(obj->win_drop, 0);
                obj->win_drop->set_text(obj->win_drop, (u8 *)def_hunt_material_name(mat), -1);
            } else {
                bs_ascii_to_glyphs("---", buf, sizeof(buf));
                obj->win_drop->set_palette(obj->win_drop, 7);
                obj->win_drop->set_text(obj->win_drop, buf, -1);
            }
        } else {
            obj->win_drop->set_palette(obj->win_drop, 7);
            obj->win_drop->set_text(obj->win_drop, (u8 *)def_str_Unknown, -1);
        }
    }
}

static void bs_update_camera(BestiaryStage *obj) {
    GfxLayer *layer3d = gfx_module.funcs.get_layer(BS_LAYER_3D);
    GsRVIEW2 view;
    GsF_LIGHT light;
    s32 dist;
    s32 h = obj->model_height > 40 ? obj->model_height : 240;
    s32 d = obj->model_depth > 40 ? obj->model_depth : 200;
    s32 base_dist;
    s32 cy, sy, cp, sp;
    s32 horiz;

    if (layer3d == NULL) {
        return;
    }
    base_dist = d > 400 ? (d * 155 / 100) : (h * 9 + 1100);
    if (base_dist < 1700) {
        base_dist = 1700;
    }
    dist = base_dist * obj->cam_zoom / 100;
    if (dist < 600) {
        dist = 600;
    }

    {
        double yaw_rad = (double)obj->cam_yaw * 6.28318530718 / 4096.0;
        double pitch_rad = (double)obj->cam_pitch * 6.28318530718 / 4096.0;
        cy = (s32)(cos(yaw_rad) * 4096.0);
        sy = (s32)(sin(yaw_rad) * 4096.0);
        cp = (s32)(cos(pitch_rad) * 4096.0);
        sp = (s32)(sin(pitch_rad) * 4096.0);
    }
    horiz = (dist * cp) >> 12;

    view.vrx = 0;
    view.vry = -(h * 6 / 10);
    view.vrz = 0;
    view.vpx = (horiz * sy) >> 12;
    view.vpy = view.vry - ((dist * sp) >> 12);
    view.vpz = (horiz * cy) >> 12;
    view.rz = 0;
    view.super = NULL;

    GsSetProjection(125);
    GsSetRefView2(&view);
    layer3d->save_camera(layer3d, 1, 125);

    if (obj->model_sil) {
        light.vx = 0;
        light.vy = -2000;
        light.vz = 2000;
        light.r = 0x20;
        light.g = 0x30;
        light.b = 0x50;
        GsSetFlatLight(0, &light);
        light.r = 0;
        light.g = 0;
        light.b = 0;
        GsSetFlatLight(1, &light);
        GsSetFlatLight(2, &light);
        SetBackColor(0x0C, 0x12, 0x22);
        layer3d->save_light(layer3d, 1);
        return;
    }

    light.vx = 1800;
    light.vy = -2200;
    light.vz = 2600;
    light.r = 0xE8;
    light.g = 0xEE;
    light.b = 0xFF;
    GsSetFlatLight(0, &light);
    light.vx = -2000;
    light.vy = -800;
    light.vz = -1600;
    light.r = 0x58;
    light.g = 0x80;
    light.b = 0xB8;
    GsSetFlatLight(1, &light);
    light.vx = 0;
    light.vy = 2000;
    light.vz = 0;
    light.r = 0x20;
    light.g = 0x38;
    light.b = 0x58;
    GsSetFlatLight(2, &light);
    SetBackColor(0x52, 0x5C, 0x72);
    layer3d->save_light(layer3d, 1);
}

static void bs_tick_model_loader(BestiaryStage *obj) {
    if (obj->pending_entry_idx < 0) {
        return;
    }
    if (obj->load_delay > 0) {
        obj->load_delay--;
        return;
    }
    if (obj->loaded_entry_idx == obj->pending_entry_idx && obj->loaded_phase == obj->phase && obj->model != NULL) {
        return;
    }
    {
        const DefBestiaryEntry *e = def_bestiary_ext_get(obj->pending_entry_idx);
        FightstgModelRecordA *mrec;
        s32 mf, af;

        if (e == NULL) {
            return;
        }
        if (obj->loaded_entry_idx == obj->pending_entry_idx && obj->model != NULL && obj->loaded_phase != obj->phase) {
            obj->loaded_phase = obj->phase;
            if (def_bestiary_ext_seen(e) && e->recolor == DEF_RECOLOR_PERSONA) {
                def_recolor_enemy_hue(e->hue[obj->phase & 1]);
            }
            return;
        }
        mrec = (FightstgModelRecordA *)fightstg_models.get(e->species_id);
        if (mrec == NULL || mrec->model_file == 0) {
            bs_unload_model(obj, 0, 0);
            obj->loaded_entry_idx = obj->pending_entry_idx;
            return;
        }
        mf = mrec->model_file >> 16;
        af = mrec->anim_file >> 16;
        if (obj->loading_model_file != mf || obj->loading_anim_file != af) {
            bs_unload_model(obj, mf, af);
            obj->loading_model_file = mf;
            obj->loading_anim_file = af;
        }
        if (cdload_module.is_loading(mf) != 0 || (af != 0 && cdload_module.is_loading(af) != 0)) {
            return;
        }

        memset(&obj->mparams, 0, sizeof(obj->mparams));
        obj->mparams.in_use = 1;
        obj->mparams.id = 0x10;
        obj->mparams.model_id = e->species_id;
        obj->mparams.layers[0].shown = 1;
        obj->mparams.layers[0].layer_id = BS_LAYER_3D;
        obj->mparams.layers[0].edges = 0;

        {
            FightstgPos vram_pos = { 0x200, 0 };
            s32 seen = def_bestiary_ext_seen(e);

            obj->model = fightstg_model_create_idle(mrec->model_file, mrec->anim_file, vram_pos, &obj->mparams);
            obj->loaded_model_file = mf;
            obj->loaded_anim_file = af;
            obj->loaded_entry_idx = obj->pending_entry_idx;
            obj->loaded_phase = obj->phase;
            obj->model_height = mrec->height > 20 ? mrec->height : 240;
            {
                double ex = (double)(mrec->camera_eyes[0][0] - mrec->camera_targets[0][0]);
                double ey = (double)(mrec->camera_eyes[0][1] - mrec->camera_targets[0][1]);
                double ez = (double)(mrec->camera_eyes[0][2] - mrec->camera_targets[0][2]);
                s32 cam_d = (s32)sqrt(ex * ex + ey * ey + ez * ez);
                obj->model_depth = cam_d > 500 ? (cam_d * 135 / 100) : (mrec->depth > 20 ? mrec->depth * 6 : 1200);
            }
            obj->cam_yaw = 320;
            obj->cam_pitch = 160;
            obj->cam_zoom = 100;
            obj->auto_rot = 1;

            obj->model_sil = !seen;
            obj->model_sil_done = 0;
            if (!seen) {
                def_recolor_set_model(obj->model, DEF_RECOLOR_SILHOUETTE, 0);
            } else {
                def_recolor_set_model(obj->model, e->recolor, e->hue[obj->phase & 1]);
            }
        }
    }
}

static void bs_draw_chrome(BestiaryStage *obj) {
    GfxLayer *bg = gfx_module.funcs.get_layer(BS_LAYER_BG);
    GfxLayer *ui = gfx_module.funcs.get_layer(BS_LAYER_UI);
    u32 *ot_bg;
    u32 *ot_ui0;
    u32 *ot_ui1;
    s32 i;
    s32 row_y;

    if (bg == NULL || ui == NULL) {
        return;
    }
    ot_bg = bg->get_ot_entry(bg, 1);
    ot_ui0 = ui->get_ot_entry(ui, 0);
    ot_ui1 = ui->get_ot_entry(ui, 1);

    /* Deep Persona navy backdrop + subtle diagonal cyan-navy bands */
    bs_slant(ot_bg, 210, 0, 74, 240, -34, -1, 10, 24, 58);
    bs_slant(ot_bg, 72, 0, 44, 240, -34, -1, 8, 19, 46);
    bs_rect(ot_bg, 0, 0, 320, 240, -1, 6, 13, 30);

    /* 3D viewport floor grid & inner glass backdrop (behind the 3D model) */
    for (i = 0; i < 4; i++) {
        bs_rect(ot_bg, BS_VP_X + 6, BS_VP_Y + BS_VP_H - 18 + i * 5, BS_VP_W - 12, 1, -1, 18, 52, 106);
    }
    bs_rect(ot_bg, BS_VP_X + 2, BS_VP_Y + 2, BS_VP_W - 4, BS_VP_H - 4, -1, 6, 16, 38);
    bs_rect(ot_bg, BS_VP_X, BS_VP_Y, 2, BS_VP_H, -1, 48, 196, 255);
    bs_rect(ot_bg, BS_VP_X, BS_VP_Y, BS_VP_W, 1, -1, 32, 92, 188);
    bs_rect(ot_bg, BS_VP_X, BS_VP_Y + BS_VP_H - 1, BS_VP_W, 1, -1, 32, 92, 188);

    /* Top bar: slanted BESTIARY nameplate */
    bs_draw_mini_str(ot_ui0, 14, 9, "BESTIARY", 248, 252, 255);
    bs_slant(ot_ui1, 6, 4, 86, 16, 6, -1, 10, 26, 62);
    bs_slant(ot_ui1, 8, 20, 84, 2, 1, -1, 56, 224, 255);

    /* Top-right counter dark chip */
    bs_rect(ot_ui1, 252, 5, 62, 15, -1, 6, 14, 34);
    bs_rect(ot_ui1, 252, 19, 62, 1, -1, 48, 196, 255);

    /* Top-left quadrant: Info, Weaknesses & Drops glass card */
    bs_rect(ot_ui1, 8, 26, 2, 104, -1, 48, 196, 255);
    bs_rect(ot_ui1, 8, 26, 2, 12, -1, 240, 248, 255);
    bs_rect(ot_ui1, 12, 43, 140, 1, -1, 32, 88, 176);
    bs_rect(ot_ui1, 12, 114, 140, 1, -1, 32, 88, 176);
    bs_rect(ot_ui1, 10, 26, 144, 104, 2, 170, 155, 115);

    /* Right column: Digimon list glass card */
    bs_rect(ot_ui1, 160, 26, 2, 192, -1, 48, 196, 255);
    bs_rect(ot_ui1, 160, 26, 2, 14, -1, 240, 248, 255);

    /* Selected row highlight (no [A] confirm badge since the list is browse-only) */
    if (obj->filtered_count > 0) {
        s32 rel = obj->sel - obj->scroll;
        if (rel >= 0 && rel < BS_LIST_ROWS) {
            row_y = BS_ROW_Y0 + rel * BS_ROW_H;
            bs_rect(ot_ui1, 160, row_y - 1, 3, 15, -1, 240, 250, 255);
            bs_rect(ot_ui1, 163, row_y - 1, 149, 15, 1, 18, 56, 118);
            bs_rect(ot_ui1, 163, row_y - 1, 149, 1, -1, 64, 224, 255);
        }
    }
    bs_rect(ot_ui1, 162, 26, 152, 192, 2, 170, 155, 115);

    /* Scrollbar on right edge of list */
    if (obj->filtered_count > BS_LIST_ROWS) {
        s32 track_h = 180;
        s32 thumb_h = track_h * BS_LIST_ROWS / obj->filtered_count;
        s32 thumb_y;
        if (thumb_h < 14) {
            thumb_h = 14;
        }
        thumb_y = 32 + (track_h - thumb_h) * obj->scroll / (obj->filtered_count - BS_LIST_ROWS);
        bs_rect(ot_ui1, 312, 32, 2, track_h, -1, 18, 40, 82);
        bs_rect(ot_ui1, 312, thumb_y, 2, thumb_h, -1, 56, 224, 255);
    }

    /* Bottom-right red [B] BACK sticker */
    bs_draw_mini_glyph(ot_ui0, 260, 225, 'B', 255, 248, 248);
    bs_draw_mini_str(ot_ui0, 272, 225, "BACK", 240, 248, 255);
    bs_slant(ot_ui1, 255, 221, 14, 13, -2, -1, 216, 40, 68);
    bs_slant(ot_ui1, 254, 220, 16, 15, -2, -1, 255, 240, 244);
    bs_slant(ot_ui1, 268, 222, 44, 12, -2, -1, 10, 24, 58);

    /* Bottom-left controls hint */
    bs_draw_mini_str(ot_ui0, 10, 225, "R ROT  L2R2 ZOOM", 140, 196, 240);

    /* Loading indicator in 3D viewport */
    if (obj->pending_entry_idx >= 0 &&
        (obj->loaded_entry_idx != obj->pending_entry_idx || obj->load_delay > 0)) {
        if ((obj->tick / 6) & 1) {
            bs_draw_mini_str(ot_ui0, BS_VP_X + BS_VP_W - 42, BS_VP_Y + 6, "LOADING", 72, 232, 255);
        }
    }
}

static void bs_create_windows(BestiaryStage *obj) {
    s32 i;

    obj->win_tabs = message_create_window(BS_LAYER_UI, 1, 104, 6);
    obj->win_count = message_create_window(BS_LAYER_UI, 1, 310, 6);
    obj->win_name = message_create_window(BS_LAYER_UI, 1, 14, 29);
    obj->win_tag = message_create_window(BS_LAYER_UI, 1, 150, 29);
    for (i = 0; i < 5; i++) {
        obj->win_fam_icon[i] = message_create_window(BS_LAYER_UI, 1, 14, 46 + i * 13);
        obj->win_fam_state[i] = message_create_window(BS_LAYER_UI, 1, 148, 46 + i * 13);
    }
    obj->win_drop = message_create_window(BS_LAYER_UI, 1, 14, 116);
    for (i = 0; i < BS_LIST_ROWS; i++) {
        s32 y = BS_ROW_Y0 + i * BS_ROW_H;
        obj->win_rows[i] = message_create_window(BS_LAYER_UI, 1, 168, y);
        obj->win_row_tags[i] = message_create_window(BS_LAYER_UI, 1, 308, y);
    }
}

static void bs_update_windows(BestiaryStage *obj) {
    s32 i;

    heap_objects.try_run((Object *)obj->win_tabs);
    heap_objects.try_run((Object *)obj->win_count);
    heap_objects.try_run((Object *)obj->win_name);
    heap_objects.try_run((Object *)obj->win_tag);
    heap_objects.try_run((Object *)obj->win_drop);
    for (i = 0; i < 5; i++) {
        heap_objects.try_run((Object *)obj->win_fam_icon[i]);
        heap_objects.try_run((Object *)obj->win_fam_state[i]);
    }
    for (i = 0; i < BS_LIST_ROWS; i++) {
        heap_objects.try_run((Object *)obj->win_rows[i]);
        heap_objects.try_run((Object *)obj->win_row_tags[i]);
    }
}

static void bs_stage_update(BestiaryStage *obj) {
    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default: {
        RECT r_full = { 0, 0, 320, 240 };
        RECT r_vp = { BS_VP_X, BS_VP_Y, BS_VP_W, BS_VP_H };
        GfxLayer *bg;
        GfxLayer *vp;
        GfxLayer *ui;

        bs_init_actions();
        fightstg_models.id = -1;
        fightstg_models.record = NULL;
        gfx_module.reset();
        gfx_module.alloc_packet_buffers(0x28000);
        gfx_module.funcs.init_display(320, 240, 0, 0);

        bg = gfx_module.funcs.create_layer(&r_full, 2, BS_LAYER_BG);
        bg->set_bg_color(bg, 6, 13, 30);

        vp = gfx_module.funcs.create_layer(&r_vp, 0xC, BS_LAYER_3D);
        vp->set_draw_offset(vp, BS_VP_X + BS_VP_W / 2, BS_VP_Y + BS_VP_H / 2 + 8);
        vp->alloc_callbacks(vp, 0x60);

        ui = gfx_module.funcs.create_layer(&r_full, 2, BS_LAYER_UI);
        (void)ui;

        obj->loaded_entry_idx = -1;
        obj->pending_entry_idx = -1;
        obj->cam_yaw = 320;
        obj->cam_pitch = 160;
        obj->cam_zoom = 100;
        obj->auto_rot = 1;
        obj->step = 0;
        obj->base.next_state(obj);
        break;
    }
    case OBJECT_STATE_RUN:
        obj->tick++;
        if (obj->step == 0) {
            s32 a = cdload_module.is_loading(0x1CC);
            s32 b = cdload_module.is_loading(0x1CF);
            s32 c = cdload_module.is_loading(records_language + 0x4E);
            s32 d = cdload_module.is_loading(records_language + 0x6A);
            if (a == 0 && b == 0 && c == 0 && d == 0) {
                def_bestiary_ext_build();
                bs_create_windows(obj);
                bs_rebuild_filter(obj);
                bs_refresh_ui(obj);
                obj->step = 1;
            }
            break;
        }
        if (obj->step == 1) {
            s32 changed = 0;

            if (PAD_PRESSED(14)) {
                sound_module.play(BS_SND_BACK);
                bs_unload_model(obj, 0, 0);
                gamestate_data.funcs.set_next_map(gamestate_data.field_map, 0);
                obj->step = 2;
                break;
            }
            if (PAD_PRESSED(10)) {
                sound_module.play(BS_SND_TAB);
                obj->cat = (obj->cat + DEF_BS_CAT_COUNT - 1) % DEF_BS_CAT_COUNT;
                bs_rebuild_filter(obj);
                bs_refresh_ui(obj);
            } else if (PAD_PRESSED(11)) {
                sound_module.play(BS_SND_TAB);
                obj->cat = (obj->cat + 1) % DEF_BS_CAT_COUNT;
                bs_rebuild_filter(obj);
                bs_refresh_ui(obj);
            } else if (obj->filtered_count > 0) {
                if (PAD_PRESSED(4) || PAD_REPEAT(4)) {
                    sound_module.play(BS_SND_MOVE);
                    obj->sel = (obj->sel + obj->filtered_count - 1) % obj->filtered_count;
                    obj->phase = 0;
                    changed = 1;
                } else if (PAD_PRESSED(6) || PAD_REPEAT(6)) {
                    sound_module.play(BS_SND_MOVE);
                    obj->sel = (obj->sel + 1) % obj->filtered_count;
                    obj->phase = 0;
                    changed = 1;
                } else if (PAD_PRESSED(7) || PAD_REPEAT(7)) {
                    sound_module.play(BS_SND_MOVE);
                    obj->sel = obj->sel - BS_LIST_ROWS;
                    if (obj->sel < 0) {
                        obj->sel = 0;
                    }
                    obj->phase = 0;
                    changed = 1;
                } else if (PAD_PRESSED(5) || PAD_REPEAT(5)) {
                    sound_module.play(BS_SND_MOVE);
                    obj->sel = obj->sel + BS_LIST_ROWS;
                    if (obj->sel >= obj->filtered_count) {
                        obj->sel = obj->filtered_count - 1;
                    }
                    obj->phase = 0;
                    changed = 1;
                } else if (PAD_PRESSED(12)) {
                    const DefBestiaryEntry *e = def_bestiary_ext_get(obj->filtered[obj->sel]);
                    if (e != NULL && e->phases > 1) {
                        sound_module.play(BS_SND_OK);
                        obj->phase ^= 1;
                        bs_refresh_ui(obj);
                    }
                }
            }

            if (changed) {
                bs_request_model(obj);
                bs_refresh_ui(obj);
            }

            /* Right stick / IJKL rotation + L2/R2 zoom */
            {
                s32 manual = 0;
                if (port_input_held(bs_act_l)) {
                    obj->cam_yaw = (obj->cam_yaw - 36) & 0xFFF;
                    manual = 1;
                }
                if (port_input_held(bs_act_r)) {
                    obj->cam_yaw = (obj->cam_yaw + 36) & 0xFFF;
                    manual = 1;
                }
                if (port_input_held(bs_act_u)) {
                    obj->cam_pitch += 24;
                    if (obj->cam_pitch > 520) {
                        obj->cam_pitch = 520;
                    }
                    manual = 1;
                }
                if (port_input_held(bs_act_d)) {
                    obj->cam_pitch -= 24;
                    if (obj->cam_pitch < -240) {
                        obj->cam_pitch = -240;
                    }
                    manual = 1;
                }
                if (manual) {
                    obj->auto_rot = 0;
                } else if (obj->auto_rot) {
                    obj->cam_yaw = (obj->cam_yaw + 8) & 0xFFF;
                }
                if (PAD_HELD(8)) {
                    obj->cam_zoom += 2;
                    if (obj->cam_zoom > 200) {
                        obj->cam_zoom = 200;
                    }
                }
                if (PAD_HELD(9)) {
                    obj->cam_zoom -= 2;
                    if (obj->cam_zoom < 55) {
                        obj->cam_zoom = 55;
                    }
                }
            }

            bs_tick_model_loader(obj);
            bs_update_camera(obj);
            if (obj->model != NULL) {
                fightstg_battle.frames = 1;
                obj->model = (FightstgModel *)heap_objects.try_run((Object *)obj->model);
                if (obj->model != NULL && obj->model_sil && !obj->model_sil_done &&
                    obj->model->base.state == OBJECT_STATE_RUN && obj->model->base.children != NULL) {
                    CVECTOR sil = { 10, 14, 28, 0 };
                    fightstg_model_set_color(obj->model, 1, &sil);
                    obj->model_sil_done = 1;
                }
            }
            bs_update_windows(obj);
            bs_draw_chrome(obj);
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        bs_unload_model(obj, 0, 0);
        gfx_module.funcs.delete_layer(BS_LAYER_UI);
        gfx_module.funcs.delete_layer(BS_LAYER_3D);
        gfx_module.funcs.delete_layer(BS_LAYER_BG);
        break;
    }
}

Object *def_bestiary_stage_entry(void) {
    return object_new(bs_stage_update, sizeof(BestiaryStage), 0);
}
