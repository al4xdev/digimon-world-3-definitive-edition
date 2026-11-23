-- The expansion save (card1.def.json) travels with the card. For a database created before it; schema.sql has the
-- columns for a new one. Run once: npm run db:migrate:expansion:remote
ALTER TABLE saves ADD COLUMN pair_sha256 TEXT;
ALTER TABLE saves ADD COLUMN ext_blob BLOB;
