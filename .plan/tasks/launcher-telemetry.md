# Launcher: SEND LOGS to our Cloudflare worker + Windows input-lag hunt

## 1. Windows lag: suspects (no repro on the dev machine)
- DONE (untested on Windows): the game's stdout pipe was CreatePipe(...,0) = 4 KB. The game logs unbuffered
  (`port_file_line_buffered` = _IONBF on UCRT) and the launcher drains every 50 ms; a full pipe blocks the game's
  WriteFile = a stalled frame, worst on one core / HEAVY logs. Now 1 MB.
- DONE: the periodic cloud push thread ran at the game's priority; now BELOW_NORMAL.
- TODO: the launcher's own main thread does backup rotate + hashing mid-game (main.c ~2755); lower the launcher
  process priority while the game runs.
- TODO (measure, do not guess): the game already has `port_phase*` and framecsv; add a frame-time summary
  (p50/p95/p99/max ms, late frames, worst present, worst sleep overshoot, pipe-write time) to the end of game.log, so
  the telemetry answers this on the players' machines.

## 2. Telemetry
- Launcher: SAVE LOGS -> SEND LOGS (label says it uploads to our server; all 6 languages in i18n.h; the local zip is
  still written first). Zip = today's files + the player's card (card1.mcd, and card1.def.json when <= 512 KB) so battle/quest bugs reproduce; never the cloud token. The button's text says the save goes along + system.txt gets OS version, CPU
  count, GPU/renderer, display, launcher+game build. Android too (main_android.c).
- Worker (cloud-sync/): POST /v1/logs, unauthenticated-capable (a crash may precede registration) but limited: body
  <= 1 MB, per-IP-hash rate limit (rateLimit.ts), stored in R2 (or D1 blob) with RETENTION_DAYS, ADMIN_SECRET to list
  and download. No account needed; device_id attached when registered.
- Open decision: R2 needs enabling on the account; D1 blob (<= 1 MB rows) works today. Default: D1.

## Status (2026-11-24)
DONE and tested here (Linux): worker `POST /v1/logs` (+ admin list/get, table `log_reports`, migration 0003, retention,
rate limit 5/h 20/day per IP, report code XXXXX-XXXXX), worker tests (the new one passes; `e2e_launcher.test.mjs` fails
on `tui.state` before reaching the worker, not checked against a clean base), launcher SEND LOGS (cfg.c `logs_pack`,
cloud.c `cloud_send_logs`, main.c, main_android.c, i18n.h in 6 languages), real upload through the launcher's own C
code to a local `wrangler dev`, zip read back identical, card + system.txt in it, no hostname. Launcher self-test 36/36.
Issue form requires the code, `.github/workflows/report-code.yml` closes a runtime bug without one; docs updated.
NOT DONE / NOT TESTED:
- Windows runtime: cfg.c/win32.c/cloud.c cross-compile clean with llvm-mingw, but nothing ran (no Wine here): the
  stored-zip writer's tail cap, the 1 MB pipe, the BELOW_NORMAL thread. Try a build on the Windows PC.
- Deploy (the owner's call): `cd cloud-sync && npm run db:migrate:logs:remote && npx wrangler deploy`; `ADMIN_SECRET`
  secret must exist for the admin routes.
- The frame-time summary in the game's log (psxstack runtime, its own repo/PR) and the launcher's own priority while the
  game runs.
- Android: main_android.c edited, not built.
- The workflow was not run on GitHub (labels `bug` come from the template; `needs-report-code` is created on first use).
