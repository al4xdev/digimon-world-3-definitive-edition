/* The Time Room (owner, 2026-11-17; .plan/tasks/velvet-historia.md): the gatekeeper before the point of no return.
 *
 * Step 40: in the Master Room of Asuka (map 0x288, WSTAG311) Megadeath's voice opens the Warp Gate to the Gunslinger
 * (a map event of type 14 to map 0x2DD), and nothing on the Gunslinger leads back. The port holds the gate shut until
 * the player has passed the Time Room, so no one is sent into the last area unready: stepping onto it, Igor's voice
 * stops the player (with no Chosen Evolution yet, it tells them to finish Gennai's road first, or to prove their own
 * strength), and a blue door of the Velvet Room stands beside it.
 *
 * Through that door the Velvet Room is the Time Room: Igor and the three challengers, Oikawa, the Sepikmon and the old
 * man (the real Gennai once he came, Myotismon in his shape before: def_record_blast_quest), and time stands still.
 * The room's upper door (shut on other visits) leads to the trials: three areas of the game, the room's blue behind
 * them, a challenger in each on his own ground (the figure the area's stage places, at its spot and in its VRAM place),
 * fought in turn. A trial is one battle, the challenger's three members one after the other (def_time_trials, a boss
 * battle's kind: no escape). A won trial skips the results screen (no XP, no Bits: there is no prize) and the next
 * area follows at once; a lost one is no game over (the party as it went in, as the DRI's simulations) and, as walking
 * out of an area, sends the player back to the room with the run reset. The third win opens the gate.
 *
 * The run is the port's RAM (a save state carries it); the save keeps the gate's bits (DEF_STORY_GATE_*). Off in a
 * replay script (def_on). */
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "fieldstg.h"
#include "fightstg.h"
#include "gamestate.h"
#include "heap.h"
#include "records.h"
#include "sound.h"

#include "psxstack/savestate.h"

#include "definitive.h"
#include "port.h"
#include "port_runtime.h"

extern int def_on(void);
extern void fieldstg_start_battle(s32 battle);

#define TM_GATE_MAP 0x288   /* the Master Room of Asuka (WSTAG311): Megadeath opens the Warp Gate there */
#define TM_GATE_STEP 0x28   /* step 40: the gate is open (event 982) */
#define TM_WARP_TYPE 14     /* its map event: a launch to the Gunslinger */
#define TM_WARP_TO 0x2DD
#define TM_DOOR_X 160       /* the Velvet Room's door: the nook at the left of the gate's platform, off the way up */
#define TM_DOOR_Y 440       /* from the stairs (the platform x 216..392, y 384..448; the gate x 268..312, y 412..432) */
#define TM_BACK_X 184       /* where the room's front door leaves the player: beside the door, not on it */
#define TM_BACK_Y 448
#define TM_ROOM_MAP 0x20F   /* the Velvet Room's stage (definitive_velvet.c VL_MAP) */
#define TM_ROOM_X 215       /* its entry, inside the front door */
#define TM_ROOM_Y 335
#define TM_SIDE_DOOR 0x20A  /* where the room's upper door goes in the game (the bar's other door) */
#define TM_TALK05 0xE8      /* the Velvet Room's talk file: the trials' areas talk from it too */
#define TM_TALK03 0xDA      /* map 0x288's */
#define TM_STAGE 17         /* the trials' battle scenery and music: Megadeath's (the Gunslinger's) */
#define TM_MUSIC 0x608C0000
#define TM_SND_PORTAL 0x40004
#define TM_ROOM_GUESTS 4

/* Their lines: TALK05 entries the disc leaves as the letter "a" (no stage reaches them), and two of TALK03's. */
enum {
    TM_IGOR_MEET = 47,   /* +answer to Act II's question - 1 (47..49); 50 not asked */
    TM_IGOR_WAIT = 51,   /* talked to again */
    TM_IGOR_RESET = 52,  /* back from a lost or abandoned run */
    TM_IGOR_PASSED = 53, /* the three won */
    TM_ROOM_GENNAI = 54, /* the challengers in the room */
    TM_ROOM_MYOTIS = 55,
    TM_ROOM_SEPIK = 56,
    TM_ROOM_OIKAWA = 57,
    TM_FIGHT = 58,       /* +trial: before the battle (58..61) */
    TM_WON = 62,         /* +trial: after it (62..65) */
    TM_WARN_DARK = 311,  /* TALK03: the gate held, no Chosen Evolution */
    TM_WARN = 312        /* and with it */
};

/* The trials' areas (owner, 2026-11-17: regions of the game, the room's blue behind them): the area's map, the
 * challenger's figure (actor ID; VRAM place 4, the slot the area's own first townsfolk take, free while the trial
 * leaves them out: a slot no figure of the stage uses shows garbage, as the bar's did) and where he stands, facing the
 * player, who comes in two steps in front of him. Spots measured on the attribute layer (DW3_TIME_TEST's survey): open
 * ground, far from the area's exits. */
static const struct {
    s16 map, actor, place, x, y, dir, px, py, pdir;
} tm_areas[DEF_TIME_ROUNDS] = {
    { 0x202, 42, 4, 440, 392, 1, 418, 414, 5 }, /* Oikawa: mid-span on the Asuka Bridge (WSTAG205) */
    { 0x26F, 61, 4, 300, 480, 1, 278, 502, 5 }, /* the Sepikmon: Genbu City's west square (WSTAG760) */
    { 0x258, 57, 4, 570, 470, 1, 548, 492, 5 }, /* the old man: the middle of the Mobius Desert (WSTAG635) */
};

enum { TM_AFTER_NONE, TM_AFTER_FIGHT, TM_AFTER_WON };

/* The run. No pointers: a save state carries it. */
static struct {
    s32 on;      /* the Velvet Room is the Time Room this visit */
    s32 round;   /* 0 the room, 1..DEF_TIME_ROUNDS the trial area of that round */
    s32 moving;  /* the run itself goes to the next area (def_time_leave lets it) */
    s32 greeted; /* Igor told the room this visit */
    s32 reset;   /* the run was lost or left: Igor says so */
    s32 after;   /* what the challenger's closing line leads to (TM_AFTER_*) */
    s32 pending, fighting, won, lost; /* the trial's battle, as the DRI's simulation */
    s32 touch, touching; /* the frame the Warp Gate was last stood on; the player was on it */
    s16 party[3], hp[3], mp[3];
} tm;

static FieldstgTalk tm_talks[2] = { { NULL, NULL, TM_FIGHT }, { NULL, NULL, 0 } };
static FieldstgPlacedActor tm_npc;
static FieldstgPlacedActor *tm_npc_list[2];

static int gate_open_step(void) {
    return def_on() && gamestate_data.progress >= TM_GATE_STEP && gamestate_data.progress < 0x2B;
}

int def_time_on(void) {
    return def_on() && tm.on;
}

/* The trial table's row of round `r` (1..): the old man's is Myotismon's while the Blast quest still deceives. */
static int trial_of(int r) {
    if (r == DEF_TIME_ROUNDS && def_record_blast_quest() == DEF_BLAST_QUEST_DECEIVED) {
        return DEF_TIME_ROUNDS;
    }
    return r - 1;
}

int def_time_door(int map, int *x, int *y) {
    if (map != TM_GATE_MAP || !gate_open_step()) {
        return 0;
    }
    *x = TM_DOOR_X;
    *y = TM_DOOR_Y;
    return 1;
}

void def_time_enter(void) {
    memset(&tm, 0, sizeof(tm));
    tm.on = 1;
    port_log("time: frame %ld: the Time Room's door taken (gate %s)", port_frames,
             def_record_story(DEF_STORY_GATE_PASSED) ? "passed" : "shut");
}

/* Where the player comes in on round `r`'s area. */
static void area_spots(int r, s32 *x, s32 *y, s32 *dir) {
    *x = tm_areas[r - 1].px;
    *y = tm_areas[r - 1].py;
    *dir = tm_areas[r - 1].pdir;
}

/* A map change while the Time Room runs (dw3_velvet_leave, before its own): 0 not the Time Room's; 1 the run goes on
 * (still the room's: the trials' areas are); 2 back out to the gate. The upper door starts a run; a trial's area left
 * by any of its own ways, not the run's, goes back to the room, the run reset. */
int def_time_leave(int *map, int *x, int *y, int *dir) {
    s32 here = gamestate_data.funcs.get_map(), ax, ay, ad;

    if (!def_time_on()) {
        return 0;
    }
    if (tm.moving) { /* the run's own step: the area or the room it named */
        tm.moving = 0;
        return 1;
    }
    if (*map == TM_ROOM_MAP && (here == TM_ROOM_MAP || tm.round == 0)) {
        return 1; /* the room entered, from the gate's door or again (a cut): the Time Room */
    }
    if (here == TM_ROOM_MAP && *map == TM_SIDE_DOOR) {
        tm.round = 1;
        tm.reset = 0;
        def_record_set_story_gate(0);
        area_spots(1, &ax, &ay, &ad);
        *map = tm_areas[0].map;
        *x = ax << 8;
        *y = ay << 8;
        *dir = ad;
        sound_module.play(TM_SND_PORTAL);
        port_log("time: frame %ld: the upper door: trial 1 (map 0x%X)", port_frames, *map);
        return 1;
    }
    if (here == TM_ROOM_MAP) { /* the front door: back to the gate, in front of its door */
        memset(&tm, 0, sizeof(tm));
        *map = TM_GATE_MAP;
        *x = TM_BACK_X << 8;
        *y = TM_BACK_Y << 8;
        *dir = 6;
        port_log("time: frame %ld: out of the Time Room, back to the gate", port_frames);
        return 2;
    }
    if (tm.round <= 0) {
        return 0;
    }
    /* a trial's area left by its own way: the run is over */
    tm.round = 0;
    tm.reset = 1;
    tm.greeted = 1;
    def_record_set_story_gate(0);
    *map = TM_ROOM_MAP;
    *x = TM_ROOM_X << 8;
    *y = TM_ROOM_Y << 8;
    *dir = 1;
    port_log("time: frame %ld: a trial's area left: back to the room, the run reset", port_frames);
    return 1;
}

/* The run's own way to `map` (the next area, or the room). */
static void run_goto(s32 map, s32 x, s32 y, s32 dir) {
    tm.moving = 1;
    sound_module.play(TM_SND_PORTAL);
    fieldstg_goto_map(map, -1, x << 8, y << 8, dir);
}

/* The room's figures: Igor and the three challengers, where the room places them (definitive_velvet.c). */
FieldstgPlacedActor **def_time_room_actors(void) {
    extern FieldstgPlacedActor *dw3_velvet_figure(s32 code);
    static FieldstgPlacedActor *list[TM_ROOM_GUESTS + 1];

    list[0] = dw3_velvet_figure(DW3_VELVET_PULL_IGOR);
    list[1] = dw3_velvet_figure(DW3_VELVET_PULL_MENTOR);
    list[2] = dw3_velvet_figure(DW3_VELVET_PULL_SEPIK);
    list[3] = dw3_velvet_figure(DW3_VELVET_PULL_MERCHANT);
    list[4] = NULL;
    return list;
}

FieldstgPlacedActor *def_time_challenger(void) {
    return def_time_on() && tm.round > 0 ? &tm_npc : NULL;
}

int def_time_side_door_open(void) {
    return def_time_on() && !def_record_story(DEF_STORY_GATE_PASSED);
}

/* The end of a trial area's stage setup (fieldstg_manager_start): the area as the trial has it. Only the challenger
 * stands there, talking from the room's file; no random battles; the room's music. The map's own events stay (its
 * stairs and layers), its exits lead back to the room (def_time_leave). */
void def_time_stage_setup(void) {
    extern void dw3_velvet_room_music(s32 *music, s32 *sound);
    int r = tm.round;

    if (!def_time_on() || r <= 0 || gamestate_data.funcs.get_map() != tm_areas[r - 1].map) {
        return;
    }
    tm_npc.flags_required = NULL;
    tm_npc.talks = tm_talks;
    tm_npc.id = tm_areas[r - 1].actor;
    tm_npc.vram_place = tm_areas[r - 1].place;
    tm_npc.x = tm_areas[r - 1].x;
    tm_npc.y = tm_areas[r - 1].y;
    tm_npc.dir = tm_areas[r - 1].dir;
    tm_npc_list[0] = &tm_npc;
    tm_npc_list[1] = NULL;
    fieldstg_stage.actors = tm_npc_list;
    fieldstg_stage.battle_lists = NULL;
    fieldstg_stage.talk_file = records_language + TM_TALK05;
    dw3_velvet_room_music(&fieldstg_stage.music, &fieldstg_stage.sound);
    port_log("time: trial %d's area (map 0x%X): the challenger %d at %d,%d", r, tm_areas[r - 1].map, tm_npc.id,
             tm_npc.x, tm_npc.y);
}

/* The line of a figure the player talks to while the Time Room runs: 0 when the figure is not the Time Room's. */
int def_time_talk(FieldstgActor *actor) {
    extern FieldstgPlacedActor *dw3_velvet_figure(s32 code);
    int m;

    if (!def_time_on() || actor == NULL) {
        return 0;
    }
    if (tm.round > 0 && actor->placed == &tm_npc) {
        int t = trial_of(tm.round);

        if (tm.won) {
            tm.won = 0;
            tm.after = TM_AFTER_WON;
            return TM_WON + t;
        }
        tm.after = TM_AFTER_FIGHT;
        return TM_FIGHT + t;
    }
    if (actor->placed == dw3_velvet_figure(DW3_VELVET_PULL_IGOR)) {
        if (def_record_story(DEF_STORY_GATE_PASSED)) {
            m = TM_IGOR_PASSED;
        } else if (tm.reset) {
            m = TM_IGOR_RESET;
            tm.reset = 0;
        } else if (!tm.greeted) {
            int a = def_record_story_answer(1); /* Act II: the burden taken (Igor's second dream) */

            m = a >= 1 && a <= 3 ? TM_IGOR_MEET + a - 1 : TM_IGOR_MEET + 3;
            tm.greeted = 1;
        } else {
            m = TM_IGOR_WAIT;
        }
        return m;
    }
    if (actor->placed == dw3_velvet_figure(DW3_VELVET_PULL_MENTOR)) {
        return def_record_blast_quest() == DEF_BLAST_QUEST_DECEIVED ? TM_ROOM_MYOTIS : TM_ROOM_GENNAI;
    }
    if (actor->placed == dw3_velvet_figure(DW3_VELVET_PULL_SEPIK)) {
        return TM_ROOM_SEPIK;
    }
    if (actor->placed == dw3_velvet_figure(DW3_VELVET_PULL_MERCHANT)) {
        return TM_ROOM_OIKAWA;
    }
    return 0;
}

/* A set battle (a trial, the Oikawa's arc): the template as fieldstg_start_battle sets it up, then the three enemy
 * members of `t`, its scenery and its music (fieldstg_start_battle leaves those as the last battle had them: garbage
 * right after a load, which crashed FIGHTSTG; `stage` < 0 keeps them only for a caller that set them itself). No escape. The party's HP and MP are kept in `party`,
 * `hp`, `mp` for a defeat that costs nothing (def_time_restore). 0 when the battle did not start. */
int def_time_set_battle(const DefTimeTrial *t, int stage, int music, short *party, short *hp, short *mp) {
    int i, id;

    fieldstg_start_battle(t->battle);
    if (!fieldstg_stage.battle_starting) {
        return 0;
    }
    if (stage >= 0) {
        records_state.stage = stage;
        records_state.music = music;
    }
    for (i = 0; i < DEF_DRI_STAGES; i++) {
        RecordsEnemy *e = &records_state.enemies[i];

        e->digimon = t->stages[i].digimon;
        e->level = t->stages[i].level;
        e->hp = t->stages[i].hp;
        e->mp = t->stages[i].mp;
        e->stat_scale = t->stages[i].stat_scale;
    }
    records_state.blocked[11] = 1; /* no escape */
    for (i = 0; i < 3; i++) {
        id = gamestate_data.funcs.get_party_member(i);
        party[i] = (s16)id;
        if (id >= 0) {
            hp[i] = gamestate_data.digimon[id].record.stats.values[2];
            mp[i] = gamestate_data.digimon[id].record.stats.values[4];
        }
    }
    return 1;
}

/* The party as it went into a set battle (a defeat that is no game over). */
void def_time_restore(const short *party, const short *hp, const short *mp) {
    int i, id;

    for (i = 0; i < 3; i++) {
        id = party[i];
        if (id >= 0 && id == gamestate_data.funcs.get_party_member(i)) {
            gamestate_data.digimon[id].record.stats.values[2] = hp[i];
            gamestate_data.digimon[id].record.stats.values[4] = mp[i];
        }
    }
}

/* The trial's battle: the challenger's three, on Megadeath's ground; no prize. */
static int fight(void) {
    const DefTimeTrial *t = &def_time_trials[trial_of(tm.round)];

    if (!def_time_set_battle(t, TM_STAGE, TM_MUSIC, tm.party, tm.hp, tm.mp)) {
        return 0;
    }
    records_state.has_prize = 0;
    tm.pending = 1;
    tm.fighting = tm.won = tm.lost = 0;
    port_log("time: frame %ld: trial %d (row %d) begins: %d, %d, %d at %d, %d, %d", port_frames, tm.round,
             trial_of(tm.round), t->stages[0].digimon, t->stages[1].digimon, t->stages[2].digimon,
             t->stages[0].level, t->stages[1].level, t->stages[2].level);
    return 1;
}

/* After a won trial's closing line: the next area, or the room with the gate open. */
static void advance(void) {
    s32 x, y, dir;

    def_record_set_story_gate(tm.round);
    if (tm.round >= DEF_TIME_ROUNDS) {
        def_record_set_story(DEF_STORY_GATE_PASSED);
        tm.round = 0;
        tm.greeted = 1;
        port_log("time: frame %ld: the three trials won: the Warp Gate opens", port_frames);
        run_goto(TM_ROOM_MAP, TM_ROOM_X, TM_ROOM_Y, 1);
        return;
    }
    tm.round++;
    area_spots(tm.round, &x, &y, &dir);
    port_log("time: frame %ld: on to trial %d (map 0x%X)", port_frames, tm.round, tm_areas[tm.round - 1].map);
    run_goto(tm_areas[tm.round - 1].map, x, y, dir);
}

/* Each field frame while the Velvet Room runs (dw3_velvet_draw): the challenger's line closed (his battle, or the next
 * area), a trial's outcome back on its area (lost: the room, the run reset; won: the challenger calls the player over). */
void def_time_frame(void) {
    extern void dw3_velvet_call(s32 code);

    FieldstgActor *player;

    if (!def_time_on()) {
        return;
    }
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player == NULL || player->base.state != OBJECT_STATE_RUN || player->control == NULL) {
        return; /* the stage is still setting up, or a map change is under way */
    }
    if (tm.after != TM_AFTER_NONE && fieldstg_stage.actor_busy == 0 && fieldstg_stage.event_running == 0) {
        s32 after = tm.after;

        tm.after = TM_AFTER_NONE;
        if (after == TM_AFTER_FIGHT) {
            fight();
        } else {
            advance();
        }
        return;
    }
    if (tm.round <= 0 || fieldstg_stage.battle_starting) {
        return;
    }
    if (tm.lost) { /* back on the area from a lost trial: the room, the run reset */
        tm.lost = 0;
        tm.round = 0;
        tm.reset = 1;
        tm.greeted = 1;
        def_record_set_story_gate(0);
        port_log("time: frame %ld: the trial lost: back to the room, the run reset", port_frames);
        run_goto(TM_ROOM_MAP, TM_ROOM_X, TM_ROOM_Y, 1);
        dw3_velvet_call(DW3_VELVET_PULL_IGOR);
        return;
    }
    if (tm.won == 1) { /* back from a won trial: the challenger calls the player over */
        tm.won = 2;
        dw3_velvet_call(DW3_VELVET_PULL_TRIAL);
    }
}

void def_time_battle_begin(void) {
    tm.fighting = 0;
    if (!tm.pending) {
        return;
    }
    tm.pending = 0;
    if (def_time_on() && records_state.enemies[0].digimon == def_time_trials[trial_of(tm.round)].stages[0].digimon) {
        tm.fighting = 1;
        port_log("time: trial %d: the battle begins", tm.round);
    }
}

void def_time_battle_end(int result) {
    if (!tm.fighting) {
        return;
    }
    tm.fighting = 0;
    tm.won = result == 1;
    tm.lost = result != 1;
    port_log("time: trial %d ended: result %d", tm.round, result);
}

/* WFIGHTMN, a won battle: 1 when it was a trial (no results screen: there is no prize). */
int def_time_won(void) {
    return def_time_on() && tm.won == 1;
}

/* WFIGHTMN, a lost battle: 1 when it was a trial (no game over: back to the area, the party as it went in). It asks
 * every frame until the map changes, so it stays 1 until the field takes the outcome. */
int def_time_defeat(void) {
    if (tm.fighting) {
        def_time_battle_end(2);
    }
    if (!def_time_on() || !tm.lost) {
        return 0;
    }
    def_time_restore(tm.party, tm.hp, tm.mp);
    return 1;
}

int def_time_fighting(void) {
    return tm.fighting;
}

/* ---- The gate itself, on map 0x288. */

/* The Warp Gate's map event (FIELDSTG's check, dw3_velvet_event_closed): shut until the trials are passed. The check
 * runs while the player stands on it, which is how the warning knows. */
int def_time_warp_closed(int type, int param) {
    if (type != TM_WARP_TYPE || param != TM_WARP_TO || !gate_open_step() ||
        gamestate_data.funcs.get_map() != TM_GATE_MAP || def_record_story(DEF_STORY_GATE_PASSED)) {
        return 0;
    }
    tm.touch = (s32)port_frames;
    return 1;
}

/* A test's way in (DW3_TIME_TEST="map:x:y:step", hex or decimal): at the first idle frame, the story at `step` and the
 * player on `map` at x,y. Step 0x2B also stands for event 1080's end (Galacticmon held for Igor's last dream). */
static void test_warp(void) {
    static int phase;
    static long v[4];
    const char *e;
    char *end;
    int i;

    if (phase == 2) {
        return;
    }
    if (phase == 0) {
        phase = 2;
        if ((e = getenv("DW3_TIME_TEST")) == NULL) {
            return;
        }
        for (i = 0; i < 4; i++) {
            v[i] = strtol(e, &end, 0);
            e = *end == ':' ? end + 1 : end;
        }
        phase = 1; /* the map first, the step once there: no other step's hook fires on the way */
        port_log("time: test: map 0x%lX at %ld,%ld, then step 0x%lX", v[0], v[1], v[2], v[3]);
        fieldstg_goto_map(v[0], -1, v[1] << 8, v[2] << 8, 0);
        return;
    }
    if (gamestate_data.funcs.get_map() != v[0]) {
        return;
    }
    phase = 2;
    if (gamestate_data.progress == v[3] && v[3] != 0x2B) {
        return; /* already at that step: the stage set up as it should */
    }
    gamestate_data.progress = v[3];
    if (v[3] == 0x2B) {
        def_story_hold_galacticmon();
    }
    port_log("time: test: step 0x%lX, map 0x%lX at %ld,%ld", v[3], v[0], v[1], v[2]);
    fieldstg_goto_map(v[0], -1, v[1] << 8, v[2] << 8, 0); /* again, so the stage sets up at that step */
}

s32 def_time_test_go[2]; /* a test's spot to set the player down at (DW3_TIME_TEST only) */

/* A test's survey of the map it lands on (DW3_TIME_TEST): where each map event's cells lie (attribute layer 7), and a
 * coarse grid of the free ground around the first area found, to place a door or a spawn. */
static void test_survey(void) {
    static s32 seen = -1;
    s32 map = gamestate_data.funcs.get_map(), w, h, x, y, k;
    s32 box[32][4];
    FieldstgPos pos;
    char row[128];

    if (getenv("DW3_TIME_TEST") == NULL) {
        return;
    }
    if (def_time_test_go[0] != 0) { /* the test pokes a spot: the player is set down there (the pad is slow) */
        FieldstgActor *player = (FieldstgActor *)heap_objects.find(5, -1, 0);

        if (player != NULL) {
            player->pos.x = def_time_test_go[0] << 8;
            player->pos.y = def_time_test_go[1] << 8;
            player->pixel_pos.x = def_time_test_go[0];
            player->pixel_pos.y = def_time_test_go[1];
            port_log("time: test: set down at %d,%d", def_time_test_go[0], def_time_test_go[1]);
        }
        def_time_test_go[0] = 0;
    }
    if (port_frames % 30 == 0) {
        FieldstgActor *player = (FieldstgActor *)heap_objects.find(5, -1, 0);

        if (player != NULL) {
            port_log("time: test player at %d,%d dir %d", player->pixel_pos.x, player->pixel_pos.y, player->dir);
        }
    }
    if (map == seen) {
        return;
    }
    seen = map;
    w = fieldstg_attr.width * 128;
    h = fieldstg_attr.height * 128;
    for (k = 0; k < 32; k++) {
        box[k][0] = box[k][1] = 0x7FFF;
        box[k][2] = box[k][3] = -1;
    }
    for (y = 0; y < h; y += 4) {
        for (x = 0; x < w; x += 4) {
            u8 a;

            pos.x = x;
            pos.y = y;
            a = fieldstg_attr.get(7, &pos);
            if (a == 0) {
                continue;
            }
            k = a & 0x1F;
            box[k][0] = x < box[k][0] ? x : box[k][0];
            box[k][1] = y < box[k][1] ? y : box[k][1];
            box[k][2] = x > box[k][2] ? x : box[k][2];
            box[k][3] = y > box[k][3] ? y : box[k][3];
        }
    }
    port_log("time: test survey of map 0x%X (%dx%d)", map, w, h);
    for (k = 0; k < 32; k++) {
        if (box[k][2] >= 0) {
            FieldstgMapEvent *ev = &fieldstg_stage.map_events[k];

            port_log("time: test   event %d type %d param 0x%X: x %d..%d y %d..%d", k, ev->type, ev->param, box[k][0],
                     box[k][2], box[k][1], box[k][3]);
        }
    }
    for (y = 0; y < h && y < 1024; y += 16) {
        for (x = 0, k = 0; x < w && k < 120; x += 8, k++) {
            pos.x = x;
            pos.y = y;
            row[k] = fieldstg_attr.get(gamestate_data.attr_layer, &pos) != 0 ? '.' : '#';
        }
        row[k] = 0;
        port_log("time: test grid y %4d %s", y, row);
    }
}

/* The field's idle frame on the gate's map (def_story_frame): stepping onto the shut gate, Igor's voice stops the
 * player (once per step onto it). `say` starts a story talk of the map's talk file; 1 when it did. */
void def_time_gate_frame(int (*say)(const int *messages, int n)) {
    s32 on;

    test_warp();
    test_survey();
    if (!gate_open_step() || gamestate_data.funcs.get_map() != TM_GATE_MAP || def_record_story(DEF_STORY_GATE_PASSED)) {
        tm.touching = 0;
        return;
    }
    on = (s32)port_frames - tm.touch <= 2;
    if (on && !tm.touching) {
        s32 line = def_record_blast_quest() == DEF_BLAST_QUEST_DONE ? TM_WARN : TM_WARN_DARK;

        if (!say(&line, 1)) {
            return; /* the next free frame */
        }
        def_record_set_story(DEF_STORY_GATE_WARNED);
        port_log("time: frame %ld: the Warp Gate held shut (line %d)", port_frames, line);
    }
    tm.touching = on;
}

/* The Time Room's entries: served in every language (English until translated). */
int def_time_text_own(int base_id, int index) {
    if (base_id == TM_TALK05) {
        return index >= TM_IGOR_MEET && index <= TM_WON + DEF_TIME_TRIALS - 1;
    }
    return base_id == TM_TALK03 && (index == TM_WARN_DARK || index == TM_WARN);
}

void def_time_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, tm);
    PORT_STATE_VAR(s, tm_npc);   /* its talks point at tm_talks, in the same binary */
    PORT_STATE_VAR(s, tm_npc_list);
    PORT_STATE_VAR(s, tm_talks);
}
