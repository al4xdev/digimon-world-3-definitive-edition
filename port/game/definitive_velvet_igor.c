/* Igor's head in the Velvet Room (the owner's, 2026-11-17): the room's host is the game's suited figure (actor 109,
 * P055GMGD) with a head drawn by hand for this port over the figure's own (definitive_velvet_igor.h). Only the head is
 * ours: the body, its idle and its shadow stay the game's. */
#include "common.h"

#include "fieldstg.h"
#include "gfx.h"

#include "psyq/libgpu.h"

#include "definitive_velvet_igor.h"

/* The frames the head fits, as sub-files of the figure's sprite file, and how far down the frame each draws the head.
 * The figure's idle facing down-left cycles 9, 10, 11, 10: 11 is 9 a row lower; 10, the in-between, keeps 9's place
 * (two dark pixels of its nape stay out, against the head's outline). Igor keeps that facing (no_turn, set with the
 * room's figures); any other frame keeps the game's head. */
static const struct {
    u16 frame;
    s16 down;
} igor_frames[] = { { VELVET_IGOR_FRAME, 0 }, { 10, 0 }, { 11, 1 } };

/* Called by the room's actor draw for Igor (definitive_velvet.c's dw3_velvet_actor_art), before FIELDSTG takes the
 * figure's own packet: the head's quads go into the actor's ordering-table entry first, so they draw after (over) the
 * figure that is added to the same entry later. (x, y): the spot the figure's frame is placed from, as
 * fieldstg_actor_draw places it. Mirrored with the figure when it faces right (dir 5..7). Always 0: the game's figure
 * is drawn too. */
int dw3_velvet_igor_head(FieldstgActor *actor, u32 *ot, s32 x, s32 y) {
    u32 file = actor->frame_file >> 16;
    u32 sub = actor->frame_file & 0xFFFF;
    s32 flip = actor->dir >= 5;
    s32 left, top, i, down = -1;

    if (file != VELVET_IGOR_FILE || actor->frame_w != VELVET_IGOR_W) {
        return 0;
    }
    for (i = 0; i < (s32)(sizeof(igor_frames) / sizeof(igor_frames[0])); i++) {
        if (igor_frames[i].frame == sub) {
            down = igor_frames[i].down;
        }
    }
    if (down < 0) {
        return 0;
    }
    left = x + (flip ? -(actor->frame_w + actor->frame_x) : actor->frame_x);
    top = y + actor->frame_y + down;
    if (left < -VELVET_IGOR_W || left > 320 + 64 || top < -64 || top > 240) {
        return 0;
    }
    for (i = 0; i < VELVET_IGOR_RUNS; i++) {
        const unsigned char *run = velvet_igor_runs[i];
        const unsigned char *rgb = velvet_igor_palette[run[3] & 15];
        POLY_F4 *poly = gfx_module.funcs.get_packet();
        /* mirrored, the frame's column c lands at its right edge minus c (the game's flipped quad) */
        s32 x0 = flip ? left + VELVET_IGOR_W - run[1] - run[2] : left + run[1];
        s32 x1 = x0 + run[2];
        s32 y0 = top + run[0];

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
    return 0;
}
