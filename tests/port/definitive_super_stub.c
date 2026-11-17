/* The Super Rare hunts' record hooks (definitive_superhunt.c) for the host tests that build definitive_bestiary.c
 * without them: no hunt, nothing to keep (tests/port/definitive_superhunt.py covers the real ones). */
#include <string.h>

#include "definitive.h"

void def_super_reset(void) {
}
int def_super_record_empty(void) {
    return 1;
}
void def_super_record_write(unsigned char *out) {
    memset(out, 0, DEF_SUPER_RECORD);
}
void def_super_record_read(const unsigned char *in) {
    (void)in;
}
void def_super_savestate(struct PortState *s) {
    (void)s;
}
int def_super_battle_ready(void) {
    return 0;
}
void def_super_battle_end(int result, int hp) {
    (void)result;
    (void)hp;
}

/* The skins' record hooks (definitive_skins.c; tests/port/definitive_skins.py covers the real ones): none kept. */
void def_skin_reset(void) {
}
int def_skin_record_empty(void) {
    return 1;
}
void def_skin_record_write(unsigned char *out) {
    memset(out, 0, DEF_SKIN_RECORD);
}
void def_skin_record_read(const unsigned char *in) {
    (void)in;
}
