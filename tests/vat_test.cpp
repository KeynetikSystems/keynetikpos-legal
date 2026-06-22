// =============================================================================
// vat_test.cpp — Tests for the VAT engine and VAT-3 aggregation.
// -----------------------------------------------------------------------------
// Pins the inclusive VAT split (standard / zero / exempt) and the VAT-3 roll-up
// over a small in-memory dataset (sales + received purchases by tax code).
// =============================================================================
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "vat.h"

class VatTest : public QObject
{
    Q_OBJECT

    QSqlDatabase db;

    void exec(const QString &sql) {
        QSqlQuery q(db);
        QVERIFY2(q.exec(sql), qPrintable(q.lastError().text()));
    }

private slots:
    void initTestCase()
    {
        db = QSqlDatabase::addDatabase("QSQLITE", "vattest");
        db.setDatabaseName(":memory:");
        QVERIFY(db.open());

        exec("CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, "
             "tax_code TEXT DEFAULT 'standard', is_active INTEGER DEFAULT 1)");
        exec("CREATE TABLE sales (id INTEGER PRIMARY KEY, sale_date TEXT)");
        exec("CREATE TABLE sale_items (id INTEGER PRIMARY KEY, sale_id INTEGER, "
             "product_id INTEGER, subtotal INTEGER)");
        exec("CREATE TABLE purchase_orders (id INTEGER PRIMARY KEY, status TEXT, "
             "order_date TEXT, received_date TEXT)");
        exec("CREATE TABLE purchase_order_items (id INTEGER PRIMARY KEY, po_id INTEGER, "
             "product_id INTEGER, subtotal INTEGER)");

        exec("INSERT INTO products (id, name, tax_code) VALUES "
             "(1,'Std','standard'), (2,'Zero','zero'), (3,'Exempt','exempt')");

        // Sales in June 2026 (VAT-inclusive line subtotals, cents):
        //   std 11,600 -> net 10,000 + VAT 1,600;  zero 5,000;  exempt 3,000.
        exec("INSERT INTO sales (id, sale_date) VALUES (1,'2026-06-15')");
        exec("INSERT INTO sale_items (sale_id, product_id, subtotal) VALUES "
             "(1,1,1160000), (1,2,500000), (1,3,300000)");

        // A received standard purchase of 5,800 -> net 5,000 + input VAT 800.
        exec("INSERT INTO purchase_orders (id, status, order_date, received_date) "
             "VALUES (1,'Received','2026-06-10','2026-06-12')");
        exec("INSERT INTO purchase_order_items (po_id, product_id, subtotal) "
             "VALUES (1,1,580000)");
    }

    void cleanupTestCase()
    {
        db.close();
        QSqlDatabase::removeDatabase("vattest");
    }

    void split_standardBacksOutVat()
    {
        const VatSplit s = Vat::splitInclusive(Money::fromCents(1160000), TaxCode::Standard, 0.16);
        QCOMPARE(s.vat.cents(), qint64(160000));   // 1,600.00
        QCOMPARE(s.net.cents(), qint64(1000000));  // 10,000.00
    }

    void split_zeroAndExemptCarryNoVat()
    {
        for (TaxCode c : { TaxCode::Zero, TaxCode::Exempt }) {
            const VatSplit s = Vat::splitInclusive(Money::fromCents(500000), c, 0.16);
            QCOMPARE(s.vat.cents(), qint64(0));
            QCOMPARE(s.net.cents(), qint64(500000));
        }
    }

    void vat3_aggregatesByTaxCode()
    {
        Vat vat(db);
        const Vat3 v = vat.computeVat3(QDate(2026, 6, 1), QDate(2026, 6, 30));
        QCOMPARE(v.salesStandardNet.cents(),    qint64(1000000));
        QCOMPARE(v.outputVat.cents(),           qint64(160000));
        QCOMPARE(v.salesZero.cents(),           qint64(500000));
        QCOMPARE(v.salesExempt.cents(),         qint64(300000));
        QCOMPARE(v.purchasesStandardNet.cents(),qint64(500000));
        QCOMPARE(v.inputVat.cents(),            qint64(80000));
        QCOMPARE(v.netPayable().cents(),        qint64(80000));   // 1,600 - 800 = 800
    }

    void vat3_excludesOutOfPeriod()
    {
        Vat vat(db);
        const Vat3 v = vat.computeVat3(QDate(2026, 7, 1), QDate(2026, 7, 31));
        QCOMPARE(v.outputVat.cents(), qint64(0));
        QCOMPARE(v.inputVat.cents(),  qint64(0));
    }
};

QTEST_GUILESS_MAIN(VatTest)
#include "vat_test.moc"
