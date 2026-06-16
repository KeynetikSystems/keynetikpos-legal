# -*- coding: utf-8 -*-
"""Generate the KeynetikPOS System Documentation PDF (admin / IT audience)."""

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
    PageBreak, NextPageTemplate, ListFlowable, ListItem, HRFlowable,
)

OUT = "KeynetikPOS_System_Documentation.pdf"
RUNNING_TITLE = "System Documentation"

# ---------------------------------------------------------------- palette
NAVY   = colors.HexColor("#1b2a4a")
BLUE   = colors.HexColor("#2c5fa8")
ACCENT = colors.HexColor("#27ae60")
LIGHT  = colors.HexColor("#eef3fb")
GREYBG = colors.HexColor("#f5f7fa")
LINE   = colors.HexColor("#d4dcea")
DARK   = colors.HexColor("#22272e")
MUTED  = colors.HexColor("#5a6473")
WARNBG = colors.HexColor("#fdf3e7")
WARNBAR = colors.HexColor("#e08a1e")

# ---------------------------------------------------------------- styles
ss = getSampleStyleSheet()


def style(name, **kw):
    return ParagraphStyle(name, parent=ss["Normal"], **kw)


H1 = style("H1", fontName="Helvetica-Bold", fontSize=18, textColor=NAVY,
           spaceBefore=6, spaceAfter=10, leading=22)
H2 = style("H2", fontName="Helvetica-Bold", fontSize=13, textColor=BLUE,
           spaceBefore=14, spaceAfter=6, leading=16)
H3 = style("H3", fontName="Helvetica-Bold", fontSize=10.5, textColor=DARK,
           spaceBefore=8, spaceAfter=3, leading=13)
BODY = style("Body", fontName="Helvetica", fontSize=9.5, textColor=DARK,
             leading=14, alignment=TA_JUSTIFY, spaceAfter=5)
BULLET = style("Bullet", fontName="Helvetica", fontSize=9.3, textColor=DARK,
               leading=13.5)
SMALL = style("Small", fontName="Helvetica", fontSize=8.2, textColor=MUTED,
              leading=11)
MONO = style("Mono", fontName="Courier", fontSize=8.3, textColor=DARK, leading=12)
CELL = style("Cell", fontName="Helvetica", fontSize=8.6, textColor=DARK, leading=11.5)
CELLB = style("CellB", fontName="Helvetica-Bold", fontSize=8.6, textColor=NAVY, leading=11.5)
CELLH = style("CellH", fontName="Helvetica-Bold", fontSize=8.8, textColor=colors.white, leading=11.5)
CELLC = style("CellC", fontName="Helvetica-Bold", fontSize=8.6, textColor=ACCENT,
              leading=11.5, alignment=TA_CENTER)
TOC = style("TOC", fontName="Helvetica", fontSize=10, textColor=DARK, leading=17)
TOCNUM = style("TOCnum", fontName="Helvetica-Bold", fontSize=10, textColor=BLUE, leading=17)

COVER_TITLE = style("CoverTitle", fontName="Helvetica-Bold", fontSize=30,
                    textColor=colors.white, alignment=TA_CENTER, leading=34)
COVER_SUB = style("CoverSub", fontName="Helvetica", fontSize=13,
                  textColor=colors.HexColor("#cdd9ef"), alignment=TA_CENTER, leading=18)
COVER_SMALL = style("CoverSmall", fontName="Helvetica", fontSize=10,
                    textColor=colors.HexColor("#9fb3d6"), alignment=TA_CENTER, leading=14)

# ---------------------------------------------------------------- doc furniture
PW, PH = A4
MARGIN = 18 * mm


class Doc(BaseDocTemplate):
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        frame = Frame(MARGIN, MARGIN, PW - 2 * MARGIN, PH - 2 * MARGIN - 6 * mm, id="body")
        cover = Frame(0, 0, PW, PH, id="cover",
                      leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([
            PageTemplate(id="cover", frames=[cover], onPage=self._cover_bg),
            PageTemplate(id="body", frames=[frame], onPage=self._chrome),
        ])

    def _cover_bg(self, canvas, doc):
        canvas.saveState()
        canvas.setFillColor(NAVY)
        canvas.rect(0, 0, PW, PH, fill=1, stroke=0)
        canvas.setFillColor(BLUE)
        canvas.rect(0, PH - 70 * mm, PW, 4 * mm, fill=1, stroke=0)
        canvas.setFillColor(ACCENT)
        canvas.rect(0, PH - 70 * mm - 1.4 * mm, PW, 1.4 * mm, fill=1, stroke=0)
        canvas.restoreState()

    def _chrome(self, canvas, doc):
        canvas.saveState()
        canvas.setFillColor(NAVY)
        canvas.rect(0, PH - 12 * mm, PW, 12 * mm, fill=1, stroke=0)
        canvas.setFillColor(colors.white)
        canvas.setFont("Helvetica-Bold", 8.5)
        canvas.drawString(MARGIN, PH - 8 * mm, "KeynetikPOS")
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(colors.HexColor("#b9c6e0"))
        canvas.drawRightString(PW - MARGIN, PH - 8 * mm, RUNNING_TITLE)
        canvas.setStrokeColor(LINE)
        canvas.setLineWidth(0.5)
        canvas.line(MARGIN, 13 * mm, PW - MARGIN, 13 * mm)
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(MUTED)
        canvas.drawString(MARGIN, 9 * mm, "Next-GEN Professional Point of Sale System")
        canvas.drawRightString(PW - MARGIN, 9 * mm, "Page %d" % doc.page)
        canvas.restoreState()


# ---------------------------------------------------------------- helpers
def bullets(items, st=BULLET):
    return ListFlowable(
        [ListItem(Paragraph(t, st), leftIndent=10, value="•") for t in items],
        bulletType="bullet", start="•", leftIndent=14, bulletColor=BLUE,
        bulletFontSize=7, spaceBefore=1, spaceAfter=1,
    )


def table(rows, col_widths, header=True, center_cols=()):
    data = []
    for i, r in enumerate(rows):
        if header and i == 0:
            data.append([Paragraph(c, CELLH) for c in r])
        else:
            row = []
            for j, c in enumerate(r):
                if j == 0:
                    row.append(Paragraph(c, CELLB))
                elif j in center_cols:
                    row.append(Paragraph(c, CELLC))
                else:
                    row.append(Paragraph(c, CELL))
            data.append(row)
    t = Table(data, colWidths=col_widths, repeatRows=1 if header else 0)
    cmds = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
        ("ROWBACKGROUNDS", (0, 1 if header else 0), (-1, -1), [colors.white, GREYBG]),
    ]
    if header:
        cmds += [
            ("BACKGROUND", (0, 0), (-1, 0), BLUE),
            ("TOPPADDING", (0, 0), (-1, 0), 6),
            ("BOTTOMPADDING", (0, 0), (-1, 0), 6),
        ]
    t.setStyle(TableStyle(cmds))
    return t


def callout(title, text, bg=LIGHT, bar=BLUE):
    inner = [Paragraph("<b>%s</b>" % title, style("coT", fontName="Helvetica-Bold",
             fontSize=9.5, textColor=NAVY, leading=13, spaceAfter=2))]
    if text:
        inner.append(Paragraph(text, style("coB", fontName="Helvetica", fontSize=9,
                     textColor=DARK, leading=13)))
    box = Table([[inner]], colWidths=[PW - 2 * MARGIN - 8])
    box.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), bg),
        ("LEFTPADDING", (0, 0), (-1, -1), 12),
        ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 8),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 8),
        ("LINEBEFORE", (0, 0), (0, -1), 3, bar),
    ]))
    return box


def code(lines):
    body = "<br/>".join(lines)
    box = Table([[Paragraph(body, MONO)]], colWidths=[PW - 2 * MARGIN - 8])
    box.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#1e2430")),
        ("LEFTPADDING", (0, 0), (-1, -1), 10),
        ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    # override text colour to light for code box
    box._argW  # touch
    return box


def codebox(lines):
    light = style("MonoL", fontName="Courier", fontSize=8.2,
                  textColor=colors.HexColor("#e6edf6"), leading=12)
    body = "<br/>".join(lines)
    box = Table([[Paragraph(body, light)]], colWidths=[PW - 2 * MARGIN - 8])
    box.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#1e2430")),
        ("LEFTPADDING", (0, 0), (-1, -1), 10),
        ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    return box


def rule():
    return HRFlowable(width="100%", thickness=0.6, color=LINE, spaceBefore=4, spaceAfter=8)


story = []

# ---------------------------------------------------------------- COVER
story.append(Spacer(1, 74 * mm))
story.append(Paragraph("KeynetikPOS", COVER_TITLE))
story.append(Spacer(1, 6 * mm))
story.append(Paragraph("System Documentation", COVER_SUB))
story.append(Spacer(1, 3 * mm))
story.append(Paragraph("Installation &middot; Architecture &middot; Administration &middot; Operations",
                       COVER_SMALL))
story.append(Spacer(1, 60 * mm))
story.append(Paragraph("Version 2.0.0", COVER_SUB))
story.append(Spacer(1, 2 * mm))
story.append(Paragraph("Keynetik Solutions  &bull;  Qt 6 / C++17  &bull;  Windows",
                       COVER_SMALL))
story.append(NextPageTemplate("body"))
story.append(PageBreak())

# ---------------------------------------------------------------- INTRO + TOC
story.append(Paragraph("About this document", H1))
story.append(Paragraph(
    "This document describes KeynetikPOS as built and deployed: what it requires, how it "
    "is installed and configured, where its data lives, how licensing works, and how to "
    "operate, back up, and troubleshoot it. The audience is installers, system "
    "administrators and IT support staff.", BODY))
story.append(Paragraph(
    "For a code-level walkthrough of every class see <b>CODE_DOCUMENTATION.md</b>; for "
    "day-to-day cashier and manager usage see the <b>User Guide</b>.", BODY))

story.append(rule())
story.append(Paragraph("Contents", H2))
toc_items = [
    ("1", "System overview"),
    ("2", "System requirements"),
    ("3", "Architecture"),
    ("4", "Data model & storage"),
    ("5", "Installation & deployment"),
    ("6", "Licensing system"),
    ("7", "Peripheral configuration"),
    ("8", "Administration"),
    ("9", "Backup & recovery"),
    ("10", "Security considerations"),
    ("11", "Known limitations & technical debt"),
    ("12", "Troubleshooting"),
    ("13", "Reference"),
]
toc_rows = [[Paragraph(n, TOCNUM), Paragraph(t, TOC)] for n, t in toc_items]
toc = Table(toc_rows, colWidths=[12 * mm, PW - 2 * MARGIN - 12 * mm])
toc.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"),
                         ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
                         ("TOPPADDING", (0, 0), (-1, -1), 2)]))
story.append(toc)
story.append(PageBreak())

# ---------------------------------------------------------------- 1 OVERVIEW
story.append(Paragraph("1. System overview", H1))
story.append(Paragraph(
    "KeynetikPOS is a single-machine, offline-first point-of-sale application for small "
    "and mid-size retailers, built with the Kenyan market in mind (KSh currency, 16% VAT "
    "context, M-Pesa mobile money, WhatsApp/SMS reporting). All business data lives in a "
    "local SQLite database on the terminal; the only external dependencies are optional: "
    "an online license server (activation/heartbeat) and optional messaging gateways "
    "(Twilio WhatsApp, Africa's Talking SMS) for scheduled reports.", BODY))
story.append(Paragraph(
    "The application is self-contained: it bootstraps its own database schema on first "
    "run, seeds a default administrator account, and needs no separate database server, "
    "web server, or runtime install beyond the bundled Qt DLLs.", BODY))
story.append(Paragraph("Capability summary", H3))
story.append(table([
    ["Area", "Capabilities"],
    ["Sales", "Product grid + search, barcode scanning, multiple carts, PIN-gated "
     "discounts, cash/card/mobile/split payments, receipt printing / PDF."],
    ["Inventory", "Catalog, stock levels, low/critical alerts, restock, CSV "
     "import/export, audit-logged adjustments."],
    ["Users", "RBAC (Admin/Manager/Cashier/Viewer), PBKDF2 passwords, forced password "
     "change, user-action audit log."],
    ["Reporting", "Analytics dashboards, operational reports (CSV/PDF), sales history, "
     "refunds, end-of-day Z-Reports."],
    ["Shifts", "Open/close shifts with counted cash floats, over/short reconciliation."],
    ["Messaging", "Scheduled Z-Report / inventory / sales reports via WhatsApp or SMS."],
    ["Licensing", "30-day trial, CD-key activation (online + offline fallback), device "
     "limits, revocation, tamper detection."],
], [26 * mm, PW - 2 * MARGIN - 26 * mm]))
story.append(PageBreak())

# ---------------------------------------------------------------- 2 REQUIREMENTS
story.append(Paragraph("2. System requirements", H1))
story.append(Paragraph("Hardware", H3))
story.append(table([
    ["Item", "Minimum", "Recommended"],
    ["CPU", "Dual-core x86-64", "Quad-core"],
    ["RAM", "2 GB", "4 GB+"],
    ["Disk", "300 MB free + DB growth", "1 GB+ free"],
    ["Display", "1024 x 640 (enforced min)", "1366 x 768 or larger"],
], [30 * mm, 55 * mm, PW - 2 * MARGIN - 85 * mm]))
story.append(Paragraph("Software", H3))
story.append(bullets([
    "<b>Operating system:</b> Windows 10 or 11 (64-bit). Verified on Windows 11.",
    "<b>Runtime:</b> none to install separately &mdash; the installer bundles all required "
    "Qt 6 runtime DLLs via windeployqt. No system-wide Qt install is needed on the target.",
]))
story.append(Paragraph("Optional peripherals", H3))
story.append(bullets([
    "<b>Barcode scanner</b> &mdash; USB keyboard-wedge (HID, no driver) or serial / "
    "virtual-COM (RS-232). Both supported (see &sect;7).",
    "<b>Printer</b> &mdash; any Windows printer; receipts can also export as PDF with no "
    "printer at all.",
]))
story.append(Paragraph("Network", H3))
story.append(bullets([
    "<b>Not required for normal operation</b> &mdash; the POS runs fully offline.",
    "Internet is used only for (a) license activation/heartbeat and (b) sending scheduled "
    "WhatsApp/SMS reports, if configured. Offline terminals fall back to local license "
    "validation and skip report sending.",
]))
story.append(PageBreak())

# ---------------------------------------------------------------- 3 ARCHITECTURE
story.append(Paragraph("3. Architecture", H1))
story.append(Paragraph("3.1 Process & startup sequence", H3))
story.append(Paragraph(
    "KeynetikPOS is a single-process desktop application. <b>main()</b> runs a series of "
    "sequential gates at startup, each protecting the next:", BODY))
story.append(codebox([
    "Anti-tamper  -&gt;  License validation  -&gt;  Database init  -&gt;  Login (max 3)",
    "   -&gt;  Forced password change (if required)  -&gt;  MainWindow",
    "   -&gt;  (deferred ~2s) online license heartbeat",
]))
story.append(bullets([
    "<b>Anti-tamper</b> &mdash; light, debugger-only check on release builds; generic error "
    "and exit if a debugger is attached.",
    "<b>License validation</b> &mdash; strictly offline and non-blocking at startup "
    "(&sect;6). Trial/expired/invalid states prompt for a CD key.",
    "<b>Database init</b> &mdash; opens/creates the SQLite DB, builds schema, seeds sample "
    "products and the default admin on a fresh install.",
    "<b>Login</b> &mdash; capped at 3 attempts; a must-change-password account is forced "
    "through a password change before the main window opens.",
    "<b>Deferred heartbeat</b> &mdash; online revalidation runs ~2s after the UI shows, so "
    "a slow/absent network never freezes startup.",
]))
story.append(Paragraph(
    "Logout does <b>not</b> restart the process &mdash; it closes the main window and loops "
    "back to the login dialog within the same process.", BODY))
story.append(Paragraph("3.2 Layered design", H3))
story.append(bullets([
    "<b>Singletons</b> for process-wide services: Database, UserManager, LicenseManager, "
    "ThemeManager &mdash; one instance, one source of truth.",
    "<b>Managers</b> (QObject) hold business logic + persistence; <b>Dialogs</b> (QDialog) "
    "hold UI only and delegate to managers.",
    "<b>Self-bootstrapping schema</b> &mdash; each manager creates its own tables on "
    "construction and adds missing columns via a lightweight ensureColumn() migration.",
    "<b>Parameterised SQL everywhere</b> &mdash; prepared statements with bound values "
    "(SQL-injection safe).",
    "Single-threaded DB access; SQLite runs in WAL mode with a busy timeout for resilience.",
]))
story.append(Paragraph("3.3 Technology stack", H3))
story.append(table([
    ["Layer", "Technology"],
    ["Language", "C++17"],
    ["UI / framework", "Qt 6 (Widgets, Core, Sql, PrintSupport, Network, SerialPort)"],
    ["Database", "SQLite (via Qt SQL, WAL journal mode)"],
    ["Build", "CMake >= 3.16, MinGW or MSVC toolchain"],
    ["Packaging", "CPack + NSIS installer, windeployqt for runtime DLLs"],
    ["License server", "Cloudflare Worker + D1 (separate, in license-server/)"],
], [34 * mm, PW - 2 * MARGIN - 34 * mm]))
story.append(PageBreak())

# ---------------------------------------------------------------- 4 DATA MODEL
story.append(Paragraph("4. Data model & storage", H1))
story.append(Paragraph("4.1 Where data lives", H3))
story.append(table([
    ["Data", "Location"],
    ["Business database", "pos_database.db in the per-user AppData directory "
     "(QStandardPaths::AppDataLocation), falling back to the executable directory."],
    ["License state", "Windows registry HKCU\\Software\\KeynetikSolutions\\KeynetikPOS"
     "\\License, plus a machine-bound breadcrumb file in the user config dir."],
    ["Theme / receipt / messaging keys", "QSettings under org “KeynetikPOS”."],
    ["Generated receipts", "Receipt_&lt;saleId&gt;_&lt;timestamp&gt;.pdf (current build "
     "emits PDF)."],
], [42 * mm, PW - 2 * MARGIN - 42 * mm]))
story.append(callout(
    "Per-user database",
    "The database lives per Windows user under AppData, not in Program Files (read-only "
    "for non-admins). If multiple Windows accounts share the terminal, each gets its own "
    "database unless you relocate it.", bg=GREYBG, bar=ACCENT))
story.append(Paragraph("4.2 Core tables (created automatically)", H3))
story.append(table([
    ["Table", "Purpose"],
    ["products", "Catalog: name, category, price, cost_price, profit_margin, "
     "stock_quantity, barcode, is_active."],
    ["sales", "Transaction headers: total, tax, discount, payment_method, amount_paid, "
     "change_due, sale_date."],
    ["sale_items", "Line items with snapshots of name/price/cost at sale time."],
    ["users", "Accounts: username, password_hash (PBKDF2), full_name, email, role, "
     "is_active, must_change_password."],
    ["stock_adjustments", "Audit log of manual stock changes (reason, who, when)."],
    ["refunds", "Refund records linked to the original sale."],
    ["AppSettings", "Key/value business settings (currency, tax, hashed discount PIN...)."],
    ["Shifts", "Cashier, opening/closing float, running totals, open flag."],
    ["payments", "Per-tender payment records (supports split tender)."],
    ["message_schedules", "Scheduled report definitions."],
], [34 * mm, PW - 2 * MARGIN - 34 * mm]))
story.append(Paragraph(
    "Key choices: sale items <b>snapshot</b> product name/price/cost so historical reports "
    "and profit math stay correct after products change; money is stored as REAL (double) "
    "by current design &mdash; see &sect;11.", BODY))
story.append(PageBreak())

# ---------------------------------------------------------------- 5 INSTALL
story.append(Paragraph("5. Installation & deployment", H1))
story.append(Paragraph("5.1 Installing on a terminal", H3))
story.append(Paragraph(
    "The standard deliverable is an NSIS installer: <b>KeynetikPOS-2.0.0-Setup.exe</b>.", BODY))
story.append(bullets([
    "Run the installer (admin rights recommended so shortcuts are created system-wide).",
    "It installs the executable plus bundled Qt runtime DLLs, creates Start-menu and "
    "desktop shortcuts, and can launch on finish.",
    "On first launch the app validates its license (or starts a 30-day trial), builds its "
    "database, and creates the default admin account (&sect;8).",
]))
story.append(Paragraph("5.2 Building from source", H3))
story.append(Paragraph(
    "Prerequisites: Qt 6.11 (MinGW or MSVC), CMake >= 3.16, a C++17 compiler. Verified "
    "Release build (MinGW, PowerShell):", BODY))
story.append(codebox([
    "$env:PATH = \"C:\\Qt\\Tools\\mingw1310_64\\bin;\" + `",
    "            \"C:\\Qt\\6.11.0\\mingw_64\\bin;\" + $env:PATH",
    "cmake --build \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\"",
]))
story.append(Paragraph(
    "Required Qt components: Widgets, Core, Sql, PrintSupport, Network, SerialPort (and "
    "Test for the unit tests).", BODY))
story.append(Paragraph("5.3 Packaging the installer", H3))
story.append(Paragraph(
    "When LICENSE.txt exists, CPack produces an NSIS installer with icon, shortcuts and "
    "launch-after-install; windeployqt stages the exe with all runtime DLLs.", BODY))
story.append(codebox(["cpack -G NSIS   # from the build directory"]))
story.append(Paragraph("5.4 Tests", H3))
story.append(Paragraph(
    "A QtTest target (carttotals_test) covers the pure cart-totals math (tax, discount, "
    "rounding &mdash; 15 cases). Enabled by default (-DKEYNETIK_BUILD_TESTS=ON).", BODY))
story.append(codebox([
    "cmake --build &lt;build-dir&gt; --target carttotals_test",
    "&amp; \"&lt;build-dir&gt;\\carttotals_test.exe\" -o results.txt,txt",
    "ctest --test-dir &lt;build-dir&gt;",
]))
story.append(PageBreak())

# ---------------------------------------------------------------- 6 LICENSING
story.append(Paragraph("6. Licensing system", H1))
story.append(Paragraph(
    "KeynetikPOS uses an offline-first licensing model so a shop with no internet is never "
    "locked out, while still enforcing device limits and revocation when online.", BODY))
story.append(Paragraph("6.1 License states & flow", H3))
story.append(table([
    ["State", "Meaning"],
    ["Trial", "30-day evaluation (TRIAL_DAYS=30). Nags at first run and when <= 7 days "
     "remain."],
    ["FullLicense", "Activated with a valid CD key."],
    ["TrialExpired", "Trial used up &mdash; prompts for a CD key."],
    ["InvalidKey", "Stored license invalid or revoked &mdash; prompts for a CD key."],
], [34 * mm, PW - 2 * MARGIN - 34 * mm]))
story.append(bullets([
    "<b>Activation</b> is online-first (activateOnServer): the server enforces device "
    "limits and revocation. If unreachable, the app falls back to local format validation "
    "(activateKey) so offline shops can still activate; local activations are never "
    "heartbeat-checked.",
    "<b>Heartbeat</b> (startOnlineHeartbeat) runs ~2s after the UI is up, asynchronously "
    "revalidates server-activated keys, and tracks offline days against a 7-day grace "
    "period (MAX_OFFLINE_DAYS).",
]))
story.append(Paragraph("6.2 Anti-tamper", H3))
story.append(bullets([
    "License state in HKCU is covered by a salted SHA-256 hash; hand-editing the registry "
    "trips wasTampered() and blocks startup.",
    "A second machine-bound signed copy of the install date (a hidden breadcrumb file) "
    "prevents trial resets by wiping the registry alone &mdash; startup takes the earliest "
    "of the two dates.",
]))
story.append(callout(
    "Operational caution",
    "Changing TAMPER_SALT or the registry-hash formula after shipping will brick "
    "legitimately installed copies and requires a migration. A local tamper lockout can be "
    "cleared by deleting the HKCU ...\\KeynetikPOS\\License key (resets activation, not the "
    "trial breadcrumb).", bg=WARNBG, bar=WARNBAR))
story.append(Paragraph("6.3 License server (deployed)", H3))
story.append(bullets([
    "Cloudflare Worker + D1 (code in license-server/, free tier).",
    "<b>Live endpoint:</b> https://keynetik-license.kelvinyabate.workers.dev "
    "(SERVER_URL in licensemanager.cpp).",
    "Admin ops (mint/revoke/list) require a bearer ADMIN_TOKEN (Cloudflare Worker secret); "
    "admin script license-server/admin.ps1, schema license-server/schema.sql.",
    "Key minting: tools/generate-keys.ps1. Keys are XXXX-XXXX-XXXX-XXXX with a checksum "
    "segment.",
    "Server reasons returned to the client: device_limit_reached, key_revoked, key_expired.",
]))
story.append(callout(
    "Wrangler & token notes",
    "Wrangler (Cloudflare CLI) needs Node 22+. The ADMIN_TOKEN must be set with no "
    "trailing newline (a shell-pipe newline previously caused a 401; the server trims it "
    "defensively).", bg=GREYBG, bar=BLUE))
story.append(PageBreak())

# ---------------------------------------------------------------- 7 PERIPHERALS
story.append(Paragraph("7. Peripheral configuration", H1))
story.append(Paragraph("7.1 Barcode scanners", H3))
story.append(Paragraph(
    "BarcodeReader supports both common scanner types behind one signal (barcodeScanned):",
    BODY))
story.append(bullets([
    "<b>Keyboard-wedge (USB HID)</b> &mdash; the scanner types the code + Enter. Installed "
    "as an app-wide Qt event filter; an inter-character timer (~100 ms) distinguishes a "
    "scan burst from human typing. No driver, no config for most scanners.",
    "<b>Serial (RS-232 / virtual COM)</b> &mdash; default 9600-8-N-1; available COM ports "
    "are enumerated for the settings UI. Configurable prefix/suffix stripping removes "
    "STX/ETX framing bytes.",
]))
story.append(Paragraph(
    "The app can also generate and print EAN-13 labels (valid check digits) for unlabelled "
    "stock, plus Code128/QR strings &mdash; no GS1 subscription required.", BODY))
story.append(Paragraph("7.2 Receipts & printing", H3))
story.append(Paragraph(
    "ReceiptPrinter builds receipts as fixed-width text for 40-column thermal printers and "
    "as styled HTML rendered via QPrinter for standard printers and PDF export. <b>In the "
    "current build receipts are emitted as PDF by default</b> "
    "(Receipt_&lt;saleId&gt;_&lt;timestamp&gt;.pdf), usable with zero printer setup. "
    "Company header/footer and printer config persist in QSettings.", BODY))
story.append(PageBreak())

# ---------------------------------------------------------------- 8 ADMIN
story.append(Paragraph("8. Administration", H1))
story.append(Paragraph("8.1 First-run default account", H3))
story.append(table([
    ["Field", "Value"],
    ["Username", "admin"],
    ["Password", "admin123"],
    ["Role", "Admin"],
    ["Flag", "must_change_password = true (new password required at first login)"],
], [34 * mm, PW - 2 * MARGIN - 34 * mm]))
story.append(callout(
    "Change this immediately",
    "The default password is published in source and logs. The first login forces a "
    "password change; do not skip or share it.", bg=WARNBG, bar=WARNBAR))
story.append(Paragraph("8.2 Roles & permissions (RBAC)", H3))
story.append(table([
    ["Permission", "Admin", "Mgr", "Cashier", "Viewer"],
    ["Make / view sales", "Yes", "Yes", "Yes", "view"],
    ["Apply discounts", "Yes", "Yes", "Yes", "-"],
    ["Void / delete transactions", "Yes", "void", "-", "-"],
    ["View inventory", "Yes", "Yes", "Yes", "Yes"],
    ["Add/edit products, adjust stock", "Yes", "Yes", "-", "-"],
    ["Delete products", "Yes", "-", "-", "-"],
    ["View reports / analytics", "Yes", "Yes", "-", "Yes"],
    ["Export reports", "Yes", "Yes", "-", "-"],
    ["Manage users", "Yes", "-", "-", "-"],
    ["View users", "Yes", "Yes", "-", "-"],
    ["Settings, backup, view logs", "Yes", "-", "-", "-"],
], [58 * mm, 24 * mm, 18 * mm, 24 * mm, 24 * mm],
    center_cols=(1, 2, 3, 4)))
story.append(Paragraph("8.3 Password & account policy", H3))
story.append(bullets([
    "Passwords hashed with salted PBKDF2-SHA256 (100,000 iterations, 16-byte OS-random "
    "salt). Legacy unsalted SHA-256 hashes are transparently rehashed on next login.",
    "Minimum password length 8; new passwords must differ from the old.",
    "Admin-set passwords are temporary (must_change_password).",
    "Logins, password changes and significant user actions are audit-logged.",
]))
story.append(Paragraph("8.4 Business settings & 8.5 Scheduled messaging", H3))
story.append(bullets([
    "Settings (Admin only): business identity, currency (default KSh / KES), receipt "
    "footer, tax (rate/label, inclusive pricing), receipt-print toggle, hashed discount "
    "PIN, and messaging credentials. Stored in the AppSettings table so they travel with a "
    "backup; saving triggers a live refresh.",
    "ScheduleManager polls once a minute and fires due schedules (Daily/Weekly/Monthly/"
    "OnShiftClose/OnSalesThreshold), generating and dispatching reports through the chosen "
    "provider. Schedules and lastSent timestamps persist; “Send Now” tests "
    "credentials immediately.",
]))
story.append(PageBreak())

# ---------------------------------------------------------------- 9 BACKUP
story.append(Paragraph("9. Backup & recovery", H1))
story.append(Paragraph(
    "The entire business state is in one file: <b>pos_database.db</b> (plus the WAL/SHM "
    "sidecar files SQLite may create alongside it).", BODY))
story.append(Paragraph("9.1 Backup procedure", H3))
story.append(bullets([
    "Close KeynetikPOS (checkpoints WAL; no write mid-flight).",
    "Locate the database in the per-user AppData directory (under ...\\AppData\\...\\"
    "KeynetikPOS\\).",
    "Copy pos_database.db (and any -wal / -shm files if present) to a safe location "
    "&mdash; external drive, network share, or cloud folder.",
]))
story.append(Paragraph(
    "Because all business settings live inside the database, this single copy captures "
    "products, sales, users, settings, shifts and schedules.", BODY))
story.append(Paragraph("9.2 Restore", H3))
story.append(Paragraph(
    "Close the app, replace pos_database.db in AppData with the backup, relaunch. Don't mix "
    "a database with mismatched WAL sidecars &mdash; copy them together or checkpoint first "
    "by opening/closing cleanly.", BODY))
story.append(Paragraph("9.3 What is not in the database", H3))
story.append(bullets([
    "License state (registry + breadcrumb) &mdash; re-activation may be needed on a new "
    "machine, subject to the key's device limit.",
    "Theme choice and printer/messaging org keys stored in QSettings.",
]))
story.append(callout("Simplest policy",
                     "Schedule an OS-level backup of the AppData folder.", bg=GREYBG, bar=ACCENT))
story.append(PageBreak())

# ---------------------------------------------------------------- 10 SECURITY
story.append(Paragraph("10. Security considerations", H1))
story.append(bullets([
    "<b>Authentication:</b> PBKDF2-hashed passwords with constant-time comparison; login "
    "attempts capped at 3 per session.",
    "<b>Authorization:</b> RBAC enforced in the UI (disabling controls) and in managers "
    "(permission re-checks). Discounts and refunds are PIN/permission-gated and "
    "audit-logged.",
    "<b>SQL injection:</b> prevented via prepared statements throughout.",
    "<b>Audit trails:</b> user actions, stock adjustments, discounts and refunds logged "
    "with who/when.",
    "<b>Anti-tamper / licensing:</b> the in-app checks are an obfuscation/format gate "
    "&mdash; the license server is the real authority; client-side TAMPER_SALT and hash "
    "are deterrents, not cryptographic security.",
    "<b>Data at rest:</b> the SQLite database is not encrypted. Rely on Windows account "
    "permissions and disk encryption (BitLocker) for confidentiality.",
]))
story.append(rule())

# ---------------------------------------------------------------- 11 LIMITATIONS
story.append(Paragraph("11. Known limitations & technical debt", H1))
story.append(bullets([
    "<b>Money stored as double/REAL</b>, not integer cents. Totals math is unit-tested and "
    "“contained” (a characterization test pins the rounding artifact, e.g. "
    "roundCents(1.005) -&gt; 1.00), but a full integer-cents migration is deferred.",
    "<b>Single-machine, single-process, single-threaded</b> &mdash; no multi-terminal "
    "networking or concurrent DB writers. WAL + busy-timeout cover crash resilience, not "
    "multi-user concurrency.",
    "<b>Overlapping UIs kept in the build</b> &mdash; three analytics screens and two "
    "inventory screens overlap in scope; consolidation is a future cleanup.",
    "<b>Inventory reorder levels</b> are not persisted per-product in the database.",
    "<b>Email receipts</b> are a stub; WhatsApp/SMS are the live channels.",
    "<b>No built-in automated backup</b> &mdash; backups are a manual/OS-level procedure "
    "(&sect;9).",
]))
story.append(PageBreak())

# ---------------------------------------------------------------- 12 TROUBLESHOOTING
story.append(Paragraph("12. Troubleshooting", H1))
story.append(table([
    ["Symptom", "Likely cause", "Resolution"],
    ["“License data has been modified...” at startup",
     "Registry tamper-hash mismatch (salt/formula change or manual edit)",
     "Delete HKCU ...\\KeynetikPOS\\License and re-activate."],
    ["“Error code: 0x00000005” and exit",
     "Anti-tamper tripped (debugger attached) on a release build",
     "Close debuggers/diagnostic tools and relaunch."],
    ["Trial expired sooner than expected / won't reset",
     "Breadcrumb holds the earliest install date (by design)",
     "Trial resets are intentionally prevented; obtain a CD key."],
    ["Activation fails offline with “device limit”",
     "Reached the server; key at its device cap",
     "Free a device via admin (admin.ps1) or use another key."],
    ["Scheduled reports never send",
     "Missing/wrong messaging credentials, or terminal offline at fire time",
     "Use “Send Now” to test; verify provider keys in Settings."],
    ["Scans go to wrong field / nothing happens",
     "Wedge timing, or wrong serial port",
     "Confirm scanner mode; pick the correct COM port; check prefix/suffix."],
    ["Forgot the admin password",
     "No self-service reset for the last admin",
     "Restore from a pre-change backup, or reset via direct DB access."],
    ["Database “failed to initialize”",
     "AppData not writable / disk full / locked DB",
     "Check disk space and permissions; ensure no second instance holds the file."],
    ["Receipts not printing",
     "PDF-by-default in current build",
     "Find the generated Receipt_*.pdf; configure a printer for hardware printing."],
], [42 * mm, 50 * mm, PW - 2 * MARGIN - 92 * mm]))
story.append(callout(
    "Admin token storage",
    "The deployed ADMIN_TOKEN was written to %TEMP%\\knk_admin_token.txt on the deployer's "
    "machine &mdash; move it to a password manager and delete the temp copy.",
    bg=WARNBG, bar=WARNBAR))
story.append(PageBreak())

# ---------------------------------------------------------------- 13 REFERENCE
story.append(Paragraph("13. Reference", H1))
story.append(bullets([
    "<b>Code-level documentation:</b> CODE_DOCUMENTATION.md (repo root).",
    "<b>End-user manual:</b> USER_GUIDE.md / KeynetikPOS_User_Guide.pdf (this folder).",
    "<b>Engineering / feature PDFs:</b> KeynetikPOS_Engineering_Documentation.pdf, "
    "KeynetikPOS_Feature_Documentation.pdf (this folder).",
    "<b>License server:</b> license-server/README.md.",
    "<b>Support:</b> support@keynetik.com.",
]))
story.append(Spacer(1, 6 * mm))
story.append(HRFlowable(width="100%", thickness=1, color=NAVY, spaceAfter=6))
story.append(Paragraph(
    "KeynetikPOS v2.0.0 &mdash; Keynetik Solutions. This document reflects the system as "
    "built; verify version-specific details (default credentials, server URLs, key salts) "
    "against the current source before relying on them in production.", SMALL))

# ----------------------------------------------------------------
Doc(OUT, pagesize=A4, title="KeynetikPOS System Documentation",
    author="Keynetik Solutions").build(story)
print("Wrote", OUT)
