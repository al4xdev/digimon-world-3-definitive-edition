-- Log reports the launcher's SEND LOGS uploads (a zip: logs, crash reports, the player's card). For a database created
-- before it; schema.sql has the table for a new one. Run once: npm run db:migrate:logs:remote
CREATE TABLE IF NOT EXISTS log_reports (
  id TEXT PRIMARY KEY,
  created_at INTEGER NOT NULL,
  ip_hash TEXT NOT NULL,
  country TEXT NOT NULL,
  platform TEXT NOT NULL,
  device_id TEXT,
  launcher TEXT,
  sha256 TEXT NOT NULL,
  size INTEGER NOT NULL,
  zip BLOB NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_log_reports_created ON log_reports(created_at);
