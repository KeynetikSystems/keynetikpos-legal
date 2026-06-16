# KeynetikPOS — User Guide & Manual

**KeynetikPOS — Next-GEN Professional Point of Sale System**
**Version 2.0.0 · Keynetik Solutions**

Welcome! This manual explains how to use KeynetikPOS day to day — from logging in
and ringing up a sale to managing stock, running reports, and closing the till. No
technical background is required.

> Looking for installation, backup, or admin/server details? See
> [`SYSTEM_DOCUMENTATION.md`](SYSTEM_DOCUMENTATION.md).

---

## Table of contents

1. [Getting started](#1-getting-started)
2. [The main screen](#2-the-main-screen)
3. [Making a sale](#3-making-a-sale)
4. [Discounts](#4-discounts)
5. [Taking payment & receipts](#5-taking-payment--receipts)
6. [Working with multiple carts](#6-working-with-multiple-carts)
7. [Barcode scanning](#7-barcode-scanning)
8. [Refunds & sales history](#8-refunds--sales-history)
9. [Managing inventory](#9-managing-inventory)
10. [Shifts & cash drawer](#10-shifts--cash-drawer)
11. [Reports & analytics](#11-reports--analytics)
12. [Scheduled reports (WhatsApp / SMS)](#12-scheduled-reports-whatsapp--sms)
13. [User accounts & roles](#13-user-accounts--roles)
14. [Settings](#14-settings)
15. [Keyboard shortcuts](#15-keyboard-shortcuts)
16. [Frequently asked questions](#16-frequently-asked-questions)

---

## 1. Getting started

### First login

When KeynetikPOS opens, you'll see a **login** window. On a brand-new
installation, use the default administrator account:

- **Username:** `admin`
- **Password:** `admin123`

You will be **required to set a new password** immediately on this first login.
Choose a password that is at least **8 characters** and different from the old one,
then confirm it. (Keep it safe — there is no self-service reset for the last admin
account.)

> You have **3 login attempts** per session. After three failures the app closes —
> just reopen it to try again.

### Trial vs. full licence

If you're evaluating the software, it runs in **30-day trial** mode and will
occasionally remind you how many days remain. To unlock the full version, enter
your **CD key** when prompted (or ask your administrator). KeynetikPOS works fully
offline; an internet connection is only needed to activate a key and to send
scheduled reports.

### Switching theme

KeynetikPOS supports **Light** and **Dark** themes. Toggle it any time from
**Settings → 🌙 Toggle Dark Mode**. Your choice is remembered.

---

## 2. The main screen

The main window is your sales workspace. From left to right and top to bottom:

- **Menu bar** — File, Sales, Inventory, Reports, Settings, User, Help.
- **Product grid** (left) — cards for every product, colour-coded by stock level
  (green = healthy, orange = low, red = critical, grey = out of stock). Each card
  shows the price and a stock note (e.g. "LOW — 3 left", "OUT OF STOCK").
- **Search box** — filter products by name or barcode as you type.
- **Category filter** — narrow the grid to one product category.
- **Cart panel** (right) — the current customer's order: product, price, quantity
  (with − / ＋ tap steppers), subtotal, and an ✖ to remove a line.
- **Totals panel** — subtotal, discount, tax, and the grand total.
- **Action buttons** — Apply Discount, Checkout, New Cart.
- **Status bar** (bottom) — shows who is logged in, their role, and transient
  messages (like "Change due: …" after a sale).

The window adapts to your screen and works on touch displays (bigger tap targets,
− / ＋ quantity buttons). You can drag the divider between the product grid and the
cart to resize them.

---

## 3. Making a sale

1. **Add products to the cart**, by any of:
   - **Clicking a product card** in the grid.
   - **Searching** (press **F3** or click the search box), typing a name or
     barcode, then selecting the product. Use arrow keys + **Enter** to add the
     highlighted card.
   - **Scanning a barcode** (see §7) — the matching product is added instantly.
2. **Adjust quantities** in the cart using the **−** and **＋** buttons, or type a
   quantity directly. You can't exceed available stock — the app clamps it and
   warns you.
3. **Remove a line** with the **✖** button on that row, or clear the whole cart
   with **Ctrl+W**.
4. Watch the **totals** update live (subtotal → discount → tax → total).
5. When ready, **Checkout** (press **F2** or click Checkout).

> Out-of-stock products appear greyed out and cannot be added.

---

## 4. Discounts

Discounts are protected to prevent accidental or unauthorized markdowns.

1. With items in the cart, click **Apply Discount** (or press **F4**).
2. Enter the **discount PIN** when prompted (set by your administrator in
   Settings).
3. Choose **Percentage** (e.g. 10%) or **Fixed amount** (e.g. KSh 50 off), enter
   the value, and type a **reason** (required — it's recorded for the audit log).
4. The dialog shows a **live preview** of the discount amount and the new total.
   A fixed discount can never push the total below zero, and a percentage can't
   exceed 100%.
5. Confirm. The discount is applied to the current cart and recorded against your
   user and shift.

Cashiers and Managers can apply discounts; Viewers cannot.

---

## 5. Taking payment & receipts

After **Checkout** (F2):

1. The **Payment** dialog opens. Choose a method: **Cash**, **Card**,
   **Mobile** (M-Pesa / mobile money), or **Multiple** (split tender).
2. Enter the **amount tendered**. For cash, the **change due** is calculated live,
   and you can't confirm until the amount covers the total.
3. For **Mobile money**, enter the **reference number** (required) — it's printed
   on the receipt.
4. Confirm payment. The sale is recorded, stock is reduced automatically, and a
   **receipt** is generated.

**Receipts:** in the current build receipts are saved as **PDF** files
(`Receipt_<number>_<date>.pdf`), so you can print, archive, or share them even
without a printer attached. To reprint the last receipt, go to
**Sales → 🖨 Reprint Last Receipt**.

After a successful sale you'll see a brief confirmation toast and the change-due
amount in the status bar — no pop-up to dismiss, so you can serve the next
customer immediately.

---

## 6. Working with multiple carts

KeynetikPOS lets you keep **several orders open at once** — handy when one customer
steps away to grab a forgotten item.

- **New cart:** **File → ➕ New Sale**, the **New Cart** button, or **F5**.
- **Switch carts:** click the cart tabs.
- **Delete or merge a cart:** right-click a cart tab for **🗑 Delete Cart** or
  **🔀 Merge Cart**.

Each cart has its own items, discount, and totals. Checkout applies to the cart
you're currently viewing.

---

## 7. Barcode scanning

KeynetikPOS works with two kinds of scanner, and usually needs no setup:

- **USB "keyboard-wedge" scanners** — plug in and scan; the product is added to
  the current cart automatically.
- **Serial / COM-port scanners** — selectable in settings (your administrator
  configures the COM port).

If you scan a code the system doesn't recognise, you'll see a quick "unknown
barcode" message. You can **assign** a barcode to a product, or **generate and
print EAN-13 labels** for unlabelled stock, from the barcode tools (under
Inventory).

---

## 8. Refunds & sales history

Go to **Sales → 📜 View Sales History**.

- **Browse** every transaction with filters: date range, quick filters (Today,
  This Week), payment method, and free-text search.
- A **summary strip** shows total sales, count, average, and tax for the current
  filter.
- **Drill down** into any sale to see its line items.
- **Export** the list to CSV.

**To process a refund:**

1. Select the **original sale** in the list.
2. Click **Refund** and enter a **reason**.
3. Confirm. The refund is recorded, stock is restored, and the sale is marked
   refunded.

You can't refund the same sale twice, and refunds always tie back to a real
transaction — this protects against refunding amounts that were never charged.
Refund rights depend on your role.

> The Sales History window can stay **open while you keep selling** — it won't
> block the till.

---

## 9. Managing inventory

Open **Inventory → 📦 Manage Inventory** for the full workbench, with tabs:

- **Overview** — searchable/filterable product table plus summary cards
  (total products, stock value, counts by status).
- **Stock Management** — edit stock directly in a spreadsheet-style grid; invalid
  entries are highlighted with tooltips. Changes auto-save (or save explicitly),
  and you have **undo/redo**.
- **Alerts** — low- and critical-stock lists with one-click **bulk restock**.
- **Analytics** — stock value, fast and slow movers.

Other handy entry points:

- **Inventory → ➕ Add Product** — quickly add a new catalog item.
- **Inventory → 🚨 Low Stock Alert** — a focused popup of exactly what needs
  reordering now, with restock buttons. The app also pops up a toast when an item
  crosses a low/critical threshold during sales.

Inventory features also support **CSV import/export** and printing. Every manual
stock change is **logged** (who, when, why). Adding/editing products and adjusting
stock require Manager or Admin rights; deleting products is Admin-only.

> The Inventory window can also stay open while you continue ringing up sales.

---

## 10. Shifts & cash drawer

Shifts track who was on the till and how much cash should be in the drawer.

1. **Open a shift** — enter the cashier name and the **counted opening float**
   (the cash you start with).
2. **During the shift**, sales, discounts, and voids accumulate automatically.
3. **Close the shift** — the app shows the expected totals; count the drawer,
   enter the **closing float** and any notes. The **over/short** variance is
   calculated for you.

If the app crashes or the power cuts out mid-shift, the open shift is
**re-adopted** on restart — you won't lose it. The end-of-shift figures feed the
**Z-Report** (§11).

---

## 11. Reports & analytics

**On-screen analytics** — **Reports → 📊 Analytics Dashboard**:

- Revenue today vs. yesterday (with change indicator), this week, this month, and
  growth.
- Top products (with medals), category breakdown, peak business hours, and
  alerts for products with zero sales.
- Pick different periods (7 / 30 / 90 days) or explicit date ranges (depending on
  the dashboard), and view profit/margin and tax-collected metrics.

**Quick daily report** — **Reports → 📋 Daily Report** gives a fast snapshot of the
day's numbers.

**Operational reports** (the printable ones for an accountant) — choose a report
type (sales by date, by category, by payment method, top sellers, daily summary)
and a date range, then **export to CSV or PDF**.

**Z-Report** (end-of-day / end-of-shift fiscal summary) — gross sales, discounts,
net sales, tax collected, transaction count, sales by category, top products, and
**cash reconciliation** (opening float, closing float, expected cash, and the
over/short variance). This is the standard control document for detecting drawer
shortages.

Reporting/analytics access depends on your role (Cashiers don't see reports;
Viewers can view but not export).

---

## 12. Scheduled reports (WhatsApp / SMS)

KeynetikPOS can **send reports to your phone automatically** — so the owner gets
the day's numbers without standing at the till.

Set up under **Settings → 📅 Message Schedules**:

1. **Add a schedule** — give it a name, choose **when** it fires (Daily, Weekly,
   Monthly, on shift close, or when sales pass a threshold), pick **which report**
   (Z-Report, inventory alert, sales summary…), set the **time**, add
   **recipients**, and choose a **provider** (WhatsApp via Twilio, or SMS via
   Africa's Talking).
2. **Send Now** — test the schedule immediately to confirm your messaging
   credentials work (much better than discovering a typo at 9 p.m. when the
   nightly report silently fails).
3. **Enable / disable** schedules with the toggle; the list shows each schedule's
   status and when it last sent.

Provider credentials (Twilio SID/token/number; Africa's Talking API
key/username/sender ID) are entered by an administrator in Settings → Messaging.

---

## 13. User accounts & roles

Administrators manage staff accounts under **User → 👤 User Management**:

- **Add / edit / delete** users, **enable/disable** accounts, and
  **change passwords**. New users get a temporary password and must set their own
  at first login.
- A live **password-strength** indicator helps create strong passwords.
- A **permissions viewer** shows exactly what each role can do.

**Anyone** can change their own password via **User → 🔑 Change Password**, and
log out via **User → 🚪 Logout** (or **Ctrl+L**). Logging out returns you to the
login screen without closing the program.

### What each role can do

| Task | Admin | Manager | Cashier | Viewer |
|---|:---:|:---:|:---:|:---:|
| Make sales | ✓ | ✓ | ✓ | — |
| View sales | ✓ | ✓ | ✓ | ✓ |
| Apply discounts | ✓ | ✓ | ✓ | — |
| Void / delete transactions | ✓ | void only | — | — |
| View inventory | ✓ | ✓ | ✓ | ✓ |
| Add / edit products, adjust stock | ✓ | ✓ | — | — |
| Delete products | ✓ | — | — | — |
| View reports & analytics | ✓ | ✓ | — | ✓ |
| Export reports | ✓ | ✓ | — | — |
| Manage users | ✓ | — | — | — |
| Settings, backup, logs | ✓ | — | — | — |

Buttons and menu items you don't have permission for are **greyed out**.

---

## 14. Settings

**Settings → 🏢 Company Information** (and related menus) open the configuration
editor (Admin only), with tabs for:

- **Business** — name, address, phone, email, website, **currency** (default
  `KSh`), and receipt footer.
- **Tax** — turn tax on/off, set the **rate** and **label** (e.g. "VAT"), and
  choose **tax-inclusive pricing** if your shelf prices already include tax.
- **Receipt** — header/footer and whether to print receipts.
- **Security** — the **discount PIN** (entered twice to confirm) and whether a PIN
  is required for discounts.
- **Messaging** — Twilio WhatsApp and Africa's Talking credentials, with a
  **test-connection** button.

Changes apply immediately — for example, changing the currency re-labels prices
across the app without a restart.

---

## 15. Keyboard shortcuts

Fast keys for busy tills:

| Key | Action |
|---|---|
| **F1** | Show keyboard shortcuts help |
| **F2** | Checkout (take payment) |
| **F3** | Focus the product search box |
| **F4** | Apply a discount |
| **F5** | New cart / new sale |
| **Ctrl+N** | New sale |
| **Ctrl+W** | Clear the current cart |
| **Ctrl+L** | Log out |
| **Ctrl+Q** | Exit the application |

Hover over toolbar buttons to see their shortcuts in the tooltip.

---

## 16. Frequently asked questions

**I forgot my password — what do I do?**
Ask an administrator to reset it (User Management → Change Password). They'll set a
temporary password that you'll change at your next login. If the *last admin*
account is locked out, the system administrator must restore from a backup — see
[`SYSTEM_DOCUMENTATION.md`](SYSTEM_DOCUMENTATION.md).

**Do I need the internet to sell?**
No. KeynetikPOS runs fully offline. Internet is only used to activate your licence
and to send scheduled WhatsApp/SMS reports.

**My barcode scanner isn't adding products.**
Most USB scanners work with no setup. If yours connects by serial cable, ask your
administrator to select the correct COM port. Check that the scanned product
actually exists in the catalog — unknown codes show an "unknown barcode" message.

**Why can't I see Reports / Settings / certain buttons?**
Those require a higher role (Manager or Admin). Greyed-out items are ones your
account isn't permitted to use.

**Where do my receipts go?**
They're saved as PDF files named `Receipt_<number>_<date>.pdf`. You can open and
print them later, or attach a real printer for direct printing.

**The total looks off by a cent on a big order.**
Rounding on very large or unusual orders can occasionally differ by a cent due to
how amounts are stored. For normal retail transactions this is not noticeable.

**How do I back up my data?**
That's an administrator task — the whole business is in one database file. See the
Backup section of [`SYSTEM_DOCUMENTATION.md`](SYSTEM_DOCUMENTATION.md).

---

*Need help? Contact support@keynetik.com.*
*KeynetikPOS v2.0.0 — © Keynetik Solutions.*
