# RecompLauncher

[RecompLauncher](https://recomplauncher.com) installs and updates the game from this repository's GitHub releases:
it always takes the latest full release (never a pre-release), picks the file for the player's system by its name
(`...-x86_64.AppImage`, `...-windows-x86_64.zip`) and checks it against GitHub's SHA-256.

- The manifest is [`recomplauncher.json`](../../recomplauncher.json), at the repository root, where RecompLauncher
  looks for it ("found in your repo"); do not move it. It names the disc it accepts (Europe, SLES-03936), what it starts
  and the settings it shows.
- RecompLauncher starts our launcher (`dw2003-launcher`), never the game directly: the launcher's PLAY does the save
  backup and the cloud sync around the game (cross-save). The disc RecompLauncher picks is `settings.json`'s
  `disc/path`, which the launcher reads. Its settings are exactly the launcher's VIDEO, GAME and DEBUG LOGS options
  (the launcher reads them back from `settings.json`), plus "Launcher menu": Show (the default: the launcher's menu)
  or Skip (`--play`: PLAY at once; the sync and backups still run, and a save conflict still asks). In
  RecompLauncher's Big Picture mode the launcher's window opens fullscreen, as the game's does.
- Every PC release carries a copy at its own root: [`linux/package_appimage.sh`](../linux/package_appimage.sh) (the
  AppImage) and [`windows/package_windows.sh`](../windows/package_windows.sh) (the zip) copy it, and
  [`common/check_release_contents.py`](../common/check_release_contents.py) requires it.
- The page's art (icon, backdrop, logo) is in [`docs/images/launcher/`](../../docs/images/launcher).
- A test build goes out as a GitHub pre-release (a tag with a suffix, `v0.7.5-rc1`): players on the normal release
  never get it.
