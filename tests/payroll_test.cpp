// =============================================================================
// payroll_test.cpp — Unit tests for the Kenya statutory-deduction engine.
// -----------------------------------------------------------------------------
// Pins computePayslip() against hand-worked figures (post-2024 treatment:
// NSSF/SHIF/Housing deductible before PAYE) and the core invariants. If KRA
// rates change, update PayrollRates defaults AND these expectations together.
// =============================================================================
#include <QtTest>

#include "payroll.h"

class PayrollTest : public QObject
{
    Q_OBJECT
    PayrollRates r;   // defaults

private slots:
    void midEarner_exactBreakdown()
    {
        // Gross 50,000.00. Hand-worked:
        //   NSSF    = 6% of 36,000 cap            = 2,160.00
        //   SHIF    = 2.75% of 50,000             = 1,375.00
        //   Housing = 1.5% of 50,000              =   750.00
        //   taxable = 50,000 - 2,160 - 1,375 -750 = 45,715.00
        //   PAYE    = 8,497.85 - 2,400 relief     = 6,097.85
        //   net                                   = 39,617.15
        const Payslip p = Payroll::computePayslip(Money::fromCents(5000000), r);
        QCOMPARE(p.nssf.cents(),    qint64(216000));
        QCOMPARE(p.shif.cents(),    qint64(137500));
        QCOMPARE(p.housing.cents(), qint64(75000));
        QCOMPARE(p.taxable.cents(), qint64(4571500));
        QCOMPARE(p.paye.cents(),    qint64(609785));
        QCOMPARE(p.net.cents(),     qint64(3961715));
    }

    void deductionsAlwaysReconcile()
    {
        for (qint64 gross : { 1500000LL, 5000000LL, 12345678LL, 20000000LL }) {
            const Payslip p = Payroll::computePayslip(Money::fromCents(gross), r);
            const qint64 sum = p.nssf.cents() + p.shif.cents() + p.housing.cents()
                             + p.paye.cents() + p.net.cents();
            QCOMPARE(sum, gross);   // gross fully accounted for
        }
    }

    void nssfIsCapped()
    {
        // Above the 36,000 pensionable cap, NSSF stays at 6% of 36,000 = 2,160.
        const Payslip p = Payroll::computePayslip(Money::fromCents(20000000), r);
        QCOMPARE(p.nssf.cents(), qint64(216000));
    }

    void shifHasMinimum()
    {
        // 2.75% of 5,000 = 137.50, below the 300 floor -> SHIF = 300.00.
        const Payslip p = Payroll::computePayslip(Money::fromCents(500000), r);
        QCOMPARE(p.shif.cents(), qint64(30000));
    }

    void lowEarnerPaysNoPaye()
    {
        // Gross 10,000: tax before relief < 2,400 relief, so PAYE floors at 0.
        const Payslip p = Payroll::computePayslip(Money::fromCents(1000000), r);
        QCOMPARE(p.paye.cents(), qint64(0));
        QVERIFY(p.net.cents() > 0);
    }
};

QTEST_APPLESS_MAIN(PayrollTest)
#include "payroll_test.moc"
