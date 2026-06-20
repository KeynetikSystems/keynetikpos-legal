// =============================================================================
// ledger_test.cpp — Characterization tests for the double-entry Ledger.
// -----------------------------------------------------------------------------
// Pins the accounting invariants: the chart of accounts seeds, unbalanced
// entries are rejected, a balanced entry moves the right account balances, and
// a trial balance always nets (total debits == total credits).
// =============================================================================
#include <QtTest>
#include <QSqlDatabase>

#include "ledger.h"

class LedgerTest : public QObject
{
    Q_OBJECT

    QSqlDatabase m_db;

    Ledger ledger() { return Ledger(m_db); }
    static QDate today() { return QDate::currentDate(); }

private slots:
    void init()
    {
        const QString conn = "ledger_test";
        if (m_db.isOpen()) m_db.close();
        m_db = QSqlDatabase();
        if (QSqlDatabase::contains(conn)) QSqlDatabase::removeDatabase(conn);
        m_db = QSqlDatabase::addDatabase("QSQLITE", conn);
        m_db.setDatabaseName(":memory:");
        QVERIFY(m_db.open());
        Ledger l(m_db);
        QVERIFY(l.initSchema());
    }

    void schema_seedsChartOfAccounts()
    {
        Ledger l = ledger();
        QVERIFY(l.accounts().size() >= 15);
        QVERIFY(l.accountExists("1000"));   // Cash
        QVERIFY(l.accountExists("4000"));   // Sales Revenue
        QVERIFY(!l.accountExists("9999"));
    }

    void postEntry_rejectsUnbalanced()
    {
        Ledger l = ledger();
        const int id = l.postEntry(today(), "bad", "manual", {
            { "1000", Money::fromCents(10000), Money(), {} },
            { "4000", Money(), Money::fromCents(9000), {} },   // 90.00 != 100.00
        });
        QCOMPARE(id, -1);
        QVERIFY(l.trialBalance(today(), today()).isEmpty());   // nothing posted
    }

    void postEntry_rejectsSingleLine()
    {
        Ledger l = ledger();
        QCOMPARE(l.postEntry(today(), "x", "manual",
                             { { "1000", Money::fromCents(100), Money(), {} } }), -1);
    }

    void postEntry_balancedMovesBalances()
    {
        Ledger l = ledger();
        // A KSh 100.00 cash sale: Dr Cash 100, Cr Sales 100.
        const int id = l.postEntry(today(), "cash sale", "sale", {
            { "1000", Money::fromCents(10000), Money(), {} },
            { "4000", Money(), Money::fromCents(10000), {} },
        });
        QVERIFY(id > 0);
        QCOMPARE(l.accountBalance("1000", today(), today()).cents(), 10000);  // asset +dr
        QCOMPARE(l.accountBalance("4000", today(), today()).cents(), 10000);  // income +cr
    }

    void trialBalance_alwaysBalances()
    {
        Ledger l = ledger();
        l.postEntry(today(), "sale", "sale", {
            { "1010", Money::fromCents(23200), Money(), {} },   // M-Pesa 232.00
            { "4000", Money(), Money::fromCents(20000), {} },   // Sales 200.00
            { "2100", Money(), Money::fromCents(3200),  {} },   // VAT Output 32.00
        });
        l.postEntry(today(), "rent", "manual", {
            { "6100", Money::fromCents(50000), Money(), {} },   // Rent 500.00
            { "1020", Money(), Money::fromCents(50000), {} },   // Bank 500.00
        });

        qint64 dr = 0, cr = 0;
        for (const TrialRow &r : l.trialBalance(today(), today())) {
            dr += r.debit.cents();
            cr += r.credit.cents();
        }
        QCOMPARE(dr, cr);          // the books balance
        QVERIFY(dr > 0);
    }
};

QTEST_GUILESS_MAIN(LedgerTest)
#include "ledger_test.moc"
