// =============================================================================
// KeynetikPOS license/activation server — Cloudflare Worker + D1
// -----------------------------------------------------------------------------
// WHAT: Implements the exact API contract the desktop client expects
//       (licensemanager.cpp):
//         GET /activate?key=&device_id=&device_name=
//         GET /validate?key=&device_id=
//       Both return JSON: { "valid": bool, "reason": "..." }.
//       The client maps these reason strings, so they must not change:
//         "device_limit_reached" | "key_revoked" | "key_expired"
//       Admin endpoints (Bearer ADMIN_TOKEN) manage keys:
//         POST /admin/keys        body: { key, max_devices?, expires_at?, note? }
//         POST /admin/generate    body: { max_devices?, expires_at?, note? }
//         POST /admin/sell        body: { customer, email?, max_devices?, note? }
//         POST /admin/revoke?key=XXXX-...
//         GET  /admin/list
// HOW:  Keys and activations live in two D1 (SQLite) tables; see schema.sql.
//       Keys are normalized (uppercase, dashes stripped) before storage and
//       lookup, because the client sends them dashed.
//       Key generation uses the same checksum as licensemanager.cpp:
//         seg1 = first 4 hex chars of SHA256(seg0 + seg2 + "KNK-SALT-2025")
// WHY:  Cloudflare's free tier (100k req/day) is thousands of times the
//       expected load (one heartbeat per terminal per launch), needs no
//       patching or uptime monitoring, and includes HTTPS — see README.md.
// =============================================================================

const CHARSET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"; // no 0/O/1/I/L look-alikes
const KEY_SALT = "KNK-SALT-2025";                   // must match licensemanager.cpp

// ── Key generation helpers ────────────────────────────────────────────────────

function randomSegment() {
  let seg = "";
  const rnd = new Uint8Array(4);
  crypto.getRandomValues(rnd);
  for (const b of rnd) seg += CHARSET[b % CHARSET.length];
  return seg;
}

async function checksumSegment(seg0, seg2) {
  const data = new TextEncoder().encode(seg0 + seg2 + KEY_SALT);
  const hashBuf = await crypto.subtle.digest("SHA-256", data);
  const hex = Array.from(new Uint8Array(hashBuf))
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");
  return hex.slice(0, 4).toUpperCase();
}

async function mintKey() {
  const seg0 = randomSegment();
  const seg2 = randomSegment();
  const seg3 = randomSegment();
  const seg1 = await checksumSegment(seg0, seg2);
  return { plain: `${seg0}-${seg1}-${seg2}-${seg3}`, norm: seg0 + seg1 + seg2 + seg3 };
}

// ── Main handler ──────────────────────────────────────────────────────────────

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    const json = (obj, status = 200) =>
      new Response(JSON.stringify(obj), {
        status,
        headers: { "Content-Type": "application/json" },
      });

    // Client sends "XXXX-XXXX-XXXX-XXXX"; store/compare without dashes.
    const norm = (k) => (k || "").toUpperCase().replace(/-/g, "").trim();

    const isExpired = (row) =>
      row.expires_at && new Date(row.expires_at) < new Date();

    // ── GET /activate ─────────────────────────────────────────────
    if (url.pathname === "/activate" && request.method === "GET") {
      const key = norm(url.searchParams.get("key"));
      const deviceId = (url.searchParams.get("device_id") || "").trim();
      const deviceName = (url.searchParams.get("device_name") || "").trim();

      if (key.length !== 16 || !deviceId)
        return json({ valid: false, reason: "bad_request" }, 400);

      const row = await env.DB.prepare("SELECT * FROM keys WHERE key = ?")
        .bind(key)
        .first();
      if (!row) return json({ valid: false, reason: "invalid_key" });
      if (row.revoked) return json({ valid: false, reason: "key_revoked" });
      if (isExpired(row)) return json({ valid: false, reason: "key_expired" });

      // Re-activating the same device is always allowed (reinstall case).
      const existing = await env.DB.prepare(
        "SELECT 1 FROM activations WHERE key = ? AND device_id = ?",
      )
        .bind(key, deviceId)
        .first();

      if (!existing) {
        const { cnt } = await env.DB.prepare(
          "SELECT COUNT(*) AS cnt FROM activations WHERE key = ?",
        )
          .bind(key)
          .first();
        if (cnt >= row.max_devices)
          return json({ valid: false, reason: "device_limit_reached" });

        await env.DB.prepare(
          "INSERT INTO activations (key, device_id, device_name) VALUES (?, ?, ?)",
        )
          .bind(key, deviceId, deviceName)
          .run();
      }
      return json({ valid: true });
    }

    // ── GET /validate (heartbeat) ─────────────────────────────────
    if (url.pathname === "/validate" && request.method === "GET") {
      const key = norm(url.searchParams.get("key"));
      const deviceId = (url.searchParams.get("device_id") || "").trim();

      const row = await env.DB.prepare("SELECT * FROM keys WHERE key = ?")
        .bind(key)
        .first();
      if (!row) return json({ valid: false, reason: "invalid_key" });
      if (row.revoked) return json({ valid: false, reason: "key_revoked" });
      if (isExpired(row)) return json({ valid: false, reason: "key_expired" });

      // CAUTION: the client writes Revoked=true on ANY valid:false response
      // and enforces it at next launch. Deleting an activations row therefore
      // un-licenses that device — which is the intended way to free a seat.
      const act = await env.DB.prepare(
        "SELECT 1 FROM activations WHERE key = ? AND device_id = ?",
      )
        .bind(key, deviceId)
        .first();
      if (!act) return json({ valid: false, reason: "not_activated" });

      return json({ valid: true });
    }

    // ── Admin (Bearer token) ──────────────────────────────────────
    if (url.pathname.startsWith("/admin/")) {
      // Trim both sides: a secret set via a shell pipe can pick up a trailing
      // newline, which would otherwise never match the header.
      const auth = (request.headers.get("Authorization") || "").trim();
      const expected = `Bearer ${(env.ADMIN_TOKEN || "").trim()}`;
      if (!env.ADMIN_TOKEN || auth !== expected)
        return json({ error: "unauthorized" }, 401);

      // POST /admin/keys — register a pre-minted key (legacy / bulk import)
      if (url.pathname === "/admin/keys" && request.method === "POST") {
        const body = await request.json().catch(() => null);
        const key = norm(body && body.key);
        if (key.length !== 16) return json({ error: "bad key" }, 400);

        await env.DB.prepare(
          `INSERT INTO keys (key, max_devices, revoked, expires_at, note)
           VALUES (?, ?, 0, ?, ?)
           ON CONFLICT(key) DO UPDATE SET
             max_devices = excluded.max_devices,
             expires_at  = excluded.expires_at,
             note        = excluded.note`,
        )
          .bind(
            key,
            (body && body.max_devices) || 1,
            (body && body.expires_at) || null,
            (body && body.note) || null,
          )
          .run();
        return json({ ok: true, key });
      }

      // POST /admin/generate — mint + register a new key in one step
      if (url.pathname === "/admin/generate" && request.method === "POST") {
        const body = await request.json().catch(() => ({}));
        const { plain, norm: key } = await mintKey();

        await env.DB.prepare(
          `INSERT INTO keys (key, max_devices, revoked, expires_at, note)
           VALUES (?, ?, 0, ?, ?)`,
        )
          .bind(
            key,
            body.max_devices || 1,
            body.expires_at || null,
            body.note || null,
          )
          .run();

        return json({ ok: true, key: plain });
      }

      // POST /admin/sell — assign the next unsold key to a customer
      // body: { customer, email?, max_devices?, note? }
      // "Unsold" = in keys table, devices_used = 0, not revoked, not expired.
      // If note contains "unsold" (case-insensitive) it's treated as stock.
      // After assignment the note is replaced with the customer details.
      if (url.pathname === "/admin/sell" && request.method === "POST") {
        const body = await request.json().catch(() => null);
        if (!body || !body.customer)
          return json({ error: "customer field required" }, 400);

        // Pick the oldest unsold key (FIFO — first in, first out of stock)
        const row = await env.DB.prepare(
          `SELECT k.key FROM keys k
           LEFT JOIN activations a ON a.key = k.key
           WHERE k.revoked = 0
             AND (k.expires_at IS NULL OR k.expires_at > datetime('now'))
           GROUP BY k.key
           HAVING COUNT(a.device_id) = 0
           ORDER BY k.created_at ASC
           LIMIT 1`,
        ).first();

        if (!row) return json({ error: "no_stock", message: "No unsold keys available. Generate more with POST /admin/generate." }, 409);

        const note = body.email
          ? `${body.customer} <${body.email}>`
          : body.customer;

        await env.DB.prepare(
          "UPDATE keys SET note = ?, max_devices = ? WHERE key = ?",
        )
          .bind(note, body.max_devices || 1, row.key)
          .run();

        // Format key back to XXXX-XXXX-XXXX-XXXX for the caller
        const k = row.key;
        const plain = `${k.slice(0,4)}-${k.slice(4,8)}-${k.slice(8,12)}-${k.slice(12,16)}`;

        return json({ ok: true, key: plain, customer: note });
      }

      // POST /admin/revoke?key=XXXX-...
      if (url.pathname === "/admin/revoke" && request.method === "POST") {
        const key = norm(url.searchParams.get("key"));
        const r = await env.DB.prepare(
          "UPDATE keys SET revoked = 1 WHERE key = ?",
        )
          .bind(key)
          .run();
        return json({ ok: true, changed: r.meta.changes });
      }

      // GET /admin/list
      if (url.pathname === "/admin/list" && request.method === "GET") {
        const { results } = await env.DB.prepare(
          `SELECT k.key, k.max_devices, k.revoked, k.expires_at, k.note,
                  k.created_at, COUNT(a.device_id) AS devices_used
           FROM keys k LEFT JOIN activations a ON a.key = k.key
           GROUP BY k.key ORDER BY k.created_at DESC`,
        ).all();
        // Format keys with dashes for readability
        const formatted = results.map((r) => ({
          ...r,
          key: `${r.key.slice(0,4)}-${r.key.slice(4,8)}-${r.key.slice(8,12)}-${r.key.slice(12,16)}`,
        }));
        return json({ keys: formatted });
      }
    }

    return json({ error: "not_found" }, 404);
  },
};
