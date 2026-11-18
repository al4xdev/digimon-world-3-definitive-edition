# The Velvet Room

[Technical reference index](README.md) · [Player README](../../README.md)

## The Velvet Room

**Status: working and committed (`d927826` .. `46424ef`), with open items (below).** `port/game/definitive_velvet.c`. It runs
the bar map 0x20F (WSTAG255) as a room. **Ways in (rewritten 2026-11-18, `46424ef`; the owner vetoed drawn doors): no door
sprite.** Each inn has a spot (`doors[]`, one per inn map, the second half's copies sharing the first half's layouts) in front
of a piece of its own scenery that a faint blue light marks (`looks[]`). Standing within `VL_RADIUS` (12 px) of the spot shows
the map exits' own prompt (the balloon with the up arrow: `fieldstg_icon_create` type 10, id 6) and only **CIRCLE** enters,
as a map exit is taken; stepping off closes the icon, and a way that opened under the player (Repeating Tom's news, see the
story layer) prompts only once they stepped off and back on (`vl_armed`). The prompt belongs to no game object, so
`vl_icons_run` runs it every idle frame and `dw3_velvet_field_loaded` drops it at each stage setup. `vl_from` remembers the
inn. The ways stay closed until the story opens the room (`def_story_velvet_level() > 0`, see
[The story layer](story.md#the-story-layer-velvet-room-in-the-main-story)). Spots (map x,y: the light): 0x20A 339,192 the wall
niche's dark pane lit from inside; 0x223 205,269 the lantern on the deck's logs burns blue; 0x230 210,300 light under the
painted stage's lip; 0x238 504,296 light through the red double door's crack; 0x23F 628,392 light under the bedside chest's
lid; 0x247 540,304 light from the gap under the metal ramp's foot, on the sand; 0x249 403,187 the fallen pipe's open end;
0x25B 355,313 the floating platform's two pins run blue; 0x25D 1074,604 a mortar joint of the brick wall; 0x263 383,235
light from behind the cabin's barrels, on their rim and the post; 0x269 553,262 a slit vent in the machine's dark base;
0x26D 156,500 the last capsule lit inside its hood; 0x26F 1120,830 the alcove capsule by the hatch. The second half (0x279,
0x292, 0x29E, 0x2A5, 0x2AC, 0x2B1, 0x2B3, 0x2C4, 0x2C6, 0x2CB, 0x2D1, 0x2D6; 0x26D has no copy) uses the same spots and
lights over the digital-world palette: 25 maps, plus the Time Room's door on map 0x288 (160,440, a circuit trace of the
floor carries the light; `def_time_door`).

- **The lights** (`VelvetHint`, up to `VL_HINTS` 4 per map): `VL_HINT_GLOW` (a Gouraud fan ellipse, blue at its heart,
  nothing at its rim), `VL_HINT_SEAM` (a 1 px crack with a halo, fading in over its first quarter and out over its last),
  `VL_HINT_PIXELS` (lights the scenery's own pixels: `vl_runs`, rows read off a screenshot taken without the lights). Each
  has `power`, `front` (drawn over the foreground tiles, depth 2, instead of on the floor and walls, depth 5) and `dim`
  (first subtracts red/green, abr 2, because blue added to a light surface only whitens it). The lights are added (abr 1)
  through `DR_TPAGE` steps without dithering (dithered, a faint light is a checkerboard); one breathing pulse, a triangle of
  `VL_PULSE` 120 frames from `VL_PULSE_LOW` 150/256 to full. `DW3_VELVET_NOHINT=1` leaves the lights out (the bare
  screenshot the tuning compares against). The table and `vl_runs` were written by a tuning script from screenshots
  (`.dumps/tasks/velvet/looks/looks.py`, `runs/*.json` from `mask.py`; scratch). Each light was judged from the image alone
  by Sonnet agents (before/after, whole screen and zoom, up to 5 rounds); for 0x238, 0x25B, 0x25D, 0x263, 0x269 and 0x26F the
  judges stayed divided (too weak against too strong), so the middle was kept and the owner decides in game.
- Music: the pair `music` 0x3B with `sound` 0x60EC0000 (**set `sound`, not only `music`**), unless the room's own
  arrangement was generated: then bank 72 and key `0x61200000` (`dw3_velvet_music`, see "The room's own music" below).
  Map title "VELVET / SERVER" (`$VelvetTitle`/`$VelvetTitle2`; "TIME / ROOM" while the Time Room runs). The player comes in
  at (215,335) facing in; the exit is the front door, back to the Guardromon of the inn the way was taken in
  (`dw3_travel_node`). The upper door (the bar's map event type 1 to 0x20A) is closed, except in the Time Room before its
  gate is passed, when it leads to the trials (`def_time_side_door_open`).
- Actors (`FieldstgPlacedActor` + `FieldstgTalk`; who is present depends on the room's story level, `vl_guests[]`):
  - **Igor**, the host (actor 109, the game's suited man `P055GMGD` with a hand-drawn head, VRAM place 8, at (268,318) on the
    lower floor, dir 7 towards the door, `no_turn`); TALK05 18/19/20. See the story layer.
  - **Oikawa**, the Black Market trader (hand-drawn, actor 42 at (275,255), dir 1, `no_turn`); TALK05 8/9/12/13, and 99 the
    first time after the Bug Maze Pit.
  - **The mentor**, Gennai (the old man, actor 0x39, at (352,194) dir 1, in front of the upper door, the Time Room's way;
    owner 2026-11-18, `f8f0982`; before that on the organ stage at (318,165)); TALK05 81/82.
  - **Etemon** (actor 0x61 at (352,284), dir 5, facing the piano).
  - **The pig** (actor 318 at (226,296), dir 0, on the floor by the carpet's edge, left of the counter's front; before
    2026-11-18 at (245,292)); TALK05 79. It is the only truly unseen art of the 54 never-used actor IDs (P182BOMP, ID 318).
  - **Guilmon** (actor 0x172, upstairs by the stairs' head at (126,200), dir 0) with the Demon Bell; TALK05 77/78.
  - **Sepikmon** (actor 61, VRAM place 11, at (86,214), dir 7 towards the stairs' head: upstairs in the corner left of the
    Demon Bell's keeper; owner 2026-11-18, `e854782`/`f8f0982`; before that (176,306)), the hunts' master; TALK05 14/15
    (16/17 in the story's Seiryu hook).
  - VRAM places: of the bar's patron places the room leaves free, 8, 9 and 10 draw a figure; 7, 12 and 13 show garbage for
    any figure (tried 2026-11-17).
- `dw3_velvet_text_own()` serves the English text in every language for the port-owned TALK05 entries.
- State is RAM only (`vl_room`, `vl_from`, `vl_shop_*`): a slot saved inside the room loads as the normal bar. Savestate
  hooks: `dw3_velvet_savestate`, `def_bm_savestate`, `def_travel_savestate`, `def_evohint_savestate`,
  `def_callout_savestate`, `def_dri_savestate`.
- Talk reach: `dw3_velvet_talk_find` (the trader 30 px sideways / 16 up / 28 down; others 24x16).
- Debug env: `DW3_FEAT_VELVET`.
- **Pitfalls:** a `POLY_F4` with semi-transparency dirties the void; the layer draws what was added last, first; the
  wstag handlers table is shifted by +1.
- **Pending:** the owner's look in game at the lights (the six "divided" ones above); FR/IT/DE/ES translations of port
  texts; a save inside the room. The doors' sprites were dropped (the owner vetoed drawn doors, 2026-11-18) and the room
  was arranged on 2026-11-18 (positions above).

### Velvet Room: details, decisions and history

**Owner decisions, in order.** (1) First the Velvet Server, a room where the new NPCs live. (2) NPCs: the Black Market trader (moves
into the room) and a Gennai-like mentor (Blast Gauge / Chosen Evolution; the hook and the place only, the content is another task);
later the owner asked for a joke Etemon at the piano with the line "I heard this music in another game", through the same path
(translatable texts). (3) Scenery: Asuka City's bar, map `0x20F` (`src/wstag/wstag255.c`, background `S255PACK` = file `0x1AA`), the void
painted blue (a 16-band gradient on layer 0x1000, measured from the owner's mock-up `bar_mock_azul.png`) and the room in a blue tone.
(4) Entry: a blue door in each of the 13 inns of the Guardromon network (`nodes[]` of `definitive_travel.c`, `gs_inns[]` of
`game_mods.c`), the owner approves or moves each. (5) The front door (exit) returns to the inn; the top door stays closed. (6) Music: random
among rarely used tracks for now, the owner composes later. (7) A test build is fine, the owner adjusts later. (8) 2026-11-16: the area's
name is **Velvet Server** (the map card shows VELVET / SERVER in both languages; place names stay English in PT-BR). (9) The trader is
named **Oikawa** (Yukio Oikawa, after the Digimon character); the name is in the lines; the art is original, hand-drawn: two attempts
resembling the character and a hat seller were rejected for clashing with the game's style. (10) The mentor is, for now, an old NPC of the
game, swappable for a better one in a good corner; drawing a new NPC comes later, the focus is the mechanics. The owner's answer on the doors
(2026-11-16): redraw them: a better hand-drawn door sprite in the game's style (nothing cut from the disc) and place each beside the wardrobes
and cupboards of the maps, as in the original text of "section 6" below, instead of the inn Guardromon; "any point of the map is allowed".
(11) 2026-11-17: the room grows with the story (guests arrive by story step; see the story layer). (12) 2026-11-18: **no drawn
door at all** (vetoed): each way in is a spot before the inn's own scenery with a faint blue light, entered with CIRCLE like a
map exit; this supersedes (4) and the 2026-11-16 answer on the doors. The room's arrangement (Sepikmon, Gennai, the pig) was
redone the same day. (6) is superseded by the owner's own arrangement (below), and (5) by the Time Room (the upper door).

**How it works.** The way in is a spot of the inn (see the lights above; until 2026-11-18 it was a drawn blue door); standing on it (radius
12 px) shows the prompt, and CIRCLE (`dw3_velvet_frame`) calls `fieldstg_goto_map(0x20F, -1, 215, 335, dir 1)` with the warp sound (0x40004); `vl_from` remembers the inn. In room mode (`vl_room`,
RAM) `wstag255_setup` ends by calling `dw3_velvet_stage_setup`, which swaps `fieldstg_stage.actors` for the NPCs, the music, and paints the void.
**The room's own music** (commit `8042491`, 2026-11-17; the owner listened and accepted it, smoothing included):
an octet arrangement played by the game's own sequencer (LIBSND, `SsSeqCalledTbyT` every vsync), not a WAV.
`tools/gen_velvet_music.py` runs at CMake configure: it reads the owner's local MIDI (never committed nor shipped; path
in the script, `DW3_VELVET_MIDI` overrides) and the disc, and writes the gitignored
`port/game/definitive_velvet_music.inc`; without the MIDI or the disc nothing is written and the room keeps bank 59
(`ENV019`). `port/game/definitive_velvet_music.c` serves bank 72, a copy of BGM031's files with its own SEP score
(sequence 0 the piece, 15 empty ones) and patched tones, through `PC_PORT` hooks in `src/main/sound.c`
(`dw3_velvet_sound_bank/header/body/music/stopped/silenced`). The mix: eight parts on their own channels (horn and
double bass shared channel 8 in the MIDI and cut each other off), CC7 levels 100/76/30/66/66/71/71/82 (flute, oboe,
piano, violin, viola, horn, cello, bass) written into the score so a loop keeps them; programs 8, 5, 10, 4, 6, 1, 2, 3
of BGM031; the oboe's tone centred on 62 (at 84 it played two octaves flat); the piano is BOSS01's vag 9 copied into a
spare slot (tone volume 96, centre 66). Smoothing (why it sounded coarse: attacks of 0.2-2 ms clicked, releases of
6-46 ms cut notes at note-off, and the game zeroes the reverb depth so the tones' reverb never sounded): attacks
~37-62 ms and releases ~186-372 ms on the sustained parts (piano untouched), reverb type 3 depth 64 raised by the score
and dropped when its music stops; horn and piano get the reverb flag. Knobs at the top of the generator (`ATTACK_MS`,
`RELEASE_MS`, `REVERB_DEPTH`, `REVERB_PARTS`, `LOOP_GAP_TICKS`); `--smooth off` gives the unsmoothed mix; audition
tools in `tools/music_lab/velvet/` (`play.py --smooth on|off`, `--reverb N`; below). Test: `tests/port/definitive_velvet_music.py`
(skips without the MIDI). A second tune (Adventure) is described below; it is not in the game.

**The room's music: the handoff's details (kept from the deleted `handoff_velvet_music.md`).** Score: the owner's local MIDI
`~/Downloads/Persona 5 - Aria of the Soul - Octet - Aragi Shota.mid` (`MIDI_DEFAULT` in the generator; format 1, 480 ticks
per quarter, eight named tracks; the owner supplied it and accepted the fan-project risk; read at build time, never committed
nor shipped). The SEP is written for the port's LIBSND (`psxstack/psyq/libsnd_seq.c`, `docs/FORMATS.md` "SEP"): `pQES`,
version 0, 16 sequences when `SsSepOpen` is called with 16 (sequence 0 the piece, the other 15 an empty end-of-track);
note-off is a note-on with velocity 0 (`9n`): `8n` is **skipped and its data bytes are not consumed**; tempo is
`FF 51 tt tt tt` with **no length byte**; the end is `FF 2F 00`; after a meta event the running status is `FF`, so the next
event needs its own status byte. Tempo 600000 (100 bpm), then 659340 (~91 bpm) from tick 19200 (the MIDI's values; the
generator reads them from the file). Parts (channel, first note, note range): flute 0, tick 19200, 64-81; oboe 1, tick 28800
(~36 s: the silence before is the score, not a mute), 49-74; piano 2, tick 0, 49-83; violin 3, tick 720; viola 4, tick 0;
horn 5, tick 19200; cello 6, tick 0; double bass 7, tick 3840. 24 voices: the octet steals voices (accepted). Mix (`PARTS`:
level / CC7 = round(level x 127 / 100) / pan): flute 100/127/32, oboe 76/97/96, piano 30/38/64, violin 66/84/40, viola
66/84/88, horn 71/90/48, cello 71/90/80, bass 82/104/64 (the pans are the staging that was playing; the owner did not ask to
change them; the levels are also in `tools/music_lab/velvet/volumes.json`). An extra bank's header buffer is `0xA000`
(`sound_buffer_1`/`_2` in `src/main/sound.c`). Instruments: the flute (program 8) is not retuned (the oboe's waveform, BGM031
vag 9, whose centre 84 sits on the flute's register: leave it); the oboe is program 5 with centre 62; the piano is program 10:
BGM031's own vag 14 is a noise burst, the owner rejected it, then a darker pluck, and picked **option 19 of 51**, BOSS01's
vag 9 (program 3 of that bank, 5376 bytes, ~213 ms), copied over the spare slot of BGM031's vag 14 (5824 bytes at SPU
`vag_end[13]*8`), tone volume 96, centre 66, shift 0; the tone volume (96) and the channel level (30) are different knobs:
keep both. **What not to repeat:** writing the score over the VAB header (on the listen the header lived in `sound_buffer_1`,
the score went in `sound_buffer_2`); seeking by poking `SndSeq.pos` (a bad landing ran the parser off the stream, a zero
tempo divided by zero and the score table was garbage); trusting `nm` of a binary another build replaced for `snd`'s address
in a running process (`play.py` writes the live one to `session.txt` next to itself).

**Audition tools (`tools/music_lab/`, moved from `.dumps/velvet-live/` and `.dumps/adventure-music/`).** None patches the
disc or the source; they start their own windowed port (`build/port-sdl/dw2003 --window`) on a private debug socket and
point the playing title sequence at the score (stay on the title screen: starting a game reloads the bank). Runtime files
next to them (`game.log`, `session.txt`, `mute.mask`, `*.index`, `piano_options.json`, `render/`) are gitignored
(`tools/music_lab/.gitignore`). `tools/venv/bin/python tools/music_lab/velvet/play.py` installs the room's tune from
`gen_velvet_music` (`--smooth on|off`, `--reverb N`; offline, for numbers only: `--wav render/x.wav --seconds 50`, `--probe`
one 1 s note per part; `analyze.py` and `analyze_probe.py` compare two renders, run with `uv run --with numpy python ...`);
`... play.py adventure` plays the Adventure score instead (socket `/tmp/adventure-music.sock`, without the Velvet retune).
`velvet/solo.py` (`next`, `mute N`, `solo N`, `rem N...`, `add N`, `vol`, `vol N 0-100`, `reset`; parts 1..8 in play.py's
order; a level is written into the score so a loop keeps it); `velvet/piano.py` (`next`, `prev`, `N`, `list`, `test`:
auditions one of 51 deduplicated game samples, 80-250 ms, in the spare slot at centre 66 and solos the piano; `test` puts
it back in the full piece); `adventure/guitar.py` (`next`, `prev`, `N`, `test`, `list`: the same 51 samples on the Adventure
window's program 5, centre 64; `next`/`test` rewind to the score start, `SndSeq.start`, which must still be `FF 51`).
`velvet/offsets.c` prints LIBSND's sequencer layout on the host and `offsets.json` holds the offsets the tools poke.

**Adventure music (a second tune; first pass, not signed off, not tied to any cue).** Same method as the room's, through the
game's sequencer, no WAV, no source change: `play.py adventure`. Score: the owner's local
`~/Downloads/Digimon Adventure OP1 - Butterfly (Full) - Octet - Thymm Rayne.mid` (not in the repo, never committed): format 1,
480 ticks per quarter, one tempo 363636 (~165 bpm) at tick 0, about 335650 ticks (~4.2 minutes), 7812 note events; the built
SEP is 32009 bytes and fits `sound_buffer_2` (40960). Each named track gets its own channel (the file's shared channels do not
matter). Nothing plays at tick 0: the guitar comes in at tick 1920 (~1.4 s), so a check of the first 40 vsyncs hears
silence. Parts (`ADVENTURE_LEVELS` in `play.py`; channel, BGM031 program, first note, level, CC7, pan): Trumpet 0, 8, 74640,
90, 114, 64; ElectricGuitarOverdriven 1, 5 (sample swapped), 1920, 80, 102, 40; Cymbal 2, 10, 1950, 45, 57, 100; Doublebass 3,
4, 24960, 85, 108, 64; BassDrum 4, 6, 1728, 60, 76, 64; Harp 5, 1, 24960, 70, 89, 32; Saxophone 6, 2, 7680, 90, 114, 96;
SnareDrum 7, 3, 1950, 50, 64, 80. The programs are the loaded title bank's, longest usable sample first (8, 5, 10, 4, 6, 1,
2, 3); program 10 is BGM031's noise burst and landed on the cymbal; kick, snare and cymbal are tonal samples played at drum
notes, not a drum kit (replacing them is the same spare-slot copy, not done). The levels are a starting balance so the drums
do not bury the melody; the owner has not tuned them by ear. Guitar: program 5 of BGM031 is the flute-like waveform, so the
owner picked **option 29 of 51** in `guitar.py`: BOSS02's vag 8 (program 3 of that bank, 3872 bytes, ~154 ms) copied into the
spare slot (BGM031 vag 14, the slot the Velvet piano uses, 5824 bytes at `vag_end[13]*8`), program 5's tone pointed at it,
tone volume 96, centre 64, shift 0; channel level stays 80 (CC7 102), pan 40; only program 5's tone record changes (on the
Velvet piece program 8 shares a waveform with program 5's original vag: do not retarget the flute). The bank on that boot was
BGM031 as for the Velvet listen; `ENV019` is too small; if the tune is ever given to the room or another cue, both
`fieldstg_stage.music` and `fieldstg_stage.sound` must change. Same "what not to repeat" rules as above.

**Music pitfall (already made once):** the field plays `fieldstg_stage.sound` (the track) *after* loading the bank of `fieldstg_stage.music`;
changing only `music` changes the bank and the same tune keeps playing. The room uses the pair of map `0x25F`: `music = 0x3B`, `sound = 0x60EC0000`
(only map `0x25F` used it). Hooks: field idle step (`fieldstg_manager_update`, RUN step 0: door and drawing), `fieldstg_goto_map_delayed` ->
`dw3_velvet_leave`, `fieldstg_map_events_check` -> `dw3_velvet_event_closed(type, param)` (it takes `type, param`, not the `FieldstgMapEvent`, because
`include/port.h` is included by `common.h` without game types), `wstag255_setup`, `dw3_travel_node` (the Guardromon node getter),
`fieldstg_map_title_create_windows` -> `dw3_velvet_title_window`. Outside the room map `0x20F` is still the game's bar: entering by the normal doors
changes nothing; the bar's other door (the right one, to inn `0x20A`) is **closed** while the room runs, except in the Time Room before
its gate is passed, when it starts the trials (`def_time_side_door_open`). `dw3_velvet_event_closed` also holds shut the Warp Gate on map 0x288
(`def_time_warp_closed`), event 1080's trigger on map 0x2DF (`def_story_event_closed`) and a trial area's own events. Idle-step order
(`src/fieldstg/fieldstg_80087DB0.c`): `def_story_frame`, then `dw3_velvet_frame` (the prompt and CIRCLE), then `dw3_velvet_draw` (the
room's blue, the Time Room's frame, the call-overs, or the inn's light).

**Oikawa (the trader).** Behind the counter at (275,255), **dir 1** (front-left, the direction the counter runs, the owner's choice), `no_turn = 1`; still
**actor 42** (it gives the speech box and opens the market screen). `fieldstg_actor_draw` calls `dw3_velvet_actor_art`, which draws the hand-made art **in
place of** the actor's sprite (same ordering-table entry: the player passes in front of and behind him; the game's shadow stays). The art is hand-placed
pixels (black shoulder-length hair, plain dark coat, a gold coin in hand; one 3/4 drawing facing down-left, he does not turn) in
`.dumps/tasks/velvet/art/v2/hand.txt` (one character per pixel, 16 palette; the NPC `P068SARM` only appears faded underneath as a pose and size guide, no
disc pixel enters); `hand.py show OUT.png` renders the review (8x, over the guide, beside game NPCs, in the bar); `hand.py header
port/game/definitive_velvet_art.h` generates the header. He breathes like town NPCs (2 frames of 23 vsyncs). The port draws **one quad per colour run**:
no VRAM, no CLUT, exact colours; the 4-bit TIM route was tried and **discarded** because the upload (via `tim.load`) dirtied the scenery's VRAM. Tried and
vetoed: giving him the hero's file (Girls Power repaints it for Kail, so there were two identical heroes). Reach: the game's interaction box
(`fieldstg_actor_find_at`) is 16 x 8 around each figure and the counter's front row left the player's point 12 to 23 px below him (nobody reached him;
at (275,244) he was out of reach), so when the game finds nobody `fieldstg_actor_talk_to` asks `dw3_velvet_talk_find`: the trader 30 px sideways, 16 up and 28
down (the counter row x 256 to 300); the others 24 x 16; the nearest wins. Tested (`.dumps/tasks/velvet/talktest4.py`, **one process per point**: it
does not reload the state between points, the previous balloon stays open and the circle advances it): trader, mentor, Demon Bell Guilmon, Etemon and
the pig answer. His lines (EN + PT-BR, `TALK05` entries 8, 9, 12, 13): the two greetings **alternate on each visit**; the "bought something" (12) and "left without
buying" (13) lines appear on the **next visit** (the market screen has nowhere to show text after itself) and **once per sequence**, then the greeting alternates
again. Flow: talk -> Oikawa's line in the game dialogue -> the Black Market screen opens -> leave -> the next visit comments how the previous ended. State is RAM
(`vl_shop_*`). The same hand-drawn art is drawn over the Time Room's first challenger and the hooded man of the Bug Maze Pit
(`vl_dealer`, `def_story_oikawa_figure`). Since the story layer he is behind the counter only from the room's level 5 (the Oikawa
beaten), and his first line there is `TALK05` 99 (`VL_SHOP_OIKAWA`).

**The other figures.** (Positions below are the 2026-11-17 ones; the 2026-11-18 arrangement is in the Actors list above.) Mentor = actor `0x39` (`P074JIJI`, the body of the "Grandpa ..." and the Tamer Genji: bald, white beard, 24x32, 5 directions; fits VRAM
slot 5) on the stage between the organ pipes at (318,165) dir 1 (2026-11-17; before that the Kotemon `0x178`, and `376`'s Kotemon `P189PKEL.BIN`/`0x19`, 32x32,
1 animation, in a previous try). The stage is a dead end joined to the upper floor by a strip (y 160..176): it blocks no way, the entry camera already shows
its red lights, and he can be talked to from any side. Alternatives seen: Piximon (the Adventure trainer, 48x40: does not fit the slot), Wizardmon (32x40), Master
John. The slot `vram_places[5]`: the neighbour uses u=32, so the frame must be at most 32 px wide. Etemon: actor `0x61` (97, vram 4) **back to the room, facing the
piano** at (352,284) **dir 5** (the keyboard is up and to his right, so 5 points to the keys); he turns when the player talks. The pig: actor 318 (`0x13E`, vram 6) at
(245,292), the only art in the game that no map uses; the line is the owner's joke ("somebody found me in a pile of toys nobody used"), `TALK05` 79. Guilmon (actor
`0x172`, vram place 3) lives on the upper floor; his talk toggles the key item **26** (a Japanese-tag slot the US disc never gives, renamed "Demon Bell" /
"Sineta Demoniaca" in `en.txt`/`pt.txt`); lines `TALK05` 77 (handed over) and 78 (taken back). `TALK05` 81 and 82 (the mentor) and the others are entries of the bar's
talk file (`records_language + 0xE8`) that on the disc hold only the letter "a" and nobody uses; the port overrides them in EN and PT-BR (`tools/gen_text_override.py`),
and `dw3_velvet_text_own()` (list in `definitive_velvet.c`) marks the entries that are **port text**, so `def_text_override` serves the English text in **any** language
(proved with `records_language = 3`, French: the balloon shows the English line instead of "a"). Translating those lines to FR/IT/DE/ES is the general
translation task; PT-BR has its own. The mentor's content was a placeholder until the Blast task wrote the real one (now the Gennai quest).

**Doors (the 13 positions, done properly). History: superseded on 2026-11-18** by the lights on the scenery (no door sprite; the spots moved, see
the lead of this section); kept for the method. The first attempt was an algorithm plus a contact sheet (the owner caught a door in front of a Digimon and in the middle
of the scenery); the second was by eye over 13 small screens and misjudged what was behind several. Now each was checked in its own map at 2x with the actors where the game
puts them (the search excludes 56 px around every actor). The door is a **door-shaped portal**: a dark frame (two jambs, a lintel and a sill) with light in 6
bands (dim at the lintel, bright at the floor), so it stands anywhere and does not look like a plaque; depth **3** (the layer draws back to front): in front of floor and
walls and **behind the player** (at depth 2 the player vanished behind the door). Positions: 0x20A (339,192) right wall by the stairs; 0x223 (205,269) end of the deck's
logs; 0x230 (516,380) stair landing, stained glass; 0x238 (300,440) shop floor (no wall; nothing in front); 0x23F (580,392) white counter wall; 0x247 (540,304) foot of the
metal ramp (no wall); 0x249 (403,187) stone step of the trail; 0x25B (355,313) platform edge (no wall); 0x25D (1074,604) brick low wall; 0x263 (383,235) log wall by the
armchair; 0x269 (553,262) face of the machine block; 0x26D (396,520) tower wall; 0x26F (1156,833) metal wall far from the Digimon. Second half: the same coordinates in 0x279,
0x292, 0x29E, 0x2A5, 0x2AC, 0x2B1, 0x2B3, 0x2C4, 0x2C6, 0x2CB, 0x2D1, 0x2D6 (`0x26D` has no copy): 25 maps in all. Compromise: 0x223, 0x238, 0x247 and 0x25B have **no wall** where a
door fits without covering an actor or prop, so there it stands on open floor held up by its own frame. The `doors` table is not `const` on purpose (`doors_try.py MAP:x:y ...`
moves doors in RAM and takes a screenshot of each without rebuilding). Proofs: `.dumps/tasks/velvet/doors/*.png` (the 25 screens), `doors_final_a.png`, `doors_final_b.png`.

**Unused-actor study (2026-11-16)**, `.dumps/tasks/velvet/unused/REPORT.md` (63 preview sheets, `_shortlist.png`, `unused_ids.txt`; scratch tools `tim.py`, `render.py`, `contact.py`,
`sprites_fixed.py` decode 4-bit TIM from the disc: the old `sprites.py` read the TIM wrongly, CLUT in bytes and image `w`/`h` in words, frame `w*4` x `h`, and is broken: do not use). 406 actor ids
in `fieldstg_actor_sprite_files`; 342 are placed by some of the 293 stages; of the remaining 64, 9 the engine uses without placing (id 2 = spare player, 3..10 = the 8 partners), leaving
**54 never-used ids**. "Unused id" is not unseen art: 54 of those 64 draw the same sub-file as an id the game places; the only art nobody sees is `P182BOMP` (id 318), **a pig**. Identified by
twin sprite and the `<7>Name<7>` tag of lines: 29 Divermon; 188-191/387/388 Etemon; 207 MetalEtemon; 233/234 Patamon; 236 Renamon; 237/238 Kotemon; 240 Monmon; 248-250 Knightmon;
286 WaruMonzaemon; 312/313 soldier; 366 Agumon; 368 Veemon; 370 Guilmon; 372 Patamon; 374 Renamon; 376 Kotemon; 378/380 Kumamon; 17/18/19 generic bodies; 177/251 pigs; 275-279 "Guard
A/B/C"; 353-360 are the BLOK objects (0x148/0x15F/0x160), not NPCs.

**Savestate fixes.** The state of the Black Market screen (`bm_ui`: open, cursor, windows in the field heap) and the room's (`vl_room`, `vl_from`, `vl_shop_*`) were not in
the savestate (loading a state from before the shop opened left the screen "open" over windows the heap no longer had: the player froze, the third consecutive load
segfaulted); now `def_bm_savestate` and `dw3_velvet_savestate` are in `def_savestate`, tested with 7 consecutive loads alternating trader (shop opening), Etemon and pig
(`talktest3.py`). On 2026-11-17 the same was done for `def_travel_savestate` (the Guardromon screen's `nt`), `def_evohint_savestate` (`eh`), `def_callout_savestate` (`callout`) and
`def_dri_savestate` (which now also carries `dri`, `dri_wait` and the list's text buffers), all registered in `def_savestate` (`definitive.c`); proof `ss/travel_ss.py BIN OUT` (opens
the Guardromon screen at Asuka's inn, saves before and with it open, loads alternating 4 times and checks open/closed, cursor, triangle and floor): the old binary **crashes on the first
load**, the new one passes the 25 checks.

**Verification done.** `trigger.py` (the 25 doors, at the final positions, lead to map `0x20F`, 0 failures, reads the table directly from `definitive_velvet.c` via `doors_of.py`);
`e2e.py` (inn `0x20A` -> door -> room -> talk to Etemon (dialogue with the port's line) -> front door -> back at `0x20A` on the Guardromon); `gen_text_override.py --check`; `tests/port/run.py`
(the room is off under `--script`); `tests/port/field.py --only lobby_lab_exit items_list` ok (the `swamp_dive`/`swamp_flies` cases **already failed before** on a `write_mem` step that this
checkout's `psxstack/runtime/script.c` does not implement, "unknown step type", unrelated); the SDL3 build (`build/port-sdl`) compiles and the room looks the same. Scratch tools in
`.dumps/tasks/velvet/`: `cycle.sh COMMAND...` (rebuild, regenerate `field.state`, run), `shot.py MAP X Y DIR out [--poke sym=value] [--player X:Y:DIR]`, `doors_try.py`, `walk.py`, `montage.py`,
`trigger.py`, `e2e.py`, `attr.py` (collision map as text; `--around X Y`), `vrun.py` (runs the scratch tools against another binary, `DW3_BIN=...`); a private build `build/port-velvet/dw2003`
was used so as not to fight other sessions over `build/port` (a worktree via `wtsync.sh` while the Black Market was under construction).
The 2026-11-18 prompt (scratch `.dumps/tasks/story/time/prompt.py`): no icon off the spot; on the spot the icon and **no entry in 180 frames without
CIRCLE**; stepping off closes it, coming back reopens it; CIRCLE enters (map 0x20F); passed on 0x23F, 0x25D (x > 960) and 0x288 (opens the Time Room).
`def_story_savestate` and `def_time_savestate` are registered in `def_savestate` too (the story's talk, dream, choice panel and Oikawa state; the Time
Room's run).

**Code map.** Stage `0x20F`: `src/wstag/wstag255.c` (setup, `actors[]`, `sprites[]`, `map_events[]`); stage code per map `fieldstg_stages[240]` (`{id, file, entry}`); talk `fieldstg_actor_talk_to` (`def_bm_talk` /
`def_dri_talk` before the redirect); traders `traders[]` / `def_bm_talk` in `definitive_market_screen.c`; door/exit between maps `FieldstgMapEvent` (flags, type 1 = move, x, y, dir, route, room,
`include/fieldstg.h`); map spots (layer 7) `fieldstg_map_events_check`: `attr & 0x1F` = index, `attr >> 5` = direction; music `fieldstg_stage.music` (wstag255 uses 8; the room 0x3B); background
`fieldstg_stage.background_file`.

**The original text of the next tasks (owner, 2026-11-16), "section 6: the world, organic onboarding and the Velvet Room"**: hidden room reached through mysterious doors beside the wardrobes and
cupboards scattered through the game's maps (inspired by the Persona Velvet Room and Gennai); inhabitants: the Black Market merchant (rare materials and replicas) and a Gennai-inspired mentor
who explains the risk of the Cursed Evolution, runs the Purification Gauntlet and grants the Chosen Evolution. **Organic onboarding without boring tutorials (out of scope for now, the owner is changing
the main quest in another session and will create guide quests later):** small mandatory hooks in the main progression: make the player interact with a Guardromon right after unlocking the second
teleport point (teaches the fast-travel network in practice), and steer the player through one of the wardrobe doors for the first time when rare Hunt Materials start to appear. This is the open
"onboarding hooks" item.

**Pending / questions to the owner (2026-11-17, updated 2026-11-18):** test in game (enter an inn, find the light, take it with CIRCLE, talk to every figure, leave by the front); look at each
inn's light (the six "divided" ones first) or say "in that inn move it / make it stronger or weaker"; approve or replace the mentor's figure (the old man `0x39` until one is drawn); the room's music
is done (the owner's arrangement, above); the door sprites and the 13 door positions were dropped with the doors; FR/IT/DE/ES lines (the port override
covers English and PT-BR only, `dw3_velvet_text_own()` serves English elsewhere); **saving on the memory card inside the room**: the save falls on the generic `0xC00` and loading opens `0x20F` as the
normal bar (`vl_room` is RAM, not on the card; the port's savestate carries it since 2026-11-16): acceptable? if not, it is the next decision. The owner's rules (summary): pt-BR with the owner,
no commit without being asked, no AI attribution in repo files, scratch in `.dumps/`, plans in `.plan/tasks/`, move files to `/tmp` instead of `rm`, "no 'I don't know, wait': measure and go; ask only
what is the owner's decision".
