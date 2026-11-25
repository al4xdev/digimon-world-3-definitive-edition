# Bestiary 3D (Extended Save + Interactive 3D Viewer)

## Goal

Replace the cramped field-menu Bestiary popup with a dedicated full-screen 3D Bestiary (stage `0x1100`, native loading transition) backed by the expansion save (`card1.def.json`, section `"bestiary"`), covering all 193 base enemy species (`SDIGIEDT`) plus every special battle variation (`Rare Targets`, `Super Rare Hunts`, `DRI Challenges`, `Campaign Bosses` including Dark Galacticmon, and `Dark Forms` from Purification).

## Layout (320x240, Persona 3 Reload style)

- **Top bar**: slanted `BESTIARY` nameplate, `L1`/`R1` category tabs (`ALL`, `WILD`, `BOSS`, `RARE`, `SUPER`, `DRI`, `DARK`), discovery counter (`XXX/YYY`).
- **Top-left quadrant**: selected entry's name + variation tag, 5 elemental families with icons & states (`WEAK` / `RESIST` / `NEUTRAL` / `???`), Hunt Material drop + count, Rare Core status, and battle/phase notes.
- **Bottom-left quadrant**: interactive 3D Digimon viewport using `fightstg_model_create_idle` (1 model loaded at a time on a dedicated VRAM slot with debounce + `LOADING` indicator, real-time `def_recolor_clut` for variants, black silhouette via `fightstg_model_set_color` for undiscovered `???`, `R-Stick` / `IJKL` rotation, `L2`/`R2` zoom, `Triangle` phase/anim toggle).
- **Right half (top + bottom merged)**: scrollable Digimon list with P3R animations, `[A]` green cursor chip, variation badges, `[B] BACK` sticker.

## Data & Persistence

- **Catalog (`port/game/definitive_bestiary_ext.c`)**:
  - Builds a deterministic catalog of all enemy entries:
    1. 193 base species from `SDIGIEDT` (tag `WILD` or `BOSS`),
    2. Regional `Rare Targets` (`def_hunt_targets`, tag `RARE`, recolor 120° or persona `HUES`),
    3. `Super Rare Hunts` (`def_super_hunts`, tag `SUPER`, recolor 200° or persona `HUES`),
    4. `DRI Challenges` (`def_dri_challenges`, tag `DRI`, persona weaknesses/hues),
    5. `Campaign Bosses` with special battle personas + Dark/Mega Galacticmon (`CAMPAIGN` rows + Final Act, tag `BOSS`, `DEF_RECOLOR_BLACK` where applicable),
    6. `Purification Dark Forms` (`def_purify`, tag `DARK`, `DEF_RECOLOR_DARK`).
- **Expansion save section `"bestiary"` (`def_bestiary_ext_section`)**:
  - Appended to `def_expansion_sections[]`.
  - Tracks `seen` / `defeated` / `known_mask` for entries not already covered by legacy `table[193]` or `def_persona_record_known`.
  - Automatically reflects existing progress from `def_bestiary_known(i)`, `def_persona_record_known(keep, phase)`, `def_record_rare_seen(r)`, `def_dri_best(c)`, and `def_blast_purified`.

## Architecture

1. **Stage `17` (`0x1100`)** in `src/main/overlay.c`:
   - `overlay_entries[17]` -> `def_bestiary_stage_entry` (PC_PORT only, `overlay_files[17] = 0`).
   - `fieldmenu.c` opens `0x1100` via `gamestate_data.funcs.set_next_map(0x1100, 0)` instead of the old inline popup.
   - Exiting calls `gamestate_data.funcs.set_next_map(gamestate_data.field_map, 0)` for a clean native loading transition back to `FIELDSTG`.
2. **Input**:
   - Register 4 `port_input_action` bindings for right stick (`rightx-`/`J`, `rightx+`/`L`, `righty-`/`I`, `righty+`/`K`) without modifying `psxstack`.
   - Use `PAD_HELD` for `L2`/`R2` zoom and `PAD_PRESSED`/`PAD_REPEAT` for list navigation.
3. **3D Viewport**:
   - Single dedicated 3D `GfxLayer` (`0x1002`) positioned at the bottom-left quadrant center, plus 2D UI layer (`0x1005`).
   - Load `0x1CC` (`fightstg_models`), `0x1CF` (`SDIGIEDT`), `0x4E` (Digimon names), `0x6A` (item names).
   - Load only 1 Digimon model + anim file at a time; unload previous model file before loading the next so RAM and VRAM never grow.
