PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS accounts (
  id TEXT PRIMARY KEY,
  created_at INTEGER NOT NULL,
  last_seen_at INTEGER NOT NULL,
  status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'banned')),
  ban_reason TEXT
);

CREATE INDEX IF NOT EXISTS idx_accounts_last_seen ON accounts(last_seen_at);

CREATE TABLE IF NOT EXISTS devices (
  id TEXT PRIMARY KEY,
  account_id TEXT NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  platform TEXT NOT NULL,
  token_hash TEXT NOT NULL UNIQUE,
  status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'revoked')),
  created_at INTEGER NOT NULL,
  last_seen_at INTEGER NOT NULL,
  last_ip_hash TEXT,
  last_country TEXT,
  last_asn INTEGER
);

CREATE INDEX IF NOT EXISTS idx_devices_account ON devices(account_id);
CREATE INDEX IF NOT EXISTS idx_devices_token_hash ON devices(token_hash);

CREATE TABLE IF NOT EXISTS saves (
  account_id TEXT NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
  slot TEXT NOT NULL CHECK (slot IN ('card1', 'card2')),
  sha256 TEXT NOT NULL,
  raw_size INTEGER NOT NULL,
  seq INTEGER NOT NULL,
  updated_at INTEGER NOT NULL,
  updated_by TEXT NOT NULL,
  blob BLOB NOT NULL,
  PRIMARY KEY (account_id, slot)
);

CREATE TABLE IF NOT EXISTS pairing_codes (
  code TEXT PRIMARY KEY,
  account_id TEXT NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
  host_device_id TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'awaiting_approval', 'approved', 'denied')),
  created_at INTEGER NOT NULL,
  expires_at INTEGER NOT NULL,
  claim_id TEXT UNIQUE,
  claim_secret_hash TEXT,
  pending_device_name TEXT,
  pending_platform TEXT,
  pending_ip_hash TEXT,
  pending_country TEXT,
  pending_asn INTEGER
);

CREATE INDEX IF NOT EXISTS idx_pairing_expires ON pairing_codes(expires_at);
CREATE INDEX IF NOT EXISTS idx_pairing_account ON pairing_codes(account_id);

CREATE TABLE IF NOT EXISTS rate_limits (
  bucket TEXT PRIMARY KEY,
  count INTEGER NOT NULL,
  reset_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_rate_limits_reset ON rate_limits(reset_at);
