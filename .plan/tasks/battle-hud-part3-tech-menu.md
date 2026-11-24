# Battle HUD, part 3 (idea, not scheduled): Tech / Item menus in the INFO panel's mould

Owner, 2026-11-24, not for today. The INFO panel (part 2a, commit ee77da4) showed that the right half of the screen
holds a tall framed panel with pictures and lines of text. The Tech menu (and Item) could open there the same way:
the same frame, the flat-pixel pictures, and the animation engine of part 2b (the menu comes out of its button, as
INFO comes out of INFO's).

- Widescreen: do not fight the framebuffer seam; give the widescreen animation its own offset so it lands exactly
  where the 4:3 one does (the reasoning of part 1's `FIGHTSTG_BMENU_WIDE_SHIFT`).
- Depends on 2b (the engine) existing first.
