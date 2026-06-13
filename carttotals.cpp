// =============================================================================
// carttotals.cpp — Implementation of computeCartTotals() (see carttotals.h).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Discount is clamped to [0, subtotal] so the taxable base — and therefore
//    the total — can never go negative.
//  - Every component is rounded to cents, and total is assembled from the
//    already-rounded parts, so the stored subtotal/discount/tax/total always
//    reconcile exactly (no half-cent drift between what's shown and what's
//    summed). This is the contained mitigation for double-based money: values
//    are pinned to cents at each step rather than left to accumulate error.
// =============================================================================
#include "carttotals.h"

#include "cart.h"        // roundCents()
#include <QtGlobal>      // qBound

CartTotals computeCartTotals(double subtotal, double discount,
                             const BusinessSettings &bs)
{
    CartTotals t;
    t.subtotal = roundCents(subtotal);
    t.discount = roundCents(qBound(0.0, discount, t.subtotal));
    const double base = t.subtotal - t.discount;

    if (!bs.taxEnabled || bs.taxRate <= 0.0) {
        t.tax   = 0.0;
        t.total = roundCents(base);
    } else if (bs.taxInclusive) {
        t.tax   = roundCents(base - base / (1.0 + bs.taxRate));
        t.total = roundCents(base);
    } else {
        t.tax   = roundCents(base * bs.taxRate);
        t.total = roundCents(base + t.tax);
    }
    return t;
}
