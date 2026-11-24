/* The Velvet Room pig's stat swaps, kept per slot (definitive_respec_screen.c): the expansion save's "respec" section
 * (definitive_expansion.h), the number of swaps paid so far. The price climbs with it (DEF_RESPEC_PRICES):
 *
 *   "respec": {"swaps": 3}
 */
#include "psxstack/savestate.h"

#include "../third_party/cjson/cJSON.h"
#include "definitive.h"
#include "definitive_expansion.h"

static int swaps;

int def_respec_swaps(void) {
    return swaps;
}

void def_respec_swaps_add(void) {
    if (swaps < 255) {
        swaps++;
    }
}

/* The next swap's price: the table's step for the swaps paid, then the last one for good. */
int def_respec_price(void) {
    static const int prices[DEF_RESPEC_STEPS] = DEF_RESPEC_PRICES;

    return prices[swaps < DEF_RESPEC_STEPS ? swaps : DEF_RESPEC_STEPS - 1];
}

static void reset(void) {
    swaps = 0;
}

static void load(const cJSON *section) {
    const cJSON *v = cJSON_IsObject(section) ? cJSON_GetObjectItemCaseSensitive(section, "swaps") : NULL;

    swaps = cJSON_IsNumber(v) && v->valuedouble > 0 && v->valuedouble < 256 ? (int)v->valuedouble : 0;
}

static void save(cJSON *section) {
    cJSON_AddNumberToObject(section, "swaps", swaps);
}

static int any(void) {
    return swaps != 0;
}

static void savestate(struct PortState *s) {
    PORT_STATE_VAR(s, swaps);
}

const DefExpansionSection def_respec_section = { "respec", reset, load, save, any, NULL, savestate };
