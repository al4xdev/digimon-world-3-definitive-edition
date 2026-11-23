# Battle HUD redesign: decisions for the owner (branch `feat/battle-hud`)

Each item has the default already implemented; nothing here blocks the branch.

1. **Screen place follows the player's mapping** (your 2026-11-23 decision, round 5): each action is drawn on the spot
   of the button it is mapped to, so a remapped pad moves the word with the button (`bm_place`, display only: what each
   button *does* is unchanged). With a remapping that puts two actions on the same button, both fall back to their
   defaults. Nothing to decide unless you want the cf5401b fixed places back.
2. **The portrait window (acting Digimon's close-up, top right) is no longer opened with the battle menu.** The critic
   ranked it the main hierarchy problem (largest, brightest box, repeating the Digimon already on screen). The
   confusion menu still opens it. **Default: gone while `battle_menu` is on.**
3. **The Blast tab is the gauge's bar now** (your round-6 idea): the separate 4-px meter is gone, and the tab fills
   from its left end in cyan as the gauge charges. The original HUD's bar under the party panel stays, and it is what
   shows the gauge filling during actions, while the menu is closed; it is hidden while the menu is open. **Default:
   both, each where it belongs.** Option: hide the original bar always, if the tab is enough for you.
4. **The command words are English only** (your call, round 6): FIGHT, TECH, DIGIVOLVE, TAG, ITEM, RUN, INSPECT,
   BLAST. Eight words read in a fraction of a second at 320x240 stop being readable once they change length, wrap or
   shrink per language; the game's own texts (submenus, notices, the Guide) stay in the player's language.
5. **No "Blast calls..." line any more** (round 5): the lit tab itself calls, shining and shaking, so there is no
   wording left to decide. The string is still in the text table, unused; delete it there if you prefer.
6. **Showcase images**: `docs/images/definitive/shots/battle-hud.png` and `battle-inspect.png` are recaptured from this
   round's build. Still stale: the README's HUD animation (`hud.svg`, fist/bolt icons) shows the old cross — regenerate
   it with your README pipeline when merging. Round-6 shots: `.dumps/tasks/battle-hud/shots/r9_*.png` (worktree).
7. **"Enemy x2" and LIMIT BREAK** sit just right of the tab rows (x 160). Worth one real look in a battle where the
   enemy acts twice: the block moved up, and those two labels are the only text near the fighter's side of the screen.
