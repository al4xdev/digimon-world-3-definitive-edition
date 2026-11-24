#include "common.h"
#ifdef PC_PORT
#include "port/game/definitive.h" /* the definitive combat's constants (DEF_BATON_PASS_MP) */
#endif
/* blocked[8..10] (Strength, Defense, Speed down): the definitive edition lets them through (def_stat_blocked: a boss is
 * immune to status, not to debuffs); the PS1 build reads the flag as the original does. */
#ifdef PC_PORT
#define FIGHTSTG_STAT_BLOCKED(k) def_stat_blocked(k)
#define FIGHTSTG_DEBUFF(k, amount) def_debuff_scale((k), (amount))
#else
#define FIGHTSTG_STAT_BLOCKED(k) records_state.blocked[k]
#define FIGHTSTG_DEBUFF(k, amount) (amount)
#define def_status_blocked(k) records_state.blocked[k]
#endif

#include "object.h"
#include "heap.h"
#include "gfx.h"
#include "sound.h"
#include "gamestate.h"
#include "cdload.h"
#include "message.h"
#include "records.h"
#include "pad.h"
#include "fightstg.h"
#include "psyq/gtemac.h"

/* The pad the technique and item menus read: the PS1's own, and on the host also the confirm press that battle_menu's
 * buffer_input gives (port_bm_take_confirm; 0 when there is none). A macro because LOOP_BLOCK's arguments cannot hold #ifdef. */
#ifdef PC_PORT
#define FIGHTSTG_MENU_PAD() (pad_state.get_pressed(0) | port_bm_take_confirm())
#else
#define FIGHTSTG_MENU_PAD() pad_state.get_pressed(0)
#endif
#ifdef PC_PORT
/* battle_menu's buffer_input: the command object is in its menu step (a menu is open, so a press is the menu's, not the
 * buffer's); set by fightstg_command_update every frame, cleared with the toast at a battle's start. */
s32 fightstg_bm_in_menu;
#ifdef PC_PORT
/* The vsync (port_frames) of the battle menu's last RUN tick: while it is fresh the menu is up, and the HUD leaves the
 * original Blast bar out (the menu's Blast tab carries the gauge then; the owner, 2026-11-23). A stamp, not a flag, so
 * the bar is back the frame after the menu goes, however it goes. */
extern long port_frames;
long fightstg_bmenu_seen = -100;
#endif
#endif

/* .bss, in address order. GCC 2.8 emits uninitialized definitions in the order of their first declaration
 * (fightstg_camera_setting's is in fightstg.h), so they are defined here, before the code's own externs. */
FightstgCameraSetting fightstg_camera_setting;
s32 fightstg_unused_2; /* unreferenced */
RECT fightstg_portrait_rect;
DR_MOVE fightstg_cursor_moves[4];
u32 fightstg_cursor_ot[2];
FightstgNewEvent fightstg_new_event;

/* Per event type: 0 = keep it queued and return 0, -1 = keep it queued, else take it off. */
extern s32 fightstg_events_take_modes[];
extern struct FightstgCursorParams fightstg_command_menu_cursor;
/* Event types, indexed by fightstg_events_start_modifier's arg2. */
extern s32 fightstg_modifier_event_types[];

/* File 0x1CC's header (fightstg_models_get's records: FightstgModelRecordA/_B). */
typedef struct FightstgModelFile {
    /* 0x0 */ s32 base;
    /* 0x4 */ s32 entries; /* offset of the entries: {s16 ID, u8 record index, u8 kind}, 0-terminated */
    /* 0x8 */ s32 records_a; /* offset of the type A records */
    /* 0xC */ s32 records_b; /* offset of the type B records */
} FightstgModelFile;

/* File 0x1CC's entries (at `entries`), 0-terminated. */
typedef struct FightstgModelEntry {
    /* 0x0 */ s16 id;    /* ID */
    /* 0x2 */ u8 index;  /* record index */
    /* 0x3 */ u8 kind;   /* kind: below 0x3A a type A record, else type B */
} FightstgModelEntry; /* size 0x4 */

/* Objects (object_new / object_create), named after the function that creates them. */
/* The party's action (fightstg_item_update). */
typedef struct FightstgItem {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 args[3]; /* the message's arguments (fightstg_message_show's args) */
    /* 0x5C */ u8 unk_5C[0x14];
    /* 0x70 */ s32 item; /* action */
    /* 0x74 */ s32 field_element;
    /* 0x78 */ s32 amount; /* amount */
    /* 0x7C */ s32 lowered;
} FightstgItem; /* size 0x80 */

/* A technique or item used by side `side` (0 = the party, 0x10 = the enemy). */
typedef struct FightstgCounter {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 args[3]; /* the message's arguments (fightstg_message_show's args) */
    /* 0x5C */ u8 unk_5C[0x14];
    /* 0x70 */ u8 side;
    /* 0x71 */ u8 pad_71[0x3];
    /* 0x74 */ s32 damage_taken;
    /* 0x78 */ s32 tech; /* record of records_techniques (from 1) */
    /* 0x7C */ s32 hit; /* it hits */
    /* 0x80 */ s32 damage;
    /* 0x84 */ s32 no_knockout;
    /* 0x88 */ u8 unk_88[0x4];
} FightstgCounter; /* size 0x8C */

/* A technique used by side `side` (fightstg_tech_update). */
typedef struct FightstgTech {
    /* 0x00 */ Object base;
    /* 0x50 */ u8 side; /* side */
    /* 0x51 */ u8 pad_51[0x3];
    /* 0x54 */ s32 tech; /* technique: record of records_techniques (from 1) */
    /* 0x58 */ s32 damage;
    /* 0x5C */ s32 healed; /* amount healed */
    /* 0x60 */ s32 target_asleep; /* the target counters */
    /* 0x64 */ s32 args[3]; /* the message's arguments (fightstg_message_show's args) */
    /* 0x70 */ u8 unk_70[0x14];
} FightstgTech; /* size 0x84 */

/* An enemy attack (counter = 0) or counter (1): numbers and effects through a child object. */
typedef struct FightstgBossTurn {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 args[3]; /* the message's arguments (fightstg_message_show's args) */
    /* 0x5C */ u8 unk_5C[0x4];
    /* 0x60 */ s32 restore_idle; /* the boss's weakened phase ended: reset its idle animation after the attack */
    /* 0x64 */ u8 unk_64[0xC];
    /* 0x70 */ s32 tech; /* the technique used (record of records_techniques, from 1) */
    /* 0x74 */ s32 counter; /* fightstg_boss_turn_create's arg0: 0 the boss's turn (state 1), 1 its counter (state 2) */
    /* 0x78 */ s32 no_knockout; /* the counter doesn't knock out (fightstg_boss_turn_create's arg1) */
    /* 0x7C */ s32 target_asleep; /* the party's member is asleep: it may wake (fightstg_rules.roll_wake) */
} FightstgBossTurn; /* size 0x80 */

/* Applies the status effects of an action (fightstg_action.effects), one per step. */
typedef struct FightstgResults {
    /* 0x00 */ Object base;
    /* 0x50 */ u8 side; /* side acting: 0 or 0x10 */
    /* 0x51 */ u8 pad_51[0x3];
    /* 0x54 */ s32 args[3]; /* the message's arguments (fightstg_message_show's args) */
    /* 0x60 */ u8 unk_60[0x14];
} FightstgResults; /* size 0x74 */

/* A view of a model on layer 0x1009 (fightstg_portrait_update's window). */
typedef struct FightstgModelView {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 model_id; /* model ID (fightstg_models.get) */
    /* 0x54 */ FightstgModelParams *params;
    /* 0x58 */ GsRVIEW2 view;
    /* 0x78 */ s32 projection; /* the layer's projection */
    /* 0x7C */ GsCOORDINATE2 coord;
    /* 0xCC */ SVECTOR rot; /* rotation of coord */
    /* 0xD4 */ VECTOR trans;  /* translation of coord */
    /* 0xE4 */ s32 frames;     /* frames left to update */
} FightstgModelView; /* size 0xE8 */

/* A fade: level goes 0 -> 0xFF by step per frame (state 1), then back to 0 (state 2). */
typedef struct FightstgFlash {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 step; /* step: 0xFF / frames */
    /* 0x54 */ s32 level; /* level, 0..0xFF */
} FightstgFlash; /* size 0x58 */

typedef struct FightstgCommandMenu {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 sel;
    /* 0x54 */ s32 *result; /* the caller's slot: set to -1 */
} FightstgCommandMenu; /* size 0x58 */

/* What fightstg_cursor_create copies into its object (e.g. fightstg_command_menu_cursor). */
typedef struct FightstgCursorParams {
    /* 0x00 */ s32 count; /* entries */
    /* 0x04 */ s32 vram_x; /* VRAM x the highlight is copied from */
    /* 0x08 */ s32 vram_y; /* VRAM y of entry 0 */
    /* 0x0C */ s32 vram_step; /* VRAM y step */
    /* 0x10 */ s32 sprite; /* the entries' sprite (-1: none) */
    /* 0x14 */ s32 x;
    /* 0x18 */ s32 y; /* y of entry 0 */
    /* 0x1C */ s32 y_step;
} FightstgCursorParams; /* size 0x20 */

/* The object fightstg_cursor_create creates (a cursor/selection). */
typedef struct FightstgCursor {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 sel; /* selected entry */
    /* 0x54 */ s32 locked;
    /* 0x58 */ FightstgCursorParams params;
    /* 0x78 */ s32 drawn_sel; /* sel when the highlight was last drawn */
    /* 0x7C */ s32 highlight; /* highlight frame, 0..11 */
    /* 0x80 */ s32 times[10]; /* per entry: animation time */
} FightstgCursor; /* size 0xA8 */

/* An item menu: the items of kind 2 (records_funcs.get_item), seven per page. */
typedef struct FightstgItemMenu {
    /* 0x000 */ Object base;
    /* 0x050 */ s32 *result; /* the caller's slot: set to -1, then to the item chosen (-2: cancelled) */
    /* 0x054 */ s32 shown;   /* entry shown */
    /* 0x058 */ s16 items[0x194];  /* items (records_funcs.list_items), 0-terminated */
    /* 0x380 */ s16 *list;    /* the items listed */
    /* 0x384 */ s32 count;    /* their number */
    /* 0x388 */ s32 page;     /* page */
    /* 0x38C */ s32 pages;    /* pages */
} FightstgItemMenu; /* size 0x390 */

/* A technique menu: six per page (fightstg_tech_menu_draw, 80096048, 80096314 show it). */
typedef struct FightstgTechMenu {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 *result; /* the caller's slot: set to -1 */
    /* 0x54 */ s32 shown;  /* the cursor's entry, as last shown */
    /* 0x58 */ s16 forms[3];  /* the acting member's chosen forms (gamestate_data.funcs.get_chosen_forms) */
    /* 0x5E */ u8 pad_5E[0x2];
    /* 0x60 */ GamestateForm form_records[3]; /* the forms' records (gamestate_data.funcs.get_form) */
    /* 0x9C */ s32 techniques[12]; /* technique IDs, 0x8000/0x4000 flags */
    /* 0xCC */ s32 count;  /* their number */
    /* 0xD0 */ s32 page;   /* page */
    /* 0xD4 */ s32 pages;  /* pages */
} FightstgTechMenu; /* size 0xD8 */

/* fightstg_command_menu_update's data block (object_new's third argument). */
typedef struct FightstgCommandMenuData {
    /* 0x00 */ FightstgCursor *cursor; /* fightstg_cursor_create(&fightstg_command_menu_cursor) */
    /* 0x04 */ MessageWindow *windows[6];
} FightstgCommandMenuData; /* size 0x1C */

/* A Digimon choice for a party member (its own and its other forms), with a status screen
 * (fightstg_status_create) of the one under the cursor. */
typedef struct FightstgDigivolveMenu {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 *result; /* the caller's slot: its value is taken, then it is set to the choice (-2: cancelled) */
    /* 0x54 */ s32 member; /* party member */
    /* 0x58 */ s32 status_page; /* status page */
    /* 0x5C */ s32 cursor; /* entry under the cursor */
    /* 0x60 */ s16 digimon[4]; /* the Digimon listed */
    /* 0x68 */ s32 count;  /* their number */
    /* 0x6C */ s16 forms[3];  /* gamestate_data.funcs.get_chosen_forms */
    /* 0x72 */ u8 pad_72[0x2];
} FightstgDigivolveMenu; /* size 0x74 */

/* A party member's status screen (stats, techniques). */
typedef struct FightstgStatus {
    /* 0x00 */ Object base;
    /* 0x50 */ u8 unk_50[0x4];
    /* 0x54 */ s32 member; /* party member (gamestate_data.funcs) */
    /* 0x58 */ s32 page;   /* page: 0 = stats, 1-2 */
    /* 0x5C */ s32 form;   /* entry of forms, from 1; 0 = none */
    /* 0x60 */ s32 form_level;
    /* 0x64 */ GamestateStats stats;  /* stats (gamestate_data.funcs.get_stats) */
    /* 0x90 */ s16 forms[3];  /* chosen forms (gamestate_data.funcs.get_chosen_forms) */
    /* 0x96 */ u8 pad_96[0x2];
    /* 0x98 */ GamestateForm form_records[3]; /* their records (gamestate_data.funcs.get_form) */
    /* 0xD4 */ s32 techniques[6]; /* technique IDs, 0x8000/0x4000 flags */
} FightstgStatus; /* size 0xEC */

/* fightstg_status_update's data block: its message windows. */
typedef struct FightstgStatusData {
    /* 0x00 */ u8 unk_00[0x4];
    /* 0x04 */ MessageWindow *l1_window;
    /* 0x08 */ MessageWindow *r1_window;
    /* 0x0C */ MessageWindow *skill_label;
    /* 0x10 */ MessageWindow *skill_level;
    /* 0x14 */ MessageWindow *stat_windows[13]; /* the stats (fightstg_status_stat_windows) */
    /* 0x48 */ MessageWindow *tech_windows[6]; /* the techniques */
#ifdef PC_PORT
    MessageWindow *elem_icons[6]; /* definitive: each technique's element (NULL when !def_on()) */
#endif
} FightstgStatusData; /* size 0x60 */

/* fightstg_member_menu_create's object: picks a party member (not the acting one). */
typedef struct FightstgMemberMenu {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 *result; /* the caller's slot: set to -1, then the member picked (-2: cancelled) */
    /* 0x54 */ s32 *cursor; /* the cursor, kept by the caller */
    /* 0x58 */ s32 check_items; /* items: also check the members' items (fightstg_member_menu_get_team_item) */
    /* 0x5C */ s32 members[2]; /* the members shown */
    /* 0x64 */ s32 count;   /* how many */
    /* 0x68 */ s32 cursor_started;
    /* 0x6C */ s32 cursor_time; /* time of the last cursor frame */
    /* 0x70 */ s32 cursor_frame; /* blinking cursor frame, 0..4 */
    /* 0x74 */ s32 team_items[2]; /* per member shown: fightstg_member_menu_get_team_item */
    /* 0x7C */ s16 forms[3];  /* the acting member's chosen forms (gamestate_data.funcs.get_chosen_forms) */
    /* 0x82 */ u8 unk_82[0x6];
} FightstgMemberMenu; /* size 0x88 */

/* Its data block (base.children): message windows. */
typedef struct FightstgMemberMenuData {
    /* 0x00 */ FightstgCursor *cursor;
    /* 0x04 */ MessageWindow *hp_labels[2];
    /* 0x0C */ MessageWindow *hp_values[2]; /* value of hp_labels */
    /* 0x14 */ MessageWindow *hp_slashes[2];
    /* 0x1C */ MessageWindow *max_hp_values[2]; /* value of hp_slashes */
    /* 0x24 */ MessageWindow *mp_labels[2];
    /* 0x2C */ MessageWindow *mp_values[2]; /* value of mp_labels */
    /* 0x34 */ MessageWindow *mp_slashes[2];
    /* 0x3C */ MessageWindow *max_mp_values[2]; /* value of mp_slashes */
    /* 0x44 */ MessageWindow *names[2];  /* name */
    /* 0x4C */ MessageWindow *none_window; /* shown when no one can be picked */
} FightstgMemberMenuData; /* size 0x50 */

/* fightstg_switch_menu_create's object: picks a partner for member `member` (and with fightstg_switch_menu_create_team an
 * action with a cost). */
typedef struct FightstgSwitchMenu {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 *result; /* the caller's slot: its value is taken, then it is set to -1; result */
    /* 0x54 */ s32 page;    /* page, 0..2 */
    /* 0x58 */ s32 page_shown; /* page as last shown */
    /* 0x5C */ s32 member;  /* member */
    /* 0x60 */ s32 member_id; /* gamestate_data.funcs.get_party_member(member) */
    /* 0x64 */ s32 picked;  /* entry picked */
    /* 0x68 */ s16 entries[4]; /* the entries: Digimon IDs */
    /* 0x70 */ s32 count;   /* entries */
    /* 0x74 */ s16 forms[3];  /* the member's chosen forms (gamestate_data.funcs.get_chosen_forms) */
    /* 0x7A */ u8 pad_7A[0x2];
    /* 0x7C */ s32 action;  /* the action (record of records_techniques, from 1), 0: none */
    /* 0x80 */ s32 *action_result; /* the caller's second slot: action */
} FightstgSwitchMenu; /* size 0x84 */

/* Its data block (base.children). */
typedef struct FightstgSwitchMenuData {
    /* 0x00 */ FightstgCursor *cursor; /* the list's cursor */
    /* 0x04 */ FightstgCursor *cursor_2; /* the second step's cursor */
    /* 0x08 */ FightstgStatus *status; /* fightstg_status_create's objects (two, swapped) */
    /* 0x0C */ FightstgStatus *status_2;
    /* 0x10 */ MessageWindow *names[4];  /* the entries' names */
    /* 0x20 */ MessageWindow *action_windows[5];
    /* 0x34 */ MessageWindow *mp_windows[4];
#ifdef PC_PORT
    MessageWindow *baton[3]; /* Baton Pass's row (definitive combat): its name, "MP" and its cost */
#endif
} FightstgSwitchMenuData; /* size 0x44 */

/* fightstg_message_create's object: the battle message box (two windows, shown one after the other). */
typedef struct FightstgMessage {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 queue[9]; /* queued messages: a message ID and its arguments, 0 ends */
    /* 0x74 */ s32 ready; /* set by fightstg_message_show: a message is ready */
    /* 0x78 */ s32 window; /* window being shown */
    /* 0x7C */ s32 window_count; /* windows used */
    /* 0x80 */ s32 window_frames; /* frames per window */
    /* 0x84 */ s32 window_time; /* time the window was shown */
    /* 0x88 */ s32 queue_index; /* index into queue */
    /* 0x8C */ s32 port_class; /* PC_PORT only (unused on the PS1): battle_text's class of the message being shown */
    /* 0x90 */ s32 arrow_frame; /* "next" arrow animation frame, 0..4 */
    /* 0x94 */ s32 arrow_time; /* its last time */
    /* 0x98 */ s32 waiting; /* waiting for the button: the arrow is shown */
    /* 0x9C */ s32 members[3]; /* members listed (fightstg_message_list_members) */
    /* 0xA8 */ s32 member_count; /* their count */
    /* 0xAC */ void (*show)(); /* fightstg_message_show */
    /* 0xB0 */ void (*close)(); /* fightstg_message_close */
} FightstgMessage; /* size 0xB4 */

/* Its data block (base.children). */
typedef struct FightstgMessageData {
    /* 0x0 */ MessageWindow *windows[2]; /* the two windows */
} FightstgMessageData; /* size 0x8 */

typedef struct FightstgConfusedMenu {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 sel;
    /* 0x54 */ s32 selected; /* selected entry */
    /* 0x58 */ s32 *result; /* the caller's slot: set to -1 */
    /* 0x5C */ Object *portrait;
    /* 0x60 */ Object *idle_camera;
    /* 0x64 */ s32 queue[9]; /* 0-terminated, shown one by one (show) */
    /* 0x88 */ s32 ready;
    /* 0x8C */ s32 window; /* window being shown */
    /* 0x90 */ s32 windows; /* windows */
    /* 0x94 */ s32 frames_per_window; /* frames per window */
    /* 0x98 */ s32 window_time; /* time the window was shown */
    /* 0x9C */ s32 entry; /* entry of queue */
    /* 0xA0 */ u8 unk_A0[0x4];
    /* 0xA4 */ s32 cursor_frame; /* blinking cursor frame, 0..4 */
    /* 0xA8 */ s32 cursor_time; /* time of the last cursor frame */
    /* 0xAC */ s32 waiting; /* waiting for a button */
    /* 0xB0 */ u8 unk_B0[0x10];
    /* 0xC0 */ void (*show)(struct FightstgConfusedMenu *, s32, s32);
} FightstgConfusedMenu; /* size 0xC4 */

extern s32 fightstg_rules_opposite_elements[];

/* Entries of fightstg_jump_params (6 of them), indexed by fightstg_jump_create's arg1 - 1. */
typedef struct FightstgJumpParams {
    /* 0x0 */ s32 height;
    /* 0x4 */ s32 speed;
} FightstgJumpParams; /* size 0x8 */

extern FightstgJumpParams fightstg_jump_params[];

typedef struct FightstgJump {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 kind; /* kind, 1..6 */
    /* 0x54 */ s32 distance;
    /* 0x58 */ s32 z_offset;
    /* 0x5C */ FightstgModelParams *params; /* the model moved (its pos) */
    /* 0x60 */ s32 height; /* fightstg_jump_params[kind - 1].height */
    /* 0x64 */ s32 start_y; /* params->pos.y, at the start */
    /* 0x68 */ s32 speed; /* fightstg_jump_params[kind - 1].speed */
    /* 0x6C */ s32 time;
} FightstgJump; /* size 0x70 */

typedef struct FightstgSound {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 sound;
    /* 0x54 */ s32 key; /* voice (sound_play) */
    /* 0x58 */ s32 timer; /* timer, counted down by fightstg_battle.frames */
} FightstgSound; /* size 0x5C */

void fightstg_item_update();
void fightstg_counter_update();
void fightstg_tech_update();
void fightstg_scripted_turn_update();
void fightstg_boss_turn_update();
void fightstg_results_update();
void fightstg_model_view_update();
void fightstg_camera_update();
void fightstg_camera_set(FightstgCamera *obj, FightstgCameraSetting *params);
void fightstg_camera_move();
struct FightstgCameraSetting *fightstg_camera_get_preset();
struct FightstgCameraSetting *fightstg_camera_get_default();
void fightstg_command_update();
void fightstg_flash_update();
void fightstg_flash_draw(FightstgFlash *obj);
void fightstg_hud_update();
void fightstg_command_menu_update();
void fightstg_portrait_update();
void fightstg_status_update();
void fightstg_digivolve_menu_update();
void fightstg_member_menu_update();
void fightstg_switch_menu_update();
void fightstg_message_show();
void fightstg_message_close();
void fightstg_confused_menu_update();
void fightstg_jump_update();
void fightstg_message_draw(struct FightstgMessage *obj);
void fightstg_message_step(struct FightstgMessage *obj, struct FightstgMessageData *data);
void fightstg_events_add(FightstgNewEvent *event);
void fightstg_events_add_first(FightstgNewEvent *event);
s32 fightstg_events_get_delay(u8 side, s32 kind);
s32 fightstg_events_find_from(s32 i);
s32 fightstg_events_find_member(s32 type, u8 side, s32 member);
struct FightstgCursor *fightstg_cursor_create(struct FightstgCursorParams *params);
void fightstg_events_start_modifier(u8 side, s32 member, s32 kind, s32 tech);
void fightstg_command_menu_open(struct FightstgCommandMenu *obj);
void fightstg_battle_update_speed(void);
void fightstg_item_menu_update();
void fightstg_tech_menu_update();
void fightstg_cursor_update();
void fightstg_battle_draw_quad(s32 layer_id, s32 depth, DVECTOR *pos, CVECTOR *colors, s32 semi);
void *fightstg_models_get(s32 id);

/* An action's script parameters (fightstg_item_run_script's default case): -1 ends the table. */
typedef struct FightstgItemScript {
    /* 0x0 */ s16 action; /* action */
    /* 0x2 */ s16 effect;
    /* 0x4 */ s16 hit_sound;
} FightstgItemScript; /* size 0x6 */

extern FightstgItemScript fightstg_item_scripts[];

/* One step of the party's action `item` (fightstg_item_update): starts the battle script for it
 * (fightstg_script_create) and applies its effect. Returns 1 when done. */
s32 fightstg_item_run_script(FightstgItem *obj, FightstgScript **data) {
    FightstgMember *member;
    FightstgStats *p;
    s32 a;
    s32 b;
    s32 lim;
    s32 i;
    FightstgMember *target;
    RecordsTechnique *rec;

    switch (obj->base.substep) {
    case 0:
    default:
        *data = fightstg_script_create();
        (*data)->side = 0;
        switch (obj->item) {
        case 0x4D:
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
            (*data)->script = 0xF;
            (*data)->stage = (obj->item - 0x4D) * 3 + 0x22;
            (*data)->effect = 2;
            (*data)->hit_sound = 0x39;
            break;
        case 0x54:
            obj->field_element = pad_random.next() % 7 + 2;
            (*data)->script = 0xF;
            (*data)->stage = (obj->field_element - 2) * 3 + 0x22;
            (*data)->effect = 2;
            (*data)->hit_sound = 0x39;
            break;
        case 0x56:
            (*data)->script = 0x11;
            (*data)->stage = -1;
            (*data)->effect = 0x15;
            (*data)->hit_sound = 0x1B;
            break;
        case 0x57:
            (*data)->script = 0x11;
            (*data)->stage = -1;
            (*data)->effect = 0x27;
            (*data)->hit_sound = 0x31;
            break;
        case 0x59:
            (*data)->script = 0x11;
            (*data)->stage = -1;
            (*data)->effect = 0x29;
            (*data)->hit_sound = 0x31;
            break;
        case 0x58:
            member = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
            p = fightstg_rules.get_stats(0x10, 0, fightstg_battle.state.current[1]);
            if (p->stats[0] > p->stats[1]) {
                if (FIGHTSTG_STAT_BLOCKED(8) == 0) {
                    obj->lowered = 1;
#ifdef PC_PORT
                    if (!def_mod_apply(0x10, fightstg_battle.state.current[1], 0, -2, DEF_MOD_TURNS_BASE, 8)) {
#endif
                        a = p->stats[0];
                        b = p->stats[1];
                        if (member->modifiers[0] != 0) {
                            p->stats[0] -= member->modifiers[0];
                        }
                        lim = -(p->stats[0] / 2);
                        member->modifiers[0] -= a - b;
                        if (member->modifiers[0] < lim) {
                            member->modifiers[0] = lim;
                        }
#ifdef PC_PORT
                    }
#endif
                }
            } else if (p->stats[0] < p->stats[1] && FIGHTSTG_STAT_BLOCKED(9) == 0) {
                obj->lowered = 2;
#ifdef PC_PORT
                if (!def_mod_apply(0x10, fightstg_battle.state.current[1], 1, -2, DEF_MOD_TURNS_BASE, 9)) {
#endif
                    a = p->stats[0];
                    b = p->stats[1];
                    if (member->modifiers[1] != 0) {
                        p->stats[1] -= member->modifiers[1];
                    }
                    lim = -(p->stats[1] / 2);
                    member->modifiers[1] -= b - a;
                    if (member->modifiers[1] < lim) {
                        member->modifiers[1] = lim;
                    }
#ifdef PC_PORT
                }
#endif
            }
            (*data)->script = 0x11;
            (*data)->stage = -1;
            if (obj->lowered == 1) {
                (*data)->effect = 0x25;
                (*data)->hit_sound = 0x31;
            } else if (obj->lowered == 2) {
                (*data)->effect = 0x27;
                (*data)->hit_sound = 0x31;
            } else {
                (*data)->effect = 0x2E;
                (*data)->hit_sound = 0x1E;
            }
            break;
        case 0x55:
            (*data)->script = 0xE;
            (*data)->stage = -1;
            (*data)->effect = 0x1C;
            (*data)->hit_sound = 0x27;
            if (records_state.blocked[5] == 0) {
                FightstgMember *enemy = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];

                obj->amount = enemy->max_hp;
                obj->amount /= 5;
                if (enemy->hp - obj->amount <= 0) {
                    (*data)->results[3] = 2;
                    obj->base.timer = 1;
                } else {
                    (*data)->results[3] = 1;
                    wfightmn_update_idle_anim(0x10, obj->amount);
                    wfightmn_update_idle_anim(0, -obj->amount);
                }
            } else {
                (*data)->results[3] = 3;
            }
            break;
        case 0x5A:
            (*data)->script = 0xE;
            rec = &records_techniques[0x88];
            (*data)->stage = rec->anim_stage;
            (*data)->effect = rec->anim_effect;
            (*data)->hit_sound = rec->hit_sound;
            obj->amount = wfightmn_cap_damage(0, fightstg_rules.get_special_damage(0, 0x89), 0);
            target = fightstg_battle.state.members[1];
            if (obj->amount > 0) {
                if (target[fightstg_battle.state.current[1]].hp - obj->amount <= 0) {
                    (*data)->results[3] = 2;
                } else {
                    (*data)->results[3] = 1;
                    wfightmn_update_idle_anim(0x10, obj->amount);
                }
            } else {
                (*data)->results[3] = 3;
            }
            break;
        default:
            (*data)->script = 0xA;
            (*data)->stage = -1;
            for (i = 0; fightstg_item_scripts[i].action != -1; i++) {
                if (fightstg_item_scripts[i].action == obj->item) {
                    (*data)->effect = fightstg_item_scripts[i].effect;
                    (*data)->hit_sound = fightstg_item_scripts[i].hit_sound;
                    break;
                }
            }
            if (obj->item >= 0x2B && obj->item < 0x2F) {
                wfightmn_update_idle_anim(0, -((RecordsUsable *)records_funcs.get_item(obj->item)->data)->amount);
            } else if (obj->item == 0x47) {
                wfightmn_update_idle_anim(0, -((fightstg_battle.state.members[0] + fightstg_battle.state.current[0])->max_hp >> 1));
            }
            break;
        }
        obj->base.substep++;
        if (fightstg_battle.state.type == 6) {
            if (obj->item == 0x5A) {
                wfightmn_count_boss_hits(0, obj->amount);
            } else {
                wfightmn_check_final_phase_end(0);
            }
        }
        return 0;
    case 1:
        if (*data == NULL) {
            if (obj->item == 0x55 && obj->base.timer == 0 && records_state.blocked[5] == 0) {
                *data = fightstg_script_create();
                (*data)->side = 0;
                (*data)->script = 0xA;
                (*data)->effect = 0x21;
                (*data)->hit_sound = 0x1F;
                obj->base.substep++;
                break;
            }
            return 1;
        }
        break;
    case 2:
        if (*data == NULL) {
            return 1;
        }
        break;
    }
    return 0;
}

/* What a status-cure item (0x42-0x45) clears. */
typedef struct FightstgItemCure {
    /* 0x0 */ s16 message; /* message */
    /* 0x2 */ s16 status; /* FightstgMember.status flag */
    /* 0x4 */ s32 cure_id;
} FightstgItemCure; /* size 0x8 */

extern FightstgItemCure fightstg_item_cure_poison;
extern FightstgItemCure fightstg_item_cure_paralysis;
extern FightstgItemCure fightstg_item_cure_confusion;
extern FightstgItemCure fightstg_item_cure_all;

void fightstg_events_set_field_end(s32 delay);
void *fightstg_boss_turn_create(s32 arg0, s32 arg1);

/* The party uses item `item` in battle: its script (fightstg_item_run_script), then its effect and message, then
 * one fewer in the bag (gamestate_data.items). */
void fightstg_item_update(FightstgItem *obj, FightstgMessage **data) {
    u8 unused[0x90]; /* never referenced, but the frame has room for it */

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        switch (obj->base.step) {
        case 0:
        default:
            *data = fightstg_message_create();
            obj->args[0] = 0;
            obj->args[1] = obj->item;
            (*data)->show(*data, 0xE, obj->args);
            obj->base.step++;
            break;
        case 1:
            if (*data == NULL && fightstg_item_run_script(obj, (FightstgScript **)data) != 0) {
                obj->base.next_state(obj);
            }
            break;
        }
        break;
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 0:
        default:
            if (*data == NULL) {
                obj->base.step++;
            }
            break;
        case 1:
#ifdef PC_PORT
            /* after the Blast quest the DV Plug (0x4C) is a revive item: the game's own revive path below */
            switch (obj->item == 0x4C && def_blast_chosen_mode() ? 0x46 : obj->item) {
#else
            switch (obj->item) {
#endif
            case 0x2B:
            default: {
                FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                RecordsUsable *item = records_funcs.get_item(obj->item)->data;
                s32 n;

                if (member->max_hp == member->hp) {
                    *data = fightstg_message_create();
                    obj->args[0] = 0x2F;
                    (*data)->show(*data, 1, obj->args);
                    obj->base.next_step(obj);
                } else {
                    if (member->max_hp >= member->hp + item->amount) {
                        n = item->amount;
                        member->hp += n;
                    } else {
                        n = member->max_hp - member->hp;
                        member->hp = member->max_hp;
                    }
                    *data = fightstg_message_create();
                    obj->args[0] = 0;
                    obj->args[1] = n;
                    (*data)->show(*data, 8, obj->args);
                    obj->base.next_step(obj);
                }
                break;
            }
            case 0x42:
#ifndef PC_PORT
            case 0x43:
#endif
            case 0x44:
            case 0x45: {
                FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                FightstgItemCure *cure;

                switch (obj->item) {
                case 0x42:
                default:
                    cure = &fightstg_item_cure_poison;
                    break;
                case 0x43:
                    cure = &fightstg_item_cure_paralysis;
                    break;
                case 0x44:
                    cure = &fightstg_item_cure_confusion;
                    break;
                case 0x45:
                    cure = &fightstg_item_cure_all;
                    break;
                }
                *data = fightstg_message_create();
                if (member->status & cure->status) {
                    member->status &= ~cure->status;
                    fightstg_events.cure_status(0, fightstg_battle.state.current[0], cure->cure_id);
                    obj->args[0] = cure->message;
                    obj->args[1] = 0;
                    (*data)->show(*data, 2, obj->args);
                } else {
                    obj->args[0] = 0x2F;
                    (*data)->show(*data, 1, obj->args);
                }
                obj->base.next_step(obj);
                break;
            }
#ifdef PC_PORT
            case 0x2E:
            case 0x43:
#endif
            case 0x46:
                switch (obj->base.substep) {
                case 0:
                    *data = fightstg_message_create();
                    obj->args[0] = 0;
                    obj->args[1] = 5;
                    (*data)->show(*data, 9, obj->args);
                    obj->base.next_substep(obj);
                    break;
                case 1:
                    if (*data == NULL) {
                        s32 i;

                        for (i = 0; i < 3; i++) {
                            if (fightstg_battle.state.members[0][i].hp == 0) {
#ifdef PC_PORT
                                s32 revived = obj->item == 0x4C
                                                  ? def_blast_plug_revive(i)
                                                  : def_revive_hp(obj->item, fightstg_battle.state.members[0][i].max_hp);

                                if (revived > 0) {
                                    fightstg_battle.state.members[0][i].hp = revived;
                                    wfightmn_check_regen(i);
                                }
#else
                                fightstg_battle.state.members[0][i].hp = fightstg_battle.state.members[0][i].max_hp;
                                wfightmn_check_regen(i);
#endif
                            }
                        }
                        obj->base.next_substep(obj);
                    }
                    break;
                case 2:
                    if (*data == NULL) {
                        gamestate_data.items[obj->item]--;
                        obj->base.state = OBJECT_STATE_END;
                    }
                    break;
                }
                break;
            case 0x47: {
                FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];

                *data = fightstg_message_create();
                if (member->hp != member->max_hp || member->mp != member->max_mp) {
                    s32 a = member->max_hp / 2;
                    s32 b = member->max_mp / 2;

                    if (member->hp + a >= member->max_hp) {
                        member->hp = member->max_hp;
                    } else {
                        member->hp += a;
                    }
                    if (member->max_mp <= member->mp + b) {
                        member->mp = member->max_mp;
                    } else {
                        member->mp += b;
                    }
                    obj->args[0] = 0x3B;
                    obj->args[1] = 0;
                    (*data)->show(*data, 2, obj->args);
                } else {
                    obj->args[0] = 0x2F;
                    (*data)->show(*data, 1, obj->args);
                }
                obj->base.next_step(obj);
                break;
            }
            case 0x48: {
#ifdef PC_PORT
                if (!def_mod_apply(0, fightstg_battle.state.current[0], 2, 1, DEF_MOD_TURNS_BASE, 0)) {
#endif
                    RecordsUsable *item = records_funcs.get_item(obj->item)->data;
                    FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                    FightstgStats *p = fightstg_rules.get_stats(0, 1, fightstg_battle.state.current[0]);
                    s32 lim;

                    if (member->modifiers[2] != 0) {
                        p->stats[4] -= member->modifiers[2];
                    }
                    lim = p->stats[4];
                    member->modifiers[2] += lim * item->amount / 128;
                    if (member->modifiers[2] > lim) {
                        member->modifiers[2] = lim;
                    }
                    fightstg_events_start_modifier(0, fightstg_battle.state.current[0], 2, 0);
#ifdef PC_PORT
                }
#endif
                *data = fightstg_message_create();
                obj->args[0] = 0x32;
                obj->args[1] = 0;
                obj->args[2] = fightstg_battle.state.current[0];
                (*data)->show(*data, 7, obj->args);
                obj->base.next_step(obj);
                break;
            }
            case 0x49: {
#ifdef PC_PORT
                if (!def_mod_apply(0, fightstg_battle.state.current[0], 0, 1, DEF_MOD_TURNS_BASE, 0) ||
                    !def_mod_apply(0, fightstg_battle.state.current[0], 1, -1, DEF_MOD_TURNS_BASE, 0)) {
#endif
                    RecordsUsable *item = records_funcs.get_item(obj->item)->data;
                    FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                    FightstgStats *p = fightstg_rules.get_stats(0, 1, fightstg_battle.state.current[0]);
                    s32 max;
                    s32 min;

                    if (member->modifiers[0] != 0) {
                        p->stats[0] -= member->modifiers[0];
                    }
                    if (member->modifiers[1] != 0) {
                        p->stats[1] -= member->modifiers[1];
                    }
                    max = p->stats[0];
                    member->modifiers[0] += max * item->amount / 128;
                    if (member->modifiers[0] > max) {
                        member->modifiers[0] = max;
                    }
                    min = -(p->stats[1] / 2);
                    member->modifiers[1] -= p->stats[1] * item->amount / 512;
                    if (member->modifiers[1] < min) {
                        member->modifiers[1] = min;
                    }
                    fightstg_events_start_modifier(0, fightstg_battle.state.current[0], 0, 0);
                    fightstg_events_start_modifier(0, fightstg_battle.state.current[0], 1, 0);
#ifdef PC_PORT
                }
#endif
                *data = fightstg_message_create();
                obj->args[0] = 0x3C;
                obj->args[1] = 0;
                (*data)->show(*data, 2, obj->args);
                obj->base.next_step(obj);
                break;
            }
            case 0x4A: {
#ifdef PC_PORT
                if (!def_mod_apply(0, fightstg_battle.state.current[0], 1, 1, DEF_MOD_TURNS_BASE, 0) ||
                    !def_mod_apply(0, fightstg_battle.state.current[0], 0, -1, DEF_MOD_TURNS_BASE, 0)) {
#endif
                    RecordsUsable *item = records_funcs.get_item(obj->item)->data;
                    FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                    FightstgStats *p = fightstg_rules.get_stats(0, 1, fightstg_battle.state.current[0]);
                    s32 max;
                    s32 min;

                    if (member->modifiers[0] != 0) {
                        p->stats[0] -= member->modifiers[0];
                    }
                    if (member->modifiers[1] != 0) {
                        p->stats[1] -= member->modifiers[1];
                    }
                    max = p->stats[1];
                    member->modifiers[1] += max * item->amount / 128;
                    if (member->modifiers[1] > max) {
                        member->modifiers[1] = max;
                    }
                    min = -(p->stats[0] / 2);
                    member->modifiers[0] -= p->stats[0] * item->amount / 512;
                    if (member->modifiers[0] < min) {
                        member->modifiers[0] = min;
                    }
                    fightstg_events_start_modifier(0, fightstg_battle.state.current[0], 0, 0);
                    fightstg_events_start_modifier(0, fightstg_battle.state.current[0], 1, 0);
#ifdef PC_PORT
                }
#endif
                *data = fightstg_message_create();
                obj->args[0] = 0x3D;
                obj->args[1] = 0;
                (*data)->show(*data, 2, obj->args);
                obj->base.next_step(obj);
                break;
            }
            case 0x4B:
                (fightstg_battle.state.members[0] + fightstg_battle.state.current[0])->boosted = 1;
                *data = fightstg_message_create();
                obj->args[0] = 0x3E;
                obj->args[1] = 0;
                (*data)->show(*data, 2, obj->args);
                obj->base.next_step(obj);
                break;
            case 0x4C: {
                RecordsUsable *item = records_funcs.get_item(obj->item)->data;
                s32 i = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);

#ifdef PC_PORT
                {
                    /* In the Ogremon tutorial the DV Plug also revives any fallen partners while triggering the
                     * blessed Blast on the acting Digimon. */
                    s32 revived_any = 0;
                    s32 blasted;

                    if (def_super_tutorial_battle()) {
                        s32 s;

                        for (s = 0; s < 3; s++) {
                            if (fightstg_battle.state.members[0][s].hp == 0) {
                                s32 revived = def_blast_plug_revive(s);

                                if (revived > 0) {
                                    fightstg_battle.state.members[0][s].hp = revived;
                                    wfightmn_check_regen(s);
                                    revived_any = 1;
                                }
                            }
                        }
                    }
                    /* Blast Gauge phase 1: the DV Plug is the cursed catalyst, one use sets the Blast off (the Blast, not the Plug, costs TP) */
                    blasted = def_blast_plug();
                    if (revived_any || blasted) {
                        *data = fightstg_message_create();
                        if (revived_any) {
                            obj->args[0] = 0;
                            obj->args[1] = 5;
                            (*data)->show(*data, 9, obj->args);
                        } else {
                            obj->args[0] = 0x3F;
                            obj->args[1] = 0;
                            (*data)->show(*data, 2, obj->args);
                        }
                        obj->base.next_step(obj);
                        break;
                    }
                }
#endif
                records_state.gauges[i] += item->amount;
                if (records_state.gauges[i] >= 999) {
                    records_state.gauges[i] = 999;
                }
                *data = fightstg_message_create();
                obj->args[0] = 0x3F;
                obj->args[1] = 0;
                (*data)->show(*data, 2, obj->args);
                obj->base.next_step(obj);
                break;
            }
            case 0x4D:
            case 0x4E:
            case 0x4F:
            case 0x50:
            case 0x51:
            case 0x52:
            case 0x53:
                fightstg_events_set_field_end(fightstg_events.get_delay(0, 8));
                fightstg_battle.state.field.element = obj->item - 0x4B;
                fightstg_battle.state.field.power = 0x40;
                *data = fightstg_message_create();
                obj->args[0] = obj->item + 0x16;
                (*data)->show(*data, 1, obj->args);
                obj->base.next_step(obj);
                break;
            case 0x54:
                fightstg_events_set_field_end(fightstg_events.get_delay(0, 8));
                fightstg_battle.state.field.element = obj->field_element;
                fightstg_battle.state.field.power = 0x7F;
                *data = fightstg_message_create();
                obj->args[0] = obj->field_element + 0x61;
                (*data)->show(*data, 1, obj->args);
                obj->base.next_step(obj);
                break;
            case 0x55:
                if (records_state.blocked[5] == 0) {
                    FightstgMember *member;

                    *data = fightstg_message_create();
                    obj->args[0] = 0x10;
                    obj->args[1] = obj->amount;
                    member = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
                    if (member->hp - obj->amount <= 0) {
                        (*data)->show(*data, 4, obj->args);
                        member->hp = 0;
                        fightstg_events_add_knockout(0x10);
                    } else {
                        (*data)->show(*data, 0x14, obj->args);
                        member->hp -= obj->amount;
                        member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                        member->hp += obj->amount;
                        if (member->hp > member->max_hp) {
                            member->hp = member->max_hp;
                        }
                    }
                } else {
                    *data = fightstg_message_create();
                    obj->args[0] = 0x2F;
                    (*data)->show(*data, 1, obj->args);
                }
                obj->base.next_step(obj);
                break;
            case 0x56: {
                RecordsUsable *item = records_funcs.get_item(obj->item)->data;
                FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                FightstgMember *other = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];

                switch (obj->base.substep) {
                case 0:
                default:
                    if ((pad_random.next() & 1) && records_state.blocked[2] == 0) {
                        fightstg_events_start_confusion(0x10, 0, item->amount);
                        *data = fightstg_message_create();
                        obj->args[0] = 0x20;
                        obj->args[1] = 0x10;
                        (*data)->show(*data, 2, obj->args);
                        if ((pad_random.next() & 3) == 0) {
                            obj->base.next_substep(obj);
                        } else {
                            obj->base.next_step(obj);
                        }
                        other->status |= 4;
                    } else {
                        *data = fightstg_message_create();
                        obj->args[0] = 0x2F;
                        (*data)->show(*data, 1, obj->args);
                        obj->base.next_step(obj);
                    }
                    break;
                case 1:
                    if (*data == NULL) {
                        member->status |= 4;
                        fightstg_events_start_confusion(0, 0, item->amount);
                        *data = fightstg_message_create();
                        obj->args[0] = 0x20;
                        obj->args[1] = 0;
                        (*data)->show(*data, 2, obj->args);
                        obj->base.next_step(obj);
                    }
                    break;
                }
                break;
            }
            case 0x57:
                if (FIGHTSTG_STAT_BLOCKED(9) == 0) {
#ifdef PC_PORT
                    if (!def_mod_apply(0x10, fightstg_battle.state.current[1], 0, 2, DEF_MOD_TURNS_BASE, 0) ||
                        !def_mod_apply(0x10, fightstg_battle.state.current[1], 1, -2, DEF_MOD_TURNS_BASE, 9)) {
#endif
                        FightstgMember *member = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
                        FightstgStats *p = fightstg_rules.get_stats(0x10, 0, fightstg_battle.state.current[1]);
                        s32 a = p->stats[0];
                        s32 b = p->stats[1];
                        s32 max;

                        if (member->modifiers[0] != 0) {
                            p->stats[0] -= member->modifiers[0];
                        }
                        if (member->modifiers[1] != 0) {
                            p->stats[1] -= member->modifiers[1];
                        }
                        member->modifiers[0] += a * 3 / 10;
                        member->modifiers[1] -= b / 2;
                        max = p->stats[0];
                        if (member->modifiers[0] > max) {
                            member->modifiers[0] = max;
                        }
                        if (member->modifiers[1] < -p->stats[1] / 2) {
                            member->modifiers[1] = -p->stats[1] / 2;
                        }
                        fightstg_events_start_modifier(0x10, fightstg_battle.state.current[1], 0, 0);
                        fightstg_events_start_modifier(0x10, fightstg_battle.state.current[1], 1, 0);
#ifdef PC_PORT
                    }
#endif
                    *data = fightstg_message_create();
                    obj->args[0] = 0x40;
                    obj->args[1] = 0x10;
                    (*data)->show(*data, 2, obj->args);
                    obj->base.next_step(obj);
                } else {
                    *data = fightstg_message_create();
                    obj->args[0] = 0x2F;
                    (*data)->show(*data, 1, obj->args);
                    obj->base.next_step(obj);
                }
                break;
            case 0x58:
                switch (obj->lowered) {
                case 1:
                    fightstg_events_start_modifier(0x10, fightstg_battle.state.current[1], 0, 0);
                    *data = fightstg_message_create();
                    obj->args[0] = 0x33;
                    obj->args[1] = 0x10;
                    obj->args[2] = fightstg_battle.state.current[1];
                    (*data)->show(*data, 2, obj->args);
                    break;
                case 2:
                    fightstg_events_start_modifier(0x10, fightstg_battle.state.current[1], 1, 0);
                    *data = fightstg_message_create();
                    obj->args[0] = 0x34;
                    obj->args[1] = 0x10;
                    obj->args[2] = fightstg_battle.state.current[1];
                    (*data)->show(*data, 2, obj->args);
                    break;
                default:
                    *data = fightstg_message_create();
                    obj->args[0] = 0x2F;
                    (*data)->show(*data, 1, obj->args);
                    break;
                }
                obj->base.next_step(obj);
                break;
            case 0x59:
                if (FIGHTSTG_STAT_BLOCKED(10) == 0) {
#ifdef PC_PORT
                    if (!def_mod_apply(0x10, fightstg_battle.state.current[1], 2, -1, DEF_MOD_TURNS_BASE, 10)) {
#endif
                        RecordsUsable *item = records_funcs.get_item(obj->item)->data;
                        FightstgMember *member = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
                        FightstgStats *p = fightstg_rules.get_stats(0x10, 0, fightstg_battle.state.current[1]);
                        s32 lim;

                        if (member->modifiers[2] != 0) {
                            p->stats[4] -= member->modifiers[2];
                        }
                        lim = -(p->stats[4] / 2);
                        member->modifiers[2] -= p->stats[4] * item->amount / 128;
                        if (member->modifiers[2] < lim) {
                            member->modifiers[2] = lim;
                        }
                        fightstg_events_start_modifier(0x10, fightstg_battle.state.current[1], 2, 0);
#ifdef PC_PORT
                    }
#endif
                    *data = fightstg_message_create();
                    obj->args[0] = 0x35;
                    obj->args[1] = 0x10;
                    obj->args[2] = fightstg_battle.state.current[1];
                    (*data)->show(*data, 2, obj->args);
                    obj->base.next_step(obj);
                } else {
                    *data = fightstg_message_create();
                    obj->args[0] = 0x2F;
                    (*data)->show(*data, 1, obj->args);
                    obj->base.next_step(obj);
                }
                break;
            case 0x5A:
                if (obj->amount > 0) {
                    FightstgMember *member;

                    *data = fightstg_message_create();
                    obj->args[0] = 0x10;
                    obj->args[1] = obj->amount;
                    (*data)->show(*data, 4, obj->args);
                    member = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
                    if (member->hp - obj->amount <= 0) {
                        member->hp = 0;
                        fightstg_events_add_knockout(0x10);
                    } else {
                        member->hp -= obj->amount;
                    }
                } else {
                    *data = fightstg_message_create();
                    obj->args[0] = 0x1D;
                    obj->args[1] = 0x10;
                    (*data)->show(*data, 2, obj->args);
                }
                obj->base.next_step(obj);
                break;
            }
            break;
        case 2:
            if (*data == NULL) {
                gamestate_data.items[obj->item]--;
                if (obj->item == 0x55 && fightstg_battle.state.type == 6) {
                    *data = fightstg_boss_turn_create(1, 0);
                    obj->base.next_step(obj);
                } else {
                    obj->base.state = OBJECT_STATE_END;
                }
            }
            break;
        case 3:
            if (*data == NULL) {
                obj->base.set_state(obj, OBJECT_STATE_END);
            }
            break;
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

OBJECT_V0(FightstgItem *) fightstg_item_create(s32 item) {
    FightstgItem *obj = object_new(fightstg_item_update, sizeof(FightstgItem), sizeof(FightstgMessage *));

    obj->item = item;
    OBJECT_V0_RETURN(obj) /* PC_PORT: FINDINGS 8: callers use the object (v0) */
}

s32 fightstg_enemy_check_condition(u8 type, s16 value);
s32 fightstg_enemy_get_action(u8 type);

/* The stage is computed as in WFIGHTMN's tech setup (`stage = n * 3 + 0x21`, + 1 for a strong element). wip-14: own
 * variables for case 0's side index and the damage block's target/victim; the enemy's copied tech reads
 * `(members[1] + current[1])->digimon` for the original's operand order (addu base,index). */
void fightstg_counter_update(FightstgCounter *obj, FightstgMessage **data) {
    FightstgMember *member;
    RecordsTechnique *rec;
    FightstgEnemyRecord *digimon;
    FightstgStats *values;
    FightstgMember *members;
    s32 side;
    s32 j;
    s32 n;
    s32 own;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        if (obj->damage_taken == 0) {
            obj->base.state = OBJECT_STATE_DONE;
            break;
        }
        own = obj->side != 0;
        member = &fightstg_battle.state.members[own][fightstg_battle.state.current[own]];
        if ((member->status & 2) && fightstg_rules.roll_paralyzed(obj->side) != 0) {
            obj->base.state = OBJECT_STATE_DONE;
            break;
        }
        if (obj->side == 0) {
            j = fightstg_events.find_member(8, 0, fightstg_battle.state.current[0]);
            if (j >= 0) {
                obj->tech = fightstg_events.events[j].args[2];
                obj->base.step = 0;
                fightstg_events.events[j].type = 0;
            } else {
                if (fightstg_rules.get_stats(0, 1, fightstg_battle.state.current[0])->counter == 0) {
                    obj->base.state = OBJECT_STATE_DONE;
                    break;
                }
                if (fightstg_rules.roll_counter(0, obj->damage_taken) != 0) {
                    obj->tech = records_get_digimon_func(member->digimon)->techniques[0];
                    obj->base.step = 1;
                } else {
                    obj->base.state = OBJECT_STATE_DONE;
                    break;
                }
            }
        } else {
            digimon = fightstg_enemy_records.get(member->digimon);
            if (digimon->counter_condition == 0) {
                obj->base.state = OBJECT_STATE_DONE;
                break;
            }
            if (fightstg_enemy_check_condition(digimon->counter_condition, digimon->counter_value) == 0) {
                obj->base.state = OBJECT_STATE_DONE;
                break;
            }
            n = fightstg_enemy_get_action(digimon->counter_action);
            obj->tech = n;
            if (n == 1) {
                obj->tech = fightstg_enemy_records.get((fightstg_battle.state.members[1] + fightstg_battle.state.current[1])->digimon)->tech;
                obj->base.step = n;
            }
        }
        *data = fightstg_message_create();
        obj->args[0] = obj->side;
        obj->args[1] = obj->tech;
        (*data)->show(*data, obj->base.step + 5, obj->args);
        obj->hit = fightstg_rules.roll_hit(obj->side, obj->tech);
        if (obj->hit != 0) {
            obj->damage = fightstg_rules.get_counter_damage(obj->side, obj->tech, obj->damage_taken);
#ifdef PC_PORT
            obj->damage = def_counter_damage(obj->side, obj->damage);
#endif
            if (fightstg_battle.state.type != 0) {
                obj->damage = wfightmn_cap_damage(obj->side, obj->damage, 0);
            }
        }
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 0:
        default:
            if (*data == NULL) {
                rec = &records_techniques[obj->tech - 1];
                side = obj->side != 0;
                *data = (FightstgMessage *)fightstg_script_create();
                ((FightstgScript *)*data)->side = obj->side;
                if (obj->side == 0 && rec->anim_script == 5) {
                    ((FightstgScript *)*data)->script = 6;
                } else {
                    ((FightstgScript *)*data)->script = rec->anim_script;
                }
                if (rec->hit_sound != 0) {
                    ((FightstgScript *)*data)->hit_sound = rec->hit_sound;
                }
                values = fightstg_rules.get_stats(obj->side, 1, fightstg_battle.state.current[side]);
                if (rec->defense_stat >= 2 || values->attack_element >= 2) {
                    s32 level;
                    s32 stage;

                    if (rec->defense_stat >= 2) {
                        level = rec->defense_stat - 2;
                    } else {
                        level = values->attack_element - 2;
                    }
                    stage = level * 3 + 0x21;
                    if (values->attack_element_power >= 0x40) {
                        ((FightstgScript *)*data)->stage = stage + 1;
                    } else {
                        ((FightstgScript *)*data)->stage = stage;
                    }
                } else {
                    ((FightstgScript *)*data)->stage = -1;
                }
                if (rec->anim_effect != 0) {
                    ((FightstgScript *)*data)->effect = rec->anim_effect;
                } else if (rec->anim_script == 5) {
                    ((FightstgScript *)*data)->effect = 0x2E;
                    ((FightstgScript *)*data)->hit_sound = 0x1E;
                }
                members = fightstg_battle.state.members[1 - side];
                if (obj->hit != 0) {
                    if (members[fightstg_battle.state.current[1 - side]].hp - obj->damage <= 0) {
                        ((FightstgScript *)*data)->results[3] = 2;
                    } else {
                        ((FightstgScript *)*data)->results[3] = 1;
                        wfightmn_count_boss_hits(obj->side, obj->damage);
                        wfightmn_update_idle_anim((u8)(0x10 - obj->side), obj->damage);
                    }
                } else {
                    ((FightstgScript *)*data)->results[3] = 3;
                }
                members = obj->side != 0 ? fightstg_battle.state.members[1] : fightstg_battle.state.members[0];
                members[fightstg_battle.state.current[obj->side != 0]].power_up = 0;
                obj->base.step++;
            }
            break;
        case 1:
            if (*data == NULL) {
                *data = fightstg_message_create();
                if (obj->hit != 0) {
                    obj->args[0] = (obj->side == 0) << 4;
                    obj->args[1] = obj->damage;
                    (*data)->show(*data, 4, obj->args);
                } else {
                    obj->args[0] = 0x1D;
                    obj->args[1] = (obj->side == 0) << 4;
                    (*data)->show(*data, 2, obj->args);
                }
                if (obj->damage != 0) {
                    s32 target;
                    FightstgMember *victim;

                    target = obj->side == 0;
                    victim = &fightstg_battle.state.members[target][fightstg_battle.state.current[target]];
                    victim->hp -= obj->damage;
                    if (victim->hp <= 0) {
                        victim->hp = 0;
                        if (obj->no_knockout == 0) {
                            fightstg_events_add_knockout(target << 4);
                        }
                    }
                }
                obj->base.step++;
            }
            break;
        case 2:
            if (*data == NULL) {
                wfightmn_add_gauge(obj->side, obj->damage_taken);
                obj->base.state = OBJECT_STATE_END;
            }
            break;
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

void *fightstg_counter_create(s8 side, s32 damage_taken, s32 no_knockout) {
    FightstgCounter *obj = object_new(fightstg_counter_update, sizeof(FightstgCounter), sizeof(FightstgMessage *));

    obj->side = side;
    obj->damage_taken = damage_taken;
    obj->no_knockout = no_knockout;
    return obj;
}

/* Event types taken off the queue when a party member is revived. */
extern s32 fightstg_revive_event_types[6];

/* Revives party member `i` with item `rec` (a record of records_techniques, from 1). */
void fightstg_revive_member(s32 rec, s32 i) {
    FightstgMember *party = fightstg_battle.state.members[0];
    RecordsTechnique *item = &records_techniques[rec - 1];
    s32 k;
    s32 j;

    if (party[i].digimon != 0) {
        for (k = 0; k < 6; k++) {
            j = fightstg_events.find_member(fightstg_revive_event_types[k], 0, i);
            if (j >= 0) {
                fightstg_events.events[j].type = 0;
            }
        }
        if (party[i].hp == 0) {
            wfightmn_check_regen(i);
        }
        party[i].status = 0;
        party[i].hp = party[i].max_hp;
#ifdef PC_PORT
        if (!def_mod_apply_tech(0, i, 1, rec, item->effect_power, 0)) {
            fightstg_rules.change_modifier(0, i, 1, item->effect_power);
            fightstg_events_start_modifier(0, i, 1, rec);
        }
#else
        fightstg_rules.change_modifier(0, i, 1, item->effect_power);
        fightstg_events_start_modifier(0, i, 1, rec);
#endif
    }
}

/* A status technique's effect (fightstg_tech_update): technique `technique` changes stat `stat` by `factor` times its
 * power, with message `message`; -1 ends the table. */
typedef struct FightstgTechStatEffect {
    /* 0x0 */ s16 technique; /* technique */
    /* 0x2 */ s16 factor; /* <= 0: on the other side */
    /* 0x4 */ s16 stat;  /* stat */
    /* 0x6 */ s16 message; /* message */
} FightstgTechStatEffect; /* size 0x8 */

extern FightstgTechStatEffect fightstg_tech_stat_effects[];
extern FightstgTechStatEffect fightstg_tech_party_stat_effects[];
extern u8 fightstg_tech_cure_flags[];
extern s32 fightstg_tech_cure_kinds[];
extern s32 fightstg_tech_cure_messages[];

void fightstg_events_add_regen_end(u8 side);
void fightstg_events_start_regen(u8 side, s32 arg1, s32 arg2);
void fightstg_events_add_counter_stance(s32 arg0);
void fightstg_events_add_revert(void);

void fightstg_tech_update(FightstgTech *obj, FightstgMessage **data) {
    RecordsTechnique *rec;
    s32 side;
    FightstgMember *member;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        rec = &records_techniques[obj->tech - 1];
        if (rec->element == 2 || rec->element == 3) {
            fightstg_action.run(obj->side, obj->tech);
        } else {
            heap_funcs.bzero(&fightstg_action, 0x68); /* PC_PORT: a byte count: the state before `run` (pointer-free) */
        }
        *data = fightstg_message_create();
        if (rec->anim_script == 0xC) {
            obj->args[0] = 0x3A;
            obj->args[1] = obj->side;
            (*data)->show(*data, 2, obj->args);
        } else {
            obj->args[0] = obj->side;
            obj->args[1] = obj->tech;
            (*data)->show(*data, 3, obj->args);
        }
        side = 1 - (obj->side >> 4);
        member = &fightstg_battle.state.members[side][fightstg_battle.state.current[side]];
        if (member->status & 8) {
            obj->target_asleep = 1;
        }
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 0:
        default:
            if (*data == NULL) {
                if (fightstg_action.effects[0xA] == 0) {
                    FightstgMember *members;

                    *data = wfightmn_tech_script_create(obj->side, obj->tech);
                    members = obj->side != 0 ? fightstg_battle.state.members[1] : fightstg_battle.state.members[0];
                    members[fightstg_battle.state.current[obj->side != 0]].power_up = 0;
                } else {
                    wfightmn_check_final_phase_end(obj->side);
                }
                obj->base.step++;
            }
            break;
        case 1:
            if (*data == NULL) {
                RecordsTechnique *rec = &records_techniques[obj->tech - 1];
                switch (rec->element) {
                case 2:
                case 3: {
                    s32 side;

                    *data = fightstg_message_create();
                    side = obj->side == 0;
                    if (fightstg_action.effects[0xA] != 0) {
                        fightstg_events_add_counter_stance(obj->tech);
                        obj->base.state = OBJECT_STATE_END;
                        break;
                    }
                    if (fightstg_action.effects[9] != 0) {
                        if (records_techniques[obj->tech - 1].kind == 0x1F) {
                            obj->damage = fightstg_action.damages[0] + fightstg_action.damages[1];
                            obj->args[0] = side << 4;
                            obj->args[1] = obj->damage;
                            (*data)->show(*data, 4, obj->args);
                        } else {
                            obj->args[0] = side << 4;
                            obj->args[1] = fightstg_action.damage;
                            obj->args[2] = (s16)fightstg_action.hit_count;
                            (*data)->show(*data, 0x10, obj->args);
                            obj->damage = fightstg_action.damage * (s16)fightstg_action.hit_count;
                            if (obj->damage >= 10000) {
                                obj->damage = 9999;
                            }
                        }
                    } else if (fightstg_action.effects[6] != 0) {
                        FightstgMember *p = &fightstg_battle.state.members[side][fightstg_battle.state.current[side]];

                        p->hp = 0;
                        fightstg_events_add_knockout(side << 4);
                        (*data)->base.state = OBJECT_STATE_END;
                    } else if (fightstg_action.hits[0] != 0) {
                        obj->args[0] = side << 4;
                        obj->args[1] = fightstg_action.damage;
                        (*data)->show(*data, 4, obj->args);
                        obj->damage = fightstg_action.damage;
                    } else {
                        obj->args[0] = 0x1D;
                        obj->args[1] = side << 4;
                        (*data)->show(*data, 2, obj->args);
                    }
                    if (obj->damage != 0) {
                        FightstgMember *member = &fightstg_battle.state.members[side][fightstg_battle.state.current[side]];

                        member->hp -= obj->damage;
                        if (member->hp <= 0) {
                            member->hp = 0;
                            fightstg_events_add_knockout(side << 4);
                            obj->base.substep = 1;
                        }
                    }
                    obj->base.step++;
                    break;
                }
                case 4:
                    obj->args[0] = obj->side;
                    switch (obj->tech) {
                    case 0xBD:
                        fightstg_events_add_regen_end(obj->side);
                        fightstg_events_start_regen(obj->side, fightstg_battle.state.current[obj->side != 0], obj->tech);
                        *data = fightstg_message_create();
                        obj->args[0] = 0x27;
                        obj->args[1] = obj->side;
                        (*data)->show(*data, 2, obj->args);
                        obj->base.next_step(obj);
                        break;
                    case 0xBE:
                    case 0xBF:
                    case 0xC0:
                    case 0xC1:
                    case 0xC2:
                    case 0xC3:
                    case 0xC4:
                    case 0xC5: {
                        s32 side = obj->side != 0;
                        s32 k;
                        FightstgMember *members;

                        switch (obj->tech) {
                        case 0xBE:
                        case 0xBF:
                        default:
                            k = 0;
                            break;
                        case 0xC0:
                        case 0xC1:
                            k = 1;
                            break;
                        case 0xC2:
                        case 0xC3:
                            k = 2;
                            break;
                        case 0xC4:
                        case 0xC5:
                            k = 3;
                            break;
                        }
                        members = fightstg_battle.state.members[side];
                        switch (obj->base.substep) {
                        case 0:
                        default:
                            *data = fightstg_message_create();
                            if (obj->tech & 1) {
                                obj->args[0] = obj->side;
                                obj->args[1] = fightstg_tech_cure_kinds[k];
                                (*data)->show(*data, 9, obj->args);
                                obj->base.substep = 1;
                                obj->base.timer = 3;
                                return;
                            }
                            if (members[fightstg_battle.state.current[side]].status & fightstg_tech_cure_flags[k]) {
                                obj->args[0] = fightstg_tech_cure_messages[k];
                                obj->args[1] = obj->side;
                                obj->args[2] = fightstg_battle.state.current[0];
                                (*data)->show(*data, 2, obj->args);
                                obj->base.substep = 1;
                                obj->base.timer = 1;
                                return;
                            }
                            obj->args[0] = 0x2F;
                            (*data)->show(*data, 1, obj->args);
                            obj->base.next_step(obj);
                            break;
                        case 1: {
                            s32 i;

                            for (i = 0; i < obj->base.timer; i++) {
                                fightstg_events.cure_status(obj->side, i, obj->tech);
                            }
                            obj->base.next_step(obj);
                            break;
                        }
                        }
                        break;
                    }
                    case 0x64: {
                        FightstgMember *members = fightstg_battle.state.members[0];

                        switch (obj->base.substep) {
                        case 0:
                        default:
                            *data = fightstg_message_create();
                            obj->args[0] = 0;
                            obj->args[1] = 5;
                            (*data)->show(*data, 9, obj->args);
                            obj->base.substep++;
                            break;
                        case 1: {
                            s32 i;

                            for (i = 0; i < 3; i++) {
                                if (members[i].digimon != 0 && members[i].hp == 0) {
                                    members[i].hp = members[i].max_hp;
                                    wfightmn_check_regen(i);
                                }
                            }
                            obj->base.next_step(obj);
                            break;
                        }
                        }
                        break;
                    }
                    case 0x177:
                        switch (obj->base.substep) {
                        case 0:
                        default:
                            *data = fightstg_message_create();
                            obj->args[0] = 0;
                            obj->args[1] = 6;
                            (*data)->show(*data, 9, obj->args);
                            obj->base.substep++;
                            break;
                        case 1: {
                            s32 i;

                            for (i = 0; i < 3; i++) {
                                fightstg_revive_member(0x177, i);
                            }
                            obj->base.next_step(obj);
                            break;
                        }
                        }
                        break;
                    default: {
                        FightstgMember *members = obj->side != 0 ? fightstg_battle.state.members[1] : fightstg_battle.state.members[0];

                        switch (obj->base.substep) {
                        case 0:
                        default: {
                            s32 msg;
                            s32 n;
                            s32 j;
                            s32 i;

                            msg = -1;
                            obj->healed = fightstg_rules.get_heal(obj->side, obj->tech);
                            n = 0;
                            if (obj->tech >= 0xBB) {
                                for (i = 0; i < 3; i++) {
                                    if (members[i].digimon != 0 && members[i].hp != 0) {
                                        if (n < members[i].max_hp - members[i].hp) {
                                            n = members[i].max_hp - members[i].hp;
                                        }
                                    }
                                }
                                if (n != 0) {
                                    msg = 9;
                                    if (obj->healed < n) {
                                        n = obj->healed;
                                    }
                                    obj->args[1] = 0;
                                    obj->args[2] = n;
                                }
                            } else {
                                i = fightstg_battle.state.current[obj->side != 0];
                                if (n < members[i].max_hp - members[i].hp) {
                                    n = members[i].max_hp - members[i].hp;
                                }
                                if (n != 0) {
                                    msg = 8;
                                    if (obj->healed < n) {
                                        n = obj->healed;
                                    }
                                    obj->healed = n;
                                    obj->args[1] = n;
                                }
                            }
                            *data = fightstg_message_create();
                            obj->args[0] = obj->side;
                            if (msg != -1) {
                                (*data)->show(*data, msg, obj->args);
                                if (obj->tech >= 0xBB) {
                                    for (j = 0; j < 3; j++) {
                                        if (members[j].digimon != 0 && members[j].hp != 0) {
                                            if (members[j].hp + obj->healed > members[j].max_hp) {
                                                members[j].hp = members[j].max_hp;
                                            } else {
                                                members[j].hp += obj->healed;
                                            }
                                        }
                                    }
                                } else {
                                    j = fightstg_battle.state.current[obj->side != 0];
                                    members[j].hp = members[j].hp + obj->healed > members[j].max_hp ? members[j].max_hp : members[j].hp + obj->healed;
                                }
                            } else {
                                obj->args[0] = 0x2F;
                                obj->args[1] = obj->side;
                                (*data)->show(*data, 2, obj->args);
                            }
                            obj->base.next_step(obj);
                            break;
                        }
                        case 1:
                            break;
                        }
                        break;
                    }
                    }
                    break;
                case 5:
                    *data = fightstg_message_create();
#ifdef PC_PORT
                    {
                        s32 def_msg;
                        s32 def_done = def_tech_effect(obj->side, obj->tech, &def_msg);

                        if (def_done == 1) { /* Libra, Relay, Resist Down, Mega Break (definitive combat) */
                            obj->args[0] = def_msg;
                            obj->args[1] = obj->side;
                            (*data)->show(*data, 23, obj->args);
                            obj->base.next_step(obj); /* as the original's cases end, else the turn never goes on */
                            break;
                        } else if (def_done == 2) { /* immune: the original's "no effect" */
                            obj->args[0] = 0x2F;
                            (*data)->show(*data, 1, obj->args);
                            obj->base.next_step(obj);
                            break;
                        }
                    }
#endif
                    switch (obj->tech) {
#ifdef PC_PORT
                    case 0xC2:
                    case 0xC3:
#endif
                    case 0xC6:
                    case 0xC8:
                    case 0xCA:
                    case 0xCC:
                    case 0xCD:
                    case 0xCE:
                    case 0xCF:
                    case 0xD0: {
                        FightstgTechStatEffect *e;
                        s32 ok;
                        u32 side;

                        for (e = fightstg_tech_stat_effects; e->technique != -1; e++) {
                            if (e->technique == obj->tech) {
                                break;
                            }
                        }
#ifdef PC_PORT
                        {
                            static const FightstgTechStatEffect extra_effects[2] = {
                                { 194, -1, 0, 51 }, { 195, -1, 0, 51 },
                            };
                            if (e->technique == -1) {
                                e = (FightstgTechStatEffect *)(obj->tech == 194 ? &extra_effects[0] : &extra_effects[1]);
                            }
                        }
#endif
                        ok = 1;
                        side = obj->side >> 4;
                        if (e->factor <= 0) {
                            side ^= 1;
                            if (obj->side == 0 && FIGHTSTG_STAT_BLOCKED(e->stat + 8) != 0) {
                                obj->args[0] = 0x2F;
                                (*data)->show(*data, 1, obj->args);
                                ok = 0;
                            }
                        }
                        if (ok) {
#ifdef PC_PORT
                            if (!def_mod_apply_tech(side << 4, fightstg_battle.state.current[side], e->stat, obj->tech,
                                                    e->factor * rec->effect_power,
                                                    (e->factor <= 0 && obj->side == 0) ? e->stat + 8 : 0)) {
                                fightstg_rules.change_modifier(side << 4, fightstg_battle.state.current[side], e->stat,
                                                               e->factor > 0 || obj->side != 0 ? e->factor * rec->effect_power
                                                                   : FIGHTSTG_DEBUFF(e->stat + 8, e->factor * rec->effect_power));
                                fightstg_events_start_modifier(side << 4, fightstg_battle.state.current[side], e->stat, obj->tech);
                            }
#else
                            fightstg_rules.change_modifier(side << 4, fightstg_battle.state.current[side], e->stat, e->factor * rec->effect_power);
                            fightstg_events_start_modifier(side << 4, fightstg_battle.state.current[side], e->stat, obj->tech);
#endif
                            obj->args[0] = e->message;
                            obj->args[1] = side << 4;
                            obj->args[2] = fightstg_battle.state.current[side];
                            (*data)->show(*data, 2, obj->args);
                        }
                        break;
                    }
                    case 0xC7:
                    case 0xC9:
                    case 0xCB: {
                        FightstgTechStatEffect *e;
                        s32 i;

                        for (e = fightstg_tech_party_stat_effects; e->technique != -1; e++) {
                            if (e->technique == obj->tech) {
                                break;
                            }
                        }
                        for (i = 0; i < 3; i++) {
                            if (fightstg_battle.state.members[0][(obj->side >> 4) * 3 + i].digimon != 0 &&
                                fightstg_battle.state.members[0][(obj->side >> 4) * 3 + i].hp > 0) {
#ifdef PC_PORT
                                if (!def_mod_apply_tech(obj->side, i, e->stat, obj->tech, e->factor * rec->effect_power, 0)) {
                                    fightstg_rules.change_modifier(obj->side, i, e->stat, e->factor * rec->effect_power);
                                    fightstg_events_start_modifier(obj->side, i, e->stat, obj->tech);
                                }
#else
                                fightstg_rules.change_modifier(obj->side, i, e->stat, e->factor * rec->effect_power);
                                fightstg_events_start_modifier(obj->side, i, e->stat, obj->tech);
#endif
                            }
                        }
                        obj->args[0] = obj->side;
                        switch (e->stat) {
                        case 0:
                            obj->args[1] = 7;
                            break;
                        case 1:
                            obj->args[1] = 8;
                            break;
                        case 2:
                            obj->args[1] = 9;
                            break;
                        }
                        (*data)->show(*data, 9, obj->args);
                        break;
                    }
                    case 0xD1:
                    case 0xD2: {
                        s32 s = obj->side >> 4;
                        FightstgMember *p = &fightstg_battle.state.members[s][fightstg_battle.state.current[s]];

                        p->power_up = rec->effect_power;
                        obj->args[0] = 0x36;
                        obj->args[1] = obj->side;
                        (*data)->show(*data, 2, obj->args);
                        break;
                    }
                    case 0xD3:
                        if (fightstg_rules.roll_switch_seal(0x10, 0xD3) != 0) {
                            fightstg_events_start_seal(0xD3);
                            obj->args[0] = 0x44;
                            obj->args[1] = 0;
                            (*data)->show(*data, 2, obj->args);
                        } else {
                            obj->args[0] = 0x2F;
                            (*data)->show(*data, 1, obj->args);
                        }
                        break;
                    case 0xD4:
                        if (fightstg_rules.roll_digivolve_seal(0x10, 0xD4) != 0) {
                            fightstg_events_start_seal(0xD4);
                            obj->args[0] = 0x45;
                            obj->args[1] = 0;
                            (*data)->show(*data, 2, obj->args);
                        } else {
                            obj->args[0] = 0x2F;
                            (*data)->show(*data, 1, obj->args);
                        }
                        break;
                    case 0x187:
                        if (fightstg_rules.roll_revert(0x10, 0x187) != 0) {
                            fightstg_events_add_revert();
                            (*data)->base.state = OBJECT_STATE_END;
                        } else {
                            obj->args[0] = 0x2F;
                            (*data)->show(*data, 1, obj->args);
                        }
                        break;
                    case 0x188: {
                        FightstgMember *other = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
                        FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                        s32 n;

                        if (member->mp != 0) {
                            n = member->max_mp * rec->effect_power / 128;
                            if (n > member->mp) {
                                n = member->mp;
                            }
                            other->mp += n;
                            member->mp -= n;
                            if (other->mp > other->max_mp) {
                                other->mp = other->max_mp;
                            }
                            if (member->mp < 0) {
                                member->mp = 0;
                            }
                            obj->args[0] = 0x10;
                            obj->args[1] = n;
                            obj->args[2] = 1;
                            (*data)->show(*data, 0x12, obj->args);
                        } else {
                            obj->args[0] = 0x2F;
                            obj->args[1] = 0x10;
                            (*data)->show(*data, 2, obj->args);
                        }
                        break;
                    }
                    case 0x190: {
                        FightstgMember *member = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];

                        obj->healed = fightstg_rules.get_heal(obj->side, 0x190);
                        member->hp += obj->healed;
                        if (member->hp > member->max_hp) {
                            member->hp = member->max_hp;
                        }
                        member->power_up = 0x20;
                        obj->args[0] = 0x46;
                        obj->args[1] = 0x10;
                        (*data)->show(*data, 2, obj->args);
                        break;
                    }
                    }
                    obj->base.next_step(obj);
                    break;
                case 6: {
                    FightstgStats *p;
                    s32 n;

                    *data = fightstg_message_create();
                    p = fightstg_rules.get_stats(obj->side, 1, fightstg_battle.state.current[obj->side == 0x10]);
                    fightstg_battle.state.field.element = rec->element;
                    n = (s16)(p->stats[2] / 10) + rec->effect_power;
                    if (n >= 0x80) {
                        n = 0x7F;
                    }
                    fightstg_battle.state.field.power = n;
                    fightstg_events_set_field_end(p->stats[2] * 12 + 1000);
                    obj->args[0] = rec->defense_stat + 0x61;
                    (*data)->show(*data, 1, obj->args);
                    obj->base.next_step(obj);
                    break;
                }
                }
            }
            break;
        case 2:
            if (*data == NULL) {
                RecordsTechnique *rec = &records_techniques[obj->tech - 1];
                if ((rec->element == 2 || rec->element == 3) && obj->base.substep == 0) {
                    if (obj->damage == 0) {
                        if (obj->side == 0 && fightstg_battle.state.type == 6) {
                            obj->base.set_step(obj, 7);
                            break;
                        }
                    } else {
                        *data = (FightstgMessage *)fightstg_results_create(obj->side);
                        obj->base.next_step(obj);
                        break;
                    }
                }
                obj->base.state = OBJECT_STATE_END;
            }
            break;
        case 3:
            if (*data == NULL) {
                obj->base.step++;
            }
            break;
        case 4: {
            s32 side = obj->side == 0;
            FightstgMember *member = &fightstg_battle.state.members[side][fightstg_battle.state.current[side]];
            RecordsTechnique *rec;

            rec = &records_techniques[obj->tech - 1];
            if (member->status & 8) {
                if (obj->target_asleep != 0 && fightstg_rules.roll_wake(side << 4, obj->damage) != 0) {
                    s32 j;

                    obj->args[0] = 0x2B;
                    obj->args[1] = 0x10 - obj->side;
                    obj->args[2] = fightstg_battle.state.current[side];
                    *data = fightstg_message_create();
                    (*data)->show(*data, 7, obj->args);
                    member->status &= ~8;
                    j = fightstg_events.find_member(0xC, 0x10 - obj->side, fightstg_battle.state.current[side]);
                    if (j >= 0) {
                        fightstg_events.events[j].type = 0;
                    }
                    obj->base.step = 9;
                    break;
                }
                obj->base.state = OBJECT_STATE_END;
                break;
            }
            if (rec->element == 2) {
                *data = fightstg_counter_create((obj->side == 0) << 4, obj->damage, rec->anim_script == 0xC);
                obj->base.step = 6;
            } else {
                obj->base.step = 5;
            }
            break;
        }
        case 6:
            if (*data == NULL) {
                obj->base.state = OBJECT_STATE_END;
                break;
            }
            if ((*data)->base.state != OBJECT_STATE_DONE) {
                break;
            }
        case 5:
            wfightmn_add_gauge(obj->side, obj->damage);
            obj->base.state = OBJECT_STATE_END;
            break;
        case 7:
            if (*data == NULL) {
                RecordsTechnique *rec = &records_techniques[obj->tech - 1];
                *data = fightstg_boss_turn_create(1, rec->anim_script == 0xC);
                obj->base.step++;
            }
            break;
        case 8:
            if (*data == NULL) {
                obj->base.set_state(obj, OBJECT_STATE_END);
            }
            break;
        case 9:
            if (*data == NULL) {
                RecordsTechnique *rec = &records_techniques[obj->tech - 1];
                if (rec->element == 2) {
                    *data = fightstg_counter_create((obj->side == 0) << 4, obj->damage, rec->anim_script == 0xC);
                    obj->base.step = 6;
                } else {
                    obj->base.step = 5;
                }
            }
            break;
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

OBJECT_V0(FightstgTech *) fightstg_tech_create(s8 side, s32 tech) {
    FightstgTech *obj = object_new(fightstg_tech_update, sizeof(FightstgTech), sizeof(FightstgMessage *));

    obj->side = side;
    obj->tech = tech;
    OBJECT_V0_RETURN(obj) /* PC_PORT: FINDINGS 8: callers use the object (v0) */
}

/* The scripted battle's enemy turn (battle type 5): the enemy uses its tech_2, which leaves the party's member at
 * 1 HP, then the boss enters (fightstg_events_add_boss_entrance). */
typedef struct FightstgScriptedTurn {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 args[3]; /* the message's arguments (fightstg_message_show's args); [1] is also the technique used */
    /* 0x5C */ u8 unk_5C[0x14];
} FightstgScriptedTurn; /* size 0x70 */

void fightstg_scripted_turn_update(FightstgScriptedTurn *obj, FightstgMessage **data) {
    FightstgEnemyRecord *digimon;
    FightstgMember *member;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        digimon = fightstg_enemy_records.get(fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon);
        *data = fightstg_message_create();
        obj->args[0] = 0x10;
        obj->args[1] = digimon->tech_2;
        (*data)->show(*data, 3, obj->args);
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 0:
        default:
            if (*data == NULL) {
                *data = wfightmn_tech_script_create(0x10, obj->args[1]);
                obj->base.step++;
            }
            break;
        case 1:
            if (*data == NULL) {
                member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                *data = fightstg_message_create();
                obj->args[0] = 0;
                obj->args[1] = member->hp - 1;
                (*data)->show(*data, 4, obj->args);
                member->hp = 1;
                obj->base.step++;
            }
            break;
        case 2:
            if (*data == NULL) {
                fightstg_events_add_enemy_turn(fightstg_events.get_delay(0x10, 0));
                fightstg_events_add_boss_entrance();
                obj->base.state = OBJECT_STATE_END;
            }
            break;
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

OBJECT_V0(FightstgScriptedTurn *) fightstg_scripted_turn_create(void) {
    /* PC_PORT: FINDINGS 8: callers use the object (v0) */
    OBJECT_V0_TAIL(object_new(fightstg_scripted_turn_update, sizeof(FightstgScriptedTurn), sizeof(FightstgMessage *)))
}

void fightstg_boss_turn_update(FightstgBossTurn *obj, FightstgMessage **data) {
    FightstgEnemyRecord *digimon = fightstg_enemy_records.get(fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon);
    s32 j;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->base.set_state(obj, obj->counter + 1);
        break;
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 0:
        default:
            fightstg_events_add_enemy_turn(fightstg_events.get_delay(0x10, 0));
            if (fightstg_battle.state.final_phase != 0) {
                if (fightstg_events.find(0x18) >= 0) {
                    obj->base.set_state(obj, OBJECT_STATE_END);
                } else {
                    *data = fightstg_message_create();
                    obj->args[0] = 8;
                    obj->args[1] = 0x10;
                    obj->tech = digimon->tech_3;
                    (*data)->show(*data, 2, obj->args);
                    fightstg_battle.state.final_phase = 0;
                    fightstg_battle.state.hits = 0;
                    obj->restore_idle = 1;
                }
            } else {
                *data = fightstg_message_create();
                obj->args[0] = 0x10;
                obj->args[1] = fightstg_battle.state.copied_tech != 0 ? fightstg_battle.state.copied_tech : digimon->tech;
                (*data)->show(*data, 3, obj->args);
                obj->tech = digimon->tech;
            }
            if ((fightstg_battle.state.members[0] + fightstg_battle.state.current[0])->status & 8) {
                obj->target_asleep = 1;
            }
            obj->base.step++;
            break;
        case 1:
            if (*data == NULL) {
                fightstg_action.run(0x10, obj->tech);
                *data = wfightmn_tech_script_create(0x10, obj->tech);
                if (obj->restore_idle != 0) {
                    wfightmn_update_idle_anim(0x10, 0);
                    obj->restore_idle = 0;
                }
                obj->base.step++;
            }
            break;
        case 2:
            if (*data == NULL) {
                *data = fightstg_message_create();
                if (fightstg_action.hits[0] != 0) {
                    FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];

                    if (fightstg_action.effects[6] != 0) {
                        member->hp = 0;
                        (*data)->base.state = OBJECT_STATE_END;
                    } else {
                        obj->args[0] = 0;
                        obj->args[1] = fightstg_action.damage;
                        (*data)->show(*data, 4, obj->args);
                        member->hp -= fightstg_action.damage;
                        if (member->hp <= 0) {
                            member->hp = 0;
                        }
                    }
                    obj->base.step++;
                } else {
                    obj->args[0] = 0x1D;
                    obj->args[1] = 0;
                    (*data)->show(*data, 2, obj->args);
                    obj->base.step = 5;
                }
            }
            break;
        case 3:
            if (*data == NULL) {
                FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];

                if (fightstg_action.effects[6] != 0) {
                    member->hp = 0;
                    fightstg_events_add_knockout(0);
                    obj->base.state = OBJECT_STATE_END;
                } else if (member->hp <= 0) {
                    member->hp = 0;
                    fightstg_events_add_knockout(0);
                    obj->base.state = OBJECT_STATE_END;
                } else {
                    *data = (FightstgMessage *)fightstg_results_create(0x10);
                    obj->base.step++;
                }
            }
            break;
        case 4:
            if (*data == NULL) {
                FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];

                if (!(member->status & 8)) {
                    obj->base.step = 5;
                } else if (obj->target_asleep != 0 && fightstg_rules.roll_wake(0, fightstg_action.damage) != 0) {
                    obj->args[0] = 0x2B;
                    obj->args[1] = 0;
                    obj->args[2] = fightstg_battle.state.current[0];
                    *data = fightstg_message_create();
                    (*data)->show(*data, 7, obj->args);
                    member->status &= ~8;
                    j = fightstg_events.find_member(0xC, 0, fightstg_battle.state.current[0]);
                    if (j >= 0) {
                        fightstg_events.events[j].type = 0;
                    }
                    obj->base.step = 5;
                } else {
                    obj->base.state = OBJECT_STATE_END;
                }
            }
            break;
        case 5:
            if (*data == NULL) {
                wfightmn_add_gauge(0x10, fightstg_action.damage);
                obj->base.state = OBJECT_STATE_END;
            }
            break;
        }
        break;
    case OBJECT_STATE_DONE:
        if (*data == NULL) {
            switch (obj->base.step) {
            case 0:
            default:
                *data = fightstg_message_create();
                obj->args[0] = 0x10;
                (*data)->show(*data, 6, obj->args);
                obj->base.step++;
                break;
            case 1:
                fightstg_action.run(0x10, digimon->tech_2);
                *data = wfightmn_tech_script_create(0x10, digimon->tech_2);
                obj->base.step++;
                break;
            case 2:
                *data = fightstg_message_create();
                if (fightstg_action.hits[0] != 0) {
                    FightstgMember *member;

                    obj->args[0] = 0;
                    obj->args[1] = fightstg_action.damage;
                    (*data)->show(*data, 4, obj->args);
                    member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                    member->hp -= fightstg_action.damage;
                    if (member->hp <= 0) {
                        member->hp = 0;
                        if (obj->no_knockout == 0) {
                            fightstg_events_add_knockout(0);
                        }
                    } else {
                        wfightmn_add_gauge(0x10, fightstg_action.damage);
                    }
                } else {
                    obj->args[0] = 0x1D;
                    obj->args[1] = 0;
                    (*data)->show(*data, 2, obj->args);
                }
                obj->base.step++;
                break;
            case 3:
                fightstg_events_end_final_phase();
                obj->base.state = OBJECT_STATE_END;
                break;
            }
        }
        break;
    case OBJECT_STATE_END:
        break;
    }
}

void *fightstg_boss_turn_create(s32 arg0, s32 arg1) {
    FightstgBossTurn *obj = object_new(fightstg_boss_turn_update, sizeof(FightstgBossTurn), sizeof(FightstgMessage *));

    obj->counter = arg0;
    obj->no_knockout = arg1;
    return obj;
}

/* Each branch clears its effect itself (the copies are cross-jumped into one tail, except 0x21's, which keeps it):
 * the per-branch copies make the loop long enough (396 insns) that loop.c leaves the seven 0x10 constants in place
 * (last-fight; the one shared clear gave 376 insns and hoisted 0x10 into s6). The original's jump table starts at
 * case 2 (hence the case 2 label); `enemy = obj->side != 0` is its own variable (wip-9: written inline GCC folds
 * `1 - (side != 0)` to `side == 0`). */
void fightstg_results_update(FightstgResults *obj, FightstgMessage **data) {
    s32 i;
    s32 side;
    s32 enemy;
    FightstgMember *member;
    FightstgMember *target;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        fightstg_action.effects[9] = 0;
        fightstg_action.effects[10] = 0;
        fightstg_action.effects[11] = 0;
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 0:
        default:
            for (i = 0; i < 0x25; i++) {
                if (fightstg_action.effects[i] != 0) {
                    obj->base.substep = i;
                    side = obj->side >> 4;
                    obj->base.step++;
                    member = &fightstg_battle.state.members[1 - side][fightstg_battle.state.current[1 - side]];
                    if (i == 5 && (member->status & 8)) {
                        fightstg_events_start_sleep(0x10 - obj->side, fightstg_action.tech, fightstg_action.effects[5]);
                        fightstg_action.effects[obj->base.substep] = 0;
                        break;
                    } else {
                        *data = fightstg_message_create();
                        if (i == 12) {
                            target = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
                            obj->args[0] = target->item;
                            (*data)->show(*data, 0x11, obj->args);
                            gamestate_data.items[target->item]++;
                            if (gamestate_data.items[target->item] >= 100) {
                                gamestate_data.items[target->item] = 99;
                            }
                            target->item = -1;
                            fightstg_action.effects[obj->base.substep] = 0;
                        } else if (i == 8) {
                            member = &fightstg_battle.state.members[side][fightstg_battle.state.current[side]];
                            obj->args[0] = obj->side;
                            if (member->max_hp < member->hp + fightstg_action.drained) {
                                obj->args[1] = member->max_hp - member->hp;
                            } else {
                                obj->args[1] = fightstg_action.drained;
                            }
                            obj->args[2] = 0;
                            (*data)->show(*data, 0x12, obj->args);
                            member->hp += obj->args[1];
                            fightstg_action.effects[obj->base.substep] = 0;
                        } else if (i == 0x1D) {
                            member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                            target = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
                            obj->args[0] = 0x10;
                            if (member->mp < fightstg_action.drained) {
                                obj->args[1] = member->mp;
                            } else {
                                obj->args[1] = fightstg_action.drained;
                            }
                            obj->args[2] = 1;
                            (*data)->show(*data, 0x12, obj->args);
                            target->mp += obj->args[1];
                            if (target->max_mp < target->mp) {
                                target->mp = target->max_mp;
                            }
                            member->mp -= obj->args[1];
                            fightstg_action.effects[obj->base.substep] = 0;
                        } else if (i == 0x20) {
                            obj->args[0] = 0x10 - obj->side;
                            if (fightstg_action.effects[0x20] & 1) {
                                obj->args[1] = 0;
                            } else if (fightstg_action.effects[0x20] & 2) {
                                obj->args[1] = 1;
                            } else if (fightstg_action.effects[0x20] & 4) {
                                obj->args[1] = 2;
                            }
                            (*data)->show(*data, 0x13, obj->args);
                            fightstg_action.effects[obj->base.substep] = 0;
                        } else if (i == 0x21) {
                            obj->args[0] = 0x10 - obj->side;
                            if (fightstg_action.effects[0x21] & 1) {
                                obj->args[1] = 0;
                                fightstg_action.effects[obj->base.substep] &= ~1;
                            } else if (fightstg_action.effects[0x21] & 2) {
                                obj->args[1] = 1;
                                fightstg_action.effects[obj->base.substep] &= ~2;
                            } else if (fightstg_action.effects[0x21] & 4) {
                                obj->args[1] = 2;
                                fightstg_action.effects[obj->base.substep] &= ~4;
                            }
                            (*data)->show(*data, 0x13, obj->args);
                            break;
                        } else {
                            enemy = obj->side != 0;
                            switch (i) {
                            case 2:
                            default:
                                fightstg_events_start_poison(0x10 - obj->side, fightstg_battle.state.current[1 - enemy],
                                                       fightstg_action.effects[i]);
                                obj->args[0] = 0x1E;
                                break;
                            case 3:
                                fightstg_events_start_paralysis(0x10 - obj->side, fightstg_action.tech, fightstg_action.effects[i]);
                                obj->args[0] = 0x1F;
                                break;
                            case 4:
                                fightstg_events_start_confusion(0x10 - obj->side, fightstg_action.tech, fightstg_action.effects[i]);
                                obj->args[0] = 0x20;
                                break;
                            case 5:
                                fightstg_events_start_sleep(0x10 - obj->side, fightstg_action.tech, fightstg_action.effects[i]);
                                obj->args[0] = 0x21;
                                break;
                            case 13:
                                fightstg_events_start_seal(0xD3);
                                obj->args[0] = 0x44;
                                break;
                            case 26:
                                obj->args[0] = 0x33;
                                break;
                            case 27:
                                obj->args[0] = 0x34;
                                break;
                            case 34:
                                obj->args[0] = 0x4C;
                                break;
                            case 35:
                                obj->args[0] = 0x48;
                                fightstg_events_end_battle(0);
                                break;
                            }
                            obj->args[1] = (obj->side == 0) << 4;
                            (*data)->show(*data, 2, obj->args);
                            fightstg_action.effects[obj->base.substep] = 0;
                        }
                    }
                    break;
                }
            }
            if (obj->base.substep == 0) {
                obj->base.state = OBJECT_STATE_END;
            }
            break;
        case 1:
            if (*data == NULL) {
                obj->base.set_step(obj, 0);
            }
            break;
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

FightstgResults *fightstg_results_create(u8 side) {
    FightstgResults *obj = object_new(fightstg_results_update, sizeof(FightstgResults), sizeof(FightstgMessage *));

    obj->side = side;
    return obj;
}

void fightstg_model_view_set(FightstgModelView *obj) {
    GfxLayer *layer;

    RotMatrixYXZ_gte(&obj->rot, &obj->coord.coord);
    obj->coord.flg = 0;
    obj->view.super = &obj->coord;
    obj->coord.coord.t[0] = obj->trans.vx;
    obj->coord.coord.t[1] = obj->trans.vy;
    obj->coord.coord.t[2] = obj->trans.vz;
    GsSetRefView2(&obj->view);
    layer = gfx_module.funcs.get_layer(0x1009);
    layer->save_camera(layer, 1, obj->projection);
    obj->frames--;
}

void fightstg_model_view_reset(FightstgModelView *obj) {
    FightstgModelRecordA *rec = fightstg_models.get(obj->model_id);
    s32 i = 6;

    if (obj->params->idle_anim != 0) {
        i = 7;
    }
    obj->view.vpx = rec->camera_eyes[i][0];
    obj->view.vpy = -rec->camera_eyes[i][1];
    obj->view.vpz = -rec->camera_eyes[i][2];
    obj->view.vrx = rec->camera_targets[i][0];
    obj->view.vry = -rec->camera_targets[i][1];
    obj->view.vrz = -rec->camera_targets[i][2];
    obj->projection = rec->camera_projections[i];
    obj->rot.vx = 0;
    obj->rot.vy = 0;
    obj->rot.vz = 0;
    obj->trans.vx = 0;
    obj->trans.vy = 0;
    obj->trans.vz = 0;
}

void fightstg_model_view_update(FightstgModelView *obj) {
    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->base.next_state(obj);
    case OBJECT_STATE_RUN:
        switch (obj->base.substep) {
        case 0:
        default:
            if (gfx_module.funcs.get_layer(0x1009) != NULL) {
                fightstg_model_view_reset(obj);
                obj->frames = 2;
                obj->base.next_substep(obj);
            }
        case 1:
            break;
        }
        if (obj->frames != 0) {
            fightstg_model_view_set(obj);
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

FightstgModelView *fightstg_model_view_create(s32 id, FightstgModelParams *params) {
    FightstgModelView *obj = object_new(fightstg_model_view_update, sizeof(FightstgModelView), 0);

    obj->model_id = id;
    obj->params = params;
    return obj;
}

/* The camera: moves setting from move_from to move_to (move) and sets the view of layer layer_id. */
void fightstg_camera_update(FightstgCamera *obj) {
    SVECTOR a;
    SVECTOR b;
    SVECTOR out;
    GsCOORDINATE2 coord;
    GsRVIEW2 view;
    GfxLayer *layer;
    s32 t;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->move_pos = 0x1000;
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_DONE:
        obj->base.set_state(obj, OBJECT_STATE_RUN);
        obj->base.set_step(obj, 1);
        /* fallthrough */
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 1:
        case 2:
            if (obj->move_pos != 0x1000) {
                t = obj->move_pos + (obj->move_step >> 8) * gfx_module.funcs.get_frame_ticks();
                obj->move_pos = t;
                if (t < 0x1000) {
                    a.vx = obj->move_from.eye[0];
                    a.vy = obj->move_from.eye[1];
                    a.vz = obj->move_from.eye[2];
                    b.vx = obj->move_to.eye[0];
                    b.vy = obj->move_to.eye[1];
                    b.vz = obj->move_to.eye[2];
                    fightstg_math.lerp(&a, &b, t, &out);
                    obj->setting.eye[0] = out.vx;
                    obj->setting.eye[1] = out.vy;
                    obj->setting.eye[2] = out.vz;
                    a.vx = obj->move_from.target[0];
                    a.vy = obj->move_from.target[1];
                    a.vz = obj->move_from.target[2];
                    b.vx = obj->move_to.target[0];
                    b.vy = obj->move_to.target[1];
                    b.vz = obj->move_to.target[2];
                    fightstg_math.lerp(&a, &b, t, &out);
                    obj->setting.target[0] = out.vx;
                    obj->setting.target[1] = out.vy;
                    obj->setting.target[2] = out.vz;
                    a.vx = obj->move_from.trans[0];
                    a.vy = obj->move_from.trans[1];
                    a.vz = obj->move_from.trans[2];
                    b.vx = obj->move_to.trans[0];
                    b.vy = obj->move_to.trans[1];
                    b.vz = obj->move_to.trans[2];
                    fightstg_math.lerp(&a, &b, t, &out);
                    obj->setting.trans[0] = out.vx;
                    obj->setting.trans[1] = out.vy;
                    obj->setting.trans[2] = out.vz;
                    fightstg_math.lerp((SVECTOR *)obj->move_from.rot, (SVECTOR *)obj->move_to.rot, t, &out);
                    obj->setting.rot[0] = out.vx;
                    obj->setting.rot[1] = out.vy;
                    obj->setting.rot[2] = out.vz;
                    a.vx = obj->move_from.roll;
                    a.vy = obj->move_from.projection;
                    a.vz = 0;
                    b.vx = obj->move_to.roll;
                    b.vy = obj->move_to.projection;
                    b.vz = 0;
                    fightstg_math.lerp(&a, &b, t, &out);
                    obj->setting.roll = out.vx;
                    obj->setting.projection = out.vy;
                } else {
                    obj->move_pos = 0x1000;
                    obj->setting = obj->move_to;
                }
            }
            RotMatrixYXZ_gte((SVECTOR *)obj->setting.rot, &coord.coord);
            coord.coord.t[0] = obj->setting.trans[0];
            coord.coord.t[1] = obj->setting.trans[1];
            coord.coord.t[2] = obj->setting.trans[2];
            coord.flg = 0;
            coord.param = NULL;
            coord.super = NULL;
            coord.sub = NULL;
            view.vpx = obj->setting.eye[0];
            view.vpy = obj->setting.eye[1];
            view.vpz = obj->setting.eye[2];
            view.vrx = obj->setting.target[0];
            view.vry = obj->setting.target[1];
            view.vrz = obj->setting.target[2];
            view.rz = obj->setting.roll << 12;
            view.super = &coord;
            GsSetRefView2(&view);
            layer = gfx_module.funcs.get_layer(obj->layer_id);
            layer->save_camera(layer, 1, obj->setting.projection);
            if (obj->move_pos == 0x1000) {
                obj->base.next_step(obj);
            }
            break;
        }
        break;
    case OBJECT_STATE_END:
        layer = gfx_module.funcs.get_layer(obj->layer_id);
        layer->save_camera(layer, 0, 0);
        break;
    }
}

void fightstg_camera_set(FightstgCamera *obj, FightstgCameraSetting *params) {
    obj->setting = *params;
    obj->move_pos = 0x1000;
    obj->base.set_state(obj, OBJECT_STATE_DONE);
}

void fightstg_camera_move(FightstgCamera *obj, FightstgCameraSetting *from, FightstgCameraSetting *to, s32 frames) {
    if (from != NULL) {
        obj->move_from = *from;
    } else {
        obj->move_from = obj->setting;
    }
    obj->move_to = *to;
    obj->move_step = 0x100000 / frames;
    obj->move_pos = 0;
    obj->base.set_state(obj, OBJECT_STATE_DONE);
}

FightstgCameraSetting *fightstg_camera_get_preset(FightstgCamera *obj, s32 slot, s32 preset) {
    FightstgSlots *list;
    s32 id;
    FightstgModelRecordA *a;
    FightstgModelRecordB *b;

    /* Evidence (class A2, sched2 barrier; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the original finishes every register save and
     * argument copy before the first call's arguments. */
    LOOP_BARRIER();
    list = (FightstgSlots *)heap_objects.find(0x14, -1, -1);
    if (list != NULL) {
        id = list->get_model_id(list, slot);
#ifdef PC_PORT
        if ((slot & 0xF0) && id != 0) { /* an enemy's cameras are a type B record's: a player form fighting as one borrows its stand-in's */
            id = def_enemy_camera_model(id);
        }
#endif
        if (id != 0) {
            if (!(slot & 0xF0)) {
                a = fightstg_models.get(id);
                fightstg_camera_setting.eye[0] = a->camera_eyes[preset][0];
                fightstg_camera_setting.eye[1] = -a->camera_eyes[preset][1];
                fightstg_camera_setting.eye[2] = -a->camera_eyes[preset][2];
                fightstg_camera_setting.target[0] = a->camera_targets[preset][0];
                fightstg_camera_setting.target[1] = -a->camera_targets[preset][1];
                fightstg_camera_setting.target[2] = -a->camera_targets[preset][2];
                fightstg_camera_setting.projection = a->camera_projections[preset];
            } else {
                b = fightstg_models.get(id);
                fightstg_camera_setting.eye[0] = b->camera_eyes[preset][0];
                fightstg_camera_setting.eye[1] = -b->camera_eyes[preset][1];
                fightstg_camera_setting.eye[2] = -b->camera_eyes[preset][2];
                fightstg_camera_setting.target[0] = b->camera_targets[preset][0];
                fightstg_camera_setting.target[1] = -b->camera_targets[preset][1];
                fightstg_camera_setting.target[2] = -b->camera_targets[preset][2];
                fightstg_camera_setting.projection = b->camera_projections[preset];
            }
            fightstg_camera_setting.rot[0] = 0;
            fightstg_camera_setting.rot[1] = 0;
            fightstg_camera_setting.rot[2] = 0;
            fightstg_camera_setting.trans[0] = 0;
            fightstg_camera_setting.trans[1] = 0;
            fightstg_camera_setting.trans[2] = 0;
            fightstg_camera_setting.roll = 0;
            obj->base.set_state(obj, OBJECT_STATE_DONE);
        }
    }
    return &fightstg_camera_setting;
}

FightstgCameraSetting *fightstg_camera_get_default(FightstgCamera *obj) {
    FightstgSlots *list;
    s32 id;

    /* Evidence (class A2, sched2 barrier; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the original finishes every register save (s0
     * after ra) before the first call's arguments. */
    LOOP_BARRIER();
    list = (FightstgSlots *)heap_objects.find(0x14, -1, -1);
    if (list != NULL) {
        id = list->get_model_id(list, 0x10);
#ifdef PC_PORT
        id = def_enemy_camera_model(id); /* as fightstg_camera_get_preset's */
#endif
        if (id != 0) {
            fightstg_models.get(id);
            fightstg_camera_get_preset(obj, 0, fightstg_models.record_b->default_camera - 1);
        }
    }
    return &fightstg_camera_setting;
}

OBJECT_V0(FightstgCamera *) fightstg_camera_create(s32 layer_id) {
    FightstgCamera *obj = object_create(fightstg_camera_update, sizeof(FightstgCamera), 0, 0x12);

    obj->set = fightstg_camera_set;
    obj->move = fightstg_camera_move;
    obj->get_default = fightstg_camera_get_default;
    obj->layer_id = layer_id;
    obj->get_preset = fightstg_camera_get_preset;
    OBJECT_V0_RETURN(obj) /* PC_PORT: FINDINGS 8: callers use the object (v0) */
}

/* The party's command menu (on the object list as 0xE): base.step = phase, base.substep = the command
 * chosen, base.timer = its step. The menus write their choice to `choice` (-1 = none yet, -2 = back). */
typedef struct FightstgCommand {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 last; /* last command chosen */
    /* 0x54 */ s32 member_sel;
    /* 0x58 */ s32 choice; /* the menus' choice */
    /* 0x5C */ s32 command; /* 0 attack, 1 escape, 2 digivolve, 3 item, 4 technique, 5 switch, 6 team technique, -1 confused */
    /* 0x60 */ s32 arg;
    /* 0x64 */ s32 arg2;
    /* 0x68 */ s32 team_tech;
} FightstgCommand; /* size 0x6C */

/* fightstg_command_update's data block: its child objects. */
typedef struct FightstgCommandData {
    /* 0x00 */ void *children[7]; /* 0: HUD, 1: command menu, 2: model view, 3: sub-menu, 4: target,
                               * 6: fightstg_idle_camera_create's */
} FightstgCommandData; /* size 0x1C */

struct FightstgHud *fightstg_hud_create(void);
struct FightstgCommandMenu *fightstg_command_menu_create(s32 sel, s32 *result);
#ifdef PC_PORT
struct FightstgCommandMenu *fightstg_battle_menu_create(s32 sel, s32 *result);
#endif
struct FightstgPortrait *fightstg_portrait_create(void);
struct FightstgDigivolveMenu *fightstg_digivolve_menu_create(s32 *result);
struct FightstgItemMenu *fightstg_item_menu_create(s32 *result);
struct FightstgTechMenu *fightstg_tech_menu_create(s32 *result);
struct FightstgMemberMenu *fightstg_member_menu_create(s32 *result, s32 *cursor, s32 team);
struct FightstgSwitchMenu *fightstg_switch_menu_create(s32 *result);
struct FightstgSwitchMenu *fightstg_switch_menu_create_team(s32 *result, s32 *tech);
struct FightstgConfusedMenu *fightstg_confused_menu_create(s32 *result, Object *portrait, Object *idle_camera);
/* fightstg_80086A00.c defines it as void; it leaves its object in v0, which this file keeps (OBJECT_V0). */
void *fightstg_idle_camera_create(void);

void fightstg_command_update(FightstgCommand *obj, FightstgCommandData *data) {
    s32 i;
    Object *child;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->base.next_state(obj);
        return;
    case OBJECT_STATE_RUN:
#ifdef PC_PORT
        fightstg_bm_in_menu = obj->base.step == 2;
#endif
        switch (obj->base.step) {
        case 0:
        default:
            switch (obj->base.substep) {
            case 0:
            default:
#ifdef PC_PORT
                if (port_mod_bm_remember == 0) /* remember_cursor: the command menu's cursor is kept between turns */
#endif
                obj->last = 0;
                for (i = 1; i < 7; i++) {
                    child = data->children[i];
                    if (child != NULL) {
                        child->set_state(child, OBJECT_STATE_END);
                    }
                }
                obj->base.next_substep(obj);
                return;
            case 1:
                return;
            }
            return;
        case 1:
            if (data->children[0] == NULL) {
                data->children[0] = fightstg_hud_create();
            }
            obj->base.set_step(obj, 0);
            return;
        case 2:
            switch (obj->base.substep) {
            case 0:
            default:
                switch (obj->base.timer) {
                case 0:
                default:
                    if (data->children[0] == NULL) {
                        data->children[0] = fightstg_hud_create();
                    }
                    if (data->children[1] == NULL) {
#ifdef PC_PORT
                        if (port_mod_battle_menu != 0) { /* always, but under a replay script (game_mods.c) */
                            data->children[1] = fightstg_battle_menu_create(obj->last, &obj->choice);
                        } else
#endif
                        data->children[1] = fightstg_command_menu_create(obj->last, &obj->choice);
                    }
#ifdef PC_PORT
                    if (port_mod_battle_menu != 0) {
                        port_bm_log_last(obj->last);
                    }
#endif
#ifdef PC_PORT
                    /* The battle menu's HUD has no portrait window: it showed the acting Digimon that is already on
                     * the arena, as the largest, brightest box on screen, over the arena (the HUD redesign's critique:
                     * it outweighed the commands). The name box says whose turn it is. */
                    if (data->children[2] == NULL && port_mod_battle_menu == 0) {
#else
                    if (data->children[2] == NULL) {
#endif
                        data->children[2] = fightstg_portrait_create();
                    }
                    if (data->children[6] == NULL) {
                        data->children[6] = fightstg_idle_camera_create();
                    }
                    obj->choice = -1;
                    obj->base.next_timer(obj);
                case 1:
#ifdef PC_PORT
                    if (obj->choice == DEF_BLAST_COMMAND_ID) { /* the battle menu's Blast: no submenu, and the list's
                                                                 * cursor (last) keeps its own six entries */
                        obj->command = DEF_BLAST_COMMAND_ID;
                        obj->base.set_step(obj, 0);
                        return;
                    }
#endif
                    if (obj->choice != -1) {
                        obj->last = obj->choice;
                        switch (obj->choice) {
                        case 1:
                            obj->base.set_substep(obj, 3);
                            return;
                        case 2:
                            obj->base.set_substep(obj, 1);
                            return;
                        case 3:
                            obj->base.set_substep(obj, 4);
                            obj->member_sel = 0;
                            return;
                        case 4:
                            obj->base.set_substep(obj, 2);
                            return;
                        case 5:
#ifdef PC_PORT
                            if (def_flee_ask_on()) { /* Escape asks first (port/game/definitive_flee_ask.c) */
                                obj->base.set_substep(obj, 10);
                                return;
                            }
#endif
                            obj->command = 1;
                            obj->base.set_step(obj, 0);
                            return;
                        case 0:
                            obj->command = 0;
                            obj->base.set_step(obj, 0);
                            return;
                        }
                    }
                    return;
                }
                return;
            case 1:
                switch (obj->base.timer) {
                case 0:
                default:
                    obj->choice = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);
                    data->children[3] = fightstg_digivolve_menu_create(&obj->choice);
                    obj->base.next_timer(obj);
                case 1:
                    if (obj->choice == -2) {
                        obj->base.set_step(obj, 2);
                    } else if (obj->choice != -1) {
                        obj->command = 2;
                        obj->arg = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);
                        obj->arg2 = obj->choice;
                        obj->base.set_step(obj, 0);
                    }
                    return;
                }
                return;
            case 2:
                switch (obj->base.timer) {
                case 0:
                default:
                    data->children[3] = fightstg_item_menu_create(&obj->choice);
                    obj->base.next_timer(obj);
                case 1:
                    if (obj->choice == -2) {
                        obj->base.set_step(obj, 2);
                    } else if (obj->choice != -1) {
                        obj->command = 3;
                        obj->arg = obj->choice;
#ifdef PC_PORT
                        port_bm_remember(1, gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]), obj->choice);
#endif
                        obj->base.set_step(obj, 0);
                    }
                    return;
                }
                return;
            case 3:
                switch (obj->base.timer) {
                case 0:
                default:
                    data->children[3] = fightstg_tech_menu_create(&obj->choice);
                    obj->base.next_timer(obj);
                case 1:
                    if (obj->choice == -2) {
                        obj->base.set_step(obj, 2);
                    } else if (obj->choice != -1) {
                        obj->command = 4;
                        obj->arg = obj->choice;
#ifdef PC_PORT
                        port_bm_remember(0, gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]), obj->choice);
#endif
                        obj->base.set_step(obj, 0);
                    }
                    return;
                }
                return;
            case 4:
                switch (obj->base.timer) {
                case 0:
                default:
                    data->children[3] = fightstg_member_menu_create(&obj->choice, &obj->member_sel, 1);
                    obj->base.next_timer(obj);
                case 1:
                    switch (obj->choice) {
                    case -1:
                        break;
                    case -2:
                        obj->base.set_step(obj, 2);
                        break;
                    default:
                        obj->arg = obj->choice;
                        data->children[4] = fightstg_switch_menu_create_team(&obj->choice, &obj->team_tech);
                        obj->base.next_timer(obj);
                        break;
                    }
                    return;
                case 2:
                    if (obj->choice == -2) {
                        obj->base.set_timer(obj, 0);
                    } else if (obj->choice != -1) {
                        if (obj->choice & 0xF) {
                            obj->command = 6;
                            obj->arg2 = obj->choice >> 4;
                            obj->base.set_step(obj, 0);
                            obj->member_sel = 0;
                        } else {
                            obj->command = 5;
                            obj->arg2 = obj->choice >> 4;
                            obj->base.set_step(obj, 0);
                            obj->member_sel = 0;
                        }
                    }
                    return;
                }
                return;
#ifdef PC_PORT
            case 10: /* Escape's question: run (the game's escape command), or back to the menu as a submenu's back */
                switch (def_flee_ask_frame()) {
                case 1:
                    obj->command = 1;
                    obj->base.set_step(obj, 0);
                    break;
                case -1:
                    obj->base.set_step(obj, 2);
                    break;
                }
                return;
#endif
            }
            return;
        case 3:
            switch (obj->base.timer) {
            case 0:
            default:
                if (data->children[2] == NULL) {
                    data->children[2] = fightstg_portrait_create();
                }
                if (data->children[6] == NULL) {
                    data->children[6] = fightstg_idle_camera_create();
                }
                if (data->children[1] == NULL) {
                    data->children[1] = fightstg_confused_menu_create(&obj->choice, data->children[2], data->children[6]);
                }
                obj->choice = -1;
                obj->base.next_timer(obj);
            case 1:
                if (obj->choice != -1) {
                    if (obj->choice == 0) {
                        obj->command = 0;
                        obj->base.set_step(obj, 0);
                    } else {
                        obj->command = -1;
                        obj->base.set_step(obj, 0);
                    }
                }
                return;
            }
            return;
        case 4:
            switch (obj->base.timer) {
            case 0:
            default:
                if (data->children[6] == NULL) {
                    data->children[6] = fightstg_idle_camera_create();
                }
                if (data->children[3] == NULL) {
                    data->children[3] = fightstg_member_menu_create(&obj->choice, &obj->member_sel, 0);
                }
                obj->base.next_timer(obj);
            case 1:
                if (obj->choice != -1) {
                    obj->arg = obj->choice;
                    data->children[4] = fightstg_switch_menu_create(&obj->choice);
                    obj->base.next_timer(obj);
                }
                return;
            case 2:
                switch (obj->choice) {
                case -1:
                    break;
                case -2:
                    obj->base.set_timer(obj, 0);
                    break;
                default:
                    if (!(obj->choice & 0xF)) {
                        obj->command = 5;
                        obj->arg2 = obj->choice >> 4;
                        obj->base.set_step(obj, 0);
                        obj->member_sel = 0;
                    }
                    break;
                }
                return;
            }
            return;
        }
        return;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        return;
    }
}

OBJECT_V0(FightstgCommand *) fightstg_command_create(void) {
    /* PC_PORT: FINDINGS 8: callers use the object (v0) */
    OBJECT_V0_TAIL(object_create(fightstg_command_update, sizeof(FightstgCommand), sizeof(FightstgCommandData), 0xE))
}

void fightstg_command_set_phase(s32 phase) {
    Object *obj = heap_objects.find(0xE, -1, -1);

    if (obj != NULL && obj->state == OBJECT_STATE_RUN) {
        obj->set_step(obj, phase);
    }
}

s32 fightstg_command_is_open(void) {
    return heap_objects.find(0xE, -1, -1)->step != 0;
}

/* Draws the fade: a full-screen (320x240) quad, additive (abr 1), each channel `level`. */
void fightstg_flash_draw(FightstgFlash *obj) {
    POLY_F4 *poly;
    DR_TPAGE *tpage;
    u32 *ot;
    GfxLayer *layer;

    layer = gfx_module.funcs.get_layer(0x1006);
    ot = layer->get_ot_entry(layer, 0);
    poly = gfx_module.funcs.get_packet();
    tpage = (DR_TPAGE *)(poly + 1);
    setRGB0(poly, obj->level, obj->level, obj->level);
    setPolyF4(poly);
    setSemiTrans(poly, 1);
    poly->x3 = poly->x1 = 320;
    poly->x2 = poly->x0 = 0;
    poly->y1 = poly->y0 = 0;
    poly->y3 = poly->y2 = 240;
    addPrim(ot, poly);
    SetDrawTPage(tpage, 0, 1, getTPage(1, 1, 0, 0));
    addPrim(ot, tpage);
    tpage++;
    gfx_module.funcs.set_packet(tpage);
}

void fightstg_flash_update(FightstgFlash *obj) {
    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->base.next_state(obj);
        obj->level = 0;
        /* fallthrough */
    case OBJECT_STATE_RUN:
        if (obj->base.step == 0) {
            obj->level += obj->step * gfx_module.funcs.get_frame_ticks();
            if (obj->level >= 0xFF) {
                obj->level = 0xFF;
                obj->base.next_step(obj);
            }
        }
        break;
    case OBJECT_STATE_DONE:
        obj->level -= obj->step * gfx_module.funcs.get_frame_ticks();
        if (obj->level < 0) {
            obj->level = 0;
            obj->base.set_state(obj, OBJECT_STATE_END);
        }
        break;
    case OBJECT_STATE_END:
        break;
    }
    fightstg_flash_draw(obj);
}

void fightstg_flash_end(FightstgFlash *obj, s32 frames) {
    obj->step = 0xFF / frames;
    obj->base.set_state(obj, OBJECT_STATE_DONE);
}

FightstgFlash *fightstg_flash_create(s32 frames) {
    FightstgFlash *obj = object_new(fightstg_flash_update, sizeof(FightstgFlash), 0);

    obj->step = 0xFF / frames;
    return obj;
}

/* An HP bar: moves from hp_from to hp_to in `duration` frames along a sine curve. */
typedef struct FightstgHudBar {
    /* 0x00 */ s32 hp_from; /* HP moved from */
    /* 0x04 */ s32 hp_to; /* HP moved to */
    /* 0x08 */ s32 hp; /* HP shown */
    /* 0x0C */ s16 moving;
    /* 0x0E */ s16 member; /* member shown (fightstg_battle.state) */
    /* 0x10 */ s16 frames;
    /* 0x12 */ s16 duration;
} FightstgHudBar; /* size 0x14 */

/* The battle HUD (fightstg_hud_update): the acting members' names and HP bars. */
typedef struct FightstgHud {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 named[2]; /* per side: the member whose name is shown */
    /* 0x58 */ FightstgHudBar bars[2]; /* per side */
    /* 0x80 */ s32 update_time; /* frames since the last HP update */
    /* 0x84 */ s32 update_frames; /* frames between HP updates */
#ifdef PC_PORT
    s8 mod_sign[2][3]; /* last shown sign (-1, 0, 1) of modifiers[0..2] */
    u8 mod_elem[2];    /* last shown def_resist_down_families per side */
#endif
} FightstgHud; /* size 0x88 */

/* fightstg_hud_update's data block: its message windows. */
typedef struct FightstgHudData {
    /* 0x00 */ MessageWindow *party_name; /* the party member's name */
    /* 0x04 */ MessageWindow *enemy_name; /* the enemy's name */
    /* 0x08 */ MessageWindow *slash;
    /* 0x0C */ MessageWindow *max_hp; /* max HP */
    /* 0x10 */ MessageWindow *hp;
#ifdef PC_PORT
    MessageWindow *stat_mod[2][3]; /* Attack, Defense, Speed buff/debuff icons */
    MessageWindow *elem_mod[2][DEF_FAMILIES];   /* Resist Down: the lowered family's picture, */
    MessageWindow *elem_arrow[2][DEF_FAMILIES]; /* and the red arrow over it */
#endif
} FightstgHudData; /* size 0x14 */

#ifdef PC_PORT
static void fightstg_hud_draw_mods(FightstgHud *obj, FightstgHudData *data);
#endif

void fightstg_hud_update_hp(FightstgHud *obj, FightstgHudData *data);

void fightstg_hud_update_hp(FightstgHud *obj, FightstgHudData *data) {
    FightstgMember *row;
    s32 i;
    s32 j;
    s32 hp;

    obj->update_time += gfx_module.funcs.get_frame_ticks();
    for (i = 0; i < 2; i++) {
        row = fightstg_battle.state.members[i];
        if (obj->bars[i].member != fightstg_battle.state.current[i]) {
            obj->bars[i].hp_from = row[fightstg_battle.state.current[i]].hp;
            obj->bars[i].hp_to = row[fightstg_battle.state.current[i]].hp;
            obj->bars[i].hp = row[fightstg_battle.state.current[i]].hp;
            obj->bars[i].moving = 0;
            obj->bars[i].member = fightstg_battle.state.current[i];
        }
    }
    if (obj->update_time > obj->update_frames) {
        obj->update_time -= obj->update_frames;
        for (j = 0; j < 2; j++) {
            row = fightstg_battle.state.members[j];
            hp = row[fightstg_battle.state.current[j]].hp;
            if (hp <= 0) {
                hp = 0;
            }
            if (row[fightstg_battle.state.current[j]].max_hp < hp) {
                hp = row[fightstg_battle.state.current[j]].max_hp;
            }
            if (hp != obj->bars[j].hp_to) {
                obj->bars[j].hp_to = hp;
                obj->bars[j].moving = 1;
                obj->bars[j].frames = 0;
                obj->bars[j].duration = 30;
                obj->bars[j].hp_from = obj->bars[j].hp;
            }
        }
    }
}

void fightstg_hud_move_bar(FightstgHudBar *bar) {
    s32 t;

    if (bar->moving != 0) {
        bar->frames += gfx_module.funcs.get_frame_ticks();
        if (bar->frames >= bar->duration) {
            bar->moving = 0;
            bar->hp_from = bar->hp = bar->hp_to;
        } else {
            t = rsin((bar->frames << 10) / bar->duration) * bar->duration / 4096;
            bar->hp = bar->hp_from + (bar->hp_to - bar->hp_from) * t / bar->duration;
        }
    }
}

void fightstg_hud_update_names(FightstgHud *obj, FightstgHudData *data) {
    FightstgEnemyRecord *digimon;
    s32 i;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
#endif

    if (fightstg_battle.state.current[0] != obj->named[0]) {
        if (data->party_name == NULL) {
#ifdef PC_PORT
            data->party_name = message_create_window(0x1005, 1, 0xAE + wide_pad, 0x15);
#else
            data->party_name = message_create_window(0x1005, 1, 0xAE, 0x15);
#endif
        }
        data->party_name->set_text(data->party_name,
            gamestate_data.digimon[gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0])].record.name, -1);
    }
    if (fightstg_battle.state.current[1] != obj->named[1]) {
        digimon = fightstg_enemy_records.get(records_state.enemies[fightstg_battle.state.current[1]].digimon);
        if (data->enemy_name == NULL) {
#ifdef PC_PORT
            data->enemy_name = message_create_window(0x1005, 1, 0x11 - wide_pad, 0x15);
#else
            data->enemy_name = message_create_window(0x1005, 1, 0x11, 0x15);
#endif
        }
        if (digimon != NULL) {
#ifdef PC_PORT
            const unsigned char *rare = def_hunt_foe_name(digimon->id, 0); /* a Rare Target: "Datamon IV" */

            if (rare != NULL) {
                data->enemy_name->set_text(data->enemy_name, (u8 *)rare, -1);
            } else
#endif
            data->enemy_name->set_text(data->enemy_name, cdload_module.files.get_file(records_language + 0x4E), digimon->name);
        }
    }
    for (i = 0; i < 2; i++) {
        obj->named[i] = fightstg_battle.state.current[i];
    }
}

/* The HUD's HP bars (quads drawn by fightstg_battle.draw_quad) and their colours. */
extern DVECTOR fightstg_hud_bars[2][4]; /* per side; the bar's right (party) or left end moves */
extern CVECTOR fightstg_hud_bar_colors[2][2]; /* above a quarter of max HP, else below */
extern DVECTOR fightstg_hud_gauge[4];
extern CVECTOR fightstg_hud_gauge_colors[4];
extern s16 fightstg_hud_icon_x[2][3]; /* per side and member: x of its icon */

void fightstg_hud_draw(FightstgHud *obj, FightstgHudData *data) {
    Sprite sprite;
    CVECTOR colors[4];
    s32 *bank;
    FightstgMember *member;
    s32 side;
    s32 i;
    s32 j;
    s32 w;
    s32 frame;
    s32 k;
    s32 idx;
    s32 x;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
    DVECTOR bar[4];
    DVECTOR gauge[4];
#endif

    bank = cdload_module.get_subfile_by_id(0x4550000);
    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    sprite.set_vram_pos(0x200, 0);
#ifdef PC_PORT
    sprite.draw(bank, 0, 8 - wide_pad, 0xF);
    sprite.draw(bank, 1, 0xA1 + wide_pad, 0xF);
#else
    sprite.draw(bank, 0, 8, 0xF);
    sprite.draw(bank, 1, 0xA1, 0xF);
#endif
    fightstg_hud_update_names(obj, data);
    member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
    if (data->hp == NULL) {
#ifdef PC_PORT
        data->hp = message_create_window(0x1005, 3, 0x10B + wide_pad, 0x1A);
#else
        data->hp = message_create_window(0x1005, 3, 0x10B, 0x1A);
#endif
    }
    data->hp->set_line_number(data->hp, 0, obj->bars[0].hp);
    data->hp->measure(data->hp, 1);
    if (data->max_hp == NULL) {
#ifdef PC_PORT
        data->max_hp = message_create_window(0x1005, 3, 0x12E + wide_pad, 0x1A);
#else
        data->max_hp = message_create_window(0x1005, 3, 0x12E, 0x1A);
#endif
    }
    data->max_hp->set_line_number(data->max_hp, 0, member->max_hp);
    data->max_hp->measure(data->max_hp, 1);
    for (side = 0; side < 2; side++) {
        member = &fightstg_battle.state.members[side][fightstg_battle.state.current[side]];
        w = (obj->bars[side].hp << 7) / member->max_hp;
        if (obj->bars[side].hp > 0 && w < 4) {
            w = 3;
        }
        if (side == 0) {
            fightstg_hud_bars[0][1].vx = fightstg_hud_bars[0][0].vx + w;
            fightstg_hud_bars[0][3].vx = fightstg_hud_bars[0][2].vx + w;
        } else {
            fightstg_hud_bars[side][1].vx = fightstg_hud_bars[side][0].vx - w;
            fightstg_hud_bars[side][3].vx = fightstg_hud_bars[side][2].vx - w;
        }
        if ((member->max_hp >> 2) < obj->bars[side].hp) {
            colors[0] = colors[2] = fightstg_hud_bar_colors[0][0];
            colors[1] = colors[3] = fightstg_hud_bar_colors[0][1];
        } else {
            colors[0] = colors[2] = fightstg_hud_bar_colors[1][0];
            colors[1] = colors[3] = fightstg_hud_bar_colors[1][1];
        }
#ifdef PC_PORT
        for (k = 0; k < 4; k++) {
            bar[k] = fightstg_hud_bars[side][k];
            bar[k].vx += side == 0 ? wide_pad : -wide_pad;
        }
        fightstg_battle.draw_quad(0x1005, 1, bar, colors);
#else
        fightstg_battle.draw_quad(0x1005, 1, fightstg_hud_bars[side], colors);
#endif
    }
    frame = (gfx_module.funcs.get_time() >> 2) & 3;
    idx = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);
#ifdef PC_PORT
    if (port_mod_battle_menu != 0 && port_frames - fightstg_bmenu_seen <= 2) {
        goto blast_bar_done;
    }
    sprite.draw(bank, 0x1E, 0x104 + wide_pad, 0x3D);
    if (records_state.gauges[idx] < 1000) {
        fightstg_hud_gauge[2].vx = fightstg_hud_gauge[0].vx = records_state.gauges[idx] / 25 + 0x109;
        for (k = 0; k < 4; k++) {
            gauge[k] = fightstg_hud_gauge[k];
            gauge[k].vx += wide_pad;
        }
        fightstg_battle.draw_quad(0x1005, 1, gauge, fightstg_hud_gauge_colors);
    }
    if (records_state.gauges[idx] < 1000) {
        sprite.draw(bank, frame + 0x33, 0x109 + wide_pad, 0x3E);
    } else {
        sprite.draw(bank, frame + 0x37, 0x109 + wide_pad, 0x3E);
    }
    sprite.draw(bank, 0xB, 0x104 + wide_pad, 0x3D);
blast_bar_done:
#else
    sprite.draw(bank, 0x1E, 0x104, 0x3D);
    if (records_state.gauges[idx] < 1000) {
        fightstg_hud_gauge[2].vx = fightstg_hud_gauge[0].vx = records_state.gauges[idx] / 25 + 0x109;
        fightstg_battle.draw_quad(0x1005, 1, fightstg_hud_gauge, fightstg_hud_gauge_colors);
    }
    if (records_state.gauges[idx] < 1000) {
        sprite.draw(bank, frame + 0x33, 0x109, 0x3E);
    } else {
        sprite.draw(bank, frame + 0x37, 0x109, 0x3E);
    }
    sprite.draw(bank, 0xB, 0x104, 0x3D);
#endif
#ifdef PC_PORT
    fightstg_hud_draw_mods(obj, data);
#endif
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            if (fightstg_battle.state.type >= 4 && j > 0 && i > 0) {
                return;
            }
            member = &fightstg_battle.state.members[i][j];
            if (member->digimon != 0) {
                if (member->status != 0) {
                    k = 3;
                } else if (member->hp == 0) {
                    k = 2;
                } else {
                    k = member->hp != member->max_hp;
                }
#ifdef PC_PORT
                sprite.draw(bank, (fightstg_battle.state.current[i] == j ? 0xC : 0x3C) + k,
                              fightstg_hud_icon_x[i][j] + (i == 0 ? wide_pad : -wide_pad), 0x30);
#else
                sprite.draw(bank, (fightstg_battle.state.current[i] == j ? 0xC : 0x3C) + k,
                              fightstg_hud_icon_x[i][j], 0x30);
#endif
            }
        }
    }
}

void fightstg_hud_update(FightstgHud *obj, FightstgHudData *data) {
    FightstgMember (*members)[3];
    s32 i;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
#endif

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->named[1] = -1;
        obj->named[0] = -1;
        fightstg_hud_update_names(obj, data);
#ifdef PC_PORT
        data->slash = message_create_window(0x1005, 3, 0x10C + wide_pad, 0x1A);
        data->slash->set_text(data->slash, cdload_module.files.get_file(records_language + 0x7F), 0x10);
        members = fightstg_battle.state.members;
        data->hp = message_create_window(0x1005, 3, 0x10B + wide_pad, 0x1A);
        data->hp->set_line_number(data->hp, 0, members[0][fightstg_battle.state.current[0]].hp);
        data->hp->measure(data->hp, 1);
        data->max_hp = message_create_window(0x1005, 3, 0x12E + wide_pad, 0x1A);
#else
        data->slash = message_create_window(0x1005, 3, 0x10C, 0x1A);
        data->slash->set_text(data->slash, cdload_module.files.get_file(records_language + 0x7F), 0x10);
        members = fightstg_battle.state.members;
        data->hp = message_create_window(0x1005, 3, 0x10B, 0x1A);
        data->hp->set_line_number(data->hp, 0, members[0][fightstg_battle.state.current[0]].hp);
        data->hp->measure(data->hp, 1);
        data->max_hp = message_create_window(0x1005, 3, 0x12E, 0x1A);
#endif
        data->max_hp->set_line_number(data->max_hp, 0, members[0][fightstg_battle.state.current[0]].max_hp);
        data->max_hp->measure(data->max_hp, 1);
        for (i = 0; i < 2; i++) {
            obj->bars[i].hp_from = members[i][fightstg_battle.state.current[i]].hp;
            obj->bars[i].hp_to = members[i][fightstg_battle.state.current[i]].hp;
            obj->bars[i].hp = members[i][fightstg_battle.state.current[i]].hp;
            obj->bars[i].moving = 0;
            obj->bars[i].member = fightstg_battle.state.current[i];
            obj->bars[i].frames = 0;
            obj->bars[i].duration = 0;
        }
        obj->update_time = 0;
        obj->update_frames = 8;
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        fightstg_hud_update_hp(obj, data);
        fightstg_hud_move_bar(&obj->bars[0]);
        fightstg_hud_move_bar(&obj->bars[1]);
        fightstg_hud_draw(obj, data);
        break;
    case OBJECT_STATE_DONE:
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_END:
        break;
    }
}

FightstgHud *fightstg_hud_create(void) {
    return object_new(fightstg_hud_update, sizeof(FightstgHud), sizeof(FightstgHudData));
}

void fightstg_command_menu_open(FightstgCommandMenu *obj) {
    FightstgCommandMenuData *data = (FightstgCommandMenuData *)obj->base.children;
    FightstgMember *member;
    u8 *text;
    s32 i;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
#endif

    if (data->windows[0] == NULL) {
        text = cdload_module.files.get_file(records_language + 0x7F);
        for (i = 0; i < 6; i++) {
#ifdef PC_PORT
            data->windows[i] = message_create_window(0x1005, 1, 0x24 - wide_pad, i * 19 + 0x6D);
#else
            data->windows[i] = message_create_window(0x1005, 1, 0x24, i * 19 + 0x6D);
#endif
            data->windows[i]->set_text(data->windows[i], text, i + 1);
        }
        member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
        if (member->status & 8) {
            for (i = 0; i < 3; i++) {
                data->windows[0]->set_palette(data->windows[i], 7);
            }
        }
        if (member->status & 0x20) {
            data->windows[0]->set_palette(data->windows[2], 7);
        }
#ifdef PC_PORT
        if (def_super_tutorial_battle()) {
            data->windows[0]->set_palette(data->windows[5], 7);
        }
#endif
    }
}

void fightstg_command_menu_update(FightstgCommandMenu *obj, FightstgCommandMenuData *data) {
    FightstgMember *member;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
#ifdef PC_PORT
    {
        FightstgCursorParams params = fightstg_command_menu_cursor;

        params.x -= port_video_widescreen_pad();
        data->cursor = fightstg_cursor_create(&params);
    }
#else
        data->cursor = fightstg_cursor_create(&fightstg_command_menu_cursor);
#endif
        data->cursor->sel = obj->sel;
        fightstg_command_menu_open(obj);
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        /* Evidence (class B, register priority only; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the original keeps the state in s2
         * through case 1 (its 1 is stored to unk_54); without the block only registers differ, same layout. */
        LOOP_BLOCK(
                if (pad_state.get_pressed(0) & 0x2000) {
                    sound_module.play(0x4001C);
                    member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                    if ((member->status & 8) && data->cursor->sel < 3) {
                        break;
                    }
                    if ((member->status & 0x20) && data->cursor->sel == 2) {
                        break;
                    }
#ifdef PC_PORT
                    if (def_super_tutorial_battle() && data->cursor->sel == 5) {
                        break;
                    }
#endif
                    *obj->result = data->cursor->sel;
                    obj->base.set_state(obj, OBJECT_STATE_END);
                    data->cursor->locked = 1;
                }
        );
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

#ifdef PC_PORT
/* battle_menu (docs/LAUNCHER.md "Battle menu"): the command menu with one button per action instead of the list and
 * its cursor. It keeps the original's contract: *result is -1, then the chosen entry (0 Fight, 1 Tech, 2 Digivolve,
 * 3 Tag, 4 Item, 5 Run), and the same refusals (a Digimon with status 8 cannot Fight, Tech or Digivolve; with 0x20 it
 * cannot Digivolve). Each action is an icon window (the button's glyph, which dw3_button_glyph turns into the player's
 * own letter) and the entry's label window, placed where the button is on the player's pad. */
/* The actions: the game's six (0 Fight, 1 Tech, 2 Digivolve, 3 Tag, 4 Item, 5 Run), 6 Inspect and 7 Blast (definitive
 * combat; DEF_BLAST_COMMAND_ID is its result). */
#define FIGHTSTG_BMENU_ACTIONS 8
#define FIGHTSTG_BMENU_RUN 5
#define FIGHTSTG_BMENU_INSPECT 6
#define FIGHTSTG_BMENU_BLAST 7
typedef struct FightstgBattleMenuData {
    MessageWindow *icons[FIGHTSTG_BMENU_ACTIONS];
    MessageWindow *labels[FIGHTSTG_BMENU_ACTIONS];
    MessageWindow *inspect_names[7];  /* Inspect's panel: the element's picture, */
    MessageWindow *inspect_states[7]; /* and Weak / Resist / - / ??? (empty while the panel is closed) */
    MessageWindow *inspect_title;     /* the panel's title: the enemy's name */
    MessageWindow *legend[DEF_TRAITS]; /* the panel's legend: one window per trait of the enemy, two lines at most */
    MessageWindow *tempt;             /* the Blast's temptation (def_blast_tempt), above the cross; empty otherwise */
    MessageWindow *limit_break;       /* "LIMIT BREAK" badge when Chosen Evolution or Dark Galacticmon exceeds 999 */
    MessageWindow *notice;            /* why the grey Blast refuses the Chosen Evolution (def_chosen_refusal); empty */
} FightstgBattleMenuData; /* only pointers: the block is run and scanned as objects */

/* The menu's object: FightstgCommandMenu's own fields, then what the drawing and the animation need (never in the data
 * block). The layout is the pad's (2026-11-23 redesign, .plan/tasks/battle-hud-redesign.md): the four face buttons are
 * a diamond as on the pad, each a chip with the button's glyph and the command written out beside it in the callouts'
 * polygon capitals (def_poly_text: no texture, sharp at any scale), the word on the outside; the shoulders are a
 * separate group of tabs above it, the left ones on the left and the right ones on the right, L2/R2 above L1/R1 as on
 * the pad. Fight, the action of most turns, has the largest word and the warm plate; the Blast tab carries the gauge.
 * The animation: the tabs slide in (8 ticks), the pressed one nudges and lights while the others grey (7 ticks, then
 * the choice is given), a refused one shakes red. Per frame an action costs two polygon words, three or four flat
 * quads and one window move, no allocation. */
typedef struct FightstgBattleMenu {
    /* 0x00 */ Object base;
    /* 0x50 */ s32 sel;
    /* 0x54 */ s32 *result;
    /* 0x58 */ s32 age;     /* ticks since it opened */
    /* 0x5C */ s32 press;   /* the action being confirmed, -1: none */
    /* 0x60 */ s32 press_age;
    /* 0x64 */ s32 refused; /* the action last refused, -1: none */
    /* 0x68 */ s32 refused_age;
    /* Action 6 is Inspect (definitive combat): a button like the others, but it opens a panel instead of a result.
     * Action 7 is Blast: grey until the acting Digimon's gauge is full (def_blast_ready). */
    s16 x[FIGHTSTG_BMENU_ACTIONS];       /* the button chip's top-left corner, at rest */
    s16 y[FIGHTSTG_BMENU_ACTIONS];
    u8 grey[FIGHTSTG_BMENU_ACTIONS];     /* the Digimon cannot do it now */
    u8 chip_w[FIGHTSTG_BMENU_ACTIONS];   /* the chip: the glyph and its margin */
    u8 sym_w[FIGHTSTG_BMENU_ACTIONS];    /* the glyph's own width (centred in the chip) */
    u8 tint[FIGHTSTG_BMENU_ACTIONS];     /* the chip's colour: the Xbox letter's, or neutral */
    s16 word_x[FIGHTSTG_BMENU_ACTIONS];  /* from x: the word's left end */
    s16 word_w[FIGHTSTG_BMENU_ACTIONS];
    s16 word_size[FIGHTSTG_BMENU_ACTIONS]; /* 8.8 of the callouts' 16-pixel letters */
    s16 plate_x[FIGHTSTG_BMENU_ACTIONS]; /* from x: the plate behind chip and word */
    s16 plate_w[FIGHTSTG_BMENU_ACTIONS];
    s32 inspect; /* the Inspect panel is open */
    DefInspectInfo info; /* the panel's traits, read when it opens (nothing changes them while the menu is up) */
    s16 inspect_rows_y;  /* the grid's first line: under the traits row when there is one */
    s16 inspect_h;       /* the open panel's height */
    s16 panel_t;         /* the panel's animation: 0 closed .. FIGHTSTG_PANEL_OPEN open (walks toward `inspect`) */
    s16 info_blip;       /* ticks left of INFO's tab lighting as the panel leaves it or comes back */
    s16 legend_y[DEF_TRAITS]; /* each legend entry's first line */
    u8 queue[3];         /* the next turns (DEF_TURN_*), read once at the menu's creation: def_turn_queue rewrites */
    s32 queue_n;         /* fightstg_rules.stats */
    s32 tag_free; /* the Tag tab shows the free swap's picture (an enemy is open and the window's swap is unspent) */
    s32 tempt;    /* a full gauge at a critical moment: the Blast tab turns bright and shakes (def_blast_tempt) */
    s32 notice;   /* the refusal's box is open (above the menu) */
    u8 spot[FIGHTSTG_BMENU_ACTIONS]; /* where each action is drawn (fightstg_bmenu_place's index) */
    u8 at[FIGHTSTG_BMENU_ACTIONS];   /* and the action drawn at each spot */
} FightstgBattleMenu;

#define FIGHTSTG_BMENU_SLIDE 8
#define FIGHTSTG_BMENU_PRESS 7
#define FIGHTSTG_BMENU_SHAKE 14 /* long enough to be seen: a refusal is the menu's only error */
#define FIGHTSTG_BMENU_PANEL_DEPTH 1 /* the windows and the words draw at depth 0, over the plates */
#define FIGHTSTG_BMENU_PAL_GREY 7    /* the font's grey row (the list's refused entries) */
#define FIGHTSTG_BMENU_PAL_HOT 0     /* the pressed action keeps its own colours while the others grey */
#define FIGHTSTG_BMENU_FIGHT 0
#define FIGHTSTG_BMENU_TAG 3
#define FIGHTSTG_BMENU_INSPECT 6
#define FIGHTSTG_BMENU_BLINK 8 /* ticks of each look of the full Blast tab */
#define FIGHTSTG_BMENU_WIDE_SHIFT 110 /* % of the widescreen pad the block follows in widescreen: the owner's
                                       * walk of the range on screen, and 110 is the minimum where the words clear the
                                       * seam between the 4:3 buffer and the wide canvas (100..109 leave the band over
                                       * the commands) */

/* The places (4:3 screen), one compact block in the left middle (the owner, round 6: the v3 cut's placement, which he
 * prefers to the split layout's one along the bottom edge): the shoulders' two rows on top, L2/L1 at the block's left
 * and R2/R1 at its right, then the face diamond below them on a dark rhombus of its own, so it reads as the pad's
 * diamond and not as a list. The block spans x 6-157 and y 136-225: the arena's middle (the enemy) and the fighter on
 * the right stay clear, and the right middle, which the portrait window used, holds the Inspect panel and the
 * refusal's box. In each tab the chip is on the outside (where the button is on the pad) and the word inside; in the
 * diamond the chip is on the diamond and the word outside. The block is one piece in widescreen too: it does not
 * reach the wide screen's edges, so it keeps the 4:3 places. */
#define FIGHTSTG_BMENU_M 6         /* the left margin (the handhelds' overscan) */
#define FIGHTSTG_BMENU_R 152       /* the shoulder tabs' right edge: R2/R1's chips end here */
#define FIGHTSTG_BMENU_CX 64       /* the top and bottom chips' left: the diamond's middle column */
#define FIGHTSTG_BMENU_CY 193      /* the side chips' top */
#define FIGHTSTG_BMENU_STEP_X 19   /* chip to chip in the diamond */
#define FIGHTSTG_BMENU_STEP_Y 16
#define FIGHTSTG_BMENU_ROW2 136    /* L2 / R2 (above, as on the pad) */
#define FIGHTSTG_BMENU_ROW1 152    /* L1 / R1 */
#define FIGHTSTG_BMENU_CHIP 15
#define FIGHTSTG_BMENU_GAP 4       /* chip to word */
#define FIGHTSTG_BMENU_SIZE_TAB 160   /* 10 pixels */
#define FIGHTSTG_BMENU_SIZE_INSPECT 128 /* 8 pixels */
#define FIGHTSTG_BMENU_SIZE_FACE 160
#define FIGHTSTG_BMENU_SIZE_FIGHT 208 /* 13 pixels */
#define FIGHTSTG_BMENU_SKEW 3      /* the plates lean as the letters do */
#define FIGHTSTG_BMENU_WORD_W 70   /* a word's room, at any spot (a longer one is drawn smaller) */
#define FIGHTSTG_BMENU_WEST_W 34   /* but the left face's: between the left tabs' plates and its chip (RUN fits) */

/* The eight spots, by place on the pad (port_battle_menu_screen_place: 0 south, 1 west, 2 north, 3 east, 4 L1, 5 R1,
 * 6 L2, 7 R2). An action is drawn at the spot of the button it is mapped to (the owner, 2026-11-23: a remapped action
 * moves with its button; the chip shows that button's glyph). kind 0: a face chip with its word on the right; 1: a face
 * chip with its word on the left (the diamond's left arm); 2: a left shoulder tab (chip at the block's left edge, then
 * the word); 3: a right shoulder tab (word, then chip at the block's right edge). */
static const struct {
    s16 x, y;
    u8 kind;
} fightstg_bmenu_place[FIGHTSTG_BMENU_ACTIONS] = {
    { FIGHTSTG_BMENU_CX, FIGHTSTG_BMENU_CY + FIGHTSTG_BMENU_STEP_Y, 0 }, /* south: Fight by default */
    { FIGHTSTG_BMENU_CX - FIGHTSTG_BMENU_STEP_X, FIGHTSTG_BMENU_CY, 1 }, /* west: Run */
    { FIGHTSTG_BMENU_CX, FIGHTSTG_BMENU_CY - FIGHTSTG_BMENU_STEP_Y, 0 }, /* north: Digivolve */
    { FIGHTSTG_BMENU_CX + FIGHTSTG_BMENU_STEP_X, FIGHTSTG_BMENU_CY, 0 }, /* east: Tag */
    { FIGHTSTG_BMENU_M, FIGHTSTG_BMENU_ROW1, 2 },    /* L1: Item */
    { 0, FIGHTSTG_BMENU_ROW1, 3 },                   /* R1: Tech */
    { FIGHTSTG_BMENU_M, FIGHTSTG_BMENU_ROW2, 2 },    /* L2: Inspect */
    { 0, FIGHTSTG_BMENU_ROW2, 3 },                   /* R2: Blast */
};

/* The action's spot: its button's place, or its default button's when the place is not one of the eight. */
static s32 fightstg_bmenu_spot(s32 action) {
    static const u8 defaults[FIGHTSTG_BMENU_ACTIONS] = { 0, 5, 2, 3, 4, 1, 6, 7 }; /* game_mods.c's */
    s32 place = port_battle_menu_screen_place(action);

    return place >= 0 && place < FIGHTSTG_BMENU_ACTIONS ? place : defaults[action];
}

/* The command's word, in the language in use (capitals of the callouts' letters: accents in UTF-8). */
static const char *fightstg_bmenu_word(s32 action) {
    /* English only (the owner, 2026-11-23): these words are read at 320x240 in a fraction of a second, and a
     * translated set makes them change length, wrap and shrink per language - a UI/UX problem, not a translation one.
     * The game's own texts stay in the player's language; only the eight commands are fixed. */
    static const char *const words[FIGHTSTG_BMENU_ACTIONS] = {"FIGHT", "TECH",   "DIGIVOLVE", "TAG",
                                                              "BAG",   "RUN",    "INFO",      "BLAST"};

    return action >= 0 && action < FIGHTSTG_BMENU_ACTIONS ? words[action] : "BLAST";
}

/* Tag's picture while the swap is free (the enemy is open): the red disc after the word. */
static const u8 fightstg_bmenu_tag_free_glyph[3] = { 1, 0x2F, 0 };

/* The game's text for the button of an action: direct letter from the configured place if mapped,
 * else the face buttons' own glyphs (01 xx, the symbol), else its name in the main font (L1, R1...). */
static void fightstg_battle_menu_icon_text(u8 *buf, s32 action) {
    char letter = port_battle_menu_letter(action);
    /* The symbol follows pad.map's row at the action's place, as the letter does; the input bit is unrelated. */
    s32 bit = port_battle_menu_symbol_bit(action);
    u8 code = port_face_symbol_code(bit);

    if (letter >= 'A' && letter <= 'Z') {
        buf[0] = (u8)(0x0E + (letter - 'A'));
        buf[1] = 0;
        return;
    }
    if (code != 0) {
        extern int port_script_active;
        if (!port_script_active) {
            if (code == 0x2F) {
                code = 0x34;
            } else if (code == 0x34) {
                code = 0x2F;
            }
        }
        buf[0] = 1;
        buf[1] = code;
        buf[2] = 0;
        return;
    }
    buf[0] = (bit == 10 || bit == 8) ? 0x19 : (bit == 11 || bit == 9) ? 0x1F : 0x25; /* L, R, else X */
    buf[1] = (bit == 10 || bit == 11) ? 5 : (bit == 8 || bit == 9) ? 6 : 0;
    buf[2] = 0;
}

/* A flat quad on the menu's layer, behind the text: `abr` 2 subtracts the colour from the scene (the dark panel, which
 * keeps the text readable over the bright arena), 1 adds it (a press's glow). */
static void fightstg_battle_menu_quad(s32 x, s32 y, s32 w, s32 h, s32 abr, u8 r, u8 g, u8 b) {
    POLY_F4 *poly;
    DR_TPAGE *tpage;
    GfxLayer *layer = gfx_module.funcs.get_layer(0x1005);
    u32 *ot;

    if (layer == NULL) {
        return;
    }
    ot = layer->get_ot_entry(layer, FIGHTSTG_BMENU_PANEL_DEPTH);
    poly = gfx_module.funcs.get_packet();
    tpage = (DR_TPAGE *)(poly + 1);
    setRGB0(poly, r, g, b);
    setPolyF4(poly);
    setSemiTrans(poly, 1);
    poly->x0 = poly->x2 = x;
    poly->x1 = poly->x3 = x + w;
    poly->y0 = poly->y1 = y;
    poly->y2 = poly->y3 = y + h;
    addPrim(ot, poly);
    SetDrawTPage(tpage, 0, 1, getTPage(1, abr, 0, 0));
    addPrim(ot, tpage);
    tpage++;
    gfx_module.funcs.set_packet(tpage);
}

#ifdef PC_PORT
/* The acting member's buffs and debuffs, on a strip just under its HUD frame (clear of the frame's
 * edge and of the member squares' tab), from the frame's outer end inwards. A stat is its letter and an arrow, green
 * up or red down (A Attack, D Defense, S Speed); a lowered resist is the element family's own picture (the one the
 * bestiary and Inspect show) with the red arrow over its corner, so it reads as "this element, down" and never as a
 * stat. One picture a family: Water and Ice lowered together are one mark. */
#define FIGHTSTG_HUD_MOD_Y 59
#define FIGHTSTG_HUD_MOD_STAT_STEP 17
#define FIGHTSTG_HUD_MOD_ELEM_STEP 19 /* the 15-pixel picture, the arrow's overhang and a gap */
#define FIGHTSTG_HUD_MOD_ARROW_DX 9   /* the arrow over the picture's right side */
#define FIGHTSTG_HUD_MOD_ENEMY_X 18   /* the enemy's strip starts under its name box */
#define FIGHTSTG_HUD_MOD_PARTY_X 256  /* the party's ends left of the Blast gauge (x 262..305 on this line) */

static void fightstg_hud_hide_mods(FightstgHud *obj, FightstgHudData *data, s32 side) {
    s32 k;

    for (k = 0; k < 3; k++) {
        if (data->stat_mod[side][k] != NULL) {
            data->stat_mod[side][k]->set_visible(data->stat_mod[side][k], 0);
        }
        obj->mod_sign[side][k] = 0;
    }
    for (k = 0; k < DEF_FAMILIES; k++) {
        if (data->elem_mod[side][k] != NULL) {
            data->elem_mod[side][k]->set_visible(data->elem_mod[side][k], 0);
            data->elem_arrow[side][k]->set_visible(data->elem_arrow[side][k], 0);
        }
    }
    obj->mod_elem[side] = 0;
}

static void fightstg_hud_draw_mods(FightstgHud *obj, FightstgHudData *data) {
    static const u8 stat_glyph[3] = { 0x0E, 0x11, 0x20 }; /* A (Attack), D (Defense), S (Speed) */
    static const u8 down[3] = { 1, 0x3A, 0 };
    s32 side;

    if (!def_on()) {
        fightstg_hud_hide_mods(obj, data, 0);
        fightstg_hud_hide_mods(obj, data, 1);
        return;
    }
    for (side = 0; side < 2; side++) {
        s32 slot = fightstg_battle.state.current[side];
        FightstgMember *m = &fightstg_battle.state.members[side][slot];
        s8 signs[3];
        u8 mask = (u8)def_resist_down_families(side, slot);
        s32 changed = (mask != obj->mod_elem[side]);
        s32 total_w = 0;
        s32 k;
        s32 x0;
        s32 cur_x;

        for (k = 0; k < 3; k++) {
            signs[k] = m->modifiers[k] > 0 ? 1 : (m->modifiers[k] < 0 ? -1 : 0);
            changed |= signs[k] != obj->mod_sign[side][k];
            total_w += signs[k] != 0 ? FIGHTSTG_HUD_MOD_STAT_STEP : 0;
        }
        for (k = 0; k < DEF_FAMILIES; k++) {
            total_w += (mask >> k) & 1 ? FIGHTSTG_HUD_MOD_ELEM_STEP : 0;
        }
        if (total_w == 0) {
            if (changed) {
                fightstg_hud_hide_mods(obj, data, side);
            }
            continue;
        }
        total_w -= 2; /* the last mark's gap */
        {
            s32 wide_pad = port_video_widescreen_pad();

            x0 = side == 1 ? FIGHTSTG_HUD_MOD_ENEMY_X - wide_pad : FIGHTSTG_HUD_MOD_PARTY_X + wide_pad - total_w;
        }
        fightstg_battle_menu_quad(x0 - 3, FIGHTSTG_HUD_MOD_Y - 1, total_w + 6, 15, 2, 150, 140, 100);
        if (!changed) {
            continue;
        }
        cur_x = x0;
        for (k = 0; k < 3; k++) {
            if (signs[k] != 0) {
                u8 buf[4];
                if (data->stat_mod[side][k] == NULL) {
                    data->stat_mod[side][k] = message_create_window(0x1005, 1, cur_x, FIGHTSTG_HUD_MOD_Y);
                }
                buf[0] = stat_glyph[k];
                buf[1] = 1;
                buf[2] = signs[k] > 0 ? 0x39 : 0x3A;
                buf[3] = 0;
                data->stat_mod[side][k]->set_text(data->stat_mod[side][k], buf, -1);
                data->stat_mod[side][k]->set_palette(data->stat_mod[side][k], signs[k] > 0 ? 4 : 5);
                data->stat_mod[side][k]->set_pos(data->stat_mod[side][k], cur_x, FIGHTSTG_HUD_MOD_Y);
                data->stat_mod[side][k]->set_visible(data->stat_mod[side][k], 1);
                cur_x += FIGHTSTG_HUD_MOD_STAT_STEP;
            } else if (data->stat_mod[side][k] != NULL) {
                data->stat_mod[side][k]->set_visible(data->stat_mod[side][k], 0);
            }
            obj->mod_sign[side][k] = signs[k];
        }
        for (k = 0; k < DEF_FAMILIES; k++) {
            if ((mask >> k) & 1) {
                if (data->elem_mod[side][k] == NULL) {
                    /* the picture first: the arrow, created after it, is drawn over it */
                    data->elem_mod[side][k] = message_create_window(0x1005, 1, cur_x, FIGHTSTG_HUD_MOD_Y);
                    data->elem_mod[side][k]->set_text(data->elem_mod[side][k], (u8 *)def_family_icon(k, 0), -1);
                    data->elem_arrow[side][k] = message_create_window(0x1005, 1, cur_x, FIGHTSTG_HUD_MOD_Y);
                    data->elem_arrow[side][k]->set_text(data->elem_arrow[side][k], (u8 *)down, -1);
                    data->elem_arrow[side][k]->set_palette(data->elem_arrow[side][k], 5);
                }
                data->elem_mod[side][k]->set_pos(data->elem_mod[side][k], cur_x, FIGHTSTG_HUD_MOD_Y);
                data->elem_mod[side][k]->set_visible(data->elem_mod[side][k], 1);
                data->elem_arrow[side][k]->set_pos(data->elem_arrow[side][k], cur_x + FIGHTSTG_HUD_MOD_ARROW_DX, FIGHTSTG_HUD_MOD_Y + 2);
                data->elem_arrow[side][k]->set_visible(data->elem_arrow[side][k], 1);
                cur_x += FIGHTSTG_HUD_MOD_ELEM_STEP;
            } else if (data->elem_mod[side][k] != NULL) {
                data->elem_mod[side][k]->set_visible(data->elem_mod[side][k], 0);
                data->elem_arrow[side][k]->set_visible(data->elem_arrow[side][k], 0);
            }
        }
        obj->mod_elem[side] = mask;
    }
}
#endif

/* A plate: the quad of fightstg_battle_menu_quad with its top edge `skew` pixels right of its foot, as the letters
 * lean. `abr` -1 is opaque (the chips: the glyph must read over any arena). */
static void fightstg_battle_menu_plate(s32 x, s32 y, s32 w, s32 h, s32 skew, s32 abr, u8 r, u8 g, u8 b) {
    POLY_F4 *poly;
    DR_TPAGE *tpage;
    GfxLayer *layer = gfx_module.funcs.get_layer(0x1005);

    if (layer == NULL) {
        return;
    }
    poly = gfx_module.funcs.get_packet();
    tpage = (DR_TPAGE *)(poly + 1);
    setRGB0(poly, r, g, b);
    setPolyF4(poly);
    setSemiTrans(poly, abr >= 0);
    setXY4(poly, x + skew, y, x + w + skew, y, x, y + h, x + w, y + h);
    addPrim(layer->get_ot_entry(layer, FIGHTSTG_BMENU_PANEL_DEPTH), poly);
    SetDrawTPage(tpage, 0, 1, getTPage(1, abr >= 0 ? abr : 0, 0, 0));
    addPrim(layer->get_ot_entry(layer, FIGHTSTG_BMENU_PANEL_DEPTH), tpage);
    gfx_module.funcs.set_packet(tpage + 1);
}

/* The diamond's backing: a dark rhombus (one quad, its corners at the middle of each side) behind the four face chips,
 * so the four read as one pad-shaped group. */
static void fightstg_battle_menu_rhombus(s32 cx, s32 cy, s32 rx, s32 ry) {
    POLY_F4 *poly;
    DR_TPAGE *tpage;
    GfxLayer *layer = gfx_module.funcs.get_layer(0x1005);

    if (layer == NULL) {
        return;
    }
    poly = gfx_module.funcs.get_packet();
    tpage = (DR_TPAGE *)(poly + 1);
    setRGB0(poly, 200, 195, 150); /* the reference veil: the diamond stays translucent */
    setPolyF4(poly);
    setSemiTrans(poly, 1);
    setXY4(poly, cx, cy - ry, cx + rx, cy, cx - rx, cy, cx, cy + ry);
    addPrim(layer->get_ot_entry(layer, FIGHTSTG_BMENU_PANEL_DEPTH), poly);
    SetDrawTPage(tpage, 0, 1, getTPage(1, 2, 0, 0));
    addPrim(layer->get_ot_entry(layer, FIGHTSTG_BMENU_PANEL_DEPTH), tpage);
    gfx_module.funcs.set_packet(tpage + 1);
}

/* The chips' outlines, at full strength so the colour is the controller's: neutral (the shoulders, the PlayStation
 * symbols, which are coloured glyphs of their own), then the Xbox face letters' own (A green, B red, X blue,
 * Y yellow). */
static const u8 fightstg_bmenu_tints[5][3] = {
    { 170, 176, 205 }, { 30, 190, 70 }, { 235, 50, 45 }, { 50, 125, 250 }, { 250, 200, 30 },
};

static u8 fightstg_battle_menu_tint_of(const u8 *icon) {
    if (icon[0] >= 0x0E && icon[0] < 0x0E + 26 && icon[1] == 0) {
        switch ('A' + icon[0] - 0x0E) {
        case 'A':
            return 1;
        case 'B':
            return 2;
        case 'X':
            return 3;
        case 'Y':
            return 4;
        }
    }
    return 0;
}

/* Lays action i out from its place, the chip's glyph width and the word's: chip, word and plate, relative to the chip.
 * A word too wide for its room (a long translation) is drawn smaller. */
static void fightstg_battle_menu_layout(FightstgBattleMenu *obj, s32 i, s32 sym_w) {
    const char *word = fightstg_bmenu_word(i);
    s32 spot = fightstg_bmenu_spot(i);
    s32 kind = fightstg_bmenu_place[spot].kind;
    s32 size = i == FIGHTSTG_BMENU_FIGHT ? FIGHTSTG_BMENU_SIZE_FIGHT
               : i == FIGHTSTG_BMENU_INSPECT
                   ? FIGHTSTG_BMENU_SIZE_INSPECT
               : kind >= 2 ? FIGHTSTG_BMENU_SIZE_TAB
                           : FIGHTSTG_BMENU_SIZE_FACE;
    s32 chip_w = sym_w + 4 < FIGHTSTG_BMENU_CHIP ? FIGHTSTG_BMENU_CHIP : sym_w + 4;
    s32 room = kind == 1 ? FIGHTSTG_BMENU_WEST_W : FIGHTSTG_BMENU_WORD_W;
    s32 w = def_poly_text_width(size, word);

    if (w > room && w > 0) {
        size = size * room / w;
        w = def_poly_text_width(size, word);
    }
    obj->x[i] = fightstg_bmenu_place[spot].x;
    obj->y[i] = fightstg_bmenu_place[spot].y;
    obj->spot[i] = spot;
    obj->at[spot] = i;
    if (kind == 3) { /* the chip at the block's right edge, the word before it */
        obj->x[i] = FIGHTSTG_BMENU_R - chip_w;
    }
    obj->chip_w[i] = chip_w;
    obj->sym_w[i] = sym_w;
    obj->word_size[i] = size;
    obj->word_w[i] = w;
    obj->word_x[i] = (kind == 1 || kind == 3) ? -FIGHTSTG_BMENU_GAP - w : chip_w + FIGHTSTG_BMENU_GAP;
    obj->plate_x[i] = (kind == 1 || kind == 3) ? obj->word_x[i] - 5 : -2;
    obj->plate_w[i] = chip_w + FIGHTSTG_BMENU_GAP + w + 7;
}

/* The acting Digimon's Blast Gauge, 0..1000 (the HUD's bar at the top right shows the same). */
static s32 fightstg_battle_menu_gauge(void) {
    s32 id = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);

    return id >= 0 && id < 8 ? records_state.gauges[id] : 0;
}

/* Action i at its rest place moved by (dx, dy): its word, its chip and glyph, its plate, and `glow` (0 none, 1
 * pressed, 2 refused, 3 the ready Blast's pulse, 4 the tempting Blast: the plate lit, the word dark). Queued front to back: in an ordering-table entry what is
 * added last is drawn first. */
static void fightstg_battle_menu_show(FightstgBattleMenu *obj, FightstgBattleMenuData *data, s32 i, s32 dx, s32 dy,
                                      s32 glow) {
    static const u8 white[3] = { 236, 242, 255 }, warm[3] = { 255, 214, 120 }, grey[3] = { 112, 116, 130 },
                    cyan[3] = { 170, 240, 255 }, green[3] = { 150, 255, 170 },
                    shadow[3] = { 6, 10, 34 };
    GfxLayer *layer = gfx_module.funcs.get_layer(0x1005);
    s32 x = obj->x[i] + dx - port_video_widescreen_pad() * FIGHTSTG_BMENU_WIDE_SHIFT / 100;
    s32 y = obj->y[i] + dy;
    s32 h = FIGHTSTG_BMENU_CHIP;
    s32 size = obj->word_size[i];
    s32 wy = y + (h - (16 * size >> 8)) / 2;
    s32 dim = obj->grey[i] || (obj->press >= 0 && obj->press != i);
    /* the Blast's gauge: the tab is its bar (below), and a full tab inks its word and chip dark, as the temptation
     * does, because the bright cyan leaves a light word nothing to stand on */
    s32 gauge = i == FIGHTSTG_BMENU_BLAST ? fightstg_battle_menu_gauge() : 0;
    s32 full = i == FIGHTSTG_BMENU_BLAST && gauge >= 1000;
    /* lit: full AND spendable. A full gauge the battle refuses (no TP, a status, a purification) keeps the charging
     * blue and only turns its word white: the amber and the buzz mean "you can spend it now" (the owner) */
    s32 lit = full && !obj->grey[i];
    /* the full tab alternates between its two looks (the owner): the dark amber with the word white, then the pale
     * straw with the word dark, so the word keeps its contrast on whichever fill is showing */
    s32 blink = (obj->age / FIGHTSTG_BMENU_BLINK) % 2;
    const u8 *rgb = glow == 4                                       ? shadow
                    : lit                                           ? (blink ? shadow : white)
                    : full                                          ? white
                    : glow == 2 && i == obj->refused                 ? white
                    : dim                                           ? grey
                    : i == FIGHTSTG_BMENU_FIGHT                     ? warm
                    : i == FIGHTSTG_BMENU_BLAST                     ? cyan
                    : i == FIGHTSTG_BMENU_TAG && obj->tag_free      ? green
                                                                    : white;
    const u8 *tint = fightstg_bmenu_tints[obj->tint[i]];
    const char *word = fightstg_bmenu_word(i);
    if (layer == NULL) {
        return;
    }
    def_poly_text(layer, 0, x + obj->word_x[i], wy, size, word, rgb);
    def_poly_text(layer, 0, x + obj->word_x[i] + 1, wy + 1, size, word, glow == 4 ? white : shadow);
    data->icons[i]->set_pos(data->icons[i], x + (obj->chip_w[i] - obj->sym_w[i]) / 2, y + 1);
    if (data->labels[i] != NULL) { /* Tag's free swap: the red disc after the word */
        data->labels[i]->set_pos(data->labels[i], x + obj->word_x[i] + obj->word_w[i] + 3, y);
    }
    /* the chip: opaque, dark, outlined in the button's colour (one quad on another) */
    fightstg_battle_menu_plate(x + 1, y + 1, obj->chip_w[i] - 2, h - 2, 0, -1, dim ? 26 : 16, dim ? 26 : 18, dim ? 30 : 30);
    if (dim) {
        fightstg_battle_menu_plate(x, y, obj->chip_w[i], h, 0, -1, 70, 72, 84);
    } else if (glow == 4 || lit) { /* on the lit plate the chip is inked as its word is, not pasted on in grey */
        fightstg_battle_menu_plate(x, y, obj->chip_w[i], h, 0, -1, shadow[0], shadow[1], shadow[2]);
    } else {
        fightstg_battle_menu_plate(x, y, obj->chip_w[i], h, 0, -1, tint[0], tint[1], tint[2]);
    }
    if (glow == 1) {
        fightstg_battle_menu_plate(x + obj->plate_x[i], y - 1, obj->plate_w[i], h + 2, FIGHTSTG_BMENU_SKEW, 1, 40, 110, 190);
    } else if (glow == 2) {
        fightstg_battle_menu_plate(x + obj->plate_x[i], y - 1, obj->plate_w[i], h + 2, FIGHTSTG_BMENU_SKEW, 1, 170, 20, 20);
    } else if (glow == 4) { /* the temptation: the plate itself bright cyan (opaque), the word dark on it */
        fightstg_battle_menu_plate(x + obj->plate_x[i], y - 1, obj->plate_w[i], h + 2, FIGHTSTG_BMENU_SKEW, -1, 70, 215, 245);
    }
    if (i == FIGHTSTG_BMENU_BLAST) {
        /* the tab itself is the gauge (the owner, round 6: a bar of its own was one more thing to read). The tab is a
         * dark slot filled from its left end as the gauge charges, in the Blast's cyan (never Fight's warm colour, never
         * a refusal's red), so the fill grows towards the button it stands for; full, it is white-cyan, and glow 3's
         * pulse and glow 4's bright plate still mark ready and tempting. Queued after the word and the chip and before
         * the dark plate: the chip and the word draw over the fill, the dark plate under it. */
        s32 g = gauge;
        s32 tx = x + obj->plate_x[i];
        s32 tw = obj->plate_w[i];

        g = g < 0 ? 0 : g > 1000 ? 1000 : g;
        /* the fill alone, with no slot of its own behind it (the owner): the tab's empty part is the arena, and the
         * charge is what shows over it. Queued after the word and the chip, so drawn under them, and before the dark
         * plate, so over it. */
        if (g > 0) {
            /* mode 0, the half blend: the charge reads as a translucent slab over the arena, as the tabs' own veils
             * do, instead of the additive mode that clipped to solid cyan over the bright floor (the owner) */
            fightstg_battle_menu_plate(tx, y - 1, tw * g / 1000, h + 2, FIGHTSTG_BMENU_SKEW, 0,
                                       lit ? (blink ? 250 : 230) : 80, lit ? (blink ? 225 : 160) : 230,
                                       lit ? (blink ? 160 : 40) : 255);
        }
    }
    /* the dark plate: darkens the arena so the word reads over any stage */
    /* Three weights by PLACE, not by action (the owner, round 6): the bottom and left arms carry the diamond's own
     * black, the right arm sits between that and the shoulders' veil, the top arm keeps the plain veil. Swapping Fight
     * and Tag between arms keeps the weights where they are. */
    {
        s32 spot = obj->spot[i];
        s32 r = 90, g = 95, b = 65;

        if (spot == 0 || spot == 1) { /* south (bottom), west (left): the diamond's black */
            r = 200;
            g = 195;
            b = 150;
        } else if (spot == 3) { /* east (right): between the shoulders' veil (90,95,65) and the diamond's black
                                 * (200,195,150), a touch under halfway (the owner: 145 was too strong, 118 too weak) */
            r = 131;
            g = 133;
            b = 97;
        }
        fightstg_battle_menu_plate(x + obj->plate_x[i], y - 1, obj->plate_w[i], h + 2, FIGHTSTG_BMENU_SKEW, 2, (u8)r,
                                   (u8)g, (u8)b);
    }
}

static void fightstg_battle_menu_paint(FightstgBattleMenuData *data, s32 i, u8 palette) {
    data->icons[i]->set_palette(data->icons[i], palette);
}

/* Inspect (definitive combat): the current enemy's seven elements as Weak / Resist / - (neutral) / ??? (not
 * learnt), from its resists as they stand now (buffs and debuffs included). Open: the text is set; closed: the windows
 * are emptied. The panel is a title (the enemy's name) over two columns of picture + state in the 14-pixel font (the
 * 9-pixel one has two usable colours and reads badly at 320x240): Fire, Water, Ice, Wind | Thunder, Machine, Dark.
 * It sits in the right middle (x 170-308, from y 75), the room the portrait window had: clear of the enemy (centre-left)
 * and of the menu (under y 184); with a traits row and a legend it grows down over the player's Digimon. */
#define FIGHTSTG_INSPECT_X 174
#define FIGHTSTG_INSPECT_Y 78        /* the title's line */
#define FIGHTSTG_INSPECT_ROW_Y 92    /* the first element's */
#define FIGHTSTG_INSPECT_LINE 14
#define FIGHTSTG_INSPECT_FAMILIES 5  /* the cells: the elements by family (DEF_FAMILIES), a pair as one cell */
#define FIGHTSTG_INSPECT_ROWS 3      /* families per column */
#define FIGHTSTG_INSPECT_COL_DX 68
#define FIGHTSTG_INSPECT_PAIR_DX 11  /* a pair's second picture, behind the first's right side */
#define FIGHTSTG_INSPECT_STATE_DX 29 /* the state's text, after a pair's two pictures (every cell's, so they line up) */
#define FIGHTSTG_INSPECT_W 138
#define FIGHTSTG_TWICE_X (FIGHTSTG_BMENU_R + 8) /* the LIMIT BREAK label, right of the tabs, one line under where */
                                        /* "Enemy x2" was before it moved into the panel */
#define FIGHTSTG_TWICE_Y (FIGHTSTG_BMENU_ROW1 + 3)
#define FIGHTSTG_TEMPT_X 248 /* the temptation window: kept, always empty (the Blast tab calls instead) */
#define FIGHTSTG_TEMPT_Y 166
#define FIGHTSTG_TEMPT_SHAKE_EVERY 24 /* the tempting Blast tab shakes in bursts: one every 24 ticks, */
#define FIGHTSTG_TEMPT_SHAKE 8        /* 8 ticks long */
#define FIGHTSTG_NOTICE_W 200    /* the refusal's box: the Inspect panel's place, right-aligned (its lines are sentences) */
#define FIGHTSTG_NOTICE_X 112
#define FIGHTSTG_NOTICE_Y 78
#define FIGHTSTG_NOTICE_LINES 3
static const u8 fightstg_inspect_empty[1] = { 0 };

/* The INFO panel's second part (.plan/tasks/battle-hud-part2-info.md): what kind of enemy it is. Under the title a
 * traits row: one picture per trait (SHIFT's two halves, the guard's shield, the speed's chevrons, the cracked block
 * of a lowered Defense), and at its right end the next turns as a queue of chips (red diamond the enemy, blue disc
 * us: two diamonds before the disc is the old "Enemy x2"), for every enemy, so it needs no legend; under the grid the
 * legend, one entry per trait with the same picture and the row's own values. The pictures are new on purpose,
 * drawn as flat pixels on the panel's layer, so none reads as an element (part 1's critic). The panel grows down
 * over the player's Digimon, inside the 240 lines with the most traits a row has (6 legend lines). */
#define FIGHTSTG_INSPECT_PICT 11       /* a picture's side */
#define FIGHTSTG_INSPECT_TRAIT_STEP 15 /* picture to picture on the traits row */
#define FIGHTSTG_INSPECT_CHIP 7         /* a turn's chip in the queue, */
#define FIGHTSTG_INSPECT_QUEUE_STEP 12  /* chip to chip, the arrow between them */
#define FIGHTSTG_INSPECT_PIP 3         /* SHIFT's countdown: one 3-pixel pip per enemy turn left */
#define FIGHTSTG_INSPECT_PIP_STEP 4
#define FIGHTSTG_INSPECT_LEGEND_DX 14  /* a legend entry's text, after its picture */
#define FIGHTSTG_INSPECT_RIGHT (FIGHTSTG_INSPECT_X - 4 + FIGHTSTG_INSPECT_W - 4) /* where the panel's text ends */

/* The pictures, row by row: '#' the first colour, '+' the second, '.' none. */
static const char *const fightstg_inspect_picts[DEF_TRAITS][FIGHTSTG_INSPECT_PICT] = {
    /* SHIFT: two halves, the phase it is in lit (A the left one) */
    { "...##.++...", "..###.+++..", ".####.++++.", "#####.+++++", "#####.+++++", "#####.+++++", "#####.+++++",
      "#####.+++++", ".####.++++.", "..###.+++..", "...##.++..." },
    /* the guard: a shield, lit while it is guarded now */
    { "###########", "#+++++++++#", "#+++++++++#", "#+++++++++#", "#+++++++++#", ".#+++++++#.", ".#+++++++#.",
      "..#+++++#..", "..#+++++#..", "...#+++#...", "....###...." },
    /* the speed: two chevrons */
    { "#....#.....", "##...##....", ".##...##...", "..##...##..", "...##...##.", "....##...##", "...##...##.",
      "..##...##..", ".##...##...", "##...##....", "#....#....." },
    /* a lowered Defense: a cracked block */
    { "#####.#####", "#++++..+++#", "#+++++.+++#", "#++++..+++#", "#++++.++++#", "#+++..++++#", "#++++.++++#",
      "#++++..+++#", "#+++++.+++#", "#++++..+++#", "#####.#####" },
};
static const u8 fightstg_inspect_pict_rgb[DEF_TRAITS][2][3] = {
    { { 240, 120, 255 }, { 240, 120, 255 } }, /* SHIFT: the unlit half is dimmed when drawn */
    { { 255, 210, 60 }, { 60, 120, 220 } },   /* gold rim, blue field */
    { { 90, 235, 130 }, { 90, 235, 130 } },
    { { 215, 160, 95 }, { 150, 95, 55 } },
};

/* The queue's chips (DEF_TURN_PARTY, DEF_TURN_ENEMY) and the arrow between two. */
static const char *const fightstg_inspect_chips[2][FIGHTSTG_INSPECT_CHIP] = {
    { "..###..", ".#####.", "#######", "#######", "#######", ".#####.", "..###.." },
    { "...#...", "..###..", ".#####.", "#######", ".#####.", "..###..", "...#..." },
};
static const char *const fightstg_inspect_arrow[5] = { "#..", ".#.", "..#", ".#.", "#.." };
static const u8 fightstg_inspect_chip_rgb[3][3] = { { 90, 170, 255 }, { 240, 70, 60 }, { 120, 130, 160 } };

/* An opaque flat quad on the panel's layer, over the backdrop (queued before it): corners top-left, top-right,
 * bottom-left, bottom-right (any shape: the animation's trace is a slanted one). */
static void fightstg_inspect_quad(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, const u8 *rgb,
                                  s32 dim) {
    POLY_F4 *poly;
    GfxLayer *layer = gfx_module.funcs.get_layer(0x1005);

    if (layer == NULL) {
        return;
    }
    poly = gfx_module.funcs.get_packet();
    setPolyF4(poly);
    setRGB0(poly, dim ? rgb[0] / 2 : rgb[0], dim ? rgb[1] / 2 : rgb[1], dim ? rgb[2] / 2 : rgb[2]); /* half: an open
                                                                                   * shield must still read on the navy */
    setXY4(poly, x0, y0, x1, y1, x2, y2, x3, y3);
    addPrim(layer->get_ot_entry(layer, FIGHTSTG_BMENU_PANEL_DEPTH), poly);
    gfx_module.funcs.set_packet(poly + 1);
}

/* An opaque flat rectangle on the panel's layer. */
static void fightstg_inspect_pixels(s32 x, s32 y, s32 w, s32 h, const u8 *rgb, s32 dim) {
    fightstg_inspect_quad(x, y, x + w, y, x, y + h, x + w, y + h, rgb, dim);
}

/* A bitmap at (x, y), `h` lines: a quad per run of one colour on a line, '#' in `a`, '+' in `b`, each dimmed when
 * its flag says so. */
static void fightstg_inspect_bitmap(s32 x, s32 y, const char *const *rows, s32 h, const u8 *a, const u8 *b, s32 dim_a,
                                    s32 dim_b) {
    s32 r;
    s32 c;
    s32 end;

    for (r = 0; r < h; r++) {
        const char *row = rows[r];

        for (c = 0; row[c] != 0; c = end) {
            char k = row[c];

            for (end = c + 1; row[end] == k; end++) {
            }
            if (k != '.') {
                fightstg_inspect_pixels(x + c, y + r, end - c, 1, k == '#' ? a : b, k == '#' ? dim_a : dim_b);
            }
        }
    }
}

/* Trait `kind`'s picture at (x, y). `lit`: 0 dims it all (the guard open now); for SHIFT, the phase whose half is
 * lit (A the left one). */
static void fightstg_inspect_pict(s32 x, s32 y, s32 kind, s32 lit) {
    s32 shift = kind == DEF_TRAIT_SHIFT;

    fightstg_inspect_bitmap(x, y, fightstg_inspect_picts[kind], FIGHTSTG_INSPECT_PICT, fightstg_inspect_pict_rgb[kind][0],
                            fightstg_inspect_pict_rgb[kind][1], !lit, shift ? lit != 0 : !lit);
}

static s32 fightstg_inspect_lines(const u8 *text) {
    s32 n = 1;

    for (; text != NULL && *text != 0; text++) {
        if (text[0] == 2 && text[1] == 1) {
            n++;
        }
    }
    return n;
}

/* The panel's animation (the owner, 2026-11-24; .plan/tasks/battle-hud-part2b-anim.md): it comes out of INFO's tab
 * and goes back into it. One parameter, obj->panel_t, from 0 (closed) to FIGHTSTG_PANEL_OPEN (open) in game ticks,
 * walks toward what obj->inspect says: the menu's state still changes at once, input never waits for the picture, and a
 * press mid-way turns it around from where it is. The close is the open played backwards, faster. Stages: a trace
 * runs from INFO's tab to the panel's corner; it is gone as the frame starts (it would cut across the enemy), a bar
 * unfolds the width, overshooting a little, then the frame grows down and each line lights as the edge passes it (the
 * text windows can appear, vanish or move, never stretch or fade). Everything sits on whole PS1 pixels, so at a high
 * internal scale a step is many screen pixels: the slow part, the settling, lasts two drawn frames. Purely visual:
 * nothing the battle reads. */
#define FIGHTSTG_PANEL_TRACE 6       /* ticks: the trace has run out */
#define FIGHTSTG_PANEL_PEAK 10       /* the bar is widest (the overshoot), */
#define FIGHTSTG_PANEL_WIDE 12       /* and settles to the panel's width */
#define FIGHTSTG_PANEL_OPEN 18       /* the frame has its height: the panel at rest, exactly the static one */
#define FIGHTSTG_PANEL_CLOSE_SPEED 2 /* closing walks back this many per tick: whoever closes has read it */
#define FIGHTSTG_PANEL_OVERSHOOT 4
#define FIGHTSTG_PANEL_BAR 3         /* the bar's height while the width unfolds */
#define FIGHTSTG_PANEL_BLIP 4        /* ticks INFO's tab lights as the panel leaves it, and as it comes back */
static const u8 fightstg_panel_trace_rgb[2][3] = { { 90, 190, 255 }, { 225, 248, 255 } }; /* the line, its head */

/* a + (b - a) at n of d, eased out (fast, then slow): the HUD transitions' one curve */
static s32 fightstg_anim_ease(s32 a, s32 b, s32 n, s32 d) {
    if (n <= 0) {
        return a;
    }
    if (n >= d) {
        return b;
    }
    return a + (b - a) * n * (2 * d - n) / (d * d);
}

/* The panel's height at its animation's point: 0 while only the trace shows, the bar's while the width unfolds. */
static s32 fightstg_panel_height(FightstgBattleMenu *obj) {
    s32 t = obj->panel_t;

    return t < FIGHTSTG_PANEL_TRACE ? 0
           : t < FIGHTSTG_PANEL_WIDE ? FIGHTSTG_PANEL_BAR
                                     : fightstg_anim_ease(FIGHTSTG_PANEL_BAR, obj->inspect_h, t - FIGHTSTG_PANEL_WIDE,
                                                          FIGHTSTG_PANEL_OPEN - FIGHTSTG_PANEL_WIDE);
}

/* Something drawn down to line `end` (excluded) shows once the growing frame's inside has passed it. */
static s32 fightstg_panel_shows(FightstgBattleMenu *obj, s32 end) {
    return obj->panel_t >= FIGHTSTG_PANEL_WIDE && end <= FIGHTSTG_INSPECT_Y - 3 + fightstg_panel_height(obj) - 1;
}

/* The text windows follow the frame's edge (set_visible keeps their text; an empty one stays hidden). */
static void fightstg_inspect_reveal(FightstgBattleMenu *obj, FightstgBattleMenuData *data) {
    s32 e;
    s32 f;
    s32 y;
    s32 end;

    data->inspect_title->set_visible(data->inspect_title,
                                     fightstg_panel_shows(obj, FIGHTSTG_INSPECT_Y + 12));
    for (e = 0; e < 7; e++) {
        f = e < FIGHTSTG_INSPECT_FAMILIES ? e : e == 5 ? 1 : 3;
        y = obj->inspect_rows_y + FIGHTSTG_INSPECT_LINE * (f % FIGHTSTG_INSPECT_ROWS);
        data->inspect_names[e]->set_visible(data->inspect_names[e], fightstg_panel_shows(obj, y + 12));
        data->inspect_states[e]->set_visible(data->inspect_states[e], fightstg_panel_shows(obj, y + 12));
    }
    for (e = 0; e < obj->info.count; e++) {
        end = e + 1 < obj->info.count ? obj->legend_y[e + 1] : FIGHTSTG_INSPECT_Y - 3 + obj->inspect_h - 2;
        data->legend[e]->set_visible(data->legend[e], fightstg_panel_shows(obj, end));
    }
}

/* The trace: a 2-pixel line from (x0, y0) to (x1, y1), thick across its main direction, and a bright head. */
static void fightstg_panel_trace(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 dx = x1 - x0;
    s32 dy = y1 - y0;

    fightstg_inspect_pixels(x1 - 1, y1 - 1, 3, 3, fightstg_panel_trace_rgb[1], 0);
    if ((dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy)) {
        fightstg_inspect_quad(x0, y0, x1, y1, x0, y0 + 2, x1, y1 + 2, fightstg_panel_trace_rgb[0], 0);
    } else {
        fightstg_inspect_quad(x0, y0, x0 + 2, y0, x1, y1, x1 + 2, y1, fightstg_panel_trace_rgb[0], 0);
    }
}

static void fightstg_battle_menu_inspect_set(FightstgBattleMenu *obj, FightstgBattleMenuData *data, s32 open) {
    FightstgStats *enemy;
    FightstgEnemyRecord *rec;
    s32 wide_pad = port_video_widescreen_pad();
    s32 traits;
    s32 x;
    s32 y;
    s32 e;
    s32 n;

    for (e = 0; e < DEF_TRAITS; e++) {
        data->legend[e]->set_text(data->legend[e], (u8 *)fightstg_inspect_empty, -1);
    }
    if (!open) {
        data->inspect_title->set_text(data->inspect_title, (u8 *)fightstg_inspect_empty, -1);
        for (e = 0; e < 7; e++) {
            data->inspect_names[e]->set_text(data->inspect_names[e], (u8 *)fightstg_inspect_empty, -1);
            data->inspect_states[e]->set_text(data->inspect_states[e], (u8 *)fightstg_inspect_empty, -1);
        }
        return;
    }
    def_inspect_info(&obj->info);
    traits = obj->info.count > 0 || obj->queue_n > 0;
    obj->inspect_rows_y = traits ? FIGHTSTG_INSPECT_ROW_Y + FIGHTSTG_INSPECT_LINE : FIGHTSTG_INSPECT_ROW_Y;
    enemy = fightstg_rules.get_stats(0x10, 0, fightstg_battle.state.current[1]);
    rec = fightstg_enemy_records.get(fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon);
    if (rec != NULL) {
#ifdef PC_PORT
        /* a Rare Target: the HUD's numeral here too; an enemy with a persona: its mark before the name */
        const unsigned char *rare = def_persona_title(rec->id, def_hunt_foe_name(rec->id, 1));

        if (rare != NULL) {
            data->inspect_title->set_text(data->inspect_title, (u8 *)rare, -1);
        } else
#endif
        data->inspect_title->set_text(data->inspect_title, cdload_module.files.get_file(records_language + 0x4E), rec->name);
    }
    for (e = 0; e < 7; e++) {
        /* five cells by family; windows 5 and 6 are the second pictures of the pairs (families 1 and 3) */
        s32 f = e < FIGHTSTG_INSPECT_FAMILIES ? e : e == 5 ? 1 : 3;

        x = FIGHTSTG_INSPECT_X + wide_pad + FIGHTSTG_INSPECT_COL_DX * (f / FIGHTSTG_INSPECT_ROWS);
        y = obj->inspect_rows_y + FIGHTSTG_INSPECT_LINE * (f % FIGHTSTG_INSPECT_ROWS);
        data->inspect_names[e]->set_pos(data->inspect_names[e], e >= FIGHTSTG_INSPECT_FAMILIES ? x + FIGHTSTG_INSPECT_PAIR_DX : x, y);
        data->inspect_states[e]->set_pos(data->inspect_states[e], x + FIGHTSTG_INSPECT_STATE_DX, y);
    }
    for (e = 0; e < FIGHTSTG_INSPECT_FAMILIES; e++) {
        data->inspect_names[e]->set_text(data->inspect_names[e], (u8 *)def_family_icon(e, 0), -1);
        data->inspect_states[e]->set_text(data->inspect_states[e], (u8 *)def_inspect_family_state(enemy->resists, e, &n), -1);
        data->inspect_states[e]->set_palette(data->inspect_states[e], n);
    }
    data->inspect_names[5]->set_text(data->inspect_names[5], (u8 *)def_family_icon(1, 1), -1); /* the pairs' second */
    data->inspect_names[6]->set_text(data->inspect_names[6], (u8 *)def_family_icon(3, 1), -1); /* pictures */
    y = obj->inspect_rows_y + FIGHTSTG_INSPECT_LINE * FIGHTSTG_INSPECT_ROWS + 4;
    for (e = 0; e < obj->info.count; e++) {
        const u8 *text = def_inspect_legend(obj->info.kind[e]);

        obj->legend_y[e] = (s16)y;
        data->legend[e]->set_pos(data->legend[e], FIGHTSTG_INSPECT_X + wide_pad + FIGHTSTG_INSPECT_LEGEND_DX, y);
        data->legend[e]->set_text(data->legend[e], (u8 *)text, -1);
        y += FIGHTSTG_INSPECT_LINE * fightstg_inspect_lines(text);
    }
    obj->inspect_h = (s16)(y + 2 - (FIGHTSTG_INSPECT_Y - 3));
    fightstg_inspect_reveal(obj, data); /* now: a window shows its new text on this very frame */
}

/* The panel's backdrop at its animation's point: the dark quad, a line under the title (and under the grid when a
 * legend follows), a frame in the windows' blue, and the traits' pictures, each once the frame has grown past it;
 * before the frame, the trace from (ox, oy), INFO's tab. */
static void fightstg_battle_menu_inspect_frame(FightstgBattleMenu *obj, s32 ox, s32 oy) {
    s32 x = FIGHTSTG_INSPECT_X + port_video_widescreen_pad() - 4;
    s32 y = FIGHTSTG_INSPECT_Y - 3;
    s32 w = FIGHTSTG_INSPECT_W;
    s32 h = fightstg_panel_height(obj);
    s32 t = obj->panel_t;
    s32 px = x + 4;
    s32 i;
    s32 k;

    if (t < FIGHTSTG_PANEL_TRACE) {
        fightstg_panel_trace(ox, oy, fightstg_anim_ease(ox, x, t, FIGHTSTG_PANEL_TRACE),
                             fightstg_anim_ease(oy, y, t, FIGHTSTG_PANEL_TRACE));
        return;
    }
    if (t < FIGHTSTG_PANEL_WIDE) {
        w = t < FIGHTSTG_PANEL_PEAK ? fightstg_anim_ease(2, w + FIGHTSTG_PANEL_OVERSHOOT, t - FIGHTSTG_PANEL_TRACE,
                                                         FIGHTSTG_PANEL_PEAK - FIGHTSTG_PANEL_TRACE)
                                    : fightstg_anim_ease(w + FIGHTSTG_PANEL_OVERSHOOT, w, t - FIGHTSTG_PANEL_PEAK,
                                                         FIGHTSTG_PANEL_WIDE - FIGHTSTG_PANEL_PEAK);
        /* the bar is the trace turned along the panel's top, in its colours: the frame's own thin blue lines over
         * the arena did not read (capture, 2026-11-24) */
        fightstg_inspect_pixels(x + w - 2, y - 1, 3, 3, fightstg_panel_trace_rgb[1], 0);
        fightstg_inspect_pixels(x, y, w, 2, fightstg_panel_trace_rgb[0], 0);
        return;
    }

    for (i = 0; i < obj->info.count; i++) {
        s32 kind = obj->info.kind[i];
        s32 lit = kind == DEF_TRAIT_SHIFT ? obj->info.phase == 0 : kind == DEF_TRAIT_GUARD ? obj->info.guarded : 1;

        if (fightstg_panel_shows(obj, FIGHTSTG_INSPECT_ROW_Y + 1 + FIGHTSTG_INSPECT_PICT)) {
            fightstg_inspect_pict(px, FIGHTSTG_INSPECT_ROW_Y + 1, kind, lit);
        }
        px += FIGHTSTG_INSPECT_TRAIT_STEP;
        if (kind == DEF_TRAIT_SHIFT) { /* the countdown: the enemy's turns left in this phase */
            for (k = 0; k < obj->info.left &&
                        fightstg_panel_shows(obj, FIGHTSTG_INSPECT_ROW_Y + 1 + FIGHTSTG_INSPECT_PICT);
                 k++) {
                fightstg_inspect_pixels(px - 2 + FIGHTSTG_INSPECT_PIP_STEP * k, FIGHTSTG_INSPECT_ROW_Y + 5,
                                        FIGHTSTG_INSPECT_PIP, FIGHTSTG_INSPECT_PIP, fightstg_inspect_pict_rgb[kind][0], 0);
            }
            px += obj->info.left > 0 ? FIGHTSTG_INSPECT_PIP_STEP * obj->info.left + 2 : 0;
        }
        /* the legend's picture is the row's, state included (one picture, one meaning) */
        if (fightstg_panel_shows(obj, obj->legend_y[i] + 1 + FIGHTSTG_INSPECT_PICT)) {
            fightstg_inspect_pict(x + 4, obj->legend_y[i] + 1, kind, lit);
        }
    }
    /* the next turns, right-aligned on the traits row, an arrow between two */
    px = x + 4 + FIGHTSTG_INSPECT_RIGHT - FIGHTSTG_INSPECT_X - (FIGHTSTG_INSPECT_QUEUE_STEP * (obj->queue_n - 1) +
                                                              FIGHTSTG_INSPECT_CHIP);
    for (i = 0; i < obj->queue_n && fightstg_panel_shows(obj, FIGHTSTG_INSPECT_ROW_Y + 1 + FIGHTSTG_INSPECT_PICT);
         i++) {
        const u8 *rgb = fightstg_inspect_chip_rgb[obj->queue[i]];

        fightstg_inspect_bitmap(px, FIGHTSTG_INSPECT_ROW_Y + 3, fightstg_inspect_chips[obj->queue[i]],
                                FIGHTSTG_INSPECT_CHIP, rgb, rgb, 0, 0);
        if (i + 1 < obj->queue_n) {
            fightstg_inspect_bitmap(px + FIGHTSTG_INSPECT_CHIP + 1, FIGHTSTG_INSPECT_ROW_Y + 4, fightstg_inspect_arrow,
                                    5, fightstg_inspect_chip_rgb[2], fightstg_inspect_chip_rgb[2], 0, 0);
        }
        px += FIGHTSTG_INSPECT_QUEUE_STEP;
    }
    fightstg_battle_menu_quad(x, y, w, 1, 1, 40, 110, 220);
    fightstg_battle_menu_quad(x, y + h - 1, w, 1, 1, 40, 110, 220);
    fightstg_battle_menu_quad(x, y + 1, 1, h - 2, 1, 40, 110, 220);
    fightstg_battle_menu_quad(x + w - 1, y + 1, 1, h - 2, 1, 40, 110, 220);
    if (fightstg_panel_shows(obj, FIGHTSTG_INSPECT_ROW_Y - 1)) {
        fightstg_battle_menu_quad(x + 3, FIGHTSTG_INSPECT_ROW_Y - 2, w - 6, 1, 1, 30, 70, 150);
    }
    if (obj->info.count > 0 && fightstg_panel_shows(obj, obj->legend_y[0] - 1)) {
        fightstg_battle_menu_quad(x + 3, obj->legend_y[0] - 2, w - 6, 1, 1, 30, 70, 150);
    }
    fightstg_battle_menu_quad(x, y, w, h, 2, 190, 180, 130); /* added last: the layer draws it first, under the lines */
}

/* The refusal's box: the Inspect panel's backdrop and frame, without the title's line. */
static void fightstg_battle_menu_notice_frame(void) {
    s32 x = FIGHTSTG_NOTICE_X + port_video_widescreen_pad() - 4;
    s32 y = FIGHTSTG_NOTICE_Y - 3;
    s32 w = FIGHTSTG_NOTICE_W;
    s32 h = FIGHTSTG_INSPECT_LINE * FIGHTSTG_NOTICE_LINES + 8;

    fightstg_battle_menu_quad(x, y, w, 1, 1, 40, 110, 220);
    fightstg_battle_menu_quad(x, y + h - 1, w, 1, 1, 40, 110, 220);
    fightstg_battle_menu_quad(x, y + 1, 1, h - 2, 1, 40, 110, 220);
    fightstg_battle_menu_quad(x + w - 1, y + 1, 1, h - 2, 1, 40, 110, 220);
    fightstg_battle_menu_quad(x, y, w, h, 2, 190, 180, 130);
}

static void fightstg_battle_menu_notice_set(FightstgBattleMenu *obj, FightstgBattleMenuData *data, const u8 *text) {
    obj->notice = text != NULL;
    if (text != NULL) { /* the notice takes the panel's place: no fold under it */
        obj->panel_t = 0;
    }
    data->notice->set_text(data->notice, (u8 *)(text != NULL ? text : fightstg_inspect_empty), -1);
}

void fightstg_battle_menu_update(FightstgBattleMenu *obj, FightstgBattleMenuData *data) {
    FightstgMember *member;
    u8 icon[4];
    s32 x;
    s32 y;
    s32 r;
    s32 dx;
    s32 dy;
    s32 glow;
    s32 i;
    s32 buffered;
    s32 b_action;
    u32 b_confirm;
    s32 wide_pad = port_video_widescreen_pad();

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
        obj->press = obj->refused = -1;
        port_bm_open(); /* buffer_input: what was pressed just before is given from the first frame */
        /* buffer_input: the press already chose a valid action, so the menu is not drawn at all: the action goes to
         * the battle now (a confirm still in the buffer is left for a technique or item menu). */
        if (port_bm_peek(&b_action, &b_confirm) && b_action >= 0 &&
            !((member->status & 8) && b_action < 3) && !((member->status & 0x20) && b_action == 2) &&
            !(b_action == FIGHTSTG_BMENU_RUN && (def_super_tutorial_battle() || def_finale_active())) &&
            !(b_action == FIGHTSTG_BMENU_BLAST && !def_blast_ready() && !def_finale_blast_only()) &&
            !(def_finale_blast_only() && b_action != FIGHTSTG_BMENU_BLAST) &&
            !(def_finale_golden_mode() && (b_action == 2 || b_action == 3 || b_action == 5 || b_action == FIGHTSTG_BMENU_BLAST))) {
            port_bm_take_action();
            port_battle_menu_chosen(b_action, port_battle_menu_bit(b_action), 0);
            sound_module.play(0x4001C);
            *obj->result = b_action;
            obj->base.set_state(obj, OBJECT_STATE_END);
            break;
        }
        for (i = 0; i < FIGHTSTG_BMENU_ACTIONS; i++) {
            fightstg_battle_menu_icon_text(icon, i);
            if (def_finale_blast_only()) {
                obj->grey[i] = (i != FIGHTSTG_BMENU_BLAST);
            } else if (def_finale_golden_mode()) {
                obj->grey[i] = (i == 2 || i == 3 || i == FIGHTSTG_BMENU_RUN || i == FIGHTSTG_BMENU_BLAST);
            } else {
                obj->grey[i] = ((member->status & 8) && i < 3) || ((member->status & 0x20) && i == 2) ||
                               (i == FIGHTSTG_BMENU_RUN && (def_super_tutorial_battle() || def_finale_active())) ||
                               (i == FIGHTSTG_BMENU_BLAST && !def_blast_ready());
            }
            /* the glyph in the dialogues' 14-pixel font: a letter, a PlayStation symbol or a shoulder's name */
            data->icons[i] = message_create_window(0x1005, 1, 0, 0);
            data->icons[i]->set_text(data->icons[i], icon, -1);
            data->icons[i]->measure(data->icons[i], 1); /* its width, then back to placing it by its left end */
            x = data->icons[i]->measured_width;
            data->icons[i]->measure(data->icons[i], 0);
            obj->tint[i] = fightstg_battle_menu_tint_of(icon);
            fightstg_battle_menu_layout(obj, i, x);
            data->labels[i] = NULL;
            fightstg_battle_menu_paint(data, i, obj->grey[i] ? FIGHTSTG_BMENU_PAL_GREY : 0);
        }
        data->labels[FIGHTSTG_BMENU_TAG] = message_create_window(0x1005, 1, 0, 0);
        data->labels[FIGHTSTG_BMENU_TAG]->set_text(data->labels[FIGHTSTG_BMENU_TAG], (u8 *)fightstg_inspect_empty, -1);
        obj->tag_free = 0;
        data->inspect_title = message_create_window(0x1005, 1, FIGHTSTG_INSPECT_X + wide_pad, FIGHTSTG_INSPECT_Y);
        for (i = 0; i < 7; i++) { /* placed when the panel opens (the grid moves down under a traits row) */
            data->inspect_names[i] = message_create_window(0x1005, 1, 0, 0);
            data->inspect_states[i] = message_create_window(0x1005, 1, 0, 0);
        }
        for (i = 0; i < DEF_TRAITS; i++) {
            data->legend[i] = message_create_window(0x1005, 1, 0, 0);
        }
        /* The next turns (the old "Enemy x2", which left the middle of the screen: the owner, 2026-11-24) live in the
         * panel only. Read here, once: def_turn_queue asks the turn delays, which rewrites fightstg_rules.stats. */
        obj->queue_n = def_on() ? def_turn_queue(obj->queue) : 0;
        fightstg_battle_menu_inspect_set(obj, data, 0);
        data->limit_break = message_create_window(0x1005, 3, FIGHTSTG_TWICE_X - wide_pad, FIGHTSTG_TWICE_Y + 11);
        data->limit_break->set_palette(data->limit_break, 2);
        data->limit_break->set_text(
            data->limit_break,
            (u8 *)((def_limit_break_active(0) || def_limit_break_active(1)) ? def_limit_break_label()
                                                                            : fightstg_inspect_empty),
            -1);
        obj->tempt = def_blast_tempt() || def_finale_blast_only();
        data->notice = message_create_window(0x1005, 1, FIGHTSTG_NOTICE_X + wide_pad, FIGHTSTG_NOTICE_Y);
        data->notice->set_page_lines(data->notice, FIGHTSTG_NOTICE_LINES); /* a window pages two lines by default */
        fightstg_battle_menu_notice_set(obj, data, def_battle_menu_notice());
        /* The temptation has no line of its own any more (the owner, 2026-11-23): the Blast tab itself calls, turning
         * bright cyan with a dark word and shaking (fightstg_battle_menu_show's glow 4). The window stays, empty. */
        data->tempt = message_create_window(0x1005, 1, FIGHTSTG_TEMPT_X + wide_pad, FIGHTSTG_TEMPT_Y);
        data->tempt->set_text(data->tempt, (u8 *)fightstg_inspect_empty, -1);
        if (obj->tempt) {
            port_log("battle menu: the Blast tempts");
        }
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        fightstg_bmenu_seen = port_frames;
        obj->age += gfx_module.funcs.get_frame_ticks();
        r = FIGHTSTG_BMENU_SLIDE - obj->age;
        r = r < 0 ? 0 : r;
        if (obj->press >= 0) {
            obj->press_age += gfx_module.funcs.get_frame_ticks();
        }
        if (obj->info_blip > 0) {
            obj->info_blip -= gfx_module.funcs.get_frame_ticks();
        }
        if (obj->refused >= 0) {
            obj->refused_age += gfx_module.funcs.get_frame_ticks();
            if (obj->refused_age >= FIGHTSTG_BMENU_SHAKE) {
                obj->refused = -1;
            }
        }
        if (obj->tag_free != def_swap_free()) { /* the free swap came or went: Tag's word and disc follow */
            obj->tag_free = def_swap_free();
            data->labels[FIGHTSTG_BMENU_TAG]->set_text(data->labels[FIGHTSTG_BMENU_TAG],
                                                       (u8 *)(obj->tag_free ? fightstg_bmenu_tag_free_glyph
                                                                            : fightstg_inspect_empty), -1);
        }
        for (i = 0; i < FIGHTSTG_BMENU_ACTIONS; i++) {
            dx = -(r * r) / 3;
            dy = 0;
            glow = 0;
            if (i == obj->press) {
                /* a nudge toward the diamond's middle that settles back: 2 3 2 1 0 0 0 */
                dx += obj->press_age < 4 ? (obj->press_age < 2 ? 2 + obj->press_age : 4 - obj->press_age) : 0;
                glow = 1;
            } else if (i == obj->refused) {
                dx += (obj->refused_age & 1) ? 3 : -3;
                glow = 2;
            } else if (i == FIGHTSTG_BMENU_INSPECT && obj->info_blip > 0) {
                glow = 1; /* the panel leaves the tab, or comes back into it */
            } else if (i == FIGHTSTG_BMENU_BLAST && !obj->grey[i] && obj->press < 0 && obj->tempt) {
                /* the temptation: the tab stays lit (bright cyan, dark word) and shakes in bursts, 8 ticks in 24 (critic, round 5:
                 * any more and it nears the meter above and Tech below) */
                glow = 4;
                if (obj->age % FIGHTSTG_TEMPT_SHAKE_EVERY < FIGHTSTG_TEMPT_SHAKE) {
                    dx += (obj->age & 1) ? 2 : -2; /* sideways only: a vertical pixel blurs the word at 320 */
                }
            } else if (i == FIGHTSTG_BMENU_BLAST && !obj->grey[i] && obj->press < 0) {
                /* the full tab buzzes in place (the owner): one pixel each way on every tick, this tab only */
                dx += (obj->age & 1) ? 1 : -1;
            }
            fightstg_battle_menu_show(obj, data, i, dx, dy, glow);
        }
        /* queued after the plates, so drawn under them */
        fightstg_battle_menu_rhombus(FIGHTSTG_BMENU_CX + FIGHTSTG_BMENU_CHIP / 2 - (r * r) / 3 -
                                         port_video_widescreen_pad() * FIGHTSTG_BMENU_WIDE_SHIFT / 100,
                                     FIGHTSTG_BMENU_CY + FIGHTSTG_BMENU_CHIP / 2, FIGHTSTG_BMENU_STEP_X + 12,
                                     FIGHTSTG_BMENU_STEP_Y + 9);
        if (obj->inspect ? obj->panel_t < FIGHTSTG_PANEL_OPEN : obj->panel_t > 0) {
            s32 ticks = gfx_module.funcs.get_frame_ticks();

            if (obj->inspect) {
                obj->panel_t =
                    (s16)(obj->panel_t + ticks > FIGHTSTG_PANEL_OPEN ? FIGHTSTG_PANEL_OPEN : obj->panel_t + ticks);
            } else {
                obj->panel_t = (s16)(obj->panel_t - ticks * FIGHTSTG_PANEL_CLOSE_SPEED);
                if (obj->panel_t <= 0) { /* back into the tab: it lights */
                    obj->panel_t = 0;
                    obj->info_blip = FIGHTSTG_PANEL_BLIP;
                }
            }
            fightstg_inspect_reveal(obj, data);
        }
        if (obj->panel_t > 0) {
            /* the trace starts at INFO's tab where it is now: its place, the slide-in, the widescreen shift */
            i = FIGHTSTG_BMENU_INSPECT;
            fightstg_battle_menu_inspect_frame(obj, obj->x[i] - (r * r) / 3 -
                                                        port_video_widescreen_pad() * FIGHTSTG_BMENU_WIDE_SHIFT / 100 +
                                                        obj->plate_x[i] + obj->plate_w[i],
                                               obj->y[i] + FIGHTSTG_BMENU_CHIP / 2 - 1);
        } else if (obj->notice) {
            fightstg_battle_menu_notice_frame();
        }
        if (obj->press >= 0) {
            /* The choice is given when the nudge is done; the original gives it at once. */
            if (obj->press_age >= FIGHTSTG_BMENU_PRESS) {
                *obj->result = obj->press;
                obj->base.set_state(obj, OBJECT_STATE_END);
            }
            break;
        }
        /* Nothing is read on the frame of the INIT above, as the original's: a press that closed a submenu is gone. */
        buffered = port_bm_take_action();
        if (pad_state.get_pressed(0) & 0x8) { /* Start: the battle memory, the same panel as Inspect */
            sound_module.play(0x4001C);
            obj->inspect = !obj->inspect;
            fightstg_battle_menu_inspect_set(obj, data, obj->inspect);
            fightstg_battle_menu_notice_set(obj, data, NULL);
            obj->info_blip = obj->inspect && obj->panel_t == 0 ? FIGHTSTG_PANEL_BLIP : obj->info_blip;
        }
        for (i = 0; i < FIGHTSTG_BMENU_ACTIONS; i++) {
            extern int port_script_active;
            if (i == buffered || port_battle_menu_pressed(i) ||
                (port_script_active && (pad_state.get_pressed(0) & (1 << port_battle_menu_bit(i))))) {
                sound_module.play(0x4001C);
                if (i == FIGHTSTG_BMENU_INSPECT) { /* Inspect: opens or closes the panel, the menu stays and no turn is spent */
                    obj->inspect = !obj->inspect;
                    fightstg_battle_menu_inspect_set(obj, data, obj->inspect);
                    fightstg_battle_menu_notice_set(obj, data, NULL);
                    obj->info_blip = obj->inspect && obj->panel_t == 0 ? FIGHTSTG_PANEL_BLIP : obj->info_blip;
                    break;
                }
                if (obj->inspect) {
                    obj->inspect = 0;
                    fightstg_battle_menu_inspect_set(obj, data, 0);
                }
                if (obj->grey[i]) {
                    port_bm_clear(); /* a refused press leaves the menu open, and nothing else is given */
                    port_battle_menu_chosen(i, port_battle_menu_bit(i), 1);
                    obj->refused = i;
                    obj->refused_age = 0;
                    /* after the quest a refused Blast says why (the short line over the cross only names the form) */
                    fightstg_battle_menu_notice_set(obj, data, i == FIGHTSTG_BMENU_BLAST ? def_chosen_refusal() : NULL);
                    break;
                }
                fightstg_battle_menu_notice_set(obj, data, NULL);
                port_battle_menu_chosen(i, port_battle_menu_bit(i), 0);
                obj->press = i;
                obj->press_age = 0;
                obj->panel_t = 0; /* cut: the menu ends in 7 ticks and frees the panel's windows */
                for (r = 0; r < FIGHTSTG_BMENU_ACTIONS; r++) {
                    fightstg_battle_menu_paint(data, r, r == i ? FIGHTSTG_BMENU_PAL_HOT : FIGHTSTG_BMENU_PAL_GREY);
                }
                break;
            }
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

FightstgCommandMenu *fightstg_battle_menu_create(s32 sel, s32 *result) {
    FightstgBattleMenu *obj = object_new(fightstg_battle_menu_update, sizeof(FightstgBattleMenu), sizeof(FightstgBattleMenuData));

    obj->result = result;
    *result = -1;
    obj->sel = sel;
    return (FightstgCommandMenu *)obj; /* the object starts with FightstgCommandMenu's fields */
}
#endif

FightstgCommandMenu *fightstg_command_menu_create(s32 sel, s32 *result) {
    FightstgCommandMenu *obj = object_new(fightstg_command_menu_update, sizeof(FightstgCommandMenu), sizeof(FightstgCommandMenuData));

    obj->result = result;
    *result = -1;
    obj->sel = sel;
    return obj;
}

/* A window showing the party's model (fightstg_model_view_create's views) on layer 0x1009. */
typedef struct FightstgPortrait {
    /* 0x00 */ Object base;
    /* 0x50 */ GfxLayer *layer;  /* layer 0x1009 */
    /* 0x54 */ u8 unk_54[0x4];
} FightstgPortrait; /* size 0x58 */

/* fightstg_portrait_update's data block: the view shown and the one replaced. */
typedef struct FightstgPortraitData {
    /* 0x0 */ FightstgModelView *view;
    /* 0x4 */ FightstgModelView *view_2;
} FightstgPortraitData; /* size 0x8 */

extern RECT fightstg_portrait_rect;

void fightstg_portrait_draw(FightstgPortrait *obj, FightstgPortraitData *data) {
    Sprite sprite;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
#endif

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 0);
    sprite.set_vram_pos(0x200, 0);
#ifdef PC_PORT
    sprite.draw(cdload_module.get_subfile_by_id(0x4550000), 0xA, 0xF6 + wide_pad, 0x4A);
#else
    sprite.draw(cdload_module.get_subfile_by_id(0x4550000), 0xA, 0xF6, 0x4A);
#endif
}

void fightstg_portrait_update(FightstgPortrait *obj, FightstgPortraitData *data) {
    FightstgSlots *slots;
    FightstgModelParams *params;
    FightstgModelView *old;
    s32 id;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
#endif

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
#ifdef PC_PORT
        if (wide_pad > 0) {
            fightstg_portrait_rect.x = 0;
            fightstg_portrait_rect.y = 0x4C;
            fightstg_portrait_rect.w = 320;
            fightstg_portrait_rect.h = 0x3C;
            obj->layer = gfx_module.funcs.create_layer(&fightstg_portrait_rect, 0xC, 0x1009);
            obj->layer->set_draw_offset(obj->layer, 0x102 + wide_pad, 0x6A);
        } else
#endif
        {
            fightstg_portrait_rect.x = 0xD0;
            fightstg_portrait_rect.y = 0x4C;
            fightstg_portrait_rect.w = 0x64;
            fightstg_portrait_rect.h = 0x3C;
            obj->layer = gfx_module.funcs.create_layer(&fightstg_portrait_rect, 0xC, 0x1009);
            obj->layer->set_draw_offset(obj->layer, 0x102, 0x6A);
        }
        obj->layer->alloc_callbacks(obj->layer, 0x32);
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        if (obj->base.step == 0) {
            slots = (FightstgSlots *)heap_objects.find(0x14, -1, -1);
            if (slots != NULL) {
                params = slots->get_params(slots, 0);
                params->layers[1].shown = 1;
                params->layers[1].edges = 0;
                params->layers[1].layer_id = 0x1009;
                id = params->model_id;
                if (data->view == NULL) {
                    data->view = fightstg_model_view_create(id, params);
                    old = data->view_2;
                } else {
                    data->view_2 = fightstg_model_view_create(id, params);
                    old = data->view;
                }
                if (old != NULL) {
                    old->base.set_state(old, OBJECT_STATE_END);
                }
                obj->base.next_step(obj);
            }
        }
        fightstg_portrait_draw(obj, data);
        break;
    case OBJECT_STATE_DONE:
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_END:
        if (obj->layer != NULL) {
            slots = (FightstgSlots *)heap_objects.find(0x14, -1, -1);
            slots->get_params(slots, 0)->layers[1].shown = 0;
            gfx_module.funcs.delete_layer(0x1009);
        }
        break;
    }
}

FightstgPortrait *fightstg_portrait_create(void) {
    return object_new(fightstg_portrait_update, sizeof(FightstgPortrait), sizeof(FightstgPortraitData));
}

/* fightstg_digivolve_menu_update's data block. */
typedef struct FightstgDigivolveMenuData {
    /* 0x00 */ FightstgCursor *cursor;
    /* 0x04 */ MessageWindow *names[4];  /* the Digimon's names */
    /* 0x14 */ FightstgStatus *status; /* status screens: one shown, the other closing */
    /* 0x18 */ FightstgStatus *status_2;
} FightstgDigivolveMenuData; /* size 0x1C */

extern FightstgCursorParams fightstg_digivolve_menu_cursor;

FightstgStatus *fightstg_status_create(s32 member, s32 page, s32 entry);


void fightstg_digivolve_menu_show_names(FightstgDigivolveMenu *obj) {
    FightstgDigivolveMenuData *data = (FightstgDigivolveMenuData *)obj->base.children;
    s32 slot;
    s32 i;
    FightstgMember *member;
    s32 cur;
    RecordsDigimon *digimon;

    for (i = 0, slot = -1; i < 3; i++) {
        if (obj->member == gamestate_data.funcs.get_party_member(i)) {
            slot = i;
            break;
        }
    }
    if (slot != -1) {
        member = &fightstg_battle.state.members[0][slot];
        if (member->blasted != 0) {
            cur = member->base_digimon;
        } else {
            cur = member->digimon;
        }
        for (i = 0; i < obj->count; i++) {
            if (data->names[i] == NULL) {
                data->names[i] = message_create_window(0x1005, 1, 0xBB, i * 19 + 0x92);
            }
            digimon = records_get_digimon_func(obj->digimon[i]);
            if (digimon != NULL) {
                data->names[i]->set_text(data->names[i], cdload_module.files.get_file(records_language + 0x4E), digimon->name_id);
                if (cur == obj->digimon[i]) {
                    data->names[i]->set_palette(data->names[i], 7);
                } else {
                    data->names[i]->set_palette(data->names[i], 0);
                }
            }
        }
    }
}

/* The data block comes in untyped and is read through a typed local (the US decomp's shape): with a typed parameter,
 * data takes the argument's slot, its longer life puts it after `changed` in global-alloc (s3/s2, 99.5%). */
void fightstg_digivolve_menu_update(FightstgDigivolveMenu *obj, void *arg) {
    FightstgDigivolveMenuData *data = arg;
    s32 member;
    s32 i;
    s32 n;
    s32 changed;
    s32 open;
    s32 pad;
    FightstgMember *m;
    s32 cur;
    s32 sel;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        member = obj->member;
        gamestate_data.funcs.get_chosen_forms(member, obj->forms);
        n = 1;
        obj->digimon[0] = records_digimon[member].id;
        for (i = 0; i < 3; i++) {
            if (obj->forms[i] >= 3) {
                obj->digimon[n++] = obj->forms[i];
            }
        }
        obj->count = n;
        fightstg_digivolve_menu_cursor.count = n;
        data->cursor = fightstg_cursor_create(&fightstg_digivolve_menu_cursor);
        data->status = fightstg_status_create(obj->member, obj->status_page, data->cursor->sel);
        fightstg_digivolve_menu_show_names(obj);
        obj->cursor = -1;
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        changed = 0;
        open = 1;
        /* Class C (block placement; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the confirm branch goes out of line. */
        LOOP_BLOCK(
        pad = pad_state.get_pressed(0);
        if (pad & 0x800) {
            if (++obj->status_page == 3) {
                obj->status_page = 0;
            }
            changed = 1;
            sound_module.play(0x4001B);
        } else if (pad & 0x400) {
            if (--obj->status_page < 0) {
                obj->status_page = 2;
            }
            changed = 1;
            sound_module.play(0x4001B);
        } else if (pad & 0x2000) {
            sound_module.play(0x4001C);
            m = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
            if (m->blasted != 0) {
                cur = m->base_digimon;
            } else {
                cur = m->digimon;
            }
            sel = obj->digimon[data->cursor->sel];
            if (cur != sel) {
                *obj->result = sel;
                obj->base.set_state(obj, OBJECT_STATE_END);
                data->cursor->locked = 1;
            } else if (pad & 0x4000) {
                *obj->result = -2;
                obj->base.set_state(obj, OBJECT_STATE_END);
                sound_module.play(0x800450BD);
            }
        } else if (pad & 0x4000) {
            *obj->result = -2;
            obj->base.set_state(obj, OBJECT_STATE_END);
            sound_module.play(0x800450BD);
        }
        );
        if (data->cursor->sel != obj->cursor) {
            obj->cursor = data->cursor->sel;
            changed = 1;
        }
        if (changed) {
            if (open) {
                if (data->status != NULL) {
                    data->status->base.set_state(data->status, OBJECT_STATE_DONE);
                    data->status_2 = fightstg_status_create(obj->member, obj->status_page, data->cursor->sel);
                } else {
                    if (data->status_2 != NULL) {
                        data->status_2->base.set_state(data->status_2, OBJECT_STATE_DONE);
                    }
                    data->status = fightstg_status_create(obj->member, obj->status_page, data->cursor->sel);
                }
            } else {
                if (data->status != NULL) {
                    data->status->base.set_state(data->status, OBJECT_STATE_DONE);
                }
                if (data->status_2 != NULL) {
                    data->status_2->base.set_state(data->status_2, OBJECT_STATE_DONE);
                }
            }
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

FightstgDigivolveMenu *fightstg_digivolve_menu_create(s32 *result) {
    FightstgDigivolveMenu *obj = object_new(fightstg_digivolve_menu_update, sizeof(FightstgDigivolveMenu), sizeof(FightstgDigivolveMenuData));

    obj->result = result;
    obj->member = *result;
    *result = -1;
    return obj;
}

/* Where fightstg_status_show_stats shows each stat. */
typedef struct FightstgStatWindow {
    /* 0x0 */ u8 x;     /* x */
    /* 0x1 */ u8 y;     /* y */
    /* 0x2 */ u8 stat;  /* stat (GamestateStats.values) */
} FightstgStatWindow; /* size 0x3 */

extern FightstgStatWindow fightstg_status_stat_windows[13];


void fightstg_status_draw_arrows(FightstgStatus *obj) {
    Sprite sprite;
    s32 *bank = cdload_module.get_subfile_by_id(0x4550000);

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    sprite.set_vram_pos(0x200, 0);
    if (gfx_module.funcs.get_time() & 0x10) {
        sprite.draw(bank, 0x1F, 0x18, 0xAB);
        sprite.draw(bank, 0x20, 0x48, 0xAB);
    }
}

void fightstg_status_open(FightstgStatus *obj, FightstgStatusData *data) {
    u8 *text = cdload_module.files.get_file(records_language + 0x7F);

    data->l1_window = message_create_window(0x1005, 3, 0x22, 0xAB);
    data->l1_window->set_text(data->l1_window, text, 0x11);
    data->l1_window->set_palette(data->l1_window, 2);
    data->r1_window = message_create_window(0x1005, 3, 0x38, 0xAB);
    data->r1_window->set_text(data->r1_window, text, 0x12);
    data->r1_window->set_palette(data->r1_window, 2);
}

void fightstg_status_draw_stats(FightstgStatus *obj) {
    Sprite sprite;
    s32 *bank;

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    bank = cdload_module.get_subfile_by_id(0x2860000);
    sprite.set_vram_pos(0x140, 0);
    sprite.draw(bank, 0x23, 0x18, 0x51);
    bank = cdload_module.get_subfile_by_id(0x4550000);
    sprite.set_vram_pos(0x200, 0);
    sprite.draw(bank, 0x25, 0x10, 0x4A);
}

void fightstg_status_show_stats(FightstgStatus *obj, FightstgStatusData *data) {
    s32 i;
    s32 value;
    FightstgStatWindow *e;

    for (i = 0; i < 13; i++) {
        e = &fightstg_status_stat_windows[i];
        data->stat_windows[i] = message_create_window(0x1005, 1, e->x, e->y);
        value = ((s16 *)&obj->stats)[e->stat];
#ifdef PC_PORT
        if (value >= 1000 && def_limit_break_active(0)) {
            data->stat_windows[i]->set_palette(data->stat_windows[i], 2);
        } else if (value >= 1000) {
            value = 999;
        }
#else
        if (value >= 1000) {
            value = 999;
        }
#endif
        data->stat_windows[i]->set_line_number(data->stat_windows[i], 0, value);
        data->stat_windows[i]->measure(data->stat_windows[i], 1);
    }
    if (obj->stats.penalties[0] != 0) {
        data->stat_windows[0]->set_palette(data->stat_windows[0], 6);
    }
    if (obj->stats.penalties[1] != 0) {
        data->stat_windows[1]->set_palette(data->stat_windows[1], 6);
    }
    if (obj->stats.penalties[2] != 0) {
        data->stat_windows[4]->set_palette(data->stat_windows[4], 6);
    }
}

void fightstg_status_draw_techs(FightstgStatus *obj) {
    Sprite sprite;
    s32 *bank;
    s32 i;
    s32 id;

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    bank = cdload_module.get_subfile_by_id(0x2860000);
    sprite.set_vram_pos(0x140, 0);
    for (i = 0; i < 6; i++) {
        id = obj->techniques[i] & 0x1FFF;
        if (id != 0) {
#ifdef PC_PORT
            if (def_tech_class_sprite(id)) /* definitive: the picture replaces the class (def_tech_icon) */
#endif
            sprite.draw(bank, records_techniques[id - 1].element + 0x37, 0x18, i * 14 + 0x51);
        }
    }
    bank = cdload_module.get_subfile_by_id(0x4550000);
    sprite.set_vram_pos(0x200, 0);
    sprite.draw(bank, 0x26, 0x10, 0x4A);
}

void fightstg_status_show_techs(FightstgStatus *obj, FightstgStatusData *data) {
    u8 *text = cdload_module.files.get_file(records_language + 0x7F);
    s32 i;
    s32 id;

    data->skill_label = message_create_window(0x1005, 1, 0x5A, 0xA9);
    data->skill_label->set_text(data->skill_label, text, 0x19);
    data->skill_level = message_create_window(0x1005, 1, 0x93, 0xA9);
    if (obj->form_level >= 0) {
        data->skill_level->set_line_number(data->skill_level, 0, obj->form_level);
    } else {
        data->skill_level->set_text(data->skill_level, text, 0x1A);
    }
    data->skill_level->measure(data->skill_level, 1);
    for (i = 0; i < 6; i++) {
        data->tech_windows[i] = message_create_window(0x1005, 1, 0x26, i * 14 + 0x51);
        id = obj->techniques[i];
#ifdef PC_PORT
        /* the technique's picture where the class sprite was (def_tech_icon) */
        if (def_on()) {
            data->elem_icons[i] = message_create_window(0x1005, 1, 0x18, i * 14 + 0x51);
            def_tech_icon(data->elem_icons[i], id & 0x1FFF, 0);
        }
#endif
        if (id != 0) {
            data->tech_windows[i]->set_text(data->tech_windows[i], cdload_module.files.get_file(records_language + 0xA2), id & 0x1FFF);
            if (id & 0x8000) {
                data->tech_windows[i]->set_palette(data->tech_windows[i], 3);
            } else if (id & 0x4000) {
                data->tech_windows[i]->set_palette(data->tech_windows[i], 4);
            }
        }
    }
}

void fightstg_status_update(FightstgStatus *obj, FightstgStatusData *data) {
    s32 found[10];
    s32 member;
    RecordsDigimon *digimon;
    s32 i;
    s32 j;
    s32 n;
    s32 count;
    s32 id;
    s32 k;
    s32 m;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        member = obj->member;
        switch (obj->page) {
        case 0:
            gamestate_data.funcs.get_stats(member, &obj->stats);
            if (obj->form != 0) {
                gamestate_data.funcs.get_chosen_forms(member, obj->forms);
                digimon = records_get_digimon_func(obj->forms[obj->form - 1]);
                for (k = 0; k < 6; k++) {
                    (&obj->stats.values[6])[k] += digimon->stats[k];
                }
                for (k = 0; k < 7; k++) {
                    (&obj->stats.values[12])[k] += digimon->resists[k];
                }
            }
            break;
        case 1:
            if (obj->form == 0) {
                digimon = &records_digimon[member];
                obj->techniques[0] = digimon->techniques[6] | 0x8000;
                obj->form_level = -1;
            } else {
                gamestate_data.funcs.get_chosen_forms(member, obj->forms);
                gamestate_data.funcs.get_form(member, obj->forms[obj->form - 1], &obj->form_records[0]);
                obj->form_level = obj->form_records[0].level;
                for (k = 0, n = 0; k < 6; k++) {
                    if (obj->form_records[0].techniques[k] != 0) {
                        obj->techniques[n++] = obj->form_records[0].techniques[k];
                    }
                }
            }
            break;
        case 2:
            if (obj->form == 0) {
                obj->form_level = -1;
            } else {
                for (i = 9; i >= 0; i--) {
                    found[i] = 0;
                }
                i = 0;
                count = gamestate_data.funcs.get_chosen_forms(member, obj->forms);
                for (j = 0; j < count; j++) {
                    if (gamestate_data.funcs.get_form(member, obj->forms[j], &obj->form_records[j]) >= 0
                        && j != obj->form - 1) {
                        for (k = 0; k < 6; k++) {
                            id = obj->form_records[j].techniques[k];
                            if (id != 0 && (id & 0x4000)) {
                                found[i++] = id & 0x1FFF;
                            }
                        }
                    }
                }
                n = 0;
                for (m = 0; m < 10; m++) {
                    id = found[m];
                    for (i = 0; i < 6; i++) {
                        if (obj->techniques[i] == found[m]) {
                            id = 0;
                            break;
                        }
                    }
                    if (id != 0) {
                        obj->techniques[n++] = id;
                    }
                }
                obj->form_level = obj->form_records[obj->form - 1].level;
            }
            break;
        }
        fightstg_status_open(obj, data);
        switch (obj->page) {
        case 0:
            fightstg_status_show_stats(obj, data);
            break;
        case 1:
        case 2:
            fightstg_status_show_techs(obj, data);
            break;
        }
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        fightstg_status_draw_arrows(obj);
        switch (obj->page) {
        case 0:
            fightstg_status_draw_stats(obj);
            break;
        case 1:
        case 2:
            fightstg_status_draw_techs(obj);
            break;
        }
        break;
    case OBJECT_STATE_DONE:
        fightstg_status_draw_arrows(obj);
        switch (obj->page) {
        case 0:
            fightstg_status_draw_stats(obj);
            break;
        case 1:
        case 2:
            fightstg_status_draw_techs(obj);
            break;
        }
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_END:
        break;
    }
}

FightstgStatus *fightstg_status_create(s32 member, s32 page, s32 entry) {
    FightstgStatus *obj = object_new(fightstg_status_update, sizeof(FightstgStatus), sizeof(FightstgStatusData));

    obj->member = member;
    obj->page = page;
    obj->form = entry;
    return obj;
}

/* fightstg_item_menu_update's data block: the cursor and the message windows. */
typedef struct FightstgItemMenuData {
    /* 0x00 */ FightstgCursor *cursor;
    /* 0x04 */ MessageWindow *l1_window;
    /* 0x08 */ MessageWindow *r1_window;
    /* 0x0C */ MessageWindow *times_label;
    /* 0x10 */ MessageWindow *count_window; /* how many of the item */
    /* 0x14 */ MessageWindow *description; /* the item's description */
    /* 0x18 */ MessageWindow *item_windows[7]; /* the page's items */
} FightstgItemMenuData; /* size 0x34 */

extern FightstgCursorParams fightstg_item_menu_cursor;

void fightstg_item_menu_draw(FightstgItemMenu *obj) {
    Sprite sprite;
    s32 *bank;
    s32 i;
    s32 k;

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    sprite.set_vram_pos(0x140, 0);
    bank = cdload_module.get_subfile_by_id(0x2860000);
    for (i = 0; i < 7; i++) {
        k = obj->page * 7 + i;
        if (obj->count - 1 < k) {
            break;
        }
        sprite.draw(bank, records_funcs.get_item_icon(obj->list[k]), 0x1D, i * 14 + 0x45);
    }
    bank = cdload_module.get_subfile_by_id(0x4550000);
    sprite.set_vram_pos(0x200, 0);
    if (gfx_module.funcs.get_time() & 0x10) {
        if (obj->page > 0) {
            sprite.draw(bank, 0x1F, 0x10, 0xA9);
        }
        if (obj->page < obj->pages - 1) {
            sprite.draw(bank, 0x20, 0x90, 0xA9);
        }
    }
    sprite.draw(bank, 0x27, 8, 0x3E);
    sprite.draw(bank, 0x28, 0xA6, 0xA0);
    sprite.draw(bank, 0x31, 0xB, 0xBC);
}

void fightstg_item_menu_open(FightstgItemMenu *obj, FightstgItemMenuData *data) {
    u8 *text = cdload_module.files.get_file(records_language + 0x7F);
    s32 i;

    data->l1_window = message_create_window(0x1005, 3, 0x1A, 0xA9);
    data->l1_window->set_text(data->l1_window, text, 0x11);
    data->l1_window->set_palette(data->l1_window, 2);
    data->r1_window = message_create_window(0x1005, 3, 0x80, 0xA9);
    data->r1_window->set_text(data->r1_window, text, 0x12);
    data->r1_window->set_palette(data->r1_window, 2);
    data->times_label = message_create_window(0x1005, 1, 0xAC, 0xA5);
    data->times_label->set_text(data->times_label, text, 0xC);
    for (i = 0; i < 7; i++) {
        data->item_windows[i] = message_create_window(0x1005, 1, 0x2A, i * 14 + 0x45);
    }
    data->description = message_create_window(0x1005, 1, 0x14, 0xC2);
    data->count_window = message_create_window(0x1005, 1, 0xC6, 0xA5);
}

void fightstg_item_menu_show_page(FightstgItemMenu *obj, FightstgItemMenuData *data) {
    s32 i;
    s32 k;
    s32 id;

    if (obj->count != 0) {
        for (i = 0; i < 7; i++) {
            k = obj->page * 7 + i;
            if (obj->count - 1 < k) {
                data->item_windows[i]->set_visible(data->item_windows[i], 0);
            } else {
                id = obj->list[k];
                if (id != 0) {
                    data->item_windows[i]->set_text(data->item_windows[i], cdload_module.files.get_file(records_language + 0x6A), id);
                }
            }
        }
        id = obj->list[obj->page * 7 + data->cursor->sel];
        data->description->set_text(data->description, cdload_module.files.get_file(records_language + 0x63), id);
        data->count_window->set_line_number(data->count_window, 0, gamestate_data.items[id]);
    } else {
        data->description->set_text(data->description, cdload_module.files.get_file(records_language + 0x7F), 0x15);
        data->count_window->set_text(data->count_window, cdload_module.files.get_file(records_language + 0x7F), 0x1A);
    }
    data->count_window->measure(data->count_window, 1);
}

void fightstg_item_menu_update(FightstgItemMenu *obj, FightstgItemMenuData *data) {
    s32 count;
    s32 i;
    s32 n;
    s32 page;
    s32 pad;
    s32 shown;
#ifdef PC_PORT
    s32 mem_k = 0;
    s32 want;
    s32 b_action;
    u32 b_confirm;
    s32 skip_item;
#endif

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        count = records_funcs.list_items(1, (u16 *)obj->items);
#ifdef PC_PORT
        /* buffer_input: the action and the confirm both came before the menus opened: the remembered item (still
         * held) is given now, without drawing this menu. Nothing is allocated yet: the object only ends. */
        skip_item = 0;
        if (port_mod_battle_menu != 0 && port_bm_peek(&b_action, &b_confirm) && b_confirm != 0) {
            want = port_bm_recall(1, gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]));
            if (want > 0 && gamestate_data.items[want] > 0 &&
                ((RecordsUsable *)records_funcs.get_item(want)->data)->flags & 2) {
                for (i = 0; i < count && obj->items[i] != 0; i++) {
                    if (obj->items[i] == want) {
                        skip_item = want;
                        break;
                    }
                }
            }
            if (skip_item != 0) {
                port_bm_take_confirm();
                sound_module.play(0x4001C);
                *obj->result = skip_item;
                obj->base.set_state(obj, OBJECT_STATE_END);
                break;
            }
        }
#endif
        obj->count = 0;
        for (i = 0; i < count; i++) {
            if (obj->items[i] == 0) {
                break;
            }
            if (((RecordsUsable *)records_funcs.get_item(obj->items[i])->data)->flags & 2) {
                obj->count++;
            }
        }
        if (obj->count != 0) {
            obj->list = heap_funcs.alloc(obj->count * sizeof(s16), 2);
            for (i = 0, n = 0; i < count; i++) {
                if (obj->items[i] == 0) {
                    break;
                }
                if (((RecordsUsable *)records_funcs.get_item(obj->items[i])->data)->flags & 2) {
                    obj->list[n++] = obj->items[i];
                }
            }
            if (obj->count % 7 != 0) {
                obj->pages = obj->count / 7 + 1;
            } else {
                obj->pages = obj->count / 7;
            }
            if (obj->count != 0) {
                if (obj->count >= 8) {
                    fightstg_item_menu_cursor.count = 7;
                } else {
                    fightstg_item_menu_cursor.count = obj->count;
                }
            }
        }
#ifdef PC_PORT
        /* remember_cursor: opens on the item this member used last, on its page */
        want = obj->count != 0 ? port_bm_recall(1, gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0])) : 0;
        for (i = 0; want != 0 && i < obj->count; i++) {
            if (obj->list[i] == want) {
                mem_k = i;
                port_bm_log(1, want, i);
                obj->page = i / 7;
                shown = obj->count - obj->page * 7;
                fightstg_item_menu_cursor.count = shown >= 8 ? 7 : shown;
                break;
            }
        }
#endif
        data->cursor = fightstg_cursor_create(&fightstg_item_menu_cursor);
#ifdef PC_PORT
        data->cursor->sel = obj->shown = mem_k % 7;
#endif
        fightstg_item_menu_open(obj, data);
        fightstg_item_menu_show_page(obj, data);
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        /* Evidence (class C, block placement; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the original places the cancel block out
         * of line, after the unk_54 store. */
        LOOP_BLOCK(
            fightstg_item_menu_draw(obj);
            pad = FIGHTSTG_MENU_PAD();
            page = obj->page;
            if (obj->pages != 0) {
                if (pad & 0x400) {
                    obj->page = page - 1;
                    if (obj->page < 0) {
                        obj->page = 0;
                    }
                } else if (pad & 0x800) {
                    obj->page = page + 1;
                    if (obj->pages - 1 < obj->page) {
                        obj->page = obj->pages - 1;
                    }
                }
            }
            if (page != obj->page) {
                data->cursor->sel = 0;
                shown = obj->count - obj->page * 7;
                if (shown >= 8) {
                    shown = 7;
                }
                data->cursor->params.count = shown;
                fightstg_item_menu_show_page(obj, data);
                sound_module.play(0x4001B);
            } else if (obj->shown != data->cursor->sel) {
                fightstg_item_menu_show_page(obj, data);
            } else if (pad & 0x2000) {
                sound_module.play(0x4001C);
                if (obj->count != 0) {
                    *obj->result = obj->list[obj->page * 7 + data->cursor->sel];
                    obj->base.set_state(obj, OBJECT_STATE_END);
                    data->cursor->locked = 1;
                    break;
                }
            } else if (pad & 0x4000) {
                sound_module.play(0x800450BD);
                *obj->result = -2;
                obj->base.set_state(obj, OBJECT_STATE_END);
                break;
            }
            obj->shown = data->cursor->sel;
        );
        break;
    case OBJECT_STATE_DONE:
        break;
    case OBJECT_STATE_END:
        if (obj->list != NULL) {
            heap_funcs.free(obj->list);
        }
        break;
    }
}

FightstgItemMenu *fightstg_item_menu_create(s32 *result) {
    FightstgItemMenu *obj = object_new(fightstg_item_menu_update, sizeof(FightstgItemMenu), sizeof(FightstgItemMenuData));

    obj->result = result;
    *result = -1;
    return obj;
}

/* fightstg_tech_menu_update's data block: the cursor and the message windows. */
typedef struct FightstgTechMenuData {
    /* 0x00 */ FightstgCursor *cursor;
    /* 0x04 */ MessageWindow *l1_window;
    /* 0x08 */ MessageWindow *r1_window;
    /* 0x0C */ MessageWindow *mp_label;
    /* 0x10 */ MessageWindow *max_mp;
    /* 0x14 */ MessageWindow *slash;
    /* 0x18 */ MessageWindow *mp;
    /* 0x1C */ MessageWindow *tech_windows[6]; /* the page's techniques */
    /* 0x34 */ MessageWindow *description; /* the technique's description */
    /* 0x38 */ MessageWindow *cost_label;
    /* 0x3C */ MessageWindow *cost;      /* its cost */
#ifdef PC_PORT
    /* Definitive combat: each technique's element (what the enemy's weaknesses are about; the list's own icon is the
     * technique's class). Its picture at the row's end, and picture + name beside the cost. NULL when !def_on(). */
    MessageWindow *elem_icons[6];
    MessageWindow *elem_icon;
    MessageWindow *elem_name;
#endif
} FightstgTechMenuData; /* size 0x40 */
#ifdef PC_PORT
#define FIGHTSTG_TECH_ELEM_X 0x1D      /* a row's picture: where the class sprite was (def_tech_icon) */
#define FIGHTSTG_TECH_ELEM_INFO_X 0xB4 /* the description box's: left of the cost, on its line */

/* The element of technique `id` (0: none shown) in the windows `icon` and, when given, `name`. */
static void fightstg_tech_menu_set_element(MessageWindow *icon, MessageWindow *name, s32 id, s32 palette) {
    static const u8 empty[1] = { 0 };
    s32 e = def_tech_element(id);

    if (icon == NULL) {
        return;
    }
    icon->set_text(icon, (u8 *)def_tech_picture(id), -1);
    icon->set_palette(icon, palette);
    if (name != NULL) {
        name->set_text(name, (u8 *)(e >= 0 ? def_inspect_element(e) : empty), -1);
        name->set_palette(name, palette);
    }
}
#endif

void fightstg_tech_menu_draw(FightstgTechMenu *obj) {
    Sprite sprite;
    s32 *bank;
    s32 i;
    s32 k;
    s32 id;

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    sprite.set_vram_pos(0x140, 0);
    bank = cdload_module.get_subfile_by_id(0x2860000);
    for (i = 0; i < 6; i++) {
        k = obj->page * 6 + i;
        if (k < obj->count) {
            id = obj->techniques[k] & 0x1FFF;
            if (id == 0) {
                break;
            }
#ifdef PC_PORT
            if (def_tech_class_sprite(id)) /* definitive: the picture replaces the class (def_tech_icon) */
#endif
            sprite.draw(bank, records_techniques[id - 1].element + 0x37, 0x1D, i * 14 + 0x45);
        }
    }
    sprite.set_vram_pos(0x200, 0);
    bank = cdload_module.get_subfile_by_id(0x4550000);
    if (gfx_module.funcs.get_time() & 0x10) {
        if (obj->page > 0) {
            sprite.draw(bank, 0x1F, 0x10, 0x9F);
        }
        if (obj->page < obj->pages - 1) {
            sprite.draw(bank, 0x20, 0x90, 0x9F);
        }
    }
    sprite.draw(bank, 0x2A, 8, 0x3E);
#ifdef PC_PORT
    sprite.draw(bank, 0x29, 0xA3 + port_video_widescreen_pad(), 0x21);
#else
    sprite.draw(bank, 0x29, 0xA3, 0x21);
#endif
    sprite.draw(bank, 0x31, 0xB, 0xBC);
}

void fightstg_tech_menu_open(FightstgTechMenu *obj, FightstgTechMenuData *data) {
    FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
    u8 *text = cdload_module.files.get_file(records_language + 0x7F);
    s32 i;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
#endif

    data->l1_window = message_create_window(0x1005, 3, 0x1A, 0x9F);
    data->l1_window->set_text(data->l1_window, text, 0x11);
    data->l1_window->set_palette(data->l1_window, 2);
    data->r1_window = message_create_window(0x1005, 3, 0x80, 0x9F);
    data->r1_window->set_text(data->r1_window, text, 0x12);
    data->r1_window->set_palette(data->r1_window, 2);
#ifdef PC_PORT
    data->mp_label = message_create_window(0x1005, 3, 0xAC + wide_pad, 0x3A);
    data->mp_label->set_text(data->mp_label, text, 0xD);
    data->mp = message_create_window(0x1005, 3, 0xD8 + wide_pad, 0x3A);
#else
    data->mp_label = message_create_window(0x1005, 3, 0xAC, 0x3A);
    data->mp_label->set_text(data->mp_label, text, 0xD);
    data->mp = message_create_window(0x1005, 3, 0xD8, 0x3A);
#endif
    if (member->blasted != 0) {
        if (member->mp < 100) {
            data->mp->set_text(data->mp, text, 0x1A);
        } else if (member->mp < 1000) {
            data->mp->set_text(data->mp, text, 0xF);
        } else {
            data->mp->set_text(data->mp, text, 0x24);
        }
    } else {
        data->mp->set_line_number(data->mp, 0, member->mp);
    }
    data->mp->measure(data->mp, 1);
#ifdef PC_PORT
    data->slash = message_create_window(0x1005, 3, 0xD9 + wide_pad, 0x3A);
    data->slash->set_text(data->slash, text, 0x10);
    data->max_mp = message_create_window(0x1005, 3, 0xFB + wide_pad, 0x3A);
#else
    data->slash = message_create_window(0x1005, 3, 0xD9, 0x3A);
    data->slash->set_text(data->slash, text, 0x10);
    data->max_mp = message_create_window(0x1005, 3, 0xFB, 0x3A);
#endif
    data->max_mp->set_line_number(data->max_mp, 0, member->max_mp);
    data->max_mp->measure(data->max_mp, 1);
    for (i = 0; i < 6; i++) {
        data->tech_windows[i] = message_create_window(0x1005, 1, 0x2A, i * 14 + 0x45);
    }
    data->description = message_create_window(0x1005, 1, 0x14, 0xC2);
    data->cost_label = message_create_window(0x1005, 1, 0x100, 0xD0);
    data->cost = message_create_window(0x1005, 1, 0x12B, 0xD0);
#ifdef PC_PORT
    if (def_on()) {
        for (i = 0; i < 6; i++) {
            data->elem_icons[i] = message_create_window(0x1005, 1, FIGHTSTG_TECH_ELEM_X, i * 14 + 0x45);
        }
        data->elem_icon = message_create_window(0x1005, 1, FIGHTSTG_TECH_ELEM_INFO_X, 0xD0);
        data->elem_name = message_create_window(0x1005, 1, FIGHTSTG_TECH_ELEM_INFO_X + 17, 0xD0);
    }
#endif
}

void fightstg_tech_menu_show_page(FightstgTechMenu *obj, FightstgTechMenuData *data) {
    FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
    s32 i;
    s32 k;
    s32 id;
    s32 cost;

    if (obj->count != 0) {
        for (i = 0; i < 6; i++) {
            k = obj->page * 6 + i;
            if (obj->count - 1 < k) {
                data->tech_windows[i]->set_visible(data->tech_windows[i], 0);
#ifdef PC_PORT
                fightstg_tech_menu_set_element(data->elem_icons[i], NULL, 0, 0);
#endif
            } else {
                id = obj->techniques[k];
                data->tech_windows[i]->set_text(data->tech_windows[i], cdload_module.files.get_file(records_language + 0xA2), id & 0x1FFF);
                cost = fightstg_rules.get_tech_cost(0, id);
#ifdef PC_PORT
                /* greyed with the name when the MP does not pay for it */
                fightstg_tech_menu_set_element(data->elem_icons[i], NULL, id & 0x1FFF,
                                               member->blasted == 0 && member->mp < cost ? 7 : 0);
#endif
                if (member->blasted != 0) {
                    if (id & 0x8000) {
                        data->tech_windows[i]->set_palette(data->tech_windows[i], 3);
                    } else if (id & 0x4000) {
                        data->tech_windows[i]->set_palette(data->tech_windows[i], 4);
                    } else {
                        data->tech_windows[i]->set_palette(data->tech_windows[i], 0);
                    }
                } else if (member->mp < cost) {
                    data->tech_windows[i]->set_palette(data->tech_windows[i], 7);
                } else if (id & 0x8000) {
                    data->tech_windows[i]->set_palette(data->tech_windows[i], 3);
                } else if (id & 0x4000) {
                    data->tech_windows[i]->set_palette(data->tech_windows[i], 4);
                } else {
                    data->tech_windows[i]->set_palette(data->tech_windows[i], 0);
                }
            }
        }
        cost = fightstg_rules.get_tech_cost(0, obj->techniques[obj->page * 6 + data->cursor->sel]);
        id = obj->techniques[obj->page * 6 + data->cursor->sel];
#ifdef PC_PORT
        fightstg_tech_menu_set_element(data->elem_icon, data->elem_name, id & 0x1FFF, 0);
#endif
        if (member->blasted == 0 && member->mp < cost) {
            data->description->set_text(data->description, cdload_module.files.get_file(records_language + 0x7F), 0x54);
            data->cost_label->set_text(data->cost_label, cdload_module.files.get_file(records_language + 0x7F), 0xD);
            data->cost_label->set_palette(data->cost_label, 7);
            data->cost->set_line_number(data->cost, 0, cost);
            data->cost->measure(data->cost, 1);
            data->cost->set_palette(data->cost, 7);
        } else {
            data->description->set_text(data->description, cdload_module.files.get_file(records_language + 0x9B), id & 0x1FFF);
            data->cost_label->set_text(data->cost_label, cdload_module.files.get_file(records_language + 0x7F), 0xD);
            data->cost->set_line_number(data->cost, 0, cost);
            data->cost->measure(data->cost, 1);
            if (id & 0x4000) {
                data->cost_label->set_palette(data->cost_label, 4);
                data->cost->set_palette(data->cost, 4);
            } else {
                data->cost_label->set_palette(data->cost_label, 0);
                data->cost->set_palette(data->cost, 0);
            }
        }
    } else {
        data->description->set_text(data->description, cdload_module.files.get_file(records_language + 0x7F), 0x13);
    }
}

extern FightstgCursorParams fightstg_tech_menu_cursor;

/* The technique menu's update: lists the acting member's techniques (its own form's technique, those of
 * its items' records for its form, and the 0x4000 ones of the other records), then lets one be picked
 * (cost paid unless unk_1A). */
void fightstg_tech_menu_update(FightstgTechMenu *obj, FightstgTechMenuData *data) {
    s32 list[10];
    FightstgMember *m;
    FightstgMember *member;
    RecordsDigimon *rec;
    RecordsDigimon *own;
    s32 form;
    s32 n;
    s32 id;
    s32 i;
    s32 j;
    s32 count;
    s32 found;
    s32 tech;
    s32 shown;
    s32 pad;
    s32 page;
    s32 cost;
#ifdef PC_PORT
    s32 mem_k = 0;
    s32 b_action;
    u32 b_confirm;
    s32 skip_tech;
#endif

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        id = gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]);
        m = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
        form = m->digimon;
        rec = &records_digimon[id];
#ifdef PC_PORT
        if (def_finale_golden_mode()) {
            obj->techniques[0] = 0x66 | 0x8000; /* Golden Imperialdramon-P: signature Omega Blade only */
            obj->count = 1;
        } else
#endif
        if (form == rec->id) {
            obj->techniques[0] = rec->techniques[6] | 0x8000;
            obj->count = 1;
        } else {
            n = gamestate_data.funcs.get_chosen_forms(id, obj->forms);
            obj->count = 0;
            for (i = 0; i < n; i++) {
                gamestate_data.funcs.get_form(id, obj->forms[i], &obj->form_records[i]);
                if (form == obj->form_records[i].id) {
                    for (j = 0; j < 6; j++) {
                        if (obj->form_records[i].techniques[j] != 0) {
                            obj->techniques[obj->count++] = (s16)(obj->form_records[i].techniques[j] & ~0x4000);
                        }
                    }
                }
            }
            if (m->blasted != 0) {
                own = records_get_digimon_func(m->digimon);
                found = 0;
                for (j = 0; j < obj->count; j++) {
                    if ((obj->techniques[j] & 0x1FFF) == own->techniques[6]) {
                        obj->techniques[j] = (obj->techniques[j] & 0x1FFF) | 0x8000;
                        found = -1;
                        break;
                    }
                }
                if (found != -1) {
                    obj->techniques[obj->count++] = own->techniques[6] | 0x8000;
                }
            }
            for (j = 0; j < 10; j++) {
                list[j] = 0;
            }
            count = 0;
            for (i = 0; i < n; i++) {
                if (form != obj->form_records[i].id) {
                    for (j = 0; j < 6; j++) {
                        if (obj->form_records[i].techniques[j] & 0x4000) {
                            list[count++] = obj->form_records[i].techniques[j];
                        }
                    }
                }
            }
            for (j = 0; j < count; j++) {
                tech = list[j] & 0x1FFF;
                for (i = 0; i < obj->count; i++) {
                    if (tech == (obj->techniques[i] & 0x1FFF)) {
                        tech = 0;
                        break;
                    }
                }
                if (tech != 0) {
                    obj->techniques[obj->count++] = list[j];
                }
            }
        }
        if (obj->count != 0) {
            if (obj->count % 6 != 0) {
                obj->pages = obj->count / 6 + 1;
            } else {
                obj->pages = obj->count / 6;
            }
        }
        if (obj->count != 0) {
            if (obj->count >= 7) {
                fightstg_tech_menu_cursor.count = 6;
            } else {
                fightstg_tech_menu_cursor.count = obj->count;
            }
        } else {
            fightstg_tech_menu_cursor.count = 1;
        }
#ifdef PC_PORT
        /* remember_cursor: opens on the technique this member used last, on its page (the list is rebuilt each time, so
         * it is found by ID) */
        tech = port_bm_recall(0, id);
        for (i = 0; tech != 0 && i < obj->count; i++) {
            if ((obj->techniques[i] & 0x1FFF) == tech) {
                mem_k = i;
                port_bm_log(0, tech, i);
                obj->page = i / 6;
                shown = obj->count - obj->page * 6;
                fightstg_tech_menu_cursor.count = shown >= 7 ? 6 : shown;
                break;
            }
        }
        /* buffer_input: the action and the confirm both came before the menus opened: the remembered technique is
         * paid for and given now, without drawing this menu either. */
        skip_tech = 0;
        if (port_mod_battle_menu != 0 && port_bm_peek(&b_action, &b_confirm) && b_confirm != 0) {
            tech = port_bm_recall(0, id);
            for (i = 0; tech != 0 && i < obj->count; i++) {
                if ((obj->techniques[i] & 0x1FFF) == tech) {
                    skip_tech = obj->techniques[i];
                    break;
                }
            }
            if (skip_tech != 0) {
                cost = fightstg_rules.get_tech_cost(0, skip_tech);
                if (m->blasted != 0 || m->mp >= cost) {
                    if (m->blasted == 0) {
                        m->mp -= cost;
                    }
                    port_bm_take_confirm();
                    sound_module.play(0x4001C);
                    *obj->result = skip_tech & 0x1FFF;
                    obj->base.set_state(obj, OBJECT_STATE_END);
                    break;
                }
            }
        }
#endif
        data->cursor = fightstg_cursor_create(&fightstg_tech_menu_cursor);
#ifdef PC_PORT
        data->cursor->sel = obj->shown = mem_k % 6;
#endif
        fightstg_tech_menu_open(obj, data);
        fightstg_tech_menu_show_page(obj, data);
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        /* Evidence (class C, block placement; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the original places the cancel block and
         * the confirm stub (a jump into the confirm tail) out of line before case 1. */
        LOOP_BLOCK(
            fightstg_tech_menu_draw(obj);
            pad = FIGHTSTG_MENU_PAD();
            page = obj->page;
            if (obj->pages != 0) {
                if (pad & 0x400) {
                    if (--obj->page < 0) {
                        obj->page = 0;
                    }
                } else if (pad & 0x800) {
                    if (++obj->page > obj->pages - 1) {
                        obj->page = obj->pages - 1;
                    }
                }
            }
            if (page != obj->page) {
                data->cursor->sel = 0;
                shown = obj->count - obj->page * 6;
                if (shown >= 7) {
                    shown = 6;
                }
                data->cursor->params.count = shown;
                fightstg_tech_menu_show_page(obj, data);
                sound_module.play(0x4001B);
            } else if (obj->shown != data->cursor->sel) {
                fightstg_tech_menu_show_page(obj, data);
            } else if (pad & 0x2000) {
                sound_module.play(0x4001C);
                if (obj->count != 0) {
                    member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                    if (member->blasted != 0) {
                        *obj->result = obj->techniques[obj->page * 6 + data->cursor->sel] & 0x1FFF;
                        obj->base.set_state(obj, OBJECT_STATE_END);
                        data->cursor->locked = 1;
                        break;
                    }
                    cost = fightstg_rules.get_tech_cost(0, obj->techniques[obj->page * 6 + data->cursor->sel]);
                    if (member->mp >= cost) {
                        member->mp -= cost;
                        *obj->result = obj->techniques[obj->page * 6 + data->cursor->sel] & 0x1FFF;
                        obj->base.set_state(obj, OBJECT_STATE_END);
                        data->cursor->locked = 1;
                        break;
                    }
                }
            } else if (pad & 0x4000) {
                sound_module.play(0x800450BD);
                *obj->result = -2;
                obj->base.set_state(obj, OBJECT_STATE_END);
                break;
            }
            obj->shown = data->cursor->sel;
        );
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

FightstgTechMenu *fightstg_tech_menu_create(s32 *result) {
    FightstgTechMenu *obj = object_new(fightstg_tech_menu_update, sizeof(FightstgTechMenu), sizeof(FightstgTechMenuData));

    obj->result = result;
    *result = -1;
    return obj;
}

/* The acting member's item (its record's unk_3D) if member `target` holds it, else 0. */
s32 fightstg_member_menu_get_team_item(FightstgMemberMenu *obj, s32 member, s32 target) {
    s32 id;
    s32 item;
    s32 n;
    s32 i;
    FightstgMember *m;

    gamestate_data.funcs.get_party_member(member);
    id = gamestate_data.funcs.get_party_member(target);
    m = &fightstg_battle.state.members[0][member];
    item = records_get_digimon_func(m->digimon)->level_thresholds[6];
    n = gamestate_data.funcs.get_chosen_forms(id, obj->forms);
    if (n <= 0 || item == 0) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (obj->forms[i] == records_digimon[item - 1].id) {
            return item;
        }
    }
    return 0;
}

void fightstg_member_menu_draw(FightstgMemberMenu *obj) {
    Sprite sprite;
    s32 *bank;
    s32 i;

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    bank = cdload_module.get_subfile_by_id(0x4550000);
    sprite.set_vram_pos(0x200, 0);
    for (i = 0; i < obj->count; i++) {
        if (obj->team_items[i] != 0) {
            sprite.draw(bank, 0x30, 0x5A, i * 0x1F + 0x45);
        }
    }
}

void fightstg_member_menu_open(FightstgMemberMenu *obj, FightstgMemberMenuData *data) {
    u8 *text = cdload_module.files.get_file(records_language + 0x7F);
    s32 i;

    for (i = 0; i < obj->count; i++) {
        data->hp_labels[i] = message_create_window(0x1005, 3, 0x6C, i * 0x20 + 0x4E);
        data->hp_labels[i]->set_text(data->hp_labels[i], text, 0xE);
        data->hp_slashes[i] = message_create_window(0x1005, 3, 0x99, i * 0x20 + 0x4E);
        data->hp_slashes[i]->set_text(data->hp_slashes[i], text, 0x10);
        data->mp_labels[i] = message_create_window(0x1005, 3, 0x6C, i * 0x20 + 0x5C);
        data->mp_labels[i]->set_text(data->mp_labels[i], text, 0xD);
        data->mp_slashes[i] = message_create_window(0x1005, 3, 0x99, i * 0x20 + 0x5C);
        data->mp_slashes[i]->set_text(data->mp_slashes[i], text, 0x10);
        data->hp_values[i] = message_create_window(0x1005, 3, 0x98, i * 0x20 + 0x4E);
        data->max_hp_values[i] = message_create_window(0x1005, 3, 0xBB, i * 0x20 + 0x4E);
        data->mp_values[i] = message_create_window(0x1005, 3, 0x98, i * 0x20 + 0x5C);
        data->max_mp_values[i] = message_create_window(0x1005, 3, 0xBB, i * 0x20 + 0x5C);
        data->names[i] = message_create_window(0x1005, 1, 0x24, i * 0x20 + 0x4C);
    }
}

void fightstg_member_menu_show(FightstgMemberMenu *obj, FightstgMemberMenuData *data) {
    FightstgMember *m;
    s32 i;

    for (i = 0; i < obj->count; i++) {
        m = &fightstg_battle.state.members[0][obj->members[i]];
        data->hp_values[i]->set_line_number(data->hp_values[i], 0, m->hp);
        data->hp_values[i]->measure(data->hp_values[i], 1);
        data->max_hp_values[i]->set_line_number(data->max_hp_values[i], 0, m->max_hp);
        data->max_hp_values[i]->measure(data->max_hp_values[i], 1);
        data->mp_values[i]->set_line_number(data->mp_values[i], 0, m->mp);
        data->mp_values[i]->measure(data->mp_values[i], 1);
        data->max_mp_values[i]->set_line_number(data->max_mp_values[i], 0, m->max_mp);
        data->max_mp_values[i]->measure(data->max_mp_values[i], 1);
        data->names[i]->set_text(data->names[i],
                                 gamestate_data.funcs.get_record(gamestate_data.funcs.get_party_member(obj->members[i]))->name, -1);
        if (m->hp == 0) {
            data->names[i]->set_palette(data->names[i], 7);
        } else {
            data->names[i]->set_palette(data->names[i], 0);
        }
    }
}

/* Shape from the US decomp (func_800967A4; DECISIONS "Independent EU-only project on the unpatched disc"): case 0 reads the current member
 * into a local and the loop re-reads the global, case 1 keeps the cursor's sel in a local. */
extern FightstgCursorParams fightstg_member_menu_cursor;

void fightstg_member_menu_update(FightstgMemberMenu *obj, FightstgMemberMenuData *data) {
    Sprite sprite;
    FightstgMember *members;
    FightstgMember *m;
    s32 pad;
    s32 sel;
    s32 current;
    s32 flag;
    s32 i;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        current = fightstg_battle.state.current[0];
        members = fightstg_battle.state.members[0];
        flag = members[current].blasted;
        for (i = 0; i < 3; i++) {
            if (members[i].digimon != 0 && i != fightstg_battle.state.current[0]) {
                obj->members[obj->count] = i;
                if (obj->check_items != 0 && flag == 0) {
                    obj->team_items[obj->count] = fightstg_member_menu_get_team_item(obj, fightstg_battle.state.current[0], i);
                }
                obj->count++;
            }
        }
        if (obj->count > 0) {
            fightstg_member_menu_cursor.count = obj->count;
            data->cursor = fightstg_cursor_create(&fightstg_member_menu_cursor);
            data->cursor->sel = *obj->cursor;
            fightstg_member_menu_open(obj, data);
            fightstg_member_menu_show(obj, data);
            obj->base.next_state(obj);
        } else {
            data->none_window = message_create_window(0x1005, 1, 0x14, 0xC2);
            data->none_window->set_visible(data->none_window, 0);
            obj->base.set_state(obj, OBJECT_STATE_DONE);
        }
        break;
    case OBJECT_STATE_RUN:
        fightstg_member_menu_draw(obj);
        /* Class C (block placement; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the cancel block goes out of line. */
        LOOP_BLOCK(
            pad = pad_state.get_pressed(0);
            if (obj->check_items != 0 && (pad & 0x4000)) {
                sound_module.play(0x800450BD);
                *obj->result = -2;
                obj->base.set_state(obj, OBJECT_STATE_END);
                break;
            }
            if (pad & 0x2000) {
                sound_module.play(0x4001C);
                sel = data->cursor->sel;
                m = &fightstg_battle.state.members[0][obj->members[sel]];
                if (m->hp != 0) {
                    *obj->cursor = sel;
                    *obj->result = obj->members[data->cursor->sel];
                    obj->base.set_state(obj, OBJECT_STATE_END);
                    data->cursor->locked = 1;
                }
            }
        );
        break;
    case OBJECT_STATE_DONE:
        if (pad_state.get_pressed(0) & 0x2000) {
            sound_module.play(0x4001C);
            *obj->result = -2;
            obj->base.set_state(obj, OBJECT_STATE_END);
            data->none_window->set_visible(data->none_window, 0);
            break;
        }
        sprite_init(&sprite);
        sprite.set_layer_id(0x1005, 1);
        if (obj->cursor_started != 0) {
            if (gfx_module.funcs.get_time() - obj->cursor_time >= 4) {
                obj->cursor_time = gfx_module.funcs.get_time();
                if (++obj->cursor_frame >= 5) {
                    obj->cursor_frame = 0;
                }
            }
            sprite.set_vram_pos(0x140, 0);
            sprite.set_palette(obj->cursor_frame);
            sprite.draw(cdload_module.get_subfile_by_id(0x2860000), 0xA, 0x123, 0xD0);
            sprite.set_palette(0);
        } else {
            obj->cursor_started = 1;
        }
        sprite.set_vram_pos(0x200, 0);
        sprite.draw(cdload_module.get_subfile_by_id(0x4550000), 0x31, 0xB, 0xBC);
        if (data->none_window->is_visible(data->none_window) == 0) {
            data->none_window->set_text(data->none_window, cdload_module.files.get_file(records_language + 0x7F), 0x14);
        }
        break;
    case OBJECT_STATE_END:
        break;
    }
}

FightstgMemberMenu *fightstg_member_menu_create(s32 *result, s32 *cursor, s32 team) {
    FightstgMemberMenu *obj = object_new(fightstg_member_menu_update, sizeof(FightstgMemberMenu), sizeof(FightstgMemberMenuData));

    obj->result = result;
    *result = -1;
    obj->cursor = cursor;
    obj->check_items = team;
    return obj;
}

void fightstg_switch_menu_draw(FightstgSwitchMenu *obj) {
    Sprite sprite;
    s32 *bank = cdload_module.get_subfile_by_id(0x4550000);

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    sprite.set_vram_pos(0x200, 0);
    sprite.draw(bank, 0x2B, 0xA7, 0x8F);
    if (obj->action != 0) {
        sprite.draw(bank, 2, 0xA7, 0xB8);
#ifdef PC_PORT
        sprite.draw(bank, 0x29, 0xA3 + port_video_widescreen_pad(), 0x21);
#else
        sprite.draw(bank, 0x29, 0xA3, 0x21);
#endif
    }
}

/* x of the windows mp_windows[] (fightstg_switch_menu_open). */
extern DVECTOR fightstg_switch_menu_window_x[4];

void fightstg_switch_menu_open(FightstgSwitchMenu *obj, FightstgSwitchMenuData *data) {
    s32 i;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
#endif

    for (i = 0; i < 4; i++) {
        data->names[i] = message_create_window(0x1005, 1, 0xBB, i * 0x13 + 0x92);
    }
    data->action_windows[4] = message_create_window(0x1005, 1, 0xAB, 0x92);
    data->action_windows[0] = message_create_window(0x1005, 1, 0xBB, 0xA5);
    data->action_windows[1] = message_create_window(0x1005, 1, 0xBB, 0xB8);
    data->action_windows[2] = message_create_window(0x1005, 3, 0x10E, 0xBA);
    data->action_windows[3] = message_create_window(0x1005, 3, 0x133, 0xBA);
    for (i = 0; i < 4; i++) {
#ifdef PC_PORT
        data->mp_windows[i] = message_create_window(0x1005, 3, fightstg_switch_menu_window_x[i].vx + wide_pad, 0x39);
#else
        data->mp_windows[i] = message_create_window(0x1005, 3, fightstg_switch_menu_window_x[i].vx, 0x39);
#endif
    }
#ifdef PC_PORT
    data->baton[0] = message_create_window(0x1005, 1, 0xBB, 0xCB);
    data->baton[1] = message_create_window(0x1005, 3, 0x10E, 0xCD);
    data->baton[2] = message_create_window(0x1005, 3, 0x133, 0xCD);
    for (i = 0; i < 3; i++) {
        data->baton[i]->set_visible(data->baton[i], 0);
    }
#endif
}

void fightstg_switch_menu_show_forms(FightstgSwitchMenu *obj, FightstgSwitchMenuData *data, s32 show) {
    RecordsDigimon *rec;
    s32 i;

    if (show) {
        for (i = 0; i < obj->count; i++) {
            rec = records_get_digimon_func(obj->entries[i]);
            if (rec != NULL) {
                data->names[i]->set_text(data->names[i], cdload_module.files.get_file(records_language + 0x4E), rec->name_id);
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            if (data->names[i] != NULL) {
                data->names[i]->set_visible(data->names[i], show);
            }
        }
    }
}

#ifdef PC_PORT
void fightstg_switch_menu_show_actions(FightstgSwitchMenu *obj, FightstgSwitchMenuData *data, s32 show);
/* Baton Pass's row of the second step (definitive combat): a swap that does not spend the turn, costing
 * DEF_BATON_PASS_MP; there only while the enemy is open (def_swap_free), greyed without the MP or while swaps are sealed. */
static void fightstg_baton_show(FightstgSwitchMenu *obj, FightstgSwitchMenuData *data, s32 show) {
    u8 *text = cdload_module.files.get_file(records_language + 0x7F);
    FightstgMember *self = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
    FightstgMember *partner = &fightstg_battle.state.members[0][obj->member];
    s32 y = obj->action != 0 ? 0xCB : 0xB8;
    s32 i;
    u8 palette;

    if (!def_on() || !show || !def_swap_free()) {
        for (i = 0; i < 3; i++) {
            data->baton[i]->set_visible(data->baton[i], 0);
        }
        return;
    }
    palette = (self->mp < DEF_BATON_PASS_MP || (self->status & 0x10) || (partner->status & 0x10)) ? 7 : 0;
    data->baton[0]->set_pos(data->baton[0], 0xBB, y);
    data->baton[1]->set_pos(data->baton[1], 0x10E, y + 2);
    data->baton[2]->set_pos(data->baton[2], 0x133, y + 2);
    data->baton[0]->set_text(data->baton[0], (u8 *)def_baton_label(), -1);
    data->baton[1]->set_text(data->baton[1], text, 0xD);
    data->baton[2]->set_line_number(data->baton[2], 0, DEF_BATON_PASS_MP);
    data->baton[2]->measure(data->baton[2], 1);
    for (i = 0; i < 3; i++) {
        data->baton[i]->set_palette(data->baton[i], palette);
        data->baton[i]->set_visible(data->baton[i], i == 0 || DEF_BATON_PASS_MP > 0); /* no "MP 0" for a free one */
    }
}

/* The confirm press on Baton Pass's row (before the original's own handling): pays and answers as a plain switch.
 * Returns 1 when it took the frame. */
static s32 fightstg_baton_intercept(FightstgSwitchMenu *obj, FightstgSwitchMenuData *data) {
    FightstgMember *self;
    FightstgMember *partner;

    if (!def_on() || !def_swap_free() || obj->base.state != OBJECT_STATE_RUN || obj->base.step != 1 || obj->base.substep != 1 ||
        data->cursor_2 == NULL || data->cursor_2->locked || data->cursor_2->sel != (obj->action != 0 ? 2 : 1) ||
        !(pad_state.get_pressed(0) & 0x2000)) {
        return 0;
    }
    self = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
    partner = &fightstg_battle.state.members[0][obj->member];
    sound_module.play(0x4001C);
    if (self->mp < DEF_BATON_PASS_MP || (self->status & 0x10) || (partner->status & 0x10)) {
        return 1; /* refused: the press is spent, the menu stays */
    }
    self->mp -= DEF_BATON_PASS_MP;
    def_baton_paid();
    *obj->result = 0;
    *obj->result |= obj->entries[obj->picked] << 4;
    obj->base.set_step(obj, 0);
    fightstg_switch_menu_show_actions(obj, data, 0);
    data->cursor_2->locked = 1;
    return 1;
}
#endif

void fightstg_switch_menu_show_actions(FightstgSwitchMenu *obj, FightstgSwitchMenuData *data, s32 show) {
    u8 *text = cdload_module.files.get_file(records_language + 0x7F);
    FightstgMember *self;
    FightstgMember *partner;
    RecordsDigimon *rec;
    s32 i;

    if (show) {
        self = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
        partner = &fightstg_battle.state.members[0][obj->member];
        rec = records_get_digimon_func(obj->entries[obj->picked]);
        data->action_windows[4]->set_text(data->action_windows[4], cdload_module.files.get_file(records_language + 0x4E), rec->name_id);
        data->action_windows[0]->set_text(data->action_windows[0], text, 0x1B);
        if ((self->status & 0x10) || (partner->status & 0x10)) {
            data->action_windows[0]->set_palette(data->action_windows[0], 7);
        } else {
            data->action_windows[0]->set_palette(data->action_windows[0], 0);
        }
        if (obj->action != 0) {
            data->action_windows[1]->set_text(data->action_windows[1], text, 0x1C);
            data->action_windows[2]->set_text(data->action_windows[2], text, 0xD);
            data->action_windows[3]->set_line_number(data->action_windows[3], 0, records_techniques[obj->action - 1].mp_cost);
            data->action_windows[3]->measure(data->action_windows[3], 1);
            if (self->mp < records_techniques[obj->action - 1].mp_cost || partner->mp < records_techniques[obj->action - 1].mp_cost) {
                data->action_windows[1]->set_palette(data->action_windows[1], 7);
            } else if ((self->status & 0x20) || (partner->status & 0x20) || (self->status & 8)) {
                data->action_windows[1]->set_palette(data->action_windows[1], 7);
            } else if (self->hp <= 0 || partner->hp <= 0) {
                data->action_windows[1]->set_palette(data->action_windows[1], 7);
            } else {
                data->action_windows[1]->set_palette(data->action_windows[1], 0);
            }
            data->mp_windows[0]->set_text(data->mp_windows[0], text, 0xD);
            data->mp_windows[1]->set_line_number(data->mp_windows[1], 0, self->mp);
            data->mp_windows[1]->measure(data->mp_windows[1], 1);
            data->mp_windows[2]->set_text(data->mp_windows[2], text, 0x10);
            data->mp_windows[3]->set_line_number(data->mp_windows[3], 0, self->max_mp);
            data->mp_windows[3]->measure(data->mp_windows[3], 1);
        }
    } else {
        for (i = 0; i < 5; i++) {
            if (data->action_windows[i] != NULL) {
                data->action_windows[i]->set_visible(data->action_windows[i], show);
            }
        }
        for (i = 0; i < 4; i++) {
            if (data->mp_windows[i] != NULL) {
                data->mp_windows[i]->set_visible(data->mp_windows[i], show);
            }
        }
    }
#ifdef PC_PORT
    fightstg_baton_show(obj, data, show);
#endif
}

/* The list's and the second step's parameters (fightstg_switch_menu_update). */
extern FightstgCursorParams fightstg_switch_menu_cursors[2];

void fightstg_switch_menu_update(FightstgSwitchMenu *obj, FightstgSwitchMenuData *data) {
    FightstgMember *self;
    FightstgMember *partner;
    RecordsDigimon *own;
    RecordsDigimon *rec;
    s32 id;
    s32 count;
    s32 i;
    s32 pad;
    s32 changed;

#ifdef PC_PORT
    if (fightstg_baton_intercept(obj, data)) {
        return;
    }
#endif
    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        id = obj->member_id;
        gamestate_data.funcs.get_chosen_forms(id, obj->forms);
        count = 1;
        obj->entries[0] = records_digimon[id].id;
        for (i = 0; i < 3; i++) {
#ifdef PC_PORT
            if (def_blast_heated(id)) { /* overheated from a Blast or Chosen: the Tag brings it in as its Rookie only */
                break;
            }
#endif
            if (obj->forms[i] >= 3) {
                obj->entries[count++] = obj->forms[i];
            }
        }
        obj->count = count;
        fightstg_switch_menu_cursors[0].count = count;
        fightstg_switch_menu_open(obj, data);
        obj->page_shown = -1;
        obj->base.next_state(obj);
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 0:
        default:
            switch (obj->base.substep) {
            case 0:
            default:
                data->cursor = fightstg_cursor_create(&fightstg_switch_menu_cursors[0]);
                data->cursor->sel = obj->picked;
                if (data->cursor_2 != NULL) {
                    data->cursor_2->base.set_state(data->cursor_2, OBJECT_STATE_END);
                }
                obj->base.next_substep(obj);
                break;
            case 1:
                /* Evidence (class C, block placement; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the original places the cancel and
                 * confirm blocks out of line (between cases 1/1/0 and 1/1/1). */
                LOOP_BLOCK(
                    pad = pad_state.get_pressed(0);
                    if (pad & 0x4000) {
                        sound_module.play(0x800450BD);
                        *obj->result = -2;
                        obj->base.set_state(obj, OBJECT_STATE_END);
                        break;
                    }
                    if (pad & 0x2000) {
                        sound_module.play(0x4001C);
                        obj->base.next_step(obj);
                        fightstg_switch_menu_show_forms(obj, data, 0);
                        data->cursor->locked = 1;
                        break;
                    }
                    if (pad & 0x800) {
                        if (++obj->page == 3) {
                            obj->page = 0;
                        }
                        sound_module.play(0x4001B);
                    } else if (pad & 0x400) {
                        if (--obj->page < 0) {
                            obj->page = 2;
                        }
                        sound_module.play(0x4001B);
                    } else {
                        fightstg_switch_menu_show_forms(obj, data, 1);
                        fightstg_switch_menu_show_actions(obj, data, 0);
                    }
                );
                break;
            }
            break;
        case 1:
            switch (obj->base.substep) {
            case 0:
            default:
                if (fightstg_battle.state.members[0][fightstg_battle.state.current[0]].blasted == 0) {
                    own = records_get_digimon_func(fightstg_battle.state.members[0][fightstg_battle.state.current[0]].digimon);
                    rec = records_get_digimon_func(obj->entries[obj->picked]);
                    if (own->level_thresholds[6] != 0 && own->level_thresholds[6] == rec->name_id) {
                        obj->action = own->pair_technique;
                    } else {
                        obj->action = 0;
                    }
                }
                fightstg_switch_menu_cursors[1].count = obj->action != 0 ? 2 : 1;
#ifdef PC_PORT
                if (def_swap_free()) {
                    fightstg_switch_menu_cursors[1].count++; /* Baton Pass is the last row, while the enemy is open */
                }
#endif
                data->cursor_2 = fightstg_cursor_create(&fightstg_switch_menu_cursors[1]);
                if (data->cursor != NULL) {
                    data->cursor->base.set_state(data->cursor, OBJECT_STATE_END);
                }
                obj->base.next_substep(obj);
                break;
            case 1:
                /* Evidence (class C, block placement; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the original places the cancel
                 * block out of line (before this case's code). */
                LOOP_BLOCK(
                    pad = pad_state.get_pressed(0);
                    if (pad & 0x2000) {
                        self = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
                        partner = &fightstg_battle.state.members[0][obj->member];
                        sound_module.play(0x4001C);
                        if (data->cursor_2->sel == 0) {
                            if ((self->status & 0x10) || (partner->status & 0x10)) {
                                break;
                            }
                            *obj->result = 0;
                            *obj->result |= obj->entries[obj->picked] << 4;
                            obj->base.set_step(obj, 0);
                            fightstg_switch_menu_show_actions(obj, data, 0);
                            data->cursor_2->locked = 1;
                        } else {
                            if (self->mp < records_techniques[obj->action - 1].mp_cost ||
                                partner->mp < records_techniques[obj->action - 1].mp_cost) {
                                break;
                            }
                            if ((self->status & 0x20) || (partner->status & 0x20) || (self->status & 8)) {
                                break;
                            }
                            if (self->hp <= 0 || partner->hp <= 0) {
                                break;
                            }
                            self->mp -= records_techniques[obj->action - 1].mp_cost;
                            partner->mp -= records_techniques[obj->action - 1].mp_cost;
                            *obj->result = data->cursor_2->sel;
                            *obj->result |= obj->entries[obj->picked] << 4;
                            *obj->action_result = obj->action;
                            obj->base.set_step(obj, 0);
                            fightstg_switch_menu_show_actions(obj, data, 0);
                            data->cursor_2->locked = 1;
                        }
                    } else if (pad & 0x4000) {
                        sound_module.play(0x800450BD);
                        obj->base.set_step(obj, 0);
                        fightstg_switch_menu_show_actions(obj, data, 0);
                        break;
                    } else if (pad & 0x800) {
                        if (++obj->page == 3) {
                            obj->page = 0;
                        }
                        sound_module.play(0x4001B);
                    } else if (pad & 0x400) {
                        if (--obj->page < 0) {
                            obj->page = 2;
                        }
                        sound_module.play(0x4001B);
                    } else {
                        fightstg_switch_menu_show_forms(obj, data, 0);
                        fightstg_switch_menu_show_actions(obj, data, 1);
                    }
                );
                break;
            }
            fightstg_switch_menu_draw(obj);
            break;
        }
        if (obj->page_shown != obj->page) {
            changed = 1;
        } else if (data->cursor == NULL) {
            changed = 0;
        } else if (obj->picked != data->cursor->sel) {
            obj->picked = data->cursor->sel;
            changed = 1;
        } else {
            changed = 0;
        }
        if (changed) {
            obj->page_shown = obj->page;
            if (data->status != NULL) {
                data->status->base.set_state(data->status, OBJECT_STATE_DONE);
                data->status_2 = fightstg_status_create(obj->member_id, obj->page, obj->picked);
            } else {
                if (data->status_2 != NULL) {
                    data->status_2->base.set_state(data->status_2, OBJECT_STATE_DONE);
                }
                data->status = fightstg_status_create(obj->member_id, obj->page, obj->picked);
            }
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

FightstgSwitchMenu *fightstg_switch_menu_create(s32 *result) {
    FightstgSwitchMenu *obj = object_new(fightstg_switch_menu_update, sizeof(FightstgSwitchMenu), sizeof(FightstgSwitchMenuData));

    obj->result = result;
    obj->member = *result;
    obj->member_id = gamestate_data.funcs.get_party_member(*result);
    *result = -1;
    return obj;
}

FightstgSwitchMenu *fightstg_switch_menu_create_team(s32 *result, s32 *tech) {
    FightstgSwitchMenu *obj = fightstg_switch_menu_create(result);

    obj->action_result = tech;
    return obj;
}

void fightstg_message_draw(FightstgMessage *obj) {
    Sprite sprite;
    s32 *bank;

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    if (obj->waiting != 0) {
        if (gfx_module.funcs.get_time() - obj->arrow_time >= 4) {
            obj->arrow_time = gfx_module.funcs.get_time();
            if (++obj->arrow_frame >= 5) {
                obj->arrow_frame = 0;
            }
        }
        sprite.set_vram_pos(0x140, 0);
        sprite.set_palette(obj->arrow_frame);
        sprite.draw(cdload_module.get_subfile_by_id(0x2860000), 0xA, 0x123, 0xD0);
        sprite.set_palette(0);
    }
    bank = cdload_module.get_subfile_by_id(0x4550000);
    sprite.set_vram_pos(0x200, 0);
    sprite.draw(bank, 0x31, 0xB, 0xBC);
}

#ifdef PC_PORT
/* battle_text (docs/LAUNCHER.md "Battle text"; port_mod_battle_text: 0 off, 1 skip, 2 async). The message box is pure
 * presentation: the rules ran before it was shown. Classes, set by fightstg_message_show:
 *  - KEEP: the message is the game's pacing (victory: closed by wfightmn_victory_wait after 100 frames; defeat),
 *    so it runs as on the disc in every mode;
 *  - DROP: a message the animation already says (a plain "Attack", "Evade", the stance and gauge notes);
 *  - NORMAL: skip ends it at once; async hands its windows to the toast below and ends it, so whoever waits for it
 *    goes on while the text stays up. */
enum { FIGHTSTG_MSG_NORMAL, FIGHTSTG_MSG_KEEP, FIGHTSTG_MSG_DROP };
#define FIGHTSTG_TOAST_FRAMES 90

static s32 fightstg_message_port_class(s32 msg, const s32 *args) {
    if (msg == 1 && (args[0] == 0x52 || args[0] == 0x53)) {
        return FIGHTSTG_MSG_KEEP;
    }
    if (msg == 1 || msg == 2 || msg == 7) {
        switch (args[0]) {
        case 8:    /* "Attack" */
        case 0x1D: /* "Evade" (the dodge shows) */
        case 0x3C: /* "Focus on attack" */
        case 0x3D: /* "Focus on guard" */
        case 0x3E: /* "Special effect up" */
        case 0x3F: /* "Blast gauge up" */
        case 0x40: /* "Excited" */
            return FIGHTSTG_MSG_DROP;
        }
    }
    return FIGHTSTG_MSG_NORMAL;
}

/* The toast's windows and the frames it has left: kept by game_mods.c (they are named in its save state: the windows
 * are arena objects, so a state resumes them). Owned by no object: fightstg_toast_run, called by the slots object
 * every frame of a battle, runs and draws them. */
extern void *port_battle_text_windows[2];
extern s32 port_battle_text_frames;

void fightstg_toast_clear(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (port_battle_text_windows[i] != NULL) {
            heap_objects.stop(port_battle_text_windows[i]);
            port_battle_text_windows[i] = NULL;
        }
    }
}

/* buffer_input: every frame of a battle (the slots object calls this), a press of one of the menu's six buttons, or the
 * confirm bit, is offered to the buffer unless a menu is open. */
void fightstg_bm_record(void) {
    u32 pad;
    s32 action = -1;
    s32 i;

    if (port_mod_battle_menu == 0 || port_mod_bm_buffer == 0 || fightstg_bm_in_menu) {
        return;
    }
    pad = pad_state.get_pressed(0);
    for (i = 0; i < 6; i++) {
        extern int port_script_active;
        if (port_battle_menu_pressed(i) || (port_script_active && (pad & (1 << port_battle_menu_bit(i))))) {
            action = i;
            break;
        }
    }
    if (action >= 0 || (pad & 0x2000)) {
        port_bm_record(action, pad & 0x2000);
    }
}

/* A new battle: the old toast's windows went with the previous run's heap. */
void fightstg_toast_reset(void) {
    fightstg_bm_in_menu = 0;
    port_battle_text_windows[0] = port_battle_text_windows[1] = NULL;
    port_battle_text_frames = 0;
}

void fightstg_toast_run(void) {
    Sprite sprite;
    s32 i;

    if (port_battle_text_windows[0] == NULL && port_battle_text_windows[1] == NULL) {
        return;
    }
    if (port_battle_text_frames-- <= 0) {
        fightstg_toast_clear();
        return;
    }
    for (i = 0; i < 2; i++) {
        if (port_battle_text_windows[i] != NULL) {
            heap_objects.try_run(port_battle_text_windows[i]);
        }
    }
    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    sprite.set_vram_pos(0x200, 0);
    sprite.draw(cdload_module.get_subfile_by_id(0x4550000), 0x31, 0xB, 0xBC); /* fightstg_message_draw's frame */
}

/* Async: shows the message's last page (a queued follow-up replaces its first: "HP gradually recovers" then "30 HP
 * healed" becomes the second) for a while, in place of any toast before it. */
static void fightstg_message_to_toast(FightstgMessage *obj, FightstgMessageData *data) {
    s32 msg;
    s32 i;

    while ((msg = obj->queue[++obj->queue_index]) != 0) {
        obj->show(obj, msg, NULL);
    }
    fightstg_toast_clear();
    for (i = 0; i < obj->window_count && i < 2; i++) {
        port_battle_text_windows[i] = data->windows[i];
        data->windows[i]->set_visible(data->windows[i], 1);
        data->windows[i] = NULL; /* the message no longer owns it */
    }
    port_battle_text_frames = FIGHTSTG_TOAST_FRAMES;
}

/* The message, the frame it is ready: 1 when battle_text took it (it is over), 0 when it runs as on the disc. */
static s32 fightstg_message_port_take(FightstgMessage *obj, FightstgMessageData *data) {
    if (port_mod_battle_text == 0 || obj->ready == 0) {
        return 0;
    }
    if (obj->port_class == FIGHTSTG_MSG_KEEP) {
        fightstg_toast_clear(); /* the victory/defeat box takes the same place: no toast under it */
        return 0;
    }
    if (obj->base.step != 0) {
        return 0;
    }
    if (port_mod_battle_text == 2 && obj->port_class == FIGHTSTG_MSG_NORMAL) {
        fightstg_message_to_toast(obj, data);
    }
    obj->base.state = OBJECT_STATE_END;
    return 1;
}
#endif

void fightstg_message_step(FightstgMessage *obj, FightstgMessageData *data) {
    s32 msg;

    switch (obj->base.step) {
    case 0:
    default:
        if (obj->ready != 0) {
            obj->base.step++;
        }
        break;
    case 1:
        obj->window_time = gfx_module.funcs.get_time();
        obj->window_frames = 3;
        data->windows[obj->window]->set_visible(data->windows[obj->window], 1);
        obj->base.step++;
        break;
    case 2:
        if (obj->window_frames < gfx_module.funcs.get_time() - obj->window_time) {
            if (++obj->window >= obj->window_count) {
                obj->waiting = 1;
                obj->base.step++;
            } else {
                obj->base.step = 1;
            }
        }
        break;
    case 3:
#ifdef PC_PORT
        if (port_mod_skip_dialogues != 0) {
            /* skip_dialogues: the battle's message goes on by itself (the press below, without its sound) */
            msg = obj->queue[++obj->queue_index];
            if (msg == 0) {
                obj->base.state = OBJECT_STATE_END;
            } else {
                obj->show(obj, msg, NULL);
                obj->base.set_step(obj, 0);
            }
            obj->waiting = 0;
            break;
        }
#endif
        if (pad_state.get_pressed(0) & 0x2000) {
            sound_module.play(0x4001C);
            msg = obj->queue[++obj->queue_index];
            if (msg == 0) {
                obj->base.state = OBJECT_STATE_END;
            } else {
                obj->show(obj, msg, NULL);
                obj->base.set_step(obj, 0);
            }
            obj->waiting = 0;
        }
        break;
    case 4:
        if (gfx_module.funcs.get_time() - obj->base.substep >= 0x15 || (pad_state.get_pressed(0) & 0x2000)) {
            obj->base.state = OBJECT_STATE_END;
        }
        break;
    }
}

void fightstg_message_update(FightstgMessage *obj, FightstgMessageData *data) {
    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
#ifdef PC_PORT
        port_bm_message(); /* a press while a message is up is its dismissal, not a command */
        if (fightstg_message_port_take(obj, data) != 0) {
            break;
        }
#endif
        fightstg_message_draw(obj);
        fightstg_message_step(obj, data);
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

/* Puts a member's name on line 1 of the first window: side 0 = party (member), else the enemy (member). */
void fightstg_message_set_name(FightstgMessage *obj, FightstgMessageData *data, s32 side, s32 member) {
    FightstgEnemyRecord *rec;
    FightstgMember *m;

    if (side == 0) {
        data->windows[0]->set_line_text(data->windows[0],
                                gamestate_data.funcs.get_record(gamestate_data.funcs.get_party_member(member))->name, -1, 1);
    } else {
        m = &fightstg_battle.state.members[1][member];
        rec = fightstg_enemy_records.get(m->digimon);
        data->windows[0]->set_line_text(data->windows[0], cdload_module.files.get_file(records_language + 0x4E), rec->name, 1);
    }
}

/* Lists in `members` the members of side args[0] that message args[1] is about (e.g. 0: hurt, 1..4: with a status, 5: down). */
void fightstg_message_list_members(FightstgMessage *obj, s32 *args) {
    FightstgMember *m;
    s32 i;
    s32 side;

    side = args[0] != 0;
    obj->member_count = 0;
    m = fightstg_battle.state.members[side];
    switch (args[1]) {
    case 0:
    default:
        for (i = 0; i < 3; i++) {
            if (m[i].digimon != 0 && m[i].hp != 0 && m[i].hp < m[i].max_hp) {
                obj->members[obj->member_count++] = i;
            }
        }
        break;
    case 1:
        for (i = 0; i < 3; i++) {
            if (m[i].digimon != 0 && m[i].hp != 0 && (m[i].status & 1)) {
                obj->members[obj->member_count++] = i;
            }
        }
        break;
    case 2:
        for (i = 0; i < 3; i++) {
            if (m[i].digimon != 0 && m[i].hp != 0 && (m[i].status & 2)) {
                obj->members[obj->member_count++] = i;
            }
        }
        break;
    case 3:
        for (i = 0; i < 3; i++) {
            if (m[i].digimon != 0 && m[i].hp != 0 && (m[i].status & 4)) {
                obj->members[obj->member_count++] = i;
            }
        }
        break;
    case 4:
        for (i = 0; i < 3; i++) {
            if (m[i].digimon != 0 && m[i].hp != 0 && m[i].status != 0) {
                obj->members[obj->member_count++] = i;
            }
        }
        break;
    case 5:
        for (i = 0; i < 3; i++) {
            if (m[i].digimon != 0 && m[i].hp == 0) {
                obj->members[obj->member_count++] = i;
            }
        }
        break;
    case 6:
        for (i = 0; i < 3; i++) {
            if (m[i].digimon != 0) {
                obj->members[obj->member_count++] = i;
            }
        }
        break;
    case 7:
    case 8:
    case 9:
        for (i = 0; i < 3; i++) {
            if (m[i].digimon != 0 && m[i].hp != 0) {
                obj->members[obj->member_count++] = i;
            }
        }
        break;
    }
}

/* Message 0xD (fightstg_message_show): the members listed by fightstg_message_list_members in the first window
 * (side args[0]), and in the second the text for args[1] (0: a value, args[2]). */
void fightstg_message_show_members(FightstgMessage *obj, FightstgMessageData *data, s32 *args) {
    FightstgEnemyRecord *rec;
    FightstgMember *m;
    u8 side;
    u32 kind;
    s32 id;
    s32 i;

    if (obj->member_count != 0) {
        side = args[0];
        kind = args[1];
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), obj->member_count + 0x15);
        if (side == 0) {
            for (i = 0; i < obj->member_count; i++) {
                id = gamestate_data.funcs.get_party_member(obj->members[i]);
                if (id >= 0) {
                    data->windows[0]->set_line_text(data->windows[0], gamestate_data.funcs.get_record(id)->name, -1, i + 1);
                }
            }
        } else {
            for (i = 0; i < obj->member_count; i++) {
                m = &fightstg_battle.state.members[1][obj->members[i]];
                if (m->digimon != 0) {
                    rec = fightstg_enemy_records.get(m->digimon);
                    data->windows[0]->set_line_text(data->windows[0], cdload_module.files.get_file(records_language + 0x4E), rec->name,
                                            i + 1);
                }
            }
        }
        switch (kind) {
        case 0:
        default:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x22);
            data->windows[1]->set_line_number(data->windows[1], 1, args[2]);
            break;
        case 1:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x28);
            break;
        case 2:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x29);
            break;
        case 3:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x2A);
            break;
        case 4:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x2C);
            break;
        case 5:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x2D);
            break;
        case 6:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x2E);
            break;
        case 7:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x30);
            break;
        case 8:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x31);
            break;
        case 9:
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x32);
            break;
        }
        obj->window_count = 2;
    } else {
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x2F);
        obj->window_count = 1;
    }
}

/* Shows battle message msg (1..22) with its arguments in the message box's two windows: the first window
 * usually names the actor of side args[0] (fightstg_message_set_name), the second has the message itself. Some
 * messages queue a follow-up in `queue`. */
void fightstg_message_show(FightstgMessage *obj, s32 msg, s32 *args) {
    FightstgMessageData *data = (FightstgMessageData *)obj->base.children;
    s32 side;
    s32 total;

    obj->ready = 1;
#ifdef PC_PORT
    if (args != NULL) { /* a queued follow-up (args NULL) keeps its first message's class */
        obj->port_class = fightstg_message_port_class(msg, args);
    }
#endif
    if (data->windows[0] == NULL) {
        data->windows[0] = message_create_window(0x1005, 1, 0x14, 0xC2);
    }
    if (data->windows[1] == NULL) {
        data->windows[1] = message_create_window(0x1005, 1, 0x14, 0xD0);
    }
    obj->window = 0;
    switch (msg) {
    case 1:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), args[0]);
        obj->window_count = 1;
        break;
    case 2:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), args[0]);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[1], fightstg_battle.state.current[args[1] != 0]);
        break;
#ifdef PC_PORT
    case 23: /* definitive combat: the user's name, then a text of the port's own (args[0]: def_message) */
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], (u8 *)def_message(args[0]), -1);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[1], fightstg_battle.state.current[args[1] != 0]);
        break;
#endif
    case 3:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 9);
        data->windows[1]->set_line_text(data->windows[1], cdload_module.files.get_file(records_language + 0xA2), args[1], 1);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 4:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0xB);
        data->windows[1]->set_line_number(data->windows[1], 1, args[1]);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 5:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0xA);
        data->windows[1]->set_line_text(data->windows[1], cdload_module.files.get_file(records_language + 0xA2), args[1], 1);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 6:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x25);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 7:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), args[0]);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[1], args[2]);
        break;
    case 8:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x22);
        data->windows[1]->set_line_number(data->windows[1], 1, args[1]);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 9:
        fightstg_message_list_members(obj, args);
        fightstg_message_show_members(obj, data, args);
        break;
    case 10:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x27);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], args[1]);
        obj->queue[1] = 11;
        obj->queue[2] = args[0];
        obj->queue[3] = args[1];
        obj->queue[4] = args[2];
        break;
    case 11:
        args = &obj->queue[obj->queue_index + 1];
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x22);
        data->windows[1]->set_line_number(data->windows[1], 1, args[2]);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], args[1]);
        obj->queue_index += 3;
        break;
    case 12:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x37);
        data->windows[1]->set_line_text(data->windows[1], cdload_module.files.get_file(records_language + 0x4E), args[1], 1);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 13:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x4E);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x4F);
        data->windows[1]->set_line_text(data->windows[1], cdload_module.files.get_file(records_language + 0x4E), args[0], 1);
        obj->window_count = 2;
        break;
    case 14:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 9);
        data->windows[1]->set_line_text(data->windows[1], cdload_module.files.get_file(records_language + 0x6A), args[1], 1);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 15:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x55);
        data->windows[1]->set_line_number(data->windows[1], 1, args[2]);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], args[1]);
        break;
    case 16:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x23);
        total = args[1] * args[2];
        data->windows[1]->set_line_number(data->windows[1], 1, args[1]);
        data->windows[1]->set_line_number(data->windows[1], 2, args[2]);
        data->windows[1]->set_line_number(data->windows[1], 3, total);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 17:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x26);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 9);
        data->windows[1]->set_line_text(data->windows[1], cdload_module.files.get_file(records_language + 0x6A), args[0], 1);
        obj->window_count = 2;
        break;
    case 18:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        if (args[2] == 0) {
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x22);
        } else {
            data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x8F);
        }
        data->windows[1]->set_line_number(data->windows[1], 1, args[1]);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 19:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), args[1] + 0x49);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, args[0], fightstg_battle.state.current[args[0] != 0]);
        break;
    case 20:
        fightstg_message_show(obj, 4, args);
        obj->queue[1] = 21;
        obj->queue[2] = (args[0] == 0) << 4;
        obj->queue[3] = args[1];
        break;
    case 21:
        side = obj->queue[obj->queue_index + 1];
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 0x22);
        data->windows[1]->set_line_number(data->windows[1], 1, obj->queue[obj->queue_index + 2]);
        obj->window_count = 2;
        fightstg_message_set_name(obj, data, side, fightstg_battle.state.current[side != 0]);
        obj->queue_index += 2;
        break;
    case 22:
        data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x8C);
        data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), 9);
        data->windows[1]->set_line_text(data->windows[1], cdload_module.files.get_file(records_language + 0xA2), args[0], 1);
        obj->window_count = 2;
        break;
    }
    data->windows[0]->set_visible(data->windows[0], 0);
    data->windows[1]->set_visible(data->windows[1], 0);
}

void fightstg_message_close(FightstgMessage *obj) {
    obj->base.step = 4;
    obj->waiting = 0;
    obj->base.substep = gfx_module.funcs.get_time();
}

FightstgMessage *fightstg_message_create(void) {
    FightstgMessage *obj = object_new(fightstg_message_update, sizeof(FightstgMessage), sizeof(FightstgMessageData));

    obj->show = fightstg_message_show;
    obj->close = fightstg_message_close;
    return obj;
}

/* Pairs {queue entry (an ID), text line}, indexed by fightstg_confused_menu_show's arg1. */
typedef struct FightstgConfusedMessage {
    /* 0x0 */ s16 id;
    /* 0x2 */ s16 text;
} FightstgConfusedMessage; /* size 0x4 */

extern FightstgConfusedMessage fightstg_confused_menu_messages[];
extern s16 fightstg_confused_menu_order[8]; /* shuffled by fightstg_confused_menu_update */
extern FightstgCursorParams fightstg_confused_menu_cursor;

void fightstg_confused_menu_open(FightstgConfusedMenu *obj) {
    FightstgCommandMenuData *data = (FightstgCommandMenuData *)obj->base.children;
    u8 *text;
    s32 i;
#ifdef PC_PORT
    s32 wide_pad = port_video_widescreen_pad();
#endif

    if (data->windows[0] == NULL) {
        text = cdload_module.files.get_file(records_language + 0x7F);
        for (i = 0; i < 6; i++) {
#ifdef PC_PORT
            data->windows[i] = message_create_window(0x1005, 1, 0x24 - wide_pad, i * 0x13 + 0x6D);
#else
            data->windows[i] = message_create_window(0x1005, 1, 0x24, i * 0x13 + 0x6D);
#endif
            data->windows[i]->set_text(data->windows[i], text, fightstg_confused_menu_order[i] + 0x6B);
        }
    }
}

void fightstg_confused_menu_draw(FightstgConfusedMenu *obj) {
    Sprite sprite;
    s32 *bank;

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    if (obj->waiting != 0) {
        if (gfx_module.funcs.get_time() - obj->cursor_time >= 4) {
            obj->cursor_time = gfx_module.funcs.get_time();
            if (++obj->cursor_frame >= 5) {
                obj->cursor_frame = 0;
            }
        }
        sprite.set_vram_pos(0x140, 0);
        sprite.set_palette(obj->cursor_frame);
        sprite.draw(cdload_module.get_subfile_by_id(0x2860000), 0xA, 0x123, 0xD0);
        sprite.set_palette(0);
    }
    bank = cdload_module.get_subfile_by_id(0x4550000);
    sprite.set_vram_pos(0x200, 0);
    sprite.draw(bank, 0x31, 0xB, 0xBC);
}

void fightstg_confused_menu_step(FightstgConfusedMenu *obj, FightstgCommandMenuData *data) {
    s32 pad = pad_state.get_pressed(0);

    switch (obj->base.step) {
    case 0:
    default:
        if (obj->ready != 0) {
            obj->base.step++;
        }
        break;
    case 1:
        obj->window_time = gfx_module.funcs.get_time();
        obj->frames_per_window = 3;
        data->windows[obj->window]->set_visible(data->windows[obj->window], 1);
        obj->base.step++;
        break;
    case 2:
        if (obj->frames_per_window < gfx_module.funcs.get_time() - obj->window_time) {
            if (++obj->window >= obj->windows) {
                obj->waiting = 1;
                if (pad & 0x2000) {
                    sound_module.play(0x4001C);
                    if (obj->queue[++obj->entry] == 0) {
                        obj->base.state = OBJECT_STATE_END;
                    } else {
                        obj->show(obj, obj->queue[obj->entry], 0);
                        obj->base.set_step(obj, 0);
                    }
                    obj->waiting = 0;
                }
            } else {
                obj->base.step = 1;
            }
        }
        break;
    }
}

void fightstg_confused_menu_set_name(FightstgConfusedMenu *obj, FightstgCommandMenuData *data, s32 arg2) {
    if (arg2 == 0) {
        data->windows[0]->set_line_text(data->windows[0],
                                 gamestate_data.funcs.get_record(gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]))->name,
                                 -1, 1);
    }
}

void fightstg_confused_menu_show(FightstgConfusedMenu *obj, s32 arg1, s32 arg2) {
    FightstgCommandMenuData *data = (FightstgCommandMenuData *)obj->base.children;

    obj->ready = 1;
    if (data->windows[0] == NULL) {
        data->windows[0] = message_create_window(0x1005, 1, 0x14, 0xC2);
    }
    if (data->windows[1] == NULL) {
        data->windows[1] = message_create_window(0x1005, 1, 0x14, 0xD0);
    }
    obj->queue[0] = fightstg_confused_menu_messages[arg1].id;
    data->windows[0]->set_text(data->windows[0], cdload_module.files.get_file(records_language + 0x7F), 0x16);
    data->windows[1]->set_text(data->windows[1], cdload_module.files.get_file(records_language + 0x7F), fightstg_confused_menu_messages[arg1].text);
    obj->windows = 2;
    fightstg_confused_menu_set_name(obj, data, 0);
    data->windows[0]->set_visible(data->windows[0], 0);
    data->windows[1]->set_visible(data->windows[1], 0);
}

void fightstg_confused_menu_update(FightstgConfusedMenu *obj, FightstgCommandMenuData *data) {
    s32 i;
    s32 j;
    s16 tmp;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
#ifdef PC_PORT
    {
        FightstgCursorParams params = fightstg_confused_menu_cursor;

        params.x -= port_video_widescreen_pad();
        data->cursor = fightstg_cursor_create(&params);
    }
#else
        data->cursor = fightstg_cursor_create(&fightstg_confused_menu_cursor);
#endif
        data->cursor->sel = obj->sel;
        for (i = 0; i < 8; i++) {
            j = pad_random.next() % 8;
            tmp = fightstg_confused_menu_order[i];
            fightstg_confused_menu_order[i] = fightstg_confused_menu_order[j];
            fightstg_confused_menu_order[j] = tmp;
        }
        fightstg_confused_menu_open(obj);
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 0:
            if (pad_state.get_pressed(0) & 0x2000) {
                sound_module.play(0x4001C);
                if (fightstg_rules.roll_confused(0) == 0) {
                    *obj->result = 0;
                    obj->base.set_state(obj, OBJECT_STATE_END);
                } else {
                    obj->selected = data->cursor->sel;
                    for (i = 0; i < 6; i++) {
                        data->windows[i]->base.set_state(data->windows[i], OBJECT_STATE_END);
                    }
                    data->cursor->base.set_state(data->cursor, OBJECT_STATE_END);
                    obj->portrait->state = OBJECT_STATE_END;
                    obj->idle_camera->state = OBJECT_STATE_END;
                    obj->base.next_step(obj);
                }
                data->cursor->locked = 1;
            }
            break;
        case 1:
            if (data->windows[0] == NULL && data->windows[1] == NULL) {
                fightstg_confused_menu_show(obj, fightstg_confused_menu_order[obj->selected] * 2 | (pad_random.next() & 1), 0);
                obj->base.set_state(obj, OBJECT_STATE_DONE);
            }
            break;
        }
        break;
    case OBJECT_STATE_DONE:
        fightstg_confused_menu_draw(obj);
        fightstg_confused_menu_step(obj, data);
        if (obj->base.state == OBJECT_STATE_END) {
            *obj->result = 1;
        }
        break;
    case OBJECT_STATE_END:
        break;
    }
}

FightstgConfusedMenu *fightstg_confused_menu_create(s32 *result, Object *portrait, Object *idle_camera) {
    FightstgConfusedMenu *obj = object_new(fightstg_confused_menu_update, sizeof(FightstgConfusedMenu), sizeof(FightstgCommandMenuData));

    obj->result = result;
    *result = -1;
    obj->sel = 0;
    obj->portrait = portrait;
    obj->idle_camera = idle_camera;
    return obj;
}

extern RECT fightstg_cursor_rect;
extern DR_MOVE fightstg_cursor_moves[4];
extern u32 fightstg_cursor_ot[2];

/* Vsync callback of fightstg_cursor_update: copies the highlight into VRAM (cursor entry sel, and
 * restores entry drawn_sel). */
/* `arg` is gfx_module.vsync_arg, the cursor object as an s32 (PTR_TO_S32 in fightstg_cursor_update). */
void fightstg_cursor_copy_highlight(s32 arg) {
    FightstgCursor *obj = S32_TO_PTR(FightstgCursor *, arg);
    u32 *cont;
    s32 i;
    s32 y;

    cont = BreakDraw();
    if (cont != (u32 *)-1) {
        if (++obj->highlight == 12) {
            obj->highlight = 0;
        }
        ClearOTag(fightstg_cursor_ot, 2);
        for (i = 0; i < 4; i += 2) {
            if (i == 2 && obj->drawn_sel == obj->sel) {
                break;
            }
            /* Evidence (class B, register priority only; docs/MATCHING.md "LOOP_BLOCK and LOOP_BARRIER"): the original keeps obj in s1;
             * without the block only registers differ, also with scheduling off (one block, in either branch). */
            if (i < 2) {
                fightstg_cursor_rect.x = obj->highlight * 12;
                LOOP_BLOCK(y = obj->params.vram_y + obj->sel * obj->params.vram_step;);
            } else {
                fightstg_cursor_rect.x = 0x90;
                y = obj->params.vram_y + obj->drawn_sel * obj->params.vram_step;
            }
            SetDrawMove(&fightstg_cursor_moves[i], &fightstg_cursor_rect, obj->params.vram_x, y);
            addPrim(fightstg_cursor_ot, &fightstg_cursor_moves[i]);
            SetDrawMove(&fightstg_cursor_moves[i + 1], &fightstg_cursor_rect, obj->params.vram_x, y + 0x100);
            addPrim(fightstg_cursor_ot, &fightstg_cursor_moves[i + 1]);
        }
        while (IsIdleGPU(0) != 0) {
            PLATFORM_WAIT();
        }
        ContinueDraw(fightstg_cursor_ot, cont);
    }
    obj->drawn_sel = obj->sel;
}

void fightstg_cursor_draw(FightstgCursor *obj) {
    Sprite sprite;
    s32 *bank;
    s32 i;
    s32 y;
    s32 frame;

    sprite_init(&sprite);
    sprite.set_layer_id(0x1005, 1);
    sprite.set_vram_pos(0x200, 0);
    bank = cdload_module.get_subfile_by_id(0x4550000);
    y = obj->params.y;
    for (i = 0; i < obj->params.count; i++) {
        if (obj->times[i] != 0 || obj->sel == i) {
            obj->times[i] += gfx_module.funcs.get_frame_ticks();
            if (obj->times[i] >= 20) {
                if (obj->sel != i) {
                    obj->times[i] = 0;
                } else {
                    obj->times[i] -= 20;
                }
            }
        }
        frame = obj->times[i] >> 2;
        if (frame >= 0 && frame < 5) {
            sprite.set_palette(frame);
        } else {
            sprite.set_palette(0);
        }
        sprite.draw(bank, obj->params.sprite, obj->params.x, y);
        y += obj->params.y_step;
    }
}

void fightstg_cursor_copy_highlight(s32 arg);

void fightstg_cursor_update(FightstgCursor *obj) {
    s32 pad;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_RUN:
        switch (obj->base.step) {
        case 2:
        default:
            gfx_module.vsync_callback = fightstg_cursor_copy_highlight;
            gfx_module.vsync_arg = PTR_TO_S32(obj);
        case 0:
        case 1:
            obj->base.next_step(obj);
            break;
        case 3:
            break;
        }
        if (obj->params.sprite != -1) {
            fightstg_cursor_draw(obj);
        }
        if (obj->locked == 0) {
            pad = pad_state.get_repeat(0) | pad_state.get_pressed(0);
            if (pad & 0x10) {
                if (obj->sel != 0) {
                    obj->sel--;
                    sound_module.play(0x4001B);
                }
            } else if (pad & 0x40) {
                if (obj->sel != obj->params.count - 1) {
                    obj->sel++;
                    sound_module.play(0x4001B);
                }
            }
        }
        break;
    case OBJECT_STATE_DONE:
        break;
    case OBJECT_STATE_END:
        gfx_module.vsync_callback = NULL;
        break;
    }
}

FightstgCursor *fightstg_cursor_create(FightstgCursorParams *params) {
    FightstgCursor *obj = object_new(fightstg_cursor_update, sizeof(FightstgCursor), 0);

    obj->params = *params;
    return obj;
}

void fightstg_jump_update(FightstgJump *obj) {
    s32 y;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->z_offset = 0;
        switch (obj->kind) {
        case 4:
            obj->z_offset = obj->distance + 0x2800;
            break;
        case 5:
            obj->z_offset = obj->params->pos.z - obj->params->start_pos.z;
            if (obj->z_offset < 0) {
                obj->z_offset = -obj->z_offset;
            }
            break;
        }
#ifdef PC_PORT
        /* The disc's slot 0x10, or slot 0 while the rows are exchanged (definitive_anim.c): the leader standing in the
         * enemy's row. */
        if (def_form_anim_rows_exchanged() ? obj->params->id == 0 : obj->params->id == 0x10) {
#else
        if (obj->params->id == 0x10) {
#endif
            obj->z_offset = -obj->z_offset;
        }
        obj->time = fightstg_battle.frames * obj->speed;
        obj->base.next_state(obj);
    case OBJECT_STATE_RUN:
        switch (obj->kind) {
        default:
            y = fightstg_math.wave(2, obj->time, obj->height);
            obj->params->pos.y = fightstg_math.wave(1, obj->time, obj->start_y) - y;
            break;
        case 4:
        case 5:
            obj->params->pos.y = obj->params->start_pos.y - fightstg_math.wave(2, obj->time, obj->height);
            break;
        case 6:
            obj->params->pos.y = fightstg_math.wave(0, obj->time, obj->params->start_pos.y);
            break;
        }
        if (obj->kind == 4) {
            obj->params->pos.z = obj->params->start_pos.z + fightstg_math.wave(0, obj->time, obj->z_offset);
        }
        if (obj->kind == 5) {
            obj->params->pos.z = obj->params->start_pos.z + obj->z_offset - fightstg_math.wave(0, obj->time, obj->z_offset);
        }
        obj->time += fightstg_battle.frames * obj->speed;
        if (obj->time >= 0x1000) {
            obj->base.next_state(obj);
        }
        break;
    case OBJECT_STATE_DONE:
        switch (obj->kind) {
        default:
            obj->params->pos.y = 0;
            break;
        case 4:
        case 5:
        case 6:
            obj->params->pos.y = obj->params->start_pos.y;
            break;
        }
        if (obj->kind == 4) {
            obj->params->pos.z = obj->params->start_pos.z + obj->z_offset;
        }
        if (obj->kind == 5) {
            obj->params->pos.z = obj->params->start_pos.z;
        }
        obj->base.next_state(obj);
        break;
    case OBJECT_STATE_END:
        break;
    }
}

OBJECT_V0(FightstgJump *) fightstg_jump_create(FightstgModelParams *target, s32 kind, s32 distance) {
    FightstgJump *obj = object_new(fightstg_jump_update, sizeof(FightstgJump), 0);

    obj->kind = kind;
    obj->params = target;
    obj->distance = distance;
    obj->start_y = target->pos.y;
    obj->height = fightstg_jump_params[kind - 1].height;
    obj->speed = fightstg_jump_params[kind - 1].speed;
    OBJECT_V0_RETURN(obj) /* PC_PORT: FINDINGS 8: callers use the object (v0) */
}

/* fightstg_move_create's object: moves a model (its parameters' pos) from `from` to `to`. */
typedef struct FightstgMove {
    /* 0x00 */ Object base;
    /* 0x50 */ FightstgModelParams *params;
    /* 0x54 */ FightstgVec from;
    /* 0x5A */ FightstgVec to;
    /* 0x60 */ s32 pos; /* position (0x1000 = done) */
    /* 0x64 */ s32 step; /* step: 0x1000 / frames */
} FightstgMove; /* size 0x68 */

void fightstg_move_update(FightstgMove *obj) {
    SVECTOR from;
    SVECTOR to;
    SVECTOR pos;
    s32 t;

    switch (obj->base.state) {
    case OBJECT_STATE_INIT:
    default:
        obj->pos = fightstg_battle.frames * obj->step;
        obj->base.next_state(obj);
    case OBJECT_STATE_RUN:
        t = obj->pos;
        from.vx = obj->from.x;
        from.vy = obj->from.y;
        from.vz = obj->from.z;
        to.vx = obj->to.x;
        to.vy = obj->to.y;
        to.vz = obj->to.z;
        fightstg_math.lerp(&from, &to, t, &pos);
        obj->params->pos.x = pos.vx;
        obj->params->pos.y = pos.vy;
        obj->params->pos.z = pos.vz;
        obj->pos += fightstg_battle.frames * obj->step;
        if (obj->pos >= 0x1000) {
            obj->params->pos.x = obj->to.x;
            obj->params->pos.y = obj->to.y;
            obj->params->pos.z = obj->to.z;
            obj->base.set_state(obj, OBJECT_STATE_END);
        }
        break;
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    }
}

FightstgMove *fightstg_move_create(FightstgModelParams *params, FightstgVec *to, s32 frames) {
    FightstgMove *obj = object_new(fightstg_move_update, sizeof(FightstgMove), 0);

    obj->params = params;
    obj->to = *to;
    obj->from = params->pos;
    obj->pos = 0;
    obj->step = 0x1000 / frames;
    return obj;
}

void fightstg_sound_update(FightstgSound *obj) {
    switch (obj->base.state) {
    case OBJECT_STATE_DONE:
    case OBJECT_STATE_END:
        break;
    case OBJECT_STATE_INIT:
    case OBJECT_STATE_RUN:
    default:
        obj->timer -= fightstg_battle.frames;
        if (obj->timer <= 0) {
            sound_module.key_off(obj->sound, obj->key);
            obj->base.set_state(obj, OBJECT_STATE_END);
        }
        break;
    }
}

/* Sound IDs, indexed by fightstg_sound_play's id. */
extern s32 fightstg_sounds[];

void *fightstg_sound_play(s32 id, s32 delay) {
    s32 sound;
    FightstgSound *obj;

    switch (id) {
    default:
        sound = fightstg_sounds[id];
        break;
    case 0x5A:
        sound = records_state.music;
        break;
    case 0x5B:
        sound_module.stop(0x20040006);
        return NULL;
    }
    if (delay == 0) {
        sound_module.play(sound);
        return NULL;
    }
    obj = object_new(fightstg_sound_update, sizeof(FightstgSound), 0);
    obj->sound = sound;
    obj->key = (s16)sound_module.play(sound);
    obj->timer = delay;
    return obj;
}

s32 fightstg_enemy_find_record(s32 id) {
    FightstgEnemyRecord *recs = (FightstgEnemyRecord *)cdload_module.files.get_file(0x1CF);
    s32 i;

    for (i = 0; recs[i].id != 0; i++) {
        if (recs[i].id == id) {
            return i;
        }
    }
    return -1;
}

FightstgEnemyRecord *fightstg_enemy_get_record(s32 id) {
    FightstgEnemyRecord *recs = (FightstgEnemyRecord *)cdload_module.files.get_file(0x1CF);
    s32 i;

    if ((i = fightstg_enemy_find_record(id)) >= 0) {
#ifdef PC_PORT
        def_enemy_override(&recs[i]); /* the port's table, in memory only: the disc's record is not edited */
#endif
        return &recs[i];
    }
#ifdef PC_PORT
    { /* a player form as an enemy (a dark form in a purification, a DRI stage): no record on the disc, the port's */
        FightstgEnemyRecord *made = def_purify_record(id);

        return made != NULL ? made : def_dri_record(id);
    }
#else
    return NULL;
#endif
}

void fightstg_events_add(FightstgNewEvent *event) {
    s32 i;
    s32 slot;

    for (i = 0, slot = -1; i < 99; i++) {
        if (fightstg_events.events[i].type == 0) {
            slot = i;
            break;
        }
    }
    if (slot != -1) {
        fightstg_events.events[slot].type = event->type;
        fightstg_events.events[slot].delay = event->delay;
        for (i = 0; i < 6; i++) {
            fightstg_events.events[slot].args[i] = event->args[i];
        }
    }
}

void fightstg_events_add_first(FightstgNewEvent *event) {
    s32 i;

    if (event->delay < 2) {
        event->delay = 2;
    }
    for (i = 0; i < 99; i++) {
        if (fightstg_events.events[i].type != 0) {
            fightstg_events.events[i].delay += event->delay;
        }
    }
    event->delay = 1;
    fightstg_events_add(event);
}

s32 fightstg_events_take_next(void) {
    FightstgEvent *event;
    s32 i;
    s32 min = 0x7FFF;
    s32 next = -1;
    s32 delay;
    s32 type;

    for (i = 0; i < 99; i++) {
        if (fightstg_events.events[i].type != 0) {
            if (next != -1 && fightstg_events.events[i].delay <= fightstg_events.events[next].delay
                && (fightstg_events.events[next].type == 2 || fightstg_events.events[next].type == 3)) {
                next = i;
                min = fightstg_events.events[i].delay;
            }
            if (fightstg_events.events[i].delay < min) {
                next = i;
                min = fightstg_events.events[i].delay;
            }
        }
    }
    if (next == -1) {
        return 0;
    }
    event = &fightstg_events.events[next];
    delay = event->delay;
    for (i = 0; i < 99; i++) {
        if (fightstg_events.events[i].type != 0) {
            fightstg_events.events[i].delay -= delay;
        }
    }
    fightstg_events.taken_type = event->type;
    switch (fightstg_events_take_modes[fightstg_events.taken_type]) {
    case 0:
        return 0;
    case -1:
        type = event->type;
        fightstg_events.taken = next;
        return type;
    default:
        type = event->type;
        event->type = 0;
        fightstg_events.taken = next;
        return type;
    }
}

s32 fightstg_events_find_from(s32 i) {
    s32 type = fightstg_events.find_type;

    if (type < 1 || type > 24) {
        return -1;
    }
    for (; i < 99; i++) {
        if (fightstg_events.events[i].type == type) {
            return i;
        }
    }
    return -1;
}

s32 fightstg_events_find(s32 type) {
    fightstg_events.find_type = type;
    return fightstg_events.found = fightstg_events_find_from(0);
}

s32 fightstg_events_find_next(void) {
    return fightstg_events.found = fightstg_events_find_from(fightstg_events.found + 1);
}

s32 fightstg_events_find_member(s32 type, u8 side, s32 member) {
    fightstg_events.find_type = type;
    fightstg_events.found = fightstg_events_find_from(0);
    while (fightstg_events.found >= 0) {
        if (fightstg_events.events[fightstg_events.found].args[0] == side
            && fightstg_events.events[fightstg_events.found].args[1] == member) {
            break;
        }
        fightstg_events.found = fightstg_events_find_from(fightstg_events.found + 1);
    }
    return fightstg_events.found;
}

void fightstg_events_remove_member(s32 *args) {
    s32 i;
    u8 kind = args[0];
    s32 id = args[1];

    for (i = 0; i < 99; i++) {
        if (fightstg_events.events[i].type != 0 && fightstg_events.events[i].args[0] == kind
            && fightstg_events.events[i].args[1] == id) {
            fightstg_events.events[i].type = 0;
        }
    }
}

/* Per formula of fightstg_events_get_delay: a base and the result's bounds (0: none). */
typedef struct FightstgDelayFormula {
    /* 0x0 */ s16 base;  /* base (random range, or factor) */
    /* 0x2 */ s16 min;   /* minimum */
    /* 0x4 */ s16 max;   /* maximum */
} FightstgDelayFormula; /* size 0x6 */

extern FightstgDelayFormula fightstg_events_delay_formulas[];

/* A value by formula kind for side's acting member (and the other side's): mostly random, from the stats
 * blocks of fightstg_rules_get_stats, bounded by fightstg_events_delay_formulas[kind]. */
s32 fightstg_events_get_delay(u8 side, s32 kind) {
    s32 value;
    s32 attack;
    s32 defense;

    switch (kind) {
    case 2: {
        s32 enemy = side != 0;
        FightstgStats *own = fightstg_rules.get_stats(side, 1, fightstg_battle.state.current[enemy]);

        value = pad_random.next() % fightstg_events_delay_formulas[kind].base + own->stats[2] * 10;
        break;
    }
    case 8:
        value = pad_random.next() % 8001;
        break;
    case 9: {
        s32 enemy = side != 0;
        FightstgStats *own = fightstg_rules.get_stats(side, 1, fightstg_battle.state.current[enemy]);
        FightstgStats *other =
            fightstg_rules.get_stats((side == 0) << 4, 0, fightstg_battle.state.current[1 - enemy]);

        value = pad_random.next() % fightstg_events_delay_formulas[kind].base;
        attack = (own->stats[2] + fightstg_events.status_power) * 8 + 3000;
        defense = (other->resists[8] + other->resists[4]) * 8;
        value += attack;
        value -= defense;
        break;
    }
    case 10: {
        s32 enemy = side != 0;
        FightstgStats *own = fightstg_rules.get_stats(side, 1, fightstg_battle.state.current[enemy]);
        FightstgStats *other =
            fightstg_rules.get_stats((side == 0) << 4, 0, fightstg_battle.state.current[1 - enemy]);

        value = pad_random.next() % fightstg_events_delay_formulas[kind].base;
        attack = (own->stats[2] + fightstg_events.status_power) * 8 + 3000;
        defense = (other->resists[9] + other->resists[3]) * 8;
        value += attack;
        value -= defense;
        break;
    }
    case 11: {
        s32 enemy = side != 0;
        FightstgStats *own = fightstg_rules.get_stats(side, 1, fightstg_battle.state.current[enemy]);
        FightstgStats *other =
            fightstg_rules.get_stats((side == 0) << 4, 0, fightstg_battle.state.current[1 - enemy]);

        value = pad_random.next() % fightstg_events_delay_formulas[kind].base;
        attack = (own->stats[2] + fightstg_events.status_power) * 8 + 1000;
        defense = (other->resists[10] + other->resists[2]) * 8;
        value += attack;
        value -= defense;
        break;
    }
    case 12: {
        s32 enemy = side != 0;
        FightstgStats *own = fightstg_rules.get_stats(side, 1, fightstg_battle.state.current[enemy]);

        value = pad_random.next() % fightstg_events_delay_formulas[kind].base + 2000 + own->stats[2] * 10;
        break;
    }
    default: {
        s32 enemy = side != 0;
        FightstgStats *own = fightstg_rules.get_stats(side, 1, fightstg_battle.state.current[enemy]);
        FightstgStats *other = fightstg_rules.get_stats(0x10 - side, 0, fightstg_battle.state.current[1 - enemy]);
        s32 root = 999;
#ifdef PC_PORT
        s32 own_speed = own->stats[4];
        s32 other_speed = other->stats[4];
        s32 product;
        s32 i;

        def_turn_speeds(side, &own_speed, &other_speed);
        product = own_speed * other_speed;
        for (i = 0; i < 10; i++) {
            root = (root + product / root) / 2;
        }
        value = fightstg_events_delay_formulas[kind].base * other_speed / root;
#else
        s32 product = own->stats[4] * other->stats[4];
        s32 i;

        for (i = 0; i < 10; i++) {
            root = (root + product / root) / 2;
        }
        value = fightstg_events_delay_formulas[kind].base * other->stats[4] / root;
#endif
        break;
    }
    }
    if (fightstg_events_delay_formulas[kind].min != 0 && value < fightstg_events_delay_formulas[kind].min) {
        value = fightstg_events_delay_formulas[kind].min;
    }
    if (fightstg_events_delay_formulas[kind].max != 0 && value > fightstg_events_delay_formulas[kind].max) {
        value = fightstg_events_delay_formulas[kind].max;
    }
    return value;
}

/* Event types of the status effects 0..2 (the last three bytes: for "all", with them). */
extern u8 fightstg_status_event_types[8];
/* Flags (FightstgMember.status) of the status effects 0..3. */
extern u8 fightstg_status_flags[4];

/* Cures a member's status effect, by the item / action `id` (0xBE..0xC5). One case per id (cross-jumped later;
 * their references decide effect's register). */
void fightstg_events_cure_status(u8 side, s32 member, s32 id) {
    FightstgMember *m;
    s32 effect;
    s32 i;
    s32 j;
    s32 enemy;

    switch (id) {
    case 0xBE:
    default:
        effect = 0;
        break;
    case 0xBF:
        effect = 0;
        break;
    case 0xC0:
        effect = 1;
        break;
    case 0xC1:
        effect = 1;
        break;
    case 0xC2:
        effect = 2;
        break;
    case 0xC3:
        effect = 2;
        break;
    case 0xC4:
        effect = 3;
        break;
    case 0xC5:
        effect = 3;
        break;
    }
    enemy = side != 0;
    m = fightstg_battle.state.members[enemy];
    if (m[member].digimon != 0) {
        m[member].status &= ~fightstg_status_flags[effect];
        if (effect != 3) {
            j = fightstg_events.find_member(fightstg_status_event_types[effect], side, member);
            if (j >= 0) {
                fightstg_events.events[j].type = 0;
            }
        } else {
            for (i = 0; i < 6; i++) {
                j = fightstg_events.find_member(fightstg_status_event_types[i], side, member);
                if (j >= 0) {
                    fightstg_events.events[j].type = 0;
                }
            }
        }
    }
}

void fightstg_events_add_party_turn(s32 delay) {
    fightstg_new_event.type = 2;
    fightstg_new_event.delay = delay;
    fightstg_new_event.args[0] = -1;
    fightstg_events_add(&fightstg_new_event);
}

void fightstg_events_add_enemy_turn(s32 delay) {
    fightstg_new_event.type = 3;
    fightstg_new_event.delay = delay;
    fightstg_new_event.args[0] = -1;
    fightstg_events_add(&fightstg_new_event);
}

void fightstg_events_end_battle(s8 result) {
    fightstg_new_event.type = 1;
    fightstg_new_event.delay = 1;
    fightstg_new_event.args[0] = -1;
    fightstg_events.result = result;
    fightstg_events_add_first(&fightstg_new_event);
}

void fightstg_events_add_escape(u8 side) {
    fightstg_new_event.type = 4;
    fightstg_new_event.delay = fightstg_events_get_delay(side, 1);
    fightstg_new_event.args[0] = side;
    fightstg_new_event.args[1] = fightstg_battle.state.current[side != 0];
    fightstg_events_add(&fightstg_new_event);
}

void fightstg_events_add_regen_end(u8 side) {
    s32 enemy = side != 0;
    s32 i = fightstg_events_find_member(5, side, enemy);
    s32 delay = fightstg_events_get_delay(side, 2);
    FightstgEvent *event;

    if (i >= 0) {
        event = &fightstg_events.events[i];
        event->delay = delay;
    } else {
        fightstg_new_event.type = 5;
        fightstg_new_event.delay = delay;
        fightstg_new_event.args[0] = side;
        fightstg_new_event.args[1] = fightstg_battle.state.current[enemy];
        fightstg_new_event.args[2] = 0xBD;
        fightstg_events_add(&fightstg_new_event);
    }
}

void fightstg_events_start_regen(u8 side, s32 arg1, s32 arg2) {
    s32 i = fightstg_events_find_member(6, side, arg1);
    FightstgEvent *event;

    if (i >= 0) {
        event = &fightstg_events.events[i];
        event->args[2] = 0xBD;
    } else {
        fightstg_new_event.type = 6;
        fightstg_new_event.delay = 1000;
        fightstg_new_event.args[0] = side;
        fightstg_new_event.args[1] = arg1;
        fightstg_new_event.args[2] = arg2;
        fightstg_events_add(&fightstg_new_event);
    }
}

void fightstg_events_set_field_end(s32 delay) {
    s32 i;

    fightstg_events.find_type = 7;
    i = fightstg_events_find_from(0);
    if (delay > 0x7FFF) {
        delay = 0x7FFF;
    }
    if (i >= 0) {
        fightstg_events.events[i].delay = delay;
    } else {
        fightstg_new_event.type = 7;
        fightstg_new_event.delay = delay;
        fightstg_new_event.args[0] = -1;
        fightstg_events_add(&fightstg_new_event);
    }
}

void fightstg_events_start_poison(u8 side, s32 member, s32 power) {
    s32 i = fightstg_events_find_member(9, side, member);
    u8 enemy;
    FightstgMember *m;
    FightstgEvent *event;

    if (i >= 0) {
        event = &fightstg_events.events[i];
        event->args[2] = power;
    } else {
        fightstg_new_event.type = 9;
        fightstg_new_event.delay = 1000;
        fightstg_new_event.args[0] = side;
        fightstg_new_event.args[1] = member;
        fightstg_new_event.args[2] = power;
        fightstg_events_add(&fightstg_new_event);
    }
    enemy = side != 0;
    m = &fightstg_battle.state.members[enemy][member];
    m->status |= 1;
}

void fightstg_events_start_paralysis(u8 side, s32 arg1, u8 power) {
    s32 enemy = side != 0;
    s32 i = fightstg_events_find_member(10, side, fightstg_battle.state.current[enemy]);
    s32 delay;
    FightstgMember *member;

    fightstg_events.status_power = power;
    delay = fightstg_events_get_delay(side, 9);
    if (i >= 0) {
        fightstg_events.events[i].delay = delay;
    } else {
        fightstg_new_event.type = 10;
        fightstg_new_event.delay = delay;
        fightstg_new_event.args[0] = side;
        fightstg_new_event.args[1] = fightstg_battle.state.current[enemy];
        fightstg_events_add(&fightstg_new_event);
    }
    member = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
    member->paralysis_power = power;
    member->status |= 2;
}

void fightstg_events_start_confusion(u8 side, s32 arg1, u8 power) {
    s32 enemy = side != 0;
    s32 i = fightstg_events_find_member(11, side, fightstg_battle.state.current[enemy]);
    s32 delay;
    FightstgMember *member;

    if (arg1 == 0) {
        delay = fightstg_events_get_delay(side, 8);
    } else {
        fightstg_events.status_power = power;
        delay = fightstg_events_get_delay(side, 10);
    }
    if (i >= 0) {
        fightstg_events.events[i].delay = delay;
    } else {
        fightstg_new_event.type = 11;
        fightstg_new_event.args[0] = side;
        fightstg_new_event.delay = delay;
        fightstg_new_event.args[1] = fightstg_battle.state.current[enemy];
        fightstg_events_add(&fightstg_new_event);
    }
    member = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
    member->confusion_power = power;
    member->status |= 4;
}

void fightstg_events_start_sleep(u8 side, s32 arg1, u8 power) {
    s32 enemy = side != 0;
    s32 i = fightstg_events_find_member(12, side, fightstg_battle.state.current[enemy]);
    s32 delay;
    FightstgMember *member;

    fightstg_events.status_power = power;
    delay = fightstg_events_get_delay(side, 11);
    if (i >= 0) {
        fightstg_events.events[i].delay = delay;
    } else {
        fightstg_new_event.type = 12;
        fightstg_new_event.delay = delay;
        fightstg_new_event.args[0] = side;
        fightstg_new_event.args[1] = fightstg_battle.state.current[enemy];
        fightstg_events_add(&fightstg_new_event);
    }
    member = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
    member->sleep_power = power;
    member->status |= 8;
}

void fightstg_events_start_modifier(u8 side, s32 member, s32 kind, s32 tech) {
    s32 i = fightstg_events_find_member(fightstg_modifier_event_types[kind], side, member);
    s32 delay;

#ifdef PC_PORT
    if (def_mod_start_event()) {
        return;
    }
#endif
    if (tech != 0) {
        delay = fightstg_events_get_delay(side, 12);
    } else {
        delay = fightstg_events_get_delay(side, 8);
    }

    if (i >= 0) {
        fightstg_events.events[i].delay = delay;
    } else {
        fightstg_new_event.type = fightstg_modifier_event_types[kind];
        fightstg_new_event.delay = delay;
        fightstg_new_event.args[0] = side;
        fightstg_new_event.args[1] = member;
        fightstg_new_event.args[2] = kind;
        fightstg_events_add(&fightstg_new_event);
    }
}

/* Event types, for kinds 0xD and 0xE (fightstg_events_start_seal). */
extern s32 fightstg_seal_event_types[2];

void fightstg_events_start_seal(s32 id) {
    RecordsTechnique *rec = &records_techniques[id - 1];
    s32 kind = 0;
    s32 member;
    s32 i;
    s32 delay;
    FightstgMember *m;
    FightstgEvent *event;

    if (rec->kind != 0xD) {
        kind = rec->kind == 0xE;
    }
    member = fightstg_battle.state.current[0];
    i = fightstg_events_find_member(fightstg_seal_event_types[kind], 0, member);
    delay = (pad_random.next() % 101 + 100) * rec->effect_power;
    if (i >= 0) {
        event = &fightstg_events.events[i];
        event->delay = delay;
    } else {
        fightstg_new_event.type = fightstg_seal_event_types[kind];
        fightstg_new_event.delay = delay;
        fightstg_new_event.args[0] = 0;
        fightstg_new_event.args[1] = member;
        fightstg_new_event.args[2] = kind;
        fightstg_events_add(&fightstg_new_event);
    }
    m = &fightstg_battle.state.members[0][member];
    if (kind == 0) {
        m->status |= 0x10;
    } else {
        m->status |= 0x20;
    }
}

void fightstg_events_add_counter_stance(s32 arg0) {
    fightstg_new_event.type = 8;
    fightstg_new_event.delay = 0x7FFF;
    fightstg_new_event.args[0] = 0;
    fightstg_new_event.args[1] = fightstg_battle.state.current[0];
    fightstg_new_event.args[2] = arg0;
    fightstg_events_add(&fightstg_new_event);
}

void fightstg_events_add_gauge_full(void) {
    fightstg_new_event.type = 0x12;
    fightstg_new_event.delay = 0;
    fightstg_events_add(&fightstg_new_event);
}

void fightstg_events_start_blast(s32 arg0) {
    fightstg_new_event.type = 0x13;
    fightstg_new_event.delay = fightstg_events_get_delay(0, arg0 + 3);
    fightstg_new_event.args[0] = 0;
    fightstg_new_event.args[1] = fightstg_battle.state.current[0];
    fightstg_events_add(&fightstg_new_event);
}

void fightstg_events_add_knockout(u8 side) {
    fightstg_new_event.type = 0x14;
    fightstg_new_event.delay = 1;
    fightstg_new_event.args[0] = side;
    fightstg_new_event.args[1] = fightstg_battle.state.current[side != 0];
    fightstg_events_add_first(&fightstg_new_event);
}

void fightstg_events_add_boost_end(void) {
    fightstg_new_event.type = 0x15;
    fightstg_new_event.delay = 1;
    fightstg_new_event.args[0] = 0;
    fightstg_new_event.args[1] = fightstg_battle.state.current[0];
    fightstg_events_add_first(&fightstg_new_event);
}

void fightstg_events_add_revert(void) {
    fightstg_new_event.type = 0x16;
    fightstg_new_event.delay = 1;
    fightstg_new_event.args[0] = 0;
    fightstg_new_event.args[1] = fightstg_battle.state.current[0];
    fightstg_events_add_first(&fightstg_new_event);
}

void fightstg_events_add_boss_entrance(void) {
    fightstg_new_event.type = 0x17;
    fightstg_new_event.delay = 1;
    fightstg_events_add_first(&fightstg_new_event);
}

void fightstg_events_start_final_phase(void) {
    FightstgEnemyRecord *rec;

    fightstg_new_event.type = 0x18;
    fightstg_new_event.delay = 3000;
    fightstg_events_add(&fightstg_new_event);
    rec = fightstg_enemy_records.get(0x1D3);
    fightstg_battle.state.final_phase = 1;
    fightstg_battle.state.members[1][0].modifiers[1] = -rec->stats[1] >> 1;
    fightstg_battle.state.members[1][0].modifiers[3] = -rec->stats[2] >> 1;
}

void fightstg_events_end_final_phase(void) {
    s32 i = fightstg_events.find(0x18);

    if (i >= 0) {
        fightstg_events.events[i].delay = 1;
    }
}

void fightstg_action_try_poison(void) {
    s32 enemy = fightstg_action.side != 0;
    FightstgMember *member = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
    s32 value = fightstg_rules.roll_poison(fightstg_action.side, fightstg_action.tech);

    if (value != 0) {
#ifdef PC_PORT
        if (def_on()) {
            value = def_poison_dot(&fightstg_rules.stats[0], value);
        }
#endif
        if (member->boosted != 0) {
            value *= 2;
        }
        fightstg_action.effects[2] = value;
    }
}

void fightstg_action_try_paralysis(void) {
    FightstgAction *state = &fightstg_action;
    FightstgRules *rules = &fightstg_rules;
    s32 enemy = state->side != 0;
    FightstgMember *member = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
    RecordsTechnique *rec;
    s32 value;

    if (rules->roll_paralysis(state->side, state->tech) != 0) {
        rec = &records_techniques[state->tech - 1];
        if (rec->kind < 2) {
            value = rules->stats[0].paralysis_power;
        } else {
            value = rec->effect_power;
        }
        if (member->boosted != 0) {
            value *= 2;
        }
        fightstg_action.effects[3] = value;
    }
}

void fightstg_action_try_confusion(void) {
    FightstgAction *state = &fightstg_action;
    FightstgRules *rules = &fightstg_rules;
    s32 enemy = state->side != 0;
    FightstgMember *member = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
    RecordsTechnique *rec;
    s32 value;

    if (rules->roll_confusion(state->side, state->tech) != 0) {
        rec = &records_techniques[state->tech - 1];
        if (rec->kind < 2) {
            value = rules->stats[0].confusion_power;
        } else {
            value = rec->effect_power;
        }
        if (member->boosted != 0) {
            value *= 2;
        }
        fightstg_action.effects[4] = value;
    }
}

void fightstg_action_try_sleep(void) {
    s32 enemy = fightstg_action.side != 0;
    FightstgMember *member = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
    RecordsTechnique *rec;
    s32 value;

    if (fightstg_rules.roll_sleep(fightstg_action.side, fightstg_action.tech) != 0) {
        rec = &records_techniques[fightstg_action.tech - 1];
        value = rec->effect_power;
        if (member->boosted != 0) {
            value *= 2;
        }
        fightstg_action.effects[rec->kind] = value;
    }
}

void fightstg_action_try_knockout(void) {
    FightstgAction *state = &fightstg_action;

    if (fightstg_rules.roll_knockout(state->side, state->tech) != 0) {
        state->effects[6] = 1;
    }
}

void fightstg_action_hit_again(void) {
    RecordsTechnique *rec;
    s32 i;
    s32 count = 3;
    s32 side;
    FightstgBattle *battle = &fightstg_battle;

    rec = &records_techniques[fightstg_action.tech - 1];
    side = fightstg_action.side;
    if (rec->kind >= 2) {
        count = rec->hit_count;
    }
    fightstg_action.strikes = 1;
    fightstg_action.hit_count = fightstg_action.hits[0];
    if (side == 0 && battle->state.type == 6 && fightstg_action.hits[0] == 0) {
        fightstg_action.strikes = count;
    } else {
        for (i = 1; i < count; i++) {
            if (fightstg_rules.roll_hit(fightstg_action.side, fightstg_action.tech) != 0) {
                fightstg_action.hits[fightstg_action.strikes++] = 1;
                fightstg_action.hit_count++;
            } else {
                fightstg_action.hits[fightstg_action.strikes++] = 0;
            }
        }
    }
    fightstg_action.effects[9] = 1;
}

void fightstg_action_try_drain(void) {
    FightstgAction *state = &fightstg_action;
    FightstgRules *rules = &fightstg_rules;
    s32 enemy = state->side != 0;
    FightstgMember *member = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
    RecordsTechnique *rec;
    s32 value;

    if (rules->roll_drain(state->side, state->tech) != 0) {
        rec = &records_techniques[state->tech - 1];
        if (rec->kind == 8) {
            value = rec->effect_power;
            if (member->boosted != 0) {
                value *= 2;
            }
            state->drained = state->damage * value / 128;
        } else {
            value = rules->stats[0].drain_power;
            if (member->boosted != 0) {
                value *= 2;
            }
            state->drained = state->damage * value / 128;
        }
        fightstg_action.effects[8] = 1;
    }
}

void fightstg_action_mark_counter_tech(void) {
    RecordsTechnique *rec = &records_techniques[fightstg_action.tech - 1];

    fightstg_action.effects[rec->kind] = rec->kind;
}

void fightstg_action_mark_critical_tech(void) {
    RecordsTechnique *rec = &records_techniques[fightstg_action.tech - 1];

    fightstg_action.effects[rec->kind] = rec->kind;
}

/* rules is a const pointer (the US decomp's shape): the front end substitutes its value into the call, so the original's
 * &fightstg_rules stays in a register and roll_steal is loaded from it after the branch (lw 0xB8(v0)). A plain pointer
 * local or the direct call folds the address into the load (97.4%). Only this function reads roll_steal. */
void fightstg_action_try_steal(void) {
    FightstgRules *const rules = &fightstg_rules;
    FightstgMember *members = fightstg_battle.state.members[1];
    RecordsTechnique *rec;

    if (members[fightstg_battle.state.current[1]].item > 0 && rules->roll_steal(0, fightstg_action.tech) != 0) {
        rec = &records_techniques[fightstg_action.tech - 1];
        fightstg_action.effects[rec->kind] = 1;
        cdload_module.queue_file(records_language + 0x6A);
    }
}

void fightstg_action_lower_attack(void) {
    FightstgAction *state = &fightstg_action;
    s16 enemy = state->side != 0;
    RecordsTechnique *rec;

    if (enemy || FIGHTSTG_STAT_BLOCKED(8) == 0) {
        rec = &records_techniques[state->tech - 1];
#ifdef PC_PORT
        if (!def_mod_apply_tech(0x10 - state->side, fightstg_battle.state.current[1 - enemy], 0, state->tech,
                                -rec->effect_power, enemy ? 0 : 8)) {
            fightstg_rules.change_modifier(0x10 - state->side, fightstg_battle.state.current[1 - enemy], 0,
                                           enemy ? -rec->effect_power : FIGHTSTG_DEBUFF(8, -rec->effect_power));
            fightstg_events_start_modifier(0x10 - state->side, fightstg_battle.state.current[1 - enemy], 0, state->tech);
        }
#else
        fightstg_rules.change_modifier(0x10 - state->side, fightstg_battle.state.current[1 - enemy], 0, -rec->effect_power);
        fightstg_events_start_modifier(0x10 - state->side, fightstg_battle.state.current[1 - enemy], 0, state->tech);
#endif
        state->effects[rec->kind] = rec->effect_power;
    }
}

void fightstg_action_lower_defense(void) {
    FightstgAction *state = &fightstg_action;
    s32 other = state->side == 0;
    RecordsTechnique *rec = &records_techniques[state->tech - 1];

#ifdef PC_PORT
    if (state->side != 0 || FIGHTSTG_STAT_BLOCKED(9) == 0) {
        if (!def_mod_apply_tech(0x10 - state->side, fightstg_battle.state.current[other], 1, state->tech,
                                -rec->effect_power, state->side != 0 ? 0 : 9)) {
            fightstg_rules.change_modifier(0x10 - state->side, fightstg_battle.state.current[other], 1, -rec->effect_power);
            fightstg_events_start_modifier(0x10 - state->side, fightstg_battle.state.current[other], 1, state->tech);
        }
        state->effects[rec->kind] = rec->effect_power;
    }
#else
    fightstg_rules.change_modifier(0x10 - state->side, fightstg_battle.state.current[other], 1, -rec->effect_power);
    fightstg_events_start_modifier(0x10 - state->side, fightstg_battle.state.current[other], 1, state->tech);
    state->effects[rec->kind] = rec->effect_power;
#endif
}

void fightstg_action_drain_mp(void) {
    FightstgMember *member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
    FightstgAction *state = &fightstg_action;
    RecordsTechnique *rec = &records_techniques[state->tech - 1];

    if (member->mp != 0) {
        state->drained = member->max_mp * rec->effect_power / 128;
        if (member->mp < state->drained) {
            state->drained = member->mp;
        }
        state->effects[rec->kind] = state->drained;
    }
}

void fightstg_action_try_lower_stat(void) {
    s32 i = pad_random.next() % 2;
    FightstgAction *state = &fightstg_action;
    RecordsTechnique *rec;
    GamestateRecord *digimon;

    if (fightstg_rules.roll_lower_stat(state->side, state->tech) != 0) {
        rec = &records_techniques[state->tech - 1];
        digimon = gamestate_data.funcs.get_record(gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]));
        digimon->stats.penalties[i] += rec->effect_power;
        state->effects[rec->kind] = 1 << i;
    }
}

void fightstg_action_try_lower_stats(void) {
    RecordsTechnique *rec = &records_techniques[fightstg_action.tech - 1];
    GamestateRecord *digimon = gamestate_data.funcs.get_record(gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]));
    s32 i;

    for (i = 0; i < 3; i++) {
        if (fightstg_rules.roll_lower_stat(fightstg_action.side, fightstg_action.tech) != 0) {
            digimon->stats.penalties[i] += rec->effect_power;
            fightstg_action.effects[rec->kind] |= 1 << i;
        }
    }
}

void fightstg_action_try_lower_all_stats(void) {
    RecordsTechnique *rec = &records_techniques[fightstg_action.tech - 1];
    GamestateRecord *digimon = gamestate_data.funcs.get_record(gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]));
    s32 i;

    if (fightstg_rules.roll_lower_stat(fightstg_action.side, fightstg_action.tech) != 0) {
        for (i = 0; i < 3; i++) {
            digimon->stats.penalties[i] += rec->effect_power;
        }
        fightstg_action.effects[rec->kind] = 7;
    }
}

void fightstg_action_try_seal(void) {
    RecordsTechnique *rec;

    if (fightstg_rules.roll_switch_seal(fightstg_action.side, fightstg_action.tech) != 0) {
        rec = &records_techniques[fightstg_action.tech - 1];
        fightstg_action.effects[rec->kind] = 1;
    }
}

void fightstg_action_double_strike(void) {
    s32 ids[2] = { 0x1B9, 0x1BA };
    s32 i;

    fightstg_action.hit_count = 0;
    fightstg_action.strikes = 2;
    for (i = 0; i < 2; i++) {
        if (fightstg_rules.roll_special_hit(fightstg_action.side, fightstg_action.tech) != 0) {
            fightstg_action.hits[i] = 1;
            fightstg_action.damages[i] = fightstg_rules.get_special_damage(fightstg_action.side, ids[i]);
            fightstg_action.hit_count++;
        }
    }
    fightstg_action.effects[9] = 1;
}

void fightstg_action_cut_hp(void) {
    RecordsTechnique *rec;

    if (fightstg_battle.state.type == 2) {
        rec = &records_techniques[fightstg_action.tech - 1];
        fightstg_action.effects[rec->kind] = 1;
    } else {
        fightstg_action.hits[0] = fightstg_rules.roll_hit(fightstg_action.side, fightstg_action.tech);
        fightstg_action.strikes++;
        fightstg_action.damage = fightstg_rules.get_damage(fightstg_action.side, fightstg_action.tech);
    }
}

/* Applies the action's effect by its kind (records_techniques's kind). */
void fightstg_action_apply_effect(void) {
    RecordsTechnique *rec = &records_techniques[fightstg_action.tech - 1];

    switch (rec->kind) {
    case 2:
        fightstg_action_try_poison();
        break;
    case 3:
        fightstg_action_try_paralysis();
        break;
    case 4:
        fightstg_action_try_confusion();
        break;
    case 5:
        fightstg_action_try_sleep();
        break;
    case 6:
        fightstg_action_try_knockout();
        break;
    case 8:
        fightstg_action_try_drain();
        break;
    case 12:
        fightstg_action_try_steal();
        break;
    case 26:
        fightstg_action_lower_attack();
        break;
    case 11:
        fightstg_action_mark_critical_tech();
        break;
    case 27:
        fightstg_action_lower_defense();
        break;
    case 29:
        fightstg_action_drain_mp();
        break;
    case 32:
        fightstg_action_try_lower_stat();
        break;
    case 33:
        fightstg_action_try_lower_stats();
        break;
    case 34:
        fightstg_action_try_lower_all_stats();
        break;
    case 13:
        fightstg_action_try_seal();
        break;
    }
}

/* Runs action `id` (record of records_techniques) for `side`: the hit, then its effects (fightstg_action). */
void fightstg_action_run(u8 side, s32 id) {
    RecordsTechnique *rec;

    heap_funcs.bzero(&fightstg_action, 0x68); /* PC_PORT: a byte count: the state before `run` (pointer-free) */
    rec = &records_techniques[id - 1];
    fightstg_action.side = side;
    fightstg_action.tech = id;
    if (rec->kind == 31) {
        fightstg_action_double_strike();
    } else if (rec->kind == 35) {
        fightstg_action_cut_hp();
    } else {
        if (rec->kind == 10 &&
            (side == 0 ||
             fightstg_enemy_records.get((fightstg_battle.state.members[1] + fightstg_battle.state.current[1])->digimon)->tech != id)) {
            fightstg_action_mark_counter_tech();
            return;
        }
        switch (rec->element) {
        case 2:
            fightstg_action.hits[0] = fightstg_rules.roll_hit(side, id);
            fightstg_action.strikes++;
            fightstg_action.damage = fightstg_rules.get_damage(side, id);
            if (side == 0) {
                if (rec->kind < 2) {
                    if (rec->anim_script != 0xB && rec->anim_script != 0xC) {
                        if (fightstg_rules.stats[0].multi_hit != 0) {
                            fightstg_action_hit_again();
                        } else if (fightstg_action.hits[0] != 0) {
                            if (fightstg_rules.stats[0].poison_chance != 0) {
                                fightstg_action_try_poison();
                            }
                            if (fightstg_rules.stats[0].paralysis_chance != 0) {
                                fightstg_action_try_paralysis();
                            }
                            if (fightstg_rules.stats[0].confusion_chance != 0) {
                                fightstg_action_try_confusion();
                            }
                            if (fightstg_rules.stats[0].knockout_chance != 0) {
                                fightstg_action_try_knockout();
                            }
                            if (fightstg_rules.stats[0].drain_chance != 0) {
                                fightstg_action_try_drain();
                            }
                        }
                    }
                } else if (rec->kind == 9) {
                    fightstg_action_hit_again();
                } else if (fightstg_action.hits[0] != 0) {
                    fightstg_action_apply_effect();
                }
            } else if (rec->kind >= 2) {
                if (rec->kind == 9) {
                    fightstg_action_hit_again();
                } else if (fightstg_action.hits[0] != 0) {
                    fightstg_action_apply_effect();
                }
            }
            break;
        case 3:
            fightstg_action.hits[0] = fightstg_rules.roll_special_hit(side, id);
            fightstg_action.strikes++;
            fightstg_action.damage = fightstg_rules.get_special_damage(side, id);
#ifdef PC_PORT
            /* A physical blow the definitive edition casts as magic (def_tech_was_physical) keeps the accessories'
             * on-hit chances the physical branch above gives it. Not the multi-hit: fightstg_action_hit_again rolls
             * the physical formula, so a second hit would land at physical damage. */
            if (side == 0 && rec->kind < 2 && rec->anim_script != 0xB && rec->anim_script != 0xC &&
                fightstg_action.hits[0] != 0 && def_tech_was_physical(id)) {
                if (fightstg_rules.stats[0].poison_chance != 0) {
                    fightstg_action_try_poison();
                }
                if (fightstg_rules.stats[0].paralysis_chance != 0) {
                    fightstg_action_try_paralysis();
                }
                if (fightstg_rules.stats[0].confusion_chance != 0) {
                    fightstg_action_try_confusion();
                }
                if (fightstg_rules.stats[0].knockout_chance != 0) {
                    fightstg_action_try_knockout();
                }
                if (fightstg_rules.stats[0].drain_chance != 0) {
                    fightstg_action_try_drain();
                }
            }
#endif
            if (rec->kind >= 2) {
                if (rec->kind == 9) {
                    fightstg_action_hit_again();
                } else if (fightstg_action.hits[0] != 0) {
                    fightstg_action_apply_effect();
                }
            }
            break;
        }
    }
    if (fightstg_battle.state.type != 0) {
        fightstg_action.damage = wfightmn_cap_damage(side, fightstg_action.damage,
                                                   fightstg_action.effects[9] ? fightstg_action.hit_count : 0);
    }
#ifdef PC_PORT
    fightstg_action.damage = def_super_damage(side, fightstg_action.damage); /* a Super Rare takes half (the hunt's rhythm) */
    fightstg_action.damage = def_super_bell_damage(side, fightstg_action.damage); /* Demon Bell: halved direct damage */
#endif
}

/* Sets the frames of this tick (frames) by the battle speed. */
void fightstg_battle_update_speed(void) {
    FightstgSpeed *speed = &fightstg_battle.speed;

    switch (speed->mode) {
    case 0:
    default:
        fightstg_battle.frames = gfx_module.funcs.get_frame_ticks();
        break;
    case 1:
        fightstg_battle.frames = 0;
        break;
    case 2:
        speed->frames += gfx_module.funcs.get_frame_ticks();
        fightstg_battle.frames = 0;
        while (speed->frames >= 5) {
            fightstg_battle.frames++;
            speed->frames -= 4;
        }
        break;
    case 3:
        fightstg_battle.frames = gfx_module.funcs.get_frame_ticks() * 2;
        break;
    }
}

void fightstg_battle_set_speed(s32 mode) {
    fightstg_battle.speed.mode = mode;
    fightstg_battle.speed.frames = 0;
    fightstg_battle_update_speed();
}

/* Projects `in` with the camera: out = screen x, y and the OT depth (z / 4) >> (16 - the layer's depth bits). */
void fightstg_battle_to_screen(GfxLayer *layer, SVECTOR *in, SVECTOR *out) {
    SVECTOR sxy;
    MATRIX m;
    s32 z;
    s32 shift = 0x10 - layer->get_ot_bits(layer);

    gte_MulMatrix0(&GsWSMATRIX, &message_identity_matrix, &m);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldlv0(message_identity_matrix.t);
    gte_rt();
    gte_stlvnl(m.t);
    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    gte_ldv0_u(in);
    gte_rtps();
    gte_stsxy(&sxy);
    gte_stszotz(&z);
    out->vx = sxy.vx;
    out->vy = sxy.vy;
    out->vz = z >> shift;
}

/* Draws a gouraud quad on layer `layer_id` at OT entry `depth`; semi: semi-transparent (with a draw mode
 * packet after it). */
void fightstg_battle_draw_quad(s32 layer_id, s32 depth, DVECTOR *pos, CVECTOR *colors, s32 semi) {
    GfxLayer *layer = gfx_module.funcs.get_layer(layer_id);
    u32 *ot = layer->get_ot_entry(layer, depth);
    POLY_G4 *poly = gfx_module.funcs.get_packet();
    DR_TPAGE *tpage;

    *(CVECTOR *)&poly->r0 = colors[0];
    *(CVECTOR *)&poly->r1 = colors[1];
    *(CVECTOR *)&poly->r2 = colors[2];
    *(CVECTOR *)&poly->r3 = colors[3];
    setPolyG4(poly);
    if (semi) {
        setSemiTrans(poly, 1);
    }
    poly->x0 = pos[0].vx;
    poly->x1 = pos[1].vx;
    poly->x2 = pos[2].vx;
    poly->x3 = pos[3].vx;
    poly->y0 = pos[0].vy;
    poly->y1 = pos[1].vy;
    poly->y2 = pos[2].vy;
    poly->y3 = pos[3].vy;
    addPrim(ot, poly);
    poly++;
    if (semi) {
        tpage = (DR_TPAGE *)poly;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000245;
        addPrim(ot, tpage);
        poly = (POLY_G4 *)(tpage + 1);
    }
    gfx_module.funcs.set_packet(poly);
}

void fightstg_battle_draw_quad_opaque(s32 layer_id, s32 depth, void *pos, void *colors) {
    fightstg_battle_draw_quad(layer_id, depth, pos, colors, 0);
}

void fightstg_battle_draw_quad_semi(s32 layer_id, s32 depth, void *pos, void *colors) {
    fightstg_battle_draw_quad(layer_id, depth, pos, colors, 1);
}

void *fightstg_models_get(s32 id) {
    FightstgModels *cache = &fightstg_models;
    FightstgModelFile *file;
    FightstgModelEntry *entry;
    FightstgModelRecordA *recs_a;
    FightstgModelRecordB *recs_b;
    void *rec;
    s32 index;

    if (id == fightstg_models.id) {
        if (cache->is_b != 0) {
            return cache->record_b;
        }
        return cache->record;
    }
    file = (FightstgModelFile *)cdload_module.files.get_file(0x1CC);
    entry = (FightstgModelEntry *)((u8 *)file + file->entries);
    recs_a = (FightstgModelRecordA *)((u8 *)file + file->records_a);
    recs_b = (FightstgModelRecordB *)((u8 *)file + file->records_b);
    for (; entry->id != 0; entry++) {
        if (entry->id == id) {
            fightstg_models.id = id;
            fightstg_models.index = index = entry->index;
            fightstg_models.kind = entry->kind;
            fightstg_models.is_b = entry->kind >= 0x3A;
            if (fightstg_models.is_b != 0) {
                rec = &recs_b[index];
                fightstg_models.record = rec;
                fightstg_models.record_b = rec;
                return rec;
            }
            rec = &recs_a[index];
            fightstg_models.record = rec;
            fightstg_models.record_b = rec;
            return rec;
        }
    }
    return NULL;
}

void fightstg_models_select(s32 i) {
    FightstgModelFile *file = (FightstgModelFile *)cdload_module.files.get_file(0x1CC);
    FightstgModelEntry *entry = (FightstgModelEntry *)((u8 *)file + file->entries) + i;
    FightstgModelRecordA *recs_a = (FightstgModelRecordA *)((u8 *)file + file->records_a);
    FightstgModelRecordB *recs_b = (FightstgModelRecordB *)((u8 *)file + file->records_b);
    FightstgModels *cache = &fightstg_models;
    void *rec;
    s32 index;

    cache->id = entry->id;
    cache->index = index = entry->index;
    cache->kind = entry->kind;
    cache->is_b = entry->kind >= 0x3A;
    if (cache->is_b == 0) {
        rec = &recs_a[index];
    } else {
        rec = &recs_b[index];
    }
    cache->record = rec;
    cache->record_b = rec;
}

u8 *fightstg_models_get_texture_anim(s32 id) {
    FightstgModelFile *file = (FightstgModelFile *)cdload_module.files.get_file(0x1CC);

    return PTR_ADD(u8 *, ((FightstgModelRecordA *)fightstg_models_get(id))->texture_anims - file->base, file);
}

/* The first and last entry of file 0x1CC whose record is of type `type` (0: A, 1: B). */
void fightstg_models_get_range(s32 type, s32 *min, s32 *max) {
    FightstgModelFile *file = (FightstgModelFile *)cdload_module.files.get_file(0x1CC);
    FightstgModelEntry *entry = (FightstgModelEntry *)((u8 *)file + file->entries);
    s32 first = 0xFF;
    s32 last = 0;
    s32 i;
    s32 is_b;

    for (i = 0; entry->id != 0; i++, entry++) {
        is_b = entry->kind >= 0x3A;
        if (is_b == type) {
            if (i < first) {
                first = i;
            }
            if (last < i) {
                last = i;
            }
        }
    }
    *min = first;
    *max = last;
}

/* What records_funcs.get_item(item)->data points to, as fightstg_rules_get_stats reads it: one view of
 * RecordsWeapon, RecordsArmor and RecordsAccessory (include/records.h), as the function reads them all through one
 * pointer. */
typedef struct FightstgItemBonus {
    /* 0x00 */ u8 unk_00[0x6];
    /* 0x06 */ u16 value;  /* value */
    /* 0x08 */ u8 kind;   /* kind (0x11..0x15: FightstgStats.resists[7..11]) */
    /* 0x09 */ u8 unk_09[0x5];
    /* 0x0E */ u8 accuracy;
    /* 0x0F */ u8 unk_0F;
    /* 0x10 */ u8 status_chance;
    /* 0x11 */ u8 status_power;
    /* 0x12 */ u8 strong_type;
} FightstgItemBonus;

/* An enemy's stats: its record's, scaled by the fight's strength for it, plus the member's modifiers. */
static inline void fightstg_rules_get_enemy_stats(FightstgStats *out, FightstgMember *m, s32 member) {
    FightstgEnemyRecord *enemy;
    s32 j;

    enemy = fightstg_enemy_records.get(m->digimon);
    out->level = records_state.enemies[member].level;
    for (j = 0; j < 5; j++) {
        out->stats[j] = enemy->stats[j] * records_state.enemies[member].stat_scale / 16;
    }
#ifdef PC_PORT
    def_finale_enemy_stats(out->stats); /* Extended Final Act: Dark Galacticmon Limit Break (~2000) */
    out->stats[4] = (s16)def_super_speed(out->stats[4]); /* Super Hard: past 999 the enemy acts twice */
#endif
    for (j = 0; j < 12; j++) {
        out->resists[j] = enemy->resists[j];
    }
    out->status = m->status;
    if (m->modifiers[0] != 0) {
        out->stats[0] += m->modifiers[0];
    }
    if (m->modifiers[1] != 0) {
        out->stats[1] += m->modifiers[1];
    }
    if (m->modifiers[2] != 0) {
        out->stats[4] += m->modifiers[2];
    }
    if (m->modifiers[3] != 0) {
        out->stats[2] += m->modifiers[3];
    }
    out->type = enemy->type;
    out->power_up = m->power_up;
}

/* Fills the stats block of a combatant (first: fightstg_rules.stats[0], else [1]): a party member's
 * (side 0) from its Digimon, its current form and its items; an enemy's from its record. */
FightstgStats *fightstg_rules_get_stats(u8 side, s32 first, s32 member) {
    GamestateStats stats;
    FightstgStats *out;
    FightstgMember *m;
    RecordsDigimon *rec;
    RecordsDigimon *form;
    GamestateRecord *digimon;
    RecordsItem *entry;
    FightstgItemBonus *item;
    s32 id;
    s16 *p;
    s32 n;
    s32 found;
    s32 i;

    if (first) {
        out = &fightstg_rules.stats[0];
    } else {
        out = &fightstg_rules.stats[1];
    }
    heap_funcs.bzero(out, sizeof(FightstgStats));
    if (side == 0) {
        m = &fightstg_battle.state.members[0][member];
        id = gamestate_data.funcs.get_party_member(member);
        rec = &records_digimon[id];
        gamestate_data.funcs.get_stats(id, &stats);
        for (i = 7; i < 12; i++) {
            out->resists[i] = rec->status_resists[i - 7];
        }
        if (rec->id != m->digimon) {
            form = records_get_digimon_func(m->digimon);
            for (i = 0; i < 5; i++) {
                (&stats.values[6])[i] += form->stats[i];
            }
            for (i = 0; i < 7; i++) {
                (&stats.values[12])[i] += form->resists[i];
            }
            for (i = 7; i < 12; i++) {
                out->resists[i] += form->status_resists[i - 7];
            }
        }
        if (m->modifiers[0] != 0) {
            stats.values[6] += m->modifiers[0];
        }
        if (m->modifiers[1] != 0) {
            stats.values[7] += m->modifiers[1];
        }
        if (m->modifiers[2] != 0) {
            stats.values[10] += m->modifiers[2];
        }
        out->level = stats.values[0];
        /* FAKE: p walks the stat totals here and the equipment below (the shape of the US decomp's
         * FIGHTSTG_computeStats): the extra references put p ahead of found in gcc's register allocation, so p gets
         * s0 and found s3, as in the original; a pointer of its own for the totals leaves them swapped (99.1%). */
        p = &stats.values[6];
        for (i = 0; i < 5; i++) {
            if (p[i] > 0) {
                out->stats[i] = p[i];
            } else {
                out->stats[i] = 1;
            }
        }
#ifdef PC_PORT
        def_finale_party_stats(out->stats); /* Extended Final Act: Golden Imperialdramon-P Limit Break (2500) */
#endif
        p = &stats.values[12];
        for (i = 0; i < 7; i++) {
            if (p[i] > 0) {
                out->resists[i] = p[i];
            } else {
                out->resists[i] = 1;
            }
        }
        out->status = m->status;
        digimon = gamestate_data.funcs.get_record(id);
        p = &digimon->equipment[4];
        for (i = 0; i < 2; i++) {
            if (p[i] != 0) {
                item = records_funcs.get_item(p[i])->data;
                if (item->kind == 0x11) {
                    out->resists[7] = item->value;
                } else if (item->kind == 0x12) {
                    out->resists[8] = item->value;
                } else if (item->kind == 0x13) {
                    out->resists[9] = item->value;
                } else if (item->kind == 0x14) {
                    out->resists[10] = item->value;
                } else if (item->kind == 0x15) {
                    out->resists[11] = item->value;
                }
            }
        }
        p = digimon->equipment;
        out->type = records_get_digimon_func(m->digimon)->type;
        out->power_up = m->power_up;
        n = 0;
        for (i = 0; i < 4; i++) {
            if (p[i] > 0) {
                entry = records_funcs.get_item(p[i]);
                if (entry->type >= 2 && entry->type <= 14) {
                    item = entry->data;
                    out->accuracy += item->accuracy;
                    if (item->strong_type >= 2) {
                        out->strong_types[n++] = item->strong_type;
                    }
                } else {
                    item = entry->data;
                    if (item->status_chance != 0) {
                        out->evasion += item->status_chance;
                    }
                }
            }
        }
        found = 0;
        for (i = 0; i < 4; i++) {
            if (i != 1 && p[i] > 0) {
                item = records_funcs.get_item(p[i])->data;
                if (p[i] == 0x97) {
                    out->poison_chance = item->status_chance;
                    out->poison_power = item->status_power;
                    found = 1;
                } else if (p[i] == 0xD2) {
                    out->paralysis_chance = item->status_chance;
                    out->paralysis_power = item->status_power;
                    found = 1;
                } else if (p[i] == 0xB4 || p[i] == 0xC2) {
                    out->confusion_chance = item->status_chance;
                    out->confusion_power = item->status_power;
                    found = 1;
                } else if (p[i] == 0x6D || p[i] == 0xBA) {
                    out->knockout_chance = item->status_chance;
                    out->knockout_power = item->status_power;
                    found = 1;
                } else if (p[i] == 0x5E || p[i] == 0x93 || p[i] == 0xAD) {
                    out->drain_chance = item->status_chance;
                    out->drain_power = item->status_power;
                    found = 1;
                } else if (p[i] == 0x96 || p[i] == 0xBF) {
                    out->critical = item->status_power;
                    found = 1;
                }
            }
        }
        if (!found) {
            p = &digimon->equipment[4];
            for (i = 0; i < 2; i++) {
                if (p[i] == 0x13C) {
                    out->multi_hit = 1;
                } else if (p[i] == 0x13D) {
                    item = records_funcs.get_item(0x13D)->data;
                    out->critical = item->value;
                } else if (p[i] == 0x13E) {
                    item = records_funcs.get_item(0x13E)->data;
                    out->counter = item->value;
                }
            }
        }
        p = &digimon->equipment[4];
        for (i = 0; i < 2; i++) {
            if (p[i] >= 0x153 && p[i] <= 0x167
#ifdef PC_PORT
                && !def_on()
#endif
            ) {
                item = records_funcs.get_item(p[i])->data;
                if (p[i] < 0x156) {
                    out->attack_element = 2;
                } else if (p[i] < 0x159) {
                    out->attack_element = 3;
                } else if (p[i] < 0x15C) {
                    out->attack_element = 4;
                } else if (p[i] < 0x15F) {
                    out->attack_element = 5;
                } else if (p[i] < 0x162) {
                    out->attack_element = 6;
                } else if (p[i] < 0x165) {
                    out->attack_element = 7;
                } else if (p[i] < 0x168) {
                    out->attack_element = 8;
                }
                out->attack_element_power = item->value;
            } else if (p[i] >= 0x145 && p[i] <= 0x146) {
                item = records_funcs.get_item(p[i])->data;
                out->guard = item->value;
            } else if (p[i] >= 0x14B && p[i] <= 0x14C) {
                item = records_funcs.get_item(p[i])->data;
                out->accuracy += item->value;
            } else if (p[i] >= 0x14D && p[i] <= 0x14E) {
                item = records_funcs.get_item(p[i])->data;
                out->evasion += item->value;
            } else if (p[i] >= 0x14F && p[i] <= 0x150) {
                item = records_funcs.get_item(p[i])->data;
                out->escape = item->value;
            } else if (p[i] == 0x13F) {
                item = records_funcs.get_item(p[i])->data;
                out->no_escape = 1;
            } else if (p[i] >= 0x147 && p[i] <= 0x148) {
                item = records_funcs.get_item(p[i])->data;
                out->steal = item->value;
            }
        }
#ifdef PC_PORT
        def_apply_resist_mod(0, member, out); /* an enemy's Resist Down lowers the party's affinities */
        def_blast_boost_stats(member, out);   /* a Blast or Chosen form's boost */
#endif
    } else {
        m = &fightstg_battle.state.members[1][member];
        fightstg_rules_get_enemy_stats(out, m, member);
#ifdef PC_PORT
        def_persona_apply(member, out);       /* the enemy personas: its phase's resistances */
        def_apply_resist_mod(1, member, out); /* definitive combat: Resist Down / Mega Break */
#endif
    }
    if (first) {
        return &fightstg_rules.stats[0];
    }
    return &fightstg_rules.stats[1];
}

s32 fightstg_rules_get_field_bonus(s32 value, s32 element) {
    FightstgField *info = &fightstg_battle.state.field;

    if (info->element < 2) {
        return 0;
    }
    if (element == info->element) {
        return value * info->power / 128;
    }
    if (fightstg_rules_opposite_elements[element] == info->element) {
        return -(value * info->power / 256);
    }
    return 0;
}

s32 fightstg_rules_roll_critical(u8 side, s32 id);

/* Damage of action `id` (record of records_techniques) from a base value, with fightstg_rules's stats
 * filled in ([0] the attacker, [1] the target). An old-style definition: fightstg_rules_get_damage passes
 * a fourth argument (the target's stats), fightstg_rules_get_counter_damage three. */
s32 fightstg_rules_scale_damage(side, id, base)
    u8 side;
    s32 id;
    s32 base;
{
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 value;
    s32 max;
    s32 i;

    rec = &records_techniques[id - 1];
    value = base;
    stats = &fightstg_rules.stats[0];
    value += fightstg_rules_get_field_bonus(value, rec->defense_stat);
    target = &fightstg_rules.stats[1];
    if (rec->strong_type >= 2) {
        if (rec->strong_type == target->type) {
            value += value / 2;
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (stats->strong_types[i] >= 2 && stats->strong_types[i] == target->type) {
                value += value / 2;
                break;
            }
        }
    }
    if (stats->power_up != 0) {
        value += value * stats->power_up / 64;
    }
    if (rec->defense_stat >= 2) {
#ifdef PC_PORT
        if (def_on()) { /* definitive combat: weakness, stagger and affinity replace the resist term */
            value = def_scale_physical(side, rec->defense_stat - 2, value, rec->element_power);
        } else
#endif
        value += value * rec->element_power * 2 / target->resists[rec->defense_stat - 2];
    } else if (rec->anim_script != 0xB && rec->anim_script != 0xC && stats->attack_element != 0) {
        value += value * stats->attack_element_power * 2 / target->resists[stats->attack_element - 2];
    }
#ifdef PC_PORT
    value = def_open_bonus(side, rec->defense_stat, value);
#endif
    if (rec->kind < 2 && stats->multi_hit != 0 && (rec->anim_script != 0xB && rec->anim_script != 0xC)) {
        value = value * 4 / 10;
    }
    if (fightstg_rules_roll_critical(side, id) != 0) {
        value += value * ((pad_random.next() & 0x3F) + 0x20) / 64;
    }
    if (target->guard != 0) {
        value -= target->guard;
        if (value <= 0) {
            value = 0;
        }
    }
    max = base * 5;
    if (value > max) {
        value = max;
    }
    if (value >= 10000) {
        value = 9999;
    }
    return value;
}

s32 fightstg_rules_get_damage(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 value;
    s32 member;

    if (side == 0) {
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    if (side == 0) {
        value = rec->power * stats->stats[0] / target->stats[1];
    } else {
        member = fightstg_battle.state.current[1];
        value = rec->power * records_state.enemies[member].stat_scale / 16 * stats->stats[0] / target->stats[1];
    }
    return fightstg_rules_scale_damage(side, id, value, target);
}

s32 fightstg_rules_roll_special_critical(u8 side, s32 id);

s32 fightstg_rules_get_special_damage(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 power;
    s32 base;
    s32 value;
    s32 member;
    s16 defense;

    if (side == 0) {
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    if (side == 0) {
        power = rec->power;
    } else {
        member = fightstg_battle.state.current[1];
        power = rec->power * records_state.enemies[member].stat_scale / 16;
    }
#ifdef PC_PORT
    if (def_on()) { /* definitive combat: Defense stands against magic, buffs count for magic */
        base = power * (def_magic_attack(side) * 50 / def_magic_defense(side) + 50) / 100;
    } else
#endif
    base = power * (stats->stats[2] * 50 / target->stats[2] + 50) / 100;
    if (base > power * 2) {
        base = power * 2;
    }
    if (base < power / 2) {
        base = power / 2;
    }
    value = base;
    value += fightstg_rules_get_field_bonus(value, rec->defense_stat);
    if (rec->defense_stat >= 2) {
#ifdef PC_PORT
        if (def_on()) { /* definitive combat: the damage multiplier replaces the bands */
            value = def_scale_magic(side, rec->defense_stat - 2, value);
        } else {
            defense = target->resists[rec->defense_stat - 2];
            if (defense < 100) {
                value = value * (400 - defense * 3) / 100;
            } else if (defense >= 300) {
                value = value * (65 - defense / 20) / 100;
            } else {
                value = value * (125 - defense / 4) / 100;
            }
        }
#else
        defense = target->resists[rec->defense_stat - 2];
        if (defense < 100) {
            value = value * (400 - defense * 3) / 100;
        } else if (defense >= 300) {
            value = value * (65 - defense / 20) / 100;
        } else {
            value = value * (125 - defense / 4) / 100;
        }
#endif
    }
#ifdef PC_PORT
    value = def_open_bonus(side, rec->defense_stat, value);
#endif
    if (rec->strong_type >= 2 && rec->strong_type == target->type) {
        value += value / 2;
    }
    if (fightstg_rules_roll_special_critical(side, id) != 0) {
        value += value * (pad_random.next() % 65 + 0x20) / 64;
    }
    if (target->guard != 0) {
        value -= target->guard;
        if (value <= 0) {
            value = 0;
        }
    }
    if (value > base * 5) {
        value = base * 5;
    }
    if (value >= 10000) {
        value = 9999;
    }
    return value;
}

s32 fightstg_rules_get_poison_damage(FightstgPoisonArgs *args) {
    FightstgBattle *battle = &fightstg_battle;
    u8 side = args->side;
    s32 member = args->member;
    u8 enemy = side != 0;
    s32 amount = args->amount;
    FightstgMember *m;
    FightstgStats *target;
    s32 value;
    s32 half;

    if (member != battle->state.current[enemy]) {
        return 0;
    }
    fightstg_rules_get_stats(side, 0, member);
    target = &fightstg_rules.stats[1];
    m = &fightstg_battle.state.members[enemy][member];
    value = (amount / 2 - (target->resists[7] + target->resists[1]) / 10) * m->max_hp / 100;
#ifdef PC_PORT
    if (def_poison_tick(amount, m->max_hp, enemy) >= 0) {
        return def_poison_tick(amount, m->max_hp, enemy);
    }
#endif
    if (value <= 0) {
        value = 1;
    }
    half = m->max_hp / 2;
    if (half < value) {
        value = half;
    }
    return value;
}

s32 fightstg_rules_get_counter_damage(u8 side, s32 id, s32 amount) {
    FightstgMember *member;
    RecordsTechnique *rec;
    s32 own;
    u8 saved;
    s32 ret;
    s32 value;

    if (side == 0) {
        member = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];
        own = id == records_get_digimon_func(member->digimon)->techniques[0];
    } else {
        member = &fightstg_battle.state.members[1][fightstg_battle.state.current[1]];
        fightstg_enemy_records.get(member->digimon);
        own = 0;
    }
    if (side == 0) {
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    if (own == 1) {
        saved = fightstg_rules.stats[0].multi_hit;
        fightstg_rules.stats[0].multi_hit = 0;
        ret = fightstg_rules_scale_damage(side, id, amount);
        fightstg_rules.stats[0].multi_hit = saved;
#ifdef PC_PORT
        if (def_on() && ret > 0) {
            ret = ret / 100;
            if (ret < 1) {
                ret = 1;
            }
        }
#endif
        return ret;
    }
    rec = &records_techniques[id - 1];
    if (member->boosted != 0) {
        value = amount * rec->effect_power / 32;
    } else {
        value = amount * rec->effect_power / 64;
    }
    return fightstg_rules_scale_damage(side, id, value);
}

s32 fightstg_rules_get_heal(u8 side, s32 id) {
    FightstgStats *stats;
    RecordsTechnique *rec;
    s32 value;

    if (side == 0) {
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
    } else {
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    rec = &records_techniques[id - 1];
    value = (rec->power << 6) + stats->stats[3] * rec->power / 8;
    if (value >= 10000) {
        value = 9999;
    }
    return value;
}

s32 fightstg_rules_get_regen(u8 side, s32 member, s32 arg2) {
    FightstgMember *m;
    s32 value;

    if (side == 0) {
        m = &fightstg_battle.state.members[0][member];
    } else {
        m = &fightstg_battle.state.members[1][member];
    }
    if (arg2 != 0) {
        value = m->max_hp * (pad_random.next() % 9 + 8) / 128;
    } else {
        value = m->max_hp * (pad_random.next() % 5 + 4) / 128;
    }
    if (value >= 10000) {
        value = 9999;
    }
    return value;
}

s32 fightstg_rules_roll_critical(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    FightstgMember *m;
    s32 enemy;
    s32 other;
    s32 chance;
    s32 i;

    if (side == 0) {
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    chance = 4;
    rec = &records_techniques[id - 1];
    if (rec->kind >= 2) {
        if (rec->kind == 0xB) {
            chance = rec->effect_power + 4;
        }
    } else if (stats->critical != 0) {
        enemy = side != 0;
        m = &fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]];
        if (m->boosted != 0) {
            chance = stats->critical * 2 + 4;
        } else {
            chance = stats->critical + 4;
        }
    }
    if (rec->strong_type >= 2) {
        if (rec->strong_type == target->type) {
            chance += 60;
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (stats->strong_types[i] >= 2 && stats->strong_types[i] == target->type) {
                chance += 16;
                break;
            }
        }
    }
    other = side == 0;
    m = &fightstg_battle.state.members[other][fightstg_battle.state.current[other]];
    if (m->status & 2) {
        chance += m->paralysis_power >> 3;
    }
    if (m->status & 8) {
        chance += m->sleep_power >> 3;
    }
    if (m->status & 4) {
        chance += m->confusion_power >> 1;
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_special_critical(u8 side, s32 id) {
    FightstgStats *target;
    RecordsTechnique *rec;
    FightstgMember *m;
    s32 other;
    s32 ratio;
    s32 chance;

    if (side == 0) {
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
    }
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    ratio = rec->element_power * 100 / target->resists[rec->defense_stat - 2];
    if (ratio > 64) {
        ratio = 64;
    }
    chance = ratio + 4;
    if (rec->strong_type >= 2 && rec->strong_type == target->type) {
        chance = ratio + 64;
    }
    other = side == 0;
    m = &fightstg_battle.state.members[other][fightstg_battle.state.current[other]];
    if (m->status & 2) {
        chance += m->paralysis_power >> 3;
    }
    if (m->status & 8) {
        chance += m->sleep_power >> 3;
    }
    if (m->status & 4) {
        chance += m->confusion_power >> 1;
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_hit(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 diff;
    s32 level;
    s32 chance;

    if (side == 0) {
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    if (side == 0) {
        if (fightstg_battle.state.type == 6) {
#ifdef PC_PORT
            if (def_finale_golden_mode()) {
                return 1;
            }
#endif
            return (pad_random.next() & 0x7F) >= 0x2B;
        }
        diff = stats->stats[4] + stats->accuracy - target->stats[4];
        level = stats->level - target->level;
        if (rec->kind < 2) {
            chance = rec->accuracy + rec->accuracy * (diff / 8 + (level - stats->critical)) / 128;
        } else {
            chance = rec->accuracy + rec->accuracy * (diff / 8 + level) / 128;
        }
    } else {
        level = stats->level - target->level;
        diff = stats->stats[4] - target->stats[4] - target->evasion;
        chance = rec->accuracy + rec->accuracy * (diff / 8 + level) / 128;
        if (chance < 0x20) {
            chance = 0x20;
        }
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_special_hit(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 diff;
    s32 level;
    s32 chance;

    if (side == 0) {
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    if (side == 0 && fightstg_battle.state.type == 6) {
#ifdef PC_PORT
        if (def_finale_golden_mode()) {
            return 1;
        }
#endif
        return (pad_random.next() & 0x7F) >= 0x2B;
    }
    diff = stats->stats[3] - target->stats[3];
    level = stats->level - target->level;
    chance = rec->accuracy + rec->accuracy * (diff / 8 + level) / 128;
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_poison(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 amount;
    s32 value;
    s32 chance;

    if (side == 0) {
        if (def_status_blocked(0) != 0) {
            return 0;
        }
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    if (rec->kind < 2) {
        amount = stats->poison_power;
        value = stats->poison_chance + stats->stats[3] / 8;
    } else {
        amount = rec->effect_power;
        value = rec->effect_chance + stats->stats[3] / 8;
    }
#ifdef PC_PORT
    chance = def_status_chance(0, value - stats->stats[3] / 8, stats, target, target->resists[7] + target->resists[1]);
#else
    chance = value - (target->resists[7] + target->resists[1] + target->stats[3] / 2) / 8;
#endif
    if (chance <= 0) {
        chance = 1;
    }
    if (chance >= 0x80) {
        chance = 0x7F;
    }
    if ((pad_random.next() & 0x7F) < chance) {
        return amount;
    }
    return 0;
}

s32 fightstg_rules_roll_paralysis(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 value;
    s32 chance;

    if (side == 0) {
        if (records_state.blocked[1] != 0) {
            return 0;
        }
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    if (rec->kind < 2) {
        value = stats->paralysis_chance + stats->stats[3] / 8;
    } else {
        value = rec->effect_chance + stats->stats[3] / 8;
    }
    chance = value - (target->resists[8] + target->resists[4] + target->stats[3] / 2) / 8;
    if (chance <= 0) {
        chance = 1;
    }
    if (chance >= 0x80) {
        chance = 0x7F;
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_confusion(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 value;
    s32 chance;

    if (side == 0) {
        if (def_status_blocked(2) != 0) {
            return 0;
        }
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    if (rec->kind < 2) {
        value = stats->confusion_chance + stats->stats[3] / 8;
    } else {
        value = rec->effect_chance + stats->stats[3] / 8;
    }
#ifdef PC_PORT
    chance = def_status_chance(2, value - stats->stats[3] / 8, stats, target, target->resists[9] + target->resists[3]);
#else
    chance = value - (target->resists[9] + target->resists[3] + target->stats[3] / 2) / 8;
#endif
    if (chance <= 0) {
        chance = 1;
    }
    if (chance >= 0x80) {
        chance = 0x7F;
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_sleep(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 value;
    s32 chance;

    if (side == 0) {
        if (def_status_blocked(3) != 0) {
            return 0;
        }
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    value = rec->effect_chance + stats->stats[3] / 8;
#ifdef PC_PORT
    chance = def_status_chance(3, value - stats->stats[3] / 8, stats, target, target->resists[10] + target->resists[2]);
#else
    chance = value - (target->resists[10] + target->resists[2] + target->stats[3] / 2) / 8;
#endif
    if (chance <= 0) {
        chance = 1;
    }
    if (chance >= 0x80) {
        chance = 0x7F;
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_knockout(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 value;
    s32 chance;

    if (side == 0) {
        if (records_state.blocked[4] != 0) {
            return 0;
        }
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    rec = &records_techniques[id - 1];
    if (rec->kind < 2) {
        value = stats->knockout_chance + stats->stats[3] / 8;
    } else {
        value = rec->effect_chance + stats->stats[3] / 8;
    }
    chance = value - (target->resists[11] + target->resists[6] + target->stats[3] / 2) / 8;
    if (chance <= 0) {
        chance = 1;
    }
    if (chance >= 0x80) {
        chance = 0x7F;
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_steal(u8 side, s32 id) {
    FightstgStats *stats;
    FightstgStats *target;
    RecordsTechnique *rec;
    s32 ratio;
    s32 percent;
    s32 chance;
    s32 roll;

    if (side == 0) {
        if (records_state.blocked[7] != 0) {
            return 0;
        }
        if (fightstg_battle.state.members[1][fightstg_battle.state.current[1]].item <= 0) {
            return 0;
        }
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    if (side == 0) {
        stats = &fightstg_rules.stats[0];
        target = &fightstg_rules.stats[1];
        ratio = stats->stats[4] * 100 / target->stats[4];
        rec = &records_techniques[id - 1];
        if (ratio > 200) {
            ratio = 200;
        }
        percent = (rec->effect_chance + stats->steal) * 100 / 64;
        chance = fightstg_enemy_records.get(fightstg_battle.state.members[1][fightstg_battle.state.current[1]].digimon)->drop_rate * ratio * percent / 10000;
        roll = pad_random.next() % 1024;
        if (roll < chance) {
            return 1;
        }
    }
    return 0;
}

s32 fightstg_rules_roll_drain(u8 side, s32 id) {
    FightstgStats *stats;
    RecordsTechnique *rec;
    s32 chance;

    if (side == 0 && records_state.blocked[5] != 0) {
        return 0;
    }
    stats = fightstg_rules_get_stats(side, 1, fightstg_battle.state.current[side >> 4]);
    rec = &records_techniques[id - 1];
    if (rec->kind < 2) {
        chance = stats->drain_chance;
    } else {
        chance = rec->effect_chance;
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_revert(u8 side, s32 id) {
    RecordsTechnique *rec;
    s32 i;
    s32 chance;

    for (i = 0; i < 8; i++) {
        if (records_digimon[i].id == fightstg_battle.state.members[0][fightstg_battle.state.current[0]].digimon) {
            return 0;
        }
    }
    rec = &records_techniques[id - 1];
    chance = rec->effect_chance;
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_lower_stat(u8 side, s32 id) {
    RecordsTechnique *rec;
    s32 diff;
    s32 chance;

    fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
    fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    diff = fightstg_rules.stats[0].stats[3] - fightstg_rules.stats[1].stats[3];
    rec = &records_techniques[id - 1];
    chance = rec->effect_chance + diff / 8;
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_switch_seal(u8 side, s32 id) {
    RecordsTechnique *rec = &records_techniques[id - 1];
    s32 chance = rec->effect_chance;

    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_digivolve_seal(u8 side, s32 id) {
    RecordsTechnique *rec = &records_techniques[id - 1];
    s32 chance = rec->effect_chance;

    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_counter(u8 side, s32 damage) {
    s32 chance = (damage << 6) / (fightstg_battle.state.members[0] + fightstg_battle.state.current[0])->max_hp + 0x20;

    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_escape(u8 side) {
    FightstgStats *stats;
    FightstgStats *target;
    FightstgBattleState *battle;
    s32 chance;

    if (side == 0) {
        fightstg_rules_get_stats(0, 1, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    } else {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
        fightstg_rules_get_stats(0x10, 1, fightstg_battle.state.current[1]);
    }
    battle = &fightstg_battle.state;
    stats = &fightstg_rules.stats[0];
    target = &fightstg_rules.stats[1];
    if (side == 0) {
#ifdef PC_PORT
        {
            /* A Rare Target can always be fled, whatever its boss battle blocks (but in a Super Rare
             * hunt's territory: def_hunt_flee); a Super Rare too */
            s32 flee = def_hunt_flee();

            if (flee >= 0) {
                return flee;
            }
        }
#endif
        if (records_state.blocked[11] != 0) {
            return 0;
        }
        if ((stats->status & 8) || (stats->status & 0x10)) {
            return 0;
        }
        chance = (battle->escapes + 1) * 8;
        if (stats->status & 2) {
            chance >>= 1;
        }
        if (stats->level > target->level) {
            chance += stats->level - target->level;
        }
        if (stats->stats[4] > target->stats[4]) {
            chance += (stats->stats[4] - target->stats[4]) / 10;
        }
        chance += stats->escape;
    } else {
        if (stats->status & 8) {
            return 0;
        }
        chance = 0x40;
        if (target->no_escape != 0) {
            chance = 0x20;
        }
        if (stats->status & 2) {
            chance >>= 1;
        }
        chance += (stats->stats[4] - target->stats[4]) / 10;
    }
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_wake(u8 side, s32 amount) {
    FightstgStats *target;
    s32 enemy;
    s32 i;
    s32 chance;
    s32 wait;
    s32 base;

    if (side == 0) {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
    } else {
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    }
#ifdef PC_PORT
    if (def_wake_on_hit(side)) {
        return 1;
    }
#endif
    enemy = side != 0;
    target = &fightstg_rules.stats[1];
    i = fightstg_events.find_member(0xC, side, fightstg_battle.state.current[enemy]);
    base = 64 - fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]].sleep_power / 2;
    wait = fightstg_events.events[i].delay / 100;
    chance = (amount << 7) / target->stats[1] + base - wait;
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_confused(u8 side) {
    FightstgStats *stats;
    s32 enemy;
    s32 chance;

    if (side == 0) {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
    } else {
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[1];
    enemy = side != 0;
    chance = fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]].confusion_power -
             (stats->resists[9] + stats->resists[3]) / 8;
    return (pad_random.next() & 0x7F) < chance;
}

s32 fightstg_rules_roll_paralyzed(u8 side) {
    FightstgStats *stats;
    s32 enemy;
    s32 chance;

    if (side == 0) {
        fightstg_rules_get_stats(0, 0, fightstg_battle.state.current[0]);
    } else {
        fightstg_rules_get_stats(0x10, 0, fightstg_battle.state.current[1]);
    }
    stats = &fightstg_rules.stats[1];
    enemy = side != 0;
    chance = fightstg_battle.state.members[enemy][fightstg_battle.state.current[enemy]].paralysis_power -
             (stats->resists[8] + stats->resists[4]) / 8;
    if (chance < 0x20) {
        chance = 0x20;
    }
    return (pad_random.next() & 0x7F) < chance;
}

/* The stat (index into FightstgStats.stats) each of a member's modifiers changes. */
extern s16 fightstg_rules_modifier_stats[4];

/* Changes modifier `kind` of a member by amount / 128 of the stat, within -1/2 .. 1 times it. */
void fightstg_rules_change_modifier(u8 side, s32 member, s32 kind, s32 amount) {
    FightstgStats *stats;
    FightstgMember *m;
    s32 base;
    s32 min;

#ifdef PC_PORT
    if (def_mod_apply(side, member, kind, def_mod_level_from_amount(amount), DEF_MOD_TURNS_BASE,
                      (side != 0 && amount < 0) ? kind + 8 : 0)) {
        return;
    }
#endif
    if (side == 0) {
        stats = fightstg_rules_get_stats(0, 0, member);
        m = &fightstg_battle.state.members[0][member];
    } else {
        stats = fightstg_rules_get_stats(0x10, 0, member);
        m = &fightstg_battle.state.members[1][member];
    }
    if (m->digimon != 0 && m->hp != 0) {
        if (m->modifiers[kind] != 0) {
            stats->stats[fightstg_rules_modifier_stats[kind]] -= m->modifiers[kind];
        }
        base = stats->stats[fightstg_rules_modifier_stats[kind]];
        min = -(base / 2);
        m->modifiers[kind] += base * amount / 128;
        if (m->modifiers[kind] < min) {
            m->modifiers[kind] = min;
        }
        if (base < m->modifiers[kind]) {
            m->modifiers[kind] = base;
        }
    }
}

s32 fightstg_rules_get_gauge_gain(s32 amount) {
    GamestateRecord *digimon;
    s32 percent;
    s32 value;
    s32 i;
    s16 *items;
    FightstgMember *m = &fightstg_battle.state.members[0][fightstg_battle.state.current[0]];

    percent = amount * 100 / m->max_hp;
    value = percent * percent / 20;
    digimon = gamestate_data.funcs.get_record(gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]));
    items = &digimon->equipment[4];
    for (i = 0; i < 2; i++) {
        if (items[i] == 0x149) {
            value += value / 5;
        } else if (items[i] == 0x14A) {
            value += value * 4 / 10;
        }
    }
    if (value > 1000) {
        value = 1000;
    }
    return value;
}

s32 fightstg_rules_get_tech_cost(u8 side, s32 id) {
    GamestateRecord *digimon;
    s32 cost = records_techniques[(id & 0x1FFF) - 1].mp_cost;
    s16 item;

    if (side == 0) {
        digimon = gamestate_data.funcs.get_record(gamestate_data.funcs.get_party_member(fightstg_battle.state.current[0]));
        item = 0;
        if (digimon->equipment[4] == 0x143 || digimon->equipment[4] == 0x144) {
            item = digimon->equipment[4];
        }
        if (digimon->equipment[5] == 0x143 || digimon->equipment[5] == 0x144) {
            item = digimon->equipment[5];
        }
        if (item != 0) {
            cost -= ((RecordsAccessory *)records_funcs.get_item(item)->data)->bonus_value;
            if (cost <= 0) {
                cost = 1;
            }
        }
        if (id & 0x4000) {
            if (cost / 5 != 0) {
                cost += cost / 5;
            } else {
                cost += 1;
            }
        }
    }
    return cost;
}

void fightstg_math_nop(void) {
}

/* out = a + (b - a) * t (t: 0x1000 = 1), on the GTE. */
void fightstg_math_lerp(SVECTOR *a, SVECTOR *b, s32 t, SVECTOR *out) {
    SVECTOR d;
    SVECTOR r;

    gte_lddp(t);
    d.vx = b->vx - a->vx;
    d.vy = b->vy - a->vy;
    d.vz = b->vz - a->vz;
    gte_ldsv(&d);
    gte_gpf12();
    *out = *a;
    gte_stsv(&r);
    out->vx += r.vx;
    out->vy += r.vy;
    out->vz += r.vz;
}

/* scale * sin (mode 0), cos (1) or sin of the doubled angle (2), with angles in 1/16384 turns. */
s32 fightstg_math_wave(s32 mode, s32 angle, s32 scale) {
    switch (mode) {
    case 0:
    default:
        return rsin(angle >> 2) * scale / 4096;
    case 1:
        return rsin((angle >> 2) + 0x400) * scale / 4096;
    case 2:
        return rsin(angle >> 1) * scale / 4096;
    }
}

/* The tables below point to these WIP functions (asm unless NON_MATCHING). */
void fightstg_events_cure_status(u8 side, s32 member, s32 id);
s32 fightstg_rules_roll_wake(u8 side, s32 amount);

/* .data (address order) */

FightstgItemCure fightstg_item_cure_poison = { 40, 1, 190 };
FightstgItemCure fightstg_item_cure_paralysis = { 41, 2, 192 };
FightstgItemCure fightstg_item_cure_confusion = { 42, 4, 194 };
FightstgItemCure fightstg_item_cure_all = { 44, 63, 196 };

FightstgItemScript fightstg_item_scripts[16] = {
    { 43, 33, 31 }, { 44, 33, 31 }, { 45, 33, 31 }, { 46, 33, 31 }, { 66, 34, 31 }, { 67, 34, 31 }, { 68, 34, 31 },
    { 69, 34, 31 }, { 70, 35, 31 }, { 71, 33, 31 }, { 72, 40, 31 }, { 73, 36, 31 }, { 74, 38, 31 }, { 75, 46, 30 },
    { 76, 46, 30 }, { -1, 0, 0 },
};

FightstgTechStatEffect fightstg_tech_stat_effects[9] = {
    { 198, 1, 0, 48 }, { 200, 1, 1, 49 }, { 202, 1, 2, 50 }, { 204, -1, 0, 51 }, { 205, -1, 1, 52 },
    { 206, -1, 1, 52 }, { 207, -1, 2, 53 }, { 208, -1, 2, 53 }, { -1, 0, 0, 0 },
};

FightstgTechStatEffect fightstg_tech_party_stat_effects[4] = { { 199, 1, 0, 48 }, { 201, 1, 1, 49 }, { 203, 1, 2, 50 }, { -1, 0, 0, 0 } };
u8 fightstg_tech_cure_flags[4] = { 1, 2, 4, 0x3F };
s32 fightstg_revive_event_types[6] = { 9, 10, 11, 12, 16, 17 };
s32 fightstg_tech_cure_kinds[4] = { 1, 2, 3, 4 };
s32 fightstg_tech_cure_messages[4] = { 40, 41, 42, 44 };

DVECTOR fightstg_hud_bars[2][4] = {
    { { 175, 37 }, { 303, 37 }, { 175, 43 }, { 303, 43 } }, { { 143, 37 }, { 15, 37 }, { 143, 43 }, { 15, 43 } },
};

CVECTOR fightstg_hud_bar_colors[2][2] = {
    { { 0, 0x71, 0x28, 0 }, { 0, 0xC8, 0x3E, 0 } }, { { 0xB3, 0x37, 0x16, 0 }, { 0xFF, 0x37, 0xD, 0 } },
};

DVECTOR fightstg_hud_gauge[4] = { { 265, 62 }, { 305, 62 }, { 265, 70 }, { 305, 70 } };
CVECTOR fightstg_hud_gauge_colors[4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
s16 fightstg_hud_icon_x[2][3] = { { 271, 282, 293 }, { 37, 26, 15 } };
struct FightstgCursorParams fightstg_command_menu_cursor = { 6, 17, 110, 19, 34, 16, 109, 19 };
FightstgCursorParams fightstg_digivolve_menu_cursor = { 0, 168, 147, 19, 36, 167, 146, 19 };

FightstgStatWindow fightstg_status_stat_windows[13] = {
    { 0x3C, 0x52, 6 }, { 0x3C, 0x60, 7 }, { 0x3C, 0x6E, 8 }, { 0x3C, 0x7C, 9 }, { 0x3C, 0x8A, 0xA },
    { 0x3C, 0x98, 0xB }, { 0x7E, 0x52, 0xC }, { 0x7E, 0x60, 0xD }, { 0x7E, 0x6E, 0xE }, { 0x7E, 0x7C, 0xF },
    { 0x7E, 0x8A, 0x10 }, { 0x7E, 0x98, 0x11 }, { 0x7E, 0xA6, 0x12 },
};

FightstgCursorParams fightstg_item_menu_cursor = { 1, 14, 69, 14, -1, 0, 0, 0 };
FightstgCursorParams fightstg_tech_menu_cursor = { 1, 14, 69, 14, -1, 0, 0, 0 };
FightstgCursorParams fightstg_member_menu_cursor = { 2, 17, 78, 32, 47, 16, 77, 32 };

FightstgCursorParams fightstg_switch_menu_cursors[2] = {
    { 4, 168, 147, 19, 36, 167, 146, 19 }, { 2, 168, 166, 19, 34, 167, 165, 19 },
};

DVECTOR fightstg_switch_menu_window_x[4] = { { 172, 0 }, { 216, 0 }, { 217, 0 }, { 251, 0 } };

FightstgConfusedMessage fightstg_confused_menu_messages[16] = {
    { 1, 115 }, { 2, 116 }, { 3, 117 }, { 4, 118 }, { 5, 119 }, { 6, 120 }, { 7, 121 }, { 8, 122 }, { 9, 123 },
    { 10, 124 }, { 11, 125 }, { 12, 126 }, { 13, 127 }, { 14, 128 }, { 15, 129 }, { 16, 130 },
};

s16 fightstg_confused_menu_order[8] = { 7, 1, 0, 3, 2, 5, 4, 6 };
FightstgCursorParams fightstg_confused_menu_cursor = { 6, 17, 110, 19, 34, 16, 109, 19 };
RECT fightstg_cursor_rect = { 0, 244, 12, 12 };

FightstgJumpParams fightstg_jump_params[6] = {
    { 640, 136 }, { 1280, 102 }, { 1920, 81 }, { 2560, 64 }, { 1280, 64 }, { 0, 42 },
};

s32 fightstg_sounds[92] = {
    0x6004001E, 0x0004001C, 0x00040004, 0x0004000A, 0x0004000B, 0x0004000C,
    0x4004000D, 0x00040014, 0x0004001A, 0x00040019, 0x00040017, 0x00040011,
    0x0004000F, 0x0004001F, 0x00040016, 0x00040010, 0x0004000E, 0x00040012,
    0x8004103C, 0x800410BD, 0x8004213E, 0x800421BF, 0xA0042240, 0x80042342,
    0x80042444, 0x8004293E, 0x800429BF, 0x80042A40, 0x00040001, 0x80042B42,
    0x80042C44, 0x80042CC5, 0x80042D46, 0x80042DC7, 0x80042E48, 0xA0042F4A,
    0xA0042FCB, 0xA004303C, 0x800430BD, 0x8004313E, 0xA00431BF, 0xA0043240,
    0x800432C1, 0x80043342, 0x800433C3, 0x800434C5, 0x80043546, 0xA00435C7,
    0x80043648, 0x8004374A, 0x800437CB, 0x8004383C, 0x80043A40, 0xA0043BC3,
    0xA0043C44, 0x800440BD, 0x8004413E, 0x800441BF, 0x80044240, 0x800442C1,
    0x80044444, 0x800445C7, 0x80044648, 0x800446C9, 0x8004474A, 0x8004483C,
    0x41180000, 0x8004503C, 0x800450BD, 0x8004513E, 0x800452C6, 0x80045341,
    0x0004001B, 0x800454C4, 0x8004583C, 0x800458BD, 0x8004593E, 0x800459BF,
    0x80045A40, 0x80045AC1, 0x80045B42, 0x80045BC3, 0x80045C44, 0x80045CC5,
    0x80045D46, 0x80045DC7, 0x8004603C, 0x800460BD, 0x8004613E, 0x20040006,
    0, 0,
};

FightstgEnemyRecords fightstg_enemy_records = { fightstg_enemy_get_record };

s32 fightstg_events_take_modes[26] = {
    0, 1, 1, 1, -1, 1, -1, 1,
    1, -1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0,
};

FightstgEvents fightstg_events = {
    .add = fightstg_events_add,
    .add_first = fightstg_events_add_first,
    .take_next = fightstg_events_take_next,
    .find = fightstg_events_find,
    .find_next = fightstg_events_find_next,
    .find_member = fightstg_events_find_member,
    .remove_member = fightstg_events_remove_member,
    .get_delay = fightstg_events_get_delay,
    .cure_status = (void (*)())fightstg_events_cure_status,
};

FightstgDelayFormula fightstg_events_delay_formulas[13] = {
    { 1000, 707, 1414 }, { 250, 176, 353 }, { 2001, 1001, 0 }, { 2000, 0, 0 }, { 2500, 0, 0 }, { 3000, 0, 0 },
    { 3500, 0, 0 }, { 4000, 0, 0 }, { 0, 2000, 6000 }, { 500, 1000, 0 }, { 500, 1000, 0 }, { 500, 500, 0 },
    { 2001, 0, 0 },
};

u8 fightstg_status_event_types[8] = { 9, 0xA, 0xB, 0xC, 0x10, 0x11, 0, 0 };
u8 fightstg_status_flags[4] = { 1, 2, 4, 0x3F };
s32 fightstg_modifier_event_types[3] = { 13, 14, 15 };
s32 fightstg_seal_event_types[2] = { 16, 17 };

FightstgAction fightstg_action = {
    .run = (void (*)())fightstg_action_run,
};

FightstgBattle fightstg_battle = {
    .update_speed = fightstg_battle_update_speed,
    .set_speed = fightstg_battle_set_speed,
    .to_screen = fightstg_battle_to_screen,
    .draw_quad = (void (*)(s32, s32, DVECTOR *, CVECTOR *))fightstg_battle_draw_quad_opaque,
    .draw_quad_semi = (void (*)(s32, s32, DVECTOR *, CVECTOR *))fightstg_battle_draw_quad_semi,
};

FightstgModels fightstg_models = {
    .get = fightstg_models_get,
    .select = fightstg_models_select,
    .get_range = fightstg_models_get_range,
    .get_texture_anim = (void *(*)(s32))fightstg_models_get_texture_anim,
};

FightstgRules fightstg_rules = {
    .get_stats = fightstg_rules_get_stats,
#ifdef PC_PORT
    .get_damage = def_get_damage, /* definitive.c: the same call, logged (DW3_BATTLE_LOG) */
    .get_special_damage = def_get_special_damage,
#else
    .get_damage = fightstg_rules_get_damage,
    .get_special_damage = fightstg_rules_get_special_damage,
#endif
    .get_poison_damage = fightstg_rules_get_poison_damage,
    .get_counter_damage = fightstg_rules_get_counter_damage,
    .get_heal = fightstg_rules_get_heal,
    .get_regen = fightstg_rules_get_regen,
    .roll_hit = fightstg_rules_roll_hit,
    .roll_special_hit = fightstg_rules_roll_special_hit,
    .roll_poison = fightstg_rules_roll_poison,
    .roll_paralysis = fightstg_rules_roll_paralysis,
    .roll_confusion = fightstg_rules_roll_confusion,
    .roll_sleep = fightstg_rules_roll_sleep,
    .roll_knockout = fightstg_rules_roll_knockout,
    .roll_steal = fightstg_rules_roll_steal,
    .roll_drain = fightstg_rules_roll_drain,
    .roll_revert = fightstg_rules_roll_revert,
    .roll_lower_stat = fightstg_rules_roll_lower_stat,
    .roll_switch_seal = fightstg_rules_roll_switch_seal,
    .roll_digivolve_seal = fightstg_rules_roll_digivolve_seal,
    .roll_counter = fightstg_rules_roll_counter,
    .roll_escape = fightstg_rules_roll_escape,
    .roll_wake = fightstg_rules_roll_wake,
    .roll_confused = fightstg_rules_roll_confused,
    .roll_paralyzed = fightstg_rules_roll_paralyzed,
    .change_modifier = fightstg_rules_change_modifier,
    .get_gauge_gain = fightstg_rules_get_gauge_gain,
    .get_tech_cost = fightstg_rules_get_tech_cost,
};

s32 fightstg_rules_opposite_elements[9] = {
    0, 0, 4, 2, 5, 3, 7, 8,
    6,
};

s16 fightstg_rules_modifier_stats[4] = { 0, 1, 4, 0 };

FightstgMath fightstg_math = {
    .nop = fightstg_math_nop,
    .lerp = fightstg_math_lerp,
    .wave = fightstg_math_wave,
};
