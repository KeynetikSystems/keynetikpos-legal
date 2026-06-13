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
#include "inventorymanager.h"
#include "receiptprinter.h"
#include "usermanager.h"

#include <QDateTime>

CheckoutService::CheckoutService(InventoryManager *inventory,
                                 ReceiptPrinter *printer)
    : m_inventory(inventory)
    , m_printer(printer)
{
}

CheckoutResult CheckoutService::finalizeSale(const Cart &cart,
                                             const CartTotals &t,
                                             const QString &paymentMethod,
                                             const QString &referenceNumber,
                                             double amountPaid,
                                             double change) const
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
        si.subtotal    = roundCents(item.price * item.quantity);
        saleItems.append(si);
    }

    // Atomic: sale + items + stock decrement commit together, or nothing does.
    const int saleId = Database::instance().recordSale(
        saleItems, t.subtotal, t.tax, t.discount, t.total,
        paymentMethod, amountPaid, change);

    if (saleId < 0) {
        result.error = Database::instance().getLastError();
        return result;
    }
    result.ok     = true;
    result.saleId = saleId;

    for (const CartItem &item : cart.items)
        m_inventory->refreshAfterSale(item.productId);

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
    receipt.customerName    = "";
    receipt.cashierName     =
        UserManager::instance().getCurrentUser().fullName;

    m_printer->printReceipt(receipt);

    UserManager::instance().logUserAction(
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
