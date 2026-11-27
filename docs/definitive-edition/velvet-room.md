# The Velvet Room

[Technical reference index](README.md) · [Player README](../../README.md)

## The Velvet Room

`port/game/definitive_velvet.c`. It runs the bar map 0x20F (WSTAG255) as a room. **Ways in: no door sprite.** Each inn
has a spot (`doors[]`, one per inn map, the second half's copies sharing the first half's layouts) in front of a piece
of its own scenery that a faint blue light marks (`looks[]`). Standing within `VL_RADIUS` (12 px) of the spot shows the
map exits' own prompt (the balloon with the up arrow: `fieldstg_icon_create` type 10, id 6) and only **CIRCLE** enters,
as a map exit is taken; stepping off closes the icon, and a way that opened under the player (Repeating Tom's news, see
the story layer) prompts only once they stepped off and back on (`vl_armed`). The prompt belongs to no game object, so
`vl_icons_run` runs it every idle frame and `dw3_velvet_field_loaded` drops it at each stage setup. `vl_from` remembers
the inn. The ways stay closed until the story opens the room (`def_story_velvet_level() > 0`, see [The story
layer](story.md#the-story-layer-velvet-room-in-the-main-story)). Spots (map x,y: the light): 0x20A 339,192 the wall
niche's dark pane lit from inside; 0x223 205,269 the lantern on the deck's logs burns blue; 0x230 210,300 light under
the painted stage's lip; 0x238 504,296 light through the red double door's crack; 0x23F 628,392 light under the bedside
chest's lid; 0x247 540,304 light from the gap under the metal ramp's foot, on the sand; 0x249 403,187 the fallen pipe's
open end; 0x25B 355,313 the floating platform's two pins run blue; 0x25D 1074,604 a mortar joint of the brick wall;
0x263 383,235 light from behind the cabin's barrels, on their rim and the post; 0x269 553,262 a slit vent in the
machine's dark base; 0x26D 156,500 the last capsule lit inside its hood; 0x26F 1120,830 the alcove capsule by the
hatch. The second half (0x279, 0x292, 0x29E, 0x2A5, 0x2AC, 0x2B1, 0x2B3, 0x2C4, 0x2C6, 0x2CB, 0x2D1, 0x2D6; 0x26D has
no copy) uses the same spots and lights over the digital-world palette: 25 maps, plus the Time Room's door on map 0x288
(160,440, a circuit trace of the floor carries the light; `def_time_door`).

- **The lights** (`VelvetHint`, up to `VL_HINTS` 4 per map): `VL_HINT_GLOW` (a Gouraud fan ellipse, blue at its heart,
  nothing at its rim), `VL_HINT_SEAM` (a 1 px crack with a halo, fading in over its first quarter and out over its last),
  `VL_HINT_PIXELS` (lights the scenery's own pixels: `vl_runs`, rows read off a screenshot taken without the lights). Each
  has `power`, `front` (drawn over the foreground tiles, depth 2, instead of on the floor and walls, depth 5) and `dim`
  (first subtracts red/green, abr 2, because blue added to a light surface only whitens it). The lights are added (abr 1)
  through `DR_TPAGE` steps without dithering (dithered, a faint light is a checkerboard); one breathing pulse, a triangle of
  `VL_PULSE` 120 frames from `VL_PULSE_LOW` 150/256 to full. `DW3_VELVET_NOHINT=1` leaves the lights out (the bare
  screenshot the tuning compares against). The table and `vl_runs` were tuned from screenshots.
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
  - **The mentor**, Gennai (the old man, actor 0x39, at (352,194) dir 1, in front of the upper door, the Time Room's
    way); TALK05 81/82.
  - **Etemon** (actor 0x61 at (352,284), dir 5, facing the piano).
  - **The pig** (actor 318 at (226,296), dir 0, on the floor by the carpet's edge, left of the counter's front); TALK05 79. It is the only truly unseen art of the 54 never-used actor IDs (P182BOMP, ID 318).
  - **Guilmon** (actor 0x172, upstairs by the stairs' head at (126,200), dir 0) with the Demon Bell; TALK05 77/78.
  - **Sepikmon** (actor 61, VRAM place 11, at (86,214), dir 7 towards the stairs' head: upstairs in the corner left of the
    Demon Bell's keeper), the hunts' master; TALK05 14/15
    (16/17 in the story's Seiryu hook).
  - VRAM places: of the bar's patron places the room leaves free, 8, 9 and 10 draw a figure; 7, 12 and 13 show garbage for
    any figure.
- `dw3_velvet_text_own()` serves the English text in every language for the port-owned TALK05 entries.
- State is RAM only (`vl_room`, `vl_from`, `vl_shop_*`): a slot saved inside the room loads as the normal bar. Savestate
  hooks: `dw3_velvet_savestate`, `def_bm_savestate`, `def_travel_savestate`, `def_evohint_savestate`,
  `def_callout_savestate`, `def_dri_savestate`.
- Talk reach: `dw3_velvet_talk_find` (the trader 30 px sideways / 16 up / 28 down; others 24x16).
- **The pig's stat swap (respec):** `port/game/definitive_respec_screen.c`. Talking to the pig (`VL_PIG`, `TALK05` 79) sets `vl_pig_phase = 1`, and `dw3_velvet_draw` calls `def_respec_offer()` once the dialogue closes. `def_respec_frame()` runs in the field idle chain (`src/fieldstg/fieldstg_80087DB0.c`) and draws the board over two columns: the 6 core stats (`record.stats.values[6..11]`: Str, Def, Spi, Wis, Spd, Cha) on the left and the 7 elemental Affinities (`values[12..18]`) on the right (HP, MP and TP are excluded). For a price that climbs with each swap of the save (`def_respec_price`: 1,000, 5,000, 10,000, 20,000, 30,000, 45,000, 60,000, 75,000, 90,000, then 100,000 for good; the count of swaps lives in the expansion save's `"respec"` section, `definitive_respec_save.c`), it moves `DEF_RESPEC_POINTS` (100) raw points from one stat to another on the same partner; the source must be >= 101 (so the raw stat never drops below 1) and the destination + 100 must be <= 999 (the raw cap from `gamestate_add_stat`). Because raw stats live on `GamestateRecord` and battles only add equipment/form bonuses on top, swaps persist across digivolutions and memory card saves without a new flag (the price's swap count aside). Controls: `L1`/`R1` switches partner, `CROSS` picks source -> destination -> confirms payment, `TRIANGLE` steps back or closes. Registered in `def_savestate` via `def_respec_savestate` (`vl_pig_phase` in `dw3_velvet_savestate`).
- Debug env: `DW3_FEAT_VELVET`.
- **Pitfalls:** a `POLY_F4` with semi-transparency dirties the void; the layer draws what was added last, first; the
  wstag handlers table is shifted by +1.

### Etemon's jukebox: the player's own MIDI as the random battles' music

Talk to Etemon at the piano (story step 22 and later, as before): his joke line first, then a list opens (`definitive_midi_screen.c`,
a field overlay run from FIELDSTG's idle step like the pig's stat swap; up/down move, the confirm button chooses, back leaves). The
first row is the game's own music (the default), then "All, shuffled" (a random file each battle, never the previous one) and "All, in order" (the next file by name, wrapping; the position is kept); the others are the `.mid` / `.midi` files of the player's folder, sorted without
regard to case, those that do not convert left out. The chosen row is green. The choice is for the **random battles**
(`fieldstg_encounter_start`: the Rare Targets and the Rush's pick are the same encounter). Scripted battles, bosses, the Super Rares
and the Time Room keep their own music. Pick the first row, or delete the file, and the game's music is back, with no message.

**Volume and preview**. Each file has its own volume: left and right move it in steps of 10%, from
0 up to 400%; it starts at 50% (a loud MIDI file is the common case, and the screen says so: "Starts quiet (50%) for
safety. L/R to 400%."), and the bar under the list shows it. Square (X on an Xbox pad) plays the file under the cursor
right there in the room, so the level can be set by ear; L1 / R1 jump a page.

**Where the files live.** A `midi` folder beside the settings file (`settings.json`, the memory cards `card1.mcd`/`card2.mcd`: the
launcher owns that folder, psxstack's `port_settings.dir`), or the folder named by `DW3_MIDI_DIR` (tests, power users). The port
makes it when the list is first opened and the list shows its tail, so a player finds it from the game.

| platform | folder |
|---|---|
| Linux | `~/.local/share/dw2003/midi` (the launcher's per-user directory, SDL's pref path: where `card1.mcd` is), or `midi` beside the binary in portable mode |
| Windows | `%APPDATA%\dw2003\midi` (the same), or beside the exe in portable mode |
| Android | the app's files directory (`Android/data/com.al4xdev.dw2003/files/midi`, beside `settings.json`; the app starts in that folder) |
| PortMaster handhelds | `ports/dw3/midi` (the package folder, next to `settings.json`) |

The choice is in `battle_music.txt` in that folder (`mode: own|file|shuffle|order`, `file:` or `last:`). It is the
install's, not the save's: the files are the player's, on this device. A file that does not convert is skipped in a
playlist; with none, the game's music.

**How it plays (the pipeline).** The same one as the Velvet Room's and the Adventure scores, but converted at run time
in C (`definitive_midi.c`) instead of at configure time by a generator: the game's LIBSND plays a SEP (the disc's
sequence format, `docs/FORMATS.md` "SEP") on the samples of BGM031 (sound bank 33) under a bank id of its own, **74**
(72 is the room's, 73 the Adventure score's: a bank loaded under its id is not loaded again, and BGM031 must keep its
own score). `sound.c`'s PC_PORT hooks copy BGM031's record, and write the SEP where BGM031's own is in the header
buffer of an extra entry (`dw3_midi_sound_bank/header`); `def_midi_battle_music` returns the key `0x60000000 | 74 <<
18` for `records_state.music`, which the battle plays after loading `(music >> 18) & 0x7F` as an extra bank
(`wfightmn`). When the chosen file's SEP differs from the one the loaded bank holds (another file, or the same one
edited), the entries that hold bank 74 forget it (`dw3_midi_sound_forget`) and the next battle loads it again. The
converter reads format 0 and 1 files in metric time and keeps the notes, the tempo map, the programs and the channel
volume and pan (pitch bend, aftertouch and the other controllers are dropped: the SEP's room is for the notes), merged
by tick with each file's channel kept; the General MIDI programs map onto the instruments BGM031 has (the disc has no
GM set: 1 pianos, guitars and tuned percussion, 2 brass and the cello, 3 basses, 4 violin, 5 organs and reeds, 6 viola,
ensembles and pads, 8 flutes, pipes and leads); channel 10 maps note by note onto the four drum tones of program 0
(kick 36, snare 38, hat 44, cymbal 49; other drum notes are dropped); BGM031's oboe tone is re-centred (62) as the
room's score does. The piece loops as the disc's BGMs do (NRPN 20 / 30). **Limits:** the SEP gets what the buffer has
past BGM031's VAB header, about 31 KB (roughly 4000 notes: three to five minutes of a full band, more of a small one):
a longer file is cut where it fits and every sounding note is closed there (the list says so on choosing it: "its end
is cut"), every melodic tone gets one shared attack/release and the reverb (type 3, depth 64, unlike the room's
per-instrument tuning), no piano sample (program 1's tone plays the pianos), 24 SPU voices shared with the sound
effects, files up to 3 MB. Conversion costs a few milliseconds a file; the list converts each file once when it opens
(to leave out the ones that do not play) and a battle converts the chosen one when it starts.

Tests: `tests/port/definitive_midi.py` (the module alone: list, choice file, a path out of the folder refused, the converter's
notes, drums, programs, tempo map and the cut, the bank hook on BGM031's real header file, the strings in six languages) and
`tests/port/definitive_midi_battle.py` (a random battle on map `0x233` from a card the first-battle script saves: the chosen file's
key plays and the audio is not silent; with nothing chosen or the file gone it is the game's own).

### Velvet Room: implementation details

**How it works.** Standing on an inn's spot shows the prompt, and CIRCLE (`dw3_velvet_frame`) calls
`fieldstg_goto_map(0x20F, -1, 215, 335, dir 1)` with the warp sound (0x40004); `vl_from` remembers the inn. In room mode
(`vl_room`, RAM) `wstag255_setup` ends by calling `dw3_velvet_stage_setup`, which swaps `fieldstg_stage.actors` for the
NPCs and the music, and paints the void blue (a 16-band gradient on layer 0x1000). Place names stay English in PT-BR.

**The room's own music:** an octet arrangement played by the game's own sequencer (LIBSND, `SsSeqCalledTbyT` every
vsync), not a WAV. `tools/gen_velvet_music.py` runs at CMake configure: it reads a local MIDI (never committed nor
shipped; path in the script, `DW3_VELVET_MIDI` overrides) and the disc, and writes the gitignored
`port/game/definitive_velvet_music.inc`; without the MIDI or the disc nothing is written and the room keeps bank 59
(`ENV019`). `port/game/definitive_velvet_music.c` serves bank 72, a copy of BGM031's files with its own SEP score
(sequence 0 the piece, 15 empty ones) and patched tones, through `PC_PORT` hooks in `src/main/sound.c`
(`dw3_velvet_sound_bank/header/body/music/stopped/silenced`). The mix: eight parts on their own channels (horn and
double bass shared channel 8 in the MIDI and cut each other off), CC7 levels 100/76/30/66/66/71/71/82 (flute, oboe,
piano, violin, viola, horn, cello, bass) written into the score so a loop keeps them; programs 8, 5, 10, 4, 6, 1, 2, 3
of BGM031; the oboe's tone centred on 62 (at 84 it played two octaves flat); the piano is BOSS01's vag 9 copied into a
spare slot (tone volume 96, centre 66). Smoothing (why it sounded coarse: attacks of 0.2-2 ms clicked, releases of 6-46
ms cut notes at note-off, and the game zeroes the reverb depth so the tones' reverb never sounded): attacks ~37-62 ms
and releases ~186-372 ms on the sustained parts (piano untouched), reverb type 3 depth 64 raised by the score and
dropped when its music stops; horn and piano get the reverb flag. Knobs at the top of the generator (`ATTACK_MS`,
`RELEASE_MS`, `REVERB_DEPTH`, `REVERB_PARTS`, `LOOP_GAP_TICKS`); `--smooth off` gives the unsmoothed mix; audition
tools in `tools/music_lab/velvet/` (`play.py --smooth on|off`, `--reverb N`; below). Test:
`tests/port/definitive_velvet_music.py` (skips without the MIDI). A second tune (Adventure) is described below; it is
not in the game.

**The score.** The MIDI (`MIDI_DEFAULT` in the generator: format 1, 480 ticks per quarter, eight named tracks) is read
at build time. The SEP is written for the port's LIBSND (`psxstack/psyq/libsnd_seq.c`, `docs/FORMATS.md` "SEP"): `pQES`,
version 0, 16 sequences when `SsSepOpen` is called with 16 (sequence 0 the piece, the other 15 an empty end-of-track);
note-off is a note-on with velocity 0 (`9n`): `8n` is **skipped and its data bytes are not consumed**; tempo is
`FF 51 tt tt tt` with **no length byte**; the end is `FF 2F 00`; after a meta event the running status is `FF`, so the
next event needs its own status byte. The oboe starts at ~36 s: the silence before is the score, not a mute. 24 voices:
the octet steals voices. Levels, CC7 and pans are in `PARTS` (also `tools/music_lab/velvet/volumes.json`). An extra
bank's header buffer is `0xA000` (`sound_buffer_1`/`_2` in `src/main/sound.c`). Instruments: do not retune the flute
(program 8: the oboe's waveform, whose centre 84 sits on the flute's register); the piano is BOSS01's vag 9 (program 3 of
that bank, ~213 ms) copied over the spare slot of BGM031's vag 14 (at SPU `vag_end[13]*8`); its tone volume (96) and the
channel level (30) are different knobs. **Pitfalls:** do not write the score over the VAB header; do not seek by poking
`SndSeq.pos` (a bad landing runs the parser off the stream and a zero tempo divides by zero); do not trust `nm` of a
binary another build replaced for `snd`'s address in a running process (`play.py` writes the live one to `session.txt`).

**Audition tools (`tools/music_lab/`).** None patches the
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

**Adventure music (a second tune, not tied to any cue).** Same method as the room's, through the game's sequencer, no
WAV, no source change: `play.py adventure`. Score: a local `~/Downloads/Digimon Adventure OP1 - Butterfly (Full) -
Octet - Thymm Rayne.mid` (not in the repo, never committed): format 1, 480 ticks per quarter, one tempo 363636 (~165
bpm) at tick 0, about 335650 ticks (~4.2 minutes), 7812 note events; the built SEP is 32009 bytes and fits
`sound_buffer_2` (40960). Each named track gets its own channel (the file's shared channels do not matter). Nothing
plays at tick 0: the guitar comes in at tick 1920 (~1.4 s), so a check of the first 40 vsyncs hears silence. Parts
(`ADVENTURE_LEVELS` in `play.py`; channel, BGM031 program, first note, level, CC7, pan): Trumpet 0, 8, 74640, 90, 114,
64; ElectricGuitarOverdriven 1, 5 (sample swapped), 1920, 80, 102, 40; Cymbal 2, 10, 1950, 45, 57, 100; Doublebass 3,
4, 24960, 85, 108, 64; BassDrum 4, 6, 1728, 60, 76, 64; Harp 5, 1, 24960, 70, 89, 32; Saxophone 6, 2, 7680, 90, 114,
96; SnareDrum 7, 3, 1950, 50, 64, 80. The programs are the loaded title bank's, longest usable sample first (8, 5, 10,
4, 6, 1, 2, 3); program 10 is BGM031's noise burst and landed on the cymbal; kick, snare and cymbal are tonal samples
played at drum notes, not a drum kit (replacing them is the same spare-slot copy, not done). The levels are a starting
balance so the drums do not bury the melody (not tuned by ear). Guitar: program 5 of BGM031 is the flute-like waveform,
so it uses **option 29 of 51** of `guitar.py`: BOSS02's vag 8 (program 3 of that bank, 3872 bytes, ~154 ms) copied into
the spare slot (BGM031 vag 14, the slot the Velvet piano uses, 5824 bytes at `vag_end[13]*8`), program 5's tone pointed
at it, tone volume 96, centre 64, shift 0; channel level stays 80 (CC7 102), pan 40; only program 5's tone record
changes (on the Velvet piece program 8 shares a waveform with program 5's original vag: do not retarget the flute).
`ENV019` is too small for it; if the tune is ever given to the room or another cue, both `fieldstg_stage.music` and
`fieldstg_stage.sound` must change. The pitfalls above apply.

**Music pitfall:** the field plays `fieldstg_stage.sound` (the track) *after* loading the bank of
`fieldstg_stage.music`; changing only `music` changes the bank and the same tune keeps playing. The room uses the pair
of map `0x25F`: `music = 0x3B`, `sound = 0x60EC0000` (only map `0x25F` used it). Hooks: field idle step
(`fieldstg_manager_update`, RUN step 0: prompt and drawing), `fieldstg_goto_map_delayed` -> `dw3_velvet_leave`,
`fieldstg_map_events_check` -> `dw3_velvet_event_closed(type, param)` (it takes `type, param`, not the
`FieldstgMapEvent`, because `include/port.h` is included by `common.h` without game types), `wstag255_setup`,
`dw3_travel_node` (the Guardromon node getter), `fieldstg_map_title_create_windows` -> `dw3_velvet_title_window`.
Outside the room map `0x20F` is still the game's bar: entering by the normal doors changes nothing; the bar's other
door (the right one, to inn `0x20A`) is **closed** while the room runs, except in the Time Room before its gate is
passed, when it starts the trials (`def_time_side_door_open`). `dw3_velvet_event_closed` also holds shut the Warp Gate
on map 0x288 (`def_time_warp_closed`), event 1080's trigger on map 0x2DF (`def_story_event_closed`) and a trial area's
own events. Idle-step order (`src/fieldstg/fieldstg_80087DB0.c`): `def_story_frame`, then `dw3_velvet_frame` (the
prompt and CIRCLE), then `dw3_velvet_draw` (the room's blue, the Time Room's frame, the call-overs, or the inn's
light).

**Oikawa (the trader).** Behind the counter at (275,255), **dir 1** (front-left, the direction the counter runs),
`no_turn = 1`; still **actor 42** (it gives the speech box and opens the market screen). `fieldstg_actor_draw` calls
`dw3_velvet_actor_art`, which draws the hand-made art **in place of** the actor's sprite (same ordering-table entry: the
player passes in front of and behind him; the game's shadow stays). The art is hand-placed pixels (one 3/4 drawing, 16
colours) in `port/game/definitive_velvet_art.h`; no disc pixel enters. He breathes like town NPCs (2 frames of 23
vsyncs). The port draws **one quad per colour run**: no VRAM, no CLUT (a 4-bit TIM uploaded with `tim.load` dirties the
scenery's VRAM). Do not give him the hero's file: Girls Power repaints it. Reach: the game's interaction box
(`fieldstg_actor_find_at`) is 16 x 8 around each figure and the counter keeps the player 12 to 23 px below him, so when
the game finds nobody `fieldstg_actor_talk_to` asks `dw3_velvet_talk_find`: the trader 30 px sideways, 16 up and 28 down
(the counter row x 256 to 300); the others 24 x 16; the nearest wins. His lines (`TALK05` 8, 9, 12, 13): the two
greetings alternate on each visit; the "bought something" (12) and "left without buying" (13) lines appear on the next
visit, once per sequence. Flow: talk -> his line -> the Black Market screen -> leave. State is RAM (`vl_shop_*`). The same
art is drawn over the Time Room's first challenger and the hooded man of the Bug Maze Pit (`vl_dealer`,
`def_story_oikawa_figure`). He is behind the counter only from the room's level 5 (the Oikawa beaten), and his first line
there is `TALK05` 99 (`VL_SHOP_OIKAWA`).

**Talk entries.** Guilmon's talk toggles key item **26** (a Japanese-tag slot the US disc never gives, renamed "Demon
Bell" in `en.txt`). The room's lines are entries of the bar's talk file (`records_language + 0xE8`) that on the disc hold
only the letter "a"; the port overrides them, and `dw3_velvet_text_own()` marks the entries that are **port text**, so
`def_text_override` serves the English text in any language that lacks a line. A figure in VRAM slot `vram_places[5]` must
be at most 32 px wide (the neighbour uses u=32).

**Savestates.** Every screen that keeps windows in the field heap must be in `def_savestate`, or loading a state saved
before it opened freezes or crashes: `def_bm_savestate`, `dw3_velvet_savestate`, `def_travel_savestate`,
`def_evohint_savestate`, `def_callout_savestate`, `def_dri_savestate`, `def_story_savestate`, `def_time_savestate`.

**Actor art.** 406 actor ids are in `fieldstg_actor_sprite_files`; 54 are never used by any stage or by the engine, and
all but one draw the same sub-file as an id the game places. The only unseen art is `P182BOMP` (id 318), the pig.

**Code map.** Stage `0x20F`: `src/wstag/wstag255.c` (setup, `actors[]`, `sprites[]`, `map_events[]`); stage code per map
`fieldstg_stages[240]` (`{id, file, entry}`); talk `fieldstg_actor_talk_to` (`def_bm_talk` / `def_dri_talk` before the
redirect); traders `traders[]` / `def_bm_talk` in `definitive_market_screen.c`; door/exit between maps
`FieldstgMapEvent` (flags, type 1 = move, x, y, dir, route, room, `include/fieldstg.h`); map spots (layer 7)
`fieldstg_map_events_check`: `attr & 0x1F` = index, `attr >> 5` = direction; music `fieldstg_stage.music` (wstag255 uses
8; the room 0x3B); background `fieldstg_stage.background_file`.

**Checks:** `tools/gen_text_override.py --check`; `tests/port/run.py` (the room is off under `--script`).
