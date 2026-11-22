/* A player form fighting as an enemy (the DRI's simulators, the Blast's purification) plays animations authored for the
 * party's row of the arena, because the disc wrote them for a player. This is the owner's idea (2026-11-22): during
 * one such animation, exchange the two sides' places, so the caster stands where the author put the player and the
 * party stands where the author put the enemy (the owner asked for it always on, 2026-11-22).
 *
 * Evidence, the reason a form's animation lands on the wrong side:
 *  - fightstg_slots_reset_pos (fightstg_80086A00.c): the party's slot 0 is at z = -0x1400 - depth with rot.y 0x800
 *    (facing +z); the enemy's slot 0x10 at z = +0x1400 + depth with rot.y 0 (facing -z). Nothing is on the x axis.
 *  - A technique's animation is a script of the caster's model file (FightstgScript.script = RecordsTechnique.
 *    anim_script, read at sub-file anim_script of FightstgModelRecord.script_file): its cameras are absolute settings
 *    with y and z negated (fightstg_script_run_camera, op 1), its staged models and props are at absolute positions
 *    (command 2 op 3, command 6). Measured on the disc: a player form's script looks at the party's row (its own) while
 *    a disc enemy's looks at the enemy's row (also its own) -- e.g. Greymon's Nova Blast (script 0xb of model 5) sets
 *    eye z +2496, target z +5632, and the disc's species 0x1bb sets target z +6528 with a following "preset 1" cut that
 *    reads the party's cameras. So each script assumes the caster's row is the one it frames.
 *  - The camera cuts that name a row already work on both sides (fightstg_script_run_camera: presets 1-4 read slot 0's
 *    type A cameras 8-11, presets 5-7 read slot 0x10's type B cameras 0-2), which is why such an animation still shows
 *    a scene: only the absolute settings and the staged positions are on the party's side.
 *
 * Why this way and not a blanket mirror of the script's literals: the scripts are not consistent -- some frame the
 * caster (as above), others frame the victim -- so flipping z would be right for one half of the disc and wrong for the
 * other. Exchanging the two rows reproduces, exactly, what the player sees when it casts the same technique, whatever
 * the script assumes.
 *
 * The exchange: two models are on the field at a time (slot 0, the leader, and slot 0x10, the acting enemy); a wave,
 * a digivolution or a script that shows a model (command 2 op 6) may add some of 1-2 and 0x11-0x12 as well. Each
 * present pair (0 with 0x10, 1 with 0x11, 2 with 0x12) exchanges x and z and turns 0x800 (half a turn) more, which is
 * each side's own resting facing; y stays, as it is the model's own height. Nothing else reads those positions during
 * an animation: the target was picked before it and the HUD has no world position. On the way out each side goes back
 * to fightstg_slots_reset_pos's resting place, which also undoes anything the script left behind (a model mid-jump).
 *
 * Not swapped on purpose: a player's own technique (side 0), a script of the target's reaction (created on the other
 * side: its own animations 1-4), and any enemy with a record on the disc. A player form has none (fightstg_enemy_
 * get_record asks the port's def_purify_record / def_dri_record for it), so the test needs no battle context: only the
 * DRI's simulators and the Blast's purification put one on the field, and a DRI line's disc species (its stages with
 * base 0, e.g. MetalGreymon) keep their own animations.
 *
 * No state outside the battle's objects: the script's unk_84 says it exchanged the rows, and the rows themselves say
 * they are exchanged (slot 0's start_pos.z > 0, which only this file writes; fightstg_slots_reset_pos puts it back).
 * So a save state carries it, and a battle that ends mid-animation leaves nothing behind for the next one. */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include "fightstg.h"
#include "heap.h"
#include "port.h"
#include "port_runtime.h"

/* DW3_FORM_ANIM_LOG=<path>: every exchange and every restore, a line in that file (the rows' z and facing). */
static void fa_trace(const char *what, FightstgSlots *slots, s32 a, s32 b, s32 model_id, s32 script) {
    const char *path = getenv("DW3_FORM_ANIM_LOG");
    FightstgModelParams *pa;
    FightstgModelParams *pb;
    FILE *f;

    if (path == NULL || slots == NULL || (f = fopen(path, "a")) == NULL) {
        return;
    }
    pa = slots->get_params(slots, a);
    pb = slots->get_params(slots, b);
    fprintf(f, "form anim: frame %ld: %s model %d script %d: slot %#x (model %d) z %d rot.y %#x | slot %#x (model %d) "
               "z %d rot.y %#x\n",
            port_frames, what, model_id, script, a, pa != NULL ? pa->model_id : -1, pa != NULL ? pa->pos.z : 0,
            pa != NULL ? (pa->rot.y & 0xFFF) : 0, b, pb != NULL ? pb->model_id : -1, pb != NULL ? pb->pos.z : 0,
            pb != NULL ? (pb->rot.y & 0xFFF) : 0);
    fclose(f);
}

extern s32 fightstg_enemy_find_record(s32 id);

/* The two slots of one pair, party and enemy side, by the order fightstg_slots_reset_pos knows. */
static const s32 fa_slots[] = { 0, 0x10, 1, 0x11, 2, 0x12 };

/* Exchanges `a`'s and `b`'s places: x and z of the position and of the one the scripts return to, and half a turn
 * more for both (each side's resting facing, see fightstg_slots_reset_pos). */
static void fa_exchange(FightstgModelParams *a, FightstgModelParams *b) {
    s16 t;

    t = a->pos.x;
    a->pos.x = b->pos.x;
    b->pos.x = t;
    t = a->pos.z;
    a->pos.z = b->pos.z;
    b->pos.z = t;
    t = a->start_pos.x;
    a->start_pos.x = b->start_pos.x;
    b->start_pos.x = t;
    t = a->start_pos.z;
    a->start_pos.z = b->start_pos.z;
    b->start_pos.z = t;
    a->rot.y = (a->rot.y + 0x800) & 0xFFF;
    b->rot.y = (b->rot.y + 0x800) & 0xFFF;
    a->start_rot.y = (a->start_rot.y + 0x800) & 0xFFF;
    b->start_rot.y = (b->start_rot.y + 0x800) & 0xFFF;
}

/* 1 while the two rows stand exchanged: slot 0's start_pos.z > 0, which only fa_exchange makes true (see the top). The
 * approach and the jump keep the disc's side test and invert it then (fightstg_script_run_model op 8, fightstg_jump_
 * update), so outside an exchange they are the disc's, whatever a script staged. */
s32 def_form_anim_rows_exchanged(void) {
    FightstgSlots *slots = (FightstgSlots *)heap_objects.find(0x14, -1, -1);
    FightstgModelParams *party = slots != NULL ? slots->get_params(slots, 0) : NULL;

    return party != NULL && party->start_pos.z > 0;
}

/* Called at the start of a script (fightstg_script_update's INIT) with the script's own side, its model and its index:
 * 1 when this script's animation now runs with the two rows exchanged, and then its end must call
 * def_form_anim_swap_end. */
s32 def_form_anim_swap_begin(FightstgSlots *slots, s32 side, s32 model_id, s32 script) {
    FightstgModelParams *party;
    FightstgModelParams *enemy;
    s32 i;

    if (side == 0 || script < 5 || slots == NULL) {
        return 0;
    }
    if (fightstg_enemy_find_record(model_id) >= 0) { /* an enemy species of the disc: its scripts are its own side's */
        return 0;
    }
    party = slots->get_params(slots, 0);
    if (party != NULL && party->start_pos.z > 0) { /* already exchanged (another script's): never twice */
        return 0;
    }
    for (i = 0; i < 3; i++) {
        party = slots->get_params(slots, fa_slots[i * 2]);
        enemy = slots->get_params(slots, fa_slots[i * 2 + 1]);
        if (party != NULL && enemy != NULL) {
            fa_exchange(party, enemy);
        }
    }
    fa_trace("begin", slots, 0, 0x10, model_id, script);
    return 1;
}

/* The script that exchanged them has ended: both sides back to their resting place. */
void def_form_anim_swap_end(FightstgSlots *slots) {
    s32 i;

    if (slots == NULL) {
        return;
    }
    fa_trace("end", slots, 0, 0x10, 0, 0);
    for (i = 0; i < (s32)(sizeof(fa_slots) / sizeof(fa_slots[0])); i++) {
        if (slots->get_params(slots, fa_slots[i]) != NULL) {
            slots->reset_pos(slots, fa_slots[i]);
        }
    }
}
