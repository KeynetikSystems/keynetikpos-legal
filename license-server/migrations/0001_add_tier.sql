-- Migration 0001: add the per-key entitlement tier (and optional feature list)
-- so the server can tell each activated install which plan it bought. Without
-- this, /activate and /validate returned no tier and every paid key resolved to
-- Tier 1 on the client (a purchase felt like a downgrade from the Tier-4 trial).
--
-- Apply to the live D1 database once:
--   wrangler d1 execute <DB_NAME> --remote --file migrations/0001_add_tier.sql
-- Existing keys default to tier 1; re-register or re-sell them (or UPDATE the
-- row) to grant a higher tier.

ALTER TABLE keys ADD COLUMN tier INTEGER NOT NULL DEFAULT 1;
ALTER TABLE keys ADD COLUMN features TEXT;
