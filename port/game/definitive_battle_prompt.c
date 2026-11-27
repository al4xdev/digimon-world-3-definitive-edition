#include <string.h>

#include "common.h"
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
extern int def_mod_apply(u8 side, s32 member, s32 kind, s32 level, s32 turns, s32 blocked_idx);
extern int def_tech_effect(u8 side, int tech, int *msg);
extern void port_log(const char *fmt, ...);

#define BP_LAYER 0x1005
#define BP_FONT 1
#define BP_SND_MOVE 0x8004513E
#define BP_SND_OK 0x8004503C

enum {
    BP_FX_NONE = 0,
    BP_FX_MOD = 1,   /* native 3-tier buff/debuff via def_mod_apply */
    BP_FX_LIBRA = 2, /* native Libra via def_tech_effect(0, 202) */
};

#define BP_PROMPTS 10

typedef struct DefBattleOpt {
    int fx_type;
    int fx_side;    /* 0 player, 1 enemy */
    int fx_kind;    /* 0 Atk, 1 Def, 2 Spd */
    int fx_level;   /* -3..-1 debuff, +1..+3 buff */
    int fx_turns;   /* 2..4 */
    int bond_ally;  /* -1 none, 0..4 Teddy..Kurt */
    int bond_delta;
    int rep_delta;
} DefBattleOpt;

typedef struct DefBattlePrompt {
    int id;
    int question;  /* >= 0: stored in expansion save; fires at most once per save */
    int battle_id; /* exact records_state.battle ID from wstag*.c */
    int hp_pct;    /* 100 = turn 1, 50 = enemy <= 50% HP */
    int opt_count;
    DefBattleOpt opts[3];
} DefBattlePrompt;

/* A prompt's words in one language. */
typedef struct DefBattleText {
    const char *speaker;
    const char *line;
    const char *opts[3][2]; /* each option's label, and the result once it is picked */
} DefBattleText;

#include "definitive_battle_prompt_text.inc" /* bp_texts[language][prompt], bp_bond[language] */

/* The words in the language shown (def_lang: en, pt, fr, de, it, es). */
static int bp_lang(void) {
    int l = def_lang();

    return l > 0 && l < 6 ? l : 0;
}

static const DefBattlePrompt prompts[BP_PROMPTS] = { /* their words: bp_texts, same order */
    /* Tamer Genji (wstag200, battle 200) */
    { 0, 220, 200, 100, 3, {
        { BP_FX_MOD, 0, 0, 1, 3, -1, 0, 1 },
        { BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_TEDDY, 1, 1 },
        { BP_FX_MOD, 0, 1, -1, 2, -1, 0, -1 },
    } },
    /* Keith in Asuka (wstag205, battle 263) */
    { 1, 221, 263, 100, 3, {
        { BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_KEITH, 1, 1 },
        { BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_TEDDY, 1, 1 },
        { BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_LISA, 1, 0 },
    } },
    /* MasterTyrannomon (wstag405, battle 2) */
    { 2, 222, 2, 50, 3, {
        { BP_FX_MOD, 1, 1, -1, 3, -1, 0, 2 },
        { BP_FX_MOD, 0, 2, 1, 3, DEF_BOND_TEDDY, 1, 0 },
        { BP_FX_MOD, 0, 0, -1, 2, DEF_BOND_IVY, -1, -1 },
    } },
    /* Seiryu Leader (wstag420, battle 3) */
    { 3, 223, 3, 50, 3, {
        { BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_LISA, 1, 2 },
        { BP_FX_MOD, 0, 2, 1, 3, DEF_BOND_TEDDY, 1, 1 },
        { BP_FX_MOD, 1, 0, 1, 2, DEF_BOND_KEITH, 1, -1 },
    } },
    /* Suzaku Leader (wstag520, battle 4) */
    { 4, 224, 4, 50, 3, {
        { BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_KEITH, 1, 2 },
        { BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_LISA, 1, 1 },
        { BP_FX_MOD, 0, 1, -1, 2, DEF_BOND_IVY, -1, -1 },
    } },
    /* Byakko duel (wstag675, battle 5) */
    { 5, 225, 5, 100, 3, {
        { BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_KEITH, 1, 2 },
        { BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_LISA, 1, 1 },
        { BP_FX_MOD, 0, 1, -1, 2, DEF_BOND_TEDDY, -1, -1 },
    } },
    /* Game Master, Admin Center (wstag310, battle 10) */
    { 6, 228, 10, 50, 3, {
        { BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_TEDDY, 2, 2 },
        { BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_LISA, 1, 1 },
        { BP_FX_MOD, 1, 2, 1, 2, DEF_BOND_IVY, -1, -1 },
    } },
    /* Chief Officer / MAGAMI President (wstag311, battle 32) */
    { 7, 233, 32, 50, 3, {
        { BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_LISA, 2, 2 },
        { BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_TEDDY, 1, 1 },
        { BP_FX_MOD, 1, 0, 1, 2, DEF_BOND_KEITH, 1, -1 },
    } },
    /* Lord Megadeath on the Gunslinger (wstag820, battle 33) */
    { 8, 234, 33, 50, 3, {
        { BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_TEDDY, 2, 2 },
        { BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_LISA, 2, 1 },
        { BP_FX_MOD, 0, 1, -1, 2, DEF_BOND_IVY, -1, -1 },
    } },
    /* Galacticmon, the final battle (wstag820, battle 324 / 0x144) */
    { 9, 235, 324, 50, 3, {
        { BP_FX_MOD, 0, 0, 2, 4, DEF_BOND_TEDDY, 2, 2 },
        { BP_FX_MOD, 0, 1, 2, 4, DEF_BOND_LISA, 2, 2 },
        { BP_FX_MOD, 0, 2, 2, 4, DEF_BOND_KEITH, 2, 2 },
    } },
};

static struct {
    s32 active;      /* -1 none, else index in prompts[] */
    s32 phase;       /* 0 selecting, 1 showing result */
    s32 sel;
    s32 tick;
    u32 fired_mask;  /* prompts already triggered in this battle */
    MessageWindow *w_spk, *w_line, *w_opts[3], *w_res;
    unsigned char t_spk[64], t_line[128], t_opts[3][96], t_res[128];
    s32 res_lines;   /* 2: the bond's words on a line above the result */
} bp;

/* The extra glyphs (01 xx) the European font draws as Latin-1 letters, by code point - 0xA0 (tools/dump_text.py
 * LATIN): every accent of the five languages but Œ and Î. 0: not in the font. */
static const unsigned char bp_latin[0x60] = {
    [0xA1 - 0xA0] = 0x3B, /* ¡ */ [0xA3 - 0xA0] = 0x3C, /* £ */ [0xA7 - 0xA0] = 0x3D, /* § */
    [0xB5 - 0xA0] = 0x3E, /* µ */ [0xBA - 0xA0] = 0x72, /* º */ [0xBF - 0xA0] = 0x3F, /* ¿ */
    [0xC0 - 0xA0] = 0x6D, /* À */ [0xC1 - 0xA0] = 0x68, /* Á */ [0xC2 - 0xA0] = 0x40, /* Â */
    [0xC3 - 0xA0] = 0x63, /* Ã */ [0xC4 - 0xA0] = 0x41, /* Ä */ [0xC5 - 0xA0] = 0x42, /* Å */
    [0xC6 - 0xA0] = 0x43, /* Æ */ [0xC7 - 0xA0] = 0x44, /* Ç */ [0xC8 - 0xA0] = 0x6E, /* È */
    [0xC9 - 0xA0] = 0x69, /* É */ [0xCA - 0xA0] = 0x64, /* Ê */ [0xCB - 0xA0] = 0x66, /* Ë */
    [0xCC - 0xA0] = 0x6F, /* Ì */ [0xCD - 0xA0] = 0x6A, /* Í */ [0xCF - 0xA0] = 0x67, /* Ï */
    [0xD1 - 0xA0] = 0x45, /* Ñ */ [0xD2 - 0xA0] = 0x70, /* Ò */ [0xD3 - 0xA0] = 0x6B, /* Ó */
    [0xD4 - 0xA0] = 0x65, /* Ô */ [0xD5 - 0xA0] = 0x46, /* Õ */ [0xD6 - 0xA0] = 0x47, /* Ö */
    [0xD9 - 0xA0] = 0x71, /* Ù */ [0xDA - 0xA0] = 0x6C, /* Ú */ [0xDB - 0xA0] = 0x48, /* Û */
    [0xDC - 0xA0] = 0x49, /* Ü */ [0xDF - 0xA0] = 0x4A, /* ß */ [0xE0 - 0xA0] = 0x4B, /* à */
    [0xE1 - 0xA0] = 0x5F, /* á */ [0xE2 - 0xA0] = 0x4C, /* â */ [0xE3 - 0xA0] = 0x4D, /* ã */
    [0xE4 - 0xA0] = 0x4E, /* ä */ [0xE7 - 0xA0] = 0x4F, /* ç */ [0xE8 - 0xA0] = 0x50, /* è */
    [0xE9 - 0xA0] = 0x51, /* é */ [0xEA - 0xA0] = 0x52, /* ê */ [0xEB - 0xA0] = 0x53, /* ë */
    [0xEC - 0xA0] = 0x54, /* ì */ [0xED - 0xA0] = 0x60, /* í */ [0xEE - 0xA0] = 0x55, /* î */
    [0xEF - 0xA0] = 0x56, /* ï */ [0xF1 - 0xA0] = 0x57, /* ñ */ [0xF2 - 0xA0] = 0x58, /* ò */
    [0xF3 - 0xA0] = 0x61, /* ó */ [0xF4 - 0xA0] = 0x59, /* ô */ [0xF5 - 0xA0] = 0x5A, /* õ */
    [0xF6 - 0xA0] = 0x5B, /* ö */ [0xF9 - 0xA0] = 0x5C, /* ù */ [0xFA - 0xA0] = 0x62, /* ú */
    [0xFB - 0xA0] = 0x5D, /* û */ [0xFC - 0xA0] = 0x5E, /* ü */
};

/* UTF-8 `s` as the game's glyph codes into buf (NUL-ended); returns the length. */
static int bp_encode(unsigned char *buf, int size, const char *s) {
    int p = 0;

    while (*s != 0 && p < size - 3) {
        unsigned char c = (unsigned char)*s++;
        int ext = 0x01;

        if ((c == 0xC2 || c == 0xC3) && *s != 0) { /* U+00A0..U+00FF */
            int cp = ((c & 0x1F) << 6) | ((unsigned char)*s++ & 0x3F);
            int lat = cp >= 0xA0 ? bp_latin[cp - 0xA0] : 0;
            if (lat != 0) {
                buf[p++] = 1;
                buf[p++] = (unsigned char)lat;
            }
            continue;
        }
        if (c >= '0' && c <= '9') {
            buf[p++] = (unsigned char)(4 + c - '0');
            continue;
        }
        if (c >= 'A' && c <= 'Z') {
            buf[p++] = (unsigned char)(0x0E + c - 'A');
            continue;
        }
        if (c >= 'a' && c <= 'z') {
            buf[p++] = (unsigned char)(0x28 + c - 'a');
            continue;
        }
        if (c == '!') {
            buf[p++] = 0xE7;
            continue;
        }
        switch (c) {
        case ' ': ext = 0x01; break;
        case ',': ext = 0x04; break;
        case '.': ext = 0x05; break;
        case ':': ext = 0x07; break;
        case '?': ext = 0x09; break;
        case '/': ext = 0x0F; break;
        case '\'': ext = 0x13; break;
        case '(': ext = 0x15; break;
        case ')': ext = 0x16; break;
        case '+': ext = 0x19; break;
        case '-': ext = 0x1A; break;
        case '%': ext = 0x2A; break;
        case '&': ext = 0x2B; break;
        }
        buf[p++] = 1;
        buf[p++] = (unsigned char)ext;
    }
    buf[p] = 0;
    return p;
}

static void bp_quad(GfxLayer *layer, s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
    POLY_F4 *poly = gfx_module.funcs.get_packet();
    u32 *ot = layer->get_ot_entry(layer, 0);

    setPolyF4(poly);
    setRGB0(poly, r, g, b);
    setXY4(poly, (s16)x, (s16)y, (s16)(x + w), (s16)y, (s16)x, (s16)(y + h), (s16)(x + w), (s16)(y + h));
    if (abr >= 0) {
        DR_TPAGE *tp = (DR_TPAGE *)(poly + 1);
        setSemiTrans(poly, 1);
        addPrim(ot, poly);
        SetDrawTPage(tp, 0, 1, getTPage(1, abr, 0, 0));
        addPrim(ot, tp);
        gfx_module.funcs.set_packet(tp + 1);
    } else {
        addPrim(ot, poly);
        gfx_module.funcs.set_packet(poly + 1);
    }
}

static void bp_close_windows(void) {
    int i;

    if (bp.w_spk != NULL) {
        heap_objects.stop((Object *)bp.w_spk);
        bp.w_spk = NULL;
    }
    if (bp.w_line != NULL) {
        heap_objects.stop((Object *)bp.w_line);
        bp.w_line = NULL;
    }
    for (i = 0; i < 3; i++) {
        if (bp.w_opts[i] != NULL) {
            heap_objects.stop((Object *)bp.w_opts[i]);
            bp.w_opts[i] = NULL;
        }
    }
    if (bp.w_res != NULL) {
        heap_objects.stop((Object *)bp.w_res);
        bp.w_res = NULL;
    }
}

void def_battle_prompt_reset(void) {
    bp_close_windows();
    memset(&bp, 0, sizeof(bp));
    bp.active = -1;
}

static MessageWindow *bp_win(s32 x, s32 y, s32 pal, const unsigned char *txt) {
    MessageWindow *w = message_create_window(BP_LAYER, BP_FONT, x, y);

    w->set_ot_depth(w, 0);
    w->set_palette(w, pal);
    w->set_text(w, (u8 *)txt, -1);
    return w;
}

int def_battle_prompt_check(void) {
    int i, cur_e, hp_pct, bid = records_state.battle;
    FightstgMember *em;

    if (!def_on() || bp.active >= 0) {
        return 0;
    }
    cur_e = fightstg_battle.state.current[1] % 3;
    em = &fightstg_battle.state.members[1][cur_e];
    if (em->hp <= 0 || em->max_hp <= 0) {
        return 0;
    }
    hp_pct = em->hp * 100 / em->max_hp;
    for (i = 0; i < BP_PROMPTS; i++) {
        const DefBattlePrompt *p = &prompts[i];
        const DefBattleText *t = &bp_texts[bp_lang()][i];

        if ((bp.fired_mask >> i) & 1u) {
            continue;
        }
        if (p->question >= 0 && def_record_story_answer(p->question) != 0) {
            continue;
        }
        if (p->battle_id >= 0 && bid != p->battle_id) {
            continue;
        }
        if (bid == 324 && (def_finale_phase() != 0 || fightstg_battle.state.type != 6)) {
            continue;
        }
        if (p->hp_pct < 100 && hp_pct > p->hp_pct) {
            continue;
        }
        bp.active = i;
        bp.phase = 0;
        bp.sel = 0;
        bp.tick = 0;
        bp.fired_mask |= 1u << i;
        bp_encode(bp.t_spk, sizeof(bp.t_spk), t->speaker);
        bp_encode(bp.t_line, sizeof(bp.t_line), t->line);
        bp.w_spk = bp_win(24, 56, 3, bp.t_spk);
        bp.w_line = bp_win(24, 74, 0, bp.t_line);
        for (int k = 0; k < p->opt_count && k < 3; k++) {
            bp_encode(bp.t_opts[k], sizeof(bp.t_opts[k]), t->opts[k][0]);
            bp.w_opts[k] = bp_win(36, 104 + k * 18, k == 0 ? 4 : 0, bp.t_opts[k]);
        }
        port_log("battle_prompt: opened prompt %d (battle %d, Q%d)", p->id, bid, p->question);
        return 1;
    }
    return 0;
}

static int bp_apply_choice(const DefBattlePrompt *p, int sel) {
    const DefBattleOpt *o = &p->opts[sel];
    int msg = 0, boosted = 0;

    if (p->question >= 0) {
        def_record_set_story_answer(p->question, sel + 1);
    }
    if (o->bond_ally >= 0 && o->bond_delta != 0) {
        def_story_bond_add(o->bond_ally, o->bond_delta);
    }
    if (o->rep_delta != 0) {
        def_story_rep_add(o->rep_delta);
    }
    if (o->fx_type == BP_FX_MOD) {
        int slot = fightstg_battle.state.current[o->fx_side != 0] % 3;
        int blocked = 8 + o->fx_kind;
        int lvl = o->fx_level;

        /* High ally bond (>= 2) or reputation (>= 4) amplifies a positive player buff by +1 tier (up to +3) */
        if (o->fx_side == 0 && lvl > 0 && lvl < 3) {
            if ((o->bond_ally >= 0 && def_story_bond(o->bond_ally) >= 2) || def_story_rep() >= 4) {
                lvl++;
                boosted = 1;
            }
        }
        def_mod_apply((u8)o->fx_side, slot, o->fx_kind, lvl, o->fx_turns, blocked);
    } else if (o->fx_type == BP_FX_LIBRA) {
        def_tech_effect(0, 202, &msg);
    }
    port_log("battle_prompt: prompt %d picked %d (fx=%d, boosted=%d)", p->id, sel + 1, o->fx_type, boosted);
    return boosted;
}

int def_battle_prompt_frame(void) {
    GfxLayer *layer = gfx_module.funcs.get_layer(BP_LAYER);
    s32 wide = port_video_widescreen_pad();
    const DefBattlePrompt *p;
    int i;

    if (bp.active < 0) {
        return 1;
    }
    p = &prompts[bp.active];
    bp.tick++;
    if (bp.phase == 0 && bp.tick > 2) {
        if (PAD_PRESSED(4)) {
            bp.sel = (bp.sel + p->opt_count - 1) % p->opt_count;
            sound_module.play(BP_SND_MOVE);
        } else if (PAD_PRESSED(6)) {
            bp.sel = (bp.sel + 1) % p->opt_count;
            sound_module.play(BP_SND_MOVE);
        } else if (PAD_PRESSED(13)) {
            int lang = bp_lang(), n = 0;

            sound_module.play(BP_SND_OK);
            bp.res_lines = 1;
            /* A buff the bond raised: its words on a line of their own (with the result they outgrow the window) */
            if (bp_apply_choice(p, bp.sel)) {
                n = bp_encode(bp.t_res, sizeof(bp.t_res), bp_bond[lang]);
                bp.t_res[n++] = 2; /* control code 1: a new line */
                bp.t_res[n++] = 1;
                bp.res_lines = 2;
            }
            bp_encode(bp.t_res + n, (int)sizeof(bp.t_res) - n, bp_texts[lang][bp.active].opts[bp.sel][1]);
            bp_close_windows();
            bp.w_res = bp_win(22, bp.res_lines > 1 ? 103 : 110, 3, bp.t_res);
            bp.phase = 1;
            bp.tick = 0;
        }
        for (i = 0; i < p->opt_count && i < 3; i++) {
            if (bp.w_opts[i] != NULL) {
                bp.w_opts[i]->set_palette(bp.w_opts[i], i == bp.sel ? 4 : 0);
            }
        }
    } else if (bp.phase == 1) {
        if (bp.tick >= 65 || (bp.tick > 6 && (PAD_PRESSED(13) || PAD_PRESSED(14)))) {
            bp_close_windows();
            bp.active = -1;
            return 1;
        }
    }

    if (bp.w_spk != NULL) {
        heap_objects.try_run((Object *)bp.w_spk);
    }
    if (bp.w_line != NULL) {
        heap_objects.try_run((Object *)bp.w_line);
    }
    for (i = 0; i < 3; i++) {
        if (bp.w_opts[i] != NULL) {
            heap_objects.try_run((Object *)bp.w_opts[i]);
        }
    }
    if (bp.w_res != NULL) {
        heap_objects.try_run((Object *)bp.w_res);
    }

    if (layer != NULL) {
        if (bp.phase == 0) {
            s32 sy = 102 + bp.sel * 18;
            bp_quad(layer, 22, sy, 6, 14, -1, 248, 206, 86);
            bp_quad(layer, 30, sy, 266, 14, 1, 56, 48, 22);
            bp_quad(layer, 14, 50, 2, 114, -1, 248, 206, 86);
            bp_quad(layer, 16, 50, 290, 1, -1, 176, 146, 68);
            bp_quad(layer, 16, 163, 290, 1, -1, 108, 90, 44);
            bp_quad(layer, 16, 51, 290, 112, -1, 14, 18, 30);
        } else { /* the result's box, a line taller (centred on the same row) for the bond's words */
            s32 top = bp.res_lines > 1 ? 91 : 98, h = bp.res_lines > 1 ? 52 : 38;

            bp_quad(layer, 14, top, 2, h, -1, 248, 206, 86);
            bp_quad(layer, 16, top, 290, 1, -1, 176, 146, 68);
            bp_quad(layer, 16, top + h - 1, 290, 1, -1, 108, 90, 44);
            bp_quad(layer, 16, top + 1, 290, h - 2, -1, 14, 18, 30);
        }
        bp_quad(layer, -wide, 0, 320 + wide * 2, 240, 2, 110, 110, 118);
    }
    return 0;
}

void def_battle_prompt_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, bp);
}
