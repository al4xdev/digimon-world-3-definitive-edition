#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "cdload.h"
#include "fieldstg.h"
#include "fightstg.h"
#include "records.h"

#include "psxstack/savestate.h"

#include "../third_party/cjson/cJSON.h"
#include "definitive.h"
#include "definitive_bestiary_ext.h"
#include "definitive_expansion.h"
#include "definitive_persona.h"

#define BS_EXT_MAX 512
#define BS_SAVE_MAX 512

extern s32 fightstg_enemy_find_record(s32 id);
extern int def_finale_enemy_dark(void);
extern int def_dri_fighting(void);
extern int def_purify_fighting(void);
extern int def_hunt_active(void);
extern int def_super_active(void);
extern int def_super_battle_ready(void);
extern int def_record_dri_best(int c);

typedef struct {
    u16 key;
    u8 mask;
    u8 seen;
} BsSaveSlot;

static BsSaveSlot saved[BS_SAVE_MAX];
static int saved_count;

static DefBestiaryEntry catalog[BS_EXT_MAX];
static int catalog_count;
static int catalog_built;

static u16 make_key(int cat, int id) {
    return (u16)(((cat & 0xF) << 12) | (id & 0xFFF));
}

static BsSaveSlot *find_slot(u16 key, int create) {
    int i;

    for (i = 0; i < saved_count; i++) {
        if (saved[i].key == key) {
            return &saved[i];
        }
    }
    if (!create || saved_count >= BS_SAVE_MAX) {
        return NULL;
    }
    saved[saved_count].key = key;
    saved[saved_count].mask = 0;
    saved[saved_count].seen = 0;
    return &saved[saved_count++];
}

void def_bestiary_ext_mark(u16 key, int element_mask) {
    BsSaveSlot *s = find_slot(key, 1);

    if (s != NULL) {
        s->seen = 1;
        s->mask |= (u8)(element_mask & 0x7F);
    }
}

static int find_persona_idx(int kind, int a, int b) {
    int i;

    for (i = 0; i < def_persona_count; i++) {
        const DefPersona *p = &def_personas[i];
        if (p->kind != kind || p->a != a) {
            continue;
        }
        if (kind == DEF_PERSONA_CAMPAIGN && p->b != b) {
            continue;
        }
        return i;
    }
    return -1;
}

static void add_entry(const DefBestiaryEntry *e) {
    if (catalog_count < BS_EXT_MAX) {
        catalog[catalog_count++] = *e;
    }
}

void def_bestiary_ext_build(void) {
    FightstgEnemyRecord *recs = (FightstgEnemyRecord *)cdload_module.files.get_file(0x1CF);
    int i, j, k;

    if (recs == NULL) {
        return;
    }
    catalog_count = 0;

    for (i = 0; i < DEF_BESTIARY_SPECIES && recs[i].id != 0; i++) {
        DefBestiaryEntry e;
        memset(&e, 0, sizeof(e));
        e.key = make_key(DEF_BS_CAT_WILD, i);
        e.category = DEF_BS_CAT_WILD;
        e.recolor = DEF_RECOLOR_NONE;
        e.species_id = recs[i].id;
        e.sdigiedt_idx = (s16)i;
        e.persona_idx = (s16)find_persona_idx(DEF_PERSONA_SPECIES, recs[i].id, 0);
        e.phases = (e.persona_idx >= 0 && def_personas[e.persona_idx].phases > 1) ? 2 : 1;
        add_entry(&e);
    }

    for (i = 0; i < def_persona_count; i++) {
        const DefPersona *p = &def_personas[i];
        DefBestiaryEntry e;

        if (p->kind != DEF_PERSONA_CAMPAIGN) {
            continue;
        }
        memset(&e, 0, sizeof(e));
        e.key = make_key(DEF_BS_CAT_BOSS, (p->a & 0x1FF) | ((i & 0x7) << 9));
        e.category = DEF_BS_CAT_BOSS;
        e.recolor = DEF_RECOLOR_NONE;
        e.species_id = p->b;
        e.sdigiedt_idx = (s16)fightstg_enemy_find_record(p->b);
        e.ref_a = p->a;
        e.persona_idx = (s16)i;
        e.phases = p->phases > 1 ? 2 : 1;
        e.hue[0] = p->phase[0].hue;
        e.hue[1] = p->phase[1].hue;
        if (e.hue[0] != 0 || e.hue[1] != 0) {
            e.recolor = DEF_RECOLOR_PERSONA;
        }
        add_entry(&e);
    }

    {
        static const s16 dark_gal[2] = { 466, 467 };
        for (i = 0; i < 2; i++) {
            s32 idx = fightstg_enemy_find_record(dark_gal[i]);
            if (idx >= 0) {
                DefBestiaryEntry e;
                memset(&e, 0, sizeof(e));
                e.key = make_key(DEF_BS_CAT_BOSS, 0xF00 + i);
                e.category = DEF_BS_CAT_BOSS;
                e.recolor = DEF_RECOLOR_BLACK;
                e.species_id = dark_gal[i];
                e.sdigiedt_idx = (s16)idx;
                e.persona_idx = -1;
                e.phases = 1;
                add_entry(&e);
            }
        }
    }

    for (i = 0; i < def_hunt_target_count; i++) {
        const DefHuntTarget *t = &def_hunt_targets[i];
        s32 sp = fieldstg_battles[t->battle].enemies[t->slot]->digimon;
        int pidx = find_persona_idx(DEF_PERSONA_RARE, t->region, 0);
        DefBestiaryEntry e;

        memset(&e, 0, sizeof(e));
        e.key = make_key(DEF_BS_CAT_RARE, i);
        e.category = DEF_BS_CAT_RARE;
        e.species_id = (s16)sp;
        e.sdigiedt_idx = (s16)fightstg_enemy_find_record(sp);
        e.ref_a = (s16)i;
        e.ref_b = t->region;
        e.persona_idx = (s16)pidx;
        e.phases = (pidx >= 0 && def_personas[pidx].phases > 1) ? 2 : 1;
        if (pidx >= 0 && (def_personas[pidx].phase[0].hue != 0 || def_personas[pidx].phase[1].hue != 0)) {
            e.recolor = DEF_RECOLOR_PERSONA;
            e.hue[0] = def_personas[pidx].phase[0].hue;
            e.hue[1] = def_personas[pidx].phase[1].hue;
        } else {
            e.recolor = DEF_RECOLOR_RARE;
        }
        add_entry(&e);
    }

    for (i = 0; i < DEF_SUPER_COUNT; i++) {
        const DefSuperRare *sr = &def_super_rares[i];
        int pidx = find_persona_idx(DEF_PERSONA_SUPER, i, 0);
        DefBestiaryEntry e;

        memset(&e, 0, sizeof(e));
        e.key = make_key(DEF_BS_CAT_SUPER, i);
        e.category = DEF_BS_CAT_SUPER;
        e.species_id = sr->species;
        e.sdigiedt_idx = (s16)fightstg_enemy_find_record(sr->species);
        e.ref_a = (s16)i;
        e.ref_b = sr->level;
        e.persona_idx = (s16)pidx;
        e.phases = (pidx >= 0 && def_personas[pidx].phases > 1) ? 2 : 1;
        if (pidx >= 0 && (def_personas[pidx].phase[0].hue != 0 || def_personas[pidx].phase[1].hue != 0)) {
            e.recolor = DEF_RECOLOR_PERSONA;
            e.hue[0] = def_personas[pidx].phase[0].hue;
            e.hue[1] = def_personas[pidx].phase[1].hue;
        } else {
            e.recolor = DEF_RECOLOR_SUPER;
        }
        add_entry(&e);
    }

    for (i = 0; i < def_dri_challenge_count && i < DEF_DRI_CHALLENGES; i++) {
        int pidx = find_persona_idx(DEF_PERSONA_DRI, i, 0);
        for (j = 0; j < DEF_DRI_STAGES; j++) {
            const DefDriStage *st = &def_dri_challenges[i].stages[j];
            DefBestiaryEntry e;

            if (st->digimon <= 0) {
                continue;
            }
            memset(&e, 0, sizeof(e));
            e.key = make_key(DEF_BS_CAT_DRI, i * 4 + j);
            e.category = DEF_BS_CAT_DRI;
            e.species_id = st->digimon;
            e.sdigiedt_idx = (s16)fightstg_enemy_find_record(st->base != 0 ? st->base : st->digimon);
            e.ref_a = (s16)i;
            e.ref_b = (s16)j;
            e.persona_idx = (s16)pidx;
            e.phases = (pidx >= 0 && def_personas[pidx].phases > 1) ? 2 : 1;
            if (pidx >= 0 && (def_personas[pidx].phase[0].hue != 0 || def_personas[pidx].phase[1].hue != 0)) {
                e.recolor = DEF_RECOLOR_PERSONA;
                e.hue[0] = def_personas[pidx].phase[0].hue;
                e.hue[1] = def_personas[pidx].phase[1].hue;
            } else {
                e.recolor = DEF_RECOLOR_NONE;
            }
            add_entry(&e);
        }
    }

    {
        s16 seen_dark[DEF_BLAST_PARTNERS * DEF_BLAST_STEPS];
        int n_dark = 0;

        for (i = 0; i < DEF_BLAST_PARTNERS; i++) {
            for (j = 0; j < DEF_BLAST_STEPS; j++) {
                s16 fid = records_digimon[i].blast_forms[j];
                int dup = 0;
                DefBestiaryEntry e;

                if (fid <= 0) {
                    continue;
                }
                for (k = 0; k < n_dark; k++) {
                    if (seen_dark[k] == fid) {
                        dup = 1;
                        break;
                    }
                }
                if (dup) {
                    continue;
                }
                seen_dark[n_dark++] = fid;
                memset(&e, 0, sizeof(e));
                e.key = make_key(DEF_BS_CAT_DARK, fid);
                e.category = DEF_BS_CAT_DARK;
                e.recolor = DEF_RECOLOR_DARK;
                e.species_id = fid;
                e.sdigiedt_idx = (s16)fightstg_enemy_find_record(fid);
                e.ref_a = (s16)i;
                e.ref_b = (s16)j;
                e.persona_idx = -1;
                e.phases = 1;
                add_entry(&e);
            }
        }
    }

    catalog_built = 1;
}

int def_bestiary_ext_count(void) {
    if (!catalog_built) {
        def_bestiary_ext_build();
    }
    return catalog_count;
}

const DefBestiaryEntry *def_bestiary_ext_get(int idx) {
    if (!catalog_built) {
        def_bestiary_ext_build();
    }
    return idx >= 0 && idx < catalog_count ? &catalog[idx] : NULL;
}

int def_bestiary_ext_known(const DefBestiaryEntry *e, int phase) {
    int mask = 0;
    BsSaveSlot *s;

    if (e == NULL) {
        return 0;
    }
    s = find_slot(e->key, 0);
    if (s != NULL) {
        mask |= s->mask;
    }
    if (e->persona_idx >= 0) {
        const DefPersona *p = &def_personas[e->persona_idx];
        if (p->keep != 0) {
            int kid = p->keep - 1;
            if (def_persona_record_solved(kid)) {
                return 0x7F;
            }
            mask |= def_persona_record_known(kid, phase & 1);
            return mask;
        }
    }
    if (e->category == DEF_BS_CAT_WILD && e->sdigiedt_idx >= 0) {
        mask |= def_bestiary_known(e->sdigiedt_idx);
    } else if (e->category == DEF_BS_CAT_RARE && def_record_rare_seen(e->ref_a)) {
        if (e->sdigiedt_idx >= 0) {
            mask |= def_bestiary_known(e->sdigiedt_idx);
        }
    } else if (e->category == DEF_BS_CAT_SUPER && def_super_state(e->ref_a) == DEF_SUPER_DONE) {
        mask |= 0x7F;
    } else if (e->category == DEF_BS_CAT_DRI && def_dri_tier_stages(def_record_dri_best(e->ref_a)) > e->ref_b) {
        if (e->sdigiedt_idx >= 0) {
            mask |= def_bestiary_known(e->sdigiedt_idx);
        }
    } else if (e->category == DEF_BS_CAT_DARK) {
        int i, n = def_record_blast_count();
        unsigned int pmask = def_record_blast_purified();
        for (i = 0; i < n; i++) {
            const DefBlastEntry *be = def_record_blast_entry(i);
            if (be != NULL && records_digimon[be->partner].blast_forms[be->step] == e->species_id &&
                ((pmask >> i) & 1u)) {
                mask |= 0x7F;
                break;
            }
        }
    }
    return mask & 0x7F;
}

int def_bestiary_ext_seen(const DefBestiaryEntry *e) {
    BsSaveSlot *s;

    if (e == NULL) {
        return 0;
    }
    s = find_slot(e->key, 0);
    if (s != NULL && s->seen) {
        return 1;
    }
    if (def_bestiary_ext_known(e, 0) != 0 || (e->phases > 1 && def_bestiary_ext_known(e, 1) != 0)) {
        return 1;
    }
    if (e->category == DEF_BS_CAT_RARE && def_record_rare_seen(e->ref_a)) {
        return 1;
    }
    if (e->category == DEF_BS_CAT_SUPER && def_super_state(e->ref_a) == DEF_SUPER_DONE) {
        return 1;
    }
    if (e->category == DEF_BS_CAT_DRI && def_dri_tier_stages(def_record_dri_best(e->ref_a)) > e->ref_b) {
        return 1;
    }
    return 0;
}

void def_bestiary_ext_resists(const DefBestiaryEntry *e, int phase, short out[7]) {
    FightstgEnemyRecord *recs = (FightstgEnemyRecord *)cdload_module.files.get_file(0x1CF);
    short base[7] = { 100, 100, 100, 100, 100, 100, 100 };
    int i;

    if (e != NULL && recs != NULL && e->sdigiedt_idx >= 0 && e->sdigiedt_idx < DEF_BESTIARY_SPECIES) {
        FightstgEnemyRecord copy = recs[e->sdigiedt_idx];
        def_enemy_override(&copy);
        for (i = 0; i < 7; i++) {
            base[i] = copy.resists[i];
        }
    }
    if (e != NULL && e->persona_idx >= 0) {
        def_persona_resists(&def_personas[e->persona_idx], phase & 1, base, out);
        return;
    }
    memcpy(out, base, sizeof(base));
}

void def_bestiary_ext_record_enemy(int species_id, int element_mask) {
    if (def_finale_enemy_dark()) {
        if (species_id == 466) {
            def_bestiary_ext_mark(make_key(DEF_BS_CAT_BOSS, 0xF00), element_mask);
        } else if (species_id == 467) {
            def_bestiary_ext_mark(make_key(DEF_BS_CAT_BOSS, 0xF01), element_mask);
        }
        return;
    }
    if (def_purify_fighting()) {
        def_bestiary_ext_mark(make_key(DEF_BS_CAT_DARK, species_id), element_mask);
        return;
    }
    if (def_super_battle_ready()) {
        int h = def_super_active();
        if (h >= 0) {
            def_bestiary_ext_mark(make_key(DEF_BS_CAT_SUPER, h), element_mask);
        }
        return;
    }
    if (def_hunt_active()) {
        int reg = def_hunt_region_fighting();
        int i;
        for (i = 0; i < def_hunt_target_count; i++) {
            if (def_hunt_targets[i].region == reg) {
                def_bestiary_ext_mark(make_key(DEF_BS_CAT_RARE, i), element_mask);
                return;
            }
        }
    }
    if (def_dri_fighting()) {
        int i, j;
        for (i = 0; i < def_dri_challenge_count && i < DEF_DRI_CHALLENGES; i++) {
            for (j = 0; j < DEF_DRI_STAGES; j++) {
                if (def_dri_challenges[i].stages[j].digimon == species_id) {
                    def_bestiary_ext_mark(make_key(DEF_BS_CAT_DRI, i * 4 + j), element_mask);
                    return;
                }
            }
        }
    }
    {
        int pidx = find_persona_idx(DEF_PERSONA_CAMPAIGN, records_state.battle, species_id);
        if (pidx >= 0) {
            def_bestiary_ext_mark(make_key(DEF_BS_CAT_BOSS, (records_state.battle & 0x1FF) | ((pidx & 0x7) << 9)),
                                  element_mask);
            return;
        }
    }
    {
        s32 idx = fightstg_enemy_find_record(species_id);
        if (idx >= 0 && idx < DEF_BESTIARY_SPECIES) {
            def_bestiary_ext_mark(make_key(DEF_BS_CAT_WILD, idx), element_mask);
        }
    }
}

const char *def_bestiary_ext_cat_tag(int category) {
    static const char *const tags[DEF_BS_CAT_COUNT] = {
        "ALL", "WILD", "BOSS", "RARE", "SUPER", "DRI", "DARK",
    };

    return category >= 0 && category < DEF_BS_CAT_COUNT ? tags[category] : "WILD";
}

static void ext_reset(void) {
    memset(saved, 0, sizeof(saved));
    saved_count = 0;
}

static int ext_any(void) {
    return saved_count > 0;
}

static void ext_load(const cJSON *section) {
    const cJSON *item;
    const cJSON *root = cJSON_IsObject(section) ? section : NULL;

    cJSON_ArrayForEach(item, root) {
        char *end = NULL;
        long key;

        if (item->string == NULL || item->string[0] < '0' || item->string[0] > '9') {
            continue;
        }
        key = strtol(item->string, &end, 10);
        if (*end != '\0' || key < 0 || key > 0xFFFF || !cJSON_IsNumber(item)) {
            continue;
        }
        {
            int v = (int)item->valuedouble;
            BsSaveSlot *s = find_slot((u16)key, 1);
            if (s != NULL) {
                s->seen = (u8)((v & 0x80) != 0 || (v & 0x7F) != 0);
                s->mask = (u8)(v & 0x7F);
            }
        }
    }
}

static void ext_save(cJSON *section) {
    int i;

    for (i = 0; i < saved_count; i++) {
        char key[12];
        int val;

        if (!saved[i].seen && saved[i].mask == 0) {
            continue;
        }
        val = (saved[i].mask & 0x7F) | (saved[i].seen ? 0x80 : 0);
        snprintf(key, sizeof(key), "%u", (unsigned)saved[i].key);
        cJSON_AddNumberToObject(section, key, val);
    }
}

static void ext_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, saved);
    PORT_STATE_VAR(s, saved_count);
}

const DefExpansionSection def_bestiary_ext_section = {
    "bestiary", ext_reset, ext_load, ext_save, ext_any, NULL, ext_savestate,
};
