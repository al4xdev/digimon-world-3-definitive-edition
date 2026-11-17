/* The MATERIALS screen's data (src/main/fieldmenu.c draws it in the bestiary's windows): the Hunt Materials the player
 * holds, the rare items ("<species> Core <numeral>") and the Super Rare hunts' crystals ("<species> Crystal"). They live in the card's record, not in the bag, so the game's
 * ITEMS page cannot list them: this is their view, one entry in the menu next to the Guide and the bestiary.
 *
 * Entry i < DEF_HUNT_MATERIALS is a common material; the rest are the Rare Targets' rare items, one per target
 * (def_hunt_targets order), then a crystal per hunt (def_super_rares order). A rare item is named only once it was ever
 * held (def_record_rare_seen), a crystal once its hunt was won (def_super_done) or one is held; "???" before. */
#include "common.h"

#include "definitive.h"

extern const unsigned char def_str_Unknown[], def_str_HuntHeld[], def_str_MatCommon[],
    def_str_MatRare[], def_str_MatCrystal[], def_str_SupCrystalPre[], def_str_SupCrystalPost[], def_str_MatCut[];

#define MAT_LINE 40 /* glyph bytes of a name: "Gargomon Core VIII" and room */
#define MAT_SLOTS 8 /* the windows keep the pointers: a row of the list each, and the detail's */
#define MAT_FIT 21  /* glyphs a crystal's name may have: past it, the list's column and the detail cut it off
                     * ("MasterTyrannomon Crystal" showed "MasterTyrannomon Cryst"); its species is shortened instead */

#define MAT_CRYSTALS (DEF_HUNT_MATERIALS + def_hunt_target_count) /* the first crystal's entry */

int def_mat_count(void) {
    return MAT_CRYSTALS + DEF_SUPER_COUNT;
}

int def_mat_known(int i) {
    if (i < DEF_HUNT_MATERIALS) {
        return 1;
    }
    if (i >= MAT_CRYSTALS) {
        return i - MAT_CRYSTALS < def_super_done() || def_super_crystals(i - MAT_CRYSTALS) > 0;
    }
    return def_record_rare_seen(i - DEF_HUNT_MATERIALS) != 0;
}

int def_mat_held(int i) {
    if (i < DEF_HUNT_MATERIALS) {
        return def_record_material(i);
    }
    if (i >= MAT_CRYSTALS) {
        return def_super_crystals(i - MAT_CRYSTALS);
    }
    return def_record_rare(i - DEF_HUNT_MATERIALS);
}

/* Glyphs in an encoded string: a byte each, but 1 starts a two-byte glyph (the font's second page). */
static int glyphs(const unsigned char *s) {
    int n = 0;

    while (s != NULL && *s != 0) {
        s += s[0] == 1 && s[1] != 0 ? 2 : 1;
        n++;
    }
    return n;
}

/* The first `n` glyphs of `s`. */
static void put_glyphs(unsigned char *buf, int *p, int size, const unsigned char *s, int n) {
    while (s != NULL && *s != 0 && n-- > 0) {
        int w = s[0] == 1 && s[1] != 0 ? 2 : 1;

        if (*p + w > size - 1) {
            break;
        }
        while (w-- > 0) {
            buf[(*p)++] = *s++;
        }
    }
    buf[*p] = 0;
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
    if (i >= MAT_CRYSTALS) {
        const unsigned char *name = def_species_name(def_super_rares[i - MAT_CRYSTALS].species);

        int words = glyphs(def_str_SupCrystalPre) + glyphs(def_str_SupCrystalPost), keep;

        name = name != NULL ? name : def_str_Unknown;
        keep = words + glyphs(name) <= MAT_FIT ? glyphs(name) : MAT_FIT - words - glyphs(def_str_MatCut);
        def_glyph_put(buf[slot], &p, MAT_LINE, def_str_SupCrystalPre);
        put_glyphs(buf[slot], &p, MAT_LINE, name, keep);
        if (keep < glyphs(name)) {
            def_glyph_put(buf[slot], &p, MAT_LINE, def_str_MatCut); /* "MasterTyrann. Crystal" */
        }
        def_glyph_put(buf[slot], &p, MAT_LINE, def_str_SupCrystalPost);
        return buf[slot];
    }
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
    return i < DEF_HUNT_MATERIALS ? def_str_MatCommon : i >= MAT_CRYSTALS ? def_str_MatCrystal : def_str_MatRare;
}
