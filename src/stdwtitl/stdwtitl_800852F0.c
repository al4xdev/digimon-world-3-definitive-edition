#include "common.h"
#include "object.h"
#include "gfx.h"
#include "cdload.h"
#include "sound.h"
#include "pad.h"
#include "records.h"
#include "stdwtitl.h"

/* A position of stdwtitl_menu_positions. */
typedef struct StdwtitlMenuPos {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
} StdwtitlMenuPos; /* size 0x8 */

extern StdwtitlMenuPos stdwtitl_menu_positions[3]; /* the two choices, then "press start" */
extern u8 stdwtitl_menu_sprites[7][4];     /* per language: sprites of "press start" and the choices */

void stdwtitl_menu_draw(StdwtitlMenu *obj) {
    Sprite spr;

    if (obj->cursor_shown != 0) {
        sprite_init(&spr);
        spr.set_layer_id(obj->layer_id, 0);
        spr.set_vram_pos(0x280, 0x100);
        spr.set_palette(obj->blink);
        spr.draw(cdload_module.get_subfile_by_id(stdwtitl_module.bank), 7, stdwtitl_menu_positions[obj->cursor].x,
                   stdwtitl_menu_positions[obj->cursor].y - 1);
    }
    if (obj->is_open == 0) {
        sprite_init(&spr);
        spr.set_layer_id(obj->layer_id, 0);
        spr.set_vram_pos(0x280, 0x100);
        spr.set_palette(obj->cursor != 2);
        spr.draw(cdload_module.get_subfile_by_id(stdwtitl_module.bank), stdwtitl_menu_sprites[records_language][0], 0x4F, 0xA6);
    } else {
        sprite_init(&spr);
        spr.set_layer_id(obj->layer_id, 0);
        spr.set_vram_pos(0x280, 0x100);
        spr.set_palette(obj->cursor != 0);
        spr.draw(cdload_module.get_subfile_by_id(stdwtitl_module.bank), stdwtitl_menu_sprites[records_language][1], obj->choice1_x,
                   obj->choice1_y);
        sprite_init(&spr);
        spr.set_layer_id(obj->layer_id, 0);
        spr.set_vram_pos(0x280, 0x100);
        spr.set_palette(obj->cursor != 1);
        spr.draw(cdload_module.get_subfile_by_id(stdwtitl_module.bank), stdwtitl_menu_sprites[records_language][2], obj->choice2_x,
                   obj->choice2_y);
    }
}

#ifdef PC_PORT
/* New Game's difficulty question (Hard Mode: a party of two; port/game/definitive.c). Asked between the title's choice
 * and the new game, on a panel where the title's two choices were: two lines to choose from and, under them, a remark
 * on the one the cursor is on (the easy one gets teased). Then, on the same panel, who the hero is (Girls Power). The
 * title has no message font (its windows draw nothing here), so the words are the callouts' polygon letters
 * (def_poly_text). Up/down move, circle confirms, cross goes back a question. Not asked in a replay script. */
#define DIFF_X 40
#define DIFF_Y 135 /* under the logo, above the copyright lines */
#define DIFF_W 240
#define DIFF_H 60
#define DIFF_ROW_Y (DIFF_Y + 19) /* the first choice's line; the second is DIFF_ROW_DY under it */
#define DIFF_ROW_DY 13
static s32 diff_cursor; /* 0 easy, 1 hard */
static s32 diff_ticks;

static void stdwtitl_diff_quad(StdwtitlMenu *obj, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(obj->layer_id);
    POLY_F4 *poly = gfx_module.funcs.get_packet();
    DR_TPAGE *tpage = (DR_TPAGE *)(poly + 1);
    u32 *ot = layer->get_ot_entry(layer, 0);

    setPolyF4(poly);
    setSemiTrans(poly, 1);
    setRGB0(poly, r, g, b);
    setXY4(poly, x, y, x + w, y, x, y + h, x + w, y + h);
    addPrim(ot, poly);
    SetDrawTPage(tpage, 0, 1, getTPage(1, abr, 0, 0));
    addPrim(ot, tpage);
    gfx_module.funcs.set_packet(tpage + 1);
}

/* A line of text with its dark edge (queued after it: under it). */
static void stdwtitl_diff_text(StdwtitlMenu *obj, s32 x, s32 y, s32 size, const char *text, const u8 *rgb) {
    static const u8 edge[3] = { 8, 12, 40 };
    GfxLayer *layer = gfx_module.funcs.get_layer(obj->layer_id);

    def_poly_text(layer, 0, x, y, size, text, rgb);
    def_poly_text(layer, 0, x + 1, y + 1, size, text, edge);
}

/* A question of the panel: its title, two choices (a name, a line beside it from line_x, the name's colour) and the
 * remark under them for each. */
typedef struct StdwtitlQuestion {
    const char *title;
    const char *names[2];
    const char *lines[2];
    const char *tease[2];
    s32 line_x;
    u8 colour[2][3];
} StdwtitlQuestion;

#include "port/game/definitive.h"

static const StdwtitlQuestion stdwtitl_q_difficulty = {
    "CHOOSE YOUR FATE!", { "EASY", "HARD" }, { "3 DIGIMON", "2 DIGIMON  NO THIRD WHEEL" },
    { "AWW! WANT A BLANKET TOO?", "NOW WE ARE TALKING!" }, 78, { { 96, 200, 255 }, { 255, 72, 56 } },
};
static const StdwtitlQuestion stdwtitl_q_difficulty_pt = {
    "ESCOLHA SEU DESTINO!", { "FACIL", "DIFICIL" }, { "3 DIGIMON", "2 DIGIMON  MODO DUPLA" },
    { "QUER UM COBERTOR TAMBEM?", "AGORA SIM!" }, 90, { { 96, 200, 255 }, { 255, 72, 56 } },
};
/* Girls Power (port/game/girls_power.c): who the hero is. Kaili first: the question exists for her. */
static const StdwtitlQuestion stdwtitl_q_hero = {
    "CHOOSE YOUR HERO!", { "KAILI", "JUNIOR" }, { "GIRLS POWER", "THE ORIGINAL" },
    { "GIRLS RULE THE WORLD!", "OLD SCHOOL IS GOLD!" }, 104, { { 255, 120, 170 }, { 96, 160, 255 } },
};
static const StdwtitlQuestion stdwtitl_q_hero_pt = {
    "ESCOLHA SEU HEROI!", { "KAILI", "JUNIOR" }, { "GAROTAS NO PODER", "O CLASSICO" },
    { "AS GAROTAS MANDAM!", "RAIZ E OUTRO NIVEL!" }, 104, { { 255, 120, 170 }, { 96, 160, 255 } },
};
static s32 hero_cursor; /* 0 Kaili, 1 Junior */

/* One frame of a question: 0 while it is open, 1 confirmed (*cursor), -1 cancelled. Everything is queued front to
 * back: the words, the frame's lines, the cursor's bar, then the dark panel. */
static s32 stdwtitl_question(StdwtitlMenu *obj, const StdwtitlQuestion *q, s32 *cursor) {
    static const u8 white[3] = { 255, 255, 255 }, grey[3] = { 120, 128, 150 }, title[3] = { 255, 208, 64 };
    s32 i;

    if (def_is_ptbr()) {
        if (q == &stdwtitl_q_difficulty) {
            q = &stdwtitl_q_difficulty_pt;
        } else if (q == &stdwtitl_q_hero) {
            q = &stdwtitl_q_hero_pt;
        }
    }

    diff_ticks++;
    stdwtitl_diff_text(obj, DIFF_X + 12, DIFF_Y + 5, 128, q->title, title);
    for (i = 0; i < 2; i++) {
        s32 y = DIFF_ROW_Y + i * DIFF_ROW_DY;
        s32 on = *cursor == i;

        stdwtitl_diff_text(obj, DIFF_X + 26, y, 160, q->names[i], on ? q->colour[i] : grey);
        stdwtitl_diff_text(obj, DIFF_X + q->line_x, y + 1, 128, q->lines[i], on ? white : grey);
        if (on) { /* the cursor: a wedge that nudges, and the line's bar */
            s32 nudge = (diff_ticks >> 3) & 1;

            stdwtitl_diff_text(obj, DIFF_X + 11 + nudge, y, 160, "!", q->colour[i]);
            stdwtitl_diff_quad(obj, DIFF_X + 6, y - 2, DIFF_W - 12, 13, 1, q->colour[i][0] / 4, q->colour[i][1] / 6 + 8,
                               q->colour[i][2] / 4);
        }
    }
    stdwtitl_diff_text(obj, DIFF_X + 12, DIFF_Y + 47, 128, q->tease[*cursor], q->colour[*cursor]);
    stdwtitl_diff_quad(obj, DIFF_X, DIFF_Y, DIFF_W, 1, 1, 60, 130, 230);
    stdwtitl_diff_quad(obj, DIFF_X, DIFF_Y + DIFF_H - 1, DIFF_W, 1, 1, 60, 130, 230);
    stdwtitl_diff_quad(obj, DIFF_X, DIFF_Y + 1, 1, DIFF_H - 2, 1, 60, 130, 230);
    stdwtitl_diff_quad(obj, DIFF_X + DIFF_W - 1, DIFF_Y + 1, 1, DIFF_H - 2, 1, 60, 130, 230);
    stdwtitl_diff_quad(obj, DIFF_X + 6, DIFF_Y + 15, DIFF_W - 12, 1, 1, 40, 80, 160);
    stdwtitl_diff_quad(obj, DIFF_X, DIFF_Y, DIFF_W, DIFF_H, 2, 225, 215, 160);
    if (PAD_PRESSED(4) || PAD_REPEAT(4) || PAD_PRESSED(6) || PAD_REPEAT(6)) {
        *cursor = !*cursor;
        sound_module.play(0x8004513E);
    }
    if (PAD_PRESSED(13)) {
        sound_module.play(0x8004503C);
        return 1;
    }
    if (PAD_PRESSED(14)) {
        sound_module.play(0x800450BD);
        return -1;
    }
    return 0;
}
#endif

void stdwtitl_menu_update(StdwtitlMenu *obj) {
    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        if (obj->skip_start == 0) {
            obj->base.next_state(obj);
        } else {
            obj->base.set_state(obj, OBJECT_STATE_DONE);
        }
        obj->result = 0;
        break;
    case OBJECT_STATE_RUN:
        if (obj->base.step == 0) {
            break;
        }
        switch (obj->base.step) {
        case 1:
        default:
            if (obj->ticks++ >= 6) {
                obj->cursor = 2;
                obj->ticks = 0;
                obj->cursor_shown = 1;
                obj->base.next_step(obj);
            }
            break;
        case 2:
            if (PAD_PRESSED(3)) {
                obj->base.next_step(obj);
                obj->ticks = 0;
                sound_module.play(0x8004503C);
                obj->blink = 0;
            }
            if (++obj->ticks >= 600) {
                obj->result = 3;
            }
            break;
        case 3:
            if ((++obj->ticks & 3) == 0) {
                obj->ticks = 0;
                if (++obj->blink >= 3) {
                    obj->base.next_step(obj);
                    obj->is_open = 1;
                    obj->choice1_x = obj->choice2_x = 0x4F;
                    obj->choice1_y = obj->choice2_y = 0xA6;
                    obj->blink = 0;
                    obj->cursor_shown = 0;
                    sound_module.play(0x8004113E);
                }
            }
            break;
        case 4:
            obj->choice1_x = (stdwtitl_menu_positions[0].x - 0x4F) * obj->ticks / 15 + 0x4F;
            obj->choice1_y = (stdwtitl_menu_positions[0].y - 0xA6) * obj->ticks / 15 + 0xA6;
            obj->choice2_x = (stdwtitl_menu_positions[1].x - 0x4F) * obj->ticks / 15 + 0x4F;
            obj->choice2_y = (stdwtitl_menu_positions[1].y - 0xA6) * obj->ticks / 15 + 0xA6;
            if (++obj->ticks >= 15) {
                obj->ticks = 0;
                obj->base.next_step(obj);
            }
            break;
        case 5:
            if (++obj->ticks >= 6) {
                obj->base.set_state(obj, OBJECT_STATE_DONE);
            }
            break;
        }
        stdwtitl_menu_draw(obj);
        break;
    case OBJECT_STATE_DONE:
        switch (obj->base.step) {
        case 0:
        default:
            obj->is_open = 1;
            obj->cursor = 1;
            obj->cursor_shown = 1;
            obj->choice1_x = stdwtitl_menu_positions[0].x;
            obj->choice1_y = stdwtitl_menu_positions[0].y;
            obj->choice2_x = stdwtitl_menu_positions[1].x;
            obj->choice2_y = stdwtitl_menu_positions[1].y;
            obj->base.next_step(obj);
            break;
        case 1:
            if (PAD_PRESSED(4) || PAD_REPEAT(4)) {
                if (obj->cursor != 0) {
                    sound_module.play(0x8004513E);
                }
                obj->cursor = 0;
            }
            if (PAD_PRESSED(6) || PAD_REPEAT(6)) {
                if (obj->cursor != 1) {
                    sound_module.play(0x8004513E);
                }
                obj->cursor = 1;
            }
            if (PAD_PRESSED(13)) {
                obj->ticks = 0;
                sound_module.play(0x8004503C);
                obj->base.next_step(obj);
            }
            break;
        case 2:
            if ((++obj->ticks & 3) == 0) {
                obj->ticks = 0;
                if (++obj->blink >= 3) {
#ifdef PC_PORT
                    if (obj->cursor == 0 && def_bestiary_active()) { /* New Game: the difficulty is asked first */
                        obj->blink = 0;
                        diff_cursor = 0;
                        obj->base.set_step(obj, 10);
                        break;
                    }
#endif
                    if (obj->cursor != 0) {
                        obj->result = 2;
                    } else {
                        obj->result = 1;
                    }
                    obj->base.next_step(obj);
                    obj->blink = 0;
                }
            }
            break;
        case 3:
            break;
#ifdef PC_PORT
        case 10:
            switch (stdwtitl_question(obj, &stdwtitl_q_difficulty, &diff_cursor)) {
            case 1: /* then who the hero is */
                hero_cursor = 0;
                obj->base.set_step(obj, 11);
                break;
            case -1:
                obj->base.set_step(obj, 1);
                break;
            }
            return; /* the question stands where the title's choices are: they are not drawn under it */
        case 11:
            switch (stdwtitl_question(obj, &stdwtitl_q_hero, &hero_cursor)) {
            case 1:
                def_new_game_difficulty(diff_cursor);
                def_new_game_hero(hero_cursor == 0);
                obj->result = 1;
                obj->base.set_step(obj, 3);
                break;
            case -1: /* back to the difficulty, as it was */
                obj->base.set_step(obj, 10);
                break;
            }
            return;
#endif
        }
        stdwtitl_menu_draw(obj);
        break;
    case OBJECT_STATE_END:
        break;
    }
}

void stdwtitl_menu_start(StdwtitlMenu *obj) {
    if (obj->base.state == OBJECT_STATE_RUN) {
        obj->base.set_step(obj, 1);
    }
}

void stdwtitl_menu_show_press_start(StdwtitlMenu *obj) {
    if (obj->base.state == OBJECT_STATE_RUN) {
        obj->cursor = 2;
        obj->cursor_shown = 1;
        obj->ticks = 0;
        obj->base.set_step(obj, 2);
    }
}

s16 stdwtitl_menu_get_result(StdwtitlMenu *obj) {
    return obj->result;
}

StdwtitlMenu *stdwtitl_menu_create(s16 arg0) {
    StdwtitlMenu *obj = object_new(stdwtitl_menu_update, sizeof(StdwtitlMenu), 0);

    obj->start = stdwtitl_menu_start;
    obj->show_press_start = stdwtitl_menu_show_press_start;
    obj->get_result = (s32 (*)(struct StdwtitlMenu *))stdwtitl_menu_get_result;
    obj->layer_id = 0x1000;
    obj->ot_depth = 2;
    obj->skip_start = arg0;
    return obj;
}

StdwtitlMenuPos stdwtitl_menu_positions[3] = {
    { 79, 154 },
    { 79, 174 },
    { 79, 166 },
};

u8 stdwtitl_menu_sprites[7][4] = {
    { 8, 9, 10, 0 },
    { 8, 9, 10, 0 },
    { 8, 9, 10, 0 },
    { 17, 18, 19, 0 },
    { 11, 12, 13, 0 },
    { 20, 21, 22, 0 },
    { 14, 15, 16, 0 },
};
