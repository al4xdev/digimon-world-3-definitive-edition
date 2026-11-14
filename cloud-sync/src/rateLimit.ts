import type { Env } from "./types";

export interface RateCheckResult {
  allowed: boolean;
  count: number;
  retryAfterSec: number;
}

/**
 * Fixed-window atomic rate limiter backed by D1.
 * Single UPSERT ... RETURNING statement avoids read-then-write races.
 */
export async function checkRateLimit(
  env: Env,
  bucket: string,
  limit: number,
  windowSec: number,
  nowSec: number,
): Promise<RateCheckResult> {
  const nextReset = nowSec + windowSec;
  const row = await env.DB.prepare(
    `INSERT INTO rate_limits (bucket, count, reset_at)
     VALUES (?1, 1, ?2)
     ON CONFLICT(bucket) DO UPDATE SET
       count = CASE WHEN rate_limits.reset_at <= ?3 THEN 1 ELSE rate_limits.count + 1 END,
       reset_at = CASE WHEN rate_limits.reset_at <= ?3 THEN ?2 ELSE rate_limits.reset_at END
     RETURNING count, reset_at`,
  )
    .bind(bucket, nextReset, nowSec)
    .first<{ count: number; reset_at: number }>();

  const count = Number(row?.count ?? 1);
  const resetAt = Number(row?.reset_at ?? nextReset);
  return {
    allowed: count <= limit,
    count,
    retryAfterSec: Math.max(1, resetAt - nowSec),
  };
}
