-- KeynetikPOS license server schema (Cloudflare D1 / SQLite)
-- Keys are stored normalized: uppercase, 16 chars, no dashes.

CREATE TABLE IF NOT EXISTS keys (
    key         TEXT PRIMARY KEY,
    max_devices INTEGER NOT NULL DEFAULT 1,
    revoked     INTEGER NOT NULL DEFAULT 0,
    expires_at  TEXT,                                   -- ISO date or NULL = perpetual
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
