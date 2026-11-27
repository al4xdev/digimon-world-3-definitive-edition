/* The Blast Gauge's quest: Gennai, the Velvet Room's old
 * man, his simulator and the Chosen Evolution.
 *
 * The stages (def_record_blast_quest), each opened by his talk (the panel comes after his line, definitive_velvet.c):
 *  - DECEIVED: "Gennai" is Myotismon in the old man's shape. He shows the dark forms the Blasts have called up (the
 *    third record's entries: the first DEF_BLAST_QUEST_FORMS partner+form pairs) and tells the player to dominate them.
 *    With all ten there he asks DEF_PURIFY_BITS for them; paying plays the story: he laughs, says he is Myotismon and
 *    the partners are cursed, the screen goes black with a sound, and the real Gennai comes, who believes the player is
 *    a DigiDestined.
 *  - REVEALED: the simulator: the ten forms in waves of three (a battle's three enemy members; a wave never holds one
 *    species twice, the record of a member is looked up by its species), one wave after the other with no results
 *    screen and no field in between, so nothing heals. Each form fights with the stats it had the first time, less
 *    DEF_PURIFY_SIM_PCT, and its species' techniques. A form felled is purified for good; a lost wave sends the party
 *    back to the field (no game over, its HP and MP as before the simulator) and the rest wait for the next try.
 *  - DONE, once all ten are purified: every partner's TP the Blast took comes back (up to the game's 99), each form
 *    leaves its partner DEF_CHOSEN_BUFF_PCT of the best its strongest stat ever was (past 999, into the form's next
 *    strongest stat, and so on), and the Blast is gone: its button is the Chosen Evolution (definitive.c). Gennai's
 *    panel then picks the one partner who may take it (any time) and restores its charge.
 *
 * None of the dark forms is an enemy species of the disc (file 0x1CF has no record for them), so a wave's members get
 * records made here (def_purify_record, asked by fightstg_enemy_get_record while the simulator runs): the form's name,
 * attack and techniques (its own learnset's: the BK WarGreymon of both tables shows the mapping), the recorded stat
 * line, resistances from the form's (its lowest a weakness, its highest a strong one) and a dark Mega's battle plan. A
 * wave gives no experience and no Bits (def_purify_won_wave). Off in a replay script (def_on). */
#include <string.h>

#include "common.h"

#include "cdload.h"
#include "fieldstg.h"
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
extern void fieldstg_start_battle(s32 battle);
extern void *font_get_entry(void *data, s32 i);
extern const unsigned char def_str_PfDomTitle[], def_str_PfDominated[], def_str_PfDominate[], def_str_PfDemand[],
    def_str_PfFee[], def_str_PfHeld[], def_str_PfKeysPay[], def_str_PfNoBits[], def_str_PfSimTitle[], def_str_PfPurified[],
    def_str_PfKeysFight[], def_str_PfKeysOk[], def_str_PfKeysNext[], def_str_PfDone[], def_str_PfLost[],
    def_str_PfTpBack[], def_str_PfChosenTitle[], def_str_PfCharged[], def_str_PfChosenGuide[], def_str_PfKeysChoose[], def_str_PfLv[],
    def_str_PfChosenMark[], def_str_PfNameGennai[], def_str_PfNameMyotismon[], def_str_PfStory0[], def_str_PfStory1[],
    def_str_PfStory2[], def_str_PfStory3[], def_str_PfStory4[], def_str_PfStory5[], def_str_DriSep[], def_str_DriOf[],
    def_str_HuntSpace[], def_str_Unknown[], def_str_PfBoostOffer[], def_str_PfBoost[], def_str_PfBoostTo[],
    def_str_PfBoostFor[], def_str_PfBits[], def_str_PfPct[], def_str_PfKeysBuy[], def_str_PfKeysBuyNext[];

#define PF_LAYER 0x1002 /* the field's window layer */
#define PF_Y 56         /* the panel */
#define PF_H 166
#define PF_TEXT 160
#define PF_WINDOWS 20
#define PF_PAL_TEXT 0
#define PF_PAL_HOT 3
#define PF_PAL_DONE 3 /* palette 4 garbles the 11-pixel font */
#define PF_PAL_DIM 7
#define PF_KEYS ((1 << 12) | (1 << 13) | (1 << 14))
#define PF_SND_MOVE 0x8004513E
#define PF_SND_OK 0x8004503C
#define PF_SND_BACK 0x800450BD
#define PF_SND_CURSE 0x800429BF /* the black screen's: a field event's sound (owner-tunable) */
#define PF_FADE 90              /* frames of the black screen */
#define PF_PROGRESS_SPECIAL 0x2B /* fieldstg_start_battle sends every battle elsewhere at this progress (the DRI's) */

enum { PF_STATUS = 1, PF_DEMAND, PF_STORY, PF_SIM, PF_DEBRIEF, PF_CHOSEN };

int def_purify_debug; /* tests: 1 opens Gennai's panel on the next idle field frame, anywhere */

/* The panel: windows in the field's heap, so a save state carries it with the heap. */
static struct {
    s32 request; /* opens on the field's next idle frame with its texts loaded */
    s32 open;
    s32 mode;
    s32 page;    /* PF_STORY's page */
    s32 fade;    /* frames left of the black screen (between two pages) */
    s32 sel;     /* PF_CHOSEN's row */
    s32 fresh;   /* the frame it opened: the pad is not read */
    s32 releasing;
    s32 count;
    MessageWindow *wins[PF_WINDOWS];
    MessageWindow *fee;
} pf;

/* The simulator, from its start to the debrief. No pointers: a save state carries it. */
static struct {
    signed char pending;  /* a wave's battle was set up: the next battle to begin is it */
    signed char fighting; /* that battle runs */
    signed char next;     /* a wave was won and more wait: the field starts the next at once */
    signed char lost;     /* lost: wfightmn sends it back to the field (def_purify_defeat) */
    signed char debrief;  /* its result waits for the field */
    signed char done;     /* the last wave was won: the quest is done */
    signed char won;      /* the battle that just ended was a won wave (no results screen, no rewards) */
    signed char wave;
    signed char count;    /* forms in this wave */
    unsigned short queue; /* entries still to face (a bit each) */
    unsigned short felled; /* purified in this run */
    short tp_back;
    short map;
    short level;          /* party level when the simulator began (for wave spawns) */
    short entries[3];     /* this wave's entries */
    short party[3], hp[3], mp[3]; /* the party before the simulator (restored after a defeat) */
} pf_run;

static unsigned char t_line[PF_TEXT], t_list[2 * PF_TEXT], t_fee[PF_TEXT], t_rows[3][PF_TEXT];

static FightstgEnemyRecord pf_records[3];

static int idle(void) {
    return !fieldstg_stage.event_running && !fieldstg_stage.battle_starting && !fieldstg_stage.menu_open &&
           !fieldstg_stage.actor_busy && !fieldstg_stage.title_shown;
}

/* An entry's form: its records_digimon row (1-based), its record, its name. */
static int entry_row(int i) {
    const DefBlastEntry *e = def_record_blast_entry(i);

    return e != NULL ? records_digimon[e->partner].blast_forms[e->step] : 0;
}

static RecordsDigimon *row_record(int row) {
    return &records_digimon[row > 0 ? row - 1 : 0];
}

static const unsigned char *row_name(int row) {
    void *names = cdload_module.files.get_file(records_language + 0x4E);
    const unsigned char *s =
        names != NULL && row > 0 ? (const unsigned char *)font_get_entry(names, row_record(row)->name_id) : NULL;

    return s != NULL ? s : def_str_Unknown;
}

static int count_bits(unsigned int m) {
    int n = 0;

    for (; m != 0; m >>= 1) {
        n += m & 1;
    }
    return n;
}

static unsigned int all_entries(void) {
    return (1u << def_record_blast_count()) - 1;
}

/* ---- The forms as enemies. */

/* A form's resistance as an enemy's (absolute, 100 neutral): the form's are what it adds to its partner's. Its lowest
 * are weaknesses and its highest a strong resistance, when they differ. */
static s16 enemy_resist(const RecordsDigimon *f, int e) {
    int i, lo = 0x7FFF, hi = 0;

    for (i = 0; i < 7; i++) {
        lo = f->resists[i] < lo ? f->resists[i] : lo;
        hi = f->resists[i] > hi ? f->resists[i] : hi;
    }
    if (hi > lo && f->resists[e] == lo) {
        return DEF_PURIFY_WEAK;
    }
    if (hi > lo && f->resists[e] == hi) {
        return DEF_PURIFY_STRONG;
    }
    return (s16)(100 + f->resists[e] / 4);
}

static void purify_form_techniques(FightstgEnemyRecord *r, const RecordsDigimon *f) {
    static const FightstgEnemyAction plan[3] = { { 3, 1, 76 }, { 2, 1, 48 }, { 1, 0, 0 } };
    int i, best = 0, best_score = 0;

    r->tech = (s16)(f->techniques[0] != 0 ? f->techniques[0] : f->techniques[6]);
    r->tech_3 = (s16)f->techniques[6];
    for (i = 1; i <= 5; i++) {
        int id = f->techniques[i];
        const RecordsTechnique *t = id > 0 ? &records_techniques[id - 1] : NULL;

        if (t != NULL && t->target == 2 && t->kind == 1 && t->power * t->hit_count > best_score) {
            best = id;
            best_score = t->power * t->hit_count;
        }
    }
    r->tech_2 = (s16)(best != 0 ? best : r->tech_3);
    if (r->tech_3 != 0) {
        memcpy(r->actions, plan, sizeof(plan));
    }
}

FightstgEnemyRecord *def_purify_record(s32 id) {
    int pass, i, j, cur = fightstg_battle.state.current[1];

    if (!def_on() || !(pf_run.pending || pf_run.fighting)) {
        return NULL;
    }
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < pf_run.count; i++) {
            const DefBlastEntry *e;
            const RecordsDigimon *f;
            FightstgEnemyRecord *r = &pf_records[i];

            if (pass == 0 && i != cur) {
                continue;
            }
            if (pf_run.entries[i] < 0) {
                continue;
            }
            e = def_record_blast_entry(pf_run.entries[i]);
            f = row_record(entry_row(pf_run.entries[i]));
            if (e == NULL || f->id != id) {
                continue;
            }
            memset(r, 0, sizeof(*r));
            r->id = (s16)id;
            r->name = f->name_id;
            purify_form_techniques(r, f);
            for (j = 0; j < 5; j++) { /* the stats it had the first time, less the simulator's share */
                int v = e->stats[j] * DEF_PURIFY_SIM_PCT / 100;

                r->stats[j] = (s16)(v < 1 ? 1 : v);
            }
            for (j = 0; j < 7; j++) {
                r->resists[j] = enemy_resist(f, j);
            }
            for (j = 0; j < 5; j++) {
                int v = 50 + f->status_resists[j];

                r->resists[7 + j] = (s16)(v > 99 ? 99 : v);
            }
            r->type = f->type;
            r->unk_3E[0] = 1;
            r->counter_action = 1;
            return r;
        }
    }
    return NULL;
}

int def_purify_fighting(void) {
    return def_on() && (pf_run.pending || pf_run.fighting);
}

int def_purify_queued_enemies(void) {
    int i, j, n = 0;

    if (!def_on() || !pf_run.fighting) {
        return 0;
    }
    for (i = 0; i < DEF_BLAST_QUEST_FORMS; i++) {
        int in_slot = 0;

        if (!((pf_run.queue >> i) & 1) || entry_row(i) == 0) {
            continue;
        }
        for (j = 0; j < pf_run.count && j < 3; j++) {
            if (pf_run.entries[j] == i) {
                in_slot = 1;
                break;
            }
        }
        if (!in_slot) {
            n++;
        }
    }
    return n;
}

/* Continuous single-battle waves (matching DRI's wave flow): when all active enemy slots fall, purify them and bring
 * the next queued dark form into (current[1] + 1) % 3 without reloading the field. */
int def_purify_next_wave(void) {
    extern int def_super_hp(int hp);
    int i, k, next_idx = -1, slot, hp;
    const DefBlastEntry *be;
    FightstgMember *m;
    RecordsEnemy *e;

    if (!pf_run.fighting) {
        return -1;
    }
    for (i = 0; i < pf_run.count; i++) {
        m = &fightstg_battle.state.members[1][i];
        k = pf_run.entries[i];
        if (k >= 0 && m->digimon != 0 && m->hp <= 0) {
            def_record_blast_purify(k);
            pf_run.felled |= (unsigned short)(1u << k);
            pf_run.queue &= (unsigned short)~(1u << k);
            pf_run.entries[i] = -1;
        }
    }
    for (i = 0; i < DEF_BLAST_QUEST_FORMS; i++) {
        if (((pf_run.queue >> i) & 1) && entry_row(i) != 0) {
            next_idx = i;
            break;
        }
    }
    if (next_idx < 0) {
        return -1;
    }
    slot = (fightstg_battle.state.current[1] + 1) % 3;
    if (slot >= pf_run.count) {
        pf_run.count = (signed char)(slot + 1);
    }
    pf_run.entries[slot] = (short)next_idx;
    be = def_record_blast_entry(next_idx);
    hp = be != NULL ? be->hp * DEF_PURIFY_SIM_PCT / 100 : 100;
    hp = hp < 1 ? 1 : hp > 9999 ? 9999 : hp;
    m = &fightstg_battle.state.members[1][slot];
    e = &records_state.enemies[slot];
    memset(m, 0, sizeof(*m));
    m->digimon = row_record(entry_row(next_idx))->id;
    m->hp = m->max_hp = (s16)def_super_hp(hp);
    m->mp = m->max_mp = 9999;
    e->digimon = m->digimon;
    e->level = pf_run.level > 0 ? pf_run.level : 1;
    e->hp = (s16)hp;
    e->mp = 9999;
    e->stat_scale = 16;
    port_log("purify: entry %d (species %d, HP %d) enters in slot %d", next_idx, m->digimon, m->hp, slot);
    return slot;
}

/* ---- The waves. */

static int wave_start(void) {
    const DefDriChallenge *tpl = def_dri_challenge(def_dri_challenge_count - 1);
    int i, j, level = 1, hp;

    if (tpl == NULL || pf_run.queue == 0 || fieldstg_stage.battle_starting ||
        gamestate_data.progress == PF_PROGRESS_SPECIAL) {
        return 0;
    }
    fieldstg_start_battle(tpl->battle);
    if (!fieldstg_stage.battle_starting) {
        port_log("purify: wave not started (no field manager)");
        return 0;
    }
    for (i = 0; i < 3; i++) { /* the forms fight at the party's level */
        int id = gamestate_data.funcs.get_party_member(i);

        if (id >= 0 && gamestate_data.digimon[id].record.stats.values[0] > level) {
            level = gamestate_data.digimon[id].record.stats.values[0];
        }
    }
    records_state.stage = DEF_DRI_STAGE;
    records_state.music = DEF_DRI_MUSIC;
    pf_run.level = (short)level;
    pf_run.count = 0;
    pf_run.entries[0] = pf_run.entries[1] = pf_run.entries[2] = -1;
    for (i = 0; i < DEF_BLAST_QUEST_FORMS && pf_run.count < DEF_PURIFY_WAVE; i++) {
        int row = entry_row(i), dup = 0;

        if (!((pf_run.queue >> i) & 1) || row == 0) {
            continue;
        }
        for (j = 0; j < pf_run.count; j++) { /* one species once a wave: its record is found by its species */
            dup |= entry_row(pf_run.entries[j]) == row;
        }
        if (!dup) {
            pf_run.entries[pf_run.count++] = (short)i;
        }
    }
    for (i = 0; i < 3; i++) {
        RecordsEnemy *e = &records_state.enemies[i];

        memset(e, 0, sizeof(*e));
        if (i >= pf_run.count) {
            continue;
        }
        hp = def_record_blast_entry(pf_run.entries[i])->hp * DEF_PURIFY_SIM_PCT / 100;
        e->digimon = row_record(entry_row(pf_run.entries[i]))->id;
        e->level = (s16)level;
        e->hp = (s16)(hp < 1 ? 1 : hp > 9999 ? 9999 : hp);
        e->mp = 9999;
        e->stat_scale = 16; /* the record's stats are the line itself */
    }
    records_state.blocked[11] = 1; /* no escape */
    records_state.has_prize = 0;
    pf_run.pending = 1;
    pf_run.wave++;
    port_log("purify: wave %d set up (entries %d %d %d, level %d, HP %d %d %d)", pf_run.wave, pf_run.entries[0],
             pf_run.count > 1 ? pf_run.entries[1] : -1, pf_run.count > 2 ? pf_run.entries[2] : -1, level,
             records_state.enemies[0].hp, records_state.enemies[1].hp, records_state.enemies[2].hp);
    return 1;
}

static int sim_start(void) {
    int i, id;

    memset(&pf_run, 0, sizeof(pf_run));
    pf_run.queue = (unsigned short)(all_entries() & ~def_record_blast_purified());
    if (pf_run.queue == 0) {
        return 0;
    }
    pf_run.map = (short)gamestate_data.funcs.get_map();
    for (i = 0; i < 3; i++) {
        id = gamestate_data.funcs.get_party_member(i);
        pf_run.party[i] = (short)id;
        if (id >= 0) {
            pf_run.hp[i] = gamestate_data.digimon[id].record.stats.values[2];
            pf_run.mp[i] = gamestate_data.digimon[id].record.stats.values[4];
        }
    }
    if (!wave_start()) {
        return 0;
    }
    port_log("purify: the simulator begins: entries 0x%X", pf_run.queue);
    return 1;
}

/* The quest done: each partner's owed TP back, up to the game's cap (the rest stays owed), and each form's strength. */
static void quest_rewards(void) {
    int k, i, j, owed, room, back;

    pf_run.tp_back = 0;
    for (k = 0; k < DEF_BLAST_PARTNERS; k++) {
        GamestateRecord *r = gamestate_data.funcs.get_record(k);

        owed = def_record_blast_owed(k);
        if (owed == 0 || r == NULL) {
            continue;
        }
        room = DEF_PURIFY_TP_CAP - r->stats.values[1];
        back = owed < room ? owed : room < 0 ? 0 : room;
        r->stats.values[1] += (s16)back;
        def_record_blast_owe(k, -back);
        pf_run.tp_back += (short)back;
    }
    for (i = 0; i < def_record_blast_count(); i++) {
        const DefBlastEntry *e = def_record_blast_entry(i);
        GamestateRecord *r = gamestate_data.funcs.get_record(e->partner);
        unsigned char used[5] = { 0 };
        int left = e->best_value * DEF_CHOSEN_BUFF_PCT / 100, s = e->best;

        /* the buff goes on the form's strongest stat; what passes 999 goes on its next strongest, and so on */
        while (left > 0 && s >= 0) {
            int add = 999 - r->stats.stats[s];

            add = add < left ? (add < 0 ? 0 : add) : left;
            r->stats.stats[s] += (s16)add;
            left -= add;
            used[s] = 1;
            s = -1;
            for (j = 0; j < 5; j++) {
                if (!used[j] && (s < 0 || e->stats[j] > e->stats[s])) {
                    s = j;
                }
            }
        }
        port_log("purify: entry %d leaves digimon %d %d on stat %d", i, e->partner,
                 e->best_value * DEF_CHOSEN_BUFF_PCT / 100 - left, e->best);
    }
}

/* ---- The battle's hooks (definitive.c's def_log_begin and def_log_end, wfightmn's stage entrance and defeat). */

void def_purify_battle_begin(void) {
    pf_run.fighting = 0;
    pf_run.lost = 0;
    pf_run.won = 0;
    if (!pf_run.pending) {
        return;
    }
    pf_run.pending = 0;
    if (pf_run.count > 0 && records_state.enemies[0].digimon == row_record(entry_row(pf_run.entries[0]))->id) {
        pf_run.fighting = 1;
        port_log("purify: wave %d: the battle begins", pf_run.wave);
    } else {
        pf_run.queue = 0; /* another battle began meanwhile: the simulator is over */
    }
}

int def_purify_stage_enters(int member) {
    if (!pf_run.fighting) {
        return 0;
    }
    def_callout_show(DEF_CALLOUT_EVOLVE, 1);
    port_log("purify: wave %d: member %d enters", pf_run.wave, member);
    return 1;
}

void def_purify_battle_end(int result) {
    int i, k;

    if (!pf_run.fighting) {
        return;
    }
    pf_run.fighting = 0;
    for (i = 0; i < pf_run.count; i++) {
        FightstgMember *m = &fightstg_battle.state.members[1][i];

        k = pf_run.entries[i];
        if (k >= 0 && m->digimon != 0 && m->hp <= 0) {
            def_record_blast_purify(k);
            pf_run.felled |= (unsigned short)(1u << k);
            pf_run.queue &= (unsigned short)~(1u << k);
        }
    }
    if (result == 1) {
        pf_run.won = 1;
        if (pf_run.queue != 0) {
            pf_run.next = 1;
        } else if ((def_record_blast_purified() & all_entries()) == all_entries()) {
            pf_run.done = 1;
            quest_rewards();
            def_record_blast_set_quest(DEF_BLAST_QUEST_DONE);
            pf_run.debrief = 1;
        } else {
            pf_run.debrief = 1;
        }
    } else {
        pf_run.queue = 0;
        pf_run.lost = result == 2;
        pf_run.debrief = 1;
    }
    port_log("purify: wave %d ended: result %d, purified 0x%X (this run 0x%X), %s, TP back %d", pf_run.wave, result,
             def_record_blast_purified(), pf_run.felled, pf_run.next ? "next wave" : pf_run.done ? "quest done" : "the end",
             pf_run.tp_back);
}

int def_purify_won_wave(void) {
    return def_on() && pf_run.won;
}

int def_purify_defeat(void) {
    int i, id;

    if (pf_run.fighting) {
        def_purify_battle_end(2);
    }
    if (!pf_run.lost) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        id = pf_run.party[i];
        if (id >= 0 && id == gamestate_data.funcs.get_party_member(i)) {
            gamestate_data.digimon[id].record.stats.values[2] = pf_run.hp[i];
            gamestate_data.digimon[id].record.stats.values[4] = pf_run.mp[i];
        }
    }
    return 1;
}

/* ---- The panel. */

static MessageWindow *pf_window(s32 font, s32 x, s32 y) {
    MessageWindow *w = message_create_window(PF_LAYER, font, x, y);

    w->set_ot_depth(w, 1);
    if (pf.count < PF_WINDOWS) {
        pf.wins[pf.count++] = w;
    }
    return w;
}

static MessageWindow *pf_text(s32 font, s32 y, const unsigned char *text, s32 palette, s32 lines) {
    MessageWindow *w = pf_window(font, 18, PF_Y + y);

    if (lines > 1) {
        w->set_page_lines(w, lines);
    }
    w->set_text(w, (u8 *)text, -1);
    w->set_palette(w, palette);
    return w;
}

static void pf_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(PF_LAYER);
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

static void pf_draw(void) {
    pf_rect(10, PF_Y + 2, 300, 16, 40, 0, 50); /* the title band: the Blast's dark purple */
    if (pf.mode == PF_CHOSEN) {
        pf_rect(10, PF_Y + 54 + pf.sel * 18, 300, 16, 60, 20, 90); /* the row under the cursor */
    }
    pf_rect(9, PF_Y + 1, 302, PF_H - 2, 12, 0, 20);
    pf_rect(8, PF_Y, 304, PF_H, 150, 90, 200);
}

/* "Devimon - SkullGreymon - ...": the forms of the entries in `mask`, three a line. */
static void put_forms(unsigned char *buf, int *p, int size, unsigned int mask) {
    static const unsigned char line_break[] = { 2, 1, 0 };
    int i, n = 0;

    for (i = 0; i < DEF_BLAST_QUEST_FORMS; i++) {
        if (!((mask >> i) & 1)) {
            continue;
        }
        if (n > 0) {
            def_glyph_put(buf, p, size, n % 3 == 0 ? line_break : def_str_DriSep);
        }
        def_glyph_put(buf, p, size, row_name(entry_row(i)));
        n++;
    }
}

/* "<label> n of 10" */
static void put_count(unsigned char *buf, const unsigned char *label, int n) {
    int p = 0;

    buf[0] = 0;
    def_glyph_put(buf, &p, PF_TEXT, label);
    def_glyph_count(buf, &p, PF_TEXT, n);
    def_glyph_put(buf, &p, PF_TEXT, def_str_DriOf);
    def_glyph_count(buf, &p, PF_TEXT, DEF_BLAST_QUEST_FORMS);
}

static void list_of(unsigned int mask) {
    int p = 0;

    t_list[0] = 0;
    put_forms(t_list, &p, sizeof(t_list), mask);
}

/* The Blast's boost on sale: a step for each form recorded, the next one's price. */
static int boost_offer(void) {
    return def_record_blast_quest() == DEF_BLAST_QUEST_DECEIVED && def_record_blast_boost() < def_record_blast_count();
}

static int boost_price(void) {
    return (def_record_blast_boost() + 1) * DEF_BLAST_BOOST_BITS;
}

/* "Boost +6% to +9% for 3000 Bits", or "Boost +9%" with nothing on sale. */
static void put_boost(unsigned char *buf) {
    int p = 0, now = def_record_blast_boost() * DEF_BLAST_BOOST_STEP;

    buf[0] = 0;
    def_glyph_put(buf, &p, PF_TEXT, def_str_PfBoost);
    def_glyph_count(buf, &p, PF_TEXT, now);
    if (!boost_offer()) {
        def_glyph_put(buf, &p, PF_TEXT, def_str_PfPct);
        return;
    }
    def_glyph_put(buf, &p, PF_TEXT, def_str_PfBoostTo);
    def_glyph_count(buf, &p, PF_TEXT, now + DEF_BLAST_BOOST_STEP);
    def_glyph_put(buf, &p, PF_TEXT, def_str_PfBoostFor);
    def_glyph_number(buf, &p, PF_TEXT, boost_price());
    def_glyph_put(buf, &p, PF_TEXT, def_str_PfBits);
}

static void status_open(void) {
    int offer = boost_offer();

    pf_text(2, 3, def_str_PfDomTitle, PF_PAL_HOT, 1);
    put_count(t_line, def_str_PfDominated, def_record_blast_count());
    pf_text(1, 22, t_line, PF_PAL_TEXT, 1);
    list_of(all_entries());
    pf_text(2, 38, t_list, PF_PAL_TEXT, 4);
    pf_text(2, 92, offer ? def_str_PfBoostOffer : def_str_PfDominate, PF_PAL_DIM, 2);
    if (offer || def_record_blast_boost() > 0) {
        put_boost(t_fee);
        pf.fee = pf_text(1, 126, t_fee, offer && gamestate_data.money < boost_price() ? PF_PAL_HOT : PF_PAL_TEXT, 1);
    }
    pf_text(1, 148,
            !offer                                             ? def_str_PfKeysOk
            : def_record_blast_count() >= DEF_BLAST_QUEST_FORMS ? def_str_PfKeysBuyNext
                                                                : def_str_PfKeysBuy,
            PF_PAL_TEXT, 1);
}

static void demand_open(void) {
    int p = 0;

    pf_text(2, 3, def_str_PfDomTitle, PF_PAL_HOT, 1);
    list_of(all_entries());
    pf_text(2, 22, t_list, PF_PAL_TEXT, 4);
    pf_text(2, 86, def_str_PfDemand, PF_PAL_TEXT, 2);
    t_fee[0] = 0;
    def_glyph_put(t_fee, &p, PF_TEXT, def_str_PfFee);
    def_glyph_number(t_fee, &p, PF_TEXT, DEF_PURIFY_BITS);
    def_glyph_put(t_fee, &p, PF_TEXT, def_str_PfHeld);
    def_glyph_number(t_fee, &p, PF_TEXT, gamestate_data.money);
    pf.fee = pf_text(1, 128, t_fee, gamestate_data.money < DEF_PURIFY_BITS ? PF_PAL_HOT : PF_PAL_TEXT, 1);
    pf_text(1, 148, def_str_PfKeysPay, PF_PAL_TEXT, 1);
}

/* The story: pages 0-1 Myotismon (the black screen after page 1), 2-3 the real Gennai; 4-5 after the simulator. */
static void story_open(void) {
    static const unsigned char *const pages[] = { def_str_PfStory0, def_str_PfStory1, def_str_PfStory2,
                                                  def_str_PfStory3, def_str_PfStory4, def_str_PfStory5 };
    const unsigned char *name = pf.page == 0 ? def_str_PfNameGennai : pf.page == 1 ? def_str_PfNameMyotismon
                                                                                   : def_str_PfNameGennai;

    pf_text(2, 3, name, pf.page == 1 ? PF_PAL_HOT : PF_PAL_DONE, 1);
    pf_text(2, 26, pages[pf.page], PF_PAL_TEXT, 6);
    if (pf.page == 4) { /* the simulator's end: what came back */
        int p = 0;

        t_line[0] = 0;
        def_glyph_put(t_line, &p, PF_TEXT, def_str_PfTpBack);
        def_glyph_count(t_line, &p, PF_TEXT, pf_run.tp_back);
        pf_text(1, 128, t_line, PF_PAL_DONE, 1);
    }
    pf_text(1, 148, def_str_PfKeysNext, PF_PAL_TEXT, 1);
}

static void sim_open(void) {
    unsigned int left = all_entries() & ~def_record_blast_purified();

    pf_text(2, 3, def_str_PfSimTitle, PF_PAL_DONE, 1);
    put_count(t_line, def_str_PfPurified, count_bits(def_record_blast_purified() & all_entries()));
    pf_text(1, 22, t_line, PF_PAL_TEXT, 1);
    list_of(left);
    pf_text(2, 38, t_list, PF_PAL_TEXT, 4);
    pf_text(1, 148, def_str_PfKeysFight, PF_PAL_TEXT, 1);
}

static void debrief_open(void) {
    int lost = pf_run.lost;

    pf_run.debrief = 0;
    pf_run.lost = 0; /* back on the field: the defeat's redirect is done */
    pf_text(2, 3, def_str_PfSimTitle, PF_PAL_DONE, 1);
    pf_text(2, 22, lost ? def_str_PfLost : def_str_PfDone, lost ? PF_PAL_HOT : PF_PAL_TEXT, 2);
    put_count(t_line, def_str_PfPurified, count_bits(def_record_blast_purified() & all_entries()));
    pf_text(1, 56, t_line, PF_PAL_TEXT, 1);
    list_of(pf_run.felled);
    pf_text(2, 72, t_list, PF_PAL_DONE, 4);
    pf_text(1, 148, def_str_PfKeysOk, PF_PAL_TEXT, 1);
}

/* Gennai's Chosen panel: the party's partners, the one chosen marked, the ones under the level dim. */
static void chosen_open(void) {
    static const unsigned char megas[DEF_BLAST_PARTNERS] = DEF_CHOSEN_MEGAS;
    int i;

    def_record_set_chosen_charge(1); /* talking to him restores the charge */
    if (def_record_chosen() >= 0) {
        def_record_set_heat(def_record_chosen(), 0); /* ...and cools the Chosen Digimon's overheat */
    }
    pf_text(2, 3, def_str_PfChosenTitle, PF_PAL_DONE, 1);
    pf_text(2, 22, def_str_PfCharged, PF_PAL_TEXT, 2);
    for (i = 0; i < 3; i++) {
        int id = gamestate_data.funcs.get_party_member(i), p = 0, level;
        unsigned char *t = t_rows[i];

        t[0] = 0;
        if (id < 0) {
            continue;
        }
        level = gamestate_data.digimon[id].record.stats.values[0];
        def_glyph_put(t, &p, PF_TEXT, row_name(id + 1));
        def_glyph_put(t, &p, PF_TEXT, def_str_PfLv);
        def_glyph_count(t, &p, PF_TEXT, level);
        def_glyph_put(t, &p, PF_TEXT, def_str_DriSep);
        def_glyph_put(t, &p, PF_TEXT, row_name(megas[id]));
        if (id == def_record_chosen()) {
            def_glyph_put(t, &p, PF_TEXT, def_str_PfChosenMark);
        }
        pf_text(2, 54 + i * 18, t,
                id == def_record_chosen() ? PF_PAL_DONE : level >= DEF_CHOSEN_LEVEL ? PF_PAL_TEXT : PF_PAL_DIM, 1);
    }
    pf_text(2, 114, def_str_PfChosenGuide, PF_PAL_TEXT, 2); /* the rule: from the Rookie form, its Mega past 999 */
    pf_text(1, 148, def_str_PfKeysChoose, PF_PAL_TEXT, 1);
}

static void pf_close(void) {
    s32 i;

    for (i = 0; i < pf.count; i++) {
        heap_objects.stop((Object *)pf.wins[i]);
        pf.wins[i] = NULL;
    }
    pf.count = 0;
    pf.fee = NULL;
    pf.open = 0;
    pf.request = 0;
}

static void pf_reopen(int mode) {
    pf_close();
    pf.mode = mode;
    pf.request = 1;
}

static int texts_loaded(void) {
    return cdload_module.is_loading(records_language + 0x4E) == 0;
}

static int pf_release(void) {
    FieldstgActor *player;

    if (pad_state.get_held(0) & PF_KEYS) {
        fieldstg_stage.event_running = 1;
        return 1;
    }
    pf.releasing = 0;
    fieldstg_stage.event_running = 0;
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player != NULL) {
        player->reset_control(player);
    }
    return 1;
}

static int stage_mode(void) {
    switch (def_record_blast_quest()) {
    case DEF_BLAST_QUEST_DONE:
        return PF_CHOSEN;
    case DEF_BLAST_QUEST_REVEALED:
        return PF_SIM;
    default: /* with the ten, the boost still on sale comes first (△ goes on to the demand) */
        return def_record_blast_count() >= DEF_BLAST_QUEST_FORMS && !boost_offer() ? PF_DEMAND : PF_STATUS;
    }
}

/* Gennai's line is over (definitive_velvet.c): his panel opens, the one his quest's stage has. */
void def_purify_offer(void) {
    if (!def_on() || pf.open || pf.request || def_purify_fighting() || pf_run.next) {
        return;
    }
    pf.request = 1;
    pf.mode = stage_mode();
    pf.sel = 0;
    fieldstg_stage.event_running = 1;
    port_log("purify: Gennai's panel %d: quest %d, %d forms, purified 0x%X", pf.mode, def_record_blast_quest(),
             def_record_blast_count(), def_record_blast_purified());
}

/* The panel's buttons: 1 when the panel closes. */
static int pf_input(void) {
    int ok = PAD_PRESSED(13), back = PAD_PRESSED(12) || PAD_PRESSED(14);

    switch (pf.mode) {
    case PF_STATUS:
        if (back || (ok && !boost_offer())) {
            sound_module.play(back ? PF_SND_BACK : PF_SND_OK);
            if (def_record_blast_count() >= DEF_BLAST_QUEST_FORMS) { /* the ten are there: on to his demand */
                pf_reopen(PF_DEMAND);
                return 0;
            }
            return 1;
        }
        if (ok && gamestate_data.money < boost_price()) {
            sound_module.play(PF_SND_BACK);
            if (pf.fee != NULL) {
                pf.fee->set_text(pf.fee, (u8 *)def_str_PfNoBits, -1);
                pf.fee->set_palette(pf.fee, PF_PAL_HOT);
            }
        } else if (ok) { /* "Gennai" sells a step of the Blast's boost */
            sound_module.play(PF_SND_OK);
            gamestate_data.money -= boost_price();
            def_record_set_blast_boost(def_record_blast_boost() + 1);
            port_log("purify: \"Gennai\" sells a Blast boost step: now +%d%%, %d Bits left", def_blast_boost_pct(),
                     (int)gamestate_data.money);
            pf_reopen(PF_STATUS);
        }
        return 0;
    case PF_DEMAND:
        if (back) {
            sound_module.play(PF_SND_BACK);
            return 1;
        }
        if (ok && gamestate_data.money < DEF_PURIFY_BITS) {
            sound_module.play(PF_SND_BACK);
            if (pf.fee != NULL) {
                pf.fee->set_text(pf.fee, (u8 *)def_str_PfNoBits, -1);
                pf.fee->set_palette(pf.fee, PF_PAL_HOT);
            }
        } else if (ok) { /* "Gennai" takes the Bits, and the story begins */
            sound_module.play(PF_SND_OK);
            gamestate_data.money -= DEF_PURIFY_BITS;
            port_log("purify: \"Gennai\" takes %d Bits", DEF_PURIFY_BITS);
            pf.page = 0;
            pf_reopen(PF_STORY);
        }
        return 0;
    case PF_STORY:
        if (ok) {
            sound_module.play(PF_SND_OK);
            if (pf.page == 1) { /* Myotismon's last word: the black screen, then the real Gennai */
                def_record_blast_set_quest(DEF_BLAST_QUEST_REVEALED);
                sound_module.play(PF_SND_CURSE);
                pf.fade = PF_FADE;
                pf.page = 2;
                pf_close();
                pf.mode = PF_STORY;
                port_log("purify: the reveal: the quest is at stage %d", def_record_blast_quest());
                return 0;
            }
            if (pf.page == 3) {
                pf_reopen(PF_SIM);
                return 0;
            }
            if (pf.page == 5) {
                pf.sel = 0;
                pf_reopen(PF_CHOSEN);
                return 0;
            }
            pf.page++;
            pf_reopen(PF_STORY);
        }
        return 0;
    case PF_SIM:
        if (back) {
            sound_module.play(PF_SND_BACK);
            return 1;
        }
        if (ok) {
            sound_module.play(PF_SND_OK);
            pf_close();
            if (!sim_start()) {
                pf.releasing = 1;
            }
            return -1; /* the field stays held until the battle */
        }
        return 0;
    case PF_DEBRIEF:
        if (ok || back) {
            sound_module.play(PF_SND_OK);
            if (pf_run.done) { /* the quest done: Gennai's word on it, then the Chosen panel */
                pf.page = 4;
                pf_reopen(PF_STORY);
                return 0;
            }
            return 1;
        }
        return 0;
    case PF_CHOSEN: {
        int n = 0, id;

        while (n < 3 && gamestate_data.funcs.get_party_member(n) >= 0) {
            n++;
        }
        if (back) {
            sound_module.play(PF_SND_BACK);
            return 1;
        }
        if ((PAD_PRESSED(4) || PAD_PRESSED(6)) && n > 1) {
            sound_module.play(PF_SND_MOVE);
            pf.sel = (pf.sel + (PAD_PRESSED(6) ? 1 : n - 1)) % n;
        } else if (ok) {
            id = gamestate_data.funcs.get_party_member(pf.sel);
            if (id >= 0 && gamestate_data.digimon[id].record.stats.values[0] >= DEF_CHOSEN_LEVEL) {
                sound_module.play(PF_SND_OK);
                def_record_set_chosen(id);
                port_log("purify: Gennai gives the Chosen Evolution to digimon %d", id);
                pf_reopen(PF_CHOSEN);
            } else {
                sound_module.play(PF_SND_BACK);
            }
        }
        return 0;
    }
    default:
        if (ok || back) {
            sound_module.play(PF_SND_OK);
            return 1;
        }
        return 0;
    }
}

int def_purify_frame(void) {
    int r;

    if (!def_on()) {
        return 0;
    }
    if (pf.releasing) {
        return pf_release();
    }
    if (pf_run.next) { /* a wave won, more waiting: the next one now, the field never handed back */
        fieldstg_stage.event_running = 1;
        if (fieldstg_stage.battle_starting) {
            return 1;
        }
        if (pf_run.map != gamestate_data.funcs.get_map()) { /* a load, a reset: the simulator is gone */
            pf_run.next = 0;
            pf_run.queue = 0;
            fieldstg_stage.event_running = 0;
            return 0;
        }
        if (!fieldstg_stage.title_shown && !fieldstg_stage.menu_open) {
            pf_run.next = 0;
            if (!wave_start()) {
                pf_run.queue = 0;
                pf_run.debrief = 1;
                fieldstg_stage.event_running = 0;
            }
        }
        return 1;
    }
    if (def_purify_debug && idle()) {
        def_purify_debug = 0;
        def_purify_offer();
    }
    if (pf.fade > 0) { /* the black screen between Myotismon and the real Gennai */
        fieldstg_stage.event_running = 1;
        pf_rect(0, 0, 320, 240, 0, 0, 0);
        if (--pf.fade == 0) {
            pf.request = 1;
        }
        return 1;
    }
    if (!pf.open && !pf.request) {
        if (!pf_run.debrief || !idle()) {
            return 0;
        }
        if (pf_run.map != gamestate_data.funcs.get_map()) {
            pf_run.debrief = 0;
            return 0;
        }
        pf.request = 1;
        pf.mode = PF_DEBRIEF;
    }
    fieldstg_stage.event_running = 1;
    if (!pf.open) {
        if (!texts_loaded()) {
            return 1;
        }
        pf.count = 0;
        pf.fee = NULL;
        switch (pf.mode) {
        case PF_DEMAND:
            demand_open();
            break;
        case PF_STORY:
            story_open();
            break;
        case PF_SIM:
            sim_open();
            break;
        case PF_DEBRIEF:
            debrief_open();
            break;
        case PF_CHOSEN:
            chosen_open();
            break;
        default:
            status_open();
            break;
        }
        pf.open = 1;
        pf.request = 0;
        pf.fresh = 1;
        port_log("purify: panel %d open (page %d)", pf.mode, pf.page);
    }
    r = pf.fresh ? 0 : pf_input();
    pf.fresh = 0;
    if (r < 0 || pf.fade > 0) {
        return 1;
    }
    if (r > 0) {
        pf_close();
        pf.releasing = 1;
        return 1;
    }
    if (!pf.open) {
        return 1; /* reopening on the next frame */
    }
    {
        s32 i;

        for (i = 0; i < pf.count; i++) {
            heap_objects.try_run((Object *)pf.wins[i]);
        }
    }
    pf_draw();
    return 1;
}

void def_purify_reset(void) {
    memset(&pf_run, 0, sizeof(pf_run));
    memset(&pf, 0, sizeof(pf));
}

void def_purify_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, pf_run);
    PORT_STATE_VAR(s, pf);
    PORT_STATE_VAR(s, t_line);
    PORT_STATE_VAR(s, t_list);
    PORT_STATE_VAR(s, t_fee);
    PORT_STATE_VAR(s, t_rows);
}
