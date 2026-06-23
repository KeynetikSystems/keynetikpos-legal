// =============================================================================
// KeynetikPOS license/activation server — Cloudflare Worker + D1
// -----------------------------------------------------------------------------
// Endpoints consumed by the desktop client (licensemanager.cpp):
//   GET  /activate?key=&device_id=&device_name=
//   GET  /validate?key=&device_id=
//
// Admin endpoints (Bearer ADMIN_TOKEN):
//   POST /admin/keys        body: { key, max_devices?, tier?, expires_at?, note? }
//   POST /admin/generate    body: { max_devices?, tier?, expires_at?, note? }
//   POST /admin/sell        body: { customer, email?, max_devices?, tier? }
//   ( tier: 1 POS Core .. 4 ERP Full; defaults to 1. /activate + /validate
//     return { valid, tier, features? } so the client unlocks the right plan. )
//   POST /admin/revoke?key=XXXX-...
//   GET  /admin/list
//
// In-store M-Pesa STK Push (license-key auth via X-License-Key + X-Device-Id):
//   POST /pos/mpesa/stkpush   body { phone, amount, accountRef? } -> { checkoutId }
//   GET  /pos/mpesa/status?checkout_id=...   -> { status, receipt }
//   POST /pos/mpesa/callback  (Daraja calls this; updates the request, no key)
//
// M-Pesa license-sale webhooks (no auth — called by Safaricom):
//   POST /webhook/mpesa     handles both STK Push and C2B callbacks
//
// Stripe payment webhooks (HMAC-verified — called by Stripe):
//   POST /webhook/stripe    handles checkout.session.completed
//
// Required secrets (wrangler secret put <NAME>):
//   ADMIN_TOKEN            — protects /admin/* endpoints
//   AT_API_KEY             — Africa's Talking API key (SMS delivery)
//   AT_USERNAME            — Africa's Talking username ("sandbox" or your app name)
//   AT_SENDER_ID           — optional registered sender ID
//   RESEND_API_KEY         — Resend API key (email delivery)
//   RESEND_FROM            — "KeynetikPOS <noreply@yourdomain.com>"
//   STRIPE_WEBHOOK_SECRET  — whsec_... from Stripe Dashboard → Webhooks
//   MPESA_CONSUMER_KEY     — Daraja app consumer key
//   MPESA_CONSUMER_SECRET  — Daraja app consumer secret
//   MPESA_SHORTCODE        — Paybill/Till (sandbox: 174379)
//   MPESA_PASSKEY          — Daraja Lipa-na-M-Pesa passkey
// Required vars (wrangler.toml [vars] or `wrangler deploy --var`):
//   MPESA_ENV              — "sandbox" (default) or "production"
//   MPESA_TXN_TYPE         — "paybill" (default) or "buygoods"
// =============================================================================

const CHARSET  = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"; // no 0/O/1/I/L look-alikes
const KEY_SALT = "KNK-SALT-2025";                    // must match licensemanager.cpp

// ── Key generation ────────────────────────────────────────────────────────────

function randomSegment() {
  const rnd = new Uint8Array(4);
  crypto.getRandomValues(rnd);
  return Array.from(rnd, (b) => CHARSET[b % CHARSET.length]).join("");
}

async function checksumSegment(seg0, seg2) {
  const data = new TextEncoder().encode(seg0 + seg2 + KEY_SALT);
  const buf  = await crypto.subtle.digest("SHA-256", data);
  return Array.from(new Uint8Array(buf))
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("")
    .slice(0, 4)
    .toUpperCase();
}

async function mintKey() {
  const seg0 = randomSegment();
  const seg2 = randomSegment();
  const seg3 = randomSegment();
  const seg1 = await checksumSegment(seg0, seg2);
  const norm  = seg0 + seg1 + seg2 + seg3;
  return { plain: `${seg0}-${seg1}-${seg2}-${seg3}`, norm };
}

function formatKey(raw16) {
  return `${raw16.slice(0,4)}-${raw16.slice(4,8)}-${raw16.slice(8,12)}-${raw16.slice(12,16)}`;
}

// The entitlement payload returned by /activate and /validate. The client reads
// `tier` (defaulting to 1) and applies it; `features` is only sent when a key
// carries an explicit override, otherwise the client derives the feature list
// from the tier.
function entitlement(row) {
  const out = { valid: true, tier: row.tier ?? 1 };
  if (row.features) {
    try { out.features = JSON.parse(row.features); } catch { /* ignore bad JSON */ }
  }
  return out;
}

// Maps a paid amount (whole KES) to a plan tier for self-serve M-Pesa purchases.
// ADJUST THESE THRESHOLDS to your pricing — they are the price list, in code.
function tierForAmount(kes) {
  if (kes >= 15000) return 4;   // ERP Full
  if (kes >= 7000)  return 3;   // ERP Lite
  if (kes >= 3000)  return 2;   // POS Pro
  return 1;                     // POS Core
}

// ── Stock assignment (shared by /admin/sell and /webhook/mpesa) ───────────────

async function assignNextKey(db, note, maxDevices = 1, tier = 1) {
  const row = await db.prepare(
    `SELECT k.key FROM keys k
     LEFT JOIN activations a ON a.key = k.key
     WHERE k.revoked = 0
       AND (k.expires_at IS NULL OR k.expires_at > datetime('now'))
     GROUP BY k.key
     HAVING COUNT(a.device_id) = 0
     ORDER BY k.created_at ASC
     LIMIT 1`,
  ).first();

  if (!row) return null;

  await db.prepare("UPDATE keys SET note = ?, max_devices = ?, tier = ? WHERE key = ?")
    .bind(note, maxDevices, tier, row.key)
    .run();

  return formatKey(row.key);
}

// ── SMS via Africa's Talking ──────────────────────────────────────────────────

async function sendSms(env, phone, plainKey) {
  if (!env.AT_API_KEY || !env.AT_USERNAME) {
    console.warn("SMS skipped: AT_API_KEY or AT_USERNAME not set");
    return;
  }

  const message =
    `Your KeynetikPOS license key:\n${plainKey}\n` +
    `Enter it at first launch to activate. Support: support@keynetik.com`;

  const params = new URLSearchParams({
    username: env.AT_USERNAME,
    to:       phone,
    message,
  });
  if (env.AT_SENDER_ID) params.set("from", env.AT_SENDER_ID);

  const res = await fetch("https://api.africastalking.com/version1/messaging", {
    method:  "POST",
    headers: {
      apiKey:          env.AT_API_KEY,
      "Content-Type":  "application/x-www-form-urlencoded",
      Accept:          "application/json",
    },
    body: params.toString(),
  });

  if (!res.ok) console.error("AT SMS error:", await res.text());
}

// ── Email via Resend ──────────────────────────────────────────────────────────

async function sendEmail(env, to, plainKey, displayName) {
  if (!env.RESEND_API_KEY || !env.RESEND_FROM) {
    console.warn("Email skipped: RESEND_API_KEY or RESEND_FROM not set");
    return;
  }

  const html = `
    <div style="font-family:sans-serif;max-width:520px;margin:0 auto">
      <h2 style="color:#1a1a2e">Your KeynetikPOS License Key</h2>
      <p>Hi ${displayName},</p>
      <p>Thank you for your purchase! Your license key is:</p>
      <p style="font-size:22px;font-weight:bold;letter-spacing:3px;
                font-family:monospace;background:#f4f4f8;padding:12px 16px;
                border-radius:6px;display:inline-block">${plainKey}</p>
      <p>Enter this key the first time you launch KeynetikPOS to activate your copy.</p>
      <hr style="border:none;border-top:1px solid #eee;margin:24px 0">
      <p style="color:#666;font-size:13px">
        Need help? Reply to this email or contact
        <a href="mailto:support@keynetik.com">support@keynetik.com</a>
      </p>
    </div>`;

  const res = await fetch("https://api.resend.com/emails", {
    method:  "POST",
    headers: {
      Authorization:  `Bearer ${env.RESEND_API_KEY}`,
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      from:    env.RESEND_FROM,
      to:      [to],
      subject: "Your KeynetikPOS License Key",
      html,
    }),
  });

  if (!res.ok) console.error("Resend email error:", await res.text());
}

// ── Phone normalisation ───────────────────────────────────────────────────────
// Daraja sends phone as 2547XXXXXXXX (no +). AT expects +2547XXXXXXXX.

function normalisePhone(raw) {
  const digits = String(raw).replace(/\D/g, "");
  if (digits.startsWith("0"))  return "+254" + digits.slice(1);
  if (digits.startsWith("254")) return "+"  + digits;
  return "+" + digits;
}

// ── M-Pesa Daraja (STK Push) ───────────────────────────────────────────────────
// The till never holds Daraja credentials: it asks this Worker to initiate the
// push, Daraja calls /pos/mpesa/callback with the result, and the till polls
// /pos/mpesa/status. Credentials are Worker secrets (see header).

function darajaBase(env) {
  return env.MPESA_ENV === "production"
    ? "https://api.safaricom.co.ke"
    : "https://sandbox.safaricom.co.ke";
}

// Daraja wants the MSISDN as 2547XXXXXXXX (12 digits, no '+').
function mpesaMsisdn(raw) {
  const d = String(raw).replace(/\D/g, "");
  if (d.startsWith("0"))                       return "254" + d.slice(1);
  if (d.startsWith("254"))                     return d;
  if (d.startsWith("7") || d.startsWith("1"))  return "254" + d;
  return d;
}

async function darajaToken(env) {
  const creds = btoa(`${env.MPESA_CONSUMER_KEY}:${env.MPESA_CONSUMER_SECRET}`);
  const res = await fetch(
    `${darajaBase(env)}/oauth/v1/generate?grant_type=client_credentials`,
    { headers: { Authorization: `Basic ${creds}` } },
  );
  if (!res.ok) throw new Error(`Daraja auth ${res.status}: ${await res.text()}`);
  return (await res.json()).access_token;
}

// Daraja timestamp + password must use the SAME timestamp (UTC is fine).
function darajaTimestamp() {
  const d = new Date(), p = (n) => String(n).padStart(2, "0");
  return `${d.getUTCFullYear()}${p(d.getUTCMonth() + 1)}${p(d.getUTCDate())}` +
         `${p(d.getUTCHours())}${p(d.getUTCMinutes())}${p(d.getUTCSeconds())}`;
}

// A till authenticates with its license key + device id (same pair as /validate):
// the key must exist, be unrevoked, unexpired, and activated on that device.
async function posAuthorised(env, key, deviceId) {
  if (key.length !== 16 || !deviceId) return false;
  const row = await env.DB.prepare("SELECT * FROM keys WHERE key = ?").bind(key).first();
  if (!row || row.revoked) return false;
  if (row.expires_at && new Date(row.expires_at) < new Date()) return false;
  const act = await env.DB.prepare(
    "SELECT 1 FROM activations WHERE key = ? AND device_id = ?",
  ).bind(key, deviceId).first();
  return !!act;
}

// ── Stripe signature verification ────────────────────────────────────────────
// Stripe signs every webhook with HMAC-SHA256 over "<timestamp>.<rawBody>".
// We must verify this before trusting the payload.

async function verifyStripeSignature(rawBody, sigHeader, secret) {
  if (!sigHeader || !secret) return false;

  // sigHeader: "t=1234567890,v1=abc123,v0=..."
  const parts  = Object.fromEntries(sigHeader.split(",").map((p) => p.split("=")));
  const ts     = parts.t;
  const v1     = parts.v1;
  if (!ts || !v1) return false;

  // Reject if timestamp is more than 5 minutes old (replay protection)
  if (Math.abs(Date.now() / 1000 - Number(ts)) > 300) return false;

  const key = await crypto.subtle.importKey(
    "raw",
    new TextEncoder().encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign"],
  );
  const sig = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(`${ts}.${rawBody}`));
  const hex = Array.from(new Uint8Array(sig)).map((b) => b.toString(16).padStart(2, "0")).join("");

  return hex === v1;
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

    const norm = (k) => (k || "").toUpperCase().replace(/-/g, "").trim();
    const isExpired = (row) => row.expires_at && new Date(row.expires_at) < new Date();

    // ── GET /activate ─────────────────────────────────────────────
    if (url.pathname === "/activate" && request.method === "GET") {
      const key      = norm(url.searchParams.get("key"));
      const deviceId = (url.searchParams.get("device_id")   || "").trim();
      const devName  = (url.searchParams.get("device_name") || "").trim();

      if (key.length !== 16 || !deviceId)
        return json({ valid: false, reason: "bad_request" }, 400);

      const row = await env.DB.prepare("SELECT * FROM keys WHERE key = ?").bind(key).first();
      if (!row)          return json({ valid: false, reason: "invalid_key" });
      if (row.revoked)   return json({ valid: false, reason: "key_revoked" });
      if (isExpired(row)) return json({ valid: false, reason: "key_expired" });

      const existing = await env.DB.prepare(
        "SELECT 1 FROM activations WHERE key = ? AND device_id = ?",
      ).bind(key, deviceId).first();

      if (!existing) {
        const { cnt } = await env.DB.prepare(
          "SELECT COUNT(*) AS cnt FROM activations WHERE key = ?",
        ).bind(key).first();
        if (cnt >= row.max_devices)
          return json({ valid: false, reason: "device_limit_reached" });

        await env.DB.prepare(
          "INSERT INTO activations (key, device_id, device_name) VALUES (?, ?, ?)",
        ).bind(key, deviceId, devName).run();
      }
      return json(entitlement(row));
    }

    // ── GET /validate (heartbeat) ─────────────────────────────────
    if (url.pathname === "/validate" && request.method === "GET") {
      const key      = norm(url.searchParams.get("key"));
      const deviceId = (url.searchParams.get("device_id") || "").trim();

      const row = await env.DB.prepare("SELECT * FROM keys WHERE key = ?").bind(key).first();
      if (!row)           return json({ valid: false, reason: "invalid_key" });
      if (row.revoked)    return json({ valid: false, reason: "key_revoked" });
      if (isExpired(row)) return json({ valid: false, reason: "key_expired" });

      const act = await env.DB.prepare(
        "SELECT 1 FROM activations WHERE key = ? AND device_id = ?",
      ).bind(key, deviceId).first();
      if (!act) return json({ valid: false, reason: "not_activated" });

      return json(entitlement(row));
    }

    // ── POST /pos/mpesa/stkpush ───────────────────────────────────
    // A licensed till asks the Worker to trigger an STK Push for an in-store
    // sale. Returns the CheckoutRequestID the till then polls on.
    if (url.pathname === "/pos/mpesa/stkpush" && request.method === "POST") {
      const key      = norm(request.headers.get("X-License-Key"));
      const deviceId = (request.headers.get("X-Device-Id") || "").trim();
      if (!(await posAuthorised(env, key, deviceId)))
        return json({ ok: false, error: "unauthorized" }, 401);

      if (!env.MPESA_CONSUMER_KEY || !env.MPESA_SHORTCODE || !env.MPESA_PASSKEY)
        return json({ ok: false, error: "mpesa_not_configured" }, 503);

      const body   = await request.json().catch(() => null);
      const phone  = mpesaMsisdn(body?.phone ?? "");
      const amount = Math.round(Number(body?.amount));          // whole KES
      const ref    = String(body?.accountRef ?? "POS").slice(0, 12) || "POS";
      if (!/^2547\d{8}$/.test(phone) || !(amount >= 1))
        return json({ ok: false, error: "bad_request" }, 400);

      let token;
      try { token = await darajaToken(env); }
      catch (e) { console.error(e); return json({ ok: false, error: "daraja_auth_failed" }, 502); }

      const ts       = darajaTimestamp();
      const password = btoa(`${env.MPESA_SHORTCODE}${env.MPESA_PASSKEY}${ts}`);
      const txnType  = env.MPESA_TXN_TYPE === "buygoods"
                         ? "CustomerBuyGoodsOnline" : "CustomerPayBillOnline";

      const stkRes = await fetch(`${darajaBase(env)}/mpesa/stkpush/v1/processrequest`, {
        method:  "POST",
        headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
        body: JSON.stringify({
          BusinessShortCode: env.MPESA_SHORTCODE,
          Password:          password,
          Timestamp:         ts,
          TransactionType:   txnType,
          Amount:            amount,
          PartyA:            phone,
          PartyB:            env.MPESA_SHORTCODE,
          PhoneNumber:       phone,
          CallBackURL:       `${url.origin}/pos/mpesa/callback`,
          AccountReference:  ref,
          TransactionDesc:   "KeynetikPOS sale",
        }),
      });

      const stk = await stkRes.json().catch(() => ({}));
      if (stk.ResponseCode !== "0")
        return json({ ok: false,
          error: stk.errorMessage || stk.ResponseDescription || "stk_failed" }, 502);

      await env.DB.prepare(
        `INSERT INTO stk_requests (checkout_id, merchant_id, license_key, phone, amount, account_ref)
         VALUES (?, ?, ?, ?, ?, ?)`,
      ).bind(stk.CheckoutRequestID, stk.MerchantRequestID ?? null, key, phone, amount, ref).run();

      return json({ ok: true,
        checkoutId: stk.CheckoutRequestID,
        customerMessage: stk.CustomerMessage ?? "STK push sent" });
    }

    // ── POST /pos/mpesa/callback ──────────────────────────────────
    // Daraja delivers the STK result here. Updates the till's request row; it
    // NEVER assigns license keys (that's /webhook/mpesa). Always returns 0.
    if (url.pathname === "/pos/mpesa/callback" && request.method === "POST") {
      const body = await request.json().catch(() => null);
      const cb   = body?.Body?.stkCallback;
      if (!cb || !cb.CheckoutRequestID)
        return json({ ResultCode: 0, ResultDesc: "Ignored" });

      const ok = cb.ResultCode === 0;
      let receipt = null;
      if (ok) {
        const items = cb.CallbackMetadata?.Item ?? [];
        receipt = items.find((i) => i.Name === "MpesaReceiptNumber")?.Value ?? null;
      }
      await env.DB.prepare(
        `UPDATE stk_requests
            SET status = ?, mpesa_receipt = ?, result_desc = ?, updated_at = datetime('now')
          WHERE checkout_id = ?`,
      ).bind(ok ? "success" : "failed", receipt, cb.ResultDesc ?? null, cb.CheckoutRequestID).run();

      return json({ ResultCode: 0, ResultDesc: "OK" });
    }

    // ── GET /pos/mpesa/status?checkout_id= ────────────────────────
    // The till polls this for the outcome of its STK push.
    if (url.pathname === "/pos/mpesa/status" && request.method === "GET") {
      const key      = norm(request.headers.get("X-License-Key"));
      const deviceId = (request.headers.get("X-Device-Id") || "").trim();
      if (!(await posAuthorised(env, key, deviceId)))
        return json({ error: "unauthorized" }, 401);

      const id  = (url.searchParams.get("checkout_id") || "").trim();
      const row = await env.DB.prepare(
        `SELECT status, mpesa_receipt, result_desc, amount
           FROM stk_requests WHERE checkout_id = ? AND license_key = ?`,
      ).bind(id, key).first();
      if (!row) return json({ status: "unknown" }, 404);

      return json({ status: row.status, receipt: row.mpesa_receipt,
                    resultDesc: row.result_desc, amount: row.amount });
    }

    // ── POST /webhook/mpesa ───────────────────────────────────────
    // Handles both STK Push callbacks and C2B (Paybill/Till) callbacks.
    // Always returns ResultCode:0 to stop Daraja retries — errors are logged.
    if (url.pathname === "/webhook/mpesa" && request.method === "POST") {
      const body = await request.json().catch(() => null);
      if (!body) return json({ ResultCode: 1, ResultDesc: "Invalid JSON" });

      let txId, phone, amount, email, customerName;

      // ── Detect payload type ───────────────────────────────────
      if (body.Body?.stkCallback) {
        // STK Push callback
        const cb = body.Body.stkCallback;

        // Non-zero ResultCode = user cancelled or insufficient funds — ignore
        if (cb.ResultCode !== 0)
          return json({ ResultCode: 0, ResultDesc: "Payment not completed" });

        txId = cb.CheckoutRequestID;
        const items = cb.CallbackMetadata?.Item ?? [];
        const get   = (name) => items.find((i) => i.Name === name)?.Value;
        phone  = normalisePhone(get("PhoneNumber") ?? "");
        amount = String(get("Amount") ?? "");

      } else if (body.TransID) {
        // C2B Paybill / Till callback
        txId         = body.TransID;
        phone        = normalisePhone(body.MSISDN ?? "");
        amount       = body.TransAmount ?? "";
        customerName = [body.FirstName, body.MiddleName, body.LastName]
                         .filter(Boolean).join(" ").trim() || null;

        // Customer can put their email in the payment reference field
        const ref = (body.BillRefNumber ?? "").trim();
        if (ref.includes("@")) email = ref;

      } else {
        console.warn("Unknown M-Pesa payload structure", JSON.stringify(body));
        return json({ ResultCode: 0, ResultDesc: "Unrecognised payload" });
      }

      if (!txId || !phone) {
        console.error("Missing txId or phone", { txId, phone });
        return json({ ResultCode: 0, ResultDesc: "Missing fields" });
      }

      // ── Idempotency — don't double-assign on duplicate callbacks ─
      const prior = await env.DB.prepare(
        "SELECT key_assigned FROM transactions WHERE transaction_id = ?",
      ).bind(txId).first();

      if (prior) {
        // Already processed — resend the key silently
        await sendSms(env, phone, prior.key_assigned);
        if (email) await sendEmail(env, email, prior.key_assigned, customerName ?? phone);
        return json({ ResultCode: 0, ResultDesc: "Already processed" });
      }

      // ── Assign a key from stock ───────────────────────────────
      const note    = customerName
        ? `${customerName} (${phone}) via M-Pesa ${txId}`
        : `${phone} via M-Pesa ${txId}`;
      // The amount paid picks the plan tier (see tierForAmount).
      const paidTier = tierForAmount(Number(amount) || 0);
      const plainKey = await assignNextKey(env.DB, note, 1, paidTier);

      if (!plainKey) {
        // Out of stock — log loudly; Daraja won't retry (we return 0)
        console.error("OUT OF STOCK: payment received, no keys available", { txId, phone, amount });
        // TODO: alert admin here (email/SMS to owner) when email binding available
        return json({ ResultCode: 0, ResultDesc: "Accepted — stock alert sent to admin" });
      }

      // ── Record transaction ────────────────────────────────────
      await env.DB.prepare(
        `INSERT INTO transactions (transaction_id, phone, amount, key_assigned)
         VALUES (?, ?, ?, ?)`,
      ).bind(txId, phone, amount, plainKey).run();

      // ── Deliver ───────────────────────────────────────────────
      await sendSms(env, phone, plainKey);
      if (email) await sendEmail(env, email, plainKey, customerName ?? phone);

      console.log("Key delivered", { txId, phone, plainKey });
      return json({ ResultCode: 0, ResultDesc: "Success" });
    }

    // ── POST /webhook/stripe ─────────────────────────────────────
    // Handles checkout.session.completed — Stripe always captures email,
    // so key delivery is email-only (no phone number at checkout).
    if (url.pathname === "/webhook/stripe" && request.method === "POST") {
      const rawBody  = await request.text();
      const sigHeader = request.headers.get("stripe-signature") ?? "";

      const valid = await verifyStripeSignature(rawBody, sigHeader, env.STRIPE_WEBHOOK_SECRET);
      if (!valid) return new Response("Unauthorized", { status: 401 });

      const event = JSON.parse(rawBody);

      // Only act on completed checkouts — ignore everything else silently
      if (event.type !== "checkout.session.completed")
        return new Response("ok", { status: 200 });

      const session      = event.data.object;
      const paymentId    = session.id;                              // cs_live_...
      const email        = session.customer_details?.email ?? null;
      const customerName = session.customer_details?.name  ?? email ?? "Customer";

      if (!email) {
        console.error("Stripe session has no email", paymentId);
        return new Response("ok", { status: 200 });
      }

      // Idempotency — Stripe retries on non-2xx, so guard against double delivery
      const prior = await env.DB.prepare(
        "SELECT key_assigned FROM transactions WHERE transaction_id = ?",
      ).bind(paymentId).first();

      if (prior) {
        await sendEmail(env, email, prior.key_assigned, customerName);
        return new Response("ok", { status: 200 });
      }

      // Assign a key from stock
      const amount = session.amount_total
        ? `${(session.amount_total / 100).toFixed(2)} ${(session.currency ?? "usd").toUpperCase()}`
        : "";
      const note     = `${customerName} <${email}> via Stripe ${paymentId}`;
      const plainKey = await assignNextKey(env.DB, note, 1);

      if (!plainKey) {
        console.error("OUT OF STOCK: Stripe payment received, no keys available", { paymentId, email, amount });
        // Send a holding email so the customer isn't left wondering
        if (env.RESEND_API_KEY && env.RESEND_FROM) {
          await sendEmail(env, email, "PENDING — contact support@keynetik.com", customerName);
        }
        return new Response("ok", { status: 200 });
      }

      // Record + deliver
      await env.DB.prepare(
        `INSERT INTO transactions (transaction_id, phone, amount, key_assigned)
         VALUES (?, ?, ?, ?)`,
      ).bind(paymentId, email, amount, plainKey).run();

      await sendEmail(env, email, plainKey, customerName);
      console.log("Stripe key delivered", { paymentId, email, plainKey });

      return new Response("ok", { status: 200 });
    }

    // ── Admin (Bearer ADMIN_TOKEN) ────────────────────────────────
    if (url.pathname.startsWith("/admin/")) {
      const auth     = (request.headers.get("Authorization") || "").trim();
      const expected = `Bearer ${(env.ADMIN_TOKEN || "").trim()}`;
      if (!env.ADMIN_TOKEN || auth !== expected)
        return json({ error: "unauthorized" }, 401);

      // POST /admin/keys — register a pre-minted key
      if (url.pathname === "/admin/keys" && request.method === "POST") {
        const body = await request.json().catch(() => null);
        const key  = norm(body?.key);
        if (key.length !== 16) return json({ error: "bad key" }, 400);

        await env.DB.prepare(
          `INSERT INTO keys (key, max_devices, tier, revoked, expires_at, note)
           VALUES (?, ?, ?, 0, ?, ?)
           ON CONFLICT(key) DO UPDATE SET
             max_devices = excluded.max_devices,
             tier        = excluded.tier,
             expires_at  = excluded.expires_at,
             note        = excluded.note`,
        ).bind(key, body?.max_devices ?? 1, body?.tier ?? 1,
               body?.expires_at ?? null, body?.note ?? null).run();

        return json({ ok: true, key, tier: body?.tier ?? 1 });
      }

      // POST /admin/generate — mint + register in one step
      if (url.pathname === "/admin/generate" && request.method === "POST") {
        const body          = await request.json().catch(() => ({}));
        const { plain, norm: key } = await mintKey();

        await env.DB.prepare(
          `INSERT INTO keys (key, max_devices, tier, revoked, expires_at, note)
           VALUES (?, ?, ?, 0, ?, ?)`,
        ).bind(key, body.max_devices ?? 1, body.tier ?? 1,
               body.expires_at ?? null, body.note ?? null).run();

        return json({ ok: true, key: plain, tier: body.tier ?? 1 });
      }

      // POST /admin/sell — assign next unsold key to a named customer
      if (url.pathname === "/admin/sell" && request.method === "POST") {
        const body = await request.json().catch(() => null);
        if (!body?.customer) return json({ error: "customer field required" }, 400);

        const note     = body.email ? `${body.customer} <${body.email}>` : body.customer;
        const plainKey = await assignNextKey(env.DB, note, body.max_devices ?? 1, body.tier ?? 1);

        if (!plainKey)
          return json({ error: "no_stock", message: "No unsold keys. Use POST /admin/generate." }, 409);

        return json({ ok: true, key: plainKey, customer: note, tier: body.tier ?? 1 });
      }

      // POST /admin/revoke?key=XXXX-...
      if (url.pathname === "/admin/revoke" && request.method === "POST") {
        const key = norm(url.searchParams.get("key"));
        const r   = await env.DB.prepare("UPDATE keys SET revoked = 1 WHERE key = ?").bind(key).run();
        return json({ ok: true, changed: r.meta.changes });
      }

      // GET /admin/list
      if (url.pathname === "/admin/list" && request.method === "GET") {
        const { results } = await env.DB.prepare(
          `SELECT k.key, k.max_devices, k.tier, k.revoked, k.expires_at, k.note,
                  k.created_at, COUNT(a.device_id) AS devices_used
           FROM keys k LEFT JOIN activations a ON a.key = k.key
           GROUP BY k.key ORDER BY k.created_at DESC`,
        ).all();
        return json({ keys: results.map((r) => ({ ...r, key: formatKey(r.key) })) });
      }
    }

    return json({ error: "not_found" }, 404);
  },
};
