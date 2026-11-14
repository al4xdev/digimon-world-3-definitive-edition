import test from "node:test";
import assert from "node:assert/strict";
import { spawn, execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const WORKER_DIR = new URL("..", import.meta.url).pathname;
const REPO_DIR = new URL("../..", import.meta.url).pathname;
const LAUNCHER_BIN = join(REPO_DIR, "launcher/dw3-launcher");
const PORT = 18788;
const BASE = `http://127.0.0.1:${PORT}`;
const SHOTS_DIR = join(REPO_DIR, ".dumps/launcher_shots");

function makeMemoryCard(seedByte) {
  const buf = Buffer.alloc(131072, 0);
  buf[0] = 0x4d;
  buf[1] = 0x43;
  for (let i = 2; i < 4096; i++) buf[i] = (i * 31 + seedByte) & 0xff;
  return buf;
}

async function waitForHealth(url, timeoutMs = 20000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      const r = await fetch(`${url}/v1/health`);
      if (r.ok) return;
    } catch {
      // booting
    }
    await new Promise((r) => setTimeout(r, 250));
  }
  throw new Error("wrangler dev did not become healthy");
}

let tmpPersist;
let devProc;
let dirA;
let dirB;

test.before(async () => {
  assert.ok(existsSync(LAUNCHER_BIN), `Missing launcher binary at ${LAUNCHER_BIN}`);
  mkdirSync(SHOTS_DIR, { recursive: true });
  tmpPersist = mkdtempSync(join(tmpdir(), "dw3-e2e-persist-"));
  dirA = mkdtempSync(join(tmpdir(), "dw3-e2e-devA-"));
  dirB = mkdtempSync(join(tmpdir(), "dw3-e2e-devB-"));

  writeFileSync(join(WORKER_DIR, ".dev.vars"), "IP_SALT=e2e-salt-1234\n");
  execFileSync(
    "wrangler",
    ["d1", "execute", "dw3-cloud-sync-db", "--local", "--persist-to", tmpPersist, "--file=./schema.sql"],
    { cwd: WORKER_DIR, stdio: "pipe" },
  );

  devProc = spawn(
    "wrangler",
    [
      "dev",
      "--local",
      "--port",
      String(PORT),
      "--inspector-port",
      "19229",
      "--persist-to",
      tmpPersist,
      "--show-interactive-dev-session=false",
    ],
    { cwd: WORKER_DIR, stdio: ["ignore", "pipe", "pipe"] },
  );
  await waitForHealth(BASE);
});

test.after(async () => {
  if (devProc && !devProc.killed) {
    devProc.kill("SIGTERM");
    await new Promise((r) => setTimeout(r, 400));
  }
  rmSync(join(WORKER_DIR, ".dev.vars"), { force: true });
  if (tmpPersist) rmSync(tmpPersist, { recursive: true, force: true });
  if (dirA) rmSync(dirA, { recursive: true, force: true });
  if (dirB) rmSync(dirB, { recursive: true, force: true });
});

test("real dw3-launcher registers, adjusts slider 1..100, syncs, rotates backups, and restores", async () => {
  const mcA = makeMemoryCard(7);
  writeFileSync(join(dirA, "card1.mcd"), mcA);
  // Create a dummy disc file so start_game finds it when needed
  writeFileSync(join(dirA, "dw2003.bin"), Buffer.from("dummy"));

  // Navigate Home -> SAVES & CLOUD (down x5) -> adjust slider (down x2, r1 x2 -> 30/100) ->
  // CLOUD ACCOUNT (down x3, a) -> REGISTER THIS DEVICE (a) -> back to SAVES & CLOUD (b) ->
  // SYNC NOW (up x2, a) -> RESTORE BACKUP (down, a) -> back out & save
  const script = [
    "wait:8",
    "shot:01_home",
    "down,down,down,down,down",
    "wait:8",
    "a",
    "wait:10",
    "down,down,r1,r1",
    "wait:8",
    "shot:02_saves_slider",
    "down,down,down,a",
    "wait:10",
    "shot:03_cloud_unregistered",
    "a",
    "wait:12",
    "shot:04_cloud_registered",
    "b",
    "wait:10",
    "up,up,a",
    "wait:12",
    "shot:05_saves_synced",
    "down,a",
    "wait:10",
    "shot:06_backups",
    "b",
    "wait:8",
    "b",
    "wait:8",
  ].join(",");

  execFileSync(
    LAUNCHER_BIN,
    ["--pc", "--dir", dirA, "--shots", SHOTS_DIR, "--null", script],
    {
      env: { ...process.env, DW3_SYNC_URL: BASE, DW3_UI_SCALE: "1" },
      stdio: "pipe",
    },
  );

  // Verify tui.state persisted backup_max=30
  const tuiState = readFileSync(join(dirA, "tui.state"), "utf8");
  assert.match(tuiState, /backup_max=30/);
  assert.match(tuiState, /sync_enabled=1/);
  assert.match(tuiState, /sync_interval=10/);

  // Verify cloud_sync.json created with token and seq=1
  const cs = JSON.parse(readFileSync(join(dirA, "cloud_sync.json"), "utf8"));
  assert.ok(cs.account_id.startsWith("acc_"));
  assert.equal(cs.device_token.length, 64);
  assert.equal(cs.last_seq, 1);
  assert.equal(cs.last_sha256, createHash("sha256").update(mcA).digest("hex"));

  // Verify local backup created in saves_backup/
  const backups = readdirSync(join(dirA, "saves_backup"));
  assert.equal(backups.length, 1);
  assert.match(backups[0], /^card1_\d{8}_\d{6}_[0-9a-f]{8}\.mcd$/);

  // Verify launch + game exit hook rotates backup & pushes updated save to cloud!
  const mcAUpdated = makeMemoryCard(77);
  const fakeGamePath = join(dirA, "fake_game.sh");
  writeFileSync(
    fakeGamePath,
    `#!/bin/sh
echo "port: start: ok"
python3 -c 'import sys; b=bytearray(131072); b[0]=0x4d; b[1]=0x43; b[2:4096]=[((i*31+77)&0xff) for i in range(2,4096)]; open(sys.argv[1],"wb").write(b)' "${join(dirA, "card1.mcd")}"
exit 0
`,
    { mode: 0o755 },
  );

  execFileSync(
    LAUNCHER_BIN,
    ["--pc", "--dir", dirA, "--null", "a,wait:10"],
    {
      env: { ...process.env, DW3_SYNC_URL: BASE, DW3_EXEC: fakeGamePath },
      stdio: "pipe",
    },
  );

  const csAfterGame = JSON.parse(readFileSync(join(dirA, "cloud_sync.json"), "utf8"));
  assert.equal(csAfterGame.last_seq, 2);
  assert.equal(csAfterGame.last_sha256, createHash("sha256").update(mcAUpdated).digest("hex"));

  const backupsAfterGame = readdirSync(join(dirA, "saves_backup")).sort();
  assert.equal(backupsAfterGame.length, 2);

  // Test Cross-Play 2FA pairing + Device B pulling the updated save!
  const createRes = await (
    await fetch(`${BASE}/v1/pair/create`, {
      method: "POST",
      headers: { Authorization: `Bearer ${cs.device_token}` },
    })
  ).json();
  assert.match(createRes.code, /^\d{6}$/);

  // Drive Device B through SAVES & CLOUD -> CLOUD ACCOUNT -> LINK EXISTING ACCOUNT -> enter 6 digits!
  const enterTokens = [];
  for (let i = 0; i < 6; i++) {
    const d = Number(createRes.code[i]);
    for (let k = 0; k < d; k++) enterTokens.push("up");
    if (i < 5) enterTokens.push("right");
  }
  const devBScript = [
    "wait:8",
    "down,down,down,down,down,a",
    "wait:8",
    "down,down,down,down,down,a",
    "wait:8",
    "down,a",
    "wait:8",
    ...enterTokens,
    "shot:07_pair_enter_filled",
    "a",
    "wait:10",
    "shot:08_pair_waiting",
  ].join(",");

  // Approve as soon as claim arrives in the background (poll rapidly since PLAT_NULL ticks run faster than wall clock)
  const approver = (async () => {
    for (let i = 0; i < 400; i++) {
      const st = await (
        await fetch(`${BASE}/v1/pair/status?code=${createRes.code}`, {
          headers: { Authorization: `Bearer ${cs.device_token}` },
        })
      ).json();
      if (st.status === "awaiting_approval") {
        await fetch(`${BASE}/v1/pair/approve`, {
          method: "POST",
          headers: { Authorization: `Bearer ${cs.device_token}`, "Content-Type": "application/json" },
          body: JSON.stringify({ code: createRes.code, approve: true }),
        });
        return true;
      }
      await new Promise((r) => setTimeout(r, 5));
    }
    return false;
  })();

  // Run Device B launcher asynchronously so `approver` runs concurrently while Device B polls!
  const devBFullScript = `${devBScript},wait:250,shot:09_pair_done,b,wait:8,b,wait:8,up,up,a,wait:15,shot:10_devB_synced`;
  const devBRun = new Promise((resolve, reject) => {
    const p = spawn(
      LAUNCHER_BIN,
      ["--pc", "--dir", dirB, "--shots", SHOTS_DIR, "--null", devBFullScript],
      { env: { ...process.env, DW3_SYNC_URL: BASE, DW3_UI_SCALE: "1" }, stdio: "pipe" },
    );
    p.on("error", reject);
    p.on("exit", (code) => (code === 0 ? resolve() : reject(new Error(`devB launcher exited ${code}`))));
  });
  const [approved] = await Promise.all([approver, devBRun]);
  assert.equal(approved, true);

  // Verify Device B now shares account_id and auto-downloaded exact card1.mcd!
  const csB = JSON.parse(readFileSync(join(dirB, "cloud_sync.json"), "utf8"));
  assert.equal(csB.account_id, cs.account_id);
  assert.notEqual(csB.device_id, cs.device_id);
  assert.equal(csB.last_seq, 2);
  const mcB = readFileSync(join(dirB, "card1.mcd"));
  assert.equal(Buffer.compare(mcB, mcAUpdated), 0);

  // Simulate divergence: Device A pushes rev #3 while Device B modifies card1.mcd locally offline
  const mcA3 = makeMemoryCard(88);
  const push3 = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { Authorization: `Bearer ${cs.device_token}` },
    body: mcA3,
  });
  assert.equal(push3.status, 200);

  const mcBLocal = makeMemoryCard(123);
  writeFileSync(join(dirB, "card1.mcd"), mcBLocal);

  // On Device B: SAVES & CLOUD -> SYNC NOW -> Conflict screen -> down (UPLOAD LOCAL TO CLOUD) ->
  // confirm 1/3 -> 2/3 -> 3/3 -> overwrite cloud!
  const conflictScript = [
    "wait:8",
    "down,down,down,down,down,a",
    "wait:8",
    "down,down,down,a",
    "wait:10",
    "shot:11_conflict_screen",
    "down",
    "wait:8",
    "a",
    "wait:12",
    "shot:12_confirm_step1",
    "a",
    "wait:14",
    "shot:13_confirm_step2",
    "a",
    "wait:14",
    "shot:14_confirm_step3",
    "a",
    "wait:12",
    "shot:15_conflict_resolved",
  ].join(",");

  execFileSync(
    LAUNCHER_BIN,
    ["--pc", "--dir", dirB, "--shots", SHOTS_DIR, "--null", conflictScript],
    { env: { ...process.env, DW3_SYNC_URL: BASE, DW3_UI_SCALE: "1" }, stdio: "pipe" },
  );

  const csBAfter = JSON.parse(readFileSync(join(dirB, "cloud_sync.json"), "utf8"));
  assert.equal(csBAfter.last_seq, 4);
  assert.equal(csBAfter.last_sha256, createHash("sha256").update(mcBLocal).digest("hex"));
});
