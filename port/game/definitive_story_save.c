#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psxstack/savestate.h"

#include "../third_party/cjson/cJSON.h"
#include "definitive.h"
#include "definitive_expansion.h"

#define STORY_ANSWER_BYTES (DEF_STORY_EXT_ANSWERS / 4)

static unsigned char ext_answers[STORY_ANSWER_BYTES];
static short ext_bonds[DEF_STORY_BONDS];
static short ext_rep;
static short ext_npc_seen[DEF_STORY_NPC_SLOTS];
static int ext_npc_init;

static void ensure_npc_init(void) {
    int i;

    if (!ext_npc_init) {
        for (i = 0; i < DEF_STORY_NPC_SLOTS; i++) {
            ext_npc_seen[i] = -1;
        }
        ext_npc_init = 1;
    }
}

int def_story_save_answer(int question) {
    if (question < 0 || question >= DEF_STORY_EXT_ANSWERS) {
        return 0;
    }
    return (ext_answers[question >> 2] >> ((question & 3) * 2)) & 3;
}

void def_story_save_set_answer(int question, int option) {
    int shift;

    if (question < 0 || question >= DEF_STORY_EXT_ANSWERS) {
        return;
    }
    option = option < 0 ? 0 : option > 3 ? 3 : option;
    shift = (question & 3) * 2;
    ext_answers[question >> 2] = (unsigned char)((ext_answers[question >> 2] & ~(3 << shift)) | (option << shift));
}

void def_story_save_import_legacy(const unsigned char *legacy, int bytes) {
    int i, n = bytes < STORY_ANSWER_BYTES ? bytes : STORY_ANSWER_BYTES;

    for (i = 0; i < n * 4; i++) {
        if (def_story_save_answer(i) == 0) {
            int v = (legacy[i >> 2] >> ((i & 3) * 2)) & 3;
            if (v != 0) {
                def_story_save_set_answer(i, v);
            }
        }
    }
}

void def_story_save_export_legacy(unsigned char *legacy, int bytes) {
    int n = bytes < STORY_ANSWER_BYTES ? bytes : STORY_ANSWER_BYTES;

    memcpy(legacy, ext_answers, (size_t)n);
}

int def_story_bond(int ally) {
    if (ally < 0 || ally >= DEF_STORY_BONDS) {
        return 0;
    }
    return ext_bonds[ally];
}

void def_story_bond_add(int ally, int delta) {
    int v;

    if (ally < 0 || ally >= DEF_STORY_BONDS || delta == 0) {
        return;
    }
    v = ext_bonds[ally] + delta;
    if (v < -100) {
        v = -100;
    } else if (v > 100) {
        v = 100;
    }
    ext_bonds[ally] = (short)v;
}

int def_story_rep(void) {
    return ext_rep;
}

void def_story_rep_add(int delta) {
    int v;

    if (delta == 0) {
        return;
    }
    v = ext_rep + delta;
    if (v < -100) {
        v = -100;
    } else if (v > 100) {
        v = 100;
    }
    ext_rep = (short)v;
}

int def_story_npc_seen_stage(int npc_id) {
    ensure_npc_init();
    if (npc_id < 0 || npc_id >= DEF_STORY_NPC_SLOTS) {
        return -1;
    }
    return ext_npc_seen[npc_id];
}

void def_story_npc_mark_seen(int npc_id, int stage) {
    ensure_npc_init();
    if (npc_id < 0 || npc_id >= DEF_STORY_NPC_SLOTS) {
        return;
    }
    if (stage > ext_npc_seen[npc_id]) {
        ext_npc_seen[npc_id] = (short)stage;
    }
}

static void reset(void) {
    int i;

    memset(ext_answers, 0, sizeof(ext_answers));
    memset(ext_bonds, 0, sizeof(ext_bonds));
    ext_rep = 0;
    for (i = 0; i < DEF_STORY_NPC_SLOTS; i++) {
        ext_npc_seen[i] = -1;
    }
    ext_npc_init = 1;
}

static void load(const cJSON *section) {
    const cJSON *obj, *item;
    int i;

    reset();
    if (!cJSON_IsObject(section)) {
        return;
    }
    obj = cJSON_GetObjectItemCaseSensitive(section, "answers");
    if (cJSON_IsObject(obj)) {
        cJSON_ArrayForEach(item, obj) {
            if (item->string != NULL && cJSON_IsNumber(item)) {
                int q = atoi(item->string);
                def_story_save_set_answer(q, (int)item->valuedouble);
            }
        }
    }
    obj = cJSON_GetObjectItemCaseSensitive(section, "bonds");
    if (cJSON_IsArray(obj)) {
        i = 0;
        cJSON_ArrayForEach(item, obj) {
            if (i < DEF_STORY_BONDS && cJSON_IsNumber(item)) {
                int v = (int)item->valuedouble;
                if (v < -100) {
                    v = -100;
                } else if (v > 100) {
                    v = 100;
                }
                ext_bonds[i] = (short)v;
            }
            i++;
        }
    }
    item = cJSON_GetObjectItemCaseSensitive(section, "rep");
    if (cJSON_IsNumber(item)) {
        int v = (int)item->valuedouble;
        if (v < -100) {
            v = -100;
        } else if (v > 100) {
            v = 100;
        }
        ext_rep = (short)v;
    }
    obj = cJSON_GetObjectItemCaseSensitive(section, "npc_seen");
    if (cJSON_IsObject(obj)) {
        cJSON_ArrayForEach(item, obj) {
            if (item->string != NULL && cJSON_IsNumber(item)) {
                int id = atoi(item->string);
                if (id >= 0 && id < DEF_STORY_NPC_SLOTS) {
                    ext_npc_seen[id] = (short)(int)item->valuedouble;
                }
            }
        }
    }
}

static void save(cJSON *section) {
    cJSON *answers, *bonds, *seen;
    char key[16];
    int i;

    ensure_npc_init();
    answers = cJSON_AddObjectToObject(section, "answers");
    if (answers != NULL) {
        for (i = 0; i < DEF_STORY_EXT_ANSWERS; i++) {
            int v = def_story_save_answer(i);
            if (v != 0) {
                snprintf(key, sizeof(key), "%d", i);
                cJSON_AddNumberToObject(answers, key, v);
            }
        }
    }
    bonds = cJSON_AddArrayToObject(section, "bonds");
    if (bonds != NULL) {
        for (i = 0; i < DEF_STORY_BONDS; i++) {
            cJSON_AddItemToArray(bonds, cJSON_CreateNumber(ext_bonds[i]));
        }
    }
    cJSON_AddNumberToObject(section, "rep", ext_rep);
    seen = cJSON_AddObjectToObject(section, "npc_seen");
    if (seen != NULL) {
        for (i = 0; i < DEF_STORY_NPC_SLOTS; i++) {
            if (ext_npc_seen[i] >= 0) {
                snprintf(key, sizeof(key), "%d", i);
                cJSON_AddNumberToObject(seen, key, ext_npc_seen[i]);
            }
        }
    }
}

static int any(void) {
    int i;

    ensure_npc_init();
    if (ext_rep != 0) {
        return 1;
    }
    for (i = 0; i < STORY_ANSWER_BYTES; i++) {
        if (ext_answers[i] != 0) {
            return 1;
        }
    }
    for (i = 0; i < DEF_STORY_BONDS; i++) {
        if (ext_bonds[i] != 0) {
            return 1;
        }
    }
    for (i = 0; i < DEF_STORY_NPC_SLOTS; i++) {
        if (ext_npc_seen[i] >= 0) {
            return 1;
        }
    }
    return 0;
}

static void savestate(struct PortState *s) {
    PORT_STATE_VAR(s, ext_answers);
    PORT_STATE_VAR(s, ext_bonds);
    PORT_STATE_VAR(s, ext_rep);
    PORT_STATE_VAR(s, ext_npc_seen);
    PORT_STATE_VAR(s, ext_npc_init);
}

const DefExpansionSection def_story_section = { "story", reset, load, save, any, NULL, savestate };
