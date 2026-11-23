/* The enemy personas, written by hand (definitive_persona.h has the helpers and the rules; the host test
 * tests/port/definitive_persona.py checks every row and prints the table for review). One row per special enemy;
 * what a row does not say comes from its species. KEEP bits are the card's: never reuse or renumber one. */
#include "definitive.h"
#include "definitive_persona.h"

const DefPersona def_personas[] = {
    /* The story bosses that were guarded before the personas (definitive.c's old list): their own map, guarded. */
    { CAMPAIGN(313, 141), GUARD_ALWAYS }, /* Datamon, Lv34 */
    { CAMPAIGN(15, 444), GUARD_ALWAYS },  /* BK Imperialdramon, Lv46 */
    { CAMPAIGN(31, 448), GUARD_ALWAYS },  /* BK WarGreymon, Lv53 */
};
const int def_persona_count = sizeof(def_personas) / sizeof(def_personas[0]);
