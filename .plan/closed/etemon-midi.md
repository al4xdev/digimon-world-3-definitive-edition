# Etemon's MIDI jukebox: decisions taken (owner, 2026-11-23), branch `feat/etemon-midi`

Nothing is left open.

1. **Which battles:** random battles only (the Rare Targets and the Rush's pick are the same encounter). Bosses, scripted
   battles, the Super Rares and the Time Room keep the stage's music.
2. **Where the choice lives:** config, not the save: `battle_music.txt` in the `midi` folder (`mode: own|file|shuffle|order`, then
   `file:` for a single pick or `last:` for the playlists). An older one-line file (just a name) still reads as a single pick.
3. **Sound:** the "middle setting" is on for every player file: one shared linear attack (~46 ms) and release (~186 ms) on the
   melodic tones, the reverb flag, and LIBSND's reverb (type 3, depth 64) raised by the piece and put back when it stops. It edits
   only bank 74's copy of the header, so the Velvet Room's and the Adventure scores are untouched.
4. **Preview:** none. **Length:** a piece longer than the bank's room is cut and loops, with the note on the screen.
5. **Several files (owner request):** Etemon's list has "Game's own music", "All, shuffled" (random file each battle, never the
   previous one with two or more files), "All, in order" (next file by name each battle, wraps; the position is the name of the
   last file played, kept in `battle_music.txt`, so it survives a restart and a changed folder) and then each file. A file that
   does not convert is skipped; if none does, the game's music.
