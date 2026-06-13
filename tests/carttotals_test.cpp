// =============================================================================
// carttotals_test.cpp — Unit tests for computeCartTotals() and the money
// helpers (roundCents/formatMoney).
// -----------------------------------------------------------------------------
// WHAT: Pins the most correctness-critical arithmetic in the app — discount
//       clamping, add-on vs. tax-inclusive VAT, cent rounding, and the
//       invariant that the stored components always reconcile to the total.
// HOW:  QtTest, applet-less (no event loop needed for pure functions). Links
//       only carttotals.cpp, so it builds and runs in well under a second.
// WHY:  Money math is exactly the logic that must not regress silently; before
//       this there were no tests guarding it.
// =============================================================================
#include <QtTest>

#include "carttotals.h"
#include "cart.h"            // roundCents(), formatMoney(), setCurrencySymbol()

class CartTotalsTest : public QObject
{
    Q_OBJECT

private:
    static BusinessSettings noTax()
    {
        BusinessSettings bs;
        bs.taxEnabled = false;
        bs.taxRate    = 0.0;
        return bs;
    }
    static BusinessSettings exclusiveTax(double rate)
    {
        BusinessSettings bs;
        bs.taxEnabled   = true;
        bs.taxInclusive = false;
        bs.taxRate      = rate;
        return bs;
    }
    static BusinessSettings inclusiveTax(double rate)
    {
        BusinessSettings bs;
        bs.taxEnabled   = true;
        bs.taxInclusive = true;
        bs.taxRate      = rate;
        return bs;
    }

private slots:
    void noTax_simple()
    {
        const CartTotals t = computeCartTotals(100.0, 0.0, noTax());
        QCOMPARE(t.subtotal, 100.0);
        QCOMPARE(t.discount, 0.0);
        QCOMPARE(t.tax,      0.0);
        QCOMPARE(t.total,    100.0);
    }

    void noTax_withDiscount()
    {
        const CartTotals t = computeCartTotals(100.0, 15.0, noTax());
        QCOMPARE(t.discount, 15.0);
        QCOMPARE(t.total,    85.0);
    }

    void discount_clampedToSubtotal()
    {
        // A discount bigger than the subtotal must not produce a negative total.
        const CartTotals t = computeCartTotals(50.0, 80.0, noTax());
        QCOMPARE(t.discount, 50.0);
        QCOMPARE(t.total,    0.0);
    }

    void discount_negativeClampedToZero()
    {
        const CartTotals t = computeCartTotals(50.0, -10.0, noTax());
        QCOMPARE(t.discount, 0.0);
        QCOMPARE(t.total,    50.0);
    }

    void taxEnabledButZeroRate_noTax()
    {
        const CartTotals t = computeCartTotals(100.0, 0.0, exclusiveTax(0.0));
        QCOMPARE(t.tax,   0.0);
        QCOMPARE(t.total, 100.0);
    }

    void exclusiveTax_addsOnTop()
    {
        // 16% VAT added to a 100.00 base.
        const CartTotals t = computeCartTotals(100.0, 0.0, exclusiveTax(0.16));
        QCOMPARE(t.subtotal, 100.0);
        QCOMPARE(t.tax,      16.0);
        QCOMPARE(t.total,    116.0);
    }

    void exclusiveTax_withDiscountUsesNetBase()
    {
        // Discount reduces the taxable base: (200 - 50) * 16% = 24.
        const CartTotals t = computeCartTotals(200.0, 50.0, exclusiveTax(0.16));
        QCOMPARE(t.discount, 50.0);
        QCOMPARE(t.tax,      24.0);
        QCOMPARE(t.total,    174.0);
    }

    void inclusiveTax_backCalculates()
    {
        // 116.00 already includes 16% VAT -> tax component is 16.00, total stays.
        const CartTotals t = computeCartTotals(116.0, 0.0, inclusiveTax(0.16));
        QCOMPARE(t.total, 116.0);
        QCOMPARE(t.tax,   16.0);
    }

    void roundsToCents()
    {
        // 33.333 * 3 style input: components must be pinned to 2 decimals.
        const CartTotals t = computeCartTotals(10.005, 0.0, noTax());
        QCOMPARE(t.subtotal, 10.01);   // rounds half up at the cent
        QCOMPARE(t.total,    10.01);
    }

    void exclusiveTax_componentsReconcile()
    {
        // The core invariant: subtotal - discount + tax == total exactly,
        // with no half-cent drift between displayed and summed values.
        const CartTotals t = computeCartTotals(19.99, 2.50, exclusiveTax(0.16));
        const double recomputed = roundCents(t.subtotal - t.discount + t.tax);
        QCOMPARE(t.total, recomputed);
    }

    void roundCents_basic()
    {
        QCOMPARE(roundCents(1.004), 1.00);
        QCOMPARE(roundCents(2.0),   2.00);
        QCOMPARE(roundCents(2.555), 2.56);
    }

    void roundCents_halfCentArtifactIsDocumented()
    {
        // CHARACTERIZATION TEST — pins a known limitation, not desired behavior.
        // Money is held as double, so exact-half values like 1.005 cannot be
        // represented: 1.005 is stored as 1.00499999..., and std::round() floors
        // it to 1.00 instead of the arithmetically-correct 1.01. (Compare 10.005,
        // whose representation lands just above, so it DOES round to 10.01.)
        // This asymmetry is precisely why the proper fix is to store money as
        // integer minor units (cents); see the follow-up noted in carttotals.h.
        QCOMPARE(roundCents(1.005),  1.00);   // surprising-but-current
        QCOMPARE(roundCents(10.005), 10.01);
    }

    void formatMoney_usesConfiguredSymbol()
    {
        setCurrencySymbol("KSh");
        QCOMPARE(formatMoney(1234.5), QStringLiteral("KSh 1234.50"));

        setCurrencySymbol("$");
        QCOMPARE(formatMoney(9.9), QStringLiteral("$ 9.90"));

        // Empty/whitespace symbol falls back to the KSh default.
        setCurrencySymbol("   ");
        QCOMPARE(formatMoney(0.0), QStringLiteral("KSh 0.00"));
    }
};

QTEST_APPLESS_MAIN(CartTotalsTest)
#include "carttotals_test.moc"
