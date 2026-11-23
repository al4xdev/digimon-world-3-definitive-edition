/* tests/port/definitive_expansion.py's driver: the expansion save's core (port/game/definitive_expansion.c) with its
 * real sections (the personas) and the bestiary's first record, whose last bytes carry the generation. The file is a
 * real one in the directory given as argv[1]. Prints "ok name" or "FAIL name" per case; exits 1 on a failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "psxstack/settings.h"

#include "../../port/third_party/cjson/cJSON.h"
#include "definitive.h"
#include "definitive_expansion.h"
#include "definitive_persona.h"

/* ---- The stand-ins. */
int port_script_active;
PortSettings port_settings;
int port_settings_loaded;
void port_log(const char *fmt, ...) {
    (void)fmt;
}
int fightstg_enemy_find_record(int id) {
    return id >= 1000 && id < 1000 + 200 ? id - 1000 : -1;
}
int port_file_replace(const char *tmp, const char *path) {
    return rename(tmp, path);
}
/* A savestate: saving appends the variables, loading takes them back in the same order. */
static unsigned char state[8192];
static size_t state_at;
static int state_loading;
void port_state_bytes(struct PortState *s, const char *tag, void *data, size_t size) {
    (void)s, (void)tag;
    if (state_at + size > sizeof(state)) {
        return;
    }
    if (state_loading) {
        memcpy(data, state + state_at, size);
    } else {
        memcpy(state + state_at, data, size);
    }
    state_at += size;
}

static int failures;

static void check(const char *name, int ok) {
    failures += !ok;
    printf("%s %s\n", ok ? "ok" : "FAIL", name);
}

#define REC_EXPANSION (0xD0 + 30) /* definitive_bestiary.c */
static unsigned char rec[3][DEF_BESTIARY_RECORD_SIZE];
static char path[1024];

static char *slurp(const char *p) {
    FILE *f = fopen(p, "rb");
    static char buf[1 << 16];
    size_t n;

    if (f == NULL) {
        return NULL;
    }
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

static void spit(const char *p, const char *text) {
    FILE *f = fopen(p, "wb");

    fputs(text, f);
    fclose(f);
}

static int exists(const char *p) {
    struct stat st;

    return stat(p, &st) == 0;
}

/* The stgmcard steps: save (file, then the record), load (the record, then the file). */
static int save(int slot) {
    int ok = def_expansion_save_file(path, slot);

    def_bestiary_record_write(rec[slot]);
    return ok;
}

static int load(int slot) {
    def_bestiary_record_read(rec[slot]);
    return def_expansion_load_file(path, slot);
}

static unsigned int card_gen(int slot) {
    const unsigned char *b = rec[slot] + REC_EXPANSION;

    return memcmp(b, "EXP1", 4) != 0 ? 0 : (unsigned)b[4] | (unsigned)b[5] << 8 | (unsigned)b[6] << 16 | (unsigned)b[7] << 24;
}

static const cJSON *slot_json(cJSON *root, int slot) {
    return cJSON_GetArrayItem(cJSON_GetObjectItem(root, "slots"), slot);
}

static int all_zero(const unsigned char *p, int n) {
    while (n-- > 0) {
        if (*p++) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    char other[1100], tmpdir[1100];
    cJSON *root;
    const cJSON *e, *p;
    unsigned int g;

    if (argc < 2) {
        return 2;
    }
    snprintf(path, sizeof(path), "%s/card1.def.json", argv[1]);

    /* Nothing learnt: no file, the record as it was before the expansion. */
    def_bestiary_reset();
    check("nothing learnt: saved, no file written", save(0) == 1 && !exists(path));
    check("nothing learnt: the record stays all zeros", all_zero(rec[0], DEF_BESTIARY_RECORD_SIZE));

    /* The round trip. */
    def_persona_record_learn(3, 0, 0x05);
    def_persona_record_learn(3, 1, 0x88); /* bit 7 is not an element */
    def_persona_record_solve(7);
    check("save: written", save(0) == 1 && exists(path));
    check("save: the card's record carries the magic and generation 1", card_gen(0) == 1 && def_expansion_gen() == 1);
    root = cJSON_Parse(slurp(path));
    e = slot_json(root, 0);
    p = cJSON_GetObjectItem(cJSON_GetObjectItem(e, "personas"), "3");
    check("file: version 1, the slot's gen, a persona by its KEEP id with known [a, b] and solved",
          cJSON_GetObjectItem(root, "version")->valueint == 1 && cJSON_GetObjectItem(e, "gen")->valueint == 1 &&
              cJSON_GetArrayItem(cJSON_GetObjectItem(p, "known"), 0)->valueint == 5 &&
              cJSON_GetArrayItem(cJSON_GetObjectItem(p, "known"), 1)->valueint == 8 &&
              cJSON_IsFalse(cJSON_GetObjectItem(p, "solved")) &&
              cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(e, "personas"), "7"), "solved")));
    check("file: only the ids with anything learnt", cJSON_GetArraySize(cJSON_GetObjectItem(e, "personas")) == 2);
    cJSON_Delete(root);
    def_bestiary_reset();
    check("a new game: nothing known", def_persona_record_known(3, 0) == 0 && !def_persona_record_solved(7));
    check("load: taken", load(0) == 1);
    check("load: the personas back", def_persona_record_known(3, 0) == 5 && def_persona_record_known(3, 1) == 8 &&
                                          def_persona_record_solved(7) && !def_persona_record_solved(3));

    /* Each save a new generation; the slots apart. */
    def_persona_record_learn(4, 0, 1);
    save(0);
    check("the next save: generation 2", card_gen(0) == 2);
    def_bestiary_reset();
    def_persona_record_solve(9);
    save(1);
    check("slot 2 saved: its own generation (past nothing: 1)", card_gen(1) == 1);
    load(0);
    check("slot 1 still its own", def_persona_record_known(4, 0) == 1 && !def_persona_record_solved(9));
    load(1);
    check("slot 2 its own", def_persona_record_solved(9) && def_persona_record_known(4, 0) == 0);

    /* The generation does not match: the slot reads as empty, the game goes on. */
    memcpy(rec[2], rec[0], sizeof(rec[2]));
    load(0);
    save(0); /* the file at 3, rec[2] still at 2: a card restored from before */
    check("a card older than the file: not taken", load(2) == 0);
    check("a card older than the file: empty", def_persona_record_known(3, 0) == 0 && !def_persona_record_solved(7) &&
                                                   def_expansion_gen() == 2);
    check("its next save goes past both (4), and takes the slot", def_expansion_save_file(path, 0) == 1 &&
                                                                      def_expansion_gen() == 4);
    rec[0][REC_EXPANSION + 4] = 77; /* a card whose generation the file never had */
    check("a card with another generation: not taken", load(0) == 0 && def_persona_record_known(3, 0) == 0);
    remove(path);
    rec[0][REC_EXPANSION + 4] = 4;
    check("no file: not taken, empty", load(0) == 0 && !def_persona_record_solved(9));

    /* Unknown keys: ignored on reading, kept on writing (a newer build's sections too). */
    spit(path, "{\"version\": 1, \"future\": {\"x\": 1}, \"slots\": [{\"gen\": 4, \"medals\": [1, 2],"
               " \"personas\": {\"3\": {\"known\": [5], \"seen\": 9}, \"abc\": {\"solved\": true},"
               " \"999\": {\"solved\": true}, \"-1\": {\"solved\": true}, \"12\": 7, \"6\": {\"solved\": true}}}]}");
    check("unknown keys: the slot taken", load(0) == 1);
    check("unknown keys ignored, a missing known[1] and solved read as 0",
          def_persona_record_known(3, 0) == 5 && def_persona_record_known(3, 1) == 0 && !def_persona_record_solved(3) &&
              def_persona_record_solved(6) && !def_persona_record_solved(12));
    save(0);
    root = cJSON_Parse(slurp(path));
    e = slot_json(root, 0);
    check("unknown keys kept on writing (the file's and the slot's)",
          cJSON_GetObjectItem(cJSON_GetObjectItem(root, "future"), "x") != NULL &&
              cJSON_GetArraySize(cJSON_GetObjectItem(e, "medals")) == 2 && cJSON_GetObjectItem(e, "gen")->valueint == 5);
    check("our own section written whole again (its bad ids gone)",
          cJSON_GetObjectItem(cJSON_GetObjectItem(e, "personas"), "abc") == NULL &&
              cJSON_GetObjectItem(cJSON_GetObjectItem(e, "personas"), "6") != NULL);
    cJSON_Delete(root);

    /* A new game saved over a slot with personas: the entry emptied, its gen kept as the mark, the record zeros. */
    def_bestiary_reset();
    check("a new game over the slot: saved", save(0) == 1 && card_gen(0) == 0 &&
                                                 all_zero(rec[0], DEF_BESTIARY_RECORD_SIZE));
    root = cJSON_Parse(slurp(path));
    e = slot_json(root, 0);
    check("its entry emptied, gen 5 kept", cJSON_GetObjectItem(e, "gen")->valueint == 5 &&
                                               cJSON_GetObjectItem(e, "personas")->child == NULL);
    cJSON_Delete(root);
    def_persona_record_solve(1);
    save(0);
    check("its next save with something: past the mark (6)", card_gen(0) == 6);

    /* The card's old bits (before the expansion): imported as solved, written to the file by the next save. */
    def_bestiary_reset();
    def_save_flag_set(DEF_SAVE_HARD, 1); /* so the record has its "X1" part */
    def_bestiary_record_write(rec[2]);
    rec[2][REC_EXPANSION] = 0x01;     /* KEEP 0 */
    rec[2][REC_EXPANSION + 1] = 0x02; /* KEEP 9 */
    rec[2][REC_EXPANSION + 17] = 0x80; /* KEEP 143, the last old bit */
    check("old bits: the file not read", load(2) == 1);
    check("old bits: solved, the whole map", def_persona_record_solved(0) && def_persona_record_solved(9) &&
                                                 def_persona_record_solved(143) && !def_persona_record_solved(1) &&
                                                 def_persona_record_known(9, 1) == 0x7F);
    def_bestiary_record_write(rec[1]);
    check("old bits: kept on the card until the file has them",
          rec[1][REC_EXPANSION] == 0x01 && rec[1][REC_EXPANSION + 1] == 0x02 && card_gen(1) == 0);
    check("old bits: the next save writes them, the card gets a generation", save(2) == 1 && card_gen(2) >= 1);
    def_bestiary_reset();
    check("old bits: loaded from the file now", load(2) == 1 && def_persona_record_solved(143));

    /* The atomic write: a temporary that cannot be written leaves the old file, and the card its old generation. */
    {
        char before[1 << 16];

        load(0);
        strcpy(before, slurp(path));
        g = def_expansion_gen();
        snprintf(tmpdir, sizeof(tmpdir), "%s.tmp", path);
        mkdir(tmpdir, 0755);
        def_persona_record_learn(20, 0, 1);
        check("a failed write: not written", save(0) == 0);
        check("a failed write: the old file whole", strcmp(slurp(path), before) == 0);
        check("a failed write: the card keeps its generation", card_gen(0) == g && def_expansion_gen() == g);
        rmdir(tmpdir);
        def_bestiary_reset();
        check("a failed write: the old entry still pairs", load(0) == 1 && def_persona_record_solved(1) &&
                                                               def_persona_record_known(20, 0) == 0);
        check("write_atomic: replaces", def_expansion_write_atomic(path, "{}") == 1 && strcmp(slurp(path), "{}") == 0);
        spit(path, before);
    }

    /* A newer version's file: read as empty, never overwritten. A file that is not JSON: moved aside. */
    {
        const char *newer = "{\"version\": 2, \"slots\": [{\"gen\": 6, \"personas\": {\"1\": {\"solved\": true}}}]}";

        spit(path, newer);
        check("a newer version: not taken", load(0) == 0 && !def_persona_record_solved(1));
        def_persona_record_solve(2);
        check("a newer version: not overwritten", save(0) == 0 && strcmp(slurp(path), newer) == 0);
        spit(path, "{not json");
        snprintf(other, sizeof(other), "%s.bad", path);
        check("not JSON: written anew", save(0) == 1 && exists(path) && cJSON_Parse(slurp(path)) != NULL);
        check("not JSON: the old one moved beside it", exists(other) && strcmp(slurp(other), "{not json") == 0);
        spit(path, "[1, 2]");
        check("JSON but not an object: moved aside too", save(0) == 1 && strcmp(slurp(other), "[1, 2]") == 0 &&
                                                              cJSON_IsObject(cJSON_Parse(slurp(path))));
        spit(path, "{\"Version\": 1, \"slots\": []}");
        check("a hand-edited key case is another key", save(0) == 1 && strstr(slurp(path), "\"Version\"") != NULL &&
                                                          strstr(slurp(path), "\"version\"") != NULL);
    }

    /* A savestate carries the expansion in memory. */
    def_bestiary_reset();
    load(0);
    def_persona_record_learn(30, 1, 0x10);
    g = def_expansion_gen();
    state_at = 0;
    state_loading = 0;
    def_bestiary_savestate(NULL);
    def_bestiary_reset();
    state_at = 0;
    state_loading = 1;
    def_bestiary_savestate(NULL);
    check("savestate: the personas and the generation back",
          def_persona_record_known(30, 1) == 0x10 && def_persona_record_solved(2) && def_expansion_gen() == g && g != 0);

    /* The path beside the card; without one (no --config), the card's JSON is kept in memory for the run. */
    port_settings_loaded = 1;
    port_settings.memcard[0] = "/saves/card1.mcd";
    port_settings.memcard[1] = "/saves/other";
    check("path: card1.mcd -> card1.def.json", def_expansion_path(0, other, sizeof(other)) &&
                                                   strcmp(other, "/saves/card1.def.json") == 0);
    check("path: no .mcd, appended", def_expansion_path(1, other, sizeof(other)) &&
                                         strcmp(other, "/saves/other.def.json") == 0);
    port_settings_loaded = 0;
    check("path: none without the settings", !def_expansion_path(0, other, sizeof(other)));
    def_bestiary_reset();
    def_persona_record_solve(42);
    def_expansion_save(1, 0);
    def_bestiary_record_write(rec[0]);
    def_bestiary_reset();
    def_bestiary_record_read(rec[0]);
    def_expansion_load(1, 0);
    check("in memory: a slot saved and loaded again pairs", def_persona_record_solved(42) && card_gen(0) == 1);

    printf("definitive_expansion: %d failure(s)\n", failures);
    return failures != 0;
}
