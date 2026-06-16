# -*- coding: utf-8 -*-
"""Generate the KeynetikPOS User Guide & Manual PDF (cashier / manager audience)."""

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
    PageBreak, NextPageTemplate, ListFlowable, ListItem, HRFlowable,
)

OUT = "KeynetikPOS_User_Guide.pdf"
RUNNING_TITLE = "User Guide & Manual"

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
BULLET = style("Bullet", fontName="Helvetica", fontSize=9.3, textColor=DARK, leading=13.5)
STEP = style("Step", fontName="Helvetica", fontSize=9.4, textColor=DARK, leading=14)
SMALL = style("Small", fontName="Helvetica", fontSize=8.2, textColor=MUTED, leading=11)
CELL = style("Cell", fontName="Helvetica", fontSize=8.6, textColor=DARK, leading=11.5)
CELLB = style("CellB", fontName="Helvetica-Bold", fontSize=8.6, textColor=NAVY, leading=11.5)
CELLH = style("CellH", fontName="Helvetica-Bold", fontSize=8.8, textColor=colors.white, leading=11.5)
CELLC = style("CellC", fontName="Helvetica-Bold", fontSize=8.6, textColor=ACCENT,
              leading=11.5, alignment=TA_CENTER)
KEY = style("Key", fontName="Courier-Bold", fontSize=8.8, textColor=NAVY,
            leading=11.5, alignment=TA_CENTER)
TOC = style("TOC", fontName="Helvetica", fontSize=10, textColor=DARK, leading=16)
TOCNUM = style("TOCnum", fontName="Helvetica-Bold", fontSize=10, textColor=BLUE, leading=16)

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
        canvas.setFillColor(ACCENT)
        canvas.rect(0, PH - 70 * mm, PW, 4 * mm, fill=1, stroke=0)
        canvas.setFillColor(BLUE)
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


def steps(items):
    return ListFlowable(
        [ListItem(Paragraph(t, STEP), leftIndent=12) for t in items],
        bulletType="1", leftIndent=18, bulletColor=BLUE, bulletFontName="Helvetica-Bold",
        bulletFontSize=9.4, spaceBefore=1, spaceAfter=2,
    )


def table(rows, col_widths, header=True, center_cols=(), key_cols=()):
    data = []
    for i, r in enumerate(rows):
        if header and i == 0:
            data.append([Paragraph(c, CELLH) for c in r])
        else:
            row = []
            for j, c in enumerate(r):
                if j in key_cols:
                    row.append(Paragraph(c, KEY))
                elif j == 0:
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
        cmds += [("BACKGROUND", (0, 0), (-1, 0), BLUE),
                 ("TOPPADDING", (0, 0), (-1, 0), 6),
                 ("BOTTOMPADDING", (0, 0), (-1, 0), 6)]
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


def rule():
    return HRFlowable(width="100%", thickness=0.6, color=LINE, spaceBefore=4, spaceAfter=8)


story = []

# ---------------------------------------------------------------- COVER
story.append(Spacer(1, 74 * mm))
story.append(Paragraph("KeynetikPOS", COVER_TITLE))
story.append(Spacer(1, 6 * mm))
story.append(Paragraph("User Guide &amp; Manual", COVER_SUB))
story.append(Spacer(1, 3 * mm))
story.append(Paragraph("For cashiers, supervisors and shop managers", COVER_SMALL))
story.append(Spacer(1, 60 * mm))
story.append(Paragraph("Version 2.0.0", COVER_SUB))
story.append(Spacer(1, 2 * mm))
story.append(Paragraph("Keynetik Solutions  &bull;  Next-GEN Professional Point of Sale System",
                       COVER_SMALL))
story.append(NextPageTemplate("body"))
story.append(PageBreak())

# ---------------------------------------------------------------- WELCOME + TOC
story.append(Paragraph("Welcome", H1))
story.append(Paragraph(
    "This manual explains how to use KeynetikPOS day to day &mdash; from logging in and "
    "ringing up a sale to managing stock, running reports and closing the till. No "
    "technical background is required. Looking for installation, backup or admin/server "
    "details? See the <b>System Documentation</b>.", BODY))
story.append(rule())
story.append(Paragraph("Contents", H2))
toc_items = [
    ("1", "Getting started"),
    ("2", "The main screen"),
    ("3", "Making a sale"),
    ("4", "Discounts"),
    ("5", "Taking payment & receipts"),
    ("6", "Working with multiple carts"),
    ("7", "Barcode scanning"),
    ("8", "Refunds & sales history"),
    ("9", "Managing inventory"),
    ("10", "Shifts & cash drawer"),
    ("11", "Reports & analytics"),
    ("12", "Scheduled reports (WhatsApp / SMS)"),
    ("13", "User accounts & roles"),
    ("14", "Settings"),
    ("15", "Keyboard shortcuts"),
    ("16", "Frequently asked questions"),
]
toc_rows = [[Paragraph(n, TOCNUM), Paragraph(t, TOC)] for n, t in toc_items]
toc = Table(toc_rows, colWidths=[12 * mm, PW - 2 * MARGIN - 12 * mm])
toc.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"),
                         ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
                         ("TOPPADDING", (0, 0), (-1, -1), 2)]))
story.append(toc)
story.append(PageBreak())

# ---------------------------------------------------------------- 1 GETTING STARTED
story.append(Paragraph("1. Getting started", H1))
story.append(Paragraph("First login", H3))
story.append(Paragraph(
    "When KeynetikPOS opens you'll see a login window. On a brand-new installation, use "
    "the default administrator account:", BODY))
story.append(table([
    ["Field", "Value"],
    ["Username", "admin"],
    ["Password", "admin123"],
], [34 * mm, PW - 2 * MARGIN - 34 * mm]))
story.append(Paragraph(
    "You will be required to set a new password immediately on this first login &mdash; at "
    "least 8 characters and different from the old one. Keep it safe: there is no "
    "self-service reset for the last admin account.", BODY))
story.append(callout("Login attempts",
                     "You have 3 attempts per session. After three failures the app closes "
                     "&mdash; just reopen it to try again.", bg=GREYBG, bar=BLUE))
story.append(Paragraph("Trial vs. full licence", H3))
story.append(Paragraph(
    "In trial mode the app runs for 30 days and occasionally reminds you how many days "
    "remain. To unlock the full version, enter your CD key when prompted (or ask your "
    "administrator). KeynetikPOS works fully offline; internet is only needed to activate "
    "a key and to send scheduled reports.", BODY))
story.append(Paragraph("Switching theme", H3))
story.append(Paragraph(
    "KeynetikPOS supports Light and Dark themes. Toggle it any time from "
    "<b>Settings &rarr; Toggle Dark Mode</b>. Your choice is remembered.", BODY))
story.append(PageBreak())

# ---------------------------------------------------------------- 2 MAIN SCREEN
story.append(Paragraph("2. The main screen", H1))
story.append(Paragraph(
    "The main window is your sales workspace. Its main areas:", BODY))
story.append(bullets([
    "<b>Menu bar</b> &mdash; File, Sales, Inventory, Reports, Settings, User, Help.",
    "<b>Product grid</b> (left) &mdash; cards for every product, colour-coded by stock "
    "(green = healthy, orange = low, red = critical, grey = out of stock). Each card shows "
    "the price and a stock note (e.g. “LOW &mdash; 3 left”, “OUT OF STOCK”).",
    "<b>Search box</b> &mdash; filter products by name or barcode as you type.",
    "<b>Category filter</b> &mdash; narrow the grid to one product category.",
    "<b>Cart panel</b> (right) &mdash; the current order: product, price, quantity (with "
    "&minus; / &#65291; tap steppers), subtotal, and an &#10006; to remove a line.",
    "<b>Totals panel</b> &mdash; subtotal, discount, tax, and the grand total.",
    "<b>Action buttons</b> &mdash; Apply Discount, Checkout, New Cart.",
    "<b>Status bar</b> (bottom) &mdash; who is logged in, their role, and transient "
    "messages (like “Change due: ...” after a sale).",
]))
story.append(Paragraph(
    "The window adapts to your screen and works on touch displays (bigger tap targets, "
    "&minus; / &#65291; quantity buttons). Drag the divider between the product grid and "
    "the cart to resize them.", BODY))
story.append(PageBreak())

# ---------------------------------------------------------------- 3 MAKING A SALE
story.append(Paragraph("3. Making a sale", H1))
story.append(steps([
    "<b>Add products to the cart</b>, by clicking a product card; or pressing <b>F3</b> to "
    "search by name/barcode and selecting the product (arrow keys + Enter add the "
    "highlighted card); or scanning a barcode (see &sect;7).",
    "<b>Adjust quantities</b> with the &minus; and &#65291; buttons, or type a quantity. "
    "You can't exceed available stock &mdash; the app clamps it and warns you.",
    "<b>Remove a line</b> with the &#10006; button on that row, or clear the whole cart "
    "with <b>Ctrl+W</b>.",
    "Watch the <b>totals</b> update live (subtotal &rarr; discount &rarr; tax &rarr; total).",
    "When ready, <b>Checkout</b> (press <b>F2</b> or click Checkout).",
]))
story.append(callout("Out-of-stock items",
                     "Out-of-stock products appear greyed out and cannot be added.",
                     bg=GREYBG, bar=ACCENT))
story.append(rule())

# ---------------------------------------------------------------- 4 DISCOUNTS
story.append(Paragraph("4. Discounts", H1))
story.append(Paragraph(
    "Discounts are protected to prevent accidental or unauthorised markdowns.", BODY))
story.append(steps([
    "With items in the cart, click <b>Apply Discount</b> (or press <b>F4</b>).",
    "Enter the <b>discount PIN</b> when prompted (set by your administrator in Settings).",
    "Choose <b>Percentage</b> (e.g. 10%) or <b>Fixed amount</b> (e.g. KSh 50 off), enter "
    "the value, and type a <b>reason</b> (required &mdash; it's recorded for the audit log).",
    "The dialog shows a <b>live preview</b> of the discount and the new total. A fixed "
    "discount can never push the total below zero, and a percentage can't exceed 100%.",
    "Confirm. The discount applies to the current cart and is recorded against your user "
    "and shift.",
]))
story.append(Paragraph("Cashiers and Managers can apply discounts; Viewers cannot.", BODY))
story.append(PageBreak())

# ---------------------------------------------------------------- 5 PAYMENT
story.append(Paragraph("5. Taking payment & receipts", H1))
story.append(Paragraph("After Checkout (F2):", BODY))
story.append(steps([
    "The <b>Payment</b> dialog opens. Choose a method: <b>Cash</b>, <b>Card</b>, "
    "<b>Mobile</b> (M-Pesa / mobile money), or <b>Multiple</b> (split tender).",
    "Enter the <b>amount tendered</b>. For cash, <b>change due</b> is calculated live, and "
    "you can't confirm until the amount covers the total.",
    "For <b>Mobile money</b>, enter the <b>reference number</b> (required) &mdash; it's "
    "printed on the receipt.",
    "Confirm payment. The sale is recorded, stock is reduced automatically, and a "
    "<b>receipt</b> is generated.",
]))
story.append(callout(
    "Receipts",
    "In the current build receipts are saved as PDF files "
    "(Receipt_&lt;number&gt;_&lt;date&gt;.pdf), so you can print, archive, or share them "
    "even without a printer. To reprint the last one: Sales &rarr; Reprint Last Receipt.",
    bg=GREYBG, bar=ACCENT))
story.append(Paragraph(
    "After a successful sale you'll see a brief confirmation and the change-due amount in "
    "the status bar &mdash; no pop-up to dismiss, so you can serve the next customer "
    "immediately.", BODY))
story.append(PageBreak())

# ---------------------------------------------------------------- 6 MULTI-CART
story.append(Paragraph("6. Working with multiple carts", H1))
story.append(Paragraph(
    "KeynetikPOS lets you keep several orders open at once &mdash; handy when a customer "
    "steps away to grab a forgotten item.", BODY))
story.append(bullets([
    "<b>New cart:</b> File &rarr; New Sale, the New Cart button, or <b>F5</b>.",
    "<b>Switch carts:</b> click the cart tabs.",
    "<b>Delete or merge a cart:</b> right-click a cart tab for Delete Cart or Merge Cart.",
]))
story.append(Paragraph(
    "Each cart has its own items, discount and totals. Checkout applies to the cart you're "
    "currently viewing.", BODY))
story.append(rule())

# ---------------------------------------------------------------- 7 BARCODE
story.append(Paragraph("7. Barcode scanning", H1))
story.append(Paragraph(
    "KeynetikPOS works with two kinds of scanner, and usually needs no setup:", BODY))
story.append(bullets([
    "<b>USB “keyboard-wedge” scanners</b> &mdash; plug in and scan; the product is added "
    "to the current cart automatically.",
    "<b>Serial / COM-port scanners</b> &mdash; selectable in settings (your administrator "
    "configures the COM port).",
]))
story.append(Paragraph(
    "If you scan a code the system doesn't recognise, you'll see a quick “unknown barcode” "
    "message. You can assign a barcode to a product, or generate and print EAN-13 labels "
    "for unlabelled stock, from the barcode tools (under Inventory).", BODY))
story.append(rule())

# ---------------------------------------------------------------- 8 REFUNDS
story.append(Paragraph("8. Refunds & sales history", H1))
story.append(Paragraph("Go to <b>Sales &rarr; View Sales History</b>.", BODY))
story.append(bullets([
    "<b>Browse</b> every transaction with filters: date range, quick filters (Today, This "
    "Week), payment method, and free-text search.",
    "A <b>summary strip</b> shows total sales, count, average and tax for the current "
    "filter.",
    "<b>Drill down</b> into any sale to see its line items.",
    "<b>Export</b> the list to CSV.",
]))
story.append(Paragraph("To process a refund:", H3))
story.append(steps([
    "Select the <b>original sale</b> in the list.",
    "Click <b>Refund</b> and enter a <b>reason</b>.",
    "Confirm. The refund is recorded, stock is restored, and the sale is marked refunded.",
]))
story.append(callout(
    "Safeguards",
    "You can't refund the same sale twice, and refunds always tie back to a real "
    "transaction. The Sales History window can stay open while you keep selling.",
    bg=GREYBG, bar=ACCENT))
story.append(PageBreak())

# ---------------------------------------------------------------- 9 INVENTORY
story.append(Paragraph("9. Managing inventory", H1))
story.append(Paragraph(
    "Open <b>Inventory &rarr; Manage Inventory</b> for the full workbench, with tabs:", BODY))
story.append(bullets([
    "<b>Overview</b> &mdash; searchable/filterable product table plus summary cards (total "
    "products, stock value, counts by status).",
    "<b>Stock Management</b> &mdash; edit stock directly in a spreadsheet-style grid; "
    "invalid entries are highlighted. Changes auto-save (or save explicitly), with "
    "undo/redo.",
    "<b>Alerts</b> &mdash; low- and critical-stock lists with one-click bulk restock.",
    "<b>Analytics</b> &mdash; stock value, fast and slow movers.",
]))
story.append(Paragraph("Other handy entry points:", H3))
story.append(bullets([
    "<b>Inventory &rarr; Add Product</b> &mdash; quickly add a new catalog item.",
    "<b>Inventory &rarr; Low Stock Alert</b> &mdash; a focused popup of exactly what needs "
    "reordering now, with restock buttons. A toast also pops up when an item crosses a "
    "low/critical threshold during sales.",
]))
story.append(Paragraph(
    "Inventory features support CSV import/export and printing. Every manual stock change "
    "is logged (who, when, why). Adding/editing products and adjusting stock require "
    "Manager or Admin rights; deleting products is Admin-only. The Inventory window can "
    "stay open while you continue ringing up sales.", BODY))
story.append(PageBreak())

# ---------------------------------------------------------------- 10 SHIFTS
story.append(Paragraph("10. Shifts & cash drawer", H1))
story.append(Paragraph(
    "Shifts track who was on the till and how much cash should be in the drawer.", BODY))
story.append(steps([
    "<b>Open a shift</b> &mdash; enter the cashier name and the counted opening float (the "
    "cash you start with).",
    "<b>During the shift</b>, sales, discounts and voids accumulate automatically.",
    "<b>Close the shift</b> &mdash; the app shows the expected totals; count the drawer, "
    "enter the closing float and any notes. The over/short variance is calculated for you.",
]))
story.append(callout(
    "Crash-safe",
    "If the app crashes or the power cuts out mid-shift, the open shift is re-adopted on "
    "restart &mdash; you won't lose it. End-of-shift figures feed the Z-Report (&sect;11).",
    bg=GREYBG, bar=ACCENT))
story.append(rule())

# ---------------------------------------------------------------- 11 REPORTS
story.append(Paragraph("11. Reports & analytics", H1))
story.append(Paragraph("On-screen analytics", H3))
story.append(Paragraph(
    "<b>Reports &rarr; Analytics Dashboard</b>: revenue today vs. yesterday (with change "
    "indicator), this week, this month and growth; top products, category breakdown, peak "
    "hours, and zero-sales alerts. Pick periods (7 / 30 / 90 days) or explicit date ranges, "
    "and view profit/margin and tax-collected metrics.", BODY))
story.append(Paragraph("Quick daily report", H3))
story.append(Paragraph(
    "<b>Reports &rarr; Daily Report</b> gives a fast snapshot of the day's numbers.", BODY))
story.append(Paragraph("Operational reports", H3))
story.append(Paragraph(
    "Choose a report type (sales by date, by category, by payment method, top sellers, "
    "daily summary) and a date range, then export to <b>CSV or PDF</b> &mdash; the "
    "printable documents for an accountant.", BODY))
story.append(Paragraph("Z-Report (end of day / shift)", H3))
story.append(Paragraph(
    "Gross sales, discounts, net sales, tax collected, transaction count, sales by "
    "category, top products, and cash reconciliation (opening float, closing float, "
    "expected cash, and the over/short variance) &mdash; the standard control document for "
    "detecting drawer shortages.", BODY))
story.append(Paragraph(
    "Reporting access depends on your role (Cashiers don't see reports; Viewers can view "
    "but not export).", SMALL))
story.append(PageBreak())

# ---------------------------------------------------------------- 12 MESSAGING
story.append(Paragraph("12. Scheduled reports (WhatsApp / SMS)", H1))
story.append(Paragraph(
    "KeynetikPOS can send reports to your phone automatically &mdash; so the owner gets the "
    "day's numbers without standing at the till. Set up under "
    "<b>Settings &rarr; Message Schedules</b>:", BODY))
story.append(steps([
    "<b>Add a schedule</b> &mdash; name it, choose when it fires (Daily, Weekly, Monthly, "
    "on shift close, or when sales pass a threshold), pick which report (Z-Report, "
    "inventory alert, sales summary...), set the time, add recipients, and choose a "
    "provider (WhatsApp via Twilio, or SMS via Africa's Talking).",
    "<b>Send Now</b> &mdash; test the schedule immediately to confirm your messaging "
    "credentials work (far better than discovering a typo at 9 p.m. when the nightly report "
    "silently fails).",
    "<b>Enable / disable</b> schedules with the toggle; the list shows each schedule's "
    "status and when it last sent.",
]))
story.append(Paragraph(
    "Provider credentials (Twilio SID/token/number; Africa's Talking key/username/sender "
    "ID) are entered by an administrator in Settings &rarr; Messaging.", BODY))
story.append(rule())

# ---------------------------------------------------------------- 13 USERS
story.append(Paragraph("13. User accounts & roles", H1))
story.append(Paragraph(
    "Administrators manage staff accounts under <b>User &rarr; User Management</b>:", BODY))
story.append(bullets([
    "Create, edit, enable/disable and delete users; change passwords. New users get a "
    "temporary password and must set their own at first login.",
    "A live password-strength indicator helps create strong passwords.",
    "A permissions viewer shows exactly what each role can do.",
]))
story.append(Paragraph(
    "Anyone can change their own password via <b>User &rarr; Change Password</b>, and log "
    "out via <b>User &rarr; Logout</b> (or <b>Ctrl+L</b>). Logging out returns you to the "
    "login screen without closing the program.", BODY))
story.append(Paragraph("What each role can do", H3))
story.append(table([
    ["Task", "Admin", "Mgr", "Cashier", "Viewer"],
    ["Make sales", "Yes", "Yes", "Yes", "-"],
    ["View sales", "Yes", "Yes", "Yes", "Yes"],
    ["Apply discounts", "Yes", "Yes", "Yes", "-"],
    ["Void / delete transactions", "Yes", "void", "-", "-"],
    ["View inventory", "Yes", "Yes", "Yes", "Yes"],
    ["Add/edit products, adjust stock", "Yes", "Yes", "-", "-"],
    ["Delete products", "Yes", "-", "-", "-"],
    ["View reports & analytics", "Yes", "Yes", "-", "Yes"],
    ["Export reports", "Yes", "Yes", "-", "-"],
    ["Manage users", "Yes", "-", "-", "-"],
    ["Settings, backup, logs", "Yes", "-", "-", "-"],
], [58 * mm, 24 * mm, 18 * mm, 24 * mm, 24 * mm], center_cols=(1, 2, 3, 4)))
story.append(Paragraph(
    "Buttons and menu items you don't have permission for are greyed out.", SMALL))
story.append(PageBreak())

# ---------------------------------------------------------------- 14 SETTINGS
story.append(Paragraph("14. Settings", H1))
story.append(Paragraph(
    "<b>Settings &rarr; Company Information</b> (and related menus) open the configuration "
    "editor (Admin only), with tabs for:", BODY))
story.append(table([
    ["Tab", "What you set"],
    ["Business", "Name, address, phone, email, website, currency (default KSh), receipt "
     "footer."],
    ["Tax", "Turn tax on/off, set the rate and label (e.g. “VAT”), choose tax-inclusive "
     "pricing if shelf prices already include tax."],
    ["Receipt", "Header/footer and whether to print receipts."],
    ["Security", "The discount PIN (entered twice) and whether a PIN is required for "
     "discounts."],
    ["Messaging", "Twilio WhatsApp and Africa's Talking credentials, with a "
     "test-connection button."],
], [28 * mm, PW - 2 * MARGIN - 28 * mm]))
story.append(Paragraph(
    "Changes apply immediately &mdash; for example, changing the currency re-labels prices "
    "across the app without a restart.", BODY))
story.append(PageBreak())

# ---------------------------------------------------------------- 15 SHORTCUTS
story.append(Paragraph("15. Keyboard shortcuts", H1))
story.append(Paragraph("Fast keys for busy tills:", BODY))
story.append(table([
    ["Key", "Action"],
    ["F1", "Show keyboard shortcuts help"],
    ["F2", "Checkout (take payment)"],
    ["F3", "Focus the product search box"],
    ["F4", "Apply a discount"],
    ["F5", "New cart / new sale"],
    ["Ctrl+N", "New sale"],
    ["Ctrl+W", "Clear the current cart"],
    ["Ctrl+L", "Log out"],
    ["Ctrl+Q", "Exit the application"],
], [34 * mm, PW - 2 * MARGIN - 34 * mm], key_cols=(0,)))
story.append(Paragraph(
    "Hover over toolbar buttons to see their shortcuts in the tooltip.", SMALL))
story.append(PageBreak())

# ---------------------------------------------------------------- 16 FAQ
story.append(Paragraph("16. Frequently asked questions", H1))


def qa(q, a):
    story.append(Paragraph(q, H3))
    story.append(Paragraph(a, BODY))


qa("I forgot my password — what do I do?",
   "Ask an administrator to reset it (User Management &rarr; Change Password). They'll set "
   "a temporary password that you'll change at your next login. If the last admin account "
   "is locked out, the system administrator must restore from a backup &mdash; see the "
   "System Documentation.")
qa("Do I need the internet to sell?",
   "No. KeynetikPOS runs fully offline. Internet is only used to activate your licence and "
   "to send scheduled WhatsApp/SMS reports.")
qa("My barcode scanner isn't adding products.",
   "Most USB scanners work with no setup. If yours connects by serial cable, ask your "
   "administrator to select the correct COM port. Check that the scanned product actually "
   "exists in the catalog &mdash; unknown codes show an “unknown barcode” message.")
qa("Why can't I see Reports / Settings / certain buttons?",
   "Those require a higher role (Manager or Admin). Greyed-out items are ones your account "
   "isn't permitted to use.")
qa("Where do my receipts go?",
   "They're saved as PDF files named Receipt_&lt;number&gt;_&lt;date&gt;.pdf. You can open "
   "and print them later, or attach a real printer for direct printing.")
qa("The total looks off by a cent on a big order.",
   "Rounding on very large or unusual orders can occasionally differ by a cent due to how "
   "amounts are stored. For normal retail transactions this is not noticeable.")
qa("How do I back up my data?",
   "That's an administrator task &mdash; the whole business is in one database file. See "
   "the Backup section of the System Documentation.")

story.append(Spacer(1, 6 * mm))
story.append(HRFlowable(width="100%", thickness=1, color=NAVY, spaceAfter=6))
story.append(Paragraph(
    "Need help? Contact support@keynetik.com.  &bull;  KeynetikPOS v2.0.0 — Keynetik "
    "Solutions.", SMALL))

# ----------------------------------------------------------------
Doc(OUT, pagesize=A4, title="KeynetikPOS User Guide & Manual",
    author="Keynetik Solutions").build(story)
print("Wrote", OUT)
