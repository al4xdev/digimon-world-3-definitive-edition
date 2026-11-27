/* The hero's skins: new colours for the hero (Kaili, or Junior without Girls Power),
 * picked at the Velvet Room's skins NPC. The hero's field sprites are 4-bit frames, each with its own 16-colour CLUT
 * (girls_power.c paints Junior's files into Kaili as cdload reads them); a skin changes that CLUT: the hair (or cap)
 * and the clothes each turned on the colour wheel and given a saturation and a lightness, the skin's colours left as
 * they are. Which CLUT index is hair and which is skin is the person's own (girls_power.c's GpWho masks), so one skin
 * suits both.
 *
 * Each hunt won unlocks one skin (hunt n won: skin n + 1; Sepikmon's tutorial gives the first). The choice is one
 * byte of the card's record (DEF_SKIN_RECORD). Off in a replay script (def_on). The battle Digimon keep their own
 * colours: only the rare foes are recoloured (definitive_recolor.c). */
#include <math.h>
#include <string.h>

#include "common.h"

#include "psxstack/savestate.h"

#include "definitive.h"

extern int def_on(void);

typedef struct SkinPart {
    short hue;           /* ROT: degrees round the grey axis; TINT: the hue every colour takes (0 red, 120 green) */
    unsigned char sat;   /* per cent of the colour's saturation (100: as it is, 0: grey) */
    unsigned char light; /* per cent of its lightness */
    unsigned char tint;  /* 1: TINT */
} SkinPart;

typedef struct SkinRecipe {
    SkinPart hair;    /* the hair (Kaili) or the cap (Junior) */
    SkinPart clothes; /* everything else but the skin and the transparent colour */
} SkinRecipe;

typedef char skin_per_hunt[DEF_SKIN_COUNT == DEF_SUPER_COUNT ? 1 : -1];

/* ROT turns the colours round the wheel from the hero's own, so one recipe gives each hero a different colour (Kaili's
 * pink hair and Junior's light blue cap went violet and mint under the same turn). TINT gives every colour of the part
 * one hue and keeps its lightness (the shading), so a skin looks alike on both heroes and its name can be a colour;
 * a colour near grey (Junior's white clothes) is given TINT_MIN_SAT of saturation first, or it would stay grey. */
#define ROT(h, s, l) { h, s, l, 0 }
#define TINT(h, s, l) { h, s, l, 1 }
#define SAME ROT(0, 100, 100)
#define TINT_MIN_SAT 0.45

/* In unlock order: skin i + 1 is hunt i's (def_super_rares order). Names: $Skin1..$Skin13 ($Skin0 the
 * own colours) in tools/text_override. Chosen on both heroes' field sprites (Kaili's pink hair and small outfit,
 * Junior's light blue cap and white clothes): every skin changes the hair, the part that shows most on Kaili, or the
 * clothes strongly enough to see; the hair hues keep away from pink (~345) and light blue (~215), which one hero
 * already wears. */
static const SkinRecipe skin_recipes[DEF_SKIN_COUNT] = {
    { ROT(0, 0, 45), SAME },                        /* 1  Black Hair (the tutorial's) */
    { TINT(135, 100, 100), SAME },                  /* 2  Green Hair */
    { SAME, ROT(0, 0, 115) },                       /* 3  White Outfit */
    { TINT(45, 110, 110), SAME },                   /* 4  Gold Hair */
    { SAME, TINT(0, 120, 90) },                     /* 5  Red Outfit */
    { ROT(0, 0, 45), ROT(0, 0, 55) },               /* 6  All Black */
    { TINT(275, 100, 100), SAME },                  /* 7  Purple Hair */
    { SAME, TINT(225, 110, 65) },                   /* 8  Navy Outfit */
    { ROT(0, 0, 130), SAME },                       /* 9  Silver Hair */
    { TINT(180, 100, 100), TINT(180, 100, 80) },    /* 10 Ocean */
    { TINT(25, 120, 105), TINT(270, 100, 85) },     /* 11 Sunset */
    { TINT(85, 120, 110), TINT(310, 110, 100) },    /* 12 Wild Colors */
    { ROT(0, 0, 130), ROT(0, 0, 45) },              /* 13 Silver and Black */
};

static unsigned char skin_hero; /* 0: the hero's own colours */

void def_skin_reset(void) {
    skin_hero = 0;
}

int def_skin_unlocked(int skin) {
    return skin == 0 || (skin >= 1 && skin <= DEF_SKIN_COUNT && def_super_done() >= skin);
}

int def_skin_hero(void) {
    return skin_hero;
}

int def_skin_set_hero(int skin) {
    if (!def_skin_unlocked(skin)) {
        return 0;
    }
    skin_hero = (unsigned char)skin;
    return 1;
}

/* What the hero's sprites wear now: the skin chosen while it is still unlocked (another card's hunts), 0 in a replay
 * script. */
int def_skin_worn(void) {
    return def_on() && def_skin_unlocked(skin_hero) ? skin_hero : 0;
}

/* ---- The colours. */

/* HSL on 0..1 channels. */
static void to_hsl(const double c[3], double *h, double *s, double *l) {
    double mx = c[0], mn = c[0], d;
    int i;

    for (i = 1; i < 3; i++) {
        mx = c[i] > mx ? c[i] : mx;
        mn = c[i] < mn ? c[i] : mn;
    }
    *l = (mx + mn) / 2;
    d = mx - mn;
    *h = 0;
    *s = d <= 0 ? 0 : d / (1 - fabs(2 * *l - 1));
    if (d > 0) {
        *h = mx == c[0] ? fmod((c[1] - c[2]) / d + 6, 6) : mx == c[1] ? (c[2] - c[0]) / d + 2 : (c[0] - c[1]) / d + 4;
        *h *= 60;
    }
}

static double hsl_channel(double p, double q, double t) {
    t = t < 0 ? t + 1 : t > 1 ? t - 1 : t;
    return t < 1 / 6.0 ? p + (q - p) * 6 * t : t < 0.5 ? q : t < 2 / 3.0 ? p + (q - p) * (2 / 3.0 - t) * 6 : p;
}

static void from_hsl(double h, double s, double l, double c[3]) {
    double q = l < 0.5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q;

    h /= 360;
    c[0] = hsl_channel(p, q, h + 1 / 3.0);
    c[1] = hsl_channel(p, q, h);
    c[2] = hsl_channel(p, q, h - 1 / 3.0);
}

static int clamp31(double v) {
    int i = (int)(v + 0.5);

    return i < 0 ? 0 : i > 31 ? 31 : i;
}

/* A 15-bit colour through one part of a recipe. 0 stays 0 (transparent), the STP bit stays, and a colour never
 * becomes 0 (which would turn it transparent). */
static unsigned short paint(unsigned short c, const SkinPart *p) {
    double rgb[3], out[3], g;
    double a = p->hue * 3.14159265358979 / 180.0, cs = cos(a), sn = sin(a);
    /* the usual hue rotation matrix (luminance weights 0.299/0.587/0.114) */
    double m[3][3] = {
        { 0.299 + 0.701 * cs + 0.168 * sn, 0.587 - 0.587 * cs + 0.330 * sn, 0.114 - 0.114 * cs - 0.497 * sn },
        { 0.299 - 0.299 * cs - 0.328 * sn, 0.587 + 0.413 * cs + 0.035 * sn, 0.114 - 0.114 * cs + 0.292 * sn },
        { 0.299 - 0.300 * cs + 1.250 * sn, 0.587 - 0.588 * cs - 1.050 * sn, 0.114 + 0.886 * cs - 0.203 * sn },
    };
    int i, v[3];
    unsigned short o;

    if ((c & 0x7FFF) == 0 || (!p->tint && p->hue == 0 && p->sat == 100 && p->light == 100)) {
        return c;
    }
    rgb[0] = c & 31;
    rgb[1] = (c >> 5) & 31;
    rgb[2] = (c >> 10) & 31;
    if (p->tint) {
        double h, s, l;

        for (i = 0; i < 3; i++) {
            rgb[i] /= 31;
        }
        to_hsl(rgb, &h, &s, &l);
        s = (s < TINT_MIN_SAT ? TINT_MIN_SAT : s) * p->sat / 100.0;
        l = l * p->light / 100.0;
        from_hsl(p->hue, s > 1 ? 1 : s, l > 1 ? 1 : l, out);
        for (i = 0; i < 3; i++) {
            v[i] = clamp31(out[i] * 31);
        }
        o = (unsigned short)(v[0] | v[1] << 5 | v[2] << 10);
        return (unsigned short)((o != 0 ? o : 1 | 1 << 5 | 1 << 10) | (c & 0x8000));
    }
    for (i = 0; i < 3; i++) {
        out[i] = m[i][0] * rgb[0] + m[i][1] * rgb[1] + m[i][2] * rgb[2];
    }
    g = 0.299 * out[0] + 0.587 * out[1] + 0.114 * out[2]; /* towards (or away from) the grey of the same lightness */
    for (i = 0; i < 3; i++) {
        v[i] = clamp31((g + (out[i] - g) * p->sat / 100.0) * p->light / 100.0);
    }
    o = (unsigned short)(v[0] | v[1] << 5 | v[2] << 10);
    if (o == 0) {
        o = 1 | 1 << 5 | 1 << 10;
    }
    return (unsigned short)(o | (c & 0x8000));
}

void def_skin_clut(unsigned short clut[16], int skin, unsigned short hair, unsigned short skin_mask) {
    const SkinRecipe *r;
    int i;

    if (skin < 1 || skin > DEF_SKIN_COUNT) {
        return;
    }
    r = &skin_recipes[skin - 1];
    for (i = 1; i < 16; i++) { /* index 0 is the transparent colour */
        if ((skin_mask >> i) & 1) {
            continue;
        }
        clut[i] = paint(clut[i], (hair >> i) & 1 ? &r->hair : &r->clothes);
    }
}

/* ---- The record: the hero's skin. A skin past DEF_SKIN_COUNT (another build's table) reads as 0. */

int def_skin_record_empty(void) {
    return skin_hero == 0;
}

void def_skin_record_write(unsigned char *out) {
    out[0] = skin_hero;
}

void def_skin_record_read(const unsigned char *in) {
    skin_hero = in[0] <= DEF_SKIN_COUNT ? in[0] : 0;
}

void def_skin_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, skin_hero);
}
