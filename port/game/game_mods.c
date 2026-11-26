/* The game's built-in mods (docs/LAUNCHER.md "Built-in mods"; psxstack/game.h game_mods): skip_dialogues,
 * battle_animations, preset_language, widescreen, led_bar and battle_menu (the battle text is no longer a mod: it is
 * always skipped)
 * (and, after them, the handheld's features and the player's pad.map), as PortMod records the
 * runtime's engine (port/runtime/mods.c) reads the settings of, gives hotkeys to and runs every vsync. Each reaches the game's C through
 * the port_mod_* flags and functions of include/port.h, read inside `#ifdef PC_PORT` blocks. The runtime's own mod,
 * fast_forward, is the engine's; skip_dialogues asks for it with port_fast_forward_request. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_scan.h"
#include "definitive.h"
#include "fieldstg.h"
#include "gamestate.h"
#include "led.h"
#include "port_harness.h"
#include "port_runtime.h"
#include "savestate.h"

#define PORT_BATTLE_SCAN_MAX 4096 /* words: the longest script on the disc has 567 */

/* ---- skip_dialogues (docs/LAUNCHER.md "Skip dialogues"): while it is on (the toggle pressed once, or the hold binding held) the game's C hooks
 * (port_mod_skip_dialogues, include/port.h) show every revealing message window's page at once and go on from its
 * confirm waits and the battle's message waits by themselves; choices, menus, name entry and the other overlays'
 * code-driven prompts are separate code and still wait for the player. With `fast_forward_waits` it also asks for
 * fast-forward (fast_forward's speed and mute, its defaults when that mod is off) while FIELDSTG's event VM runs
 * (fieldstg_stage.event_running: the scripted waits, walks and bubble animations); free walking is not sped up.
 *
 * DW3_PORT_SKIP_DIALOGUES=1 (a test hook, tests/port/mods.py; only while the mod is enabled): on from the start, as if
 * the toggle had been pressed. */
enum { SD_TOGGLE, SD_HOLD, SD_FF_WAITS };
static const PortModOption sd_options[] = {
    { .id = "toggle", .type = PORT_MOD_BINDING, .def = "\"F2\"", .applies = "live" },
    { .id = "hold", .type = PORT_MOD_BINDING, .def = "\"\"", .applies = "live" },
    { .id = "fast_forward_waits", .type = PORT_MOD_BOOL, .def = "false", .applies = "live" },
};
int port_mod_skip_dialogues;
static int sd_toggled;

static void sd_start(struct PortMod *mod) {
    const char *test = getenv("DW3_PORT_SKIP_DIALOGUES");
    (void)mod;
    if (test != NULL && strcmp(test, "1") == 0) {
        sd_toggled = 1;
        port_log("skip dialogues: on from the start (DW3_PORT_SKIP_DIALOGUES)");
    }
}

/* The Android touch pad's fast-forward button (android/touch_pad.c): on until tapped again. The touch pad's fingers are
 * not inputs the fast_forward mod's bindings see, so it asks through port_fast_forward_request, which skip_dialogues
 * also sets every vsync: both ask through sd_frame below, and the button asks itself when it is tapped (for a player
 * with skip_dialogues off, whose sd_frame does not run). */
int dw3_touch_fast_forward;

void dw3_touch_fast_forward_set(int on) {
    dw3_touch_fast_forward = on != 0;
    port_log("touch pad: fast-forward %s at frame %ld", on ? "on" : "off", port_frames);
    port_fast_forward_request(dw3_touch_fast_forward);
}

static void sd_frame(struct PortMod *mod) {
    const PortOverlay *field = port_overlay_current(1);
    int on;
    if (port_input_pressed(mod->values[SD_TOGGLE].action)) {
        sd_toggled = !sd_toggled;
    }
    on = sd_toggled || port_input_held(mod->values[SD_HOLD].action);
    if (on != port_mod_skip_dialogues) {
        port_mod_skip_dialogues = on;
        port_log("skip dialogues: %s at frame %ld", on ? "on" : "off", port_frames);
    }
    /* fieldstg_stage is FIELDSTG's: valid only while FIELDSTG is the tier-1 overlay */
    port_fast_forward_request(dw3_touch_fast_forward ||
                              (on && mod->values[SD_FF_WAITS].number != 0 && field != NULL &&
                               strcmp(field->name, "FIELDSTG") == 0 && fieldstg_stage.event_running != 0));
}

/* The window's title while it is on. */
static const char *sd_status(struct PortMod *mod) {
    (void)mod;
    return port_mod_skip_dialogues ? "skip dialogues" : NULL;
}

/* ---- battle_animations (docs/LAUNCHER.md "Disable battle animations"): while enabled, port_mod_battle_animations makes fightstg_script_update ask
 * port_battle_cut at the INIT of every script 5 and up (the attacks', techniques' and items' animations; the rules
 * already ran): one with a child command becomes the target's reaction (results[3] + 1: flinch, heavy hit, KO, dodge)
 * with `hit_reaction` (the default), else ends at once; one without ends at once. A knock-out keeps its KO reaction in
 * both cases (it ends on the KO pose, animation 10). The hit sound the script would have played is played. A script
 * that does not scan to its end (none on the disc: tests/port/battle.py) is left to run. */
enum { BA_HIT_REACTION };
static const PortModOption ba_options[] = {
    { .id = "hit_reaction", .type = PORT_MOD_BOOL, .def = "true", .applies = "live" },
};
int port_mod_battle_animations;
static int ba_hit_reaction;

static void ba_start(struct PortMod *mod) {
    port_mod_battle_animations = 1;
    ba_hit_reaction = mod->values[BA_HIT_REACTION].number != 0;
}

s32 port_battle_cut(const s16 *stream, s32 stage, const s32 *results, s32 hit_sound, s32 *sound_id, s32 *sound_arg) {
    PortBattleScan scan;
    s32 reaction = results[3] + 1;
    *sound_id = *sound_arg = 0;
    port_battle_scan(stream, PORT_BATTLE_SCAN_MAX, stage, &scan);
    if (!scan.ok || reaction < 1 || reaction > 4) {
        return -1;
    }
    if (scan.sound != 0) {
        /* fightstg_script_run_sound's choice for its first hit-sound command */
        *sound_id = (scan.sound == 0x62 ? results[0] : results[3]) == 3 ? 0x38 : hit_sound;
        *sound_arg = scan.sound_arg;
    }
    if (scan.child && (ba_hit_reaction || reaction == 3)) {
        port_log("battle animations: frame %ld: a script of %d words cut%s; the target's reaction %d%s", port_frames,
                 scan.length, scan.multi ? " (multi-hit)" : "", reaction, *sound_id ? ", its hit sound" : "");
        return reaction;
    }
    port_log("battle animations: frame %ld: a script of %d words cut%s; ended%s", port_frames, scan.length,
             scan.child ? "" : " (no reaction in it)", *sound_id ? ", its hit sound" : "");
    return 0;
}

/* The inns' field maps and the 0xC map their event script goes to (each inn stage file of src/wstag:
 * FIELDSTG_EVENT_GOTO_MAP(0xC..)): STGMCARD names the place by the 0xC map's low byte (stgmcard_map_names). */
static const struct { s16 map, inn; } gs_inns[] = {
    { 0x20A, 0xC01 }, { 0x223, 0xC12 }, { 0x230, 0xC02 }, { 0x238, 0xC03 }, { 0x23F, 0xC04 }, { 0x247, 0xC05 },
    { 0x249, 0xC14 }, { 0x25B, 0xC13 }, { 0x25D, 0xC06 }, { 0x263, 0xC15 }, { 0x269, 0xC07 }, { 0x26D, 0xC08 },
    { 0x26F, 0xC09 }, { 0x279, 0xC0A }, { 0x292, 0xC0B }, { 0x29E, 0xC0C }, { 0x2A5, 0xC16 }, { 0x2AC, 0xC0D },
    { 0x2B1, 0xC0E }, { 0x2B3, 0xC18 }, { 0x2C4, 0xC17 }, { 0x2C6, 0xC0F }, { 0x2CB, 0xC19 }, { 0x2D1, 0xC10 },
    { 0x2D6, 0xC11 },
};

/* The inn's save screen (0xC..) for a field map, or 0. The Guardromon network names a place by it
 * (port/game/definitive_travel.c). */
s32 port_inn_save_screen(s32 map) {
    size_t i;
    for (i = 0; i < sizeof(gs_inns) / sizeof(gs_inns[0]); i++) {
        if (gs_inns[i].map == map) {
            return gs_inns[i].inn;
        }
    }
    return 0;
}

/* ---- preset_language (docs/LAUNCHER.md "Preset language"): while enabled, port_mod_preset_language makes CNTY_SEL's root object set
 * records_language to the option's code and go on to the opening (0xE02; 0xE01 for Japanese, as the menu) instead of
 * drawing the language select screen. It acts at boot only (and after a reset, which goes through CNTY_SEL again).
 * The codes are the text sets' (records_language: the offset of every localized file ID): the five the screen offers
 * in its order (cnty_sel_languages), then USA and JPN, whose sets are on the EU disc but which the screen never
 * reaches. */
static const char *const pl_languages[] = {
    "english", "portuguese", "french", "german", "italian", "spanish", "japanese", NULL
};
static const s32 pl_codes[] = { 2, 2, 3, 5, 4, 6, 0 };
enum { PL_LANGUAGE };
static const PortModOption pl_options[] = {
    { .id = "language", .type = PORT_MOD_ENUM, .def = "\"english\"", .values = pl_languages, .applies = "restart" },
};
int port_mod_preset_language;
s32 port_preset_language = 2;

static void pl_start(struct PortMod *mod) {
    int i = (int)mod->values[PL_LANGUAGE].number;
    port_mod_preset_language = 1;
    port_preset_language = pl_codes[i];
    def_language_set(i <= 5 ? i : 0); /* the order of pl_languages is def_lang()'s: en, pt, fr, de, it, es */
    port_log("preset language: %s (records_language %d, def_lang %d), no language select screen", pl_languages[i],
             (int)port_preset_language, i <= 5 ? i : 0);
}

/* ---- The party's shared experience (definitive edition; was the party_xp mod; docs/MECHANICS.md section 6):
 * STFGTREP's report asks port_party_xp_share for every party member's experience. A member that took part keeps one
 * fighter's split share; one that sat out gets DEF_XP_PARTY_SHARE percent of it (at least 1 when both are above 0); a
 * knocked-out one (port_party_xp_knocked_out, from WFIGHTMN) gets none. */
static int px_ko[3]; /* the last battle's party slots at 0 HP */

void port_party_xp_knocked_out(s32 slot, s32 knocked_out) {
    if (slot >= 0 && slot < 3) {
        px_ko[slot] = knocked_out;
    }
}

s32 port_party_xp_share(s32 slot, s32 took_part, s32 exp) {
    s32 gain = exp;

    if (!took_part) {
        if (px_ko[slot]) {
            gain = 0;
        } else {
            gain = exp * DEF_XP_PARTY_SHARE / 100;
            if (gain == 0 && exp > 0 && DEF_XP_PARTY_SHARE > 0) {
                gain = 1;
            }
        }
    }
    port_log("party xp: frame %ld: slot %d %s: %d of the fighters' %d", port_frames, slot,
             took_part ? "took part" : px_ko[slot] ? "knocked out" : "sat out", gain, exp);
    return gain;
}

/* ---- widescreen (docs/LAUNCHER.md "Widescreen"): the battle, and with `field` the field, at the screen's shape up to
 * 16:9 through the hardware renderer's wide canvas (psxstack render_gpu_wide.c), which draws what the game sends past
 * the 320-wide screen's edges.
 *  - The battle: FIGHTSTG already sends the 3D a 16:9 frame needs: its only screen-space cull,
 *    fightstg_model_mesh_is_visible, keeps a mesh with a bounding point within the clip plus 64 pixels (x within 224
 *    of the centre; 16:9 reaches 213.5), and the GPU clips the rest to the 320-wide drawing area (issue #71). Nothing in
 *    the game changes: the mod only tells the renderer which scenes to widen.
 *  - The field (issue #78): its map is streamed tiles, and the tile objects in RAM already cover a 426-wide view, but
 *    it draws only the tiles a 320-wide screen shows. port_field_wide (include/port.h) is the canvas's columns past
 *    each side while FIELDSTG runs; the field's hooks then draw that far, in 3 more tile slots (port_field_slots) in
 *    the VRAM's extra rows (port_video_vram_extend). The game's state changes with it (the camera stops at the
 *    picture's edge), so a run's record differs from the 4:3 one; with the software renderer it is 0 and nothing does.
 * The menus and the other overlays are 320-wide 2D whose art ends at the screen's edge: they stay 4:3 (pillarboxed). */
enum { WS_FIELD };
static const PortModOption ws_options[] = {
    { .id = "field", .type = PORT_MOD_BOOL, .def = "true", .applies = "restart" },
};
s32 port_field_wide;
s32 port_field_slots[3][3];

static void ws_start(struct PortMod *mod) {
    (void)mod;
    port_video_widescreen_enable();
    port_video_vram_extend();
}

static void ws_frame(struct PortMod *mod) {
    const PortOverlay *ovl = port_overlay_current(1);
    int field = mod->values[WS_FIELD].number != 0 && ovl != NULL && strcmp(ovl->name, "FIELDSTG") == 0;
    int pad;

    port_video_widescreen(ovl != NULL && (strcmp(ovl->name, "FIGHTSTG") == 0 || field));
    pad = field ? port_video_widescreen_pad() : 0;
    port_field_wide = pad < PORT_FIELD_WIDE_MAX ? pad : PORT_FIELD_WIDE_MAX;
}

/* ---- led_bar (docs/LAUNCHER.md "LED bar"): the acting member's health on the LED bar of the RK3326 handhelds, through
 * the package's daemon (port/game/led.c, .ci-cd/portmaster/handheld/ledd.py). Off the handheld it is a no-op that logs one line, and it
 * is off by default: with it on, the bar's regulator is raised in a battle and dropped when the battle ends.
 *
 * Nothing here reads the game's data: the daemon owns the animation and the game only says which of its six readings
 * the acting member is in, once per change (measured: ~2 ms a change, so a handful of times a fight and never per
 * frame). */
static void led_bar_start(struct PortMod *mod) {
    (void)mod;
    led_start();
}

static void led_bar_frame(struct PortMod *mod) {
    (void)mod;
    led_frame();
}

static const char *led_bar_status(struct PortMod *mod) {
    (void)mod;
    return led_status();
}

/* ---- battle_menu (docs/LAUNCHER.md "Battle menu"): while enabled, port_mod_battle_menu makes FIGHTSTG's
 * fightstg_command_menu_create build a menu with one pad button per action (fightstg_bmenu_*) instead of the list and
 * its cursor (always on in the port; a replay script keeps the list). Each option names a place on the player's pad; port_battle_menu_bit (below, after pad.map's code) turns it
 * into the PS1 bit the player's own button delivers, so the menu follows the pad map and shows that button's glyph.
 * Two actions on one place cannot both work: the defaults are used then. */
/* The eight places a battle menu action (and a pad.map letter) can sit on, one table for everything that is known about
 * a place: X(name, pad.map's alias, the PS1 bit the runtime delivers there, the index of the place carrying this name in
 * the other vocabulary, the letter the Xbox layout prints at the place pad.map calls by this name when there is no
 * pad.map).
 *
 * Two vocabularies meet here. The launcher names a place by the physical button; pad.map names it in the game's terms,
 * because the launcher translates (launcher/cfg.c, game_name) when it writes the file: physical south is the game's
 * west, west is east, east is south, north is north. The game index is that translation: `game`, an index of this same
 * table. The order is the options' values and bm_place's indexes. */
#define BM_PLACES(X)                                \
    X("south", NULL, 14, 0, 'A')                    \
    X("west", NULL, 15, 1, 'X')                     \
    X("north", NULL, 12, 2, 'Y')                    \
    X("east", NULL, 13, 3, 'B')                     \
    X("l1", "leftshoulder", 10, 4, 0)               \
    X("r1", "rightshoulder", 11, 5, 0)              \
    X("l2", "lefttrigger", 8, 6, 0)                 \
    X("r2", "righttrigger", 9, 7, 0)
#define BM_PLACE_COUNT 8
#define BM_NAME(name, alias, bit, game, xbox) name,
static const char *const bm_places[] = { BM_PLACES(BM_NAME) NULL };
#define BM_SLOT(name, alias, bit, game, xbox) { name, alias, bit, game, xbox },
static const struct {
    const char *name;
    const char *alias; /* pad.map's own name for it, or NULL */
    u8 bit;
    u8 game;           /* the slot whose pad.map name this launcher place carries (the launcher's translation) */
    char xbox;         /* by pad.map's name */
} bm_slot[BM_PLACE_COUNT] = { BM_PLACES(BM_SLOT) };
/* The options' order is the original list's (and the menu's result's): Fight, Tech, Digivolve, Tag, Item, Run. */
enum { BM_FIGHT, BM_TECH, BM_DV, BM_TAG, BM_ITEM, BM_RUN, BM_COUNT, BM_REMEMBER = BM_COUNT, BM_BUFFER, BM_INSPECT_OPT,
       BM_BLAST_OPT, BM_OPTIONS };
/* Fight on the bottom face button, Tag on the right (the owner, 2026-11-23: the launcher's "Fight button" row gives the
 * old Fight right, Tag bottom back; replay scripts never see these, they keep the original list menu) */
static const u8 bm_default_places[BM_COUNT] = { 0, 5, 2, 3, 4, 1 }; /* south r1 north east l1 west */
#define BM_OPTION(name, default_place) \
    { .id = name, .type = PORT_MOD_ENUM, .def = default_place, .values = bm_places, .applies = "live" }
static const char *const bm_buffers[] = { "off", "short", "long", NULL };
static const int bm_buffer_ms[] = { 0, 1000, 1500 }; /* off, short, long (the owner played with 1.0 s: now the default) */
static const PortModOption bm_options[BM_OPTIONS] = {
    BM_OPTION("fight", "\"south\""),  BM_OPTION("tech", "\"r1\""),   BM_OPTION("digivolve", "\"north\""),
    BM_OPTION("tag", "\"east\""),    BM_OPTION("item", "\"l1\""),   BM_OPTION("run", "\"west\""),
    { .id = "remember_cursor", .type = PORT_MOD_BOOL, .def = "true", .applies = "live" },
    { .id = "buffer_input", .type = PORT_MOD_ENUM, .def = "\"short\"", .values = bm_buffers, .applies = "live" },
    BM_OPTION("inspect", "\"l2\""),
    BM_OPTION("blast", "\"r2\""),
};
int port_mod_battle_menu;
int port_mod_bm_remember;
int port_mod_bm_buffer; /* frames; 0: off */
/* The eight actions' places: the six options, then Inspect and Blast (definitive combat), each with its option. */
#define BM_ACTIONS (BM_COUNT + 2)
#define BM_INSPECT BM_COUNT       /* the actions' indexes (fightstg's FIGHTSTG_BMENU_INSPECT, _BLAST) */
#define BM_BLAST (BM_COUNT + 1)
static u8 bm_place[BM_ACTIONS] = { 0, 5, 2, 3, 4, 1, 6, 7 }; /* by action: index into bm_places (bm_default_places) */

/* An extra action (Inspect, Blast) whose option clashes with an action before it takes the first free place: L2, R2,
 * then the shoulders and the face buttons. */
static void bm_pick_free(int action) {
    static const u8 order[BM_PLACE_COUNT] = { 6, 7, 4, 5, 0, 1, 2, 3 };
    int i, a, taken;

    for (i = 0; i < BM_PLACE_COUNT; i++) {
        for (taken = 0, a = 0; a < action; a++) {
            taken |= bm_place[a] == order[i];
        }
        if (!taken) {
            bm_place[action] = order[i];
            return;
        }
    }
}
static int bm_started; /* the mod's options were applied: bm_default_hud must not overwrite them */

/* Definitive combat: the battle HUD is not a mod, it is always on. The mod record stays for its options
 * (which buttons, remember_cursor, buffer_input): when it is not enabled the defaults hold. The one exception is a
 * replay script (--script, tests/replay): the scripts are shared with the emulator and choose from the original
 * list menu, so they keep it (as they keep the console's face-button rotation). */
static void bm_default_hud(long rate) {
    if (bm_started || port_script_active) {
        return;
    }
    port_mod_battle_menu = 1;
    port_mod_bm_remember = 1;
    port_mod_bm_buffer = (int)(bm_buffer_ms[1] * rate / 1000); /* "short", the option's default */
}

/* At each battle's start (def_log_begin), not at boot: game_apply_rate runs before the runtime knows whether a script is
 * driving the run. */
void port_hud_arm(void) {
    bm_default_hud(port_rate);
}

static void bm_start(struct PortMod *mod) {
    int a, b, clash = 0;
    bm_started = 1;
    for (a = 0; a < BM_COUNT; a++) {
        bm_place[a] = (u8)mod->values[a].number;
    }
    for (a = 0; a < BM_COUNT; a++) {
        for (b = a + 1; b < BM_COUNT; b++) {
            clash |= bm_place[a] == bm_place[b];
        }
    }
    if (clash) {
        port_log("battle menu: two actions on one button, the default buttons are used");
        for (a = 0; a < BM_COUNT; a++) {
            bm_place[a] = bm_default_places[a];
        }
    }
    /* Inspect and Blast have their own options; each only falls back to the first free place when it collides with
     * an action before it (an old settings file has no value, so the defaults l2 and r2 apply, and the launcher refuses
     * to save duplicates). */
    bm_place[BM_INSPECT] = (u8)mod->values[BM_INSPECT_OPT].number;
    bm_place[BM_BLAST] = (u8)mod->values[BM_BLAST_OPT].number;
    for (b = BM_INSPECT; b <= BM_BLAST; b++) {
        for (a = 0; a < b; a++) {
            if (bm_place[a] == bm_place[b]) {
                bm_pick_free(b);
                break;
            }
        }
    }
    port_mod_battle_menu = 1;
    port_mod_bm_remember = mod->values[BM_REMEMBER].number != 0;
    /* the window in frames at the game's own rate (50 or 60 a second) */
    port_mod_bm_buffer = (int)(bm_buffer_ms[(int)mod->values[BM_BUFFER].number] * port_rate / 1000);
}

/* ---- battle_menu's two helpers for the menus (docs/LAUNCHER.md "Battle menu"):
 *  - remember_cursor: the technique and item menus open on what the Digimon used last (an ID by party member and kind:
 *    port_bm_recall/port_bm_remember), and the command menu keeps its cursor between turns;
 *  - buffer_input: the presses made in the last `port_mod_bm_buffer` frames before the command menu opens (a press of
 *    one of the six actions' buttons, and the confirm bit) are given to it as it opens, oldest first, one each time a
 *    menu asks: the command menu for the action, a technique or item menu for the confirm. At most two are kept.
 *    A press is only kept while no menu is open and no battle message is up (fightstg_8008D3B4.c decides: a press
 *    that dismisses a message is not a command), and everything is dropped when a press is refused or the turn's
 *    command is chosen. Nothing is invented: a press the menu would have taken is taken, a refused one leaves the menu
 *    open. A menu whose input the buffer already decided is not drawn: port_bm_peek reads the press without taking it,
 *    and the command menu and a technique or item menu a confirm settled give their result the frame they open. */
#define BMB_MAX 2
static struct {
    long t;      /* the frame it was pressed */
    s32 action;  /* the action (0..5) whose button it was, -1 none */
    u32 confirm; /* it was the confirm bit (0x2000) */
    s32 armed;   /* the command menu opened within the window of it: it may be given */
} bmb[BMB_MAX];
static s32 bmb_n;
static s32 bm_memory[2][64]; /* [technique, item][party member id & 63] */

void port_bm_record(s32 action, u32 confirm) {
    if (port_mod_bm_buffer == 0) {
        return;
    }
    if (bmb_n == BMB_MAX) {
        bmb[0] = bmb[1];
        bmb_n--;
    }
    bmb[bmb_n].t = port_frames;
    bmb[bmb_n].action = action;
    bmb[bmb_n].confirm = confirm;
    bmb[bmb_n].armed = 0;
    bmb_n++;
    port_log("battle menu: frame %ld: press kept (action %d)", port_frames, action);
}

/* A battle message is up this frame: a press made now or the frame before may be what dismissed it. */
void port_bm_message(void) {
    while (bmb_n > 0 && bmb[bmb_n - 1].t >= port_frames - 1) {
        bmb_n--;
    }
}

void port_bm_clear(void) {
    bmb_n = 0;
}

/* The command menu opens: what was pressed within the window may be given, the rest is gone. */
void port_bm_open(void) {
    s32 i, n = 0;
    for (i = 0; i < bmb_n; i++) {
        /* one already armed was left by the last menu (a cancelled digivolve's second press): it is gone */
        if (!bmb[i].armed && port_frames - bmb[i].t <= port_mod_bm_buffer) {
            bmb[n] = bmb[i];
            bmb[n++].armed = 1;
        }
    }
    bmb_n = n;
}

/* The oldest given press's action (-1: none, or one that was not an action's button). */
s32 port_bm_take_action(void) {
    s32 action, i;
    if (bmb_n == 0 || !bmb[0].armed) {
        return -1;
    }
    action = bmb[0].action;
    for (i = 1; i < bmb_n; i++) {
        bmb[i - 1] = bmb[i];
    }
    bmb_n--;
    if (action >= 0) {
        port_log("battle menu: frame %ld: buffered press, action %d", port_frames, action);
    }
    return action;
}

/* The oldest given press without taking it (1: there is one; action -1 for a confirm-only one). */
int port_bm_peek(s32 *action, u32 *confirm) {
    if (bmb_n == 0 || !bmb[0].armed) {
        return 0;
    }
    if (action != NULL) {
        *action = bmb[0].action;
    }
    if (confirm != NULL) {
        *confirm = bmb[0].confirm;
    }
    return 1;
}

/* The oldest given press as a confirm for a technique or item menu: 0x2000 or 0. */
u32 port_bm_take_confirm(void) {
    u32 confirm;
    s32 i;
    if (bmb_n == 0 || !bmb[0].armed) {
        return 0;
    }
    confirm = bmb[0].confirm;
    for (i = 1; i < bmb_n; i++) {
        bmb[i - 1] = bmb[i];
    }
    bmb_n--;
    if (confirm) {
        port_log("battle menu: frame %ld: buffered confirm", port_frames);
    }
    return confirm;
}

s32 port_bm_recall(s32 kind, s32 member) {
    return port_mod_bm_remember ? bm_memory[kind][member & 63] : 0;
}

/* A technique or item menu opened on the remembered entry (a line tests/port/mods.py reads). */
void port_bm_log(s32 kind, s32 id, s32 position) {
    port_log("battle menu: frame %ld: the %s menu opens on %d (entry %d)", port_frames, kind ? "item" : "technique", id,
             position);
}

void port_bm_log_last(s32 last) {
    port_log("battle menu: frame %ld: command menu created with last %d", port_frames, last);
}

void port_bm_remember(s32 kind, s32 member, s32 value) {
    bm_memory[kind][member & 63] = value;
}

/* ---- battle text: always skipped (the owner, 2026-11-23). It was the `battle_text` mod, with off / skip / async;
 * from now on the battle's message box (FightstgMessage) never waits and never shows: every action goes on, and the
 * player reads the result rather than the box. The mode stays a code variable (BT_SKIP: 1) for the FIGHTSTG side and
 * for a future change. The toast's windows are arena objects held here (FIGHTSTG runs and draws them), so they are
 * part of the save state. */
#define BT_SKIP 1
int port_mod_battle_text = BT_SKIP;
void *port_battle_text_windows[2];
s32 port_battle_text_frames;

static PortMod game_mod_table[] = {
    { .id = "skip_dialogues", .version = "0.1", .options = sd_options,
      .option_count = (int)(sizeof(sd_options) / sizeof(sd_options[0])), .start = sd_start, .frame = sd_frame,
      .status = sd_status },
    { .id = "battle_animations", .version = "0.1", .options = ba_options,
      .option_count = (int)(sizeof(ba_options) / sizeof(ba_options[0])), .start = ba_start },
    { .id = "preset_language", .version = "0.1", .options = pl_options,
      .option_count = (int)(sizeof(pl_options) / sizeof(pl_options[0])), .start = pl_start },
    { .id = "widescreen", .version = "0.2", .options = ws_options,
      .option_count = (int)(sizeof(ws_options) / sizeof(ws_options[0])), .start = ws_start, .frame = ws_frame },
    { .id = "led_bar", .version = "0.1", .start = led_bar_start, .frame = led_bar_frame,
      .status = led_bar_status },
    { .id = "battle_menu", .version = "0.1", .options = bm_options, .option_count = BM_OPTIONS, .start = bm_start },
};

int game_mod_count(void) {
    return (int)(sizeof(game_mod_table) / sizeof(game_mod_table[0]));
}

PortMod *game_mods(void) {
    return game_mod_table;
}

/* A save state (psxstack/game.h game_savestate): what the mods carry from one vsync to the next about the game (the
 * map a save-anywhere was opened from, the last battle's knocked-out party slots, the boost's tenths). Their options
 * and toggles are settings: the loading run's own. */
void game_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, px_ko);
    PORT_STATE_VAR(s, port_battle_text_windows);
    PORT_STATE_VAR(s, port_battle_text_frames);
    PORT_STATE_VAR(s, bmb);
    PORT_STATE_VAR(s, bmb_n);
    PORT_STATE_VAR(s, bm_memory);
    PORT_STATE_VAR(s, port_field_slots); /* widescreen's field tile slots and the width in force: the next vsync reads them */
    PORT_STATE_VAR(s, port_field_wide);
    def_savestate(s); /* the definitive combat's battle state (port/game/definitive.c) */
}

/* ---- handheld features (R36S). Each is an if. A miss is logged once. ---- */

int dw3_feat_icons = 1;
int dw3_feat_travel = 1;
int dw3_feat_objective = 1;
int dw3_feat_velvet = 1;

extern u8 ststatus_map_areas[];

static int feat_on(const char *name, int def) {
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0') {
        return def;
    }
    return v[0] != '0';
}

void dw3_feats_arm(void) {
    static int armed;
    if (armed) {
        return;
    }
    armed = 1;
    dw3_feat_icons = feat_on("DW3_FEAT_ICONS", dw3_feat_icons);
    dw3_feat_travel = feat_on("DW3_FEAT_TRAVEL", dw3_feat_travel);
    dw3_feat_objective = feat_on("DW3_FEAT_OBJECTIVE", dw3_feat_objective);
    dw3_feat_velvet = feat_on("DW3_FEAT_VELVET", dw3_feat_velvet);
    port_log("dw3 feats: icons %d travel %d objective %d velvet %d", dw3_feat_icons, dw3_feat_travel, dw3_feat_objective,
             dw3_feat_velvet);
}

/* ---- pad.map (the launcher, launcher/cfg.c, writes it): one line per PS1 button, "<button> <gamepad input> <letter>".
 *
 * The letter replaces the button's symbol in the game's text (dw3_button_glyph). The middle column is the player's pad,
 * which dw3_pad_permute applies inside pad_read_buttons: the handheld's SDL2 runtime (psxstack/runtime/input.c, the
 * PSXSTACK_SDL2 branch) has a fixed button map and ignores input.gamepad, so the game is the one place the player's
 * choice can land - and it has to be there anyway, because most of the game reads the pad's bits directly and never
 * goes through pad_state.button_map.
 *
 * The middle column is in the runtime's own terms (the button the player presses), and the permutation lands after
 * records_language's rotation (src/main/pad.c), so a row of the file means the action the game's text names: circle
 * confirms, cross goes back. That is the Japanese convention the text is drawn in, which is why the pad is rotated for
 * the other languages at all - and why the row cannot be read straight off the runtime's bits. */

/* The PS1 button each pad.map name means. */
static const struct {
    const char *name;
    int bit;
} dw3_pad_names[] = {
    { "select", 0 }, { "start", 3 },  { "up", 4 },  { "right", 5 }, { "down", 6 },       { "left", 7 },
    { "l2", 8 },     { "r2", 9 },     { "l1", 10 }, { "r1", 11 },   { "triangle", 12 }, { "circle", 13 },
    { "cross", 14 }, { "square", 15 },
};

/* The PS1 button the handheld's runtime wires each gamepad input to (input.c, the SDL2 branch: SDL2's own button
 * positions): the places of bm_slot, and these, which no battle menu action can sit on. A name that is in neither -
 * guide, the sticks, the touchpad - has no button of its own there. */
static const struct {
    const char *place;
    int bit;
} dw3_pad_places[] = {
    { "back", 0 }, { "start", 3 }, { "dpup", 4 }, { "dpright", 5 }, { "dpdown", 6 }, { "dpleft", 7 },
};

/* records_language's rotation of the face buttons (src/main/pad.c): cross 14 -> circle 13, triangle 12 -> cross 14,
 * circle 13 -> triangle 12, square 15 untouched. The permutation is applied to what that rotation produced, so a place's
 * bit has to go through it first. */
static const u8 dw3_pad_rot[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 12, 13, 15 };

#define DW3_PAD_NAMES ((int)(sizeof(dw3_pad_names) / sizeof(dw3_pad_names[0])))
#define DW3_PAD_PLACES ((int)(sizeof(dw3_pad_places) / sizeof(dw3_pad_places[0])))

static struct {
    int loaded;
    int has_file;    /* pad.map was read: without it the places carry the Xbox letters (bm_slot.xbox) */
    int remap;       /* DW3_PAD_REMAP: this runtime has no pad map, the game applies the file's */
    char letter[16]; /* by PS1 button: the letter the text shows, or 0 for the button's own symbol */
    u16 perm[16];    /* by PS1 button: the bit the runtime delivers when it is pressed */
    u8 at[BM_PLACE_COUNT]; /* by place (pad.map's name for it): the PS1 button whose row sits there */
} dw3_pad;

static int dw3_pad_bit_of_name(const char *name) {
    int i;

    for (i = 0; i < DW3_PAD_NAMES; i++) {
        if (strcmp(name, dw3_pad_names[i].name) == 0) {
            return dw3_pad_names[i].bit;
        }
    }
    return -1;
}

/* The index in bm_slot of a place named by pad.map (or the launcher): its name or its alias. -1: not a place there. */
static int dw3_place_index(const char *place) {
    int i;

    for (i = 0; i < BM_PLACE_COUNT; i++) {
        if (strcmp(place, bm_slot[i].name) == 0 || (bm_slot[i].alias != NULL && strcmp(place, bm_slot[i].alias) == 0)) {
            return i;
        }
    }
    return -1;
}

static int dw3_pad_bit_of_place(const char *place) {
    int i = dw3_place_index(place);

    if (i >= 0) {
        return bm_slot[i].bit;
    }
    for (i = 0; i < DW3_PAD_PLACES; i++) {
        if (strcmp(place, dw3_pad_places[i].place) == 0) {
            return dw3_pad_places[i].bit;
        }
    }
    return -1;
}

/* Reads pad.map once. Without the file: the identity, and the Xbox letters at the handheld's own positions. */
static void dw3_pad_load(void) {
    FILE *f;
    char name[16], place[16], letter;
    int bit, at, idx, i;

    if (dw3_pad.loaded) {
        return;
    }
    dw3_pad.loaded = 1;
    dw3_pad.remap = feat_on("DW3_PAD_REMAP", 0);
    for (i = 0; i < 16; i++) {
        dw3_pad.perm[i] = (u16)i;
        dw3_pad.letter[i] = 0;
    }
    for (i = 0; i < BM_PLACE_COUNT; i++) {
        dw3_pad.at[i] = bm_slot[i].bit; /* until a row says otherwise: the button the runtime delivers there */
    }
    f = fopen("pad.map", "r");
    if (f == NULL) {
        return;
    }
    dw3_pad.has_file = 1;
    {
        char line[128];
        while (fgets(line, sizeof(line), f) != NULL) {
            if (line[0] == '#') {
                continue;
            }
            if (sscanf(line, "%15s %15s %c", name, place, &letter) != 3) {
                continue;
            }
            bit = dw3_pad_bit_of_name(name);
            if (letter >= 'a' && letter <= 'z') {
                letter = (char)(letter - 'a' + 'A');
            }
            if (bit >= 0 && bit < 16) {
                dw3_pad.letter[bit] = (letter >= 'A' && letter <= 'Z') ? letter : 0; /* '-': the symbol itself */
            }
            at = dw3_pad_bit_of_place(place);
            if (at >= 0 && at < 16 && bit >= 0 && bit < 16) {
                dw3_pad.perm[bit] = (u16)at;
            }
            idx = dw3_place_index(place);
            if (idx >= 0 && bit >= 0 && bit < 16) {
                dw3_pad.at[idx] = (u8)bit;
            }
        }
    }
    fclose(f);
    port_log("button icons: cross %c circle %c square %c triangle %c (pad.map)",
             dw3_pad.letter[14] != 0 ? dw3_pad.letter[14] : '-', dw3_pad.letter[13] != 0 ? dw3_pad.letter[13] : '-',
             dw3_pad.letter[15] != 0 ? dw3_pad.letter[15] : '-', dw3_pad.letter[12] != 0 ? dw3_pad.letter[12] : '-');
    if (dw3_pad.remap) {
        port_log("pad map: remap on, cross on %d circle on %d square on %d triangle on %d (the runtime's bits)",
                 (int)dw3_pad.perm[14], (int)dw3_pad.perm[13], (int)dw3_pad.perm[15], (int)dw3_pad.perm[12]);
    } else {
        /* One line, because the handheld's dw3.sh is what sets DW3_PAD_REMAP and losing it there would make the pad
         * silently stop being remapped; on the SDL3 builds the runtime maps the pad and this is the expected line. */
        port_log("pad map: read, not applied (DW3_PAD_REMAP is not set: the runtime maps the pad itself)");
    }
}

/* The pad's bits with the player's buttons in them (pad.map's middle column), or `buttons` untouched when
 * DW3_PAD_REMAP is not set. `rotated`: records_language's face-button rotation was applied to these bits already, so
 * the place each row names has to be looked for where the rotation put it. */
u16 dw3_pad_permute(u16 buttons, int rotated) {
    u16 out = 0;
    int bit, from;

    dw3_pad_load();
    if (!dw3_pad.remap) {
        return buttons;
    }
    for (bit = 0; bit < 16; bit++) {
        from = dw3_pad.perm[bit];
        if (rotated) {
            from = dw3_pad_rot[from];
        }
        out |= (u16)(((buttons >> from) & 1) << bit);
    }
    return out;
}

/* The game's extra glyph (`01 xx`) that draws each face button's symbol. */
static const struct {
    u8 code;
    int bit;
} dw3_face_codes[] = {
    { 0x1B, 14 }, /* cross */
    { 0x2F, 13 }, /* circle */
    { 0x32, 15 }, /* square */
    { 0x34, 12 }, /* triangle */
};
#define DW3_FACE_CODES ((int)(sizeof(dw3_face_codes) / sizeof(dw3_face_codes[0])))

/* The extra glyph of the face button `bit`'s own symbol, or 0 for a button without one (a shoulder, a trigger). */
u8 port_face_symbol_code(int bit) {
    int i;

    for (i = 0; i < DW3_FACE_CODES; i++) {
        if (dw3_face_codes[i].bit == bit) {
            return dw3_face_codes[i].code;
        }
    }
    return 0;
}

/* The extra glyph to write in a text for the face button `bit` so that it comes out as that button's own symbol (or its
 * pad.map letter): message.c swaps circle and triangle in the game's own text (the rotation of the face buttons for the
 * other languages), so a text the port writes itself has to swap them first, as the battle menu does. 0: no symbol. */
u8 port_face_text_code(int bit) {
    u8 code = port_face_symbol_code(bit);

    if (!port_script_active) {
        if (code == 0x2F) {
            code = 0x34;
        } else if (code == 0x34) {
            code = 0x2F;
        }
    }
    return code;
}

/* The glyph code of the letter pad.map gives a face symbol (the extra glyph `code`), or 0 to keep the symbol. */
s16 dw3_button_glyph(u8 code) {
    char letter = 0;
    int i;

    if (!dw3_feat_icons) {
        return 0;
    }
    dw3_pad_load();
    for (i = 0; i < DW3_FACE_CODES; i++) {
        if (dw3_face_codes[i].code == code) {
            letter = dw3_pad.letter[dw3_face_codes[i].bit];
        }
    }
    return (letter >= 'A' && letter <= 'Z') ? (s16)(0x0E + (letter - 'A')) : 0;
}

/* Where a PS1 bit sits on the player's pad: the bit the runtime delivers for it (pad.map's middle column; the identity
 * when there is no pad.map). */
int port_battle_menu_place(int bit) {
    dw3_pad_load();
    return (bit >= 0 && bit < 16 && dw3_pad.has_file) ? (int)dw3_pad.perm[bit] : bit;
}

/* The PS1 bit whose button is at the action's place. A face place can hold any bit once the player has remapped, so
 * search; the identity (the first match is the bit itself) when nothing is remapped. */
int port_battle_menu_bit(int action) {
    int want, bit;

    if (action < 0 || action >= BM_ACTIONS) {
        return -1;
    }
    want = bm_slot[bm_place[action]].bit;
    for (bit = 0; bit < 16; bit++) {
        if (port_battle_menu_place(bit) == want) {
            return bit;
        }
    }
    return want;
}

/* Pressed this frame, as the runtime delivers it before pad.map's permutation (src/main/pad.c). The menu listens to
 * this and, beside it, to the logical bit pad_state reports (fightstg_battle_menu_update): the place is the physical
 * button whatever the map and the language's rotation did to the bits, and the logical bit is what the tests' scripted
 * pad and a runtime that maps the pad itself deliver. Neither alone covers both. */
u16 dw3_raw_pressed = 0;

int port_battle_menu_pressed(int action) {
    int bit;

    if (action < 0 || action >= BM_ACTIONS) {
        return 0;
    }
    dw3_pad_load();
    /* With DW3_PAD_REMAP=1 (PortMaster) dw3_raw_pressed is pre-permute (physical); on SDL3 (PC/Android) input.c
     * already applied the pad map before dw3_raw_pressed, so look up the logical bit holding that physical place. */
    bit = dw3_pad.remap ? bm_slot[bm_place[action]].bit : port_battle_menu_bit(action);
    return (bit >= 0 && bit < 16) ? ((dw3_raw_pressed >> bit) & 1) : 0;
}

/* The action's place in pad.map's names (the launcher's physical south is the game's west...: bm_slot.game). The
 * letters and symbols are indexed by the file's names, so a lookup that skipped this showed the glyph of another button
 * (only north, the one place the two agree on, was right). */
static int bm_place_in_file(int action) {
    return bm_slot[bm_place[action]].game;
}

/* Where the action is drawn on the battle menu: its place on the pad (bm_places' index: 0 south, 1 west, 2 north,
 * 3 east, 4 l1, 5 r1, 6 l2, 7 r2), so a remapped action appears at its button's spot (the owner, 2026-11-23). Display
 * only: the input never reads it. Unique once bm_start has resolved the clashes. */
int port_battle_menu_screen_place(int action) {
    return action >= 0 && action < BM_ACTIONS ? bm_place[action] : -1;
}

/* The PS1 button at the action's physical place: the symbol the menu shows when there is no letter. */
int port_battle_menu_symbol_bit(int action) {
    if (action < 0 || action >= BM_ACTIONS) {
        return -1;
    }
    return bm_slot[bm_place[action]].bit;
}

/* The letter on the button at the action's place: its row's own, or the Xbox one when there is no pad.map; 0 is the
 * button's symbol. */
char port_battle_menu_letter(int action) {
    char letter;

    if (action < 0 || action >= BM_ACTIONS || !dw3_feat_icons) { /* icons off: the symbols, as in the game's text */
        return 0;
    }
    dw3_pad_load();
    if (!dw3_pad.has_file) {
        return bm_slot[bm_place_in_file(action)].xbox;
    }
    letter = dw3_pad.letter[dw3_pad.at[bm_place_in_file(action)]];
    return (letter >= 'A' && letter <= 'Z') ? letter : 0;
}

/* One line a press on the menu: what tests/port/mods.py reads (the action the button chose, or the refusal of one the
 * Digimon cannot do, and the logical pad bit that triggered it). A press is a human's pace, never per frame. */
void port_battle_menu_chosen(int action, int bit, int refused) {
    port_log("battle menu: frame %ld: %s %d (bit %d)", port_frames, refused ? "refused" : "action", action, bit);
}

/* Feerdus's progress -> field table. Steps outside 0x200-0x2D6 have no map spot. */
static const u16 dw3_objective_modes[] = {
    0x2D8, 0x207, 0x204, 0x200, 0x22E, 0x22C, 0x232, 0x23A, 0x227, 0x239, 0x240, 0x204, 0x20B, 0x204, 0x236,
    0x260, 0x25C, 0x252, 0x254, 0x255, 0x255, 0x21C, 0x218, 0x203, 0x260, 0x240, 0x246, 0x29C, 0x2AD, 0x2AE,
    0x26E, 0x26D, 0x26D, 0x218, 0x2C7, 0x2D5, 0x272, 0x276, 0x219, 0x288, 0x2DD, 0x2DF, 0x2DF, 0x2D7, 0x2D7,
};

s32 dw3_objective_spot(s32 region) {
    static int logged;
    s32 progress;
    s32 mode;
    s32 area;
    if (!dw3_feat_objective) {
        return 0;
    }
    progress = gamestate_data.progress;
    if (progress < 0 || progress >= (s32)(sizeof dw3_objective_modes / sizeof dw3_objective_modes[0])) {
        if (!logged) {
            logged = 1;
            port_log("objective: progress %d has no map", (int)progress);
        }
        return 0;
    }
    mode = dw3_objective_modes[progress];
    { /* the Guide's row when it names a field: one story step can hold a chase of its own (the Blue Card's Guilmon,
       * all of step 6), which the Guide follows flag by flag and this table cannot */
        const DefQuest *q = def_guide_active() ? def_quest_main() : NULL;

        static s32 told = -1;

        if (q != NULL && q->map >= 0x200 && q->map <= 0x2D6) {
            mode = q->map;
        }
        if (mode != told) { /* once per change: the map screen asks every frame */
            told = mode;
            port_log("objective: progress %d points at map 0x%X%s", (int)progress, (unsigned)mode,
                     q != NULL && mode == q->map ? " (the Guide's row)" : "");
        }
    }
    if (mode < 0x200 || mode > 0x2D6 || region != (mode >= 0x270)) {
        return 0;
    }
    area = (ststatus_map_areas[mode & 0xFF] & 0x7F) + 1;
    if (area < 1 || area >= 47) {
        if (!logged) {
            logged = 1;
            port_log("objective: mode 0x%X has no spot", (unsigned)mode);
        }
        return 0;
    }
    return area;
}
