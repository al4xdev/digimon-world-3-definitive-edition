# Android Launcher Parity & Unification Backlog

Remaining architectural items from the Android vs PC launcher parity audit (2026-11-26), deferred past `v0.9.2` so each change can be tested cleanly:

## 1. Extract shared launcher core (`launcher/app_core.c` / `launcher/app_core.h`)
- Move canonical label tables (`lang_label`, `ff_label`, `load_label`, `buf_label`, `log_label`, `lay_label`, `filt_label`, `vert_label`, `hud_label`, `fight_label`, `act_order`), config steppers, `PLAY` prep (`save_backup_rotate` + `cloud_sync_card1` + `cloud_migrate_saves`), `send_logs`, and conflict/pairing state machines into one module shared by `main.c` and `main_android.c`.
- Keep only layout/drawing and input dispatch in `main.c` vs `main_android.c` so future PC changes cannot silently drift on Android.

## 2. Recoverable `S_DISC` screen on Android (refused/unreadable disc)
- Because `android/android_entry.c` runs `dw3_game_main()` in-process, `port_fatal("disc: ...")` currently calls `exit(1)` instead of showing `S_DISC` (*CHECK YOUR DISC*).
- Add a recoverable pre-game disc check or `port_fatal` hook before `dw3_game_main()` init so Android returns to `S_DISC` instead of closing the app.

## 3. Android launcher automated self-test
- Add a headless test target (`DW3_ANDROID_STANDALONE_TEST` or CI check) that renders every screen of `main_android.c` in all 6 languages and verifies `i18n.h` key coverage and horizontal fit.

## 4. Minor remaining polish
- `S_CONFLICT_CONFIRM`: upgrade Android's 1-tap confirm modal to match PC's 3-step progressive confirmation with `REV #` details.
- `SYNC INTERVAL` on Android: either implement periodic background sync / clean-exit push or clarify in the UI that Android syncs on launch and via `SYNC NOW`.
- `touch_pad` key in `settings.json` (`launcher/cfg.c:604`): stop writing `"touch_pad"` into `settings.json` (runtime logs `unknown key, ignored`) since `tui.state` already stores it.
- Capture launcher/cloud logs into `logs/game.log` on Android before `dw3_game_main()` starts (without losing logcat visibility during dev).
