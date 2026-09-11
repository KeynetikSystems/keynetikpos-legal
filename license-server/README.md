# KeynetikPOS License Server

A Cloudflare Worker + D1 (SQLite) implementation of the activation/validation
API that the desktop client (`licensemanager.cpp`) already speaks. Runs
entirely on Cloudflare's free tier (100k requests/day — thousands of times the
expected load), with HTTPS included and nothing to patch or monitor.

## API (consumed by the desktop app)

| Endpoint | Called by | Returns |
|---|---|---|
| `GET /activate?key=&device_id=&device_name=` | `LicenseManager::activateOnServer()` when the user enters a CD key | `{"valid":true,"tier":N,"features":[...]?}` or `{"valid":false,"reason":"invalid_key\|key_revoked\|key_expired\|device_limit_reached"}` |
| `GET /validate?key=&device_id=` | `LicenseManager::startOnlineHeartbeat()` ~2 s after every launch | `{"valid":true,"tier":N,"features":[...]?}` or `{"valid":false,"reason":...}` |

The `reason` strings `device_limit_reached`, `key_revoked`, and `key_expired`
are pattern-matched by the client — do not rename them.

**Tier (plan):** every key carries a `tier` (1 POS Core, 2 POS Pro, 3 ERP Lite,
4 ERP Full; default 1). `/activate` and `/validate` return it, the client
persists it, and feature menus are gated on it. Set the tier when minting/
registering (`/admin/keys`, `/admin/generate`, `New-License -Tier`) or selling
(`/admin/sell`); self-serve M-Pesa purchases map the amount paid to a tier via
`tierForAmount()` in `src/index.js` (adjust those thresholds to your pricing).
The optional `features` column is a JSON slug array that overrides the
tier-derived default for a single key. **Existing deployments must apply
`migrations/0001_add_tier.sql` once** (keys default to tier 1 until updated).

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

## Plans & pricing

Tiers are **one-time / perpetual** — the pitch is "pay once" vs. competitors'
monthly SaaS (each tier costs well under a year of the tool it replaces, then
it's free forever).

| Tier | Plan | KES (M-Pesa) | USD (card) | Unlocks |
|---|---|---|---|---|
| 1 | POS Core | (floor) | (floor) | checkout, products, inventory, M-Pesa, receipts |
| 2 | POS Pro | **KSh 8,000** | **$59** | reports, analytics, users, schedules, barcode, messaging |
| 3 | ERP Lite | **KSh 18,000** | **$139** | suppliers, purchase orders, expenses, customers, P&L |
| 4 | ERP Full | **KSh 35,000** | **$269** | general ledger, payroll, VAT |

**USD is the single source of truth**, set in `src/index.js` as
`PRICE_POS_PRO_USD` / `PRICE_ERP_LITE_USD` / `PRICE_ERP_FULL_USD`, read by
`tierForAmountUsd()` for the **Stripe** flow. The KES column — what an
M-Pesa payer actually sends — is **derived** from those USD prices via
`kesFromUsd()` (a static `USD_TO_KES_RATE` constant, rounded to the nearest
KSh 1,000) into `PRICE_POS_PRO` / `PRICE_ERP_LITE` / `PRICE_ERP_FULL`, read
by `tierForAmount()` for the **self-serve M-Pesa** flow (a customer pays the
tier's KES price and the Worker grants that tier; paying between tiers
rounds down; any completed payment grants at least POS Core).

To change a price, edit the USD constant and re-deploy — the KES price moves
with it automatically. `USD_TO_KES_RATE` is a static, periodically
hand-checked constant (like the tier prices themselves) rather than a live
FX lookup: a live rate at payment time could disagree with whatever rate the
website showed the customer when they decided what to send, and a Cloudflare
Worker on the payment-critical path is not the place to add a new external
dependency and failure mode for that. Re-check the rate and re-deploy if it
drifts meaningfully from `USD_TO_KES_RATE`.

Multi-till deals grant more device seats and are sold **manually**
(`New-License -Tier N -MaxDevices M`), not through either self-serve price map.

> The USD prices are a market-positioned starting point, not independently
> researched — validate against current local/international competitor quotes
> (Loyverse add-ons, QuickBooks KE, Odoo/Sage) before publishing. They also
> have no history in this repo before this pass: earlier versions set the KES
> price directly and had no USD price at all. The derived KES amounts happen
> to land on the exact prices this server was already charging before this
> change (at the `USD_TO_KES_RATE` checked 2026-08) — a coincidence of
> reasonable rounding, not by design, and something to re-verify if either
> number moves.

## Update entitlement & renewals

Separate from the perpetual license, a key can carry an optional
`updates_until` date (migration `0002_add_updates_until.sql`). Past that date
the key keeps activating and validating exactly as before — **it never
disables the software** — `/activate` and `/validate` just stop the point
where the client would otherwise learn about a date to show the user. There is
currently no client-side enforcement beyond exposing the date
(`LicenseManager::updatesValidUntil()`); what changes on the business side is
simply whether you ship that customer a new installer.

Suggested renewal pricing (not yet wired into `tierForAmount()` — these are
manual/self-serve-reference renewals, not new-key purchases): roughly a third
of the tier's one-time price per year — **POS Pro** ~KSh 2,800/yr, **ERP
Lite** ~KSh 6,000/yr, **ERP Full** ~KSh 12,000/yr. POS Core has no renewal;
it's free and updated forever. ERP Full is the strongest case for actually
renewing, not just the priciest: payroll and VAT are compliance features tied
to statutory rates that change with each Finance Act
(`docs/STATUTORY_RATES.md`), so a stale ERP Full install risks miscalculating
PAYE or VAT, not just missing new features.

Two ways to record a renewal payment:

- **Admin (any payment channel — bank transfer, cash, Stripe, manual M-Pesa):**
  `POST /admin/renew` with `{ "key": "...", "months": 12 }` (`months` optional,
  defaults to 12). Extends from the *later* of today and the key's current
  `updates_until`, so renewing early never wastes the remainder already paid
  for.
- **Self-serve M-Pesa:** a C2B/Paybill payment whose reference (`BillRefNumber`)
  is an existing, unrevoked license key — instead of an email or blank — is
  treated as a renewal for that key rather than a new-key sale (see
  `/webhook/mpesa` in `src/index.js`). The reference must pass the same
  checksum the client validates keys with, so a garbled or unrelated reference
  can't accidentally get treated as one. A confirmation SMS with the new
  `updates_until` date is sent back to the payer.

**Not yet built:** a Stripe equivalent of the M-Pesa path above (would need a
separate Stripe Payment Link with a custom field for the existing key, since
`/webhook/stripe` currently has no way to distinguish a renewal from a new
purchase), and any in-app surface for `updatesValidUntil()` — it's there to
read, but no dialog shows it yet.

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

Renew a key's update entitlement (payment received outside the M-Pesa
self-serve path — see "Update entitlement & renewals" above):

```powershell
curl -X POST "https://<your-worker>/admin/renew" `
  -H "Authorization: Bearer <ADMIN_TOKEN>" `
  -H "Content-Type: application/json" `
  -d '{"key":"ABCD-1F2E-WXYZ-9876","months":12}'
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
#                                       MPESA_TILL_NUMBER=<till>  (buygoods only)
wrangler deploy
```

**Paybill vs Buy Goods (till).** Daraja treats these differently and the
difference is easy to get wrong:

| | `BusinessShortCode` | `PartyB` |
|---|---|---|
| Paybill  | Paybill number | same number |
| Buy Goods | Head Office / store number | the **till** number |

`BusinessShortCode` is also what the `Password` hash is built from, so it must
be the number your passkey was issued against — for Buy Goods that is the HO
number, never the till. Set `MPESA_TILL_NUMBER` to the till; leaving it unset
falls back to `MPESA_SHORTCODE`, which is right for Paybill and wrong for a
real till. Sandbox does not enforce the pairing, so a Buy Goods config can
return `ResponseCode: 0` in sandbox and still fail in production — verify with
a small live transaction at go-live.

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
