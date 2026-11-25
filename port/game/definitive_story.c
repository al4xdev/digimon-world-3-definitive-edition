/* The Velvet Room in the main story (owner, 2026-11-17): the hooks that send the
 * player into the room's arcs at set story steps (gamestate_data.progress), and the talks they show.
 *
 * A story talk is the game's own talk balloon (fieldstg_dialog_create_talk, the stage's talk file) on the player, with
 * no actor talked to (the speaker's name plate tells who speaks): the hook asks for a message of the current stage's talk file whose disc text is the unused
 * letter "a", and the port's text override (tools/text_override, def_story_text_own) fills it. Several messages run one
 * after the other; the field is held (event_running) until the last closes and the confirm button is let go.
 *
 * The Velvet Room grows with the story (owner, 2026-11-17): its doors stay closed until step 4, and its guests come in
 * one level at a time (def_story_velvet_level), the Black Market last. Each new guest calls the player over on the
 * first visit after it came (definitive_velvet.c).
 *
 * Step 4, "The Seiryu Badge": the Guide sends the player to ask around Seiryu City. Talked to on the stairs of the
 * city's inn, Repeating Tom says the Seiryu Leader is gone (his own line, said otherwise) and the player feels
 * something wrong; then the Velvet Room's blue door opens on those stairs (the level becomes 1). Through it, Igor alone
 * shows the room again; the screen cuts, and the Sepikmon is there in his place, with the hunt that finds the Leader.
 * Until the Ogremon hunt (the Super Rare tutorial, definitive_superhunt.c) is reported to him, Tom says the Leader is
 * gone and the Guide's main quest is the missing Leader. The report tells where the Leader is (the Protocol Ruins, as
 * the game has it), and the story goes on as the game's own.
 *
 * Step 0, "Digimon Online" (WSTAG785, map 0x2D8): the opening scene is cut in two where Junior cannot pick a starter
 * pack (after Ivy's "choose, and let's go", event text 12). The first half ends in the port's script command 0x7F00,
 * which pulls him into the Velvet Room, where Igor gives him the key; the room sends him back, and the stage starts the
 * scene again (the story is still at step 0), now from the cut: everyone put back where the first half left them, Ivy
 * and Teddy asking why he stood there like a statue (event texts 1..4 the first half already said, told anew), and the
 * game's own scene from Ivy's "You still haven't made up your mind?" on.
 *
 * Igor (the room's figure, definitive_velvet.c) reads the party's DNA forms after that: for each partner, the form of
 * its learn list that joins two other forms (stfgtrep_learn_lists, as the Digi-Lab's Next Forms panel reads them) whose
 * two bonds are the closest to done, and how far each is. Off in a replay script (def_on). */
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "cdload.h"
#include "fieldstg.h"
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
#include "port.h"
#include "port_runtime.h"
#include "psxstack/platform.h"

extern int def_on(void);
extern void *fieldstg_dialog_create_talk(FieldstgActor *actor, s32 message);
extern void *font_get_entry(void *data, s32 i);

#define ST_KEYS ((1 << 12) | (1 << 13) | (1 << 14)) /* the buttons a talk is closed with */
#define ST_MAX 4                                     /* messages of one story talk */

/* Seiryu City: world map area 0x0E of the first server (maps 0x22E..0x231), whose stages talk from TALK01 (0xCC). */
#define ST_SEIRYU_STEP 4
#define OK_MAP 0x245   /* the Oikawa's arc: the Bug Maze Pit's hall, step 27 (see oikawa_hook) */
#define OK_STEP 27
#define OK_TALK06 0xEF /* its talk file */
#define OK_STAGE 10          /* his battle's scenery and music: Bulbmon's, in the same pit (WSTAG545's scripted 0); */
#define OK_MUSIC 0x60880000  /* fieldstg_start_battle leaves them as the last battle had them, unset after a load */
#define OK_CALL 5      /* TALK06's letter "a" entries (5..27 are, and no stage talks with them) */
#define OK_AGAIN 6
#define OK_BEATEN 7
#define ST_EAST_WIRE 0x222
#define ST_PROTOCOL_FOREST 0x225
#define ST_PROTOCOL_RUINS 0x226
#define ST_PROTOCOL_WARN 8   /* TALK06's letter "a" entry: voice in the head before Protocol Forest/Ruins */
#define ST_TALK01 0xCC
#define ST_TOM_LINE 79       /* Repeating Tom, in the city's inn: "Seiryu Leader is training at the Protocol Ruins" */
#define ST_SEIRYU_FELT 178   /* TALK01's letter "a" no stage talks with: the player's feeling after Tom's news */
#define ST_TALK05 0xE8       /* the Velvet Room's (the bar's) talk file */
#define ST_SEPIK_REPORT 16   /* the Sepikmon: the hunt reported, the Leader found */
#define ST_SEPIK_CALLED 17   /* his first meeting while the Leader is missing */
#define ST_IGOR_KEY 18       /* Igor at the opening: the key */
#define ST_IGOR_READ 19      /* Igor reads the party's bonds (built as he is talked to: st_igor_text) */
#define ST_IGOR_SEIRYU 20    /* Igor shows the room again, at Seiryu */

/* Step 0: WSTAG785's opening (event 1) and its text (event text file 0x14A's table 1, as def_text_override names a
 * DMG container's table: the file | (table + 1) << 16). */
#define ST_OPENING_MAP 0x2D8
#define ST_OPENING_EVENT 1
#define ST_OPENING_TEXT (0x14A | (2 << 16))
#define ST_OPENING_CUT 12    /* the scene is cut after this text (Ivy: "Come on, choose") */
#define ST_OPENING_RESUME 13 /* and the game's own goes on from this one (Ivy: "You still haven't made up your mind?") */
#define ST_JUNIOR 1          /* the scene's actors: Junior (the player), Ivy, Teddy */
#define ST_IVY 11
#define ST_TEDDY 12
#define ST_CMD_DREAM 1       /* the port's command 0x7F00 1: into the Velvet Room */
#define ST_SCRIPT_MAX 1400   /* words of a cut script (the opening is 1015) */
#define ST_ACTORS 8

/* The opening's two halves, cut from the stage's own script when it is first started (the overlay's data is the same
 * every time: they are cut again after a load). */
static s16 st_first[ST_SCRIPT_MAX], st_second[ST_SCRIPT_MAX];
static s16 *st_cut_from;
static struct {
    s16 x, y, dir; /* Junior where the first half left him: the room sends him back there */
} st_junior;

/* The talk running. The dialog is an object of the field's heap, so a save state carries it with the heap; a map left
 * under it (only a debug warp can: the talk holds the field) frees it with the heap, so the talk is dropped then. */
static struct {
    void *dialog;
    s32 map;
    s32 messages[ST_MAX];
    s32 count, next;
    s32 releasing;
    s32 tom_told; /* Repeating Tom's news is on screen: the player's feeling and the door come after it */
    s32 speaker;  /* the actor the balloon is on (its ID), 0: the player */
    s32 protocol_touch, protocol_touching; /* blocked Protocol Ruins gate: voice in the head once per step */
} story;

static int oikawa_here(void);
static int oikawa_ahead(void);

static int idle(void) {
    return !fieldstg_stage.event_running && !fieldstg_stage.battle_starting && !fieldstg_stage.menu_open &&
           !fieldstg_stage.actor_busy && !fieldstg_stage.title_shown;
}

static FieldstgActor *player_ready(void) {
    FieldstgActor *player = (FieldstgActor *)heap_objects.find(5, -1, 0);

    if (player == NULL || player->base.state != OBJECT_STATE_RUN || player->base.step >= 5 || player->control == NULL) {
        return NULL;
    }
    return player;
}

/* Stops the player where it stands, as the game's own talk does (fieldstg_actor_talk_to): event_running alone only
 * stops the pad control, while the walk and run states keep moving the actor by its speed every frame
 * (fieldstg_actor_run_state), so a talk opened by stepping on something (a held gate) had the player run on under
 * the dialog. Setting step 1 stops movement without touching player->speed (which is only initialized once on actor
 * creation, so zeroing it would leave the player running in place at speed 0 after the talk). Ladders, jumps and
 * the other special states are left to finish. */
void def_field_freeze_player(void) {
    FieldstgActor *player = (FieldstgActor *)heap_objects.find(5, -1, 0);

    if (player == NULL) {
        return;
    }
    if (player->voice != -1) {
        sound_module.key_off(0xA0045F4A, player->voice);
        player->voice = -1;
    }
    if (player->base.step == 2 || player->base.step == 3) {
        ((void (*)(void *, s32))player->base.set_step)(player, 1); /* prototyped: clang (Windows, Android) refuses the bare call */
    }
}

/* Starts a story talk of `n` messages of the current stage's talk file, on `speaker` (an actor ID, 0 the player),
 * whatever holds the field. */
static void story_begin(const s32 *messages, int n, s32 speaker) {
    memcpy(story.messages, messages, n * sizeof(s32));
    story.speaker = speaker;
    story.count = n;
    story.next = 0;
    story.dialog = NULL;
    story.map = gamestate_data.funcs.get_map();
    fieldstg_stage.event_running = 1;
    def_field_freeze_player();
}

/* Starts a story talk on the player when the field is free: 1 when it began. */
static int story_talk(const s32 *messages, int n) {
    if (story.count != 0 || story.releasing || n <= 0 || n > ST_MAX || !idle() || player_ready() == NULL) {
        return 0;
    }
    story_begin(messages, n, 0);
    return 1;
}

static int seiryu_active(void) {
    return gamestate_data.progress == ST_SEIRYU_STEP && def_record_story(DEF_STORY_SEIRYU_CALL) &&
           !def_record_story(DEF_STORY_SEIRYU_DONE);
}

/* The opening's second half, while it runs (event texts 1..4 are Ivy's and Teddy's questions then). */
static int opening_return(void) {
    return gamestate_data.progress == 0 && def_record_story(DEF_STORY_IGOR) &&
           gamestate_data.funcs.get_map() == ST_OPENING_MAP;
}

/* Step 4 before Tom's news: the Guide sends the player to ask around Seiryu City. */
static int seiryu_ask(void) {
    return gamestate_data.progress == ST_SEIRYU_STEP && !def_record_story(DEF_STORY_SEIRYU_CALL) &&
           !def_record_story(DEF_STORY_SEIRYU_DONE);
}

int def_story_cond(int cond) {
    switch (cond) {
    case DEF_STORY_COND_SEIRYU:
        return seiryu_active();
    case DEF_STORY_COND_SEIRYU_ASK:
        return def_on() && seiryu_ask();
    case DEF_STORY_COND_OIKAWA:
        return def_on() && gamestate_data.progress == OK_STEP && !def_record_story(DEF_STORY_OIKAWA_BEATEN);
    case DEF_STORY_COND_MARKET:
        return def_on() && def_record_story(DEF_STORY_OIKAWA_BEATEN) && !def_record_story(DEF_STORY_MARKET_OPEN);
    case DEF_STORY_COND_TIME:
        return def_on() && !def_record_story(DEF_STORY_GATE_PASSED);
    }
    return 0;
}

/* The Velvet Room's level: who is in it (definitive_velvet.c's guests). A save from before the port's story is
 * placed by its step alone. Not a replay's: the room is off there. */
int def_story_velvet_level(void) {
    static const struct {
        s32 progress; /* the step it comes at */
        s32 level;
    } steps[] = {
        { 10, 2 }, /* "The Suzaku Badge": Gennai and his Blast quest (the opening drags: owner), and Igor */
        { 16, 3 }, /* "The real Leader": the Demon Bell's keeper */
        { 22, 4 }, /* "Free Asuka City": Etemon and the pig */
    };
    s32 p = gamestate_data.progress, level = 0, i;

    if (def_record_story(DEF_STORY_SEIRYU_CALL) || p > ST_SEIRYU_STEP) {
        level = 1; /* Repeating Tom's news: the Sepikmon */
    }
    for (i = 0; i < (s32)(sizeof(steps) / sizeof(steps[0])); i++) {
        if (p >= steps[i].progress && level < steps[i].level) {
            level = steps[i].level;
        }
    }
    /* the Black Market: the Oikawa beaten on the way out of the Bug Maze Pit (a save already past step 27 skipped him) */
    if (def_record_story(DEF_STORY_OIKAWA_BEATEN) || p > OK_STEP) {
        level = DEF_VELVET_LEVEL_MAX;
    }
    return level;
}

/* 1 when the next way into the room is Seiryu's: Tom told his news, Igor has not shown the room again yet. */
int def_story_room_intro(void) {
    return def_on() && def_record_story(DEF_STORY_SEIRYU_CALL) && !def_record_story(DEF_STORY_SEIRYU_ROOM) &&
           !def_record_story(DEF_STORY_SEIRYU_DONE);
}

/* The port's own talk entries (their disc text is the letter "a"): every language shows them (in English when it has
 * no translation), as the Velvet Room's (dw3_velvet_text_own). */
int def_story_text_own(int base_id, int index) {
    if (base_id == ST_TALK01) {
        return index == ST_SEIRYU_FELT;
    }
    if (base_id == OK_TALK06) {
        return index == OK_CALL || index == OK_AGAIN || index == OK_BEATEN || index == ST_PROTOCOL_WARN;
    }
    if (base_id == ST_TALK05) {
        return index == ST_SEPIK_REPORT || index == ST_SEPIK_CALLED || index == ST_IGOR_KEY || index == ST_IGOR_READ ||
               index == ST_IGOR_SEIRYU;
    }
    return 0;
}

static unsigned char st_igor_text[1024]; /* Igor's reading of the bonds (ST_IGOR_READ) */

/* A line of the disc said otherwise while a hook holds the story (def_text_override; `file` is a DMG container's table
 * as file | (table + 1) << 16): NULL keeps the usual one. */
const unsigned char *def_story_text(int file, int index) {
    extern unsigned char def_str_TomNews[], def_str_TomGone[], def_str_OpenIvy1[], def_str_OpenTeddy[],
        def_str_OpenJunior[], def_str_OpenIvy2[]; /* Kaili mode: Junior and Ivy swapped (def_language_kaili) */

    if (!def_on()) {
        return NULL;
    }
    {
        extern const unsigned char *def_finale_text(int file, int index);
        const unsigned char *fin = def_finale_text(file, index);
        if (fin != NULL) {
            return fin;
        }
    }
    if (file == ST_TALK01 && index == ST_TOM_LINE && seiryu_ask()) {
        story.tom_told = 1; /* his talk is the news: the player's feeling and the door follow it (def_story_frame) */
        return def_str_TomNews;
    }
    if (file == ST_TALK01 && index == ST_TOM_LINE && seiryu_active()) {
        return def_str_TomGone;
    }
    if (file == ST_TALK05 && index == ST_IGOR_READ && st_igor_text[0] != 0) {
        return st_igor_text;
    }
    if (file == ST_OPENING_TEXT && opening_return()) {
        switch (index) {
        case 1:
            return def_str_OpenIvy1;
        case 2:
            return def_str_OpenTeddy;
        case 3:
            return def_str_OpenJunior;
        case 4:
            return def_str_OpenIvy2;
        }
    }
    return NULL;
}

/* ---- The opening, cut in two (step 0). */

/* An event script command's length in words (fieldstg_event_update; docs/FORMATS.md "Event scripts"): 0 at the end. */
static int cmd_len(const s16 *w) {
    int hi = (w[0] >> 8) & 0xFF, lo = w[0] & 0xFF;

    switch (hi) {
    case 1:
        return lo == 2 ? 5 : 4;
    case 2:
        return 5;
    case 3:
        return lo == 1 ? 1 : lo == 4 ? 5 : 2;
    case 6:
        return lo == 1 ? 4 : 3;
    }
    return 0;
}

static int put(s16 *out, int n, const s16 *w, int len) {
    if (n + len > ST_SCRIPT_MAX - 1) {
        return n;
    }
    memcpy(out + n, w, len * sizeof(s16));
    return n + len;
}

/* Cuts the opening's script: 1 when both halves were made. The first half is the scene up to the end of the cut text's
 * pause, then 0x7F00 ST_CMD_DREAM; the second puts the actors (PLACE, an idle facing) and the camera back as the first
 * half left them, plays the rest of the scene up to the resume text, Ivy's and Teddy's questions before it, and the
 * rest. */
static int opening_cut(const s16 *script) {
    struct {
        s16 id, x, y, dir;
    } actors[ST_ACTORS];
    s16 camera[4] = { 0 };
    int n_actors = 0, cut = -1, resume = -1, end = -1, said = 0, i, k, len, n;

    for (i = 0; (len = cmd_len(&script[i])) != 0 && i < ST_SCRIPT_MAX - 8; i += len) {
        int hi = (script[i] >> 8) & 0xFF, lo = script[i] & 0xFF;

        if (hi == 2 && script[i + 2] == ST_OPENING_CUT) {
            said = 1;
        } else if (said == 1 && hi == 3 && lo == 1) {
            said = 2; /* its dialog closed: the pause after it is the cut */
        } else if (said == 2 && hi == 3 && lo == 0) {
            cut = i + len;
            said = 3;
        }
        if (hi == 2 && script[i + 2] == ST_OPENING_RESUME && resume < 0) {
            resume = i;
        }
        if (cut >= 0) {
            continue; /* the second half's positions are the cut's */
        }
        if (hi == 1 && (lo == 0 || lo == 2 || (lo == 1 && script[i + 1] < 0x320))) {
            for (k = 0; k < n_actors && actors[k].id != script[i + 1]; k++) {
            }
            if (k == n_actors && n_actors < ST_ACTORS) {
                actors[n_actors].id = script[i + 1];
                actors[n_actors].dir = 1;
                n_actors++;
            }
            if (k < n_actors) {
                if (lo != 1) {
                    actors[k].x = script[i + 2];
                    actors[k].y = script[i + 3];
                }
                if (lo == 2) {
                    actors[k].dir = script[i + 4];
                } else if (lo == 1) {
                    actors[k].dir = script[i + 3];
                }
            }
        } else if (hi == 6) {
            memcpy(camera, &script[i], len * sizeof(s16));
            camera[1] = 1; /* snapped there at once */
        }
    }
    end = i;
    if (cut < 0 || resume < cut || cmd_len(&script[end]) != 0) {
        port_log("story: the opening's script is not the one expected (cut %d, resume %d): left whole", cut, resume);
        return 0;
    }
    {
        static const s16 dream[] = { 0x7F00, ST_CMD_DREAM, FIELDSTG_EVENT_END };

        n = put(st_first, 0, script, cut);
        n = put(st_first, n, dream, 3);
    }
    n = 0;
    for (k = 0; k < n_actors; k++) {
        s16 place[] = { FIELDSTG_EVENT_PLACE(actors[k].id, actors[k].x, actors[k].y),
                        FIELDSTG_EVENT_ANIM(actors[k].id, 1, actors[k].dir) };

        n = put(st_second, n, place, 8);
        if (actors[k].id == ST_JUNIOR) {
            st_junior.x = actors[k].x;
            st_junior.y = actors[k].y;
            st_junior.dir = actors[k].dir;
        }
    }
    if (camera[0] != 0) {
        n = put(st_second, n, camera, cmd_len(camera));
    }
    {
        /* who asks, in the balloon side the scene gives each (mode 0) */
        static const s16 questions[] = {
            FIELDSTG_EVENT_WAIT(30),
            FIELDSTG_EVENT_DIALOG(0, 1, ST_IVY, 0),       FIELDSTG_EVENT_ANIM(ST_IVY, 7, 1),
            FIELDSTG_EVENT_WAIT_DIALOG,                   FIELDSTG_EVENT_ANIM(ST_IVY, 1, 1),
            FIELDSTG_EVENT_DIALOG(0, 2, ST_TEDDY, 0),     FIELDSTG_EVENT_ANIM(ST_TEDDY, 7, 1),
            FIELDSTG_EVENT_WAIT_DIALOG,                   FIELDSTG_EVENT_ANIM(ST_TEDDY, 1, 1),
            FIELDSTG_EVENT_DIALOG(0, 3, ST_JUNIOR, 2),    FIELDSTG_EVENT_ANIM(ST_JUNIOR, 7, 1), /* mode 2: the balloon to his right (he stands at the screen's left edge) */
            FIELDSTG_EVENT_WAIT_DIALOG,                   FIELDSTG_EVENT_ANIM(ST_JUNIOR, 1, 1),
            FIELDSTG_EVENT_DIALOG(0, 4, ST_IVY, 0),       FIELDSTG_EVENT_ANIM(ST_IVY, 7, 1),
            FIELDSTG_EVENT_WAIT_DIALOG,                   FIELDSTG_EVENT_ANIM(ST_IVY, 1, 1),
            FIELDSTG_EVENT_WAIT(20),
        };

        n = put(st_second, n, script + cut, resume - cut);
        n = put(st_second, n, questions, (int)(sizeof(questions) / sizeof(questions[0])));
        n = put(st_second, n, script + resume, end - resume + 1);
    }
    port_log("story: the opening cut at word %d of %d (resumed at %d, %d actors; Junior at %d,%d)", cut, end, resume,
             n_actors, st_junior.x, st_junior.y);
    return 1;
}

/* The script an event runs (fieldstg_event_start): the opening's halves at step 0, its own otherwise. */
short *def_story_event_script(int id, short *script) {
    if (!def_on() || script == NULL || id != ST_OPENING_EVENT || gamestate_data.progress != 0 ||
        gamestate_data.funcs.get_map() != ST_OPENING_MAP) {
        return script;
    }
    if (st_cut_from != script) {
        st_cut_from = opening_cut(script) ? script : NULL;
        if (st_cut_from == NULL) {
            return script;
        }
    }
    port_log("story: frame %ld: the opening, %s half", port_frames,
             def_record_story(DEF_STORY_IGOR) ? "second" : "first");
    return def_record_story(DEF_STORY_IGOR) ? st_second : st_first;
}

/* The port's script command 0x7F00 (fieldstg_event_update): 1 when the event ends there. */
int def_story_event_command(int cmd) {
    if (cmd == ST_CMD_DREAM && def_on() && !def_record_story(DEF_STORY_IGOR)) {
        port_log("story: frame %ld: Junior drifts into the Velvet Room", port_frames);
        dw3_velvet_pull_by(gamestate_data.funcs.get_map(), DW3_VELVET_PULL_IGOR);
        return 1;
    }
    return 0;
}

/* ---- Igor. */

typedef struct StEvoRequirement {
    s16 technique; /* a form it needs (1-based records_digimon row), 0: none */
    s16 level;
} StEvoRequirement;
typedef struct StEvoLearn { /* STFGTREP's learn list (definitive_evohint.c's EvoLearn) */
    s32 technique;
    StEvoRequirement requires[2];
    s16 stat;
    s16 min;
} StEvoLearn;
extern StEvoLearn *stfgtrep_learn_lists[];

#define ST_LEARN_MAX 44 /* a learn list's forms (definitive_evohint.c's EH_MAX) */

static const unsigned char *form_name(int row) {
    void *names = cdload_module.files.get_file(records_language + 0x4E);

    return names != NULL && row > 0 ? (const unsigned char *)font_get_entry(names, records_digimon[row - 1].name_id)
                                    : NULL;
}

/* The level partner `d` has form `row` at, 0: not its own yet. */
static int form_level(int d, int row) {
    GamestateForm f;

    return row > 0 && gamestate_data.funcs.get_form(d, records_digimon[row - 1].id, &f) >= 0 ? f.level : 0;
}

/* Partner `d`'s DNA form the closest to done: the one of its learn list that needs two forms, is not its own yet, and
 * has the most of the two bonds done, then the most of their levels; 0 when it has none. */
static int dna_pick(int d, int *a, int *a_need, int *b, int *b_need) {
    StEvoLearn *list = stfgtrep_learn_lists[d];
    int i, best = -1, best_score = -1;

    for (i = 0; i < ST_LEARN_MAX; i++) {
        int ra = list[i].requires[0].technique, rb = list[i].requires[1].technique, score, ha, hb, na, nb;

        if (list[i].technique <= 0 || ra == 0 || rb == 0 || form_level(d, list[i].technique) > 0 ||
            records_digimon[list[i].technique - 1].id <= 0) {
            continue;
        }
        na = list[i].requires[0].level;
        nb = list[i].requires[1].level;
        ha = form_level(d, ra);
        hb = form_level(d, rb);
        score = ((ha >= na) + (hb >= nb)) * 1000 + (ha < na ? ha : na) + (hb < nb ? hb : nb);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    if (best < 0) {
        return 0;
    }
    *a = list[best].requires[0].technique;
    *a_need = list[best].requires[0].level;
    *b = list[best].requires[1].technique;
    *b_need = list[best].requires[1].level;
    return list[best].technique;
}

/* Builds Igor's reading of the bonds into st_igor_text: his greeting (his first words, to a guest who found the room
 * without the opening's dream), then for each partner with a DNA form: "<partner> could become <form>, born of two
 * forms:" and the two, with the level each needs, and how many are done; his farewell. Nothing when the names are not
 * loaded yet (the disc's line then: the room queues them as it is set up). */
static void igor_read(int first) {
    extern unsigned char def_str_IgorMeet[], def_str_IgorBack[], def_str_IgorCould[], def_str_IgorBorn[],
        def_str_IgorLv[], def_str_IgorBoth[], def_str_IgorOne[], def_str_IgorNone[], def_str_IgorBye[],
        def_str_IgorBlank[], def_str_IgorPage[], def_str_IgorLine[], def_str_IgorEnd[],
        def_str_PipimonMeet[], def_str_PipimonBack[];
    unsigned char *buf = st_igor_text;
    int size = (int)sizeof(st_igor_text), p = 0, slot, told = 0;

    buf[0] = 0;
    if (form_name(1) == NULL) {
        return;
    }
    if (def_finale_pipimon_active()) {
        def_glyph_put(buf, &p, size, first ? def_str_PipimonMeet : def_str_PipimonBack);
    } else {
        def_glyph_put(buf, &p, size, first ? def_str_IgorMeet : def_str_IgorBack);
    }
    for (slot = 0; slot < 3; slot++) {
        int d = gamestate_data.funcs.get_party_member(slot), a, an, b, bn, form, done;

        if (d < 0 || (form = dna_pick(d, &a, &an, &b, &bn)) == 0) {
            continue;
        }
        done = (form_level(d, a) >= an) + (form_level(d, b) >= bn);
        def_glyph_put(buf, &p, size, def_str_IgorPage);
        def_glyph_put(buf, &p, size, (const unsigned char *)gamestate_data.digimon[d].record.name);
        def_glyph_put(buf, &p, size, def_str_IgorCould);
        def_glyph_put(buf, &p, size, form_name(form));
        def_glyph_put(buf, &p, size, def_str_IgorBorn);
        def_glyph_put(buf, &p, size, form_name(a));
        def_glyph_put(buf, &p, size, def_str_IgorLv);
        def_glyph_number(buf, &p, size, an);
        def_glyph_put(buf, &p, size, def_str_IgorLine);
        def_glyph_put(buf, &p, size, form_name(b));
        def_glyph_put(buf, &p, size, def_str_IgorLv);
        def_glyph_number(buf, &p, size, bn);
        def_glyph_put(buf, &p, size, def_str_IgorLine);
        def_glyph_put(buf, &p, size, done == 2 ? def_str_IgorBoth : done == 1 ? def_str_IgorOne : def_str_IgorNone);
        def_glyph_put(buf, &p, size, def_str_IgorEnd);
        port_log("story: Igor reads partner %d: form %d of %d (Lv %d, has %d) and %d (Lv %d, has %d)", d, form, a, an,
                 form_level(d, a), b, bn, form_level(d, b));
        told++;
    }
    def_glyph_put(buf, &p, size, told ? def_str_IgorBye : def_str_IgorBlank);
}

/* ---- Igor's dreams (owner, 2026-11-17): at set moments of the story Igor pulls the
 * player into the room alone, as in the opening (the portal's sound, the room with him only, the two face to face),
 * says his piece and asks; the answer is kept in the save, in the order the story asks (def_record_story_answer), and
 * changes no event: later dreams recall it. Then the player is sent back to the very spot the dream took them from. A
 * new dream is a row of `dreams` and its lines in tools/text_override; DW3_DREAM=<n> dreams row n at the next free
 * frame of the field, whatever the step (a test's shortcut). */
#define DR_OPTIONS 3
#define DR_IGOR 109          /* Igor's actor (definitive_velvet.c VL_IGOR): the reply's balloon is his */
#define DR_LAYER 0x1002      /* the field's window layer (as Gennai's panel) */
#define DR_ROW 18
#define DR_PAL_TEXT 0
#define DR_PAL_HOT 3
#define DR_SND_MOVE 0x8004513E
#define DR_SND_OK 0x8004503C
#define DR_SND_BACK 0x800450BD
#define DR_CONFIRM_COOLDOWN_NS 350000000LL /* 350ms real-time cooldown after 1st confirm */
enum { DR_ASK = 1, DR_CHOOSE, DR_REPLY };

typedef struct {
    s32 question;                             /* its story answer: dreamt while it is 0 */
    s32 progress;                             /* the first step it comes at */
    s32 last;                                 /* the last (a save already past it never dreams it) */
    s32 flag;                                 /* a game flag that must be set (gamestate_get_flag), 0 none */
    const unsigned char *ask;                 /* Igor's words (after his name plate) */
    const unsigned char *options[DR_OPTIONS]; /* the player's answers, NULL past the last (none: no question) */
    const unsigned char *replies[DR_OPTIONS]; /* Igor's word on each */
    s32 recall;                               /* an earlier question Igor recalls first, -1 none */
    const unsigned char *recalls[DR_OPTIONS]; /* his words on each answer it had */
} Dream;

extern unsigned char def_str_DrIgor[], def_str_IgorPage[], def_str_Dr1Ask[], def_str_Dr1Op1[], def_str_Dr1Op2[],
    def_str_Dr1Op3[], def_str_Dr1Re1[], def_str_Dr1Re2[], def_str_Dr1Re3[], def_str_Dr2Ask[], def_str_Dr2Op1[],
    def_str_Dr2Op2[], def_str_Dr2Op3[], def_str_Dr2Re1[], def_str_Dr2Re2[], def_str_Dr2Re3[], def_str_Dr3Rc1[],
    def_str_Dr3Rc2[], def_str_Dr3Rc3[], def_str_Dr3Ask[], def_str_Dr3Op1[], def_str_Dr3Op2[], def_str_Dr3Op3[],
    def_str_Dr3Re1[], def_str_Dr3Re2[], def_str_Dr3Re3[], def_str_Dr4Rc1[], def_str_Dr4Rc2[], def_str_Dr4Rc3[],
    def_str_Dr4Ask[], def_str_Dr4Op1[], def_str_Dr4Op2[], def_str_Dr4Op3[], def_str_Dr4Re1[], def_str_Dr4Re2[],
    def_str_Dr4Re3[];

static const Dream dreams[] = {
    /* Act I, the Game Master's lie: right after she blames Lucky Mouse (WSTAG305's event 351 sets 0x1C0E). Every
     * answer goes on hunting him: the player does not heed the warning yet. Steps 12..18 (the scene is at 13/14; at 19
     * Lucky Mouse's own side is heard, in the Secret Room). */
    { 0, 12, 18, 0x1C0E, def_str_Dr1Ask, { def_str_Dr1Op1, def_str_Dr1Op2, def_str_Dr1Op3 },
      { def_str_Dr1Re1, def_str_Dr1Re2, def_str_Dr1Re3 }, -1, { NULL, NULL, NULL } },
    /* Act II, the burden: the first free moment on the Amaterasu Server (step 27, past the Bug Maze Pit). */
    { 1, 27, 27, 0, def_str_Dr2Ask, { def_str_Dr2Op1, def_str_Dr2Op2, def_str_Dr2Op3 },
      { def_str_Dr2Re1, def_str_Dr2Re2, def_str_Dr2Re3 }, -1, { NULL, NULL, NULL } },
    /* The Oinkmon (owner, 2026-11-17): the Destromon stopped, yet Teddy and Kurt are still Oinkmon, as everyone on
     * the server (WSTAG226's event 935, DMG200:41: "Wait here. I'll look into it."). It ends at step 0x26 and leaves
     * the player free on map 0x270 (WSTAG201: nothing runs there); the step lasts until the Master Room's events (0x27),
     * the restoration machine of Qing Long being within it (flag 0x1A0A). Igor recalls Act I's answer (the mask has
     * fallen) and asks what carries the hero, alone now. Question 3. */
    { 3, 0x26, 0x26, 0, def_str_Dr4Ask, { def_str_Dr4Op1, def_str_Dr4Op2, def_str_Dr4Op3 },
      { def_str_Dr4Re1, def_str_Dr4Re2, def_str_Dr4Re3 }, 0, { def_str_Dr4Rc1, def_str_Dr4Rc2, def_str_Dr4Rc3 } },
    /* Act III, the abyss: Snatchmon fused with the Gunslinger (WSTAG820's event 1080 ends at step 0x2B). In the game
     * Galacticmon's battle starts as that event ends; the port holds it until this dream is over (galactic_hook). Igor
     * recalls Act II's answer before he asks; the Time Room was passed before the Warp Gate (definitive_time_room.c). */
    { 2, 0x2B, 0x2B, 0, def_str_Dr3Ask, { def_str_Dr3Op1, def_str_Dr3Op2, def_str_Dr3Op3 },
      { def_str_Dr3Re1, def_str_Dr3Re2, def_str_Dr3Re3 }, 1, { def_str_Dr3Rc1, def_str_Dr3Rc2, def_str_Dr3Rc3 } },
};
#define DREAMS ((s32)(sizeof(dreams) / sizeof(dreams[0])))

static struct {
    s32 on;             /* the dream running: its row + 1, 0 none */
    s32 phase;          /* DR_ASK Igor speaks, DR_CHOOSE the answers' panel, DR_REPLY his word on it */
    s32 map, x, y, dir; /* where it took the player from */
    s32 forced;         /* DW3_DREAM read */
} dream;

extern s32 gamestate_get_flag(u16 flag, u16 value);

/* Igor's words into his buffer (TALK05 19, def_story_text): his name plate, the recalled answer's page, `text`. */
static void dream_say(const Dream *d, const unsigned char *text) {
    int p = 0, size = (int)sizeof(st_igor_text), was;

    st_igor_text[0] = 0;
    def_glyph_put(st_igor_text, &p, size, def_str_DrIgor);
    if (text == d->ask && d->recall >= 0 && (was = def_record_story_answer(d->recall)) >= 1 && was <= DR_OPTIONS &&
        d->recalls[was - 1] != NULL) {
        def_glyph_put(st_igor_text, &p, size, d->recalls[was - 1]);
        def_glyph_put(st_igor_text, &p, size, def_str_IgorPage);
    }
    def_glyph_put(st_igor_text, &p, size, text);
}

/* The first free frame of a dream's moment: Igor pulls the player in. */
static void dream_hook(void) {
    FieldstgActor *player;
    const char *env;
    s32 i, map = gamestate_data.funcs.get_map(), forced = -1;

    if (!dream.forced) {
        dream.forced = 1;
        if ((env = getenv("DW3_DREAM")) != NULL && atoi(env) >= 1 && atoi(env) <= DREAMS) {
            dream.forced = 1 + atoi(env);
        }
    }
    if (dream.forced > 1) {
        forced = dream.forced - 2;
    }
    if (dream.on || dw3_velvet_in_room() || (player = player_ready()) == NULL) {
        return;
    }
    for (i = 0; i < DREAMS; i++) {
        const Dream *d = &dreams[i];

        if (i != forced && (def_record_story_answer(d->question) != 0 ||
                            gamestate_data.progress < d->progress || gamestate_data.progress > d->last ||
                            (d->flag != 0 && !gamestate_get_flag((u16)d->flag, 1)) ||
                            (d->question == 1 && oikawa_ahead()))) {
            continue; /* Act II waits while the Oikawa still bars the way out of the Bug Maze */
        }
        if (i == forced) {
            dream.forced = 1;
        }
        dream.on = i + 1;
        dream.phase = DR_ASK;
        dream.map = map;
        dream.x = player->pixel_pos.x;
        dream.y = player->pixel_pos.y;
        dream.dir = player->dir;
        port_log("story: frame %ld: dream %ld (map 0x%X at %d,%d): Igor pulls the player in", port_frames, (long)i,
                 map, dream.x, dream.y);
        dw3_velvet_pull_by(map, DW3_VELVET_PULL_IGOR);
        return;
    }
}

static void dream_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(DR_LAYER);
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

/* ---- The answers' panel: the hero's choice, a dream's or a scene's (en_choices.txt). One window a row at the screen's
 * foot, the row under the cursor lit; up/down move, circle takes it. */
static struct {
    s32 open;     /* 1 while up */
    s32 sel, count;
    s32 armed;    /* it takes a press once the one that closed the last balloon is let go */
    s32 pending;  /* 1 after first confirm: a second confirm takes it, back cancels */
    s32 answered; /* the row taken + 1, 0 not yet (read and cleared by the panel's owner) */
    long long pending_ns;
    MessageWindow *wins[DR_OPTIONS];
} choice;

static s32 choice_top(void) {
    return 228 - choice.count * DR_ROW - 8;
}

/* The panel's rows are font 2 windows (the 11-pixel font of the name plates), which has no picture for most
 * punctuation: `:` (01 07) and the rest came out as a stray "0" ("Sharp0 twenty minutes"; every label is
 * "Tone: words", a few have commas, a dream's has parentheses). The missing ones borrow a picture another font already
 * keeps in VRAM, drawn with this font's own palette row: `:` and `/` the 9-pixel font's, `,` `(` `)` the 14-pixel
 * font's, raised so their baseline meets this font's. Checked on the panel (2026-11-21); `;` and `...` borrowed that
 * way came out wrong, so they are left out (no label uses them). Filled once; only empty slots are touched. */
extern FontGlyph font_glyphs_9_ext[], font_glyphs_11_ext[], font_glyphs_14_ext[];

static void panel_font_fill(void) {
    static const struct {
        u8 code;   /* extra glyph (01 xx) */
        u8 from;   /* 9 or 14: the font lending its picture */
        s8 dy;     /* y offset in this font */
    } lend[] = {
        { 7, 9, 2 },   /* : */
        { 15, 9, 2 },  /* / */
        { 4, 14, -2 }, /* , */
        { 21, 14, -2 }, /* ( */
        { 22, 14, -2 }, /* ) */
    };
    static int done;
    size_t i;

    if (done) {
        return;
    }
    done = 1;
    for (i = 0; i < sizeof(lend) / sizeof(lend[0]); i++) {
        FontGlyph *dst = &font_glyphs_11_ext[lend[i].code - 1];
        const FontGlyph *src = &(lend[i].from == 9 ? font_glyphs_9_ext : font_glyphs_14_ext)[lend[i].code - 1];

        if (dst->tpage_x != 0xFF || src->tpage_x == 0xFF) {
            continue;
        }
        *dst = *src;
        dst->clut_y = font_glyphs_11_ext[0].clut_y;
        dst->y_offset = lend[i].dy;
        dst->unk_A = font_glyphs_11_ext[0].unk_A;
    }
}

static void choice_open(const unsigned char *const *labels, s32 count) {
    s32 i;

    panel_font_fill();

    choice.count = count < DR_OPTIONS ? count : DR_OPTIONS;
    choice.sel = 0;
    choice.armed = 0;
    choice.pending = 0;
    choice.answered = 0;
    choice.open = 1;
    for (i = 0; i < choice.count; i++) {
        MessageWindow *w = message_create_window(DR_LAYER, 2, 26, choice_top() + 5 + i * DR_ROW);

        w->set_ot_depth(w, 1);
        w->set_text(w, (u8 *)labels[i], -1);
        w->set_palette(w, i == 0 ? DR_PAL_HOT : DR_PAL_TEXT);
        choice.wins[i] = w;
    }
    port_log("story: frame %ld: the answers' panel (%ld rows)", port_frames, (long)choice.count);
}

static void choice_close(void) {
    s32 i;

    for (i = 0; i < choice.count; i++) {
        if (choice.wins[i] != NULL) {
            heap_objects.stop((Object *)choice.wins[i]);
            choice.wins[i] = NULL;
        }
    }
    choice.pending = 0;
    choice.open = 0;
}

/* A frame of the panel (the windows draw when run, as Gennai's panel's). */
static void choice_frame(void) {
    s32 top = choice_top(), old = choice.sel, i;

    for (i = 0; i < choice.count; i++) {
        heap_objects.try_run((Object *)choice.wins[i]);
    }
    if (choice.pending) {
        dream_rect(14, top + 3 + choice.sel * DR_ROW, 4, 16, 220, 200, 80); /* armed marker */
        dream_rect(14, top + 3 + choice.sel * DR_ROW, 292, 16, 40, 80, 175); /* the row armed to confirm */
    } else {
        dream_rect(14, top + 3 + choice.sel * DR_ROW, 292, 16, 20, 30, 90); /* the row under the cursor */
    }
    dream_rect(9, top + 1, 302, choice.count * DR_ROW + 6, 4, 6, 30);
    dream_rect(8, top, 304, choice.count * DR_ROW + 8, 90, 120, 220); /* the room's blue */
    if (!choice.armed) {
        choice.armed = !(pad_state.get_held(0) & ST_KEYS);
        return;
    }
    if (choice.pending) {
        if (PAD_PRESSED(14)) {
            choice.pending = 0;
            sound_module.play(DR_SND_BACK);
            return;
        }
        if (PAD_PRESSED(13)) {
            long long now = port_clock_ns();

            if (now - choice.pending_ns >= DR_CONFIRM_COOLDOWN_NS || now < choice.pending_ns) {
                sound_module.play(DR_SND_OK);
                choice.answered = choice.sel + 1;
                choice_close();
            }
        }
        return;
    }
    if (PAD_PRESSED(4) || PAD_PRESSED(6)) {
        choice.sel = (choice.sel + (PAD_PRESSED(6) ? 1 : choice.count - 1)) % choice.count;
        sound_module.play(DR_SND_MOVE);
        choice.wins[old]->set_palette(choice.wins[old], DR_PAL_TEXT);
        choice.wins[choice.sel]->set_palette(choice.wins[choice.sel], DR_PAL_HOT);
        return;
    }
    if (PAD_PRESSED(13)) {
        choice.pending = 1;
        choice.pending_ns = port_clock_ns();
        sound_module.play(DR_SND_OK);
    }
}

/* The dream's panel: his question's answers. */
static void dream_choose_open(const Dream *d) {
    s32 n = 0;

    while (n < DR_OPTIONS && d->options[n] != NULL) {
        n++;
    }
    dream.phase = DR_CHOOSE;
    fieldstg_stage.event_running = 1;
    choice_open(d->options, n);
}

/* A frame of the dream's panel: on the answer, Igor's word on it. */
static void dream_choose(void) {
    static const s32 reply[] = { ST_IGOR_READ };
    const Dream *d = &dreams[dream.on - 1];

    fieldstg_stage.event_running = 1;
    if (choice.open) {
        choice_frame();
    }
    if (!choice.answered) {
        return;
    }
    def_record_set_story_answer(d->question, choice.answered);
    port_log("story: frame %ld: dream %ld answered %ld", port_frames, (long)(dream.on - 1), (long)choice.answered);
    dream_say(d, d->replies[choice.answered - 1]);
    choice.answered = 0;
    dream.phase = DR_REPLY;
    story_begin(reply, 1, DR_IGOR);
}

/* ---- The story rewrite (generated tables, definitive_story_text.c): English lines in place of the game's, the Kaili
 * mode's own lines, and the scenes' choices (a hero line the player picks; the scene goes on as the game's). Off in a
 * replay script and with DW3_STORY=0 (the game's own text, to compare). */
static int story_layer_on(void) {
    static int forced = -2;

    if (forced == -2) {
        const char *e = getenv("DW3_STORY");

        forced = e != NULL ? atoi(e) != 0 : -1;
    }
    return forced != 0 && def_on();
}

static const DefStoryLine *line_find(const DefStoryLine *tab, int count, int file, int index) {
    int lo = 0, hi = count - 1;

    while (lo <= hi) {
        int mid = (lo + hi) >> 1;

        if (tab[mid].file < file || (tab[mid].file == file && tab[mid].index < index)) {
            lo = mid + 1;
        } else if (tab[mid].file > file || tab[mid].index > index) {
            hi = mid - 1;
        } else {
            return &tab[mid];
        }
    }
    return NULL;
}

static const DefStoryChoice *choice_find(int file, int index) {
    int lo = 0, hi = def_story_choice_count - 1;

    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        const DefStoryChoice *c = &def_story_choices[mid];

        if (c->file < file || (c->file == file && c->index < index)) {
            lo = mid + 1;
        } else if (c->file > file || c->index > index) {
            hi = mid - 1;
        } else {
            return c;
        }
    }
    return NULL;
}

/* The entry's line that remembers an earlier answer (en_echoes.txt), NULL when none holds. */
static const unsigned char *echo_find(int file, int index) {
    int lo = 0, hi = def_story_echo_count, mode = port_girls_on() ? 2 : 1;
    const DefStoryEcho *e;

    while (lo < hi) { /* the entry's first echo */
        int mid = (lo + hi) >> 1;

        e = &def_story_echoes[mid];
        if (e->file < file || (e->file == file && e->index < index)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    for (e = &def_story_echoes[lo]; lo < def_story_echo_count && e->file == file && e->index == index; e++, lo++) {
        if ((e->mode == 0 || e->mode == mode) && (e->answers >> def_record_story_answer(e->question) & 1)) {
            const unsigned char *ret = def_is_ptbr() && e->pt_bytes != NULL ? e->pt_bytes : e->bytes;

            if (ret == NULL) {
                continue;
            }
            static unsigned char seen[1024]; /* one log line per echo, not per frame it is drawn */

            if (lo < 1024 && !seen[lo]) {
                seen[lo] = 1;
                port_log("story: echo 0x%X %d (question %d answered %d)", file, index, e->question,
                         def_record_story_answer(e->question));
            }
            return ret;
        }
    }
    return NULL;
}

/* The Kaili mode's line for an entry in the language shown (en_kaili.txt / pt_kaili.txt, and every other line that
 * names Kail with the name swapped to Junior), NULL outside that mode. */
static const DefStoryLine *kaili_find(int file, int index, int pt) {
    static unsigned char seen[2][256]; /* one log line per entry, not per frame it is drawn */
    const DefStoryLine *l;
    int k;

    if (!port_girls_on()) {
        return NULL;
    }
    l = pt ? line_find(def_kaili_lines_pt, def_kaili_line_pt_count, file, index)
           : line_find(def_kaili_lines, def_kaili_line_count, file, index);
    if (l != NULL && (k = (int)(l - (pt ? def_kaili_lines_pt : def_kaili_lines))) < 256 && !seen[pt][k]) {
        seen[pt][k] = 1;
        port_log("story: kaili line 0x%X %d (%s)%s", file, index, pt ? "pt" : "en", l->hero ? ", the hero's" : "");
    }
    return l;
}

const unsigned char *def_story_line(int file, int index) {
    const DefStoryChoice *c;
    const DefStoryLine *l;
    const unsigned char *e;
    int a, pt = def_is_ptbr();

    if (!story_layer_on()) {
        return NULL;
    }
    if ((c = choice_find(file, index)) != NULL && (a = def_record_story_answer(c->question)) >= 1 && a <= c->count) {
        const unsigned char *picked = pt && c->lines_pt[a - 1] != NULL ? c->lines_pt[a - 1] : c->lines[a - 1];
        if (picked != NULL) {
            return picked;
        }
    }
    if ((e = echo_find(file, index)) != NULL) {
        return e;
    }
    if (pt) { /* the Portuguese rewrite; an entry not translated yet falls through to the English one below */
        if ((l = kaili_find(file, index, 1)) != NULL) {
            return l->bytes;
        }
        if ((l = line_find(def_story_lines_pt, def_story_line_pt_count, file, index)) != NULL) {
            return l->bytes;
        }
    }
    if ((l = kaili_find(file, index, 0)) != NULL) {
        return l->bytes;
    }
    if ((l = line_find(def_story_lines, def_story_line_count, file, index)) != NULL) {
        return l->bytes;
    }
    return NULL;
}

static void choice_apply_fx(const DefStoryChoice *c, int ans) {
    int i = ans - 1;

    if (i < 0 || i >= 3) {
        return;
    }
    if (c->bond_ally[i] >= 0 && c->bond_delta[i] != 0) {
        def_story_bond_add(c->bond_ally[i], c->bond_delta[i]);
    }
    if (c->rep_delta[i] != 0) {
        def_story_rep_add(c->rep_delta[i]);
    }
}

/* fieldstg_event's dialog command is about to show event text `msg` on actor `*actor`: 1 holds the script on it while
 * the hero's choice is on screen (en_choices.txt); the Kaili mode may move the balloon to the hero (`>hero`). */
int def_story_event_dialog(s32 msg, s32 *actor) {
    s32 text = fieldstg_stage.event_text, file;
    const DefStoryChoice *c;
    const DefStoryLine *l;
    const unsigned char *const *labels;

    if (!story_layer_on() || text == 0 || (records_language != 1 && records_language != 2)) {
        return 0;
    }
    file = (((text >> 16) & 0xFFFF) - records_language) | (((text & 0xFFFF) + 1) << 16);
    if (((l = kaili_find(file, msg, def_is_ptbr())) != NULL || (l = kaili_find(file, msg, 0)) != NULL) && l->hero) {
        FieldstgActor *hero = (FieldstgActor *)heap_objects.find(5, -1, 0);

        if (hero != NULL) {
            *actor = hero->base.key1;
        }
    }
    if ((c = choice_find(file, msg)) == NULL || def_record_story_answer(c->question) != 0) {
        return 0;
    }
    labels = def_is_ptbr() && c->labels_pt[0] != NULL ? c->labels_pt : c->labels;
    if (labels[0] == NULL) {
        return 0;
    }
    if (choice.answered) {
        def_record_set_story_answer(c->question, choice.answered);
        choice_apply_fx(c, choice.answered);
        port_log("story: frame %ld: choice %ld answered %ld", port_frames, (long)c->question, (long)choice.answered);
        choice.answered = 0;
        return 0;
    }
    if (!choice.open) {
        choice_open(labels, c->count);
    }
    return 1;
}

typedef struct DefNpcArc {
    int npc_id;
    int map;
    int actor_id;
    int min_progress;
    int max_progress;
    int stage;
} DefNpcArc;

/* Registered NPC mini-arcs: when progress is in [min_progress, max_progress] and the player hasn't seen `stage`,
 * a floating thought-bubble indicator appears over the NPC's head in FIELDSTG. */
static const DefNpcArc npc_arcs[] = {
    /* Phase 1 (Asuka City) — 8 NPC mini-arcs across Central Park (0x200), Main Street (0x201) & Lobby (0x203) */
    { 0, 0x200, 46, 0, 11, 0 },    /* Mika (Patamon fan) — Stage 0 (TALK00 19, Q200) */
    { 0, 0x200, 46, 12, 999, 1 },  /* Mika — Stage 1 (TALK00 623) */
    { 1, 0x200, 371, 12, 999, 1 }, /* Mika's Patamon — Stage 1 (TALK00 613) */
    { 2, 0x200, 47, 0, 11, 0 },    /* Masato — Stage 0 (TALK00 15, Q201) */
    { 2, 0x200, 47, 12, 999, 1 },  /* Masato — Stage 1 (TALK00 665) */
    { 3, 0x200, 365, 0, 11, 0 },   /* Agumon Coach — Stage 0 (TALK00 21, Q202) */
    { 3, 0x200, 365, 12, 999, 1 }, /* Agumon Coach — Stage 1 (TALK00 602) */
    { 4, 0x201, 57, 0, 11, 0 },    /* Grandpa Jake — Stage 0 (TALK00 22, Q203) */
    { 4, 0x201, 57, 12, 999, 1 },  /* Grandpa Jake — Stage 1 (TALK00 646) */
    { 5, 0x201, 51, 0, 11, 0 },    /* Maniac Jack — Stage 0 (TALK00 11, Q204) */
    { 5, 0x201, 51, 12, 999, 1 },  /* Maniac Jack — Stage 1 (TALK00 711) */
    { 6, 0x203, 45, 0, 11, 0 },    /* Newbie Shiro — Stage 0 (TALK00 17, Q205) */
    { 6, 0x203, 45, 12, 999, 1 },  /* Newbie Shiro — Stage 1 (TALK00 329) */
    { 7, 0x203, 49, 0, 11, 0 },    /* Lazy Alan — Stage 0 (TALK00 13, Q206) */
    { 7, 0x203, 49, 12, 999, 1 },  /* Lazy Alan — Stage 1 (TALK00 331) */
    { 8, 0x203, 52, 0, 11, 0 },    /* Digimon Fan Mary — Stage 0 (TALK00 239, Q207) */
    { 8, 0x203, 52, 12, 999, 1 },  /* Digimon Fan Mary — Stage 1 (TALK00 330) */
    /* Phase 2 (Seiryu City) — 6 NPC mini-arcs across Plaza (0x22E), Shops (0x22F) & Inn (0x230) */
    { 9, 0x22E, 46, 0, 11, 0 },    /* Crybaby Maria — Stage 0 (TALK01 10, Q208) */
    { 9, 0x22E, 46, 12, 999, 1 },  /* Crybaby Maria — Stage 1 (TALK01 16) */
    { 10, 0x22F, 46, 0, 11, 0 },   /* Charming Elle — Stage 0 (TALK01 65, Q209) */
    { 10, 0x22F, 46, 12, 999, 1 }, /* Charming Elle — Stage 1 (TALK01 67) */
    { 11, 0x22E, 50, 0, 11, 0 },   /* Sarah (Seiryu Leader fan) — Stage 0 (TALK01 11, Q210) */
    { 11, 0x22E, 50, 12, 999, 1 }, /* Sarah — Stage 1 (TALK01 17) */
    { 12, 0x230, 53, 0, 11, 0 },   /* Manager Sasaki (rival spy) — Stage 0 (TALK01 77, Q211) */
    { 12, 0x230, 53, 12, 999, 1 }, /* Manager Sasaki — Stage 1 (TALK01 81) */
    { 13, 0x230, 54, 0, 11, 0 },   /* Ms. Olivia (hooked spy) — Stage 0 (TALK01 78, Q212) */
    { 13, 0x230, 54, 12, 999, 1 }, /* Ms. Olivia — Stage 1 (TALK01 82) */
    { 14, 0x22E, 365, 0, 11, 0 },  /* Seiryu Agumon — Stage 0 (TALK01 15, Q213) */
    { 14, 0x22E, 365, 12, 999, 1 },/* Seiryu Agumon — Stage 1 (TALK01 12) */
};

static const DefNpcArc *npc_arc_active(int map, int actor_id) {
    int i, prog = gamestate_data.progress;

    for (i = 0; i < (int)(sizeof(npc_arcs) / sizeof(npc_arcs[0])); i++) {
        const DefNpcArc *a = &npc_arcs[i];
        if (a->map == map && a->actor_id == actor_id && prog >= a->min_progress && prog <= a->max_progress) {
            return a;
        }
    }
    return NULL;
}

int def_story_npc_talk(s32 actor_id, s32 msg) {
    s32 file;
    const DefStoryChoice *c;
    const unsigned char *const *labels;
    const DefNpcArc *arc;

    if (!story_layer_on() || (records_language != 1 && records_language != 2)) {
        return 0;
    }
    arc = npc_arc_active(gamestate_data.funcs.get_map(), actor_id);
    if (arc != NULL) {
        def_story_npc_mark_seen(arc->npc_id, arc->stage);
    }
    if (fieldstg_stage.talk_file == 0) {
        return 0;
    }
    file = fieldstg_stage.talk_file - records_language;
    if ((c = choice_find(file, msg)) == NULL || def_record_story_answer(c->question) != 0) {
        return 0;
    }
    labels = def_is_ptbr() && c->labels_pt[0] != NULL ? c->labels_pt : c->labels;
    if (labels[0] == NULL) {
        return 0;
    }
    if (choice.answered) {
        def_record_set_story_answer(c->question, choice.answered);
        choice_apply_fx(c, choice.answered);
        port_log("story: frame %ld: npc choice %ld answered %ld", port_frames, (long)c->question, (long)choice.answered);
        choice.answered = 0;
        return 0;
    }
    if (!choice.open) {
        choice_open(labels, c->count);
    }
    choice_frame();
    return 1;
}

static void npc_ind_rect(GfxLayer *layer, s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b) {
    POLY_F4 *p = gfx_module.funcs.get_packet();

    setPolyF4(p);
    setRGB0(p, r, g, b);
    setXY4(p, (s16)x, (s16)y, (s16)(x + w), (s16)y, (s16)x, (s16)(y + h), (s16)(x + w), (s16)(y + h));
    addPrim(layer->get_ot_entry(layer, 1), p);
    gfx_module.funcs.set_packet(p + 1);
}

void def_story_npc_indicator_draw(s32 actor_id, void *layer_ptr, s32 x, s32 y) {
    GfxLayer *layer = (GfxLayer *)layer_ptr;
    const DefNpcArc *arc;
    s32 bob, bx, by, dot;

    if (!story_layer_on() || !def_is_ptbr() || layer == NULL || fieldstg_stage.event_running) {
        return;
    }
    arc = npc_arc_active(gamestate_data.funcs.get_map(), actor_id);
    if (arc == NULL || def_story_npc_seen_stage(arc->npc_id) >= arc->stage) {
        return;
    }
    bob = ((port_frames / 16) & 1) ? -2 : 0;
    bx = x - 8;
    by = y - 16 + bob;
    dot = (int)((port_frames / 12) % 3);
    npc_ind_rect(layer, bx + 2, by + 4, 2, 2, dot == 0 ? 255 : 70, dot == 0 ? 196 : 80, dot == 0 ? 64 : 110);
    npc_ind_rect(layer, bx + 6, by + 4, 2, 2, dot == 1 ? 255 : 70, dot == 1 ? 196 : 80, dot == 1 ? 64 : 110);
    npc_ind_rect(layer, bx + 10, by + 4, 2, 2, dot == 2 ? 255 : 70, dot == 2 ? 196 : 80, dot == 2 ? 64 : 110);
    npc_ind_rect(layer, bx + 6, by + 11, 3, 2, 248, 244, 224);
    npc_ind_rect(layer, bx + 1, by + 1, 13, 8, 248, 244, 224);
    npc_ind_rect(layer, bx, by, 15, 10, 28, 36, 62);
}

/* The dream is over: back to the spot it took the player from (the room keeps it: dw3_velvet_leave). */
static void dream_leave(void) {
    port_log("story: frame %ld: dream %ld over: back to map 0x%X at %d,%d", port_frames, (long)(dream.on - 1),
             dream.map, dream.x, dream.y);
    dream.on = 0;
    dream.phase = 0;
    st_igor_text[0] = 0;
    fieldstg_goto_map(dream.map, -1, dream.x << 8, dream.y << 8, dream.dir);
}

/* Igor's line as he is talked to (definitive_velvet.c): the key while the opening's dream runs, his reading of the bonds
 * after (his first words to a guest who came in without the dream: a save from before the port's story). */
int def_story_igor_line(void) {
    int first;

    if (dream.on && dream.phase == DR_ASK) {
        dream_say(&dreams[dream.on - 1], dreams[dream.on - 1].ask);
        return ST_IGOR_READ;
    }
    if (def_story_room_intro()) {
        return ST_IGOR_SEIRYU; /* Seiryu: he shows the room again (and the Sepikmon takes his place after) */
    }
    port_log("story: Igor talked to at step %ld (story flags 0x%X)", (long)gamestate_data.progress,
             def_record_story(DEF_STORY_IGOR));

    if (def_on() && gamestate_data.progress == 0 && !def_record_story(DEF_STORY_IGOR)) {
        return ST_IGOR_KEY;
    }
    first = !def_record_story(DEF_STORY_IGOR);
    igor_read(first);
    if (first) {
        def_record_set_story(DEF_STORY_IGOR);
    }
    return ST_IGOR_READ;
}

/* Igor's line closed: the opening's dream ends, back to where Junior stood (the room lets him out to the map it was
 * entered from, keeping the place: dw3_velvet_leave), and the stage starts the scene's second half. */
void def_story_igor_done(void) {
    if (dream.on && dream.phase == DR_ASK) {
        const Dream *d = &dreams[dream.on - 1];

        if (d->options[0] != NULL) {
            dream_choose_open(d);
        } else {
            def_record_set_story_answer(d->question, 1);
            dream_leave();
        }
        return;
    }
    if (def_story_room_intro()) {
        def_record_set_story(DEF_STORY_SEIRYU_ROOM);
        if (gamestate_data.items[DEF_PLUG_ITEM] < DEF_BM_ITEM_CAP) {
            gamestate_data.items[DEF_PLUG_ITEM]++; /* Igor's gift for the Ogremon hunt */
        }
        port_log("story: frame %ld: Igor showed the room again: the Sepikmon takes his place", port_frames);
        dw3_velvet_cut(DW3_VELVET_PULL_SEPIK); /* the screen cuts: the room as its level has it, he calls over */
        return;
    }
    if (!def_on() || gamestate_data.progress != 0 || def_record_story(DEF_STORY_IGOR)) {
        return;
    }
    def_record_set_story(DEF_STORY_IGOR);
    port_log("story: frame %ld: Igor gave the key: back to the opening at %d,%d", port_frames, st_junior.x, st_junior.y);
    fieldstg_goto_map(ST_OPENING_MAP, -1, st_junior.x << 8, st_junior.y << 8, st_junior.dir);
}

/* The Sepikmon's line (definitive_velvet.c), 0: his usual one. While the Leader is missing his first meeting has its
 * own words; once the Ogremon hunt is won, he hears the report and the story goes back to the game's. */
int def_story_sepik_line(int met) {
    if (!def_on() || !seiryu_active()) {
        return 0;
    }
    if (!met) {
        return ST_SEPIK_CALLED;
    }
    if (def_super_done() > 0) {
        def_record_set_story(DEF_STORY_SEIRYU_DONE);
        port_log("story: the Ogremon hunt reported: the Seiryu Leader is at the Protocol Ruins");
        return ST_SEPIK_REPORT;
    }
    return 0;
}

/* ---- The Oikawa's arc (step 26 -> 27): Bulbmon beaten in the Bug Maze Pit, event 711
 * sets step 27 and leaves the player in the maze's hall (map 0x245, WSTAG540) on the way out to Amaterasu. There the
 * hooded dealer stands in the way (the Black Market's trader, actor 42: the same figure the Velvet Room draws over),
 * calls the player over and fights (def_oikawa_fight: Arukenimon, Mummymon, VenomMyotismon, Lv28..32; no escape). A
 * defeat is no game over (the party as it went in, he calls again); beaten, he leaves a word and is gone, and the Guide
 * sends the player to the Velvet Room, where he now keeps the Black Market behind the counter (DEF_STORY_MARKET_OPEN
 * when he opens it). Igor's second dream waits until then on that map. */
#define OK_ACTOR 42 /* the dealer's figure (map 0x20C's trader) */
#define OK_PLACE 6  /* the VRAM place of map 0x245's third figure (actor 78), left out while he waits: a slot no
                     * figure of the stage uses may hold the map's own graphics */
#define OK_X 1778   /* two steps up-left of where event 711 leaves the player (1800,524), facing him */
#define OK_Y 502
#define OK_DIR 7
enum { OK_IDLE, OK_TALKING, OK_FIGHTING, OK_WON, OK_LOST };

extern s32 fieldstg_actor_talk_to(FieldstgActor *obj, FieldstgPos *pos);
extern FieldstgPos fieldstg_front_offsets[8];

static FieldstgTalk ok_talks[2] = { { NULL, NULL, OK_CALL }, { NULL, NULL, 0 } };
static FieldstgPlacedActor ok_npc = { NULL, ok_talks, OK_ACTOR, OK_PLACE, OK_X, OK_Y, OK_DIR };
static FieldstgPlacedActor *ok_actors[16];
static struct {
    s32 phase;   /* OK_* */
    s32 called;  /* he called the player over (once per visit, again after a defeat) */
    s32 tries;   /* battles lost to him */
    s32 pending; /* his battle set up, not begun yet */
    s16 party[3], hp[3], mp[3];
} oik;

/* The hooded man's figure (the Velvet Room draws the dealer's art over it). */
int def_story_oikawa_figure(const FieldstgPlacedActor *placed) {
    return placed == &ok_npc;
}

static int oikawa_here(void) {
    return def_on() && gamestate_data.progress == OK_STEP && gamestate_data.funcs.get_map() == OK_MAP &&
           !def_record_story(DEF_STORY_OIKAWA_BEATEN);
}

/* Step 27 still in the Bug Maze (its maps 0x244..0x246: Bulbmon's pit, its hall, the way back), the Oikawa not beaten:
 * Igor's second dream waits for the Amaterasu side. */
static int oikawa_ahead(void) {
    s32 map = gamestate_data.funcs.get_map();

    return def_on() && gamestate_data.progress == OK_STEP && !def_record_story(DEF_STORY_OIKAWA_BEATEN) &&
           map >= 0x244 && map <= 0x246;
}

/* The end of a stage's setup (fieldstg_manager_start): the hooded dealer added to map 0x245's figures while he waits. */
void def_story_stage_setup(void) {
    FieldstgPlacedActor **list = fieldstg_stage.actors;
    s32 n = 0;

    if (!oikawa_here()) {
        return;
    }
    for (; list != NULL && *list != NULL && n < (s32)(sizeof(ok_actors) / sizeof(ok_actors[0])) - 2; list++) {
        if ((*list)->vram_place != OK_PLACE) {
            ok_actors[n++] = *list;
        }
    }
    ok_actors[n++] = &ok_npc;
    ok_actors[n] = NULL;
    fieldstg_stage.actors = ok_actors;
    oik.called = 0;
    port_log("story: the Oikawa waits on map 0x%X (try %d)", OK_MAP, oik.tries);
}

/* A talk to the hooded dealer (dw3_velvet_shop_talk): his line, and the battle after it. 1 when it was his. */
int def_story_oikawa_talk(FieldstgActor *actor) {
    if (!oikawa_here() || actor == NULL || actor->placed != &ok_npc) {
        return 0;
    }
    ok_talks[0].message = oik.tries > 0 ? OK_AGAIN : OK_CALL;
    oik.phase = OK_TALKING;
    port_log("story: frame %ld: the Oikawa speaks (message %d)", port_frames, ok_talks[0].message);
    return 1;
}

static void oikawa_hook(void) {
    FieldstgActor *player, *him;

    if (oik.phase == OK_WON && gamestate_data.funcs.get_map() == OK_MAP) {
        static const s32 line[1] = { OK_BEATEN };

        if (story_talk(line, 1)) { /* his voice as he goes (his figure went with the battle) */
            oik.phase = OK_IDLE;
        }
        return;
    }
    if (!oikawa_here() || (player = player_ready()) == NULL) {
        return;
    }
    if (oik.phase == OK_TALKING) { /* his line closed: the battle (with Butter-Fly Adventure theme) */
        s32 ok_bank = 0, ok_music = OK_MUSIC;
        dw3_adventure_music(&ok_bank, &ok_music);
        if (def_time_set_battle(&def_oikawa_fight, OK_STAGE, ok_music, oik.party, oik.hp, oik.mp)) {
            oik.phase = OK_FIGHTING;
            oik.pending = 1;
            port_log("story: frame %ld: the Oikawa's battle begins", port_frames);
        }
        return;
    }
    if (oik.phase == OK_LOST) {
        oik.phase = OK_IDLE;
        oik.called = 0;
    }
    if (oik.called || (him = (FieldstgActor *)heap_objects.find(5, OK_ACTOR, -1)) == NULL || him->placed != &ok_npc) {
        return;
    }
    oik.called = 1; /* he calls the player over: the player turns to him and talks */
    {
        FieldstgPos pos = him->pixel_pos;
        s32 dx = pos.x - player->pixel_pos.x, dy = pos.y - player->pixel_pos.y, d, best = 0, score = -0x7FFFFFFF;

        for (d = 0; d < 8; d++) {
            s32 dot = dx * fieldstg_front_offsets[d].x + dy * fieldstg_front_offsets[d].y;

            if (dot > score) {
                score = dot;
                best = d;
            }
        }
        player->dir = best;
        port_log("story: frame %ld: the Oikawa calls the player over", port_frames);
        fieldstg_actor_talk_to(player, &pos);
    }
}

void def_story_battle_begin(void) {
    if (oik.pending) {
        oik.pending = 0;
        if (oik.phase != OK_FIGHTING) {
            return;
        }
    } else if (oik.phase == OK_FIGHTING) {
        oik.phase = OK_IDLE; /* not his battle after all */
    }
}

void def_story_battle_end(int result) {
    if (oik.phase != OK_FIGHTING) {
        return;
    }
    if (result == 1) {
        oik.phase = OK_WON;
        def_record_set_story(DEF_STORY_OIKAWA_BEATEN);
        port_log("story: the Oikawa beaten: the Black Market will open");
    } else {
        oik.phase = OK_LOST;
        oik.tries++;
        port_log("story: the Oikawa won (result %d): he waits again", result);
    }
}

/* WFIGHTMN, a lost battle: 1 when it was the Oikawa's (no game over: back to the hall, the party as it went in). */
int def_story_defeat(void) {
    if (oik.phase == OK_FIGHTING) {
        def_story_battle_end(2);
    }
    if (oik.phase != OK_LOST) {
        return 0;
    }
    def_time_restore(oik.party, oik.hp, oik.mp);
    return 1;
}

/* The Velvet Room's dealer opened the market for the first time since he was beaten (definitive_velvet.c). */
void def_story_market_opened(void) {
    if (def_record_story(DEF_STORY_OIKAWA_BEATEN) && !def_record_story(DEF_STORY_MARKET_OPEN)) {
        def_record_set_story(DEF_STORY_MARKET_OPEN);
        port_log("story: the Black Market opened");
    }
}

/* ---- Galacticmon (step 0x2B, map 0x2DF, WSTAG820): event 1080 (Snatchmon fuses with the Gunslinger) ends by setting
 * flag 0x7401, whose type starts the stage's listed battle 1, Galacticmon's, at once: no free frame for Igor's last
 * dream. The port lets the event end without it (def_story_hold_galacticmon) and starts it itself once the dream was
 * answered, on the first free frame back on the map. The event's own trigger is held shut from then on: the game never
 * gave a free frame after it, so nothing in it marks it done (its flag 0x40A5 is set by no one). */
#define ST_GALACTIC_MAP 0x2DF
#define ST_GALACTIC_STEP 0x2B
#define ST_GALACTIC_FLAG 0x7401 /* type 0x74: setting it starts listed battle 1 (gamestate_set_flag) */
#define ST_GALACTIC_EVENT 0x438 /* event 1080's map event (type 8) */
static s32 st_galactic; /* event 1080 ended without its battle: the port owes it */

int def_story_hold_galacticmon(void) {
    if (!def_on()) {
        return 0;
    }
    st_galactic = 1;
    port_log("story: frame %ld: Galacticmon's battle held for Igor's last dream", port_frames);
    return 1;
}

static void galactic_hook(void) {
    if (!st_galactic || gamestate_data.progress != ST_GALACTIC_STEP ||
        gamestate_data.funcs.get_map() != ST_GALACTIC_MAP || def_record_story_answer(2) == 0 /* Act III answered */ ||
        player_ready() == NULL) {
        return;
    }
    st_galactic = 0;
    port_log("story: frame %ld: Igor's last dream over: Galacticmon's battle", port_frames);
    gamestate_flags.set_flag(ST_GALACTIC_FLAG, 1);
}

static int protocol_gate_closed_now(void) {
    s32 map = gamestate_data.funcs.get_map();

    return def_on() && (map == ST_EAST_WIRE || map == ST_PROTOCOL_FOREST) &&
           gamestate_data.progress <= ST_SEIRYU_STEP && !def_record_story(DEF_STORY_SEIRYU_DONE);
}

/* A map event a story hook holds shut (FIELDSTG's check, through dw3_velvet_event_closed). */
int def_story_event_closed(int type, int param) {
    s32 map = gamestate_data.funcs.get_map();

    if (type == 1 &&
        ((map == ST_EAST_WIRE && param == ST_PROTOCOL_FOREST) ||
         (map == ST_PROTOCOL_FOREST && param == ST_PROTOCOL_RUINS)) &&
        protocol_gate_closed_now()) {
        story.protocol_touch = (s32)port_frames;
        return 1;
    }
    return def_on() && type == 8 && param == ST_GALACTIC_EVENT && gamestate_data.progress >= ST_GALACTIC_STEP &&
           map == ST_GALACTIC_MAP;
}

static void protocol_gate_frame(void) {
    s32 on = protocol_gate_closed_now() && story.protocol_touch > 0 &&
             (s32)port_frames - story.protocol_touch <= 2;

    if (on && !story.protocol_touching) {
        s32 line = ST_PROTOCOL_WARN;

        if (!story_talk(&line, 1)) {
            return;
        }
    }
    story.protocol_touching = on;
}

/* Step 4: Repeating Tom's news closed: the player's feeling, and the room's door opens on the inn's stairs. */
static void seiryu_hook(void) {
    static const s32 talk[] = { ST_SEIRYU_FELT };

    if (!story.tom_told || !seiryu_ask()) {
        story.tom_told = 0;
        return;
    }
    if (story_talk(talk, 1)) {
        story.tom_told = 0;
        def_record_set_story(DEF_STORY_SEIRYU_CALL); /* the room's level 1: its doors open */
        sound_module.play(0x40004);                  /* the portal's (the map warps') sound: the door appears */
        port_log("story: frame %ld: Repeating Tom's news (map 0x%X): the Leader is gone, the door opens", port_frames,
                 gamestate_data.funcs.get_map());
    }
}

static void story_release(void) {
    FieldstgActor *player;

    if (pad_state.get_held(0) & ST_KEYS) {
        fieldstg_stage.event_running = 1;
        return;
    }
    story.releasing = 0;
    fieldstg_stage.event_running = 0;
    port_log("story: frame %ld: the talk is over", port_frames);
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player != NULL) {
        player->reset_control(player);
    }
}

/* Every frame of the field's idle step (fieldstg_manager_update), before the room's portal. */
void def_story_frame(void) {
    def_language_kaili(port_girls_on()); /* the Guide's lines name Kail's friend Junior in the Kaili mode */
    if (!def_on()) {
        return;
    }
    if ((story.count != 0 || story.releasing) && story.map != gamestate_data.funcs.get_map()) {
        memset(&story, 0, sizeof(story)); /* the map left under the talk: its dialog went with the heap */
    }
    if (story.releasing) {
        story_release();
        if (!story.releasing && dream.phase == DR_REPLY) {
            dream_leave(); /* Igor's word on the answer was the dream's last */
        }
        return;
    }
    if (dream.phase == DR_CHOOSE) {
        dream_choose();
        return;
    }
    if (choice.open) {
        choice_frame(); /* a scene's choice: its event waits on it (def_story_event_dialog) */
        return;
    }
    if (story.count != 0) {
        fieldstg_stage.event_running = 1;
        if (story.dialog == NULL) {
            FieldstgActor *player = (FieldstgActor *)heap_objects.find(5, -1, 0);

            if (story.next >= story.count || player == NULL) {
                story.count = 0;
                story.releasing = 1;
                return;
            }
            FieldstgActor *on = story.speaker ? (FieldstgActor *)heap_objects.find(5, story.speaker, -1) : NULL;

            story.dialog = fieldstg_dialog_create_talk(on != NULL ? on : player, story.messages[story.next++]);
        }
        story.dialog = heap_objects.try_run((Object *)story.dialog);
        return;
    }
    if (idle()) {
        seiryu_hook();
        if (story.count == 0) {
            protocol_gate_frame();
        }
        if (story.count == 0) {
            def_time_gate_frame(story_talk); /* the Warp Gate held shut: Igor's voice (definitive_time_room.c) */
        }
        if (story.count == 0) {
            dream_hook();
        }
        if (story.count == 0 && !dream.on) {
            galactic_hook();
        }
        if (story.count == 0 && !dream.on) {
            oikawa_hook();
        }
    }
}

void def_story_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, story);
    PORT_STATE_VAR(s, st_junior);
    PORT_STATE_VAR(s, st_igor_text);
    PORT_STATE_VAR(s, dream);
    PORT_STATE_VAR(s, choice);
    PORT_STATE_VAR(s, st_galactic);
    PORT_STATE_VAR(s, oik);
    PORT_STATE_VAR(s, ok_talks);
    PORT_STATE_VAR(s, ok_actors);
}
