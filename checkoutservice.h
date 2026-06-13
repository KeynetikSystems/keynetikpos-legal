// =============================================================================
// checkoutservice.h — CheckoutService: totals math + the sale pipeline
// -----------------------------------------------------------------------------
// WHAT: computeCartTotals() derives subtotal/discount/tax/total from the
//       configurable tax settings. CheckoutService::finalizeSale() runs the
//       post-payment pipeline: cart -> SaleItems -> Database::recordSale()
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

#include "cart.h"
#include "settingsmanager.h"   // BusinessSettings

class InventoryManager;
class ReceiptPrinter;

// Totals derived from the configurable tax settings. The discount reduces
// the taxable base; with tax-inclusive pricing the tax shown is informational.
struct CartTotals {
    double subtotal = 0.0;
    double discount = 0.0;
    double tax      = 0.0;
    double total    = 0.0;
};

CartTotals computeCartTotals(double subtotal, double discount,
                             const BusinessSettings &bs);

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
                                double amountPaid, double change) const;

private:
    InventoryManager *m_inventory;
    ReceiptPrinter   *m_printer;
};

#endif // CHECKOUTSERVICE_H
