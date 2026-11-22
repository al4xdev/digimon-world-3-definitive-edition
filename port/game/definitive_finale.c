/* Extended Final Act (Dark Galacticmon & Super Chosen Evolution) and Post-Credits Farewell (Igor -> Pipimon).
 *
 * Phase 1 (Classic): original Galacticmon (battle type 6, species 442/466/467) falls normally.
 * Phase 2 (Dark Galacticmon scripted defeat): wfightmn_knockout revives Galacticmon as Dark/Mega Galacticmon
 *   (species 467, DEF_RECOLOR_BLACK CLUT, ~2000 Limit Break stats). Defeat is scripted (no Game Over), returning
 *   briefly to the map and triggering dw3_velvet_pull_by into the Last Velvet Room.
 * Phase 3 (Last Velvet Room & Blue Room Bonds): Igor asks for the Velvet Key, reveals it has transformed, and
 *   returns it with the native item sparkle. The top door (0x20A) opens into the solid-blue Bonds Room: the screen
 *   blinks black through encouraging lines from the journey's allies, then they gather around the hero.
 * Phase 4 (Super Chosen Evolution): picks the strongest joined partner (random tie-break), enters rematch vs Dark
 *   Galacticmon in base Rookie form with Blast-only tutorial lock. Triggering Blast announces Super Chosen Evolution,
 *   chains DNA fusions across all joined Digimon (party + lab, handling even and odd counts) into Golden Imperialdramon
 *   Paladin Mode (0x17D, DEF_RECOLOR_GOLD CLUT, Omega Blade 0x66 only, high HP, calibrated boss HP), and plays
 *   Butter-Fly (bank 73) when available. Victory marks DEF_STORY_FINALE_DONE and proceeds to normal credits (0xE0B).
 *
 * Post-Credits (progress == 0x2D, !DEF_STORY_IGOR_GONE): entering the Velvet Room opens the Blue Room where Igor bids
 *   farewell (mentioning prison / limousine / elevator across time) and leaves; Pipimon (Oikawa's partner from 02)
 *   takes Igor's spot as the DNA Digivolution oracle for the World Tournament late game. */
#include <string.h>

#include "common.h"

#include "fieldstg.h"
#include "fightstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "pad.h"
#include "records.h"
#include "sound.h"

#include "psyq/libgpu.h"

#include "port_runtime.h"
#include "psxstack/savestate.h"

#include "definitive.h"

extern void port_log(const char *fmt, ...);
extern void dw3_velvet_pull_by(s32 from, s32 who);
extern void dw3_adventure_music(s32 *music, s32 *sound);
extern int dw3_adventure_music_available(void);

/* Text strings (encoded by tools/gen_text_override.py from en.txt / pt.txt). */
extern const unsigned char def_str_FinIgorKey[];
extern const unsigned char def_str_FinBond1[];
extern const unsigned char def_str_FinBond2[];
extern const unsigned char def_str_FinBond3[];
extern const unsigned char def_str_FinBond4[];
extern const unsigned char def_str_FinBond5[];
extern const unsigned char def_str_FinBondAll[];
extern const unsigned char def_str_FinSuperEvo[];
extern const unsigned char def_str_FinIgorBye[];
extern const unsigned char def_str_PipimonMeet[];
extern const unsigned char def_str_PipimonBack[];

#define FIN_GALACTIC_1 442
#define FIN_GALACTIC_2 466
#define FIN_GALACTIC_3 467
#define FIN_GALACTIC_MAP 0x2DF
#define FIN_IMPERIAL_PM 0x17D /* Imperialdramon Paladin Mode (records_digimon[49]) */
#define FIN_DARK_HP_P2 25000
#define FIN_DARK_HP_P4 18000
#define FIN_DARK_STAT_P2 2100
#define FIN_DARK_STAT_P4 1950
#define FIN_GOLD_STAT 1650
#define FIN_BLINK_FRAMES 24
#define FIN_BOND_COUNT 6 /* 5 individual allies + 1 gathered finale line */
#define FIN_CHAIN_MAX 12

/* Velvet Room TALK05 synthetic message IDs handled by def_story_text / def_finale_text. */
#define FIN_MSG_IGOR_KEY 190
#define FIN_MSG_IGOR_BYE 191

typedef struct FinaleState {
    s32 phase;            /* DEF_FINALE_* */
    s32 need_velvet_pull; /* Phase 2 defeat -> pull into Last Velvet Room on next free field frame */
    s32 key_returned;     /* Phase 3: Igor returned transformed key -> top door opens */
    s32 super_chosen_on;  /* Phase 4: Blast triggered -> Golden Imperialdramon PM active */
    s32 blue_step;        /* Phase 3 Blue Room bonds index (0..FIN_BOND_COUNT) */
    s32 blue_sub;         /* 0: blink black, 1: open dialog, 2: wait dialog */
    s32 blue_timer;
    s32 post_from_map;    /* Post-credits: map player entered Velvet Room from */
    s32 saved_party[3];   /* Phase 4: party backup restored on victory/end */
    s32 saved_party_valid;
    s32 lead_member;      /* Phase 4: chosen strongest joined Digimon (0..7) */
    /* DNA fusion chain pairs: each step fuses pair_a[k] + pair_b[k] -> pair_res[k]. */
    s32 chain_len;
    s32 chain_pos;
    s32 chain_a[FIN_CHAIN_MAX];
    s32 chain_b[FIN_CHAIN_MAX];
    s32 chain_res[FIN_CHAIN_MAX];
} FinaleState;

static FinaleState fn;
static Object *fn_dialog; /* active MessageBox in Blue Room (not persisted across savestates) */

static int is_galacticmon_species(s32 id) {
    return id == FIN_GALACTIC_1 || id == FIN_GALACTIC_2 || id == FIN_GALACTIC_3;
}

int def_finale_phase(void) {
    return def_on() ? fn.phase : DEF_FINALE_NONE;
}

int def_finale_active(void) {
    return def_on() && fn.phase != DEF_FINALE_NONE && fn.phase != DEF_FINALE_POSTCREDITS_BLUE;
}

int def_finale_enemy_dark(void) {
    return def_on() && (fn.phase == DEF_FINALE_PHASE2_DARK_LOSS || fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN);
}

int def_finale_party_gold(void) {
    return def_on() && fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN && fn.super_chosen_on;
}

int def_finale_blast_only(void) {
    return def_on() && fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN && !fn.super_chosen_on;
}

int def_finale_golden_mode(void) {
    return def_on() && fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN && fn.super_chosen_on;
}

int def_finale_velvet_active(void) {
    return def_on() && fn.phase == DEF_FINALE_PHASE3_VELVET;
}

int def_finale_top_door_open(void) {
    return def_on() && fn.phase == DEF_FINALE_PHASE3_VELVET && fn.key_returned;
}

int def_finale_blue_room_active(void) {
    return def_on() && (fn.phase == DEF_FINALE_PHASE3_BLUE_ROOM || fn.phase == DEF_FINALE_POSTCREDITS_BLUE);
}

int def_finale_pipimon_active(void) {
    return def_on() && def_record_story(DEF_STORY_IGOR_GONE);
}

/* Called at battle start (def_log_begin). Detects classic Galacticmon (Phase 1) and configures Phase 4 rematch. */
void def_finale_battle_begin(void) {
    s32 eid;

    if (!def_on()) {
        return;
    }
    eid = fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon;
    if (fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN) {
        /* Phase 4 rematch: Dark Galacticmon vs strongest partner in base Rookie form with full Blast gauge. */
        FightstgMember *em = &fightstg_battle.state.members[1][0];
        FightstgMember *pm = &fightstg_battle.state.members[0][0];
        s32 lead = fn.lead_member;

        fn.super_chosen_on = 0;
        fightstg_battle.state.type = 6;
        fightstg_battle.state.current[0] = 0;
        fightstg_battle.state.current[1] = 0;
        em->digimon = FIN_GALACTIC_3;
        em->max_hp = FIN_DARK_HP_P4;
        em->hp = FIN_DARK_HP_P4;
        fightstg_battle.state.members[1][1].digimon = 0;
        fightstg_battle.state.members[1][1].hp = 0;
        fightstg_battle.state.members[1][2].digimon = 0;
        fightstg_battle.state.members[1][2].hp = 0;
        if (lead >= 0 && lead < 8) {
            pm->digimon = records_digimon[lead].id;
            pm->base_digimon = pm->digimon;
            pm->hp = pm->max_hp;
            pm->mp = pm->max_mp;
            records_state.gauges[lead] = 999;
        }
        port_log("finale: Phase 4 rematch vs Dark Galacticmon (lead %d, Rookie form, Blast locked)", lead);
        return;
    }
    if (!def_record_story(DEF_STORY_FINALE_DONE) &&
        (fightstg_battle.state.type == 4 || fightstg_battle.state.type == 6) && is_galacticmon_species(eid)) {
        fn.phase = DEF_FINALE_PHASE1_CLASSIC;
        fn.super_chosen_on = 0;
        port_log("finale: Phase 1 classic Galacticmon battle started (species %d, type %d)", eid,
                 fightstg_battle.state.type);
    }
}

/* Phase 1 -> Phase 2 mid-battle transformation when original Galacticmon falls (called from wfightmn_knockout).
 * Returns 1 if it intercepted the KO and revived Dark Galacticmon in slot 1 for fightstg_entrance_create. */
int def_finale_on_galacticmon_ko(void) {
    FightstgMember *em;

    if (!def_on() || fn.phase != DEF_FINALE_PHASE1_CLASSIC) {
        return 0;
    }
    fn.phase = DEF_FINALE_PHASE2_DARK_LOSS;
    em = &fightstg_battle.state.members[1][1];
    memset(em, 0, sizeof(*em));
    em->digimon = FIN_GALACTIC_3;
    em->max_hp = FIN_DARK_HP_P2;
    em->hp = FIN_DARK_HP_P2;
    em->max_mp = 9999;
    em->mp = 9999;
    records_state.enemies[1] = records_state.enemies[0];
    records_state.enemies[1].digimon = FIN_GALACTIC_3;
    records_state.enemies[1].level = 99;
    port_log("finale: Phase 1 -> Phase 2! Dark Galacticmon enters (Limit Break)");
    return 1;
}

void def_finale_stage_enters(void) {
    if (def_finale_enemy_dark()) {
        def_callout_show(DEF_CALLOUT_EVOLVE, 1);
    }
}

/* Phase 2 scripted defeat: Dark Galacticmon's Limit Break hits hard enough to overwhelm even a Lv99 / 9999 HP party
 * (magic damage is otherwise clamped to 2x technique power). */
int def_finale_phase2_damage(int side, int value) {
    if (!def_on() || side == 0 || value <= 0 || fn.phase != DEF_FINALE_PHASE2_DARK_LOSS) {
        return value;
    }
    {
        FightstgMember *pm = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
        s32 floor = pm->max_hp * 55 / 100;
        s32 scaled = value * 4;

        if (scaled < floor) {
            scaled = floor;
        }
        return scaled > 9999 ? 9999 : scaled;
    }
}

/* Raises Dark Galacticmon's stats to ~2000 (Limit Break) in Phase 2 and Phase 4. */
void def_finale_enemy_stats(short stats[5]) {
    int i;
    short target;

    if (!def_finale_enemy_dark()) {
        return;
    }
    target = fn.phase == DEF_FINALE_PHASE2_DARK_LOSS ? FIN_DARK_STAT_P2 : FIN_DARK_STAT_P4;
    for (i = 0; i < 5; i++) {
        if (stats[i] < target) {
            stats[i] = target;
        }
    }
    /* In Phase 4: let the player's Rookie act first before Super Chosen Evolution triggers;
     * once Golden Imperialdramon-P is out (Speed 1650), keep boss Speed moderate for clean cinematic turns. */
    if (fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN) {
        stats[4] = fn.super_chosen_on ? 950 : 1;
    }
}

/* Called from wfightmn_battle_end case 2 (party defeated). */
int def_finale_defeat(void) {
    int i;

    if (!def_on()) {
        return 0;
    }
    if (fn.phase == DEF_FINALE_PHASE3_VELVET && fn.need_velvet_pull) {
        return 1; /* wfightmn_battle_end case 2 runs every fade frame: keep holding off 0xE00 Game Over */
    }
    if (fn.phase == DEF_FINALE_PHASE2_DARK_LOSS) {
        /* Scripted loss: heal party to 1 HP, return briefly to 0x2DF, then pull into Last Velvet Room. */
        for (i = 0; i < 8; i++) {
            if (gamestate_data.digimon[i].joined != 0) {
                GamestateStats *st = &gamestate_data.digimon[i].record.stats;
                if (st->values[2] < 1) {
                    st->values[2] = 1;
                }
            }
        }
        fn.phase = DEF_FINALE_PHASE3_VELVET;
        fn.need_velvet_pull = 1;
        fn.key_returned = 0;
        port_log("finale: Phase 2 scripted defeat -> queuing Last Velvet Room pull");
        return 1;
    }
    if (fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN) {
        /* Should be nearly impossible to lose, but never Game Over: retry Phase 4 cleanly. */
        for (i = 0; i < 8; i++) {
            if (gamestate_data.digimon[i].joined != 0) {
                GamestateStats *st = &gamestate_data.digimon[i].record.stats;
                st->values[2] = st->values[3];
                st->values[4] = st->values[5];
            }
        }
        fn.super_chosen_on = 0;
        port_log("finale: Phase 4 defeat -> retrying");
        return 1;
    }
    return 0;
}

static void restore_saved_party(void) {
    if (fn.saved_party_valid) {
        int i;
        for (i = 0; i < 3; i++) {
            gamestate_data.party[i] = fn.saved_party[i];
        }
        fn.saved_party_valid = 0;
    }
}

/* Called from wfightmn_battle_end case 1 (victory) when state.type == 6. */
int def_finale_victory(void) {
    if (!def_on()) {
        return 0;
    }
    if (fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN) {
        restore_saved_party();
        def_record_set_story(DEF_STORY_FINALE_DONE);
        fn.phase = DEF_FINALE_NONE;
        fn.super_chosen_on = 0;
        port_log("finale: Phase 4 victory! DEF_STORY_FINALE_DONE set -> proceeding to normal credits");
        return 1;
    }
    if (fn.phase == DEF_FINALE_PHASE3_VELVET && fn.need_velvet_pull) {
        return 2; /* keep returning to field on subsequent fade frames of wfightmn_battle_end */
    }
    if (fn.phase == DEF_FINALE_PHASE2_DARK_LOSS) {
        /* If player somehow beat Phase 2 Dark Galacticmon anyway, still transition into Phase 3 cleanly. */
        fn.phase = DEF_FINALE_PHASE3_VELVET;
        fn.need_velvet_pull = 1;
        fn.key_returned = 0;
        return 2; /* caller returns to field instead of credits */
    }
    return 0;
}

/* Pick the player's strongest joined Digimon (highest level, tie-break by total stats, then random). */
static s32 pick_strongest_partner(void) {
    s32 best_lv = -1, best_sum = -1;
    s32 cands[8];
    s32 ncand = 0;
    s32 i, j;

    for (i = 0; i < 8; i++) {
        GamestateStats *st;
        s32 lv, sum = 0;

        if (gamestate_data.digimon[i].joined == 0) {
            continue;
        }
        st = &gamestate_data.digimon[i].record.stats;
        lv = st->values[0];
        for (j = 0; j < 6; j++) {
            sum += st->stats[j];
        }
        if (lv > best_lv || (lv == best_lv && sum > best_sum)) {
            best_lv = lv;
            best_sum = sum;
            ncand = 0;
            cands[ncand++] = i;
        } else if (lv == best_lv && sum == best_sum) {
            cands[ncand++] = i;
        }
    }
    if (ncand <= 0) {
        return gamestate_data.party[0] >= 0 ? gamestate_data.party[0] : 0;
    }
    return cands[(unsigned long)port_frames % (unsigned long)ncand];
}

/* Build the successive-pair DNA fusion chain from all joined Digimon (party + lab), handling both even and odd counts.
 * Intermediate fusions use iconic Mega/DNA forms, culminating in Golden Imperialdramon Paladin Mode (0x17D). */
static void build_dna_chain(void) {
    static const s32 mid_forms[6] = { 0x104, 0xD5, 0x94, 0xD6, 0x96, 0x179 }; /* Paildramon, WarGreymon, Imperialdramon-F, Omnimon, Gallantmon, Beelzemon */
    s32 pool[8];
    s32 n = 0;
    s32 i, step = 0;

    fn.chain_len = 0;
    fn.chain_pos = 0;

    /* Put lead partner first, then every other joined Digimon (party + lab). */
    if (fn.lead_member >= 0 && fn.lead_member < 8 && gamestate_data.digimon[fn.lead_member].joined != 0) {
        pool[n++] = records_digimon[fn.lead_member].id;
    }
    for (i = 0; i < 8; i++) {
        if (i != fn.lead_member && gamestate_data.digimon[i].joined != 0) {
            pool[n++] = records_digimon[i].id;
        }
    }
    if (n <= 1) {
        /* Single partner edge case: direct Super Chosen Evolution into Golden Imperialdramon-P. */
        fn.chain_a[0] = n == 1 ? pool[0] : records_digimon[0].id;
        fn.chain_b[0] = fn.chain_a[0];
        fn.chain_res[0] = FIN_IMPERIAL_PM;
        fn.chain_len = 1;
        return;
    }

    /* Reduce pool in pairs until 1 remains; odd counts carry the unpaired Digimon into the next round. */
    while (n > 1 && fn.chain_len < FIN_CHAIN_MAX) {
        s32 next_pool[8];
        s32 next_n = 0;

        for (i = 0; i + 1 < n && fn.chain_len < FIN_CHAIN_MAX; i += 2) {
            s32 is_final = (n == 2);
            s32 res = is_final ? FIN_IMPERIAL_PM : mid_forms[step % 6];

            fn.chain_a[fn.chain_len] = pool[i];
            fn.chain_b[fn.chain_len] = pool[i + 1];
            fn.chain_res[fn.chain_len] = res;
            fn.chain_len++;
            next_pool[next_n++] = res;
            step++;
        }
        if (n & 1) {
            /* Odd number of Digimon: carry the last one forward to fuse in the next round. */
            next_pool[next_n++] = pool[n - 1];
        }
        for (i = 0; i < next_n; i++) {
            pool[i] = next_pool[i];
        }
        n = next_n;
    }
    /* Ensure the very last step always lands on Imperialdramon Paladin Mode. */
    if (fn.chain_len > 0) {
        fn.chain_res[fn.chain_len - 1] = FIN_IMPERIAL_PM;
    }
    port_log("finale: built DNA fusion chain with %d pair steps -> 0x%X", fn.chain_len, FIN_IMPERIAL_PM);
}

int def_finale_trigger_super_chosen(void) {
    if (!def_finale_blast_only()) {
        return 0;
    }
    build_dna_chain();
    def_callout_show(DEF_CALLOUT_EVOLVE, 0);
    return 1;
}

/* Advances the DNA chain: returns 1 with the next step's (partner, result), or 0 when finished (and activates Golden mode). */
int def_finale_dna_chain_next(int *out_partner_id, int *out_result_id) {
    if (fn.phase != DEF_FINALE_PHASE4_SUPER_CHOSEN) {
        return 0;
    }
    if (fn.chain_pos < fn.chain_len) {
        s32 idx = fn.chain_pos++;
        s32 res = fn.chain_res[idx];

        /* Enable golden recolor right before the final Imperialdramon-P model loads. */
        if (idx == fn.chain_len - 1) {
            fn.super_chosen_on = 1;
            if (dw3_adventure_music_available()) {
                s32 bank = 0, snd = 0;
                dw3_adventure_music(&bank, &snd);
                if (bank > 0 && snd != 0) {
                    sound_module.load_bank(1, bank);
                    records_state.music = snd;
                    sound_module.play(snd);
                }
            }
        }
        if (out_partner_id != NULL) {
            *out_partner_id = fn.chain_b[idx];
        }
        if (out_result_id != NULL) {
            *out_result_id = res;
        }
        return 1;
    }
    /* Chain complete: empower Golden Imperialdramon Paladin Mode for the cinematic climax. */
    {
        FightstgMember *pm = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
        pm->digimon = FIN_IMPERIAL_PM;
        pm->blasted = 1;
        pm->max_hp = 9999;
        pm->hp = 9999;
        pm->max_mp = 999;
        pm->mp = 999;
    }
    return 0;
}

/* ---- Phase 3 (Last Velvet Room & Blue Room) + Post-Credits Farewell ---- */

int def_finale_igor_line(void) {
    if (!def_on()) {
        return 0;
    }
    if (fn.phase == DEF_FINALE_PHASE3_VELVET) {
        return FIN_MSG_IGOR_KEY;
    }
    if (fn.phase == DEF_FINALE_POSTCREDITS_BLUE) {
        return FIN_MSG_IGOR_BYE;
    }
    return 0;
}

const unsigned char *def_finale_text(int file, int index) {
    /* Only while the finale's phase asks for the line, and never in the item files: 190/191 are also item IDs
     * (ITMINF 0x63, ITMNAM 0x6A), so an unscoped match put Igor's farewell into the weapon shop's list. */
    if ((file & 0xFFFF) == 0x63 || (file & 0xFFFF) == 0x6A || def_finale_igor_line() != index) {
        return NULL;
    }
    if (index == FIN_MSG_IGOR_KEY) {
        return def_str_FinIgorKey;
    }
    if (index == FIN_MSG_IGOR_BYE) {
        return def_str_FinIgorBye;
    }
    return NULL;
}

void def_finale_igor_done(void) {
    if (!def_on()) {
        return;
    }
    if (fn.phase == DEF_FINALE_PHASE3_VELVET) {
        fn.key_returned = 1;
        def_field_item_sparkle();
        port_log("finale: Igor returned the transformed key -> Velvet Room top door open");
        return;
    }
    if (fn.phase == DEF_FINALE_POSTCREDITS_BLUE) {
        def_record_set_story(DEF_STORY_IGOR_GONE);
        fn.phase = DEF_FINALE_NONE;
        port_log("finale: Igor farewell done -> DEF_STORY_IGOR_GONE set, Pipimon assumes Velvet Room");
        /* Re-enter the normal Velvet Room (now hosted by Pipimon) smoothly. */
        fieldstg_goto_map(0x20F, -1, 160 << 8, 210 << 8, 4);
    }
}

/* Called when walking through the Velvet Room's top door (0x20A) in Phase 3. */
int def_finale_on_top_door(int *map, int *x, int *y, int *dir) {
    if (!def_finale_top_door_open()) {
        return 0;
    }
    fn.phase = DEF_FINALE_PHASE3_BLUE_ROOM;
    fn.blue_step = 0;
    fn.blue_sub = 0;
    fn.blue_timer = FIN_BLINK_FRAMES;
    fn_dialog = NULL;
    *map = 0x20F; /* stay on Velvet Room stage, rendered with solid Velvet Blue backdrop */
    *x = 160 << 8;
    *y = 160 << 8;
    *dir = 0;
    port_log("finale: top door taken -> entering Blue Room Bonds sequence");
    return 1;
}

/* Called when entering any Velvet Room door in post-game (progress == 0x2D) before Igor's farewell. */
int def_finale_postcredits_enter(int from_map) {
    if (!def_on() || gamestate_data.progress != 0x2D || def_record_story(DEF_STORY_IGOR_GONE)) {
        return 0;
    }
    fn.phase = DEF_FINALE_POSTCREDITS_BLUE;
    fn.post_from_map = from_map;
    port_log("finale: post-credits Velvet Room entry -> Blue Room Igor farewell");
    return 1;
}

static const unsigned char *bond_line(s32 step) {
    switch (step) {
    case 0: return def_str_FinBond1;
    case 1: return def_str_FinBond2;
    case 2: return def_str_FinBond3;
    case 3: return def_str_FinBond4;
    case 4: return def_str_FinBond5;
    default: return def_str_FinBondAll;
    }
}

/* Paints a solid Velvet Blue backdrop over the room tiles (behind actors & dialog) while in the Blue Room,
 * plus black screen blinks between the Sequence of Bonds lines. */
void def_finale_draw_blue_room(void) {
    GfxLayer *layer;
    POLY_F4 *poly;
    u32 *ot;

    if (!def_finale_blue_room_active()) {
        return;
    }
    layer = gfx_module.funcs.get_layer(0x1002);
    if (layer == NULL) {
        return;
    }
    /* Solid deep Velvet Blue behind actors (depth 6 > actors at depth 4). */
    ot = layer->get_ot_entry(layer, 6);
    poly = gfx_module.funcs.get_packet();
    setPolyF4(poly);
    poly->r0 = 16;
    poly->g0 = 36;
    poly->b0 = 112;
    poly->x0 = poly->x2 = -80;
    poly->x1 = poly->x3 = 400;
    poly->y0 = poly->y1 = 0;
    poly->y2 = poly->y3 = 240;
    addPrim(ot, poly);
    gfx_module.funcs.set_packet(poly + 1);

    /* Black blink transition between bond messages in Phase 3 Blue Room. */
    if (fn.phase == DEF_FINALE_PHASE3_BLUE_ROOM && fn.blue_sub == 0 && fn.blue_timer > 0) {
        ot = layer->get_ot_entry(layer, 1);
        poly = gfx_module.funcs.get_packet();
        setPolyF4(poly);
        poly->r0 = poly->g0 = poly->b0 = 0;
        poly->x0 = poly->x2 = -80;
        poly->x1 = poly->x3 = 400;
        poly->y0 = poly->y1 = 0;
        poly->y2 = poly->y3 = 240;
        addPrim(ot, poly);
        gfx_module.funcs.set_packet(poly + 1);
    }
}

/* Per-frame field driver for Phase 2->3 Velvet pull and Phase 3 Blue Room Bonds -> Phase 4 rematch. */
int def_finale_field_frame(void) {
    s32 map;

    if (!def_on()) {
        return 0;
    }
    map = gamestate_data.funcs.get_map();

    /* Phase 2 scripted defeat returned to map 0x2DF -> pull into Last Velvet Room on first free frame. */
    if (fn.phase == DEF_FINALE_PHASE3_VELVET && fn.need_velvet_pull) {
        if (fieldstg_stage.event_running == 0 && fieldstg_stage.menu_open == 0 && fieldstg_stage.battle_starting == 0 &&
            map == FIN_GALACTIC_MAP) {
            fn.need_velvet_pull = 0;
            port_log("finale: pulling hero from 0x%X into Last Velvet Room", map);
            dw3_velvet_pull_by(FIN_GALACTIC_MAP, DW3_VELVET_PULL_IGOR);
            return 1;
        }
        return 0;
    }

    /* Phase 3 Blue Room: Sequence of Bonds (blink black -> companion line x5 -> gathered together -> Phase 4). */
    if (fn.phase == DEF_FINALE_PHASE3_BLUE_ROOM && map == 0x20F) {
        if (fn.blue_sub == 0) {
            if (fn.blue_timer > 0) {
                fn.blue_timer -= gfx_module.funcs.get_frame_ticks();
                return 1;
            }
            fn.blue_sub = 1;
            return 1;
        }
        if (fn.blue_sub == 1) {
            const unsigned char *text = bond_line(fn.blue_step);
            fn_dialog = message_box_create(0x1004, (u8 *)text, -1);
            fn.blue_sub = 2;
            return 1;
        }
        if (fn.blue_sub == 2) {
            fn_dialog = heap_objects.try_run(fn_dialog);
            if (fn_dialog == NULL) {
                fn.blue_step++;
                if (fn.blue_step >= FIN_BOND_COUNT) {
                    /* All bonds spoken -> pick strongest partner, prepare Phase 4 rematch vs Dark Galacticmon. */
                    s32 i;
                    fn.lead_member = pick_strongest_partner();
                    for (i = 0; i < 3; i++) {
                        fn.saved_party[i] = gamestate_data.party[i];
                    }
                    fn.saved_party_valid = 1;
                    gamestate_data.party[0] = fn.lead_member;
                    gamestate_data.party[1] = -1;
                    gamestate_data.party[2] = -1;
                    fn.phase = DEF_FINALE_PHASE4_SUPER_CHOSEN;
                    fn.super_chosen_on = 0;
                    port_log("finale: Bonds complete -> launching Phase 4 rematch (lead %d)", fn.lead_member);
                    fieldstg_goto_map(FIN_GALACTIC_MAP, -1, 0x24700, 0x18C00, 0);
                    return 1;
                }
                fn.blue_sub = 0;
                fn.blue_timer = FIN_BLINK_FRAMES;
            }
            return 1;
        }
    }

    /* Arrived back on 0x2DF in Phase 4 -> immediately start Galacticmon battle (listed battle 1). */
    if (fn.phase == DEF_FINALE_PHASE4_SUPER_CHOSEN && map == FIN_GALACTIC_MAP) {
        if (fieldstg_stage.event_running == 0 && fieldstg_stage.battle_starting == 0) {
            port_log("finale: starting Phase 4 Dark Galacticmon rematch on 0x2DF");
            gamestate_flags.set_flag(0x7401, 1);
            return 1;
        }
    }
    return 0;
}

/* ---- Pipimon sprite art (Oikawa's partner, replacing Igor in Velvet Room post-credits) ----
 * Drawn as crisp 16x16 pixel-art runs over the actor's screen spot, gently bobbing up and down. */
int dw3_velvet_pipimon_art(FieldstgActor *actor, u32 *ot, s32 x, s32 y) {
    /* Palette: 1 dark outline, 2 light green body, 3 mid green shade, 4 pink sprout, 5 dark eye, 6 white shine. */
    static const unsigned char pal[7][3] = {
        { 0, 0, 0 },
        { 24, 52, 28 },
        { 126, 224, 118 },
        { 76, 178, 80 },
        { 244, 128, 176 },
        { 28, 24, 32 },
        { 248, 252, 244 },
    };
    /* Runs: { row, col, width, color_idx } in a 16x16 box. */
    static const unsigned char runs[][4] = {
        /* Pink sprout / leaf on top */
        { 1, 7, 2, 4 }, { 2, 6, 4, 4 }, { 3, 7, 2, 4 },
        /* Top outline + round green body */
        { 4, 5, 6, 1 },
        { 5, 4, 1, 1 }, { 5, 5, 6, 2 }, { 5, 11, 1, 1 },
        { 6, 3, 1, 1 }, { 6, 4, 2, 6 }, { 6, 6, 6, 2 }, { 6, 12, 1, 1 },
        { 7, 2, 1, 1 }, { 7, 3, 10, 2 }, { 7, 13, 1, 1 },
        /* Eyes row */
        { 8, 2, 1, 1 }, { 8, 3, 2, 2 }, { 8, 5, 2, 5 }, { 8, 7, 2, 2 }, { 8, 9, 2, 5 }, { 8, 11, 2, 2 }, { 8, 13, 1, 1 },
        { 9, 2, 1, 1 }, { 9, 3, 2, 2 }, { 9, 5, 1, 6 }, { 9, 6, 1, 5 }, { 9, 7, 2, 2 }, { 9, 9, 1, 6 }, { 9, 10, 1, 5 }, { 9, 11, 2, 2 }, { 9, 13, 1, 1 },
        /* Lower body + gentle green shading */
        { 10, 2, 1, 1 }, { 10, 3, 10, 2 }, { 10, 13, 1, 1 },
        { 11, 2, 1, 1 }, { 11, 3, 2, 3 }, { 11, 5, 6, 2 }, { 11, 11, 2, 3 }, { 11, 13, 1, 1 },
        { 12, 3, 1, 1 }, { 12, 4, 8, 3 }, { 12, 12, 1, 1 },
        { 13, 4, 8, 1 },
    };
    s32 bob = ((gfx_module.funcs.get_time() / 16) & 1) ? -1 : 0;
    s32 left = x - 8;
    s32 top = y - 16 + bob;
    unsigned i;

    (void)actor;
    if (left < -32 || left > 320 + 64 || top < -32 || top > 240) {
        return 1;
    }
    for (i = 0; i < sizeof(runs) / sizeof(runs[0]); i++) {
        const unsigned char *r = runs[i];
        const unsigned char *rgb = pal[r[3]];
        POLY_F4 *poly = gfx_module.funcs.get_packet();
        s32 x0 = left + r[1];
        s32 x1 = x0 + r[2];
        s32 y0 = top + r[0];

        setPolyF4(poly);
        poly->r0 = rgb[0];
        poly->g0 = rgb[1];
        poly->b0 = rgb[2];
        poly->x0 = poly->x2 = (s16)x0;
        poly->x1 = poly->x3 = (s16)x1;
        poly->y0 = poly->y1 = (s16)y0;
        poly->y2 = poly->y3 = (s16)(y0 + 1);
        addPrim(ot, poly);
        gfx_module.funcs.set_packet(poly + 1);
    }
    return 1; /* replaces Igor's body & head completely */
}

void def_finale_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, fn);
}
