/* "Definitive" combat. Always on in the port, no mod option. Here: the battle log;
 * the text overrides; Inspect, the hit sounds, the "enemy acts twice" test. The game's C calls the def_* hooks under #ifdef PC_PORT; everything else lives here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdload.h"
#include "common.h"
#include "fightstg.h"
#include "message.h"
#include "sound.h"
#include "gamestate.h"
#include "records.h"
#include "pad.h"

#include "port_runtime.h"
#include "savestate.h"

#include "definitive.h"
#include "definitive_persona.h"

s32 fightstg_rules_get_damage(u8 side, s32 id);
s32 fightstg_rules_get_special_damage(u8 side, s32 id);

/* ---- DW3_BATTLE_LOG=<file>: one plain-text line per battle event, appended. Read once; unset = every hook is a no-op.
 * The line order is the game's own, so two runs with the same seed and inputs write the same file. */
/* The terms of the last damage multiplier, for the log line of the damage it made (0 when none applied). */
static struct {
    int weak, open, resisted, affinity, mult;
} terms;
/* OVERFINISH: a blow that knocks the enemy out and earns an extra action leaves it lying there for that action (its
 * knock-out event waits behind the party's turn), and it can be hit again. Kept as a reward for chaining a weakness
 * into the kill: such a blow shows the OVERFINISH! callout and its damage pays Bits with the battle's
 * (DEF_OVERFINISH_BITS_PCT). `overfinish_hit`: the blow being shown is one; `overfinish_damage`: the battle's total. */
static int overfinish_hit;
static int guard_hit; /* the DEF_GUARD_* that cut the blow being shown (guard_damage), 0 none; every blow resets it */
static s32 overfinish_damage;
static DefChain chain; /* stagger, window, free swap (definitive_mask.c) */
static short resist_mod[2][3][7]; /* per side (0 party, 1 enemy) and slot, what Resist Down took off each element */
static unsigned char resist_turns[2][3]; /* and the turns of that side it still lasts */
static signed char mod_level[2][3][3]; /* 3-tier stat buff/debuff level (-3..+3) per side, slot, kind (0 Atk, 1 Def, 2 Spd) */
static unsigned char mod_turns[2][3][3]; /* turns of that side remaining before the stat modifier expires */
static int baton_paid; /* Baton Pass was chosen in the switch menu (its MP is paid there) */

static FILE *log_file;
static int log_armed;
static int log_seq;        /* events since the battle began */
static int log_turns[2];   /* turns taken per side, for the "end" line */
static int log_battle = -1;

static int log_on(void) {
    if (!log_armed) {
        const char *path = getenv("DW3_BATTLE_LOG");
        log_armed = 1;
        if (path != NULL && path[0] != '\0') {
            log_file = fopen(path, "a");
        }
    }
    return log_file != NULL;
}

#define LOG(...)                          \
    do {                                  \
        fprintf(log_file, __VA_ARGS__);   \
        fputc('\n', log_file);            \
        fflush(log_file);                 \
    } while (0)

static void persona_battle_begin(void);
static const DefPersona *persona_at(int slot);

/* Blast Gauge phase 1: the Digimon (bits by id) that took their Blast form this battle (def_blast_started), whose price
 * the battle's end takes (def_blast_price). Emptied when a battle begins. */
static unsigned char blast_cursed;
/* A party member was knocked out this battle: a critical moment, when a full gauge tempts (def_blast_tempt). */
static unsigned char blast_fallen;
static unsigned char blast_step_taken = 0xFF; /* def_blast_form's step, for def_blast_started (0xFF: none) */
static unsigned char chosen_used;             /* this battle's Chosen Evolution was taken (one a battle) */
/* The overheat after a Blast or Chosen form: the sealed own turns left live in the partner's record (def_record_heat,
 * by Digimon, so the seal follows it into the next battles until an inn or Gennai cools it); this is only, by party slot,
 * whether the seal (status 0x20, the game's own digivolve seal) is ours to lift. */
static unsigned char overheat_sealed[3];
static unsigned char boost_off; /* blast_record: the form's stats as they are, without the boost */
static unsigned char boost_logged; /* slots whose boost the log named this battle */
static unsigned char blessed_now;  /* this battle's Blast was the blessed one (the flag is already set by then) */
static unsigned char plug_last_stand_used; /* global auto-revive on last Digimon KO (at most once per battle) */
static unsigned char plug_last_stand_notice; /* 1 until the next battle menu shows the Last Stand notice */
static signed char plug_last_stand_slot = -1; /* the slot it revived, until its Blast (-1: none); wfightmn sets the form
                                                * before def_blast_form, so the Rookie is the member's base_digimon then */

/* The Blast opens at the Ogremon (owner, 2026-11-18, so nobody sets it off by accident before the tutorial): in the tutorial
 * hunt's battle, and for good once that hunt is won. */
static int blast_unlocked(void) {
    return def_super_tutorial_battle() || def_super_done() > DEF_SUPER_TUTORIAL;
}

/* The tutorial hunt's battle, or the first Blast of the game not yet set off. */
static int blast_blessed(void) {
    return def_super_tutorial_battle() || !def_record_story(DEF_STORY_BLAST_FIRST);
}

void def_log_begin(void) {
    int i;

    port_hud_arm();
    def_mp_hook();
    def_chain_reset(&chain);
    blast_cursed = 0;
    blast_fallen = 0;
    chosen_used = 0;
    memset(overheat_sealed, 0, sizeof(overheat_sealed));
    boost_logged = 0;
    blessed_now = 0;
    plug_last_stand_used = 0;
    plug_last_stand_notice = 0;
    plug_last_stand_slot = -1;
    def_blast_heat_begin();
    def_callout_show(DEF_CALLOUT_NONE, 0);
    def_hunt_reward_done(); /* a reward a results screen never showed does not carry over */
    def_dri_battle_begin(); /* whether this battle is the DRI simulation just set up */
    def_flee_ask_reset();   /* the escape's question: a new battle's heap */
    def_purify_battle_begin(); /* Blast phase 2: whether it is a purification's wave */
    def_time_battle_begin();   /* the Time Room: whether it is a trial */
    def_story_battle_begin();  /* the Oikawa's arc: whether it is his */
    def_finale_battle_begin(); /* Extended Final Act: Dark Galacticmon / Super Chosen Evolution */
    persona_battle_begin();    /* the enemy personas: who this battle is (after the hunts and the DRI said so) */
    if (def_hunt_active() || def_super_battle_ready()) {
        def_callout_show(DEF_CALLOUT_RARE, 1); /* a Rare Target; a Super Rare too */
    }
    overfinish_hit = 0;
    guard_hit = 0;
    overfinish_damage = 0;
    memset(resist_mod, 0, sizeof(resist_mod));
    memset(resist_turns, 0, sizeof(resist_turns));
    memset(mod_level, 0, sizeof(mod_level));
    memset(mod_turns, 0, sizeof(mod_turns));
    if (!log_on()) {
        return;
    }
    log_seq = 0;
    log_turns[0] = log_turns[1] = 0;
    log_battle = records_state.battle;
    LOG("battle %d kind %d type %d", records_state.battle, records_state.battle_kind,
        fightstg_battle.state.type);
    for (i = 0; i < 3; i++) {
        FightstgMember *m = &fightstg_battle.state.members[0][i];
        if (m->digimon != 0) {
            LOG("  party %d digimon %d hp %d/%d mp %d/%d", i, m->digimon, m->hp, m->max_hp, m->mp, m->max_mp);
        }
    }
    for (i = 0; i < 3; i++) {
        FightstgMember *m = &fightstg_battle.state.members[1][i];
        if (m->digimon != 0) {
            LOG("  enemy %d digimon %d level %d hp %d/%d mp %d scale %d", i, m->digimon,
                records_state.enemies[i].level, m->hp, m->max_hp, m->mp, records_state.enemies[i].stat_scale);
        }
    }
}

void def_log_turn(int side) {
    if (!log_on()) {
        return;
    }
    log_turns[side != 0]++;
    LOG("turn %d side %d n %d member %d", ++log_seq, side != 0, log_turns[side != 0],
        (int)fightstg_battle.state.current[side != 0]);
}

/* kind: for the party the command (0 attack, 1 escape, 2 digivolve, 3 item, 4 technique, 5 switch, 6 team tech); for
 * the enemy the action (1 basic attack, >1 technique ID, <0 escape/other). arg: the technique or item. */
void def_log_action(int side, int kind, int arg) {
    int tech = -1;

    if (side == 0) {
        guard_hit = 0;
    }
    if (!log_on()) {
        return;
    }
    if (side == 0) {
        if (kind == 4) {
            tech = arg;
        }
    } else if (kind > 1) {
        tech = kind;
    }
    if (tech >= 1) {
        RecordsTechnique *rec = &records_techniques[tech - 1];
        LOG("action side %d kind %d arg %d tech %d class %d defstat %d power %d mp %d", side != 0, kind, arg, tech,
            rec->element, rec->defense_stat, rec->power, rec->mp_cost);
    } else {
        LOG("action side %d kind %d arg %d", side != 0, kind, arg);
    }
}

/* The hit sounds: how the last damage the player dealt with an elemental technique met the target (0 none, 1 effective,
 * 2 neutral, 3 resisted); def_hit_feedback plays its sound when the hit's own sound does. */
static int hit_class;
static int hit_elem; /* its element index, valid with hit_class */
static s32 hit_id;

/* The species' Dark mark (the row table), 0 for a species with no row. */
static int dark_mark(int species) {
    int i;

    for (i = 0; i < def_enemy_row_count; i++) {
        if (def_enemy_rows[i].id == species) {
            return def_enemy_rows[i].dark;
        }
    }
    return 0;
}

/* Element `e` against a target whose resists (modifiers included) are `resists`: 1 weak, 2 neutral, 3 resisted. */
static const DefPersona *persona_map_of(int species);

static int target_class(int e, const short *resists, int species) {
    if (def_resisted(resists[e])) {
        return 3;
    }
    /* Dark is the enemy table's mark (dark_mark), not its resistance; a persona's own map writes its Dark weakness
     * into resists[6] (def_persona_resists), so that is read instead (a WEAK(DARK) row used to never stagger). */
    return (def_weakness_mask(resists, persona_map_of(species) != NULL ? resists[6] <= DEF_WEAK_MAX : dark_mark(species)) >>
            e) & 1
               ? 1
               : 2;
}

static void classify(u8 side, s32 id) {
    RecordsTechnique *rec = &records_techniques[id - 1];

    hit_class = 0;
    /* overfinish_hit is not reset here: classify runs on every damage roll, so one made between the blow's resolution
     * (def_action_resolved, which sets it for each party blow) and its hit sound would turn OVERFINISH! into WEAK! */
    if (side != 0 || rec->defense_stat < 2 || rec->defense_stat > 8) {
        return;
    }
    hit_class = target_class(rec->defense_stat - 2, fightstg_rules.stats[1].resists,
                             fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon);
    hit_elem = rec->defense_stat - 2;
    hit_id = id;
}

/* Hard Mode (the save's DEF_SAVE_HARD flag, chosen on New Game): the party is two Digimon. The third of the starting
 * pack is owned but stays out (the lab keeps it), and the third slot takes nobody: this empties it wherever a party
 * was just set (the starting pack, a loaded field), and the lab refuses the slot (def_hard_slot_closed). */
static void patch_techniques(void);

void def_hard_party_fix(void) {
    if (def_on()) {
        patch_techniques(); /* loaded save: ensure techniques & Digimon slots are patched before status/lab/rush */
    }
    if (def_save_flag(DEF_SAVE_HARD) && gamestate_data.party[2] >= 0) {
        gamestate_data.party[2] = -1;
    }
}

int def_hard_slot_closed(int slot) {
    return slot >= 2 && def_save_flag(DEF_SAVE_HARD);
}

/* New Game's choice, kept past the new game's own reset of the save's port state (gamestate_init_records). */
static int new_game_hard;

void def_new_game_difficulty(int hard) {
    new_game_hard = hard;
    def_save_flag_set(DEF_SAVE_HARD, hard);
}

/* Girls Power: the hero New Game chose, kept the same way (the save's DEF_SAVE_KAILI). */
static int new_game_kaili;

void def_new_game_hero(int kaili) {
    new_game_kaili = kaili;
    def_save_flag_set(DEF_SAVE_KAILI, kaili);
    port_girls_refresh();
}

void def_new_game_apply(void) {
    def_save_flag_set(DEF_SAVE_HARD, new_game_hard);
    def_save_flag_set(DEF_SAVE_KAILI, new_game_kaili);
    new_game_hard = 0;
    new_game_kaili = 0;
    port_girls_refresh();
}

/* The Bits the battle's OVERFINISH blows earned, given once (the results screen adds them to the battle's money). */
s32 def_overfinish_take_bits(void) {
    s32 bits = def_on() ? overfinish_damage * DEF_OVERFINISH_BITS_PCT / 100 : 0;

    overfinish_damage = 0;
    return bits;
}

void def_hit_feedback(void) {
    static const s32 sound[4] = { 0, DEF_SND_EFFECTIVE, DEF_SND_NEUTRAL, DEF_SND_RESISTED };

    if (fightstg_action.side != 0) {
        return;
    }
    if (overfinish_hit) { /* whatever the blow's element, and over WEAK!: the word of the moment is this one */
        sound_module.play(DEF_SND_EFFECTIVE);
        def_callout_show(DEF_CALLOUT_OVERFINISH, 1);
        return;
    }
    if (guard_hit && hit_class != 1) { /* a weakness is the word that matters: it opens orange, goes through red */
        sound_module.play(DEF_SND_RESISTED);
        def_callout_show(guard_hit == DEF_GUARD_RED ? DEF_CALLOUT_GUARDED_RED : DEF_CALLOUT_GUARDED, 1);
        return;
    }
    if (hit_class == 0) {
        return;
    }
    sound_module.play(sound[hit_class]);
    def_callout_show(hit_class == 1 ? DEF_CALLOUT_WEAK : hit_class == 3 ? DEF_CALLOUT_RESIST : DEF_CALLOUT_NONE, 1);
    if (log_on()) {
        LOG("hitsound class %d tech %d", hit_class, (int)hit_id);
    }
}

/* ---- The enemy personas (definitive_persona.h): this file tells them who is fought and what happens. */
static DefPersonaWhere persona_where_now(void) {
    DefPersonaWhere w;

    w.super = (short)def_super_fighting();
    w.region = (short)(w.super < 0 ? def_hunt_region_fighting() : -1);
    w.dri = (short)def_dri_challenge_fighting();
    if (w.dri < 0) { /* the model is added before the battle's begin hook: the DRI line set up for this battle */
        w.dri = (short)def_dri_challenge_pending();
    }
    w.battle = (short)records_state.battle;
    return w;
}

static void persona_battle_begin(void) {
    DefPersonaWhere w = persona_where_now();

    def_persona_battle_begin(&w);
}

/* The encounter read live, the way the old Guard list and the recolor did: one that is not what the battle began with
 * (the enemy's model is added before the battle's begin hook; a test sets the battle by hand) is looked up again. */
static void persona_refresh(void) {
    DefPersonaWhere w = persona_where_now();

    if (!def_persona_same_where(&w)) {
        def_persona_battle_begin(&w);
    }
}

/* The colour of the enemy model being added (def_recolor_model_added), 0 none. */
int def_persona_model_hue(void) {
    if (!def_on()) {
        return 0;
    }
    persona_refresh();
    return def_persona_battle_hue(-1);
}

/* The persona of enemy slot `slot` (0..2), NULL none. */
static const DefPersona *persona_at(int slot) {
    if (!def_on() || slot < 0 || slot > 2) {
        return NULL;
    }
    persona_refresh();
    return def_persona_slot(slot, fightstg_battle.state.members[1][slot].digimon);
}

/* The current target's persona when it changes the map: its own discovery, its own Dark rule. */
static const DefPersona *persona_map_of(int species) {
    int slot = fightstg_battle.state.current[1] % 3;
    const DefPersona *p;

    if (fightstg_battle.state.members[1][slot].digimon != species) {
        return NULL;
    }
    p = persona_at(slot);
    return def_persona_own_map(p) ? p : NULL;
}

/* An enemy's stats block (fightstg_rules_get_stats): its persona's resistances for the phase it is in, before Resist
 * Down / Mega Break lower them, and its Defense (stats[1]) at DEF_PCT. */
void def_persona_apply(int member, FightstgStats *out) {
    const DefPersona *p = persona_at(member);

    if (def_persona_own_map(p)) {
        def_persona_resists(p, def_persona_phase(member), out->resists, out->resists);
    }
    if (p != NULL && p->def_pct != 0) {
        out->stats[1] = (s16)def_persona_def(p, out->stats[1]);
    }
    if (p != NULL && p->pow_pct != 0) {
        out->stats[0] = (s16)def_persona_pow(p, out->stats[0]);
    }
}

/* The battle's enemies as fieldstg_start_battle just copied them: each one's HP after its persona's HP_PCT. A hunt
 * or a Super Rare that borrows the battle replaces them afterwards and is not a CAMPAIGN row anyway. */
void def_persona_enemies_scale(void) {
    DefPersonaWhere w;
    int i;

    if (!def_on()) {
        return;
    }
    w = persona_where_now();
    for (i = 0; i < 3; i++) {
        RecordsEnemy *e = &records_state.enemies[i];

        if (e->digimon != 0) {
            e->hp = (s16)def_persona_hp(def_persona_find(&w, e->digimon), e->hp);
        }
    }
}

/* 1 when enemy slot `slot` has a rule of its own: the HUD marks it (the inspect panel's title). */
int def_persona_warn(int slot) {
    return persona_at(slot % 3) != NULL;
}

const unsigned char *def_persona_title(int species, const unsigned char *name) {
    extern const unsigned char def_str_PersonaMark[];
    static unsigned char buf[48];
    int p = 0;

    if (!def_persona_warn(fightstg_battle.state.current[1]) || (name == NULL && (name = def_species_name(species)) == NULL)) {
        return name;
    }
    def_glyph_put(buf, &p, sizeof(buf), def_str_PersonaMark);
    def_glyph_put(buf, &p, sizeof(buf), name);
    return buf;
}

/* A phase changed: say so, and repaint the model when the phase has its colour. */
static void persona_phase_changed(int slot) {
    int hue = def_persona_battle_hue(slot);

    def_callout_show(DEF_CALLOUT_SHIFT, 1);
    if (hue != 0) {
        def_recolor_enemy_hue(hue);
    }
    if (log_on()) {
        LOG("persona: slot %d phase %d (hue %d)", slot, def_persona_phase(slot), hue);
    }
}

/* The Guard: some enemies take only DEF_GUARD_PCT of the party's blows until a stagger opens them (the weakness that
 * staggers is what the player has to find; the blow that opens it is still guarded, the stagger being set after the
 * damage). Who is guarded, and in which phase, is the enemy personas' (definitive_persona_table.c: the story bosses
 * are CAMPAIGN rows, battle and species, so a hunt or a duel that borrows their battle is not); the Time Room's trials
 * guard their last challenger. Never the final boss. The red guard (definitive_persona.h DEF_GUARD_RED) is never
 * opened: a stagger still gives its extra action, and guard_damage lets a blow on a weakness through instead.
 * enemy_guard_kind: the guard the current phase has, staggered or not; enemy_guarded: the one up now, DEF_GUARD_*. */
static int enemy_guard_kind(void) {
    int slot = fightstg_battle.state.current[1];
    s32 species = fightstg_battle.state.members[1][slot].digimon;

    if (fightstg_battle.state.type == 6) {
        return DEF_GUARD_NONE;
    }
    if (def_time_guarded(species)) {
        return DEF_GUARD_ORANGE;
    }
    return def_persona_guard(persona_at(slot % 3), def_persona_phase(slot % 3));
}

static int enemy_guarded(void) {
    int kind = enemy_guard_kind();

    return kind == DEF_GUARD_ORANGE && chain.staggered[fightstg_battle.state.current[1] % 3] ? DEF_GUARD_NONE : kind;
}

/* ---- The INFO panel's traits (the battle menu's Inspect): what kind of enemy the current target is, read from its
 * persona row and the battle's state, never changing them. def_persona_enemy_turn / def_persona_staggered advance the
 * phase and def_turn_queue rewrites fightstg_rules.stats, so none of them is called from here. */
static int inspect_has_guard(const DefPersona *p, s32 species) {
    if (fightstg_battle.state.type == 6) { /* the final boss is never guarded (enemy_guarded) */
        return 0;
    }
    return def_time_guarded(species) || def_persona_guard(p, 0) || def_persona_guard(p, 1);
}

void def_inspect_info(DefInspectInfo *out) {
    int slot = fightstg_battle.state.current[1];
    s32 species = fightstg_battle.state.members[1][slot].digimon;
    const DefPersona *p = persona_at(slot % 3);

    memset(out, 0, sizeof(*out));
    if (!def_on()) {
        return;
    }
    if (p != NULL && p->phases == 2) {
        out->kind[out->count++] = DEF_TRAIT_SHIFT;
        out->phase = (unsigned char)def_persona_phase(slot % 3);
        out->left = (unsigned char)def_persona_turns_left(slot % 3);
        out->every = p->turns;
    }
    if (inspect_has_guard(p, species)) {
        out->kind[out->count++] = DEF_TRAIT_GUARD;
        out->guarded = (unsigned char)(enemy_guarded() != DEF_GUARD_NONE); /* the rule itself: phase, stagger, Time Room */
        /* the colour of the guard this phase has; an open phase shows the other one's (a tide that is red half the time) */
        out->red = (unsigned char)((enemy_guard_kind() != DEF_GUARD_NONE ? enemy_guard_kind()
                                    : def_persona_guard(p, 0) | def_persona_guard(p, 1)) == DEF_GUARD_RED);
    }
    if (p != NULL && p->speed_x != 0) {
        out->kind[out->count++] = DEF_TRAIT_SPEED;
    }
    if (p != NULL && p->def_pct != 0) {
        out->kind[out->count++] = DEF_TRAIT_DEF;
    }
}

/* A legend line made of a translated head, the row's number and a translated tail ("Speed " 3 "x yours"): the
 * languages put the number in different places, so each has its own head and tail. */
static void legend_number(unsigned char *buf, int *p, int size, const unsigned char *head, int n,
                          const unsigned char *tail) {
    def_glyph_put(buf, p, size, head);
    def_glyph_number(buf, p, size, n);
    def_glyph_put(buf, p, size, tail);
}

static void legend_newline(unsigned char *buf, int *p, int size) {
    static const unsigned char newline[3] = { 2, 1, 0 };

    def_glyph_put(buf, p, size, newline);
}

const unsigned char *def_inspect_legend(int kind) {
    extern const unsigned char def_str_InfoShiftTurnsA[], def_str_InfoShiftTurnsB[], def_str_InfoShiftStagger[],
        def_str_InfoShiftWeak[], def_str_InfoShiftGuard[], def_str_InfoShiftBoth[], def_str_InfoGuardA[],
        def_str_InfoGuardB[], def_str_InfoGuardUntil[], def_str_InfoSpeedA[], def_str_InfoSpeedB[],
        def_str_InfoDef[], def_str_InfoShiftRed[], def_str_InfoGuardRed[];
    DefInspectInfo info;
    static unsigned char buf[DEF_TRAITS][96];
    const DefPersona *p = persona_at(fightstg_battle.state.current[1] % 3);
    unsigned char *b;
    int n = 0;

    if (kind < 0 || kind >= DEF_TRAITS) {
        return NULL;
    }
    b = buf[kind];
    b[0] = 0;
    switch (kind) {
    case DEF_TRAIT_SHIFT: /* when, then what changes: the weaknesses, the guard or both */
        if (p == NULL || p->phases != 2) {
            break;
        }
        if (p->turns != 0) {
            legend_number(b, &n, sizeof(buf[0]), def_str_InfoShiftTurnsA, p->turns, def_str_InfoShiftTurnsB);
        } else {
            def_glyph_put(b, &n, sizeof(buf[0]), def_str_InfoShiftStagger);
        }
        legend_newline(b, &n, sizeof(buf[0]));
        if (!def_persona_shifts_map(p) && def_persona_guard(p, 0) != DEF_GUARD_NONE &&
            def_persona_guard(p, 1) != DEF_GUARD_NONE) { /* orange one phase, red the other */
            def_glyph_put(b, &n, sizeof(buf[0]), def_str_InfoShiftRed);
            break;
        }
        def_glyph_put(b, &n, sizeof(buf[0]),
                      !def_persona_shifts_map(p)                          ? def_str_InfoShiftGuard
                      : def_persona_guard(p, 0) != def_persona_guard(p, 1) ? def_str_InfoShiftBoth
                                                                           : def_str_InfoShiftWeak);
        break;
    case DEF_TRAIT_GUARD:
        legend_number(b, &n, sizeof(buf[0]), def_str_InfoGuardA, DEF_GUARD_PCT, def_str_InfoGuardB);
        legend_newline(b, &n, sizeof(buf[0]));
        def_inspect_info(&info); /* the guard's colour, read the way the panel's picture reads it */
        def_glyph_put(b, &n, sizeof(buf[0]), info.red ? def_str_InfoGuardRed : def_str_InfoGuardUntil);
        break;
    case DEF_TRAIT_SPEED:
        if (p != NULL) {
            legend_number(b, &n, sizeof(buf[0]), def_str_InfoSpeedA, p->speed_x, def_str_InfoSpeedB);
        }
        break;
    case DEF_TRAIT_DEF:
        if (p != NULL) {
            def_glyph_put(b, &n, sizeof(buf[0]), def_str_InfoDef); /* no number: the player needs only "weak defense" */
        }
        break;
    }
    return b;
}

/* `weak`: the blow hits a weakness (classify's hit_class 1), which goes through a red guard whole. */
static s32 guard_damage(u8 side, s32 value, int weak) {
    int kind;

    if (side == 0) {
        guard_hit = 0; /* every blow of the party decides it afresh */
    }
    kind = def_on() && side == 0 && value > 0 ? enemy_guarded() : DEF_GUARD_NONE;
    if (kind == DEF_GUARD_NONE || (kind == DEF_GUARD_RED && weak)) {
        return value;
    }
    guard_hit = kind;
    if (log_on()) {
        LOG("guard: %d -> %d", (int)value, (int)(value * DEF_GUARD_PCT / 100 > 0 ? value * DEF_GUARD_PCT / 100 : 1));
    }
    value = value * DEF_GUARD_PCT / 100;
    return value > 0 ? value : 1;
}

s32 def_counter_damage(u8 side, s32 value) {
    if (!def_on() || value <= 0) {
        return value;
    }
    value = guard_damage(side, value, 0); /* a counter is a physical blow: classify did not run for it */
    value = def_finale_damage(side, value);
    value = def_super_bell_damage(side, value);
    return value;
}

static s32 log_damage(u8 side, s32 id, s32 value, const char *what) {
    FightstgMember *target;

    classify(side, id);
    value = guard_damage(side, value, hit_class == 1);
    value = def_finale_damage(side, value);
    if (log_on()) {
        target = &fightstg_battle.state.members[side == 0][fightstg_battle.state.current[side == 0]];
        LOG("damage %s side %d tech %d value %d target_hp %d -> %d", what, side != 0, id, value, target->hp,
            target->hp > value ? target->hp - value : 0);
        if (terms.mult != 0) {
            LOG("  mult %d weak %d open %d resisted %d affinity %d", terms.mult, terms.weak, terms.open, terms.resisted,
                terms.affinity);
        }
    }
    return value;
}

/* The rules table's get_damage / get_special_damage (fightstg_rules in fightstg_8008D3B4.c). Pass-through: the same
 * calls, in the same order, so the random stream is untouched. */
s32 def_get_damage(u8 side, s32 id) {
    terms.mult = 0;
    return log_damage(side, id, fightstg_rules_get_damage(side, id), "phys");
}

s32 def_get_special_damage(u8 side, s32 id) {
    terms.mult = 0;
    return log_damage(side, id, fightstg_rules_get_special_damage(side, id), "magic");
}

void def_log_end(int result) {
    int i;

    def_hunt_battle_end(result); /* the hunt's chain: counted whether or not the log is on */
    /* The Super Rare hunts: its HP at the end, kept when it fled (it is the enemies' first) */
    def_super_battle_end(result, fightstg_battle.state.members[1][0].hp);
    def_dri_battle_end(result);  /* a DRI simulation's stages felled and their rewards */
    def_persona_battle_end();      /* won, lost or fled: the same encounter again starts from phase A */
    def_purify_battle_end(result); /* Blast phase 2: the dark forms felled, the next wave or the TP back */
    def_time_battle_end(result);   /* the Time Room: a trial won or lost */
    def_story_battle_end(result);  /* the Oikawa's arc */
    if (!log_on()) {
        return;
    }
    LOG("end battle %d result %d party_turns %d enemy_turns %d", log_battle, result, log_turns[0], log_turns[1]);
    for (i = 0; i < 3; i++) {
        FightstgMember *m = &fightstg_battle.state.members[0][i];
        if (m->digimon != 0) {
            LOG("  party %d hp %d/%d", i, m->hp, m->max_hp);
        }
    }
}

static const unsigned char *lookup_override_sorted(const DefTextOverride *tab, int count, int file, int index) {
    int lo = 0, hi = count - 1;

    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int f = tab[mid].file;

        if (f < file || (f == file && tab[mid].index < index)) {
            lo = mid + 1;
        } else if (f > file || tab[mid].index > index) {
            hi = mid - 1;
        } else {
            return tab[mid].bytes;
        }
    }
    return NULL;
}

/* ---- Text overrides. font_get_entry only gets the file's buffer, so the file is found by looking the buffer
 * up in the CD module's loaded entries (no load, no side effect). In a translated language (def_lang: Portuguese, French,
 * German, Italian, Spanish) its table is searched first in O(log N) (including DMG200..DMG900 container sub-tables). The
 * English sets (records_language 1, 2) then fall back to def_text_overrides and the disc's English entry; the other
 * languages fall back to the disc's own text, and to the English only for the port's own entries. Returns the
 * replacement entry or NULL. */
void *def_text_override(void *data, s32 i) {
    int e, k, s;

    if (data == NULL) {
        return NULL;
    }
    {
        const unsigned char *sup = def_super_talk_text();

        if (sup != NULL) {
            return (void *)sup;
        }
    }
    for (e = 0; e < 64; e++) {
        const CdloadEntry *ent = &cdload_module.entries[e];
        int base_id, match_file = -1;

        if (ent->state != 3 || ent->buffer == NULL) {
            continue;
        }
        base_id = ent->id - records_language;
        if (ent->buffer == data) {
            match_file = base_id;
        } else if (base_id >= 0x112 && base_id <= 0x158 && (const u8 *)data > (const u8 *)ent->buffer &&
                   (const u8 *)data < (const u8 *)ent->buffer + 0x20000) {
            const s32 *hdr = (const s32 *)ent->buffer;
            s32 nsub = hdr[0] / 4;

            if (nsub > 0 && nsub < 128) {
                for (s = 0; s < nsub; s++) {
                    if ((const u8 *)ent->buffer + hdr[s] == (const u8 *)data) {
                        match_file = base_id | ((s + 1) << 16);
                        break;
                    }
                }
            }
        }
        if (match_file < 0) {
            continue;
        }
        if (def_story_text(match_file, i) != NULL) {
            return (void *)def_story_text(match_file, i); /* a line a story hook changes while it holds the story */
        }
        if ((records_language == 1 || records_language == 2) && def_story_line(match_file, i) != NULL) {
            return (void *)def_story_line(match_file, i); /* the story rewrite (in Portuguese: pt_story.txt, the English for an entry not translated yet) */
        }
        if (match_file == 0x63 && i == 76 && def_blast_chosen_mode()) {
            extern unsigned char def_str_PlugRevive[];
            return def_str_PlugRevive; /* ITMINF 76: after the Blast quest the DV Plug is a revive item */
        }
        if (def_lang() != 0) {
            int count;
            const DefTextOverride *tab = def_text_overrides_of(def_lang(), &count);

            if (tab != NULL && count > 0 && !def_variant_text_hidden(base_id, i)) {
                const unsigned char *tr = lookup_override_sorted(tab, count, match_file, i);

                if (tr != NULL) {
                    return (void *)tr;
                }
            }
        }
        if (match_file == base_id
            && ((records_language == 1 || records_language == 2) || dw3_velvet_text_own(base_id, i))) {
            /* the other languages keep the disc's text, except the port's own entries: the disc has nothing where they
             * are (a single "a"), so the English stands in for them */
            for (k = 0; k < def_text_override_count; k++) {
                if (def_text_overrides[k].file == base_id && def_text_overrides[k].index == i) {
                    if (def_variant_text_hidden(def_text_overrides[k].file, i)) {
                        return NULL; /* a variant's name and description go with its data (replay: the disc's) */
                    }
                    {
                        static unsigned char seen[512]; /* one line per entry, not per frame it is drawn */
                        if (k < 512 && !seen[k]) {
                            seen[k] = 1;
                            port_log("text override: file 0x%X entry %d", ent->id, i);
                        }
                    }
                    return (void *)def_text_overrides[k].bytes;
                }
            }
        }
        return NULL;
    }
    return NULL;
}

/* ---- Inspect and the hit sounds, by the weakness rule (def_weakness_mask, def_resisted). */
extern const unsigned char def_str_Fire[], def_str_Water[], def_str_Ice[], def_str_Wind[], def_str_Thunder[],
    def_str_Machine[], def_str_Dark[], def_str_Weak[], def_str_Resist[], def_str_Neutral[];

/* Turn order by Speed, asymmetric: a faster party counts only the square root of its lead (4x the enemy's Speed to
 * act twice every time, where the disc needs 2x), so the party's extra actions come from One More and Baton Pass; a
 * faster enemy keeps the disc's rule, its answer to having no One More. Both delays of a pair read the same speeds,
 * so def_turn_queue stays right. side is the acting one's (0 the party, 0x10 the enemy). */
void def_turn_speeds(int side, int *own, int *other) {
    int *party = side == 0 ? own : other, *enemy = side == 0 ? other : own;
    int product, root = 999, i;
    const DefPersona *p = persona_at(fightstg_battle.state.current[1] % 3);

    if (p != NULL && p->speed_x != 0 && *party > 0) { /* SPEED_X: its Speed is the party's times n */
        *enemy = *party * p->speed_x;
    }
    if (!def_on() || *party <= *enemy || *enemy <= 0) {
        return;
    }
    product = *party * *enemy;
    for (i = 0; i < 10; i++) {
        root = (root + product / root) / 2;
    }
    *party = root; /* the geometric mean: the lead p/e becomes sqrt(p/e) */
}

/* The next turns, for the INFO panel's queue: with the command menu open, E the time left to the enemy's turn, Pe
 * the delay its next turn would take and P the one ours would (fightstg_events.get_delay kind 0: no draw, but it
 * rewrites fightstg_rules.stats, so only here, with the menu open and nothing being calculated). The enemy acts at E
 * and E + Pe, we at P; an enemy turn comes first only when strictly earlier (the "Enemy x2" rule, E + Pe < P). Ours
 * is the last one known: after it our own next delay would be needed, so a queue that starts with us is two long. */
int def_turn_queue(unsigned char queue[3]) {
    int e = fightstg_events.find(3);
    s32 left, enemy_next, own_next;
    int n = 0;

    if (e < 0) {
        return 0;
    }
    left = fightstg_events.events[e].delay;
    enemy_next = fightstg_events.get_delay(0x10, 0);
    own_next = fightstg_events.get_delay(0, 0);
    if (log_on()) {
        LOG("twice E %d Pe %d P %d -> %d", (int)left, (int)enemy_next, (int)own_next, left + enemy_next < own_next);
    }
    if (left < own_next) {
        queue[n++] = DEF_TURN_ENEMY;
        if (left + enemy_next < own_next) {
            queue[n++] = DEF_TURN_ENEMY;
        }
        queue[n++] = DEF_TURN_PARTY;
    } else {
        queue[n++] = DEF_TURN_PARTY;
        queue[n++] = DEF_TURN_ENEMY;
    }
    return n;
}

/* Limit Break (> 999 stats): scoped in v1.0 to Chosen Evolution (side 0) and Dark Galacticmon (side 1). */
int def_limit_break_active(int side) {
    if (!def_on()) {
        return 0;
    }
    if (side == 0) {
        if (def_finale_golden_mode()) {
            return 1;
        }
        if (def_blast_chosen_mode()) {
            s32 cur = fightstg_battle.state.current[0];
            if (cur >= 0 && cur < 3 && fightstg_battle.state.members[0][cur].blasted != 0) {
                FightstgStats *st = fightstg_rules.get_stats(0, 0, cur);
                int i;
                for (i = 0; i < 5; i++) {
                    if (st->stats[i] > 999) {
                        return 1;
                    }
                }
            }
        }
        return 0;
    }
    if (side == 1) {
        return def_finale_enemy_dark();
    }
    return 0;
}

const unsigned char *def_limit_break_label(void) {
    extern const unsigned char def_str_LimitBreak[];
    return def_str_LimitBreak;
}

const unsigned char *def_inspect_element(int e) {
    static const unsigned char *const names[7] = { def_str_Fire, def_str_Water,   def_str_Ice,  def_str_Wind,
                                                    def_str_Thunder, def_str_Machine, def_str_Dark };
    return names[e];
}

/* The font's coloured picture of each element (extra glyphs `01 xx` of the 14-pixel font, drawn with palette 0): flame,
 * drop, crystal, wing, bolt, gear, dark star. The same picture stands for the element on every screen. */
const unsigned char *def_element_icon(int e) {
    static const unsigned char icons[7][3] = { { 1, 0x2D, 0 }, { 1, 0x2E, 0 }, { 1, 0x30, 0 }, { 1, 0x31, 0 },
                                               { 1, 0x33, 0 }, { 1, 0x35, 0 }, { 1, 0x36, 0 } };
    return icons[e];
}

/* The five families the bestiary, Inspect and the HUD's Resist Down arrows show the seven elements as: Water with Ice
 * and Thunder with Machine, the pairs the multiplier's affinity already sums, each named by its first. Only the
 * screens group them: the rules still read seven resists, and the technique list and the status screens keep the
 * seven pictures (which of a pair a species is weak to is the player's to find). */
static const signed char family_elems[DEF_FAMILIES][2] = { { 0, -1 }, { 1, 2 }, { 3, -1 }, { 4, 5 }, { 6, -1 } };

int def_element_family(int e) {
    static const unsigned char family[7] = { 0, 1, 1, 2, 3, 3, 4 };

    return family[e];
}

/* The family's pictures: its first element's, and with `second` its other's (empty text for a family of one). */
const unsigned char *def_family_icon(int f, int second) {
    static const unsigned char none[1] = { 0 };
    int e = family_elems[f][second != 0];

    return e >= 0 ? def_element_icon(e) : none;
}

const unsigned char *def_family_name(int f) {
    return def_inspect_element(family_elems[f][0]);
}

/* def_resist_down_mask by family: a lowered element sets its family's bit (Ice the Water's, Machine the Thunder's). */
int def_resist_down_families(int side, int slot) {
    int mask = def_resist_down_mask(side, slot);
    int out = 0;
    int e;

    for (e = 0; e < 7; e++) {
        if ((mask >> e) & 1) {
            out |= 1 << def_element_family(e);
        }
    }
    return out;
}

int def_tech_element(int tech) {
    int stat = tech >= 1 ? records_techniques[tech - 1].defense_stat : 0;

    return stat >= 2 && stat <= 8 ? stat - 2 : -1;
}

/* A technique's picture where the game drew its class (red physical, blue magic; owner, 2026-11-22): a physical blow is
 * physical only, so it shows the fist (the Strength picture, glyph 01 25); magic shows its element, or the Spirit
 * picture (01 27: Spirit is the magic attack) when it has none. Healing and support techniques keep the game's own
 * class picture: empty text here, and def_tech_class_sprite says to draw it. */
const unsigned char *def_tech_picture(int tech) {
    static const unsigned char fist[3] = { 1, 0x25, 0 }, spirit[3] = { 1, 0x27, 0 }, empty[1] = { 0 };
    int e, cls;

    if (tech < 1) {
        return empty;
    }
    cls = records_techniques[tech - 1].element;
    e = def_tech_element(tech);
    if (cls == 2) {
        return fist;
    }
    if (cls == 3) {
        return e >= 0 ? def_element_icon(e) : spirit;
    }
    return empty;
}

/* 1 when the game's class sprite (records_techniques[].element + 0x37 of 0x02860000) is drawn for `tech`: always
 * outside the definitive edition, and there only for the classes def_tech_picture leaves to it. */
int def_tech_class_sprite(int tech) {
    int cls = tech >= 1 ? records_techniques[tech - 1].element : 0;

    return !def_on() || (cls != 2 && cls != 3);
}

/* A technique's picture (def_tech_picture) in window `w`, for the screens that list a form's techniques (Digi-Lab, the
 * status pages, the battle's technique menu and its Tag and digivolve previews), placed where the class sprite was.
 * `tech` carries the record's flag bits; `palette` is the row's (the picture is coloured, but a greyed row greys it as
 * the technique menu does). */
void def_tech_icon(struct MessageWindow *w, int tech, int palette) {
    if (w == NULL) {
        return;
    }
    w->set_text(w, (unsigned char *)def_tech_picture(tech & 0x1FFF), -1);
    w->set_palette(w, palette);
}

/* The label and the 14-pixel font's palette of a target_class (0: not learnt yet): the weakness is the line to find, so
 * it gets the warm colour, a resist the cold one, and what says nothing the grey. */
static const unsigned char *state_label(int cls, int *palette) {
    extern const unsigned char def_str_Unknown[];

    switch (cls) {
    case 0:
        *palette = 7;
        return def_str_Unknown;
    case 3:
        *palette = 1;
        return def_str_Resist;
    case 1:
        *palette = 3;
        return def_str_Weak;
    default:
        *palette = 7;
        return def_str_Neutral;
    }
}

/* What the battle's inspect panel knows of the current target: its persona's own discovery when it has its own map
 * (the species' bestiary would give it away), else the bestiary's. */
static int inspect_known(int species) {
    if (def_persona_hides_species(persona_map_of(species))) {
        return def_persona_known(fightstg_battle.state.current[1] % 3);
    }
    return def_bestiary_known(def_bestiary_index(species));
}

const unsigned char *def_inspect_state(const short *resists, int e, int *palette) {
    int species = fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon;

    if (!((inspect_known(species) >> e) & 1)) { /* not learnt yet: Libra or a hit on it tells */
        return state_label(0, palette);
    }
    return state_label(target_class(e, resists, species), palette);
}

/* The species' affinities as fought: a common's own row (RESIST_ALL) over the enemy table. */
static const short *bestiary_resists(FightstgEnemyRecord *rec, short *buf) {
    const DefPersona *p = def_persona_species(rec->id);

    def_enemy_override(rec); /* the field reads SDIGIEDT itself: the record may not have gone through the battle's get */
    if (!def_persona_own_map(p)) {
        return rec->resists;
    }
    def_persona_resists(p, 0, rec->resists, buf);
    return buf;
}

const unsigned char *def_bestiary_state(FightstgEnemyRecord *rec, int index, int e, int *palette) {
    short buf[7];

    if (!((def_bestiary_known(index) >> e) & 1)) {
        return state_label(0, palette);
    }
    return state_label(target_class(e, bestiary_resists(rec, buf), rec->id), palette);
}

/* A family's line, from what is learnt of its elements (`known`: the bestiary's bits): Weak as soon as one of them is
 * known to be weak (the line to find); with both known, Resist only when both resist, else neutral; and ??? while a
 * part is not learnt and no weakness is. A hit now teaches a pair whole; only a save from before knows half of one. */
static const unsigned char *family_state(int known, const short *resists, int species, int f, int *palette) {
    int k, e, cls, all_known = 1, all_resist = 1;

    for (k = 0; k < 2 && (e = family_elems[f][k]) >= 0; k++) {
        if (!((known >> e) & 1)) {
            all_known = 0;
            continue;
        }
        cls = target_class(e, resists, species);
        if (cls == 1) {
            return state_label(1, palette);
        }
        all_resist &= cls == 3;
    }
    return state_label(!all_known ? 0 : all_resist ? 3 : 2, palette);
}

const unsigned char *def_inspect_family_state(const short *resists, int f, int *palette) {
    int species = fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon;

    return family_state(inspect_known(species), resists, species, f, palette);
}

const unsigned char *def_bestiary_family_state(FightstgEnemyRecord *rec, int index, int f, int *palette) {
    short buf[7];

    return family_state(def_bestiary_known(index), bestiary_resists(rec, buf), rec->id, f, palette);
}

/* The HP a battle gives `species` (wfightmn_init_members): the three phases of Galacticmon, the last boss, get
 * DEF_FINAL_HP_PCT of the disc's so the fight lasts (a Dark build took the first and third phases in one cast). */
int def_boss_hp(int species, int hp) {
    long v;

    if (!def_on() || (species != 442 && species != 466 && species != 467)) {
        return hp;
    }
    v = (long)hp * DEF_FINAL_HP_PCT / 100;
    return v > 32000 ? 32000 : (int)v; /* the battle's HP is a short */
}

/* ---- The enemy table. Applied each time the game asks for the record (idempotent), so it holds after a reload
 * of SDIGIEDT and a load state alike. */
void def_enemy_override(FightstgEnemyRecord *rec) {
    int i, e;

    if (!def_on()) { /* a replay script keeps the disc's enemies (its state is the emulator's) */
        return;
    }

    for (i = 0; i < def_enemy_row_count; i++) {
        if (def_enemy_rows[i].id == rec->id) {
            for (e = 0; e < 7; e++) {
                rec->resists[e] = def_enemy_rows[i].resists[e];
            }
            return;
        }
    }
}

/* ---- Turns, stagger, the chain, the free swap (the rules are in definitive_mask.c's DefChain). Everything here
 * is the game's side of it. A replay script (shared with the emulator) keeps the original rules: def_on is 0 under
 * --script unless DW3_DEFINITIVE is set (the tests that drive the new rules by script). */

int def_on(void) {
    static int on = -1;

    if (on < 0) {
        on = !port_script_active || getenv("DW3_DEFINITIVE") != NULL;
    }
    return on;
}

extern s32 wfightmn_update_idle_anim(u8 side, s32 amount);

static unsigned int def_evo_seen_bits[DEF_EVO_SEEN_WORDS];

void def_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, chain);
    PORT_STATE_VAR(s, baton_paid);
    PORT_STATE_VAR(s, resist_mod);
    PORT_STATE_VAR(s, resist_turns);
    PORT_STATE_VAR(s, mod_level);
    PORT_STATE_VAR(s, mod_turns);
    def_bestiary_savestate(s);
    def_hunt_savestate(s);
    def_rush_savestate(s); /* the floating Rush's panel and the field's heap it points into */
    def_flee_ask_savestate(s); /* the escape's question and the battle's heap it points into */
    PORT_STATE_VAR(s, hit_class);
    PORT_STATE_VAR(s, hit_elem);
    PORT_STATE_VAR(s, hit_id);
    PORT_STATE_VAR(s, overfinish_hit);
    PORT_STATE_VAR(s, overfinish_damage);
    PORT_STATE_VAR(s, records_state.gauges);
    PORT_STATE_VAR(s, def_evo_seen_bits);
    PORT_STATE_VAR(s, blast_cursed);
    PORT_STATE_VAR(s, blast_fallen);
    PORT_STATE_VAR(s, blast_step_taken);
    PORT_STATE_VAR(s, chosen_used);
    PORT_STATE_VAR(s, overheat_sealed);
    PORT_STATE_VAR(s, boost_logged);
    PORT_STATE_VAR(s, plug_last_stand_used);
    def_dri_savestate(s); /* a battle loaded mid-DRI-simulation is still one */
    def_purify_savestate(s); /* Blast phase 2: and mid-purification */
    def_finale_savestate(s); /* Extended Final Act & Post-Credits Farewell */
    def_superhunt_screen_savestate(s); /* the Sepikmon's board and the hunts' reward panel */
    def_respec_savestate(s); /* the pig's stat swap */
    def_midi_savestate(s);   /* Etemon's list */
    def_recolor_savestate(s); /* which model is the rare foe's, for its palette */
    def_persona_savestate(s); /* the enemy personas: who is fought, each slot's phase and what was learnt */
    def_skin_savestate(s);
    port_girls_savestate(s); /* what the hero's files in memory were painted for (Kaili, the skin) */
    def_guide_savestate(s); /* the Guide's screen and notice, with the field's heap they point into */
    def_bm_savestate(s);    /* the Black Market's screen, the same way */
    def_recipe_savestate(s); /* the open shop, whose regions' recipes it shows */
    dw3_velvet_savestate(s); /* the Velvet Room: the map is the room, and its shopkeeper's turn */
    def_travel_savestate(s);  /* the Guardromon's screen, the same way as the market's */
    def_evohint_savestate(s);
    def_callout_savestate(s);
    def_story_savestate(s); /* the story hooks' talk, with the field's heap it lives in */
    def_time_savestate(s);  /* the Time Room's run */
    PORT_STATE_VAR(s, plug_last_stand_slot); /* appended at tail so 0.8.5 savestates keep earlier layout */
    PORT_STATE_VAR(s, guard_hit);
}

static int target_slot(void) {
    return fightstg_battle.state.current[1];
}

/* The current target is open: its pose is the tired one, and the damage multiplier gives the player's next blow its
 * bonus. */
int def_target_staggered(void) {
    return def_on() && chain.staggered[target_slot() % 3];
}

/* A party turn begins (wfightmn_party_turn): an extra action keeps the window, a normal one starts a new one. */
void def_party_turn_begins(void) {
    int m, k;

    if (!def_on()) {
        return;
    }
    m = fightstg_battle.state.current[0] % 3;
    if (resist_turns[0][m] != 0 && --resist_turns[0][m] == 0) { /* an enemy's Resist Down on us wears off */
        memset(resist_mod[0][m], 0, sizeof(resist_mod[0][0]));
    }
    def_chain_party_turn(&chain);
    if (!chain.cur_extra) {
        def_blast_overheat_turn(m);
        for (k = 0; k < 3; k++) {
            if (mod_turns[0][m][k] != 0 && --mod_turns[0][m][k] == 0) {
                mod_level[0][m][k] = 0;
                fightstg_battle.state.members[0][m].modifiers[k] = 0;
                if (log_on()) {
                    LOG("mod expired side 0 slot %d kind %d", m, k);
                }
            }
        }
    }
    if (log_on()) {
        LOG("window %s", chain.cur_extra ? "extra action" : "new (normal action)");
    }
}

/* ---- Blast Gauge, phase 1: the acting Digimon's gauge (records_state.gauges, by Digimon, as the game keeps it). */
static FightstgMember *blast_member(void) {
    return &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
}

int def_blast_gain(int weak, int staggered, int overfinish, int extra) {
    return (weak ? DEF_BLAST_WEAK : 0) + (staggered ? DEF_BLAST_STAGGER : 0) + (overfinish ? DEF_BLAST_OVERFINISH : 0) +
           (extra ? DEF_BLAST_EXTRA : 0);
}

/* Moves the acting Digimon's gauge by `amount` within 0..DEF_GAUGE_MAX. A Digimon in its Blast form gains nothing (the
 * game's rule: its gauge is spent, and wfightmn_end_blast empties it). */
static void blast_add(int amount, const char *why) {
    FightstgMember *m = blast_member();
    s32 id = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);
    int was;

    if (amount == 0 || id < 0 || id >= DEF_GAUGE_SLOTS || m->blasted || def_blast_chosen_mode()) {
        return; /* after the quest the gauge is gone: the Chosen Evolution has a charge instead */
    }
    was = records_state.gauges[id];
    records_state.gauges[id] = (s16)def_gauge_clamp(was + amount);
    if (log_on()) {
        LOG("blast gauge %s digimon %d %d -> %d", why, (int)id, was, (int)records_state.gauges[id]);
    }
}

/* ---- The dark forms in sequence, their record, and the Chosen Evolution (owner's redesign, 2026-11-17). */


int def_blast_chosen_mode(void) {
    return def_on() && def_record_blast_quest() == DEF_BLAST_QUEST_DONE;
}

int def_blast_step(int next, int level, int *advance) {
    static const int levels[DEF_BLAST_STEPS] = DEF_BLAST_STEP_LEVELS;

    next = next < 0 || next >= DEF_BLAST_STEPS ? 0 : next;
    if (level >= levels[next]) { /* its level reaches the next form: it takes it, and the sequence moves on */
        *advance = (next + 1) % DEF_BLAST_STEPS;
        return next;
    }
    *advance = next; /* not yet: the last form it reached (next > 0 here, the first form's level is 1) */
    return next - 1;
}

int def_blast_top_step(int level) {
    static const int levels[DEF_BLAST_STEPS] = DEF_BLAST_STEP_LEVELS;
    int k = DEF_BLAST_STEPS - 1;

    while (k > 0 && level < levels[k]) {
        k--;
    }
    return k;
}

int def_blast_form(int partner, int band) {
    static const unsigned char megas[DEF_BLAST_PARTNERS] = DEF_CHOSEN_MEGAS;
    const char *force = getenv("DW3_BLAST_STEP"); /* tests: this step for every Blast (the sequence stays) */
    int level, k, advance;

    (void)band;
    blast_step_taken = 0xFF;
    if (!def_on() || partner < 0 || partner >= DEF_BLAST_PARTNERS) {
        return 0;
    }
    if (def_blast_chosen_mode()) {
        return megas[partner];
    }
    level = gamestate_data.funcs.get_record(partner)->stats.values[0];
    k = def_blast_step(def_record_blast_next(partner), level, &advance);
    if (force != NULL && force[0] != '\0') {
        k = atoi(force) % DEF_BLAST_STEPS;
    } else if (plug_last_stand_slot >= 0 && plug_last_stand_slot == fightstg_battle.state.current[0] &&
               fightstg_battle.state.members[0][plug_last_stand_slot].base_digimon == records_digimon[partner].id) {
        /* The last stand (owner, 2026-11-21): the Rookie the DV Plug got back up with 1 HP and 1 MP so it can Blast takes
         * the strongest dark form its level reaches, not the sequence's next (which can be the Champion's); the
         * sequence is left where it was, so the Gennai quest's forms still come in turn. */
        k = def_blast_top_step(level);
        plug_last_stand_slot = -1;
    } else {
        def_record_blast_set_next(partner, advance);
    }
    blast_step_taken = (unsigned char)k;
    port_log("blast: digimon %d at level %d takes step %d (next %d)", partner, level, k, def_record_blast_next(partner));
    return records_digimon[partner].blast_forms[k];
}

/* The form's record: the first time a partner+form pair is taken (while the quest has room) its stats as transformed and
 * its strongest one; any later time, the best that stat reached. */
static void blast_record(s32 id, int k) {
    FightstgMember *m = blast_member();
    FightstgStats *st;
    DefBlastEntry e;
    int i, j;

    boost_off = 1; /* the record keeps the form's own stats: the boost is bought on top */
    st = fightstg_rules.get_stats(0, 0, fightstg_battle.state.current[0]);
    boost_off = 0;

    if (k < 0 || k >= DEF_BLAST_STEPS || st == NULL) {
        return;
    }
    i = def_record_blast_find(id, k);
    if (i >= 0) {
        def_record_blast_raise_best(i, st->stats[def_record_blast_entry(i)->best]);
        return;
    }
    memset(&e, 0, sizeof(e));
    e.partner = (unsigned char)id;
    e.step = (unsigned char)k;
    for (j = 0; j < 5; j++) {
        e.stats[j] = st->stats[j];
        if (st->stats[j] > e.stats[e.best]) {
            e.best = (unsigned char)j;
        }
    }
    e.hp = m->max_hp;
    e.best_value = e.stats[e.best];
    i = def_record_blast_add(&e);
    port_log("blast: form recorded: entry %d, digimon %d step %d, stats %d %d %d %d %d, HP %d, best %d", i, (int)id, k,
             e.stats[0], e.stats[1], e.stats[2], e.stats[3], e.stats[4], e.hp, e.best);
}

void def_blast_started(s32 id) {
    FightstgMember *m = blast_member();

    if (!def_on() || id < 0 || id >= DEF_GAUGE_SLOTS) {
        return;
    }
    if (def_blast_chosen_mode()) { /* the Chosen Evolution: no curse; it spends the charge and restores all */
        chosen_used = 1;
        def_record_set_chosen_charge(0);
        m->mp = m->max_mp; /* the scene restores the HP (wfightmn_blast) */
        port_log("blast: digimon %d took its Chosen Evolution", (int)id);
        return;
    }
    if (blast_blessed()) {
        blessed_now = 1;
        def_super_blast_note();
        def_record_set_story(DEF_STORY_BLAST_FIRST);
        port_log("blast: digimon %d took the blessed Blast", (int)id);
    } else {
        GamestateRecord *r = gamestate_data.funcs.get_record(id);
        int cost = (r != NULL && r->stats.values[1] > 0)
                       ? (r->stats.values[1] < DEF_BLAST_PLUG_TP ? r->stats.values[1] : DEF_BLAST_PLUG_TP)
                       : 0;

        if (cost > 0) {
            r->stats.values[1] -= (s16)cost;
            def_record_blast_owe(id, cost); /* Gennai's quest gives it back */
            port_log("blast: digimon %d paid %d TP (%d left)", (int)id, cost, r->stats.values[1]);
        }
        blast_cursed |= 1u << id;
    }
    blast_record(id, blast_step_taken == 0xFF ? -1 : blast_step_taken);
    port_log("blast: digimon %d took its Blast form (step %d, %d forms recorded)", (int)id,
             blast_step_taken == 0xFF ? -1 : blast_step_taken, def_record_blast_count());
}

int def_blast_dark(void) {
    if (def_finale_phase() == DEF_FINALE_PHASE4_SUPER_CHOSEN) {
        return 0;
    }
    return def_on() && !def_blast_chosen_mode() && blast_member()->blasted;
}

int def_blast_boost_pct(void) {
    if (def_blast_chosen_mode()) {
        return DEF_CHOSEN_BOOST_PCT;
    }
    return def_record_blast_boost() * DEF_BLAST_BOOST_STEP + ((blessed_now || def_super_tutorial_battle()) ? DEF_SUPER_TUTORIAL_BOOST_PCT : 0);
}

/* fightstg_rules_get_stats, the party's side: a member in its Blast or Chosen form fights with the boost on its five
 * stats. Only the Chosen goes past 999 (the owner: it is the one power that breaks the cap; the last bosses are scaled
 * for it), up to 9999 to keep the damage formulas in range; the Blast's boost stops at the game's 999 (a stat already
 * past it keeps its value). */
void def_blast_boost_stats(int member, FightstgStats *out) {
    FightstgMember *m = &fightstg_battle.state.members[0][member];
    int pct, i, v;

    if (def_finale_golden_mode()) {
        return;
    }
    if (!def_on() || boost_off || !m->blasted || def_purify_fighting()) {
        return;
    }
    /* The blessed Blast (the tutorial's, or the game's first): every stat at the 999 cap (owner, 2026-11-20: a +50% was
     * too weak, and a tutorial must not be lost). */
    if (!def_blast_chosen_mode() && (blessed_now || def_super_tutorial_battle())) {
        for (i = 0; i < 5; i++) {
            if (out->stats[i] < 999) {
                out->stats[i] = 999;
            }
        }
        if (!(boost_logged >> member & 1) && member >= 0 && member < 3) {
            boost_logged |= (unsigned char)(1u << member);
            port_log("blast: slot %d fights with the blessed Blast, every stat at 999", member);
        }
        return;
    }
    pct = def_blast_boost_pct();
    if (pct > 0 && member >= 0 && member < 3 && !(boost_logged >> member & 1)) {
        boost_logged |= (unsigned char)(1u << member);
        port_log("blast: slot %d fights with its boost +%d%% (stat 0 %d -> %d)", member, pct, out->stats[0],
                 out->stats[0] + out->stats[0] * pct / 100);
    }
    for (i = 0; i < 5 && pct > 0; i++) {
        int cap = def_blast_chosen_mode() ? 9999 : 999;

        v = out->stats[i] + out->stats[i] * pct / 100;
        if (v > cap) {
            v = out->stats[i] > cap ? out->stats[i] : cap;
        }
        out->stats[i] = (s16)v;
    }
}

/* A Blast or Chosen form ends (wfightmn_end_blast, the timer's end or an enemy's revert): the Digimon is back in its
 * Rookie form (wfightmn_blast_end asks def_blast_end_form) and overheats, sealed from digivolving (and from the Baton
 * Pass's digivolution, which the game's seal refuses too) for DEF_BLAST_OVERHEAT_TURNS of its own turns. */
void def_blast_overheat(s32 slot) {
    FightstgMember *m;
    s32 id;

    if (!def_on() || slot < 0 || slot >= 3) {
        return;
    }
    m = &fightstg_battle.state.members[0][slot];
    id = gamestate_data.funcs.get_party_member(slot);
    if (id >= 0 && id < DEF_BLAST_PARTNERS) {
        def_record_set_heat(id, DEF_BLAST_OVERHEAT_TURNS);
    }
    overheat_sealed[slot] = !(m->status & 0x20); /* a seal already on is the game's: its own event lifts it */
    m->status |= 0x20;
    port_log("blast: slot %d overheats, no digivolving for %d turns (into the next battles, until an inn)", (int)slot,
             DEF_BLAST_OVERHEAT_TURNS);
}

/* A battle begins: a partner still overheated from an earlier Blast or Chosen starts it sealed, in its Rookie form
 * (the party starts in the base form), and only its own turns count the seal down. */
void def_blast_heat_begin(void) {
    int slot;

    if (!def_on()) {
        return;
    }
    for (slot = 0; slot < 3; slot++) {
        FightstgMember *m = &fightstg_battle.state.members[0][slot];
        s32 id = gamestate_data.funcs.get_party_member(slot);

        if (id < 0 || id >= DEF_BLAST_PARTNERS || m->digimon == 0 || m->hp == 0 || def_record_heat(id) == 0) {
            continue;
        }
        overheat_sealed[slot] = !(m->status & 0x20);
        m->status |= 0x20;
        port_log("blast: slot %d starts overheated, %d sealed turns left", slot, def_record_heat(id));
    }
}

int def_blast_heated(s32 id) {
    return def_on() && def_record_heat(id) != 0;
}

/* The inn's rest (Gatomon) cools the whole party; Gennai cools the Chosen Digimon (definitive_purify.c). */
void def_blast_cool_party(void) {
    int slot;

    for (slot = 0; slot < 3; slot++) {
        s32 id = gamestate_data.funcs.get_party_member(slot);

        if (id >= 0 && id < DEF_BLAST_PARTNERS) {
            def_record_set_heat(id, 0);
        }
    }
}

s32 def_blast_end_form(s32 slot, s32 form) {
    s32 id = gamestate_data.funcs.get_party_member(slot);

    if (!def_on() || id < 0 || id >= DEF_GAUGE_SLOTS) {
        return form;
    }
    return records_digimon[id].id;
}

/* The slot's own turn begins (a normal action): one sealed turn spent; the turn after the last one lifts the seal. */
void def_blast_overheat_turn(int slot) {
    FightstgMember *m = &fightstg_battle.state.members[0][slot];
    s32 id = gamestate_data.funcs.get_party_member(slot);
    int left = id >= 0 && id < DEF_BLAST_PARTNERS ? def_record_heat(id) : 0;

    if (left != 0) {
        def_record_set_heat(id, left - 1);
        port_log("blast: slot %d overheated, %d turns left after this one", slot, left - 1);
    } else if (overheat_sealed[slot]) {
        overheat_sealed[slot] = 0;
        if (m->hp > 0) {
            m->status &= ~0x20;
        }
        port_log("blast: slot %d cooled down", slot);
    }
}

void def_blast_price(void) {
    static const int stat_of[3] = { 0, 1, 4 }; /* penalties[j] comes off stats[stat_of[j]] (gamestate_get_stats) */
    GamestateRecord *r;
    int id, j, p;

    if (!def_on()) {
        blast_cursed = 0;
        return;
    }
    for (id = 0; id < DEF_GAUGE_SLOTS; id++) {
        if (!(blast_cursed & (1u << id))) {
            continue;
        }
        r = gamestate_data.funcs.get_record(id);
        r->stats.values[2] = 1;
        r->stats.values[4] = 1;
        for (j = 0; j < 3; j++) {
            p = r->stats.stats[stat_of[j]] * DEF_BLAST_CURSE_PCT / 100;
            if (p > r->stats.penalties[j]) { /* a second Blast before the inn does not deepen it */
                r->stats.penalties[j] = (s16)p;
            }
        }
        port_log("blast: digimon %d pays: HP and MP 1, penalties %d %d %d", id, r->stats.penalties[0],
                 r->stats.penalties[1], r->stats.penalties[2]);
        if (log_on()) {
            LOG("blast price digimon %d penalties %d %d %d", id, r->stats.penalties[0], r->stats.penalties[1],
                r->stats.penalties[2]);
        }
    }
    blast_cursed = 0;
}

extern void fightstg_events_add_gauge_full(void);

/* After the quest the DV Plug revives each fallen partner in its Rookie form with 1 HP and 1 MP (the owner: one who
 * fell after spending the Chosen stays a weak Rookie; one who still has it can take it and be healed). The item runs
 * the game's revive path (item 0x46, fightstg_8008D3B4.c), which calls this per fallen slot and writes the HP returned.
 * A fallen member keeps the form it fell in, so the Rookie is set here, with the Blast flag off. */
int def_blast_plug_revive(int slot) {
    FightstgMember *m = &fightstg_battle.state.members[0][slot];
    s32 id = gamestate_data.funcs.get_party_member(slot);

    if ((!def_blast_chosen_mode() && !def_super_tutorial_battle()) || id < 0 || id >= DEF_GAUGE_SLOTS ||
        m->digimon == 0 || m->hp != 0) {
        return 0;
    }
    m->digimon = records_digimon[id].id;
    m->blasted = 0;
    m->power_up = 0;
    m->mp = 1;
    overheat_sealed[slot] = 0; /* the knockout cleared its statuses, the seal with them: back on if it is still hot */
    if (id < DEF_BLAST_PARTNERS && def_record_heat(id) != 0) {
        overheat_sealed[slot] = 1;
        m->status |= 0x20;
    }
    port_log("blast: the DV Plug revives digimon %d in its Rookie form, HP and MP 1", (int)id);
    return 1;
}

/* Global safety net (once per battle): when the last party member falls and a DV Plug is in the bag (after Ogremon
 * unlocks Blast, outside Purification), spend 1 DV Plug to revive that last Digimon in Rookie form with 1 HP, 1 MP,
 * and a full Blast gauge so the player gets a turn to act. */
int def_blast_plug_last_stand(int slot) {
    FightstgMember *m;
    s32 id;

    /* Not in the finale's Phase 2 either: that defeat is the script's (def_finale_damage makes sure of it), so a
     * revive there only spends the Plug for one more turn. A DRI duel keeps it: 1 HP can still turn a duel. */
    if (!def_on() || def_purify_fighting() || def_finale_phase() == DEF_FINALE_PHASE2_DARK_LOSS || !blast_unlocked() ||
        plug_last_stand_used || gamestate_data.items[DEF_PLUG_ITEM] <= 0 || slot < 0 || slot >= 3) {
        return 0;
    }
    m = &fightstg_battle.state.members[0][slot];
    id = gamestate_data.funcs.get_party_member(slot);
    if (id < 0 || id >= DEF_GAUGE_SLOTS || m->digimon == 0 || m->hp != 0) {
        return 0;
    }
    plug_last_stand_used = 1;
    plug_last_stand_notice = 1;
    plug_last_stand_slot = (signed char)slot;
    gamestate_data.items[DEF_PLUG_ITEM]--;
    m->digimon = records_digimon[id].id;
    m->blasted = 0;
    m->power_up = 0;
    m->hp = 1;
    m->mp = 1;
    overheat_sealed[slot] = 0;
    if (id < DEF_BLAST_PARTNERS && def_record_heat(id) != 0) {
        overheat_sealed[slot] = 1;
        m->status |= 0x20;
    }
    if (!def_blast_chosen_mode()) {
        records_state.gauges[id] = DEF_GAUGE_MAX;
    }
    port_log("blast: last-stand DV Plug revives digimon %d in Rookie form (1 HP, 1 MP, gauge full; %d left)",
             (int)id, gamestate_data.items[DEF_PLUG_ITEM]);
    return 1;
}

int def_blast_plug(void) {
    FightstgMember *m = blast_member();
    s32 id = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);

    /* after the quest the Plug is a revive item instead (def_blast_plug_revive, through the game's item 0x46 path) */
    if (!def_on() || id < 0 || id >= DEF_GAUGE_SLOTS || m->hp <= 0 || def_purify_fighting() ||
        def_blast_chosen_mode() || !blast_unlocked()) {
        return 0;
    }
    /* the statuses that refuse a Blast leave the Plug the game's own (+gauge, capped under full) */
    if (m->blasted || (m->status & (8 | 0x20))) {
        return 0;
    }
    records_state.gauges[id] = DEF_GAUGE_MAX;
    fightstg_events_add_gauge_full(); /* wfightmn_blast -> def_blast_started takes the TP and records the form */
    port_log("blast: the DV Plug fills gauge for digimon %d", (int)id);
    if (log_on()) {
        LOG("blast plug digimon %d", (int)id);
    }
    return 1;
}

int def_blast_auto(void) {
    return !def_on();
}

/* The Chosen Evolution's conditions but the Rookie form: the acting Digimon is the one Gennai chose, at the level, with
 * the charge, and none taken this battle. */
static int chosen_could(void) {
    FightstgMember *m = blast_member();
    s32 id = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);

    return def_blast_chosen_mode() && id >= 0 && id == def_record_chosen() &&
           gamestate_data.funcs.get_record(id)->stats.values[0] >= DEF_CHOSEN_LEVEL && def_record_chosen_charge() &&
           !chosen_used && !m->blasted && m->hp > 0 && !(m->status & (8 | 0x20)) && !def_purify_fighting();
}

int def_blast_ready(void) {
    FightstgMember *m = blast_member();
    s32 id = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);

    if (def_blast_chosen_mode()) { /* only from the Rookie form */
        return chosen_could() && m->digimon == (s16)records_digimon[id].id;
    }
    /* the statuses that refuse Digivolve refuse the Blast too (8: no Fight/Tech/DV; 0x20: no digivolving) */
    /* and no Blast in a purification: the party faces its dark forms without them */
    if (!blast_unlocked()) {
        return 0;
    }
    return def_on() && id >= 0 && id < DEF_GAUGE_SLOTS && records_state.gauges[id] >= DEF_GAUGE_MAX && !m->blasted &&
           m->hp > 0 && !(m->status & (8 | 0x20)) && !def_purify_fighting() &&
           (blast_blessed() || gamestate_data.funcs.get_record(id)->stats.values[1] >= DEF_BLAST_PLUG_TP);
}

/* The temptation (owner's brief: "the temptation to use it at critical moments"): the gauge is full and the moment is
 * critical, a partner fell this battle or the acting Digimon is at DEF_BLAST_TEMPT_HP_PCT of its HP or less. The battle
 * menu then pulses the Blast button and says so; nothing is forced. None after the quest: the curse is gone. */
int def_blast_tempt(void) {
    FightstgMember *m = blast_member();

    return !def_blast_chosen_mode() && def_blast_ready() &&
           (blast_fallen || m->hp * 100 <= m->max_hp * DEF_BLAST_TEMPT_HP_PCT);
}

/* Why the grey Blast button refuses (the box a press on it opens in the battle menu), NULL when there is nothing to
 * tell (it is ready, before/during the Ogremon tutorial, KO, stunned, purification, or the finale's own rules). */
const unsigned char *def_chosen_refusal(void) {
    extern const unsigned char def_str_ChosenNoPartner[], def_str_ChosenUsed[], def_str_ChosenNoCharge[],
        def_str_ChosenNoLevel[], def_str_ChosenNoRookie[], def_str_BlastNoGauge[], def_str_BlastOverheated[],
        def_str_BlastNoTp[];
    FightstgMember *m = blast_member();
    s32 id = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);

    if (def_finale_active() || id < 0 || id >= DEF_GAUGE_SLOTS || def_blast_ready() || m->hp <= 0 ||
        (m->status & 8) || def_purify_fighting()) {
        return NULL;
    }
    if (!def_blast_chosen_mode()) {
        if (def_super_done() <= DEF_SUPER_TUTORIAL || m->blasted) {
            return NULL;
        }
        if (m->status & 0x20) {
            return def_str_BlastOverheated;
        }
        if (records_state.gauges[id] < DEF_GAUGE_MAX) {
            return def_str_BlastNoGauge;
        }
        return (!blast_blessed() && gamestate_data.funcs.get_record(id)->stats.values[1] < DEF_BLAST_PLUG_TP)
                   ? def_str_BlastNoTp
                   : NULL;
    }
    if (m->status & 0x20) {
        return NULL;
    }
    if (id != def_record_chosen()) {
        return def_str_ChosenNoPartner;
    }
    if (chosen_used || m->blasted) {
        return def_str_ChosenUsed;
    }
    if (!def_record_chosen_charge()) {
        return def_str_ChosenNoCharge;
    }
    if (gamestate_data.funcs.get_record(id)->stats.values[0] < DEF_CHOSEN_LEVEL) {
        return def_str_ChosenNoLevel;
    }
    return def_str_ChosenNoRookie;
}

const unsigned char *def_blast_tempt_label(void) {
    extern const unsigned char def_str_BlastTempt[];
    return def_str_BlastTempt;
}

const unsigned char *def_battle_menu_notice(void) {
    extern const unsigned char def_str_PlugLastStand[];

    if (plug_last_stand_notice) {
        plug_last_stand_notice = 0;
        return def_str_PlugLastStand;
    }
    return NULL;
}

void def_blast_command(s32 command) {
    if (!def_on()) {
        return;
    }
    if (command == 1 || command == 3) { /* Run, Item: the passive ones */
        blast_add(-DEF_BLAST_COOL, command == 1 ? "run" : "item");
    } else if (command == DEF_BLAST_COMMAND && log_on()) {
        LOG("blast set off digimon %d", (int)gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]));
    }
}

/* The player's action was resolved on the current target (wfightmn, before the pose is set): `value` > 0 is a hit. */
void def_action_resolved(u8 side, s32 id, s32 value) {
    int target = target_slot();
    int weak = hit_class == 1 && hit_id == id;
    int was;

    if (!def_on() || side != 0) {
        return;
    }
    if (value <= 0) {
        guard_hit = 0;
        hit_class = 0;
    }
    overfinish_hit = 0; /* every party blow decides it afresh */
    if (value > 0 && fightstg_battle.state.members[1][target].hp <= 0) { /* it was already down: an OVERFINISH */
        overfinish_hit = 1;
        overfinish_damage += value;
        if (log_on()) {
            LOG("overfinish damage %d total %d", (int)value, (int)overfinish_damage);
        }
    }
    was = chain.staggered[target];
    if (value > 0) {
        blast_add(def_blast_gain(weak, was, overfinish_hit, chain.cur_extra), "hit");
    }
    def_chain_player_hit(&chain, target, value > 0, weak ? hit_elem : -1, weak);
    if (value > 0 && hit_id == id && (hit_class == 1 || hit_class == 3)) { /* a weakness or a resistance found out */
        if (def_persona_hides_species(persona_at(target % 3))) { /* an encounter's own map: learnt for it alone */
            def_persona_learn(target % 3, def_family_bits(hit_elem));
        } else {
            def_bestiary_discover(def_bestiary_index(fightstg_battle.state.members[1][target].digimon),
                                  def_family_bits(hit_elem)); /* a pair is one affinity: both of it are learnt */
        }
    }
    if (!was && chain.staggered[target] && def_persona_staggered(target % 3)) { /* ON_STAGGER: a new phase */
        persona_phase_changed(target % 3);
    }
    if (log_on() && value > 0) {
        LOG("hit target %d weak %d stagger %d -> %d extra_pending %d", target, weak, was, chain.staggered[target],
            chain.extra_pending);
    }
}

/* An enemy's turn begins (wfightmn_run_events, event 3): it closes. */
void def_enemy_turn_begins(void) {
    int target = target_slot();
    int slot = target % 3;
    int k;

    if (!def_on()) {
        return;
    }
    persona_at(slot); /* looked up before its turn counts */
    if (def_persona_enemy_turn(slot)) { /* PHASES(n): its turns in this phase ran out */
        persona_phase_changed(slot);
    }
    if (resist_turns[1][slot] != 0 && --resist_turns[1][slot] == 0) { /* the lowered resists wear off */
        memset(resist_mod[1][slot], 0, sizeof(resist_mod[1][0]));
        if (log_on()) {
            LOG("resist down worn off slot %d", target);
        }
    }
    for (k = 0; k < 3; k++) {
        if (fightstg_battle.state.type == 6 && fightstg_battle.state.final_phase != 0 && k == 1) {
            continue; /* final boss scripted phase owns modifiers[1] */
        }
        if (mod_turns[1][slot][k] != 0 && --mod_turns[1][slot][k] == 0) {
            mod_level[1][slot][k] = 0;
            fightstg_battle.state.members[1][slot].modifiers[k] = 0;
            if (log_on()) {
                LOG("mod expired side 1 slot %d kind %d", slot, k);
            }
        }
    }
    if (chain.staggered[target]) {
        def_chain_enemy_acts(&chain, target);
        wfightmn_update_idle_anim(0x10, 0); /* back to its normal pose */
        if (log_on()) {
            LOG("stagger closed by the enemy's turn (slot %d)", target);
        }
    }
}

/* About to take the next event of the queue (wfightmn_run_events): an owed extra action makes the party's turn the next. */
void def_events_before_take(void) {
    int e;

    if (!def_on() || !def_chain_take_extra(&chain)) {
        return;
    }
    e = fightstg_events.find(2);
    if (e >= 0) {
        fightstg_events.events[e].delay = 0;
        if (log_on()) {
            LOG("extra action granted");
        }
    } else {
        chain.next_extra = 0;
    }
}

void def_baton_paid(void) {
    baton_paid = 1;
}

const unsigned char *def_baton_label(void) {
    extern const unsigned char def_str_BatonPass[];
    return def_str_BatonPass;
}

/* The player chose Tag (party command 5). Owner's rule (2026-11-12): only Baton Pass, offered only while the enemy is
 * open (and once a window), keeps the turn; a plain Switch spends it, unless Relay made the next swap free. */
void def_swap_chosen(void) {
    if (!def_on()) {
        return;
    }
    if (baton_paid) {
        baton_paid = 0;
        def_chain_swap_chosen(&chain, target_slot()); /* spends the window's swap, owes the extra action */
        if (log_on()) {
            LOG("baton pass");
        }
        return;
    }
    if (chain.relay) {
        def_chain_swap_chosen(&chain, target_slot());
        if (log_on()) {
            LOG("relay swap");
        }
    }
}

/* Baton Pass is offered: the enemy is open and this window's swap is unspent (the HUD's Tag picture shows it). */
int def_swap_free(void) {
    int t = target_slot() % 3;

    return def_on() && chain.staggered[t] && !chain.swap_used;
}

/* The pose of a side (wfightmn_update_idle_anim): the tired one for an open enemy, never for the player (HP no longer
 * decides it); the final boss's is its phase or an open one. -1: the original rule holds. */
int def_idle_pose(u8 side) {
    if (!def_on()) {
        return -1;
    }
    if (side != 0x10) {
        return 0;
    }
    if (fightstg_battle.state.type == 6) {
        return fightstg_battle.state.final_phase != 0 || chain.staggered[target_slot() % 3];
    }
    return chain.staggered[target_slot() % 3];
}

/* ---- The remade techniques. Libra (202, was Speed Up) learns the target's weaknesses, Relay (203, was Mega Boost)
 * makes the next swap free, Resist Down / Mega Break (207, 208, were Slow Down / Mega Break) lower the elements the
 * user is best at on the target for a few of its turns; on a boss that blocked the speed-down they replace
 * (records_state.blocked[10]) they are scaled by DEF_BOSS_DEBUFF_PCT, like every stat debuff. The effects run in fightstg_tech_update (class 5), before the original's own. */

void def_apply_resist_mod(int side, int member, FightstgStats *out) {
    int e;

    side = side != 0;
    if (!def_on() || member < 0 || member > 2 || resist_turns[side][member] == 0) {
        return;
    }
    for (e = 0; e < 7; e++) {
        out->resists[e] += resist_mod[side][member][e];
        if (out->resists[e] < 1) {
            out->resists[e] = 1; /* the old formulas divide by it */
        }
    }
}

/* Bits 0..6 of the elements whose resistance is currently lowered on `side` (0 party, 1 enemy), `slot` (0..2). */
int def_resist_down_mask(int side, int slot) {
    int e, mask = 0;

    side = side != 0;
    if (!def_on() || slot < 0 || resist_turns[side][slot % 3] == 0) {
        return 0;
    }
    slot %= 3;
    for (e = 0; e < 7; e++) {
        if (resist_mod[side][slot][e] < 0) {
            mask |= 1 << e;
        }
    }
    return mask;
}

/* 0: not one of the four; 1: done, `*msg` the message (0 Libra, 1 Relay, 2 resist lowered); 2: no effect (immune). */
int def_tech_effect(u8 side, int tech, int *msg) {
    int enemy = side != 0;
    int victim = enemy ? 0 : 1;                                 /* the side whose resists go down */
    int slot = fightstg_battle.state.current[victim] % 3;
    int e, n, best, k, delta;
    short taken[7] = { 0, 0, 0, 0, 0, 0, 0 };
    FightstgStats *me;

    if (!def_on() || (tech != 202 && tech != 203 && tech != 207 && tech != 208)) {
        return 0;
    }
    if (!enemy && tech == 202) {
        if (def_persona_hides_species(persona_at(slot))) { /* an encounter's own map: Libra reads it alone */
            def_persona_learn(slot, 0x7F);
        } else {
            def_bestiary_discover(def_bestiary_index(fightstg_battle.state.members[1][slot].digimon), 0x7F);
        }
        *msg = 0;
        if (log_on()) {
            LOG("libra slot %d", slot);
        }
        return 1;
    }
    if (!enemy && tech == 203) {
        def_chain_relay_set(&chain);
        *msg = 1;
        return 1;
    }
    /* Resist Down / Mega Break, and in an enemy's hands all four (owner's choice: Libra and Relay mean nothing to an
     * enemy): the target's resists of the user's best families go down, both elements of a pair (owner, 2026-11-13: a
     * pair is one affinity). The best is the user's highest affinity as the multiplier counts it (def_affinity: a pair's
     * sum), ties by the family order. On the player they are its affinities. */
    me = fightstg_rules.get_stats(side, 1, fightstg_battle.state.current[enemy]);
    delta = enemy ? (tech == 208 ? DEF_BREAK_MEGA_DELTA : DEF_BREAK_SLOW_DELTA)
                  : def_debuff_scale(10, tech == 208 ? DEF_BREAK_MEGA_DELTA : DEF_BREAK_SLOW_DELTA);
    for (n = tech == 208 ? 2 : 1, k = 0; k < n; k++) {
        best = -1;
        for (e = 0; e < 7; e++) {
            if (!taken[e] && def_affinity(e, me->resists) > 0 &&
                (best < 0 || def_affinity(e, me->resists) > def_affinity(best, me->resists))) {
                best = e;
            }
        }
        if (best < 0) {
            break;
        }
        for (e = 0; e < 7; e++) {
            if ((def_family_bits(best) >> e) & 1) {
                taken[e] = 1;
                resist_mod[victim][slot][e] -= delta;
                if (log_on()) {
                    LOG("resist down side %d slot %d element %d now %d", victim, slot, e, resist_mod[victim][slot][e]);
                }
            }
        }
    }
    resist_turns[victim][slot] = DEF_BREAK_TURNS;
    *msg = 2;
    return 1;
}

const unsigned char *def_message(int i) {
    extern const unsigned char def_str_MsgLibra[], def_str_MsgRelay[], def_str_MsgBreak[], def_str_MsgPlugLastStand[];
    static const unsigned char *const m[4] = { def_str_MsgLibra, def_str_MsgRelay, def_str_MsgBreak,
                                               def_str_MsgPlugLastStand };

    return m[i];
}

/* The records the techniques and Digimon forms change (written with the MP rows, every time: the reset restores the table). */
/* The player's elemental physical techniques that patch_techniques makes magic, with their magic power. 88 (WarGreymon's
 * special, Fire) and 103 (Beelzemon's, Dark) had no element as physical blows: patch_techniques gives them one
 * (owner, 2026-11-24). Their power keeps the place the physical blow had among the player's signatures of about the
 * same MP, moved to the same place among the player's magic of that MP (medians, +-20 MP; at equal stats the physical
 * and magic formulas both give 1x the power): WarGreymon's 480 is 1.2x the physical median of 400 at 140 MP, and the magic
 * median there is 490, so 590; Beelzemon's 300 is 0.67x the median 450 at 250 MP, and the magic median there is 750,
 * so 500 by the ranking. His blow strikes 3 times (hit_count 3, kind 9, kept), which the ranking above did not count: the
 * method gives 350 a hit. The owner chose 400 a hit, 1200 in all, a little over the 3-hit magic 171 and Inferno (350 a hit)
 * so it stays the strongest magic of the player, and took Dark away from Galacticmon's second phase (466) to keep the
 * last bosses from falling to it (owner, 2026-11-24). */
static const struct {
    short tech, power;
} to_magic[] = {
    { 53, 100 },  { 66, 200 },  { 68, 210 },  { 71, 220 },  { 72, 150 },  { 82, 430 },  { 95, 440 },
    { 98, 630 },  { 88, 590 },  { 103, 400 }, { 69, 230 },  { 73, 200 },  { 77, 230 },  { 79, 310 },  { 86, 590 },
    { 87, 470 }, { 89, 740 },  { 92, 590 },  { 96, 800 },  { 99, 560 },  { 101, 640 }, { 102, 800 }, { 105, 90 },  { 106, 160 }, { 107, 90 },  { 108, 140 }, { 109, 90 },  { 110, 240 },
    { 111, 90 },  { 112, 160 }, { 113, 130 }, { 114, 240 }, { 134, 190 }, { 136, 270 }, { 376, 500 },
};

/* The elements the signatures given in to_magic with none take (defense_stat 2 Fire .. 8 Dark, the element power by the
 * MP band as the elemental magic has it: 32 below 60 MP, 48 below 100, 56 below 150, 64 above, Dark 72 from 150 MP).
 * 101 and 102 (Omnimon's T-Sword, Imperialdramon Paladin's Omega Blade) become magic with no element, and the
 * counter stance of Final Purification (90, kind 10) stays physical: a counter does not go through the technique's own
 * damage; so does Hookmon's Captain Cannon (62, kind 11): the critical chance it has is a physical blow's (a magic's
 * critical roll does not read it). Their power (to_magic) is each blow's power x hits ranked among the player's signatures of about the same MP,
 * carried to the same rank among the player's magic of that MP, the ratio kept within 0.7..1.4 and the power per hit
 * within the strongest single hit of the player's magic (800) (owner, 2026-11-24, from the list he researched). */
static const struct {
    short tech;
    unsigned char stat, power;
} to_element[] = {
    { 88, 2, 56 }, { 103, 8, 72 }, { 69, 8, 32 }, { 73, 7, 48 }, { 77, 7, 48 }, { 79, 2, 48 },
    { 86, 7, 56 }, { 87, 2, 56 }, { 89, 8, 72 }, { 92, 2, 56 }, { 96, 7, 64 }, { 99, 8, 72 },
};

/* 1 for a technique the game had as a physical blow and the definitive edition casts as magic (to_magic): the battle
 * still gives it the accessories' on-hit chances a physical blow has (fightstg_action_run). */
int def_tech_was_physical(int tech) {
    size_t k;

    if (!def_on()) {
        return 0;
    }
    for (k = 0; k < sizeof(to_magic) / sizeof(to_magic[0]); k++) {
        if (to_magic[k].tech == tech) {
            return 1;
        }
    }
    return 0;
}

static void patch_techniques(void) {
    int i;

    /* The remade techniques: Libra, Relay */
    records_techniques[202 - 1].target = 2;            /* Libra reads the enemy */
    records_techniques[203 - 1].mp_cost = DEF_RELAY_MP;

    /* The technique rework: damaging attacks with secondary debuffs (E=0) */
    records_techniques[81 - 1].mp_cost = 84;           /* Grisly Wing: Atk-2, 3 turns */
    records_techniques[81 - 1].effect_power = 40;
    records_techniques[108 - 1].mp_cost = 38;          /* Cold Slash: Ice (magic, below) + Atk-1, 2 turns */
    records_techniques[108 - 1].kind = 26;
    records_techniques[108 - 1].effect_power = 28;
    records_techniques[131 - 1].mp_cost = 32;          /* Armor Pierce (ex-Bug Buster): phys + Def-1, 2 turns */
    records_techniques[131 - 1].kind = 27;
    records_techniques[131 - 1].effect_power = 24;
    records_techniques[132 - 1].mp_cost = 26;          /* Suppress Shot (ex-Wing Buster): phys + Atk-1, 2 turns */
    records_techniques[132 - 1].power = 110;
    records_techniques[132 - 1].kind = 26;
    records_techniques[132 - 1].effect_power = 28;
    records_techniques[134 - 1].mp_cost = 50;          /* Siege Salvo (ex-Dino Buster): Machine (magic, below) + Atk-2, 3 turns */
    records_techniques[134 - 1].defense_stat = 7;
    records_techniques[134 - 1].element_power = 48;
    records_techniques[134 - 1].kind = 26;
    records_techniques[134 - 1].effect_power = 36;
    records_techniques[136 - 1].mp_cost = 64;          /* Fortress Salvo (ex-Devil Buster): Machine (magic, below) + Def-2, 3 turns */
    records_techniques[136 - 1].defense_stat = 7;
    records_techniques[136 - 1].element_power = 64;
    records_techniques[136 - 1].kind = 27;
    records_techniques[136 - 1].effect_power = 40;

    /* The technique rework: recycled unused confusion cures (P=0, E=0) -> dedicated Atk debuffs */
    records_techniques[194 - 1].mp_cost = 34;          /* Power Seal (ex-Anti-Confuse): Foe Atk-2, 3 turns */
    records_techniques[194 - 1].element = 5;
    records_techniques[194 - 1].target = 2;
    records_techniques[194 - 1].kind = 26;
    records_techniques[194 - 1].effect_power = 36;
    records_techniques[194 - 1].anim_effect = 37;
    records_techniques[194 - 1].hit_sound = 49;
    records_techniques[194 - 1].anim_script = 16;

    records_techniques[195 - 1].mp_cost = 44;          /* Sapping Bloom (ex-Erase Confuse): Foe Atk-3, 3 turns */
    records_techniques[195 - 1].element = 5;
    records_techniques[195 - 1].target = 2;
    records_techniques[195 - 1].kind = 26;
    records_techniques[195 - 1].effect_power = 48;
    records_techniques[195 - 1].anim_effect = 37;
    records_techniques[195 - 1].hit_sound = 49;
    records_techniques[195 - 1].anim_script = 16;

    /* Only magic carries an element (owner, 2026-11-22): the player's elemental physical techniques become magic, so a
     * physical blow is purely physical and WEAK!/One More come from magic alone. The power is recalibrated to the magic
     * table (the median power per MP of the player's elemental magic, by MP band: x5 up to 60 MP, x5.5 up to 100, x3.15
     * past it), since the magic formula is clamped to 0.5x..2x of the power while the physical one scales with
     * Strength / Defense; less for a technique with a debuff, per hit for the 3-hit Wind one. MP costs stay. */
    {
        size_t k;

        for (k = 0; k < sizeof(to_magic) / sizeof(to_magic[0]); k++) {
            records_techniques[to_magic[k].tech - 1].element = 3;
            records_techniques[to_magic[k].tech - 1].power = (u16)to_magic[k].power;
        }
        for (k = 0; k < sizeof(to_element) / sizeof(to_element[0]); k++) {
            records_techniques[to_element[k].tech - 1].defense_stat = to_element[k].stat;
            records_techniques[to_element[k].tech - 1].element_power = to_element[k].power;
        }
    }

    /* The technique rework: MP adjustments on support / heal / charge / field skills (all E=0) */
    records_techniques[188 - 1].mp_cost = 175;         /* Final Heal */
    records_techniques[198 - 1].mp_cost = 36;          /* Double Power */
    records_techniques[201 - 1].mp_cost = 72;          /* Mega Protection */
    records_techniques[205 - 1].mp_cost = 32;          /* Armor Break */
    records_techniques[206 - 1].mp_cost = 60;          /* Armor Off */
    records_techniques[210 - 1].mp_cost = 44;          /* Misshukikou */
    for (i = 213; i <= 219; i++) {
        records_techniques[i - 1].mp_cost = 48;        /* Elemental Fields */
    }

    /* The technique rework: Digimon form learnsets (records_digimon[idx], 0-indexed) */
    records_digimon[12].techniques[4] = 194;           /* ExVeemon [4] (Lv 50): Antidote -> Power Seal */
    records_digimon[18].techniques[1] = 184;           /* Angewomon [1] (Lv 15): Antidote -> Small Heal */
    records_digimon[18].techniques[3] = 194;           /* Angewomon [3] (Lv 30): Anti-Paralysis -> Power Seal */
    records_digimon[18].techniques[4] = 200;           /* Angewomon [4] (Lv 45): empty -> Double Guard */
    records_digimon[18].technique_levels[3] = 45;
    records_digimon[19].techniques[1] = 198;           /* Kyukimon [1] (Lv 5): Heat Cutter -> Double Power */
    records_digimon[23].techniques[1] = 205;           /* SkullGreymon [1] (Lv 5): Flame Ball -> Armor Break */
    records_digimon[24].techniques[5] = 197;           /* Paildramon [5] (Lv 65): Anti-Paralysis -> Erase Magic */
    records_digimon[31].techniques[4] = 194;           /* Digitamamon [4] (Lv 40): empty -> Power Seal */
    records_digimon[31].technique_levels[3] = 40;
    records_digimon[33].techniques[1] = 201;           /* Cannondramon [1] (Lv 15): Fish Buster -> Mega Protection */
    records_digimon[35].techniques[1] = 198;           /* WarGreymon [1] (Lv 5): Flame Lance -> Double Power */
    records_digimon[41].techniques[2] = 195;           /* Rosemon [2] (Lv 20): Erase Poison -> Sapping Bloom */
    records_digimon[41].techniques[5] = 197;           /* Rosemon [5] (Lv 70): Anti-Magic -> Erase Magic */
    records_digimon[48].techniques[1] = 152;           /* Omnimon [1] (Lv 10): Flame Breath -> Giga Freeze */
    records_digimon[48].techniques[3] = 199;           /* Omnimon [3] (Lv 35): Giga Fire -> Mega Strength */
}

/* The rookies' MP rows (definitive_mp.c) into records_digimon, written every time (the reset restores the table). */
void def_mp_apply(void);

void def_mp_hook(void) {
    if (def_on()) {
        def_mp_apply();
        patch_techniques();
        def_variants_hook(); /* the equipment variants, written the same way at the same points */
    }
}

/* ---- The damage of an elemental technique, by the definitive multiplier (definitive_mask.c). Called from the
 * rules (scale_damage for physical techniques, get_special_damage for magic) with the stats of the call in
 * fightstg_rules.stats ([0] the attacker, [1] the target). `terms` (above) is what the log prints. */

/* The player's element on an enemy: the multiplier. An enemy's element on the player: the player counts as
 * DEF_PLAYER_REF_RESIST, the scale the old formulas had. Returns the damage for the physical formula's step; `power` is
 * the technique's element_power (the old formula's own term, still used against the player). */
int def_scale_physical(u8 side, int element, int value, int power) {
    if (side != 0) {
        return value + value * power * 2 / DEF_PLAYER_REF_RESIST;
    }
    return def_scale_magic(side, element, value);
}

int def_scale_magic(u8 side, int element, int value) {
    const short *own = fightstg_rules.stats[0].resists;
    const short *foe = fightstg_rules.stats[1].resists;
    int species = fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon;
    int cls;

    if (side != 0) {
        return value; /* the old bands at the reference resist: 125 - 100 / 4 = 100 percent */
    }
    cls = target_class(element, foe, species);
    terms.weak = cls == 1;
    terms.resisted = cls == 3;
    terms.open = chain.staggered[fightstg_battle.state.current[1] % 3];
    terms.affinity = def_affinity(element, own);
    terms.mult = def_multiplier(terms.weak, terms.open, terms.resisted, terms.affinity);
    return value * terms.mult / DEF_MULT_ONE;
}

/* A blow with no element (a physical technique, a basic attack, a magic with none) on an open target: 1 + 0.2. */
int def_open_bonus(u8 side, int defense_stat, int value) {
    if (!def_on() || side != 0 || defense_stat >= 2 || !chain.staggered[fightstg_battle.state.current[1] % 3]) {
        return value;
    }
    terms.weak = terms.resisted = terms.affinity = 0;
    terms.open = 1;
    terms.mult = DEF_MULT_ONE + DEF_MULT_STAGGER;
    return value * terms.mult / DEF_MULT_ONE;
}

/* ---- A boss is immune to status (blocked[0..4], [7]), not to stat debuffs (blocked[8..10]: Strength,
 * Defense, Speed down). With def_on the flag no longer blocks; where it would have, the debuff is scaled. */
int def_stat_blocked(int k) {
    if (def_on() && k >= 8 && k <= 10) {
        return 0;
    }
    return records_state.blocked[k];
}

int def_debuff_scale(int k, int amount) {
    if (def_on() && k >= 8 && k <= 10 && records_state.blocked[k] != 0) {
        return amount * DEF_BOSS_DEBUFF_PCT / 100;
    }
    return amount;
}

/* ---- 3-tier fixed stat buff/debuff system: no stacking (stronger tier wins, same/weaker refreshes turns, opposite
 * sign overwrites), exact % of base stat, turn-based expiration on the target's turn. */
int def_mod_pct(int level) {
    switch (level) {
    case 1:
        return DEF_BUFF_LV1_PCT;
    case 2:
        return DEF_BUFF_LV2_PCT;
    case 3:
        return DEF_BUFF_LV3_PCT;
    case -1:
        return -DEF_DEBUFF_LV1_PCT;
    case -2:
        return -DEF_DEBUFF_LV2_PCT;
    case -3:
        return -DEF_DEBUFF_LV3_PCT;
    default:
        return 0;
    }
}

int def_mod_level_from_amount(int amount) {
    int mag = amount < 0 ? -amount : amount;
    int sign = amount < 0 ? -1 : 1;

    if (amount == 0) {
        return 0;
    }
    if (mag < 30) {
        return sign * 1;
    }
    if (mag <= 45) {
        return sign * 2;
    }
    return sign * 3;
}

void def_mod_tech_spec(int tech, int kind, int amount, int *out_level, int *out_turns) {
    (void)kind;
    switch (tech) {
    case 198: /* Double Power: User Atk+2, 4 turns */
    case 200: /* Double Guard: User Def+2, 4 turns */
        *out_level = 2;
        *out_turns = DEF_MOD_TURNS_LONG;
        return;
    case 199: /* Mega Strength: Allies Atk+3, 3 turns */
    case 201: /* Mega Protection: Allies Def+3, 3 turns */
        *out_level = 3;
        *out_turns = DEF_MOD_TURNS_BASE;
        return;
    case 194: /* Power Seal: Foe Atk-2, 3 turns */
    case 205: /* Armor Break: Foe Def-2, 3 turns */
    case 134: /* Siege Salvo: Foe Atk-2, 3 turns */
    case 81:  /* Grisly Wing: Foe Atk-2, 3 turns */
    case 136: /* Fortress Salvo: Foe Def-2, 3 turns */
        *out_level = -2;
        *out_turns = DEF_MOD_TURNS_BASE;
        return;
    case 195: /* Sapping Bloom: Foe Atk-3, 3 turns */
    case 204: /* Lovely Charm: Enemy Atk-3, 3 turns */
    case 206: /* Armor Off: Foe Def-3, 3 turns */
        *out_level = -3;
        *out_turns = DEF_MOD_TURNS_BASE;
        return;
    case 132: /* Suppress Shot: Foe Atk-1, 2 turns */
    case 108: /* Cold Slash: Foe Atk-1, 2 turns */
    case 131: /* Armor Pierce: Foe Def-1, 2 turns */
        *out_level = -1;
        *out_turns = DEF_MOD_TURNS_SHORT;
        return;
    default: {
        int lvl = def_mod_level_from_amount(amount);

        *out_level = lvl;
        if (lvl == 0) {
            *out_turns = 0;
        } else if (lvl == -1) {
            *out_turns = DEF_MOD_TURNS_SHORT;
        } else {
            *out_turns = DEF_MOD_TURNS_BASE;
        }
        return;
    }
    }
}

int def_mod_apply(u8 side, s32 member, s32 kind, s32 level, s32 turns, s32 blocked_idx) {
    static const int stat_idx[3] = { 0, 1, 4 };
    int s = side != 0;
    FightstgMember *m;
    FightstgStats *stats;
    int base, pct, cur_lvl, cur_turns;

    if (!def_on()) {
        return 0;
    }
    if (member < 0 || member > 2 || kind < 0 || kind > 2 || level == 0 || turns <= 0) {
        return 1;
    }
    if (level < -3) {
        level = -3;
    } else if (level > 3) {
        level = 3;
    }
    m = &fightstg_battle.state.members[s][member];
    if (m->digimon == 0 || m->hp == 0) {
        return 1;
    }
    if (s && fightstg_battle.state.type == 6 && fightstg_battle.state.final_phase != 0 && kind == 1) {
        return 1;
    }
    stats = fightstg_rules.get_stats(s ? 0x10 : 0, 0, member);
    if (m->modifiers[kind] != 0) {
        stats->stats[stat_idx[kind]] -= m->modifiers[kind];
    }
    base = stats->stats[stat_idx[kind]];
    if (base < 1) {
        base = 1;
    }
    cur_lvl = mod_level[s][member][kind];
    cur_turns = mod_turns[s][member][kind];
    if (cur_lvl == 0 || cur_turns == 0) {
        cur_lvl = level;
        cur_turns = turns;
    } else if (cur_lvl > 0 && level > 0) {
        if (level > cur_lvl) {
            cur_lvl = level;
        }
        if (turns > cur_turns) {
            cur_turns = turns;
        }
    } else if (cur_lvl < 0 && level < 0) {
        if (level < cur_lvl) {
            cur_lvl = level;
        }
        if (turns > cur_turns) {
            cur_turns = turns;
        }
    } else {
        cur_lvl = level;
        cur_turns = turns;
    }
    mod_level[s][member][kind] = (signed char)cur_lvl;
    mod_turns[s][member][kind] = (unsigned char)cur_turns;
    pct = def_mod_pct(cur_lvl);
    if (cur_lvl < 0 && s && blocked_idx >= 8 && blocked_idx <= 10) {
        pct = def_debuff_scale(blocked_idx, pct);
    }
    m->modifiers[kind] = (s16)(base * pct / 100);
    if (log_on()) {
        LOG("mod side %d slot %d kind %d level %d turns %d val %d (base %d)", s, (int)member, (int)kind,
            cur_lvl, cur_turns, (int)m->modifiers[kind], base);
    }
    return 1;
}

int def_mod_apply_tech(u8 side, s32 member, s32 kind, s32 tech, s32 amount, s32 blocked_idx) {
    int level, turns;

    if (!def_on()) {
        return 0;
    }
    def_mod_tech_spec(tech, kind, amount, &level, &turns);
    return def_mod_apply(side, member, kind, level, turns, blocked_idx);
}

int def_mod_start_event(void) {
    return def_on();
}

void def_member_knockout(s32 side, s32 member) {
    int s = side != 0;
    int k;

    if (!def_on() || member < 0 || member > 2) {
        return;
    }
    if (s == 0) {
        blast_fallen = 1;
    }
    for (k = 0; k < 3; k++) {
        mod_level[s][member][k] = 0;
        mod_turns[s][member][k] = 0;
        fightstg_battle.state.members[s][member].modifiers[k] = 0;
    }
}

int def_mod_level(int side, int slot, int kind) {
    int s = side != 0;

    if (slot < 0 || slot > 2 || kind < 0 || kind > 2) {
        return 0;
    }
    return mod_level[s][slot][kind];
}

int def_mod_turns_left(int side, int slot, int kind) {
    int s = side != 0;

    if (slot < 0 || slot > 2 || kind < 0 || kind > 2) {
        return 0;
    }
    return mod_turns[s][slot][kind];
}

/* ---- The stat modifiers count for magic. The engine has none for Spirit; get_special_damage (with
 * fightstg_rules.stats filled: [0] the attacker, [1] the target, both with their modifiers in) asks for these:
 *  - the attack: Spirit scaled as Strength is by its modifier (now / (now - modifier));
 *  - the defence: the player's Defense (already modified), or the enemy's Spirit scaled as its Defense is. The
 *    final boss (state.type 6) sets its own Defense and Spirit modifiers: its Spirit is left as it comes. */
static int scaled(int stat, int now, int modifier) {
    int base = now - modifier;

    if (modifier == 0 || base <= 0) {
        return stat;
    }
    stat = stat * now / base;
    return stat < 1 ? 1 : stat;
}

int def_magic_attack(u8 side) {
    FightstgMember *m = &fightstg_battle.state.members[side != 0][fightstg_battle.state.current[side != 0]];
    const FightstgStats *a = &fightstg_rules.stats[0];

    return scaled(a->stats[2], a->stats[0], m->modifiers[0]);
}

int def_magic_defense(u8 side) {
    FightstgMember *m = &fightstg_battle.state.members[side == 0][fightstg_battle.state.current[side == 0]];
    const FightstgStats *t = &fightstg_rules.stats[1];

    if (side != 0) {
        return t->stats[1] < 1 ? 1 : t->stats[1];
    }
    if (fightstg_battle.state.type == 6) {
        return t->stats[2] < 1 ? 1 : t->stats[2];
    }
    return scaled(t->stats[2], t->stats[1], m->modifiers[1]);
}

/* ---- The game's side of the TP bands (the rule is definitive_progression.c). */
static unsigned char tp_lost[8]; /* per partner: the last level-up lost TP to the ceiling (the report's message) */

s16 def_level_tp(s32 digimon, s16 tp, s16 level) {
    int lost;
    int now = def_tp_add(tp, level, &lost);

    if (lost > 0 && digimon >= 0 && digimon < 8) {
        tp_lost[digimon] = 1;
    }
    return (s16)now;
}

int def_tp_lost_take(s32 digimon) {
    int was;

    if (!def_on() || digimon < 0 || digimon >= 8) {
        return 0;
    }
    was = tp_lost[digimon];
    tp_lost[digimon] = 0;
    return was;
}

const unsigned char *def_tp_full_text(void) {
    extern const unsigned char def_str_LevelTpFull[];
    return def_str_LevelTpFull;
}

/* ---- Training pity label for STGTRAIN. */
static void put_dec(unsigned char *out, int *p, int v, const unsigned char *digits) {
    if (v >= 10) {
        put_dec(out, p, v / 10, digits);
    }
    out[(*p)++] = digits[v % 10];
}

int def_train_pity_bonus_on(void) {
    return DEF_TRAIN_PITY_BONUS != 0;
}

const unsigned char *def_train_pity_label_tier(int tier, int *palette) {
    extern const unsigned char def_str_TrainPityPrefix[], def_str_TrainPityMarks[], def_str_TrainPityDigits[],
        def_str_TrainPityReady[];
    static unsigned char buf[64];
    int total = DEF_TRAIN_PITY_AFTER;
    int count = def_train_pity_count_tier(tier);
    int i, p = 0;

    if (!def_on() || total <= 0) {
        return NULL;
    }
    for (i = 0; def_str_TrainPityPrefix[i] != 0 && p < (int)sizeof(buf) - 24; i++) {
        buf[p++] = def_str_TrainPityPrefix[i];
    }
    buf[p++] = def_str_TrainPityMarks[4];
    buf[p++] = def_str_TrainPityMarks[5];
    if (def_train_pity_guaranteed_tier(tier)) {
        for (i = 0; def_str_TrainPityReady[i] != 0 && p < (int)sizeof(buf) - 1; i++) {
            buf[p++] = def_str_TrainPityReady[i];
        }
        *palette = 4;
    } else {
        for (i = 0; i < total && i < 8; i++) {
            int off = i < count ? 0 : 2;
            buf[p++] = def_str_TrainPityMarks[off];
            buf[p++] = def_str_TrainPityMarks[off + 1];
        }
        buf[p++] = def_str_TrainPityMarks[4];
        buf[p++] = def_str_TrainPityMarks[5];
        put_dec(buf, &p, count, def_str_TrainPityDigits);
        buf[p++] = def_str_TrainPityMarks[6];
        buf[p++] = def_str_TrainPityMarks[7];
        put_dec(buf, &p, total, def_str_TrainPityDigits);
        *palette = count > 0 ? 3 : 0;
    }
    buf[p] = 0;
    return buf;
}

const unsigned char *def_train_pity_label(int *palette) {
    return def_train_pity_label_tier(0, palette);
}

/* ---- Persist Blast Gauges in the unused tail of each save slot. */
void def_gauges_slot_write(void *slot_save) {
    if (!def_on() || slot_save == NULL) {
        return;
    }
    def_gauges_encode(records_state.gauges, (unsigned char *)slot_save + DEF_GAUGE_SLOT_OFFSET);
}

void def_gauges_slot_read(const void *slot_save) {
    if (!def_on() || slot_save == NULL) {
        return;
    }
    def_gauges_decode((const unsigned char *)slot_save + DEF_GAUGE_SLOT_OFFSET, records_state.gauges);
}

/* ---- Session RAM tracking of seen digivolution/blast animations. */
int def_evo_form_seen(int form_id) {
    return def_on() && def_evo_seen_get(def_evo_seen_bits, form_id);
}

void def_evo_form_mark(int form_id) {
    if (!def_on()) {
        return;
    }
    def_evo_seen_set(def_evo_seen_bits, form_id);
}

/* ---- Resistance training without secondary stat loss. */
int def_train_no_stat_loss(void) {
    return def_on() && def_train_no_stat_loss_on();
}

/* ---- Overlevel Rush: a random battle against enemies DEF_OVERLEVEL_GAP levels or more under the party's best is
 * offered as a skip (the field asks; fieldstg_rush_update). Taken, the battle is not loaded: the field leaves for the
 * battle's report (STFGTREP, map 0x1400) with the results a win by the first member would have left, so experience,
 * Bits and the drop come from the game's own routine. With DEF_RUSH_FLOAT the field stays instead and
 * the report's routines give them there, shown in a panel over the player (definitive_rush_panel.c, def_rush_float).
 * DW3_RUSH_TEST (a test's): 1 offers it whatever the levels, 2 also takes it without asking. */
int def_rush_test_mode = -1; /* -1: DW3_RUSH_TEST not read yet (a debug session may set it directly) */

static int rush_test(void) {
    if (def_rush_test_mode < 0) {
        const char *v = getenv("DW3_RUSH_TEST");

        def_rush_test_mode = v != NULL ? atoi(v) : 0;
    }
    return def_rush_test_mode;
}

int def_rush_auto(void) {
    return rush_test() == 2;
}

int def_rush_offer(void) {
    int best = 0, worst = 0, i, id;

    if (!def_on()) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        id = gamestate_data.funcs.get_party_member(i);
        if (id >= 0 && gamestate_data.digimon[id].record.stats.values[0] > best) {
            best = gamestate_data.digimon[id].record.stats.values[0];
        }
        if (records_state.enemies[i].digimon != 0 && records_state.enemies[i].level > worst) {
            worst = records_state.enemies[i].level;
        }
    }
    if (rush_test() == 0 && best - worst < DEF_OVERLEVEL_GAP) {
        return 0;
    }
    if (log_on() || rush_test()) {
        port_log("rush: offered (party level %d, enemies' %d)", best, worst);
    }
    return 1;
}

/* records_find_digimon looks at the first 0x34 forms only; the three DNA results after them (Agunimon,
 * BladeGarurumon, AncientGreymon: stubs with no stats nor techniques, which the game never looks up) are found here too, so
 * the DRI Gauntlet can show them. */
#define DEF_FORMS 0x37
RecordsDigimon *def_form_record(int id) {
    extern RecordsDigimon *records_get_digimon(s32 id);
    RecordsDigimon *form = records_get_digimon(id);
    int i;

    for (i = 0x34; form == NULL && id > 0 && i < DEF_FORMS; i++) {
        if (records_digimon[i].id == id) {
            form = &records_digimon[i];
        }
    }
    return form;
}

const unsigned char *def_species_name(int species) {
    extern void *font_get_entry(void *data, s32 i);
    FightstgEnemyRecord *recs = (FightstgEnemyRecord *)cdload_module.files.get_file(0x1CF);
    void *names = cdload_module.files.get_file(records_language + 0x4E);
    int i;

    if (recs == NULL || names == NULL || species == 0) {
        return NULL;
    }
    for (i = 0; i < DEF_BESTIARY_SPECIES && recs[i].id != 0; i++) {
        if (recs[i].id == species) {
            return (const unsigned char *)font_get_entry(names, recs[i].name);
        }
    }
    { /* a player form that fights as an enemy (the DRI Gauntlet's stages): its own name */
        RecordsDigimon *form = def_form_record(species);

        return form != NULL ? (const unsigned char *)font_get_entry(names, form->name_id) : NULL;
    }
}

/* The results of the battle records_state holds, as won by the party's first member able to fight; no OVERFINISH. */
void def_rush_results(void) {
    FightstgEnemyRecord *recs = (FightstgEnemyRecord *)cdload_module.files.get_file(0x1CF);
    int i, id, lead = 0;

    for (i = 2; i >= 0; i--) {
        id = gamestate_data.funcs.get_party_member(i);
        if (id >= 0 && gamestate_data.digimon[id].record.stats.values[2] > 0) {
            lead = i;
        }
    }
    memset(&records_battle_results, 0, sizeof(records_battle_results));
    records_battle_results.battle = records_state.battle;
    records_battle_results.member = lead;
    records_battle_results.members[lead].took_part = 1;
    if (records_state.has_prize == 1) {
        records_battle_results.item = records_state.prize_item;
    } else if (recs != NULL) {
        for (i = 0; i < DEF_BESTIARY_SPECIES && recs[i].id != 0; i++) { /* the first enemy's drop, at its own rate */
            if (recs[i].id == records_state.enemies[0].digimon) {
                if (recs[i].item > 0 && pad_random.next() % 1024 < recs[i].drop_rate + 1) {
                    records_battle_results.item = recs[i].item;
                }
                break;
            }
        }
    }
    overfinish_damage = 0;
    def_hunt_skipped(); /* a Rush-won encounter also advances the regional hunt chain and drops materials */
    port_log("rush: battle %d skipped (member %d, item %d)", (int)records_state.battle, lead,
             (int)records_battle_results.item);
}

/* ---- Negative status rework: precision gap, stagger bonus, boss tradeoffs for Sleep and Confusion, Poison as a DoT. */
int def_status_blocked(int k) {
    if (def_on() && (k == 0 || k == 2 || k == 3)) {
        return 0;
    }
    return records_state.blocked[k];
}

int def_status_chance(int k, int value, const FightstgStats *atk, const FightstgStats *tgt, int resist_sum) {
    return def_status_chance_calc(value, atk->stats[3], tgt->stats[3], resist_sum, records_state.blocked[k] != 0,
                                  def_target_staggered());
}

int def_poison_dot(const FightstgStats *atk, int base_power) {
    return def_poison_dot_calc(base_power, def_affinity(3, atk->resists), atk->stats[3], atk->stats[2]);
}

int def_poison_tick(int amount, int max_hp, int enemy) {
    int v;

    if (!def_on()) {
        return -1;
    }
    v = def_poison_cap(amount, max_hp);
    if (enemy && v > max_hp * DEF_POISON_ENEMY_PCT / 100) { /* the DoT build must not melt a boss on its own */
        v = max_hp * DEF_POISON_ENEMY_PCT / 100;
    }
    return v < 1 ? 1 : v;
}

int def_wake_on_hit(u8 side) {
    return def_on() && side != 0;
}

static unsigned char enemy_sleep_turns[3];
static unsigned char enemy_conf_turns[3];

void def_enemy_status_turn(void) {
    int slot = fightstg_battle.state.current[1] % 3;
    FightstgMember *m = &fightstg_battle.state.members[1][slot];
    int e;

    if (!def_on()) {
        return;
    }
    if (m->status & 8) {
        int heal = m->max_hp * DEF_SLEEP_REGEN_PCT / 100;

        m->hp = (s16)(m->hp + heal > m->max_hp ? m->max_hp : m->hp + heal);
        if (++enemy_sleep_turns[slot] > DEF_STATUS_BOSS_TURNS) {
            m->status &= ~8;
            e = fightstg_events.find_member(12, 0x10, fightstg_battle.state.current[1]);
            if (e >= 0) {
                fightstg_events.events[e].type = 0;
            }
            enemy_sleep_turns[slot] = 0;
        }
    } else {
        enemy_sleep_turns[slot] = 0;
    }
    if (m->status & 4) {
        if (++enemy_conf_turns[slot] > DEF_STATUS_BOSS_TURNS) {
            m->status &= ~4;
            e = fightstg_events.find_member(11, 0x10, fightstg_battle.state.current[1]);
            if (e >= 0) {
                fightstg_events.events[e].type = 0;
            }
        }
    }
    if (!(m->status & 4) && enemy_conf_turns[slot] != 0) {
        enemy_conf_turns[slot] = 0;
        def_mod_apply(1, slot, 0, DEF_CONFUSION_BUFF_LEVEL, DEF_MOD_TURNS_BASE, 0);
    }
}

/* ---- Waves (Option A, owner 2026-11-21): when the battle's last enemy member falls, a mode with more to come (the DRI's
 * Mega) sets a fallen enemy slot up again as the next one, and wfightmn_knockout brings it in with the DRI's entrance (its
 * model is loaded then, as for any stage). The slot's port state goes with the one that fell there: a wave starts clean. */
int def_enemy_next_wave(void) {
    int slot, k;

    if (!def_on()) {
        return -1;
    }
    slot = def_dri_next_wave();
    if (slot < 0) {
        slot = def_purify_next_wave();
    }
    if (slot < 0 || slot > 2) {
        return -1;
    }
    memset(resist_mod[1][slot], 0, sizeof(resist_mod[1][0]));
    resist_turns[1][slot] = 0;
    for (k = 0; k < 3; k++) {
        mod_level[1][slot][k] = 0;
        mod_turns[1][slot][k] = 0;
    }
    chain.staggered[slot] = 0;
    enemy_sleep_turns[slot] = 0;
    enemy_conf_turns[slot] = 0;
    if (log_on()) {
        FightstgMember *m = &fightstg_battle.state.members[1][slot];

        LOG("wave enemy %d digimon %d level %d hp %d/%d mp %d scale %d", slot, m->digimon,
            records_state.enemies[slot].level, m->hp, m->max_hp, m->mp, records_state.enemies[slot].stat_scale);
    }
    return slot;
}

/* ---- Revive tiers: the item revives each fallen ally with a chance that falls when its max HP outgrows the tier. */
int def_revive_hp(int item, int max_hp) {
    int chance, pct, rated, hp;

    if (max_hp <= 0 || !def_revive_tier(item, &chance, &pct, &rated)) {
        return 0;
    }
    chance = def_revive_chance(chance, rated, max_hp);
    if ((int)(pad_random.next() % 100) >= chance) {
        return 0;
    }
    hp = max_hp * pct / 100;
    return hp < 1 ? 1 : hp;
}
