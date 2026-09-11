-- Migration 0002: track an optional per-key "updates entitlement" date,
-- separate from the existing `expires_at` (which is a hard license expiry).
--
-- This backs the perpetual-license + optional-paid-update-renewal model: the
-- key keeps activating/validating forever regardless of this column, but once
-- `updates_until` is in the past the install is no longer entitled to new
-- version updates until it's renewed. Nothing currently reads this to block
-- anything client-side — /activate and /validate just return it in the
-- entitlement payload so the client can store and display it.
--
-- Apply to the live D1 database once:
--   wrangler d1 execute <DB_NAME> --remote --file migrations/0002_add_updates_until.sql
-- Existing keys default to NULL (updates included forever) — nothing changes
-- for them until an admin explicitly sets a value via POST /admin/renew.

ALTER TABLE keys ADD COLUMN updates_until TEXT;
