/* The Velvet Room (the owner's plan, .plan/tasks/velvet-room-plan.md): the Asuka City bar (map 0x20F, WSTAG255) run as a
 * room of its own, where the Black Market's trader, a mentor and Etemon live, reached through a blue door that stands in
 * every inn of the Guardromon network (port/game/definitive_travel.c's nodes).
 *
 * The map keeps its own code and background: `dw3_velvet_stage_setup` runs at the end of WSTAG255's setup and, when the
 * room is entered through a portal, replaces the bar's patrons with the room's figures, its music with a track few maps
 * use and paints the map's holes in the room's blue (the backdrop). The bar is unchanged when the map is entered the way
 * the game enters it (its own two doors from Asuka City), so nothing of the story is lost.
 *
 * The portal: a door of blue light on the floor of each inn, its back to a wall, a few steps from the Guardromon
 * (`doors`: one spot per inn map, the second half's copies sharing the first half's layouts). Stepping onto its foot
 * (VL_RADIUS) leaves for the room; `dw3_velvet_leave` takes the room's front door back to the inn the portal was taken
 * in, and closes the other one (`dw3_velvet_event_closed`).
 *
 * The state is the port's RAM, not the save: a slot saved inside the room loads the bar as the game has it, at the saved
 * spot (a place the game can be in anyway). */
#include <stdlib.h>

#include "common.h"

#include "cdload.h"
#include "fieldstg.h"
#include "gamestate.h"
#include "gfx.h"
#include "heap.h"
#include "message.h"
#include "records.h"

#include "psyq/libgpu.h"
#include "sound.h"

#include "definitive.h"

#include "definitive_velvet_art.h"

extern const unsigned char def_str_VelvetTitle[], def_str_VelvetTitle2[], def_str_TimeTitle[], def_str_TimeTitle2[];
#include "port.h"
#include "port_runtime.h"
#include "psxstack/savestate.h"

#define VL_MAP 0x20F        /* the bar of Asuka City (WSTAG255): the room's stage */
#define VL_LAYER 0x1002     /* the field's layer: the portals and the actors are drawn in it */
#define VL_BG_LAYER 0x1000  /* the layer drawn first: the room's backdrop goes there, behind the map's tiles */
#define VL_PORTAL_DEPTH 3 /* the layer draws its ordering table from its far end, so a lower number is drawn later:
                           * 3 is in front of the floor (7) and the walls, and behind the actors (4), so the player
                           * stands in front of his door instead of vanishing behind it */
#define VL_BG_DEPTH 0       /* the backdrop is behind everything else of its layer */
#define VL_LIGHT_BANDS 6 /* the doorway's light: a band per VL_LIGHT_BANDS rows, dim at the lintel */
#define VL_RADIUS 12        /* pixels: the player this close to a portal's centre takes it */
/* The room's music: the bank and the track of the game's that map 0x25F alone uses (the field plays
 * `fieldstg_stage.sound` after loading `fieldstg_stage.music`'s bank, so both have to change; the owner composes the
 * room's own later). */
#define VL_MUSIC 0x3B
#define VL_SOUND 0x60EC0000
void dw3_velvet_music(s32 *music, s32 *sound); /* definitive_velvet_music.c: the arrangement's bank and key */
int dw3_velvet_igor_head(FieldstgActor *actor, u32 *ot, s32 x, s32 y); /* definitive_velvet_igor.c: his own head */
#define VL_MERCHANT 42  /* the Black Market's trader: the figure that traded in map 0x20C */
/* The mentor (the Gennai of the room, owner 2026-11-16: an old figure of the game until one is drawn): 0x39, the old man
 * the game's Grandpas and Tamer Genji wear (P074JIJI): bald, white-bearded, five facings. Its frame is 24x32, which the
 * VRAM place below holds (its neighbour starts 32 pixels on). It was 0x178, a Kotemon no map places. */
#define VL_MENTOR 0x39
/* The Demon Bell's keeper (Super Hard Mode, definitive_super.c): 370 (0x172), a small Guilmon no map places, the red
 * one of the room. Talking to him hands over the Bell or takes it back (dw3_velvet_shop_talk picks the line). */
#define VL_DEMON 0x172
#define VL_ETEMON 0x61  /* Etemon, at the bar's piano (the owner's joke: he "heard this tune in another game") */
#define VL_PIG 0x13E    /* 318: the one figure of the game whose art no stage uses (P182BOMP) — the owner's toy-box joke */
/* Their lines: entries of the bar's talk file (records_language + 0xE8) that the game itself has no use for (entries 81
 * and 82 are the letter "a" in the disc's English), replaced by tools/text_override/en.txt and pt.txt: the room's own
 * text, translated like every other port text. */
#define VL_MENTOR_LINE 81
#define VL_ETEMON_LINE 82
#define VL_DEMON_GIVE 77 /* the Bell handed over (the letter "a" on the disc, said by no stage) */
#define VL_DEMON_TAKE 78 /* the Bell taken back */
#define VL_PIG_LINE 79 /* also the letter "a" on the disc, and no stage's talk reaches it */
/* The Sepikmon, the hunts' master (the Super Rare hunts, definitive_superhunt_screen.c): actor 61 (0x3D), the Shaman
 * House's own Sepikmon (WSTAG475). Its first line introduces the hunts, the other greets; its board opens after either.
 * Entries 14 and 15 of the bar's talk file are the letter "a" on the disc, and no stage's talk reaches them. */
#define VL_SEPIK 61
#define VL_SEPIK_INTRO 14
#define VL_SEPIK_LINE 15
/* Igor, the room's master (the story's, definitive_story.c; .plan/tasks/velvet-historia.md): until his own art is drawn,
 * actor 109 (0x6D), WSTAG212's man in a black suit (P055GMGD; the owner's "NPC de terno"). Actor 53 (P068SARM) is the
 * other suit of the game, brown with goggles. His line is the story's to pick (def_story_igor_line): the key at the
 * opening, then his reading of the party's DNA forms. */
#define VL_IGOR 109
/* The shopkeeper's lines (entries of the same file that no stage's talk reaches): the two greetings, said in turn one
 * per visit, the one after a trade, and the one after a visit that bought nothing. */
#define VL_SHOP_GREET_A 8
#define VL_SHOP_GREET_B 9
#define VL_SHOP_BOUGHT 12
#define VL_SHOP_LEFT 13
#define VL_SHOP_OIKAWA 99 /* his first word there, after the Bug Maze Pit (the letter "a" on the disc, no stage's) */

/* Where the player appears in the room: just inside the front door (the map's own exit spot, attr layer 7 of map
 * 0x20F), facing in. */
#define VL_ENTRY_X 215
#define VL_ENTRY_Y 335
#define VL_ENTRY_DIR 1

/* The room's figures: where they stand (on the floor, clear of the door and of each other). */
#define VL_MERCHANT_X 275 /* behind the bar's counter, where the game's own bartender stands (WSTAG255's actor at
                          * 275,255). The talk reaches a customer whose front point falls in his box: 32 pixels wide and
                          * half that tall (fieldstg_actor_find_at), so the player may stand up to 24 pixels below him —
                          * the counter's own front row */
#define VL_MERCHANT_Y 255
#define VL_MERCHANT_DIR 1 /* facing down-left, the way the counter runs (the owner picked it on the screenshots) */
#define VL_MENTOR_X 352 /* in front of the room's upper door (its cells x 336..376, y 168..184), the Time Room's way
                         * (owner, 2026-11-18): the door is still reached from the left, along the strip at y 176 */
#define VL_MENTOR_Y 194
#define VL_MENTOR_DIR 1 /* down-left: towards the room */
#define VL_DEMON_X 126 /* upstairs, by the stairs' head */
#define VL_DEMON_Y 200
#define VL_DEMON_DIR 0
#define VL_ETEMON_X 352 /* at the piano's keys, facing them (dir 5: the keyboard is up and to his right, and the
                         * player finds his back; he turns to talk) */
#define VL_ETEMON_Y 284
#define VL_ETEMON_DIR 5
#define VL_SEPIK_X 86 /* upstairs in the corner left of the Demon Bell's keeper (owner, 2026-11-18): off the way from
                       * the stairs' head to the upper floor (free ground x 72..100 there) */
#define VL_SEPIK_Y 214
#define VL_SEPIK_DIR 7 /* down-right: towards the stairs' head */
#define VL_IGOR_X 268 /* on the lower floor, on the carpet's far side from the door: the first figure the entry faces */
#define VL_IGOR_Y 318
#define VL_IGOR_DIR 7 /* towards the door */
/* Igor's scenes (vl_scene: the dreams, Seiryu's showing): the player comes in beside him, the two face each other. */
/* Igor faces down-left, the one facing his own head is drawn for (definitive_velvet_igor.c: frame 9; facing left would
 * show the game's figure's head), so the player stands two steps in front of him that way (fieldstg_front_offsets[1]). */
#define VL_SCENE_X 246
#define VL_SCENE_Y 340
#define VL_SCENE_DIR 5      /* up-right, at him */
#define VL_SCENE_IGOR_DIR 1 /* down-left, at the player */
#define VL_PIG_X 226 /* on the room's floor, by the carpet's edge, left of the counter's front (owner: room for it) */
#define VL_PIG_Y 296
#define VL_PIG_DIR 0

#define VL_BANDS 16 /* the backdrop's horizontal bands: 240 / 16 = one every 15 lines */
#define VL_TOP_R 8  /* the room's blue at the top of the screen, from the owner's mock */
#define VL_TOP_G 14
#define VL_TOP_B 60
#define VL_BOTTOM_R 17 /* and at the bottom (a straight ramp between them) */
#define VL_BOTTOM_G 43
#define VL_BOTTOM_B 149

#define VL_SND_PORTAL 0x40004 /* the map warps' sound: the portal's */

/* A portal: the inn's map and the centre of its foot, in pixels. Every layout has two maps (the second half of the game
 * repeats the first half's inns), so both are listed. Not const: the owner's test build moves a door with a poke. */
typedef struct VelvetDoor {
    s16 map;
    s16 x;
    s16 y;
} VelvetDoor;

static VelvetDoor doors[] = {
    /* map, x, y: the foot of the doorway. Each one was looked at on its own inn, at 2x, with the room's own figures
     * where the game puts them: on the floor, its back to a wall or a rock, and never in front of a figure. Where a map
     * has no wall to stand against (the canyon of 0x247, the floating platform of 0x25B) the doorway's own frame holds
     * it. */
    { 0x20A, 339, 192 }, { 0x279, 339, 192 }, /* Asuka City: the room's right wall, by the stairs */
    { 0x223, 205, 269 }, { 0x292, 205, 269 }, /* the deck's log end, between the bedroom and the lower room */
    /* the upper landing, on the rug beside Repeating Tom: the door his news opens (definitive_story.c) */
    { 0x230, 228, 388 }, { 0x29E, 228, 388 },
    { 0x238, 300, 440 }, { 0x2A5, 300, 440 }, /* the shop's floor: no wall there, but nothing in front of it */
    { 0x23F, 580, 392 }, { 0x2AC, 580, 392 }, /* the bazaar's white wall, by the vine arch */
    { 0x247, 540, 304 }, { 0x2B1, 540, 304 }, /* the canyon: at the foot of the metal ramp */
    { 0x249, 403, 187 }, { 0x2B3, 403, 187 }, /* the ledge's rock step */
    { 0x25B, 355, 313 }, { 0x2C4, 355, 313 }, /* the floating platform's edge */
    { 0x25D, 1074, 604 }, { 0x2C6, 1074, 604 }, /* the paved ledge, against the brick wall */
    { 0x263, 383, 235 }, { 0x2CB, 383, 235 }, /* the cabin's log wall, by the armchair */
    { 0x269, 553, 262 }, { 0x2D1, 553, 262 }, /* the machine block's face */
    { 0x26D, 396, 520 },
    { 0x26F, 1156, 833 }, { 0x2D6, 1156, 833 }, /* the base's metal wall, clear of its figures */
};
#define VL_DOORS ((s32)(sizeof(doors) / sizeof(doors[0])))

/* The mentor's and Etemon's one line each: the game's own dialog, from the bar's talk file (WSTAG255's
 * records_language + 0xE8), with the port's own text there. */
static FieldstgTalk vl_mentor_talks[2] = { { NULL, NULL, VL_MENTOR_LINE }, { NULL, NULL, 0 } };
static FieldstgTalk vl_etemon_talks[2] = { { NULL, NULL, VL_ETEMON_LINE }, { NULL, NULL, 0 } };
static FieldstgTalk vl_demon_talks[2] = { { NULL, NULL, VL_DEMON_GIVE }, { NULL, NULL, 0 } };
static FieldstgTalk vl_pig_talks[2] = { { NULL, NULL, VL_PIG_LINE }, { NULL, NULL, 0 } };
static FieldstgTalk vl_sepik_talks[2] = { { NULL, NULL, VL_SEPIK_LINE }, { NULL, NULL, 0 } };
static FieldstgTalk vl_igor_talks[2] = { { NULL, NULL, 0 }, { NULL, NULL, 0 } };

/* The room's figures (FieldstgPlacedActor: flags, talks, ID, VRAM place, x, y, dir). The VRAM places are three of the
 * bar's own patron slots, free while the patrons are away. */
static FieldstgTalk vl_merchant_talks[2] = { { NULL, NULL, VL_SHOP_GREET_A }, { NULL, NULL, 0 } };
static FieldstgPlacedActor vl_merchant = { NULL, vl_merchant_talks, VL_MERCHANT, 2, VL_MERCHANT_X, VL_MERCHANT_Y,
                                           VL_MERCHANT_DIR };
static FieldstgPlacedActor vl_mentor = { NULL, vl_mentor_talks, VL_MENTOR, 5, VL_MENTOR_X, VL_MENTOR_Y, VL_MENTOR_DIR };
static FieldstgPlacedActor vl_etemon = { NULL, vl_etemon_talks, VL_ETEMON, 4, VL_ETEMON_X, VL_ETEMON_Y, VL_ETEMON_DIR };
static FieldstgPlacedActor vl_demon = { NULL, vl_demon_talks, VL_DEMON, 3, VL_DEMON_X, VL_DEMON_Y, VL_DEMON_DIR };
static FieldstgPlacedActor vl_pig = { NULL, vl_pig_talks, VL_PIG, 6, VL_PIG_X, VL_PIG_Y, VL_PIG_DIR };
/* VRAM place 11 (the bar's 13th): the last patron's, free while they are away */
static FieldstgPlacedActor vl_sepik = { NULL, vl_sepik_talks, VL_SEPIK, 11, VL_SEPIK_X, VL_SEPIK_Y, VL_SEPIK_DIR };
/* VRAM place 8 (the bar's 10th patron's). Of the bar's places the room leaves free, 8, 9 and 10 draw him; 7, 12 and 13
 * show garbage for any figure here, the Sepikmon's too (tried 2026-11-17, .dumps/tasks/story/try). */
static FieldstgPlacedActor vl_igor = { NULL, vl_igor_talks, VL_IGOR, 8, VL_IGOR_X, VL_IGOR_Y, VL_IGOR_DIR };
/* The room's guests and the level each comes in at (def_story_velvet_level, owner 2026-11-17: they arrive with the
 * story, the Sepikmon first and the Black Market last), with the code it calls the player over by on the first visit
 * after (0: never; the Bell's keeper would hand the Bell over by being talked to). */
static const struct {
    FieldstgPlacedActor *who;
    s32 level;
    s32 greet;
} vl_guests[] = {
    { &vl_sepik, 1, DW3_VELVET_PULL_SEPIK }, { &vl_mentor, 2, DW3_VELVET_PULL_MENTOR }, /* Gennai greets at 2 */
    { &vl_igor, 2, DW3_VELVET_PULL_IGOR },   { &vl_demon, 3, 0 },
    { &vl_etemon, 4, DW3_VELVET_PULL_ETEMON }, { &vl_pig, 4, 0 },
    { &vl_merchant, DEF_VELVET_LEVEL_MAX, DW3_VELVET_PULL_MERCHANT },
};
#define VL_GUESTS ((s32)(sizeof(vl_guests) / sizeof(vl_guests[0])))
static FieldstgPlacedActor *vl_actors[VL_GUESTS + 1]; /* the guests of the room's level (vl_guests_now) */
/* Igor alone, a scene (the owner's, 2026-11-17): the opening's dream, and Seiryu's showing of the room. */
static FieldstgPlacedActor *vl_dream_actors[] = { &vl_igor, NULL };

static s32 vl_shop_greet;  /* which greeting the next visit begins with (they cycle) */
static s32 vl_shop_last;   /* how the last visit ended: 0 nothing, 1 it traded, 2 it bought nothing */
static s32 vl_shop_said;   /* the outcome his line already answered: a run of equal visits is answered once */
static s32 vl_shop_phase;  /* 0 idle, 1 the greeting is on screen, 2 the market is to be opened */
static s32 vl_mentor_phase; /* 1 the mentor's line is on screen: his panel (the purification) opens after it */
static s32 vl_room; /* the room is running (set when a portal is taken, cleared when the map is left) */
static s32 vl_from; /* the inn the portal was taken in */
static s32 vl_sepik_phase; /* 1 the Sepikmon's line is on screen: his board opens after it */
static s32 vl_pulled; /* the room was entered by dw3_velvet_pull: who speaks to the player on arrival (VL_PULL_*) */
static s32 vl_igor_phase; /* 1 Igor's line is on screen: the story hears when it closes (def_story_igor_done) */
static s32 vl_scene;      /* 1 Igor alone (vl_dream_actors), 0 the room as its level has it */
static s32 vl_armed;      /* the player was seen off the map's door: standing on it now takes it */

/* The feature (DW3_FEAT_VELVET, default on): off under a replay script, which must replay the emulator's own frames. */
static int vl_on(void) {
    static int on = -1;

    if (on < 0) {
        on = dw3_feat_velvet
             && (!port_script_active || getenv("DW3_DEFINITIVE") != NULL || getenv("DW3_FEAT_VELVET") != NULL);
    }
    return on;
}

/* Into the room: by its door, or beside Igor in his scenes. */
static void vl_enter(void) {
    if (vl_scene) {
        fieldstg_goto_map(VL_MAP, -1, VL_SCENE_X << 8, VL_SCENE_Y << 8, VL_SCENE_DIR);
    } else {
        fieldstg_goto_map(VL_MAP, -1, VL_ENTRY_X << 8, VL_ENTRY_Y << 8, VL_ENTRY_DIR);
    }
}

/* The guests of the room's level, as the stage's actor list. */
static FieldstgPlacedActor **vl_guests_now(void) {
    s32 level = def_story_velvet_level(), i, n = 0;

    for (i = 0; i < VL_GUESTS; i++) {
        if (vl_guests[i].level <= level) {
            vl_actors[n++] = vl_guests[i].who;
        }
    }
    vl_actors[n] = NULL;
    return vl_actors;
}

/* The figure a call-over code names. */
static FieldstgPlacedActor *vl_who(s32 code) {
    s32 i;

    if (code == DW3_VELVET_PULL_IGOR) {
        return &vl_igor;
    }
    if (code == DW3_VELVET_PULL_TRIAL) {
        return def_time_challenger(); /* the Time Room's trial (definitive_time_room.c) */
    }
    for (i = 0; i < VL_GUESTS; i++) {
        if (vl_guests[i].greet == code) {
            return vl_guests[i].who;
        }
    }
    return &vl_sepik;
}

/* The room's figure a call-over code names (the Time Room's cast is the room's own figures). */
FieldstgPlacedActor *dw3_velvet_figure(s32 code) {
    return vl_who(code);
}

/* The figure a code names calls the player over on the next free frame (the Time Room: Igor, a trial's challenger). */
void dw3_velvet_call(s32 code) {
    vl_pulled = code;
}

/* The room's music (its bank and key), for the Time Room's trial areas too. */
void dw3_velvet_room_music(s32 *music, s32 *sound) {
    *music = VL_MUSIC;
    *sound = VL_SOUND;
    dw3_velvet_music(music, sound);
}

/* The first visit after the room's level rose: the newest guest that greets calls the player over (0: none). */
static s32 vl_newcomer(void) {
    s32 level = def_story_velvet_level(), i, code = 0, best = 0;

    if (level <= def_record_velvet_seen()) {
        return 0;
    }
    def_record_set_velvet_seen(level);
    for (i = 0; i < VL_GUESTS; i++) {
        if (vl_guests[i].greet != 0 && vl_guests[i].level <= level && vl_guests[i].level > best) {
            best = vl_guests[i].level;
            code = vl_guests[i].greet;
        }
    }
    return code;
}

static const VelvetDoor *vl_door(s32 map) {
    s32 i;

    for (i = 0; i < VL_DOORS; i++) {
        if (doors[i].map == map) {
            return &doors[i];
        }
    }
    return NULL;
}

/* An opaque quadrilateral of the field's layer, at `depth`, in `layer_id`. Opaque only: a semi-transparent polygon
 * leaves the GPU's blend mode set for the field's own primitives and the map's holes then show the last texture page
 * instead of the void (a screenshot's worth of proof), so the room's light is drawn in solid colours. */
static void vl_quad(s32 layer_id, s32 depth, s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u8 r, u8 g,
                    u8 b) {
    GfxLayer *layer = gfx_module.funcs.get_layer(layer_id);
    POLY_F4 *poly = gfx_module.funcs.get_packet();

    setPolyF4(poly);
    setRGB0(poly, r, g, b);
    poly->x0 = x0;
    poly->y0 = y0;
    poly->x1 = x1;
    poly->y1 = y1;
    poly->x2 = x2;
    poly->y2 = y2;
    poly->x3 = x3;
    poly->y3 = y3;
    addPrim(layer->get_ot_entry(layer, depth), poly);
    gfx_module.funcs.set_packet(poly + 1);
}

/* The room's blue behind the map: the void the bar's tiles leave is the layer's own background, a band of colour per
 * VL_BANDS lines ramping from VL_TOP_* to VL_BOTTOM_*, as the owner's mock has it. */
static void vl_backdrop(void) {
    s32 band_h = 240 / VL_BANDS;
    s32 i;

    for (i = 0; i < VL_BANDS; i++) {
        s32 r = VL_TOP_R + (VL_BOTTOM_R - VL_TOP_R) * i / (VL_BANDS - 1);
        s32 g = VL_TOP_G + (VL_BOTTOM_G - VL_TOP_G) * i / (VL_BANDS - 1);
        s32 b = VL_TOP_B + (VL_BOTTOM_B - VL_TOP_B) * i / (VL_BANDS - 1);
        s32 y = i * band_h;
        s32 y2 = i == VL_BANDS - 1 ? 240 : y + band_h;

        vl_quad(VL_BG_LAYER, VL_BG_DEPTH, 0, y, 320, y, 0, y2, 320, y2, r, g, b);
    }
}

/* A portal: a doorway of blue light standing at (x, y) — a near-black frame of two posts, a lintel and a threshold, and
 * between them the light, brightest at the floor — pulsing with the frame counter. Drawn to stand on its own, so a spot
 * with no wall behind still reads as a door and not as a plaque on the ground. */
static void vl_portal(s32 x, s32 y) {
    static const u8 frame_rgb[3] = { 6, 10, 34 };
    static const u8 light_rgb[VL_LIGHT_BANDS][3] = { { 40, 88, 196 }, { 60, 112, 214 }, { 92, 148, 236 },
        { 126, 184, 250 }, { 168, 214, 255 }, { 216, 242, 255 } };
    GfxLayer *layer = gfx_module.funcs.get_layer(VL_LAYER);
    s32 ofs[2];
    s32 pulse = gfx_module.funcs.get_frames() % 48;
    s32 lit;
    s32 i;
    const s32 half = 13; /* the doorway's half width */
    const s32 tall = 44; /* and its height */
    const s32 post = 3;  /* the frame's thickness */
    s32 inner_w = half - post;
    s32 inner_h = tall - post;

    if (pulse >= 24) {
        pulse = 48 - pulse; /* 0..23, up and down */
    }
    lit = 200 + pulse * 2; /* the pulse: 200..246 of 255 */
    layer->get_scroll(layer, ofs);
    x -= ofs[0];
    y -= ofs[1];
    if (x < -40 || x > 360 || y < -80 || y > 280) {
        return;
    }
    /* the frame goes in first: the layer draws what is added last behind, so the frame stays in front of its light */
    vl_quad(VL_LAYER, VL_PORTAL_DEPTH, x - half, y - tall, x - half + post, y - tall, x - half, y, x - half + post, y,
            frame_rgb[0], frame_rgb[1], frame_rgb[2]);
    vl_quad(VL_LAYER, VL_PORTAL_DEPTH, x + half - post, y - tall, x + half, y - tall, x + half - post, y, x + half, y,
            frame_rgb[0], frame_rgb[1], frame_rgb[2]);
    vl_quad(VL_LAYER, VL_PORTAL_DEPTH, x - half, y - tall, x + half, y - tall, x - half, y - tall + post, x + half,
            y - tall + post, frame_rgb[0], frame_rgb[1], frame_rgb[2]);
    vl_quad(VL_LAYER, VL_PORTAL_DEPTH, x - half - 3, y - 2, x + half + 3, y - 2, x - half - 1, y + 5, x + half + 1, y + 5,
            frame_rgb[0], frame_rgb[1], frame_rgb[2]);
    /* the light between them: three bands, the dimmest at the lintel */
    for (i = 0; i < VL_LIGHT_BANDS; i++) {
        s32 y0 = y - inner_h + (inner_h * i) / VL_LIGHT_BANDS;
        s32 y1 = y - inner_h + (inner_h * (i + 1)) / VL_LIGHT_BANDS;

        vl_quad(VL_LAYER, VL_PORTAL_DEPTH, x - inner_w, y0, x + inner_w, y0, x - inner_w, y1, x + inner_w, y1,
                (u8)(light_rgb[i][0] * lit / 255), (u8)(light_rgb[i][1] * lit / 255), (u8)(light_rgb[i][2] * lit / 255));
    }
}

/* Igor first: the game's figure keeps being drawn, with his own head over it (definitive_velvet_igor.c).
 * The room's shopkeeper, the Black Market's dealer: his own art (definitive_velvet_art.h, drawn by hand for this port)
 * drawn by FIELDSTG's actor draw in place of the game's figure of actor VL_MERCHANT, so he takes the actor's own place in
 * the field's drawing order (the player passes in front of him and behind him as of any figure) and the figure under
 * him never shows. One flat quad per run of one palette colour: no VRAM, no CLUT. `ot` is the actor's ordering-table
 * entry, (x, y) the screen spot of its feet. One drawing whatever way the actor faces (he does not turn: see
 * dw3_velvet_actor_setup); he breathes as the game's townsfolk stand, on the art's idle (game ticks are vsyncs, as
 * port_frames counts). 1 when he was drawn (the caller leaves the game's sprite out, and keeps its shadow). */
/* The dealer's figure, drawn by hand over actor VL_MERCHANT's sprite: behind the room's counter, as the Time Room's
 * first challenger, and as the hooded man of the Oikawa's arc in the Bug Maze Pit (definitive_story.c). */
static int vl_dealer(const FieldstgActor *actor) {
    const FieldstgPlacedActor *placed = actor != NULL ? actor->placed : NULL;

    if (!vl_on() || placed == NULL) {
        return 0;
    }
    if (vl_room && placed == &vl_merchant) {
        return 1;
    }
    return placed->id == VL_MERCHANT && ((vl_room && placed == def_time_challenger()) || def_story_oikawa_figure(placed));
}

int dw3_velvet_actor_art(FieldstgActor *actor, u32 *ot, s32 x, s32 y) {
    s32 frame;
    s32 i;
    s32 t;

    if (vl_on() && vl_room && actor != NULL && actor->placed == &vl_igor) {
        return dw3_velvet_igor_head(actor, ot, x, y); /* over the game's figure, which is drawn too */
    }
    if (!vl_dealer(actor)) {
        return 0;
    }
    x -= VELVET_ART_LEFT;
    y -= VELVET_ART_TOP;
    if (x < -VELVET_ART_W || x > 320 + 64 || y < -VELVET_ART_H || y > 240) {
        return 1; /* off the screen: nothing of his to draw, and nothing of the figure under him either */
    }
    for (i = 0, t = 0; i < VELVET_ANIM_STEPS; i++) {
        t += velvet_anim[i][1];
    }
    t = (s32)(port_frames % t);
    for (i = 0; i < VELVET_ANIM_STEPS - 1 && t >= velvet_anim[i][1]; i++) {
        t -= velvet_anim[i][1];
    }
    frame = velvet_anim[i][0];
    for (i = velvet_frame_runs[frame]; i < velvet_frame_runs[frame + 1]; i++) {
        const unsigned char *run = velvet_runs[i];
        const unsigned char *rgb = velvet_palette[run[3] & 15];
        POLY_F4 *poly = gfx_module.funcs.get_packet();
        s32 x0 = x + run[1];
        s32 x1 = x0 + run[2];
        s32 y0 = y + run[0];

        setPolyF4(poly);
        setRGB0(poly, rgb[0], rgb[1], rgb[2]);
        poly->x0 = x0;
        poly->y0 = y0;
        poly->x1 = x1;
        poly->y1 = y0;
        poly->x2 = x0;
        poly->y2 = y0 + 1;
        poly->x3 = x1;
        poly->y3 = y0 + 1;
        addPrim(ot, poly);
        gfx_module.funcs.set_packet(poly + 1);
    }
    return 1;
}

extern s32 fieldstg_actor_talk_to(FieldstgActor *obj, FieldstgPos *pos);

/* The Sepikmon speaks to the player as if talked to (the room's first visit, dw3_velvet_pull): once the field is idle
 * and the player has control. 1 when the talk began. */
extern FieldstgPos fieldstg_front_offsets[8];

static int vl_call_over(const FieldstgPlacedActor *who) {
    FieldstgActor *player;
    FieldstgActor *obj;

    if (fieldstg_stage.event_running != 0 || fieldstg_stage.menu_open != 0 || fieldstg_stage.actor_busy != 0 ||
        fieldstg_stage.battle_starting != 0 || fieldstg_stage.title_shown != 0) {
        return 0;
    }
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player == NULL || player->base.state != OBJECT_STATE_RUN || player->control == NULL) {
        return 0;
    }
    for (obj = (FieldstgActor *)heap_objects.find(5, -1, 1); obj != NULL; obj = (FieldstgActor *)heap_objects.find_next()) {
        if (obj->placed == who && obj->base.state == OBJECT_STATE_RUN) {
            FieldstgPos pos = obj->pixel_pos;
            s32 dx = pos.x - player->pixel_pos.x, dy = pos.y - player->pixel_pos.y, d, best = 0, score = -0x7FFFFFFF;

            for (d = 0; d < 8; d++) { /* the player turns to the caller (the entry left him facing the door) */
                s32 dot = dx * fieldstg_front_offsets[d].x + dy * fieldstg_front_offsets[d].y;

                if (dot > score) {
                    score = dot;
                    best = d;
                }
            }
            player->dir = best;
            port_log("velvet: frame %ld: actor %d calls the player over", port_frames, who->id);
            return fieldstg_actor_talk_to(player, &pos) != 0;
        }
    }
    return 0; /* his actor is not made yet (the stage is still setting up): the next frame */
}

/* The room pulls the player in from map `from` (an inn: the room's door goes back to its Guardromon; another map: the
 * story sends the player back itself), as a portal taken there would, and `who` speaks first: the Sepikmon
 * (DW3_VELVET_PULL_SEPIK) or Igor (DW3_VELVET_PULL_IGOR, the opening's dream: definitive_story.c). */
void dw3_velvet_pull_by(s32 from, s32 who) {
    if (!vl_on() || vl_room) {
        return;
    }
    vl_from = from;
    vl_room = 1;
    vl_pulled = who;
    vl_scene = who == DW3_VELVET_PULL_IGOR; /* the opening's dream: Igor alone */
    sound_module.play(VL_SND_PORTAL);
    port_log("velvet: frame %ld: pulled into the room from map 0x%X (back to map 0x%X)", port_frames,
             gamestate_data.funcs.get_map(), from);
    vl_enter();
}

void dw3_velvet_pull(s32 from) {
    dw3_velvet_pull_by(from, DW3_VELVET_PULL_SEPIK);
}

/* The screen cuts inside the room (the story's: Igor's scene is over): the map is entered again, now as its level has
 * it, and `who` calls the player over. */
void dw3_velvet_cut(s32 who) {
    if (!vl_on() || !vl_room) {
        return;
    }
    vl_scene = 0;
    vl_pulled = who;
    def_record_set_velvet_seen(def_story_velvet_level()); /* the newcomer is greeted by this call */
    port_log("velvet: frame %ld: the scene cuts (who %d calls over)", port_frames, who);
    vl_enter();
}

int dw3_velvet_in_room(void) {
    return vl_on() && vl_room;
}

/* The room's backdrop and its portals: called every frame from FIELDSTG's idle step, before the frame is drawn. */
static const VelvetDoor *vl_door_at(s32 map, s32 *gate);

void dw3_velvet_draw(void) {
    const VelvetDoor *door;
    s32 gate;

    if (!vl_on()) {
        return;
    }
    if (vl_room) {
        vl_backdrop();
        def_time_frame(); /* the Time Room's trials (definitive_time_room.c) */
        if (vl_pulled && vl_who(vl_pulled) != NULL && vl_call_over(vl_who(vl_pulled))) {
            vl_pulled = 0;
        }
        if (vl_igor_phase == 1 && fieldstg_stage.actor_busy == 0) {
            vl_igor_phase = 0;
            def_story_igor_done(); /* the opening's dream ends with his line */
        }
        if (vl_sepik_phase == 1 && fieldstg_stage.actor_busy == 0) {
            vl_sepik_phase = 0;
            def_superhunt_board_offer(); /* the Super Rare hunts (definitive_superhunt_screen.c) */
        }
        if (vl_mentor_phase == 1 && fieldstg_stage.actor_busy == 0) {
            vl_mentor_phase = 0;
            def_purify_offer(); /* Blast phase 2 (definitive_purify.c) */
        }
        if (vl_shop_phase == 1 && fieldstg_stage.actor_busy == 0) {
            vl_shop_phase = 2; /* the greeting is over: the market takes the field */
        } else if (vl_shop_phase == 2) {
            if (def_bm_talk(VL_MAP, VL_MERCHANT)) {
                vl_shop_phase = 3;
                def_story_market_opened(); /* the Oikawa's arc: the Guide's errand is done */
            }
        } else if (vl_shop_phase == 3 && !def_bm_screen_open()) {
            vl_shop_last = def_bm_bought() ? 1 : 2; /* his next line answers how this visit ended */
            vl_shop_phase = 0;
            port_log("velvet: the visit ended, %s", vl_shop_last == 1 ? "it traded" : "it bought nothing");
        }
        return;
    }
    door = vl_door_at(gamestate_data.funcs.get_map(), &gate);
    if (door != NULL && fieldstg_stage.title_shown == 0 && def_story_velvet_level() > 0) {
        vl_portal(door->x, door->y);
    }
}

/* The door on `map`: an inn's, or the Time Room's beside the Warp Gate (`gate` set). */
static const VelvetDoor *vl_door_at(s32 map, s32 *gate) {
    static VelvetDoor time_door;
    s32 x, y;

    *gate = 0;
    if (def_time_door(map, &x, &y)) {
        time_door.map = (s16)map;
        time_door.x = (s16)x;
        time_door.y = (s16)y;
        *gate = 1;
        return &time_door;
    }
    return vl_door(map);
}

/* One frame of the field's idle step: takes the portal the player stands on. 0 always: the room owns no screen. */
int dw3_velvet_frame(void) {
    FieldstgActor *player;
    const VelvetDoor *door;
    s32 map;
    s32 dx;
    s32 dy;
    s32 gate;

    if (!vl_on() || vl_room || fieldstg_stage.event_running != 0 || fieldstg_stage.menu_open != 0
        || fieldstg_stage.battle_starting != 0 || fieldstg_stage.title_shown != 0) {
        return 0;
    }
    map = gamestate_data.funcs.get_map();
    door = vl_door_at(map, &gate);
    if (door == NULL || def_story_velvet_level() == 0) {
        vl_armed = 0;
        return 0; /* the doors open with the story (Repeating Tom's news) */
    }
    player = (FieldstgActor *)heap_objects.find(5, -1, 0);
    if (player == NULL || player->base.state != OBJECT_STATE_RUN || player->base.step >= 5 || player->control == NULL) {
        return 0;
    }
    dx = player->pixel_pos.x - door->x;
    dy = player->pixel_pos.y - door->y;
    if (dx * dx + dy * dy > VL_RADIUS * VL_RADIUS) {
        vl_armed = 1;
        return 0;
    }
    if (!vl_armed) {
        return 0; /* a door that opened under the player (Tom's news) takes him once he stepped off and back on */
    }
    vl_armed = 0;
    vl_from = map;
    vl_room = 1;
    vl_scene = 0;
    vl_pulled = 0;
    if (gate) {
        def_time_enter(); /* the Warp Gate's door: the room is the Time Room, Igor tells it */
        vl_pulled = DW3_VELVET_PULL_IGOR;
    } else if (def_story_room_intro()) {
        vl_scene = 1; /* Seiryu: Igor alone shows the room again, then the scene cuts (dw3_velvet_cut) */
        vl_pulled = DW3_VELVET_PULL_IGOR;
    } else {
        vl_pulled = vl_newcomer();
    }
    sound_module.play(VL_SND_PORTAL);
    port_log("velvet: frame %ld: the portal of map 0x%X taken (scene %d, who %d calls over)", port_frames, map, vl_scene,
             vl_pulled);
    vl_enter();
    return 0;
}

/* FIELDSTG leaves a map for another (fieldstg_goto_map, pixels in 24.8): from the room, the inn the portal was taken
 * in, at its Guardromon's spot. */
void dw3_velvet_leave(s32 *map, s32 *x, s32 *y, s32 *dir) {
    s32 node_x;
    s32 node_y;
    s32 node_dir;
    s32 time;

    if (vl_room && (time = def_time_leave(map, x, y, dir)) != 0) {
        if (time == 2) { /* out of the Time Room, to the gate */
            vl_room = 0;
            vl_pulled = 0;
            vl_scene = 0;
        }
        return; /* the Time Room's run goes on, or the gate takes the player back */
    }
    if (!vl_room || *map == VL_MAP) {
        return;
    }
    vl_room = 0;
    vl_pulled = 0;
    if (vl_scene) { /* a dream: the story sends the player back where it found him */
        vl_scene = 0;
    } else if (dw3_travel_node(vl_from, &node_x, &node_y, &node_dir)) {
        *x = node_x << 8; /* at the Guardromon of the inn it was entered from, as a teleport lands there */
        *y = node_y << 8;
        *dir = node_dir;
    }
    port_log("velvet: frame %ld: out of the room, to map 0x%X", port_frames, vl_from);
    *map = vl_from;
}

/* 1 when the room's map event (`type`, `param`) is closed while the room runs: the bar's side door, the one that in the
 * game goes back to the inn of Asuka City (the owner's "porta de cima fica fechada"). The bar's other door, at the
 * bottom of the room, is the room's way out (dw3_velvet_leave). */
int dw3_velvet_event_closed(s32 type, s32 param) {
    if (def_time_warp_closed(type, param)) {
        return 1; /* the Warp Gate to the Gunslinger, until the Time Room is passed */
    }
    if (def_story_event_closed(type, param)) {
        return 1; /* a story hook's (definitive_story.c) */
    }
    if (def_time_on() && def_time_challenger() != NULL) {
        return type == 8; /* a trial's area: none of its own events (its exits lead back to the room) */
    }
    return vl_room && type == 1 && param == 0x20A && !def_time_side_door_open();
}

/* The room's figures, as the actor is made: the shopkeeper keeps his own sprite and keeps facing his counter instead of
 * turning to whoever talks to him (a placed actor turns to the talker otherwise). */
void dw3_velvet_actor_setup(FieldstgActor *actor) {
    if (!vl_on() || actor == NULL || actor->base.key2 != 1) {
        return;
    }
    if (vl_dealer(actor) || (vl_room && actor->placed == &vl_igor)) {
        /* Igor's head is drawn for one facing, the dealer's art for one */
        actor->no_turn = 1;
    }
}

/* Who the player talks to in the room when the game's own lookup finds nobody at `pos` (the player's front point:
 * fieldstg_actor_find_at takes a figure's box of its width either side and half that above and below, 16 x 8 for every
 * figure here). The dealer stands behind the bar's counter, and its front row keeps the player's front point 12 to 23
 * pixels below him (measured walking up to it from x 256 to 296): out of his box from anywhere. So each figure of the
 * room reaches further, the dealer down over his counter's front row, the others a few pixels round their box (a
 * corner, the piano), the nearest first. Facing away puts the front point 16 pixels the other way, out of reach. */
#define VL_REACH_X 24
#define VL_REACH_Y 16
#define VL_COUNTER_X 30    /* the dealer: across the front row, x 256..296 stops the player under him */
#define VL_COUNTER_UP 16
#define VL_COUNTER_DOWN 28 /* front points 12..23 below him, with room for a step back */
FieldstgActor *dw3_velvet_talk_find(void *front) {
    FieldstgPos *pos = front;
    FieldstgActor *obj;
    FieldstgActor *best = NULL;
    s32 best_d = 0x7FFFFFFF;

    if (!vl_on() || !vl_room || pos == NULL) {
        return NULL;
    }
    for (obj = (FieldstgActor *)heap_objects.find(5, -1, 1); obj != NULL; obj = (FieldstgActor *)heap_objects.find_next()) {
        s32 dx = pos->x - obj->pixel_pos.x;
        s32 dy = pos->y - obj->pixel_pos.y;
        s32 in;

        if (obj->base.key2 != 1 || obj->base.state != OBJECT_STATE_RUN) {
            continue;
        }
        if (obj->placed == &vl_merchant) {
            in = dx >= -VL_COUNTER_X && dx <= VL_COUNTER_X && dy >= -VL_COUNTER_UP && dy <= VL_COUNTER_DOWN;
        } else {
            in = dx >= -VL_REACH_X && dx <= VL_REACH_X && dy >= -VL_REACH_Y && dy <= VL_REACH_Y;
        }
        if (in && (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) < best_d) {
            best_d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            best = obj;
        }
    }
    return best;
}

/* 1 when the text entry (a file's base id and an index) is one of the port's own: the room's figures' lines. The disc
 * has nothing in them (the letter "a", said by nobody) and no other language has either, so the English text is served
 * for every language. */
int dw3_velvet_text_own(s32 base_id, s32 index) {
    if (def_story_text_own(base_id, index) || def_time_text_own(base_id, index) ||
        (base_id == 0xE8 && index == VL_SHOP_OIKAWA)) {
        return 1; /* the story hooks' entries (definitive_story.c, definitive_time_room.c) */
    }
    if (base_id != 0xE8) {
        return 0; /* TALK05, the bar's talk file (records_language + 0xE8) */
    }
    switch (index) {
    case VL_SHOP_GREET_A:
    case VL_SHOP_GREET_B:
    case VL_SHOP_BOUGHT:
    case VL_SHOP_LEFT:
    case VL_PIG_LINE:
    case VL_DEMON_GIVE:
    case VL_DEMON_TAKE:
    case VL_MENTOR_LINE:
    case VL_ETEMON_LINE:
    case VL_SEPIK_INTRO:
    case VL_SEPIK_LINE:
        return 1;
    }
    return 0;
}

/* The visit's opener: the shopkeeper says a greeting (the two take turns) unless the last visit ended in a trade or in
 * nothing bought, which have their own lines. The game's own talk shows it; the market then opens behind it. */
void dw3_velvet_shop_talk(FieldstgActor *actor) {
    s32 time;

    if (def_story_oikawa_talk(actor)) {
        return; /* the Oikawa in the Bug Maze Pit's hall (definitive_story.c) */
    }
    if (vl_on() && vl_room && actor != NULL && actor->placed != NULL && actor->placed->talks != NULL &&
        (time = def_time_talk(actor)) != 0) {
        actor->placed->talks[0].message = time; /* the Time Room's lines: no shop, no board, no panel there */
        port_log("velvet: the Time Room, message %d (actor %d)", time, actor->base.key1);
        return;
    }
    if (vl_on() && vl_room && actor != NULL && actor->base.key1 == VL_DEMON && actor->base.key2 == 1) {
        /* The Bell changes hands as he speaks: the line says what just happened. */
        vl_demon_talks[0].message = def_super_toggle() ? VL_DEMON_GIVE : VL_DEMON_TAKE;
        port_log("velvet: the Bell's keeper, message %d (Super Hard %s)", vl_demon_talks[0].message,
                 def_super_on() ? "on" : "off");
        return;
    }
    if (vl_on() && vl_room && actor != NULL && actor->base.key1 == VL_SEPIK && actor->base.key2 == 1) {
        /* the first meeting introduces the hunts (and gives the tutorial, on his board) */
        s32 story = def_story_sepik_line((def_super_flags() & DEF_SUPER_FLAG_MET) != 0); /* the missing Leader's */

        vl_sepik_talks[0].message = story != 0                                ? story
                                    : (def_super_flags() & DEF_SUPER_FLAG_MET) ? VL_SEPIK_LINE
                                                                               : VL_SEPIK_INTRO;
        def_super_flag_set(DEF_SUPER_FLAG_MET);
        vl_sepik_phase = 1;
        port_log("velvet: the Sepikmon, message %d", vl_sepik_talks[0].message);
        return;
    }
    if (vl_on() && vl_room && actor != NULL && actor->base.key1 == VL_MENTOR && actor->base.key2 == 1) {
        vl_mentor_talks[0].message = VL_MENTOR_LINE; /* the Time Room may have left its own there */
        vl_mentor_phase = 1;
        return;
    }
    if (vl_on() && vl_room && actor != NULL && actor->placed == &vl_igor) {
        vl_igor_talks[0].message = def_story_igor_line(); /* the key, or his reading of the party's bonds */
        vl_igor_phase = 1;
        port_log("velvet: Igor, message %d", vl_igor_talks[0].message);
        return;
    }
    if (!vl_on() || !vl_room || actor == NULL || actor->placed != &vl_merchant || vl_shop_phase != 0) {
        return;
    }
    if (def_record_story(DEF_STORY_OIKAWA_BEATEN) && !def_record_story(DEF_STORY_MARKET_OPEN)) {
        vl_merchant_talks[0].message = VL_SHOP_OIKAWA; /* the hooded man from the Bug Maze Pit, behind the counter */
    } else if (vl_shop_last != 0 && vl_shop_last != vl_shop_said) {
        vl_merchant_talks[0].message = vl_shop_last == 1 ? VL_SHOP_BOUGHT : VL_SHOP_LEFT;
        vl_shop_said = vl_shop_last; /* a run of visits that end the same way is answered once */
    } else {
        vl_merchant_talks[0].message = vl_shop_greet == 0 ? VL_SHOP_GREET_A : VL_SHOP_GREET_B;
        vl_shop_greet ^= 1; /* the greetings take turns */
    }
    vl_shop_last = 0;
    vl_shop_phase = 1;
    port_log("velvet: the shopkeeper opens with message %d", vl_merchant_talks[0].message);
}

int dw3_velvet_shop_greeting(void) {
    return vl_shop_phase == 1;
}


/* The map title's two plates: the room's own name (two lines, as the title card has them), when the map is the room;
 * the bar's name stays otherwise. */
void dw3_velvet_title_window(void *line_0, void *line_1) {
    MessageWindow *w;

    if (!vl_on() || !vl_room) {
        return;
    }
    if (line_0 != NULL) {
        w = (MessageWindow *)line_0;
        w->set_text(w, (u8 *)(def_time_on() ? def_str_TimeTitle : def_str_VelvetTitle), -1);
    }
    if (line_1 != NULL) {
        w = (MessageWindow *)line_1;
        w->set_text(w, (u8 *)(def_time_on() ? def_str_TimeTitle2 : def_str_VelvetTitle2), -1);
    }
}

/* The end of WSTAG255's setup: the room's own stage, when the map is the room. */
void dw3_velvet_stage_setup(void) {
    if (!vl_on() || !vl_room || gamestate_data.funcs.get_map() != VL_MAP) {
        return;
    }
    vl_igor.dir = vl_scene ? VL_SCENE_IGOR_DIR : VL_IGOR_DIR;
    fieldstg_stage.actors = vl_scene ? vl_dream_actors : def_time_on() ? def_time_room_actors() : vl_guests_now();
    cdload_module.is_loading(records_language + 0x4E); /* queues the names Igor reads the party's forms with */
    fieldstg_stage.music = VL_MUSIC;
    fieldstg_stage.sound = VL_SOUND;
    dw3_velvet_music(&fieldstg_stage.music, &fieldstg_stage.sound); /* the room's own score, when it was generated */
    port_log("velvet: the room of map 0x%X (from map 0x%X), return_pos %d,%d dir %d entry %d", VL_MAP, vl_from,
             fieldstg_stage.return_pos.x >> 8, fieldstg_stage.return_pos.y >> 8, fieldstg_stage.return_dir,
             gamestate_data.funcs.get_map_entry());
}

/* A save state carries the room with the game's RAM it runs in: the map is the room or the bar by vl_room, and the
 * shopkeeper's greeting and the market it opens (vl_shop_phase) go with the screen's own state (def_bm_savestate). */
void dw3_velvet_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, vl_room);
    PORT_STATE_VAR(s, vl_from);
    PORT_STATE_VAR(s, vl_shop_greet);
    PORT_STATE_VAR(s, vl_shop_last);
    PORT_STATE_VAR(s, vl_shop_said);
    PORT_STATE_VAR(s, vl_shop_phase);
    PORT_STATE_VAR(s, vl_mentor_phase);
    PORT_STATE_VAR(s, vl_sepik_phase);
    PORT_STATE_VAR(s, vl_pulled);
    PORT_STATE_VAR(s, vl_igor_phase);
    PORT_STATE_VAR(s, vl_scene);
    PORT_STATE_VAR(s, vl_armed);
}
