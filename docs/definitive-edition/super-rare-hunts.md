# Super Rare hunts, recolour and hero skins

[Technical reference index](README.md) · [Player README](../../README.md)

## Super Rare hunts

### Rules

- Regional Rare Targets: one Digimon per battle, any thematic species (preferably one absent from the region), level =
  region average + 5, HP x4 capped at 30000, **never Galacticmon**. With a hunt active and the player in the target's
  territory, the regional Rare "gets in the way": the chain gives a chance from 2 wins instead of 6 (the Rush counts) and
  the party's flee against it drops to 50%.
- 12 Super Rares, one Digimon each, no repeats, **unlocked in sequence**: all are listed with the recommended level, the
  next opens when the previous is finished, finished ones can be repeated, the active one can be cancelled (the target
  heals). Levels 29 to 61, HP 15000 to 30000.
- Each has a fixed territory: a sector (North/East/Central/West/South, a hand-made table) of one server.
- The target sits on a **random map** of the territory at a reachable **point**; a **temperature compass** guides between
  maps (graph distance) and inside a map (distance to the point). Stepping on the point starts the battle. The compass
  also shows up to 3 exits outside the territory, to lead the player there.
- The target fights alone; losing **25% of its bar** in a fight makes it flee to another random map, **keeping the HP
  lost**; each re-meeting gives **+20% to all stats**, stacking. Fleeing or losing keeps the hunt open. The Rush never
  skips a Super Rare. Leaving the territory leaves the target where it was.
- Reward: **1 crystal per hunt**, which sells high; no big Bits or XP bonus.
- Colour: a palette swap on Super Rares and regional Rares.
- **Tutorial:** a level-3 hunt (Ogremon in Central Park, near Asuka) that the Sepikmon gives in the Velvet Room. The
  first visit comes through Repeating Tom's news at story step 4: see
  [The story layer](story.md#the-story-layer-velvet-room-in-the-main-story).

### Data

- `def_super_rares[13]` in `definitive_hunt_targets.c` (battle, slot, level, HP, sector, server, area, species, price).
  Row 0 is the tutorial `{204, 1, 3, 480, 2, 0, 0x1D, Ogremon, 1500}`; then the 12 in level order. The `area` field >=
  0 restricts a hunt to one area (the tutorial); -1 uses the whole sector. Crystal price 8000 to 16000; crystals are
  sold at the Sepikmon's board (Square), not at the Black Market. Super Rares by order: MasterTyrannomon (29),
  Pharaohmon (37), Cherrymon (40), Knightmon (46), Whamon (46), WaruMonzaemon (47), BK Seraphimon (47), Kimeramon (52),
  Gryphonmon (56), GrapLeomon (57), HiAndromon (59), Kyukimon (61).
- Sectors (`SECTORS` in `tools/gen_hunt_targets.py`): North, East, Center, West, South, each a list of world map areas,
  per server (Asuka, Amaterasu).
- **Map graph:** `tools/gen_map_graph.py --apply` -> `port/game/definitive_map_graph.c` (map event types 1 and 14 are
  exits): 239 maps, 415 exits, 123 with encounters (`def_map_wild`); `def_map_distance` is a BFS.

### Logic (`definitive_superhunt.c`, pure)

- Order open/locked/done/active, repeat, cancel (heals), the draw of a map in the territory (`draw_map`).
- Territory: `hunt_region` / `hunt_map`. Compass zone: `def_super_territory_distance` <= `DEF_SUPER_COMPASS_REACH` (3).
- Temperature: COLD (>= 4 maps), COOL (2-3), WARM (1), HOT (on the target's map), BURNING (<= 120 px from the point).
  The battle starts within `DEF_SUPER_POINT_RADIUS` (24 px).
- The point: the field sends candidates (`fieldstg_super_points` in `src/fieldstg/fieldstg_80087DB0.c`: a flood fill of 8
  neighbours over the collision layer in 16 px cells, seeded at the walkable floor nearest the player, encounter tiles
  only); the chosen point is the one at 2/3 of the distance.
- Battle: one enemy, the HP that is left, `stat_scale` +20% per re-meeting (`def_super_scale`, capped at 10 meetings), flee
  after losing 25% of the bar in one fight (counted in battle units, Super Hard included); the party's flee always works.
- Win: a crystal (cap `DEF_SUPER_CRYSTAL_CAP` 99), the hunt ends, `def_super_won_take` feeds the panel.
  `def_super_crystal_sell` returns the row's price.
- Rare chain inside the territory starts at 2 wins, sure at 6; a party flee against a regional Rare there is 50%.
- **Toughness**: the battle's HP is an `s16`, so a bar cannot be doubled; every hit of the player on a Super Rare
  (not the tutorial's Ogremon) does `DEF_SUPER_DAMAGE_PCT` 50% (`def_super_damage`, hooked after `wfightmn_cap_damage` in
  `fightstg_8008D3B4.c`): each 25% before the flee takes twice as long.
- **The Blast opens at the Ogremon** (`blast_unlocked` in `definitive.c`): the button and the DV Plug only work in the tutorial
  hunt's battle and for good once it is won. Igor gives the Plug on his Seiryu visit (`TALK05` 20, `def_story_igor_done`).
  **The blessed Blast** (the tutorial's battle, or the first Blast of the game: story bit `DEF_STORY_BLAST_FIRST`): the Plug
  costs no TP, the form carries `DEF_SUPER_TUTORIAL_BOOST_PCT` 50% more and a Super Rare target no longer flees from that battle.
- **The Hunter's Mark** (key item 27, a Japanese tag slot the disc never gives; `ITMNAM/ITMINF 27`): the Black Market's replicas
  list ends with it, `DEF_HUNT_MARK_BITS` 50000 and `DEF_HUNT_MARK_RARES` 2 different rare items (the two held most). Holding
  it a Rare Target leaves 2x its item (`rare_won`) and a Super Rare 2x its crystals (`def_hunt_mark_mult`). Tests:
  `tests/port/definitive_market.py`.

### Screen, compass, call (`definitive_superhunt_screen.c`, `def_superhunt_frame` in the field's idle step)

- **Seiryu City call:** the story layer's step-4 hook (Repeating Tom's news on the inn's stairs opens the ways in; Igor
  shows the room, the screen cuts, the Sepikmon calls the player over with `TALK05` 17). `dw3_velvet_pull` has no
  caller. `DEF_SUPER_FLAG_MET` (1) marks the Sepikmon's first meeting.
- **The board:** 13 hunts with level, state (NEW/ACTIVE/DONE/--), where (sector and server, or "Central Park near Asuka
  City"), HP left, crystals and price. Confirm starts; confirming twice cancels the active one; Square sells a crystal;
  Circle/Triangle exits. On the first opening the tutorial starts by itself (`DEF_SUPER_FLAG_TUTORIAL` = 2).
- **Reward panel** after a win: "Hunt reward: <species> Crystal".
- **Compass:** polygons on layer 0x1004 (no heap window), a word and 5 coloured bars; in Portuguese in the PT build.
- **MATERIALS screen** (`definitive_materials.c`): crystals after the Cores, one per hunt, "???" until won; names over 21
  letters cut the species with a dot (`MAT_FIT`, `$MatCut`: "MasterTyrann. Crystal", "Cristal de MasterTyr.").
- Texts: `$Sup*`, `$MatCrystal`, `$MatCut` and TALK05 14/15 in `tools/text_override/en.txt` and `pt.txt`.

### Testing

- Host: `tests/port/definitive_superhunt.py` (+ driver), `definitive_hunt.py`, and the tests that link
  `tests/port/definitive_super_stub.c` (bestiary, dri, market, buffs). All in `scripts/test.sh`.
- A savestate is valid only for the binary that wrote it: after building, make a new one.
- Through the MCP (`mcp__dw2003__*`): `mem_write` on `st` (hunt state; `st+1` = hunts done), `run` (the point), `hunt+1`
  (forces the regional Rare on the next battle), `skin_hero`. In Phoenix Bay: right for 200 frames, down for 250 gives an
  encounter (the Rush question; Confirm fights).
- Link failures on `__start_dw2003_data_wstagNNN` come from objects cut by two builds at once: move the `wstagNNN` `.o`
  files aside and rebuild.
- To play in Portuguese through the MCP: write a settings file with `{"schema": 1, "video": {"window": false}, "mods":
  {"preset_language": {"enabled": true, "language": "portuguese"}}}`, pass its path as `game_start`'s `config`, then
  `state_load` an English state. To see the Super Rare colour without hunting: with a hunt active, `mem_write` `run+6`
  (`run.battle`) = 1 before an ordinary battle, so `def_super_battle_ready` holds for it.
- The 50% flee against a regional Rare in an active hunt's territory is covered by the host test
  (`definitive_hunt_driver.c`: a draw of 49 flees, 50 does not; the call is in `fightstg_8008D3B4.c` before the game's
  escape rule).

### Save

The last 24 bytes of record 2 (`DEF_SUPER_RECORD`, `definitive_bestiary.c`; far from the Blast data that grows from the
start): 0 active+1, 1 hunts done, 2-3 map, 4-5 HP lost, 6 meets, 7 flags, 8-20 crystals. The hero skin uses the byte
just before them (`REC2_SKIN`).

## Recolour (palette swap of rare foes)

`port/game/definitive_recolor.c`: regional Rare 120 degrees, Super Rare 200 degrees (`rc_degrees`, adjustable), and the
Blast dark forms through the same path (`DEF_RECOLOR_DARK`: luminance 40%, slightly purple).

- An enemy enters `fightstg_slots_add` with model id >= 0x10 (0x10 in the opening script, 0x11 on an entrance); the party
  is 0 (1 on an entrance or a Blast form).
- **Why it is done on the CLUT, not at upload:** a battle Digimon's texture is a 16-bit VRAM page holding 4/8-bit texels and
  CLUT rows together, so recolouring at upload turns into noise. So when the texture loads (`fightstg_model_update`) the
  page is remembered (`def_recolor_texture_loaded`), and each CLUT the mesh uses (the texture command,
  `fightstg_model_mesh_set_texture`) is read from VRAM (`psyq_gpu_vram`), rotated and written back once
  (`def_recolor_mesh_clut`).
- Each model has its own VRAM page (`fightstg_slots_vram`), so the party does not change even when it is the same species.
- A boss may be the Rare Target of several regions, with a different
  colour in each. Today the 71 targets are 71 distinct species, so nothing repeats; if one ever does, `rc_degrees` must
  turn the hue per region (for example the target's index times a step, skipping the Super Rare's 200 degrees) instead
  of the fixed 120 degrees every regional Rare uses now.

## Hero skins

`port/game/definitive_skins.c` with the hook `port_girls_reskin` in `girls_power.c`. The choice screen is a page of the
Sepikmon's board.

- Skins are **for the hero only** (Kail, or Junior without Girls Power); the player's Digimon keep their own colours.
  **Each hunt won unlocks one skin**.
- 13 recipes, one per hunt (`DEF_SKIN_COUNT` = `DEF_SUPER_COUNT`): skin n opens when `def_super_done() >= n`, so the Ogremon
  tutorial gives the first. Skin 0 is the hero's own colours, always open. Each recipe has two parts, **hair** (the cap on
  Junior) and **clothes** (everything but skin and transparency). A part is either `ROT` (a hue turn from the hero's own
  colours, with saturation % and light %: used for black, silver and white, which are greys) or `TINT` (every colour of
  the part takes one hue and keeps its lightness, so the shading stays; a near-grey colour gets `TINT_MIN_SAT` first).
  Skin colours never change.
- The set (every skin changes the hair or the clothes visibly; hair hues keep away from pink and light blue): 1 Black
  Hair (the tutorial's), 2 Green Hair, 3 White Outfit, 4 Gold Hair, 5 Red Outfit, 6 All Black, 7 Purple Hair, 8 Navy
  Outfit, 9 Silver Hair, 10 Ocean (hair and clothes), 11 Sunset (orange hair, purple clothes), 12 Wild Colors (lime
  hair, magenta clothes), 13 Silver and Black. PT: Cabelo Preto, Cabelo Verde, Roupa Branca, Cabelo Dourado, Roupa
  Vermelha, Todo Preto, Cabelo Roxo, Roupa Marinho, Cabelo Prateado, Oceano, Pôr do Sol, Cores Malucas, Prata e Preto.
  Names are `$Skin0..$Skin13`; recipes are in `skin_recipes`.
- `def_skin_set_hero` accepts only an open skin; `def_skin_worn` is the one the sprites use (0 in a replay script or when
  the chosen skin is locked, for instance a card with fewer hunts).
- Painting (`girls_power.c`): the hero's field sprites are 4-bit frames, each with its own 16-colour CLUT. The hero files are
  the ones Junior's actors use (0x186, 0x187, 0x3C9..0x3CC); with Girls Power they become Kail when cdload reads them. The
  hair and skin indices come from each person's `GpWho` masks (Kail hair 4/6/14, skin 1/2; Junior cap 2/3/7, skin 8/9).
  `gp_skin` always starts from the hero's original CLUT (`gp_ref_kaili.clut` or `gp_ref_junior.clut`), so repainting never
  accumulates and a frame with another CLUT is left alone. `port_girls_reskin` (every field frame, from
  `def_superhunt_frame`) repaints files already in memory when the worn skin changes; no map change is needed (it shows on
  the next animation frame; a standing sprite does not return to VRAM). `port_girls_refresh` (New Game, card read) also
  drops the files when the skin changes.
- Save: 1 byte in record 2 (`REC2_SKIN`), out of range reads as 0; savestate `def_skin_savestate`.
- Test: `tests/port/definitive_skins.py` (+ driver): unlock, worn skin, CLUT (hair and clothes change; skin, transparency
  and STP bit stay; no colour becomes transparent) and save.
- **Skins page:** R1 on the board opens "Hero Skins", L1 goes back; the 14 listed, WORN on the one worn (palette 3, the
  active hunt's colour: the small font's green draws its O oddly), "--" and a dim name on locked ones, the info line
  names the hunt that unlocks a locked one; the confirm button wears an unlocked one (`skins_fill`, `skins_input` in
  `definitive_superhunt_screen.c`; texts `$SupSkinsTitle`, `$SupSkinWorn`, `$SupSkinFrom`, `$SupSkinOwn`,
  `$SupSkinOriginal`, `$SupSkinSet`, `$SupSkinLocked`, `$SupKeysSkins`); the hunt reward panel shows "New skin: <name>"
  under the crystal on a hunt's first win only (`def_super_won_take(&first)`, `$SupSkin`; a hunt won again unlocks
  none), `port_girls_savestate` keeps `gp_painted` and `gp_skinned`; a new skin shows on the next animation frame, so
  after under a second of walking. Portraits, menus (status, Digi-Lab) and the name/card screen keep the unskinned hero
  (other images; `port_girls_tim` swaps Junior's for Kail's, without a skin).
