# DRI player forms: the technique animations play on the player's side (handoff, 2026-11-22)

## The owner's report and his idea

The DRI's lines put **player forms** on the enemy side (`dri-player-forms.md`). The owner, watching them in game, sees that
**several special techniques stay on the player's side even though the enemy casts them** — the staging (camera, props,
the staged model) happens around the party's row instead of the caster's.

His idea: **exchange the two sides' places for the length of such an attack**, instead of the alternative of authoring
or mirroring data. Implemented on the branch `feat/dri-form-tech-anim`, **always on** (the owner tests it in his own
build with a debug save).

## Why it happens (measured, not guessed)

- **The arena** — `fightstg_slots_reset_pos` (src/fightstg/fightstg_80086A00.c:726-751): x = 0 for every slot; the
  party's slot 0 at `z = -(0x1400 + model->depth)`, `rot.y = 0x800` (facing +z); the enemy's slot 0x10 at
  `z = +(0x1400 + model->depth)`, `rot.y = 0` (facing -z). `depth`/`height` come from the disc's SMDLDATA record. Only
  slots 0 and 0x10 are on the field at once (src/wfightmn/wfightmn_800A6440.c:386-389); the others arrive with a wave,
  a digivolution or a switch.
- **The animation is a per-model script** — `FightstgScript.script` = `RecordsTechnique.anim_script`, read from sub-file
  `script` of the caster model's `script_file` (SMDLDATA). Its cameras are absolute settings
  (`fightstg_script_run_camera` op 1, y and z negated — a 180° turn about x, not a mirror) and its staged models/props
  stand at absolute positions (command 2 op 3, command 6).
- **Each script frames the row of the side it was written for.** Statistics over the disc (every model's script file):
  party records' explicit cameras aim at the party's row 120 times, at the enemy's 12; enemy records' aim at the enemy's
  row 42 times, at the party's 4. Example: Greymon's Nova Blast (script 0xb of model 5) sets eye z +2496, target
  z +5632 (the enemy's row) — right for a player casting it, wrong for Greymon standing there as an enemy.
- **The camera cuts that name a side already work on both sides** (`fightstg_script_run_camera`: presets 1-4 read slot
  0's type A cameras 8-11, presets 5-7 read slot 0x10's type B cameras 0-2). That is why such an animation still shows a
  scene: only the *absolute* settings and the staged positions are on the wrong side.
- **Nothing else mirrors anything**: one `GsWSMATRIX` camera, layers plain 320x240, no negative scales (a part with a
  negative scale component is hidden), the models' transform is `params->pos/rot` re-read every frame.
- **A blanket z-flip of the script literals is not the fix**: the scripts are not consistent about which row they frame
  (12 of the party's 197 cameras frame the enemy's row — over-the-shoulder shots), so flipping would be right for part of
  the disc and wrong for the rest. Exchanging the rows reproduces, exactly, what the player sees when it casts the same
  technique, whatever the script assumes.

## The prototype

`port/game/definitive_anim.c` (`def_form_anim_swap_begin` / `_end`), called from `fightstg_script_update`
(src/fightstg/fightstg_8008B630.c, its INIT and its END, `FightstgScript.unk_84` carries the flag):

- **When**: the script belongs to the enemy side, its index is 5 or up (not a reaction), and
  the model has no enemy record on the disc (`fightstg_enemy_find_record(model_id) < 0`): a player form, which only the
  DRI's simulators and the Blast's purification put on the field. (The first version tested
  `def_enemy_camera_model(model_id) != 0`, which is never 0 — it swapped every enemy of every battle. Checked on the
  disc 2026-11-22: every DRI stage with `base != 0` has no record, every one with `base == 0` has one.)
- **What**: for every present pair (0 with 0x10, 1 with 0x11, 2 with 0x12) exchange x and z of `pos` and `start_pos`,
  and add 0x800 to `rot.y`/`start_rot.y` — each side's own resting facing. y stays (it is the model's own height).
- **The roles follow the exchange**: a script names a *side*, not a slot (`fightstg_script_get_slot`: roles 1-3 =
  slots 0-2, roles 4-6 = slots 0x10-0x12, role 0 = the caster's own side). A player-authored script therefore hides and
  animates the party's slots for its own side and the enemy's for its opponent — with the rows exchanged, its "my side"
  is the enemy's and its "opponent" is the party's, so roles 1-6 go through `slot ^ 0x10` while the script is exchanged.
  Without this half the caster stayed hidden and the close-ups framed the wrong model.
- **Row-aware side logic**: "approach" (command 2 op 8, ±0x2800) and the jump's z direction decide their sign from the
  slot's side (`slot & 0xF0`, `params->id == 0x10`); the port keeps that test and inverts it while the rows are
  exchanged (`def_form_anim_rows_exchanged`; for the jump, slot 0 instead of 0x10), so outside an exchange it is the
  disc's whatever a script staged (a model added without reset_pos has start_pos.z 0), and during one it is also right
  for the target's reaction.
- **battle_animations** (the launcher's "Disable battle animations"): the exchange is made after the mod's cut, so a cut
  script (ended, or turned into the target's reaction on the same object) never exchanges. Made before it, the reaction
  pass overwrote `unk_84` and the rows stayed exchanged for the rest of the battle (critic, 2026-11-22).
- **Out**: `fightstg_slots_reset_pos` for both ids, which also clears anything the script left behind (a model mid-jump).
- Nothing else depends on those positions during an animation: the victim is a member index, the HUD is screen-space,
  and the portrait camera (which *does* aim at the party's row) only lives while the menu is open.

No global state: "exchanged" is slot 0's `start_pos.z > 0` (only the exchange and `reset_pos` write `start_pos`), so a
save state carries it and a battle that ends mid-animation leaves nothing for the next one. Known limit: in-flight tweens (`fightstg_jump_create`, the move objects) cache their own from/to and are not redirected.

## The camera's cuts stay (decided 2026-11-22)

The owner asked whether a shot can be entered with the camera moving instead of cutting, then, after seeing it, kept the
disc's cuts: "it changes nothing for me; leave the hard cut, simpler". Tried and removed, for the record:
`fightstg_script_run_camera` stands a cut (frames 0, 724 of them in the player forms' files) against `camera->move`
(300), so amaciar it is only an argument; but the visible cut is the **first** camera command — the entry into the
technique's close-up — which must stay a cut anyway, because the rows are exchanged at the script's INIT and a pan
would show the two models trading places. The later shots enter over a few frames without changing anything visible,
and a real orbit around the caster would be new interpolation (straight-line lerp through the arena at 40 frames).

## How to see it

```fish
# the save state must be made by THIS build (a state loads only in the same binary)
build/port/dw2003 --disc iso/dw2003.cue --cd-speed instant --script tests/replay/scripts/first_battle_save.json \
    --save-state 38640:.dumps/dri/field.state --save-state-exit

# challenge 3 (Agumon > Greymon > MetalGreymon > WarGreymon)
tools/venv/bin/python tools/dri_battle_record.py 3 --name ch3 --frames 2600 --every 6 --god --turns 3 --stages 1
# and, for a comparison, the branch's previous state (the exchange behind its switch) or `git stash`-free checkout:
# git checkout 9d43e12^ -- src/fightstg include/port.h port/game/definitive_anim.c ; rebuild
```

**Before the exchange**: `ch3v3base_s0.mp4` frames **Kotemon** (the party) in Agumon's Pepper Breath close-up. **With
it**: `ch3always_s0.mp4` frames **Agumon** (the caster) in the same shot, and the idle frames after the animation are
unchanged, so the exchange is undone. A cut of both, stacked: `.dumps/tasks/dri-cast-compare.mp4`.
