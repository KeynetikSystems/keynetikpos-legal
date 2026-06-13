// =============================================================================
// carttotals.h — CartTotals + computeCartTotals(): the pure money math
// -----------------------------------------------------------------------------
// WHAT: The subtotal/discount/tax/total derivation, split out from the checkout
//       pipeline so it can be reasoned about — and unit-tested — on its own.
// HOW:  A plain value struct plus one free function. It depends only on
//       roundCents() (cart.h) and BusinessSettings (settingsmanager.h); it
//       touches no database, UI, or singleton, so the test target links it
//       without dragging in Qt Widgets/Sql.
// WHY:  This is the most correctness-critical arithmetic in the app (tax,
//       discount clamping, inclusive vs. add-on tax, cent rounding). Keeping it
//       in a dependency-free translation unit is what makes carttotals_test.cpp
//       possible. CheckoutService still owns the side-effecting pipeline.
//
// FOLLOW-UP: money is held as double, which cannot exactly represent half-cent
//       values (roundCents(1.005) -> 1.00, not 1.01 — see the characterization
//       test in carttotals_test.cpp). The real fix is to store money as integer
//       minor units (cents) end-to-end; that is a deliberate, schema-level
//       migration deferred from this contained-hardening pass.
// =============================================================================
#ifndef CARTTOTALS_H
#define CARTTOTALS_H

#include "settingsmanager.h"   // BusinessSettings

// Totals derived from the configurable tax settings. The discount reduces
// the taxable base; with tax-inclusive pricing the tax shown is informational.
struct CartTotals {
    double subtotal = 0.0;
    double discount = 0.0;
    double tax      = 0.0;
    double total    = 0.0;
};

// Derives the totals from a raw subtotal + requested discount. The discount is
// clamped to [0, subtotal] so a total can never go negative. Every component is
// rounded to cents, and total is built from the rounded parts so the stored
// numbers always reconcile (subtotal - discount [+ tax] == total).
CartTotals computeCartTotals(double subtotal, double discount,
                             const BusinessSettings &bs);

#endif // CARTTOTALS_H
