import {
  constantTimeEqualHex,
  constantTimeEqualStr,
  hmacSha256Hex,
  randomId,
  randomSixDigitCode,
  randomTokenHex,
  sha256Hex,
} from "./crypto";
import { checkRateLimit } from "./rateLimit";
import {
  gunzipBounded,
  gzipBytes,
  PayloadError,
  readLimitedStream,
  toUint8Array,
  validateExpansion,
  validatePs1MemoryCard,
} from "./save";
import {
  ALLOWED_PLATFORMS,
  ALLOWED_SLOTS,
  EXPANSION_MAX,
  PS1_MC_SIZE,
  type AuthContext, type ClientMeta, type Env } from "./types";

const SECURITY_HEADERS: Record<string, string> = {
  "X-Content-Type-Options": "nosniff",
  "Cache-Control": "no-store",
  "Referrer-Policy": "no-referrer",
};

function json(data: unknown, status = 200, extraHeaders: Record<string, string> = {}): Response {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      "Content-Type": "application/json; charset=utf-8",
      ...SECURITY_HEADERS,
      ...extraHeaders,
    },
  });
}

function errJson(error: string, status: number, detail?: string, extraHeaders: Record<string, string> = {}): Response {
  return json(detail ? { error, detail } : { error }, status, extraHeaders);
}

function nowSec(): number {
  return Math.floor(Date.now() / 1000);
}

function intEnv(v: string | undefined, fallback: number): number {
  const n = Number(v);
  return Number.isFinite(n) && n > 0 ? Math.floor(n) : fallback;
}

function sanitizeDeviceName(raw: unknown): string {
  const s = typeof raw === "string" ? raw : "";
  const cleaned = s.replace(/[^\x20-\x7E]/g, "").trim().slice(0, 40);
  return cleaned || "DW3 Device";
}

function sanitizePlatform(raw: unknown): string {
  const s = typeof raw === "string" ? raw.trim().toLowerCase() : "unknown";
  return ALLOWED_PLATFORMS.has(s) ? s : "unknown";
}

function normalizeClientIp(rawIp: string): string {
  const ip = rawIp.trim().split("%")[0] ?? "0.0.0.0";
  if (!ip.includes(":")) return ip;
  const lower = ip.toLowerCase();
  if (lower.startsWith("::ffff:") && lower.includes(".")) {
    return lower.slice(7);
  }
  const halves = lower.split("::");
  if (halves.length > 2) return lower;
  const left = halves[0] ? halves[0].split(":").filter(Boolean) : [];
  const right = halves.length === 2 && halves[1] ? halves[1].split(":").filter(Boolean) : [];
  const missing = 8 - (left.length + right.length);
  if (missing < 0) return lower;
  const groups = [...left, ...Array<string>(missing).fill("0"), ...right].map((g) => g.padStart(4, "0"));
  return `${groups.slice(0, 4).join(":")}::/64`;
}

async function getClientMeta(request: Request, env: Env): Promise<ClientMeta> {
  const cf = (request as Request & { cf?: { country?: string; asn?: number } }).cf;
  const cfIp = request.headers.get("CF-Connecting-IP");
  // Trust X-Forwarded-For only in local dev / tests when CF-Connecting-IP and request.cf are absent.
  const rawIp = cfIp ?? (!cf ? request.headers.get("X-Forwarded-For")?.split(",")[0]?.trim() : undefined) ?? "0.0.0.0";
  const normalizedIp = normalizeClientIp(rawIp);
  const url = new URL(request.url);
  const isLocal = url.hostname === "127.0.0.1" || url.hostname === "localhost";
  const salt = env.IP_SALT || (isLocal ? "dw3-dev-local-salt" : "");
  if (!salt) throw new Error("IP_SALT secret is required in production");
  const ipHash = await hmacSha256Hex(salt, normalizedIp);
  const country = typeof cf?.country === "string" ? cf.country.slice(0, 8) : "XX";
  const asn = typeof cf?.asn === "number" ? cf.asn : 0;
  return { ipHash, country, asn };
}

async function readJsonBody(request: Request, maxBytes = 8192): Promise<Record<string, unknown>> {
  const cl = Number(request.headers.get("Content-Length") ?? "0");
  if (cl > maxBytes) throw new PayloadError("json_too_large", 413);
  if (!request.body) return {};
  const bytes = await readLimitedStream(request.body, maxBytes);
  if (bytes.byteLength === 0) return {};
  try {
    const parsed = JSON.parse(new TextDecoder().decode(bytes));
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
      throw new PayloadError("invalid_json", 400);
    }
    return parsed as Record<string, unknown>;
  } catch (e) {
    if (e instanceof PayloadError) throw e;
    throw new PayloadError("invalid_json", 400);
  }
}

async function isBadAuthBlocked(env: Env, ipHash: string, ts: number): Promise<number | null> {
  const row = await env.DB.prepare(`SELECT count, reset_at FROM rate_limits WHERE bucket = ?1`)
    .bind(`badauth:ip:${ipHash}`)
    .first<{ count: number; reset_at: number }>();
  if (row && Number(row.reset_at) > ts && Number(row.count) >= 20) {
    return Math.max(1, Number(row.reset_at) - ts);
  }
  return null;
}

async function authenticate(request: Request, env: Env, meta: ClientMeta, ts: number): Promise<AuthContext | Response> {
  const blockedRetry = await isBadAuthBlocked(env, meta.ipHash, ts);
  if (blockedRetry !== null) {
    return errJson("rate_limited", 429, undefined, { "Retry-After": String(blockedRetry) });
  }

  const authHeader = request.headers.get("Authorization") ?? "";
  if (!authHeader.startsWith("Bearer ")) {
    return errJson("unauthorized", 401);
  }
  const token = authHeader.slice(7).trim();
  if (token.length < 32 || token.length > 128 || !/^[0-9a-fA-F]+$/.test(token)) {
    const rl = await checkRateLimit(env, `badauth:ip:${meta.ipHash}`, 20, 900, ts);
    if (!rl.allowed) {
      return errJson("rate_limited", 429, undefined, { "Retry-After": String(rl.retryAfterSec) });
    }
    return errJson("unauthorized", 401);
  }

  const tokenHash = await sha256Hex(token);
  const row = await env.DB.prepare(
    `SELECT d.id AS device_id, d.account_id, d.name, d.platform, d.status AS device_status,
            d.last_seen_at AS device_last_seen, a.status AS account_status, a.ban_reason,
            a.last_seen_at AS account_last_seen
     FROM devices d
     JOIN accounts a ON a.id = d.account_id
     WHERE d.token_hash = ?1`,
  )
    .bind(tokenHash)
    .first<{
      device_id: string;
      account_id: string;
      name: string;
      platform: string;
      device_status: string;
      device_last_seen: number;
      account_status: string;
      ban_reason: string | null;
      account_last_seen: number;
    }>();

  if (!row) {
    const rl = await checkRateLimit(env, `badauth:ip:${meta.ipHash}`, 20, 900, ts);
    if (!rl.allowed) {
      return errJson("rate_limited", 429, undefined, { "Retry-After": String(rl.retryAfterSec) });
    }
    return errJson("unauthorized", 401);
  }
  if (row.account_status === "banned") {
    return errJson("account_banned", 403, row.ban_reason ?? "Account suspended");
  }
  if (row.device_status !== "active") {
    return errJson("device_revoked", 403, "Device access was revoked");
  }

  return {
    accountId: row.account_id,
    deviceId: row.device_id,
    deviceName: row.name,
    platform: row.platform,
    accountLastSeenAt: Number(row.account_last_seen),
    deviceLastSeenAt: Number(row.device_last_seen),
  };
}

/** Update last_seen_at at most once per hour on read-only endpoints to preserve D1 free-tier write quota. */
async function touchActivity(env: Env, auth: AuthContext, meta: ClientMeta, ts: number, force = false): Promise<number> {
  const needsUpdate = force || ts - auth.accountLastSeenAt >= 3600 || ts - auth.deviceLastSeenAt >= 3600;
  if (!needsUpdate) return auth.accountLastSeenAt;
  await env.DB.batch([
    env.DB.prepare(`UPDATE accounts SET last_seen_at = ?1 WHERE id = ?2`).bind(ts, auth.accountId),
    env.DB.prepare(
      `UPDATE devices SET last_seen_at = ?1, last_ip_hash = ?2, last_country = ?3, last_asn = ?4 WHERE id = ?5`,
    ).bind(ts, meta.ipHash, meta.country, meta.asn, auth.deviceId),
  ]);
  auth.accountLastSeenAt = ts;
  auth.deviceLastSeenAt = ts;
  return ts;
}

export async function runRetentionCleanup(env: Env, ts = nowSec()): Promise<{
  expiredPairings: number;
  expiredRateLimits: number;
  purgedAccounts: number;
}> {
  const retentionDays = intEnv(env.RETENTION_DAYS, 40);
  const cutoff = ts - retentionDays * 86400;

  await env.DB.prepare(`DELETE FROM log_reports WHERE created_at < ?1`).bind(cutoff).run();
  const pRes = await env.DB.prepare(`DELETE FROM pairing_codes WHERE expires_at <= ?1`).bind(ts).run();
  const rRes = await env.DB.prepare(`DELETE FROM rate_limits WHERE reset_at <= ?1`).bind(ts).run();

  let purgedAccounts = 0;
  for (let page = 0; page < 20; page++) {
    const stale = await env.DB.prepare(`SELECT id FROM accounts WHERE last_seen_at < ?1 LIMIT 100`)
      .bind(cutoff)
      .all<{ id: string }>();
    const rows = stale.results ?? [];
    if (rows.length === 0) break;

    for (const row of rows) {
      // Guard every delete against concurrent reactivation between SELECT and DELETE.
      const res = await env.DB.batch([
        env.DB.prepare(
          `DELETE FROM saves WHERE account_id = ?1 AND EXISTS (SELECT 1 FROM accounts a WHERE a.id = ?1 AND a.last_seen_at < ?2)`,
        ).bind(row.id, cutoff),
        env.DB.prepare(
          `DELETE FROM pairing_codes WHERE account_id = ?1 AND EXISTS (SELECT 1 FROM accounts a WHERE a.id = ?1 AND a.last_seen_at < ?2)`,
        ).bind(row.id, cutoff),
        env.DB.prepare(
          `DELETE FROM devices WHERE account_id = ?1 AND EXISTS (SELECT 1 FROM accounts a WHERE a.id = ?1 AND a.last_seen_at < ?2)`,
        ).bind(row.id, cutoff),
        env.DB.prepare(`DELETE FROM accounts WHERE id = ?1 AND last_seen_at < ?2`).bind(row.id, cutoff),
      ]);
      if (Number(res[3]?.meta?.changes ?? 0) > 0) purgedAccounts++;
    }
    if (rows.length < 100) break;
  }

  return {
    expiredPairings: Number(pRes.meta?.changes ?? 0),
    expiredRateLimits: Number(rRes.meta?.changes ?? 0),
    purgedAccounts,
  };
}

async function handleRegister(request: Request, env: Env, meta: ClientMeta, ts: number): Promise<Response> {
  const rlHour = await checkRateLimit(env, `reg:ip_h:${meta.ipHash}`, 5, 3600, ts);
  if (!rlHour.allowed) {
    return errJson("rate_limited", 429, "Too many registrations from this IP (hourly)", {
      "Retry-After": String(rlHour.retryAfterSec),
    });
  }
  const rlDay = await checkRateLimit(env, `reg:ip_d:${meta.ipHash}`, 15, 86400, ts);
  if (!rlDay.allowed) {
    return errJson("rate_limited", 429, "Too many registrations from this IP (daily)", {
      "Retry-After": String(rlDay.retryAfterSec),
    });
  }
  if (meta.asn > 0) {
    const rlAsn = await checkRateLimit(env, `reg:asn:${meta.asn}`, 120, 3600, ts);
    if (!rlAsn.allowed) {
      return errJson("rate_limited", 429, "Network registration rate exceeded", {
        "Retry-After": String(rlAsn.retryAfterSec),
      });
    }
  }

  const body = await readJsonBody(request);
  const deviceName = sanitizeDeviceName(body.device_name);
  const platform = sanitizePlatform(body.platform);

  const accountId = randomId("acc");
  const deviceId = randomId("dev");
  const deviceToken = randomTokenHex(32);
  const tokenHash = await sha256Hex(deviceToken);
  const retentionDays = intEnv(env.RETENTION_DAYS, 40);

  await env.DB.batch([
    env.DB.prepare(`INSERT INTO accounts (id, created_at, last_seen_at, status) VALUES (?1, ?2, ?2, 'active')`).bind(
      accountId,
      ts,
    ),
    env.DB.prepare(
      `INSERT INTO devices (id, account_id, name, platform, token_hash, status, created_at, last_seen_at, last_ip_hash, last_country, last_asn)
       VALUES (?1, ?2, ?3, ?4, ?5, 'active', ?6, ?6, ?7, ?8, ?9)`,
    ).bind(deviceId, accountId, deviceName, platform, tokenHash, ts, meta.ipHash, meta.country, meta.asn),
  ]);

  return json(
    {
      account_id: accountId,
      device_id: deviceId,
      device_token: deviceToken,
      device_name: deviceName,
      platform,
      retention_days: retentionDays,
      expires_at: ts + retentionDays * 86400,
    },
    201,
  );
}

async function handleMe(env: Env, auth: AuthContext, meta: ClientMeta, ts: number): Promise<Response> {
  const effectiveLastSeen = await touchActivity(env, auth, meta, ts, false);
  const retentionDays = intEnv(env.RETENTION_DAYS, 40);

  const [devCountRow, savesRows] = await Promise.all([
    env.DB.prepare(`SELECT COUNT(*) AS cnt FROM devices WHERE account_id = ?1 AND status = 'active'`)
      .bind(auth.accountId)
      .first<{ cnt: number }>(),
    env.DB.prepare(
      `SELECT slot, sha256, pair_sha256, raw_size, seq, updated_at, updated_by FROM saves WHERE account_id = ?1`,
    )
      .bind(auth.accountId)
      .all<{
        slot: string;
        sha256: string;
        pair_sha256: string | null;
        raw_size: number;
        seq: number;
        updated_at: number;
        updated_by: string;
      }>(),
  ]);

  const saves: Record<string, unknown> = {};
  for (const s of savesRows.results ?? []) {
    saves[s.slot] = {
      sha256: s.sha256,
      pair_sha256: s.pair_sha256 ?? s.sha256,
      raw_size: Number(s.raw_size),
      seq: Number(s.seq),
      updated_at: Number(s.updated_at),
      updated_by: s.updated_by,
    };
  }

  return json({
    account_id: auth.accountId,
    device_id: auth.deviceId,
    device_name: auth.deviceName,
    platform: auth.platform,
    active_devices: Number(devCountRow?.cnt ?? 1),
    retention_days: retentionDays,
    expires_at: effectiveLastSeen + retentionDays * 86400,
    saves,
  });
}

async function handleListDevices(env: Env, auth: AuthContext, meta: ClientMeta, ts: number): Promise<Response> {
  await touchActivity(env, auth, meta, ts, false);
  const rows = await env.DB.prepare(
    `SELECT id, name, platform, status, created_at, last_seen_at, last_country
     FROM devices
     WHERE account_id = ?1
     ORDER BY created_at ASC`,
  )
    .bind(auth.accountId)
    .all<{
      id: string;
      name: string;
      platform: string;
      status: string;
      created_at: number;
      last_seen_at: number;
      last_country: string | null;
    }>();

  return json({
    devices: (rows.results ?? []).map((d) => ({
      device_id: d.id,
      name: d.name,
      platform: d.platform,
      status: d.status,
      created_at: Number(d.created_at),
      last_seen_at: Number(d.last_seen_at),
      country: d.last_country ?? "XX",
      is_current: d.id === auth.deviceId,
    })),
  });
}

async function handleRevokeDevice(request: Request, env: Env, auth: AuthContext, ts: number): Promise<Response> {
  const body = await readJsonBody(request);
  const targetId = typeof body.device_id === "string" ? body.device_id.trim() : "";
  if (!targetId) return errJson("missing_device_id", 400);

  const res = await env.DB.prepare(
    `UPDATE devices SET status = 'revoked' WHERE id = ?1 AND account_id = ?2 AND status = 'active'`,
  )
    .bind(targetId, auth.accountId)
    .run();

  if (Number(res.meta?.changes ?? 0) === 0) {
    return errJson("device_not_found", 404);
  }
  await env.DB.prepare(`UPDATE accounts SET last_seen_at = ?1 WHERE id = ?2`).bind(ts, auth.accountId).run();
  return json({ ok: true, revoked_device_id: targetId });
}

async function handleSavePush(
  request: Request,
  env: Env,
  auth: AuthContext,
  meta: ClientMeta,
  slot: string,
  ts: number,
): Promise<Response> {
  const rl = await checkRateLimit(env, `push:acc:${auth.accountId}`, 60, 3600, ts);
  if (!rl.allowed) {
    return errJson("rate_limited", 429, "Too many save uploads", { "Retry-After": String(rl.retryAfterSec) });
  }

  /* The expansion save (card1.def.json) rides after the card's 128 KB when the client says how long it is. A client
   * from before it sends the card alone: that clears the stored expansion, which no longer pairs with the new card
   * (the game's generation check would read it as empty anyway). */
  const extHeader = request.headers.get("X-Save-Expansion-Size");
  let extSize = 0;
  if (extHeader !== null) {
    extSize = Number(extHeader);
    if (!Number.isInteger(extSize) || extSize < 0 || extSize > EXPANSION_MAX) {
      return errJson("invalid_expansion_size", 400);
    }
  }
  const rawMax = PS1_MC_SIZE + extSize;
  const cl = Number(request.headers.get("Content-Length") ?? "0");
  if (cl > rawMax + 4096) {
    return errJson("payload_too_large", 413);
  }
  if (!request.body) {
    return errJson("empty_body", 400);
  }

  const bodyBytes = await readLimitedStream(request.body, rawMax + 4096);
  const ce = (request.headers.get("Content-Encoding") ?? "").toLowerCase().trim();
  let rawBytes: Uint8Array;
  if (ce === "gzip") {
    try {
      rawBytes = await gunzipBounded(bodyBytes, rawMax);
    } catch (e) {
      if (e instanceof PayloadError) throw e;
      return errJson("invalid_gzip", 400);
    }
  } else if (ce === "" || ce === "identity") {
    rawBytes = bodyBytes;
  } else {
    return errJson("unsupported_encoding", 415);
  }

  if (rawBytes.byteLength !== rawMax) {
    return errJson("invalid_save_size", 400, `expected ${rawMax} bytes, got ${rawBytes.byteLength}`);
  }
  const cardBytes = extSize > 0 ? rawBytes.subarray(0, PS1_MC_SIZE) : rawBytes;
  validatePs1MemoryCard(cardBytes);
  if (extSize > 0) validateExpansion(rawBytes.subarray(PS1_MC_SIZE));

  /* sha256 stays the card's alone: the clients from before the expansion compare it with their card. pair_sha256 is
   * the card and the expansion together (sha256 of both, the card first), the card's own sha with no expansion. */
  const computedSha = await sha256Hex(cardBytes);
  const pairSha = extSize > 0 ? await sha256Hex(rawBytes) : computedSha;
  const declaredSha = request.headers.get("X-Save-Sha256")?.trim().toLowerCase();
  if (declaredSha && declaredSha !== computedSha) {
    return errJson("sha256_mismatch", 400, `expected ${declaredSha}, got ${computedSha}`);
  }
  const declaredPair = request.headers.get("X-Save-Pair-Sha256")?.trim().toLowerCase();
  if (declaredPair && declaredPair !== pairSha) {
    return errJson("sha256_mismatch", 400, `expected pair ${declaredPair}, got ${pairSha}`);
  }

  const existing = await env.DB.prepare(
    `SELECT sha256, pair_sha256, seq, updated_at FROM saves WHERE account_id = ?1 AND slot = ?2`,
  )
    .bind(auth.accountId, slot)
    .first<{ sha256: string; pair_sha256: string | null; seq: number; updated_at: number }>();

  const existingSeq = Number(existing?.seq ?? 0);

  // A client from before the expansion (no X-Save-Expansion-Size) re-pushing the card the cloud already holds changes
  // nothing: its JSON still pairs with that card, so it is kept (clearing it here would make a newer launcher's next
  // pull delete its local card1.def.json).
  const legacySameCard = extHeader === null && existing != null && existing.sha256 === computedSha;
  if (existing && ((existing.pair_sha256 ?? existing.sha256) === pairSha || legacySameCard)) {
    await touchActivity(env, auth, meta, ts, true);
    return json({
      ok: true,
      deduplicated: true,
      slot,
      seq: existingSeq,
      sha256: computedSha,
      pair_sha256: existing.pair_sha256 ?? existing.sha256,
      updated_at: Number(existing.updated_at),
    });
  }

  const expectedSeqHeader = request.headers.get("X-Expected-Seq");
  const force = request.headers.get("X-Force") === "1";
  if (expectedSeqHeader !== null && !force) {
    const expectedSeq = Number(expectedSeqHeader);
    if (!Number.isInteger(expectedSeq) || expectedSeq < 0) {
      return errJson("invalid_expected_seq", 400);
    }
    if (expectedSeq !== existingSeq) {
      return json(
        {
          error: "conflict",
          slot,
          server_seq: existingSeq,
          server_sha256: existing?.sha256 ?? null,
          server_updated_at: existing ? Number(existing.updated_at) : null,
        },
        409,
      );
    }
  }

  const compressedBlob = await gzipBytes(cardBytes);
  const extBlob = extSize > 0 ? await gzipBytes(rawBytes.subarray(PS1_MC_SIZE), EXPANSION_MAX + 4096) : null;

  let committedSeq = 0;
  if (!existing) {
    const ins = await env.DB.prepare(
      `INSERT INTO saves (account_id, slot, sha256, raw_size, seq, updated_at, updated_by, blob, pair_sha256, ext_blob)
       VALUES (?1, ?2, ?3, ?4, 1, ?5, ?6, ?7, ?8, ?9)
       ON CONFLICT(account_id, slot) DO NOTHING
       RETURNING seq`,
    )
      .bind(auth.accountId, slot, computedSha, PS1_MC_SIZE, ts, auth.deviceId, compressedBlob, pairSha, extBlob)
      .first<{ seq: number }>();
    if (ins) committedSeq = Number(ins.seq);
  } else if (force) {
    const upd = await env.DB.prepare(
      `UPDATE saves
       SET sha256 = ?1, raw_size = ?2, seq = seq + 1, updated_at = ?3, updated_by = ?4, blob = ?5,
           pair_sha256 = ?8, ext_blob = ?9
       WHERE account_id = ?6 AND slot = ?7
       RETURNING seq`,
    )
      .bind(computedSha, PS1_MC_SIZE, ts, auth.deviceId, compressedBlob, auth.accountId, slot, pairSha, extBlob)
      .first<{ seq: number }>();
    if (upd) committedSeq = Number(upd.seq);
  } else {
    const upd = await env.DB.prepare(
      `UPDATE saves
       SET sha256 = ?1, raw_size = ?2, seq = seq + 1, updated_at = ?3, updated_by = ?4, blob = ?5,
           pair_sha256 = ?9, ext_blob = ?10
       WHERE account_id = ?6 AND slot = ?7 AND seq = ?8
       RETURNING seq`,
    )
      .bind(computedSha, PS1_MC_SIZE, ts, auth.deviceId, compressedBlob, auth.accountId, slot, existingSeq, pairSha, extBlob)
      .first<{ seq: number }>();
    if (upd) committedSeq = Number(upd.seq);
  }

  if (committedSeq === 0) {
    const latest = await env.DB.prepare(
      `SELECT sha256, seq, updated_at FROM saves WHERE account_id = ?1 AND slot = ?2`,
    )
      .bind(auth.accountId, slot)
      .first<{ sha256: string; seq: number; updated_at: number }>();
    return json(
      {
        error: "conflict",
        slot,
        server_seq: Number(latest?.seq ?? 0),
        server_sha256: latest?.sha256 ?? null,
        server_updated_at: latest ? Number(latest.updated_at) : null,
      },
      409,
    );
  }

  await touchActivity(env, auth, meta, ts, true);

  return json({
    ok: true,
    deduplicated: false,
    slot,
    seq: committedSeq,
    sha256: computedSha,
    pair_sha256: pairSha,
    expansion_size: extSize,
    raw_size: PS1_MC_SIZE,
    stored_bytes: compressedBlob.byteLength,
    updated_at: ts,
  });
}

async function handleSavePull(
  request: Request,
  env: Env,
  auth: AuthContext,
  meta: ClientMeta,
  slot: string,
  ts: number,
): Promise<Response> {
  await touchActivity(env, auth, meta, ts, false);
  const url = new URL(request.url);
  const metaOnly = url.searchParams.get("meta") === "1" || request.method === "HEAD";

  if (metaOnly) {
    const row = await env.DB.prepare(
      `SELECT sha256, pair_sha256, ext_blob IS NOT NULL AS has_ext, raw_size, seq, updated_at, updated_by
       FROM saves WHERE account_id = ?1 AND slot = ?2`,
    )
      .bind(auth.accountId, slot)
      .first<{
        sha256: string;
        pair_sha256: string | null;
        has_ext: number;
        raw_size: number;
        seq: number;
        updated_at: number;
        updated_by: string;
      }>();

    if (!row) {
      return errJson("save_not_found", 404);
    }
    const hdrs = {
      ETag: `"${row.sha256}"`,
      "X-Save-Seq": String(row.seq),
      "X-Save-Sha256": row.sha256,
      "X-Save-Updated-At": String(row.updated_at),
      "X-Save-Size": String(row.raw_size),
      "X-Save-Pair-Sha256": row.pair_sha256 ?? row.sha256,
    };
    if (request.method === "HEAD") {
      return new Response(null, { status: 200, headers: { ...SECURITY_HEADERS, ...hdrs } });
    }
    return json(
      {
        slot,
        sha256: row.sha256,
        pair_sha256: row.pair_sha256 ?? row.sha256,
        expansion: Number(row.has_ext) === 1,
        raw_size: Number(row.raw_size),
        seq: Number(row.seq),
        updated_at: Number(row.updated_at),
        updated_by: row.updated_by,
      },
      200,
      hdrs,
    );
  }

  const row = await env.DB.prepare(
    `SELECT sha256, pair_sha256, ext_blob, raw_size, seq, updated_at, updated_by, blob
     FROM saves WHERE account_id = ?1 AND slot = ?2`,
  )
    .bind(auth.accountId, slot)
    .first<{
      sha256: string;
      pair_sha256: string | null;
      ext_blob: unknown;
      raw_size: number;
      seq: number;
      updated_at: number;
      updated_by: string;
      blob: unknown;
    }>();

  if (!row) {
    return errJson("save_not_found", 404);
  }

  const inm = request.headers.get("If-None-Match")?.replace(/"/g, "").trim().toLowerCase();
  if (inm && inm === row.sha256) {
    return new Response(null, {
      status: 304,
      headers: {
        ...SECURITY_HEADERS,
        ETag: `"${row.sha256}"`,
        "X-Save-Seq": String(row.seq),
        "X-Save-Sha256": row.sha256,
        "X-Save-Updated-At": String(row.updated_at),
      },
    });
  }

  const gzBytes = toUint8Array(row.blob);
  const cardBytes = await gunzipBounded(gzBytes, PS1_MC_SIZE);
  /* Only a client that asks gets the expansion after the card; the ones from before it read exactly 128 KB. */
  let rawBytes = cardBytes;
  let extSize = 0;
  if (request.headers.get("X-Accept-Expansion") === "1" && row.ext_blob != null) {
    const ext = await gunzipBounded(toUint8Array(row.ext_blob), EXPANSION_MAX);
    extSize = ext.byteLength;
    rawBytes = new Uint8Array(cardBytes.byteLength + extSize);
    rawBytes.set(cardBytes, 0);
    rawBytes.set(ext, cardBytes.byteLength);
  }

  return new Response(rawBytes, {
    status: 200,
    headers: {
      "Content-Type": "application/octet-stream",
      "Content-Length": String(rawBytes.byteLength),
      "X-Save-Expansion-Size": String(extSize),
      "X-Save-Pair-Sha256": row.pair_sha256 ?? row.sha256,
      ETag: `"${row.sha256}"`,
      "X-Save-Seq": String(row.seq),
      "X-Save-Sha256": row.sha256,
      "X-Save-Updated-At": String(row.updated_at),
      "X-Save-Updated-By": row.updated_by,
      ...SECURITY_HEADERS,
    },
  });
}

async function handlePairCreate(env: Env, auth: AuthContext, ts: number): Promise<Response> {
  const rl = await checkRateLimit(env, `pair_create:acc:${auth.accountId}`, 12, 3600, ts);
  if (!rl.allowed) {
    return errJson("rate_limited", 429, "Too many pairing codes created", {
      "Retry-After": String(rl.retryAfterSec),
    });
  }

  const ttl = intEnv(env.PAIRING_TTL_SEC, 300);
  const expiresAt = ts + ttl;

  // Replace any older unapproved pairing code opened by this host device (preserve approved ones awaiting poll).
  await env.DB.prepare(
    `DELETE FROM pairing_codes WHERE host_device_id = ?1 AND status IN ('pending', 'awaiting_approval', 'denied')`,
  )
    .bind(auth.deviceId)
    .run();

  let code = "";
  for (let attempt = 0; attempt < 5; attempt++) {
    const candidate = randomSixDigitCode();
    try {
      await env.DB.prepare(
        `INSERT INTO pairing_codes (code, account_id, host_device_id, status, created_at, expires_at)
         VALUES (?1, ?2, ?3, 'pending', ?4, ?5)`,
      )
        .bind(candidate, auth.accountId, auth.deviceId, ts, expiresAt)
        .run();
      code = candidate;
      break;
    } catch {
      // Collision on 6-digit PK; retry with a fresh code.
    }
  }

  if (!code) {
    return errJson("pairing_unavailable", 503);
  }

  return json({ code, expires_in: ttl, expires_at: expiresAt }, 201);
}

async function handlePairClaim(request: Request, env: Env, meta: ClientMeta, ts: number): Promise<Response> {
  const rl = await checkRateLimit(env, `pair_claim:ip:${meta.ipHash}`, 6, 900, ts);
  if (!rl.allowed) {
    return errJson("rate_limited", 429, "Too many pairing attempts", { "Retry-After": String(rl.retryAfterSec) });
  }
  if (meta.asn > 0) {
    const rlAsn = await checkRateLimit(env, `pair_claim:asn:${meta.asn}`, 40, 900, ts);
    if (!rlAsn.allowed) {
      return errJson("rate_limited", 429, "Too many pairing attempts on network", {
        "Retry-After": String(rlAsn.retryAfterSec),
      });
    }
  }

  const body = await readJsonBody(request);
  const code = typeof body.code === "string" ? body.code.replace(/\D/g, "") : "";
  if (code.length !== 6) {
    return errJson("invalid_code_format", 400);
  }

  const deviceName = sanitizeDeviceName(body.device_name);
  const platform = sanitizePlatform(body.platform);
  const claimId = randomId("clm");
  const claimSecret = randomTokenHex(32);
  const claimSecretHash = await sha256Hex(claimSecret);

  const res = await env.DB.prepare(
    `UPDATE pairing_codes
     SET status = 'awaiting_approval',
         claim_id = ?1,
         claim_secret_hash = ?2,
         pending_device_name = ?3,
         pending_platform = ?4,
         pending_ip_hash = ?5,
         pending_country = ?6,
         pending_asn = ?7
     WHERE code = ?8 AND status = 'pending' AND expires_at > ?9`,
  )
    .bind(claimId, claimSecretHash, deviceName, platform, meta.ipHash, meta.country, meta.asn, code, ts)
    .run();

  if (Number(res.meta?.changes ?? 0) === 0) {
    return errJson("invalid_or_expired_code", 404);
  }

  return json({
    claim_id: claimId,
    claim_secret: claimSecret,
    status: "awaiting_approval",
  });
}

async function handlePairStatus(request: Request, env: Env, auth: AuthContext, ts: number): Promise<Response> {
  const url = new URL(request.url);
  const code = (url.searchParams.get("code") ?? "").replace(/\D/g, "");
  if (code.length !== 6) return errJson("invalid_code_format", 400);

  const row = await env.DB.prepare(
    `SELECT status, expires_at, pending_device_name, pending_platform, pending_country
     FROM pairing_codes
     WHERE code = ?1 AND account_id = ?2 AND host_device_id = ?3`,
  )
    .bind(code, auth.accountId, auth.deviceId)
    .first<{
      status: string;
      expires_at: number;
      pending_device_name: string | null;
      pending_platform: string | null;
      pending_country: string | null;
    }>();

  if (!row || Number(row.expires_at) <= ts) {
    return errJson("pairing_expired", 404);
  }

  return json({
    code,
    status: row.status,
    expires_in: Math.max(0, Number(row.expires_at) - ts),
    pending_device_name: row.pending_device_name,
    pending_platform: row.pending_platform,
    pending_country: row.pending_country,
  });
}

async function handlePairApprove(request: Request, env: Env, auth: AuthContext, ts: number): Promise<Response> {
  const body = await readJsonBody(request);
  const code = typeof body.code === "string" ? body.code.replace(/\D/g, "") : "";
  if (code.length !== 6) return errJson("invalid_code_format", 400);
  const approve = body.approve === true;

  if (!approve) {
    await env.DB.prepare(
      `UPDATE pairing_codes SET status = 'denied'
       WHERE code = ?1 AND account_id = ?2 AND host_device_id = ?3 AND status IN ('pending', 'awaiting_approval')`,
    )
      .bind(code, auth.accountId, auth.deviceId)
      .run();
    return json({ ok: true, status: "denied" });
  }

  const maxDevices = intEnv(env.MAX_DEVICES_PER_ACCOUNT, 10);
  const cntRow = await env.DB.prepare(
    `SELECT
       (SELECT COUNT(*) FROM devices WHERE account_id = ?1 AND status = 'active') +
       (SELECT COUNT(*) FROM pairing_codes WHERE account_id = ?1 AND status = 'approved' AND expires_at > ?2) AS cnt`,
  )
    .bind(auth.accountId, ts)
    .first<{ cnt: number }>();

  if (Number(cntRow?.cnt ?? 0) >= maxDevices) {
    return errJson("max_devices_reached", 409, `Maximum ${maxDevices} active devices per account`);
  }

  const res = await env.DB.prepare(
    `UPDATE pairing_codes
     SET status = 'approved', expires_at = ?1
     WHERE code = ?2 AND account_id = ?3 AND host_device_id = ?4 AND status = 'awaiting_approval' AND expires_at > ?5`,
  )
    .bind(ts + 120, code, auth.accountId, auth.deviceId, ts)
    .run();

  if (Number(res.meta?.changes ?? 0) === 0) {
    return errJson("no_pending_claim", 409);
  }
  return json({ ok: true, status: "approved" });
}

async function handlePairPoll(request: Request, env: Env, meta: ClientMeta, ts: number): Promise<Response> {
  const rl = await checkRateLimit(env, `pair_poll:ip:${meta.ipHash}`, 90, 300, ts);
  if (!rl.allowed) {
    return errJson("rate_limited", 429, undefined, { "Retry-After": String(rl.retryAfterSec) });
  }

  const body = await readJsonBody(request);
  const claimId = typeof body.claim_id === "string" ? body.claim_id.trim() : "";
  const claimSecret = typeof body.claim_secret === "string" ? body.claim_secret.trim() : "";
  if (!claimId || claimSecret.length < 32) {
    return errJson("invalid_claim", 400);
  }

  const row = await env.DB.prepare(
    `SELECT code, account_id, status, expires_at, claim_secret_hash,
            pending_device_name, pending_platform, pending_ip_hash, pending_country, pending_asn
     FROM pairing_codes
     WHERE claim_id = ?1`,
  )
    .bind(claimId)
    .first<{
      code: string;
      account_id: string;
      status: string;
      expires_at: number;
      claim_secret_hash: string | null;
      pending_device_name: string | null;
      pending_platform: string | null;
      pending_ip_hash: string | null;
      pending_country: string | null;
      pending_asn: number | null;
    }>();

  if (!row || !row.claim_secret_hash) {
    return errJson("pairing_not_found", 404);
  }

  const providedHash = await sha256Hex(claimSecret);
  if (!constantTimeEqualHex(providedHash, row.claim_secret_hash)) {
    return errJson("unauthorized", 401);
  }

  if (Number(row.expires_at) <= ts) {
    await env.DB.prepare(`DELETE FROM pairing_codes WHERE code = ?1 AND expires_at <= ?2`).bind(row.code, ts).run();
    return errJson("pairing_expired", 410);
  }

  if (row.status === "denied") {
    await env.DB.prepare(`DELETE FROM pairing_codes WHERE code = ?1 AND status = 'denied'`).bind(row.code).run();
    return errJson("pairing_denied", 403);
  }

  if (row.status !== "approved") {
    return json({ status: row.status, expires_in: Math.max(0, Number(row.expires_at) - ts) });
  }

  const maxDevices = intEnv(env.MAX_DEVICES_PER_ACCOUNT, 10);
  // Atomically verify active account + device cap + consume approved pairing in a single statement.
  const del = await env.DB.prepare(
    `DELETE FROM pairing_codes
     WHERE code = ?1
       AND status = 'approved'
       AND expires_at > ?4
       AND (SELECT status FROM accounts WHERE id = ?2) = 'active'
       AND (SELECT COUNT(*) FROM devices WHERE account_id = ?2 AND status = 'active') < ?3`,
  )
    .bind(row.code, row.account_id, maxDevices, ts)
    .run();
  if (Number(del.meta?.changes ?? 0) === 0) {
    return errJson("pairing_unavailable_or_cap_reached", 409);
  }

  const deviceId = randomId("dev");
  const deviceToken = randomTokenHex(32);
  const tokenHash = await sha256Hex(deviceToken);
  const deviceName = sanitizeDeviceName(row.pending_device_name);
  const platform = sanitizePlatform(row.pending_platform);
  const retentionDays = intEnv(env.RETENTION_DAYS, 40);

  await env.DB.batch([
    env.DB.prepare(
      `INSERT INTO devices (id, account_id, name, platform, token_hash, status, created_at, last_seen_at, last_ip_hash, last_country, last_asn)
       VALUES (?1, ?2, ?3, ?4, ?5, 'active', ?6, ?6, ?7, ?8, ?9)`,
    ).bind(
      deviceId,
      row.account_id,
      deviceName,
      platform,
      tokenHash,
      ts,
      row.pending_ip_hash ?? meta.ipHash,
      row.pending_country ?? meta.country,
      row.pending_asn ?? meta.asn,
    ),
    env.DB.prepare(`UPDATE accounts SET last_seen_at = ?1 WHERE id = ?2`).bind(ts, row.account_id),
  ]);

  return json({
    status: "approved",
    account_id: row.account_id,
    device_id: deviceId,
    device_token: deviceToken,
    device_name: deviceName,
    platform,
    retention_days: retentionDays,
  });
}

async function handleAdmin(request: Request, env: Env, meta: ClientMeta, path: string, ts: number): Promise<Response> {
  if (!env.ADMIN_SECRET) return errJson("not_found", 404);
  const blockedRetry = await isBadAuthBlocked(env, `adm:${meta.ipHash}`, ts);
  if (blockedRetry !== null) {
    return errJson("rate_limited", 429, undefined, { "Retry-After": String(blockedRetry) });
  }
  const supplied = request.headers.get("X-Admin-Secret") ?? "";
  if (!(await constantTimeEqualStr(supplied, env.ADMIN_SECRET))) {
    const rl = await checkRateLimit(env, `badauth:ip:adm:${meta.ipHash}`, 10, 900, ts);
    if (!rl.allowed) {
      return errJson("rate_limited", 429, undefined, { "Retry-After": String(rl.retryAfterSec) });
    }
    return errJson("unauthorized", 401);
  }

  if (path === "/v1/admin/ban" && request.method === "POST") {
    const body = await readJsonBody(request);
    if (typeof body.account_id === "string" && body.account_id) {
      const reason = typeof body.reason === "string" ? body.reason.slice(0, 120) : "Abuse";
      await env.DB.prepare(`UPDATE accounts SET status = 'banned', ban_reason = ?1 WHERE id = ?2`)
        .bind(reason, body.account_id)
        .run();
      return json({ ok: true, banned_account_id: body.account_id });
    }
    if (typeof body.device_id === "string" && body.device_id) {
      await env.DB.prepare(`UPDATE devices SET status = 'revoked' WHERE id = ?1`).bind(body.device_id).run();
      return json({ ok: true, revoked_device_id: body.device_id });
    }
    return errJson("missing_target", 400);
  }

  if (path === "/v1/admin/logs" && request.method === "GET") {
    const rows = await env.DB.prepare(
      `SELECT id, created_at, country, platform, device_id, launcher, size FROM log_reports ORDER BY created_at DESC LIMIT 100`,
    ).all();
    return json({ ok: true, reports: rows.results ?? [] });
  }
  const logMatch = /^\/v1\/admin\/logs\/([A-Z0-9]{5}-[A-Z0-9]{5})$/.exec(path);
  if (logMatch && request.method === "GET") {
    const row = await env.DB.prepare(`SELECT zip FROM log_reports WHERE id = ?1`).bind(logMatch[1]!).first<{ zip: unknown }>();
    if (!row) return errJson("not_found", 404);
    return new Response(toUint8Array(row.zip), { headers: { "Content-Type": "application/zip", "Content-Disposition": `attachment; filename="dw3-logs-${logMatch[1]}.zip"` } });
  }

  if (path === "/v1/admin/cleanup" && request.method === "POST") {
    const stats = await runRetentionCleanup(env, ts);
    return json({ ok: true, ...stats });
  }

  return errJson("not_found", 404);
}

/* The launcher's SEND LOGS: a zip (logs, crash reports, the player's card). No account: a crash can come before the
 * registration. Bounded instead: 1.5 MB (a D1 row holds 2 MB), a ZIP's magic, 5 a hour and 20 a day per IP. */
const LOG_MAX = 1_500_000;
/* The report's code, the one a player types or photographs into an issue: 10 characters of an alphabet without the look-alikes
 * (no 0/O, 1/I/L), as XXXXX-XXXXX. 32^10 is plenty against guessing a stranger's report. */
const CODE_ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
function reportCode(): string {
  const b = crypto.getRandomValues(new Uint8Array(10));
  const c = Array.from(b, (x) => CODE_ALPHABET[x % CODE_ALPHABET.length]).join("");
  return `${c.slice(0, 5)}-${c.slice(5)}`;
}
async function handleLogUpload(request: Request, env: Env, meta: ClientMeta, ts: number): Promise<Response> {
  const hour = await checkRateLimit(env, `logs:h:${meta.ipHash}`, 5, 3600, ts);
  const day = await checkRateLimit(env, `logs:d:${meta.ipHash}`, 20, 86400, ts);
  if (!hour.allowed || !day.allowed) {
    return errJson("rate_limited", 429, undefined, { "Retry-After": String(Math.max(hour.retryAfterSec, 1)) });
  }
  const cl = Number(request.headers.get("Content-Length") ?? "0");
  if (cl > LOG_MAX) throw new PayloadError("logs_too_large", 413);
  if (!request.body) throw new PayloadError("empty_body", 400);
  const bytes = await readLimitedStream(request.body, LOG_MAX);
  if (bytes.byteLength < 22 || bytes[0] !== 0x50 || bytes[1] !== 0x4b) throw new PayloadError("not_a_zip", 400);
  const clean = (name: string, max: number): string =>
    (request.headers.get(name) ?? "").replace(/[^\x20-\x7e]/g, "").slice(0, max);
  const platform = ALLOWED_PLATFORMS.has(clean("X-Platform", 16)) ? clean("X-Platform", 16) : "unknown";
  const id = reportCode();
  await env.DB.prepare(
    `INSERT INTO log_reports (id, created_at, ip_hash, country, platform, device_id, launcher, sha256, size, zip)
     VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)`,
  )
    .bind(id, ts, meta.ipHash, meta.country, platform, clean("X-Device-Id", 64) || null, clean("X-Launcher", 40) || null,
      await sha256Hex(bytes), bytes.byteLength, bytes)
    .run();
  return json({ ok: true, id });
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const ts = nowSec();
    const url = new URL(request.url);
    const path = url.pathname;

    try {
      if (path === "/v1/health" && request.method === "GET") {
        return json({
          ok: true,
          service: "dw3-cloud-sync",
          retention_days: intEnv(env.RETENTION_DAYS, 40),
        });
      }

      const meta = await getClientMeta(request, env);

      if (path.startsWith("/v1/admin/")) {
        return await handleAdmin(request, env, meta, path, ts);
      }

      if (path === "/v1/register" && request.method === "POST") {
        return await handleRegister(request, env, meta, ts);
      }
      if (path === "/v1/logs" && request.method === "POST") {
        return await handleLogUpload(request, env, meta, ts);
      }
      if (path === "/v1/pair/claim" && request.method === "POST") {
        return await handlePairClaim(request, env, meta, ts);
      }
      if (path === "/v1/pair/poll" && request.method === "POST") {
        return await handlePairPoll(request, env, meta, ts);
      }

      // All remaining endpoints require a valid active device Bearer token.
      const authOrErr = await authenticate(request, env, meta, ts);
      if (authOrErr instanceof Response) return authOrErr;
      const auth = authOrErr;

      if (path === "/v1/me" && request.method === "GET") {
        return await handleMe(env, auth, meta, ts);
      }
      if (path === "/v1/devices" && request.method === "GET") {
        return await handleListDevices(env, auth, meta, ts);
      }
      if (path === "/v1/devices/revoke" && request.method === "POST") {
        return await handleRevokeDevice(request, env, auth, ts);
      }
      if (path === "/v1/pair/create" && request.method === "POST") {
        return await handlePairCreate(env, auth, ts);
      }
      if (path === "/v1/pair/status" && request.method === "GET") {
        return await handlePairStatus(request, env, auth, ts);
      }
      if (path === "/v1/pair/approve" && request.method === "POST") {
        return await handlePairApprove(request, env, auth, ts);
      }

      const saveMatch = /^\/v1\/saves\/(card1|card2)\/(push|pull)$/.exec(path);
      if (saveMatch) {
        const slot = saveMatch[1]!;
        const op = saveMatch[2]!;
        if (!ALLOWED_SLOTS.has(slot)) return errJson("invalid_slot", 400);
        if (op === "push" && request.method === "POST") {
          return await handleSavePush(request, env, auth, meta, slot, ts);
        }
        if (op === "pull" && (request.method === "GET" || request.method === "HEAD")) {
          return await handleSavePull(request, env, auth, meta, slot, ts);
        }
      }

      return errJson("not_found", 404);
    } catch (err) {
      if (err instanceof PayloadError) {
        return errJson(err.message, err.status);
      }
      console.error("Unhandled worker error:", err);
      return errJson("internal_error", 500);
    }
  },

  async scheduled(_controller: ScheduledController, env: Env, ctx: ExecutionContext): Promise<void> {
    ctx.waitUntil(runRetentionCleanup(env));
  },
} satisfies ExportedHandler<Env>;
