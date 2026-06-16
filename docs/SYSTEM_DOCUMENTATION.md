# KeynetikPOS — System Documentation

**Product:** KeynetikPOS — Next-GEN Professional Point of Sale System
**Version:** 2.0.0
**Publisher:** Keynetik Solutions
**Platform:** Windows desktop (Qt 6 / C++17)
**Audience:** Installers, system administrators, and IT support staff.

> This document describes the system as built and deployed: what it requires, how
> it is installed and configured, where its data lives, how licensing works, and
> how to operate, back up, and troubleshoot it. For a code-level walkthrough of
> every class, see [`CODE_DOCUMENTATION.md`](../CODE_DOCUMENTATION.md). For day-to-day
> cashier and manager usage, see [`USER_GUIDE.md`](USER_GUIDE.md).

---

## 1. System overview

KeynetikPOS is a single-machine, offline-first point-of-sale application aimed at
small and mid-size retailers (built with the Kenyan market in mind — KSh currency,
16% VAT context, M-Pesa mobile money, WhatsApp/SMS reporting). All business data
lives in a local SQLite database on the terminal; the only external dependencies
are optional: an online **license server** (for activation/heartbeat) and optional
**messaging gateways** (Twilio WhatsApp, Africa's Talking SMS) for scheduled
reports.

The application is self-contained: it bootstraps its own database schema on first
run, seeds a default administrator account, and requires no separate database
server, web server, or runtime install beyond the bundled Qt DLLs.

### Capability summary

| Area | Capabilities |
|---|---|
| Sales | Product grid + search, barcode scanning, multiple simultaneous carts, discounts (PIN-gated), payments (cash/card/mobile/split), receipt printing/PDF |
| Inventory | Product catalog, stock levels, low/critical-stock alerts, restock, CSV import/export, audit-logged adjustments |
| Users | Role-based access control (Admin/Manager/Cashier/Viewer), PBKDF2-hashed passwords, forced password change, user action audit log |
| Reporting | On-screen analytics dashboards, operational reports (CSV/PDF), sales history, refunds, end-of-day Z-Reports |
| Shifts | Open/close shifts with counted cash floats, over/short reconciliation |
| Messaging | Scheduled Z-Report/inventory/sales reports via WhatsApp or SMS |
| Licensing | 30-day trial, CD-key activation (online + offline fallback), device limits, revocation, tamper detection |

---

## 2. System requirements

### Hardware (minimum / recommended)

| Item | Minimum | Recommended |
|---|---|---|
| CPU | Dual-core x86-64 | Quad-core |
| RAM | 2 GB | 4 GB+ |
| Disk | 300 MB free (app + Qt DLLs) plus DB growth | 1 GB+ free |
| Display | 1024 × 640 (enforced minimum window size) | 1366 × 768 or larger |

### Software

- **Operating system:** Windows 10 or Windows 11 (64-bit). Developed and verified
  on Windows 11.
- **Runtime:** None to install separately — the installer bundles all required Qt
  6 runtime DLLs via `windeployqt`. No system-wide Qt installation is needed on
  the target machine.

### Optional peripherals

- **Barcode scanner** — either USB keyboard-wedge (HID, no driver) or serial /
  virtual-COM (RS-232). Both are supported; see §7.
- **Printer** — any Windows printer for receipts. The system can also export
  receipts as PDF with no printer at all.

### Network

- **Not required for normal operation.** The POS runs fully offline.
- Internet is used only for (a) license activation/heartbeat and (b) sending
  scheduled WhatsApp/SMS reports, if configured. Offline terminals fall back to
  local license validation and simply skip report sending.

---

## 3. Architecture

### 3.1 Process & startup sequence

KeynetikPOS is a single-process desktop application. `main()` runs a series of
sequential **gates** at startup, each protecting the next:

```
Anti-tamper check ─► License validation ─► Database init ─► Login (max 3 tries)
   ─► Forced password change (if required) ─► MainWindow ─► (deferred) license heartbeat
```

- **Anti-tamper** — a light, debugger-only check on release builds; shows a
  generic error and exits if a debugger is attached.
- **License validation** — strictly offline and non-blocking at startup
  (see §6). Trial/expired/invalid states prompt for a CD key.
- **Database init** — opens (or creates) the SQLite database, builds the schema,
  seeds sample products and the default admin on a fresh install.
- **Login** — capped at 3 attempts per session; a `must_change_password` account
  is forced through a password change before the main window opens.
- **Deferred heartbeat** — the online license revalidation runs ~2 seconds *after*
  the UI is shown, so a slow or absent network never freezes startup.

Logout does **not** restart the process — it closes the main window and loops back
to the login dialog within the same process.

### 3.2 Layered design

```
                    main.cpp (startup gates)
                            │
        ┌───────────────────┼─────────────────────────┐
   LicenseManager      UserManager                Database (SQLite)
   (HKCU registry)    (auth/session)            (pos_database.db)
                            │
                       MainWindow ── owns ──► InventoryManager, ReceiptPrinter,
                            │                  ScheduleManager, SettingsManager,
                            │                  BarcodeReader
                            │
   ┌────────────┬──────────┼──────────┬───────────────┬──────────────┐
 Sales/      Inventory   Analytics  Reports/        Shifts &       Scheduled
 Checkout    dialogs     dashboards  SalesHistory    Z-Reports      messaging
                                                                      │
                                                          MessageProvider (abstract)
                                                          ├── WhatsAppManager (Twilio)
                                                          └── AfricasTalkingProvider (SMS)
```

**Design conventions:**

- **Singletons** for process-wide services: `Database`, `UserManager`,
  `LicenseManager`, `ThemeManager`. One instance, one source of truth.
- **Managers** (`QObject` subclasses) hold business logic + persistence;
  **Dialogs** (`QDialog` subclasses) hold UI only and delegate to managers.
- **Self-bootstrapping schema** — each manager creates its own tables on
  construction (`CREATE TABLE IF NOT EXISTS`) and adds missing columns via a
  lightweight `ensureColumn()` migration. No separate migration tool is needed.
- **Parameterized SQL everywhere** — all queries use prepared statements with
  bound values (SQL-injection safe).
- The application is **single-threaded** for DB access; SQLite runs in WAL mode
  with a busy timeout for resilience.

### 3.3 Technology stack

| Layer | Technology |
|---|---|
| Language | C++17 |
| UI / framework | Qt 6 (Widgets, Core, Sql, PrintSupport, Network, SerialPort) |
| Database | SQLite (via Qt SQL, WAL journal mode) |
| Build | CMake ≥ 3.16, MinGW or MSVC toolchain |
| Packaging | CPack + NSIS (Windows installer), `windeployqt` for runtime DLLs |
| License server | Cloudflare Worker + D1 (separate, in `license-server/`) |

---

## 4. Data model & storage

### 4.1 Where data lives

| Data | Location |
|---|---|
| Business database | `pos_database.db` in the per-user AppData directory (`QStandardPaths::AppDataLocation`), falling back to the executable directory if AppData is unavailable |
| License state | Windows registry, `HKCU\Software\KeynetikSolutions\KeynetikPOS\License` (and a machine-bound "breadcrumb" file in the user config dir) |
| Theme choice & receipt/printer config & messaging org keys | `QSettings` under org `KeynetikPOS` |
| Generated receipts (PDF) | `Receipt_<saleId>_<timestamp>.pdf` (current build always emits PDF) |

> **Important:** the database is stored **per Windows user** under AppData, *not*
> in `Program Files` (which is read-only for non-admin users). If multiple Windows
> accounts use the same terminal, each gets its own database unless you relocate it
> (see §9 Backup).

### 4.2 Core tables (created automatically)

| Table | Purpose |
|---|---|
| `products` | Catalog: name, category, price, cost_price, profit_margin, stock_quantity, barcode, is_active |
| `sales` | Transaction headers: total, tax, discount, payment_method, amount_paid, change_due, sale_date |
| `sale_items` | Line items with **snapshots** of name/price/cost at sale time |
| `users` | Accounts: username, password_hash (PBKDF2), full_name, email, role, is_active, must_change_password |
| `stock_adjustments` | Audit log of manual stock changes (reason, who, when) |
| `refunds` | Refund records (links to original sale) |
| `AppSettings` | Key/value business settings (currency, tax config, hashed discount PIN, etc.) |
| `Shifts` | Shift records: cashier, opening/closing float, running totals, open flag |
| `payments` | Per-tender payment records (supports split tender) |
| `message_schedules` | Scheduled report definitions |
| Discount/user audit logs | Discount applications and user-action audit trail |

Key design choices: sale items **snapshot** product name/price/cost (so historical
reports and profit math stay correct even after products are renamed, repriced, or
deleted); money is stored as `REAL` (double) by current design — see §11 Known
limitations.

---

## 5. Installation & deployment

### 5.1 Installing on a terminal (end deployment)

The standard deliverable is a Windows installer produced by NSIS:
`KeynetikPOS-2.0.0-Setup.exe`.

1. Run the installer (admin rights recommended so shortcuts are created
   system-wide).
2. It installs the executable plus all bundled Qt runtime DLLs, creates
   Start-menu and desktop shortcuts, and can launch the app on finish.
3. On first launch the app validates its license (or starts a 30-day trial),
   builds its database, and creates the default admin account (see §8).

No separate Qt install, database server, or runtime is required on the terminal.

### 5.2 Building from source

**Prerequisites:** Qt 6.11 (MinGW or MSVC), CMake ≥ 3.16, a C++17 compiler.

Verified Release build command (MinGW, PowerShell):

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.0\mingw_64\bin;" + $env:PATH
cmake --build "build\Desktop_Qt_6_11_0_MinGW_64_bit-Release"
```

`CMakeUserPresets.json` also defines `Qt-Debug` / `Qt-Release` presets that output
to `out/`. An MSVC2022 Debug configuration exists under `build/`.

**Required Qt components:** Widgets, Core, Sql, PrintSupport, Network, SerialPort
(and Test, for the unit tests).

### 5.3 Packaging the installer

The build is packaging-aware: when `LICENSE.txt` exists, CPack is configured to
produce an NSIS installer with icon, shortcuts, and launch-after-install. The
`windeployqt` post-build step stages the executable with all its runtime DLLs into
`windeployqt_stage/`, which the installer then bundles.

```powershell
# from the build directory, after a successful build
cpack -G NSIS
```

### 5.4 Tests

The project ships a QtTest unit-test target (`carttotals_test`) covering the pure
cart-totals math (tax, discount, rounding — 15 cases).

```powershell
# build & run
cmake --build <build-dir> --target carttotals_test
& "<build-dir>\carttotals_test.exe" -o results.txt,txt   # pipe swallows QtTest stdout
ctest --test-dir <build-dir>                              # or via ctest
```

Tests are enabled by default (`-DKEYNETIK_BUILD_TESTS=ON`) and link only the
dependency-free totals code, so they stay fast.

---

## 6. Licensing system

KeynetikPOS uses an **offline-first** licensing model so a shop with no internet
is never locked out, while still enforcing device limits and revocation when
online.

### 6.1 License states & flow

| State | Meaning |
|---|---|
| `Trial` | 30-day evaluation (`TRIAL_DAYS = 30`). Nags at first run and when ≤ 7 days remain. |
| `FullLicense` | Activated with a valid CD key. |
| `TrialExpired` | Trial used up — prompts for a CD key. |
| `InvalidKey` | Stored license invalid or revoked — prompts for a CD key. |

**Activation** is online-first: `activateOnServer()` contacts the license server,
which enforces device limits and revocation. If the server is unreachable, the app
falls back to local format validation (`activateKey()`) so offline shops can still
activate. Locally-activated copies are never heartbeat-checked.

**Heartbeat** (`startOnlineHeartbeat()`) runs ~2 seconds after the UI is up,
asynchronously revalidates server-activated keys, and tracks consecutive offline
days against a 7-day grace period (`MAX_OFFLINE_DAYS`).

### 6.2 Anti-tamper

- License state is stored in `HKCU` with a salted SHA-256 hash over the covered
  values; hand-editing the registry trips `wasTampered()` and blocks startup.
- A second machine-bound signed copy of the install date (a hidden "breadcrumb"
  file) prevents trial resets by wiping the registry alone — startup takes the
  *earliest* of the two dates.

> **Operational caution:** changing `TAMPER_SALT` or the registry-hash formula
> after shipping will brick legitimately installed copies (the stored hash no
> longer matches) and requires a migration. A local tamper lockout can be cleared
> by deleting `HKCU\Software\KeynetikSolutions\KeynetikPOS\License` (this resets
> activation, not the trial breadcrumb).

### 6.3 License server (deployed)

The server is a Cloudflare Worker backed by a D1 database (code in
`license-server/`, free tier).

- **Live endpoint:** `https://keynetik-license.kelvinyabate.workers.dev`
  (referenced by `SERVER_URL` in `licensemanager.cpp`).
- **Admin operations** (mint/revoke keys, list devices) require a bearer
  `ADMIN_TOKEN`, stored as a Cloudflare Worker secret. The admin script is
  `license-server/admin.ps1`; the schema is `license-server/schema.sql`.
- **Key minting:** `tools/generate-keys.ps1`. Keys are `XXXX-XXXX-XXXX-XXXX`
  with a checksum segment (first 4 hex of `SHA256(seg0 + seg2 + "KNK-SALT-2025")`).
- **Server reasons** returned to the client: `device_limit_reached`,
  `key_revoked`, `key_expired`.

> Wrangler (the Cloudflare CLI) requires Node 22+. The `ADMIN_TOKEN` must be set
> with **no trailing newline** (a shell-pipe newline previously caused a 401; the
> server trims it defensively).

---

## 7. Peripheral configuration

### 7.1 Barcode scanners

`BarcodeReader` supports both common scanner types behind one signal
(`barcodeScanned`):

- **Keyboard-wedge (USB HID)** — the scanner "types" the code + Enter. Installed
  as an application-wide Qt event filter; an inter-character timer (~100 ms)
  distinguishes a fast scan burst from human typing. No driver, no configuration
  for most scanners.
- **Serial (RS-232 / virtual COM)** — default 9600-8-N-1; available COM ports are
  enumerated for the settings UI. Configurable prefix/suffix stripping removes
  STX/ETX framing bytes some scanners add.

The app can also **generate and print EAN-13 labels** (with valid check digits)
for unlabelled stock, plus Code128/QR strings — no GS1 subscription required.

### 7.2 Receipts & printing

`ReceiptPrinter` builds receipts two ways: fixed-width text for 40-column thermal
printers, and styled HTML rendered via `QPrinter` for standard printers and PDF
export. **In the current build, receipts are emitted as PDF by default** (named
`Receipt_<saleId>_<timestamp>.pdf`), which makes the system usable with zero
printer setup. Company header/footer and printer config persist in `QSettings`.

---

## 8. Administration

### 8.1 First-run default account

On a fresh database the system creates **one** administrator:

| Field | Value |
|---|---|
| Username | `admin` |
| Password | `admin123` |
| Role | Admin |
| Flag | `must_change_password` = true (a new password is required at first login) |

> **Change this immediately.** The default password is published in the source and
> logs. The first login forces a password change; do not skip or share it.

### 8.2 Roles & permissions (RBAC)

Four hierarchical roles. The authoritative role→permission table lives in
`RoleManager`; the UI disables anything the current user may not do, and managers
re-check permissions server-side.

| Permission | Admin | Manager | Cashier | Viewer |
|---|:---:|:---:|:---:|:---:|
| Make / view sales | ✓ | ✓ | ✓ | view only |
| Apply discounts | ✓ | ✓ | ✓ | — |
| Void / delete transactions | ✓ | void only | — | — |
| View inventory | ✓ | ✓ | ✓ | ✓ |
| Add / edit products, adjust stock | ✓ | ✓ | — | — |
| Delete products | ✓ | — | — | — |
| View reports / analytics | ✓ | ✓ | — | ✓ |
| Export reports | ✓ | ✓ | — | — |
| Manage users | ✓ | — | — | — |
| View users | ✓ | ✓ | — | — |
| Access settings, backup/restore, view logs | ✓ | — | — | — |

(Admin has every permission. Cashier is the till operator; Viewer is read-only
oversight.)

### 8.3 Password & account policy

- Passwords are hashed with **salted PBKDF2-SHA256** (100,000 iterations,
  16-byte OS-random salt). Legacy unsalted SHA-256 hashes are transparently
  rehashed to PBKDF2 on the next successful login.
- Minimum password length is 8 characters; new passwords must differ from the old.
- Admin-set passwords are temporary (`must_change_password`) — the user must pick
  their own at next login.
- Every login, password change, and significant user action is written to an
  audit log.

### 8.4 Business settings

The Settings dialog (Admin only) configures: business identity, currency symbol
(default `KSh`) and code (`KES`), receipt footer, tax (enabled/rate/label, with
**tax-inclusive pricing** support), receipt printing toggle, the discount PIN
(PBKDF2-hashed), and messaging credentials (Twilio WhatsApp SID/token/number;
Africa's Talking API key/username/sender ID). Settings live in the **database**
(`AppSettings`) so they travel with a backup; messaging org keys also persist in
`QSettings`. Saving emits a live-refresh so open windows update without a restart.

### 8.5 Scheduled messaging

`ScheduleManager` polls once a minute and fires due schedules (Daily/Weekly/
Monthly/OnShiftClose/OnSalesThreshold). Reports (Z-Report, inventory alerts, sales
summaries) are generated and dispatched through the configured provider. Schedules
and their `lastSent` timestamps persist, so they survive restarts without
re-sending. "Send Now" lets an admin test credentials immediately.

---

## 9. Backup & recovery

The entire business state is in **one file**: `pos_database.db` (plus the
WAL/SHM sidecar files SQLite may create alongside it).

### 9.1 Backup procedure

1. Close KeynetikPOS (ensures WAL is checkpointed and no write is mid-flight).
2. Locate the database in the per-user AppData directory (the app logs its full
   path on startup in debug builds; it is under `…\AppData\…\KeynetikPOS\`).
3. Copy `pos_database.db` (and any `pos_database.db-wal` / `-shm` files if present)
   to a safe location — external drive, network share, or cloud folder.

Because all business settings live inside the database, this single copy captures
products, sales, users, settings, shifts, and schedules.

### 9.2 Restore

Close the app, replace `pos_database.db` in the AppData directory with the backup,
and relaunch. Do not mix a database with mismatched WAL sidecar files — copy them
together or checkpoint first by opening/closing cleanly.

### 9.3 What is *not* in the database

- **License state** (registry + breadcrumb) — re-activation may be needed on a new
  machine, subject to the key's device limit.
- **Theme choice and printer/messaging org keys** stored in `QSettings`.

> A scheduled OS-level backup of the AppData folder is the simplest robust policy.

---

## 10. Security considerations

- **Authentication:** PBKDF2-hashed passwords with constant-time comparison;
  login attempts capped at 3 per session.
- **Authorization:** RBAC enforced both in the UI (disabling controls) and in the
  managers (permission re-checks). Discounts and refunds are PIN/permission-gated
  and audit-logged to deter "sweethearting" and false refunds.
- **SQL injection:** prevented via prepared statements throughout.
- **Audit trails:** user actions, stock adjustments, discounts, and refunds are
  all logged with who/when for cash-handling accountability.
- **Anti-tamper / licensing:** see §6. Note the in-app license checks are an
  obfuscation/format gate — **the license server is the real authority**; the
  client-side `TAMPER_SALT` and hash are deterrents, not cryptographic security.
- **Data at rest:** the SQLite database is **not encrypted**. Physical access to
  the terminal grants access to the data file — rely on Windows account
  permissions and disk encryption (BitLocker) for confidentiality.

---

## 11. Known limitations & technical debt

These are deliberate trade-offs, documented for maintainers:

- **Money stored as `double`/`REAL`**, not integer cents. The pure totals math is
  unit-tested and "contained" (a characterization test pins the rounding artifact,
  e.g. `roundCents(1.005) → 1.00`), but a full integer-cents migration is
  deferred. Treat very large transactions / extreme volumes with this in mind.
- **Single-machine, single-process, single-threaded** — no multi-terminal
  networking or concurrent DB writers. SQLite WAL + busy-timeout cover crash
  resilience, not multi-user concurrency.
- **Overlapping UIs kept in the build** — three analytics screens
  (`AnalyticsDialog`, `AnalyticsDashboard`, `EnhancedAnalyticsDialog`) and two
  inventory screens (`InventoryDialog`, `StockManager`) overlap in scope;
  consolidation is a future cleanup.
- **Inventory reorder levels** are not persisted per-product in the database.
- **Email receipts** are a stub; WhatsApp/SMS are the live channels.
- **No built-in automated backup** — backups are a manual/OS-level procedure (§9).

---

## 12. Troubleshooting

| Symptom | Likely cause | Resolution |
|---|---|---|
| "License data has been modified…" at startup | Registry tamper hash mismatch (often after a salt/formula change or manual registry edit) | Delete `HKCU\Software\KeynetikSolutions\KeynetikPOS\License` and re-activate |
| "Error code: 0x00000005" and exit | Anti-tamper tripped (debugger attached) on a release build | Close debuggers/diagnostic tools and relaunch |
| Trial says expired sooner than expected / won't reset | Breadcrumb file holds the earliest install date (by design) | Trial resets are intentionally prevented; obtain a CD key |
| Activation fails offline with "device limit" | App reached the server, key is at its device cap | Free a device via admin (`admin.ps1`) or use another key |
| Scheduled reports never send | Missing/incorrect messaging credentials, or terminal offline at fire time | Use "Send Now" to test; verify Twilio/Africa's Talking keys in Settings |
| Barcode scans go into the wrong field / nothing happens | Wedge scanner timing, or wrong serial port | Confirm scanner mode; for serial, pick the correct COM port; check prefix/suffix settings |
| Forgot the admin password | No self-service reset for the last admin | Restore from a backup taken before the change, or (last resort) inspect/reset via direct DB access |
| Database "failed to initialize" | AppData not writable / disk full / locked DB file | Check disk space and permissions; ensure no second instance holds the file |
| Receipts not printing | PDF-by-default in current build | Find the generated `Receipt_*.pdf`; configure a real printer if hardware printing is needed |

**Admin token storage:** the deployed `ADMIN_TOKEN` was written to
`%TEMP%\knk_admin_token.txt` on the deployer's machine — move it to a password
manager and delete the temp copy.

---

## 13. Reference

- **Code-level documentation:** [`CODE_DOCUMENTATION.md`](../CODE_DOCUMENTATION.md)
- **End-user manual:** [`USER_GUIDE.md`](USER_GUIDE.md)
- **Engineering / feature PDFs:** `KeynetikPOS_Engineering_Documentation.pdf`,
  `KeynetikPOS_Feature_Documentation.pdf` (this folder)
- **License server:** `../license-server/README.md`
- **Support contact:** support@keynetik.com

---

*This document reflects KeynetikPOS v2.0.0 as built. Verify version-specific
details (default credentials, server URLs, key salts) against the current source
before relying on them in production.*
