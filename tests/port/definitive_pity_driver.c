/* tests/port/definitive_pity.py's driver for port/game/definitive_progression.c (training pity). */
#include <stdio.h>

#include "definitive.h"

static int failures;

static void check(const char *name, int got, int want) {
    if (got != want) {
        failures++;
        printf("FAIL %s: got %d want %d\n", name, got, want);
    } else {
        printf("ok %s\n", name);
    }
}

int main(void) {
    def_train_pity_reset();
    check("initial count", def_train_pity_count(), 0);
    check("initial guaranteed", def_train_pity_guaranteed(), 0);

    def_train_pity_record(0);
    check("after 1 miss: count", def_train_pity_count(), 1);
    check("after 1 miss: guaranteed", def_train_pity_guaranteed(), 0);

    def_train_pity_record(0);
    check("after 2 misses: count", def_train_pity_count(), 2);
    check("after 2 misses: guaranteed", def_train_pity_guaranteed(), 0);

    /* Natural perfect resets the counter mid-way. */
    def_train_pity_record(1);
    check("after natural perfect: count", def_train_pity_count(), 0);
    check("after natural perfect: guaranteed", def_train_pity_guaranteed(), 0);

    /* Three misses reach the threshold and arm the guarantee. */
    def_train_pity_record(0);
    def_train_pity_record(0);
    def_train_pity_record(0);
    check("after 3 misses: count", def_train_pity_count(), 3);
    check("after 3 misses: guaranteed", def_train_pity_guaranteed(), 1);

    /* Guaranteed run ends (even if recorded as 0, safety guard) and resets counter to 0. */
    def_train_pity_record(1);
    check("after guaranteed finish: count", def_train_pity_count(), 0);
    check("after guaranteed finish: guaranteed", def_train_pity_guaranteed(), 0);

    /* Check DEF_TRAIN_PITY_AFTER clamping/zero-guard invariant via reset -> miss. */
    def_train_pity_record(0);
    def_train_pity_reset();
    check("after reset: count", def_train_pity_count(), 0);

    /* Per-tier isolation: 1 TP misses (tier 0) must not arm 5 TP (tier 1) or 10 TP (tier 2). */
    def_train_pity_record_tier(0, 0);
    def_train_pity_record_tier(0, 0);
    def_train_pity_record_tier(0, 0);
    check("tier 0 guaranteed after 3 misses", def_train_pity_guaranteed_tier(0), 1);
    check("tier 1 unaffected by tier 0", def_train_pity_count_tier(1), 0);
    check("tier 1 not guaranteed", def_train_pity_guaranteed_tier(1), 0);
    check("tier 2 unaffected by tier 0", def_train_pity_count_tier(2), 0);
    check("tier 2 not guaranteed", def_train_pity_guaranteed_tier(2), 0);
    def_train_pity_record_tier(2, 0);
    check("tier 2 count independent", def_train_pity_count_tier(2), 1);
    def_train_pity_reset();

    return failures ? 1 : 0;
}
