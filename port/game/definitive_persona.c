/* Enemy personas: the table's rules and a battle's state (definitive_persona.h). Nothing here reads the game's battle:
 * definitive.c asks and tells (who is fought, a turn, a stagger, a hit), so the host test drives it all. */
#include <string.h>

#include "definitive.h"
#include "definitive_persona.h"

#include "psxstack/savestate.h"

extern const DefHuntTarget def_hunt_targets[];
extern const int def_hunt_target_count;

/* The family bit of each element (Fire, Water, Ice, Wind, Thunder, Machine, Dark). */
static const unsigned char family_of[7] = { FIRE, WATER, WATER, WIND, THUNDER, THUNDER, DARK };

static int elements_of(int families) {
    int e, out = 0;

    for (e = 0; e < 7; e++) {
        if (families & family_of[e]) {
            out |= 1 << e;
        }
    }
    return out;
}

/* ---- The table. */

static int matches(const DefPersona *p, const DefPersonaWhere *w, int species) {
    switch (p->kind) {
    case DEF_PERSONA_SUPER:
        return w->super >= 0 && p->a == w->super;
    case DEF_PERSONA_RARE:
        return w->super < 0 && w->region >= 0 && p->a == w->region;
    case DEF_PERSONA_DRI:
        return w->dri >= 0 && p->a == w->dri;
    case DEF_PERSONA_CAMPAIGN:
        return w->super < 0 && w->region < 0 && w->dri < 0 && p->a == w->battle && p->b == species;
    default:
        return 0;
    }
}

/* An encounter's row first (the Rare Target that is a Tyrannomon is not every Tyrannomon), then the species'. */
const DefPersona *def_persona_find(const DefPersonaWhere *w, int species) {
    int i;

    if (w == NULL || species <= 0) {
        return NULL;
    }
    for (i = 0; i < def_persona_count; i++) {
        if (matches(&def_personas[i], w, species)) {
            return &def_personas[i];
        }
    }
    for (i = 0; i < def_persona_count; i++) {
        if (def_personas[i].kind == DEF_PERSONA_SPECIES && def_personas[i].a == species) {
            return &def_personas[i];
        }
    }
    return NULL;
}

/* The phase whose map applies: a phase B that names no weakness or resistance keeps A's. */
static int map_phase(const DefPersona *p, int phase) {
    if (phase == 1 && p->phases == 2 && (p->phase[1].weak | p->phase[1].resist) != 0) {
        return 1;
    }
    return 0;
}

int def_persona_own_map(const DefPersona *p) {
    return p != NULL && (p->phase[0].weak | p->phase[0].resist | p->phase[1].weak | p->phase[1].resist) != 0;
}

int def_persona_weak(const DefPersona *p, int phase) {
    return p != NULL ? p->phase[map_phase(p, phase & 1)].weak : 0;
}

int def_persona_guard(const DefPersona *p, int phase) {
    return p != NULL && p->phase[phase & 1].guard != 0;
}

int def_persona_hue(const DefPersona *p, int phase) {
    return p != NULL ? p->phase[phase & 1].hue : 0;
}

void def_persona_resists(const DefPersona *p, int phase, const short base[7], short out[7]) {
    const DefPersonaPhase *ph;
    int e;

    if (!def_persona_own_map(p)) {
        if (out != base) {
            memcpy(out, base, 7 * sizeof(short));
        }
        return;
    }
    ph = &p->phase[map_phase(p, phase & 1)];
    for (e = 0; e < 7; e++) {
        out[e] = ph->weak & family_of[e] ? DEF_PERSONA_WEAK
                 : ph->resist & family_of[e] ? DEF_PERSONA_RESIST
                                             : DEF_PERSONA_NEUTRAL;
    }
}

static int report_one(void (*report)(const char *, int), const char *what, int row) {
    if (report != NULL) {
        report(what, row);
    }
    return 1;
}

static int same_who(const DefPersona *x, const DefPersona *y) {
    return x->kind == y->kind && x->a == y->a && (x->kind != DEF_PERSONA_CAMPAIGN || x->b == y->b);
}

int def_persona_check_rows(const DefPersona *rows, int count, void (*report)(const char *what, int row)) {
    int i, j, k, bad = 0;

    for (i = 0; i < count; i++) {
        const DefPersona *p = &rows[i];

        switch (p->kind) {
        case DEF_PERSONA_SUPER:
            if (p->a < 0 || p->a >= DEF_SUPER_COUNT) {
                bad += report_one(report, "SUPER: no such hunt", i);
            }
            break;
        case DEF_PERSONA_RARE:
            for (k = 0; k < def_hunt_target_count && def_hunt_targets[k].region != p->a; k++) {
            }
            if (k == def_hunt_target_count) {
                bad += report_one(report, "RARE: no Rare Target in that region", i);
            }
            break;
        case DEF_PERSONA_DRI:
            if (p->a < 0 || p->a >= DEF_DRI_CHALLENGES) {
                bad += report_one(report, "DRI: no such challenge", i);
            }
            break;
        case DEF_PERSONA_CAMPAIGN:
            if (p->a <= 0 || p->b <= 0) {
                bad += report_one(report, "CAMPAIGN: needs a battle and a species", i);
            }
            break;
        case DEF_PERSONA_SPECIES:
            if (p->a <= 0) {
                bad += report_one(report, "SPECIES: no species", i);
            }
            if (p->phase[0].hue != 0 || p->phase[1].hue != 0) {
                bad += report_one(report, "SPECIES: a hue is an encounter's (a common enemy keeps its colours)", i);
            }
            break;
        default:
            bad += report_one(report, "no who (SUPER, RARE, DRI, CAMPAIGN or SPECIES)", i);
        }
        for (j = 0; j < i; j++) {
            if (same_who(p, &rows[j])) {
                bad += report_one(report, "the same encounter twice", i);
            }
            if (p->keep != 0 && p->keep == rows[j].keep) {
                bad += report_one(report, "KEEP bit used twice", i);
            }
        }
        if (def_persona_own_map(p) && p->kind != DEF_PERSONA_SPECIES && p->keep == 0) {
            bad += report_one(report, "changes the map but has no KEEP bit", i);
        }
        if (p->keep > DEF_PERSONA_KEEP_BITS) {
            bad += report_one(report, "KEEP bit out of the card's room", i);
        }
        for (k = 0; k < 2; k++) {
            if (p->phase[k].weak & p->phase[k].resist) {
                bad += report_one(report, "a family both weak and resisted", i);
            }
            if ((p->phase[k].weak | p->phase[k].resist) & ~DEF_PERSONA_FAMILIES) {
                bad += report_one(report, "not a family (FIRE, WATER, WIND, THUNDER, DARK)", i);
            }
        }
        if (p->phases != 0 && p->phases != 1 && p->phases != 2) {
            bad += report_one(report, "phases: 1 or 2", i);
        }
        if (p->phases != 2 && memcmp(&p->phase[1], &(DefPersonaPhase){ 0 }, sizeof(DefPersonaPhase)) != 0 &&
            !(p->phase[1].guard && p->phase[0].guard && p->phase[1].weak == 0 && p->phase[1].resist == 0 &&
              p->phase[1].hue == 0)) {
            bad += report_one(report, "phase B set without PHASES or ON_STAGGER", i);
        }
        if (p->phases == 2) {
            int map_b = map_phase(p, 1) != 0;

            if (!map_b && p->phase[0].guard == p->phase[1].guard && p->phase[0].hue == p->phase[1].hue) {
                bad += report_one(report, "two phases that are the same", i);
            }
            if (map_b && p->kind != DEF_PERSONA_SPECIES &&
                (p->phase[0].hue == 0 || p->phase[1].hue == 0 || p->phase[0].hue == p->phase[1].hue)) {
                bad += report_one(report, "a weakness that changes needs a colour per phase (HUES)", i);
            }
        }
        if (p->speed_x > 5) {
            bad += report_one(report, "SPEED_X past 5", i);
        }
        if (p->kind == DEF_PERSONA_SUPER) { /* no two Super Rares the same puzzle */
            for (j = 0; j < i; j++) {
                const DefPersona *q = &rows[j];

                if (q->kind == DEF_PERSONA_SUPER && q->phases == p->phases && q->turns == p->turns &&
                    q->speed_x == p->speed_x && q->phase[0].weak == p->phase[0].weak &&
                    q->phase[0].resist == p->phase[0].resist && q->phase[0].guard == p->phase[0].guard &&
                    q->phase[1].weak == p->phase[1].weak && q->phase[1].guard == p->phase[1].guard) {
                    bad += report_one(report, "the same puzzle as another Super Rare", i);
                }
            }
        }
    }
    return bad;
}

int def_persona_check(void (*report)(const char *what, int row)) {
    return def_persona_check_rows(def_personas, def_persona_count, report);
}

/* ---- A battle. */

static DefPersonaWhere where;
static struct {
    short species;              /* whose row it is (0: not looked up yet) */
    short row;                  /* index in def_personas, -1 none */
    signed char phase;
    unsigned char turns;        /* the enemy's turns in this phase */
    unsigned char known[2];     /* element bits learnt this battle, per map */
} slots[3];

int def_persona_same_where(const DefPersonaWhere *w) {
    return w->super == where.super && w->region == where.region && w->dri == where.dri && w->battle == where.battle;
}

void def_persona_battle_begin(const DefPersonaWhere *w) {
    int i;

    where = *w;
    memset(slots, 0, sizeof(slots));
    for (i = 0; i < 3; i++) {
        slots[i].row = -1;
    }
}

const DefPersona *def_persona_slot(int slot, int species) {
    const DefPersona *p;

    if (slot < 0 || slot > 2 || species <= 0) {
        return NULL;
    }
    if (slots[slot].species != species) { /* a new enemy here (a DRI wave): its own state */
        memset(&slots[slot], 0, sizeof(slots[slot]));
        slots[slot].species = (short)species;
        p = def_persona_find(&where, species);
        slots[slot].row = p != NULL ? (short)(p - def_personas) : -1;
    }
    return slots[slot].row >= 0 ? &def_personas[slots[slot].row] : NULL;
}

static const DefPersona *row_of(int slot) {
    return slot >= 0 && slot < 3 && slots[slot].row >= 0 ? &def_personas[slots[slot].row] : NULL;
}

int def_persona_phase(int slot) {
    return row_of(slot) != NULL ? slots[slot].phase : 0;
}

static int next_phase(int slot) {
    slots[slot].phase ^= 1;
    slots[slot].turns = 0;
    return 1;
}

int def_persona_enemy_turn(int slot) {
    const DefPersona *p = row_of(slot);

    if (p == NULL || p->phases != 2 || p->turns == 0) {
        return 0;
    }
    if (++slots[slot].turns < p->turns) {
        return 0;
    }
    return next_phase(slot);
}

int def_persona_staggered(int slot) {
    const DefPersona *p = row_of(slot);

    return p != NULL && p->phases == 2 && p->turns == 0 ? next_phase(slot) : 0;
}

/* Solved: every weakness of every phase learnt (a phase with none: its whole map). */
static int solved(int slot, const DefPersona *p) {
    int k, n = p->phases == 2 ? 2 : 1;

    for (k = 0; k < n; k++) {
        int need = p->phase[map_phase(p, k)].weak != 0 ? elements_of(p->phase[map_phase(p, k)].weak) : 0x7F;

        if ((slots[slot].known[map_phase(p, k)] & need) != need) {
            return 0;
        }
    }
    return 1;
}

int def_persona_known(int slot) {
    const DefPersona *p = row_of(slot);

    if (p == NULL) {
        return 0;
    }
    if (p->keep != 0 && def_record_persona_kept(p->keep - 1)) {
        return 0x7F;
    }
    return slots[slot].known[map_phase(p, slots[slot].phase)];
}

void def_persona_learn(int slot, int element_bits) {
    const DefPersona *p = row_of(slot);

    if (p == NULL) {
        return;
    }
    slots[slot].known[map_phase(p, slots[slot].phase)] |= (unsigned char)(element_bits & 0x7F);
    if (p->keep != 0 && solved(slot, p)) {
        def_record_persona_keep(p->keep - 1);
    }
}

/* The enemy's model is added before any slot was looked up, so the encounter's row is found from `where` alone (a
 * Super Rare, a Rare Target or a DRI line: the rows a hue is allowed on, CAMPAIGN aside, which needs the species). */
int def_persona_battle_hue(void) {
    int i;

    for (i = 0; i < 3; i++) {
        const DefPersona *p = row_of(i);

        if (p != NULL && p->kind != DEF_PERSONA_SPECIES && def_persona_hue(p, slots[i].phase) != 0) {
            return def_persona_hue(p, slots[i].phase);
        }
    }
    for (i = 0; i < def_persona_count; i++) {
        const DefPersona *p = &def_personas[i];

        if (p->kind != DEF_PERSONA_CAMPAIGN && p->kind != DEF_PERSONA_SPECIES && matches(p, &where, 0)) {
            return def_persona_hue(p, 0);
        }
    }
    return 0;
}

void def_persona_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, where);
    PORT_STATE_VAR(s, slots);
}
