// =============================================================================
// salejournal_test.cpp — Tests for buildSaleJournal() (sale -> GL lines).
// -----------------------------------------------------------------------------
// Verifies the core invariant (every sale entry balances) and the account
// mapping for cash, split tenders, COGS, no-tax, and store-credit/on-account.
// =============================================================================
#include <QtTest>

#include "salejournal.h"

class SaleJournalTest : public QObject
{
    Q_OBJECT

    static qint64 dr(const QVector<GLLine> &ls, const QString &acct) {
        qint64 s = 0; for (const GLLine &l : ls) if (l.account == acct) s += l.debit.cents(); return s;
    }
    static qint64 cr(const QVector<GLLine> &ls, const QString &acct) {
        qint64 s = 0; for (const GLLine &l : ls) if (l.account == acct) s += l.credit.cents(); return s;
    }
    static void assertBalanced(const QVector<GLLine> &ls) {
        qint64 d = 0, c = 0;
        for (const GLLine &l : ls) { d += l.debit.cents(); c += l.credit.cents(); }
        QCOMPARE(d, c);
        QVERIFY(d > 0);
    }

private slots:
    void cashSale_balances()
    {
        const auto ls = buildSaleJournal(Money::fromCents(1160000), Money::fromCents(160000),
                                         { { "Cash", Money::fromCents(1160000) } },
                                         Money(), Money());
        assertBalanced(ls);
        QCOMPARE(dr(ls, "1000"), qint64(1160000));   // cash in
        QCOMPARE(cr(ls, "4000"), qint64(1000000));   // net revenue
        QCOMPARE(cr(ls, "2100"), qint64(160000));    // output VAT
        QCOMPARE(dr(ls, "1100"), qint64(0));         // nothing on account
    }

    void splitTenderWithCogs_balances()
    {
        const auto ls = buildSaleJournal(
            Money::fromCents(1160000), Money::fromCents(160000),
            { { "Cash", Money::fromCents(500000) }, { "M-Pesa", Money::fromCents(660000) } },
            Money(), Money::fromCents(700000));
        assertBalanced(ls);
        QCOMPARE(dr(ls, "1000"), qint64(500000));
        QCOMPARE(dr(ls, "1010"), qint64(660000));
        QCOMPARE(dr(ls, "5000"), qint64(700000));    // COGS
        QCOMPARE(cr(ls, "1200"), qint64(700000));    // inventory out
    }

    void noTax_omitsVatLine()
    {
        const auto ls = buildSaleJournal(Money::fromCents(1000000), Money(),
                                         { { "Cash", Money::fromCents(1000000) } },
                                         Money(), Money());
        assertBalanced(ls);
        QCOMPARE(cr(ls, "2100"), qint64(0));         // no VAT line
        QCOMPARE(cr(ls, "4000"), qint64(1000000));
    }

    void storeCredit_bookedToReceivable()
    {
        const auto ls = buildSaleJournal(
            Money::fromCents(1160000), Money::fromCents(160000),
            { { "Cash", Money::fromCents(160000) } },
            /*storeCreditUsed=*/Money::fromCents(1000000), Money());
        assertBalanced(ls);
        QCOMPARE(dr(ls, "1000"), qint64(160000));
        QCOMPARE(dr(ls, "1100"), qint64(1000000));   // settled via store credit
    }

    void tenderAccountMapping()
    {
        QCOMPARE(saleTenderAccount("Cash"),          QString("1000"));
        QCOMPARE(saleTenderAccount("M-Pesa"),        QString("1010"));
        QCOMPARE(saleTenderAccount("Mobile Money"),  QString("1010"));
        QCOMPARE(saleTenderAccount("Card"),          QString("1020"));
        QCOMPARE(saleTenderAccount("Visa"),          QString("1020"));
        QCOMPARE(saleTenderAccount("Bank Transfer"), QString("1020"));
        QCOMPARE(saleTenderAccount("Gift Voucher"),  QString("1000"));  // fallback
    }
};

QTEST_APPLESS_MAIN(SaleJournalTest)
#include "salejournal_test.moc"
