// =============================================================================
// checkoutservice.h — CheckoutService: totals math + the sale pipeline
// -----------------------------------------------------------------------------
// WHAT: CheckoutService::finalizeSale() runs the post-payment pipeline. The
//       totals math (CartTotals/computeCartTotals) now lives in carttotals.h so
//       it can be unit-tested without the DB/UI; this header re-exports it via
//       that include. Pipeline: cart -> SaleItems -> Database::recordSale()
//       (atomic stock check + insert + decrement) -> receipt print ->
//       InventoryManager::refreshAfterSale() per product -> audit log.
// HOW:  Plain class (no QObject) holding the InventoryManager and
//       ReceiptPrinter it needs; the payment details are passed in from
//       PaymentDialog by the caller. Returns a CheckoutResult so the UI
//       decides how to present success/failure.
// WHY:  The sales pipeline is business logic, not window logic — extracting
//       it from MainWindow::onCheckout() makes the money path reviewable in
//       isolation. Stock is only VALIDATED in the UI but ENFORCED in
//       recordSale(); on failure the caller keeps the cart so the cashier
//       can retry.
// =============================================================================
#ifndef CHECKOUTSERVICE_H
#define CHECKOUTSERVICE_H

#include <QString>
#include <QVector>

#include "cart.h"
#include "carttotals.h"        // CartTotals, computeCartTotals()
#include "database.h"          // SalePayment

class InventoryManager;
class ReceiptPrinter;

struct CheckoutResult {
    bool    ok     { false };
    int     saleId { -1 };
    QString error;
};

class CheckoutService
{
public:
    CheckoutService(InventoryManager *inventory, ReceiptPrinter *printer);

    CheckoutResult finalizeSale(const Cart &cart, const CartTotals &totals,
                                const QString &paymentMethod,
                                const QString &referenceNumber,
                                Money amountPaid, Money change,
                                int customerId = 0,
                                Money storeCreditUsed = Money::fromCents(0),
                                const QVector<SalePayment> &payments = {}) const;

private:
    InventoryManager *m_inventory;
    ReceiptPrinter   *m_printer;
};

#endif // CHECKOUTSERVICE_H
