# -*- coding: utf-8 -*-
"""Generate KeynetikPOS_Technical_Manual.pdf — the software-engineer manual:
architecture, build, data layer, the sale pipeline, extension recipes."""

from reportlab.lib.units import mm
from _pdf_common import (
    Doc, H1, H2, H3, BODY, SMALL, PW, MARGIN, WARNBG, WARNBAR,
    Spacer, Paragraph, PageBreak, HRFlowable, LINE,
    bullets, steps, table, callout, code_block, cover, toc,
)

OUT = "KeynetikPOS_Technical_Manual.pdf"
S = []


def h1(t): S.append(Paragraph(t, H1))
def h2(t): S.append(Paragraph(t, H2))
def h3(t): S.append(Paragraph(t, H3))
def p(t):  S.append(Paragraph(t, BODY))
def sp(h=4): S.append(Spacer(1, h))
def rule(): S.append(HRFlowable(width="100%", thickness=0.5, color=LINE, spaceBefore=8, spaceAfter=8))


# ============================================================= cover + TOC
S += cover(
    "Technical Manual",
    "Architecture, build &amp; developer guide",
    ["Version 2.0 &nbsp;&bull;&nbsp; For software engineers",
     "Qt 6 &bull; C++17 &bull; SQLite &bull; CMake"],
)

h1("Contents")
S.append(toc([
    ("1", "Introduction &amp; audience"),
    ("2", "Technology stack &amp; dependencies"),
    ("3", "Repository layout"),
    ("4", "Building the project"),
    ("5", "Running &amp; testing"),
    ("6", "Architecture overview"),
    ("7", "Ownership &amp; dependency injection"),
    ("8", "The data layer: Money, Database, repositories"),
    ("9", "Schema &amp; migrations"),
    ("10", "The sale pipeline"),
    ("11", "Accounting &amp; ERP tier"),
    ("12", "UI architecture &amp; theming"),
    ("13", "Security &amp; licensing"),
    ("14", "Coding conventions"),
    ("15", "Extension recipes"),
    ("16", "Known gaps &amp; technical debt"),
]))
S.append(PageBreak())

# ============================================================= 1 intro
h1("1. Introduction &amp; audience")
p("KeynetikPOS is a single-machine, offline-first point-of-sale and ERP application for the "
  "Kenyan retail market. This manual is for engineers maintaining or extending the codebase. It "
  "assumes working C++ and Qt knowledge and focuses on how the system is structured and why.")
p("Companion documents: <font face='Courier'>CODE_DOCUMENTATION.md</font> (per-file reference) and "
  "the in-source header comments — every <font face='Courier'>.h</font>/<font face='Courier'>.cpp</font> "
  "opens with a WHAT / HOW / WHY block. This manual is the bird's-eye view those sit under.")

# ============================================================= 2 stack
h1("2. Technology stack &amp; dependencies")
S.append(table([
    ["Concern", "Choice"],
    ["Language", "C++17"],
    ["UI / framework", "Qt 6 (Widgets, Sql, Network, PrintSupport, SerialPort, Concurrent); Qt 5 fallback"],
    ["Build", "CMake &ge; 3.16, AUTOMOC/AUTOUIC/AUTORCC"],
    ["Database", "SQLite via Qt's QSQLITE driver, WAL mode"],
    ["Tests", "QtTest (GUI-less), driven by ctest"],
    ["Packaging", "windeployqt + CPack/NSIS installer"],
    ["Licence server", "Cloudflare Worker + D1 (see license-server/)"],
]))
p("There is no ORM and no third-party business-logic library — money, tax, payroll and the ledger "
  "are all implemented directly over Qt and SQLite.")

# ============================================================= 3 layout
h1("3. Repository layout")
p("The source tree is flat (one directory) — translation units sit at the repository root, tests "
  "under <font face='Courier'>tests/</font>. Files group into logical tiers:")
S.append(table([
    ["Tier", "Representative files"],
    ["Domain / money math (dependency-free, unit-tested)", "money.h, carttotals.*, salejournal.*, payroll.*, vat.*"],
    ["Data layer", "database.*, *repository.* (8 repos), ledger.*"],
    ["Services", "cartservice.*, checkoutservice.*, paymentmanager.*, discount.*, inventorymanager.*"],
    ["UI", "mainwindow.*, *dialog.*, cartmodel.*, productgridmodel.*, appstyle.*, colorscheme.*"],
    ["Infrastructure", "usermanager.*, settingsmanager.*, secretstore.*, smtpclient.*, licensemanager.*, schedulemanager.*"],
]))

# ============================================================= 4 build
h1("4. Building the project")
p("On Windows with the Qt MinGW kit (the verified configuration):")
S.append(code_block([
    '# add the Qt + MinGW toolchain to PATH, then build the Release preset',
    '$env:PATH = "C:\\Qt\\Tools\\mingw1310_64\\bin;C:\\Qt\\6.11.0\\mingw_64\\bin;" + $env:PATH',
    'cmake --build "build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release"',
]))
p("CMake presets (<font face='Courier'>CMakePresets.json</font> / <font face='Courier'>CMakeUserPresets.json</font>) "
  "define Qt-Debug / Qt-Release configurations that output under <font face='Courier'>out/</font>. An "
  "MSVC 2022 Debug configuration also exists under <font face='Courier'>build/</font>. The build is "
  "warning-clean under <font face='Courier'>-Wall -Wextra</font> (MSVC <font face='Courier'>/W4</font>); "
  "<font face='Courier'>-Werror</font> is not enabled.")
S.append(callout("All sources are listed explicitly",
    "PROJECT_SOURCES in CMakeLists.txt enumerates every file (no globbing). When you add a .cpp/.h, add "
    "it there — and to the relevant test target if it is exercised by tests."))

# ============================================================= 5 testing
h1("5. Running &amp; testing")
p("Six GUI-less QtTest suites run under ctest (option <font face='Courier'>KEYNETIK_BUILD_TESTS=ON</font>, "
  "default):")
S.append(table([
    ["Suite", "Covers"],
    ["carttotals_test", "Subtotal / tax / discount / rounding (pure math)."],
    ["database_test", "recordSale + processRefund money/stock invariants and migration persistence, against an in-memory DB through the repositories."],
    ["ledger_test", "Double-entry GL invariants."],
    ["payroll_test", "Kenyan statutory deduction maths."],
    ["vat_test", "VAT split + VAT-3 aggregation."],
    ["salejournal_test", "Balanced journal-line construction for sales/purchases/expenses."],
]))
S.append(code_block([
    'ctest --test-dir "build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release" --output-on-failure',
]))
p("CI (<font face='Courier'>.github/workflows/ci.yml</font>) builds and runs the suites on Ubuntu and "
  "does a full GUI build + ctest on Windows.")

# ============================================================= 6 architecture
h1("6. Architecture overview")
p("KeynetikPOS layers cleanly: UI talks to services and the data facade; services hold business "
  "logic; the data facade delegates to per-aggregate repositories; pure-maths modules underpin "
  "money, tax and accounting and have no DB or UI dependency.")
S.append(code_block([
    'main()  ──owns──>  Database  ──delegates──>  SaleRepository / ProductRepository /',
    '  │  injects Database&                       CustomerRepository / SupplierRepository /',
    '  v                                          ExpenseRepository / PurchaseOrderRepository /',
    'MainWindow(Database&)                        RefundRepository / SalesAnalyticsRepository',
    '  │  forwards Database&',
    '  ├─> dialogs (UI only)        ├─> CheckoutService ──> SaleRepository.recordSale (atomic)',
    '  └─> managers (InventoryManager, ScheduleManager, ...)        └─> Ledger (auto-post)',
    '',
    'Pure maths (no DB/UI): Money, computeCartTotals, buildSaleJournal, Payroll, Vat',
]))
S.append(callout("Design priorities",
    "Money correctness (exact integer cents), transactional integrity at checkout, an auditable trail "
    "for every stock/discount/refund movement, and books that reconcile (a real double-entry ledger)."))

# ============================================================= 7 DI
h1("7. Ownership &amp; dependency injection")
p("<b>There is no Database singleton.</b> Exactly one <font face='Courier'>Database</font> is "
  "constructed in <font face='Courier'>main()</font>, owned for the process lifetime, and injected by "
  "reference:")
S.append(code_block([
    '// main.cpp',
    'Database db;            // owns the one connection',
    'db.initialize();',
    'MainWindow w(db);       // injected down the tree',
]))
p("<font face='Courier'>MainWindow</font> holds a <font face='Courier'>Database&amp; m_db</font> and "
  "forwards it to every dialog/manager/service it constructs; each consumer stores its own "
  "<font face='Courier'>Database&amp; m_db</font>. This makes dependencies explicit, the data layer "
  "testable (tests own their own <font face='Courier'>Database</font>), and multiple connections "
  "possible. <font face='Courier'>UserManager</font>, <font face='Courier'>LicenseManager</font> and "
  "<font face='Courier'>ThemeManager</font> remain singletons by design.")

# ============================================================= 8 data layer
h1("8. The data layer: Money, Database, repositories")
h2("Money (money.h)")
p("All monetary values are <font face='Courier'>Money</font> — a value type wrapping "
  "<font face='Courier'>qint64</font> cents. Conversions are explicit "
  "(<font face='Courier'>fromCents</font>/<font face='Courier'>fromMajor</font>, "
  "<font face='Courier'>cents()</font>/<font face='Courier'>toMajor()</font>) so a stray "
  "<font face='Courier'>double</font> can never silently become money. Rounding happens once, at the "
  "UI/parse edge. Dates are likewise typed — <font face='Courier'>QDateTime</font>/"
  "<font face='Courier'>QDate</font> in structs, ISO strings only at the SQL boundary.")
h2("Database (database.h/.cpp) — the facade")
p("<font face='Courier'>Database</font> owns the connection and the schema lifecycle, defines the "
  "domain structs (<font face='Courier'>Product, Sale, SaleItem, SalePayment, SaleRequest, Supplier, "
  "PurchaseOrder, Expense, Customer</font>), and exposes the full data API — but holds <i>no business "
  "SQL of its own</i>. Every data method delegates to a repository and propagates "
  "<font face='Courier'>lastError()</font> on writes:")
S.append(code_block([
    'int Database::recordSale(const SaleRequest &req) {',
    '    SaleRepository repo(db);',
    '    const int id = repo.recordSale(req);',
    '    lastError = repo.lastError();',
    '    return id;',
    '}',
]))
h2("The repository layer")
p("Eight classes own the SQL for one aggregate each. Each takes a "
  "<font face='Courier'>QSqlDatabase</font> <i>by value</i> (a cheap ref-counted handle to the one "
  "open connection — same pattern as <font face='Courier'>Ledger</font>), so it shares the connection "
  "and any in-flight transaction without owning it.")
S.append(table([
    ["Repository", "Owns"],
    ["SaleRepository", "recordSale() + Sale/SaleItem queries"],
    ["ProductRepository", "catalog CRUD, stock mutators, adjustStockWithLog, history, valuation"],
    ["CustomerRepository", "customer CRUD, phone lookup, store-credit / loyalty"],
    ["SupplierRepository", "supplier CRUD (soft delete)"],
    ["ExpenseRepository", "expense categories + expenses + totals"],
    ["PurchaseOrderRepository", "PO CRUD + transactional receivePurchaseOrder"],
    ["RefundRepository", "processRefund / isRefunded"],
    ["SalesAnalyticsRepository", "takings, top sellers, payment totals, gross profit, P&amp;L"],
]))
p("Cross-aggregate operations compose on the shared connection: "
  "<font face='Courier'>RefundRepository::processRefund</font> reuses Sale + Product repositories, and "
  "<font face='Courier'>PurchaseOrderRepository::receivePurchaseOrder</font> reuses "
  "<font face='Courier'>ProductRepository</font> for the per-line audit log — all inside one "
  "transaction.")

# ============================================================= 9 migrations
h1("9. Schema &amp; migrations")
p("<font face='Courier'>initialize()</font> opens the DB (foreign keys + WAL + busy_timeout), creates "
  "tables idempotently, runs the versioned migrations, then the one-time money-to-cents conversion, "
  "and seeds sample data on a fresh DB.")
S.append(bullets([
    "<b>Versioned migrations.</b> runMigrations() applies each numbered step newer than the schema_meta 'schema_version' row, each in its own transaction, written idempotently (via ensureColumn). A fresh DB and an old DB converge on the same shape.",
    "<b>Money-to-cents</b> is a one-time data conversion kept separate and gated by its own marker row — never by schema_version, since re-running would re-scale every amount by 100.",
]))
S.append(callout("Adding a migration",
    "Append a new entry to the migrations table in runMigrations() with the next version number and an "
    "idempotent body. Do not reuse or renumber existing versions.", WARNBG, WARNBAR))

# ============================================================= 10 sale pipeline
h1("10. The sale pipeline")
p("Checkout is the system's most important transaction. The flow:")
S.append(steps([
    "<b>CartService</b> holds the cart (multi-cart state machine); <b>CartModel</b> adapts it to the table view.",
    "<b>computeCartTotals()</b> (carttotals.h, pure) derives subtotal/tax/discount/total in integer cents.",
    "<b>CheckoutService::finalizeSale()</b> builds a <font face='Courier'>SaleRequest</font> (items, totals, tender, customer, cashier) and calls <font face='Courier'>Database::recordSale()</font>.",
    "<b>SaleRepository::recordSale()</b> runs ONE transaction: re-validate stock, insert the sale, insert each line (snapshotting name/price/cost), decrement stock, accrue loyalty/credit, record split tenders. Any failure rolls back and returns -1.",
    "On success the sale is auto-posted to the General Ledger (best-effort, isolated), the receipt is printed/emailed, inventory refreshes, and the action is audit-logged.",
]))
S.append(callout("Stock is validated in the UI but ENFORCED in the transaction",
    "The UI check is fast feedback; correctness comes from re-validating inside recordSale's "
    "transaction, so two near-simultaneous sales cannot oversell. On failure the cart is preserved so "
    "the cashier can retry."))

# ============================================================= 11 accounting
h1("11. Accounting &amp; ERP tier")
p("A double-entry back office sits under the POS; all modules post into the same "
  "<font face='Courier'>QSqlDatabase</font> and use <font face='Courier'>Money</font>.")
S.append(table([
    ["Module", "Role"],
    ["Ledger", "Seeded Kenyan chart of accounts; postEntry() rejects unbalanced/empty entries; trial balance."],
    ["SaleJournal", "Pure functions building balanced GL lines for sales / purchases / expenses; posted best-effort after the underlying record commits."],
    ["Payroll", "NSSF/SHIF/Housing before PAYE; computePayslip() is pure + tested; runPayroll posts one balanced wage journal."],
    ["Vat / Etims", "VAT-3 from the transactional tables (not the GL); per-product tax codes; eTIMS client interface + logging stub."],
]))
p("Auto-posting lives in the service/dialog layer (not in database.cpp) so the data tests stay "
  "isolated. A posting failure is logged but never undoes a durably recorded sale/purchase/expense.")

# ============================================================= 12 UI
h1("12. UI architecture &amp; theming")
S.append(bullets([
    "<b>MainWindow</b> owns the subsystem objects and the dialogs; UI is built programmatically in setupUI() (no .ui files).",
    "<b>Model/View</b> — CartModel (QAbstractTableModel) and ProductGridModel (QAbstractListModel + filter proxy + painting delegate) back the cart table and product grid.",
    "<b>Theming</b> — ColorScheme is the palette; appstyle.cpp generates one app-wide stylesheet from it using dynamic-property selectors (kind/role/textScale/bold). ThemeManager is the single source of truth for light/dark; a toggle re-applies the sheet app-wide.",
    "<b>Dialogs</b> are UI-only and delegate to repositories/managers via the injected Database&.",
]))

# ============================================================= 13 security
h1("13. Security &amp; licensing")
S.append(bullets([
    "<b>Passwords</b> — PBKDF2 (passwordhasher.h); legacy SHA-256 hashes are rehashed on next login. A forced-password-change flow covers the seeded admin.",
    "<b>Secrets at rest</b> — SecretStore uses Windows DPAPI (CryptProtectData) to encrypt the SMTP password; a 'dpapi:v1:' marker tags encrypted values and legacy plaintext is migrated on load.",
    "<b>Licensing</b> — LicenseManager activates online-first with a local fallback so offline shops are never locked out; the Cloudflare Worker + D1 server (license-server/) is the authority. Client-side key checks are a format gate, not security.",
    "<b>RBAC</b> — UserManager enforces role-based permissions; the UI disables disallowed actions and the rule is also checked in the business layer.",
]))

# ============================================================= 14 conventions
h1("14. Coding conventions")
S.append(bullets([
    "Every file opens with a WHAT / HOW / WHY header comment. Keep it current when behaviour changes.",
    "Classes PascalCase; functions camelCase; private members m_ prefixed.",
    "Money is always the Money type; dates are QDateTime/QDate in structs, ISO strings only at SQL bind.",
    "DB writes return bool/int with getLastError(); multi-part outcomes use result structs (e.g. CheckoutResult). Exceptions are not used (Qt-traditional).",
    "Repositories take QSqlDatabase by value; injected Database& is stored as m_db.",
    "Atomic operations wrap check + write in one transaction on the shared connection.",
]))

# ============================================================= 15 extension
h1("15. Extension recipes")
h3("Add a new repository method")
S.append(steps([
    "Add the method to the relevant XRepository (.h + .cpp), using m_db and m_lastError.",
    "Add a thin delegating method on Database that forwards and copies lastError() on writes.",
    "If tests exercise it, the repo is already linked into database_test; add a case there.",
]))
h3("Add a schema migration")
S.append(steps([
    "Append { N+1, \"desc\", [this]{ return ensureColumn(...); } } to runMigrations().",
    "Write the body idempotently; never renumber or reuse a version.",
    "Add a database_test case asserting the new column/round-trip if it carries data.",
]))
h3("Add a dialog")
S.append(steps([
    "Give the constructor a Database&amp; first parameter; store Database&amp; m_db (declare it first, init first).",
    "Build the UI in setupUI(); use property-based styling (kind/role/textScale) rather than inline colours.",
    "Construct it from MainWindow passing m_db; add the files to PROJECT_SOURCES.",
]))

# ============================================================= 16 gaps
h1("16. Known gaps &amp; technical debt")
S.append(table([
    ["Item", "Notes"],
    ["Shift not wired to checkout", "ShiftManager / ZReportDialog exist but aren't on the live sale path; Sale.shiftId is forward-compatible plumbing (always 0 today)."],
    ["Error-handling conventions", "Three styles coexist (bool+getLastError, int -1, result structs). A single Result<T> would unify them."],
    ["Product.profitMargin is double", "The one double in a domain struct; basis-points (int) would keep the struct float-free."],
    ["Inventory auto-refresh", "InventoryManager polls on a timer; a single-process app could make this event-driven."],
    ["eTIMS", "Only a logging stub; real KRA OSCU/VSCU integration needs device onboarding."],
]))
sp(6)
S.append(Paragraph("KeynetikPOS Technical Manual &bull; Version 2.0 &bull; Pairs with "
                   "CODE_DOCUMENTATION.md and the in-source WHAT/HOW/WHY headers.", SMALL))

# ============================================================= build
Doc(OUT, "Technical Manual", "Architecture & developer guide").build(S)
print("wrote", OUT)
