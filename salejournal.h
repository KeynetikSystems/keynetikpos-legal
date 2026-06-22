// =============================================================================
// salejournal.h — Build the GL journal lines for a completed sale (Tier 4)
// -----------------------------------------------------------------------------
// WHAT: A pure function that turns a sale's totals + tenders into balanced
//       double-entry journal lines, so every sale can post to the General
//       Ledger automatically at checkout.
// HOW:  Debit the money received (each tender to its account: cash 1000,
//       M-Pesa 1010, card/bank 1020) plus any non-cash settlement (store
//       credit / on-account) to Accounts Receivable 1100; credit Sales Revenue
//       4000 (net of VAT) and VAT Output 2100. If cost of goods is known, add
//       the matching Dr COGS 5000 / Cr Inventory 1200 pair. No DB, no UI — just
//       maths over Money, so it is unit-tested in isolation.
// WHY:  Keeps the ledger a complete record (sales were the missing piece) while
//       keeping the account-mapping logic reviewable and pinned by tests.
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

#endif // SALEJOURNAL_H
