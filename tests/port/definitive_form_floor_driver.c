#include <stdio.h>
#include "definitive.h"

/* The 44 forms of records_digimon rows 9..52 and the stage each is (A Adult, P Perfect, M Mega), as the franchise has
 * them and the owner confirmed: Armormon and GrapLeomon are Perfect, MetalGarurumon is Mega. */
static const struct {
    const char *name;
    char stage;
} forms[44] = {
    {"Dinohumon", 'A'}, {"Hookmon", 'A'}, {"Grizzmon", 'A'}, {"Greymon", 'A'}, {"ExVeemon", 'A'},
    {"Growlmon", 'A'}, {"Kyubimon", 'A'}, {"Angemon", 'A'}, {"Devimon", 'A'}, {"Stingmon", 'A'},
    {"Angewomon", 'P'}, {"Kyukimon", 'P'}, {"Armormon", 'P'}, {"GrapLeomon", 'P'}, {"MetalGreymon", 'P'},
    {"SkullGreymon", 'P'}, {"Paildramon", 'P'}, {"WarGrowlmon", 'P'}, {"Taomon", 'P'}, {"MagnaAngemon", 'P'},
    {"Myotismon", 'P'}, {"MetalMamemon", 'P'}, {"Kabuterimon", 'A'}, {"Digitamamon", 'P'}, {"GuardiAngemon", 'M'},
    {"Cannondramon", 'M'}, {"Marsmon", 'M'}, {"WarGreymon", 'M'}, {"Imperialdramon", 'M'}, {"Gallantmon", 'M'},
    {"Sakuyamon", 'M'}, {"Seraphimon", 'M'}, {"MetalGarurumon", 'M'}, {"Rosemon", 'M'}, {"BK WarGreymon", 'M'},
    {"Imperialdramon-F", 'M'}, {"MaloMyotismon", 'M'}, {"MegaGargomon", 'M'}, {"GranKuwagamon", 'M'},
    {"Phoenixmon", 'M'}, {"Omnimon", 'M'}, {"Imperialdramon-P", 'M'}, {"Beelzemon", 'M'}, {"Diaboromon", 'M'},
};

int main(void) {
    int failures = 0, i;

    for (i = 0; i < 44; i++) {
        int row = 9 + i;
        int want = forms[i].stage == 'P' ? DEF_EVO_FLOOR_PERFECT : forms[i].stage == 'M' ? DEF_EVO_FLOOR_MEGA : 0;
        int got = def_form_floor(row);

        if (got != want) {
            printf("FAIL row %d %s: floor %d, want %d\n", row, forms[i].name, got, want);
            failures++;
        }
    }
    /* rows that are no form at all have no floor */
    if (def_form_floor(0) != 0 || def_form_floor(8) != 0 || def_form_floor(53) != 0 || def_form_floor(-1) != 0) {
        printf("FAIL rows outside 9..52 must have no floor\n");
        failures++;
    }
    if (failures == 0) {
        printf("ok   44 forms: Perfect %d, Mega %d, Adult none\n", DEF_EVO_FLOOR_PERFECT, DEF_EVO_FLOOR_MEGA);
    }
    return failures ? 1 : 0;
}
