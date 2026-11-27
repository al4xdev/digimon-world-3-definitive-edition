/* The DRI Agents' Gauntlet, its rules (the screen and the battle are definitive_dri_screen.c).
 *
 * A challenge (def_dri_challenges, hand-maintained in definitive_dri_table.c) is one ordinary battle that runs an
 * evolution line's four stages, Rookie, Champion, Ultimate, Mega. The first three are its enemy members: the battle
 * already brings the next member in when the one fighting falls (wfightmn_knockout: the first member still standing,
 * with its entrance), at the full HP it was given at the start, while the party keeps its HP, MP and the items it
 * spent: that is "the battle does not end, the next form comes in". The Mega comes in the same way once the third has
 * fallen, as a wave set up in a fallen member's slot (def_dri_next_wave).
 *
 * Unlocking a challenge hands over its Hunt Materials (a common and one or two rare items) once; it stays open for good.
 * Each stage felled past the Rookie reaches a tier of the line's Synergy accessory (tier n for the Rookie and n more):
 * every tier is given the first time it is reached, so the first full clear gives all three and a replay gives none;
 * the best tier per challenge only rises. Both live in the slot's second card record (definitive_bestiary.c: an
 * unlocked bit and two bits of best tier per challenge; with three stages, before the Rookie came, the tier was the
 * stages felled, so an older card means the same).
 *
 * Off in a replay script (def_on): no challenge exists, nothing is unlocked or awarded. */
#include <stddef.h>

#include "definitive.h"

extern int def_on(void); /* port.h declares it under PC_PORT only; the host test builds without it */
extern void port_log(const char *fmt, ...);
extern const unsigned char def_str_DriSep[], def_str_HuntSpace[], def_str_DriOf[], def_str_DriUnlock[];
extern const unsigned char def_str_DriRival0[], def_str_DriRival0Hello[], def_str_DriRival0Won[], def_str_DriRival0Lost[],
    def_str_DriRival1[], def_str_DriRival1Hello[], def_str_DriRival1Won[], def_str_DriRival1Lost[],
    def_str_DriRival2[], def_str_DriRival2Hello[], def_str_DriRival2Won[], def_str_DriRival2Lost[],
    def_str_DriRival3[], def_str_DriRival3Hello[], def_str_DriRival3Won[], def_str_DriRival3Lost[],
    def_str_DriRival4[], def_str_DriRival4Hello[], def_str_DriRival4Won[], def_str_DriRival4Lost[],
    def_str_DriRival5[], def_str_DriRival5Hello[], def_str_DriRival5Won[], def_str_DriRival5Lost[],
    def_str_DriRival6[], def_str_DriRival6Hello[], def_str_DriRival6Won[], def_str_DriRival6Lost[],
    def_str_DriRival7[], def_str_DriRival7Hello[], def_str_DriRival7Won[], def_str_DriRival7Lost[],
    def_str_DriRival8[], def_str_DriRival8Hello[], def_str_DriRival8Won[], def_str_DriRival8Lost[],
    def_str_DriRival9[], def_str_DriRival9Hello[], def_str_DriRival9Won[], def_str_DriRival9Lost[],
    def_str_DriRival10[], def_str_DriRival10Hello[], def_str_DriRival10Won[], def_str_DriRival10Lost[],
    def_str_DriRival11[], def_str_DriRival11Hello[], def_str_DriRival11Won[], def_str_DriRival11Lost[],
    def_str_DriRival12[], def_str_DriRival12Hello[], def_str_DriRival12Won[], def_str_DriRival12Lost[],
    def_str_DriRival13[], def_str_DriRival13Hello[], def_str_DriRival13Won[], def_str_DriRival13Lost[],
    def_str_DriRival14[], def_str_DriRival14Hello[], def_str_DriRival14Won[], def_str_DriRival14Lost[],
    def_str_DriRival15[], def_str_DriRival15Hello[], def_str_DriRival15Won[], def_str_DriRival15Lost[];

const DefDriChallenge *def_dri_challenge(int c) {
    return def_on() && c >= 0 && c < def_dri_challenge_count && c < DEF_DRI_CHALLENGES ? &def_dri_challenges[c] : NULL;
}

int def_dri_tier(int fallen) {
    int tier = fallen - (DEF_DRI_STAGES - DEF_DRI_TIERS);

    return tier < 0 ? 0 : tier > DEF_DRI_TIERS ? DEF_DRI_TIERS : tier;
}

/* The stages felled a tier stands for (the screen's pips): a tier past 0 had the Rookie and as many more; 0 may have
 * had the Rookie, which the card does not keep. */
int def_dri_tier_stages(int tier) {
    return tier <= 0 ? 0 : (tier > DEF_DRI_TIERS ? DEF_DRI_TIERS : tier) + DEF_DRI_STAGES - DEF_DRI_TIERS;
}

int def_dri_agent_index(int map, int actor) {
    int i;

    if (!def_on()) {
        return -1;
    }
    for (i = 0; i < def_dri_agent_count; i++) {
        if (def_dri_agents[i].actor == actor && (def_dri_agents[i].map < 0 || def_dri_agents[i].map == map)) {
            return i;
        }
    }
    return -1;
}

int def_dri_agent(int map, int actor) {
    return def_dri_agent_index(map, actor) >= 0;
}

/* No names: each is a classic Tamer by the title of what their partner's line stood for (en.txt). */
static const unsigned char *const rival_texts[DEF_DRI_RIVALS][4] = {
    { def_str_DriRival0, def_str_DriRival0Hello, def_str_DriRival0Won, def_str_DriRival0Lost },
    { def_str_DriRival1, def_str_DriRival1Hello, def_str_DriRival1Won, def_str_DriRival1Lost },
    { def_str_DriRival2, def_str_DriRival2Hello, def_str_DriRival2Won, def_str_DriRival2Lost },
    { def_str_DriRival3, def_str_DriRival3Hello, def_str_DriRival3Won, def_str_DriRival3Lost },
    { def_str_DriRival4, def_str_DriRival4Hello, def_str_DriRival4Won, def_str_DriRival4Lost },
    { def_str_DriRival5, def_str_DriRival5Hello, def_str_DriRival5Won, def_str_DriRival5Lost },
    { def_str_DriRival6, def_str_DriRival6Hello, def_str_DriRival6Won, def_str_DriRival6Lost },
    { def_str_DriRival7, def_str_DriRival7Hello, def_str_DriRival7Won, def_str_DriRival7Lost },
    { def_str_DriRival8, def_str_DriRival8Hello, def_str_DriRival8Won, def_str_DriRival8Lost },
    { def_str_DriRival9, def_str_DriRival9Hello, def_str_DriRival9Won, def_str_DriRival9Lost },
    { def_str_DriRival10, def_str_DriRival10Hello, def_str_DriRival10Won, def_str_DriRival10Lost },
    { def_str_DriRival11, def_str_DriRival11Hello, def_str_DriRival11Won, def_str_DriRival11Lost },
    { def_str_DriRival12, def_str_DriRival12Hello, def_str_DriRival12Won, def_str_DriRival12Lost },
    { def_str_DriRival13, def_str_DriRival13Hello, def_str_DriRival13Won, def_str_DriRival13Lost },
    { def_str_DriRival14, def_str_DriRival14Hello, def_str_DriRival14Won, def_str_DriRival14Lost },
    { def_str_DriRival15, def_str_DriRival15Hello, def_str_DriRival15Won, def_str_DriRival15Lost },
};

const unsigned char *def_dri_rival(int c, int what) {
    if (def_dri_challenge(c) == NULL || c >= DEF_DRI_RIVALS || what < DEF_DRI_RIVAL_NAME || what > DEF_DRI_RIVAL_LOST) {
        return NULL;
    }
    return rival_texts[c][what];
}

static int has_cost(const DefDriChallenge *ch) {
    int i;

    if (ch->common >= 0 && def_record_material(ch->common) < ch->common_n) {
        return 0;
    }
    for (i = 0; i < 2; i++) {
        if (ch->rare[i] >= 0 && def_record_rare(ch->rare[i]) < ch->rare_n[i]) {
            return 0;
        }
    }
    return 1;
}

int def_dri_can_unlock(int c) {
    const DefDriChallenge *ch = def_dri_challenge(c);

    return ch != NULL && !def_record_dri_unlocked(c) && has_cost(ch);
}

int def_dri_unlock(int c) {
    const DefDriChallenge *ch = def_dri_challenge(c);
    int i;

    if (!def_dri_can_unlock(c)) {
        return 0;
    }
    if (ch->common >= 0) {
        def_record_material_add(ch->common, -ch->common_n);
    }
    for (i = 0; i < 2; i++) {
        if (ch->rare[i] >= 0) {
            def_record_rare_add(ch->rare[i], -ch->rare_n[i]);
        }
    }
    def_record_dri_unlock(c);
    port_log("dri: challenge %d unlocked (material %d x%d, rares %d x%d, %d x%d)", c, ch->common, ch->common_n, ch->rare[0],
             ch->rare_n[0], ch->rare[1], ch->rare_n[1]);
    return 1;
}

int def_dri_award(int c, int fallen, short items[DEF_DRI_TIERS]) {
    const DefDriChallenge *ch = def_dri_challenge(c);
    int tier = def_dri_tier(fallen), t, n = 0;

    if (ch == NULL) {
        return 0;
    }
    for (t = def_record_dri_best(c) + 1; t <= tier; t++) {
        items[n++] = ch->rewards[t - 1];
    }
    def_record_dri_raise_best(c, tier);
    return n;
}

/* "Unlock: <common> 12/30" and, on a second line, "<Rare Target> IV 0/1, <Rare Target> II 1/1": the rare items are
 * named by the targets that leave them, as the forge's line does (shorter than the items' names). */
const unsigned char *def_dri_cost_label(int c, int *palette) {
    static const unsigned char line_break[] = { 2, 1, 0 }; /* control code 1: the next line */
    static unsigned char buf[2 * DEF_HUNT_LINE_SIZE];
    const DefDriChallenge *ch = def_dri_challenge(c);
    int p = 0, i, first = 1;

    if (ch == NULL) {
        return NULL;
    }
    buf[0] = 0;
    def_glyph_put(buf, &p, sizeof(buf), def_str_DriUnlock);
    if (ch->common >= 0) {
        def_glyph_put(buf, &p, sizeof(buf), def_hunt_material_name(ch->common));
        def_glyph_put(buf, &p, sizeof(buf), def_str_HuntSpace);
        def_glyph_count(buf, &p, sizeof(buf), def_record_material(ch->common));
        def_glyph_put(buf, &p, sizeof(buf), def_str_DriOf);
        def_glyph_count(buf, &p, sizeof(buf), ch->common_n);
    }
    for (i = 0; i < 2; i++) {
        if (ch->rare[i] < 0) {
            continue;
        }
        def_glyph_put(buf, &p, sizeof(buf), first ? line_break : def_str_DriSep);
        first = 0;
        def_hunt_target_name(buf, &p, sizeof(buf), ch->rare[i]);
        def_glyph_put(buf, &p, sizeof(buf), def_str_HuntSpace);
        def_glyph_count(buf, &p, sizeof(buf), def_record_rare(ch->rare[i]));
        def_glyph_put(buf, &p, sizeof(buf), def_str_DriOf);
        def_glyph_count(buf, &p, sizeof(buf), ch->rare_n[i]);
    }
    if (palette != NULL) {
        *palette = has_cost(ch) ? 0 : 3;
    }
    return buf;
}
