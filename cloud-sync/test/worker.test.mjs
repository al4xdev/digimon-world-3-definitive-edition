import test from "node:test";
import assert from "node:assert/strict";
import { spawn, execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { gzipSync } from "node:zlib";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const WORKER_DIR = new URL("..", import.meta.url).pathname;
const PORT = 18787;
const BASE = `http://127.0.0.1:${PORT}`;
const ADMIN_SECRET = "test-admin-secret-0123456789";

function makeMemoryCard(seedByte = 1) {
  const buf = Buffer.alloc(131072, 0);
  buf[0] = 0x4d; // 'M'
  buf[1] = 0x43; // 'C'
  for (let i = 2; i < 4096; i++) {
    buf[i] = (i * 17 + seedByte) & 0xff;
  }
  return buf;
}

function sha256(buf) {
  return createHash("sha256").update(buf).digest("hex");
}

async function waitForHealth(url, timeoutMs = 20000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      const r = await fetch(`${url}/v1/health`);
      if (r.ok) return;
    } catch {
      // still booting
    }
    await new Promise((r) => setTimeout(r, 250));
  }
  throw new Error("wrangler dev did not become healthy in time");
}

let tmpPersist;
let devProc;

test.before(async () => {
  tmpPersist = mkdtempSync(join(tmpdir(), "dw3-sync-test-"));
  writeFileSync(
    join(WORKER_DIR, ".dev.vars"),
    `IP_SALT=test-salt-abcdef\nADMIN_SECRET=${ADMIN_SECRET}\n`,
  );

  execFileSync(
    "wrangler",
    ["d1", "execute", "dw3-cloud-sync-db", "--local", "--persist-to", tmpPersist, "--file=./schema.sql"],
    { cwd: WORKER_DIR, stdio: "pipe" },
  );

  devProc = spawn(
    "wrangler",
    ["dev", "--local", "--port", String(PORT), "--persist-to", tmpPersist, "--show-interactive-dev-session=false"],
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
});

test("health, registration, save push/pull, conflict detection, and deduplication", async () => {
  const h = await fetch(`${BASE}/v1/health`);
  assert.equal(h.status, 200);
  const hj = await h.json();
  assert.equal(hj.ok, true);
  assert.equal(hj.retention_days, 40);

  // Unauthenticated /me fails
  const unauth = await fetch(`${BASE}/v1/me`);
  assert.equal(unauth.status, 401);

  // Register device A
  const regRes = await fetch(`${BASE}/v1/register`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.10" },
    body: JSON.stringify({ device_name: "R36S-Alex", platform: "portmaster" }),
  });
  assert.equal(regRes.status, 201);
  const devA = await regRes.json();
  assert.ok(devA.account_id.startsWith("acc_"));
  assert.ok(devA.device_id.startsWith("dev_"));
  assert.equal(devA.device_token.length, 64);

  // Reject wrong size & wrong magic
  const badSize = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { Authorization: `Bearer ${devA.device_token}` },
    body: Buffer.from("MChello"),
  });
  assert.equal(badSize.status, 400);

  const badMagic = Buffer.alloc(131072, 0xaa);
  const badMagicRes = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { Authorization: `Bearer ${devA.device_token}` },
    body: badMagic,
  });
  assert.equal(badMagicRes.status, 400);

  // Push valid raw save #1
  const mc1 = makeMemoryCard(1);
  const mc1Sha = sha256(mc1);
  const push1 = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: {
      Authorization: `Bearer ${devA.device_token}`,
      "X-Save-Sha256": mc1Sha,
      "X-Expected-Seq": "0",
    },
    body: mc1,
  });
  assert.equal(push1.status, 200);
  const push1Json = await push1.json();
  assert.equal(push1Json.seq, 1);
  assert.equal(push1Json.sha256, mc1Sha);
  assert.equal(push1Json.deduplicated, false);
  assert.ok(push1Json.stored_bytes < 10000);

  // Deduplicate identical push
  const pushDup = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { Authorization: `Bearer ${devA.device_token}` },
    body: mc1,
  });
  assert.equal(pushDup.status, 200);
  const dupJson = await pushDup.json();
  assert.equal(dupJson.deduplicated, true);
  assert.equal(dupJson.seq, 1);

  // Conflict when X-Expected-Seq is stale
  const mc2 = makeMemoryCard(2);
  const conflict = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { Authorization: `Bearer ${devA.device_token}`, "X-Expected-Seq": "0" },
    body: mc2,
  });
  assert.equal(conflict.status, 409);
  const conflictJson = await conflict.json();
  assert.equal(conflictJson.server_seq, 1);

  // Push gzipped save #2 with correct X-Expected-Seq
  const push2 = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: {
      Authorization: `Bearer ${devA.device_token}`,
      "Content-Encoding": "gzip",
      "X-Expected-Seq": "1",
      "X-Save-Sha256": sha256(mc2),
    },
    body: gzipSync(mc2),
  });
  assert.equal(push2.status, 200);
  const push2Json = await push2.json();
  assert.equal(push2Json.seq, 2);

  // Pull with If-None-Match -> 304
  const notMod = await fetch(`${BASE}/v1/saves/card1/pull`, {
    headers: { Authorization: `Bearer ${devA.device_token}`, "If-None-Match": `"${sha256(mc2)}"` },
  });
  assert.equal(notMod.status, 304);

  // Pull full save -> byte-identical
  const pullRes = await fetch(`${BASE}/v1/saves/card1/pull`, {
    headers: { Authorization: `Bearer ${devA.device_token}` },
  });
  assert.equal(pullRes.status, 200);
  assert.equal(pullRes.headers.get("X-Save-Seq"), "2");
  const downloaded = Buffer.from(await pullRes.arrayBuffer());
  assert.equal(Buffer.compare(downloaded, mc2), 0);
});

test("2-step cross-play pairing, cross-device pull, device revocation, and denial", async () => {
  // Host device A
  const regA = await fetch(`${BASE}/v1/register`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.20" },
    body: JSON.stringify({ device_name: "Handheld-Host", platform: "portmaster" }),
  });
  const host = await regA.json();

  const mc = makeMemoryCard(42);
  await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { Authorization: `Bearer ${host.device_token}` },
    body: mc,
  });

  // Create pairing code on host
  const cRes = await fetch(`${BASE}/v1/pair/create`, {
    method: "POST",
    headers: { Authorization: `Bearer ${host.device_token}` },
  });
  assert.equal(cRes.status, 201);
  const { code } = await cRes.json();
  assert.match(code, /^\d{6}$/);

  // New device B claims code
  const clRes = await fetch(`${BASE}/v1/pair/claim`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.21" },
    body: JSON.stringify({ code, device_name: "Desktop-PC", platform: "pc" }),
  });
  assert.equal(clRes.status, 200);
  const claim = await clRes.json();
  assert.equal(claim.status, "awaiting_approval");

  // Wrong secret rejected without consuming
  const badPoll = await fetch(`${BASE}/v1/pair/poll`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.21" },
    body: JSON.stringify({ claim_id: claim.claim_id, claim_secret: "0".repeat(64) }),
  });
  assert.equal(badPoll.status, 401);

  // Poll before approval -> awaiting_approval
  const pBefore = await fetch(`${BASE}/v1/pair/poll`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.21" },
    body: JSON.stringify({ claim_id: claim.claim_id, claim_secret: claim.claim_secret }),
  });
  assert.equal(pBefore.status, 200);
  assert.equal((await pBefore.json()).status, "awaiting_approval");

  // Host checks status and approves
  const stRes = await fetch(`${BASE}/v1/pair/status?code=${code}`, {
    headers: { Authorization: `Bearer ${host.device_token}` },
  });
  const stJson = await stRes.json();
  assert.equal(stJson.status, "awaiting_approval");
  assert.equal(stJson.pending_device_name, "Desktop-PC");

  const apRes = await fetch(`${BASE}/v1/pair/approve`, {
    method: "POST",
    headers: { Authorization: `Bearer ${host.device_token}`, "Content-Type": "application/json" },
    body: JSON.stringify({ code, approve: true }),
  });
  assert.equal(apRes.status, 200);

  // Device B polls and receives its own token on the same account
  const pAfter = await fetch(`${BASE}/v1/pair/poll`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.21" },
    body: JSON.stringify({ claim_id: claim.claim_id, claim_secret: claim.claim_secret }),
  });
  assert.equal(pAfter.status, 200);
  const devB = await pAfter.json();
  assert.equal(devB.status, "approved");
  assert.equal(devB.account_id, host.account_id);
  assert.notEqual(devB.device_id, host.device_id);
  assert.notEqual(devB.device_token, host.device_token);

  // Replay poll fails (single-use)
  const replay = await fetch(`${BASE}/v1/pair/poll`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.21" },
    body: JSON.stringify({ claim_id: claim.claim_id, claim_secret: claim.claim_secret }),
  });
  assert.equal(replay.status, 404);

  // Device B pulls Device A's save
  const pullB = await fetch(`${BASE}/v1/saves/card1/pull`, {
    headers: { Authorization: `Bearer ${devB.device_token}` },
  });
  assert.equal(pullB.status, 200);
  assert.equal(Buffer.compare(Buffer.from(await pullB.arrayBuffer()), mc), 0);

  // Host lists devices & revokes Device B
  const listRes = await fetch(`${BASE}/v1/devices`, {
    headers: { Authorization: `Bearer ${host.device_token}` },
  });
  const listJson = await listRes.json();
  assert.equal(listJson.devices.length, 2);

  const revRes = await fetch(`${BASE}/v1/devices/revoke`, {
    method: "POST",
    headers: { Authorization: `Bearer ${host.device_token}`, "Content-Type": "application/json" },
    body: JSON.stringify({ device_id: devB.device_id }),
  });
  assert.equal(revRes.status, 200);

  // Device B is now rejected with 403 device_revoked
  const bAfterRev = await fetch(`${BASE}/v1/me`, {
    headers: { Authorization: `Bearer ${devB.device_token}` },
  });
  assert.equal(bAfterRev.status, 403);
  assert.equal((await bAfterRev.json()).error, "device_revoked");

  // Denial flow
  const c2 = await (
    await fetch(`${BASE}/v1/pair/create`, { method: "POST", headers: { Authorization: `Bearer ${host.device_token}` } })
  ).json();
  const cl2 = await (
    await fetch(`${BASE}/v1/pair/claim`, {
      method: "POST",
      headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.22" },
      body: JSON.stringify({ code: c2.code, device_name: "Stranger", platform: "unknown" }),
    })
  ).json();
  await fetch(`${BASE}/v1/pair/approve`, {
    method: "POST",
    headers: { Authorization: `Bearer ${host.device_token}`, "Content-Type": "application/json" },
    body: JSON.stringify({ code: c2.code, approve: false }),
  });
  const deniedPoll = await fetch(`${BASE}/v1/pair/poll`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.22" },
    body: JSON.stringify({ claim_id: cl2.claim_id, claim_secret: cl2.claim_secret }),
  });
  assert.equal(deniedPoll.status, 403);
  assert.equal((await deniedPoll.json()).error, "pairing_denied");
});

test("registration rate limiting and 40-day retention cleanup", async () => {
  // 5 registrations per hour from same IP allowed; 6th blocked with 429
  const spamIp = "203.0.113.77";
  for (let i = 0; i < 5; i++) {
    const r = await fetch(`${BASE}/v1/register`, {
      method: "POST",
      headers: { "Content-Type": "application/json", "CF-Connecting-IP": spamIp },
      body: JSON.stringify({ device_name: `Dev-${i}`, platform: "pc" }),
    });
    assert.equal(r.status, 201);
  }
  const blocked = await fetch(`${BASE}/v1/register`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": spamIp },
    body: JSON.stringify({ device_name: "Dev-Spam", platform: "pc" }),
  });
  assert.equal(blocked.status, 429);
  assert.ok(blocked.headers.get("Retry-After"));

  // Retention cleanup: backdate an account by 41 days and verify cleanup purges it + its saves
  const regOld = await (
    await fetch(`${BASE}/v1/register`, {
      method: "POST",
      headers: { "Content-Type": "application/json", "CF-Connecting-IP": "203.0.113.88" },
      body: JSON.stringify({ device_name: "OldConsole", platform: "portmaster" }),
    })
  ).json();

  await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { Authorization: `Bearer ${regOld.device_token}` },
    body: makeMemoryCard(99),
  });

  const staleTs = Math.floor(Date.now() / 1000) - 41 * 86400;
  execFileSync(
    "wrangler",
    [
      "d1",
      "execute",
      "dw3-cloud-sync-db",
      "--local",
      "--persist-to",
      tmpPersist,
      "--command",
      `UPDATE accounts SET last_seen_at = ${staleTs} WHERE id = '${regOld.account_id}';`,
    ],
    { cwd: WORKER_DIR, stdio: "pipe" },
  );

  const cleanRes = await fetch(`${BASE}/v1/admin/cleanup`, {
    method: "POST",
    headers: { "X-Admin-Secret": ADMIN_SECRET },
  });
  assert.equal(cleanRes.status, 200);
  const cleanJson = await cleanRes.json();
  assert.ok(cleanJson.purgedAccounts >= 1);

  const afterPurge = await fetch(`${BASE}/v1/me`, {
    headers: { Authorization: `Bearer ${regOld.device_token}` },
  });
  assert.equal(afterPurge.status, 401);
});

test("the expansion save rides with the card, and the clients from before it never see it", async () => {
  const reg = await fetch(`${BASE}/v1/register`, {
    method: "POST",
    headers: { "Content-Type": "application/json", "CF-Connecting-IP": "198.51.100.70" },
    body: JSON.stringify({ device_name: "PC-Expansion", platform: "pc" }),
  });
  assert.equal(reg.status, 201);
  const dev = await reg.json();
  const auth = { Authorization: `Bearer ${dev.device_token}` };

  const card = makeMemoryCard(7);
  const ext = Buffer.from(JSON.stringify({ version: 1, slots: [{ gen: 3, personas: { "3": { known: [5, 0], solved: false } } }] }));
  const pair = Buffer.concat([card, ext]);

  // Not JSON, or a size that does not match the body: refused
  const badJson = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { ...auth, "X-Save-Expansion-Size": "4" },
    body: Buffer.concat([card, Buffer.from("nope")]),
  });
  assert.equal(badJson.status, 400);
  const badSize = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { ...auth, "X-Save-Expansion-Size": String(ext.length + 1) },
    body: pair,
  });
  assert.equal(badSize.status, 400);

  // The pair, gzipped as the launcher may send it
  const push = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: {
      ...auth,
      "Content-Encoding": "gzip",
      "X-Expected-Seq": "0",
      "X-Save-Sha256": sha256(card),
      "X-Save-Pair-Sha256": sha256(pair),
      "X-Save-Expansion-Size": String(ext.length),
    },
    body: gzipSync(pair),
  });
  assert.equal(push.status, 200);
  const pj = await push.json();
  assert.equal(pj.seq, 1);
  assert.equal(pj.sha256, sha256(card));
  assert.equal(pj.pair_sha256, sha256(pair));

  // The same card with another expansion is a change, not a duplicate
  const ext2 = Buffer.from(JSON.stringify({ version: 1, slots: [{ gen: 4 }] }));
  const pair2 = Buffer.concat([card, ext2]);
  const push2 = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { ...auth, "X-Expected-Seq": "1", "X-Save-Expansion-Size": String(ext2.length) },
    body: pair2,
  });
  assert.equal((await push2.json()).deduplicated, false);
  const pushDup = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { ...auth, "X-Save-Expansion-Size": String(ext2.length) },
    body: pair2,
  });
  assert.equal((await pushDup.json()).deduplicated, true);

  // Meta: the card's sha for the old clients, the pair's for the new
  const meta = await (await fetch(`${BASE}/v1/saves/card1/pull?meta=1`, { headers: auth })).json();
  assert.equal(meta.sha256, sha256(card));
  assert.equal(meta.pair_sha256, sha256(pair2));
  assert.equal(meta.expansion, true);

  // A new client gets the card then the expansion
  const pullNew = await fetch(`${BASE}/v1/saves/card1/pull`, { headers: { ...auth, "X-Accept-Expansion": "1" } });
  assert.equal(pullNew.status, 200);
  assert.equal(pullNew.headers.get("X-Save-Expansion-Size"), String(ext2.length));
  assert.equal(pullNew.headers.get("X-Save-Pair-Sha256"), sha256(pair2));
  assert.equal(Buffer.compare(Buffer.from(await pullNew.arrayBuffer()), pair2), 0);

  // An old client gets exactly the 128 KB card, with the sha it checks
  const pullOld = await fetch(`${BASE}/v1/saves/card1/pull`, { headers: auth });
  assert.equal(pullOld.headers.get("X-Save-Sha256"), sha256(card));
  assert.equal(Buffer.compare(Buffer.from(await pullOld.arrayBuffer()), card), 0);

  // An old client pushing a card alone clears the expansion: it no longer pairs with that card
  const card2 = makeMemoryCard(8);
  const pushOld = await fetch(`${BASE}/v1/saves/card1/push`, {
    method: "POST",
    headers: { ...auth, "X-Expected-Seq": "2" },
    body: card2,
  });
  assert.equal(pushOld.status, 200);
  const meta2 = await (await fetch(`${BASE}/v1/saves/card1/pull?meta=1`, { headers: auth })).json();
  assert.equal(meta2.expansion, false);
  assert.equal(meta2.pair_sha256, sha256(card2));
  const pullAfter = await fetch(`${BASE}/v1/saves/card1/pull`, { headers: { ...auth, "X-Accept-Expansion": "1" } });
  assert.equal(pullAfter.headers.get("X-Save-Expansion-Size"), "0");
  assert.equal(Buffer.compare(Buffer.from(await pullAfter.arrayBuffer()), card2), 0);
});
