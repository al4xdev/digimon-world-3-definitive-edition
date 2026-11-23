/* The expansion save: the slot's port state that has no room on the memory card, in a JSON file beside it
 * (card1.mcd -> card1.def.json), read whole when a slot is loaded and written whole, before the card, when one is
 * saved. The core (definitive_expansion.c) owns the file, its atomic write, the pairing with the card and the
 * savestate; what goes in it are sections, one named object per slot each, that consumers register in
 * definitive_expansion_sections.c. The core never knows what a section holds.
 *
 *   {"version": 1, "slots": [{"gen": 7, "personas": {...}, "<another section>": {...}}, ...]}
 *
 * The pairing: a generation per slot, in the slot's entry and in the card's first record (DEF_EXPANSION_CARD_BYTES:
 * the magic "EXP1", the generation, 4 bytes low first, the rest spare). Every save that keeps anything takes a new one,
 * past both the card's and the file's; the card gets it only once the file is written. An entry whose gen is not the
 * card's (a card restored, synced or copied without its file, a card write that failed after the file's) reads as
 * empty, logged: every section resets and the game goes on. Keys the core or a section does not know (a newer build's
 * sections among them) are kept as they are when the file is written again; a file of a newer version is read as
 * empty and never overwritten.
 *
 * The launcher carries this file with the card as one save (launcher/cloud.h: the sha, backups, restore, cloud, the
 * RecompLauncher mirror). Changing the schema (bumping "version"): the game only reads its own version or older, so the
 * migration belongs to the launcher, which does not do it yet: .plan/backlog/launcher-expansion-save.md.
 *
 * Adding a section: .plan/tasks/expansion-save.md "How to add a section". */
#ifndef DEFINITIVE_EXPANSION_H
#define DEFINITIVE_EXPANSION_H

struct cJSON;
struct PortState;

typedef struct DefExpansionSection {
    const char *name; /* its key in the slot's entry: never renamed once shipped */
    /* Empty: a new game, a slot loaded with no linked entry. Before every load and migrate. */
    void (*reset)(void);
    /* From the slot's object under `name` (NULL: the entry has none). Missing keys read as 0, unknown ones ignored. */
    void (*load)(const struct cJSON *section);
    /* Into `section`, a fresh empty object that replaces the old one under `name`. */
    void (*save)(struct cJSON *section);
    /* 1 when it has anything to keep: with no section keeping anything and no generation yet, the card's bytes stay
     * all zeros (the record byte-identical to one written before the expansion) and the file is not created. */
    int (*any)(void);
    /* NULL, or the card's DEF_EXPANSION_CARD_BYTES when they hold no magic (a record written before the expansion):
     * what the section takes from them, its count (0: nothing; the file is then read as usual). */
    int (*migrate_from_card)(const unsigned char *bytes, int size);
    void (*savestate)(struct PortState *s); /* its state in memory, into the port's savestate */
} DefExpansionSection;

/* The registered sections, ended by NULL (definitive_expansion_sections.c). Their order is the savestate's. */
extern const DefExpansionSection *const def_expansion_sections[];

#define DEF_EXPANSION_CARD_BYTES 18
#define DEF_EXPANSION_VERSION 1

/* ---- The card's side (definitive_bestiary.c's first record). */
void def_expansion_reset(void);                       /* a new game: every section empty, no generation */
void def_expansion_card_write(unsigned char *out);    /* all zeros while there is no generation */
void def_expansion_card_read(const unsigned char *in); /* NULL: none; resets the sections, takes the generation */
unsigned int def_expansion_gen(void);
int def_expansion_keeps(void); /* 1 when the card's bytes will not be zeros (a generation, or a section keeps something) */
void def_expansion_savestate(struct PortState *s);

/* ---- The file (stgmcard's load step 0x48 after the record's read; its save step 0x33 before the records' write). */
void def_expansion_load(int port, int slot);
void def_expansion_save(int port, int slot);
/* The file beside card `port`'s image (port_settings); 0 when the card has none (in memory: its JSON is kept in
 * memory too, for the run). */
int def_expansion_path(int port, char *out, unsigned long size);
/* The host test drives these with its own path. load: 1 when the entry was taken. save: 1 when written (or when
 * nothing needed writing), 0 when the card keeps its old generation. */
int def_expansion_load_file(const char *path, int slot);
int def_expansion_save_file(const char *path, int slot);
int def_expansion_write_atomic(const char *path, const char *text);

#endif
