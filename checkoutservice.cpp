// =============================================================================
// checkoutservice.cpp — Implementation of CheckoutService (see
// checkoutservice.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - recordSale() is atomic: sale + items + stock decrement commit together,
//    or nothing does. On failure finalizeSale() returns ok=false with the
//    Database error and touches nothing else — no receipt, no inventory
//    refresh, no audit entry.
//  - Receipt printing happens after the sale is durably recorded; a printer
//    problem can never lose a sale.
// =============================================================================
#include "checkoutservice.h"

#include "database.h"
#include "customerrepository.h"
#include "salerepository.h"
#include "inventorymanager.h"
#include "receiptprinter.h"
#include "ledger.h"
#include "salejournal.h"

#include <QSqlDatabase>
#include <QDate>
#include <QDateTime>

CheckoutService::CheckoutService(Database &db, InventoryManager *inventory,
                                 ReceiptPrinter *printer, OperatorContext op)
    : m_db(db)
    , m_inventory(inventory)
    , m_printer(printer)
    , m_operator(std::move(op))
{
}

CheckoutResult CheckoutService::finalizeSale(const Cart &cart,
                                             const CartTotals &t,
                                             const QString &paymentMethod,
                                             const QString &referenceNumber,
                                             Money amountPaid,
                                             Money change,
                                             int customerId,
                                             Money storeCreditUsed,
                                             const QVector<SalePayment> &payments) const
{
    CheckoutResult result;

    QVector<SaleItem> saleItems;
    saleItems.reserve(cart.items.size());
    for (const CartItem &item : cart.items) {
        SaleItem si;
        si.productId   = item.productId;
        si.productName = item.name;
        si.quantity    = item.quantity;
        si.price       = item.price;
        si.costPrice   = item.costPrice;
        si.subtotal    = item.price * item.quantity;
        saleItems.append(si);
    }

    // Atomic: sale + items + stock decrement + customer loyalty/credit all
    // commit together, or nothing does.
    SaleRequest request;
    request.items           = saleItems;
    request.subtotal        = t.subtotal;
    request.tax             = t.tax;
    request.discount        = t.discount;
    request.total           = t.total;
    request.paymentMethod   = paymentMethod;
    request.amountPaid      = amountPaid;
    request.changeDue       = change;
    request.customerId      = customerId;
    request.storeCreditUsed = storeCreditUsed;
    request.payments        = payments;
    // Audit: stamp the sale with the signed-in cashier. shiftId stays 0 until a
    // live shift is wired into checkout (ShiftManager isn't on this path yet).
    request.cashier         = m_operator.username;

    const int saleId = m_db.sales().recordSale(request);

    if (saleId < 0) {
        result.error = m_db.sales().lastError();
        return result;
    }
    result.ok     = true;
    result.saleId = saleId;

    for (const CartItem &item : cart.items)
        m_inventory->refreshAfterSale(item.productId);

    // Auto-post the sale to the General Ledger so the books stay complete:
    // Dr cash/M-Pesa/bank (+ AR for non-cash) / Cr Sales + VAT, plus the
    // COGS/Inventory pair. Best-effort and isolated — a ledger hiccup must
    // never undo a durably recorded sale.
    {
        QVector<SaleTender> tenders;
        if (payments.isEmpty()) {
            // Single-tender path: the whole net amount went to one method.
            tenders.append({ paymentMethod, t.total - storeCreditUsed });
        } else {
            for (const SalePayment &p : payments)
                tenders.append({ p.method, p.amount });
        }

        Money cogs;
        for (const CartItem &item : cart.items)
            cogs += item.costPrice * item.quantity;

        const QVector<GLLine> lines =
            buildSaleJournal(t.total, t.tax, tenders, storeCreditUsed, cogs);

        if (!lines.isEmpty()) {
            Ledger ledger(QSqlDatabase::database());
            ledger.initSchema();   // idempotent; ensures the chart exists
            if (ledger.postEntry(QDate::currentDate(),
                                 QString("Sale #%1").arg(saleId), "sale", lines) < 0) {
                m_operator.log(
                    "Ledger Posting Failed",
                    QString("Sale #%1 not posted to GL: %2")
                        .arg(saleId).arg(ledger.lastError()));
            }
        }
    }

    Receipt receipt;
    receipt.saleId          = saleId;
    receipt.dateTime        = QDateTime::currentDateTime();
    receipt.items           = cart.items;
    receipt.subtotal        = t.subtotal;
    receipt.tax             = t.tax;
    receipt.discount        = t.discount;
    receipt.discountReason  = cart.discountReason;
    receipt.total           = t.total;
    receipt.paymentMethod   = paymentMethod;
    receipt.referenceNumber = referenceNumber;
    receipt.amountPaid      = amountPaid;
    receipt.change          = change;
    receipt.customerName    = customerId > 0
                                  ? m_db.customers().getCustomerById(customerId).name
                                  : QString();
    receipt.cashierName     = m_operator.fullName;

    // Best-effort: a sale is already durably recorded by here, so a missing or
    // failing printer must never undo it (and a terminal with no receipt printer
    // wired up shouldn't crash checkout).
    if (m_printer)
        m_printer->printReceipt(receipt);

    m_operator.log(
        "Sale Completed",
        QString("Sale #%1, Total: %2, Method: %3%4, Items: %5")
            .arg(saleId).arg(formatMoney(t.total))
            .arg(paymentMethod)
            .arg(referenceNumber.isEmpty()
                     ? QString()
                     : QString(" (ref %1)").arg(referenceNumber))
            .arg(cart.items.size()));

    return result;
}
