# Performance

Measured frame costs of the handheld port, in one place. Hardware: RK3326 (4x Cortex-A35 in AArch32 at 1.3 GHz, Mali-G31,
1 GB of RAM, 640x480 panel), one handheld and one firmware, at the machine's 60 Hz setting unless a figure says 50 Hz.
The audio's design notes are in [`SOUND.md`](SOUND.md).

- [The budget](#the-budget)
- [Where a frame goes, by scene](#where-a-frame-goes-by-scene)
- [The heaviest battle: software against gles](#the-heaviest-battle-software-against-gles)
- [A gles battle frame](#a-gles-battle-frame)
- [A software battle frame](#a-software-battle-frame)
- [Audio](#audio)
- [Where gles loses](#where-gles-loses)
- [Limits](#limits)
- [Measuring](#measuring)

## The budget

The game runs at a fixed pace, 50 Hz (PAL) or 60 Hz, and does not skip frames. A frame that takes longer than its budget
(20 ms at 50 Hz, 16.7 ms at 60 Hz) slows the whole game down, audio included. The goal is therefore to keep every frame
inside the budget, not to drop frames.

## Where a frame goes, by scene

| Scene | Software | `gles` | Against the 20 ms budget |
|---|---|---|---|
| Menus, map, field (2D) | ~8 ms | ~11 ms | fits |
| Opening movie (24-bit) | ~15 ms | not measured | fits, full speed |
| Scripted first battle | ~13 ms of drawing | not measured | fits |
| Heaviest battle (large enemy, ~150k textured pixels) | ~27 ms (37.70 fps) | ~24 ms (41.85 fps) | over budget |

The 2D rows count an audio mix of 4.7 ms (a town frame: GPU 1.2 + audio 4.7 + present 1.0); [Audio](#audio) has the
mix's own cost. The battle rows are 600 frames from one saved state, read from the `window: N frames presented in X s`
line of `game.log`, with the same binary for both renderers.

## The heaviest battle: software against gles

`gles` runs it at 41.85 fps, the software renderer at 37.70 on the same state. The VRAM of the `gles` renderer differs
from the software one in 0.2% of its texels before the 3D models appear and 0.7% once they do (edges and 5-bit
rounding). At the frames compared with `--screenshot` the picture that reaches the screen is byte-identical.

## A gles battle frame

| Part | ms |
|---|---|
| the listener (packing ~1,100 primitives) | 1.0 |
| the flush (one draw) | 0.6 |
| the read copy (one blit) | 0.6 |
| the copies (two blits) | 0.0-0.1 |
| the present (PBO memcpy, 5-bit conversion, fb0 write) | 3.2 |
| the game's own CPU (recompiled code, GTE, SPU mix) | the rest, ~14 |

The last row includes the SPU mix (3.0-3.3 ms), which leaves about 11 ms for the game's own code by subtraction. The rows
come from the renderer's per-second meter and do not add up to the ~24 ms the fps figure implies; the note labels the last
row "the rest".

## A software battle frame

A heavier fight than the one above (~30 ms of CPU time per frame):

| Part | ms |
|---|---|
| rasteriser (the order-list walk) | 18-19 |
| audio mix | 4.9 |
| present | 1.0 |
| the game's own logic (GTE, models), left over by subtraction | 6-8 |

Do not combine these rows with the `gles` ones. The two splits come from different fights and different ways of
attributing the remainder; where they disagree on the game's own cost (6-8 ms against ~14 ms), trust the `gles` figure.

## Audio

The SPU mix is the second largest item of a frame after the game's own code: 3.1 ms a vsync in the scripted field/menu
run (voices 2.27, of which interpolation 0.31, reverb 0.71, rest 0.13) and 3.0-3.3 ms in a battle. The cost is the
voice loop. A sample count must accompany any figure, because it moves with the rate. See [`SOUND.md`](SOUND.md), "What
it costs".

## Where gles loses

In light 2D scenes `gles` is slower than the software renderer: 43.5 fps against 57.7 on the scripted boot and menu run
(1884 frames). Software draws those scenes in a couple of milliseconds and pays a 1 ms present. `gles` pays about 3.2 ms
for the present, and on the boot and menu scenes the readback waits 10-14 ms, because the frame's own GPU work (the boot
re-uploads its art to the VRAM target) is queued before it and the driver drains it there. Giving the read two vsyncs of
slack does not change this. A menu is fine either way, and a player who only uses 2D scenes can run `--renderer software`.

## Limits

- **Memory latency, not bandwidth.** The DDR does ~4.2 GB/s at 528 MHz and the game touches ~75 MB/s. A texture fetch
  walks one VRAM row per pixel, and the A35's micro-TLB has 10 entries, so a 2 KB row stride crosses a 4 KB page every
  two rows and a 100-row walk overflows it continuously. The hardware prefetcher only follows strides inside one page,
  and `PLD` prefetching gains nothing. A working set that fits the 32 KB L1 costs 14.6 ns a pixel; the whole 1 MB VRAM
  costs 54.8.
- **The game's own C is not built with `-O2`.** It crashes at boot (`sound_init`, `sound_update_loading`,
  `cdload_get_subfile`): the decompilation has undefined behaviour the optimiser exploits. `PSXSTACK_GAME_OPT` allows a
  narrower try per module.
- **`gles` buffers and readback.** The vertex buffer is orphaned with `glBufferData` (`glBufferSubData` into a reused
  one stalls 105 us a draw, 60 ms a frame in 2D). The target is read back as RGBA8 and converted to 5 bits (1.7-2.0 ms;
  reading it as RGB565 costs 28 ms a frame).

## Measuring

The meters cost real frames, so `.ci-cd/portmaster/handheld/Digimon World 3.sh` leaves them off (commented exports). To measure one run,
enable them by hand and read `logs/gpu.log` while the scene is on screen. `DW3_PHASE` wraps every GTE call (~111k a
battle frame) and costs about 23 ms of a 20 ms budget: a frame rate measured with it on says nothing. `DW3_GPU_LOG` prints one line a second:

```
gpu: 19.3 ms fill 0 flat 0 ftex 130450 fblend 0 gour 1664 gtex 16155 gblend 0 |
depth4 146605 depth8 0 depth15 0 |
walk 19.3 ms pkt 18.8 ms tri 16.7 ms tset 0.9 ms row 1.8 ms flat 7.5 ms gour 2.5 ms mask 0.0 ms pre 0.6 ms |
spans 9891 neon 5734 scal 4157 fopq 5872 gok 0 alias 138 maskpx 0 load 0 | aud N ms
```

| Field | Meaning |
|---|---|
| `walk` | the whole order-list walk: everything the GPU does in a frame |
| `pkt` | the GP0 words inside it, that is every drawing command |
| `tri` | the triangles inside those (`tri` within `pkt` within `walk`) |
| `tset` | a triangle's setup, before its first row |
| `row` | a row's attribute setup plus the dirty-block stamp |
| `pre` | a row's preamble: the edges' x, the side test, the clipping |
| `flat`, `gour` | the span loops themselves |
| `mask` | the mask-checked spans, one pixel at a time |
| `spans`, `neon`, `scal` | rows drawn, and which loop drew them |
| `fopq`, `gok`, `alias` | spans the vector path could take, and how many the self-aliasing check sent back |
| `load` | pixels of CPU-to-VRAM transfers (texture uploads) |
| `aud` | the SPU's per-frame mix |

It also prints the SPU's own split per vsync:

```
spu: per vsync (735 samples): total 3.11 ms = voices 2.27 (interpolation 0.31) + reverb 0.71 + rest 0.13
```

With `--renderer gles`, `DW3_GLES_LOG` adds the renderer's split (listener, flush, present). The variables are listed in
[`HANDHELD.md`](HANDHELD.md#environment-variables).

Two checks worth knowing:

- **Is the whole frame inside the budget?** Compare the process's CPU time with wall time. At 50 Hz, 10 s of CPU in 10 s
  is a frame at 100% of the budget: `cut -d" " -f14,15 /proc/<pid>/stat` twice, 20 s apart.
- **Are the pixels right?** Run twice with `--save-state N:/tmp/a.state` and `--save-state-exit`, once with NEON and once
  with `DW3_NO_NEON_FLAT=1`, and compare the files. They must be byte-identical except the stack block at the tail.