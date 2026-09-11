-- KeynetikPOS license server schema (Cloudflare D1 / SQLite)
-- Keys are stored normalized: uppercase, 16 chars, no dashes.

CREATE TABLE IF NOT EXISTS keys (
    key         TEXT PRIMARY KEY,
    max_devices INTEGER NOT NULL DEFAULT 1,
    tier        INTEGER NOT NULL DEFAULT 1,             -- 1 POS Core .. 4 ERP Full
    features    TEXT,                                   -- optional JSON array of feature
                                                        --   slugs; NULL = derive from tier
    revoked     INTEGER NOT NULL DEFAULT 0,
    expires_at  TEXT,                                   -- ISO date or NULL = perpetual;
                                                        --   a hard license expiry (distinct
                                                        --   from updates_until below)
    updates_until TEXT,                                 -- ISO date or NULL = updates included
                                                        --   forever. Past this date the key
                                                        --   still activates/validates fine
                                                        --   (the license never expires) — it
                                                        --   only marks the install as no longer
                                                        --   entitled to new version updates.
    note        TEXT,                                   -- e.g. customer name
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS activations (
    key          TEXT NOT NULL REFERENCES keys(key),
    device_id    TEXT NOT NULL,                         -- QSysInfo::machineUniqueId()
    device_name  TEXT,                                  -- hostname, for the admin's benefit
    activated_at TEXT NOT NULL DEFAULT (datetime('now')),
    PRIMARY KEY (key, device_id)
);

-- M-Pesa payment idempotency log.
-- Prevents double key assignment when Daraja retries callbacks.
CREATE TABLE IF NOT EXISTS transactions (
    transaction_id TEXT PRIMARY KEY,                    -- CheckoutRequestID or TransID
    phone          TEXT NOT NULL,
    amount         TEXT,
    key_assigned   TEXT NOT NULL,                       -- the sold key, or the renewed key
    kind           TEXT NOT NULL DEFAULT 'sale',         -- 'sale' | 'renewal' — lets a retried
                                                        --   callback resend the right message
    processed_at   TEXT NOT NULL DEFAULT (datetime('now'))
);

-- In-store M-Pesa STK Push requests initiated by a till (POS checkout).
-- Separate from `transactions` (which assigns license keys): these just track a
-- single customer payment so the till can poll for the result. The till creates
-- the row (status 'pending') and Daraja's callback updates it.
CREATE TABLE IF NOT EXISTS stk_requests (
    checkout_id   TEXT PRIMARY KEY,                     -- Daraja CheckoutRequestID
    merchant_id   TEXT,                                 -- Daraja MerchantRequestID
    license_key   TEXT,                                 -- normalized key of the till
    phone         TEXT NOT NULL,                        -- 2547XXXXXXXX
    amount        INTEGER NOT NULL,                     -- whole KES (M-Pesa has no cents)
    account_ref   TEXT,
    status        TEXT NOT NULL DEFAULT 'pending',      -- pending | success | failed
    mpesa_receipt TEXT,                                 -- e.g. SLJ7X8K2P0
    result_desc   TEXT,
    created_at    TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at    TEXT
);
