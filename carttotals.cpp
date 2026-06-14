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

#include <QtGlobal>      // qBound
#include <cmath>         // std::llround

CartTotals computeCartTotals(Money subtotal, Money discount,
                             const BusinessSettings &bs)
{
    CartTotals t;
    t.subtotal = subtotal;
    t.discount = qBound(Money::fromCents(0), discount, t.subtotal);
    const Money base = t.subtotal - t.discount;

    if (!bs.taxEnabled || bs.taxRate <= 0.0) {
        t.tax   = Money::fromCents(0);
        t.total = base;
    } else if (bs.taxInclusive) {
        // base already includes tax: tax = base - base/(1+rate), rounded once.
        const qint64 net = std::llround(base.cents() / (1.0 + bs.taxRate));
        t.tax   = base - Money::fromCents(net);
        t.total = base;
    } else {
        t.tax   = Money::fromCents(std::llround(base.cents() * bs.taxRate));
        t.total = base + t.tax;
    }
    return t;
}
