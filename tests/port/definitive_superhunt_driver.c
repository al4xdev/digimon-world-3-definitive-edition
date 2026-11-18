/* tests/port/definitive_superhunt.py's driver for port/game/definitive_superhunt.c (the Super Rare hunts). The game's
 * state the module reads (records_state, pad_random, ststatus_map_areas, def_on), the hunts' table and the map graph
 * are stand-ins. Prints "ok name" / "FAIL name" per case; exits 1 on a failure. */
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "pad.h"
#include "records.h"

#include "definitive.h"

void port_log(const char *fmt, ...) {
    (void)fmt;
}
static int on = 1;
int def_on(void) {
    return on;
}

/* The Hunter's Mark (definitive_market.c): not held here. */
int def_hunt_mark_mult(int n) {
    return n;
}

RecordsState records_state;
unsigned char ststatus_map_areas[0xF0];

static int draws;
static s32 next_value;
static s32 next(void) {
    draws++;
    return next_value;
}
PadRandom pad_random = { 0, NULL, next };

/* The world: areas 0 and 1 are sector 0, area 2 sector 1; area 3 none. Maps 0x200 (area 0), 0x201 and 0x202 (area 1)
 * have random encounters, 0x203 (area 2) too, 0x204 (area 0) none. 0x280 is 0x200's area on the second server.
 * Exits: 0x204 - 0x200 - 0x201 - 0x202 - 0x203, both ways; 0x280 is reached from nowhere. */
const signed char def_hunt_sector_of[DEF_HUNT_AREAS] = { 0, 0, 1, -1 };
const unsigned short def_map_nodes[] = { 0x200, 0x201, 0x202, 0x203, 0x204, 0x280 };
const unsigned short def_map_first[] = { 0, 2, 4, 6, 7, 8, 8 };
const unsigned short def_map_edges[] = { 1, 4, 0, 2, 1, 3, 2, 0 };
const unsigned char def_map_wild[] = { 1, 1, 1, 1, 0, 1 };
const int def_map_node_count = 6;

/* Hunt 0: battle 5's second enemy, level 40, 10000 HP, sector 0 of the first server; hunt 1 sector 1; hunt 2 (a
 * tutorial's shape) only area 1 of sector 0. */
const DefSuperRare def_super_rares[DEF_SUPER_COUNT] = {
    { 5, 1, 40, 10000, 0, 0, -1, 301, 9000 },
    { 6, 0, 50, 20000, 1, 0, -1, 400, 12000 },
    { 7, 0, 3, 480, 0, 0, 1, 34, 1500 },
};

static int failures;
static void check(const char *name, int ok) {
    if (!ok) {
        failures++;
    }
    printf("%s %s\n", ok ? "ok" : "FAIL", name);
}

/* The battle the field sets up: battle 5's enemies (the Super Rare in slot 1), then def_super_scale. */
static void set_up_battle(void) {
    memset(&records_state, 0, sizeof(records_state));
    records_state.enemies[0].digimon = 300;
    records_state.enemies[1].digimon = 301;
    records_state.enemies[1].level = 70;
    records_state.enemies[1].hp = 5000;
    records_state.enemies[1].stat_scale = 20;
    records_state.enemies[2].digimon = 302;
    def_super_scale();
}

int main(void) {
    static const short points[] = { 20, 20, 300, 200, 50, 50 };
    unsigned char rec[DEF_SUPER_RECORD];
    int i, map, old, first;

    ststatus_map_areas[0x00] = 0x00;
    ststatus_map_areas[0x01] = 0x01;
    ststatus_map_areas[0x02] = 0x01;
    ststatus_map_areas[0x03] = 0x02;
    ststatus_map_areas[0x04] = 0x00;
    ststatus_map_areas[0x80] = 0x80;
    def_super_reset();

    check("the graph: maps apart by exits, none to a map with no way in or out of the graph",
          def_map_distance(0x200, 0x200) == 0 && def_map_distance(0x204, 0x203) == 4 && def_map_distance(0x203, 0x201) == 2 &&
              def_map_distance(0x200, 0x280) == -1 && def_map_distance(0x200, 0x999) == -1);

    check("a new game: the first hunt open, the others locked, none active",
          def_super_state(0) == DEF_SUPER_OPEN && def_super_state(1) == DEF_SUPER_LOCKED && def_super_active() == -1 &&
              def_super_map() == 0 && def_super_record_empty());
    check("a locked hunt cannot start", !def_super_start(1) && def_super_active() == -1);

    next_value = 1; /* the second of the territory's maps with random encounters: 0x201 */
    check("the open hunt starts", def_super_start(0) && def_super_active() == 0 && def_super_state(0) == DEF_SUPER_ACTIVE);
    check("its target sits on a map with random encounters of its territory, drawn", def_super_map() == 0x201);
    check("no second hunt while one is active", !def_super_start(0) && !def_super_start(1));
    check("its territory: the sector's maps of its server, not another sector's nor the other server's",
          def_super_in_territory(0x200) && def_super_in_territory(0x204) && !def_super_in_territory(0x203) &&
              !def_super_in_territory(0x280) && !def_super_in_territory(0x300) && def_super_in_region(0x01) &&
              !def_super_in_region(0x81) && !def_super_in_region(0x02) && !def_super_in_region(0xFF));

    check("the compass zone: the territory, and the maps a few exits out of it; none past the reach or unreachable",
          def_super_territory_distance(0, 0x201) == 0 && def_super_territory_distance(0, 0x203) == 1 &&
              def_super_territory_distance(2, 0x204) == 2 && def_super_territory_distance(0, 0x280) == -1 &&
              def_super_in_compass_zone(0x203) && !def_super_in_compass_zone(0x280) &&
              def_super_temperature(0x280, 0, 0) == DEF_TEMP_NONE);
    check("the compass: cool at 2-3 maps (outside the territory too), warm next door",
          def_super_temperature(0x203, 0, 0) == DEF_TEMP_COOL && def_super_temperature(0x204, 0, 0) == DEF_TEMP_COOL &&
              def_super_temperature(0x200, 0, 0) == DEF_TEMP_WARM && def_super_temperature(0x202, 0, 0) == DEF_TEMP_WARM);
    check("on the target's map, before its point is known: hot, and the field is asked for the candidates",
          def_super_temperature(0x201, 10, 10) == DEF_TEMP_HOT && def_super_step(0x201, 10, 10) == 2 &&
              def_super_step(0x200, 10, 10) == 0);
    def_super_set_points(points, 3, 10, 10);
    check("its point: of three candidates, the farthest from the arrival; hot away from it, burning close, a step on it "
          "starts the battle",
          def_super_temperature(0x201, 10, 10) == DEF_TEMP_HOT && def_super_temperature(0x201, 250, 150) == DEF_TEMP_BURNING &&
              !def_super_step(0x201, 20, 20) && !def_super_step(0x202, 300, 200) && !def_super_battle_ready() &&
              def_super_step(0x201, 300 + DEF_SUPER_POINT_RADIUS - 4, 200) && def_super_battle_ready());

    check("the point stays while the target does", def_super_step(0x201, 300, 200) == 1);
    set_up_battle();
    check("the battle: the Super Rare alone, at its level, its full HP for the bar, its stats as they are the first time",
          records_state.enemies[0].digimon == 301 && records_state.enemies[0].level == 40 &&
              records_state.enemies[0].hp == 10000 && records_state.enemies[0].stat_scale == 20 &&
              records_state.enemies[1].digimon == 0 && records_state.enemies[2].digimon == 0 && def_super_hp_left() == 10000);
    check("the party cannot flee from the tutorial hunt", def_hunt_flee() == 0);
    check("it flees once it has lost DEF_SUPER_FLEE_PCT of its full HP in the battle, not before, nor when it falls",
          !def_super_should_flee(10000 - 2499) && def_super_should_flee(10000 - 2500) && !def_super_should_flee(0));
    next_value = 0;
    old = def_super_map();
    def_super_battle_end(0, 7500);
    check("it fled: it keeps the HP it lost and moves to another map of its territory",
          def_super_active() == 0 && def_super_hp_left() == 7500 && def_super_meets() == 1 && def_super_map() != old &&
              def_super_in_territory(def_super_map()) && def_super_map() != 0x204 && !def_super_battle_ready());

    map = def_super_map();
    def_super_set_points(points, 3, 0, 0);
    def_super_step(map, 300, 200);
    set_up_battle();
    check("found again: its full HP bar, the HP it has left, its stats DEF_SUPER_BUFF_PCT up",
          records_state.enemies[0].hp == 10000 && def_super_hp_left() == 7500 &&
              records_state.enemies[0].stat_scale == 20 * (100 + DEF_SUPER_BUFF_PCT) / 100);
    check("it flees again after losing another DEF_SUPER_FLEE_PCT in this battle",
          !def_super_should_flee(7500 - 2499) && def_super_should_flee(7500 - 2500));
    def_super_battle_end(2, 7400); /* the party fell */
    check("the party fell: the hunt goes on, the target moved with what it lost", def_super_active() == 0 &&
                                                                                 def_super_hp_left() == 7400 && def_super_meets() == 2);

    /* Super Hard doubles the HP in battle: the battle side counts in its units, the hunt in the table's. */
    map = def_super_map();
    def_super_set_points(points, 3, 0, 0);
    def_super_step(map, 300, 200);
    set_up_battle();
    i = def_super_hp_start(20000);
    check("in a battle with twice the HP: it starts at its share of the bar, flees after a quarter of the bar",
          i == 14800 && !def_super_should_flee(14800 - 4999) && def_super_should_flee(14800 - 5000));
    def_super_battle_end(0, 14800 - 5000);
    check("and what it lost goes back in the table's units", def_super_hp_left() == 4900 && def_super_meets() == 3);

    def_super_record_write(rec);
    def_super_reset();
    check("a new game forgets the hunt", def_super_active() == -1 && def_super_record_empty());
    def_super_record_read(rec);
    check("the record keeps the hunt under way: its target's map, HP lost and battles",
          def_super_active() == 0 && def_super_hp_left() == 4900 && def_super_meets() == 3 && def_super_map() != 0);

    map = def_super_map();
    def_super_set_points(points, 3, 0, 0);
    def_super_step(map, 300, 200);
    set_up_battle();
    def_super_battle_end(1, 0);
    check("won: its crystal, the hunt ends, the next one opens and this one can be repeated",
          def_super_crystals(0) == 1 && def_super_active() == -1 && def_super_done() == 1 && def_super_state(0) == DEF_SUPER_DONE &&
              def_super_state(1) == DEF_SUPER_OPEN && def_super_state(2) == DEF_SUPER_LOCKED);
    i = def_super_won_take(&first);
    check("the field's panel gets the hunt once, as a first win (it unlocks skin 1)",
          i == 0 && first == 1 && def_super_won_take(&first) == -1);

    next_value = 0;
    check("a finished hunt is repeated, with the target healed", def_super_start(0) && def_super_hp_left() == 10000 &&
                                                                    def_super_meets() == 0);
    map = def_super_map();
    def_super_set_points(points, 3, 0, 0);
    def_super_step(map, 300, 200);
    set_up_battle();
    def_super_battle_end(1, 0);
    check("won again: a second crystal, and the order does not move", def_super_crystals(0) == 2 && def_super_done() == 1);
    i = def_super_won_take(&first);
    check("and the panel gets it as a win again: no new skin", i == 0 && first == 0);

    def_super_start(1);
    def_super_battle_end(0, 100); /* no Super Rare battle under way: nothing happens */
    check("a battle's end that was not the Super Rare's changes nothing", def_super_active() == 1 && def_super_meets() == 0);
    check("hunt 1's target is in its own sector", def_super_map() == 0x203);
    def_super_cancel();
    check("cancelled: no hunt, the next one still open", def_super_active() == -1 && def_super_state(1) == DEF_SUPER_OPEN &&
                                                          def_super_temperature(0x203, 0, 0) == DEF_TEMP_NONE);

    {
        static short many[2 * 9];

        for (i = 0; i < 9; i++) {
            many[2 * i] = (short)(100 * (8 - i)); /* 800, 700, ..., 0 from the arrival at 0 */
            many[2 * i + 1] = 0;
        }
        next_value = 0;
        def_super_start(1);
        def_super_set_points(many, 9, 0, 0);
        check("of nine candidates, the one two thirds of the way by distance (600 of 0..800), not the farthest",
              def_super_step(0x203, 600, 0) == 1 && def_super_battle() == 6);
        def_super_battle_end(0, 100);
        def_super_set_points(many, 0, 55, 66);
        check("no candidate: it is found where the player stands", def_super_step(def_super_map(), 55, 66) == 1);
        def_super_cancel();
    }

    for (i = 0; i < 200; i++) {
        def_super_crystal_add(0, 1);
    }
    check("crystals stop at their cap", def_super_crystals(0) == DEF_SUPER_CRYSTAL_CAP);

    def_super_reset();
    memset(rec, 0xAA, sizeof(rec));
    def_super_record_write(rec);
    for (i = 0; i < DEF_SUPER_RECORD && rec[i] == 0; i++) {
    }
    check("nothing to keep writes all zeros", i == DEF_SUPER_RECORD && def_super_record_empty());

    memset(rec, 0, sizeof(rec));
    rec[0] = 1;            /* hunt 0 active */
    rec[2] = 0x03;         /* on 0x203: another sector's map (a table that changed) */
    rec[3] = 0x02;
    rec[4] = 0x10;         /* and more HP lost than it has */
    rec[5] = 0x27;
    rec[8] = 250;
    next_value = 0;
    def_super_record_read(rec);
    check("a record from another table: the target heals and moves into its territory; counts clamped",
          def_super_active() == 0 && def_super_hp_left() == 10000 && def_super_in_territory(def_super_map()) &&
              def_super_crystals(0) == DEF_SUPER_CRYSTAL_CAP);

    def_super_reset();
    memset(rec, 0, sizeof(rec));
    rec[1] = 2; /* two hunts finished: hunt 2 open */
    def_super_record_read(rec);
    next_value = 0;
    check("a hunt of one area: its target only on that area's maps (0x201, 0x202), the compass there and round it",
          def_super_start(2) && (def_super_map() == 0x201 || def_super_map() == 0x202) &&
              def_super_in_territory(0x201) && def_super_in_territory(0x202) && !def_super_in_territory(0x200) &&
              !def_super_in_territory(0x204) && def_super_in_region(0x01) && !def_super_in_region(0x00) &&
              !def_super_in_region(0x81) && def_super_temperature(0x200, 0, 0) != DEF_TEMP_NONE &&
              def_super_in_compass_zone(0x204));
    def_super_battle_end(0, 100);
    for (i = 0; i < 20; i++) {
        next_value = i;
        def_super_step(def_super_map(), 0, 0);
        def_super_set_points(points, 0, 0, 0);
        def_super_step(def_super_map(), 0, 0);
        def_super_battle_end(0, 400);
        if (def_super_map() != 0x201 && def_super_map() != 0x202) {
            break;
        }
    }
    check("fleeing, it stays on its area's maps", i == 20);
    def_super_cancel();

    check("the story's flags: none in a new game, kept by the record, and they make it not empty",
          def_super_flags() == 0);
    def_super_reset();
    def_super_flag_set(DEF_SUPER_FLAG_MET);
    def_super_flag_set(DEF_SUPER_FLAG_TUTORIAL);
    check("a record with only the flags is not empty", !def_super_record_empty());
    def_super_record_write(rec);
    def_super_reset();
    def_super_record_read(rec);
    check("the flags come back from the record", def_super_flags() == (DEF_SUPER_FLAG_MET | DEF_SUPER_FLAG_TUTORIAL));

    def_super_crystal_add(1, 2);
    check("a crystal sold gives its price and one fewer; none held gives nothing",
          def_super_crystal_sell(1) == 12000 && def_super_crystals(1) == 1 && def_super_crystal_sell(2) == 0 &&
              def_super_crystal_sell(-1) == 0 && def_super_crystal_sell(DEF_SUPER_COUNT) == 0);

    /* Tutorial Ogremon: player cannot flee, though Ogremon can flee (unless blessed Blast); non-tutorial allows flee
     * with buff tradeoff and never vanishes after 10 flees, reporting who fled. */
    def_super_reset();
    next_value = 0;
    def_super_start(0);
    def_super_step(def_super_map(), 0, 0);
    def_super_set_points(points, 3, 20, 20);
    def_super_step(def_super_map(), 300, 200);
    check("tutorial Ogremon blocks player flee while target flee and precision proximity work",
          def_super_tutorial_battle() && def_hunt_flee() == 0 &&
              def_super_proximity(def_super_map(), 300, 200) == 100 &&
              def_super_proximity(def_super_map(), 220, 150) > def_super_proximity(def_super_map(), 50, 50) &&
              def_super_proximity(def_super_map(), 50, 50) > def_super_proximity(def_super_map(), 20, 20) &&
              def_super_proximity(0x280, 0, 0) == -1);
    def_super_flee_note(DEF_SUPER_FLED_TARGET);
    def_super_battle_end(0, 360);
    check("target flee in tutorial records DEF_SUPER_FLED_TARGET once",
          def_super_fled_take() == DEF_SUPER_FLED_TARGET && def_super_fled_take() == DEF_SUPER_FLED_NONE);
    def_super_cancel();

    /* Non-tutorial Super Rare: player fleeing 15 times keeps HP, caps meets at DEF_SUPER_MEETS_CAP, never vanishes */
    def_super_reset();
    memset(rec, 0, sizeof(rec));
    rec[1] = 1;
    def_super_record_read(rec);
    next_value = 0;
    def_super_start(1);
    for (i = 0; i < 15; i++) {
        next_value = i;
        def_super_step(def_super_map(), 0, 0);
        def_super_set_points(points, 0, 0, 0);
        def_super_step(def_super_map(), 0, 0);
        check("player can flee non-tutorial Super Rare", def_hunt_flee() == 1);
        def_super_flee_note(DEF_SUPER_FLED_PLAYER);
        def_super_battle_end(0, 18000);
    }
    check("after 15 player flees, Super Rare stays active, keeps HP, caps meets at DEF_SUPER_MEETS_CAP, notes player flee",
          def_super_active() == 1 && def_super_hp_left() == 18000 && def_super_meets() == DEF_SUPER_MEETS_CAP &&
              def_super_fled_take() == DEF_SUPER_FLED_PLAYER && def_super_fled_take() == DEF_SUPER_FLED_NONE);
    def_super_cancel();

    on = 0;
    def_super_reset();
    draws = 0;
    check("in a replay script no hunt starts and nothing is drawn", !def_super_start(0) && draws == 0 &&
                                                                       !def_super_step(0x201, 300, 200));
    return failures != 0;
}
