/* tests/port/definitive_skins.py's driver for port/game/definitive_skins.c (the hero's skins). The game's state it
 * reads (def_on, def_super_done) are stand-ins. Prints "ok name" / "FAIL name" per case; exits 1 on a failure. */
#include <stdio.h>
#include <string.h>

#include "common.h"

#include "definitive.h"

static int on = 1, done;
int def_on(void) {
    return on;
}
int def_super_done(void) {
    return done;
}

static int failures;
static void check(const char *name, int ok) {
    if (!ok) {
        failures++;
    }
    printf("%s %s\n", ok ? "ok" : "FAIL", name);
}

#define RGB(r, g, b) ((unsigned short)((r) | (g) << 5 | (b) << 10))

int main(void) {
    /* Kaili's roles (girls_power.c's gp_kaili): hair 4, 6, 14; skin 1, 2. */
    static const unsigned short hair = (1 << 4) | (1 << 6) | (1 << 14), skin = (1 << 1) | (1 << 2);
    unsigned short base[16], clut[16];
    unsigned char rec[DEF_SKIN_RECORD], bad[DEF_SKIN_RECORD];
    int i, k, ok, all_differ;

    for (i = 0; i < 16; i++) {
        base[i] = RGB((i * 7 + 3) % 32, (i * 11 + 5) % 32, (i * 5 + 9) % 32);
    }
    base[0] = 0;          /* transparent */
    base[15] = 0x8000 | RGB(31, 31, 31); /* white with the STP bit */

    def_skin_reset();
    check("a new game: the own colours, nothing to keep", def_skin_hero() == 0 && def_skin_worn() == 0 &&
                                                              def_skin_record_empty());
    check("no hunt won: only the own colours", def_skin_unlocked(0) && !def_skin_unlocked(1) && !def_skin_set_hero(1));
    done = 1;
    check("the tutorial won: the first skin, not the second", def_skin_unlocked(1) && !def_skin_unlocked(2));
    done = DEF_SUPER_COUNT;
    check("every hunt won: every skin, none past them",
          def_skin_unlocked(DEF_SKIN_COUNT) && !def_skin_unlocked(DEF_SKIN_COUNT + 1) && !def_skin_unlocked(-1));
    check("a skin chosen is worn", def_skin_set_hero(5) && def_skin_hero() == 5 && def_skin_worn() == 5);
    done = 4;
    check("no longer unlocked (another card's hunts): kept, but the own colours worn",
          def_skin_hero() == 5 && def_skin_worn() == 0);
    done = DEF_SUPER_COUNT;
    on = 0;
    check("a replay script: the own colours", def_skin_worn() == 0);
    on = 1;

    ok = 1;
    all_differ = 1;
    for (k = 1; k <= DEF_SKIN_COUNT; k++) {
        int changed = 0;

        memcpy(clut, base, sizeof(clut));
        def_skin_clut(clut, k, hair, skin);
        ok &= clut[0] == 0 && clut[1] == base[1] && clut[2] == base[2] && (clut[15] & 0x8000);
        for (i = 1; i < 16; i++) {
            ok &= (clut[i] & 0x7FFF) != 0;
            changed |= clut[i] != base[i];
        }
        all_differ &= changed;
    }
    check("every skin: transparent and the skin's colours stay, the STP bit stays, no colour turns transparent", ok);
    check("every skin changes something", all_differ);

    memcpy(clut, base, sizeof(clut));
    def_skin_clut(clut, 3, hair, skin); /* white clothes: the hair as it is */
    check("a clothes skin leaves the hair", clut[4] == base[4] && clut[6] == base[6] && clut[14] == base[14] &&
                                                clut[3] != base[3]);
    memcpy(clut, base, sizeof(clut));
    def_skin_clut(clut, 1, hair, skin); /* black hair: the clothes as they are */
    check("a hair skin leaves the clothes, and darkens the hair",
          clut[3] == base[3] && clut[5] == base[5] && (clut[4] & 31) <= (base[4] & 31) && clut[4] != base[4]);
    memcpy(clut, base, sizeof(clut));
    def_skin_clut(clut, 2, hair, skin); /* green hair: a tint, the shading kept */
    {
        int g4 = (clut[4] >> 5) & 31, r4 = clut[4] & 31, b4 = (clut[4] >> 10) & 31;
        int lum_in = (base[4] & 31) + ((base[4] >> 5) & 31) + ((base[4] >> 10) & 31);
        int lum_out = r4 + g4 + b4, lum_dark = (clut[14] & 31) + ((clut[14] >> 5) & 31) + ((clut[14] >> 10) & 31);
        int lum_dark_in = (base[14] & 31) + ((base[14] >> 5) & 31) + ((base[14] >> 10) & 31);

        check("a tint makes the hair green and keeps light and dark apart",
              g4 > r4 && g4 > b4 && clut[3] == base[3] && (lum_in > lum_dark_in) == (lum_out > lum_dark) &&
                  lum_out > 0);
    }
    memcpy(clut, base, sizeof(clut));
    def_skin_clut(clut, 0, hair, skin);
    check("no skin: the CLUT as it is", memcmp(clut, base, sizeof(clut)) == 0);
    def_skin_clut(clut, DEF_SKIN_COUNT + 1, hair, skin);
    check("a skin out of range: the CLUT as it is", memcmp(clut, base, sizeof(clut)) == 0);

    def_skin_record_write(rec);
    def_skin_reset();
    def_skin_record_read(rec);
    check("the record: the skin comes back", !def_skin_record_empty() && def_skin_hero() == 5 && rec[0] == 5);
    memset(bad, DEF_SKIN_COUNT + 1, sizeof(bad));
    def_skin_record_read(bad);
    check("a record from another build's table: a skin out of range reads as the own colours", def_skin_hero() == 0);

    return failures != 0;
}
