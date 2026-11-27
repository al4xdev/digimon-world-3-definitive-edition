# Publishing on PortMaster

## Where it goes

A decompilation of a commercial game is not accepted by the main PortMaster catalogue. The port belongs to
**PortMaster-MV-New** (the multiverse repository, `https://github.com/PortsMaster-MV/PortMaster-MV-New`), under
`ports/dw3/`. The portname is `dw3` (lowercase, starts with a letter).

`.ci-cd/portmaster/make_release.sh` builds `dist/dw3.zip` (multi-arch: `armhf` + `aarch64`) in the layout the PortMaster packaging page asks for. It refuses to
build when a required file is missing, when `port.json` does not describe the layout, when the binaries are not valid `armhf` / `aarch64`
ELFs, or when a disc image is inside.

## Automated CI & Sync

`.github/workflows/release.yml` also builds `dw3.zip` in the `portmaster` job using `.ci-cd/portmaster/Dockerfile.armhf` and `.ci-cd/portmaster/Dockerfile.aarch64`. When the
repository secret `PORTMASTER_REPO_TOKEN` (`Contents: write` + `Actions: write` on `al4xdev/PortMaster-MV-New`) is
configured, tagged releases (or manual `workflow_dispatch` runs with `sync_portmaster=true`) automatically unpack
`dw3.zip` into `ports/dw3/` on `al4xdev/PortMaster-MV-New`, commit, push, and trigger its `release.yaml` workflow.

## Build locally

```
.ci-cd/portmaster/build_arm.sh               # build/{armhf,aarch64}-sdl2/: dw2003 and dw3-launcher
.ci-cd/portmaster/make_release.sh   # dist/dw3.zip
```

## After any change

1. Fix the source here (`.ci-cd/portmaster/handheld/`, `.ci-cd/portmaster/`), not the copy in the zip.
2. Rebuild: the two commands above.
3. Copy the new zip's contents over `ports/dw3/` in the PortMaster-MV-New fork.

The menu on a test device must be built from the committed `launcher/`. To push it without a full install:
see `.ci-cd/portmaster/handheld/README.md`, "Deploying the launcher".