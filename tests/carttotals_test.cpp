// =============================================================================
// carttotals_test.cpp — Unit tests for computeCartTotals() and the Money type.
// -----------------------------------------------------------------------------
// WHAT: Pins the most correctness-critical arithmetic in the app — discount
//       clamping, add-on vs. tax-inclusive VAT, and the invariant that the
//       stored components always reconcile to the total — now that money is
//       held as exact integer minor units (Money) rather than double.
// HOW:  QtTest, applet-less (no event loop needed for pure functions). Links
//       only carttotals.cpp, so it builds and runs in well under a second.
// WHY:  Money math is exactly the logic that must not regress silently.
// =============================================================================
#include <QtTest>

#include "carttotals.h"
#include "money.h"           // Money, formatMoney(), setCurrencySymbol()

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
    static Money c(qint64 cents) { return Money::fromCents(cents); }

private slots:
    void noTax_simple()
    {
        const CartTotals t = computeCartTotals(c(10000), c(0), noTax());
        QCOMPARE(t.subtotal.cents(), 10000);
        QCOMPARE(t.discount.cents(), 0);
        QCOMPARE(t.tax.cents(),      0);
        QCOMPARE(t.total.cents(),    10000);
    }

    void noTax_withDiscount()
    {
        const CartTotals t = computeCartTotals(c(10000), c(1500), noTax());
        QCOMPARE(t.discount.cents(), 1500);
        QCOMPARE(t.total.cents(),    8500);
    }

    void discount_clampedToSubtotal()
    {
        // A discount bigger than the subtotal must not produce a negative total.
        const CartTotals t = computeCartTotals(c(5000), c(8000), noTax());
        QCOMPARE(t.discount.cents(), 5000);
        QCOMPARE(t.total.cents(),    0);
    }

    void discount_negativeClampedToZero()
    {
        const CartTotals t = computeCartTotals(c(5000), c(-1000), noTax());
        QCOMPARE(t.discount.cents(), 0);
        QCOMPARE(t.total.cents(),    5000);
    }

    void taxEnabledButZeroRate_noTax()
    {
        const CartTotals t = computeCartTotals(c(10000), c(0), exclusiveTax(0.0));
        QCOMPARE(t.tax.cents(),   0);
        QCOMPARE(t.total.cents(), 10000);
    }

    void exclusiveTax_addsOnTop()
    {
        // 16% VAT added to a 100.00 base.
        const CartTotals t = computeCartTotals(c(10000), c(0), exclusiveTax(0.16));
        QCOMPARE(t.subtotal.cents(), 10000);
        QCOMPARE(t.tax.cents(),      1600);
        QCOMPARE(t.total.cents(),    11600);
    }

    void exclusiveTax_withDiscountUsesNetBase()
    {
        // Discount reduces the taxable base: (200 - 50) * 16% = 24.
        const CartTotals t = computeCartTotals(c(20000), c(5000), exclusiveTax(0.16));
        QCOMPARE(t.discount.cents(), 5000);
        QCOMPARE(t.tax.cents(),      2400);
        QCOMPARE(t.total.cents(),    17400);
    }

    void inclusiveTax_backCalculates()
    {
        // 116.00 already includes 16% VAT -> tax component is 16.00, total stays.
        const CartTotals t = computeCartTotals(c(11600), c(0), inclusiveTax(0.16));
        QCOMPARE(t.total.cents(), 11600);
        QCOMPARE(t.tax.cents(),   1600);
    }

    void exclusiveTax_componentsReconcileExactly()
    {
        // The core invariant: subtotal - discount + tax == total, EXACTLY.
        // With integer cents there is no half-cent drift to tolerate.
        const CartTotals t = computeCartTotals(c(1999), c(250), exclusiveTax(0.16));
        QCOMPARE((t.subtotal - t.discount + t.tax).cents(), t.total.cents());
    }

    void money_arithmeticIsExact()
    {
        // The classic float trap (0.1 + 0.2 != 0.3) cannot happen with Money.
        const Money sum = Money::fromCents(10) + Money::fromCents(20);
        QCOMPARE(sum.cents(), 30);
        QCOMPARE((Money::fromCents(199) * 3).cents(), 597);
    }

    void money_fromMajorRoundsAtParseBoundary()
    {
        // fromMajor() is the ONE lossy edge (e.g. a spin box value). It rounds
        // to the nearest cent; thereafter arithmetic is exact.
        QCOMPARE(Money::fromMajor(19.99).cents(), 1999);
        QCOMPARE(Money::fromMajor(0.1).cents(),   10);
        QCOMPARE(Money::fromMajor(2.555).cents(), 256);   // rounds half up
    }

    void formatMoney_usesConfiguredSymbol()
    {
        setCurrencySymbol("KSh");
        QCOMPARE(formatMoney(Money::fromCents(123450)), QStringLiteral("KSh 1234.50"));

        setCurrencySymbol("$");
        QCOMPARE(formatMoney(Money::fromCents(990)), QStringLiteral("$ 9.90"));

        // Empty/whitespace symbol falls back to the KSh default.
        setCurrencySymbol("   ");
        QCOMPARE(formatMoney(Money::fromCents(0)), QStringLiteral("KSh 0.00"));
    }
};

QTEST_APPLESS_MAIN(CartTotalsTest)
#include "carttotals_test.moc"
