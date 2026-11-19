/* The rare foes' colours (the owner's "shiny"): a regional Rare Target and a Super
 * Rare come with their palette swapped, a real palette swap (their texture's CLUT turned on the colour wheel as it is
 * uploaded), not a tint.
 *
 * Each battle Digimon has its own texture place in VRAM (fightstg_slots_vram, a slot per model), so the enemy's model
 * can be recoloured alone, even when a party member is the same species. The enemy's model comes in with an id from
 * 0x10 up (fightstg_slots_add, RC_ENEMY_SLOT_ID): that model is remembered with the kind of foe the battle has. Its
 * texture is a plain 16-bit VRAM page holding 4-bit (or 8-bit) texels and the CLUT rows side by side, so the page is
 * not touched as it loads; when the model's texture has loaded (fightstg_model_update) its page is remembered, and
 * each CLUT its meshes name (the texture command's offsets in the page) is read from the VRAM, turned and written
 * again the first time it is used. The texture file in RAM is never changed (it may be shared). Off in a replay
 * script (def_on). */
#include <math.h>
#include <string.h>

#include "common.h"
#include "psyq/libgpu.h"

#include "psxstack/savestate.h"

#include "definitive.h"

extern int def_on(void);
extern const unsigned short *psyq_gpu_vram(void); /* psxstack/psyq/psyq.h: the VRAM as the GPU left it */
extern int psyq_gpu_vram_stride(void);
extern void port_log(const char *fmt, ...);

/* fightstg_slots_add's ids, as seen in game: the battle's opening script adds the party's model with id 0 and the
 * enemy's with 0x10; an entrance adds side key2 + 1 (the party's 1, the enemy's 0x11), and a Blast's form comes in
 * with 1 (fightstg_digivolve_update, then renamed 0). Ids from 0x10 up are the enemy's side. */
#define RC_ENEMY_SLOT_ID 0x10
#define RC_PARTY_SLOT_ID 1
#define RC_MAX 256  /* colours of a CLUT: 16 (4-bit texels) or 256 (8-bit) */
#define RC_PAGES 4  /* recoloured models' pages at once (fightstg_slots_vram's slots) */
#define RC_CLUTS 32 /* CLUTs turned per page (a model names a few) */

/* The turn of the colour wheel, in degrees, per kind: the regional Rare's and the Super Rare's (owner-tunable). */
static const int rc_degrees[DEF_RECOLOR_KINDS] = { 0, 120, 200, 0, 0, 0 };

static const void *rc_model; /* the enemy's model in a rare battle (compared only, never followed) */
static int rc_kind;          /* its kind */
static const void *rc_party_model; /* the party's model in a Blast or Super Chosen form */
static int rc_party_kind;          /* DEF_RECOLOR_DARK or DEF_RECOLOR_GOLD */
/* The recoloured models' VRAM pages since their texture loaded, and the CLUTs already turned in each (x | y << 16,
 * VRAM coordinates), so none is turned twice. */
static struct {
    short x, y;
    signed char kind; /* 0: free */
    unsigned char n;
    unsigned int done[RC_CLUTS];
} rc_pages[RC_PAGES];

void def_recolor_model_added(const void *model, int id) {
    int kind;

    if (id == RC_PARTY_SLOT_ID || (id == 0 && def_finale_party_gold())) {
        if (def_finale_party_gold()) {
            rc_party_model = model;
            rc_party_kind = DEF_RECOLOR_GOLD;
            port_log("recolor: the party's Super Chosen form, gold");
        } else if (id == RC_PARTY_SLOT_ID && def_blast_dark()) {
            rc_party_model = model;
            rc_party_kind = DEF_RECOLOR_DARK;
            port_log("recolor: the party's Blast form, dark");
        } else {
            rc_party_model = NULL;
            rc_party_kind = DEF_RECOLOR_NONE;
        }
        return;
    }
    if (id < RC_ENEMY_SLOT_ID) {
        return;
    }
    if (def_finale_enemy_dark()) { /* Extended Final Act: Dark / Mega Galacticmon painted black */
        rc_model = model;
        rc_kind = DEF_RECOLOR_BLACK;
        port_log("recolor: Dark Galacticmon, black");
        return;
    }
    if (def_purify_fighting()) { /* the Blast Gauge's quest: the dark forms of the simulator come in dark too */
        rc_model = model;
        rc_kind = DEF_RECOLOR_DARK;
        port_log("recolor: the simulator's dark form");
        return;
    }
    kind = !def_on() ? DEF_RECOLOR_NONE : def_super_battle_ready() ? DEF_RECOLOR_SUPER : def_hunt_active() ? DEF_RECOLOR_RARE
                                                                                                          : DEF_RECOLOR_NONE;
    rc_model = kind != DEF_RECOLOR_NONE ? model : NULL;
    rc_kind = kind;
    if (kind != DEF_RECOLOR_NONE) {
        port_log("recolor: the enemy's model, kind %d (%d degrees)", kind, rc_degrees[kind]);
    }
}

int def_recolor_of(const void *model) {
    if (model != NULL && model == rc_party_model) {
        return rc_party_kind;
    }
    return model != NULL && model == rc_model ? rc_kind : DEF_RECOLOR_NONE;
}

/* A 15-bit colour turned `deg` degrees round the grey axis (the usual hue rotation matrix, luminance weights
 * 0.299/0.587/0.114). 0 stays 0 (transparent), the STP bit stays, and a colour never becomes 0 (which would turn it
 * transparent). */
static unsigned short rotate(unsigned short c, const double m[3][3]) {
    double r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31, out[3];
    int i, v[3];
    unsigned short o;

    if ((c & 0x7FFF) == 0) {
        return c;
    }
    for (i = 0; i < 3; i++) {
        out[i] = m[i][0] * r + m[i][1] * g + m[i][2] * b;
        v[i] = (int)(out[i] + 0.5);
        v[i] = v[i] < 0 ? 0 : v[i] > 31 ? 31 : v[i];
    }
    o = (unsigned short)(v[0] | v[1] << 5 | v[2] << 10);
    if (o == 0) {
        o = 1 | 1 << 5 | 1 << 10;
    }
    return (unsigned short)(o | (c & 0x8000));
}

/* The Blast Gauge's dark forms (owner, 2026-11-17: "paint them black"): the colour's luminance, dimmed to
 * RC_DARK_PCT and tinted RC_DARK_TINT (a little purple), so the shapes still read. 0 stays 0, the STP bit stays, and a
 * colour never becomes 0 (transparent). */
#define RC_DARK_PCT 40
static unsigned short darken(unsigned short c) {
    static const int tint[3] = { 95, 70, 120 }; /* per cent of the dimmed grey for red, green and blue */
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31, l, v[3], i;
    unsigned short o;

    if ((c & 0x7FFF) == 0) {
        return c;
    }
    l = (r * 299 + g * 587 + b * 114) / 1000 * RC_DARK_PCT / 100;
    for (i = 0; i < 3; i++) {
        v[i] = l * tint[i] / 100;
        v[i] = v[i] > 31 ? 31 : v[i];
    }
    o = (unsigned short)(v[0] | v[1] << 5 | v[2] << 10);
    if (o == 0) {
        o = 1 | 1 << 5 | 1 << 10;
    }
    return (unsigned short)(o | (c & 0x8000));
}

/* Dark / Mega Galacticmon: deep obsidian black with subtle crimson-violet highlights. */
static unsigned short blacken(unsigned short c) {
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31, l, vr, vg, vb;
    unsigned short o;

    if ((c & 0x7FFF) == 0) {
        return c;
    }
    l = (r * 299 + g * 587 + b * 114) / 1000;
    vr = l * 32 / 100;
    vg = l * 18 / 100;
    vb = l * 28 / 100;
    if (l > 22) {
        vr += (l - 22) * 2; /* glowing dark-crimson rim on bright accents */
    }
    vr = vr > 31 ? 31 : vr;
    vg = vg > 31 ? 31 : vg;
    vb = vb > 31 ? 31 : vb;
    o = (unsigned short)(vr | vg << 5 | vb << 10);
    if (o == 0) {
        o = 1 | 1 << 5 | 1 << 10;
    }
    return (unsigned short)(o | (c & 0x8000));
}

/* Golden Imperialdramon Paladin Mode: radiant royal gold mapped from luminance. */
static unsigned short golden(unsigned short c) {
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31, l, vr, vg, vb;
    unsigned short o;

    if ((c & 0x7FFF) == 0) {
        return c;
    }
    l = (r * 299 + g * 587 + b * 114) / 1000;
    vr = l * 125 / 100 + 3;
    vg = l * 100 / 100 + 1;
    vb = l * 35 / 100;
    if (l > 24) {
        vb += (l - 24) * 2; /* warm white-gold speculars */
    }
    vr = vr > 31 ? 31 : vr;
    vg = vg > 31 ? 31 : vg;
    vb = vb > 31 ? 31 : vb;
    o = (unsigned short)(vr | vg << 5 | vb << 10);
    if (o == 0) {
        o = 1 | 1 << 5 | 1 << 10;
    }
    return (unsigned short)(o | (c & 0x8000));
}

const unsigned short *def_recolor_clut(const unsigned short *src, int n, int kind) {
    static unsigned short out[RC_MAX];
    double a, cs, sn, m[3][3];
    int i;

    if (kind <= DEF_RECOLOR_NONE || kind >= DEF_RECOLOR_KINDS || n <= 0 || n > RC_MAX) {
        return src;
    }
    if (kind == DEF_RECOLOR_DARK) {
        for (i = 0; i < n; i++) {
            out[i] = darken(src[i]);
        }
        return out;
    }
    if (kind == DEF_RECOLOR_BLACK) {
        for (i = 0; i < n; i++) {
            out[i] = blacken(src[i]);
        }
        return out;
    }
    if (kind == DEF_RECOLOR_GOLD) {
        for (i = 0; i < n; i++) {
            out[i] = golden(src[i]);
        }
        return out;
    }
    a = rc_degrees[kind] * 3.14159265358979 / 180.0;
    cs = cos(a);
    sn = sin(a);
    m[0][0] = 0.299 + 0.701 * cs + 0.168 * sn;
    m[0][1] = 0.587 - 0.587 * cs + 0.330 * sn;
    m[0][2] = 0.114 - 0.114 * cs - 0.497 * sn;
    m[1][0] = 0.299 - 0.299 * cs - 0.328 * sn;
    m[1][1] = 0.587 + 0.413 * cs + 0.035 * sn;
    m[1][2] = 0.114 - 0.114 * cs + 0.292 * sn;
    m[2][0] = 0.299 - 0.300 * cs + 1.250 * sn;
    m[2][1] = 0.587 - 0.588 * cs - 1.050 * sn;
    m[2][2] = 0.114 + 0.886 * cs - 0.203 * sn;
    for (i = 0; i < n; i++) {
        out[i] = rotate(src[i], (const double(*)[3])m);
    }
    return out;
}

void def_recolor_texture_loaded(const void *model, int x, int y) {
    int kind = def_recolor_of(model), i, free = -1;

    for (i = 0; i < RC_PAGES; i++) { /* the page's last owner is gone: its CLUTs are new */
        if (rc_pages[i].kind != 0 && rc_pages[i].x == x && rc_pages[i].y == y) {
            rc_pages[i].kind = 0;
        }
        if (rc_pages[i].kind == 0 && free < 0) {
            free = i;
        }
    }
    if (kind == DEF_RECOLOR_NONE || free < 0) {
        return;
    }
    rc_pages[free].x = (short)x;
    rc_pages[free].y = (short)y;
    rc_pages[free].kind = (signed char)kind;
    rc_pages[free].n = 0;
}

void def_recolor_mesh_clut(int x, int y, int cx, int cy, int tp) {
    static unsigned short buf[RC_MAX];
    unsigned int key = (unsigned int)(cx & 0xFFFF) | (unsigned int)cy << 16;
    RECT r;
    int i, j;

    for (i = 0; i < RC_PAGES; i++) {
        if (rc_pages[i].kind != 0 && rc_pages[i].x == x && rc_pages[i].y == y) {
            break;
        }
    }
    if (i == RC_PAGES || (tp & 3) > 1) { /* not a recoloured page, or 16-bit texels (no CLUT) */
        return;
    }
    for (j = 0; j < rc_pages[i].n; j++) {
        if (rc_pages[i].done[j] == key) {
            return;
        }
    }
    if (rc_pages[i].n == RC_CLUTS) {
        return; /* full: the rest stay as they are rather than turn twice */
    }
    rc_pages[i].done[rc_pages[i].n++] = key;
    r.x = (short)cx;
    r.y = (short)cy;
    r.w = (tp & 3) == 0 ? 16 : 256;
    r.h = 1;
    if (cx < 0 || cy < 0 || cy >= 512 || cx + r.w > 1024) {
        return;
    }
    memcpy(buf, psyq_gpu_vram() + cy * psyq_gpu_vram_stride() + cx, r.w * sizeof(buf[0]));
    LoadImage(&r, (u32 *)def_recolor_clut(buf, r.w, rc_pages[i].kind));
}

void def_recolor_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, rc_model);
    PORT_STATE_VAR(s, rc_kind);
    PORT_STATE_VAR(s, rc_party_model);
    PORT_STATE_VAR(s, rc_party_kind);
    PORT_STATE_VAR(s, rc_pages);
}
