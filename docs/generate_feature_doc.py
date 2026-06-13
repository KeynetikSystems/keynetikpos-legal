# -*- coding: utf-8 -*-
"""Generate the KeynetikPOS feature documentation PDF."""

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_JUSTIFY
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
    PageBreak, NextPageTemplate, KeepTogether, ListFlowable, ListItem, HRFlowable,
)

OUT = "KeynetikPOS_Feature_Documentation.pdf"

# ----------------------------------------------------------------------------
# Palette
# ----------------------------------------------------------------------------
NAVY   = colors.HexColor("#1b2a4a")
BLUE   = colors.HexColor("#2c5fa8")
ACCENT = colors.HexColor("#27ae60")
LIGHT  = colors.HexColor("#eef3fb")
GREYBG = colors.HexColor("#f5f7fa")
LINE   = colors.HexColor("#d4dcea")
DARK   = colors.HexColor("#22272e")
MUTED  = colors.HexColor("#5a6473")

# ----------------------------------------------------------------------------
# Styles
# ----------------------------------------------------------------------------
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
CELL = style("Cell", fontName="Helvetica", fontSize=8.6, textColor=DARK, leading=11.5)
CELLB = style("CellB", fontName="Helvetica-Bold", fontSize=8.6, textColor=NAVY, leading=11.5)
CELLH = style("CellH", fontName="Helvetica-Bold", fontSize=8.8, textColor=colors.white, leading=11.5)
TOC = style("TOC", fontName="Helvetica", fontSize=10, textColor=DARK, leading=18)
TOCNUM = style("TOCnum", fontName="Helvetica-Bold", fontSize=10, textColor=BLUE, leading=18)

COVER_TITLE = style("CoverTitle", fontName="Helvetica-Bold", fontSize=30,
                    textColor=colors.white, alignment=TA_CENTER, leading=34)
COVER_SUB = style("CoverSub", fontName="Helvetica", fontSize=13,
                  textColor=colors.HexColor("#cdd9ef"), alignment=TA_CENTER, leading=18)
COVER_SMALL = style("CoverSmall", fontName="Helvetica", fontSize=10,
                    textColor=colors.HexColor("#9fb3d6"), alignment=TA_CENTER, leading=14)

# ----------------------------------------------------------------------------
# Document with page furniture
# ----------------------------------------------------------------------------
PW, PH = A4
MARGIN = 18 * mm


class Doc(BaseDocTemplate):
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        frame = Frame(MARGIN, MARGIN, PW - 2 * MARGIN, PH - 2 * MARGIN - 6 * mm,
                      id="body")
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
        # header band
        canvas.setFillColor(NAVY)
        canvas.rect(0, PH - 12 * mm, PW, 12 * mm, fill=1, stroke=0)
        canvas.setFillColor(colors.white)
        canvas.setFont("Helvetica-Bold", 8.5)
        canvas.drawString(MARGIN, PH - 8 * mm, "KeynetikPOS")
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(colors.HexColor("#b9c6e0"))
        canvas.drawRightString(PW - MARGIN, PH - 8 * mm, "Feature Documentation")
        # footer
        canvas.setStrokeColor(LINE)
        canvas.setLineWidth(0.5)
        canvas.line(MARGIN, 13 * mm, PW - MARGIN, 13 * mm)
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(MUTED)
        canvas.drawString(MARGIN, 9 * mm, "Next-GEN Professional Point of Sale System")
        canvas.drawRightString(PW - MARGIN, 9 * mm, "Page %d" % doc.page)
        canvas.restoreState()


# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------
def bullets(items, st=BULLET):
    return ListFlowable(
        [ListItem(Paragraph(t, st), leftIndent=10, value="•") for t in items],
        bulletType="bullet", start="•", leftIndent=14, bulletColor=BLUE,
        bulletFontSize=7, spaceBefore=1, spaceAfter=1,
    )


def feature_table(rows, col_widths, header=True):
    data = []
    for i, r in enumerate(rows):
        if header and i == 0:
            data.append([Paragraph(c, CELLH) for c in r])
        else:
            data.append([Paragraph(r[0], CELLB)] + [Paragraph(c, CELL) for c in r[1:]])
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


def section_rule():
    return HRFlowable(width="100%", thickness=0.6, color=LINE,
                      spaceBefore=4, spaceAfter=8)


story = []

# ----------------------------------------------------------------------------
# COVER
# ----------------------------------------------------------------------------
story.append(Spacer(1, 78 * mm))
story.append(Paragraph("KeynetikPOS", COVER_TITLE))
story.append(Spacer(1, 6 * mm))
story.append(Paragraph("Feature Documentation", COVER_SUB))
story.append(Spacer(1, 3 * mm))
story.append(Paragraph("Next-GEN Professional Point of Sale System", COVER_SMALL))
story.append(Spacer(1, 60 * mm))
story.append(Paragraph("Version 2.0.0", COVER_SUB))
story.append(Spacer(1, 2 * mm))
story.append(Paragraph("Keynetik Solutions  &bull;  Built with Qt 6 / C++17  &bull;  Windows",
                       COVER_SMALL))

story.append(NextPageTemplate("body"))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# INTRO + TOC
# ----------------------------------------------------------------------------
story.append(Paragraph("Overview", H1))
story.append(Paragraph(
    "KeynetikPOS is a desktop point-of-sale application for retail shops, built with "
    "Qt 6 and C++17 on top of an embedded SQLite database. It is designed for the "
    "Kenyan market (KSh currency, 16% VAT, M-Pesa-friendly SMS) but is fully "
    "configurable for any locale. The application bundles everything a counter needs: "
    "a touch-friendly sales screen, multi-cart support, inventory and stock control, "
    "role-based staff accounts, receipts, analytics, shift &amp; cash management, "
    "automated report messaging, and a licensing/anti-tamper layer.", BODY))
story.append(Paragraph(
    "This document catalogues every feature area of the system, grouped by module.", BODY))

story.append(section_rule())
story.append(Paragraph("Contents", H2))

toc_items = [
    ("1", "Point of Sale & Checkout"),
    ("2", "Multi-Cart Management"),
    ("3", "Discounts"),
    ("4", "Payments & Receipts"),
    ("5", "Products & Inventory"),
    ("6", "Stock Control"),
    ("7", "Barcode Scanning"),
    ("8", "Users, Roles & Permissions"),
    ("9", "Shift & Cash Management"),
    ("10", "Reports"),
    ("11", "Analytics Dashboard"),
    ("12", "Automated Messaging & Schedules"),
    ("13", "Refunds"),
    ("14", "Settings & Configuration"),
    ("15", "Licensing, Trial & Security"),
    ("16", "Platform, Data & Keyboard Shortcuts"),
]
toc_rows = [[Paragraph(n, TOCNUM), Paragraph(t, TOC)] for n, t in toc_items]
toc = Table(toc_rows, colWidths=[12 * mm, PW - 2 * MARGIN - 12 * mm])
toc.setStyle(TableStyle([
    ("VALIGN", (0, 0), (-1, -1), "TOP"),
    ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
    ("TOPPADDING", (0, 0), (-1, -1), 2),
]))
story.append(toc)
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 1. POS & CHECKOUT
# ----------------------------------------------------------------------------
story.append(Paragraph("1. Point of Sale &amp; Checkout", H1))
story.append(Paragraph(
    "The main window is the live sales terminal. A searchable, category-filtered grid "
    "of product buttons sits beside the active cart and a running order summary.", BODY))

story.append(Paragraph("Sales screen", H3))
story.append(bullets([
    "Touch-friendly product grid with colour-coded stock status (green healthy, "
    "amber low, red critical, grey out-of-stock).",
    "Live product search box and category drop-down filter.",
    "Click a product to add it to the active cart; out-of-stock items are blocked.",
    "Cart table shows item, quantity, unit price and line subtotal with per-item remove.",
    "Order summary panel: Subtotal, Tax, Discount and a bold Total, updated live.",
    "Status bar with rolling messages, live date/time clock and current-user label.",
]))

story.append(Paragraph("Checkout flow", H3))
story.append(bullets([
    "Checkout opens a payment dialog pre-filled with the amount due.",
    "Tax is computed from the configurable tax settings on the discounted base "
    "(inclusive or exclusive pricing supported).",
    "<b>Atomic transaction:</b> the sale header, every line item and the stock "
    "decrement are committed together in a single database transaction — any "
    "failure rolls everything back, keeps the cart, and prints no receipt.",
    "Stock availability is re-validated inside the transaction so concurrent sales "
    "cannot oversell.",
    "On success the receipt prints, inventory warnings refresh, the sale is written to "
    "the activity log and the shift, and the cart resets.",
]))
story.append(callout(
    "Reliability by design",
    "Earlier builds decremented stock twice and printed receipts even on partial "
    "failures. Checkout is now a single all-or-nothing operation, so inventory counts "
    "and recorded sales always agree."))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 2. MULTI-CART
# ----------------------------------------------------------------------------
story.append(Paragraph("2. Multi-Cart Management", H1))
story.append(Paragraph(
    "Cashiers can hold several open transactions at once — useful when a customer "
    "steps away to fetch an item or when serving a queue in parallel.", BODY))
story.append(bullets([
    "Tabbed cart bar; each tab is an independent cart with its own items and discount.",
    "Create a new cart (➕ New Cart / Ctrl+N) and switch between carts by tab.",
    "Rename, close and reorder (movable) tabs.",
    "Right-click a tab for a context menu to <b>delete</b> or <b>merge</b> carts.",
    "Merging combines one cart's items into another.",
    "Per-cart metadata: created time, last-modified time and live item count.",
    "New Sale (Ctrl+? / F-key) clears the current cart after confirmation.",
]))
story.append(section_rule())

# ----------------------------------------------------------------------------
# 3. DISCOUNTS
# ----------------------------------------------------------------------------
story.append(Paragraph("3. Discounts", H1))
story.append(Paragraph(
    "Discounts are applied per cart through a dedicated dialog and are permission- and "
    "PIN-gated to prevent unauthorised markdowns.", BODY))
story.append(feature_table([
    ["Capability", "Detail"],
    ["Discount types", "Percentage off, fixed amount off, or fixed price; "
     "preset quick-discount buttons plus custom entry."],
    ["Reason capture", "An optional discount reason is stored on the cart and printed "
     "on the receipt."],
    ["Permission gate", "Requires the APPLY_DISCOUNTS permission (Admin, Manager, "
     "Cashier)."],
    ["PIN protection", "When enabled, a manager PIN must be entered before a discount "
     "is allowed; the PIN is stored as a salted PBKDF2 hash, never plaintext."],
    ["Audit", "Every applied discount (amount and reason) is written to the user "
     "activity log."],
], [38 * mm, PW - 2 * MARGIN - 38 * mm]))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 4. PAYMENTS & RECEIPTS
# ----------------------------------------------------------------------------
story.append(Paragraph("4. Payments &amp; Receipts", H1))
story.append(Paragraph("Payment capture", H3))
story.append(bullets([
    "Payment dialog records the payment method, amount tendered and computes change.",
    "Amount paid and change due are persisted with the sale for reconciliation and "
    "Z-reports (not just shown on the receipt).",
    "Payment method is stored per sale and feeds the payment-method analytics.",
]))
story.append(Paragraph("Receipts", H3))
story.append(bullets([
    "Three output paths: thermal printer, standard (A4/Letter) printer, and PDF export.",
    "Receipts carry company header (name, address, phone, tax ID), line items, "
    "subtotal, tax, discount &amp; reason, total, payment, change, cashier and a "
    "configurable footer.",
    "Reprint the last receipt from the Sales menu.",
    "Email-receipt and save-to-PDF hooks for digital copies.",
    "Optional auto-show of the receipt preview on checkout (setting).",
]))
story.append(callout(
    "Configurable output",
    "Company details and the receipt footer come from Settings, so receipts rebrand "
    "instantly without code changes.", bg=GREYBG, bar=ACCENT))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 5. PRODUCTS & INVENTORY
# ----------------------------------------------------------------------------
story.append(Paragraph("5. Products &amp; Inventory", H1))
story.append(Paragraph(
    "A full inventory management dialog (tabbed) covers the product catalogue, stock "
    "levels, alerts and inventory analytics.", BODY))
story.append(bullets([
    "Product catalogue with name, category, selling price, cost price, profit margin, "
    "stock quantity, barcode and active/inactive flag.",
    "Add, edit and (soft) delete products; selling price can be auto-derived from cost "
    "price + margin.",
    "Category management; products are grouped and filterable by category.",
    "<b>CSV import</b> of products with an imported / skipped / errors summary.",
    "<b>CSV export</b> of the full inventory and of alert lists.",
    "Inventory cache with auto-refresh on a timer for responsive stock display.",
    "Low-stock and critical-stock alert views.",
]))
story.append(Paragraph("Stock status thresholds", H3))
story.append(feature_table([
    ["Status", "Rule", "Indicator"],
    ["Out of stock", "quantity = 0", "Grey"],
    ["Critical", "1 – 4 units", "Red"],
    ["Low", "5 – 20 units", "Amber"],
    ["Healthy", "above 20 units", "Green"],
], [40 * mm, 60 * mm, PW - 2 * MARGIN - 100 * mm]))
story.append(section_rule())

# ----------------------------------------------------------------------------
# 6. STOCK CONTROL
# ----------------------------------------------------------------------------
story.append(Paragraph("6. Stock Control", H1))
story.append(Paragraph(
    "A dedicated Stock Manager handles day-to-day stock movements with a full audit "
    "trail.", BODY))
story.append(bullets([
    "Searchable, filterable stock table (all / low / critical / out-of-stock).",
    "Restock products (with optional supplier) and make manual stock adjustments.",
    "Every adjustment is logged transactionally with old qty, new qty, change, reason "
    "and the user who made it — viewable as stock history per product.",
    "Summary panel: total products, total units, low/critical/out-of-stock counts and "
    "total stock value.",
    "Restocking and selling emit live low/critical/out-of-stock notifications.",
    "CSV export of stock data.",
]))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 7. BARCODE
# ----------------------------------------------------------------------------
story.append(Paragraph("7. Barcode Scanning", H1))
story.append(Paragraph(
    "Barcode input is handled by a flexible reader supporting the two common scanner "
    "types, configurable from Settings.", BODY))
story.append(feature_table([
    ["Mode", "Detail"],
    ["USB / keyboard-wedge", "Scanner types characters + Enter; an application-wide "
     "event filter captures the burst and assembles the barcode with an "
     "inter-character timeout."],
    ["Serial (RS-232 / COM)", "Configurable port name and baud rate (default 9600); "
     "lists available serial ports on the system."],
    ["Cleaning", "Configurable prefix/suffix stripping (e.g. STX/ETX framing bytes)."],
    ["Lookup", "A scanned barcode resolves to a product and adds it to the active cart."],
], [42 * mm, PW - 2 * MARGIN - 42 * mm]))
story.append(section_rule())

# ----------------------------------------------------------------------------
# 8. USERS / RBAC
# ----------------------------------------------------------------------------
story.append(Paragraph("8. Users, Roles &amp; Permissions", H1))
story.append(Paragraph(
    "Access is governed by role-based access control. Login is required at startup "
    "(max three attempts) and every privileged action is permission-checked.", BODY))
story.append(Paragraph("Roles", H3))
story.append(feature_table([
    ["Role", "Access summary"],
    ["Admin", "Full access to every feature, including user management, settings, "
     "backup and logs."],
    ["Manager", "Sales, discounts, void; inventory add/edit and stock adjust; all "
     "reports &amp; analytics with export; view users."],
    ["Cashier", "Make &amp; view sales, apply discounts, view inventory."],
    ["Viewer", "Read-only: view sales, inventory, reports and analytics."],
], [30 * mm, PW - 2 * MARGIN - 30 * mm]))
story.append(Paragraph("User management", H3))
story.append(bullets([
    "Create, edit, enable/disable and delete users; deleting a user also clears their "
    "activity log in the same transaction.",
    "Per-user fields: username, full name, email, role, active flag, created-by and "
    "last-login.",
    "Username and email validation; live password-strength meter (Weak/Medium/Strong).",
    "Users change their own password; admins can reset others'.",
    "Full activity log: logins, logouts, sales, discounts, user and settings changes.",
]))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 9. SHIFT & CASH
# ----------------------------------------------------------------------------
story.append(Paragraph("9. Shift &amp; Cash Management", H1))
story.append(Paragraph(
    "Shifts tie sales to a cashier and a cash drawer for end-of-day reconciliation.", BODY))
story.append(bullets([
    "Open a shift with a cashier name and an opening cash float.",
    "Sales, discounts and voids are recorded against the open shift as they happen.",
    "Close a shift with a counted closing float and optional notes.",
    "Shift history retrievable (default last 30 days) with per-shift detail.",
    "Drives the Z-Report's expected-cash and cash-variance figures.",
]))
story.append(section_rule())

# ----------------------------------------------------------------------------
# 10. REPORTS
# ----------------------------------------------------------------------------
story.append(Paragraph("10. Reports", H1))
story.append(Paragraph("Sales reports dialog", H3))
story.append(bullets([
    "Sales by Date Range",
    "Sales by Category",
    "Sales by Payment Method",
    "Top Selling Products",
    "Daily Sales Summary",
]))
story.append(Paragraph(
    "Reports can be exported to <b>CSV</b> or <b>PDF</b>. A one-click Daily Report is "
    "available from the Reports menu.", BODY))
story.append(Paragraph("Z-Report (end of day / shift)", H3))
story.append(bullets([
    "Gross sales, total discounts, net sales, tax collected and transaction count.",
    "Breakdown by category and a top-products list.",
    "Cash reconciliation: opening float, closing float, expected cash and variance.",
    "Generated per date and/or per shift; exportable and printable.",
    "Reusable text generator so the same Z-Report can be sent automatically (see §12).",
]))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 11. ANALYTICS
# ----------------------------------------------------------------------------
story.append(Paragraph("11. Analytics Dashboard", H1))
story.append(Paragraph(
    "A visual analytics dashboard turns sales data into charts over a selectable date "
    "range, plus an at-a-glance quick-stats strip for the main window.", BODY))
story.append(feature_table([
    ["Widget", "Shows"],
    ["Sales trend", "Daily revenue over the range with total, average and trend "
     "direction."],
    ["Category breakdown", "Share of sales by category (pie)."],
    ["Hourly heatmap", "Sales intensity by hour of day to spot peak trading times."],
    ["Top products", "Ranked products by quantity, revenue and profit."],
    ["Payment analysis", "Split of takings across payment methods."],
    ["Customer analytics", "Total / new / returning customers, average value, top tier."],
    ["Quick stats", "Today's sales, transactions, customers and average ticket."],
], [38 * mm, PW - 2 * MARGIN - 38 * mm]))
story.append(bullets([
    "Selectable start/end date range with refresh-all.",
    "Export the analytics report to CSV and print it.",
    "Actual gross-profit reporting (uses stored cost prices) for today, this month or "
    "any range.",
]))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 12. MESSAGING / SCHEDULES
# ----------------------------------------------------------------------------
story.append(Paragraph("12. Automated Messaging &amp; Schedules", H1))
story.append(Paragraph(
    "KeynetikPOS can send reports automatically to staff or owners over SMS / WhatsApp, "
    "driven by a scheduling engine with a pluggable provider registry.", BODY))
story.append(Paragraph("Providers", H3))
story.append(feature_table([
    ["Provider", "Use"],
    ["Africa's Talking", "SMS for the Kenyan market (~KES 0.80/SMS, M-Pesa friendly); "
     "API key, username and optional sender ID."],
    ["Twilio WhatsApp", "WhatsApp messages via Twilio (account SID, auth token, "
     "WhatsApp-enabled number)."],
], [40 * mm, PW - 2 * MARGIN - 40 * mm]))
story.append(Paragraph("Scheduling engine", H3))
story.append(bullets([
    "Schedule types: Daily, Weekly, Monthly, On-Shift-Close, On-Sales-Threshold, Custom.",
    "Report types: Z-Report, X-Report, Inventory Alert, Sales Summary, Custom Report.",
    "Per-schedule send time, day-of-week / day-of-month, recipient list and provider.",
    "Active/inactive toggle, next-scheduled tracking, last-sent and failed-attempt "
    "counters.",
    "Timer-driven dispatch with a manual 'send now' for testing.",
    "Schedules persist in SQLite; managed through schedule and schedule-editor dialogs.",
    "Delivery and error signals surface success/failure of each send.",
]))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 13. REFUNDS
# ----------------------------------------------------------------------------
story.append(Paragraph("13. Refunds", H1))
story.append(Paragraph(
    "Refunds are processed from the Sales History screen with safeguards against "
    "double-refunding.", BODY))
story.append(bullets([
    "Browse sales history; sales already refunded are flagged and locked.",
    "Process a refund with a captured reason and the processing user recorded.",
    "Refund is written transactionally and returns the items to stock with logged "
    "stock adjustments.",
    "Sales history is searchable and exportable to CSV; last receipt reprintable.",
]))
story.append(section_rule())

# ----------------------------------------------------------------------------
# 14. SETTINGS
# ----------------------------------------------------------------------------
story.append(Paragraph("14. Settings &amp; Configuration", H1))
story.append(Paragraph(
    "A central settings dialog persists configuration to the database (and provider "
    "credentials to user settings).", BODY))
story.append(feature_table([
    ["Group", "Settings"],
    ["Business", "Business name, address, phone, email, website."],
    ["Currency", "Currency symbol and code (defaults to KSh)."],
    ["Tax", "Enable/disable, rate, label (VAT/GST/...), tax-inclusive vs exclusive."],
    ["Receipt", "Footer text, auto-show receipt preview, show-logo flag."],
    ["Discount", "Require-PIN toggle and PIN (stored hashed)."],
    ["Messaging", "Twilio SID/token/number; Africa's Talking key/username/sender ID."],
    ["Barcode", "Input mode, serial port and baud rate."],
    ["Theme", "Light / dark mode toggle, persisted."],
], [30 * mm, PW - 2 * MARGIN - 30 * mm]))
story.append(Paragraph(
    "Tax and theme changes propagate live — totals, the tax label and the UI "
    "update immediately when settings are saved.", BODY))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 15. LICENSING & SECURITY
# ----------------------------------------------------------------------------
story.append(Paragraph("15. Licensing, Trial &amp; Security", H1))
story.append(Paragraph("Trial &amp; licensing", H3))
story.append(bullets([
    "30-day free trial from first run, with reminder prompts and a key-entry prompt on "
    "expiry.",
    "CD-key activation: local activation, or server-backed online activation with "
    "device binding.",
    "Online activations re-validate via a background heartbeat after the window opens — "
    "startup never blocks on the network.",
    "Offline grace window (7 days) for server-backed keys; server-side revocation is "
    "honoured at next launch (no mid-shift lockout).",
    "Trial clock is anchored to the earliest of a registry stamp and a signed, "
    "machine-bound breadcrumb file, so wiping the registry alone does not reset it.",
]))
story.append(Paragraph("Integrity &amp; hardening", H3))
story.append(bullets([
    "Tamper detection: license data is covered by a salted integrity hash (key, install "
    "date, activation source and revocation flag); deleting or editing any of it locks "
    "the app.",
    "Passwords and the discount PIN use salted PBKDF2-SHA256; legacy SHA-256 hashes are "
    "transparently upgraded on next login.",
    "Forced password change on first login for the default admin and any admin-reset "
    "account.",
    "Minimal release-only anti-debug check (debugger-present), tuned to avoid false "
    "positives on legitimate machines.",
    "All database access uses parameterised queries; SQLite foreign keys enforced.",
]))
story.append(callout(
    "Default credentials",
    "First run seeds an <b>admin / admin123</b> account flagged must-change-password — "
    "the app forces a new password before it can be used.", bg=colors.HexColor("#fdf3e7"),
    bar=colors.HexColor("#e08a1e")))
story.append(PageBreak())

# ----------------------------------------------------------------------------
# 16. PLATFORM
# ----------------------------------------------------------------------------
story.append(Paragraph("16. Platform, Data &amp; Keyboard Shortcuts", H1))
story.append(Paragraph("Platform &amp; data", H3))
story.append(bullets([
    "Qt 6 / C++17 desktop application, packaged for Windows (NSIS installer via CPack, "
    "windeployqt bundling, desktop &amp; Start-menu shortcuts).",
    "Embedded SQLite database stored in the per-user application-data folder; tables "
    "auto-created and migrated on launch.",
    "Sample catalogue (categories &amp; products) seeded on a fresh database.",
    "Light and dark themes; localised currency formatting.",
]))
story.append(Paragraph("Keyboard shortcuts", H3))
story.append(feature_table([
    ["Shortcut", "Action"],
    ["F2", "Checkout"],
    ["F3", "Focus product search"],
    ["F4", "Apply discount"],
    ["F5", "New cart"],
    ["Ctrl+N", "New cart"],
    ["Ctrl+W", "Clear cart"],
    ["Ctrl+L", "Logout"],
    ["Ctrl+Q", "Exit"],
], [34 * mm, PW - 2 * MARGIN - 34 * mm]))

story.append(Spacer(1, 6 * mm))
story.append(HRFlowable(width="100%", thickness=1, color=NAVY, spaceAfter=6))
story.append(Paragraph(
    "KeynetikPOS v2.0.0 &mdash; Keynetik Solutions. This document was generated from the "
    "application source and reflects the feature set at the time of writing.", SMALL))

# ----------------------------------------------------------------------------
Doc(OUT, pagesize=A4, title="KeynetikPOS Feature Documentation",
    author="Keynetik Solutions").build(story)
print("Wrote", OUT)
