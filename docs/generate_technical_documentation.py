# -*- coding: utf-8 -*-
"""Renders the KeynetikPOS technical documentation to PDF — architecture, data
layer, domain subsystems, integrations, build and packaging. Shares the house
style with the other manuals via _pdf_common.

Content is grounded in the source as of 19 July 2026 (branch feature/erp-tier4);
every structural claim was checked against the files cited.

Run:  python docs/generate_technical_documentation.py
Out:  docs/KeynetikPOS_Technical_Documentation.pdf
"""
import os
from reportlab.lib.units import mm
from reportlab.platypus import Paragraph, Spacer, PageBreak, HRFlowable

from _pdf_common import (
    Doc, cover, toc, bullets, steps, table, callout, code_block,
    H1, H2, H3, BODY, SMALL, LIGHT, BLUE, WARNBG, WARNBAR, ACCENT, LINE,
)

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "KeynetikPOS_Technical_Documentation.pdf")

S = []


def h1(t):
    S.append(Paragraph(t, H1))
    S.append(HRFlowable(width="100%", thickness=1, color=LINE,
                        spaceBefore=1, spaceAfter=7))


def h2(t):
    S.append(Paragraph(t, H2))


def h3(t):
    S.append(Paragraph(t, H3))


def p(t):
    S.append(Paragraph(t, BODY))


def sp(h=4):
    S.append(Spacer(1, h * mm))


# ───────────────────────────────────────────────────────── cover
S += cover(
    "Technical Documentation",
    "Architecture, data layer, domain subsystems and build",
    ["Version 2.1.0 &nbsp;|&nbsp; Branch: feature/erp-tier4",
     "Qt 6.11 &nbsp;·&nbsp; C++17 &nbsp;·&nbsp; SQLite &nbsp;·&nbsp; Cloudflare Workers",
     "Current as of 19 July 2026"],
)

# ───────────────────────────────────────────────────────── TOC
h1("Contents")
S.append(toc([
    ("1",  "System overview"),
    ("2",  "Toolchain and build"),
    ("3",  "Architecture and ownership"),
    ("4",  "The data layer"),
    ("5",  "Schema and migrations"),
    ("6",  "Money as a value type"),
    ("7",  "The sale pipeline"),
    ("8",  "Finance and accounting (Tier 4)"),
    ("9",  "M-Pesa payment integration"),
    ("10", "Licensing and tiers"),
    ("11", "Theming and the UI system"),
    ("12", "Testing"),
    ("13", "Packaging and deployment"),
    ("14", "Known gaps and technical debt"),
]))
S.append(PageBreak())

# ───────────────────────────────────────────────────────── 1
h1("1 &nbsp; System overview")
p("KeynetikPOS is a Windows desktop point-of-sale and light-ERP application "
  "for the Kenyan retail market. It is a single-process Qt Widgets application "
  "over a local SQLite database, paired with a Cloudflare Worker that handles "
  "licensing and brokers M-Pesa payments.")
sp(2)
S.append(table([
    ["Property", "Value"],
    ["Language / standard", "C++17"],
    ["UI framework", "Qt 6.11 Widgets"],
    ["Persistence", "SQLite via Qt SQL, WAL journal mode"],
    ["Backend", "Cloudflare Worker + D1 (licensing, M-Pesa brokering)"],
    ["Source size", "~28,700 lines across 66 .cpp and 74 .h files"],
    ["Test suites", "7 (CTest)"],
    ["Target market", "Kenya — KSh currency, 16% VAT, M-Pesa, KRA eTIMS"],
]))
sp(3)

h2("1.1 &nbsp; Functional scope")
S.append(bullets([
    "<b>Point of sale</b> — product grid, cart, split-tender checkout, "
    "discounts, refunds, receipt printing and emailing, barcode scanning.",
    "<b>Inventory</b> — catalogue CRUD, per-product reorder levels, stock "
    "adjustment audit trail, stock-take, low-stock alerting, valuation.",
    "<b>Purchasing</b> — suppliers, purchase orders, transactional goods "
    "receipt that updates stock and cost.",
    "<b>Customers</b> — store credit, loyalty points, purchase history.",
    "<b>Reporting</b> — analytics dashboard, detailed and accountant reports, "
    "Z-reports, profit and loss.",
    "<b>Finance (Tier 4)</b> — general ledger, VAT returns, payroll with "
    "Kenyan statutory deductions, automatic journal posting.",
]))
sp(3)

h2("1.2 &nbsp; Design principles visible in the code")
S.append(bullets([
    "<b>Money is never a float.</b> A dedicated <b>Money</b> value type wraps "
    "integer cents; the compiler flags every boundary (section 6).",
    "<b>Sales are atomic.</b> Stock validation and decrement occur in the same "
    "transaction as the sale insert, so a sale cannot rest on a stock count "
    "that changed underneath it (section 7).",
    "<b>History is snapshotted.</b> Sale line items copy name, price and cost "
    "at sale time, because products are later renamed, repriced and retired.",
    "<b>Offline shops must not be locked out.</b> Licence activation is "
    "online-first with a local fallback (section 10).",
    "<b>Secrets stay server-side.</b> The till never holds Daraja credentials "
    "(section 9).",
]))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 2
h1("2 &nbsp; Toolchain and build")
h2("2.1 &nbsp; Environment")
S.append(table([
    ["Component", "Version / location"],
    ["Qt", "6.11.0, MinGW 64-bit — C:\\Qt\\6.11.0\\mingw_64"],
    ["Compiler", "MinGW 13.1.0 — C:\\Qt\\Tools\\mingw1310_64\\bin"],
    ["Build system", "CMake, Qt Creator generated trees under build/"],
    ["Installer", "NSIS — C:\\Program Files (x86)\\NSIS"],
    ["Qt modules", "Widgets, Core, Sql, PrintSupport, Network, Concurrent, "
                   "SerialPort"],
    ["Windows libs", "crypt32 (DPAPI, for credential encryption)"],
]))
sp(3)

h2("2.2 &nbsp; Building")
S.append(code_block([
    "$env:PATH = \"C:\\Qt\\Tools\\mingw1310_64\\bin;C:\\Qt\\6.11.0\\mingw_64\\bin;\" + $env:PATH",
    "cmake --build \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\"",
    "ctest --test-dir \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\" --output-on-failure",
]))
sp(2)
S.append(callout(
    "Changing the project version forces a full rebuild",
    "<b>project(KeynetikPOS VERSION ...)</b> participates in CMake "
    "configuration, so bumping it reconfigures and recompiles the entire tree "
    "rather than building incrementally. Budget several minutes.",
    LIGHT, BLUE))
sp(3)

h2("2.3 &nbsp; Compiler warnings")
p("Warnings are enabled but not fatal — <b>-Wall -Wextra</b> on GCC, "
  "<b>/W4 /utf-8</b> on MSVC. <b>-Werror</b> is deliberately not set because "
  "pre-existing warnings remain. The MSVC <b>/utf-8</b> flag is required: the "
  "sources are UTF-8 without BOM, and without it MSVC reads string literals in "
  "the system code page and corrupts non-ASCII text.")

sp(4)

# ───────────────────────────────────────────────────────── 3
h1("3 &nbsp; Architecture and ownership")
h2("3.1 &nbsp; Layers")
S.append(table([
    ["Layer", "Responsibility", "Representative files"],
    ["Presentation",
     "Qt Widgets dialogs and models. Holds no business rules beyond input "
     "validation.",
     "mainwindow, paymentdialog, inventorydialog, cartmodel, productgridmodel"],
    ["Services",
     "Orchestration across repositories and UI-independent workflows.",
     "checkoutservice, cartservice, inventorymanager, schedulemanager"],
    ["Domain",
     "Pure, testable computation with no I/O.",
     "money, carttotals, salejournal, payroll, vat, discount"],
    ["Data access",
     "Eight repositories, each owning the SQL for one aggregate.",
     "productrepository, salerepository, customerrepository, …"],
    ["Infrastructure",
     "Connection lifecycle, schema, migrations, backup, integrity.",
     "database"],
    ["Integrations",
     "Outbound network and hardware.",
     "mpesaclient, licensemanager, smtpclient, barcodereader, etims"],
]))
sp(3)

h2("3.2 &nbsp; The pure-function core")
p("The most valuable design decision in the codebase is that the calculations "
  "which must be correct are pure functions taking values and returning "
  "values, with no database or widget dependency. <b>computeCartTotals()</b>, "
  "<b>computePayslip()</b>, <b>splitInclusive()</b> and the "
  "<b>build*Journal()</b> family are all of this shape. This is why they carry "
  "genuine unit tests while the UI does not — the tests exercise the logic "
  "directly rather than through a widget hierarchy.")
sp(3)

h2("3.3 &nbsp; Ownership and dependency injection")
p("<b>Database is not a singleton.</b> <b>Database::instance()</b> was removed "
  "and there are now zero references to it in the codebase. <b>main()</b> "
  "constructs one <b>Database db;</b> for the process and injects it as a "
  "reference into <b>MainWindow</b>, which forwards it to every dialog, "
  "manager and service it constructs. Each consumer stores a "
  "<b>Database&nbsp;&amp;m_db</b> member.")
sp(1)
S.append(code_block([
    "Database db;                    // main.cpp — the app owns the connection",
    "if (!db.initialize()) { /* fatal */ }",
    "db.backupIfDue();",
    "MainWindow w(db);               // injected by reference",
]))
sp(2)
p("This exists so tests can supply an isolated in-memory database via "
  "<b>configureForTesting()</b> rather than sharing process-global state.")
sp(2)
S.append(callout(
    "Three singletons are retained deliberately",
    "<b>UserManager</b>, <b>LicenseManager</b> and <b>ThemeManager</b> remain "
    "singletons because they model genuinely process-global state — one "
    "operator session, one licence, one theme. Unlike the database, they have "
    "no per-test isolation requirement. Blanket removal of the remaining "
    "instance() call sites was assessed as low return for high risk.",
    LIGHT, BLUE))
sp(2)
p("Where a service would otherwise reach for a singleton, the dependency is "
  "passed instead. <b>CheckoutService</b> takes an <b>OperatorContext</b> "
  "— a plain struct of username, full name and a logging callback — so it can "
  "be exercised with no user session at all.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 4
h1("4 &nbsp; The data layer")
p("<b>Database</b> exposes eight lazy accessors returning references to "
  "repository instances it owns. Consumers call through the accessor and read "
  "errors from the same accessor.")
sp(2)
S.append(code_block([
    "if (!m_db.customers().addCustomer(c))",
    "    show(m_db.customers().lastError());",
    "",
    "const auto items = m_db.products().getAllProducts();",
]))
sp(2)
p("Each accessor returns a reference to a persistent member — never a "
  "temporary — so an error read immediately after a failed write is still "
  "valid.")
sp(2)
S.append(table([
    ["Repository", "Owns"],
    ["ProductRepository",
     "Catalogue CRUD, stock mutators, adjustStockWithLog, stock history, "
     "valuation."],
    ["SaleRepository",
     "recordSale plus all sale reads — getAllSales, date ranges, line items, "
     "customer purchase history."],
    ["SalesAnalyticsRepository",
     "Aggregate reads — totals by day and month, top sellers, payment totals "
     "by method, gross profit, profit and loss."],
    ["RefundRepository",
     "processRefund and isRefunded. Cross-table and transactional; "
     "orchestrates SaleRepository and ProductRepository on the shared "
     "connection."],
    ["SupplierRepository", "Supplier CRUD and deactivation."],
    ["PurchaseOrderRepository",
     "PO CRUD and the transactional receivePurchaseOrder, which updates stock "
     "and cost and writes per-line audit rows."],
    ["ExpenseRepository", "Expense categories, expenses, totals."],
    ["CustomerRepository",
     "Customer CRUD, store credit adjustment, loyalty point redemption."],
]))
sp(3)
S.append(callout(
    "Accessors cache a copy of the connection handle",
    "<b>configureForTesting()</b> swaps the underlying connection, so it calls "
    "<b>resetRepositories()</b> before removing the old database. Without "
    "that, the second test in a run would use a dangling handle and crash "
    "— a failure mode that presented as the first test always passing when "
    "run alone.", WARNBG, WARNBAR))
sp(2)
p("<b>Database</b> itself now holds only connection lifecycle, schema creation "
  "and migration, sample-data seeding, backup and integrity checking. "
  "<b>getLastError()</b> survives on it for those lifecycle paths alone.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 5
h1("5 &nbsp; Schema and migrations")
h2("5.1 &nbsp; Tables")
p("The database file lives in the per-user AppData directory, keeping it out "
  "of read-only Program Files. Foreign keys are enabled, and "
  "<b>initialize()</b> sets <b>journal_mode=WAL</b> with a 5-second busy "
  "timeout.")
sp(2)
S.append(table([
    ["Area", "Tables"],
    ["Catalogue &amp; stock",
     "products, categories, stock_adjustments"],
    ["Sales",
     "sales, sale_items, sale_payments, refunds"],
    ["Parties",
     "customers, suppliers"],
    ["Purchasing",
     "purchase_orders, purchase_order_items"],
    ["Expenses",
     "expenses, expense_categories"],
    ["Ledger",
     "gl_accounts, gl_entries, gl_lines"],
    ["Payroll",
     "employees, payslips, pay_runs, payroll_config"],
    ["VAT", "vat_config"],
    ["Operations",
     "users, user_activity_log, till_reconciliations, message_schedules"],
    ["Meta", "schema_meta"],
]))
sp(3)

h2("5.2 &nbsp; Versioned migrations")
p("Schema evolution runs through a numbered migration runner. The version is "
  "stored as a row in <b>schema_meta</b>, and each step above the current "
  "version applies inside its own transaction so the DDL and the version bump "
  "commit together. Steps are written idempotently, so fresh and old databases "
  "converge on the same shape.")
sp(2)
S.append(callout(
    "Never gate a migration on PRAGMA user_version",
    "An earlier implementation tracked completion with <b>PRAGMA "
    "user_version</b>, which does <b>not</b> persist across close and reopen "
    "under WAL via the Qt SQLite driver. The value read back as zero on every "
    "launch, so the money migration re-ran each time and re-scaled every "
    "monetary value by 100 — silent, compounding data corruption. The fix was "
    "a marker row in a real table, written in the same transaction as the data "
    "change. A regression test pins this behaviour using a real file database "
    "rather than an in-memory one.", WARNBG, WARNBAR))
sp(2)
p("The money migration remains deliberately separate from the numbered "
  "sequence, gated by its own marker. Version-gating it would cause a legacy "
  "database reporting version zero to be re-scaled.")

sp(4)

# ───────────────────────────────────────────────────────── 6
h1("6 &nbsp; Money as a value type")
p("Monetary amounts are a strong type wrapping <b>qint64</b> cents, not "
  "doubles.")
sp(1)
S.append(code_block([
    "class Money {",
    "  static constexpr Money fromCents(qint64 cents);",
    "  static Money fromMajor(double major);",
    "  constexpr qint64 cents()   const;",
    "  constexpr double toMajor() const;   // cents / 100.0",
    "  explicit constexpr Money(qint64 cents);   // private",
    "};",
]))
sp(2)
p("Conversions are explicit in both directions. Because there is no implicit "
  "conversion to or from <b>double</b>, the compiler flags every boundary "
  "where a raw number meets a monetary value — which is precisely how the "
  "original migration was carried out safely across the domain structs, the "
  "database columns and the reporting code.")
sp(2)
S.append(bullets([
    "Domain structs carry <b>Money</b>: product price and cost, all sale and "
    "line-item totals, cart discount, receipt fields, analytics metrics.",
    "Database money columns are <b>INTEGER</b>.",
    "Cart totals are computed entirely in integers — tax is a single "
    "<b>llround</b> of base times rate.",
    "UI dialogs keep <b>QDoubleSpinBox</b> internally and convert only at the "
    "getter boundary.",
]))
sp(2)
S.append(callout(
    "Why this was worth the migration",
    "A characterisation test documents the original defect: with doubles, "
    "rounding 1.005 yielded 1.00 while 10.005 yielded 10.01 — the same "
    "half-cent input rounding in opposite directions depending on magnitude. "
    "Cash-drawer float amounts in the shift and payment managers remain "
    "doubles; that is a separate concept from the sales money path and is "
    "intentional.", LIGHT, BLUE))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 7
h1("7 &nbsp; The sale pipeline")
h2("7.1 &nbsp; Flow")
S.append(steps([
    "The cashier builds a cart; <b>CartService</b> holds the state and "
    "<b>CartModel</b> presents it, clamping quantity edits to available stock.",
    "<b>computeCartTotals()</b> derives subtotal, discount, tax and total from "
    "the cart and the business settings. Pure and unit-tested.",
    "<b>PaymentDialog</b> collects tenders — cash, card, mobile money, store "
    "credit, loyalty redemption — and refuses to confirm until the entered "
    "amount covers the total.",
    "<b>CheckoutService::finalizeSale()</b> builds a <b>SaleRequest</b> and "
    "calls <b>recordSale()</b>.",
    "<b>recordSale()</b> re-validates stock, inserts the sale header and line "
    "items, and decrements stock — all inside one transaction.",
    "On success the sale is posted to the general ledger, the receipt is "
    "printed if a printer is attached, and the operator action is logged.",
]))
sp(3)

h2("7.2 &nbsp; Transactional guarantees")
S.append(callout(
    "Validation and mutation share a transaction",
    "Stock is re-checked inside the same transaction that decrements it. Any "
    "failure — including insufficient stock on a single line — rolls the whole "
    "sale back, leaving stock untouched. This is directly covered by "
    "database_test and checkoutservice_test.", LIGHT, ACCENT))
sp(2)
p("Refunds are guarded symmetrically: <b>processRefund</b> checks "
  "<b>isRefunded()</b> <i>inside</i> the transaction, closing a window in "
  "which a sale could be refunded twice.")
sp(3)

h2("7.3 &nbsp; Tenders and split payment")
p("A sale carries a vector of <b>SalePayment</b> entries of method, amount and "
  "reference. Cash absorbs change, so its recorded contribution is the "
  "remainder after the exact tenders rather than the amount handed over. Card "
  "and mobile-money tenders carry their reference — for M-Pesa, the ten-"
  "character receipt code, which is validated before confirm is enabled.")
sp(2)

h2("7.4 &nbsp; Audit context")
p("<b>Sale</b> carries <b>cashier</b> and <b>shiftId</b>. The cashier is "
  "populated from the injected <b>OperatorContext</b>. The shift id is "
  "plumbing only and is always zero: <b>ShiftManager</b> and "
  "<b>ZReportDialog</b> exist but are not wired into the main window, so there "
  "is no live current-shift source. The column and field exist for when one "
  "is wired in.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 8
h1("8 &nbsp; Finance and accounting (Tier 4)")
p("The finance subsystem is gated behind licence tier 4 and reached from the "
  "Finance menu.")
sp(2)

h2("8.1 &nbsp; General ledger")
p("<b>Ledger</b> takes a <b>QSqlDatabase</b> by value and seeds a Kenyan chart "
  "of accounts on first initialisation. <b>postEntry()</b> enforces balanced "
  "debits and credits within a single transaction; an unbalanced entry cannot "
  "be written.")
sp(2)
S.append(table([
    ["Range", "Accounts"],
    ["Assets (1xxx)",
     "1000 Cash on Hand · 1010 M-Pesa / Mobile Money · 1020 Bank · "
     "1100 Accounts Receivable · 1200 Inventory · 1300 VAT Input"],
    ["Liabilities (2xxx)",
     "2000 Accounts Payable · 2100 VAT Output · 2200 PAYE · 2210 SHIF · "
     "2220 NSSF · 2230 Housing Levy · 2300 Net Pay"],
    ["Equity (3xxx)", "3000 Owner's Equity · 3100 Retained Earnings"],
    ["Revenue (4xxx)", "4000 Sales Revenue"],
    ["Cost (5xxx)", "5000 Cost of Goods Sold"],
    ["Expenses (6xxx)",
     "6000 Wages &amp; Salaries · 6100 Rent · 6200 Utilities · 6300 Other"],
]))
sp(3)

h2("8.2 &nbsp; Automatic journal posting")
p("Three pure builder functions in <b>salejournal</b> return balanced journal "
  "lines. Being pure, they are directly unit-tested.")
sp(2)
S.append(table([
    ["Builder", "Posting"],
    ["buildSaleJournal",
     "Debit tender accounts (cash 1000 / M-Pesa 1010 / card and bank 1020, "
     "resolved by saleTenderAccount) with non-cash settlement to AR 1100; "
     "credit 4000 Sales at net and 2100 VAT Output. Adds a "
     "debit 5000 COGS / credit 1200 Inventory pair when cost is known."],
    ["buildPurchaseJournal",
     "Debit 1200 Inventory at net and 1300 VAT Input; credit 2000 Accounts "
     "Payable at gross."],
    ["buildExpenseJournal",
     "Debit the mapped expense account (via expenseAccount); credit "
     "1000 Cash."],
]))
sp(2)
p("Posting is invoked after the underlying transaction succeeds — sales from "
  "<b>CheckoutService</b>, purchases on goods receipt, expenses on entry. All "
  "three are <b>best-effort</b>: a posting failure is logged but never undoes "
  "the business transaction.")
sp(2)
S.append(callout(
    "Posting lives in the UI layer for purchases and expenses",
    "This keeps database_test isolated from ledger concerns, but it does mean "
    "the posting is skipped if those paths are ever driven "
    "programmatically rather than through their dialogs. Accounts receivable "
    "and payable accrue correctly, but settling them is still a manual "
    "journal.", WARNBG, WARNBAR))
sp(3)

h2("8.3 &nbsp; VAT")
p("Products carry a <b>tax_code</b> of Standard, Zero or Exempt. "
  "<b>splitInclusive()</b> backs VAT out of tax-inclusive prices using "
  "<i>vat = gross × r / (1 + r)</i>. <b>computeVat3()</b> aggregates output VAT "
  "from sale items and input VAT from received purchase-order lines.")
sp(2)
S.append(callout(
    "Transactional tables are the VAT source of truth",
    "The VAT-3 return is computed from sales and purchase-order tables, not "
    "from the general ledger. This is deliberate: the return must reflect what "
    "was actually transacted, independently of whether best-effort GL posting "
    "succeeded.", LIGHT, BLUE))
sp(2)
p("The VAT rate is <b>read-only</b> in the VAT dialog. It is sourced from "
  "Settings &rarr; Tax and mirrored into <b>vat_config</b> at startup and on "
  "every settings change, so VAT calculations always use the rate customers "
  "are actually charged. This single-source arrangement fixed a latent "
  "divergence between the two.")
sp(2)
p("<b>eTIMS</b> is defined as an interface with a stub implementation that "
  "logs and reports not-onboarded. Real KRA OSCU/VSCU integration requires "
  "device onboarding and is not implemented.")
sp(3)

h2("8.4 &nbsp; Payroll")
p("<b>computePayslip(gross, rates)</b> is pure and tested. It implements the "
  "post-2024 Kenyan treatment in which NSSF, SHIF and the Housing Levy are "
  "deducted <b>before</b> PAYE is assessed.")
sp(2)
S.append(table([
    ["Deduction", "Basis"],
    ["NSSF", "6%, capped at 36,000 pensionable — maximum 2,160"],
    ["SHIF", "2.75%, minimum 300"],
    ["Housing Levy", "1.5%"],
    ["PAYE",
     "Progressive bands 10 / 25 / 30 / 32.5 / 35%, less personal relief of "
     "2,400"],
]))
sp(2)
p("A pay run stores payslips and posts one balanced journal entry: debit "
  "6000 Wages; credit 2300 Net Pay, 2200 PAYE, 2210 SHIF, 2220 NSSF and "
  "2230 Housing Levy. Rates persist in <b>payroll_config</b> and are editable.")
sp(2)
S.append(callout(
    "Statutory rates require verification",
    "The rates encoded here reflect the position at the time of "
    "implementation. Kenyan statutory rates change with each Finance Act — "
    "verify against the current Act before relying on payroll output. "
    "See docs/STATUTORY_RATES.md.", WARNBG, WARNBAR))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 9
h1("9 &nbsp; M-Pesa payment integration")
h2("9.1 &nbsp; Why a broker exists")
p("A desktop till sits behind NAT and cannot receive Safaricom's callback. The "
  "public Cloudflare Worker is therefore the rendezvous point. Routing through "
  "it also keeps Daraja credentials server-side rather than on every shop PC.")
sp(2)

h2("9.2 &nbsp; Flow")
S.append(steps([
    "The cashier enters the mobile-money amount and the customer's phone "
    "number, then presses <i>Send STK Prompt</i>.",
    "<b>MpesaClient::requestPayment()</b> POSTs to <b>/pos/mpesa/stkpush</b>, "
    "authenticated with the till's licence key and device id — the same pair "
    "used for licence validation.",
    "The Worker obtains a Daraja token and initiates the STK push, storing the "
    "request in <b>stk_requests</b> and returning the CheckoutRequestID.",
    "Safaricom prompts the customer, who enters their M-Pesa PIN.",
    "Daraja delivers the outcome to <b>/pos/mpesa/callback</b>, which updates "
    "the row's status, receipt and result description.",
    "The till polls <b>/pos/mpesa/status</b> every 3&nbsp;s, up to 25 times "
    "(75&nbsp;s — deliberately longer than the prompt's own ~60&nbsp;s life).",
    "On success the receipt code populates the reference field and the sale "
    "can be confirmed; on failure or timeout the cashier may enter the code "
    "manually.",
]))
sp(3)

h2("9.3 &nbsp; Why polling rather than push")
p("Polling is a deliberate choice, not a shortcut. The worst case is three "
  "seconds of latency on a transaction where the customer is physically "
  "present entering a PIN, which itself takes 15–30 seconds. A persistent "
  "connection would buy nothing a cashier can perceive, while adding a socket "
  "that breaks on unreliable shop connectivity and needs reconnection logic. "
  "Polling is stateless and self-healing.")
sp(3)

h2("9.4 &nbsp; Paybill versus Buy Goods")
p("Daraja treats these differently, and the distinction is easy to get wrong.")
sp(1)
S.append(table([
    ["Transaction type", "BusinessShortCode", "PartyB"],
    ["Paybill", "Paybill number", "The same number"],
    ["Buy Goods (till)", "Head Office / store number", "The <b>till</b> number"],
]))
sp(2)
p("<b>BusinessShortCode</b> is also what the <b>Password</b> hash is built "
  "from, so it must be the number the passkey was issued against — for Buy "
  "Goods that is the Head Office number, never the till. Set "
  "<b>MPESA_TILL_NUMBER</b> for Buy Goods; leaving it unset falls back to "
  "<b>MPESA_SHORTCODE</b>, which is correct for Paybill.")
sp(2)
S.append(callout(
    "Sandbox will not catch a wrong pairing",
    "Sandbox does not enforce the relationship between the Head Office number "
    "and the till, so a Buy Goods configuration can return "
    "<b>ResponseCode: 0</b> in sandbox and still fail in production. Confirm "
    "with a small live transaction at go-live.", WARNBG, WARNBAR))
sp(3)

h2("9.5 &nbsp; Worker configuration")
S.append(table([
    ["Variable", "Purpose"],
    ["MPESA_CONSUMER_KEY / _SECRET", "Daraja application credentials (secrets)."],
    ["MPESA_SHORTCODE",
     "The shortcode the passkey belongs to. Paybill number, or the Head "
     "Office number for Buy Goods."],
    ["MPESA_PASSKEY", "Daraja Lipa-na-M-Pesa passkey (secret)."],
    ["MPESA_ENV", "sandbox (default) or production."],
    ["MPESA_TXN_TYPE", "paybill (default) or buygoods."],
    ["MPESA_TILL_NUMBER", "Buy Goods only — sent as PartyB."],
]))
sp(2)
S.append(callout(
    "The C2B webhook is not the POS path",
    "<b>/webhook/mpesa</b> handles licence purchases and assigns licence keys. "
    "POS sales travel exclusively through the STK path above. The two must not "
    "be confused.", LIGHT, BLUE))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 10
h1("10 &nbsp; Licensing and tiers")
h2("10.1 &nbsp; Activation")
p("Activation is online-first: the client calls the Worker and falls back to "
  "local activation when the server is unreachable. This is deliberate — "
  "offline shops must never be locked out of their own till. Local activations "
  "are never heartbeat-checked.")
sp(2)
S.append(table([
    ["Tier", "Capability"],
    ["1", "POS Core"],
    ["2", "Adds inventory and customer depth"],
    ["3", "Reports, users, multi-till, messaging"],
    ["4", "Full ERP — general ledger, VAT, payroll"],
]))
sp(2)
p("The server returns the tier on both activation and validation; the client "
  "applies it and caches it. During trial the effective tier is 4 so that "
  "everything can be evaluated. A full licence with no server-supplied tier "
  "defaults to 1 — the safe minimum. Feature gates call "
  "<b>checkLicenseTier()</b> at the menu-action level.")
sp(3)
S.append(callout(
    "The licence check is a gate, not a security boundary",
    "Client-side key validation and the tamper salt are obfuscation and "
    "format-checking. The server is the authority. This is documented honestly "
    "in the source rather than overstated. Note also that changing the tamper "
    "salt or the registry-hash formula after shipping will brick legitimate "
    "installations and requires a migration.", WARNBG, WARNBAR))
sp(3)

h2("10.2 &nbsp; Credential storage")
p("The SMTP password is encrypted at rest with Windows DPAPI "
  "(<b>CryptProtectData</b>, linked via crypt32) behind a <b>dpapi:v1:</b> "
  "marker. Settings load decrypts and migrates legacy plaintext values; save "
  "always encrypts.")

sp(4)

# ───────────────────────────────────────────────────────── 11
h1("11 &nbsp; Theming and the UI system")
p("<b>colorscheme.h</b> is the single palette — base colours plus soft "
  "semantic surfaces for success, warning, error and info. "
  "<b>appstyle.cpp</b> generates one application-wide stylesheet from it, "
  "applied via <b>qApp-&gt;setStyleSheet</b> so that parentless dialogs such as "
  "the login window are themed too.")
sp(2)
p("Styling is driven by dynamic properties rather than per-widget stylesheets:")
S.append(table([
    ["Property", "Values"],
    ["kind",
     "primary / danger / warning / info / secondary / tertiary — on buttons, "
     "labels and group boxes"],
    ["role",
     "amount / amountTotal / amountDiscount / totalTitle / banner / chip / "
     "statValue / statValueLg / dialogTitle / sectionTitle / hline"],
    ["textScale", "sm / md / lg / xl / 2xl — theme-independent sizing"],
    ["bold", "true"],
]))
sp(2)
p("<b>setStyleProperty()</b> performs the unpolish/polish cycle needed for a "
  "runtime property change to repaint. Theme switching goes through "
  "<b>ThemeManager::setDark()</b> as the single source of truth; views that "
  "derive cell brushes re-run their load to pick up new colours.")
sp(2)
S.append(callout(
    "Deliberate exceptions to the theme",
    "A handful of surfaces intentionally keep fixed colours: the Z-report's "
    "printable HTML (light is correct for print), the barcode preview canvas "
    "(white is required for scannability), and the WhatsApp brand-green test "
    "button.", LIGHT, BLUE))
sp(3)

h2("11.1 &nbsp; Accessibility and touch")
S.append(bullets([
    "High-DPI rounding policy is set to <b>PassThrough</b> before "
    "<b>QApplication</b> construction; card sizing is font-metric relative "
    "rather than fixed pixels.",
    "Stock severity is conveyed as <b>text</b> as well as colour "
    "(&ldquo;CRITICAL — 3 left&rdquo;, &ldquo;OUT OF STOCK&rdquo;), not colour "
    "alone.",
    "The product grid is keyboard navigable — arrow keys move, Enter adds, "
    "out-of-stock cards are skipped.",
    "The cart has tap-stepper columns flanking quantity, with 46&nbsp;px rows "
    "for larger touch targets.",
]))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 12
h1("12 &nbsp; Testing")
p("Seven CTest suites, all currently passing. The suite is built when "
  "<b>KEYNETIK_BUILD_TESTS=ON</b>.")
sp(2)
S.append(table([
    ["Suite", "Covers"],
    ["carttotals_test",
     "Tax, discount and rounding across 15 cases — including the "
     "characterisation test that documented the original double-rounding "
     "defect."],
    ["database_test",
     "recordSale stock decrement, empty-item rejection, all-or-nothing "
     "rollback on insufficient stock, refund stock restoration, the "
     "double-refund guard, and money-migration persistence across reopen."],
    ["ledger_test", "Balanced posting, account balances, trial balance."],
    ["payroll_test",
     "Statutory computation across five cases, including an exact 50,000 "
     "gross breakdown."],
    ["vat_test", "Inclusive VAT splitting and VAT-3 aggregation."],
    ["salejournal_test",
     "11 cases across the sale, purchase and expense journal builders."],
    ["checkoutservice_test",
     "finalizeSale with an injected operator and no session — cashier "
     "attribution, atomic stock decrement, audit callback, and "
     "insufficient-stock failure preserving stock."],
]))
sp(3)

h2("12.1 &nbsp; What makes these tests possible")
p("Two decisions carry the suite. First, the calculations live in pure "
  "functions, so they can be tested without a database or a widget. Second, "
  "<b>Database::configureForTesting()</b> repoints the connection at a private "
  "in-memory database and <b>initialize(false)</b> skips sample-data seeding, "
  "so each test starts from a known empty schema.")
sp(2)
p("<b>checkoutservice_test</b> runs GUI-less by passing a null "
  "<b>ReceiptPrinter</b>; <b>finalizeSale</b> tolerates this deliberately, on "
  "the reasoning that a sale is already durably recorded and a terminal "
  "without a printer should not crash checkout. <b>ReceiptPrinter</b> is still "
  "linked because the symbol is referenced.")
sp(2)
S.append(callout(
    "Two operational notes",
    "Tests seed a category before inserting products, because "
    "<b>products.category</b> is an enforced foreign key. And QtTest output is "
    "swallowed by a PowerShell pipe — run the executable with "
    "<b>-o file,txt</b> when invoking a suite directly rather than through "
    "ctest.", LIGHT, BLUE))
sp(3)

h2("12.2 &nbsp; What is not tested")
p("The UI layer has no automated coverage. Converting the widgets to .ui files "
  "was assessed and recommended against — it is a large mechanical change with "
  "no functional benefit that does not improve testability, because the logic "
  "would remain welded to the widgets. The real fix is extracting view models, "
  "which is a separate and larger initiative.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 13
h1("13 &nbsp; Packaging and deployment")
h2("13.1 &nbsp; The desktop installer")
p("A POST_BUILD step runs <b>windeployqt</b> into <b>windeployqt_stage/</b>, "
  "collecting Qt DLLs, platform plugins and SQL drivers beside a copy of the "
  "executable. <b>install()</b> ships that directory; CPack's NSIS generator "
  "wraps it with Start Menu and desktop shortcuts, matching uninstaller "
  "entries, the application icon and a run-on-finish option.")
sp(2)
S.append(code_block([
    "$env:PATH = \"C:\\Program Files (x86)\\NSIS;\" + $env:PATH",
    "cpack --config \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\\CPackConfig.cmake\" \\",
    "      -B \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\"",
]))
sp(2)
p("Output is <b>KeynetikPOS-&lt;version&gt;-Setup.exe</b>, approximately "
  "28&nbsp;MB. The CPack block is conditional on <b>LICENSE.txt</b> existing; "
  "the icon is applied only if <b>resources/app.ico</b> is present.")
sp(2)
S.append(callout(
    "Bump the version before every release",
    "<b>CPACK_PACKAGE_FILE_NAME</b> derives from <b>APP_VERSION</b>, so "
    "packaging without bumping it silently overwrites the previous installer "
    "with an identically named file. Update both <b>APP_VERSION</b> and the "
    "<b>project()</b> version.", WARNBG, WARNBAR))
sp(3)

h2("13.2 &nbsp; The Worker")
S.append(code_block([
    "cd license-server",
    "wrangler d1 execute keynetik-license --remote --file=schema.sql",
    "wrangler deploy",
]))
sp(2)
p("Wrangler requires Node 22 or later. Secrets are set with <b>wrangler secret "
  "put</b>; note that a shell pipe can append a newline to a secret, which has "
  "previously caused authentication failures — the admin token check trims for "
  "this reason.")
sp(3)

h2("13.3 &nbsp; Backups")
p("<b>backupIfDue()</b> runs once per calendar day at startup: it checkpoints "
  "the WAL with <b>wal_checkpoint(TRUNCATE)</b>, copies the database file to "
  "the AppData backups directory with a timestamped name, and rotates to the "
  "newest ten. A manual <b>Backup Now</b> action is available under Settings, "
  "gated by the backup permission.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 14
h1("14 &nbsp; Known gaps and technical debt")
p("Recorded honestly rather than omitted. Ordered by operational risk.")
sp(2)
S.append(table([
    ["Area", "Gap"],
    ["M-Pesa reconciliation",
     "If a callback arrives while the till is offline or closed, the sale is "
     "lost from the cashier's view although the customer was charged. The "
     "receipt exists in stk_requests but nothing reconciles it. A lookup "
     "screen by phone and amount would close this. <b>Recommended before "
     "go-live.</b>"],
    ["Buy Goods verification",
     "The Paybill/Buy Goods field semantics are implemented but have not been "
     "confirmed against a live Daraja endpoint; sandbox cannot validate the "
     "pairing."],
    ["Statutory rates",
     "Payroll rates need checking against the current Finance Act."],
    ["eTIMS",
     "Stub only. Real KRA OSCU/VSCU integration requires device onboarding."],
    ["Shift wiring",
     "ShiftManager and ZReportDialog are orphaned — not reachable from the "
     "main window. sales.shift_id is therefore always zero."],
    ["AR / AP settlement",
     "Receivables and payables accrue automatically, but settling them still "
     "requires a manual journal entry."],
    ["Inventory polling",
     "InventoryManager reloads the whole product table every 60 seconds. In a "
     "single-process application there is no external stock writer, so this "
     "should be event-driven on sale, refund and restock, or the interval "
     "widened."],
    ["Reporting N+1",
     "Reports fetch a product per sale line, although sale_items already "
     "snapshots the name and the lookup is largely unnecessary."],
    ["Stale documentation",
     "database.h still describes the class as a singleton in its header "
     "comment, and CODE_DOCUMENTATION.md describes the pre-repository "
     "Database CRUD API. Both predate the refactors."],
    ["build-release.bat",
     "Targets the wrong build directory and never produces an installer. Fix "
     "or delete."],
]))
sp(3)
S.append(callout(
    "A note on the two stale documents",
    "The header comment on <b>database.h</b> and <b>CODE_DOCUMENTATION.md</b> "
    "both describe an architecture that no longer exists — the singleton was "
    "removed and the CRUD surface was replaced by repository accessors. They "
    "are actively misleading to a new reader and should be corrected or "
    "removed rather than left in place.", WARNBG, WARNBAR))
sp(3)
S.append(Paragraph(
    "KeynetikPOS technical documentation, generated by "
    "docs/generate_technical_documentation.py. Structural claims verified "
    "against the source on 19 July 2026 (branch feature/erp-tier4, "
    "version 2.1.0).", SMALL))


def main():
    doc = Doc(OUT, "Technical Documentation v2.1.0",
              "KeynetikPOS — technical reference")
    doc.build(S)
    print("Wrote %s" % OUT)


if __name__ == "__main__":
    main()
