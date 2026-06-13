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
//         POST /admin/keys    body: { key, max_devices?, expires_at?, note? }
//         POST /admin/revoke?key=XXXX-...
//         GET  /admin/list
// HOW:  Keys and activations live in two D1 (SQLite) tables; see schema.sql.
//       Keys are normalized (uppercase, dashes stripped) before storage and
//       lookup, because the client sends them dashed.
// WHY:  Cloudflare's free tier (100k req/day) is thousands of times the
//       expected load (one heartbeat per terminal per launch), needs no
//       patching or uptime monitoring, and includes HTTPS — see README.md.
// =============================================================================

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

      if (url.pathname === "/admin/revoke" && request.method === "POST") {
        const key = norm(url.searchParams.get("key"));
        const r = await env.DB.prepare(
          "UPDATE keys SET revoked = 1 WHERE key = ?",
        )
          .bind(key)
          .run();
        return json({ ok: true, changed: r.meta.changes });
      }

      if (url.pathname === "/admin/list" && request.method === "GET") {
        const { results } = await env.DB.prepare(
          `SELECT k.key, k.max_devices, k.revoked, k.expires_at, k.note,
                  k.created_at, COUNT(a.device_id) AS devices_used
           FROM keys k LEFT JOIN activations a ON a.key = k.key
           GROUP BY k.key ORDER BY k.created_at DESC`,
        ).all();
        return json({ keys: results });
      }
    }

    return json({ error: "not_found" }, 404);
  },
};
