-- Jira-style triage fields on log_reports. For a database created before it; schema.sql has the columns for a new one.
-- Run once: npm run db:migrate:triage:remote
ALTER TABLE log_reports ADD COLUMN status TEXT NOT NULL DEFAULT 'new';
ALTER TABLE log_reports ADD COLUMN note TEXT NOT NULL DEFAULT '';
ALTER TABLE log_reports ADD COLUMN updated_at INTEGER;
CREATE INDEX IF NOT EXISTS idx_log_reports_status ON log_reports(status);
