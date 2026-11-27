# The story layer

[Technical reference index](README.md) · [Player README](../../README.md)

## The story layer (Velvet Room in the main story)

Code: `port/game/definitive_story.c` (the hooks, Igor, the dreams, the rewrite's lookup), `definitive_time_room.c` and
its generated `definitive_time_room_table.c` (`tools/gen_time_room.py`), `definitive_velvet_igor.c/.h` (Igor's head),
generated `definitive_story_text.c` (`tools/gen_text_override.py` from the story files of `tools/text_override/`).
Everything is keyed on the story step `gamestate_data.progress` and is off in a replay script (`def_on`). A story talk
is the game's own talk balloon on the player (or on a given actor) with no actor talked to: the hook asks for an entry
of the stage's talk file whose disc text is the unused letter "a", and the port's override fills it; the field is held
(`event_running`) until the last balloon closes and the confirm button is let go.

**The room grows with the story** (`def_story_velvet_level`; a save from before the port's story is placed by its step alone):

| Level | When | Who comes in (`vl_guests[]`) |
|---|---|---|
| 0 | until Repeating Tom's news at step 4 | nobody: every inn's way in is closed |
| 1 | Tom's news (`DEF_STORY_SEIRYU_CALL`), or any step past 4 | the Sepikmon |
| 2 | step 10, "The Suzaku Badge" | Gennai (the old man, the Blast quest) and Igor |
| 3 | step 16, "The real Leader" | Guilmon, the Demon Bell's keeper |
| 4 | step 22, "Free Asuka City" | Etemon and the pig |
| 5 (`DEF_VELVET_LEVEL_MAX`) | the Oikawa beaten (`DEF_STORY_OIKAWA_BEATEN`), or a save past step 27 | Oikawa behind the counter: the Black Market |

Steps 10/16/22 were picked to spread the guests over the campaign (`steps[]` in `def_story_velvet_level`). The first
visit after the level rose, the guest of the highest level that greets calls the player over (`vl_newcomer`; codes
`DW3_VELVET_PULL_SEPIK`, `_MENTOR`, `_IGOR`, `_ETEMON`, `_MERCHANT`, plus `_TRIAL` for the Time Room in
`include/port.h`); the Bell's keeper and the pig never call, so at level 3 it is still Gennai. The level last greeted
is saved in bits 12..14 of the story flags (`def_record_velvet_seen`). The room is entered by portal
(`dw3_velvet_pull_by`) for the scenes; `dw3_velvet_cut(who)` re-enters the map in place "as its level has it" (a screen
cut). `dw3_velvet_pull` is still defined but has no caller.

**Step 0, "Digimon Online": the opening cut in two.** At New Game the scene (WSTAG785, map 0x2D8, event 1; text of event text
file 0x14A, table 1) is cut where Junior cannot pick a starter pack, after event text 12 (Ivy's "choose, and let's go"):
`opening_cut` rebuilds the overlay's script at runtime into two buffers (`st_first`, `st_second`, 1400 words each; cut again
after every load). The first half ends with the port's event command **`0x7F00 1`** (`ST_CMD_DREAM`, handled in
`fieldstg_event.c` case 0x7F): Junior is pulled into the room alone with Igor, who gives him the key (`TALK05` 18,
`DEF_STORY_IGOR`); the room sends him back to the spot he stood on (`st_junior`) and the stage replays the scene from the
cut: actors put back (PLACE, idle facing) and the camera snapped, then Ivy and Teddy ask why he froze (event texts 1..4
replaced by `$OpenIvy1`, `$OpenTeddy`, `$OpenJunior`, `$OpenIvy2`; the Kaili mode swaps the names), then the game's own scene
from text 13 ("You still haven't made up your mind?"). If the script is not the expected one the cut is skipped (logged).

**Step 4, "The Seiryu Badge": Repeating Tom**. The Guide's row asks
the player to ask around Seiryu City for the Leader (`DEF_STORY_COND_SEIRYU_ASK`, map 0x230). Talked to on the stairs of the
city's inn, Repeating Tom's own line (`TALK01` 0xCC entry 79, "Seiryu Leader is training at the Protocol Ruins") is said as
`$TomNews` (the Leader is gone), then the player's feeling (`TALK01` 178) with the portal sound, and `DEF_STORY_SEIRYU_CALL`
raises the room to level 1: every inn's way in opens, Seiryu's inn's at (210,300). Through any of them Igor alone shows the
room again (`TALK05` 20, `DEF_STORY_SEIRYU_ROOM`), the screen cuts (`dw3_velvet_cut`) and the Sepikmon calls the player
over (`TALK05` 17, his first words while the Leader is missing) with the Ogremon tutorial hunt (Super Rare hunts). Until the
hunt is reported Tom says `$TomGone` and the Guide's main quest is the missing Leader (`DEF_STORY_COND_SEIRYU`); reporting
the won hunt (`def_super_done() > 0`) gives `TALK05` 16 (his trackers saw the Leader at the Protocol Ruins), sets
`DEF_STORY_SEIRYU_DONE`, and the story goes on as the game's. `DEF_SUPER_FLAG_MET` still marks the Sepikmon's first meeting.

**Igor** (`VL_IGOR` 109, the game's man in a black suit `P055GMGD`, file 0x356, 24 px frames; actor 53 `P068SARM` is the
game's other suit). **His head** is hand-drawn for this port (`definitive_velvet_igor.c/.h`, 92 runs of one palette colour,
one quad each): over frames 9, 10 and 11 of his down-left idle
(11 is 9 a row lower; 10 keeps 9's place), mirrored when he faces right; any other frame keeps the game's head; body, idle
and shadow stay the game's. `dw3_velvet_actor_art` calls `dw3_velvet_igor_head` before FIELDSTG adds the figure, so the quads
draw over it in the same ordering-table entry; he keeps his facing (`no_turn`). In his scenes (the dreams, Seiryu's showing:
`vl_scene`, `vl_dream_actors`) he is alone, facing down-left (dir 1), and the player comes in two steps in front of him at
(246,340) dir 5. **His line** is the story's (`def_story_igor_line`, closed by `def_story_igor_done`): the key at the opening
(`TALK05` 18), the showing at Seiryu (20), otherwise **his reading of the party's DNA forms** (`TALK05` 19, built into
`st_igor_text`, 1024 bytes, by `igor_read`): a greeting (`$IgorMeet` to a guest who found the room without the opening's
dream, i.e. a save from before the story layer, else `$IgorBack`), then for each party member with one, a page "<partner>
could become <form>, born of two forms:" with the two forms, the level each needs and how many are done (`$IgorBoth`,
`$IgorOne`, `$IgorNone`), and a farewell (`$IgorBye`, or `$IgorBlank` when nobody has a DNA form). `dna_pick` takes, from the
partner's learn list (`stfgtrep_learn_lists`, as the Digi-Lab's Next Forms panel reads it), a form not owned yet that needs
two other forms, with the most of its two bonds done, then the highest levels. Nothing is built until the Digimon names are
loaded (the room queues file `records_language + 0x4E` at setup); the disc's line shows then.

**Igor's dreams** (`dreams[]`): at a story moment Igor pulls the player into the room alone (`dw3_velvet_pull_by(map,
DW3_VELVET_PULL_IGOR)`: the portal sound, the room with only him), speaks, asks with the **answers' panel** (up to three rows
at the screen's foot over the room's blue; up/down move with sound 0x8004513E, CIRCLE takes with 0x8004503C; it arms only once
the button that closed the last balloon is let go), answers the choice, and sends the player back to the exact spot
(`dream_leave`). The answer is saved and changes no event; later dreams recall earlier answers. A row: answer slot, first and
last step, a game flag that must be set (or 0), Igor's ask, 3 options, 3 replies, the slot of an earlier answer to recall
and its 3 recall pages. A new dream is a row plus its `$Dr*` lines in `tools/text_override/en.txt`. Rows, in table order:

| `DW3_DREAM` | Slot | Step | Trigger | Theme |
|---|---|---|---|---|
| 1 | 0 | 12..18 | game flag 0x1C0E (set by WSTAG305's event 351, the Game Master blames Lucky Mouse, DMG305:1) | Act I, the Game Master's lie: the eyes see the shadow on the wall, not every mask is the culprit's; every answer goes on hunting Lucky Mouse (at 19 his side is heard) |
| 2 | 1 | 27 | the first free frame once the Oikawa no longer bars the Bug Maze (maps 0x244..0x246, `oikawa_ahead`) | Act II, the burden: past this threshold there is no easy way back; do you carry it by your own will? |
| 3 | 3 | 0x26 | WSTAG226's event 935 (DMG200:41 "Wait here. I'll look into it.") ends at 0x26 leaving the player free on map 0x270; the step lasts until the Master Room's events (0x27), Qing Long's restoration machine (flag 0x1A0A) inside it | the Oinkmon: the Destromon stopped, yet Teddy and Kurt are still Oinkmon; recalls the Act I answer and asks what carries the hero, alone now (`$Dr4*`) |
| 4 | 2 | 0x2B | the end of WSTAG820's event 1080 on map 0x2DF (Snatchmon fuses with the Gunslinger, DMG800:17) | Act III, the abyss: accept defeat, or forge a miracle at the edge of the abyss (the canonical defeat becomes the Chosen's awakening); recalls the Act II answer |

Strings: `$DrIgor` (the plate) and 34 `$Dr1*`..`$Dr4*` keys (`Dr1` Act I, `Dr2` Act II, `Dr3` Act III, `Dr4` the Oinkmon; `Ask`,
`Op1..3`, `Re1..3`, `Rc1..3` for the recalls). `DW3_DREAM=<n>` (1..4, the table's order above) dreams row n at the next free
field frame whatever the step and answers (a test shortcut). **Act III and Galacticmon:** in the game event 1080 ends
(`wstag820_event_1080_end`) by setting flag 0x7401, whose type starts the stage's listed battle 1 (Galacticmon) at once, so no
free frame was left and the dream never came. `def_story_hold_galacticmon` (called there under `PC_PORT`) lets the event end
without it; `galactic_hook` sets 0x7401 itself on the first free frame back on map 0x2DF once answer 2 is given; the event's
own trigger (map event type 8, param 0x438) stays shut afterwards (`def_story_event_closed`), since nothing in the game marks
it done (its flag 0x40A5 is set by no one).

**The hero's choices and the echoes.** The scenes' choices (`en_choices.txt`, `Q<n> FILE:sub index@<k>: label | full balloon
text`): before entry `index` the event's dialog holds (`def_story_event_dialog`, asked by `fieldstg_event.c` before each event
dialog) while the same panel shows up to 3 labels (at most 26 letters); the balloon then says the line picked
(`def_story_line`) and the scene goes on as the game's. An echo (`en_echoes.txt`, `Q<n>=<k>[,<k>...] [kaili] FILE[:sub] index:
text`) is said in place of an entry while answer n is one of the options listed (`kaili`: only in Girls Power's mode, a plain
echo in both); an entry's echoes are tried in file order, the first that holds wins; an echo keeps the entry's speaker, and a
choice entry cannot have one. `def_story_line(file, index)` answers in this order: the hero's choice, an echo, the Kaili
line, the rewritten line, then the disc's. Counts today: 111 scene questions (numbered between Q8 and Q150, never
renumbered) with 336 options; 600 echoes (30 of them Kaili-only).

**Story answers in the save** (`def_record_story_answer` / `_set_`, `definitive_bestiary.c`): `DEF_STORY_ANSWERS` = 160 answers
at 2 bits each (`DEF_STORY_ANSWER_BYTES` = 40 bytes at `REC2_ANSWERS`, right after the Blast data in card record 2; answer q
is byte q >> 2, bits (q & 3) x 2; 0 not answered, 1..3 the option); slots 0..7 are Igor's dreams, 8..159 the scenes' choices. A compile-time check keeps them clear of the skin byte;
an older record reads 0. See [Save format](save-format.md#save-format-memory-card-records).

**The story rewrite.** Written in Brazilian Portuguese, with the English, French, German, Italian and Spanish
translated from it. The sources are in `tools/text_override/`. `pt_story.txt` / `en_story.txt` (`FILE:sub index: text`
for an event scene's table, `FILE index: text` for a talk file): childish or simple dialogue made more serious without
changing order or quests, and gender-neutral wherever the hero is concerned. `pt_kaili.txt` / `en_kaili.txt`: Girls
Power's mode, the lines the automatic name swap gets wrong; a text starting `>hero ` is said by the hero (the balloon
moves to the player). FR/DE/IT/ES have the same four files. `DW3_STORY=0` turns the layer off (the game's own text, to compare). Logs: `story: echo ...`, `story:
kaili line ...`, `story: ... choice N answered M`. Text rules: the same speaker plate and control codes as the disc's line, 22 letters a line in balloons (38 in unplated
narration), 3 lines a page, each page ending with `<2,2>` and every later page starting with `<3>`, and no gendered word
about the hero. Girls Power's swap (`tools/gen_text_override.py`, `NAMES`): every line without its own
entry names Junior as Ivy, and Ivy or Kail as Junior (Smith Junior keeps his name), rewrapped within its page. With
their own entries in `en_kaili.txt`: the brother's lines, the "he/him" ones (DMG700:0 2, DMG200:32 3, DMG600:15
18/20/23) and the account-name joke (DMG200:3/4/5 7-8); the swap makes the `$Open*` lines. Portuguese swaps the same
names: its rewrite is written without gendered words or articles next to them, and a line that still needs its own
Girls Power wording has one in `pt_kaili.txt`.

**The Oikawa's arc (step 26 -> 27, `oikawa_hook`).** After Bulbmon, event 711 sets step 27 and leaves the player in the
Bug Maze Pit's hall (map 0x245, WSTAG540) at (1800,524). While he is unbeaten the hooded dealer is added to that map's
figures (`def_story_stage_setup`): actor 42 with the Black Market trader's hand-drawn art over it, in VRAM place 6 (the
map's third figure, actor 78, is left out while he waits), at (1778,502) dir 7, two steps up-left. He calls the player
over once per visit (the player turns to him) and, after his line (`TALK06` 0xEF entry 5, or 6 after a lost try),
fights: **Arukenimon Lv28, Mummymon Lv30, VenomMyotismon Lv32** (`def_oikawa_fight`, boss battle template 7, Bulbmon's
scenery and music: stage 10, `0x60880000`; no escape). A defeat is no game over (`def_story_defeat` from WFIGHTMN: the
party back as it went in, he calls again); beaten, he says `TALK06` 7 (his name is Oikawa) and is gone, the room
reaches level 5, and the Guide sends the player to the Velvet Room (`DEF_STORY_COND_MARKET`) until he first opens the
market behind the counter (`TALK05` 99, `def_story_market_opened`, `DEF_STORY_MARKET_OPEN`). The Velvet Room's Black
Market is closed before that (he is not in the room); the houses' traders of maps 0x20C/0x27B are unchanged.

**The Time Room (step 40, the gatekeeper before the point of no return).** In the Master Room of Asuka (map 0x288, WSTAG311,
talk file `TALK03` 0xDA) Megadeath's voice opens the Warp Gate to the Gunslinger (a map event of type 14 to map 0x2DD), and
nothing there leads back. While the step is 0x28..0x2A and `DEF_STORY_GATE_PASSED` is unset, the port holds the gate shut
(`def_time_warp_closed`); stepping onto it, Igor's voice stops the player, once per step onto it (`TALK03` 311 while the
Blast quest is not done: finish Gennai's road or prove your own strength; 312 with it; `DEF_STORY_GATE_WARNED`), and the
Velvet Room's way in stands in the nook left of the gate's platform at (160,440); the room's front door brings the player
back beside it at (184,448). Through it the room is the Time Room (`def_time_on`, title "TIME / ROOM"): Igor and the three
challengers (Gennai or, while the quest is still DECEIVED, Myotismon in his shape; the Sepikmon; Oikawa), time stands still.
**No prize**: it is mandatory and measures the player's level instead of throwing them into a no-return zone. Igor
opens by recalling the Act II answer (`TALK05` 47..49 by that answer, 50 if never asked; 51 talked to again, 52 after a
lost or abandoned run, 53 all won); the guests 54 (Gennai), 55 (Myotismon), 56 (Sepikmon), 57 (Oikawa). The upper door
(open only here) starts a run of three trial areas, each with the room's music, no random battles, the area's own events
shut and its exits leading back to the room, and the challenger at his spot in VRAM place 4, the player arriving two steps in
front of him:

| Round | Area | Challenger (actor, spot) | Team (`def_time_trials`, battle template) |
|---|---|---|---|
| 1 | Asuka Bridge, mid-span (0x202, WSTAG205) | Oikawa (42, 440,392) | Mummymon Lv56, Arukenimon Lv58, VenomMyotismon Lv60 (301) |
| 2 | Genbu City's west square (0x26F, WSTAG760) | the Sepikmon (61, 300,480) | Gryphonmon Lv60, SkullMammothmon Lv62, Kimeramon Lv64 (303) |
| 3 | the middle of the Mobius Desert (0x258, WSTAG635) | the old man (57 = 0x39, 570,470) | Gennai: Valkyrimon Lv63, Knightmon Lv65, Zanbamon Lv67; Myotismon (quest DECEIVED): Puppetmon Lv63, Machinedramon Lv65, Piedmon Lv67 (303) |

A trial is one battle of the three brought in one after the other (a boss battle's own entrance, as the DRI Gauntlet's),
Megadeath's scenery and music (stage 17, `0x608C0000`), no escape; his line before it is `TALK05` 58..61 and after a win
62..65 (by row). A won trial skips the results screen (`def_time_won`: no XP, no Bits) and the next area follows; a lost one
is no game over (`def_time_defeat`, the party as it went in) and, like walking out of an area, sends the player back to the
room with the run reset (`story_gate` 0). The third win sets `DEF_STORY_GATE_PASSED` and the gate opens. The run is RAM (a
save state carries it); the save keeps the gate bits and `story_gate` (0..3 trials won in the current run). `tools/gen_time_room.py
[--apply]` builds the table: each species' stats at its level from its own rows in the game's battles
(`game_tables.stage_of`) and, as the template, an existing boss battle near the trial's top level (not one of the
special ones, none with the final bosses); the levels climb towards Galacticmon's. Spots were measured on the attribute map:
`DW3_TIME_TEST="map:x:y:step"` (hex or decimal) warps to the map at the first idle frame, then sets the step and reloads it
(0x2B also holds Galacticmon, as event 1080's end); it logs the player's position every 30 frames and the landed map's size,
and with `DW3_TIME_SURVEY` also each map event's cells (attribute layer 7) and a free-ground grid. **A set battle** (a trial,
the Oikawa's) goes through `def_time_set_battle`, which sets the scenery and music itself: `fieldstg_start_battle` leaves
them as the last battle had them, garbage right after a load, which crashed FIGHTSTG (`stage < 0` keeps them for a caller
that set them).

**A story talk stops the player** (`def_field_freeze_player`, from `story_begin`, and the Blue Room's lock): `event_running`
alone stops only the pad control, while the walk and run states (2, 3) keep moving the actor by its `speed` every frame, so
a talk set off by stepping on something (the held Warp Gate, the Protocol Ruins) had the player run on under the dialog.
It turns the footsteps' voice off and sets a walking or running player standing (step 1), without zeroing `player->speed`
(which `fieldstg_actor_init` sets once per map).

**The Guide's story conditions** (`definitive_quest.c`): a quest row whose flag is `STORY(cond)` (`DEF_STORY_FLAG` 0xF000 |
cond) asks `def_story_cond` instead of a game flag: `DEF_STORY_COND_SEIRYU_ASK` (step 4, map 0x230, "The Seiryu Leader": ask
Repeating Tom), `_SEIRYU` (step 4, map 0x230, "The missing Leader"), `_OIKAWA` (step 27, map 0x245, "The hooded man" bars the
way), `_MARKET` (steps 27..45, map 0x20F, mission "The Black Market"). `tests/port/definitive_quest.py` accepts the
`STORY(...)` rows (its driver stubs `def_story_cond` to 0).

**Saved state** (card record 2, after the Blast bytes): `story_flags` (u16: bit 0 `DEF_STORY_IGOR`, 1 `_SEIRYU_CALL`, 2
`_SEIRYU_DONE`, 3 `_OIKAWA_BEATEN`, 4 `_MARKET_OPEN`, 5 `_GATE_WARNED`, 6 `_GATE_PASSED`, 7 `_SEIRYU_ROOM`; bits 12..14 the
room's level last greeted), `story_gate`, then the 40 answer bytes. The port-owned talk entries (served in every language,
English until translated: `def_story_text_own`, `def_time_text_own`, `dw3_velvet_text_own`): `TALK01` 178; `TALK05` 16..20,
47..65, 99; `TALK06` 5..7; `TALK03` 311/312; Tom's `TALK01` 79 and the opening's event texts 1..4 are replaced only while
their hook holds (`def_story_text`).

**Hook points in the game's code** (all under `PC_PORT`): `src/fieldstg/fieldstg_event.c` (`def_story_event_dialog` before an
event dialog, `def_story_event_command` for command 0x7F, `def_story_event_script` when an event starts);
`src/fieldstg/fieldstg_80087DB0.c` (stage setup: `dw3_velvet_field_loaded`, `def_time_stage_setup`, `def_story_stage_setup`;
idle step: `def_story_frame`, `dw3_velvet_frame`, `dw3_velvet_draw`); `src/fieldstg/fieldstg_80085590.c`
(`dw3_velvet_title_window`: VELVET / SERVER or TIME / ROOM); `src/wstag/wstag820.c` (`def_story_hold_galacticmon`);
`src/wfightmn/wfightmn_800A6440.c` (`def_time_won` skips the results; `def_time_defeat`, `def_story_defeat`: no game over);
`port/game/definitive.c` (`def_time_battle_begin/_end`, `def_story_battle_begin/_end` with the other battle hooks;
`def_story_savestate`, `def_time_savestate` in `def_savestate`); declarations in `include/port.h` and `definitive.h`.

## The Extended Final Act and the farewell (spoilers)

`port/game/definitive_finale.c`, story bits `DEF_STORY_FINALE_DONE` and `DEF_STORY_IGOR_GONE`. Off in a replay script.

**For the player, in short:** beating Galacticmon is no longer the end of the fight. It rises again as a dark form you
cannot beat; the room pulls you in one last time, Igor hands back the key changed, and the door above opens on a blue
room where the friends you made along the way speak to you. Then you go back in with one partner, as a Rookie, and the
Blast is the only way forward: it becomes the **Super Chosen Evolution**, every partner you raised fusing into one golden
form for the last battle. After the credits, the next visit to the Velvet Room is Igor's goodbye, and a small friend
takes his place to read your partners' DNA bonds.

How it is built, phase by phase:

1. **Classic.** The disc's Galacticmon (battle type 6, species 442 / 466 / 467, each phase at 150% of the disc's HP,
   `DEF_FINAL_HP_PCT`) falls as in the game.
2. **Dark Galacticmon, a scripted defeat.** `wfightmn_knockout` revives it as a dark Mega (species 467,
   `DEF_RECOLOR_BLACK`, stats around 2000). The defeat is scripted: no Game Over; the player is back on the map for a
   moment and `dw3_velvet_pull_by` takes them into the last Velvet Room.
3. **The last Velvet Room and the Blue Room.** Igor asks for the Velvet Key, says it has changed and gives it back with the
   item sparkle. The upper door (0x20A) opens on the solid-blue **Bonds Room**: the screen blinks black through lines
   from the journey's allies, then they gather around the hero. The controls are locked there and the room's state is
   reset before phase 4.
4. **Super Chosen Evolution.** The strongest partner who joined (a random pick on a tie) goes back in against Dark
   Galacticmon in its Rookie form, with only the Blast open (the tutorial's lock). Setting the Blast off announces the
   Super Chosen Evolution and chains DNA fusions across every Digimon who joined (party and Digi-Lab, odd counts
   included) into a golden Imperialdramon Paladin Mode (0x17D, `DEF_RECOLOR_GOLD`, Omega Blade only, high HP; the boss's
   HP is calibrated for it). Where the build carries the Adventure score (`definitive_adventure_music.c`, generated from
   a local MIDI and not committed: without it the hook does nothing), Butter-Fly plays. Winning sets
   `DEF_STORY_FINALE_DONE` and the credits roll (0xE0B).
5. **After the credits** (progress 0x2D, `DEF_STORY_IGOR_GONE` clear): the Velvet Room opens on the Blue Room, Igor says
   goodbye (`$FinIgorBye`: "a prison, a limousine... or perhaps an elevator") and leaves; **Pipimon** (Oikawa's partner)
   takes his place as the DNA oracle for the late game (`$PipimonMeet`, `$PipimonBack`).

## Small things the story layer does

- **A choice asks twice**: on the story's answer panel the first press picks a row, the second
  confirms it, and the second press is only read 350 ms after the first, so a held or doubled button cannot answer for
  you.
- **The Guardromon network's lore**: the first time you register a Guardromon it tells, in two pages, why
  the network opens to a Tamer who walked up to it, with a pop-culture joke for each of the 13 places (English only).
