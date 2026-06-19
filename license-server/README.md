# KeynetikPOS License Server

A Cloudflare Worker + D1 (SQLite) implementation of the activation/validation
API that the desktop client (`licensemanager.cpp`) already speaks. Runs
entirely on Cloudflare's free tier (100k requests/day — thousands of times the
expected load), with HTTPS included and nothing to patch or monitor.

## API (consumed by the desktop app)

| Endpoint | Called by | Returns |
|---|---|---|
| `GET /activate?key=&device_id=&device_name=` | `LicenseManager::activateOnServer()` when the user enters a CD key | `{"valid":true}` or `{"valid":false,"reason":"invalid_key\|key_revoked\|key_expired\|device_limit_reached"}` |
| `GET /validate?key=&device_id=` | `LicenseManager::startOnlineHeartbeat()` ~2 s after every launch | `{"valid":true}` or `{"valid":false,"reason":...}` |

The `reason` strings `device_limit_reached`, `key_revoked`, and `key_expired`
are pattern-matched by the client — do not rename them.

Client behaviour to keep in mind:

- A `valid:false` heartbeat sets the local **Revoked** flag, enforced at the
  *next launch* (no mid-session lockout). A `valid:true` heartbeat clears it
  and resets the 7-day offline grace window.
- If the server is unreachable at activation time, the client falls back to
  **local activation** (format check only) so offline shops are never locked
  out. Those installs have `ActivationSource=local` and are never heartbeat-
  checked — revocation cannot reach them. That is a deliberate trade-off.
- Deleting a row from `activations` frees that seat; that device's next
  heartbeat returns `not_activated`, which un-licenses it at next launch.

## Deploy (one time, ~10 minutes)

```powershell
cd license-server
npm install -g wrangler          # or use npx wrangler ...
wrangler login                   # opens browser, free Cloudflare account

wrangler d1 create keynetik-license
#  -> copy the printed database_id into wrangler.toml

wrangler d1 execute keynetik-license --remote --file=schema.sql

wrangler secret put ADMIN_TOKEN  # paste a long random string; this protects /admin/*

wrangler deploy
#  -> prints your URL, e.g. https://keynetik-license.<account>.workers.dev
```

Then point the app at it: in `licensemanager.cpp`, set

```cpp
static const QString SERVER_URL = "https://keynetik-license.<account>.workers.dev";
```

(no port, no trailing slash) and rebuild.

## Day-to-day key management

Mint keys (checksum format the client accepts) with the repo tool:

```powershell
..\tools\generate-keys.ps1 -Count 5
```

Register a key on the server (do this when you sell one):

```powershell
curl -X POST "https://<your-worker>/admin/keys" `
  -H "Authorization: Bearer <ADMIN_TOKEN>" `
  -H "Content-Type: application/json" `
  -d '{"key":"ABCD-1F2E-WXYZ-9876","max_devices":2,"note":"Mama Njeri Shop, Nakuru"}'
```

Revoke a key (refund/chargeback):

```powershell
curl -X POST "https://<your-worker>/admin/revoke?key=ABCD-1F2E-WXYZ-9876" `
  -H "Authorization: Bearer <ADMIN_TOKEN>"
```

List keys and seat usage:

```powershell
curl "https://<your-worker>/admin/list" -H "Authorization: Bearer <ADMIN_TOKEN>"
```

Free a seat (customer replaced a till):

```powershell
wrangler d1 execute keynetik-license --remote `
  --command "DELETE FROM activations WHERE key='ABCD1F2EWXYZ9876' AND device_name='OLD-TILL-PC'"
```

(Keys are stored normalized: uppercase, dashes stripped.)

## In-store M-Pesa STK Push (POS checkout)

The Worker can trigger a Daraja STK Push so a till can charge a customer at
checkout and auto-confirm — the till never holds Daraja credentials.

| Endpoint | Called by | Notes |
|---|---|---|
| `POST /pos/mpesa/stkpush` | the till (`MpesaClient`) | auth `X-License-Key` + `X-Device-Id`; body `{phone, amount, accountRef}`; returns `{ok,checkoutId}` |
| `GET /pos/mpesa/status?checkout_id=` | the till, polling | returns `{status:"pending\|success\|failed", receipt}` |
| `POST /pos/mpesa/callback` | Safaricom (Daraja) | set this as the STK CallBackURL; updates the request, never assigns keys |

Set up (one time):

```powershell
cd license-server
wrangler d1 execute keynetik-license --remote --file=schema.sql   # adds stk_requests

wrangler secret put MPESA_CONSUMER_KEY
wrangler secret put MPESA_CONSUMER_SECRET
wrangler secret put MPESA_SHORTCODE       # sandbox: 174379
wrangler secret put MPESA_PASSKEY
# vars (wrangler.toml [vars] or --var): MPESA_ENV=sandbox|production,
#                                       MPESA_TXN_TYPE=paybill|buygoods
wrangler deploy
```

In the Daraja portal, set the app's STK callback URL to
`https://<your-worker>/pos/mpesa/callback`. A till only gets STK if it's
licensed (activated key); otherwise the cashier enters the M-Pesa code manually.

> **Note:** M-Pesa charges whole shillings, so the STK amount is the cart total
> rounded to the nearest shilling. **The `/pos/mpesa/callback` is unauthenticated**
> (Safaricom can't sign callbacks). It only flips a server-generated, unguessable
> `CheckoutRequestID` to success/failed, but for production consider restricting
> it to Safaricom's published IP ranges.

## Notes

- A key must exist in the `keys` table before `/activate` accepts it — minting
  a key with the PowerShell tool does **not** register it on the server.
- The client sends the key and machine id as URL query parameters over HTTPS.
  Cloudflare does not expose query strings in your Worker logs by default;
  avoid `console.log(request.url)` in any future edits.
- Local dev: `wrangler dev --remote` serves the Worker locally against the
  real D1 database.
