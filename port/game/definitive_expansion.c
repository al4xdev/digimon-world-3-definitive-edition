/* The expansion save's core (definitive_expansion.h): the pairing with the card, the file and its sections. Nothing
 * here knows what a section holds; definitive_expansion_sections.c lists them. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "psxstack/platform.h"
#include "psxstack/savestate.h"
#include "psxstack/settings.h"

#include "../third_party/cjson/cJSON.h"
#include "definitive_expansion.h"

extern void port_log(const char *fmt, ...);

#define MAX_BYTES (4 << 20) /* far past what a few slots of sections print to: a bigger file is not ours */

static const unsigned char card_magic[4] = { 'E', 'X', 'P', '1' };
typedef char card_bytes_fit[8 <= DEF_EXPANSION_CARD_BYTES ? 1 : -1];

static unsigned int gen;       /* the slot's generation: the card's (and the file's entry, when they agree); 0 none */
static unsigned char migrated; /* the card's bytes were a section's old layout: taken, the file not read */
/* Those old bytes, written back as they were until a save has put what they held in the file (a failed file write must
 * not lose them from the card too). */
static unsigned char card_old[DEF_EXPANSION_CARD_BYTES];
/* A card with no file beside it (in memory: no --config, or memcard null) keeps its JSON here, so that a slot saved
 * and loaded again in the same run still pairs. */
static char *memory_text[2];

#define FOR_SECTIONS(s) for (const DefExpansionSection *const *s = def_expansion_sections; *s != NULL; s++)

static void sections_reset(void) {
    FOR_SECTIONS(s) {
        (*s)->reset();
    }
}

static int sections_any(void) {
    FOR_SECTIONS(s) {
        if ((*s)->any()) {
            return 1;
        }
    }
    return 0;
}

void def_expansion_reset(void) {
    sections_reset();
    gen = 0;
    migrated = 0;
    memset(card_old, 0, sizeof(card_old));
}

unsigned int def_expansion_gen(void) {
    return gen;
}

int def_expansion_keeps(void) {
    return gen != 0 || migrated || sections_any();
}

/* Only the generation: what the sections keep is the file's. No generation: all zeros. */
void def_expansion_card_write(unsigned char *out) {
    int i;

    memset(out, 0, DEF_EXPANSION_CARD_BYTES);
    if (gen == 0) {
        if (migrated) {
            memcpy(out, card_old, DEF_EXPANSION_CARD_BYTES);
        }
        return;
    }
    memcpy(out, card_magic, sizeof(card_magic));
    for (i = 0; i < 4; i++) {
        out[4 + i] = (unsigned char)(gen >> (8 * i));
    }
}

void def_expansion_card_read(const unsigned char *in) {
    int i, n = 0;

    def_expansion_reset();
    if (in == NULL) {
        return;
    }
    if (memcmp(in, card_magic, sizeof(card_magic)) == 0) {
        for (i = 0; i < 4; i++) {
            gen |= (unsigned int)in[4 + i] << (8 * i);
        }
        return;
    }
    /* No magic: zeros, or bytes a section kept on the card before the expansion (its own layout, its to tell). */
    FOR_SECTIONS(s) {
        int got = (*s)->migrate_from_card != NULL ? (*s)->migrate_from_card(in, DEF_EXPANSION_CARD_BYTES) : 0;

        if (got > 0) {
            port_log("expansion: %d from the card's old bytes into \"%s\" (the next save writes them to the file)", got,
                     (*s)->name);
            n += got;
        }
    }
    migrated = n > 0;
    if (migrated) {
        memcpy(card_old, in, DEF_EXPANSION_CARD_BYTES);
    }
}

void def_expansion_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, gen);
    PORT_STATE_VAR(s, migrated);
    PORT_STATE_VAR(s, card_old);
    FOR_SECTIONS(x) {
        (*x)->savestate(s);
    }
}

/* ---- The file. */

int def_expansion_path(int port, char *out, unsigned long size) {
    const char *card;
    size_t n;

    if (port < 0 || port > 1 || !port_settings_loaded || (card = port_settings.memcard[port]) == NULL) {
        return 0;
    }
    n = strlen(card);
    if (n >= 4 && strcmp(card + n - 4, ".mcd") == 0) {
        n -= 4;
    }
    return snprintf(out, size, "%.*s.def.json", (int)n, card) < (int)size;
}

/* The whole file, NUL-ended; NULL when it does not exist (*missing) or cannot be read. */
static char *read_file(const char *path, int *missing) {
    FILE *f = fopen(path, "rb");
    char *text;
    long n;

    *missing = f == NULL && errno == ENOENT;
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || n > MAX_BYTES || fseek(f, 0, SEEK_SET) != 0 ||
        (text = malloc((size_t)n + 1)) == NULL) {
        fclose(f);
        return NULL;
    }
    if (fread(text, 1, (size_t)n, f) != (size_t)n) {
        free(text);
        fclose(f);
        return NULL;
    }
    text[n] = 0;
    fclose(f);
    return text;
}

/* Through a temporary file flushed to the disk, renamed over the old one (port_file_replace: Windows's rename refuses
 * an existing target), as the card's own image: a crash leaves the old file or the new one; a failure, the old one. */
int def_expansion_write_atomic(const char *path, const char *text) {
    size_t n = strlen(text), plen = strlen(path);
    char *tmp = malloc(plen + 5);
    FILE *f;
    int ok;

    if (tmp == NULL) {
        return 0;
    }
    memcpy(tmp, path, plen);
    memcpy(tmp + plen, ".tmp", 5);
    if ((f = fopen(tmp, "wb")) == NULL) {
        port_log("expansion: cannot write %s: %s", tmp, strerror(errno));
        free(tmp);
        return 0;
    }
    ok = fwrite(text, 1, n, f) == n && fflush(f) == 0;
#ifdef _WIN32
    ok = ok && _commit(_fileno(f)) == 0;
#else
    ok = ok && fsync(fileno(f)) == 0;
#endif
    ok = fclose(f) == 0 && ok;
    if (ok && port_file_replace(tmp, path) != 0) {
        port_log("expansion: cannot rename %s to %s: %s", tmp, path, strerror(errno));
        ok = 0;
    }
    if (!ok) {
        remove(tmp); /* our own half-written temporary; the old file is whole */
    }
    free(tmp);
    return ok;
}

static int newer_version(const cJSON *root) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "version");

    return cJSON_IsNumber(v) && v->valuedouble > DEF_EXPANSION_VERSION;
}

static cJSON *slot_entry(const cJSON *root, int slot) {
    cJSON *slots = cJSON_GetObjectItemCaseSensitive(root, "slots");
    cJSON *entry = cJSON_IsArray(slots) ? cJSON_GetArrayItem(slots, slot) : NULL;

    return cJSON_IsObject(entry) ? entry : NULL;
}

static unsigned int entry_gen(const cJSON *entry) {
    const cJSON *g = cJSON_GetObjectItemCaseSensitive(entry, "gen");

    return cJSON_IsNumber(g) && g->valuedouble >= 0 && g->valuedouble <= 4294967295.0 ? (unsigned int)g->valuedouble
                                                                                     : 0;
}

/* The slot's sections from `text` (NULL: no file) when its entry's gen is the card's. */
static int load_text(const char *text, const char *where, int slot) {
    cJSON *root = text != NULL ? cJSON_Parse(text) : NULL;
    cJSON *entry = root != NULL && !newer_version(root) ? slot_entry(root, slot) : NULL;
    unsigned int have = entry_gen(entry);

    if (entry == NULL || have != gen) {
        port_log("expansion: slot %d: %s: %s, the card's is %u: the slot's expansion reads as empty", slot + 1, where,
                 text == NULL           ? "no file"
                 : root == NULL         ? "not JSON"
                 : newer_version(root)  ? "a newer version's file"
                 : entry == NULL        ? "no entry for the slot"
                                        : "another generation",
                 gen);
        cJSON_Delete(root);
        return 0;
    }
    FOR_SECTIONS(s) {
        (*s)->reset();
        (*s)->load(cJSON_GetObjectItemCaseSensitive(entry, (*s)->name));
    }
    port_log("expansion: slot %d: %s, generation %u", slot + 1, where, gen);
    cJSON_Delete(root);
    return 1;
}

int def_expansion_load_file(const char *path, int slot) {
    int missing, ok;
    char *text;

    if (migrated || gen == 0) {
        return migrated; /* nothing on the card points at the file */
    }
    text = read_file(path, &missing);
    if (text == NULL && !missing) {
        port_log("expansion: slot %d: %s cannot be read: the slot's expansion reads as empty", slot + 1, path);
        return 0;
    }
    ok = load_text(text, path, slot);
    free(text);
    return ok;
}

static void put(cJSON *o, const char *key, cJSON *item) {
    /* Case-sensitive on both sides: a hand-edited "Gen" is another key (HasObjectItem would match it, the replace
     * would not, and the item would leak). */
    if (cJSON_GetObjectItemCaseSensitive(o, key) != NULL) {
        cJSON_ReplaceItemInObjectCaseSensitive(o, key, item);
    } else {
        cJSON_AddItemToObject(o, key, item);
    }
}

/* `old` (NULL: none, or not JSON: a new tree) with the slot's entry taking the sections in memory at generation `g`.
 * The rest of the tree, the entry's own unknown keys and sections included, goes back as it came. *out NULL: nothing to
 * write (nothing kept, no generation, and the entry holds none of ours); returns 0 when `old` is a newer version's. */
static int save_text(const char *old, int slot, char **out, unsigned int *out_gen) {
    cJSON *root = old != NULL ? cJSON_Parse(old) : NULL, *slots, *entry;
    unsigned int g;
    int keep = def_expansion_keeps();

    *out = NULL;
    if (root != NULL && newer_version(root)) {
        cJSON_Delete(root);
        return 0;
    }
    entry = slot_entry(root, slot);
    if (!keep) {
        int ours = 0;

        FOR_SECTIONS(s) {
            const cJSON *o = cJSON_GetObjectItemCaseSensitive(entry, (*s)->name);

            ours |= o != NULL && o->child != NULL;
        }
        if (!ours) {
            cJSON_Delete(root);
            return 1;
        }
        g = entry_gen(entry); /* emptied, its gen kept as the mark the next save goes past */
    } else {
        unsigned int f = entry_gen(entry);

        /* Past the card's and the file's: a new game saved over a slot, or an older card restored, never meets an
         * old entry with its number. */
        g = (f > gen ? f : gen) + 1;
        g = g != 0 ? g : 1;
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        root = cJSON_CreateObject();
    }
    put(root, "version", cJSON_CreateNumber(DEF_EXPANSION_VERSION));
    slots = cJSON_GetObjectItemCaseSensitive(root, "slots");
    if (!cJSON_IsArray(slots)) {
        put(root, "slots", slots = cJSON_CreateArray());
    }
    while (cJSON_GetArraySize(slots) <= slot) { /* the slots before it, empty */
        entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "gen", 0);
        cJSON_AddItemToArray(slots, entry);
    }
    entry = cJSON_GetArrayItem(slots, slot);
    if (!cJSON_IsObject(entry)) {
        cJSON_ReplaceItemInArray(slots, slot, entry = cJSON_CreateObject());
    }
    put(entry, "gen", cJSON_CreateNumber(g));
    FOR_SECTIONS(s) {
        cJSON *o = cJSON_CreateObject();

        (*s)->save(o);
        put(entry, (*s)->name, o);
    }
    *out = cJSON_Print(root);
    *out_gen = keep ? g : 0;
    cJSON_Delete(root);
    return *out != NULL;
}

int def_expansion_save_file(const char *path, int slot) {
    int missing;
    char *old = read_file(path, &missing), *text;
    unsigned int g = 0;
    cJSON *probe;

    if (old == NULL && !missing) {
        port_log("expansion: slot %d: %s cannot be read: not overwritten, the card keeps generation %u", slot + 1, path,
                 gen);
        return 0;
    }
    probe = old != NULL ? cJSON_Parse(old) : NULL;
    if (old != NULL && !cJSON_IsObject(probe)) { /* not our JSON (or none at all): kept beside it, never lost */
        char *bad = malloc(strlen(path) + 5);
        int moved = bad != NULL;

        cJSON_Delete(probe);
        if (moved) {
            sprintf(bad, "%s.bad", path);
            moved = port_file_replace(path, bad) == 0;
            port_log("expansion: %s is not a JSON object: %s %s", path, moved ? "moved to" : "could not be moved to",
                     bad);
            free(bad);
        }
        free(old);
        if (!moved) { /* never written over: the card keeps its generation, the slot's expansion reads as empty */
            return 0;
        }
        old = NULL;
    } else if (old != NULL) {
        cJSON_Delete(probe);
    }
    if (!save_text(old, slot, &text, &g)) {
        free(old);
        port_log("expansion: %s is a newer version's: not overwritten, the card keeps generation %u", path, gen);
        return 0;
    }
    free(old);
    if (text == NULL) {
        return 1;
    }
    if (!def_expansion_write_atomic(path, text)) {
        cJSON_free(text);
        port_log("expansion: slot %d: %s not written: the card keeps generation %u", slot + 1, path, gen);
        return 0;
    }
    cJSON_free(text);
    gen = g;
    migrated = 0;
    port_log("expansion: slot %d: %s written, generation %u", slot + 1, path, g);
    return 1;
}

void def_expansion_load(int port, int slot) {
    char path[4096];

    if (port < 0 || port > 1) {
        return;
    }
    if (def_expansion_path(port, path, sizeof(path))) {
        def_expansion_load_file(path, slot);
    } else if (!migrated && gen != 0) {
        load_text(memory_text[port], "the card's expansion in memory", slot);
    }
}

void def_expansion_save(int port, int slot) {
    char path[4096], *text;
    unsigned int g = 0;

    if (port < 0 || port > 1) {
        return;
    }
    if (def_expansion_path(port, path, sizeof(path))) {
        def_expansion_save_file(path, slot);
    } else if (save_text(memory_text[port], slot, &text, &g) && text != NULL) {
        cJSON_free(memory_text[port]);
        memory_text[port] = text;
        gen = g;
        migrated = 0;
        port_log("expansion: slot %d: card %d has no file beside it: generation %u kept in memory", slot + 1, port + 1,
                 g);
    }
}
