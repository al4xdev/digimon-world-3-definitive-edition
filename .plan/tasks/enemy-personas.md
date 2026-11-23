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
- Card: one "solved" bit per row (`KEEP(n)`, 144 max) in record 1's last 18 bytes. Record 2 is full and record 3 has
  ~10 bytes, so a per-element map per encounter does not fit; within a fight the inspect learns hit by hit, and once
  every weakness of every phase was found, later fights show the whole map.
- Phase change: `SHIFT!` callout; with `HUES` the model's CLUTs are turned again from the stashed originals.
- Inspect title gets a `! ` mark (`$PersonaMark`) for any enemy with a row.
- `tests/port/definitive_persona.py`: checks the real table (and prints it for review), then drives the engine with its
  own case rows and confirms the check catches each class of mistake.

## Next

1. Joint session to fill the cases (Super Rares, DRI lines, Rare Target molds, commons from Lv10).
2. In game, confirm on a real fight: the SHIFT callout, the hue change, and SPEED_X (applied against the current enemy
   target; the delay code does not say which enemy is acting).
