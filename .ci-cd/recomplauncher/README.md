# RecompLauncher

[RecompLauncher](https://recomplauncher.com) installs and updates the game from this repository's GitHub releases:
it always takes the latest full release (never a pre-release), picks the file for the player's system by its name
(`...-x86_64.AppImage`, `...-windows-x86_64.zip`) and checks it against GitHub's SHA-256.

- The manifest is [`recomplauncher.json`](../../recomplauncher.json), at the repository root, where RecompLauncher
  looks for it ("found in your repo"); do not move it. It names the disc it accepts (Europe, SLES-03936), how the game
  starts (`--game`) and the settings the launcher shows.
- Every PC release carries a copy at its own root: [`linux/package_appimage.sh`](../linux/package_appimage.sh) (the
  AppImage) and [`windows/package_windows.sh`](../windows/package_windows.sh) (the zip) copy it, and
  [`common/check_release_contents.py`](../common/check_release_contents.py) requires it.
- The page's art (icon, backdrop, logo) is in [`docs/images/launcher/`](../../docs/images/launcher).
- A test build goes out as a GitHub pre-release (a tag with a suffix, `v0.7.5-rc1`): players on the normal release
  never get it.
