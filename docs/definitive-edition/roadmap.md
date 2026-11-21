# Planned work and open decisions

[Technical reference index](README.md) · [Player README](../../README.md)

## The owner's status (2026-11-20)

The player README's "What's left" now lists two items, and they are the only open ones for 1.0: **the overall balance** (tuned
by calculation so far; the owner plays through in parallel and ships hotfixes) and **the translation review** (FR/DE/IT/ES of the
Definitive text exist since 2026-11-20 but have not been read in game; the story rewrite is English and Portuguese only). Everything
below is the record of ideas, checks and decisions kept for reference: none of it blocks 1.0 unless the balance or the translation
work turns it up.

## Planned / backlog

Not started or only staged unless the entry says otherwise. Each entry says where its detail lives.

- **muOS aarch64 (staging 10):** the build exists since `8373ea6` (see the History entry for the whole plan and the tester's data); on-device result unverified.
- **Android (staging 14):** the code exists (`android/`, `.ci-cd/android/build_android.sh`, `android/touch_pad.c`; the README line above), the plan's phase gates are the open part: the Phase 0 audit of `psxstack` for the NDK/Bionic,
  the "Show Touch Controls" toggle only in the Android build, SAF disc import, and **a test on a real Android device** (`adb install`, `adb logcat`: boot, disc selection, entering the game, battle with Vulkan + widescreen,
  the toggle). Whole plan in History. On an API 34 emulator (2026-11-18, the arm64 APK under translation): disc import,
  boot, title, a new game into the field in 16:9, the touch pad, the gamepad hiding it, Back in the launcher all ran;
  the x86_64 build crashes at boot in `cdload_get_subfile` (`sound_init`), the arm64 one does not (x86_64 is not shipped).
- **FR/DE/ES/IT delta of the Definitive text (staging 15):** done 2026-11-20 (`fr/de/it/es.txt`); the in-game native read and the story rewrite's translation remain.
- **PortMaster submission:** the package is checked (`.ci-cd/portmaster/make_release.sh`, `dist/dw3.zip`) and **in its testing phase on the PortMaster Discord** (owner, 2026-11-18), not yet in the catalogue. Every CFW of the test matrix was run under emulation; real-device results on CFWs other than the XF35H's are the open part (plan 1).
- **Telemetry Harness and Plugin Gatekeeper (staging 16):** a separate PRD v0.1 draft, unrelated to the game; the whole PRD is condensed in History.
- **Combat ideas never done:** the Layer 6 technique candidates (Combat design record); the Super Hard rebalance by the owner.
- **Onboarding hooks in the main story** (the Velvet Room "section 6": a forced Guardromon interaction after the second teleport point; steering the player through a wardrobe door when rare materials start to appear): out of scope, the owner is rewriting the main quest elsewhere and will make guide quests.
- **Skin ideas:** a price (free or one crystal), a 3D preview; Digimon skins were dropped. (The Velvet Room's hand-drawn door sprites were dropped: the owner vetoed drawn doors on 2026-11-18; the ways in are lights on each inn's scenery.)
- **Velvet Room / story ideas:** Igor's own figure (his head is drawn; the body is still the game's actor 109, "until his own art is drawn" in `definitive_velvet.c`); a drawn figure for the mentor (the old man `0x39` "until one is drawn"); the Adventure tune (`tools/music_lab/`, a first pass) is not tied to any cue.

## Pending work / open decisions

Every item the owner wanted or planned that is not done yet, with its source plan (the plan files may be deleted; the names are kept for reference). Nothing here is in the game unless it says "partly".

**Hunt, Super Rare, skins**
- **Commit**: done. The Super Rare front, the skins, the recolour, the purify and the map graph are in `8c0971b`; the
  regional forge, the replica Cores and the skins page in `eb2fa36` and `89c46af`; the Black Market UI pass in `def4d30`;
  the Elemental weapons and Trade-off pieces in `11d9e54` and `188618a` (git checked 2026-11-18: nothing of these is
  untracked).
- **A boss repeated across regions** (the owner's 2026-11-17 answer): if the target table ever gives two regions the same
  species, the recolour must turn the hue per region (see Recolour); not needed with today's 71 distinct species.
- **Check in game**: done 2026-11-17 (Super Rare colour, compass outside the territory, MATERIALS cut names EN/PT, PT texts); the 50% flee is host-tested only.
- **Sepikmon's position** in the room: done; moved on 2026-11-18 (owner) upstairs to (86,214), dir 7, left of the Demon Bell's keeper (it was (176,306)).
- **Skins choice screen** (`skins-npc.md`): done 2026-11-17 as the Sepikmon board's R1 page (no NPC of its own); the reward panel's "New skin: <name>" line is done (2026-11-17, seen on screen); tested with Kail (2026-11-17); skins on portraits, menus and the name/card screen dropped by the owner.

**Velvet Room** (`velvet-room-plan.md`)
- Done since the plan: the door sprite was dropped (the owner vetoed drawn doors, 2026-11-18: the ways in are lights on each inn's scenery, entered with CIRCLE); the room's music is the owner's arrangement (bank 72, `8042491`); the room was arranged on 2026-11-18 (Sepikmon, Gennai, the pig).
- Open: the owner's look in game at each inn's light, first the six the judges left "divided" (0x238, 0x25B, 0x25D, 0x263, 0x269, 0x26F; the middle was kept); the mentor's approval or replacement (old man `0x39`, line provisional);
  FR/IT/DE/ES lines for `TALK05` 8, 9, 12, 13, 14, 15, 16..20, 47..65, 77, 78, 79, 81, 82, 99, `TALK06` 5..7, `TALK03` 311/312, `TALK01` 178 and the other port-owned texts (the story rewrite itself is English only); **a memory-card save inside the room** loads as the normal bar (`vl_room` is RAM; the Time Room's run is RAM too): acceptable or not is an open question;
  testing in game of the whole entry-talk-exit flow by the owner (the CIRCLE prompt was checked by script on 0x23F, 0x25D and 0x288).
- Texts that still speak of a "blue door" while the way in is now a light at Seiryu's inn's stage (210,300), not on the stairs: `$TomGone` ("that blue door on the stairs"), the Guide's `$Q4sT` ("a blue door opened on the stairs") and `$M4T` ("by an inn's blue door").
- **Not tested in a real game (story layer; the owner's tests are one English run):** the opening's cut and Tom's hook were tested by scripts (`.dumps/tasks/story/opening/`, `tom/tom_test.py`), not yet in that run; the Oikawa's arc (the hooded figure's sprite in VRAM place 6 of map 0x245 while actor 78 is left out, his spot (1778,502), the fight, a defeat and the market's opening); the Time Room (the way in at (160,440), the warnings, the trial areas' positions and sprites on 0x202, 0x26F and 0x258, the Myotismon/Gennai switch, a lost run's reset); Igor's dreams by their real triggers (Act I's flag 0x1C0E, Act II after the Bug Maze, the Oinkmon at step 0x26, Act III and the held Galacticmon battle; all were seen only through `DW3_DREAM` or `DW3_TIME_TEST`); the scenes' choices and echoes in a full English run.

**Blast / Purify / Chosen** (`velvet-room-plan.md`): the Chosen's break of the 999 stat cap is done (up to 9999) but never seen in a real fight; a real playtest of a full-gauge battle (gauge to 1000 by WEAK/stagger in a real fight was never seen); a final sound for the black screen (`PF_SND_CURSE` is provisional); the main-story onboarding
(out of scope).
**Super Hard** (`velvet-room-plan.md`): a real fight with the Bell was never run; balance of the x1.5 HP / x1.5 Speed / x2 Bits / x2 materials is the owner's.
**Combat balance** (`definitive-combat-prd.md`): layer 10 is the owner's, by playing; the guidance is in the Combat design record (boss HP first, then the magic clamp). Open design questions never answered: the Layer 6 technique candidates, the OVERFINISH 10% (may be lowered). (The PRD's "does the enemy get an extra action" is answered by the code: no, the chain is the player's only.)
**Girls Power** (`girls-power-kaili-prd.md`, `girls-power-pixel-art.md`): the owner's full start-to-end playtest and redoing frames they point at (kick, fishing, ladder, lying poses, deck and Digimon name screens, anywhere else Junior might appear); weak spots listed in the Girls Power section.
**Black Market / texts** (`black-market-ux.md`): PT names "Crest de Recuper" (ITMNAM 320, cut), "Pulse Guard", "Shard de Bits/Bytes"; the colon-as-"0" glyph defect of the 14 px font hits about 30 other port strings (a pass or a font glyph fix); the 11 px font lacks `/ : , '` and accents. The small font's green palette (4, "the bestiary's green") draws an `O` oddly (the Sepikmon board's DONE reads like "D3NE"); the owner likes the look (2026-11-17): kept, not a defect. Owner confirmations PRD 4 left open ("applied, the owner may change"): the trader (actor 42 on maps 0x20C / 0x27B, now also the Velvet Room), the 12-commons-per-rare rate, and the replica list and prices. Known limit: the replica bit clears only on the per-frame sweep (`def_bm_sweep`), so between a sale or discard and the sweep the shop pays 0 only for an ID no shop sells. The Portuguese of the forge's renamed pieces (Elemental weapons, Trade-off items 292/295/297/298) and of the eight DRI hint pages is the owner's: `pt.txt` still has the plain lines.
**Translation** (`staging/15`): FR/IT/DE/ES delta done 2026-11-20; open: the native read in game, the story rewrite in those languages, the `ã`/`õ` glyph injection check.
**Not tested in a real game** (`handoff-definitive-pendencias.md`, `definitive-hunt-economy-dri-prd.md`): (the DRI agent talk: seen 2026-11-17 at Johann, the quest at a party level sum of 87, the hand-in talk without the list and the next one with it; still open: the list's own screen after a real agent talk); a HUNT record
save and load on a card at a Guardromon save point; the forge requirement line on the real shop screen (and the refusal and max-buy limit, which have only unit tests); the Guide's arrival notice (the log shows it but a capture 30 frames later shows only the map title). Also from the PRDs and the DRI
task: of the five planned DRI agent checks only Johann's (quest line at sum 87, hand-in, list on the next talk) was seen; a party level sum
under 30, sum 60+, the DDNA poked into the bag, Agumon poked as joined, and a starter's agent (Ken on 0x258 room 1 with Kotemon joined)
are untested; that an ordinary NPC with an agent's sprite (43/44/163) on an agent's own map does not open the list (`dri_rookie` should
refuse it); the floating Rush's level-up stat numbers against the full report screen (same rule, different moment of the draw); combat
layers 2-4 and progression layers C, E, G, J were accepted by host tests and the owner's play, with no measured timing of the short
digivolution and no kept screenshot set (the "Enemy x2" indicator against an enemy that really acts twice included).
**Small fixes** (`handoff-definitive-pendencias.md`): the DRI victory debrief's reward line overflows the panel's right edge ("Metal Power 1 - Metal Power 2 - Metal Pow...", `definitive_dri_screen.c`, reward at +70: split in two lines or shorten); (done, checked 2026-11-17: `def_hunt_battle_end` now counts only result 1; `fightstg_events_end_battle` gives 1 when the enemy side falls, 2 when the party falls, 0 on flight); the floating Rush level-up stat rolls happen at a different moment from the original screen; `docs/images/definitive/definitive_ui.png` predates the per-family screens and needs a new capture; `docs/MECHANICS.md` has a wrong crit range (+148%), fix from the code.
**Tests not registered** (`scripts/test.sh`): `definitive_dri.py`, `definitive_market.py`, `definitive_economy.py`, `definitive_variants.py`, `definitive_quest.py`, `definitive_rush.py` (run by hand; see Tests).
**Launcher / battle menu** (`9-Commit...`, `8-Battle...`): the 11 simplification items of plan 9 (done status unverified; History); the `confusion_style` option never existed.
**CI / release / psxstack** (`handoff-definitive-pendencias.md`): confirm that a push to `main` starts CI by itself; set the optional secrets/variables if wanted (`GAMEDATA_DEPLOY_KEY`, `RELEASE_DRAFT`); push the psxstack patch to `dw3-handheld` then update the pin and drop the workaround block of `port/CMakeLists.txt`
(after verifying the `PSXSTACK_SDL2`/`PSXSTACK_EGL` assumption); the first release `v0.1.0` tag; the widescreen field mod for the handheld package.
**Platforms** (`staging/10`, `11`, `14`): see Planned / backlog; muOS/TrimUI/Android on-device results; the PortMaster CFW research matrix (History) is incomplete.
**Performance leftovers** (closed plans 2-4): the GLES present (about 3.2 ms a frame), the 0.7% VRAM difference and the 2D check at frame 1700, the decoded-texture cache, the SPU stages 2 and 5 (`aud` stayed 3.0-3.3 ms against a 2.5 ms target), the `--input-test` hang on the device (History).
**Housekeeping:** the owner will delete `.plan/`; everything in the plan files is meant to be in this README. `.plan/.state` was not read (internal state), and `.plan/para-o-dono/` does not exist.
