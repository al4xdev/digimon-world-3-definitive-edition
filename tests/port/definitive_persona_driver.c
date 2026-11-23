/* The enemy personas' host driver (tests/port/definitive_persona.py). Built twice: with the game's table
 * (definitive_persona_table.c) it checks every row and prints the table for review; with -DPERSONA_CASES it brings its
 * own rows and drives the engine through each rule (phases, the Guard, the map, discovery and the card's bit). */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "definitive.h"
#include "definitive_persona.h"

static int fails __attribute__((unused));

#define CHECK(cond, ...)                \
    do {                                \
        if (!(cond)) {                  \
            printf("FAIL: " __VA_ARGS__); \
            printf("\n");               \
            fails++;                    \
        }                               \
    } while (0)

/* ---- The stand-ins the engine links against. */
void port_log(const char *fmt, ...) {
    (void)fmt;
}
void port_state_bytes(struct PortState *s, const char *name, void *p, size_t n) {
    (void)s, (void)name, (void)p, (void)n;
}
static unsigned char kept[DEF_PERSONA_KEEP_BITS / 8];
int def_record_persona_kept(int bit) {
    return (kept[bit / 8] >> (bit % 8)) & 1;
}
void def_record_persona_keep(int bit) {
    kept[bit / 8] |= (unsigned char)(1 << (bit % 8));
}

__attribute__((unused)) static const char *kind_name(int k) {
    static const char *const n[] = { "?", "SUPER", "RARE", "DRI", "CAMPAIGN", "SPECIES" };
    return k >= 0 && k <= 5 ? n[k] : "?";
}

__attribute__((unused)) static void families(char *out, int f) {
    static const char *const n[] = { "FIRE", "WATER", "WIND", "THUNDER", "DARK" };
    int i;

    out[0] = 0;
    for (i = 0; i < 5; i++) {
        if (f & (1 << i)) {
            strcat(out, out[0] ? "+" : "");
            strcat(out, n[i]);
        }
    }
    if (!out[0]) {
        strcpy(out, "-");
    }
}

static void report(const char *what, int row) {
    printf("FAIL: row %d: %s\n", row, what);
}

#ifdef PERSONA_CASES

const DefPersona def_personas[] = {
    { SUPER(2), KEEP(0), WEAK(FIRE), GUARD_ALWAYS },                                       /* 0 */
    { RARE(0x07), KEEP(1), WEAK(WATER), RESIST(FIRE), PHASES(2), B_WEAK(WIND), HUES(40, 220) }, /* 1 */
    { DRI(3), KEEP(2), RESIST_ALL, SPEED_X(3) },                                           /* 2 */
    { CAMPAIGN(31, 448), GUARD_ALWAYS },                                                   /* 3 */
    { SPECIES(80), ON_STAGGER, GUARD_A },                                                  /* 4 */
};
const int def_persona_count = sizeof(def_personas) / sizeof(def_personas[0]);

static DefPersonaWhere at(int super, int region, int dri, int battle) {
    DefPersonaWhere w = { (short)super, (short)region, (short)dri, (short)battle };
    return w;
}

int main(void) {
    DefPersonaWhere w;
    short base[7] = { 100, 100, 100, 100, 100, 100, 100 }, out[7];
    const DefPersona *p;

    CHECK(def_persona_check(report) == 0, "the case rows break the rules");

    /* Who is who. */
    w = at(2, 0x07, -1, 5);
    CHECK(def_persona_find(&w, 99) == &def_personas[0], "a Super Rare comes before its region's Rare Target");
    w = at(-1, 0x07, -1, 5);
    CHECK(def_persona_find(&w, 99) == &def_personas[1], "the Rare Target by region");
    w = at(-1, -1, 3, 5);
    CHECK(def_persona_find(&w, 12) == &def_personas[2], "a DRI line, whatever species");
    w = at(-1, -1, -1, 31);
    CHECK(def_persona_find(&w, 448) == &def_personas[3], "a story boss by battle and species");
    CHECK(def_persona_find(&w, 447) == NULL, "the same battle, another species: none");
    w = at(-1, 0x09, -1, 31);
    CHECK(def_persona_find(&w, 448) == NULL, "a hunt that borrows the boss's battle is not the boss");
    CHECK(def_persona_find(&w, 80) == &def_personas[4], "a species row anywhere");

    /* The map: phase A, then B. */
    def_persona_resists(&def_personas[1], 0, base, out);
    CHECK(out[1] == DEF_PERSONA_WEAK && out[2] == DEF_PERSONA_WEAK && out[0] == DEF_PERSONA_RESIST &&
              out[3] == DEF_PERSONA_NEUTRAL && out[6] == DEF_PERSONA_NEUTRAL,
          "phase A: Water+Ice weak, Fire resisted, the rest neutral");
    def_persona_resists(&def_personas[1], 1, base, out);
    CHECK(out[3] == DEF_PERSONA_WEAK && out[1] == DEF_PERSONA_NEUTRAL && out[0] == DEF_PERSONA_NEUTRAL,
          "phase B: its own map, Wind weak");
    def_persona_resists(&def_personas[2], 0, base, out);
    CHECK(out[0] == DEF_PERSONA_RESIST && out[6] == DEF_PERSONA_RESIST, "RESIST_ALL");
    base[0] = 40;
    def_persona_resists(&def_personas[4], 0, base, out);
    CHECK(out[0] == 40, "a row with no map keeps the species'");
    base[0] = 100;

    /* A Rare Target: phases every 2 enemy turns, discovery per phase, the card's bit when both are solved. */
    w = at(-1, 0x07, -1, 5);
    def_persona_battle_begin(&w);
    CHECK(def_persona_battle_hue() == 40, "the model's colour before any slot is looked up");
    p = def_persona_slot(0, 99);
    CHECK(p == &def_personas[1] && def_persona_phase(0) == 0, "slot 0 is the Rare Target, phase A");
    CHECK(def_persona_enemy_turn(0) == 0 && def_persona_enemy_turn(0) == 1 && def_persona_phase(0) == 1,
          "two of its turns: phase B");
    CHECK(def_persona_battle_hue() == 220, "phase B's colour");
    def_persona_learn(0, 1 << 3);
    CHECK(def_persona_known(0) == (1 << 3) && !def_record_persona_kept(1), "phase B learnt, not solved yet");
    CHECK(def_persona_enemy_turn(0) == 0 && def_persona_enemy_turn(0) == 1 && def_persona_phase(0) == 0, "back to A");
    CHECK(def_persona_known(0) == 0, "phase A's map knows nothing yet");
    def_persona_learn(0, 1 << 1);
    CHECK(!def_record_persona_kept(1), "Water alone: Ice is its pair, found with it by the caller");
    def_persona_learn(0, (1 << 1) | (1 << 2));
    CHECK(def_record_persona_kept(1), "every weakness of both phases: KEEP(1) on the card");
    def_persona_battle_begin(&w);
    def_persona_slot(0, 99);
    CHECK(def_persona_known(0) == 0x7F, "a later fight shows its whole map");

    /* A story boss, guarded always; a common Goburimon guarded until staggered, then open until the next. */
    w = at(-1, -1, -1, 31);
    def_persona_battle_begin(&w);
    p = def_persona_slot(1, 448);
    CHECK(def_persona_guard(p, 0) && def_persona_guard(p, 1), "GUARD_ALWAYS");
    CHECK(def_persona_staggered(1) == 0, "one phase: a stagger changes nothing");
    p = def_persona_slot(2, 80);
    CHECK(def_persona_guard(p, def_persona_phase(2)), "Goburimon guarded in A");
    CHECK(def_persona_enemy_turn(2) == 0, "ON_STAGGER: turns do not count");
    CHECK(def_persona_staggered(2) == 1 && !def_persona_guard(p, def_persona_phase(2)), "staggered: open in B");
    CHECK(def_persona_battle_hue() == 0, "a species row never recolours");

    /* A DRI wave: a new species in a slot starts its state again. */
    w = at(-1, -1, 3, 5);
    def_persona_battle_begin(&w);
    def_persona_slot(0, 12);
    def_persona_learn(0, 0x7F);
    CHECK(def_record_persona_kept(2), "a map with no weakness: solved when all of it is learnt");
    p = def_persona_slot(0, 13);
    CHECK(p == &def_personas[2] && def_persona_phase(0) == 0 && p->speed_x == 3, "the next stage: the line's row");

    /* The rules catch the mistakes the owner will make. */
    {
        static const struct {
            DefPersona row;
            const char *why;
        } bad[] = {
            { { RARE(0x07), WEAK(FIRE) }, "a map without KEEP" },
            { { SUPER(2), KEEP(5), WEAK(FIRE), RESIST(FIRE) }, "weak and resisted" },
            { { SUPER(2), KEEP(5), B_WEAK(FIRE) }, "phase B without PHASES" },
            { { RARE(0x07), KEEP(5), WEAK(FIRE), PHASES(2), B_WEAK(WIND) }, "a weakness change without HUES" },
            { { SPECIES(80), HUE(30) }, "a species with a hue" },
            { { SUPER(99), KEEP(5) }, "no such hunt" },
            { { RARE(0x7E), KEEP(5) }, "no Rare Target there" },
            { { SPECIES(80), PHASES(2) }, "two phases the same" },
            { { SUPER(4), KEEP(1), RESIST(FIRE) }, "KEEP reused" },
        };
        int i;

        for (i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++) { /* each in place of row 0 of the case rows */
            DefPersona rows[sizeof(def_personas) / sizeof(def_personas[0])];

            memcpy(rows, def_personas, sizeof(rows));
            rows[0] = bad[i].row;
            CHECK(def_persona_check_rows(rows, def_persona_count, NULL) > 0, "the check missed: %s", bad[i].why);
        }
    }
    printf("persona cases: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}

#else

int main(void) {
    char wa[64], ra[64], wb[64], rb[64];
    int i, bad = def_persona_check(report);

    printf("%-3s %-9s %6s %5s %4s %-5s %-18s %-14s %-4s %-18s %-14s %-4s %s\n", "row", "who", "a", "b", "keep", "phase",
           "A weak", "A resist", "A gd", "B weak", "B resist", "B gd", "speed/hue");
    for (i = 0; i < def_persona_count; i++) {
        const DefPersona *p = &def_personas[i];
        char ph[8];

        families(wa, p->phase[0].weak);
        families(ra, p->phase[0].resist);
        families(wb, p->phase[1].weak);
        families(rb, p->phase[1].resist);
        snprintf(ph, sizeof(ph), "%s", p->phases != 2 ? "1" : p->turns ? "2/t" : "2/stg");
        printf("%-3d %-9s %6d %5d %4s %-5s %-18s %-14s %-4s %-18s %-14s %-4s x%d %d/%d\n", i, kind_name(p->kind), p->a,
               p->b, p->keep ? (char[4]){ (char)('0' + (p->keep - 1) / 10 % 10), (char)('0' + (p->keep - 1) % 10), 0 } : "-",
               ph, wa, ra, p->phase[0].guard ? "yes" : "", wb, rb, p->phase[1].guard ? "yes" : "", p->speed_x,
               p->phase[0].hue, p->phase[1].hue);
    }
    printf("persona table: %d rows, %s\n", def_persona_count, bad ? "FAIL" : "ok");
    return bad != 0;
}

#endif
