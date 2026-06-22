// =============================================================================
// salejournal.cpp — Implementation of buildSaleJournal (see salejournal.h).
// =============================================================================
#include "salejournal.h"

QString saleTenderAccount(const QString &method)
{
    const QString m = method.toLower();
    if (m.contains("mpesa") || m.contains("m-pesa") || m.contains("mobile"))
        return "1010";   // M-Pesa / Mobile Money
    if (m.contains("card") || m.contains("visa") || m.contains("master")
        || m.contains("bank") || m.contains("transfer") || m.contains("cheque"))
        return "1020";   // Bank
    return "1000";       // Cash (and any unrecognised tender)
}

QVector<GLLine> buildSaleJournal(Money total, Money tax,
                                 const QVector<SaleTender> &tenders,
                                 Money storeCreditUsed, Money cogs)
{
    QVector<GLLine> lines;

    // ── Debit: money received, by tender ─────────────────────────────────────
    qint64 receivedCash = 0;
    for (const SaleTender &tn : tenders) {
        if (tn.amount.cents() <= 0) continue;
        lines.append({ saleTenderAccount(tn.method), tn.amount, Money(),
                       "Sale tender: " + tn.method });
        receivedCash += tn.amount.cents();
    }

    // Anything not settled by a cash/card/mobile tender (store credit redeemed,
    // or a credit sale) is value owed/applied — booked to Accounts Receivable.
    const qint64 nonCash = total.cents() - receivedCash;
    if (nonCash > 0)
        lines.append({ "1100", Money::fromCents(nonCash), Money(),
                       storeCreditUsed.cents() > 0 ? "Store credit / on account"
                                                   : "On account" });

    // ── Credit: revenue (net of VAT) + output VAT ────────────────────────────
    const Money netSales = total - tax;   // holds for tax-inclusive and exclusive
    if (netSales.cents() > 0)
        lines.append({ "4000", Money(), netSales, "Sales revenue" });
    if (tax.cents() > 0)
        lines.append({ "2100", Money(), tax, "Output VAT" });

    // ── Cost of goods sold (balanced pair) ───────────────────────────────────
    if (cogs.cents() > 0) {
        lines.append({ "5000", cogs, Money(), "Cost of goods sold" });
        lines.append({ "1200", Money(), cogs, "Inventory reduction" });
    }

    // A valid entry needs at least both sides; if only one leg survived
    // (degenerate totals), return nothing so the caller skips posting.
    if (lines.size() < 2)
        lines.clear();
    return lines;
}

QVector<GLLine> buildPurchaseJournal(Money total, Money inputVat)
{
    QVector<GLLine> lines;
    if (total.cents() <= 0) return lines;

    const Money net = total - inputVat;
    if (net.cents() > 0)
        lines.append({ "1200", net, Money(), "Inventory received" });
    if (inputVat.cents() > 0)
        lines.append({ "1300", inputVat, Money(), "Input VAT (recoverable)" });
    lines.append({ "2000", Money(), total, "Accounts payable" });

    if (lines.size() < 2) lines.clear();
    return lines;
}

QString expenseAccount(const QString &categoryName)
{
    const QString c = categoryName.toLower();
    if (c.contains("rent"))
        return "6100";   // Rent
    if (c.contains("util") || c.contains("electric") || c.contains("water")
        || c.contains("power") || c.contains("internet"))
        return "6200";   // Utilities
    if (c.contains("wage") || c.contains("salar") || c.contains("payroll")
        || c.contains("staff"))
        return "6000";   // Wages & Salaries
    return "6300";       // Other Expenses
}

QVector<GLLine> buildExpenseJournal(const QString &categoryName, Money amount)
{
    QVector<GLLine> lines;
    if (amount.cents() <= 0) return lines;
    lines.append({ expenseAccount(categoryName), amount, Money(),
                   categoryName.isEmpty() ? "Expense" : categoryName });
    lines.append({ "1000", Money(), amount, "Cash paid" });
    return lines;
}
