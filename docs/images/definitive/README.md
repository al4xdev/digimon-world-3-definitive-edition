# Definitive Edition images

The pictures the [player README](../../../README.md) and the [technical reference](../../definitive-edition/README.md)
show.

## Animated infographics (the player README)

| File | What it explains | Shown in |
|---|---|---|
| `hero.svg` | the title banner: the tagline, a WEAK! slash, the five element families | README top |
| `combat-loop.svg` | hit a weakness → stagger → act again → another weak family, Baton Pass or any other blow; OVERFINISH | README, combat |
| `weakness-discovery.svg` | INFO (the old Inspect) learning a foe's families from `???`, the pair rule, Resist Down opening a weakness, Libra | README, combat |
| `damage-scale.svg` | the multiplier ladder from 0.5 to 3.3 and what affinity adds (convex, up to 0.4 / 0.8) | README, combat |
| `buffs.svg` | buff and debuff tiers and the no-stacking rule | README, combat |
| `hud.svg` | the direct-button battle HUD as a pad diagram | README, combat |
| `progression.svg` | total TP by level, the Definitive Edition against the original | README, progression |
| `hunt-chain.svg` | the regional chain, the Rare Target, its rewards, what resets the chain | README; `hunt-and-economy.md` |
| `economy-flow.svg` | commons and Cores flowing into the forge, the Black Market and the DRI Gauntlet | README; `hunt-and-economy.md` |
| `npc-time.svg` | everyone has their own time: the main quest and three people's stories moving on with it, visited or not | README, story |
| `velvet-room.svg` | the room's guests arriving with the story, the way in, Igor, the dreams, the Time Room | README, Velvet Room |
| `blast.svg` | the Blast arc: unlock, gauge, dark forms, the price, Purify, the Chosen Evolution | README, Blast |
| `super-rare-compass.svg` | the temperature compass, the flee and return, the 13 hunts, crystals and skins | README, Super Rares |
| `cross-play.svg` | cloud sync between a handheld and a PC, pairing, conflicts, snapshots | README, platforms |

The SVGs are the source. The numbers they show come from `port/game/definitive.h`: when one changes, redraw the picture.

## Game captures (`shots/`)

The battle, Rush, Bestiary, Black Market and hunt-board shots come from the SDL build on the GPU
renderer at 8x internal resolution with smooth vertices and widescreen on (English, default HUD): PNGs at 1280×720,
README GIFs at 480 wide and 8 fps, and the site's 1280×720 MP4 versions with a JPG poster in `site/assets/shots/`.
The older ones (`battle-persona.png`, Velvet Room, GUIDE) are still software captures. Some states were staged with the
debug channel to show a mechanic (an enemy's HP raised so a stagger is visible before it falls, or lowered for a KO;
most of the Bestiary marked as discovered; the Rush offered by `DW3_RUSH_TEST`; the Black Market and the hunt board
opened by their test pokes); the behaviour shown is the game's own.

| File | What it shows |
|---|---|
| `battle-weak.gif` | a Thunder technique on a weakness: WEAK!, the stagger, the HUD back for the extra action, INFO |
| `battle-baton-pass.gif`, `battle-baton-pass-menu.png` | Baton Pass while the enemy is staggered |
| `battle-overfinish.gif` | OVERFINISH! on a knocked-out enemy |
| `battle-inspect.png`, `battle-hud.png` | the direct-button HUD and the INFO panel |
| `battle-tech.png` | the R1 technique list with each technique's element and MP |
| `battle-tag.png` | Tag's close-up of the bench partner with its forms in a ring (WarGreymon picked) |
| `battle-digivolve.gif`, `battle-digivolve.png` | Digivolve's ring, the two-step confirm and the digivolution |
| `battle-persona.png` | INFO on an enemy with a persona: the `!` mark, SHIFT and the guard explained at the foot (Woodmon on map 0x23B) |
| `overlevel-rush.gif`, `overlevel-rush-panel.png` | the Rush prompt while walking and its rewards panel |
| `velvet-way-in.gif` | an inn's blue light, the prompt, the VELVET / SERVER card, the room |
| `velvet-room.png` | the room at its highest guest level |
| `black-market-replicas.png`, `black-market.gif` | the Black Market: a Core traded for materials (the DEAL! stamp), then the Replicas tab |
| `bestiary.gif`, `bestiary.png` | the 3D Bestiary stage through its tabs (ALL, WILD, BOSS, RARE, SUPER, DRI, DARK) |
| `super-hunt-board.png`, `super-hunt-dossier.gif`, `super-hunt-dossier.png` | the Sepikmon's hunt board and a locked hunt's Target Dossier |
| `guide-quest.png` | the GUIDE's Quest tab |

Still to capture: the Digi-Lab's next-form requirements, Igor's dream panel, the Super Rare compass, hero skins, the
forge's requirement line, the DRI Gauntlet, a Blast form, Girls Power, the Time Room and the launcher.

## Static figures (the technical reference)

| File | What it explains |
|---|---|
| `weakness.svg` | the five families; weak / neutral / resisted / not learnt; one value per family; Resist Down's −80 |
| `stagger.svg` | weakness → stagger → one more, once per weak family a round; OVERFINISH |
| `damage.svg` | the multiplier part by part and each family's affinity ceiling (0.4 Fire/Wind, 0.8 pairs and Dark) |
| `damage-pipeline.svg` | where the multiplier sits among the game's own damage steps |
| `definitive_ui.png` | a capture: the bestiary, the five-family weakness panel, the technique menu's element |
| `banner.svg` | the previous title banner (no longer shown) |

## Rules for the SVGs

They are written for GitHub, which shows them through `<img>`: no scripts, no `foreignObject`, no external fonts,
images or links, a `viewBox` with a width and height, a system font stack, and their own dark background so they read
on the light and the dark theme alike. They are drawn 900 px wide and shown at 800, so no text is smaller than
13.5 px. Animation is CSS `@keyframes` or SMIL; the still frame must already read as the finished picture, and
`prefers-reduced-motion` turns the motion off. Check a change parses with
`python3 -c "import sys, xml.etree.ElementTree as E; E.parse(sys.argv[1])" <file>.svg`, then look at it in a browser
at 800 px.
