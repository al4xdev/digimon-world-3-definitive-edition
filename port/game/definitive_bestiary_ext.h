#ifndef DEFINITIVE_BESTIARY_EXT_H
#define DEFINITIVE_BESTIARY_EXT_H

#include "common.h"

enum {
    DEF_BS_CAT_ALL = 0,
    DEF_BS_CAT_WILD,
    DEF_BS_CAT_BOSS,
    DEF_BS_CAT_RARE,
    DEF_BS_CAT_SUPER,
    DEF_BS_CAT_DRI,
    DEF_BS_CAT_DARK,
    DEF_BS_CAT_COUNT
};

typedef struct DefBestiaryEntry {
    u16 key;
    u8 category;
    u8 recolor;
    s16 species_id;
    s16 sdigiedt_idx;
    s16 ref_a;
    s16 ref_b;
    s16 hue[2];
    s16 persona_idx;
    u8 phases;
} DefBestiaryEntry;

void def_bestiary_ext_build(void);
int def_bestiary_ext_count(void);
const DefBestiaryEntry *def_bestiary_ext_get(int idx);

int def_bestiary_ext_seen(const DefBestiaryEntry *e);
int def_bestiary_ext_known(const DefBestiaryEntry *e, int phase);
void def_bestiary_ext_mark(u16 key, int element_mask);
void def_bestiary_ext_resists(const DefBestiaryEntry *e, int phase, short out[7]);

void def_bestiary_ext_record_enemy(int species_id, int element_mask);
const char *def_bestiary_ext_cat_tag(int category);

#endif
