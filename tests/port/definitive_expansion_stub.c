/* The expansion save's card hooks (definitive_expansion.h) for the host tests that build definitive_bestiary.c without
 * it: no generation, nothing kept, the card's bytes all zeros (tests/port/definitive_expansion.py covers the real ones,
 * with the record). */
#include <string.h>

#include "definitive_expansion.h"

void def_expansion_reset(void) {
}
void def_expansion_card_write(unsigned char *out) {
    memset(out, 0, DEF_EXPANSION_CARD_BYTES);
}
void def_expansion_card_read(const unsigned char *in) {
    (void)in;
}
unsigned int def_expansion_gen(void) {
    return 0;
}
int def_expansion_keeps(void) {
    return 0;
}
void def_expansion_savestate(struct PortState *s) {
    (void)s;
}
