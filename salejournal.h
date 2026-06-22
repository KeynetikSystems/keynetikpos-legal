// =============================================================================
// salejournal.h — Build GL journal lines for sales, purchases, expenses (Tier 4)
// -----------------------------------------------------------------------------
// WHAT: Pure functions that turn a sale / received purchase / expense into
//       balanced double-entry journal lines, so every money movement can post
//       to the General Ledger automatically.
// HOW:  Sale — Dr money received per tender (cash 1000, M-Pesa 1010, card/bank
//       1020) plus non-cash settlement to AR 1100; Cr Sales 4000 (net of VAT) +
//       VAT Output 2100; plus the Dr COGS 5000 / Cr Inventory 1200 pair.
//       Purchase — Dr Inventory 1200 (net) + VAT Input 1300 / Cr Accounts
//       Payable 2000. Expense — Dr the expense account / Cr Cash 1000.
//       No DB, no UI — just maths over Money, unit-tested in isolation.
// WHY:  Keeps the ledger a complete record while keeping the account-mapping
//       logic reviewable and pinned by tests.
// =============================================================================
#ifndef SALEJOURNAL_H
#define SALEJOURNAL_H

#include <QString>
#include <QVector>

#include "ledger.h"   // GLLine
#include "money.h"

// One payment tender (method + amount). Mirrors the fields of SalePayment we
// need, without pulling in database.h.
struct SaleTender {
    QString method;
    Money   amount;
};

// Maps a payment-method string to a GL cash/bank account code.
QString saleTenderAccount(const QString &method);

// Builds the balanced journal lines for a sale. `total` is the gross amount
// (VAT-inclusive); `tax` is the VAT portion; `cogs` is total cost of goods (0
// if unknown). Zero-amount lines are omitted. Returns an empty vector when
// there is nothing to post.
QVector<GLLine> buildSaleJournal(Money total, Money tax,
                                 const QVector<SaleTender> &tenders,
                                 Money storeCreditUsed, Money cogs);

// Received purchase order (on credit): Dr Inventory 1200 (net = total - inputVat)
// + Dr VAT Input 1300 (inputVat) / Cr Accounts Payable 2000 (total).
QVector<GLLine> buildPurchaseJournal(Money total, Money inputVat);

// Maps an expense category name to a GL expense account code.
QString expenseAccount(const QString &categoryName);

// Expense paid in cash: Dr <expense account> / Cr Cash 1000.
QVector<GLLine> buildExpenseJournal(const QString &categoryName, Money amount);

#endif // SALEJOURNAL_H
