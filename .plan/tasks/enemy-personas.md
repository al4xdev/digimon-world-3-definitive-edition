# Enemy personas (hand-picked puzzles per encounter)

The design guide lives in the shared doc "Personas dos inimigos — guia de design":
https://claude.ai/artifact/En5xMhEhs9cw85zhdoXGoq
When it closes, its final text moves to `.plan/closed/` here.

## Decided (2026-11-23)

- Personas are per encounter (Super Rare, Rare Target, DRI stage, campaign battle, or a common species), hand-written
  in C: one row per special enemy in `port/game/definitive_persona.c`, attributes as readable helper macros
  (`WEAK`, `RESIST`, `RESIST_ALL`, `GUARD_*`, `PHASES`, `HUES`, `SPEED_X`). No Python generator.
- The weakness/hunt/DRI generators are frozen as legacy (`f7d1024`).
- Engine hook: `fightstg_rules_get_stats` → next to `def_apply_resist_mod`; the inspect panel reads the same path.
- No magic immunity: "resists everything" (0.5x elemental; physical and non-elemental magic go through) is enough.
- Speed is relative to the party (`SPEED_X(3)`), not a fixed 999.
- Phase shift: every N turns (or on stagger) the colour (CLUT re-turn from a stashed original) and the weakness change;
  each colour keeps one weakness for the whole fight.
- Discovery of a persona's weaknesses is saved per encounter (new save part); the species bestiary must not leak it.
- HUD hook `def_persona_warn(slot)`: a warning symbol on encounters with their own rule (new HUD calls it).
- Ogremon (tutorial hunt) gets no rule: a normal mob with 2 weaknesses.
- "Lost" commons with a rule appear only from Lv10 on, as hints before the rares.

## Tooling (built 2026-11-23)

- `port/game/definitive_persona.h`: the row type and the helpers (`SUPER/RARE/DRI/CAMPAIGN/SPECIES`, `KEEP`, `WEAK`,
  `RESIST(_ALL)`, `GUARD_A/B/ALWAYS`, `PHASES(n)`, `ON_STAGGER`, `B_WEAK/B_RESIST`, `HUE(S)`, `SPEED_X`).
- `port/game/definitive_persona_table.c`: the rows. Only the 3 story bosses that were already guarded so far.
- `port/game/definitive_persona.c`: lookup (encounter rows before species rows), the phase machine, discovery, the rules
  check. `definitive.c` glues it into the stats hook, Guard, speed, hit/Libra discovery, enemy turn, inspect.
- Discovery is saved in the expansion save (`.plan/tasks/expansion-save.md`), section `"personas"`
  (`port/game/definitive_persona_save.c`): per KEEP id (0..255, `DEF_PERSONA_KEEPS`) the per-phase map learnt over
  every fight (the inspect shows it in later fights before it is solved) and the solved flag (then the whole map).
  The card's old 144 solved bits (record 1's last 18 bytes) are migrated on first load; those bytes now hold the
  expansion's generation. The engine uses `def_persona_record_known/learn/solved/solve`, no file I/O.
- Phase change: `SHIFT!` callout; with `HUES` the model's CLUTs are turned again from the stashed originals.
- Inspect title gets a `! ` mark (`$PersonaMark`) for any enemy with a row.
- `tests/port/definitive_persona.py`: checks the real table (and prints it for review), then drives the engine with its
  own case rows and confirms the check catches each class of mistake.

## Next

1. Joint session to review the cases. A full draft is in the table since 2026-11-23 (108 rows: 12 Super Rares, 16 DRI
   lines, 71 Rare Targets, 6 lost commons), written by the assistant alone from the guide; the review table for the
   owner (and other LLMs) is `.dumps/tasks/persona/personas.proposed.json`, both made by
   `.dumps/tasks/persona/draft_personas.py` (an authoring aid: the C table is the source once reviewed). Open: Dark as a
   key only from Lv45 (solvability), HP/DEF of Relampago/Brutamontes, the commons' regions.
2. In game, confirm on a real fight: the SHIFT callout, the hue change, and SPEED_X (applied against the current enemy
   target; the delay code does not say which enemy is acting).
