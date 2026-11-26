export interface Env {
  DB: D1Database;
  RETENTION_DAYS: string;
  PAIRING_TTL_SEC: string;
  MAX_DEVICES_PER_ACCOUNT: string;
  ALLOW_TEST_ENDPOINTS?: string;
  IP_SALT?: string;
  ADMIN_SECRET?: string;
}

export interface ClientMeta {
  ipHash: string;
  country: string;
  asn: number;
}

export interface AuthContext {
  accountId: string;
  deviceId: string;
  deviceName: string;
  platform: string;
  accountLastSeenAt: number;
  deviceLastSeenAt: number;
}

export const PS1_MC_SIZE = 131072;
/* The expansion save (card1.def.json) beside the card: a few KB in practice, 1 MB at most. */
export const EXPANSION_MAX = 1 << 20;
export const ALLOWED_SLOTS = new Set(["card1", "card2"]);
export const ALLOWED_PLATFORMS = new Set(["portmaster", "pc", "android", "unknown"]);
export const ALLOWED_LOG_STATUSES = new Set([
  "new",
  "triaging",
  "bug",
  "critical",
  "fixed",
  "wontfix",
  "duplicate",
]);
