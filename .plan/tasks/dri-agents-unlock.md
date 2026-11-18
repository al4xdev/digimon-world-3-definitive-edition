# DRI Agents: no level locks, a hint to the DDNA, the Gauntlet only after the quest

Owner's request (2026-11-17):

1. Remove the original locks: the level requirement, and the block when the player already has that Rookie (as a
   starter).
2. While the agent's own event is not done, his line gives a clear hint of where to find what he needs.
3. Open the agent's intro and Simulations (DRI Gauntlet) menu only after his original event is done, so the canonical
   quest and the simulator never overlap.

Status (2026-11-17): mapped, nothing applied.

- Parts 1 and 3 touch no text and can start at once.
- Part 2 is text (`tools/text_override/en.txt`), so it waits until the story session says the texts are free (do not
  run `tools/gen_text_override.py` before that).

The map comes from `.dumps/tasks/dri/agents_map.py`, which writes `agents_map.txt`. That file has every agent
variant's conditions, flags set and decoded lines.

## How the game does it today

### The agents and their quests

Each agent is a placed actor in a stage file (`src/wstag/wstagNNN.c`). Several `FieldstgPlacedActor` variants stand
on the same spot, chosen by `flags_required`:

- one variant without the DDNA, one with it;
- each pair is placed twice: during the story window and in the post-game (progress == 43).

Each variant has a `FieldstgTalk` list; the first entry whose flags pass is said.

| agent | map (stage file) | actor | Rookie | talk file | DDNA item | from (boss, where) | quest flag | level window (party level sum) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Kazuki | 0x21D (330) | 43 | Patamon (7) | TALK02 | 39 | MagnaAngemon, South Badland 0x247 (wstag550, actor 124) | 0x602 | 45 ≤ sum < 75 |
| Michael | 0x21D (330) | 44 | Renamon (6) | TALK02 | 38 | Taomon, North Badland W 0x24A (wstag565, actor 125) | 0x603 | 45 ≤ sum < 75 |
| Bob | 0x226 (375) | 43 | Veemon (4) | TALK06 | 35 | Paildramon, Bulk Swamp 0x234 (wstag450, actor 127) | 0x607 | sum < 30 (no lower bound) |
| Johann | 0x22B (405) | 43 | Agumon (3) | TALK06 | 6 | MetalGreymon, Bulk Swamp 0x233 (wstag445, actor 123) | 0x600 | 30 ≤ sum < 60 |
| Jimmy | 0x22E (420) | 43 | Guilmon (5) | TALK01 | 34 | WarGrowlmon, Ether Jungle 0x23C (wstag490, actor 126) | 0x601 | 30 ≤ sum < 60 |
| Ken | 0x258 (635) | 43 | Kotemon (0) | TALK02 | 19 | Kyukimon, map 0x241 "Zhu Que City" (wstag525, actor 128) | 0x604 | 60 ≤ sum < 90 |
| Yuji | 0x258 (635) | 44 | Kumamon (1) | TALK02 | 395 | GrapLeomon, Ether Jungle 0x23C (wstag490, actor 129) | 0x605 | 60 ≤ sum < 90 |
| Yang | 0x258 (635) | 163 | Monmon (2) | TALK02 | 360 | Armormon, Duel Island 0x228 (wstag385, actor 100) | 0x606 | 60 ≤ sum < 90 |

### The flags

The flag numbers are decoded as `gamestate_get_flag` reads them:

- `0x70xx`: a `gamestate_conditions` entry.
- **The party level sum** is `gamestate_cond_digimon(5, a)`: the levels of the 3 party members summed, at least
  `a * 15 + 30`. The condition IDs 0x25 to 0x29 stand for sums 30, 45, 60, 75 and 90. Only these five agents' stage
  files use them.
- **"[Rookie] NOT joined"** is `cond_digimon(6, r)`, and **JOIN** is `cond_digimon(3, r)`, which the hand-in line sets.
- **`0x80xx`** is an item held.
- **`0x60xx` with value 1** means progress == xx; the condition IDs 0x24 and 0x26 to 0x29 are progress ranges
  (`gamestate_progress_ranges`).

### One agent's talk, in order (Kazuki shown, the others alike)

| entry | when | what is said |
| --- | --- | --- |
| 1 | first time (`0x1A1F` = 0) | his intro, then `0x1A1F` = 1 |
| 2 | Patamon already joined | "I hope Patamon can digivolve…": this is the starter/already-have case |
| 3 | sum below the lower bound | "too busy", the lower lock |
| 4 | inside the window | the quest line (msg 633, "Bring Patamon's DDNA…"), sets the quest flag 0x602 |
| 5 | sum at or over the upper bound | "your Digimon look strong, you don't need it", the upper lock: the quest is never given once the party is too strong |

### The other pieces

- **The DDNA boss** (another stage file) is placed only with `quest flag = 1` and the DDNA not held. Talking to it
  gives the DDNA.
- **The variant with the DDNA held** says the hand-in line: JOIN the Rookie, then "Go to Piximon or Dr. Kadomatsu".
- **Visibility windows** (not a lock the owner named; left as is):
  - Kazuki and Michael: progress 14 to 38, and 43.
  - Bob: 17 to 38, and 43.
  - Johann and Jimmy: 5 to 38, and 43.
  - Ken, Yuji and Yang (0x258): progress 15 to 99 except 39 and 40, on route 1, each in his own room (rooms 1, 4,
    7).
- **The Gauntlet today:** `def_dri_talk` (`port/game/definitive_dri_screen.c:609`, called from
  `src/fieldstg/fieldstg_80087DB0.c:3235`) arms the list after any talk with an agent, quest done or not. The talk
  itself is never taken.

## Part 1: lift the level locks (no text)

- **Where the change goes:** a new `port/game/definitive_dri_quest.c` (or a section of `definitive_dri.c`), run as
  `def_variants_hook` is: every time when `def_on`, writing absolute values, so it is never applied twice and a
  replay script keeps the disc's data.
- **What it rewrites:** the agents' talk flag lists, in place. Each target is a global `u16 D_WSTAGnnn_xxx[]` array
  in the stage files, reachable by `extern`. A small generated table (from `agents_map.py`) lists for each agent:
  - the quest entry's level pairs, changed to an always-true pair;
  - the lower-lock entry and the upper-lock entry, given an always-false pair.
- **The two pairs:**
  - Always true: a flag type `gamestate_get_flag` does not know returns 1, for example `0x5000, 1`.
  - Always false: `0x61FF, 1` (progress == 511).
  - Measure both in a host test before relying on them.
- **The result:** after the intro, an agent whose Rookie has not joined always gives the quest line and sets the
  quest flag, at any party level. Bob loses his upper lock the same way ("concentrate on one" becomes unreachable).
- **Before writing:**
  - Check that the reset/boot snapshot covers the stage files' data, as it covers the records for the variants. If
    it does not, write the original values back when `def_on` is 0.
  - Check that no other actor of the five stage files reads the same arrays (`grep` the symbol).

**"Already has that Rookie":** there is nothing to give then, since JOIN would only set `joined` again. The block the
owner means is that the agent's event can never complete for a starter (Kotemon, Kumamon, Monmon), which would lock
part 3's menu forever. Part 3's rule ("done" = the Rookie has joined) settles it: a starter's agent counts as done
from the start. His original "I love Kotemon!" line stays, and the menu opens after it.

## Part 3: the Gauntlet only after the agent's own event (no text)

- **The rule:** in `def_dri_talk`, arm the wait only when the agent's Rookie has joined
  (`gamestate_data.digimon[r].joined != 0`).
- **The agent's Rookie:** add it to the agent table (map, actor) → Rookie:

  | (map, actor) | Rookie |
  | --- | --- |
  | (0x21D, 43) | Patamon |
  | (0x21D, 44) | Renamon |
  | (0x226, 43) | Veemon |
  | (0x22B, 43) | Agumon |
  | (0x22E, 43) | Guilmon |
  | (0x258, 43) | Kotemon |
  | (0x258, 44) | Kumamon |
  | (0x258, 163) | Monmon |

  - Today `AGENTS` in `tools/gen_dri_challenges.py` uses map -1 for actors 43, 44 and 163. Change the rows to these
    eight (map, actor, Rookie), and change the generated table and `def_dri_agent` to match.
  - An actor 43/44/163 on any other map is then not an agent. That matches the README's "an ordinary NPC with
    sprite 43/44 does not open the DRI" check.
- **The flow:** `def_dri_talk` runs at the talk's start. On the talk that hands the DDNA in, the Rookie is not joined
  yet, so the list does not open on that talk; it opens on the next one. That is the zero overlap the owner asked.
- **Debug path:** the menu's debug path (`def_dri_debug`) is unchanged.
- **Unlocked challenges:** the challenges already unlocked stay in the save. Only the door to the list changes.

## Part 2: the hint line (text; waits for the texts to be free)

- **Which lines:**
  - With part 1, an agent whose quest is not done says his quest line on every talk until the DDNA is brought.
  - So the hint goes into those quest lines, as an override of the talk file message with one page added at the
    end. Use the `TALKnn msg:` form, as the Velvet lines do (`en.txt`, `TALK05 17`).
- **Before writing the hints, verify in game where each boss stands.** The table above is from the stage files: map
  0x241 for Kyukimon is "Zhu Que City" by its area, but it may be a building. Write the place as the player sees it
  on the map screen.
- **The eight lines** (keep the game's style: about 20 characters a line, `<1>` breaks, `<2,2><3>` pages):

  | file | msg | agent | hint (English draft) |
  | --- | --- | --- | --- |
  | TALK02 | 633 | Kazuki | "MagnaAngemon was seen in the South Badland." |
  | TALK02 | 647 | Michael | "Taomon hides in the west of the North Badland." |
  | TALK06 | 676 | Bob | "Paildramon roams Bulk Swamp, by the bridge." |
  | TALK06 | 691 | Johann | "MetalGreymon is said to be at Bulk Bridge." |
  | TALK01 | 277 | Jimmy | "WarGrowlmon lives in the Ether Jungle." |
  | TALK02 | 831 | Ken | "Kyukimon is somewhere in Zhu Que City." (verify the spot) |
  | TALK02 | 839 | Yuji | "GrapLeomon trains in the Ether Jungle." |
  | TALK02 | 847 | Yang | "Armormon guards Duel Island." |

- **Translation:** Portuguese is the owner's translation, done later. Do not write `pt.txt`.
- **Then:**
  - Run `gen_text_override.py`, only after the story session's OK.
  - Run `tests/port/text_override.py` (line widths and pages).

## Tests

- **Host:** `tests/port/definitive_dri.py`, through its driver, gets two new cases:
  - the list is armed only when the agent's Rookie has joined;
  - an actor 43 on a map outside the table is not an agent.
- **Host:** a unit case for the always-true and always-false pairs against `gamestate_get_flag`.
- **In game**, from a save near 0x22B (Johann), with a scratch script beside `agents_map.py`:
  1. With a party sum under 30: he gives the quest line (no "busy"), and the list does not open.
  2. With a sum of 60 or more: the same (no upper lock).
  3. With the DDNA poked into the bag: the hand-in line; the list does not open on that talk, and does on the next.
  4. With Agumon poked as joined: the "great Agumon" line, then the list.
  5. A starter's agent (0x258, room 1, Ken with Kotemon joined): his line, then the list.
- **Not tested before:** the README lists a real DRI talk as never tested in game. These shots close that.
- **Regression:** `tests/port/definitive*.py`, `tests/port/savestate.py` and `tests/port/run.py` (the replays must
  not change: everything is behind `def_on`).

## Docs

- README, "The DRI Gauntlet": the list opens once the agent's own Rookie has joined (a starter's at once). The DDNA
  quests have no level window, and each agent hints where to look.
- README, the technical DRI paragraph: the new agent table (map, actor, Rookie) and the talk-list rewrite.
- `definitive-hunt-economy-dri-prd.md`, layer H: one line on the owner's amendment of 2026-11-17.

## Not this task

- "Refine the Velvet Room's doors" (the owner's note after the request) is the Velvet session's.
