dw2003recomp @VERSION@ for Windows (x86_64)
===========================================

Digimon World 3: Definitive Edition, from Digimon World 2003 (PlayStation, Europe) rebuilt from its decompiled
code as a native program.
https://github.com/al4xdev/digimon-world-3-definitive-edition (built on https://github.com/gascarcella/dw2003recomp)

What you need
-------------
* Windows 10 or newer, 64-bit.
* Your own disc image of Digimon World 2003 (PlayStation, Europe, SLES-03936), as a .cue with its .bin (or the .bin
  alone). No game data is included: the launcher checks the image's SHA-1 and only the European disc is accepted.

Starting
--------
1. Unzip this folder anywhere (its files must stay together: the launcher finds the game beside itself).
2. Run dw2003-launcher.exe. The programs are not signed, so Windows may show a SmartScreen warning the first time:
   choose "More info", then "Run anyway".
3. On GAME > DISC IMAGE, pick your disc image (BROWSE opens the Windows file dialog).
4. Press PLAY. The launcher waits while the game runs and closes when it ends normally.

GAME, VIDEO, BATTLE HUD, CONTROLS, SAVES & CLOUD and DEBUG are the launcher's other screens; the same launcher and
the same options as on Linux, Android and the handhelds. In the launcher: arrows or a gamepad's D-pad move, Enter or
A changes, Backspace/Escape or B goes back. In the game: arrows the D-pad, X cross, C circle, Z square, S triangle,
Enter start, Backspace select, Q/E L1/R1, 1/3 L2/R2; P pauses, F11 fullscreen, Tab holds fast-forward; a gamepad works
as on a PlayStation, and CONTROLS rebinds it.

Where things are kept
---------------------
%APPDATA%\dw2003\  (usually C:\Users\<you>\AppData\Roaming\dw2003\):
  tui.state, pad.map      the launcher's settings and controls
  settings.json           the game's settings, written by the launcher (RecompLauncher edits the same file)
  card1.mcd, card2.mcd    the memory cards (also readable by an emulator)
  logs\game.log           the last game run's output
  crash-<date>.txt        crash reports (and crash-<date>.dmp, their minidumps)
A portable.txt file beside dw2003-launcher.exe keeps all of this in the launcher's own folder instead.

Reporting a problem
-------------------
This build was tested on Linux through Wine and on Windows 11 (the launcher's self-test, 14 checks, and a title-screen replay
with a real disc). If the game crashes, the launcher shows why.
Set DEBUG > LOGS to HEAVY, play until it happens again, then DEBUG > SAVE LOGS writes dw3-logs-<date>.zip (the logs and
crash reports) into the folder above: attach it to a new issue at https://github.com/al4xdev/digimon-world-3-definitive-edition/issues (the bug
report form). Your disc image and your saves are never part of it.

LICENSES\README.txt lists the licences of what this package contains.
