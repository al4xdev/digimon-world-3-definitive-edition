#include <stdio.h>
#include <string.h>

#include "common.h"
#include "fightstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "pad.h"
#include "psyq/libgpu.h"
#include "sound.h"

#include "psxstack/savestate.h"

#include "definitive.h"
#include "port_runtime.h"

extern int def_on(void);
extern int def_is_ptbr(void);
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

typedef struct DefBattleOpt {
    const char *label_pt;
    const char *result_pt;
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
    int question;    /* >= 0: stored in expansion save */
    int enemy_id;    /* active enemy species ID, or -1 for any */
    int min_prog;
    int max_prog;
    int battle_type; /* -1 any, else fightstg_battle.state.type (0 random, 1+ scripted/boss/tamer) */
    int hp_pct;      /* 100 = turn 1, 50 = enemy <= 50% HP */
    const char *speaker_pt;
    const char *line_pt;
    int opt_count;
    DefBattleOpt opts[3];
} DefBattlePrompt;

static const DefBattlePrompt prompts[] = {
    /* Phase 1: Primeiro duelo de Tamer em Asuka (Tamer de treino / Batalha de Arena) */
    {
        0, 220, -1, 0, 6, 1, 100,
        "DOMADOR DE ASUKA",
        "Ei, desafiante! Achou que Asuka era passeio?",
        3,
        {
            {
                "Pode vir quente que eu to fervendo!",
                "PROVOCACAO! Seu ATK subiu (ATK +1)!",
                BP_FX_MOD, 0, 0, 1, 3, -1, 0, 1
            },
            {
                "Deixa eu ler o jogo dele primeiro...",
                "OLHO CLINICO! Libra revelou as fraquezas!",
                BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_TEDDY, 1, 1
            },
            {
                "Uai, eu nem li o tutorial direito...",
                "VACILOU! Sua guarda caiu (DEF -1)!",
                BP_FX_MOD, 0, 1, -1, 2, -1, 0, -1
            },
        },
    },
    /* Phase 1: Primeira luta contra chefe / A.o.A no inicio da campanha (progress 0..10, tipo chefe) */
    {
        1, 221, -1, 1, 12, 2, 100,
        "PARCEIRO",
        "Esse cara e barra-pesada! Qual e a call, chefe?",
        3,
        {
            {
                "Vai pra cima sem do! (ATK +2)",
                "SANGUE NOS OLHOS! ATK +2 por 3 turnos!",
                BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_IVY, 1, 1
            },
            {
                "Analisa a brecha dele! (Libra)",
                "LIBRA! Fraquezas do chefe reveladas!",
                BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_TEDDY, 1, 1
            },
            {
                "Segura a onda, nao se expoe! (DEF +2)",
                "MURALHA! DEF +2 por 3 turnos!",
                BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_LISA, 1, 0
            },
        },
    },
    /* Phase 1/2: Pressao no meio da luta contra chefe (HP <= 50%) */
    {
        2, 222, -1, 1, 15, 2, 50,
        "CHEFE",
        "Tsc... quanta folga! Agora o bicho vai pegar!",
        3,
        {
            {
                "Cala a boca e segura essa! (DEF inimigo -1)",
                "DESESTABILIZADO! Defesa inimiga caiu (DEF -1)!",
                BP_FX_MOD, 1, 1, -1, 3, -1, 0, 2
            },
            {
                "Mantem o ritmo, sem pressa! (SPD +1)",
                "FOCO TOTAL! Velocidade aumentou (SPD +1)!",
                BP_FX_MOD, 0, 2, 1, 3, DEF_BOND_TEDDY, 1, 0
            },
            {
                "Ja era, vamos perder essa...",
                "MORAL ABALADA! Seu ATK caiu (ATK -1)!",
                BP_FX_MOD, 0, 0, -1, 2, DEF_BOND_IVY, -1, -1
            },
        },
    },
    /* Phase 2: Batalha contra Lider de Ginásio / Seiryu (progress 4..9, tipo Tamer/Lider) */
    {
        3, 223, -1, 4, 9, 1, 50,
        "LIDER DE SEIRYU",
        "Nada mal! Mas quero ver segurar o vento de Seiryu!",
        3,
        {
            {
                "Pode soprar que eu nao arredo pe! (DEF +2)",
                "FIRME COMO ROCHA! DEF +2 por 3 turnos!",
                BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_LISA, 1, 2
            },
            {
                "Entendi teu ritmo! (SPD +1)",
                "LEITURA PERFEITA! SPD +1 por 3 turnos!",
                BP_FX_MOD, 0, 2, 1, 3, DEF_BOND_TEDDY, 1, 1
            },
            {
                "Esse ventinho ai nao assusta ninguem!",
                "PROVOCOU DEMAIS! O inimigo ficou furioso (ATK +1)!",
                BP_FX_MOD, 1, 0, 1, 2, DEF_BOND_KEITH, 1, -1
            },
        },
    },
};

static struct {
    s32 active;      /* -1 none, else index in prompts[] */
    s32 phase;       /* 0 selecting, 1 showing result */
    s32 sel;
    s32 tick;
    u32 fired_mask;  /* prompts already triggered in this battle */
    MessageWindow *w_spk, *w_line, *w_opts[3], *w_res;
    unsigned char t_spk[64], t_line[128], t_opts[3][96], t_res[128];
} bp;

static void bp_ascii(unsigned char *buf, int size, const char *s) {
    int p = 0;

    for (; *s != 0 && p < size - 3; s++) {
        unsigned char c = (unsigned char)*s;
        int ext = 0x01;

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
    int i, cur_e, hp_pct, prog = gamestate_data.progress;
    FightstgMember *em;

    if (!def_on() || !def_is_ptbr() || bp.active >= 0) {
        return 0;
    }
    cur_e = fightstg_battle.state.current[1] % 3;
    em = &fightstg_battle.state.members[1][cur_e];
    if (em->hp <= 0 || em->max_hp <= 0) {
        return 0;
    }
    hp_pct = em->hp * 100 / em->max_hp;
    for (i = 0; i < (int)(sizeof(prompts) / sizeof(prompts[0])); i++) {
        const DefBattlePrompt *p = &prompts[i];

        if ((bp.fired_mask >> i) & 1u) {
            continue;
        }
        if (p->enemy_id >= 0 && em->digimon != p->enemy_id) {
            continue;
        }
        if (prog < p->min_prog || prog > p->max_prog) {
            continue;
        }
        if (p->battle_type >= 0 && fightstg_battle.state.type != p->battle_type) {
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
        bp_ascii(bp.t_spk, sizeof(bp.t_spk), p->speaker_pt);
        bp_ascii(bp.t_line, sizeof(bp.t_line), p->line_pt);
        bp.w_spk = bp_win(24, 56, 3, bp.t_spk);
        bp.w_line = bp_win(24, 74, 0, bp.t_line);
        for (int k = 0; k < p->opt_count && k < 3; k++) {
            bp_ascii(bp.t_opts[k], sizeof(bp.t_opts[k]), p->opts[k].label_pt);
            bp.w_opts[k] = bp_win(36, 104 + k * 18, k == 0 ? 4 : 0, bp.t_opts[k]);
        }
        port_log("battle_prompt: opened prompt %d (Q%d)", p->id, p->question);
        return 1;
    }
    return 0;
}

static void bp_apply_choice(const DefBattlePrompt *p, int sel) {
    const DefBattleOpt *o = &p->opts[sel];
    int msg = 0;

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

        def_mod_apply((u8)o->fx_side, slot, o->fx_kind, o->fx_level, o->fx_turns, blocked);
    } else if (o->fx_type == BP_FX_LIBRA) {
        def_tech_effect(0, 202, &msg);
    }
    port_log("battle_prompt: prompt %d picked %d (fx=%d)", p->id, sel + 1, o->fx_type);
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
            sound_module.play(BP_SND_OK);
            bp_apply_choice(p, bp.sel);
            bp_close_windows();
            bp_ascii(bp.t_res, sizeof(bp.t_res), p->opts[bp.sel].result_pt);
            bp.w_res = bp_win(24, 110, 3, bp.t_res);
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
        } else {
            bp_quad(layer, 14, 98, 2, 38, -1, 248, 206, 86);
            bp_quad(layer, 16, 98, 290, 1, -1, 176, 146, 68);
            bp_quad(layer, 16, 135, 290, 1, -1, 108, 90, 44);
            bp_quad(layer, 16, 99, 290, 36, -1, 14, 18, 30);
        }
        /* Grey/dark veil over the frozen battle HUD */
        bp_quad(layer, -wide, 0, 320 + wide * 2, 240, 2, 110, 110, 118);
    }
    return 0;
}

void def_battle_prompt_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, bp);
}
