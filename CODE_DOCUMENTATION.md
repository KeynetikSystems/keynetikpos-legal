# KeynetikPOS — Code Documentation

KeynetikPOS is a Qt 6 (C++17) desktop Point-of-Sale application for Windows, built
with CMake and backed by a local SQLite database. This document describes every
source file and the classes it contains: **what** it does, **how** it does it, and
**why** it is designed that way.

Architecture at a glance:

```
main.cpp ─► AntiDebug ─► LicenseManager ─► Database (owned here) ─► LoginDialog/UserManager ─► MainWindow(Database&)
                                               │                                                     │
                          delegates to         ▼                  ┌──────────┬──────────┬───────────┼───────────┬──────────┐
                    ┌── SaleRepository          repositories       ▼          ▼          ▼           ▼           ▼          ▼
                    ├── ProductRepository                   InventoryManager ReceiptPrinter ScheduleManager SettingsManager BarcodeReader Dialogs
                    ├── CustomerRepository                                          │             (each receives the Database&)
                    ├── SupplierRepository                              MessageProvider (abstract)
                    ├── ExpenseRepository                               ├── WhatsAppManager (Twilio)
                    ├── PurchaseOrderRepository                         └── AfricasTalkingProvider (SMS)
                    ├── RefundRepository
                    └── SalesAnalyticsRepository

Accounting/ERP layer (Tier 4): Ledger (double-entry GL) ◄─ SaleJournal builds balanced
entries; Payroll, Vat, Etims post into the same QSqlDatabase connection.
```

`main()` owns the one `Database`; it is injected by reference down the tree. The
data API delegates to the repositories on the left; the accounting modules
(`Ledger`/`Payroll`/`Vat`) take the shared `QSqlDatabase` directly.

Conventions used throughout the codebase:

- **Dependency injection for the database.** `Database` is *not* a singleton: a
  single instance is created in `main()`, owned for the life of the process, and
  passed by reference (`Database&`) into `MainWindow`, which forwards it to every
  dialog/manager/service it constructs. (`UserManager`, `LicenseManager`, and
  `ThemeManager` remain singletons.)
- **Repository layer.** `Database` itself holds no business SQL — it is a thin
  facade over per-aggregate repositories (`SaleRepository`,
  `ProductRepository`, `CustomerRepository`, `SupplierRepository`,
  `ExpenseRepository`, `PurchaseOrderRepository`, `RefundRepository`,
  `SalesAnalyticsRepository`). Each takes a `QSqlDatabase` by value (like
  `Ledger`) and owns the queries for one slice. See §2.
- **Manager classes** (`QObject` subclasses) hold business logic and persistence;
  **Dialog classes** (`QDialog` subclasses) hold UI only and delegate to managers.
- **Money is integer cents** everywhere via the `Money` value type (`money.h`),
  never floating point — see §2.
- **Versioned schema migrations.** The schema is self-bootstrapping
  (`CREATE TABLE IF NOT EXISTS`) and upgraded in place by a numbered migration
  runner keyed on a `schema_meta` version row — no separate migration tool is
  needed for a single-machine POS install.
- All SQL uses prepared statements with bound values to prevent SQL injection.

---

## 1. Application entry & protection

### main.cpp

**What:** The application entry point. Orchestrates the startup sequence:
anti-debug check → license validation → database initialization → login (max 3
attempts) → forced password change if required → show `MainWindow` → deferred
online license heartbeat → logout on exit.

**How:** A plain `main()` that runs each gate sequentially and returns a non-zero
exit code if any gate fails. The static helper `forcePasswordChange()` loops a
`QInputDialog` until the user supplies a valid new password (≥ 8 chars, different
from the old one, confirmed twice) or cancels. The online license revalidation is
deferred with `QTimer::singleShot(2000, …)` so it runs *after* the event loop is
live.

**Why:** Each gate protects the next: there is no point opening the database for an
unlicensed copy, and no point showing the POS to an unauthenticated user. The
license heartbeat is deferred because a blocking network call at startup would
freeze the UI on slow or offline networks — a real risk for shops with poor
connectivity. Login attempts are capped at 3 to slow down password guessing on a
shared terminal.

### antidebug.h — `AntiDebug`

**What:** Minimal anti-tamper check. `isThreatDetected()` returns `true` if a
debugger is attached to the process.

**How:** Header-only static class. On Windows *release* builds only
(`Q_OS_WIN && NDEBUG`) it calls the Win32 APIs `IsDebuggerPresent()` and
`CheckRemoteDebuggerPresent()`. In debug builds and on other platforms it always
returns `false`. When triggered, `main()` shows a deliberately generic error
("Error code: 0x00000005") instead of explaining the real reason.

**Why:** A light deterrent against casual cracking of the licensing system. The
file's own comment explains the design decision: earlier timing heuristics, VM
detection, and process scanning were removed because they locked out legitimate
users (slow machines, virtualized POS terminals, IT diagnostic tools) far more
often than they stopped crackers. The vague error message avoids telling an
attacker which check fired.

### licensemanager.h / licensemanager.cpp — `LicenseManager`

**What:** Singleton that enforces the licensing model: a 30-day trial
(`TRIAL_DAYS`), CD-key activation (local or server-backed), a 7-day offline grace
period for activated copies (`MAX_OFFLINE_DAYS`), revocation, and tamper
detection. Exposes `LicenseState` (FullLicense / Trial / TrialExpired /
InvalidKey) plus an `ActivationResult` enum for server activation outcomes.

**How:**
- **Storage:** all license state (CD key, install date, activation source,
  revoked flag, last online check, offline-day counter) lives in the Windows
  registry under HKCU via `QSettings::NativeFormat`.
- **Tamper detection:** a salted SHA-256 hash (`TAMPER_SALT`) is computed over the
  key + install date + activation source + revoked flag and stored alongside
  them. Every write of a covered value rewrites the hash; `verifyRegistryHash()`
  detects hand-edited registry values and flips `wasTampered()`.
- **Trial-reset defence:** a second, machine-bound signed copy of the install
  date (the "breadcrumb") is written to a hidden file under the user's config
  directory. `initialize()` takes the *earliest* of the registry and breadcrumb
  dates, so wiping HKCU alone cannot restart the trial.
- **Two-phase validation:** `initialize()` is strictly offline and never blocks;
  `startOnlineHeartbeat()` (called by `main()` two seconds after the UI is up)
  revalidates the key against the activation server asynchronously and tracks
  consecutive offline days against the grace period.

**Why:** A POS terminal must boot even with no internet, so offline-first
validation with an online heartbeat is the only practical scheme. The redundant
breadcrumb + registry hash is defence-in-depth: each individually is easy to
defeat, but together they force an attacker to find and forge *both* locations
with the correct machine-bound signature. The server URL is a placeholder
(`https://yourserver.com:5000`) to be configured per deployment.

---

## 2. Data layer

### money.h — `Money`

**What:** The single currency type used across the whole app — an exact amount
in integer minor units (cents), plus the configurable currency symbol and
`formatMoney()`.

**How:** A value type wrapping a `qint64` cent count. Conversions to/from the
outside world are *explicit* (`fromCents`/`fromMajor`, `cents()`/`toMajor()`) so
a stray `double` can never silently become money. The only float involved is
`fromMajor()`'s round-half-away conversion at UI input, and tax-rate
multiplication.

**Why:** Doubles cannot represent money exactly (`0.1 + 0.2 != 0.3`), which for a
POS is a latent liability — totals that don't reconcile, unsafe `==`, drift that
compounds over a day's sales. Integer cents are exact; rounding happens once,
deliberately, at the edge.

### database.h / database.cpp — `Database` + the core data structs

**What:** The persistence layer's connection holder and public facade. Defines
the data structs — `Product`, `Sale`, `SaleItem`, `SalePayment`, `SaleRequest`,
`Supplier`, `PurchaseOrder`/`PurchaseOrderItem`, `Expense`/`ExpenseCategory`,
`Customer` — and exposes the full data API (product/stock, sales, refunds,
suppliers, purchase orders, expenses, customers, analytics, backup). All money
fields are `Money`; all date fields are typed (`QDateTime`/`QDate`). `Sale`
carries `cashier` and `shiftId` audit context.

**How:**
- **Not a singleton.** A single `Database` is constructed in `main()` and
  injected by reference everywhere (see Conventions). The constructor resolves a
  per-user data directory via `QStandardPaths::AppDataLocation` (falling back to
  the executable dir) and opens `pos_database.db`. `configureForTesting()`
  repoints it at an in-memory/file connection so tests own their own instance.
- `initialize()` enables `PRAGMA foreign_keys`/WAL/busy_timeout, creates all
  tables idempotently, runs the versioned migrations, then the one-time
  money-to-cents data conversion, and seeds sample data on a fresh DB.
- **Versioned migrations:** `runMigrations()` applies each numbered step newer
  than the `schema_meta` `schema_version` row, each in its own transaction and
  written idempotently (via `ensureColumn()`), so a fresh DB and an old DB
  converge on the same schema. The money-to-cents conversion is kept separate
  and marker-gated so it can never re-run.
- **Thin facade.** `Database` holds no business SQL of its own — every data
  method delegates to a repository (e.g. `recordSale()` →
  `SaleRepository(db).recordSale()`), propagating `lastError()` on writes. This
  keeps the public API (and its ~120 call sites) stable while the query logic
  lives in focused, separately-testable units.

**Why:** Injecting one owned connection (rather than a global accessor) makes
dependencies explicit and the data layer testable and multi-DB-capable. The
repository split keeps each aggregate's SQL cohesive and reviewable. Typed money
and dates push correctness to the type system. Stock validation *inside*
`recordSale`'s transaction makes concurrent overselling impossible; sale items
snapshot name/price/cost because products get renamed/repriced/deleted later and
historical reports must reflect what was actually charged.

### Repository layer — `*repository.h` / `*repository.cpp`

**What:** Eight classes that own the read/write SQL for one aggregate each,
extracted from what used to be the `Database` "god object":

| Repository | Owns |
|---|---|
| `SaleRepository` | `recordSale()` (the atomic checkout write) + the `Sale`/`SaleItem` queries |
| `ProductRepository` | catalog CRUD, stock mutators, `adjustStockWithLog`, stock history + valuation |
| `CustomerRepository` | customer CRUD, phone lookup, store-credit/loyalty adjustments |
| `SupplierRepository` | supplier CRUD (soft delete) |
| `ExpenseRepository` | expense categories + expenses + date-range totals |
| `PurchaseOrderRepository` | PO CRUD + the transactional `receivePurchaseOrder` |
| `RefundRepository` | `processRefund`/`isRefunded` |
| `SalesAnalyticsRepository` | takings, top sellers, payment totals, gross profit, P&L |

**How:** Each takes a `QSqlDatabase` *by value* (a cheap ref-counted handle to
the one open connection — the same pattern as `Ledger`), so it shares
`Database`'s connection and any in-flight transaction without owning it.
Cross-aggregate operations compose: `RefundRepository::processRefund` reuses
`SaleRepository` + `ProductRepository`, and `PurchaseOrderRepository::
receivePurchaseOrder` reuses `ProductRepository` for the per-line audit log, all
on the same connection inside one transaction.

**Why:** A single class owning the entire ERP persistence layer was the codebase's
biggest structural debt. One repository per aggregate makes each slice cohesive,
independently testable, and safe to extend without touching the others; the
`Database` facade keeps every existing call site working.

### CartItem.h — `CartItem`

**What:** Plain struct for one line in a shopping cart: product id, name, selling
price, cost price, quantity, category, with `getSubtotal()` and `getProfit()`
helpers.

**How:** Header-only value type with two constructors (default and full) and
inline arithmetic.

**Why:** Carts are transient UI state; a lightweight copyable struct avoids any
database coupling. Carrying `costPrice` through the cart lets checkout write
profit data into `sale_items` without re-querying products mid-transaction.

---

## 3. Users, authentication & permissions

### user.h / user.cpp — `UserRole`, `Permission`, `User`, `RoleManager`

**What:** The role-based access control (RBAC) model. Four hierarchical roles
(ADMIN → MANAGER → CASHIER → VIEWER), ~18 granular `Permission` values covering
sales, inventory, reports, user management, and system areas, the `User` record
struct (including `mustChangePassword`), and `RoleManager`, a static helper that
maps roles ⇄ strings and answers `hasPermission(role, permission)`.

**How:** Enums plus a static class. `RoleManager::getPermissionsForRole()` holds
the authoritative role→permission table in one place; everything else
(`hasPermission`, UI gating) derives from it. Role strings are used for database
storage and display.

**Why:** Separating *roles* from *permissions* means a feature checks "can this
user APPLY_DISCOUNTS?" rather than "is this user a manager?", so role definitions
can change without touching feature code. Centralizing the mapping in
`RoleManager` prevents permission logic from drifting across dialogs.

### usermanager.h / usermanager.cpp — `UserManager`

**What:** Singleton owning authentication and the user lifecycle: login/logout,
the in-memory current-user session, permission checks, user CRUD, password
changes, username/email validation, password-strength feedback, and an audit log
of user actions.

**How:** `login()` looks up the user by username (active users only), verifies
the password via `PasswordHasher::verify()`, and — if the stored hash is in the
legacy unsalted SHA-256 format — **transparently rehashes it to salted PBKDF2**
using the just-verified plaintext, then updates `last_login` and writes an audit
entry. `changeOwnPassword()` requires the old password; `changePassword()` is the
admin path. All queries are prepared statements against the `users` table created
by `Database`. Session state is just two members: `currentUser` and `loggedIn`.

**Why:** A singleton session matches the physical reality of a POS terminal — one
operator at a time. The opportunistic rehash-on-login is the only moment the
plaintext is available, making it the standard way to migrate legacy hashes
without forcing a mass password reset. Audit logging (`logUserAction`) exists
because cash-handling environments need accountability for who did what.

### passwordhasher.h — `PasswordHasher` namespace

**What:** Header-only password/PIN hashing utility. Produces and verifies salted
PBKDF2-SHA256 hashes in the format
`pbkdf2-sha256$<iterations>$<salt-hex>$<hash-hex>`, with backward compatibility
for legacy unsalted SHA-256 hex digests.

**How:** `hash()` draws a 16-byte salt from `QRandomGenerator::system()` (the
OS CSPRNG) and derives a 32-byte key with `QPasswordDigestor::deriveKeyPbkdf2`
at 100,000 iterations. `verify()` parses the stored format (or falls back to
legacy SHA-256), re-derives, and compares with `constantTimeEquals()` — a
XOR-accumulator loop that takes the same time regardless of where the mismatch
occurs. `needsRehash()` flags legacy hashes so callers can upgrade them.

**Why:** Unsalted fast hashes are trivially crackable with rainbow tables; PBKDF2
with a per-credential salt and high iteration count makes offline cracking
expensive. The constant-time comparison closes the timing side-channel of early
`==` returns. Embedding the iteration count in the stored string lets the cost be
raised in the future without breaking existing hashes. It is a stateless inline
namespace (not a class) because hashing needs no state and must be usable from
both `UserManager` and `SettingsManager` (discount PIN) without link-order
concerns.

### logindialog.h / logindialog.cpp — `LoginDialog`

**What:** The modal username/password dialog shown before the main window.

**How:** Hand-built UI (no `.ui` file): line edits (password masked), login and
cancel buttons, inline error label, themed via `applyStyles()`. It only *collects*
credentials — `main.cpp` performs the actual `UserManager::login()` call and
handles attempt counting.

**Why:** Keeping authentication logic out of the dialog separates presentation
from policy: the retry limit, lockout messaging, and forced password change all
live in one place (`main.cpp`/`UserManager`) instead of being baked into a widget.

### usermanagementdialog.h / usermanagementdialog.cpp — `UserManagementDialog`, `UserDialog`, `PermissionsViewDialog`

**What:** The admin UI for managing accounts. `UserManagementDialog` lists all
users in a filterable/searchable table with add / edit / delete / toggle-status /
change-password actions. `UserDialog` is the add/edit form with live
password-strength feedback. `PermissionsViewDialog` shows a read-only table of
what a given role may do.

**How:** All three delegate every mutation to `UserManager` (which enforces
validation and hashes passwords); the dialogs never touch SQL or hashes
themselves. The list dialog keeps a local `QVector<User>` cache and filters it
in-memory for search/role filters. `UserDialog::validateInput()` checks username
uniqueness and email format via `UserManager` before accepting.

**Why:** Routing everything through `UserManager` guarantees the same rules apply
whether a user is created from this UI or anywhere else, and keeps password
handling in exactly one audited code path. The permissions viewer exists so admins
can see the effective RBAC matrix instead of guessing what "Manager" means.

---

## 4. Main window & sales flow

### mainwindow.h / mainwindow.cpp — `MainWindow`, `Cart`

**What:** The central POS screen and application hub (~1,900 lines). The `Cart`
struct models one open transaction (line items, discount + reason, timestamps,
subtotal/item-count helpers). `MainWindow` provides: a product grid with search
and category filter, **multiple simultaneous carts** in a tab bar (create, switch,
merge, close), the totals panel (subtotal/tax/discount/total), discount
application gated by PIN, checkout, receipt printing, and the menu bar that
launches every other dialog. It also owns the subsystem objects:
`InventoryManager`, `ReceiptPrinter`, `ScheduleManager`, `SettingsManager`, and
`BarcodeReader`.

**How:**
- The UI is built entirely in code (`setupUI()` and its helpers), not from the
  `.ui` file; `applyTheme()` restyles everything from the current `ColorScheme`.
- Carts live in a `QMap<int, Cart>` keyed by id, with `currentCartId` tracking
  the active tab; `getCurrentCart()` is the single accessor all cart slots use.
- `addToCart()` checks available stock (via the products cache) before adding;
  `updateTotals()` recomputes subtotal → discount → tax using
  `SettingsManager`'s tax helpers (supporting tax-inclusive pricing).
- **Checkout (`onCheckout()`):** converts cart items to `SaleItem`s, collects
  payment via `PaymentDialog`, calls `Database::recordSale()` (the atomic
  transaction), then on success prints a receipt via `ReceiptPrinter`, calls
  `InventoryManager::refreshAfterSale()` per product to emit low-stock warnings,
  refreshes the product grid, and removes the completed cart.
- Barcode scans arrive via the `onBarcodeScanned()` slot and add the matching
  product to the current cart; unknown barcodes show a toast.
- `updateUIPermissions()` enables/disables menu actions and buttons from
  `UserManager::hasPermission()`, so a cashier physically cannot click what they
  may not do.
- Keyboard shortcuts (F-keys etc.) are bound in `setupKeyboardShortcuts()` for
  fast cashier operation; transient feedback uses `showToast()` rather than modal
  boxes.

**Why:** Multiple carts mirror real retail: a cashier parks one customer's order
while serving another. MainWindow deliberately owns the subsystem objects and
passes them into dialogs so there is exactly one instance of each manager and one
source of truth. Stock is only *validated* in the UI but *enforced* in
`Database::recordSale()` — the UI check gives fast feedback, the transaction
gives correctness. Permission-driven UI disabling complements (not replaces) the
role checks in managers.

> **Note:** every screen builds its UI programmatically in `setupUI()`; there are
> no Qt Designer `.ui` files. This makes runtime theming (light/dark restyling)
> straightforward and keeps layout logic next to the code that drives it.

---

## 5. Checkout support: payments, discounts, receipts

### paymentdialog.h / paymentdialog.cpp — `PaymentDialog`

**What:** The modal "take payment" step of checkout: choose Cash / Card / Mobile /
Multiple, enter amount tendered, see change due live.

**How:** Radio buttons in a `QButtonGroup`; `calculateChange()` re-runs on every
amount keystroke and the confirm button is only enabled when `amountPaid ≥ total`
(for cash). Results are exposed via simple getters that `MainWindow::onCheckout()`
reads after `exec()` returns `Accepted`.

**Why:** Computing change in the dialog (before the sale is recorded) prevents a
half-recorded sale when the cashier mistypes; the sale only hits the database
after payment details are confirmed.

### paymentmanager.h / paymentmanager.cpp — `PaymentManager`

**What:** Persistence and reporting for *how* sales were paid. Defines the
`PaymentMethod` enum (Cash, Card, MobileMoney, BankTransfer, Check, GiftCard,
StoreCredit, Other), `PaymentRecord`/`PaymentSummary` structs, and queries for
per-transaction history, daily summaries, and per-shift summaries by method.

**How:** Creates its own payments table on construction; `recordPayment()` inserts
one row per tender. Static helpers map each method to a display name, icon, and
`QColor` so every screen renders payment methods identically. Aggregation queries
(`getDailySummary`, `getShiftSummary`) GROUP BY method.

**Why:** Splitting payment records from the `sales` table supports split tender
(one sale paid partly cash, partly card) and enables the cash-reconciliation math
that Z-Reports and shift closes need ("how much *cash* should be in the drawer?"
≠ "what were total sales?"). MobileMoney is a first-class method because M-Pesa
is the dominant tender in the Kenyan target market.

### discount.h / discount.cpp — `DiscountManager`

**What:** Discount business logic: PIN-gated authorization, percentage and
fixed-amount calculators, a list of preset discounts ("10% Off", "Staff 25%",
"$5 Off"…), and an audit log of every discount applied.

**How:** `applyPercentage()`/`applyFixedAmount()` return a `DiscountResult`
(type, value, computed amount, label, who applied it); fixed discounts are
clamped with `qMin(amount, subtotal)` so a discount can never make a total
negative. `logDiscount()` writes shift id, cart id, type, amounts, cashier, and
ISO timestamp into the self-created `DiscountLog` table and emits
`discountApplied`. Note: this class's own `m_pin` is a plain default ("1234");
the *production* PIN path used by `MainWindow::onApplyDiscount()` is
`SettingsManager::verifyDiscountPin()`, which is PBKDF2-hashed.

**Why:** Discounts are the easiest way for an employee to leak money ("sweethearting"),
so every application requires a PIN and leaves an audit row tied to the shift and
cashier. Presets reduce keying errors at the till.

### discountdialog.h / discountdialog.cpp — `DiscountDialog`

**What:** The UI for entering a discount on the current cart: percentage or fixed
radio choice, value field, required reason field, live preview of discount amount
and new total.

**How:** `calculateDiscount()` re-runs on each input change and validates ranges
(percentage ≤ 100, fixed ≤ subtotal). `MainWindow` performs the PIN prompt before
opening this dialog, then stores the result on the active `Cart`.

**Why:** The live preview prevents "oops, I meant 10 not 100" errors, and the
mandatory reason feeds the audit trail.

### receiptprinter.h / receiptprinter.cpp — `ReceiptPrinter`, `Receipt`

**What:** Receipt generation and output. `Receipt` snapshots everything a printed
receipt needs (items, totals, tender, change, cashier, timestamps). The class
supports three nominal output types (thermal / standard printer / PDF), company
header and footer configuration, last-receipt reprint, and (stub) email.

**How:** Builds receipts twice — `generateReceiptText()` produces fixed-width
text (with `centerText`/`padRight` helpers) suitable for 40-column thermal
printers, and `generateReceiptHTML()` produces styled HTML rendered through
`QTextDocument` + `QPrinter` for standard printing and PDF export. Company
info and printer config persist in `QSettings` ("KeynetikPOS/ReceiptPrinter")
and load in the constructor. In the current build `printReceipt()` always takes
the PDF path, naming files `Receipt_<saleId>_<timestamp>.pdf`. The last receipt
is cached in memory for `reprintLastReceipt()`.

**Why:** PDF-by-default makes the system usable with zero printer setup (files
can be archived, emailed, or printed later) while the thermal text path is kept
ready for real ESC/POS hardware. Receipts snapshot data rather than re-query it
so a reprint is identical to the original even if products changed since.

---

## 6. Inventory subsystem

### inventorymanager.h / inventorymanager.cpp — `InventoryManager`

**What:** Inventory intelligence layer above raw stock numbers. Classifies each
product into `InventoryStatus` (Healthy / Low ≤ 50 / Critical ≤ 4 / OutOfStock),
caches per-product `InventoryInfo` (quantity, reorder level, supplier, status),
answers `canSell()`, lists low/critical items, performs restocks, and emits Qt
signals (`inventoryLow`, `inventoryCritical`, `inventoryOutOfStock`,
`inventoryRestocked`) when levels change.

**How:** Maintains `QMap<int, InventoryInfo> cachedInventory` loaded lazily from
`Database`; a `QTimer` (default 60 s) drives `checkAllInventoryLevels()` for
periodic re-scans. `refreshAfterSale()` deliberately does **not** decrement stock
— `Database::recordSale()` already did that inside the checkout transaction — it
only re-reads the new quantity, updates the cache, and fires threshold signals.

**Why:** The signal/slot design decouples detection from presentation: MainWindow
shows toasts, InventoryDialog updates alert tables, and future listeners (e.g.
scheduled SMS alerts) can subscribe without touching this class. Keeping the
decrement in the database transaction and the *notification* here avoids
double-decrement bugs while still giving the UI real-time warnings.

### inventorydialog.h / inventorydialog.cpp — `InventoryDialog`

**What:** The full inventory workbench (largest UI file, ~80 KB): four tabs —
Overview (filter/search table + summary cards), Stock Management (editable grid
with add/delete rows, auto-save, undo/redo), Alerts (low/critical lists with bulk
restock), and Analytics (stock value, fast/slow movers).

**How:** Edits happen directly in `QTableWidget` cells; `onStockTableCellChanged`
marks rows dirty, `validateProductRow()` checks them (highlighting invalid cells
with explanatory tooltips), and changes are committed via explicit Save or a
`QTimer`-driven auto-save. A `QUndoStack` provides undo/redo for stock edits.
Live updates arrive by subscribing to `InventoryManager` signals. CSV
import/export, printing, keyboard shortcuts, and toast-style notifications
(`showNotification`) round it out.

**Why:** Stock-take is the most data-entry-heavy task in a POS; spreadsheet-style
in-place editing with validation, auto-save, and undo is much faster than
one-modal-dialog-per-product, while validation + audit logging keep bulk edits
safe.

### lowstockdialog.h / lowstockdialog.cpp — `LowStockDialog`

**What:** A focused "what needs reordering *now*" popup: two tables (Critical and
Low stock) with double-click or button-triggered restock.

**How:** Pulls `getCriticalStockItems()`/`getLowStockItems()` from
`InventoryManager`, colour-codes rows by severity, and opens a small restock
dialog that calls `restockProduct()`.

**Why:** It exists separately from `InventoryDialog` because reacting to a
low-stock warning shouldn't require navigating a four-tab workbench — this is the
one-click triage view MainWindow opens from alerts.

---

## 7. Barcode scanning

### barcodereader.h / barcodereader.cpp — `BarcodeReader`

**What:** The current hardware-input class for barcode scanners (the header notes
it *replaces* `BarcodeScannerManager` for input). Supports the two ways real
scanners present themselves: **keyboard-wedge** (USB HID — the scanner "types"
the code followed by Enter) and **serial** (RS-232 / virtual COM). Emits
`barcodeScanned(QString)` with a cleaned code, or `errorOccurred`.

**How:**
- *Keyboard-wedge mode:* installed as a Qt event filter (typically on `qApp`), it
  buffers incoming key characters; Enter/Return flushes the buffer, and a
  single-shot inter-character timer (default 100 ms) flushes it even without a
  terminator. Scanners burst characters ~1 ms apart, humans cannot, which is how
  scans are distinguished from typing.
- *Serial mode:* opens a `QSerialPort` (default 9600-8-N-1), accumulates bytes on
  `readyRead`, and splits on CR/LF. `availablePorts()` enumerates COM ports for
  the settings UI.
- Configurable prefix/suffix stripping removes STX/ETX framing bytes some
  scanners add.

**Why:** Keyboard-wedge is the cheapest, most common scanner type and requires no
drivers, but raw keystrokes would otherwise land in whatever widget has focus —
the event-filter + timing approach captures scans application-wide without
stealing real typing. Serial support covers older/industrial scanners. The two
modes are one class so `MainWindow` connects a single signal regardless of
hardware.

### barcodescannermanager.h / barcodescannermanager.cpp — `BarcodeScannerManager`, `BarcodeGeneratorDialog`, `BarcodeScannerWidget`

**What:** The barcode *data* layer plus legacy input. `BarcodeScannerManager`
does product lookup by barcode, barcode assignment/removal, **EAN-13 generation
with valid check digits** (plus Code128/QR string generation), bulk "generate for
all products", scan history, and beep-on-scan. `BarcodeGeneratorDialog` is the UI
for generating/printing barcode labels; `BarcodeScannerWidget` is the older
event-filter input widget superseded by `BarcodeReader`.

**How:** `lookupByBarcode()` queries the products table and returns a
`ProductInfo` with a `found` flag; every scan is recorded to history.
`generateEAN13()` builds a 12-digit base from the product id and appends the
standard modulo-10 checksum (`calculateEAN13Checksum`). Signals
(`productFound`/`productNotFound`) let UIs react without polling.

**Why:** Many small shops stock unlabelled goods; generating printable EAN-13
labels in-app removes the need for a GS1 subscription or external tooling.
Lookup/generation stays separate from `BarcodeReader` so the same data logic
serves any input source — scanner, keyboard, or test button.

---

## 8. Settings & theming

### settingsmanager.h / settingsmanager.cpp — `SettingsManager`, `BusinessSettings`

**What:** Business-level configuration: identity (name, address, phone, email,
website), currency symbol/code, receipt footer, full tax configuration (enabled,
rate, label like "VAT"/"GST", and tax-*inclusive* pricing), receipt printing
toggle, and the PBKDF2-hashed discount PIN.

**How:** Persists to a simple `AppSettings(Key, Value)` table via
`INSERT OR REPLACE`; `load()` reads every key with the struct defaults as
fallbacks, so missing keys are harmless. On load it migrates any legacy plaintext
PIN (including the factory default "1234") to a PBKDF2 hash;
`verifyDiscountPin()` only ever compares hashes and the plaintext is never
exposed. Tax helpers (`taxAmount`, `applyTax`, `extractTax`) implement both
add-on tax and back-calculation of tax from inclusive prices
(`total − total/(1+rate)`). `save()` emits `settingsChanged` so open windows
(e.g. MainWindow's tax label) refresh live.

**Why:** This data lives in the *database* (not `QSettings`) because it is
business data that should travel with a backup of `pos_database.db`. Key-value
storage means adding a setting never requires a schema migration. Tax-inclusive
support matters because Kenya (and much of the world) displays VAT-inclusive
shelf prices.

### settingsdialog.h / settingsdialog.cpp — `SettingsDialog`

**What:** Tabbed editor for all of the above: Business, Tax, Receipt, Security
(discount PIN + require-PIN toggle), and Messaging (Twilio WhatsApp SID/token/
number and Africa's Talking API key/username/sender ID, with a test-connection
button).

**How:** `loadCurrentValues()` populates widgets from `SettingsManager`; `save()`
writes back the struct, hashes a newly entered PIN (entered twice for
confirmation), stores messaging credentials, and triggers
`MainWindow::reloadProviderCredentials()` so providers pick up new keys without a
restart. Tax controls enable/disable as a group via `onTaxEnabledChanged`.

**Why:** One dialog for all configuration keeps administration discoverable; the
PIN confirmation field and write-only PIN handling prevent both typos and
shoulder-surfing of the stored value.

### colorscheme.h / colorscheme.cpp — `ColorScheme`, `ThemeManager`

**What:** Theming. `ColorScheme` is a flat struct of named colour strings
(backgrounds, text, borders, semantic accents, button states); `ThemeManager` is
a singleton holding the current light/dark state and scheme, emitting
`themeChanged(bool)` on toggle.

**How:** `getLightColorScheme()` / `getDarkColorScheme()` are **inline** header
functions returning hard-coded palettes (light mimics classic Windows system
colours; dark mimics Win11/VS Code). The header comments explicitly warn the
inline bodies must stay in the header to avoid multiple-definition linker errors.
`ThemeManager` persists the choice in `QSettings` and rebuilds the scheme on
toggle; widgets either connect to `themeChanged` or call `getColorScheme()` when
(re)styling.

**Why:** Qt stylesheets are strings, so a struct of colour strings composes
directly into `setStyleSheet()` calls. Centralizing the palette means dark mode
is a single boolean flip rather than per-dialog colour edits; persistence makes
the choice survive restarts.

---

## 9. Analytics & reporting

Analytics queries live in `SalesAnalyticsRepository` (see §2); the UIs below
present them. (Three earlier overlapping analytics screens — `AnalyticsManager`,
`AnalyticsDialog`, and `EnhancedAnalyticsDialog` — were removed in a
consolidation pass; `AnalyticsDashboard` is the surviving dashboard.)

### analyticsdashboard.h / analyticsdashboard.cpp — `AnalyticsDashboard`

**What:** The analytics dashboard: date-range driven (quick ranges + explicit
start/end date pickers), metric cards (sales, profit, margin, tax collected,
discounts given, transactions, items), and tabs for top products, slow movers,
and an hourly-sales table, with CSV export.

**How:** Runs SQL aggregates over `sales`/`sale_items` (and uses
`Database::getActualGrossProfit()` for profit from the `sale_items.cost_price`
snapshots) through the injected `Database`, rendering into `QTableWidget`s. The
date pickers yield `QDate`s passed straight to the typed query API.

**Why:** Arbitrary date ranges and a profit/margin focus answer "how did the
business do between these two dates" — the everyday management view.

### reportsdialog.h / reportsdialog.cpp — `ReportsDialog`

**What:** Tabular operational reports: sales by date, by category, by payment
method, top sellers, and a daily summary — selectable by type and date range,
exportable to CSV and PDF.

**How:** A combo box picks the generator method; each `generate*Report()` fills
the shared `QTableWidget` and a `QVector<QStringList>` backing store from SQL
aggregates. PDF export renders an HTML version of the table via
`QTextDocument`/`QPrinter`; CSV export writes the backing store directly.

**Why:** These are the printable/exportable documents an owner hands to an
accountant — distinct from the on-screen exploratory dashboards, hence
table-first layout and file export as the primary action.

### saleshistorydialog.h / saleshistorydialog.cpp — `SalesHistoryDialog`

**What:** Transaction browser and the refund entry point: filterable list of all
sales (date range, quick filters like Today/This Week, payment method, free-text
search), summary strip (total, count, average, tax), drill-down to line items,
CSV export, and a Refund button.

**How:** Loads `Sale` rows from `Database::getSalesByDateRange()`, filters in
memory, and shows items from `getSaleItems()` in a details view. Refunds prompt
for a reason, check permissions and `isRefunded()` (no double refunds), then call
`Database::processRefund()` which records the refund and restores stock.

**Why:** Refunds deliberately live behind the sales history rather than a free-
standing "refund" form: forcing selection of the *actual original sale* prevents
refunding amounts that were never charged, and ties every refund to a real
transaction, a reason, and a user for the audit trail.

### zreportdialog.h / zreportdialog.cpp — `ZReportDialog`, `ZReportData`

**What:** End-of-day / end-of-shift fiscal summary (the classic POS "Z-Report"):
gross sales, discounts, net sales, tax collected, transaction count, sales by
category, top products, and cash reconciliation — opening float, closing float,
expected cash, and the variance (over/short).

**How:** `buildReport()` aggregates `sales`/`sale_items` for the chosen date
(optionally a specific shift from `ShiftManager`) into the `ZReportData` struct;
`renderHtml()` formats it for the preview pane, with print and export actions.
The static `generateZReportText()` produces a plain-text rendering **callable
without any UI** — this is what `ScheduleManager` uses to build SMS/WhatsApp
report bodies.

**Why:** The Z-Report is the standard retail control document for detecting
drawer shortages and summarizing the day. The static text generator exists so
scheduled messaging reuses exactly the same numbers as the on-screen report —
no second implementation to drift.

---

## 10. Shifts

### shift_type.h — `ShiftRecord`

**What:** Plain struct for one cashier shift: id, cashier, opening/closing float,
running totals (sales, discounts, transaction count), open/close timestamps,
open flag, notes.

**How/Why:** Header-only POD shared by `ShiftManager`, the shift dialogs, and
`ZReportDialog`; keeping it dependency-free avoids include cycles between those
three.

### shiftmanager.h / shiftmanager.cpp — `ShiftManager`

**What:** Shift lifecycle and accounting: open a shift with a counted starting
float, accumulate sales/discounts/voids as they happen, close with a counted
ending float and notes, query history.

**How:** Self-creates the `Shifts` table. `loadActiveShift()` runs at
construction and re-adopts any shift still flagged `IsOpen = 1` — so a crash or
power cut doesn't orphan the shift. `recordSale()`/`recordVoid()` update both the
in-memory `ShiftRecord` and the database row as transactions occur. Signals
`shiftOpened`/`shiftClosed` notify the UI and any report triggers.

**Why:** Shifts are the unit of cash accountability: the float counted at open
plus recorded cash sales is what the drawer *should* contain at close, and the
Z-Report computes the variance. Persisting the running totals (rather than
recomputing at close) keeps the open-shift state crash-safe.

### shift_dialogs.h / shift_dialogs.cpp — `OpenShiftDialog`, `CloseShiftDialog`

**What:** The two small modal forms bracketing a shift: open (cashier name +
counted opening float) and close (shows the shift summary, takes counted closing
float + notes).

**How:** Thin `QDialog`s exposing values through getters; the caller passes them
to `ShiftManager`. `CloseShiftDialog` receives the current `ShiftRecord` and
currency symbol so the cashier sees expected totals while counting the drawer.

**Why:** Forcing an explicit counted float entry at both ends (rather than
defaulting) is what makes the over/short calculation meaningful.

---

## 11. Scheduled messaging

### messageprovider.h — `MessageProvider`

**What:** Abstract base class for anything that can deliver a message: declares
`sendMessage(recipient, message, options)`, `isConfigured()`, `providerName()`,
`providerType()`, an optional native `scheduleMessage()`, and the common signals
`messageSent`, `errorOccurred`, `statusChanged`.

**How:** Pure-virtual `QObject` interface; concrete providers implement transport
details and report results via the signals (all sending is asynchronous).

**Why:** This is the strategy pattern applied to messaging: `ScheduleManager` and
the settings UI talk only to this interface, so adding email or another SMS
gateway means writing one subclass — zero changes to scheduling code.

### MessageSchedule.h — `MessageSchedule`

**What:** Plain struct describing one scheduled message: schedule type (Daily /
Weekly / Monthly / OnShiftClose / OnSalesThreshold / Custom), report type
(Z-Report / X-Report / Inventory Alert / Sales Summary / Custom), send time,
day-of-week/month, recipients, provider name, active flag, last-sent timestamp,
failure counter, threshold value, and metadata — plus `typeString()` /
`reportTypeString()` display helpers.

**How/Why:** A pure value type so it can be copied freely between the editor
dialog, the manager's in-memory list, and the SQLite row without lifetime
concerns. Storing `lastSent` is what makes once-per-day semantics possible.

### schedulemanager.h / schedulemanager.cpp — `ScheduleManager`

**What:** The scheduling engine: a provider registry, schedule CRUD with SQLite
persistence (`message_schedules` table), and a timer loop that fires due
schedules and dispatches generated reports through the chosen provider.

**How:** A `QTimer` ticks every 60 seconds; `onTick()` runs `shouldFire()` on
each active schedule — comparing current time/day against the schedule and using
`lastSent` to prevent duplicate sends within the same window. `buildReportBody()`
assembles the report text (Z-Report text via `ZReportDialog`'s static generator,
inventory alerts from stock queries, sales summaries from `Database`);
`dispatchSchedule()` resolves the provider by name and calls `sendMessage()` for
each recipient, updating `lastSent`/`failedAttempts` and emitting
`scheduleFired`/`messageDelivered`/`messageError`. Providers are registered by
name with duplicate protection; schedules are loaded from the database at startup.

**Why:** A one-minute poll is precise enough for "send the Z-Report at 21:00" and
vastly simpler than per-schedule timers. Persisting schedules makes them survive
restarts; persisting `lastSent` makes them survive restarts *without* re-sending.
The owner gets their daily numbers by SMS/WhatsApp without standing at the till.

### whatsappmanager.h / whatsappmanager.cpp — `WhatsAppManager`

**What:** `MessageProvider` implementation for WhatsApp via the Twilio API,
including a formatted-Z-Report convenience (`sendZReport`).

**How:** POSTs form-encoded requests to Twilio's Messages endpoint with HTTP
Basic auth (Account SID + auth token) and `whatsapp:`-prefixed numbers, via
`QNetworkAccessManager`. `formatPhoneNumber()` normalizes input to E.164
(strips non-digits, ensures `+`, applies the Kenyan 254 prefix to local numbers).
Replies are parsed in `onReplyFinished` and surfaced through the standard
provider signals. Credentials come from `SettingsDialog`'s Messaging tab.

**Why:** WhatsApp is the default business communication channel in the target
market, and Twilio's sandbox makes it testable for free before production setup
(the header documents pricing and setup steps for the deployer).

### Africatalkingprovider.h — `AfricasTalkingProvider`

**What:** `MessageProvider` implementation for plain SMS via Africa's Talking, a
Kenyan SMS gateway (~KES 0.80/message, documented in the header).

**How:** Header-only implementation: POSTs `username/to/message` (plus optional
sender ID) form data with an `apiKey` header to the Africa's Talking messaging
endpoint, then parses the JSON response's `SMSMessageData.Recipients[0]` for
status code 101 / "Success", emitting `messageSent` or `errorOccurred`
accordingly.

**Why:** SMS reaches owners on any phone — no smartphone or WhatsApp account
needed — and a local provider is cheaper and better supported than Twilio for
Kenyan numbers. Together the two providers demonstrate the `MessageProvider`
abstraction doing its job.

### scheduledialog.h / scheduledialog.cpp — `ScheduleDialog`

**What:** Management list of all message schedules: table with name, type, report,
recipients, provider, status, last sent; buttons for add / edit / delete /
toggle-active / **Send Now**.

**How:** Reads `getAllSchedules()` into the table; buttons call straight into
`ScheduleManager`; Send Now invokes `sendNow()` so a schedule (and provider
credentials) can be tested without waiting for the trigger time.

**Why:** Send Now exists because the worst time to discover a typo'd API key is
21:00 when the nightly report silently fails.

### scheduleeditordialog.h / scheduleeditordialog.cpp — `ScheduleEditorDialog`

**What:** The add/edit form for one schedule: name, type, report type, time,
day-of-week/month, provider, recipient list, sales threshold, notes, active flag.

**How:** Created with an optional `scheduleId` (−1 = new); `loadSchedule()`
populates fields for editing. `updateVisibility()` shows only the controls
relevant to the chosen type (day-of-week appears for Weekly, threshold for
OnSalesThreshold, etc.); `validate()` gates the save button; the result is read
back via `getSchedule()`.

**Why:** Conditional field visibility prevents semantically invalid schedules
(e.g. a "day of month" on a daily schedule) at the input stage rather than at
fire time, where the user wouldn't see the error.

---

## 12. Accounting & ERP (Tier 4)

A double-entry accounting spine sits under the POS so the numbers reconcile. All
of these take the shared `QSqlDatabase` by value and post into the same
connection; money is `Money` (integer cents) throughout.

### ledger.h / ledger.cpp — `Ledger`

**What:** A minimal but real general ledger — a seeded Kenyan chart of accounts,
balanced journal entries (debits must equal credits), account balances, and a
trial balance. **How:** `postEntry()` validates and writes header + lines in one
transaction; an unbalanced or empty entry is rejected. **Why:** "ERP-complete"
means every money movement (sales, purchases, payroll, VAT) hangs off one spine
that always balances, turning a POS-with-reports into an accounting system.

### salejournal.h / salejournal.cpp — journal builders

**What:** Pure functions that turn a sale / received purchase / expense into
balanced GL journal lines (`buildSaleJournal`, `buildPurchaseJournal`,
`buildExpenseJournal`). **How:** No DB, no UI — just maths over `Money`, unit
tested in isolation; `CheckoutService` and the PO/expense dialogs call them and
post the result to `Ledger` best-effort after the underlying record commits.
**Why:** keeps the account-mapping logic reviewable and pinned by tests, and the
GL a complete record without coupling the journal maths to the database.

### payroll.h / payroll.cpp — `Payroll`

**What:** Kenyan statutory payroll — NSSF/SHIF/Housing levy deducted before PAYE,
progressive PAYE bands less personal relief. `computePayslip()` is pure and
tested; `runPayroll()` stores payslips and posts one balanced GL entry.
**Why:** correct statutory deductions are legally required; isolating the pure
maths makes them auditable against the Finance Act.

### vat.h / vat.cpp, etims.h / etims.cpp — `Vat`, eTIMS

**What:** VAT engine + VAT-3 aggregation (`splitInclusive`, `computeVat3`) keyed
off per-product tax codes, plus a KRA eTIMS client interface (`IEtimsClient` +
a logging stub pending device onboarding). **How:** VAT-3 is computed from the
transactional tables (sales/purchase items), not the GL, which is the legal
source of truth; the standard rate is kept in lock-step with the POS sale-tax
rate. **Why:** statutory returns must reconcile to what was actually charged.

### ledgerdialog / payrolldialog / vatdialog, and the purchasing/customer/expense dialogs

The Finance menu hosts `LedgerDialog` (trial balance + journal entry),
`PayrollDialog`, and `VatDialog` (VAT-3 return + tax-code editor), all gated by a
Tier-4 licence check. Purchasing and customer accounts are driven by
`SupplierDialog`, `PurchaseOrderDialog`/`NewPurchaseOrderDialog`,
`ExpenseDialog`, `CustomerDialog`, `RefundDialog`, and `StockTakeDialog` — thin
UIs over the repositories in §2. Each source file carries its own WHAT/HOW/WHY
header with the specifics.

---

## 13. Build system

### CMakeLists.txt

**What:** The complete build, deployment, and installer definition for
`KeynetikPOS` v2.0.0.

**How:** Requires CMake ≥ 3.16 and C++17; finds Qt 6 (or 5) components Widgets,
Core, Sql, PrintSupport, Network, and SerialPort with AUTOMOC/AUTOUIC/AUTORCC
enabled. Lists every source explicitly in `PROJECT_SOURCES`; builds a `WIN32`
(no-console) executable via `qt_add_executable`. Post-build, it stages the exe
and runs `windeployqt` to copy all Qt runtime DLLs into a deploy directory.
CPack/NSIS packaging (gated on `LICENSE.txt` existing) produces a Windows
installer with Start-menu/desktop shortcuts, icon, launch-after-install, and a
commented-out CD-key registry prompt; the comments document the NSIS/CMake
backslash-escaping pitfalls that shaped the implementation.

**Why:** Targets are non-technical shop owners: the NSIS installer plus
`windeployqt` staging yields a double-click setup with zero Qt knowledge
required. Optional resources (icon, `.qrc`, license) are existence-checked so
the project still builds from a bare source checkout.

**Tests:** With `KEYNETIK_BUILD_TESTS=ON` (default) CMake builds six GUI-less
QtTest suites run via `ctest` — `carttotals_test`, `database_test` (the
`recordSale`/refund money path through an in-memory DB and the repository
delegations), `ledger_test`, `payroll_test`, `vat_test`, and `salejournal_test`.
The CI workflow (`.github/workflows/ci.yml`) builds and runs them on Ubuntu and
does the full GUI build + `ctest` on Windows.

---

## Appendix: known overlaps & legacy files

| Item | Status |
|---|---|
| `xdebug.h`, `mainwindow.ui`, `analyticsdialog.ui` | **Removed.** Dead/empty or superseded by programmatic `setupUI()`. |
| `AnalyticsManager`, `AnalyticsDialog`, `EnhancedAnalyticsDialog`, `StockManager` | **Removed** in a consolidation pass — they overlapped `AnalyticsDashboard` / `InventoryDialog`, which are the survivors. |
| `BarcodeScannerWidget` (in barcodescannermanager.h) | Legacy input path; `BarcodeReader` is the current one. |
| `DiscountManager::m_pin` (plaintext compare) | Superseded in practice by `SettingsManager::verifyDiscountPin()` (PBKDF2). |
| `ShiftManager` / `ZReportDialog` | Implemented but not yet wired into the live checkout flow; `Sale.shiftId` is forward-compatible plumbing for when they are. |
