/* Enemy personas (owner, 2026-11-23): a hand-picked puzzle per encounter instead of more HP or damage. One row per
 * special enemy in definitive_persona_table.c, written with the helpers below; what a row does not say comes from the
 * species (definitive_enemies.c). The engine side (definitive_persona.c, the battle's glue in definitive.c) never
 * changes when rows are added: a new rule is a new helper here plus one `if` there.
 *
 *   { SUPER(3), KEEP(0), WEAK(FIRE), GUARD_ALWAYS },                        a Super Rare: one weakness, always guarded
 *   { RARE(0x12), KEEP(1), WEAK(FIRE), PHASES(2), B_WEAK(WIND), HUES(40, 220) }  colour and weakness change every 2 turns
 *   { DRI(3), KEEP(2), RESIST_ALL, SPEED_X(3) },                            a whole DRI line
 *   { CAMPAIGN(31, 448), GUARD_ALWAYS },                                    a story boss (battle + species)
 *   { SPECIES(80), PHASES(2), GUARD_A },                                    every Goburimon: guarded every other 2 turns
 *
 * The design guide: .plan/tasks/enemy-personas.md. */
#ifndef DEFINITIVE_PERSONA_H
#define DEFINITIVE_PERSONA_H

/* The element families (def_element_family): a pair is one affinity, so a row names families, not elements. */
#define FIRE 0x01
#define WATER 0x02   /* Water + Ice */
#define WIND 0x04
#define THUNDER 0x08 /* Thunder + Machine */
#define DARK 0x10
#define DEF_PERSONA_FAMILIES 0x1F

enum { DEF_PERSONA_NONE, DEF_PERSONA_SUPER, DEF_PERSONA_RARE, DEF_PERSONA_DRI, DEF_PERSONA_CAMPAIGN, DEF_PERSONA_SPECIES };

/* What one phase of a persona is. weak = resist = 0: the species' own resistances (a phase B that says nothing of
 * them takes phase A's). */
typedef struct DefPersonaPhase {
    unsigned char weak;   /* families at DEF_PERSONA_WEAK */
    unsigned char resist; /* families at DEF_PERSONA_RESIST; the rest neutral */
    unsigned char guard;  /* 1: guarded in this phase (DEF_GUARD_PCT until a stagger) */
    short hue;            /* the enemy's palette turned this many degrees; 0 none (an encounter row only) */
} DefPersonaPhase;

typedef struct DefPersona {
    unsigned char kind;       /* DEF_PERSONA_* */
    short a, b;               /* SUPER: hunt index; RARE: region; DRI: challenge; CAMPAIGN: battle, species; SPECIES */
    unsigned short keep;      /* KEEP(n): n + 1, the row's discovery id (DEF_PERSONA_KEEPS); 0 none */
    unsigned char phases;     /* 1, or 2 with PHASES / ON_STAGGER */
    unsigned char turns;      /* the enemy's turns a phase lasts; 0 with two phases: until it is staggered */
    unsigned char speed_x;    /* its Speed as this many times the party's; 0 its own */
    unsigned char def_pct;    /* its Defense at this percent of its own (1..100, lower only); 0 its own */
    DefPersonaPhase phase[2]; /* A, B */
} DefPersona;

#define DEF_PERSONA_WEAK 60    /* the enemy table's weakness (DEF_WEAK_MAX and below) */
#define DEF_PERSONA_RESIST 300 /* the enemy table's strong resistance (DEF_RESIST_MIN and up) */
#define DEF_PERSONA_NEUTRAL 100

/* Who. DRI: the whole line (owner: the persona is the line's, its stages only scale). */
#define SUPER(i) .kind = DEF_PERSONA_SUPER, .a = (i)
#define RARE(region) .kind = DEF_PERSONA_RARE, .a = (region)
#define DRI(c) .kind = DEF_PERSONA_DRI, .a = (c)
#define CAMPAIGN(battle, species) .kind = DEF_PERSONA_CAMPAIGN, .a = (battle), .b = (species)
#define SPECIES(id) .kind = DEF_PERSONA_SPECIES, .a = (id)
/* Its discovery's id in the slot's save (the expansion's "personas", definitive_persona_save.c): what was learnt of
 * its map in any fight, and once every weakness of it was found, later fights show the whole map. Needed by every row
 * that changes the map, except SPECIES (the bestiary keeps those). Never reuse or renumber an id. */
#define KEEP(n) .keep = (n) + 1

/* Phase A (or the only one). */
#define WEAK(f) .phase[0].weak = (f)
#define RESIST(f) .phase[0].resist = (f)
#define RESIST_ALL .phase[0].resist = DEF_PERSONA_FAMILIES
#define GUARD_A .phase[0].guard = 1
#define HUE(deg) .phase[0].hue = (deg)
/* Both phases. */
#define GUARD_ALWAYS .phase[0].guard = 1, .phase[1].guard = 1
#define HUES(a, b) .phase[0].hue = (a), .phase[1].hue = (b)
/* Two phases: every `n` of the enemy's turns, or at each stagger. */
#define PHASES(n) .phases = 2, .turns = (n)
#define ON_STAGGER .phases = 2, .turns = 0
/* Phase B. */
#define B_WEAK(f) .phase[1].weak = (f)
#define B_RESIST(f) .phase[1].resist = (f)
#define B_RESIST_ALL .phase[1].resist = DEF_PERSONA_FAMILIES
#define GUARD_B .phase[1].guard = 1
/* Speed relative to the party (owner: a fixed 999 means little late in the game). */
#define SPEED_X(n) .speed_x = (n)
/* Its Defense at n% (owner, 2026-11-23: the molds that are fought hard, Brutamontes going physical and Relampago
 * hit in one go, take a lower Defense instead of the guide's lower HP, which a row cannot set). */
#define DEF_PCT(n) .def_pct = (n)

extern const DefPersona def_personas[];
extern const int def_persona_count;

/* Where a battle is: -1 for what it is not. */
typedef struct DefPersonaWhere {
    short super;  /* the Super Rare hunt being fought */
    short region; /* the Rare Target's region */
    short dri;    /* the DRI challenge */
    short battle; /* records_state.battle */
} DefPersonaWhere;

/* ---- The table (no game state: the host test drives these). */
const DefPersona *def_persona_find(const DefPersonaWhere *w, int species);
int def_persona_own_map(const DefPersona *p);        /* 1: it changes the weaknesses */
int def_persona_hides_species(const DefPersona *p);  /* 1: an encounter's own map, its discovery apart (not SPECIES) */
const DefPersona *def_persona_species(int species);  /* a common species' row, NULL none */
int def_persona_def(const DefPersona *p, int defense); /* its Defense after DEF_PCT (at least 1) */
int def_persona_weak(const DefPersona *p, int phase); /* the phase's weak families */
int def_persona_guard(const DefPersona *p, int phase);
int def_persona_hue(const DefPersona *p, int phase);
/* The phase's resistances over the species' own (`base`), into `out` (may be `base`). */
void def_persona_resists(const DefPersona *p, int phase, const short base[7], short out[7]);
/* The table's rules (the host test): the number of problems, each logged through `report` when not NULL. */
int def_persona_check(void (*report)(const char *what, int row));
int def_persona_check_rows(const DefPersona *rows, int count, void (*report)(const char *what, int row));

/* ---- A battle's state, per enemy slot (0..2). */
void def_persona_battle_begin(const DefPersonaWhere *w);
int def_persona_same_where(const DefPersonaWhere *w); /* 1: the battle began as `w` */
void def_persona_battle_end(void); /* forget the encounter: the next battle, even the same one again, starts anew */
const DefPersona *def_persona_slot(int slot, int species); /* the slot's row (a new species there starts it again) */
int def_persona_phase(int slot);
int def_persona_enemy_turn(int slot);  /* its turn begins: 1 when the phase changed */
int def_persona_staggered(int slot);   /* it was staggered: 1 when the phase changed */
int def_persona_known(int slot);       /* element bits the inspect shows (all once the row is solved) */
void def_persona_learn(int slot, int element_bits);
int def_persona_battle_hue(int slot);  /* the hue now of enemy slot `slot` (-1: the encounter's first), 0 none */

/* ---- Discovery, kept per slot by KEEP id (definitive_persona_save.c, the expansion save's "personas" section):
 * the element bits learnt of the id's map `phase` (0: A's, 1: B's own) in any fight, and whether it was solved. */
#define DEF_PERSONA_KEEPS 256 /* KEEP(0)..KEEP(255) */
int def_persona_record_known(int id, int phase);
void def_persona_record_learn(int id, int phase, int bits);
int def_persona_record_solved(int id);
void def_persona_record_solve(int id);
struct PortState;
void def_persona_savestate(struct PortState *s);

#endif
