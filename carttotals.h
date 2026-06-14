// =============================================================================
// carttotals.h — CartTotals + computeCartTotals(): the pure money math
// -----------------------------------------------------------------------------
// WHAT: The subtotal/discount/tax/total derivation, split out from the checkout
//       pipeline so it can be reasoned about — and unit-tested — on its own.
// HOW:  A plain value struct plus one free function. It depends only on Money
//       (money.h) and BusinessSettings (settingsmanager.h); it touches no
//       database, UI, or singleton, so the test target links it without
//       dragging in Qt Widgets/Sql.
// WHY:  This is the most correctness-critical arithmetic in the app (tax,
//       discount clamping, inclusive vs. add-on tax). Money is integer cents,
//       so totals are exact and always reconcile; the only rounding is the
//       single tax-rate multiplication. Keeping it in a dependency-free
//       translation unit is what makes carttotals_test.cpp possible.
//       CheckoutService still owns the side-effecting pipeline.
// =============================================================================
#ifndef CARTTOTALS_H
#define CARTTOTALS_H

#include "money.h"             // Money
#include "settingsmanager.h"   // BusinessSettings

// Totals derived from the configurable tax settings. The discount reduces
// the taxable base; with tax-inclusive pricing the tax shown is informational.
struct CartTotals {
    Money subtotal;
    Money discount;
    Money tax;
    Money total;
};

// Derives the totals from a raw subtotal + requested discount. The discount is
// clamped to [0, subtotal] so a total can never go negative. All arithmetic is
// in integer cents, so the stored numbers always reconcile exactly
// (subtotal - discount [+ tax] == total) with no floating-point drift.
CartTotals computeCartTotals(Money subtotal, Money discount,
                             const BusinessSettings &bs);

#endif // CARTTOTALS_H
