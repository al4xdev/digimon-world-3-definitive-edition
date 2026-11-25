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
#include "records.h"
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
    int question;  /* >= 0: stored in expansion save; fires at most once per save */
    int battle_id; /* exact records_state.battle ID from wstag*.c */
    int hp_pct;    /* 100 = turn 1, 50 = enemy <= 50% HP */
    const char *speaker_pt;
    const char *line_pt;
    int opt_count;
    DefBattleOpt opts[3];
} DefBattlePrompt;

static const DefBattlePrompt prompts[] = {
    /* Fase 1: Tamer Genji (wstag200, battle 188) */
    {
        0, 220, 188, 100,
        "TAMER GENJI",
        "Ho ho! Mostra se tem fibra de Tamer!",
        3,
        {
            {
                "Pode mandar ver, coroa! (ATK +1)",
                "Ânimo lá em cima! Ataque subiu (+1)!",
                BP_FX_MOD, 0, 0, 1, 3, -1, 0, 1
            },
            {
                "Vou ler o seu jogo primeiro! (Libra)",
                "Olho clínico! Libra revelou fraquezas!",
                BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_TEDDY, 1, 1
            },
            {
                "Calma, nem li o manual ainda...",
                "Vacilou na entrada! Defesa caiu (-1)!",
                BP_FX_MOD, 0, 1, -1, 2, -1, 0, -1
            },
        },
    },
    /* Fase 1/2: Keith em Asuka (wstag200, battle 200) */
    {
        1, 221, 200, 100,
        "KEITH",
        "Venceu Seiryu na sorte e já tá se achando?!",
        3,
        {
            {
                "Cai dentro e descobre! (ATK +2)",
                "Sangue ferveu! Ataque subiu (+2)!",
                BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_KEITH, 1, 1
            },
            {
                "Vou achar tua brecha! (Libra)",
                "Libra ativado! Fraquezas reveladas!",
                BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_TEDDY, 1, 1
            },
            {
                "Fecha a guarda e espera! (DEF +2)",
                "Postura firme! Defesa subiu (+2)!",
                BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_LISA, 1, 0
            },
        },
    },
    /* Fase 2: MasterTyrannomon (wstag405, battle 2) */
    {
        2, 222, 2, 50,
        "MASTERTYRANNO",
        "Bate bem, mas ainda não digeri o almoço!",
        3,
        {
            {
                "Aguenta essa então! (DEF alvo -1)",
                "Quebrou o ritmo dele! Defesa caiu (-1)!",
                BP_FX_MOD, 1, 1, -1, 3, -1, 0, 2
            },
            {
                "Mantém o compasso, sem pressa! (SPD +1)",
                "Ritmo certeiro! Velocidade subiu (+1)!",
                BP_FX_MOD, 0, 2, 1, 3, DEF_BOND_TEDDY, 1, 0
            },
            {
                "Ih, esse dinossauro é grande demais...",
                "Amarelou na hora H! Ataque caiu (-1)!",
                BP_FX_MOD, 0, 0, -1, 2, DEF_BOND_IVY, -1, -1
            },
        },
    },
    /* Fase 2: Líder de Seiryu (wstag420, battle 3) */
    {
        3, 223, 3, 50,
        "LÍDER DE SEIRYU",
        "Belo combate! Aguenta o vento de Seiryu?",
        3,
        {
            {
                "Não arredo o pé daqui! (DEF +2)",
                "Firme como rocha! Defesa subiu (+2)!",
                BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_LISA, 1, 2
            },
            {
                "Já saquei o teu ritmo! (SPD +1)",
                "Leitura afiada! Velocidade subiu (+1)!",
                BP_FX_MOD, 0, 2, 1, 3, DEF_BOND_TEDDY, 1, 1
            },
            {
                "Esse ventinho não assusta ninguém!",
                "Provocou demais! Ataque dele subiu (+1)!",
                BP_FX_MOD, 1, 0, 1, 2, DEF_BOND_KEITH, 1, -1
            },
        },
    },
    /* Fase 3: Líder de Suzaku (wstag520, battle 4) */
    {
        4, 224, 4, 50,
        "LÍDER DE SUZAKU",
        "Não me subestime! Minhas chamas não perdoam!",
        3,
        {
            {
                "Então aumenta essas chamas! (ATK +2)",
                "Fogo no ringue! Ataque subiu (+2)!",
                BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_KEITH, 1, 2
            },
            {
                "Hora de ler seu jogo! (Libra)",
                "Libra revelou as fraquezas da Líder!",
                BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_LISA, 1, 1
            },
            {
                "Calma, pega leve aí...",
                "Bobeou na arena! Defesa caiu (-1)!",
                BP_FX_MOD, 0, 1, -1, 2, DEF_BOND_IVY, -1, -1
            },
        },
    },
    /* Fase 4: Duelo em Byakko (wstag675, battle 5) */
    {
        5, 225, 5, 100,
        "LÍDER DE BYAKKO",
        "Mostre sua força... mas não espere piedade!",
        3,
        {
            {
                "Não vim pra brincar de lutar! (ATK +2)",
                "Sangue frio! Ataque subiu (+2)!",
                BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_KEITH, 1, 2
            },
            {
                "Tem algo estranho nesse estilo! (Libra)",
                "Libra revelou as fraquezas do Líder!",
                BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_LISA, 1, 1
            },
            {
                "Esse sorrisinho tá suspeito demais...",
                "Hesitou na arena! Defesa caiu (-1)!",
                BP_FX_MOD, 0, 1, -1, 2, DEF_BOND_TEDDY, -1, -1
            },
        },
    },
    /* Fase 5: Game Master no Centro Admin (wstag310, battle 10) */
    {
        6, 228, 10, 50,
        "GAME MASTER",
        "Teimosia adorável... Pena que acaba aqui.",
        3,
        {
            {
                "Pelo Teddy e por Asuka! (ATK +2)",
                "Vontade inabalável! Ataque subiu (+2)!",
                BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_TEDDY, 2, 2
            },
            {
                "Segura o contra-ataque! (DEF +2)",
                "Muralha levantada! Defesa subiu (+2)!",
                BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_LISA, 1, 1
            },
            {
                "Cala a boca! Eu acabo com você!",
                "Perdeu a cabeça! Ela acelerou (+1)!",
                BP_FX_MOD, 1, 2, 1, 2, DEF_BOND_IVY, -1, -1
            },
        },
    },
    /* Fase 6: Oficial-Chefe / Presidente da MAGAMI (wstag311, battle 32) */
    {
        7, 233, 32, 50,
        "OFICIAL-CHEFE",
        "Ainda insiste?! O Destromon é invencível!",
        3,
        {
            {
                "Teu império acaba hoje! (ATK +2)",
                "Golpe certeiro! Ataque subiu (+2)!",
                BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_LISA, 2, 2
            },
            {
                "Aguenta o tranco, equipe! (DEF +2)",
                "Guarda fechada! Defesa subiu (+2)!",
                BP_FX_MOD, 0, 1, 2, 3, DEF_BOND_TEDDY, 1, 1
            },
            {
                "Destromon vai virar sucata!",
                "Provocou demais! Ataque dele subiu (+1)!",
                BP_FX_MOD, 1, 0, 1, 2, DEF_BOND_KEITH, 1, -1
            },
        },
    },
    /* Fase 6: Lord Megadeath na Gunslinger (wstag820, battle 33) */
    {
        8, 234, 33, 50,
        "LORD MEGADEATH",
        "A humanidade acabou! Curvem-se ao deus!",
        3,
        {
            {
                "Deus nenhum! Eu vou te derrubar! (ATK +2)",
                "Chama acesa! Ataque subiu (+2)!",
                BP_FX_MOD, 0, 0, 2, 3, DEF_BOND_TEDDY, 2, 2
            },
            {
                "Todo plano tem uma falha! (Libra)",
                "Libra revelou as fraquezas de Megadeath!",
                BP_FX_LIBRA, 1, 0, 0, 0, DEF_BOND_LISA, 2, 1
            },
            {
                "Ele é forte demais... não dá...",
                "Vacilou no clímax! Defesa caiu (-1)!",
                BP_FX_MOD, 0, 1, -1, 2, DEF_BOND_IVY, -1, -1
            },
        },
    },
    /* Fase 6: Galacticmon — Batalha Final (wstag820, battle 324 / 0x144) */
    {
        9, 235, 324, 50,
        "GALACTICMON",
        "Eu sou o fim de tudo! Queime no vácuo!",
        3,
        {
            {
                "Por todos os nossos mundos! (ATK +2)",
                "Força dos vínculos! Ataque subiu (+2)!",
                BP_FX_MOD, 0, 0, 2, 4, DEF_BOND_TEDDY, 2, 2
            },
            {
                "Não caio antes de vencer! (DEF +2)",
                "Escudo inabalável! Defesa subiu (+2)!",
                BP_FX_MOD, 0, 1, 2, 4, DEF_BOND_LISA, 2, 2
            },
            {
                "Vamos superar a luz! (SPD +2)",
                "Velocidade máxima! Ritmo subiu (+2)!",
                BP_FX_MOD, 0, 2, 2, 4, DEF_BOND_KEITH, 2, 2
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

static int bp_utf8_latin(unsigned char c0, unsigned char c1) {
    if (c0 != 0xC3) {
        return 0;
    }
    switch (c1) {
    case 0x80: return 0x6D; /* À */
    case 0x81: return 0x68; /* Á */
    case 0x82: return 0x40; /* Â */
    case 0x83: return 0x63; /* Ã */
    case 0x87: return 0x44; /* Ç */
    case 0x89: return 0x69; /* É */
    case 0x8A: return 0x64; /* Ê */
    case 0x8D: return 0x6A; /* Í */
    case 0x93: return 0x6B; /* Ó */
    case 0x94: return 0x65; /* Ô */
    case 0x95: return 0x46; /* Õ */
    case 0x9A: return 0x6C; /* Ú */
    case 0xA0: return 0x4B; /* à */
    case 0xA1: return 0x5F; /* á */
    case 0xA2: return 0x4C; /* â */
    case 0xA3: return 0x4D; /* ã */
    case 0xA7: return 0x4F; /* ç */
    case 0xA9: return 0x51; /* é */
    case 0xAA: return 0x52; /* ê */
    case 0xAD: return 0x60; /* í */
    case 0xB3: return 0x61; /* ó */
    case 0xB4: return 0x59; /* ô */
    case 0xB5: return 0x5A; /* õ */
    case 0xBA: return 0x62; /* ú */
    default: return 0;
    }
}

static void bp_encode(unsigned char *buf, int size, const char *s) {
    int p = 0;

    while (*s != 0 && p < size - 3) {
        unsigned char c = (unsigned char)*s++;
        int ext = 0x01;

        if (c == 0xC3 && *s != 0) {
            int lat = bp_utf8_latin(c, (unsigned char)*s++);
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
        if (p->question >= 0 && def_record_story_answer(p->question) != 0) {
            continue;
        }
        if (p->battle_id >= 0 && bid != p->battle_id) {
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
        bp_encode(bp.t_spk, sizeof(bp.t_spk), p->speaker_pt);
        bp_encode(bp.t_line, sizeof(bp.t_line), p->line_pt);
        bp.w_spk = bp_win(24, 56, 3, bp.t_spk);
        bp.w_line = bp_win(24, 74, 0, bp.t_line);
        for (int k = 0; k < p->opt_count && k < 3; k++) {
            bp_encode(bp.t_opts[k], sizeof(bp.t_opts[k]), p->opts[k].label_pt);
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
            int boosted;
            char rbuf[96];

            sound_module.play(BP_SND_OK);
            boosted = bp_apply_choice(p, bp.sel);
            bp_close_windows();
            if (boosted) {
                snprintf(rbuf, sizeof(rbuf), "Vínculo forte! %s", p->opts[bp.sel].result_pt);
                bp_encode(bp.t_res, sizeof(bp.t_res), rbuf);
            } else {
                bp_encode(bp.t_res, sizeof(bp.t_res), p->opts[bp.sel].result_pt);
            }
            bp.w_res = bp_win(22, 110, 3, bp.t_res);
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
        bp_quad(layer, -wide, 0, 320 + wide * 2, 240, 2, 110, 110, 118);
    }
    return 0;
}

void def_battle_prompt_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, bp);
}
