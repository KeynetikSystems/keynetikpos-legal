# -*- coding: utf-8 -*-
"""Generate the KeynetikPOS technical / engineering documentation PDF."""

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
    PageBreak, NextPageTemplate, ListFlowable, ListItem, HRFlowable,
)

OUT = "KeynetikPOS_Engineering_Documentation.pdf"

# ---- palette -----------------------------------------------------------------
INK    = colors.HexColor("#10243a")   # deep slate
STEEL  = colors.HexColor("#2563a8")   # heading blue
TEAL   = colors.HexColor("#0f8a7e")
AMBER  = colors.HexColor("#c9791b")
RED    = colors.HexColor("#b5402f")
LIGHT  = colors.HexColor("#eaf1f9")
GREYBG = colors.HexColor("#f4f6f9")
CODEBG = colors.HexColor("#1c2733")
CODEFG = colors.HexColor("#e6edf3")
LINE   = colors.HexColor("#cfd9e6")
DARK   = colors.HexColor("#1f262e")
MUTED  = colors.HexColor("#586273")

ss = getSampleStyleSheet()

def st(name, **kw):
    return ParagraphStyle(name, parent=ss["Normal"], **kw)

H1 = st("H1", fontName="Helvetica-Bold", fontSize=17, textColor=INK, spaceBefore=4, spaceAfter=9, leading=21)
H2 = st("H2", fontName="Helvetica-Bold", fontSize=12.5, textColor=STEEL, spaceBefore=12, spaceAfter=5, leading=16)
H3 = st("H3", fontName="Helvetica-Bold", fontSize=10.3, textColor=DARK, spaceBefore=7, spaceAfter=3, leading=13)
BODY = st("Body", fontName="Helvetica", fontSize=9.4, textColor=DARK, leading=13.8, alignment=TA_JUSTIFY, spaceAfter=5)
BULLET = st("Bullet", fontName="Helvetica", fontSize=9.2, textColor=DARK, leading=13.2)
SMALL = st("Small", fontName="Helvetica", fontSize=8.1, textColor=MUTED, leading=11)
CODE = st("Code", fontName="Courier", fontSize=8.2, textColor=CODEFG, leading=12)
CELL = st("Cell", fontName="Helvetica", fontSize=8.3, textColor=DARK, leading=11)
CELLM = st("CellM", fontName="Courier", fontSize=8.0, textColor=INK, leading=11)
CELLB = st("CellB", fontName="Helvetica-Bold", fontSize=8.3, textColor=INK, leading=11)
CELLH = st("CellH", fontName="Helvetica-Bold", fontSize=8.5, textColor=colors.white, leading=11)
TOC = st("TOC", fontName="Helvetica", fontSize=9.7, textColor=DARK, leading=16.5)
TOCN = st("TOCn", fontName="Helvetica-Bold", fontSize=9.7, textColor=STEEL, leading=16.5)

CT = st("CT", fontName="Helvetica-Bold", fontSize=29, textColor=colors.white, alignment=TA_CENTER, leading=33)
CS = st("CS", fontName="Helvetica", fontSize=12.5, textColor=colors.HexColor("#bcd0ea"), alignment=TA_CENTER, leading=17)
CX = st("CX", fontName="Helvetica", fontSize=9.5, textColor=colors.HexColor("#8fa6c6"), alignment=TA_CENTER, leading=13)

PW, PH = A4
M = 18 * mm


class Doc(BaseDocTemplate):
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        body = Frame(M, M, PW - 2 * M, PH - 2 * M - 6 * mm, id="b")
        cover = Frame(0, 0, PW, PH, id="c", leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([
            PageTemplate(id="cover", frames=[cover], onPage=self._cover),
            PageTemplate(id="body", frames=[body], onPage=self._chrome),
        ])

    def _cover(self, c, d):
        c.saveState()
        c.setFillColor(INK); c.rect(0, 0, PW, PH, fill=1, stroke=0)
        c.setFillColor(STEEL); c.rect(0, PH - 88 * mm, PW, 3.5 * mm, fill=1, stroke=0)
        c.setFillColor(TEAL); c.rect(0, PH - 88 * mm - 1.2 * mm, PW, 1.2 * mm, fill=1, stroke=0)
        # blueprint grid hint
        c.setStrokeColor(colors.HexColor("#1b3450"))
        c.setLineWidth(0.3)
        gh = int(40 * mm)
        for x in range(0, int(PW), 14):
            c.line(x, 0, x, gh)
        for y in range(0, gh, 14):
            c.line(0, y, PW, y)
        c.restoreState()

    def _chrome(self, c, d):
        c.saveState()
        c.setFillColor(INK); c.rect(0, PH - 12 * mm, PW, 12 * mm, fill=1, stroke=0)
        c.setFillColor(colors.white); c.setFont("Helvetica-Bold", 8.5)
        c.drawString(M, PH - 8 * mm, "KeynetikPOS")
        c.setFont("Helvetica", 8); c.setFillColor(colors.HexColor("#9fb6d4"))
        c.drawRightString(PW - M, PH - 8 * mm, "Engineering Documentation")
        c.setStrokeColor(LINE); c.setLineWidth(0.5); c.line(M, 13 * mm, PW - M, 13 * mm)
        c.setFont("Helvetica", 8); c.setFillColor(MUTED)
        c.drawString(M, 9 * mm, "Technical Reference — v2.0.0")
        c.drawRightString(PW - M, 9 * mm, "Page %d" % d.page)
        c.restoreState()


def bl(items, s=BULLET):
    return ListFlowable(
        [ListItem(Paragraph(t, s), leftIndent=10, value="•") for t in items],
        bulletType="bullet", start="•", leftIndent=14, bulletColor=STEEL,
        bulletFontSize=7, spaceBefore=1, spaceAfter=1)


def tbl(rows, widths, header=True, mono_col=None):
    data = []
    for i, r in enumerate(rows):
        if header and i == 0:
            data.append([Paragraph(x, CELLH) for x in r])
        else:
            cells = []
            for j, x in enumerate(r):
                s = CELLB if j == 0 else (CELLM if mono_col == j else CELL)
                cells.append(Paragraph(x, s))
            data.append(cells)
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    cmds = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 3.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 3.5),
        ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
        ("ROWBACKGROUNDS", (0, 1 if header else 0), (-1, -1), [colors.white, GREYBG]),
    ]
    if header:
        cmds += [("BACKGROUND", (0, 0), (-1, 0), STEEL),
                 ("TOPPADDING", (0, 0), (-1, 0), 5), ("BOTTOMPADDING", (0, 0), (-1, 0), 5)]
    t.setStyle(TableStyle(cmds))
    return t


def code(lines):
    body = "<br/>".join(l.replace(" ", "&nbsp;").replace("<", "&lt;").replace(">", "&gt;") for l in lines)
    p = Paragraph(body, CODE)
    box = Table([[p]], colWidths=[PW - 2 * M - 6])
    box.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), CODEBG),
        ("LEFTPADDING", (0, 0), (-1, -1), 10), ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 8), ("BOTTOMPADDING", (0, 0), (-1, -1), 8),
        ("LINEBEFORE", (0, 0), (0, -1), 3, TEAL),
    ]))
    return box


def note(title, text, bg=LIGHT, bar=STEEL):
    inner = [Paragraph("<b>%s</b>" % title, st("nt", fontName="Helvetica-Bold", fontSize=9.3, textColor=INK, leading=12.5, spaceAfter=2))]
    if text:
        inner.append(Paragraph(text, st("nb", fontName="Helvetica", fontSize=8.8, textColor=DARK, leading=12.5)))
    b = Table([[inner]], colWidths=[PW - 2 * M - 8])
    b.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), bg),
        ("LEFTPADDING", (0, 0), (-1, -1), 11), ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 7), ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
        ("LINEBEFORE", (0, 0), (0, -1), 3, bar),
    ]))
    return b


def rule():
    return HRFlowable(width="100%", thickness=0.6, color=LINE, spaceBefore=4, spaceAfter=8)


S = []

# ---- COVER -------------------------------------------------------------------
S.append(Spacer(1, 70 * mm))
S.append(Paragraph("KeynetikPOS", CT))
S.append(Spacer(1, 5 * mm))
S.append(Paragraph("Engineering &amp; Technical Documentation", CS))
S.append(Spacer(1, 2 * mm))
S.append(Paragraph("Architecture &bull; Data Model &bull; Modules &bull; Build &bull; Security", CX))
S.append(Spacer(1, 66 * mm))
S.append(Paragraph("Version 2.0.0", CS))
S.append(Spacer(1, 1.5 * mm))
S.append(Paragraph("Qt 6 / C++17 &bull; SQLite &bull; CMake &bull; Windows (MinGW / MSVC)", CX))
S.append(NextPageTemplate("body"))
S.append(PageBreak())

# ---- 0. ABOUT + TOC ----------------------------------------------------------
S.append(Paragraph("About this document", H1))
S.append(Paragraph(
    "This is the technical reference for KeynetikPOS, intended for engineers building, "
    "extending or maintaining the system. It describes the architecture, the persistence "
    "layer and schema, module responsibilities and collaborations, the critical "
    "transaction flows, the concurrency/threading model, the security implementation, and "
    "the build &amp; deployment pipeline. It closes with known engineering trade-offs and "
    "technical debt.", BODY))

S.append(tbl([
    ["Property", "Value"],
    ["Language / standard", "C++17"],
    ["UI framework", "Qt 6 (Widgets, Sql, PrintSupport, Network, SerialPort)"],
    ["Persistence", "SQLite (QSQLITE driver), single embedded file"],
    ["Build system", "CMake &ge; 3.16 (AUTOMOC / AUTOUIC / AUTORCC)"],
    ["Toolchains", "MinGW (Qt 6.11) and MSVC 2022"],
    ["Packaging", "windeployqt + CPack / NSIS installer"],
    ["Source size", "~21,000 LOC across ~70 translation units / headers"],
    ["Target OS", "Windows (Win32 GUI subsystem)"],
], [42 * mm, PW - 2 * M - 42 * mm]))

S.append(rule())
S.append(Paragraph("Contents", H2))
toc = [
    ("1", "System Architecture"),
    ("2", "Source Layout & Module Map"),
    ("3", "Persistence Layer & Data Model"),
    ("4", "Core Domain Types"),
    ("5", "Checkout Transaction Flow"),
    ("6", "Subsystem Design Notes"),
    ("7", "Concurrency & Threading Model"),
    ("8", "Security Implementation"),
    ("9", "Build, Packaging & Deployment"),
    ("10", "Configuration & Runtime Environment"),
    ("11", "Engineering Notes & Technical Debt"),
    ("12", "Extension Guide"),
]
rows = [[Paragraph(n, TOCN), Paragraph(t, TOC)] for n, t in toc]
tt = Table(rows, colWidths=[11 * mm, PW - 2 * M - 11 * mm])
tt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"),
                        ("TOPPADDING", (0, 0), (-1, -1), 1), ("BOTTOMPADDING", (0, 0), (-1, -1), 1)]))
S.append(tt)
S.append(PageBreak())

# ---- 1. ARCHITECTURE ---------------------------------------------------------
S.append(Paragraph("1. System Architecture", H1))
S.append(Paragraph(
    "KeynetikPOS is a single-process, single-binary desktop application. It follows a "
    "loosely layered design: a Qt Widgets presentation layer, a set of manager/service "
    "objects holding business logic, and a thin data-access layer over SQLite. "
    "Several cross-cutting services (database, authentication, licensing) are exposed as "
    "process-wide singletons.", BODY))

S.append(Paragraph("Layered view", H3))
S.append(tbl([
    ["Layer", "Responsibility", "Representative types"],
    ["Presentation", "Windows, dialogs, widgets, theming, input",
     "MainWindow, *Dialog, enhancedanalytics widgets, ColorScheme"],
    ["Application / logic", "Business rules, orchestration, validation",
     "UserManager, InventoryManager, ShiftManager, ScheduleManager, "
     "SettingsManager, ReceiptPrinter, LicenseManager"],
    ["Data access", "SQL, schema, migrations, transactions",
     "Database (singleton), per-manager table owners"],
    ["Infrastructure", "OS / hardware / network I/O",
     "BarcodeReader (serial/HID), Qt Network (messaging, licensing), QPrinter"],
], [26 * mm, 52 * mm, PW - 2 * M - 78 * mm]))

S.append(Paragraph("Design patterns in use", H3))
S.append(tbl([
    ["Pattern", "Where", "Why"],
    ["Singleton (Meyers)", "Database, UserManager, LicenseManager, ThemeManager",
     "One DB connection / one auth context / global theme"],
    ["Strategy", "BarcodeReader (HID vs serial); ReceiptPrinter (thermal/standard/PDF)",
     "Swap behaviour behind one interface"],
    ["Observer (signals/slots)", "InventoryManager, ScheduleManager, managers&rarr;UI",
     "Decouple producers of events from the UI"],
    ["Abstract factory / registry", "MessageProvider + ScheduleManager registry",
     "Add SMS/WhatsApp providers without touching the engine"],
    ["Value object / DTO", "Product, Sale, Receipt, BusinessSettings, ShiftRecord",
     "Plain structs move data across layers"],
    ["Active record-ish DAO", "Database + per-manager table owners",
     "Typed query methods return domain structs"],
], [34 * mm, 64 * mm, PW - 2 * M - 98 * mm]))

S.append(Paragraph("Component dependency sketch", H3))
S.append(code([
    "                       +-------------------+",
    "                       |    MainWindow     |  (sales terminal)",
    "                       +---------+---------+",
    "       cart/checkout /     menus |        \\ inventory signals",
    "                     /           |         \\",
    "        +-----------v--+  +------v------+  +-v-------------------+",
    "        |  *Dialog UIs  |  | Settings/   |  | InventoryManager   |",
    "        | (reports,     |  | Schedule/   |  | (cache + alerts)   |",
    "        |  users, etc.) |  | Shift mgrs  |  +-+------------------+",
    "        +-------+-------+  +------+------+    |",
    "                \\                |           |",
    "                 \\        +------v-----------v--+",
    "                  +------>|   Database (singleton)|<--- UserManager",
    "                         |   SQLite connection    |     LicenseManager",
    "                         +------------------------+     (HKCU + file)",
]))

S.append(Paragraph("Startup sequence (main.cpp)", H3))
S.append(code([
    "QApplication a(argc, argv);",
    "AntiDebug::isThreatDetected()      -> abort if debugger (release only)",
    "LicenseManager::instance().initialize()   // offline, non-blocking",
    "  - tamper check -> trial/limited/full state",
    "Database::instance().initialize()  // open, PRAGMA, create+migrate, seed",
    "LoginDialog (<=3 attempts) -> UserManager::login()",
    "  - if must_change_password -> forcePasswordChange()",
    "MainWindow w; w.show();",
    "QTimer::singleShot(2000, startOnlineHeartbeat)  // async revalidation",
    "return a.exec();",
]))
S.append(note("Singletons",
    "Database, UserManager and LicenseManager use the Meyers singleton "
    "(function-local static). This gives global access and a single DB connection, "
    "but couples callers to the instances and is the main obstacle to unit testing "
    "(see &sect;11)."))
S.append(PageBreak())

# ---- 2. SOURCE LAYOUT --------------------------------------------------------
S.append(Paragraph("2. Source Layout &amp; Module Map", H1))
S.append(Paragraph(
    "All translation units live at the project root and are listed explicitly in "
    "CMakeLists.txt. Modules pair a <font face='Courier'>.cpp/.h</font> by feature.", BODY))
S.append(tbl([
    ["Module", "Files", "Role"],
    ["Entry point", "main.cpp", "Bootstrap, license/login gating, password enforcement"],
    ["Main window", "mainwindow.*", "Sales terminal, cart UI, menu wiring, checkout"],
    ["Database", "database.*", "Schema, migrations, products/sales/refunds/stock DAO"],
    ["Auth / RBAC", "usermanager.*, user.*, logindialog.*, usermanagementdialog.*",
     "Login, hashing, permissions, user CRUD UI"],
    ["Inventory", "inventorymanager.*, inventorydialog.*, lowstockdialog.*",
     "Stock cache, alerts, catalogue UI, CSV import/export"],
    ["Stock control", "stockmanager.*", "Restock/adjust UI with audit"],
    ["Discounts", "discount.*, discountdialog.*", "Discount engine, presets, PIN, log"],
    ["Payments", "paymentmanager.*, paymentdialog.*", "Tender capture, change, records"],
    ["Receipts", "receiptprinter.*", "Thermal / standard / PDF rendering"],
    ["Shifts", "shiftmanager.*, shift_dialogs.*, shift_type.h", "Shift lifecycle, cash"],
    ["Reports", "reportsdialog.*, zreportdialog.*, saleshistorydialog.*",
     "Sales reports, Z-report, history + refunds"],
    ["Analytics", "analyticsmanager.*, analyticsdashboard.*, analyticsdialog.*, "
     "enhancedanalytics.*", "Aggregation + chart widgets"],
    ["Messaging", "schedulemanager.*, messageprovider.*, whatsappmanager.*, "
     "Africatalkingprovider.h, scheduledialog.*, scheduleeditordialog.*",
     "Provider registry + scheduling engine"],
    ["Barcode", "barcodereader.*, barcodescannermanager.*", "HID/serial input"],
    ["Settings", "settingsmanager.*, settingsdialog.*", "Config persistence + UI"],
    ["Licensing", "licensemanager.*, antidebug.h", "Trial, activation, tamper, anti-debug"],
    ["Shared", "passwordhasher.h, colorscheme.*, CartItem.h", "Crypto helper, theme, cart item"],
], [24 * mm, 56 * mm, PW - 2 * M - 80 * mm]))
S.append(PageBreak())

# ---- 3. DATA MODEL -----------------------------------------------------------
S.append(Paragraph("3. Persistence Layer &amp; Data Model", H1))
S.append(Paragraph(
    "A single SQLite file holds all state, created at "
    "<font face='Courier'>QStandardPaths::AppDataLocation/pos_database.db</font>. "
    "The <font face='Courier'>Database</font> singleton owns the default connection; "
    "<font face='Courier'>PRAGMA foreign_keys = ON</font> is set per-connection at open. "
    "Schema is created idempotently with "
    "<font face='Courier'>CREATE TABLE IF NOT EXISTS</font>, and additive column changes "
    "are applied by an <font face='Courier'>ensureColumn()</font> migration helper "
    "(since IF NOT EXISTS never alters existing tables).", BODY))

S.append(Paragraph("Core schema (database.cpp)", H3))
S.append(tbl([
    ["Table", "Key columns", "Notes"],
    ["categories", "id PK, name UNIQUE, description", "Lookup for product grouping"],
    ["products", "id PK, name, category FK&rarr;categories(name), price, cost_price, "
     "profit_margin, stock_quantity, barcode UNIQUE, is_active", "Soft-deleted via is_active"],
    ["sales", "id PK, sale_date, subtotal, tax, discount, total, payment_method, "
     "amount_paid, change_due", "amount_paid/change_due added by migration"],
    ["sale_items", "id PK, sale_id FK, product_id FK, product_name, quantity, price, "
     "cost_price, subtotal", "Denormalised name + cost for historical accuracy"],
    ["users", "id PK, username UNIQUE, password_hash, full_name, email UNIQUE, role, "
     "is_active, last_login, created_by, must_change_password", "role CHECK constraint"],
    ["user_activity_log", "id PK, user_id FK, username, action, details, timestamp",
     "Append-only audit trail"],
    ["stock_adjustments", "id PK, product_id FK, product_name, change_qty, old_qty, "
     "new_qty, reason, adjusted_by, adjusted_at", "Stock movement audit"],
    ["refunds", "id PK, sale_id FK, refund_date, total_refunded, reason, processed_by",
     "One refund per sale enforced in code"],
], [26 * mm, 78 * mm, PW - 2 * M - 104 * mm]))

S.append(Paragraph("Module-owned tables", H3))
S.append(tbl([
    ["Table", "Owner", "Purpose"],
    ["AppSettings", "SettingsManager", "Key/Value config store"],
    ["Shifts", "ShiftManager", "Shift lifecycle, floats, totals"],
    ["DiscountLog", "DiscountManager", "Applied-discount audit"],
    ["PaymentRecords", "PaymentManager", "Tender records"],
    ["message_schedules", "ScheduleManager", "Automated report schedules"],
    ["ProductBarcodes / ScanHistory", "BarcodeScannerManager", "Barcode map &amp; scan log"],
], [44 * mm, 40 * mm, PW - 2 * M - 84 * mm]))
S.append(note("Indexing",
    "Indexes back the hot audit/login paths: idx_users_username, idx_users_email, "
    "idx_activity_log_user / _timestamp, idx_adj_product / _time.", bg=GREYBG, bar=TEAL))
S.append(PageBreak())

# ---- 3b. DDL + relationships -------------------------------------------------
S.append(Paragraph("Representative DDL", H2))
S.append(Paragraph("The two central sales tables, verbatim from database.cpp:", BODY))
S.append(code([
    "CREATE TABLE IF NOT EXISTS sales (",
    "    id             INTEGER PRIMARY KEY AUTOINCREMENT,",
    "    sale_date      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,",
    "    subtotal       REAL NOT NULL,",
    "    tax            REAL DEFAULT 0,",
    "    discount       REAL DEFAULT 0,",
    "    total          REAL NOT NULL,",
    "    payment_method TEXT DEFAULT 'Cash',",
    "    amount_paid    REAL DEFAULT 0,   -- added via ensureColumn()",
    "    change_due     REAL DEFAULT 0);  -- added via ensureColumn()",
    "",
    "CREATE TABLE IF NOT EXISTS sale_items (",
    "    id           INTEGER PRIMARY KEY AUTOINCREMENT,",
    "    sale_id      INTEGER NOT NULL,",
    "    product_id   INTEGER NOT NULL,",
    "    product_name TEXT NOT NULL,   -- denormalised snapshot",
    "    quantity     INTEGER NOT NULL,",
    "    price        REAL NOT NULL,",
    "    cost_price   REAL NOT NULL DEFAULT 0,  -- for profit calc",
    "    subtotal     REAL NOT NULL,",
    "    FOREIGN KEY (sale_id)    REFERENCES sales(id),",
    "    FOREIGN KEY (product_id) REFERENCES products(id));",
]))
S.append(Paragraph("Relationships &amp; integrity", H3))
S.append(bl([
    "<font face='Courier'>sales 1&mdash;N sale_items</font>; "
    "<font face='Courier'>products 1&mdash;N sale_items</font>; "
    "<font face='Courier'>sales 1&mdash;1 refunds</font> (one refund per sale, enforced "
    "in code via isRefunded()).",
    "<font face='Courier'>users 1&mdash;N user_activity_log</font>; "
    "<font face='Courier'>products 1&mdash;N stock_adjustments</font>.",
    "products.category is a <b>text</b> FK to categories(name) (a non-PK column) &mdash; "
    "renaming a category can orphan products; flagged in &sect;11.",
    "Denormalisation is intentional: sale_items snapshots product_name and cost_price so "
    "historical sales and profit survive later product edits or deletions.",
]))

S.append(Paragraph("Database API surface (database.h)", H3))
S.append(tbl([
    ["Group", "Methods"],
    ["Products", "getAllProducts, getProductsByCategory, getProductById, "
     "getProductByBarcode, addProduct, updateProduct, deleteProduct (soft), "
     "getAllCategories"],
    ["Stock", "updateStock, decreaseStock, increaseStock, getStock, "
     "adjustStockWithLog, logStockAdjustment, getStockHistory"],
    ["Sales", "recordSale (atomic), getAllSales, getSalesByDateRange, getSaleItems, "
     "getSaleById"],
    ["Analytics", "getTotalSalesToday / ThisMonth, getTotalTransactionsToday, "
     "getTopSellingProducts, getActualGrossProfit[Today/ThisMonth]"],
    ["Refunds", "processRefund (atomic, restocks + logs), isRefunded"],
    ["Infra", "initialize, isOpen, ensureColumn, executeQuery, getLastError"],
], [24 * mm, PW - 2 * M - 24 * mm]))
S.append(PageBreak())

# ---- 4. DOMAIN TYPES ---------------------------------------------------------
S.append(Paragraph("4. Core Domain Types", H1))
S.append(Paragraph(
    "Plain structs carry data between layers; richer behaviour lives on managers.", BODY))
S.append(tbl([
    ["Type", "Defined in", "Purpose"],
    ["Product", "database.h", "Catalogue row; static calculateSellingPrice(cost, margin)"],
    ["Sale / SaleItem", "database.h", "Sale header and line items"],
    ["CartItem", "CartItem.h", "In-memory cart line with getSubtotal()"],
    ["Cart", "mainwindow.h", "One open transaction: items, discount, timestamps"],
    ["Receipt", "receiptprinter.h", "Render model for a printed/exported receipt"],
    ["User / UserRole / Permission", "user.h", "Identity + RBAC enums; RoleManager maps role&rarr;perms"],
    ["BusinessSettings", "settingsmanager.h", "All configurable settings as a value object"],
    ["ShiftRecord", "shift_type.h", "Shift state and running totals"],
    ["MessageSchedule", "MessageSchedule.h", "Schedule definition + type/report enums"],
    ["ZReportData", "zreportdialog.h", "Computed Z-report model"],
    ["DiscountResult / DiscountPreset", "discount.h", "Discount computation outputs"],
], [44 * mm, 30 * mm, PW - 2 * M - 74 * mm]))

S.append(Paragraph("RBAC model", H3))
S.append(Paragraph(
    "<font face='Courier'>RoleManager::hasPermission(role, perm)</font> is a pure function: "
    "Admin is allowed everything; Manager, Cashier and Viewer are explicit allow-lists over "
    "the <font face='Courier'>Permission</font> enum (18 permissions spanning sales, "
    "inventory, reports, users, system). UI affordances are enabled/disabled from these "
    "checks, and every privileged manager method re-checks server-side.", BODY))
S.append(PageBreak())

# ---- 5. CHECKOUT FLOW --------------------------------------------------------
S.append(Paragraph("5. Checkout Transaction Flow", H1))
S.append(Paragraph(
    "Checkout is the most critical path. It is fully atomic: the sale header, all line "
    "items and stock decrements commit together or not at all.", BODY))

S.append(Paragraph("Sequence", H3))
S.append(code([
    "MainWindow::onCheckout()",
    " 1. computeCartTotals(subtotal, discount, settings)",
    "      tax on (subtotal - discount); inclusive or exclusive",
    " 2. PaymentDialog -> method, amountPaid, change",
    " 3. build QVector<SaleItem> from cart",
    " 4. Database::recordSale(items, totals, method, paid, change):",
    "      db.transaction()",
    "      for each item: SELECT stock FOR validation (oversell guard)",
    "      INSERT INTO sales ...            -> saleId",
    "      for each item:",
    "         INSERT INTO sale_items ...",
    "         UPDATE products SET stock_quantity = stock_quantity - qty",
    "      db.commit()   (rollback on ANY failure, returns -1)",
    " 5. on success: refresh inventory cache, print receipt,",
    "      log action, record to shift, reset cart",
    " 6. on failure: keep cart, show error, print nothing",
]))
S.append(note("Invariants enforced",
    "(a) stock never decremented more than once per unit sold; (b) a sale row never exists "
    "without its line items; (c) stock cannot go negative under concurrent sales because "
    "validation and decrement share one transaction.",
    bg=colors.HexColor("#e9f5ee"), bar=TEAL))
S.append(Paragraph("Money handling", H3))
S.append(Paragraph(
    "Amounts are <font face='Courier'>double</font> / SQLite <font face='Courier'>REAL</font>. "
    "A <font face='Courier'>roundCents()</font> helper (round to 2 dp) is applied at totals "
    "boundaries to contain floating-point drift. Integer-cents storage is a recommended "
    "future change (see &sect;11).", BODY))

S.append(Paragraph("Totals algorithm (computeCartTotals)", H3))
S.append(code([
    "base = subtotal - clamp(discount, 0, subtotal)",
    "if !taxEnabled || taxRate <= 0:",
    "     tax = 0;            total = base",
    "elif taxInclusive:       # price already contains tax",
    "     tax = base - base / (1 + taxRate)",
    "     total = base",
    "else:                    # tax added on top",
    "     tax = base * taxRate",
    "     total = base + tax",
    "# all three lines rounded to cents",
]))
S.append(Paragraph("recordSale return semantics", H3))
S.append(tbl([
    ["Result", "Meaning", "Side effects"],
    ["&ge; 1", "Success; value is the new sale id",
     "sales + sale_items inserted, stock decremented, committed"],
    ["-1", "Failure (empty cart, oversell, insert error, commit error)",
     "Full rollback; getLastError() set; cart preserved; no receipt"],
], [16 * mm, 60 * mm, PW - 2 * M - 76 * mm]))

S.append(Paragraph("Refund flow (Database::processRefund) &mdash; also atomic", H3))
S.append(code([
    "db.transaction()",
    "  sale = getSaleById(id);  abort if not found",
    "  INSERT INTO refunds (sale_id, total_refunded, reason, processed_by)",
    "  for each sale_item:",
    "     increaseStock(product_id, qty)        // return to stock",
    "     logStockAdjustment(... 'Refund for Sale #id' ...)",
    "db.commit()   // rollback on any failure",
]))
S.append(note("Guard",
    "The Sales History UI calls isRefunded() before allowing a refund and disables the "
    "button afterwards, so a sale cannot be refunded twice.", bg=GREYBG, bar=TEAL))
S.append(PageBreak())

# ---- 6. SUBSYSTEM NOTES ------------------------------------------------------
S.append(Paragraph("6. Subsystem Design Notes", H1))

S.append(Paragraph("InventoryManager", H3))
S.append(bl([
    "Holds an in-memory <font face='Courier'>QMap&lt;int, InventoryInfo&gt;</font> cache, "
    "refreshed on a QTimer and after each sale.",
    "Status thresholds: 0 = out, 1&ndash;4 critical, 5&ndash;20 low, &gt;20 healthy.",
    "Emits inventoryLow / Critical / OutOfStock / Updated / Restocked signals consumed by "
    "the main window and dialogs.",
    "Post-fix, it no longer issues its own sale decrement &mdash; recordSale owns stock "
    "mutation; the manager only syncs cache and emits warnings.",
]))
S.append(Paragraph("ScheduleManager &amp; providers", H3))
S.append(bl([
    "Provider registry over a <font face='Courier'>MessageProvider</font> interface; "
    "concrete providers: AfricasTalkingProvider (SMS) and WhatsAppManager (Twilio).",
    "QTimer tick evaluates each schedule's shouldFire() against type/time/day/threshold.",
    "Report bodies are built from the shared Z-report text generator; sends are async via "
    "Qt Network with delivered/error signals; schedules persist in message_schedules.",
]))
S.append(Paragraph("BarcodeReader", H3))
S.append(bl([
    "Strategy by mode: keyboard-wedge installs an application event filter and buffers "
    "keystrokes with an inter-character timeout; serial mode reads a QSerialPort.",
    "Configurable port, baud, prefix/suffix stripping; emits barcodeScanned(QString).",
]))
S.append(Paragraph("ReceiptPrinter", H3))
S.append(bl([
    "Three strategies behind one API: thermal (plain text), standard (HTML via "
    "QTextDocument/QPrinter), and PDF export.",
    "Caches the last receipt for reprint; company header and footer injected from settings.",
    "Thermal layout is a fixed 42-column monospace render (centered header, "
    "padded item/qty/total columns, '=' / '-' rules) suited to roll printers.",
]))
S.append(Paragraph("Discounts &amp; payments", H3))
S.append(bl([
    "DiscountManager exposes applyPercentage / applyFixedAmount returning a DiscountResult "
    "(type, value, computed amount, label, appliedBy) and logs to DiscountLog with the "
    "shift id and cart id.",
    "Built-in presets: 5/10/15/20% off, Staff 25%, and $2/$5/$10 fixed-amount off.",
    "PaymentManager models 8 tender types (Cash, Card, MobileMoney, BankTransfer, Check, "
    "GiftCard, StoreCredit, Other) with per-method name/icon/colour and daily/shift "
    "summaries; the live checkout stores method + amount_paid + change_due on the sale.",
]))
S.append(Paragraph("Theme system (ColorScheme / ThemeManager)", H3))
S.append(bl([
    "ThemeManager is a singleton holding the active ColorScheme (a flat bag of "
    "background/text/border/semantic/button-state colours).",
    "Dialogs pull colours from the scheme so light/dark mode is consistent; the choice is "
    "persisted in QSettings and toggled from the Settings menu.",
]))
S.append(PageBreak())

# ---- 6b. ANALYTICS QUERY CATALOGUE ------------------------------------------
S.append(Paragraph("Analytics query catalogue", H2))
S.append(Paragraph(
    "AnalyticsManager (and Database) compute metrics directly in SQL over sales / "
    "sale_items. Key building blocks:", BODY))
S.append(tbl([
    ["Metric", "Approach (SQL essence)"],
    ["Today / month revenue", "SUM(total) filtered by DATE(sale_date)=DATE('now') or "
     "strftime('%Y-%m', ...)"],
    ["Transactions today", "COUNT(*) over sales for the current date"],
    ["Top sellers", "SUM(quantity) grouped by product_name, ORDER BY DESC LIMIT n"],
    ["Actual gross profit", "SUM((price - cost_price) * quantity) joining sale_items&rarr;sales "
     "over a date range"],
    ["Category breakdown", "Revenue grouped by product/category with % of total"],
    ["Hourly heatmap", "Revenue / count bucketed by hour-of-day (strftime '%H')"],
    ["Peak hours", "Ranked hour buckets with a recommended trading window"],
    ["Dashboard summary", "today/yesterday/week/month revenue, avg ticket, top "
     "product/category, growth rate"],
], [40 * mm, PW - 2 * M - 40 * mm]))
S.append(Paragraph(
    "Richer typed structs (DailySalesStats, HourlySalesStats, ProductSalesStats, "
    "CategoryStats, PeakHourInfo, DashboardSummary) carry results to the chart widgets in "
    "enhancedanalytics, which render without QtCharts (custom-drawn widgets).", BODY))
S.append(PageBreak())

# ---- 7. THREADING ------------------------------------------------------------
S.append(Paragraph("7. Concurrency &amp; Threading Model", H1))
S.append(bl([
    "The application is effectively single-threaded: all UI and business logic run on the "
    "Qt GUI (main) thread driven by the event loop.",
    "There are no worker threads, mutexes or shared-state locks; the single SQLite "
    "connection is only touched from the GUI thread, which is the supported usage.",
    "Network I/O (licensing heartbeat, SMS/WhatsApp sends) is asynchronous and "
    "non-blocking via QNetworkAccessManager signals &mdash; the UI never stalls on a socket.",
    "Time-based work (inventory refresh, schedule ticks, datetime clock, the post-startup "
    "license heartbeat) is driven by QTimer callbacks on the same event loop.",
    "Hardware input (serial barcode) arrives via QSerialPort readyRead signals; HID input "
    "via the event filter &mdash; both on the GUI thread.",
]))
S.append(note("Implication",
    "Any future long-running operation (large CSV import, bulk report generation, blocking "
    "DB work) should be moved off the GUI thread (QtConcurrent / QThread + a dedicated DB "
    "connection) to keep the terminal responsive.", bg=colors.HexColor("#fdf3e7"), bar=AMBER))

S.append(Paragraph("Key signal / slot wiring", H3))
S.append(tbl([
    ["Emitter &mdash; signal", "Consumer &mdash; slot / effect"],
    ["InventoryManager::inventoryLow/Critical/OutOfStock", "MainWindow alert handlers + "
     "status toasts"],
    ["InventoryManager::inventoryUpdated", "Product grid / totals refresh"],
    ["SettingsManager::settingsChanged", "MainWindow refreshTaxTitle + updateTotals; "
     "receipt header reload"],
    ["ScheduleManager::messageDelivered / messageError", "Surface send success/failure"],
    ["BarcodeReader::barcodeScanned", "Resolve product &rarr; addToCart"],
    ["ShiftManager::shiftOpened / shiftClosed", "Update shift UI and Z-report inputs"],
    ["QTimer (inventory / schedule / clock / heartbeat)", "Periodic refresh, dispatch, "
     "revalidation"],
], [70 * mm, PW - 2 * M - 70 * mm]))

S.append(Paragraph("Error-handling conventions", H3))
S.append(bl([
    "DAO methods return bool / -1 sentinels and stash a message in Database::lastError() "
    "(getLastError()); the UI surfaces it via QMessageBox.",
    "Multi-statement mutations (recordSale, processRefund, deleteUser) wrap work in "
    "db.transaction() / commit() with rollback on any failure.",
    "Network calls degrade gracefully: timeouts/aborts set serverReachable=false rather "
    "than blocking or crashing.",
    "Diagnostic logging uses qDebug/qWarning/qCritical; user actions go to the "
    "user_activity_log audit table.",
]))
S.append(rule())

# ---- 8. SECURITY -------------------------------------------------------------
S.append(Paragraph("8. Security Implementation", H1))
S.append(Paragraph("Credential hashing (passwordhasher.h)", H3))
S.append(bl([
    "Salted <b>PBKDF2-HMAC-SHA256</b>, 100,000 iterations, 16-byte random salt (from "
    "QRandomGenerator::system()), 32-byte derived key, via "
    "<font face='Courier'>QPasswordDigestor::deriveKeyPbkdf2</font>.",
    "Constant-time comparison (XOR-accumulate) to resist timing attacks.",
    "Legacy unsalted SHA-256 hashes still verify and are transparently re-hashed to PBKDF2 "
    "on next successful login (needsRehash()).",
    "Used for both user passwords and the discount PIN.",
]))
S.append(Paragraph("Stored hash format", H3))
S.append(code([
    "pbkdf2-sha256 $ <iterations> $ <salt-hex> $ <derived-key-hex>",
    "  example:",
    "  pbkdf2-sha256$100000$9f2c...b1$4d6a...e0",
    "",
    "verify(secret, stored):",
    "  if not startswith 'pbkdf2-sha256$':  legacy SHA-256 path",
    "  split into 4 parts -> iters, salt, expected",
    "  key = PBKDF2(SHA256, secret, salt, iters, len(expected))",
    "  return constantTimeEquals(key, expected)",
]))
S.append(Paragraph("Account policy", H3))
S.append(bl([
    "Default seed admin/admin123 carries must_change_password = 1; admin-issued resets set "
    "it too, forcing a new password before entry.",
    "Login is capped at 3 attempts; all auth events are audited.",
]))
S.append(Paragraph("Licensing &amp; tamper resistance (licensemanager.cpp)", H3))
S.append(bl([
    "State in HKCU; integrity hash = SHA-256(key + installDate + source + revokedFlag + "
    "secret salt). Deleting/editing any covered value &rarr; mismatch &rarr; locked.",
    "Trial install date is mirrored to a signed, machine-bound breadcrumb file outside the "
    "registry; the effective date is the earliest of both, so an HKCU wipe alone cannot "
    "reset the trial.",
    "Activation: local, or server-backed with device binding. Server-unreachable activation "
    "is refused (no offline grant). Online keys re-validate via background heartbeat with a "
    "7-day offline grace; revocation is enforced at next launch.",
]))
S.append(Paragraph("Offline key format check (validateKey)", H3))
S.append(code([
    "key = 16 hex chars in 4 groups (dashes stripped, upper-cased)",
    "seg0 seg1 seg2 seg3  -> take seg0, seg1, seg2",
    "h = SHA256( seg0 + seg2 + 'KNK-SALT-2025' ).hex()",
    "valid  <=>  h startswith seg1.lower()",
    "# a self-checking key: seg1 is a checksum of seg0+seg2.",
    "# Defeats typos, not a determined keygen (salt is in the binary).",
]))
S.append(Paragraph("License state machine", H3))
S.append(tbl([
    ["Condition at launch", "Resulting state"],
    ["Integrity hash mismatch / deleted", "Tampered &rarr; InvalidKey (locked)"],
    ["No key, within 30 days of install", "Trial"],
    ["No key, past 30 days", "TrialExpired (key prompt or exit)"],
    ["Valid local key", "FullLicense (no heartbeat required)"],
    ["Valid online key, last check &le; 7 days", "FullLicense (grace counting down)"],
    ["Valid online key, last check &gt; 7 days", "InvalidKey until server reached"],
    ["Revoked flag set (signed)", "InvalidKey"],
], [70 * mm, PW - 2 * M - 70 * mm]))

S.append(Paragraph("Other controls", H3))
S.append(bl([
    "100% parameterised SQL (prepared statements / bound values) &mdash; no string-built "
    "queries; SQLite foreign keys enforced.",
    "Release-only minimal anti-debug (IsDebuggerPresent / CheckRemoteDebuggerPresent), "
    "tuned to avoid false positives on legitimate hardware/VMs.",
]))
S.append(note("Threat-model caveat",
    "The key checksum salt and tamper salt ship in the binary, so the scheme deters casual "
    "tampering and keygens but is not cryptographically unbreakable. Real enforcement "
    "depends on the activation server.", bg=colors.HexColor("#f7eaea"), bar=RED))
S.append(PageBreak())

# ---- 9. BUILD ----------------------------------------------------------------
S.append(Paragraph("9. Build, Packaging &amp; Deployment", H1))
S.append(Paragraph("CMake configuration", H3))
S.append(bl([
    "C++17; AUTOMOC / AUTOUIC / AUTORCC enabled for Qt meta-object, .ui and .qrc handling.",
    "find_package selects Qt6 (fallback Qt5) components: Widgets, Core, Sql, PrintSupport, "
    "Network, SerialPort.",
    "Single executable target <font face='Courier'>KeynetikPOS</font> (WIN32 GUI subsystem); "
    "sources listed explicitly.",
    "Optional resources: resources/resources.qrc and a Windows .rc when present.",
]))
S.append(Paragraph("Deploy &amp; package", H3))
S.append(bl([
    "Post-build: windeployqt stages the exe with all Qt runtime DLLs and plugins into "
    "windeployqt_stage/.",
    "CPack + NSIS produces an installer (icon, Start-menu &amp; desktop shortcuts, run-on-"
    "finish, license page); install rules ship the staged directory.",
    "Presets: CMakePresets (Qt) + CMakeUserPresets (Qt-Debug / Qt-Release &rarr; out/build/...).",
]))
S.append(Paragraph("Verified build", H3))
S.append(code([
    "$env:PATH = 'C:\\Qt\\Tools\\mingw1310_64\\bin;' +",
    "            'C:\\Qt\\6.11.0\\mingw_64\\bin;' + $env:PATH",
    "cmake --build build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release --parallel 8",
    "# -> [100%] Built target KeynetikPOS",
]))
S.append(PageBreak())

# ---- 10. CONFIG / RUNTIME ----------------------------------------------------
S.append(Paragraph("10. Configuration &amp; Runtime Environment", H1))
S.append(tbl([
    ["Concern", "Mechanism / location"],
    ["Application DB", "QStandardPaths::AppDataLocation + /pos_database.db (auto-created)"],
    ["Business / tax / receipt config", "AppSettings table (SettingsManager)"],
    ["License + trial state", "HKCU\\Software\\KeynetikSolutions\\KeynetikPOS"],
    ["Trial breadcrumb", "Signed file under GenericConfigLocation/KeynetikSolutions"],
    ["Provider credentials", "QSettings (YourCompany/KeynetikPOS) Twilio + Africa's Talking"],
    ["Barcode + theme prefs", "QSettings (input mode, port, baud, dark mode)"],
    ["Licensing endpoint", "SERVER_URL constant in licensemanager.cpp (placeholder)"],
], [52 * mm, PW - 2 * M - 52 * mm]))
S.append(Paragraph(
    "On first launch the database is seeded with sample categories, ~35 demo products and "
    "the default admin account. Tax defaults to disabled; currency formatting and the "
    "16% VAT context target the Kenyan market but are configurable.", BODY))
S.append(rule())

# ---- 11. TECH DEBT -----------------------------------------------------------
S.append(Paragraph("11. Engineering Notes &amp; Technical Debt", H1))
S.append(Paragraph(
    "Known trade-offs and recommended improvements, roughly in priority order:", BODY))
S.append(tbl([
    ["Area", "Current state", "Recommendation"],
    ["Testability", "Singletons + logic inside MainWindow; no automated tests",
     "Extract services behind interfaces; add a unit-test target"],
    ["Money type", "double / REAL with roundCents at boundaries",
     "Store integer cents end-to-end to remove drift"],
    ["MainWindow size", "~1,900 LOC mixing UI and business logic",
     "Split cart/checkout/menu controllers"],
    ["Reorder levels", "Hardcoded 20/100 in InventoryManager cache",
     "Per-product columns persisted in DB"],
    ["DB connection use", "Some managers use the default QSqlQuery connection implicitly",
     "Pass the connection explicitly everywhere"],
    ["Parallel barcode paths", "BarcodeReader (active) vs BarcodeScannerManager (legacy tables)",
     "Consolidate on BarcodeReader"],
    ["Licensing salts", "Embedded in binary",
     "Deploy the activation server; treat client as advisory"],
], [26 * mm, 60 * mm, PW - 2 * M - 86 * mm]))
S.append(note("Recent hardening",
    "The checkout double-decrement, non-atomic sale, no-op tax settings, lost payment "
    "fields, unsalted hashing, license offline-grant bypass and tamper-deletion bypass have "
    "all been resolved in v2.0.0.", bg=colors.HexColor("#e9f5ee"), bar=TEAL))
S.append(PageBreak())

# ---- 12. EXTENSION GUIDE -----------------------------------------------------
S.append(Paragraph("12. Extension Guide", H1))
S.append(Paragraph("Add a database table / entity", H3))
S.append(bl([
    "Add the <font face='Courier'>CREATE TABLE IF NOT EXISTS</font> in the owning module "
    "(Database::createTables or the manager's createTableIfNotExist).",
    "For new columns on existing tables, use Database::ensureColumn(table, column, def).",
    "Expose typed DAO methods returning domain structs; keep all SQL parameterised.",
]))
S.append(Paragraph("Add a messaging provider", H3))
S.append(bl([
    "Implement the MessageProvider interface (sendMessage + isConfigured).",
    "Register it with ScheduleManager::registerProvider(); surface credentials in "
    "SettingsDialog and persist via QSettings.",
]))
S.append(Paragraph("Add a permission / role rule", H3))
S.append(bl([
    "Extend the Permission enum in user.h and describe it in RoleManager.",
    "Update RoleManager::hasPermission allow-lists; gate UI with checkPermission() and "
    "re-check inside the manager method.",
]))
S.append(Paragraph("Add a report or analytics widget", H3))
S.append(bl([
    "Add aggregation in AnalyticsManager/Database; build a QWidget chart in "
    "enhancedanalytics and register it in EnhancedAnalyticsDialog, or add a report type to "
    "ReportsDialog with CSV/PDF export.",
]))

S.append(Paragraph("Testing &amp; QA recommendations", H3))
S.append(bl([
    "High-value regression cases: checkout atomicity (force a mid-loop failure &rarr; "
    "expect rollback, intact cart, unchanged stock); oversell guard under low stock; "
    "tax inclusive vs exclusive vs disabled; refund double-submit; legacy-hash login "
    "upgrade; trial reset after registry wipe (breadcrumb should hold).",
    "To make these unit-testable, inject the DB connection / managers instead of using the "
    "singletons, and run against an in-memory SQLite database (':memory:').",
    "Add a QtTest target in CMake (enable_testing + add_test) for CI.",
]))

S.append(Paragraph("Glossary", H3))
S.append(tbl([
    ["Term", "Meaning"],
    ["DAO", "Data-access object &mdash; typed methods wrapping SQL"],
    ["RBAC", "Role-based access control (Admin/Manager/Cashier/Viewer)"],
    ["PBKDF2", "Password-Based Key Derivation Function 2 (salted, iterated hash)"],
    ["Z-Report", "End-of-day/shift sales &amp; cash reconciliation report"],
    ["X-Report", "Mid-shift read-only sales snapshot (no reset)"],
    ["Keyboard wedge", "USB scanner that emulates a keyboard typing the barcode"],
    ["Breadcrumb", "Signed out-of-registry file anchoring the trial install date"],
    ["windeployqt", "Qt tool that bundles runtime DLLs/plugins next to the exe"],
], [30 * mm, PW - 2 * M - 30 * mm]))

S.append(Spacer(1, 6 * mm))
S.append(HRFlowable(width="100%", thickness=1, color=INK, spaceAfter=6))
S.append(Paragraph(
    "KeynetikPOS v2.0.0 &mdash; Engineering Documentation. Generated from the application "
    "source; reflects the architecture at the time of writing.", SMALL))

Doc(OUT, pagesize=A4, title="KeynetikPOS Engineering Documentation",
    author="Keynetik Solutions").build(S)
print("Wrote", OUT)
