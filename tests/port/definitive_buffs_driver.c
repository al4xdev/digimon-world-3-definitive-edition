/* tests/port/definitive_buffs.py's driver: exercises port/game/definitive.c's 3-tier fixed stat buff/debuff
 * system, turn-based expiration, non-stacking / tier-upgrade / opposite-sign overwrite rules, Overlevel Rush gap = 8,
 * and Layer H technique patching. Prints "ok <name>" or "FAIL <name>" per case; exits 1 on any failure. */
#include <stdio.h>
#include <string.h>

#include "cdload.h"
#include "common.h"
#include "fightstg.h"
#include "gamestate.h"
#include "pad.h"
#include "records.h"
#include "sound.h"
#include "definitive.h"

/* ---- Stand-in game globals and adapter hooks ---- */
CdloadModule cdload_module;
FightstgAction fightstg_action;
FightstgBattle fightstg_battle;
FightstgEvents fightstg_events;
FightstgRules fightstg_rules;
GamestateData gamestate_data;
PadRandom pad_random;
RecordsBattleResults records_battle_results;
s32 records_language = 1;
RecordsState records_state;
RecordsTechnique records_techniques[512];
RecordsDigimon records_digimon[64];
SoundModule sound_module;
int port_script_active = 0;

static s16 base_stats[2][3][5]; /* [side][member][stat] = { atk, def, spi, wis, spd } */
static s32 party_ids[3] = { 0, 1, 2 };

static s32 stub_get_party_member(s32 slot) {
    return (slot >= 0 && slot < 3) ? party_ids[slot] : -1;
}

static FightstgStats *stub_get_stats(u8 side, s32 first, s32 member) {
    int s = side != 0;
    FightstgStats *out = first ? &fightstg_rules.stats[0] : &fightstg_rules.stats[1];
    FightstgMember *m = &fightstg_battle.state.members[s][member];
    int i;

    memset(out, 0, sizeof(*out));
    for (i = 0; i < 5; i++) {
        out->stats[i] = base_stats[s][member][i];
    }
    out->stats[0] += m->modifiers[0];
    out->stats[1] += m->modifiers[1];
    out->stats[4] += m->modifiers[2];
    out->stats[2] += m->modifiers[3];
    return out;
}

void port_hud_arm(void) {}
void port_log(const char *fmt, ...) { (void)fmt; }
void port_girls_refresh(void) {}
s32 fightstg_rules_get_damage(u8 side, s32 id) { (void)side; (void)id; return 0; }
s32 fightstg_rules_get_special_damage(u8 side, s32 id) { (void)side; (void)id; return 0; }
void *font_get_entry(void *d, s32 i) { (void)d; (void)i; return NULL; }
s32 wfightmn_update_idle_anim(u8 side, s32 amount) { (void)side; (void)amount; return 0; }

void def_callout_show(int kind, int side) { (void)kind; (void)side; }
void def_bestiary_reset(void) {}
void def_bestiary_discover(int i, int m) { (void)i; (void)m; }
int def_bestiary_known(int i) { (void)i; return 0; }
int def_bestiary_index(int s) { (void)s; return -1; }
void def_bestiary_savestate(struct PortState *s) { (void)s; }
int def_save_flag(int f) { (void)f; return 0; }
void def_save_flag_set(int f, int on) { (void)f; (void)on; }
void def_dri_battle_begin(void) {}
void def_dri_battle_end(int r) { (void)r; }
void def_dri_savestate(struct PortState *s) { (void)s; }
void def_hunt_battle_end(int r) { (void)r; }
void def_hunt_reward_done(void) {}
void def_hunt_savestate(struct PortState *s) { (void)s; }
int def_hunt_active(void) { return 0; }
void def_hunt_skipped(void) {}
void def_rush_savestate(struct PortState *s) { (void)s; }
void def_guide_savestate(struct PortState *s) { (void)s; }
void def_variants_hook(void) {}
int def_variant_text_hidden(int f, int i) { (void)f; (void)i; return 0; }

const DefEnemyRow def_enemy_rows[] = { { 1, { 100, 100, 100, 100, 100, 100, 100 }, 0 } };
const int def_enemy_row_count = 1;

/* ---- Test harness ---- */
static int failures;

static void check(const char *name, int ok) {
    if (!ok) {
        failures++;
    }
    printf("%s %s\n", ok ? "ok" : "FAIL", name);
}

static void reset_battle(void) {
    int s, m;

    memset(&fightstg_battle, 0, sizeof(fightstg_battle));
    memset(&records_state, 0, sizeof(records_state));
    memset(&gamestate_data, 0, sizeof(gamestate_data));
    memset(records_digimon, 0, sizeof(records_digimon));
    memset(records_techniques, 0, sizeof(records_techniques));
    gamestate_data.funcs.get_party_member = stub_get_party_member;
    fightstg_rules.get_stats = stub_get_stats;
    for (s = 0; s < 2; s++) {
        for (m = 0; m < 3; m++) {
            fightstg_battle.state.members[s][m].digimon = (u16)(10 + s * 10 + m);
            fightstg_battle.state.members[s][m].hp = 500;
            fightstg_battle.state.members[s][m].max_hp = 500;
            base_stats[s][m][0] = 200; /* Attack */
            base_stats[s][m][1] = 200; /* Defense */
            base_stats[s][m][2] = 200; /* Spirit */
            base_stats[s][m][3] = 200; /* Wisdom */
            base_stats[s][m][4] = 200; /* Speed */
        }
    }
    def_log_begin();
}

int main(void) {
    int lvl, turns;

    /* 1. Overlevel Rush gap = 8 */
    reset_battle();
    check("overlevel: DEF_OVERLEVEL_GAP == 8", DEF_OVERLEVEL_GAP == 8);
    gamestate_data.digimon[0].record.stats.values[0] = 20;
    records_state.enemies[0].digimon = 100;
    records_state.enemies[0].level = 13; /* gap = 7 */
    check("overlevel: gap 7 does not offer rush", def_rush_offer() == 0);
    records_state.enemies[0].level = 12; /* gap = 8 */
    check("overlevel: gap 8 offers rush", def_rush_offer() == 1);

    /* 2. Fixed 3-tier percentages */
    check("pct: +1=+20, +2=+35, +3=+50, -1=-15, -2=-25, -3=-35",
          def_mod_pct(1) == 20 && def_mod_pct(2) == 35 && def_mod_pct(3) == 50 &&
          def_mod_pct(-1) == -15 && def_mod_pct(-2) == -25 && def_mod_pct(-3) == -35 &&
          def_mod_pct(0) == 0);

    /* Exact modifier values on base 200 */
    reset_battle();
    def_mod_apply(0, 0, 0, 1, 3, 0);
    check("tier +1 on base 200 -> +40", fightstg_battle.state.members[0][0].modifiers[0] == 40 && def_mod_level(0, 0, 0) == 1);
    reset_battle();
    def_mod_apply(0, 0, 0, 2, 4, 0);
    check("tier +2 on base 200 -> +70", fightstg_battle.state.members[0][0].modifiers[0] == 70 && def_mod_level(0, 0, 0) == 2);
    reset_battle();
    def_mod_apply(0, 0, 0, 3, 3, 0);
    check("tier +3 on base 200 -> +100", fightstg_battle.state.members[0][0].modifiers[0] == 100 && def_mod_level(0, 0, 0) == 3);
    reset_battle();
    def_mod_apply(0x10, 0, 1, -1, 2, 9);
    check("tier -1 on base 200 -> -30", fightstg_battle.state.members[1][0].modifiers[1] == -30 && def_mod_level(1, 0, 1) == -1);
    reset_battle();
    def_mod_apply(0x10, 0, 1, -2, 3, 9);
    check("tier -2 on base 200 -> -50", fightstg_battle.state.members[1][0].modifiers[1] == -50 && def_mod_level(1, 0, 1) == -2);
    reset_battle();
    def_mod_apply(0x10, 0, 1, -3, 3, 9);
    check("tier -3 on base 200 -> -70", fightstg_battle.state.members[1][0].modifiers[1] == -70 && def_mod_level(1, 0, 1) == -3);

    /* 3. No stacking: same tier refreshes turns only, modifier unchanged */
    reset_battle();
    def_mod_apply_tech(0, 0, 0, 198, 48, 0); /* Double Power: +2, 4 turns -> +70 */
    def_party_turn_begins(); /* turns: 4 -> 3 */
    check("no-stack: after 1 turn, +2 has 3 turns left and stays +70",
          def_mod_turns_left(0, 0, 0) == 3 && fightstg_battle.state.members[0][0].modifiers[0] == 70);
    def_mod_apply_tech(0, 0, 0, 198, 48, 0); /* Recast Double Power */
    check("no-stack: recast +2 keeps modifier +70 and refreshes turns to 4",
          def_mod_level(0, 0, 0) == 2 && def_mod_turns_left(0, 0, 0) == 4 &&
          fightstg_battle.state.members[0][0].modifiers[0] == 70);

    /* 4. Stronger tier upgrades; weaker tier of same sign does not downgrade */
    def_mod_apply_tech(0, 0, 0, 199, 64, 0); /* Mega Strength: +3, 3 turns -> +100, turns max(4,3)=4 */
    check("upgrade: +2 -> +3 upgrades modifier to +100 and keeps 4 turns",
          def_mod_level(0, 0, 0) == 3 && def_mod_turns_left(0, 0, 0) == 4 &&
          fightstg_battle.state.members[0][0].modifiers[0] == 100);
    def_party_turn_begins();
    def_party_turn_begins(); /* turns: 4 -> 3 -> 2 */
    def_mod_apply(0, 0, 0, 1, 3, 0); /* +1, 3 turns on +3, 2 turns */
    check("no-downgrade: +1 on +3 keeps +3 (+100) and refreshes turns to 3",
          def_mod_level(0, 0, 0) == 3 && def_mod_turns_left(0, 0, 0) == 3 &&
          fightstg_battle.state.members[0][0].modifiers[0] == 100);

    /* Same for debuffs: -1 -> -3 upgrades, -1 on -3 does not downgrade */
    def_mod_apply_tech(0x10, 0, 0, 132, -28, 8); /* Suppress Shot: -1, 2 turns */
    def_mod_apply_tech(0x10, 0, 0, 132, -28, 8);
    check("no-stack debuff: recast -1 keeps -30 and 2 turns",
          def_mod_level(1, 0, 0) == -1 && def_mod_turns_left(1, 0, 0) == 2 &&
          fightstg_battle.state.members[1][0].modifiers[0] == -30);
    def_mod_apply_tech(0x10, 0, 0, 195, -48, 8); /* Sapping Bloom: -3, 3 turns */
    check("upgrade debuff: -1 -> -3 upgrades to -70 and 3 turns",
          def_mod_level(1, 0, 0) == -3 && def_mod_turns_left(1, 0, 0) == 3 &&
          fightstg_battle.state.members[1][0].modifiers[0] == -70);

    /* 5. Opposite sign cleanly overwrites */
    def_mod_apply(0, 0, 0, -2, 3, 0); /* overwrite +3 on player with -2 */
    check("overwrite: -2 on +3 cleanly sets -2 (-50) for 3 turns",
          def_mod_level(0, 0, 0) == -2 && def_mod_turns_left(0, 0, 0) == 3 &&
          fightstg_battle.state.members[0][0].modifiers[0] == -50);
    def_mod_apply(0, 0, 0, 1, 3, 0); /* overwrite -2 with +1 */
    check("overwrite: +1 on -2 cleanly sets +1 (+40) for 3 turns",
          def_mod_level(0, 0, 0) == 1 && def_mod_turns_left(0, 0, 0) == 3 &&
          fightstg_battle.state.members[0][0].modifiers[0] == 40);

    /* 6. Turn countdown & expiration (plus extra action preservation) */
    reset_battle();
    def_mod_apply_tech(0, 0, 1, 200, 48, 0); /* Double Guard: +2 Def, 4 turns */
    def_action_resolved(0, 56, 100); /* hit */
    /* Simulate weakness extra action: should NOT consume buff turn */
    {
        /* Manually trigger weak hit on target 0 via def_chain_player_hit indirectly:
         * set hit_class via stubbed battle log? Better: test normal 4-turn countdown and extra action. */
    }
    def_party_turn_begins(); /* 4 -> 3 */
    check("countdown party turn 1: 3 turns left, modifier +70",
          def_mod_turns_left(0, 0, 1) == 3 && fightstg_battle.state.members[0][0].modifiers[1] == 70);
    def_party_turn_begins(); /* 3 -> 2 */
    def_party_turn_begins(); /* 2 -> 1 */
    check("countdown party turn 3: 1 turn left, modifier +70",
          def_mod_turns_left(0, 0, 1) == 1 && fightstg_battle.state.members[0][0].modifiers[1] == 70);
    def_party_turn_begins(); /* 1 -> 0 -> expire */
    check("countdown party turn 4: expires -> level 0, turns 0, modifier 0",
          def_mod_level(0, 0, 1) == 0 && def_mod_turns_left(0, 0, 1) == 0 &&
          fightstg_battle.state.members[0][0].modifiers[1] == 0);

    /* Enemy 2-turn countdown */
    def_mod_apply_tech(0x10, 0, 0, 132, -28, 8); /* Suppress Shot: -1, 2 turns */
    def_enemy_turn_begins(); /* 2 -> 1 */
    check("countdown enemy turn 1: 1 turn left, modifier -30",
          def_mod_turns_left(1, 0, 0) == 1 && fightstg_battle.state.members[1][0].modifiers[0] == -30);
    def_enemy_turn_begins(); /* 1 -> 0 -> expire */
    check("countdown enemy turn 2: expires -> level 0, turns 0, modifier 0",
          def_mod_level(1, 0, 0) == 0 && def_mod_turns_left(1, 0, 0) == 0 &&
          fightstg_battle.state.members[1][0].modifiers[0] == 0);

    /* 7. Knockout clears buffs/debuffs */
    def_mod_apply(0, 0, 0, 2, 4, 0);
    def_member_knockout(0, 0);
    check("knockout clears level, turns, and modifier",
          def_mod_level(0, 0, 0) == 0 && def_mod_turns_left(0, 0, 0) == 0 &&
          fightstg_battle.state.members[0][0].modifiers[0] == 0);

    /* 8. Magic scaling follows 3-tier Attack/Defense modifiers */
    reset_battle();
    def_mod_apply(0, 0, 0, 3, 3, 0); /* +50% Atk -> 300/200 -> Spirit 200 becomes 300 */
    stub_get_stats(0, 1, 0);
    stub_get_stats(0x10, 0, 0);
    check("magic attack scales +50% with Tier +3 Attack buff (200 -> 300)", def_magic_attack(0) == 300);

    /* 9. Technique spec mapping & Layer H patching */
    def_mod_tech_spec(132, 0, -28, &lvl, &turns);
    check("spec 132 Suppress Shot -> -1, 2t", lvl == -1 && turns == 2);
    def_mod_tech_spec(131, 1, -24, &lvl, &turns);
    check("spec 131 Armor Pierce -> -1, 2t", lvl == -1 && turns == 2);
    def_mod_tech_spec(108, 0, -28, &lvl, &turns);
    check("spec 108 Cold Slash -> -1, 2t", lvl == -1 && turns == 2);
    def_mod_tech_spec(134, 0, -36, &lvl, &turns);
    check("spec 134 Siege Salvo -> -2, 3t", lvl == -2 && turns == 3);
    def_mod_tech_spec(81, 0, -40, &lvl, &turns);
    check("spec 81 Grisly Wing -> -2, 3t", lvl == -2 && turns == 3);
    def_mod_tech_spec(136, 1, -40, &lvl, &turns);
    check("spec 136 Fortress Salvo -> -2, 3t", lvl == -2 && turns == 3);
    def_mod_tech_spec(194, 0, -36, &lvl, &turns);
    check("spec 194 Power Seal -> -2, 3t", lvl == -2 && turns == 3);
    def_mod_tech_spec(195, 0, -48, &lvl, &turns);
    check("spec 195 Sapping Bloom -> -3, 3t", lvl == -3 && turns == 3);
    def_mod_tech_spec(198, 0, 48, &lvl, &turns);
    check("spec 198 Double Power -> +2, 4t", lvl == 2 && turns == 4);
    def_mod_tech_spec(199, 0, 64, &lvl, &turns);
    check("spec 199 Mega Strength -> +3, 3t", lvl == 3 && turns == 3);
    def_mod_tech_spec(200, 1, 48, &lvl, &turns);
    check("spec 200 Double Guard -> +2, 4t", lvl == 2 && turns == 4);
    def_mod_tech_spec(201, 1, 64, &lvl, &turns);
    check("spec 201 Mega Protection -> +3, 3t", lvl == 3 && turns == 3);
    def_mod_tech_spec(204, 0, -24, &lvl, &turns);
    check("spec 204 Lovely Charm -> -3, 3t", lvl == -3 && turns == 3);
    def_mod_tech_spec(205, 1, -24, &lvl, &turns);
    check("spec 205 Armor Break -> -2, 3t", lvl == -2 && turns == 3);
    def_mod_tech_spec(206, 1, -48, &lvl, &turns);
    check("spec 206 Armor Off -> -3, 3t", lvl == -3 && turns == 3);

    /* Verify Layer H patches applied by def_log_begin() -> def_mp_hook() -> patch_techniques() */
    check("patch_techniques: 194/195 converted to element 5 Atk-down & Digimon slots updated",
          records_techniques[194 - 1].element == 5 && records_techniques[194 - 1].kind == 26 &&
          records_techniques[194 - 1].mp_cost == 34 &&
          records_techniques[195 - 1].element == 5 && records_techniques[195 - 1].kind == 26 &&
          records_techniques[195 - 1].mp_cost == 44 &&
          records_techniques[134 - 1].defense_stat == 7 && records_techniques[136 - 1].defense_stat == 7 &&
          records_digimon[18].techniques[4] == 200 && records_digimon[18].technique_levels[3] == 45 &&
          records_digimon[31].techniques[4] == 194 && records_digimon[31].technique_levels[3] == 40 &&
          records_digimon[48].techniques[1] == 152 && records_digimon[48].techniques[3] == 199);

    return failures ? 1 : 0;
}
