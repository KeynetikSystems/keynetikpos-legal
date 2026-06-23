# -*- coding: utf-8 -*-
"""Generate KeynetikPOS_User_Manual.pdf — the cashier / manager / owner manual."""

from reportlab.lib.units import mm
from _pdf_common import (
    Doc, H1, H2, H3, BODY, SMALL, PW, MARGIN, ACCENT, WARNBG, WARNBAR, LIGHT,
    Spacer, Paragraph, PageBreak, HRFlowable, CondPageBreak,
    bullets, steps, table, callout, cover, toc, colors, LINE,
)

OUT = "KeynetikPOS_User_Manual.pdf"
S = []


def h1(t): S.append(Paragraph(t, H1))
def h2(t): S.append(Paragraph(t, H2))
def h3(t): S.append(Paragraph(t, H3))
def p(t):  S.append(Paragraph(t, BODY))
def sp(h=4): S.append(Spacer(1, h))
def rule(): S.append(HRFlowable(width="100%", thickness=0.5, color=LINE, spaceBefore=8, spaceAfter=8))


# ============================================================= cover + TOC
S += cover(
    "User Manual",
    "Point of Sale, Inventory &amp; Business Management",
    ["Version 2.0 &nbsp;&bull;&nbsp; For cashiers, managers and owners",
     "Built for the Kenyan retail market (KSh, 16% VAT, M-Pesa)"],
)

h1("Contents")
S.append(toc([
    ("1", "Welcome &amp; what KeynetikPOS does"),
    ("2", "Getting started: install, activate, sign in"),
    ("3", "The sales screen at a glance"),
    ("4", "Making a sale, step by step"),
    ("5", "Taking payment (cash, card, M-Pesa, split)"),
    ("6", "Discounts"),
    ("7", "Customers, loyalty points &amp; store credit"),
    ("8", "Refunds"),
    ("9", "Inventory &amp; stock control"),
    ("10", "Purchasing: suppliers &amp; purchase orders"),
    ("11", "Expenses"),
    ("12", "Reports, analytics &amp; sales history"),
    ("13", "Shifts &amp; the Z-report"),
    ("14", "Finance: ledger, payroll &amp; VAT"),
    ("15", "Settings"),
    ("16", "Backups &amp; data safety"),
    ("17", "Staff accounts, roles &amp; permissions"),
    ("18", "Troubleshooting &amp; FAQ"),
]))
S.append(PageBreak())

# ============================================================= 1 welcome
h1("1. Welcome &amp; what KeynetikPOS does")
p("KeynetikPOS is a complete point-of-sale and small-business management system for "
  "shops and retailers. It rings up sales, tracks stock, manages customers and "
  "suppliers, records expenses, and keeps a full set of books — all on one Windows "
  "computer, with your data stored safely on the same machine.")
h3("What you can do")
S.append(bullets([
    "<b>Sell faster</b> — tap products or scan barcodes, take cash, card or M-Pesa, and print or email receipts.",
    "<b>Never run out</b> — live stock counts, low-stock alerts and per-product reorder levels.",
    "<b>Know your numbers</b> — daily takings, profit, top sellers and tax, on demand.",
    "<b>Keep customers</b> — phone-number lookup, loyalty points and store credit.",
    "<b>Stay compliant</b> — VAT-3 figures, PAYE/SHIF/NSSF payroll and a real double-entry ledger.",
]))
S.append(callout("Designed for Kenya",
    "Prices are in Kenyan Shillings (KSh) by default, VAT follows the 16% standard rate, "
    "and M-Pesa is a first-class payment method with a reference number printed on every receipt. "
    "All of these are configurable in Settings."))

# ============================================================= 2 getting started
h1("2. Getting started")
h2("Installing the app")
S.append(steps([
    "Run the KeynetikPOS installer (<font face='Courier'>KeynetikPOS-Setup.exe</font>) and follow the prompts.",
    "A desktop and Start-menu shortcut are created. Double-click to launch.",
    "On first launch the app creates its database automatically in your private app-data folder — no setup required.",
]))
h2("Activating your licence")
p("KeynetikPOS asks for a CD key the first time it runs. Enter the key you were given. "
  "Activation happens online; if the internet is down, the app activates locally so an "
  "offline shop is never locked out. A trial period is available before activation.")
h2("Signing in")
p("Log in with your username and password. The system ships with one administrator account "
  "so you can get started:")
S.append(table([
    ["Field", "Default value"],
    ["Username", "admin"],
    ["Password", "admin123"],
], [40*mm, PW-2*MARGIN-40*mm]))
S.append(callout("Change the default password immediately",
    "On first login as admin you are required to set a new password (minimum 8 characters). "
    "Passwords are stored securely (PBKDF2 hashing) — never in plain text.", WARNBG, WARNBAR))

# ============================================================= 3 screen tour
h1("3. The sales screen at a glance")
p("The main window is split into two panels. The <b>left</b> shows your products; the "
  "<b>right</b> is the current sale (the cart).")
S.append(table([
    ["Area", "What it is"],
    ["Search box", "Type a product name or scan a barcode to find items fast."],
    ["Category buttons", "Filter the product grid to one category."],
    ["Product grid", "Tap a card to add the item to the cart. Cards are colour-coded by stock level."],
    ["Cart table", "The items in the current sale, with quantity steppers (minus / plus) and a remove (X) button."],
    ["Cart tabs", "Run several sales at once — start a new tab for a second customer and switch between them."],
    ["Totals", "Live subtotal, tax and total as you add items."],
    ["Checkout button", "Opens payment to finish the sale."],
]))
S.append(callout("Touchscreen-friendly",
    "Buttons, cards and the minus/plus quantity steppers are sized for finger taps, so KeynetikPOS "
    "works on a touchscreen terminal as well as with a mouse and keyboard."))

# ============================================================= 4 making a sale
h1("4. Making a sale, step by step")
S.append(steps([
    "<b>Add items.</b> Tap a product card, scan its barcode, or search by name. Repeat for every item.",
    "<b>Adjust quantities.</b> Use the minus / plus steppers in the cart, or type a quantity. Stock is checked so you can't oversell.",
    "<b>Apply a discount</b> (optional) — see section 6.",
    "<b>Attach a customer</b> (optional) to earn loyalty points or use store credit — see section 7.",
    "<b>Press Checkout</b> and take payment — see section 5.",
    "<b>Hand over the receipt.</b> Print it, or email it to the customer.",
]))
p("After a sale the cart clears and the cursor returns to the search box, ready for the next "
  "customer. The change due is shown on the status bar so a queue keeps moving.")
S.append(callout("Multiple carts",
    "Need to pause one customer to serve another? Open a new cart tab. Each tab is an independent "
    "sale; switch back any time to finish it."))

# ============================================================= 5 payment
h1("5. Taking payment")
p("Pressing Checkout opens the payment dialog. Choose a method, enter the amount tendered, and "
  "the change is calculated for you.")
S.append(table([
    ["Method", "Notes"],
    ["Cash", "Enter the amount received; change due is shown."],
    ["Card", "Record a card payment with an optional reference."],
    ["M-Pesa (Mobile Money)", "A reference number is required and is printed on the receipt for reconciliation."],
    ["Split payment", "Combine methods on one sale — e.g. part cash, part M-Pesa. Each tender is recorded separately so your end-of-day method totals stay accurate."],
]))
h3("Receipts")
S.append(bullets([
    "<b>Print</b> to a connected thermal or standard printer.",
    "<b>Email</b> the last receipt to the customer (requires email settings — see section 15).",
    "Receipts show the cashier, items, tax, totals, payment method and any M-Pesa reference.",
]))

# ============================================================= 6 discounts
h1("6. Discounts")
p("Discounts are protected by a PIN so only authorised staff can apply them. You can give either "
  "a <b>percentage</b> (e.g. 10% off) or a <b>fixed amount</b> (e.g. KSh 50 off).")
S.append(steps([
    "In the cart, open the discount dialog.",
    "Enter the discount PIN.",
    "Choose percentage or fixed amount and enter the value, with an optional reason.",
    "The discount is applied to the total and recorded for the audit trail.",
]))
S.append(callout("Set your discount PIN",
    "The discount PIN is configured under Settings. Every discount is logged with the staff member, "
    "amount and reason, so nothing is given away untracked."))

# ============================================================= 7 customers
h1("7. Customers, loyalty &amp; store credit")
p("Attaching a customer to a sale lets them earn rewards and pay with store credit.")
S.append(table([
    ["Feature", "How it works"],
    ["Quick lookup", "Find a customer by phone number at checkout."],
    ["Loyalty points", "Earned automatically — 1 point per KSh 100 spent. Redeem points for store credit."],
    ["Store credit", "A balance the customer can spend on future sales. Top it up manually or let it accrue."],
    ["History", "Each customer's past purchases are available from the Customers screen."],
]))
p("Open the <b>Customers</b> screen to add a customer, edit details, adjust store credit, or "
  "redeem loyalty points.")

# ============================================================= 8 refunds
h1("8. Refunds")
p("A refund reverses a past sale: it restocks the items and records the refund for your books. "
  "Refunds require the relevant permission and a reason.")
S.append(steps([
    "Open the Refund screen (or select a sale in Sales History and choose Process Refund).",
    "Enter or confirm the sale number; its items are shown.",
    "Type the reason for the refund.",
    "Confirm. Stock is returned and the refund is logged.",
]))
S.append(callout("No double refunds",
    "A sale can only be refunded once. The system blocks a second refund automatically so stock "
    "is never credited back twice.", WARNBG, WARNBAR))

# ============================================================= 9 inventory
h1("9. Inventory &amp; stock control")
p("The <b>Inventory</b> screen is mission control for stock: view and edit products, see stock "
  "levels at a glance, and manage reorder thresholds.")
S.append(bullets([
    "<b>Add / edit products</b> — name, category, cost price, profit margin (the selling price is computed for you), stock quantity, reorder level and barcode.",
    "<b>Low-stock alerts</b> — products at or below their reorder level are flagged; the product grid colours cards red/orange/green by status.",
    "<b>Reorder levels</b> — set per product so alerts match how fast each item sells.",
    "<b>Stock take</b> — count physical stock and post adjustments; every change is logged with a reason and who made it.",
    "<b>Backup Now</b> and other admin actions live on the menus.",
]))
S.append(callout("Every stock change is auditable",
    "Sales, refunds, deliveries and manual adjustments each write an audit row recording the old and "
    "new quantity, the reason, the user and the time."))

# ============================================================= 10 purchasing
h1("10. Purchasing: suppliers &amp; purchase orders")
p("Track stock <i>arriving</i> into the business, with costs, so your margins stay accurate.")
S.append(steps([
    "Open <b>Suppliers</b> and add the businesses you buy from.",
    "Open <b>Purchase Orders</b> and create a new order: choose a supplier and add the products and quantities you're ordering, each with its unit cost.",
    "When the goods arrive, mark the order <b>Received</b>.",
    "Receiving raises stock for every line, updates each product's cost price to the latest, and logs an adjustment — all in one step.",
]))
S.append(callout("Costs flow through automatically",
    "Receiving a purchase order updates the product cost used for profit calculations, so your "
    "reports reflect what you actually paid."))

# ============================================================= 11 expenses
h1("11. Expenses")
p("Record business expenses (rent, utilities, wages, other) so your profit-and-loss is complete. "
  "Open the <b>Expenses</b> screen, choose a category, enter the amount and date, and save. "
  "Expenses are deducted from gross profit in the P&amp;L report and posted to the ledger.")

# ============================================================= 12 reports
h1("12. Reports, analytics &amp; sales history")
S.append(table([
    ["Report", "Tells you"],
    ["Daily report", "Today's takings and transaction count at a glance."],
    ["Detailed reports", "Sales by date range, category or payment method; top sellers; daily summary; profit &amp; loss; stock valuation — exportable to CSV and PDF."],
    ["Analytics dashboard", "Date-range metrics: sales, profit, margin, tax, discounts, items, top products and slow movers."],
    ["Sales history", "Browse and search every past sale; view details or start a refund."],
]))
p("Use the date pickers and quick filters (Today, Last 7 days, This month, etc.) to focus any "
  "report on the period you care about.")

# ============================================================= 13 shifts
h1("13. Shifts &amp; the Z-report")
p("Shift tools let a cashier open a till with a counted starting float and close it with a "
  "settlement (Z-report) showing expected versus counted cash. Use these to reconcile the drawer "
  "at the end of a shift.")
S.append(Paragraph("Note: shift opening/closing is available as a tool; ask your administrator if "
                   "it has been enabled for your terminal.", SMALL))

# ============================================================= 14 finance
h1("14. Finance: ledger, payroll &amp; VAT")
p("KeynetikPOS includes a real accounting back office (Finance menu, available on the full ERP "
  "tier). Sales, purchases and expenses post to it automatically.")
S.append(table([
    ["Module", "What it gives you"],
    ["General Ledger", "A double-entry trial balance that always reconciles; post manual journal entries."],
    ["Payroll", "Kenyan statutory payroll — PAYE, SHIF, NSSF and Housing Levy — with payslips and a posted wage journal."],
    ["VAT", "VAT-3 return figures (output and input VAT) over any period, with per-product tax codes; record VAT payments."],
]))
S.append(callout("One tax rate, everywhere",
    "The VAT rate and the POS sale-tax rate are the same single setting, so what customers are charged "
    "always matches what your VAT return reports."))

# ============================================================= 15 settings
h1("15. Settings")
S.append(bullets([
    "<b>Business</b> — shop name, contact details, currency symbol and code.",
    "<b>Tax</b> — enable/disable tax, the rate, and whether prices are tax-inclusive.",
    "<b>Discount PIN</b> — the code required to apply discounts.",
    "<b>Email (SMTP)</b> — server, port, security and credentials for emailing receipts; the password is encrypted at rest.",
    "<b>Messaging</b> — WhatsApp / SMS providers for scheduled messages and reports.",
    "<b>Theme</b> — switch between light and dark at any time.",
]))

# ============================================================= 16 backups
h1("16. Backups &amp; data safety")
p("Your data lives in a single database file on this computer. KeynetikPOS protects it for you:")
S.append(bullets([
    "<b>Automatic daily backup</b> — once per day the app copies the database to a backups folder and keeps the newest ten copies.",
    "<b>Backup Now</b> — make an on-demand backup from the menu before risky operations.",
    "<b>Integrity checks</b> — backups are verified so you know they're readable.",
]))
S.append(callout("Keep a copy off the machine",
    "The built-in backups guard against accidental data loss, but they're on the same computer. "
    "Periodically copy the backups folder to a USB drive or cloud storage to survive hardware failure.",
    WARNBG, WARNBAR))

# ============================================================= 17 roles
h1("17. Staff accounts, roles &amp; permissions")
p("Administrators manage staff under <b>User Management</b>. Each user has a role that controls "
  "what they can see and do.")
S.append(table([
    ["Role", "Typical access"],
    ["Administrator", "Everything — settings, users, reports, refunds, backups."],
    ["Manager", "Day-to-day operations and reports; most actions except sensitive admin."],
    ["Cashier", "Selling, basic lookups; restricted from sensitive actions."],
]))
p("Permissions gate individual actions (refunds, discounts, stock adjustments, reports, backups). "
  "Buttons a user isn't allowed to use are disabled, and the rule is also enforced underneath the UI.")

# ============================================================= 18 troubleshooting
h1("18. Troubleshooting &amp; FAQ")
S.append(table([
    ["Question / problem", "What to do"],
    ["I forgot the admin password", "Another administrator can reset it under User Management. If none exists, contact your supplier."],
    ["The app says my licence is invalid", "Check the key was entered correctly. If you changed computers, you may need a re-activation — contact your supplier."],
    ["A barcode scanner isn't adding items", "Confirm the scanner is connected; the search box must have focus. Scanners type the code then press Enter."],
    ["Receipt email didn't send", "Check the SMTP settings and that the computer is online. Use an app password if your mail provider requires one."],
    ["Stock looks wrong", "Open the product's stock history to see every change; run a stock take to correct and log the difference."],
    ["A sale won't complete (insufficient stock)", "Another action reduced stock; re-check quantities. The sale is kept so you can adjust and retry."],
]))
sp(6)
S.append(Paragraph("KeynetikPOS User Manual &bull; Version 2.0 &bull; Keep this guide near the till "
                   "for quick reference.", SMALL))

# ============================================================= build
Doc(OUT, "User Manual", "Next-generation professional point of sale").build(S)
print("wrote", OUT)
