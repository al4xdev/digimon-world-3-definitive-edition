/* Definitive combat: the bestiary's table (what the player learned of each species' elements) and its card
 * record. The record lives in the 0x800 bytes of the save file the game never writes (file offset 0x7800.., one 0x100
 * record per slot), read and written by memcard.c's part 5; the slot's own tail holds the MP converter's
 * marker (0x26EC). Records: "BEST", version, XOR checksum of the table, 193 bytes. */
#include <stdlib.h>
#include <string.h>

#include "psxstack/savestate.h"

#include "definitive.h"
#include "definitive_expansion.h"

extern int port_script_active;
extern void port_log(const char *fmt, ...);
extern int fightstg_enemy_find_record(int id);

#define REC_TABLE 8 /* offset of the table in the record (magic 4, version 1, checksum 1, 2 unused) */
/* The record's spare bytes after the table carry the slot's other port state (the slot's own tail is full): at
 * REC_EXTRA "X1", the flags (DEF_SAVE_HARD), a byte unused, the Guardromon network's mask (4 bytes, low first), then the
 * hunt's region and chain, then the Hunt Materials, a byte each, then the Legendary Replicas held, a bit each, low
 * byte first (the rest of the 48 bytes is spare). A record without the "X1" (an older save) reads as none of it, and
 * one written before a field existed reads its bytes as 0. */
#define REC_EXTRA 0xD0
/* After the replicas, to the record's end: the expansion save's bytes (definitive_expansion.h: its magic and the
 * generation that pairs the slot with its entry in the file beside the card). They were the enemy personas' "solved"
 * bits before; the expansion takes those as its personas section's (migrate_from_card). An older record reads as no
 * generation. */
#define REC_EXPANSION (REC_EXTRA + 30)
typedef char rec_expansion_fits[REC_EXPANSION + DEF_EXPANSION_CARD_BYTES <= DEF_BESTIARY_RECORD_SIZE ? 1 : -1];

static unsigned char extra_flags;
static unsigned int extra_network;
static unsigned char extra_region, extra_chain;
static unsigned char extra_materials[DEF_HUNT_MATERIALS];
static unsigned int extra_replicas; /* DEF_REPLICAS bits: four bytes of the record (REC_EXTRA + 26..29) */
/* The second record: "HUNT", version, XOR checksum of the rest, a byte unused, the rare items' counts,
 * then the bits of the ones ever held (the bestiary's collection); the rest of the 0x100 is spare. */
#define REC2_RARES 8
#define REC2_SEEN (REC2_RARES + DEF_HUNT_RARES)
static unsigned char rares[DEF_HUNT_RARES];
static unsigned char rares_seen[DEF_HUNT_RARES / 8];
static unsigned char buffer2[DEF_RECORD2_SIZE];
/* After the rares' bits: the DRI challenges unlocked, a bit each (2 bytes, low first), then each one's best
 * stage, 2 bits each (4 bytes, low first). The record's version stays 1: one written before them reads them as 0, and
 * the checksum already covered these bytes as zeros. */
#define REC2_DRI (REC2_SEEN + DEF_HUNT_RARES / 8)
typedef char rec2_dri_fits[DEF_DRI_CHALLENGES <= 16 && REC2_DRI + 6 <= DEF_RECORD2_SIZE ? 1 : -1];
static unsigned short dri_unlocked;
static unsigned int dri_best;
/* The Blast Gauge (owner's redesign, 2026-11-17), after the DRI's bytes: each partner's next dark form in its sequence
 * (a byte each), the TP the Blast took from each (2 bytes each, low first) that the purification gives back, the
 * quest's stage, the quest's forms purified (a bit per entry of the third record, 2 bytes), the Chosen Evolution's
 * partner + 1 (0: none) and its charge; then the Blast's boost bought from the false Gennai (steps of
 * DEF_BLAST_BOOST_STEP %), and the Velvet Room's story hooks (DEF_STORY_* bits, 2 bytes, and the gatekeeper's stage).
 * Version 1 still: an older record reads them as 0. */
#define REC2_BLAST (REC2_DRI + 6)
#define REC2_BLAST_OWED (REC2_BLAST + DEF_BLAST_PARTNERS)
#define REC2_BLAST_QUEST (REC2_BLAST_OWED + 2 * DEF_BLAST_PARTNERS)
#define REC2_BLAST_END (REC2_BLAST_QUEST + 9)
typedef char rec2_blast_fits[DEF_BLAST_QUEST_FORMS <= 16 && REC2_BLAST_END <= DEF_RECORD2_SIZE ? 1 : -1];
static unsigned char blast_next[DEF_BLAST_PARTNERS];
static unsigned char blast_heat[DEF_BLAST_PARTNERS]; /* the overheat's turns left: the high nibble of blast_next's byte */
static unsigned short blast_owed[DEF_BLAST_PARTNERS];
static unsigned char blast_quest, chosen_partner, chosen_charge; /* chosen_partner: the partner + 1, 0 none */
static unsigned short blast_purified;
static unsigned char blast_boost, story_gate;
static unsigned short story_flags;
/* The story's answers (definitive_story.c: Igor's dreams and the scenes' choices), two bits per question, four to a
 * byte from the low bits: 0 not asked yet, else the option taken (1..3). Right after the Blast's bytes; an older record
 * reads them as 0. */
#define REC2_ANSWERS REC2_BLAST_END
static unsigned char story_answers[DEF_STORY_ANSWER_BYTES];

static int any_answer(void) {
    int i;

    for (i = 0; i < DEF_STORY_ANSWER_BYTES; i++) {
        if (story_answers[i] != 0) {
            return 1;
        }
    }
    return 0;
}
/* The third record (DEF_RECORD3_*, memcard.c part 7): "BLST", version, XOR checksum of the rest, the entries' count, a
 * byte unused, then the quest's dark forms, the first DEF_BLAST_QUEST_FORMS partner+form pairs a Blast called up, each
 * packed in REC3_ENTRY bytes (low bit first): the pair (partner * DEF_BLAST_STEPS + step, 6 bits), its first stats (5 x
 * 10 bits, as FightstgStats.stats), its HP (14 bits), its strongest stat (3 bits) and the best that stat ever was (10
 * bits). */
#define REC3_ENTRIES 8
#define REC3_ENTRY 11
typedef char rec3_fits[REC3_ENTRIES + DEF_BLAST_QUEST_FORMS * REC3_ENTRY <= DEF_RECORD3_SIZE ? 1 : -1];
static DefBlastEntry blast_entries[DEF_BLAST_QUEST_FORMS];
static int blast_count;
static unsigned char buffer3[DEF_RECORD3_SIZE];
/* The Super Rare hunts (definitive_superhunt.c) keep their DEF_SUPER_RECORD bytes at the record's end, away from the
 * layers that grow from its start. Version 1 still: an older record reads them as 0 (no hunt). */
#define REC2_SUPER (DEF_RECORD2_SIZE - DEF_SUPER_RECORD)
typedef char rec2_super_fits[REC2_BLAST_END <= REC2_SUPER ? 1 : -1];
/* The hero's skin (definitive_skins.c), just before the hunts' bytes. Version 1 still: an older record reads it as 0
 * (the own colours). */
#define REC2_SKIN (REC2_SUPER - DEF_SKIN_RECORD)
typedef char rec2_skin_fits[REC2_ANSWERS + DEF_STORY_ANSWER_BYTES <= REC2_SKIN ? 1 : -1];

static unsigned char table[DEF_BESTIARY_SPECIES];
static unsigned char buffer[DEF_BESTIARY_RECORD_SIZE];

static void blast_reset2(void) {
    memset(blast_next, 0, sizeof(blast_next));
    memset(blast_heat, 0, sizeof(blast_heat));
    memset(blast_owed, 0, sizeof(blast_owed));
    blast_quest = 0;
    chosen_partner = 0;
    chosen_charge = 0;
    blast_purified = 0;
    blast_boost = 0;
    story_flags = 0;
    story_gate = 0;
    memset(story_answers, 0, sizeof(story_answers));
}

static void blast_reset(void) {
    blast_reset2();
    memset(blast_entries, 0, sizeof(blast_entries));
    blast_count = 0;
}

static int blast_any(void) {
    unsigned int i;

    for (i = 0; i < DEF_BLAST_PARTNERS; i++) {
        if (blast_next[i] != 0 || blast_heat[i] != 0 || blast_owed[i] != 0) {
            return 1;
        }
    }
    return blast_quest != 0 || chosen_partner != 0 || chosen_charge != 0 || blast_purified != 0 || blast_boost != 0 ||
           story_flags != 0 || story_gate != 0 || any_answer();
}

void def_bestiary_reset(void) {
    memset(table, 0, sizeof(table));
    extra_flags = 0;
    extra_network = 0;
    extra_region = 0;
    extra_chain = 0;
    memset(extra_materials, 0, sizeof(extra_materials));
    extra_replicas = 0;
    def_expansion_reset();
    memset(rares, 0, sizeof(rares));
    memset(rares_seen, 0, sizeof(rares_seen));
    dri_unlocked = 0;
    dri_best = 0;
    blast_reset();
    def_super_reset();
    def_skin_reset();
    def_finale_reset();
    def_purify_reset();
    def_dri_reset();
    dw3_velvet_reset();
    def_time_reset();
}

unsigned int def_network_mask(void) {
    return extra_network;
}

void def_network_unlock(int node) {
    if (node >= 0 && node < 32) {
        extra_network |= 1u << node;
    }
}

int def_save_flag(int flag) {
    return (extra_flags & flag) != 0;
}

void def_record_hunt(int *region, int *chain) {
    *region = extra_region;
    *chain = extra_chain;
}

void def_record_set_hunt(int region, int chain) {
    extra_region = (unsigned char)region;
    extra_chain = (unsigned char)(chain < 0 ? 0 : chain > 255 ? 255 : chain);
}

int def_record_material(int material) {
    return material >= 0 && material < DEF_HUNT_MATERIALS ? extra_materials[material] : 0;
}

void def_record_material_add(int material, int n) {
    int v;

    if (material < 0 || material >= DEF_HUNT_MATERIALS) {
        return;
    }
    v = extra_materials[material] + n;
    extra_materials[material] = (unsigned char)(v < 0 ? 0 : v > DEF_HUNT_MATERIAL_CAP ? DEF_HUNT_MATERIAL_CAP : v);
}

int def_record_replica(int r) {
    return r >= 0 && r < DEF_REPLICAS && ((extra_replicas >> r) & 1) != 0;
}

void def_record_replica_set(int r, int on) {
    if (r >= 0 && r < DEF_REPLICAS) {
        extra_replicas = on ? extra_replicas | (1u << r) : extra_replicas & ~(1u << r);
    }
}

/* 1 when anything of the slot's other port state is set: the record (and its "X1" part) is written only then. */
static int extra_any(void) {
    int i;

    for (i = 0; i < DEF_HUNT_MATERIALS; i++) {
        if (extra_materials[i] != 0) {
            return 1;
        }
    }
    return def_expansion_keeps() || extra_flags != 0 || extra_network != 0 || extra_region != 0 || extra_chain != 0 ||
           extra_replicas != 0;
}

void def_save_flag_set(int flag, int on) {
    extra_flags = on ? extra_flags | flag : extra_flags & ~flag;
}

void def_bestiary_discover(int index, int element_mask) {
    if (index >= 0 && index < DEF_BESTIARY_SPECIES) {
        table[index] |= element_mask & ((1 << DEF_BESTIARY_ELEMENTS) - 1);
    }
}

int def_bestiary_known(int index) {
    return index >= 0 && index < DEF_BESTIARY_SPECIES ? table[index] : 0;
}

int def_bestiary_index(int species_id) {
    int i = fightstg_enemy_find_record(species_id);

    return i >= 0 && i < DEF_BESTIARY_SPECIES ? i : -1;
}

void def_bestiary_savestate(struct PortState *s) {
    PORT_STATE_VAR(s, table);
    PORT_STATE_VAR(s, extra_flags);
    PORT_STATE_VAR(s, extra_network);
    PORT_STATE_VAR(s, extra_region);
    PORT_STATE_VAR(s, extra_chain);
    PORT_STATE_VAR(s, extra_materials);
    PORT_STATE_VAR(s, extra_replicas);
    def_expansion_savestate(s); /* the expansion save's generation and sections, in memory */
    PORT_STATE_VAR(s, rares);
    PORT_STATE_VAR(s, rares_seen);
    PORT_STATE_VAR(s, dri_unlocked);
    PORT_STATE_VAR(s, dri_best);
    PORT_STATE_VAR(s, blast_next);
    PORT_STATE_VAR(s, blast_heat);
    PORT_STATE_VAR(s, blast_owed);
    PORT_STATE_VAR(s, blast_quest);
    PORT_STATE_VAR(s, chosen_partner);
    PORT_STATE_VAR(s, chosen_charge);
    PORT_STATE_VAR(s, blast_purified);
    PORT_STATE_VAR(s, blast_boost);
    PORT_STATE_VAR(s, story_flags);
    PORT_STATE_VAR(s, story_gate);
    PORT_STATE_VAR(s, story_answers);
    PORT_STATE_VAR(s, blast_entries);
    PORT_STATE_VAR(s, blast_count);
    def_super_savestate(s);
}

static unsigned char sum(const unsigned char *p, int n) {
    unsigned char x = 0;

    while (n-- > 0) {
        x ^= *p++;
    }
    return x;
}

void def_bestiary_record_write(unsigned char *out) {
    int i;

    memset(out, 0, DEF_BESTIARY_RECORD_SIZE);
    for (i = 0; i < DEF_BESTIARY_SPECIES; i++) {
        if (table[i] != 0) {
            break;
        }
    }
    if (i == DEF_BESTIARY_SPECIES && !extra_any()) {
        return;
    }
    port_log("bestiary: the slot's record written");
    memcpy(out, "BEST", 4);
    out[4] = DEF_BESTIARY_VERSION;
    memcpy(out + REC_TABLE, table, DEF_BESTIARY_SPECIES);
    out[5] = sum(out + REC_TABLE, DEF_BESTIARY_SPECIES);
    if (extra_any()) { /* none of it: the record stays as it always was */
        out[REC_EXTRA] = 'X';
        out[REC_EXTRA + 1] = '1';
        out[REC_EXTRA + 2] = extra_flags;
        for (i = 0; i < 4; i++) {
            out[REC_EXTRA + 4 + i] = (unsigned char)(extra_network >> (8 * i));
        }
        out[REC_EXTRA + 8] = extra_region;
        out[REC_EXTRA + 9] = extra_chain;
        memcpy(out + REC_EXTRA + 10, extra_materials, DEF_HUNT_MATERIALS);
        for (i = 0; i < 4; i++) {
            out[REC_EXTRA + 26 + i] = (unsigned char)(extra_replicas >> (8 * i));
        }
        def_expansion_card_write(out + REC_EXPANSION);
    }
}

void def_bestiary_record_read(const unsigned char *in) {
    int i, known = 0;

    def_bestiary_reset();
    if (in == NULL || memcmp(in, "BEST", 4) != 0 || in[4] != DEF_BESTIARY_VERSION ||
        in[5] != sum(in + REC_TABLE, DEF_BESTIARY_SPECIES)) {
        return;
    }
    for (i = 0; i < DEF_BESTIARY_SPECIES; i++) {
        table[i] = in[REC_TABLE + i] & ((1 << DEF_BESTIARY_ELEMENTS) - 1);
        known += table[i] != 0;
    }
    if (in[REC_EXTRA] == 'X' && in[REC_EXTRA + 1] == '1') {
        extra_flags = in[REC_EXTRA + 2];
        for (i = 0; i < 4; i++) {
            extra_network |= (unsigned int)in[REC_EXTRA + 4 + i] << (8 * i);
        }
        extra_region = in[REC_EXTRA + 8];
        extra_chain = in[REC_EXTRA + 9];
        for (i = 0; i < DEF_HUNT_MATERIALS; i++) { /* no checksum covers it: a count past the cap reads as the cap */
            extra_materials[i] = in[REC_EXTRA + 10 + i] > DEF_HUNT_MATERIAL_CAP ? DEF_HUNT_MATERIAL_CAP
                                                                               : in[REC_EXTRA + 10 + i];
        }
        extra_replicas = 0;
        for (i = 0; i < 4; i++) {
            extra_replicas |= (unsigned int)in[REC_EXTRA + 26 + i] << (8 * i);
        }
        def_expansion_card_read(in + REC_EXPANSION);
    }
    port_log("bestiary: the slot's record read: %d species known, flags %d, network 0x%X", known, extra_flags,
             extra_network);
}

int def_record_rare(int rare) {
    return rare >= 0 && rare < DEF_HUNT_RARES ? rares[rare] : 0;
}

int def_record_rare_seen(int rare) {
    return rare >= 0 && rare < DEF_HUNT_RARES && ((rares_seen[rare / 8] >> (rare % 8)) & 1);
}

void def_record_rare_add(int rare, int n) {
    int v;

    if (rare < 0 || rare >= DEF_HUNT_RARES) {
        return;
    }
    v = rares[rare] + n;
    rares[rare] = (unsigned char)(v < 0 ? 0 : v > DEF_HUNT_RARE_CAP ? DEF_HUNT_RARE_CAP : v);
    if (n > 0) {
        rares_seen[rare / 8] |= 1 << (rare % 8);
    }
}

void def_record2_write(unsigned char *out) {
    unsigned int i;

    memset(out, 0, DEF_RECORD2_SIZE);
    for (i = 0; i < sizeof(rares_seen) && rares_seen[i] == 0; i++) {
    }
    if (i == sizeof(rares_seen) && dri_unlocked == 0 && dri_best == 0 && !blast_any() &&
        def_super_record_empty() && def_skin_record_empty()) {
        return; /* no rare ever held (a count needs one), no DRI challenge, no dark form (TP owed needs one), no Super
                 * Rare hunt, no skin: the card stays as it was */
    }
    memcpy(out, "HUNT", 4);
    out[4] = DEF_RECORD2_VERSION;
    memcpy(out + REC2_RARES, rares, sizeof(rares));
    memcpy(out + REC2_SEEN, rares_seen, sizeof(rares_seen));
    for (i = 0; i < 2; i++) {
        out[REC2_DRI + i] = (unsigned char)(dri_unlocked >> (8 * i));
    }
    for (i = 0; i < 4; i++) {
        out[REC2_DRI + 2 + i] = (unsigned char)(dri_best >> (8 * i));
    }
    for (i = 0; i < DEF_BLAST_PARTNERS; i++) {
        out[REC2_BLAST + i] = (unsigned char)(blast_next[i] | blast_heat[i] << 4);
        out[REC2_BLAST_OWED + 2 * i] = (unsigned char)blast_owed[i];
        out[REC2_BLAST_OWED + 2 * i + 1] = (unsigned char)(blast_owed[i] >> 8);
    }
    out[REC2_BLAST_QUEST] = blast_quest;
    out[REC2_BLAST_QUEST + 1] = (unsigned char)blast_purified;
    out[REC2_BLAST_QUEST + 2] = (unsigned char)(blast_purified >> 8);
    out[REC2_BLAST_QUEST + 3] = chosen_partner;
    out[REC2_BLAST_QUEST + 4] = chosen_charge;
    out[REC2_BLAST_QUEST + 5] = blast_boost;
    out[REC2_BLAST_QUEST + 6] = (unsigned char)story_flags;
    out[REC2_BLAST_QUEST + 7] = (unsigned char)(story_flags >> 8);
    out[REC2_BLAST_QUEST + 8] = story_gate;
    memcpy(out + REC2_ANSWERS, story_answers, sizeof(story_answers));
    def_super_record_write(out + REC2_SUPER);
    def_skin_record_write(out + REC2_SKIN);
    out[5] = sum(out + REC2_RARES, DEF_RECORD2_SIZE - REC2_RARES);
    port_log("hunt: the slot's second record written");
}

void def_record2_read(const unsigned char *in) {
    int i;

    memset(rares, 0, sizeof(rares));
    memset(rares_seen, 0, sizeof(rares_seen));
    dri_unlocked = 0;
    dri_best = 0;
    blast_reset2();
    def_super_reset();
    def_skin_reset();
    if (in == NULL || memcmp(in, "HUNT", 4) != 0 || in[4] != DEF_RECORD2_VERSION ||
        in[5] != sum(in + REC2_RARES, DEF_RECORD2_SIZE - REC2_RARES)) {
        return;
    }
    for (i = 0; i < DEF_HUNT_RARES; i++) {
        rares[i] = in[REC2_RARES + i] > DEF_HUNT_RARE_CAP ? DEF_HUNT_RARE_CAP : in[REC2_RARES + i];
    }
    memcpy(rares_seen, in + REC2_SEEN, sizeof(rares_seen));
    dri_unlocked = (unsigned short)(in[REC2_DRI] | in[REC2_DRI + 1] << 8);
    for (i = 0; i < 4; i++) {
        dri_best |= (unsigned int)in[REC2_DRI + 2 + i] << (8 * i);
    }
    for (i = 0; i < DEF_BLAST_PARTNERS; i++) {
        blast_next[i] = (in[REC2_BLAST + i] & 15) < DEF_BLAST_STEPS ? in[REC2_BLAST + i] & 15 : 0;
        blast_heat[i] = (in[REC2_BLAST + i] >> 4) <= DEF_BLAST_OVERHEAT_TURNS ? in[REC2_BLAST + i] >> 4 : 0;
        blast_owed[i] = (unsigned short)(in[REC2_BLAST_OWED + 2 * i] | in[REC2_BLAST_OWED + 2 * i + 1] << 8);
    }
    blast_quest = in[REC2_BLAST_QUEST] <= DEF_BLAST_QUEST_DONE ? in[REC2_BLAST_QUEST] : 0;
    blast_purified = (unsigned short)(in[REC2_BLAST_QUEST + 1] | in[REC2_BLAST_QUEST + 2] << 8);
    chosen_partner = in[REC2_BLAST_QUEST + 3] <= DEF_BLAST_PARTNERS ? in[REC2_BLAST_QUEST + 3] : 0;
    chosen_charge = in[REC2_BLAST_QUEST + 4] ? 1 : 0;
    blast_boost = in[REC2_BLAST_QUEST + 5] <= DEF_BLAST_QUEST_FORMS ? in[REC2_BLAST_QUEST + 5] : 0;
    story_flags = (unsigned short)(in[REC2_BLAST_QUEST + 6] | in[REC2_BLAST_QUEST + 7] << 8);
    story_gate = in[REC2_BLAST_QUEST + 8];
    memcpy(story_answers, in + REC2_ANSWERS, sizeof(story_answers));
    def_super_record_read(in + REC2_SUPER);
    def_skin_record_read(in + REC2_SKIN);
    port_log("hunt: the slot's second record read");
}

int def_record_dri_unlocked(int c) {
    return c >= 0 && c < DEF_DRI_CHALLENGES && ((dri_unlocked >> c) & 1);
}

void def_record_dri_unlock(int c) {
    if (c >= 0 && c < DEF_DRI_CHALLENGES) {
        dri_unlocked = (unsigned short)(dri_unlocked | 1u << c);
    }
}

int def_record_dri_best(int c) {
    return c >= 0 && c < DEF_DRI_CHALLENGES ? (int)((dri_best >> (2 * c)) & 3) : 0;
}

void def_record_dri_raise_best(int c, int stage) {
    if (c < 0 || c >= DEF_DRI_CHALLENGES || stage <= def_record_dri_best(c)) {
        return;
    }
    stage = stage > 3 ? 3 : stage;
    dri_best = (dri_best & ~(3u << (2 * c))) | (unsigned int)stage << (2 * c);
}

int def_record_blast_next(int partner) {
    return partner >= 0 && partner < DEF_BLAST_PARTNERS ? blast_next[partner] : 0;
}

void def_record_blast_set_next(int partner, int step) {
    if (partner >= 0 && partner < DEF_BLAST_PARTNERS) {
        blast_next[partner] = (unsigned char)(step >= 0 && step < DEF_BLAST_STEPS ? step : 0);
    }
}

int def_record_blast_quest(void) {
    return blast_quest;
}

void def_record_blast_set_quest(int stage) {
    blast_quest = (unsigned char)(stage < 0 ? 0 : stage > DEF_BLAST_QUEST_DONE ? DEF_BLAST_QUEST_DONE : stage);
}

unsigned int def_record_blast_purified(void) {
    return blast_purified;
}

void def_record_blast_purify(int entry) {
    if (entry >= 0 && entry < DEF_BLAST_QUEST_FORMS) {
        blast_purified = (unsigned short)(blast_purified | 1u << entry);
    }
}

int def_record_chosen(void) {
    return (int)chosen_partner - 1;
}

void def_record_set_chosen(int partner) {
    chosen_partner = (unsigned char)(partner >= 0 && partner < DEF_BLAST_PARTNERS ? partner + 1 : 0);
}

int def_record_chosen_charge(void) {
    return chosen_charge;
}

void def_record_set_chosen_charge(int charge) {
    chosen_charge = (unsigned char)(charge != 0);
}

int def_record_heat(int partner) {
    return partner >= 0 && partner < DEF_BLAST_PARTNERS ? blast_heat[partner] : 0;
}

void def_record_set_heat(int partner, int turns) {
    if (partner >= 0 && partner < DEF_BLAST_PARTNERS) {
        blast_heat[partner] = (unsigned char)(turns < 0 ? 0 : turns > DEF_BLAST_OVERHEAT_TURNS ? DEF_BLAST_OVERHEAT_TURNS : turns);
    }
}

int def_record_blast_boost(void) {
    return blast_boost;
}

void def_record_set_blast_boost(int steps) {
    blast_boost = (unsigned char)(steps < 0 ? 0 : steps > DEF_BLAST_QUEST_FORMS ? DEF_BLAST_QUEST_FORMS : steps);
}

int def_record_story(int bit) {
    return bit >= 0 && bit < 16 && (story_flags >> bit & 1);
}

void def_record_set_story(int bit) {
    if (bit >= 0 && bit < 16) {
        story_flags = (unsigned short)(story_flags | 1u << bit);
    }
}

/* The Velvet Room's level its last new guest was greeted at: story_flags' bits 12..14 (the hooks' bits stay below). */
#define STORY_SEEN_SHIFT 12
int def_record_velvet_seen(void) {
    return story_flags >> STORY_SEEN_SHIFT & 7;
}

void def_record_set_velvet_seen(int level) {
    level = level < 0 ? 0 : level > 7 ? 7 : level;
    story_flags = (unsigned short)((story_flags & ~(7u << STORY_SEEN_SHIFT)) | (unsigned)level << STORY_SEEN_SHIFT);
}

int def_record_story_answer(int question) {
    return question >= 0 && question < DEF_STORY_ANSWERS ? story_answers[question >> 2] >> ((question & 3) * 2) & 3 : 0;
}

void def_record_set_story_answer(int question, int option) {
    if (question >= 0 && question < DEF_STORY_ANSWERS) {
        int shift = (question & 3) * 2;

        option = option < 0 ? 0 : option > 3 ? 3 : option;
        story_answers[question >> 2] = (unsigned char)((story_answers[question >> 2] & ~(3 << shift)) | option << shift);
    }
}

void def_record_set_story_gate(int stage) {
    story_gate = (unsigned char)stage;
}

int def_record_blast_count(void) {
    return blast_count;
}

const DefBlastEntry *def_record_blast_entry(int i) {
    return i >= 0 && i < blast_count ? &blast_entries[i] : NULL;
}

int def_record_blast_find(int partner, int step) {
    int i;

    for (i = 0; i < blast_count; i++) {
        if (blast_entries[i].partner == partner && blast_entries[i].step == step) {
            return i;
        }
    }
    return -1;
}

static int clamp(int v, int hi) {
    return v < 0 ? 0 : v > hi ? hi : v;
}

int def_record_blast_add(const DefBlastEntry *e) {
    DefBlastEntry *d;
    int j;

    if (e == NULL || blast_count >= DEF_BLAST_QUEST_FORMS || e->partner >= DEF_BLAST_PARTNERS ||
        e->step >= DEF_BLAST_STEPS || def_record_blast_find(e->partner, e->step) >= 0) {
        return -1;
    }
    d = &blast_entries[blast_count];
    *d = *e;
    for (j = 0; j < 5; j++) { /* what the third record can hold */
        d->stats[j] = (short)clamp(d->stats[j], 999);
    }
    d->hp = (short)clamp(d->hp, 0x3FFF);
    d->best = (unsigned char)(d->best < 5 ? d->best : 0);
    d->best_value = (short)clamp(d->best_value, 999);
    return blast_count++;
}

void def_record_blast_raise_best(int i, int value) {
    if (i >= 0 && i < blast_count && value > blast_entries[i].best_value) {
        blast_entries[i].best_value = (short)clamp(value, 999);
    }
}

/* The third record's bits, low bit first. */
static void put_bits(unsigned char *out, int *at, unsigned int v, int n) {
    int i;

    for (i = 0; i < n; i++, (*at)++) {
        if ((v >> i) & 1) {
            out[*at / 8] |= (unsigned char)(1 << (*at % 8));
        }
    }
}

static unsigned int get_bits(const unsigned char *in, int *at, int n) {
    unsigned int v = 0;
    int i;

    for (i = 0; i < n; i++, (*at)++) {
        v |= (unsigned int)((in[*at / 8] >> (*at % 8)) & 1) << i;
    }
    return v;
}

void def_record3_write(unsigned char *out) {
    int i, j, at;

    memset(out, 0, DEF_RECORD3_SIZE);
    if (blast_count == 0) {
        return; /* no dark form recorded: the card stays as it was */
    }
    memcpy(out, "BLST", 4);
    out[4] = DEF_RECORD3_VERSION;
    out[6] = (unsigned char)blast_count;
    for (i = 0; i < blast_count; i++) {
        const DefBlastEntry *e = &blast_entries[i];

        at = (REC3_ENTRIES + i * REC3_ENTRY) * 8;
        put_bits(out, &at, (unsigned int)(e->partner * DEF_BLAST_STEPS + e->step), 6);
        for (j = 0; j < 5; j++) {
            put_bits(out, &at, (unsigned int)e->stats[j], 10);
        }
        put_bits(out, &at, (unsigned int)e->hp, 14);
        put_bits(out, &at, e->best, 3);
        put_bits(out, &at, (unsigned int)e->best_value, 10);
    }
    out[5] = sum(out + 6, DEF_RECORD3_SIZE - 6);
    port_log("blast: the slot's third record written (%d forms)", blast_count);
}

void def_record3_read(const unsigned char *in) {
    int i, j, at, pair;

    memset(blast_entries, 0, sizeof(blast_entries));
    blast_count = 0;
    if (in == NULL || memcmp(in, "BLST", 4) != 0 || in[4] != DEF_RECORD3_VERSION ||
        in[5] != sum(in + 6, DEF_RECORD3_SIZE - 6)) {
        return;
    }
    for (i = 0; i < in[6] && i < DEF_BLAST_QUEST_FORMS; i++) {
        DefBlastEntry *e = &blast_entries[blast_count];

        at = (REC3_ENTRIES + i * REC3_ENTRY) * 8;
        pair = (int)get_bits(in, &at, 6);
        e->partner = (unsigned char)(pair / DEF_BLAST_STEPS);
        e->step = (unsigned char)(pair % DEF_BLAST_STEPS);
        for (j = 0; j < 5; j++) {
            e->stats[j] = (short)get_bits(in, &at, 10);
        }
        e->hp = (short)get_bits(in, &at, 14);
        e->best = (unsigned char)get_bits(in, &at, 3);
        e->best_value = (short)get_bits(in, &at, 10);
        if (e->partner < DEF_BLAST_PARTNERS && e->best < 5) {
            blast_count++;
        }
    }
    port_log("blast: the slot's third record read (%d forms)", blast_count);
}

unsigned char *def_record3_buffer(void) {
    return buffer3;
}

int def_record_blast_owed(int partner) {
    return partner >= 0 && partner < DEF_BLAST_PARTNERS ? blast_owed[partner] : 0;
}

void def_record_blast_owe(int partner, int tp) {
    int v;

    if (partner < 0 || partner >= DEF_BLAST_PARTNERS) {
        return;
    }
    v = blast_owed[partner] + tp;
    blast_owed[partner] = (unsigned short)(v < 0 ? 0 : v > 0xFFFF ? 0xFFFF : v);
}

unsigned char *def_record2_buffer(void) {
    return buffer2;
}

/* DW3_BESTIARY turns only this layer on in a script: a card test needs the record's extra steps without the combat
 * layers (DW3_DEFINITIVE), which change the battles the script's hashes cover. */
int def_bestiary_active(void) {
    return !port_script_active || getenv("DW3_DEFINITIVE") != NULL || getenv("DW3_BESTIARY") != NULL;
}

unsigned char *def_bestiary_buffer(void) {
    return buffer;
}
