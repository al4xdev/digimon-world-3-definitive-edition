# Launcher auto-update (Windows and Linux)

Today the launcher only tells (`update.c`: GitHub's latest release against the running version; DEBUG > NEW VERSION opens the
page). This adds installing it. Android keeps its own; PortMaster has its own; **off when RecompLauncher runs it**
(`recomp_active()`: it has its own updater) and for any build that is not a plain release version.

## What the release gives us (checked against v0.8.16)
- Windows: `dw2003-<v>-windows-x86_64.zip`, 5.3 MB, deflated, one folder at the root: `dw2003.exe`, `dw2003-launcher.exe`,
  `recomplauncher.json`, `README.txt`, `LICENSES/`, `mods/`. `SHA256SUMS-windows` lists `<sha256>  <name>`.
- Linux: `dw2003-<v>-x86_64.AppImage`, 6 MB, one file. `SHA256SUMS` likewise.
- The asset URL answers 302 to another host: the HTTP helper must follow it (curl did; WinHTTP does by default,
  unproven on the launcher's own helper). Downloaded the zip here and its hash matched the sums file.
- The player's data lives in the same folder (settings, cards, logs/, user mods in mods/): **the swap touches only the
  files the new zip contains**, never deletes anything else.

## Pieces
1. **`update.c`, extended:** besides tag and page, pick the platform's asset (name, URL, size) from the release JSON, and
   its sha256 from the API's asset `digest` (or from the release's SHA256SUMS file as the fallback). No asset or no hash:
   falls back to today's "open the page".
2. **HTTP:** the cloud.c path, not curl, on Windows (WinHTTP via `w32_http_request`, system certificate store).
   Two additions: a streaming download to a file with a progress count (today a response is held whole in memory, which
   hides progress) and a self-test that downloads a known file from a local server and compares sha256 byte for byte,
   so the "helper that breaks bytes" cannot come back unseen. Linux keeps cloud.c's curl (already how it talks).
3. **`dw2003-updater` (`launcher/updater.c`, own small exe, no SDL, no UI, rarely changes):** the only code that touches
   files. Reuses `sha256.c` and the HTTP helper; a vendored single-file inflate (miniz, MIT, listed in THIRD_PARTY) for the zip.
   The launcher copies it to `.update/run/` and runs the copy, so the installed one can itself be replaced by an update.
   Arguments: URL, sha256, size, install dir, launcher PID, status file.
4. **Launcher side:** NEW VERSION (DEBUG) and the HOME notice offer "UPDATE", the same screen as the handhelds' loading
   (`loading.c` runner minigame) with the updater's status as its stage line: DOWNLOADING n%, CHECKING, PREPARING. B cancels
   (the updater is killed, `.update/` removed). The updater only ever **reads status** back through `update.status` (a
   small text file the launcher polls each frame).

## Updater steps
1. Stage in `<install>/.update/` (same volume as the install, so renames are atomic moves, never copies).
2. Download to `.update/pkg.part`; check size and **sha256**; any mismatch deletes it and reports FAILED, nothing else
   touched. A staged package that already matches (an interrupted earlier run) is reused.
3. Extract to `.update/new/`. **Zip-slip guard:** refuse absolute paths, `..`, drive letters, backslashes. Check the layout
   (launcher exe present, a PE header, version matches the tag) and run the staged launcher's `--version` once as a smoke test.
4. Write `READY` to the status file and **wait**: for the go-ahead file `update.go` *and* for the launcher's PID to exit
   (`OpenProcess` + `WaitForSingleObject` on Windows, `kill(pid,0)` poll on Linux), so the old process is truly gone.
   If the launcher exits without go-ahead the updater exits and the staging stays, so the next start offers it again.
5. **Swap**, same volume only: delete the previous `backup/` first (the earlier update's), then for each file in the new
   package that exists in the install, `MoveFileExW(old, backup/<rel>, WRITE_THROUGH)` (Linux: `rename`), then move the new
   file in. Windows allows renaming a running exe (not deleting or overwriting it), which is why it is a move, never an
   overwrite. Locked by an antivirus: retry 5 times 200 ms apart.
6. **Rollback:** the updater keeps the list of what it moved; on any failure it moves every backed-up file back in reverse
   order and starts the **old** launcher, with a line in `update.log`. `dw2003-updater --rollback` does the same by hand
   from `backup/` (a documented one-liner for a launcher that will not start).
7. Start the new launcher, detached, and exit. `.update/` is removed on the next start.
- Linux: only an AppImage (`$APPIMAGE`) whose folder is writable updates in place; the one file moves to `backup/` beside
  it and the new one (chmod +x) takes its place. Anything else (a checkout, a read-only folder, PortMaster) keeps opening the page.

## UX
While the updater works the old launcher stays open on the runner minigame. When it reports READY the minigame stays and a
quiet banner reads "UPDATE READY - PRESS START TO RESTART" (6 languages). Nothing closes by itself. START writes `update.go`,
the launcher runs its normal exit (the cloud/backups already ran when the last game ended) and the updater takes over. Any
other key dismisses the banner and a smaller notice stays on HOME. Never offered while a game is running.

## Tests
- Linux CI: the updater against `python3 -m http.server` serving a fake release (a zip, a sums file, a 302): a good update,
  a wrong hash (nothing changed), a truncated download, a zip with `../x` (refused), a failure injected at the swap
  (rollback restores every byte), the launcher's PID still alive (waits), the player's files untouched.
- Windows CI job under Wine: the same with `.exe`s; the helper's byte-for-byte download check.
- Real Windows PC (you): one update from a build to a newer test release, with the folder on `C:` and on another drive.
- Self-test additions for the launcher's new strings and the banner fitting in every language.

## Not in this plan / honest limits
- The hash comes from the same GitHub release as the file: it catches a broken or tampered download, not a compromised
  release. Signing `SHA256SUMS` (minisign, public key embedded in the launcher) is the next step and not needed to start.
- No delta updates; the whole zip (about 5 MB) is fetched.
- Files a new release stops shipping stay in the folder (harmless).

## Status (2026-11-24): implemented, approved by the owner (all four decisions: yes)
Done and tested on Linux (`tests/launcher_updater_test.py`: updater and the launcher's side against a fake release; launcher
self-test 37/37; find_asset against the live API answer): `launcher/updater.c`, `selfupdate.c/h`, `update.c` (asset + digest),
`win32.c` (streaming download, plain processes), the screen and the SELECT keys in `main.c`, 6 languages, CMake/Makefile,
the Windows zip and AppImage packaging, `check_release_contents.py`, CI step, docs, THIRD_PARTY (miniz).
Cross-built for Windows with the repo's toolchain: both `.exe`s link, GUI subsystem, manifest, only Windows DLLs.
Differences from the plan above: the minigame's back/east button is a jump, so SELECT hides or cancels (not B); a staged
package is not reused after an interrupted run (it is downloaded again, 5 MB); Linux uses `.dw2003-update/` and
`.dw2003-backup/` beside the AppImage (a bare `backup/` next to a file in Downloads is not a good neighbour).
NOT run: anything on real Windows or under Wine (WinHTTP download with the redirect, MoveFileExW on a running exe, the
real swap); the Wine CI job for the updater; a real update from one release to the next (there is no release with the
updater yet, so the first one that ships it cannot update itself: the second can).

## Decisions for you (answered: yes to all)
1. Name `dw2003-updater(.exe)` shipped in the Windows zip and, on Linux, inside the AppImage? (Recommended: yes.)
2. Vendor miniz (MIT) for the unzip: fine? The alternative is Windows' own `tar.exe`, which is an external program.
3. Hash trust model above (same release, no signature yet): OK for a first version?
4. Linux scope: AppImage only: OK?
