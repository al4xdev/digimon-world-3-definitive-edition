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
int def_super_tutorial_battle(void) {
    return 0;
}
const unsigned char *def_super_talk_text(void) {
    return NULL;
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

void def_finale_reset(void) {
}
void def_purify_reset(void) {
}
void def_dri_reset(void) {
}
void dw3_velvet_reset(void) {
}
void def_time_reset(void) {
}

#include "definitive_expansion.h"
/* The extended bestiary's hooks (definitive_bestiary_ext.c): nothing recorded. */
void def_bestiary_ext_record_enemy(int species_id, int element_mask) {
    (void)species_id;
    (void)element_mask;
}
static void bext_reset(void) {}
static void bext_load(const struct cJSON *s) { (void)s; }
static void bext_save(struct cJSON *s) { (void)s; }
static int bext_any(void) { return 0; }
static void bext_savestate(struct PortState *s) { (void)s; }
const DefExpansionSection def_bestiary_ext_section = { "bestiary_ext", bext_reset, bext_load, bext_save, bext_any, NULL, bext_savestate };
