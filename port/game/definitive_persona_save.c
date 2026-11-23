/* The enemy personas' discovery, kept per slot (definitive_persona.h "Discovery"): the expansion save's "personas"
 * section (definitive_expansion.h), a KEEP id each:
 *
 *   "personas": {"3": {"known": [5, 0], "solved": false}, "7": {"known": [127, 8], "solved": true}}
 *
 * known: the element bits learnt of each map (phase A's, phase B's own), the union of every fight's; solved: every
 * weakness of every phase found once (definitive_persona.c), after which the inspect shows the whole map. Only ids
 * with anything learnt are written. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psxstack/savestate.h"

#include "../third_party/cjson/cJSON.h"
#include "definitive_expansion.h"
#include "definitive_persona.h"

extern void port_log(const char *fmt, ...);

/* Before the expansion, the card bytes it now owns (DEF_EXPANSION_CARD_BYTES) were these: a "solved" bit per KEEP id,
 * low byte first, no header. */
#define OLD_CARD_BITS 144

static struct {
    unsigned char known[2];
    unsigned char solved;
} kept[DEF_PERSONA_KEEPS];

static int valid(int id) {
    return id >= 0 && id < DEF_PERSONA_KEEPS;
}

int def_persona_record_known(int id, int phase) {
    return valid(id) ? kept[id].known[phase & 1] : 0;
}

void def_persona_record_learn(int id, int phase, int bits) {
    if (valid(id)) {
        kept[id].known[phase & 1] |= (unsigned char)(bits & 0x7F);
    }
}

int def_persona_record_solved(int id) {
    return valid(id) && kept[id].solved != 0;
}

void def_persona_record_solve(int id) {
    if (valid(id) && !kept[id].solved) {
        kept[id].solved = 1;
        port_log("persona: KEEP %d solved", id);
    }
}

static void reset(void) {
    memset(kept, 0, sizeof(kept));
}

static int any(void) {
    int i;

    for (i = 0; i < DEF_PERSONA_KEEPS; i++) {
        if (kept[i].known[0] | kept[i].known[1] | kept[i].solved) {
            return 1;
        }
    }
    return 0;
}

static int small(const cJSON *v) {
    return cJSON_IsNumber(v) && v->valuedouble >= 0 && v->valuedouble < 256 ? (int)v->valuedouble & 0x7F : 0;
}

/* A KEEP id's key: decimal digits only, in range; -1 otherwise (ignored). */
static int id_of(const char *key) {
    char *end;
    long id;

    if (key == NULL || key[0] < '0' || key[0] > '9') {
        return -1;
    }
    id = strtol(key, &end, 10);
    return *end == 0 && id >= 0 && id < DEF_PERSONA_KEEPS ? (int)id : -1;
}

static void load(const cJSON *section) {
    const cJSON *p, *ids = cJSON_IsObject(section) ? section : NULL;

    cJSON_ArrayForEach(p, ids) {
        const cJSON *known = cJSON_GetObjectItemCaseSensitive(p, "known");
        int id = id_of(p->string);

        if (id >= 0 && cJSON_IsObject(p)) {
            kept[id].known[0] = (unsigned char)small(cJSON_IsArray(known) ? cJSON_GetArrayItem(known, 0) : NULL);
            kept[id].known[1] = (unsigned char)small(cJSON_IsArray(known) ? cJSON_GetArrayItem(known, 1) : NULL);
            kept[id].solved = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(p, "solved"));
        }
    }
}

static void save(cJSON *section) {
    int id;

    for (id = 0; id < DEF_PERSONA_KEEPS; id++) {
        char key[12];
        cJSON *p, *known;

        if ((kept[id].known[0] | kept[id].known[1] | kept[id].solved) == 0) {
            continue;
        }
        snprintf(key, sizeof(key), "%d", id);
        p = cJSON_AddObjectToObject(section, key);
        known = cJSON_AddArrayToObject(p, "known");
        cJSON_AddItemToArray(known, cJSON_CreateNumber(kept[id].known[0]));
        cJSON_AddItemToArray(known, cJSON_CreateNumber(kept[id].known[1]));
        cJSON_AddBoolToObject(p, "solved", kept[id].solved);
    }
}

/* Each old bit set: that id solved, its whole map shown (what the bit meant). */
static int migrate_from_card(const unsigned char *bytes, int size) {
    int i, n = 0;

    for (i = 0; i < OLD_CARD_BITS && i / 8 < size; i++) {
        if ((bytes[i / 8] >> (i % 8)) & 1) {
            kept[i].known[0] = kept[i].known[1] = 0x7F;
            kept[i].solved = 1;
            n++;
        }
    }
    return n;
}

static void savestate(struct PortState *s) {
    PORT_STATE_VAR(s, kept);
}

const DefExpansionSection def_persona_section = { "personas", reset, load, save, any, migrate_from_card, savestate };
