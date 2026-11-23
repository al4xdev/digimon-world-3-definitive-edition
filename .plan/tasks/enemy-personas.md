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

## Next

1. Build the tooling (table, helpers, engine functions, the C checker/report test). No distribution yet.
2. One joint session to fill the cases (Super Rares, DRI lines, Rare Target molds, commons).
