# The Definitive Edition: technical reference

The long form behind the [player README](../../README.md): rules, numbers, files, save bytes, tests and pitfalls, so
nothing depends on remembering it. Where a point could not be verified while writing, it says so ("unverified").

These pages were the second half of the old single README, moved here verbatim (only links were rewritten), so what
was validated against the code there still holds here. The decompilation and the port's own internals (formats,
matching, performance, the GPU work) stay in [`docs/`](../README.md).

## Pages

| Page | What it covers |
|---|---|
| [Combat](combat.md) | weaknesses, stagger, One More, the whole damage calculation with a worked example, buffs and debuffs, Inspect, techniques and MP, the combat layers 0-10, constants, the combat design record, buff and debuff tiers, the status/poison/revive rework and its balance measurements, turn order and the progression numbers |
| [Progression and difficulty](progression.md) | training points, guaranteed training, experience, digivolution hints and short animations, Hard Mode, Super Hard Mode (the Demon Bell) |
| [The hunt and the economy](hunt-and-economy.md) | the regional chain, Rare Targets, Hunt Materials, prices, the forge, equipment variants, the Black Market, the DRI Gauntlet, the Guide, the Guardromon network, the Overlevel Rush, and the hunt/forge/DRI design record |
| [The Velvet Room](velvet-room.md) | the room, its ways in (the inns' lights), its guests and music, details, decisions and history |
| [The story layer](story.md) | the room growing with the story, the opening's cut, Repeating Tom, Igor and his dreams, the hero's choices and echoes, the rewrite, the Oikawa's arc, the Time Room, the Guide's story conditions, saved state and hook points |
| [Super Rare hunts, recolour and hero skins](super-rare-hunts.md) | the 12 Super Rares and the tutorial, territories, the temperature compass, the palette swap of rare foes, the 14 hero skins |
| [Blast, Purify and Chosen Evolution](blast.md) | the Blast Gauge, overheat, the boost, the DV Plug, the Purify quest, the Chosen Evolution, the design record |
| [Girls Power](girls-power.md) | playable Kail: sprite repaint, hand-drawn frames, the story text swap, the workflow |
| [Comfort, battle HUD and launcher options](interface.md) | the comfort and PC features list, the direct-button battle HUD's design record and the glyph fix |
| [Texts and translation](texts-and-translation.md) | text overrides, fonts, PT-BR as a 6th language, the translation pipeline, the FR/DE/ES/IT plan |
| [Save format](save-format.md) | the memory card records the edition adds |
| [Building, running, mods, tools, tests, debugging](port-and-tools.md) | command line options, environment variables, `port/mods/`, `tools/`, the tests, the MCP |
| [Platforms](platforms.md) | the launcher, cloud sync, handhelds, Android, PortMaster, performance |
| [History and decisions log](history.md) | closed plans and their post-mortems, the PortMaster CFW research, muOS, Android, the rebalance brief |

The figures these pages show live in [`docs/images/definitive/`](../images/definitive/README.md).

## How the definitive layers work (read this first)

- The "definitive" layers are **always on** (there is no mod switch). They are gated by `def_on()`, which is off under a
  `--script` replay, so scripted replays and the golden tests keep the original game.
- **Every number is a constant in `port/game/definitive.h`.** Tuning the balance means editing that file (the balance
  itself is the owner's call).
- The game's C calls hooks under `#ifdef PC_PORT`; the prototypes are in `include/port.h`. The PS1 matching build and the
  goldens must stay unchanged, so no hook may change behaviour outside `PC_PORT`.
- Main files: `port/game/definitive.c` (combat, training, XP, status), `definitive_bestiary.c` (card records),
  `definitive_hunt.c`, `definitive_superhunt*.c`, `definitive_materials.c`, `definitive_market*.c`, `definitive_dri*.c`,
  `definitive_economy.c`, `definitive_prices.c`, `definitive_recipes.c`, `definitive_velvet.c`, `definitive_purify.c`,
  `definitive_recolor.c`, `definitive_skins.c`, `definitive_travel.c`, `definitive_evohint.c`, `definitive_callout.c`,
  `definitive_text.c` / `definitive_text_lang.inc`, `girls_power.c`, `game_mods.c`.
- Rules every layer keeps: an old save opens and plays; the player's MP stays limited (Relay and technique costs are
  real); a boss is immune to status (poison, sleep and the like) but not to stat debuffs nor to elemental damage. New
  code never assumes three party members: ask `gamestate_data.funcs.get_party_member` (Hard Mode runs with two).
