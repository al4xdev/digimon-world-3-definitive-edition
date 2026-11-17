/* The MATERIALS screen's data (src/main/fieldmenu.c draws it in the bestiary's windows): the Hunt Materials the player
 * holds and the rare items ("<species> Core <numeral>"). They live in the card's record, not in the bag, so the game's
 * ITEMS page cannot list them: this is their view, one entry in the menu next to the Guide and the bestiary.
 *
 * Entry i < DEF_HUNT_MATERIALS is a common material; the rest are the Rare Targets' rare items, one per target
 * (def_hunt_targets order). A rare item is named only once it was ever held (def_record_rare_seen), "???" before. */
#include "common.h"

#include "definitive.h"

extern const unsigned char def_str_Unknown[], def_str_HuntHeld[], def_str_MatCommon[],
    def_str_MatRare[];

#define MAT_LINE 40 /* glyph bytes of a name: "Gargomon Core VIII" and room */
#define MAT_SLOTS 8 /* the windows keep the pointers: a row of the list each, and the detail's */

int def_mat_count(void) {
    return DEF_HUNT_MATERIALS + def_hunt_target_count;
}

int def_mat_known(int i) {
    if (i < DEF_HUNT_MATERIALS) {
        return 1;
    }
    return def_record_rare_seen(i - DEF_HUNT_MATERIALS) != 0;
}

int def_mat_held(int i) {
    if (i < DEF_HUNT_MATERIALS) {
        return def_record_material(i);
    }
    return def_record_rare(i - DEF_HUNT_MATERIALS);
}

const unsigned char *def_mat_name(int i, int slot) {
    static unsigned char buf[MAT_SLOTS][MAT_LINE];
    int p = 0;

    if (!def_mat_known(i)) {
        return def_str_Unknown;
    }
    if (i < DEF_HUNT_MATERIALS) {
        return def_hunt_material_name(i);
    }
    slot &= MAT_SLOTS - 1;
    buf[slot][0] = 0;
    def_hunt_target_name(buf[slot], &p, MAT_LINE, i - DEF_HUNT_MATERIALS);
    return buf[slot];
}

/* "x12": how many are held. */
const unsigned char *def_mat_held_text(int i, int slot) {
    static unsigned char buf[MAT_SLOTS][8];
    int p = 0;

    slot &= MAT_SLOTS - 1;
    buf[slot][0] = 0;
    def_glyph_put(buf[slot], &p, 8, def_str_HuntHeld);
    def_glyph_count(buf[slot], &p, 8, def_mat_held(i));
    return buf[slot];
}

const unsigned char *def_mat_kind(int i) {
    return i < DEF_HUNT_MATERIALS ? def_str_MatCommon : def_str_MatRare;
}
