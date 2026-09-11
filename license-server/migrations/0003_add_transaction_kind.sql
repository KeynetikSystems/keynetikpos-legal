-- Migration 0003: distinguish a new-key sale from a renewal in `transactions`,
-- so the idempotent-retry path in /webhook/mpesa (a duplicate Daraja callback
-- for a transaction_id already processed) resends the message that actually
-- matches what happened — "here's your license key" for a sale, "updates
-- renewed through <date>" for a renewal — instead of always assuming a sale.
--
-- Apply to the live D1 database once:
--   wrangler d1 execute <DB_NAME> --remote --file migrations/0003_add_transaction_kind.sql
-- Existing rows default to 'sale', which is correct — this migration predates
-- the renewal feature, so every existing transaction row is a sale.

ALTER TABLE transactions ADD COLUMN kind TEXT NOT NULL DEFAULT 'sale';
