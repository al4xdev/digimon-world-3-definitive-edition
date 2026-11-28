/* "Definitive" combat: the constants (every number is a constant, with
 * a line saying what it controls) and the hooks the game's C calls. Always on; no mod option, no port_mod_* flag.
 * The hooks' prototypes are in include/port.h (the game's units reach them through common.h). */
#ifndef PORT_DEFINITIVE_H
#define PORT_DEFINITIVE_H
#include <stddef.h>

/* The tunable numbers (the battle log has none). */
#define DEF_FAMILIES 5          /* element families on the grouped screens (Water+Ice, Thunder+Machine as one each) */
#define DEF_OVERFINISH_BITS_PCT 10 /* Bits earned per 100 damage dealt to an enemy already knocked out (OVERFINISH) */
#define DEF_OVERLEVEL_GAP 3        /* levels the party's best must be above the enemies' best for a random battle to offer the Overlevel Rush */
#define DEF_RUSH_TICKS 70          /* ticks (1/60 s) of the rush on the field before the battle's report opens */
#define DEF_RUSH_ASK_TICKS 300     /* ticks (1/60 s, 5 s) the Rush's question waits for an answer; none goes to the battle */
#define DEF_RUSH_WARN_SOUND 0x4001F /* WNG_STEP (COMMON seq 31, ~1.7 s with its tail): the question's warning */
#define DEF_RUSH_JINGLE 0x4000A     /* JINGLE01 (COMMON seq 10, ~2.9 s, plays once): the results' sound. NOT W_JINGLE
                                     * (0x6004001E): that one is the battle's looping victory theme and never ends */
#define DEF_WEAK_MAX 80         /* a family is a weakness while one of its values is at most this (modifiers included): the
                                 * table's weak 60, or a neutral 100..115 lowered by Resist Down (80) or Mega Break (100);
                                 * a resisted one lowered stays above it. No cap on the count */
#define DEF_BATON_PASS_MP 0     /* MP Baton Pass costs (offered only while the enemy is open; Persona's is free) */
#define DEF_MULT_ONE 1000       /* the fixed-point unit of every multiplier and affinity below (thousandths) */
#define DEF_MULT_WEAK 500       /* bonus for hitting a weakness */
#define DEF_MULT_STAGGER 200    /* bonus for hitting a target that is open (staggered) */
#define DEF_MULT_RESISTED 500   /* the whole multiplier against a resisted element, no bonuses */
#define DEF_AFF_SINGLE_MAX 400  /* affinity ceiling of Fire and of Wind (the player's resist 999) */
#define DEF_AFF_PAIR_MAX 800    /* of Water+Ice and of Thunder+Machine (both resists 999) */
#define DEF_AFF_DARK_MAX 800    /* of Dark (999) */
#define DEF_PLAYER_REF_RESIST 100 /* the resist the player counts as when an enemy's element hits it: the old formulas' scale */
#define DEF_RELAY_MP 50         /* MP the Relay technique (Mega Boost, remade) costs */
#define DEF_BREAK_SLOW_DELTA 80 /* how far Slow Down (remade) lowers the enemy's resist of the user's best element */
#define DEF_BREAK_MEGA_DELTA 100 /* how far Mega Break lowers each of the user's two best elements */
#define DEF_BREAK_TURNS 3       /* the enemy's turns a lowered resist lasts */
#define DEF_FINAL_HP_PCT 150 /* percent of the HP the disc gives each of Galacticmon's three phases */
#define DEF_BOSS_DEBUFF_PCT 100 /* strength of a stat debuff in a battle whose boss the original made immune to it */
#define DEF_BUFF_LV1_PCT 20      /* Tier +1 stat buff: +20% of base stat */
#define DEF_BUFF_LV2_PCT 35      /* Tier +2 stat buff: +35% of base stat */
#define DEF_BUFF_LV3_PCT 50      /* Tier +3 stat buff: +50% of base stat */
#define DEF_DEBUFF_LV1_PCT 15    /* Tier -1 stat debuff: -15% of base stat */
#define DEF_DEBUFF_LV2_PCT 25    /* Tier -2 stat debuff: -25% of base stat */
#define DEF_DEBUFF_LV3_PCT 35    /* Tier -3 stat debuff: -35% of base stat */
#define DEF_BOSS_STATUS_PCT 60    /* percent of a status' chance that remains on a boss that the original made immune */
#define DEF_GUARD_PCT 30          /* percent of the party's damage a guarded boss takes until a stagger opens it (definitive.c guard_damage) */
#define DEF_STAGGER_STATUS_BONUS 24 /* out of 128: flat bonus (after x1.5) when the target is staggered */
#define DEF_BOSS_STATUS_CAP 100    /* out of 128 (~78%): no status is surer than this on a boss, stagger or not */
#define DEF_POISON_WIND_DIV 6     /* Wind affinity (0..400) divided by this feeds poison damage */
#define DEF_POISON_PREC_DIV 4     /* Precision divided by this feeds poison damage */
#define DEF_POISON_SPIRIT_DIV 4   /* Spirit divided by this feeds poison damage */
#define DEF_SUPER_ITEM 26         /* Super Hard: the Demon Bell, a key item (a Japanese tag slot the US disc never gives) */
#define DEF_SUPER_HP_PCT 100      /* percent of an enemy's HP while the player holds the Bell (100: the halved
                                  * player damage already doubles it, and HP has a cap) */
#define DEF_SUPER_SPEED_PCT 150   /* ... and of its Speed (may pass 999: the enemy then acts twice) */
#define DEF_SUPER_REWARD_PCT 200  /* ... and of the Bits, XP and hunt bonuses a won battle pays */
#define DEF_SUPER_MAT_PCT 300     /* ... and of the common Hunt Materials a won battle pays */
#define DEF_SUPER_PLAYER_DMG_PCT 50 /* ... player direct damage % while holding the Bell (rewards Weak/One More/Baton Pass) */
#define DEF_SUPER_ENEMY_DMG_PCT 125 /* ... enemy direct damage % while holding the Bell (a broken chain costs more) */
#define DEF_POISON_ENEMY_PCT 8    /* percent of max HP: the most a tick takes from an enemy (a pure precision build ticks ~15% of a boss) */
#define DEF_SLEEP_REGEN_PCT 6     /* percent of max HP a sleeping enemy regenerates each turn */
#define DEF_STATUS_BOSS_TURNS 2   /* turns Sleep or Confusion hold an enemy */
#define DEF_CONFUSION_BUFF_LEVEL 2 /* Attack tier an enemy gains when its confusion ends */
#define DEF_REVIVE_T1_ITEM 67     /* Revive Disk: the first revive tier (item ID) */
#define DEF_REVIVE_T1_CHANCE 40   /* percent chance to revive a fallen ally, at or under its rated max HP */
#define DEF_REVIVE_T1_HP_PCT 30   /* percent of the ally's max HP it returns with */
#define DEF_REVIVE_T1_RATED_HP 1500 /* max HP the tier is rated for: above it the chance falls in proportion */
#define DEF_REVIVE_T1_BUY 2000    /* shop price, Bits */
#define DEF_REVIVE_T1_SELL 1000
#define DEF_REVIVE_T2_ITEM 70     /* Life Disk: the second revive tier (item ID) */
#define DEF_REVIVE_T2_CHANCE 65
#define DEF_REVIVE_T2_HP_PCT 60
#define DEF_REVIVE_T2_RATED_HP 4000
#define DEF_REVIVE_T2_BUY 10000
#define DEF_REVIVE_T2_SELL 5000
#define DEF_REVIVE_T3_ITEM 46     /* Max Charge: the third revive tier in battle (the menu still heals) */
#define DEF_REVIVE_T3_CHANCE 100
#define DEF_REVIVE_T3_HP_PCT 100
#define DEF_REVIVE_T3_RATED_HP 9999
#define DEF_REVIVE_T3_SELL 3000
#define DEF_REVIVE_MIN_CHANCE 15  /* percent: the chance never falls under this */
#define DEF_MOD_TURNS_SHORT 2    /* Duration (turns) of secondary-effect stat debuffs on attacks */
#define DEF_MOD_TURNS_BASE 3     /* Standard duration (turns) of dedicated stat buff/debuff skills & items */
#define DEF_MOD_TURNS_LONG 4     /* Extended duration (turns) for single-target specialist buff/debuff skills */
#define DEF_RESIST_MIN 250      /* resist value from which an element is "resisted" */
#define DEF_SND_EFFECTIVE 0x800450BD /* sound id of a hit on a weakness (the menus' accept chime) */
#define DEF_SND_NEUTRAL 0x4001C      /* of a neutral hit (the menus' confirm blip) */
#define DEF_SND_RESISTED 0x4001B     /* of a resisted hit (the menus' cancel blip) */

/* definitive_mask.c (pure): the elements `resists[0..6]` (Fire, Water, Ice, Wind, Thunder, Machine, Dark) are weak
 * to, as a bit mask (bit = element index), a family at a time: Water+Ice and Thunder+Machine are one affinity each, so a family is weak while either of its values is at most DEF_WEAK_MAX, and then all its bits
 * are set. Dark is a weakness only when `dark_weak` (the species' mark). */
int def_weakness_mask(const short resists[7], int dark_weak);
/* The elements of `e`'s family as a bit mask (Water or Ice: both; Fire: itself); 0 outside 0..6. */
int def_family_bits(int e);
int def_resisted(int resist); /* 1 from DEF_RESIST_MIN up */
/* The damage multiplier of an elemental technique of the player (thousandths, DEF_MULT_ONE = 1.0):
 * 1 + (weak ? 0.5) + (open ? 0.2) + (weak ? affinity) + (open ? affinity); resisted: DEF_MULT_RESISTED alone.
 * `affinity` from def_affinity: ceiling * (the attacker's own resist / 999)^2 (`resists[0..6]`, 0..999 each). */
int def_affinity(int element, const short resists[7]);
/* The same in a finer unit, for ranking families (def_affinity rounds to 0 at low stats). */
long long def_affinity_fine(int element, const short resists[7]);
int def_status_chance_calc(int value, int atk_prec, int tgt_prec, int resist_sum, int boss, int staggered);
int def_poison_dot_calc(int base_power, int wind_affinity, int precision, int spirit);
int def_poison_cap(int amount, int max_hp);
int def_revive_tier(int item, int *chance, int *hp_pct, int *rated);
int def_revive_chance(int chance, int rated, int max_hp);
int def_multiplier(int weak, int open, int resisted, int affinity);
int def_mod_pct(int level);
void def_mod_tech_spec(int tech, int kind, int amount, int *out_level, int *out_turns);
int def_mod_level(int side, int slot, int kind);
int def_mod_turns_left(int side, int slot, int kind);

/* Turns (definitive_mask.c, pure): stagger, the chain of extra actions and the free swap. Fed by definitive.c with the
 * game's events; tests/port/definitive_driver.c plays the acceptance sequences on it. */
typedef struct DefChain {
    unsigned char staggered[3];  /* per enemy slot: open */
    unsigned char window_used;   /* weakness families (their elements' bits) that already gave their extra action */
    unsigned char swap_used;     /* the free swap of this window is spent */
    unsigned char extra_pending; /* an extra action is owed: the next turn event is made immediate */
    unsigned char next_extra;    /* the next party turn is that extra action */
    unsigned char cur_extra;     /* the party turn now running is an extra action */
    unsigned char relay;         /* Relay was used: the next swap is free */
} DefChain;
void def_chain_reset(DefChain *c);
/* The player's action resolved on `target`: it `hit` (damage > 0); `elem` the element index of an elemental technique,
 * -1 for a physical blow or a crest's element (they never open); `weak` it hit a weakness. */
void def_chain_player_hit(DefChain *c, int target, int hit, int elem, int weak);
void def_chain_relay_set(DefChain *c);           /* the Relay technique: the next swap is free */
void def_chain_enemy_acts(DefChain *c, int slot); /* an enemy's turn begins: it closes */
void def_chain_party_turn(DefChain *c);           /* a party turn begins: an extra one, or a normal one (new window) */
int def_chain_swap_free(const DefChain *c, int target);
void def_chain_swap_chosen(DefChain *c, int target); /* the player chose Tag: free when allowed, else it costs the turn */
int def_chain_take_extra(DefChain *c);              /* about to take the next event: 1 if the party turn must go first */

/* The port's table of enemy species (definitive_enemies.c,
 * hand-maintained): the seven resists a species is given (SDIGIEDT is never edited: the record in memory is
 * overridden when the game asks for it) and its Dark mark (Dark is one of its weaknesses). */
typedef struct DefEnemyRow {
    short id; /* FightstgEnemyRecord.id */
    short resists[7];
    unsigned char dark; /* the Dark mark */
} DefEnemyRow;
extern const DefEnemyRow def_enemy_rows[];
extern const int def_enemy_row_count;

/* Text overrides (port/game/definitive_text.c, generated by tools/gen_text_override.py): entry `index` of the
 * text file whose Japanese file ID is `file` (the game adds records_language; for a container's sub-table `s`, `file`
 * holds `(s + 1) << 16 | jpn_id`), already encoded as glyph bytes. */
typedef struct DefTextOverride {
    int file;
    int index;
    const unsigned char *bytes;
} DefTextOverride;
/* The story rewrite (tools/text_override/<lang>_story.txt, _kaili.txt, _choices.txt, _echoes.txt,
 * generated into definitive_story_text.c, read by definitive_story.c), in every language of DEF_STORY_LANGS (def_lang()'s
 * order): sorted by file then entry, the file named as def_text_override matches it (a DMG container's table:
 * file | (table + 1) << 16). `hero` (Kaili mode): the balloon moves to the hero. A choice: the hero line `index` the
 * player picks among `count` (labels for the panel, lines for the balloon); its answer is kept as story answer
 * `question` (def_record_story_answer). What a language lacks (a NULL, an entry not in its table) is the English's. */
#define DEF_STORY_LANGS 6 /* English, Portuguese, French, German, Italian, Spanish */
typedef struct DefStoryLine {
    int file;
    int index;
    int hero;
    const unsigned char *bytes;
} DefStoryLine;
typedef struct DefStoryChoice {
    int file;
    int index;
    int question;
    int count;
    const unsigned char *labels[DEF_STORY_LANGS][3]; /* a language's three NULL while it is not translated */
    const unsigned char *lines[DEF_STORY_LANGS][3];
    signed char bond_ally[3]; /* -1: none, 0..4: Teddy, Ivy, Lisa, Keith, Kurt */
    signed char bond_delta[3];
    signed char rep_delta[3];
} DefStoryChoice;
extern const DefStoryLine *const def_story_lines[DEF_STORY_LANGS], *const def_kaili_lines[DEF_STORY_LANGS];
extern const int def_story_line_count[DEF_STORY_LANGS], def_kaili_line_count[DEF_STORY_LANGS];
extern const DefStoryChoice def_story_choices[];
extern const int def_story_choice_count;
/* A line that remembers an earlier answer (<lang>_echoes.txt): said in place of the entry's line while story answer
 * `question` is one of `answers` (bit k: option k). `mode` 0 in both heroes' games, 1 only without Girls Power, 2 only
 * with it. An entry's echoes are in the source file's order; the first that holds wins. */
typedef struct DefStoryEcho {
    int file;
    int index;
    int question;
    int answers;
    int mode;
    const unsigned char *bytes[DEF_STORY_LANGS];
} DefStoryEcho;
extern const DefStoryEcho def_story_echoes[];
extern const int def_story_echo_count;
const unsigned char *def_story_line(int file, int index); /* the rewrite's line for an entry, NULL: the game's */
extern const DefTextOverride def_text_overrides[];
extern const int def_text_override_count;
extern const DefTextOverride *def_text_overrides_of(int lang, int *count); /* lang 1 pt, 2 fr, 3 de, 4 it, 5 es */
void def_language_set(int lang); /* 0 en, 1 pt, 2 fr, 3 de, 4 it, 5 es */
int def_lang(void);
int def_is_ptbr(void);
/* The one of six texts in the language in use (en, pt, fr, de, it, es); a NULL falls back to English. */
static inline const char *def_pick(const char *en, const char *pt, const char *fr, const char *de, const char *it,
                                   const char *es) {
    const char *const t[6] = { en, pt, fr, de, it, es };
    int l = def_lang();

    return l > 0 && l < 6 && t[l] != NULL ? t[l] : en;
}
void def_language_kaili(int on); /* the port's own strings name Kail's friend Junior in the Kaili mode */

/* The rookies' MP (port/game/definitive_mp.c). The MP is the rookie's; the forms have none of their own. */
#define DEF_MP_START 140        /* starting MP and max MP of Kotemon, Kumamon, Monmon and Guilmon (Veemon keeps its 150) */
#define DEF_MP_START_VEEMON 150 /* Veemon's starting MP: already the disc's value, listed so the table is complete */
#define DEF_MP_GAIN 57          /* max MP gain base per level of Kotemon, Kumamon, Monmon and Veemon */
#define DEF_MP_GAIN_GUILMON 60  /* Guilmon's max MP gain base per level: already the disc's value */

/* One rookie's MP row: its records_digimon index (the gamestate_data.digimon index too), starting MP, gain base. */
typedef struct DefMpRow {
    unsigned char digimon;
    unsigned char start_mp;
    unsigned char gain;
} DefMpRow;
extern const DefMpRow def_mp_rows[];
extern const int def_mp_row_count;

/* Writes the rows into records_digimon (the boot patch; the src/ table stays as the matching build has it). */
void def_mp_apply(void);

/* ---- Progression and comfort (definitive_progression.c) ---- */
#define DEF_TP_GAIN_LV_2_20 7     /* TP a level gives, levels 2..20 */
#define DEF_TP_GAIN_LV_21_40 9    /* levels 21..40 */
#define DEF_TP_GAIN_LV_41_50 30   /* levels 41..50 */
#define DEF_TP_GAIN_LV_51_UP 0    /* after 50 only the Train Chip gives TP */
#define DEF_TP_CAP 99             /* the TP ceiling, the original's on purpose: no hoarding for the last gym */
int def_tp_gain(int level);
int def_tp_total(int level);
int def_tp_add(int tp, int level, int *lost);

#ifndef DEF_TRAIN_PITY_AFTER
#define DEF_TRAIN_PITY_AFTER 3 /* non-perfect trainings before the next training hits everything (0 disables) */
#endif
#ifndef DEF_TRAIN_PITY_BONUS
#define DEF_TRAIN_PITY_BONUS 1 /* when 1, the guaranteed training also hits its bonus round when eligible */
#endif
#define DEF_TRAIN_PITY_TIERS 3 /* 0: 1 TP, 1: 5 TP, 2: 10 TP */
int def_train_pity_count_tier(int tier);
int def_train_pity_guaranteed_tier(int tier);
void def_train_pity_record_tier(int tier, int perfect);
int def_train_pity_count(void);
int def_train_pity_guaranteed(void);
void def_train_pity_record(int perfect);
void def_train_pity_reset(void);

#define DEF_GAUGE_SLOTS 8            /* Digimon slots in records_state.gauges */
#define DEF_GAUGE_MAX 1000           /* maximum valid Blast Gauge value (0..1000) */
#define DEF_GAUGE_SLOT_OFFSET 0x26F0 /* tail offset inside a 0x2700 save slot (after GsRecord + DFV2 marker) */
int def_gauge_clamp(int val);
void def_gauges_encode(const short gauges[DEF_GAUGE_SLOTS], unsigned char out[DEF_GAUGE_SLOTS * 2]);
void def_gauges_decode(const unsigned char in[DEF_GAUGE_SLOTS * 2], short out[DEF_GAUGE_SLOTS]);

/* Blast Gauge ("the cursed evolution"): the gauge fills with risky offence instead of
 * with the damage taken, and the player sets the Blast off (the battle menu's Blast button, R2 by default) instead of
 * the full gauge doing it. What one party blow that hits adds (they add up): */
#define DEF_BLAST_WEAK 150      /* it struck a weakness (WEAK!) */
#define DEF_BLAST_STAGGER 100   /* its target was open (staggered) */
#define DEF_BLAST_OVERFINISH 200 /* its target was already down (OVERFINISH!) */
#define DEF_BLAST_EXTRA 100     /* it was an extra action (One More) */
/* and what a passive command takes away: an item, a try to run */
#define DEF_BLAST_COOL 100
/* The price: a Digimon that took its Blast form ends the battle with HP and MP 1 and loses this share of its Power,
 * Guard and Speed (the game's own GamestateStats.penalties, which only an inn's rest, inn_heal_party, clears; the
 * status screens already mark a penalised stat). */
#define DEF_BLAST_CURSE_PCT 25
/* The dark forms: the game's own (RecordsDigimon.blast_forms, five per partner, records_digimon
 * rows), taken in sequence: each Blast of a partner takes its next form when its level has reached that form's
 * (DEF_BLAST_STEP_LEVELS), else the last one it reached; after the fifth it starts over. Painted dark as they come in
 * (definitive_recolor.c's DEF_RECOLOR_DARK). */
#define DEF_BLAST_STEPS 5
#define DEF_BLAST_STEP_LEVELS { 1, 10, 25, 35, 40 }
/* The DV Plug (item 76), the cursed catalyst: one use fills the acting Digimon's gauge and sets the Blast off at once
 * (the game's gauge-full event). The Plug itself costs no TP: the Blast does. Every non-blessed Blast activation takes
 * this many TP (GamestateStats.values[1]; all it has when it has fewer), and R2 needs that much unless it is blessed. */
#define DEF_BLAST_PLUG_TP 10 /* the Blast's TP cost (the name is from when the Plug paid it) */
#define DEF_PLUG_ITEM 76 /* the DV Plug's item ID (ITMNAM/ITMINF 76) */
#define DEF_BLAST_TEMPT_HP_PCT 25 /* the acting Digimon's HP share at or under which a full gauge tempts */
int def_blast_step(int partner, int level, int *advance); /* pure: the step a Blast takes, and the sequence's next */
int def_blast_top_step(int level); /* pure: the highest step the level reaches (the last stand's Blast) */
int def_blast_gain(int weak, int staggered, int overfinish, int extra); /* pure: the sum above */
#define DEF_BLAST_COMMAND DEF_BLAST_COMMAND_ID /* include/port.h: the battle menu's result and FightstgCommand.command */

#define DEF_EVO_FORM_MAX 512            /* form ID bitset ceiling in RAM (all playable + blast forms) */
#define DEF_EVO_SEEN_WORDS (DEF_EVO_FORM_MAX / 32)
#define DEF_EVO_SKIP_HOLD_FRAMES 4      /* frames Circle or Cross must be held to skip digivolve scene */
void def_evo_seen_reset(unsigned int bits[DEF_EVO_SEEN_WORDS]);
int def_evo_seen_get(const unsigned int bits[DEF_EVO_SEEN_WORDS], int form_id);
void def_evo_seen_set(unsigned int bits[DEF_EVO_SEEN_WORDS], int form_id);
int def_evo_skip_update(int *hold_frames, int skip_pressed);

#ifndef DEF_TRAIN_NO_STAT_LOSS
#define DEF_TRAIN_NO_STAT_LOSS 1 /* resistance trainings (8..14) do not lower secondary stats 1..5 */
#endif
int def_train_no_stat_loss_on(void);

/* ---- Former mods, now the game's own (party_xp, xp_boost) ---- */
#define DEF_XP_CURVE 10          /* every partner levels on this curve (Kumamon's, the highest of the eight:
                                   * Monmon 6, Guilmon/Renamon 7, Kotemon/Veemon 8, Agumon/Patamon 9, Kumamon 10).
                                   * The form's own level thresholds are unchanged. */
#define DEF_XP_PARTY_SHARE 100   /* % of one fighter's share that a member who sat out (not knocked out) receives */
#define DEF_XP_FORM_MULT 14      /* multiplier of a won battle's form (digivolution) experience; the clamp is skipped */
#define DEF_XP_ALL_FORMS 1       /* the three chosen forms of every party member earn form experience, fought or not;
                                  * a form left in the lab earns nothing */
#define DEF_CARD_BOOST_PACKS 15  /* Booster Packs a battle's Booster Pack drop gives */

/* Digivolution floors: the original paced Perfect and Mega by the slow form levels (one form level a battle), which the
 * x14 form experience removes. So a form's stage sets the Digimon's own level it needs, whatever its learn list says:
 * Perfect (Ultimate) DEF_EVO_FLOOR_PERFECT, Mega and its variations DEF_EVO_FLOOR_MEGA; Adult forms keep the list's own
 * rules. `row` is a learn list's technique (records_digimon row, 1-based); 0 for a form with no floor. The stages are
 * a reading of the franchise, checked once more by a second pair of eyes (Armormon and GrapLeomon are
 * Perfect, MetalGarurumon is Mega). */
#define DEF_EVO_FLOOR_PERFECT 20
#define DEF_EVO_FLOOR_MEGA 40
int def_form_floor(int row);

#define DEF_EVO_HINT_DEPTH 1     /* Digi-Lab Next Forms: forms shown one step ahead; 0 turns it off, 99 shows every form */

/* The hit callouts (definitive_callout.c): the slanted word that pops over the side hit. def_callout_show starts one
 * (0 clears it) over `side`'s acting member (0 party, 1 enemy); def_callout_frame runs once per battle frame. */
enum { DEF_CALLOUT_NONE, DEF_CALLOUT_WEAK, DEF_CALLOUT_RESIST, DEF_CALLOUT_OVERFINISH, DEF_CALLOUT_RARE, DEF_CALLOUT_EVOLVE,
       DEF_CALLOUT_GUARDED, DEF_CALLOUT_SHIFT, DEF_CALLOUT_GUARDED_RED, DEF_CALLOUT_COUNT };
void def_callout_show(int kind, int side);
void def_callout_frame(void);

/* The regional hunt chain and the Rare Target (definitive_hunt.c; the table is
 * definitive_hunt_targets.c, hand-maintained). */
#define DEF_HUNT_CHAIN_MIN 5    /* won random encounters in a row in one region with no Rare Target chance yet */
#define DEF_HUNT_CHAIN_SURE 10  /* from this many the next encounter is the Rare Target; past the minimum the chance rises
                                 * evenly to it (20% at 6 wins, 40% at 7, ..., certain at 10) */
#define DEF_HUNT_CHAIN_CAP 255  /* the chain stops counting here (one byte of the card record) */
#define DEF_HUNT_HP_MAX 30000   /* a raised Rare Target's HP never passes this (RecordsEnemy.hp is an s16) */
/* The Rare Target's rewards. */
#define DEF_HUNT_XP_MULT 10           /* XP: this many times the random encounter the Rare Target replaced */
#define DEF_HUNT_MATERIALS 16         /* common Hunt Materials: one u8 counter each in the card record (REC_EXTRA + 10..25) */
#define DEF_HUNT_MATERIAL_CAP 99      /* the most of one material the player can hold */
#define DEF_HUNT_PACK_FIRST 0x186     /* the rare Booster (R-Booster 01..05, items 0x186..0x18A) a Rare Target drops... */
#define DEF_HUNT_PACK_LEVELS 15       /* ...one more per this many levels from level 25 (01 below 25, 05 from 70) */
#define DEF_HUNT_PACKS 1              /* how many of it (a plain Booster drop gives DEF_CARD_BOOST_PACKS) */
#define DEF_HUNT_COMMON_PCT 25        /* % chance each enemy of a won random encounter leaves its common material */
#define DEF_HUNT_RARES 128            /* rare items the second card record has room for (one per region with a target) */
#define DEF_HUNT_RARE_CAP 99          /* the most of one rare item the player can hold */
#define DEF_HUNT_LINES 4              /* material lines one results screen lists: three commons and a rare at most */
#define DEF_HUNT_LINE_SIZE 72         /* bytes of such a line, or of a Rare Target's name, in glyphs */
#define DEF_HUNT_RARE_HP_MULT 4 /* a Rare Target's HP: its source's, scaled to its level, times this */
typedef struct DefHuntTarget {
    unsigned char region; /* ststatus_map_areas's byte: the world map area, bit 7 the server */
    short battle;         /* an existing battle: an index into fieldstg_battles */
    unsigned char slot;   /* the enemy of that battle that is the Rare Target: it fights alone */
    short level;          /* the level it fights at, above or below its own */
    unsigned char variant; /* its rare item's numeral: the region's rank, by level, among those whose target
                            * is the same species; 0 when no other region's is (no numeral) */
} DefHuntTarget;
extern const DefHuntTarget def_hunt_targets[];
extern const int def_hunt_target_count;
extern const unsigned char def_hunt_commons[]; /* each bestiary species' common material (generated; 0xFF: none) */
/* The % chance of the Rare Target for a region's chain (0 under the minimum). */
int def_hunt_chance(int chain);
/* The same for a chain in region `region`: sooner in an active Super Rare hunt's territory (DEF_SUPER_RARE_CHAIN_*). */
int def_hunt_chance_in(int chain, int region);
/* A common Hunt Material's name (glyph bytes, 0-ended). */
const unsigned char *def_hunt_material_name(int material);
/* The bestiary's Hunt line: bestiary species `index`'s common material (-1: none), and of the rare items of
 * the targets species `species` leads, how many were ever held and how many there are. */
int def_hunt_common_of(int index);
void def_hunt_species_rares(int species, int *seen, int *total);
/* In a Rare Target's battle, species `species`'s name with the region's numeral ("Datamon IV"), in buffer `slot` (0, 1:
 * the windows keep the pointer); NULL outside one, or when the name is not loaded. */
const unsigned char *def_hunt_foe_name(int species, int slot);
/* A species' name in the game's glyphs (its record's line of text 0x4E: definitive.c), NULL when not loaded. */
const unsigned char *def_species_name(int species);
/* A player form's record by its ID, the DNA stubs past records_find_digimon's 0x34 included; NULL: none. */
struct RecordsDigimon *def_form_record(int id);
/* The results screen of a Rare Target won (STFGTREP): the battle whose XP it pays DEF_HUNT_XP_MULT times
 * (-1: none pending), the rare Booster it drops when the battle left no item (0: none), 1 while a material line (the
 * commons of any won encounter, a Rare Target's rare item) has not been shown, the next one (glyph bytes, marks it
 * shown), and the end of the screen. */
int def_hunt_reward_battle(void);
int def_hunt_reward_pack(void);
/* The screen gives that Booster (returns it, as def_hunt_reward_pack), and how many packs an item the screen shows is:
 * DEF_HUNT_PACKS for the Booster it gave, 0 for any other item (the definitive experience's count stays theirs). */
int def_hunt_take_pack(void);
int def_hunt_pack_count(int item);
int def_hunt_material_pending(void);
const unsigned char *def_hunt_material_text(void);
void def_hunt_reward_done(void);
/* The hooks the game's C calls (also declared in include/port.h, for the units that do not include this header). */
void def_hunt_map(int map);
int def_hunt_encounter(int battle);
void def_hunt_scale(void);
int def_hunt_active(void);
int def_hunt_region_fighting(void); /* the Rare Target's region being fought, -1 none */
void def_hunt_skipped(void);
/* The battle's end (def_log_end): a won random encounter adds to the chain and leaves its commons; a Rare Target fought
 * resets it, and won gives its rare item. The Rush's skip (def_hunt_skipped) counts for the chain and leaves commons. */
void def_hunt_battle_end(int result);
/* The Guardromon teleport resets the chain, whatever region it lands in. */
void def_hunt_chain_reset(void);
/* Whether the party's escape from the battle is decided here: 1 it succeeds, 0 it fails, -1 the game's own rule. A Rare
 * Target is always fled, except in the territory of an active Super Rare hunt (DEF_SUPER_RARE_FLEE_PCT); a Super Rare
 * always. */
int def_hunt_flee(void);
struct PortState;
void def_hunt_savestate(struct PortState *s);

/* The Super Rare hunts (definitive_superhunt.c). Thirteen hunts, in order: the
 * Sepikmon of the Velvet Room starts the next one or repeats a finished one. Hunt 0 is the tutorial (DEF_SUPER_TUTORIAL:
 * a level 3 target in one area by Asuka City, given at the Velvet Room's first visit); the twelve Super Rares follow. The target sits in a map of its territory (a sector
 * of one server, def_hunt_sector_of), the compass tells how close the player is, and stepping on its point starts the
 * battle. Having lost DEF_SUPER_FLEE_PCT of its full HP in one battle it flees to another map of its territory; it keeps
 * the HP it lost, and comes back DEF_SUPER_BUFF_PCT stronger each time. Won: its crystal. */
#define DEF_SUPER_COUNT 13
#define DEF_SUPER_TUTORIAL 0         /* the tutorial hunt's index */
#define DEF_HUNT_AREAS 0x2E          /* the world map areas of a server (ststatus_map_areas & 0x7F) */
#define DEF_HUNT_SECTORS 5           /* North, East, Center, West, South (hand-made) */
#define DEF_SUPER_FLEE_PCT 25        /* of its full HP lost in one battle: it flees */
/* The hits a Super Rare takes (but the tutorial's) do this percent of their damage: the battle's HP is an s16, so its bar
 * cannot be doubled (the rows already hold 15000 to 30000); halving the damage is the same, and each 25% takes twice as
 * long to take off (the flee-and-chase rhythm is the hunt). */
#define DEF_SUPER_DAMAGE_PCT 50
/* The blessed Blast: the tutorial hunt's battle, and the first Blast of the whole game wherever it comes (a player who
 * beats the Ogremon without one still has it for his first hard fight). Igor's DV Plug used for it costs no TP, the form
 * carries this much more boost, and a Super Rare no longer flees from that battle once it is set off. */
#define DEF_SUPER_TUTORIAL_BOOST_PCT 50
#define DEF_SUPER_BUFF_PCT 20        /* its stats, each battle after the first: +this % of the base, cumulative */
#define DEF_SUPER_MEETS_CAP 10       /* the buff stops growing here (+200%) */
#define DEF_SUPER_CRYSTAL_CAP 99
#define DEF_SUPER_RARE_CHAIN_MIN 1   /* in the hunt's territory a Rare Target may come from 2 wins... */
#define DEF_SUPER_RARE_CHAIN_SURE 6  /* ...20% more each win, certain at 6 */
#define DEF_SUPER_RARE_FLEE_PCT 50   /* and the party's escape from it succeeds this often */
#define DEF_SUPER_POINT_RADIUS 24    /* pixels: this close to the point, the battle starts */
#define DEF_SUPER_COMPASS_REACH 3    /* exits out of the territory the compass still shows */
#define DEF_SUPER_POINTS_MAX 256     /* candidate points the field gives for a map (encounter tiles on a grid) */
typedef struct DefSuperRare {
    short battle;          /* an existing battle (fieldstg_battles) */
    unsigned char slot;    /* its enemy that is the Super Rare: it fights alone */
    unsigned char level;   /* recommended level shown by the Sepikmon, and the level it fights at */
    short hp;              /* its full HP */
    signed char sector;    /* its territory: a sector (0..DEF_HUNT_SECTORS-1) */
    unsigned char server;  /* of this server (0 Asuka, 1 Amaterasu: ststatus_map_areas's bit 7) */
    signed char area;      /* -1: the whole sector; else only this world map area of it (the tutorial's) */
    short species;         /* the target's species (its name on the Sepikmon's list) */
    unsigned short price;  /* its crystal's price at the Black Market, in Bits */
} DefSuperRare;
extern const DefSuperRare def_super_rares[DEF_SUPER_COUNT];
extern const signed char def_hunt_sector_of[DEF_HUNT_AREAS];
/* The map graph (definitive_map_graph.c, generated by tools/gen_map_graph.py from the stages' exits): node i is map
 * def_map_nodes[i]; its neighbours are def_map_edges[def_map_first[i] .. def_map_first[i + 1] - 1] (node indices);
 * def_map_wild[i]: the map has random encounters (a Super Rare can sit there). */
extern const unsigned short def_map_nodes[];
extern const unsigned short def_map_first[];
extern const unsigned short def_map_edges[];
extern const unsigned char def_map_wild[];
extern const int def_map_node_count;
int def_map_node(int map);                  /* the node of a map, -1: not in the graph */
int def_map_distance(int from, int to);     /* maps apart (exits walked), -1: no path */
/* The compass's reading (DEF_TEMP_*): none outside the hunt's territory or with no hunt. */
enum { DEF_TEMP_NONE, DEF_TEMP_COLD, DEF_TEMP_COOL, DEF_TEMP_WARM, DEF_TEMP_HOT, DEF_TEMP_BURNING, DEF_TEMP_COUNT };
/* The hunts' state: the active one (-1: none), how many were finished in order (the next one to unlock), a hunt's state
 * for the Sepikmon's list (DEF_SUPER_LOCKED, _OPEN: the next, _DONE: finished, can be repeated, _ACTIVE). */
enum { DEF_SUPER_LOCKED, DEF_SUPER_OPEN, DEF_SUPER_DONE, DEF_SUPER_ACTIVE };
int def_super_active(void);
int def_super_done(void);
int def_super_state(int hunt);
int def_super_start(int hunt);              /* 1: started (an open or done hunt, none active) */
void def_super_cancel(void);                /* the active hunt ends; the target heals */
int def_super_map(void);                    /* the target's map (0: no hunt) */
int def_super_hp_left(void);
int def_super_meets(void);
int def_super_crystals(int hunt);
void def_super_crystal_add(int hunt, int n);
int def_super_won_take(int *first);         /* the hunt just won (once), -1: none: the field's reward panel; *first: it
                                              * was its first win (a skin unlocked) */
int def_super_crystal_sell(int hunt);       /* one crystal sold: its price (0: none held); the caller pays it */
/* The story's flags (saved): the Velvet Room's first visit (Seiryu City pulls the player in once, or the Sepikmon met
 * before), and the tutorial hunt given. */
#define DEF_SUPER_FLAG_MET 1
#define DEF_SUPER_FLAG_TUTORIAL 2
int def_super_flags(void);
void def_super_flag_set(int flag);
/* Whether map `map`, or region `region` (ststatus_map_areas's byte), is in the active hunt's territory. */
int def_super_in_territory(int map);
int def_super_territory_distance(int hunt, int map); /* exits to the hunt's territory, -1: none */
int def_super_in_compass_zone(int map);              /* the territory and DEF_SUPER_COMPASS_REACH maps round it */
int def_super_in_region(int region);
/* The compass for the player on map `map` at pixel (x, y). */
int def_super_temperature(int map, int x, int y);
int def_super_proximity(int map, int x, int y); /* 0..100 precision on the target's map once placed, -1 elsewhere */
/* Why a Super Rare battle ended in escape (0): target fled or player fled, reported once back on the field. */
#define DEF_SUPER_FLED_NONE 0
#define DEF_SUPER_FLED_TARGET 1
#define DEF_SUPER_FLED_PLAYER 2
void def_super_flee_note(int who);
int def_super_fled_take(void);
const unsigned char *def_super_talk_text(void);
/* The hooks: a field step of the player on map `map` at pixel (x, y): 1 the Super Rare's battle starts now (battle
 * def_super_battle()), 2 the target is on this map but its point is not taken yet (the field gives the candidates,
 * then asks again), 0 nothing; the battle's set up (one enemy, its level, HP left and buffs), whether it flees now (its
 * HP in the battle), and the battle's end (`hp`: the target's HP at the end; `result` def_log_end's: 1 won). */
int def_super_step(int map, int x, int y);
int def_super_battle(void);
int def_super_battle_ready(void);           /* the battle being set up is the Super Rare's */
int def_super_fighting(void);               /* the Super Rare hunt being fought, -1 none */
void def_super_scale(void);
/* The battle's members are set up: its HP at the start, given its full HP in the battle's units (max_hp: Super Hard's
 * raise included); the flight and the end then count in those units. */
int def_super_hp_start(int max_hp);
int def_super_tutorial_battle(void); /* 1 while the tutorial hunt's battle is the one being fought */
void def_super_blast_note(void);     /* the blessed Blast was set off in a Super Rare's battle: the target stops fleeing */
int def_super_damage(int side, int damage);
int def_super_should_flee(int hp_now);
void def_super_battle_end(int result, int hp);
/* The candidate points of the target's map (x, y pairs): the field gives the walkable encounter tiles, the player
 * being at (x, y), just come in; the one two thirds of the way from there by distance is taken. */
void def_super_set_points(const short *xy, int count, int x, int y);
/* The save: the hunts' bytes in the second record (definitive_bestiary.c, DEF_SUPER_RECORD bytes at its end). */
#define DEF_SUPER_RECORD 24
void def_super_record_write(unsigned char *out);
void def_super_record_read(const unsigned char *in);
int def_super_record_empty(void);
void def_super_reset(void);
void def_super_savestate(struct PortState *s);
/* Sepikmon's board, the compass and Seiryu City's call to the Velvet Room (definitive_superhunt_screen.c). */
void def_superhunt_board_offer(void);       /* his line is over: the board opens on the next idle frame */
/* Bits the Velvet Room\'s pig asks to move stat points: the price climbs with each swap of the save,
 * ten steps, the last one for good (definitive_respec_screen.c, definitive_respec_save.c). */
#define DEF_RESPEC_STEPS 10
#define DEF_RESPEC_PRICES { 1000, 5000, 10000, 20000, 30000, 45000, 60000, 75000, 90000, 100000 }
int def_respec_price(void);
int def_respec_swaps(void);
void def_respec_swaps_add(void);
#define DEF_RESPEC_POINTS 100   /* raw points one swap moves from one stat to another of the same partner */
void def_respec_offer(void);    /* the pig's line is over: his board opens on the field's next frame */
int def_respec_frame(void);     /* the field's idle step: 1 while his board owns the field */
void def_respec_savestate(struct PortState *s);
/* Etemon's jukebox (definitive_midi_screen.c, definitive_midi.c): the player's MIDI files as the music of the random
 * battles. */
void def_midi_offer(void);   /* Etemon's line is over: his list opens on the field's next frame */
int def_midi_frame(void);    /* the field's idle step: 1 while his list owns the field */
void def_midi_savestate(struct PortState *s);
void def_midi_dir(char *out, unsigned long size); /* the folder the files are in (DW3_MIDI_DIR, or `midi` beside the settings) */
int def_midi_scan(void);                   /* the folder's playable files into the list: their count */
const char *def_midi_name(int i);
int def_midi_was_cut(int i);               /* the piece is longer than a sound bank's room: its end is cut */
int def_midi_chosen(void);                 /* the chosen file's index in the list, -1: the game's own music */
void def_midi_choose(int i);               /* -1: the game's own music */
int def_midi_battle_music(int music);
int def_superhunt_frame(void);              /* the field's idle step: 1 while the board or a panel owns the field */
void def_superhunt_screen_savestate(struct PortState *s);
/* The rare foes' palette swap (definitive_recolor.c): the kinds, and its save-state part (the hooks are in port.h). */
enum { DEF_RECOLOR_NONE, DEF_RECOLOR_RARE, DEF_RECOLOR_SUPER, DEF_RECOLOR_DARK, DEF_RECOLOR_BLACK, DEF_RECOLOR_GOLD,
       DEF_RECOLOR_PERSONA, DEF_RECOLOR_SILHOUETTE, DEF_RECOLOR_KINDS };
void def_recolor_enemy_hue(int degrees); /* an enemy persona's phase colour (definitive_persona.h HUES) */
int def_persona_model_hue(void);         /* the colour of the enemy model being added, 0 none (definitive.c) */
void def_recolor_savestate(struct PortState *s);
/* The hero's skins (definitive_skins.c): a colour recipe for the hero's field sprites, each hunt won unlocks one. */
#define DEF_SKIN_COUNT 13   /* one per hunt (DEF_SUPER_COUNT) */
#define DEF_SKIN_RECORD 1   /* the card's bytes: the hero's skin */
void def_skin_reset(void);
int def_skin_unlocked(int skin);             /* 0 (the own colours) always; skin n once n hunts were won */
int def_skin_hero(void);                     /* the skin chosen */
int def_skin_set_hero(int skin);             /* 0: locked or out of range */
int def_skin_worn(void);                     /* the skin the sprites wear now (0: none, a replay script, locked) */
/* A 16-colour CLUT through skin `skin`: `hair` and `skin_mask` are bit masks of CLUT indices (girls_power.c's GpWho). */
void def_skin_clut(unsigned short clut[16], int skin, unsigned short hair, unsigned short skin_mask);
int def_skin_record_empty(void);
void def_skin_record_write(unsigned char *out);
void def_skin_record_read(const unsigned char *in);
void def_skin_savestate(struct PortState *s);
extern int def_superhunt_debug;            /* tests: 1 opens the board on the next idle field frame, anywhere */

/* The definitive prices (definitive_economy.c; the table is definitive_prices.c, generated by
 * tools/gen_item_prices.py from records_items and the shops' goods). */
#define DEF_PRICE_KNEE 10000    /* a shop's weapon, armour or accessory from this buying price up is "advanced"... */
#define DEF_PRICE_DRAIN_PCT 150 /* ...and costs this % of it, so the Bits of OVERFINISH have somewhere to go */
#define DEF_SELL_BASE 5000      /* its resale: this, */
#define DEF_SELL_ABOVE_PCT 8    /* plus this % of its old price above the knee (a 60000 sword: 9000, not 30000) */
typedef struct DefItemPrice {
    short item; /* the item's ID (records_get_item) */
    int buy;    /* its buying price in a shop */
    int sell;   /* its selling price */
} DefItemPrice;
extern const DefItemPrice def_item_prices[];
extern const int def_item_price_count;
/* An item's buying and selling price in a shop (STITSHOP): the table's, else records_items' (always in a replay
 * script); 0 for no item. */
int def_item_buy(int item);
int def_item_sell(int item);
/* The forge (definitive_economy.c; the recipes are definitive_recipes.c, generated by
 * tools/gen_recipes.py). Every weapon, armour and accessory a shop sells asks for Hunt Materials of the hunting regions
 * around that shop (the vendor's own region), so one item can ask for different ones in two
 * towns. The game reads only the table; the generator reads these: */
#define DEF_RECIPE_REACH 4          /* the shop's regions: those up to this many map exits away, on its own server */
#define DEF_RECIPE_OVER 6           /* ...whose level is at most this over the level the shop's dearest item is bought at */
#define DEF_RECIPE_RARE_FROM 9500   /* from this old price a region's rare item too (below: a common only) */
#define DEF_RECIPE_TWO_RARES 50000  /* from this one, two of it */
typedef struct DefRecipe {
    unsigned char shop;     /* the shop (STITSHOP's map entry, stitshop_shops' index) */
    signed char common;     /* its common Hunt Material (0..DEF_HUNT_MATERIALS-1) */
    short item;             /* the item's ID */
    unsigned char common_n; /* how many */
    unsigned char rare_n;   /* how many of the rare */
    short rare;             /* its rare item (an index of def_hunt_targets), -1: none */
} DefRecipe;
extern const DefRecipe def_recipes[];
extern const int def_recipe_count;
/* The shop the next calls are about (STITSHOP sets it as it opens). Then how many of `item` the held materials make (99
 * without a recipe, or in a replay script), whether one, taking them for `count`, the requirements' line (glyph bytes,
 * NULL without a recipe; `palette` 3 while short, else 0) and the shop's refusal. */
void def_recipe_set_shop(int shop);
struct PortState;
void def_recipe_savestate(struct PortState *s);
int def_recipe_max(int item);
int def_recipe_can_buy(int item);
void def_recipe_pay(int item, int count);
const unsigned char *def_recipe_label(int item, int *palette);
const unsigned char *def_recipe_missing_text(void);
/* Glyph strings (definitive_glyph.c): append a string, a count (0..99) or a roman numeral at buf[*p]. */
void def_glyph_put(unsigned char *buf, int *p, int size, const unsigned char *s);
void def_glyph_count(unsigned char *buf, int *p, int size, int n);
void def_glyph_roman(unsigned char *buf, int *p, int size, int n);
/* Rare item `rare`'s Rare Target as the battle names it, "Datamon IV" (definitive_hunt.c). */
void def_hunt_target_name(unsigned char *buf, int *p, int size, int rare);

/* The DRI Agents' Gauntlet (definitive_dri.c the rules, definitive_dri_screen.c the screen and the
 * battle; the table is definitive_dri_table.c, hand-maintained). A challenge is one ordinary
 * battle that runs an evolution line's four stages, Rookie to Mega (four duels from the Rookie): the
 * first three are the battle's enemy members, and when one falls the battle brings the next in at its own full HP while
 * the party keeps its HP, MP and spent items (wfightmn_knockout); when the third falls, the Mega comes in as a wave in a
 * fallen member's slot (def_enemy_next_wave), so the battle goes on. The n-th stage felled past the Rookie reaches tier
 * n of the line's Synergy accessory (the game's elemental "Power" accessories), each tier given the first time;
 * unlocking a challenge hands over Hunt Materials once, for good. Kept in the slot's second card record. */
#define DEF_DRI_CHALLENGES 16    /* challenges the second card record has room for: an unlocked bit and a 2-bit best tier each */
#define DEF_DRI_STAGES 4         /* a challenge's stages: Rookie, Champion, Ultimate, Mega */
#define DEF_DRI_MEMBERS 3        /* the first stages, the battle's enemy members; the rest come in as waves */
#define DEF_DRI_TIERS 3          /* reward tiers: the stages past the Rookie (the card keeps the best tier, 0..3, in 2 bits) */
#define DEF_DRI_LEVEL_FIRST 24   /* the easiest challenge's Champion fights at this level (its Rookie a step under)... */
#define DEF_DRI_LEVEL_GAP 4      /* ...each next one's in difficulty this many higher (16: the last Mega at 90) */
#define DEF_DRI_LEVEL_STEP 3     /* within a challenge, each stage fights this many levels above the one before */
#define DEF_DRI_SCALE_EXP_PCT 80 /* past its species' own levels a stage's HP and stat_scale grow as (level ratio)^(this/100) */
#define DEF_DRI_COMMON_LO 15     /* unlock: the first challenge's common Hunt Material, how many... */
#define DEF_DRI_COMMON_HI 40     /* ...rising to this for the last */
#define DEF_DRI_RARE_OVER 5      /* unlock: its rare items are Rare Targets' of at most this many levels above its Champion */
#define DEF_DRI_TWO_RARES 2      /* from this rank on (difficulty order) the unlock takes two rare items, one before */
#define DEF_DRI_STAGE 19         /* the simulations' battle scenery (records_state.stage: the game's boss battles') */
#define DEF_DRI_MUSIC 0x60880000 /* and their music (the boss battles') */
#define DEF_DRI_DEBUG_MENU 100   /* def_dri_debug's value that opens the list as a DRI Agent would (tests) */
#define DEF_DRI_DEBUG_AGENT 102  /* ... that sets the player 24 pixels under the map's first DRI Agent (tests: UP, then talk) */
typedef struct DefDriStage {
    short digimon;    /* the species (FightstgEnemyRecord.id) */
    short level;
    short hp;
    short mp;
    short stat_scale; /* / 16, as RecordsEnemy's */
    short base;       /* 0, or: `digimon` is a player form (records_digimon, no enemy record on the disc) and this enemy
                       * species' record lends it its stats, resistances and battle plan (def_dri_record) */
} DefDriStage;
typedef struct DefDriChallenge {
    DefDriStage stages[DEF_DRI_STAGES]; /* in order: Rookie, Champion, Ultimate (the enemy members), Mega (a wave) */
    short battle;                       /* the battle it is set up as: an existing boss battle (kind, immunities, XP, Bits) */
    short rewards[DEF_DRI_TIERS];       /* the Synergy accessory of each tier (item IDs): def_dri_tier */
    signed char common;                 /* unlock: a common Hunt Material (0..DEF_HUNT_MATERIALS-1), -1: none */
    unsigned char common_n;             /* how many */
    short rare[2];                      /* and rare items (indices of def_hunt_targets), -1: none */
    unsigned char rare_n[2];
} DefDriChallenge;
#define DEF_DRI_PER_AGENT 2      /* challenges each DRI Agent offers (no agent holds a pile of them) */
typedef struct DefDriAgent {
    short map;   /* the field map, -1: any */
    short actor; /* FieldstgPlacedActor.id, the talked-to actor's base.key1 */
    signed char challenges[DEF_DRI_PER_AGENT]; /* the ones he offers (def_dri_challenges indices), easier first */
} DefDriAgent;
extern const DefDriChallenge def_dri_challenges[];
extern const int def_dri_challenge_count;
extern const DefDriAgent def_dri_agents[];
extern const int def_dri_agent_count;
/* The rules (definitive_dri.c; under def_on() 0 there is no challenge): challenge `c` (NULL outside the table), the tier
 * `fallen` stages reach (the Rookie's reaches none), the stages a best tier stands for, whether `map`'s actor `actor` is a DRI Agent, whether its materials are there (and it is still
 * locked), unlocking it (takes them; 0 when it cannot), the accessories `fallen` stages newly earn (tiers above the best,
 * written to `items`; the best rises, never falls) and the unlock's line (glyph bytes; `palette` 3 while short). */
const DefDriChallenge *def_dri_challenge(int c);
int def_dri_tier(int fallen);
int def_dri_tier_stages(int tier);
int def_dri_agent(int map, int actor);
int def_dri_agent_index(int map, int actor); /* which of def_dri_agents, -1: none */
int def_dri_can_unlock(int c);
int def_dri_unlock(int c);
int def_dri_award(int c, int fallen, short items[DEF_DRI_TIERS]);
const unsigned char *def_dri_cost_label(int c, int *palette);
/* The Tamers the simulations replay (classic characters, the old Digimon tamers, as rivals), one per
 * challenge in the table's order: its title, its line
 * before the fight, after a full clear and after less (glyph bytes, $DriRival<n>*); NULL past them or in a replay. */
#define DEF_DRI_RIVALS 16      /* Tamers with texts */
#define DEF_DRI_RIVAL_NAME 0
#define DEF_DRI_RIVAL_HELLO 1
#define DEF_DRI_RIVAL_WON 2
#define DEF_DRI_RIVAL_LOST 3
const unsigned char *def_dri_rival(int c, int what);
/* The game's side (definitive_dri_screen.c; the ones the game's C calls are in include/port.h too): a talk (1: a DRI
 * Agent whose DDNA quest is done says the network's line and asks; 0: the game's own talk runs), FIELDSTG's idle frame (1 while the screen owns the field), the
 * battle's begin and end
 * (def_log_begin, def_log_end), a stage's entrance (wfightmn_knockout), a lost battle (1 when it was a simulation: back to
 * the field, the party restored), the save state, and the tests' hook. */
int def_dri_talk(int map, int actor);
int def_dri_frame(void);
void def_dri_battle_begin(void);
int def_dri_fighting(void);
int def_dri_challenge_fighting(void); /* the challenge being fought, -1 none */
int def_dri_challenge_pending(void);  /* the challenge set up for the battle about to begin, -1 none */
void def_dri_battle_end(int result);
void def_dri_stage_enters(int member);
int def_dri_next_wave(void); /* the Mega: set up in a fallen enemy slot (its index), -1: none (def_enemy_next_wave) */
/* Waves: the battle's last enemy member fell; a mode with more to come set a fallen enemy
 * slot up again as the next one (its index: wfightmn_knockout brings it in with the DRI's entrance), -1: the battle ends
 * as usual. */
int def_enemy_next_wave(void);
int def_dri_defeat(void);
extern int def_dri_debug;
struct PortState;
void def_dri_savestate(struct PortState *s);
/* The Black Market (definitive_market.c: the rules; definitive_market_screen.c: the trader and its
 * screen; the replicas are definitive_replicas.c, generated by tools/gen_replicas.py). */
#define DEF_REPLICAS 32        /* Legendary Replicas: their bits in the card record (REC_EXTRA + 26..29, def_replicas' index) */
#define DEF_BM_COMMONS 12      /* commons one rare item buys at the Exchange; it happens only if all of them fit the cap */
#define DEF_BM_REPLICA_RARES 2 /* rare items a replica takes: one each of its own Rare Targets (the generator's pick) */
#define DEF_BM_REPLICA_BITS 20 /* a replica's Bits: this many times the original's selling price (2000: 40000) */
#define DEF_BM_ITEM_CAP 99     /* the most of one item the bag holds (gamestate_change_item's cap) */
typedef struct DefReplica {
    short item;                        /* the item's ID: equipment no shop sells */
    short cores[DEF_BM_REPLICA_RARES]; /* the rare items it takes, one of each (indices of def_hunt_targets) */
    int bits;                          /* its price at the Black Market */
} DefReplica;
extern const DefReplica def_replicas[];
extern const int def_replica_count;
/* What an exchange or a purchase did, or why it was refused. */
enum { DEF_BM_OK, DEF_BM_OFF, DEF_BM_NO_RARE, DEF_BM_NO_ROOM, DEF_BM_NO_CORES, DEF_BM_NO_BITS, DEF_BM_BAG_FULL, DEF_BM_OWNED };
/* The Hunter's Mark: a key item (27, a Japanese tag slot the US disc never gives, as the Demon Bell is
 * 26) the Black Market sells for DEF_HUNT_MARK_BITS and DEF_HUNT_MARK_RARES different rare items: holding it, a Rare
 * Target leaves 2x its item and a Super Rare 2x its crystals. It is the replicas' list's last row: index def_replica_count
 * stands for it in the replica calls below. */
#define DEF_HUNT_MARK_ITEM 27
#define DEF_HUNT_MARK_BITS 50000
#define DEF_HUNT_MARK_RARES DEF_BM_REPLICA_RARES
int def_hunt_mark_on(void);
int def_hunt_mark_mult(int n); /* n, doubled while the Mark is held */
int def_bm_replica_rows(void); /* the replicas' list: the replicas and the Mark */
int def_bm_replica_item(int r);
int def_bm_replica_bits(int r);
int def_bm_replica_owned(int r); /* a replica's bit, or the Mark held */
/* The Exchange: one of rare item `rare` (an index of def_hunt_targets) for DEF_BM_COMMONS of common `common`. */
int def_bm_exchange_check(int rare, int common);
int def_bm_exchange(int rare, int common);
/* Replica `r` (an index of def_replicas): the rare items it takes (its own, one of each; 0 while one is not held),
 * whether it can be bought, and buying it. */
int def_bm_replica_pick(int r, int picks[DEF_BM_REPLICA_RARES]);
int def_bm_replica_check(int r, int picks[DEF_BM_REPLICA_RARES]);
int def_bm_replica_buy(int r);
/* An item's replica index (-1: none), how many of it the player holds (bag and equipped), 1 while a replica of it is
 * held (def_item_sell then pays 0), and the field's per-frame pass that clears the bit of a replica no longer held. */
int def_bm_replica_of(int item);
int def_bm_held(int item);
int def_bm_resale_zero(int item);
void def_bm_sweep(void);
/* Rare item `rare`'s boss: its Rare Target's lead species, the battle's first enemy (definitive_hunt.c). */
int def_hunt_target_lead(int rare);

/* The equipment variants, shop items converted in place (definitive_variants.c; the table is
 * definitive_equipment.c, generated by tools/gen_equipment.py: the game reads only DEF_EQUIP_ON below, the generator
 * the rest). A variant is built against its tier's best plain item of its slot: */
#define DEF_EQUIP_ON 1            /* 0: no variant is written (their names and descriptions go with them) */
#define DEF_EQUIP_SPEED_COST 50   /* Speed: % of the tier's best weapon power it gives up for Boost */
#define DEF_EQUIP_ELEM_COST 50    /* Elemental: % of the tier's best head guard it gives up for its family's resists */
#define DEF_EQUIP_ELEM_MAX 400    /* Elemental: the most it adds to one element's resist */
#define DEF_EQUIP_TANK_GUARD 50   /* Tank: % of guard above the tier's best body armour or shield */
#define DEF_EQUIP_TANK_SPIRIT 25  /* Tank: Spirit, as % of that best guard; a Boost penalty pays for both */
/* The budget: a stat's worth in hundredths of a Power point (a variant never scores above its tier's best plain item) */
#define DEF_EQUIP_W_GUARD 100     /* Guard: Power's mirror in the damage formula */
#define DEF_EQUIP_W_SPIRIT 100    /* Spirit: the magic side's Power and Guard */
#define DEF_EQUIP_W_WISDOM 50     /* Wisdom: heals and the magic hit only */
#define DEF_EQUIP_W_BOOST 100     /* Boost: the turn rate (actions per enemy action = own / enemy Boost, up to 2) */
#define DEF_EQUIP_W_HIT 25        /* accuracy or evasion: a point is a Boost point for the hit roll alone */
#define DEF_EQUIP_W_ELEMENT 5     /* a resist point: affinity +0.3 per 999 on WEAK! and open hits only (Dark twice) */
enum { DEF_EQUIP_SPEED, DEF_EQUIP_ELEMENT, DEF_EQUIP_TANK, DEF_EQUIP_TRADE };
typedef struct DefEquipVariant {
    short item;                  /* the item's ID: a weapon or armour a shop sells */
    unsigned char kind;          /* DEF_EQUIP_SPEED, _ELEMENT, _TANK or _TRADE */
    unsigned char weapon;        /* 1: RecordsWeapon (power, accuracy); 0: RecordsArmor (guard, evasion); 2:
                                  * RecordsAccessory (main and hit unused; the second stat is its penalty) */
    unsigned short main;         /* the new power or guard */
    unsigned char hit;           /* the new accuracy or evasion */
    unsigned char stats[2];      /* the new bonus stats (RecordsBonusStat, 0: none) */
    short values[2];             /* and their values */
} DefEquipVariant;
extern const DefEquipVariant def_equip_variants[];
extern const int def_equip_variant_count;
/* Writes the variants into the items' data, every time (absolute values: a second call changes nothing, and the
 * reset's restore of the table is undone at the next call); nothing in a replay script, so its items keep the disc's
 * data. Called where def_mp_hook is (a new game, a battle's start) and at every field map, so a loaded save has them
 * before its first shop or status screen. */
void def_variants_hook(void);
/* An accessory's second stat (a DEF_EQUIP_TRADE variant's penalty: RecordsAccessory has one bonus, so the variant
 * keeps the second in its unused unk_09 bytes, the stat then the signed value): 1 and the pair when `data` has one,
 * added where the game adds the accessory's own bonus (gamestate_get_stats, the shop's preview). */
int def_accessory_penalty(const void *data, short *stat, short *value);
/* 1 when text entry `index` of file `file` (a Japanese file ID, as def_text_overrides has it) is a variant's name or
 * description that must not show: a replay script, or DEF_EQUIP_ON 0 (def_text_override then gives the disc's). */
#define DEF_TEXT_ITMNAM 0x6A      /* the item names' file (records_language + 0x6A), JPN ID */
#define DEF_TEXT_ITMINF 0x63      /* the item descriptions' file (records_language + 0x63), JPN ID */
int def_variant_text_hidden(int file, int index);

/* Append a whole number (0 and up, every digit) at buf[*p] (definitive_glyph.c). */
void def_glyph_number(unsigned char *buf, int *p, int size, int n);

/* The Overlevel Rush's results on the field, without the battle's report (definitive_rush.c, pure: the
 * panel's rows and clock; definitive_rush_panel.c: the panel and the switch; def_rush_apply_quiet in
 * src/stfgtrep/stfgtrep_80082E70.c: the report's rewards, by its own routines and in its order). DW3_RUSH_FLOAT (read
 * once: 0 the report, else the panel) overrides DEF_RUSH_FLOAT for a test; def_rush_float_mode is that switch. */
#define DEF_RUSH_FLOAT 1          /* a taken Rush gives its results at once in a panel on the field, not on the report */
#define DEF_RUSH_FLOAT_FRAMES 175 /* how long that panel stays up, in ticks (1/60 s): DEF_RUSH_JINGLE's ~2.9 s */
#define DEF_RUSH_ROWS 10          /* rows the panel lists at most (the rest is given, not listed) */
typedef struct DefRushMember {
    short digimon;              /* its gamestate_data.digimon index; -1: no member in this party slot */
    short level;                /* its level after the rewards */
    int exp;                    /* the experience it got (the bonus item and the party's share counted) */
    unsigned char levels;       /* levels it went up */
    unsigned char learnt;       /* techniques and forms the report would have announced */
    unsigned char tp_full;      /* a level's TP did not fit under the ceiling (the report's "TP full: train!") */
    const unsigned char *name;  /* its name, glyph bytes (its record's) */
} DefRushMember;
typedef struct DefRushSummary {
    DefRushMember members[3];   /* by party slot */
    int bits;                   /* the Bits given (before the wallet's ceiling) */
    short item;                 /* the item given, 0: none */
    short item_count;           /* how many of it (Booster Packs: DEF_CARD_BOOST_PACKS) */
    int materials;              /* Hunt Material lines below */
    unsigned char material[DEF_HUNT_LINES][DEF_HUNT_LINE_SIZE]; /* as the report would have shown them (glyph bytes) */
    int random;                 /* pad_random's index before the rewards (logged: the in-game test lines the report's
                                 * level-ups up with it) */
} DefRushSummary;
enum { DEF_RUSH_ROW_MEMBER, DEF_RUSH_ROW_TECH, DEF_RUSH_ROW_TP, DEF_RUSH_ROW_BITS, DEF_RUSH_ROW_ITEM, DEF_RUSH_ROW_MATERIAL };
typedef struct DefRushRow {
    signed char kind;           /* DEF_RUSH_ROW_* */
    signed char index;          /* the party slot (member, tech, TP rows) or the material line */
} DefRushRow;
/* The panel's rows for `s`, at most `max`; their count. */
int def_rush_rows(const DefRushSummary *s, DefRushRow *rows, int max);
/* A row's two texts (glyph bytes, `size` each): the left one, and the right one the panel aligns on its right edge
 * (empty when the row has none). `item_name` is the item's name, NULL while the item names' file is not in ("???"). */
void def_rush_row_text(const DefRushSummary *s, DefRushRow row, const unsigned char *item_name, unsigned char *left,
                       unsigned char *right, int size);
/* A results screen's material line as one panel line: what follows its first line break, its control codes dropped. */
void def_rush_material_text(unsigned char *buf, int *p, int size, const unsigned char *line);
/* The panel's clock: a Rush's results open it for DEF_RUSH_FLOAT_FRAMES (one still up is replaced, never stacked:
 * returns 1 then); `ticks` more of the field leave it up while time remains, and close it at once when `held` (the
 * field holds the player: a menu, an event, a battle's start, the map's end), so it never sits over another screen. */
typedef struct DefRushClock {
    int left;                   /* ticks it has left, 0: no panel up */
    int opened;                 /* panels opened so far (the in-game test watches it) */
} DefRushClock;
int def_rush_clock_open(DefRushClock *c);
int def_rush_clock_tick(DefRushClock *c, int ticks, int held);
/* The game side (also declared in include/port.h): the switch; the report's rewards given without it, into `out`; the
 * panel opened, run once a frame (fieldstg_rush_frame) and forgotten at a map's load (that heap went with the stage). */
extern int def_rush_float_mode;
int def_rush_float(void);
void def_rush_apply_quiet(DefRushSummary *out);
void def_rush_panel_open(const DefRushSummary *s);
void def_rush_panel_frame(void);
void def_rush_panel_reset(void);
int def_rush_panel_up(void); /* 1 while the results' panel is up (the Guide's notices wait for it: same place) */
/* The Rush's question at the top of the screen, `waited` of its `total` ticks gone; closed when it is answered. */
void def_rush_ask_frame(int waited, int total);
void def_rush_ask_close(void);
struct PortState;
void def_rush_savestate(struct PortState *s);
void def_map_banner_open(short map);
void def_map_banner_frame(void);
void def_map_banner_reset(void);
int def_map_banner_up(void);
void def_map_banner_savestate(struct PortState *s);
/* The battle's escape asks first (definitive_flee_ask.c): on (not in a replay script), one frame of the question (0
 * open, 1 run, -1 back to the menu; it closes itself on an answer), close, a new battle's reset, the save state. */
int def_flee_ask_on(void);
int def_flee_ask_frame(void);
void def_flee_ask_close(void);
void def_flee_ask_reset(void);
void def_flee_ask_savestate(struct PortState *s);

void def_battle_prompt_reset(void);
int def_battle_prompt_check(void);
int def_battle_prompt_frame(void);
void def_battle_prompt_savestate(struct PortState *s);

/* The Guide (definitive_quest.c: the rules; definitive_quest_screen.c: the screen and the field's notices). The field
 * menu gets a GUIDE entry: the main quest's current step, the missions still open and the Cargo Tower's auctions. */
#define DEF_QUEST_NO_FLAG 0xFFFF  /* a quest row that asks for no game flag beside its story steps */
#define DEF_QUEST_MISSIONS 5      /* rows of def_missions: what the Guide's list holds under the main quest */
#define DEF_QUEST_AUCTIONS 16     /* the Cargo Tower's auctions (fieldstg_flag_events) */
#define DEF_AUCTION_FLAG 0x400    /* auction N's "attended" flag is this + N (set by either answer) */
#define DEF_GUIDE_TRAVEL_MAP 0x213 /* where the Guide's travel lands: El Dorado, at the stage's own start position */
#define DEF_GUIDE_LAST_MAP 0x2D7  /* maps from here on (the real world, Gunslinger, the seabed) offer no travel */
#define DEF_NOTICE_TICKS 300      /* how long a notice stays on the field, in 1/60 s */
#define DEF_NOTICE_FADE 20        /* of which this slides it in, and out */
typedef struct DefQuest {
    unsigned char first, last; /* the story steps (gamestate_data.progress) it is shown at, inclusive */
    unsigned short flag;       /* a game flag it also asks for (gamestate_flags.get_flag), or DEF_QUEST_NO_FLAG */
    unsigned char value;       /* the value that flag must have */
    unsigned short map;        /* where it sends the player (shown by the game's own map title), 0: nowhere */
    const unsigned char *title, *text;
} DefQuest;
typedef struct DefAuction {
    unsigned char progress; /* the one story step it runs at */
    unsigned short item;    /* what the winning bid gives */
    unsigned char money;    /* its price's index in gamestate_money_check */
    const unsigned char *when;
} DefAuction;
enum { DEF_AUCTION_SOON, DEF_AUCTION_OPEN, DEF_AUCTION_DONE, DEF_AUCTION_MISSED };
enum { DEF_NOTICE_NONE, DEF_NOTICE_QUEST, DEF_NOTICE_AUCTION };
extern const DefQuest def_quests[];
extern const int def_quest_count;
extern const DefQuest def_missions[DEF_QUEST_MISSIONS];
extern const DefAuction def_auctions[DEF_QUEST_AUCTIONS];
int def_guide_active(void);
int def_quest_main_index(void);
const DefQuest *def_quest_main(void);
int def_quest_missions(const DefQuest **out, int max);
int def_auction_state(int a);
int def_auction_open(void);
int def_auction_price(int a);
int def_guide_can_travel(int a, int map);
int def_quest_notice(int *arg);
void def_quest_notice_reset(void);
/* The screen: the field menu's GUIDE entry asks for it (def_guide_request) and closes; def_guide_frame runs it from
 * FIELDSTG's idle step (fieldstg_manager_update) and returns 1 while it owns the field. The notices: def_notice_frame
 * runs once a frame from the camera's update, def_notice_reset when a map's load took the field's heap. */
void def_guide_request(void);
int def_guide_frame(void);
void def_notice_frame(void);
void def_notice_reset(void);
struct PortState;
void def_guide_savestate(struct PortState *s);
void def_quest_savestate(struct PortState *s);
/* The Black Market's screen (definitive_market_screen.c) and the Velvet Room (definitive_velvet.c) in a save state. */
void def_bm_savestate(struct PortState *s);
void dw3_velvet_savestate(struct PortState *s);
/* The other screens and overlays with state outside the game's RAM: the Guardromon's (definitive_travel.c), the
 * evolution hint (definitive_evohint.c) and the battle callout (definitive_callout.c). */
void def_travel_savestate(struct PortState *s);
void def_evohint_savestate(struct PortState *s);
void def_callout_savestate(struct PortState *s);

#endif /* PORT_DEFINITIVE_H */

/* Battle memory and the bestiary (port/game/definitive_bestiary.c). One byte per species, bit = element index. */
#define DEF_BESTIARY_SPECIES 193      /* species in SDIGIEDT (file 0x1CF): the table's size */
#define DEF_BESTIARY_ELEMENTS 7       /* elements in a byte of the table (bits 0..6) */
#define DEF_BESTIARY_SLOTS 3          /* the game's save slots: one record each */
#define DEF_BESTIARY_RECORD_SIZE 0x100 /* bytes of a slot's record on the card (two 0x80 card frames) */
#define DEF_BESTIARY_RECORD_AT 0x7800 /* file offset of slot 0's record: the card file is 0x8000, the game uses up to here */
#define DEF_BESTIARY_PART 5           /* memcard_read/_write part of the record; the slot rides in part >> 8 */
#define DEF_BESTIARY_VERSION 1        /* the record's layout version; an unknown one reads as an empty table */
#define DEF_BESTIARY_ROWS 6           /* species listed per page on the bestiary screen */

void def_bestiary_reset(void);
/* The slot's other port state, kept in the same card record: the Guardromon network's unlocked nodes (a bit each,
 * game_mods.c's table) and the save's flags. Reset with the table (a new game), read and written with it. */
#define DEF_SAVE_HARD 1 /* the save is a Hard Mode one: the party is two Digimon */
#define DEF_SAVE_KAILI 2 /* the save's hero is Kaili (Girls Power, port/game/girls_power.c); without it, Junior */
unsigned int def_network_mask(void);
void def_network_unlock(int node);
int def_save_flag(int flag);
void def_save_flag_set(int flag, int on);
/* The hunt's chain and the region it counts in, kept in the same record (REC_EXTRA + 8, + 9). */
void def_record_hunt(int *region, int *chain);
void def_record_set_hunt(int region, int chain);
/* The Hunt Materials held, same record (REC_EXTRA + 10..25): the count of one, and adding to it
 * (clamped to 0..DEF_HUNT_MATERIAL_CAP). */
int def_record_material(int material);
void def_record_material_add(int material, int n);
/* The rare items held, in the slot's second record: the count of one (its index in def_hunt_targets),
 * adding to it (clamped to 0..DEF_HUNT_RARE_CAP; a gain also marks it as ever held), and whether it ever was. */
int def_record_rare(int rare);
void def_record_rare_add(int rare, int n);
int def_record_rare_seen(int rare);
/* The Legendary Replicas held, a bit each in the first record (REC_EXTRA + 26..27): replica `r`'s
 * (an index of def_replicas), and setting or clearing it. */
int def_record_replica(int r);
void def_record_replica_set(int r, int on);
void def_bestiary_discover(int index, int element_mask);
int def_bestiary_known(int index);
int def_bestiary_index(int species_id);
struct PortState;
void def_bestiary_savestate(struct PortState *s);
/* The card record of a slot (the host test drives these two): `out`/`in` are DEF_BESTIARY_RECORD_SIZE bytes. An empty
 * table writes all zeros (a card nothing was discovered on stays byte-identical); a missing, wrongly versioned or
 * badly summed record (or NULL) reads as an empty table. */
void def_bestiary_record_write(unsigned char *out);
void def_bestiary_record_read(const unsigned char *in);
/* The slot's second record (memcard.c part DEF_RECORD2_PART at DEF_RECORD2_AT + slot * 0x100, the card
 * file's free area after the first three): the rare items. The first record's rules: nothing to keep writes all
 * zeros, a missing or damaged one reads as empty. Read after the first, whose read resets the slot's port state. */
#define DEF_RECORD2_SIZE 0x100
#define DEF_RECORD2_PART 6
#define DEF_RECORD2_AT 0x7B00
#define DEF_RECORD2_VERSION 1
void def_record2_write(unsigned char *out);
void def_record2_read(const unsigned char *in);
unsigned char *def_record2_buffer(void);
/* In the same second record: whether DRI challenge `c` is unlocked, unlocking it, its best stage (0..3) and
 * raising it (it never falls). */
int def_record_dri_unlocked(int c);
void def_record_dri_unlock(int c);
int def_record_dri_best(int c);
void def_record_dri_raise_best(int c, int stage);
/* The Blast Gauge, in the same second record: each partner's next step in its sequence of
 * dark forms, the TP the Blast took from each partner (0..DEF_BLAST_PARTNERS-1) that the quest gives back (adding a
 * negative amount pays it back), the quest's stage, its forms purified (a bit per entry), the Chosen Evolution's
 * partner (-1: none) and its charge. */
#define DEF_BLAST_PARTNERS 8 /* the partners (gamestate_data.digimon) */
#define DEF_BLAST_QUEST_FORMS 10 /* the quest: this many partner+form pairs taken */
enum { DEF_BLAST_QUEST_DECEIVED, DEF_BLAST_QUEST_REVEALED, DEF_BLAST_QUEST_DONE }; /* its stages */
int def_record_blast_next(int partner);
void def_record_blast_set_next(int partner, int step);
int def_record_blast_owed(int partner);
void def_record_blast_owe(int partner, int tp);
int def_record_blast_quest(void);
void def_record_blast_set_quest(int stage);
unsigned int def_record_blast_purified(void);
void def_record_blast_purify(int entry);
int def_record_chosen(void);
void def_record_set_chosen(int partner);
int def_record_chosen_charge(void);
void def_record_set_chosen_charge(int charge);
/* The Blast's boost: a Digimon in its Blast form fights with its stats raised by the steps bought from
 * the false Gennai times DEF_BLAST_BOOST_STEP %; a step goes on sale with each new form recorded (up to the quest's
 * ten, so +30%), the k-th for k * DEF_BLAST_BOOST_BITS. The Chosen Evolution has DEF_CHOSEN_BOOST_PCT from the start.
 * When either form ends the Digimon is back in its Rookie form, sealed from digivolving for DEF_BLAST_OVERHEAT_TURNS of
 * its own turns. */
#define DEF_BLAST_BOOST_STEP 3
#define DEF_BLAST_BOOST_BITS 1000
#define DEF_CHOSEN_BOOST_PCT 50
#define DEF_BLAST_OVERHEAT_TURNS 3
int def_record_blast_boost(void); /* the steps bought, 0..DEF_BLAST_QUEST_FORMS */
/* The overheat's sealed own turns left, by Digimon (0..DEF_BLAST_OVERHEAT_TURNS), saved with the record: it follows the
 * partner into the next battles, each of its own turns counts it down, an inn (def_blast_cool_party) or Gennai clears it. */
int def_record_heat(int partner);
void def_record_set_heat(int partner, int turns);
void def_blast_heat_begin(void);
void def_blast_cool_party(void);
void def_record_set_blast_boost(int steps);
int def_blast_boost_pct(void);    /* the Blast form's boost now, % */
/* The Velvet Room's story hooks, in the same second record: a bit per hook seen or done
 * and the gatekeeper's stage. */
enum {
    DEF_STORY_IGOR,          /* the opening: Igor gave the key */
    DEF_STORY_SEIRYU_CALL,   /* step 4: the Leader is missing, Junior felt something */
    DEF_STORY_SEIRYU_DONE,   /* the Ogremon hunt reported to the Sepikmon: the Leader is back */
    DEF_STORY_OIKAWA_BEATEN, /* step 26: the Oikawa fought on the way to Amaterasu */
    DEF_STORY_MARKET_OPEN,   /* the Oikawa behind the counter opened the Black Market */
    DEF_STORY_GATE_WARNED,   /* step 40: the warning before the warp gate was given */
    DEF_STORY_GATE_PASSED,   /* the three trials were won: the warp gate opens */
    DEF_STORY_SEIRYU_ROOM,   /* step 4: Igor showed the room again, the Sepikmon took his place */
    DEF_STORY_BLAST_FIRST,   /* the first Blast of the game was set off (it is the blessed one: see DEF_SUPER_TUTORIAL_BOOST_PCT) */
    DEF_STORY_FINALE_DONE,   /* Extended Final Act (Dark Galacticmon + Super Chosen Evolution) completed */
    DEF_STORY_IGOR_GONE      /* Post-credits Blue Room farewell played: Igor left, Pipimon took his place */
};
/* The Velvet Room's level (definitive_story.c): who is in the room, rising with the story. 0 the doors are closed; 1
 * (Repeating Tom's news, step 4) the Sepikmon; 2 Gennai and Igor; 3 the Demon Bell's keeper; 4 Etemon and the pig;
 * DEF_VELVET_LEVEL_MAX (the Oikawa beaten) the Black Market. The level the room last greeted a guest at is kept too. */
#define DEF_VELVET_LEVEL_MAX 5
int def_story_velvet_level(void);
int def_record_velvet_seen(void);
void def_record_set_velvet_seen(int level);
int def_record_story(int bit);
void def_record_set_story(int bit);
/* The story's answers, in the order it asks them (Igor's dreams): 0 not asked yet, else the option taken (1..). */
#define DEF_STORY_ANSWERS 160 /* legacy memory-card slot count (0..159), mirrored with expansion save */
#define DEF_STORY_ANSWER_BYTES (DEF_STORY_ANSWERS / 4) /* two bits an answer: what the record has room for */
#define DEF_STORY_EXT_ANSWERS 512 /* expanded narrative save: 0..511 questions (0 = ignored/unspoken, 1..3 = choice) */
#define DEF_STORY_BONDS 5         /* 0 Teddy, 1 Ivy, 2 Lisa, 3 Keith, 4 Kurt */
#define DEF_STORY_NPC_SLOTS 128
enum {
    DEF_BOND_TEDDY = 0,
    DEF_BOND_IVY = 1,
    DEF_BOND_LISA = 2,
    DEF_BOND_KEITH = 3,
    DEF_BOND_KURT = 4,
};
int def_story_save_answer(int question);
void def_story_save_set_answer(int question, int option);
void def_story_save_import_legacy(const unsigned char *legacy, int bytes);
void def_story_save_export_legacy(unsigned char *legacy, int bytes);
int def_story_bond(int ally);
void def_story_bond_add(int ally, int delta);
int def_story_rep(void);
void def_story_rep_add(int delta);
int def_story_npc_seen_stage(int npc_id);
void def_story_npc_mark_seen(int npc_id, int stage);
int def_record_story_answer(int question);
void def_record_set_story_answer(int question, int option);
void def_record_set_story_gate(int stage);
/* The hooks themselves (definitive_story.c): the Guide's conditions on them (a quest row's flag STORY(cond)), the talk
 * entries they own and the disc lines they change, the Sepikmon's line, and the field's frame. */
enum {
    DEF_STORY_COND_SEIRYU = 1, /* step 4: the Leader is missing, the Ogremon hunt not reported */
    DEF_STORY_COND_SEIRYU_ASK, /* step 4, before Repeating Tom's news: ask around Seiryu City */
    DEF_STORY_COND_MARKET,     /* the Oikawa beaten, the Black Market not opened yet: to the Velvet Room */
    DEF_STORY_COND_OIKAWA,     /* step 27, the Oikawa not beaten: he bars the Bug Maze Pit's hall */
    DEF_STORY_COND_TIME        /* step 40, the Time Room's trials not passed: the blue door above the Master Room's stairs */
};
#define DEF_STORY_FLAG 0xF000 /* a quest row's flag at or past it is a story condition (game flags stay below) */
int def_story_cond(int cond);
int def_story_text_own(int base_id, int index);
const unsigned char *def_story_text(int file, int index);
int def_story_sepik_line(int met);
int def_story_igor_line(void); /* Igor's line as he is talked to (definitive_velvet.c), and when it closed */
void def_story_igor_done(void);
void def_field_freeze_player(void); /* the player stops dead under a port talk (speed 0, standing) */
int def_story_room_intro(void); /* 1: the next entry through a door is Seiryu's, Igor alone shows the room again */
int def_story_event_closed(int type, int param); /* 1: a story gate holds this map event shut */
struct PortState;
void def_story_savestate(struct PortState *s);

/* Native item-pickup sparkle/aura burst around the protagonist (fieldstg actor effect 0x16, step 3). */
void def_field_item_sparkle(void);

/* Limit Break (> 999 stats): scoped in v1.0 to Chosen Evolution (side 0) and Dark Galacticmon (side 1). */
int def_limit_break_active(int side);
const unsigned char *def_limit_break_label(void);

/* Extended Final Act (Dark Galacticmon + Last Velvet Room + Blue Room Bonds + Super Chosen Evolution) and
 * Post-Credits Farewell (Igor -> Pipimon as DNA oracle), implemented in port/game/definitive_finale.c. */
enum {
    DEF_FINALE_NONE = 0,
    DEF_FINALE_PHASE1_CLASSIC = 1,
    DEF_FINALE_PHASE2_DARK_LOSS = 2,
    DEF_FINALE_PHASE3_VELVET = 3,
    DEF_FINALE_PHASE3_BLUE_ROOM = 4,
    DEF_FINALE_PHASE4_SUPER_CHOSEN = 5,
    DEF_FINALE_POSTCREDITS_BLUE = 6
};
int def_finale_phase(void);
int def_finale_active(void);
void def_finale_battle_prepare(int *leader);
void def_finale_battle_begin(void);
int def_finale_on_galacticmon_ko(void);
void def_finale_stage_enters(void);
int def_finale_defeat(void);
int def_finale_victory(void);
int def_finale_enemy_dark(void);
int def_finale_party_gold(void);
void def_finale_enemy_stats(short stats[5]);
void def_finale_party_stats(short stats[5]);
int def_finale_damage(int side, int value);
int def_finale_blast_only(void);
int def_finale_golden_mode(void);
int def_finale_trigger_super_chosen(void);
int def_finale_dna_chain_next(int *out_partner_id, int *out_result_id);
int def_finale_velvet_active(void);
int def_finale_top_door_open(void);
int def_finale_blue_room_active(void);
int def_finale_on_top_door(int *map, int *x, int *y, int *dir);
int def_finale_postcredits_enter(int from_map);
int def_finale_pipimon_active(void);
int def_finale_igor_line(void);
void def_finale_igor_done(void);
int def_finale_field_frame(void);
void def_finale_draw_blue_room(void);
void def_finale_reset(void);
void def_finale_savestate(struct PortState *s);
/* The slot's third record (memcard.c part 7, DEF_RECORD3_AT + slot * DEF_RECORD3_SIZE, the card file's last free bytes):
 * the quest's dark forms, the first DEF_BLAST_QUEST_FORMS partner+form pairs a Blast called up, each with the stats it
 * had the first time (the Digimon as transformed: FightstgStats.stats and its max HP), its strongest stat and the best
 * that stat ever was. Read after the second record; nothing to keep writes all zeros. */
#define DEF_RECORD3_SIZE 0x80
#define DEF_RECORD3_PART 7
#define DEF_RECORD3_AT 0x7E00
#define DEF_RECORD3_VERSION 1
typedef struct DefBlastEntry {
    unsigned char partner; /* gamestate_data.digimon index */
    unsigned char step;    /* its blast_forms index */
    short stats[5];        /* the first time: FightstgStats.stats (0..999) */
    short hp;              /* and its max HP */
    unsigned char best;    /* its strongest stat (an index of stats) */
    short best_value;      /* the best that stat ever was, any time the pair was taken */
} DefBlastEntry;
int def_record_blast_count(void);
const DefBlastEntry *def_record_blast_entry(int i);
int def_record_blast_find(int partner, int step);
int def_record_blast_add(const DefBlastEntry *e); /* its index, -1: full, already there or out of range */
void def_record_blast_raise_best(int i, int value);
void def_record3_write(unsigned char *out);
void def_record3_read(const unsigned char *in);
unsigned char *def_record3_buffer(void);
/* The Chosen Evolution: the partner's Mega (records_digimon rows), Kumamon's Beelzemon. */
#define DEF_CHOSEN_MEGAS { 33, 51, 34, 36, 37, 38, 39, 40 } /* GuardiAngemon, Beelzemon, Cannondramon, WarGreymon,
                                                             * Imperialdramon, Gallantmon, Sakuyamon, Seraphimon */
#define DEF_CHOSEN_LEVEL 45  /* the level it takes */
#define DEF_CHOSEN_BUFF_PCT 10 /* the quest's buff: per form purified, this share of its best stat, for good */
/* The purification (definitive_purify.c): Gennai's quest and simulator. */
#define DEF_PURIFY_BITS 10000     /* what "Gennai" (Myotismon) takes when the quest's ten forms are there */
#define DEF_PURIFY_WAVE 3         /* forms per wave: a battle's three enemy members */
#define DEF_PURIFY_SIM_PCT 90     /* a form in the simulator: its first stats and HP times this / 100 */
#define DEF_PURIFY_WEAK 60        /* its lowest resistances, as the enemy table's weaknesses (DEF_WEAK_MAX) */
#define DEF_PURIFY_STRONG 300     /* its highest, as a strong resistance */
#define DEF_PURIFY_TP_CAP 99      /* the TP the game's level-ups stop at (stfgtrep) */
/* The Time Room (definitive_time_room.c): the gatekeeper before the Warp Gate to the Gunslinger, the
 * point of no return. Three trials in a row against the room's challengers, each one battle of the challenger's three
 * members (tools/gen_time_room.py); no prize, no game over: a defeat or a step back starts the run over. */
#define DEF_TIME_TRIALS 4 /* Oikawa, Sepikmon, Gennai, and Myotismon in the old man's shape (before Gennai came) */
#define DEF_TIME_ROUNDS 3 /* trials won in a row to pass */
typedef struct DefTimeTrial {
    DefDriStage stages[DEF_DRI_MEMBERS]; /* the enemy members, in the order they come in */
    short battle;                       /* the boss battle it is set up as (kind, immunities) */
} DefTimeTrial;
extern const DefTimeTrial def_time_trials[DEF_TIME_TRIALS];
extern const DefTimeTrial def_oikawa_fight; /* the Oikawa's arc (definitive_story.c) */
int def_time_set_battle(const DefTimeTrial *t, int stage, int music, short *party, short *hp, short *mp);
void def_time_restore(const short *party, const short *hp, const short *mp);
int def_time_on(void);                  /* the Time Room or one of its trials runs (the Velvet Room's map state) */
int def_time_door(int map, int *x, int *y); /* the gate's door on `map`: 1 and its foot */
void def_time_enter(void);              /* its door was taken: the Velvet Room is the Time Room this visit */
int def_time_leave(int *map, int *x, int *y, int *dir); /* a map change while it runs: see definitive_time_room.c */
struct FieldstgPlacedActor;
struct FieldstgActor;
struct FieldstgPlacedActor **def_time_room_actors(void); /* the Time Room's figures (NULL-ended) */
struct FieldstgPlacedActor *def_time_challenger(void);   /* the trial's challenger, NULL outside a trial */
int def_time_talk(struct FieldstgActor *actor);          /* the line of a figure talked to, 0: not the Time Room's */
int def_time_guarded(int species);      /* the trial's guarded challenger (its last member): takes DEF_GUARD_PCT (30%) until staggered */
int def_time_side_door_open(void);      /* the room's upper door leads to the trials */
void def_time_frame(void);              /* each field frame while the room runs */
int def_time_warp_closed(int type, int param); /* the Warp Gate held shut until the trials are passed */
void def_time_battle_begin(void);
void def_time_battle_end(int result);
void def_time_reset(void);
void def_time_savestate(struct PortState *s);
int def_time_text_own(int base_id, int index);
void def_time_gate_frame(int (*say)(const int *messages, int n)); /* the shut gate's warning (def_story_frame) */
int def_story_event_closed(int type, int param); /* a map event a story hook holds shut (definitive_story.c) */
/* The Oikawa's arc (definitive_story.c): his talk, his battle, and the Black Market opened for the first time. */
int def_story_oikawa_talk(struct FieldstgActor *actor);
int def_story_oikawa_figure(const struct FieldstgPlacedActor *placed);
void def_story_battle_begin(void);
void def_story_battle_end(int result);
void def_story_market_opened(void);
void dw3_adventure_music(int *music, int *sound);
int def_purify_fighting(void);
int def_purify_next_wave(void);
void def_purify_battle_begin(void);
void def_purify_battle_end(int result);
void def_purify_offer(void);
struct PortState;
void def_purify_reset(void);
void def_purify_savestate(struct PortState *s);
void def_dri_reset(void);
void dw3_velvet_reset(void);
/* 1 when the screen and the card record are in use: never in a replay script (shared with the emulator), unless
 * DW3_DEFINITIVE is set, as the rest of the layers (DW3_BESTIARY: this layer alone). */
int def_bestiary_active(void);
/* The record's buffer for the card transfers (a static one: the slot's 0x2700 buffer has no room). */
unsigned char *def_bestiary_buffer(void);
