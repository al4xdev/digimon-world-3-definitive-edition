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
 * and the new game, on a panel under the logo: the question, the two answers as cards side by side (a name, two lines
 * on what it means) and, under them, a remark on the one chosen (the easy one gets teased). Then, on the same panel,
 * who the hero is (Girls Power). The title has no message font (its windows draw nothing here), so the words are the
 * callouts' polygon letters (def_poly_text). Their sizes were picked on a TV: nothing under 10 pixels of the 240, and
 * what is not chosen stays light enough to read. Any direction moves, circle confirms, cross goes back a question. Not
 * asked in a replay script. */
#define DIFF_X 14
#define DIFF_Y 138 /* under the logo; over the copyright lines while it is up */
#define DIFF_W 292
#define DIFF_H 101
#define DIFF_CARD_Y (DIFF_Y + 23)
#define DIFF_CARD_W 136
#define DIFF_CARD_H 54
#define DIFF_CARD_DX 144
#define DIFF_PAD 8
#define DIFF_TITLE_SIZE 176 /* 8.8 of the 16-pixel letters */
#define DIFF_NAME_SIZE 256
#define DIFF_LINE_SIZE 160
#define DIFF_TEASE_SIZE 160
static s32 diff_cursor; /* 0 easy, 1 hard */
s32 stdwtitl_question_open; /* the copyright lines (stdwtitl_glow_draw) give the panel their place */
static s32 diff_ticks;

/* A flat quad on the title's layer: abr -1 opaque, else that blend (1 adds, 2 subtracts). */
static void stdwtitl_diff_quad(StdwtitlMenu *obj, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(obj->layer_id);
    POLY_F4 *poly = gfx_module.funcs.get_packet();
    DR_TPAGE *tpage = (DR_TPAGE *)(poly + 1);
    u32 *ot = layer->get_ot_entry(layer, 0);

    setPolyF4(poly);
    setSemiTrans(poly, abr >= 0);
    setRGB0(poly, r, g, b);
    setXY4(poly, x, y, x + w, y, x, y + h, x + w, y + h);
    addPrim(ot, poly);
    if (abr < 0) {
        gfx_module.funcs.set_packet(poly + 1);
        return;
    }
    SetDrawTPage(tpage, 0, 1, getTPage(1, abr, 0, 0));
    addPrim(ot, tpage);
    gfx_module.funcs.set_packet(tpage + 1);
}

/* A line of text with its dark edge (queued after it: under it), shrunk until it fits max_w (0: any width); centred
 * on x when centre is set. */
static void stdwtitl_diff_text(StdwtitlMenu *obj, s32 x, s32 y, s32 size, s32 max_w, s32 centre, const char *text,
                               const u8 *rgb) {
    static const u8 edge[3] = { 4, 6, 24 };
    GfxLayer *layer = gfx_module.funcs.get_layer(obj->layer_id);
    s32 w = def_poly_text_width(size, text);

    while (max_w > 0 && w > max_w && size > 112) {
        size -= 8;
        w = def_poly_text_width(size, text);
    }
    if (centre) {
        x -= w / 2;
    }
    def_poly_text(layer, 0, x, y, size, text, rgb);
    def_poly_text(layer, 0, x + 1, y + 1, size, text, edge);
}

/* A question of the panel: its title, two answers (a name, two lines under it, the answer's colour) and the remark
 * under them for each. */
typedef struct StdwtitlQuestion {
    const char *title;
    const char *names[2];
    const char *lines[2][2];
    const char *tease[2];
    u8 colour[2][3];
} StdwtitlQuestion;

#include "port/game/definitive.h"

static const StdwtitlQuestion stdwtitl_q_difficulty = {
    "CHOOSE YOUR FATE!", { "EASY", "HARD" }, { { "3 DIGIMON", "THE CLASSIC PARTY" }, { "2 DIGIMON", "NO THIRD WHEEL" } },
    { "AWW! WANT A BLANKET TOO?", "NOW WE ARE TALKING!" }, { { 96, 200, 255 }, { 255, 88, 64 } },
};
static const StdwtitlQuestion stdwtitl_q_difficulty_pt = {
    "ESCOLHA SEU DESTINO!", { "FÁCIL", "DIFÍCIL" }, { { "3 DIGIMON", "O TIME CLÁSSICO" }, { "2 DIGIMON", "MODO DUPLA" } },
    { "QUER UM COBERTOR TAMBÉM?", "AGORA SIM!" }, { { 96, 200, 255 }, { 255, 88, 64 } },
};
/* Girls Power (port/game/girls_power.c): who the hero is. Kail first: the question exists for her. */
static const StdwtitlQuestion stdwtitl_q_hero = {
    "CHOOSE YOUR HERO!", { "KAIL", "JUNIOR" }, { { "GIRLS POWER", "A NEW HEROINE" }, { "THE ORIGINAL", "AS IN 2003" } },
    { "GIRLS RULE THE WORLD!", "OLD SCHOOL IS GOLD!" }, { { 255, 128, 184 }, { 104, 168, 255 } },
};
static const StdwtitlQuestion stdwtitl_q_hero_pt = {
    "ESCOLHA SEU HERÓI!", { "KAIL", "JUNIOR" }, { { "GAROTAS NO PODER", "UMA NOVA HEROÍNA" }, { "O CLÁSSICO", "COMO EM 2003" } },
    { "AS GAROTAS MANDAM!", "RAIZ É OUTRO NÍVEL!" }, { { 255, 128, 184 }, { 104, 168, 255 } },
};
static s32 hero_cursor; /* 0 Kail, 1 Junior */

/* One frame of a question: 0 while it is open, 1 confirmed (*cursor), -1 cancelled. Everything is queued front to
 * back: the words, the cards (a frame, then its fill), the panel's lines, then its dark fill. */
static s32 stdwtitl_question(StdwtitlMenu *obj, const StdwtitlQuestion *q, s32 *cursor) {
    static const u8 white[3] = { 255, 255, 255 }, name_off[3] = { 176, 182, 206 }, line_off[3] = { 146, 152, 178 },
                    title[3] = { 255, 212, 72 };
    s32 i, k;

    if (def_is_ptbr()) {
        if (q == &stdwtitl_q_difficulty) {
            q = &stdwtitl_q_difficulty_pt;
        } else if (q == &stdwtitl_q_hero) {
            q = &stdwtitl_q_hero_pt;
        }
    }

    diff_ticks++;
    stdwtitl_question_open = 1;
    stdwtitl_diff_text(obj, DIFF_X + DIFF_W / 2, DIFF_Y + 6, DIFF_TITLE_SIZE, DIFF_W - 16, 1, q->title, title);
    stdwtitl_diff_text(obj, DIFF_X + DIFF_W / 2, DIFF_CARD_Y + DIFF_CARD_H + 9, DIFF_TEASE_SIZE, DIFF_W - 16, 1,
                       q->tease[*cursor], q->colour[*cursor]);
    for (i = 0; i < 2; i++) {
        s32 x = DIFF_X + 4 + i * DIFF_CARD_DX;
        s32 on = *cursor == i;
        const u8 *c = q->colour[i];

        stdwtitl_diff_text(obj, x + DIFF_PAD, DIFF_CARD_Y + 6, DIFF_NAME_SIZE, DIFF_CARD_W - 2 * DIFF_PAD, 0, q->names[i],
                           on ? c : name_off);
        for (k = 0; k < 2; k++) {
            stdwtitl_diff_text(obj, x + DIFF_PAD, DIFF_CARD_Y + 27 + k * 15, DIFF_LINE_SIZE, DIFF_CARD_W - 2 * DIFF_PAD, 0,
                               q->lines[i][k], on ? white : line_off);
        }
        if (on) {
            /* the chosen card: its colour's frame, two pixels, breathing; a fill of the colour, dark */
            s32 glow = 160 + ((diff_ticks & 31) < 16 ? (diff_ticks & 15) : 15 - (diff_ticks & 15)) * 6;

            stdwtitl_diff_quad(obj, x + 2, DIFF_CARD_Y + 2, DIFF_CARD_W - 4, DIFF_CARD_H - 4, -1, c[0] / 5, c[1] / 5,
                               c[2] / 4 + 8);
            stdwtitl_diff_quad(obj, x, DIFF_CARD_Y, DIFF_CARD_W, DIFF_CARD_H, -1, c[0] * glow / 255, c[1] * glow / 255,
                               c[2] * glow / 255);
        } else {
            stdwtitl_diff_quad(obj, x + 1, DIFF_CARD_Y + 1, DIFF_CARD_W - 2, DIFF_CARD_H - 2, -1, 14, 18, 36);
            stdwtitl_diff_quad(obj, x, DIFF_CARD_Y, DIFF_CARD_W, DIFF_CARD_H, -1, 64, 72, 104);
        }
    }
    stdwtitl_diff_quad(obj, DIFF_X, DIFF_Y, DIFF_W, 1, 1, 60, 130, 230);
    stdwtitl_diff_quad(obj, DIFF_X, DIFF_Y + DIFF_H - 1, DIFF_W, 1, 1, 60, 130, 230);
    stdwtitl_diff_quad(obj, DIFF_X, DIFF_Y + 1, 1, DIFF_H - 2, 1, 60, 130, 230);
    stdwtitl_diff_quad(obj, DIFF_X + DIFF_W - 1, DIFF_Y + 1, 1, DIFF_H - 2, 1, 60, 130, 230);
    stdwtitl_diff_quad(obj, DIFF_X, DIFF_Y, DIFF_W, DIFF_H, 2, 235, 228, 190);
    /* any direction: the cards sit side by side, and the old panel's rows went up and down */
    if (PAD_PRESSED(4) || PAD_REPEAT(4) || PAD_PRESSED(5) || PAD_REPEAT(5) || PAD_PRESSED(6) || PAD_REPEAT(6) ||
        PAD_PRESSED(7) || PAD_REPEAT(7)) {
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
                stdwtitl_question_open = 0;
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

/* The slot get_result is read as a word (stdwtitl_title_run), so this returns one: a 16-bit return leaves the upper half of
 * the register undefined on the host (the PS1's MIPS sign-extended it), and a stale half read as a choice started the
 * flash and sent the title to a wrong map: the black screen after Start. */
s32 stdwtitl_menu_get_result(StdwtitlMenu *obj) {
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
