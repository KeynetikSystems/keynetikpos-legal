// =============================================================================
// database_test.cpp — Characterization tests for the money path in Database.
// -----------------------------------------------------------------------------
// WHAT: Pins the behaviour of recordSale() and processRefund() — the two
//       methods that move money and stock — so the upcoming integer-cents
//       migration (and any future change) can't silently break them.
// HOW:  QtTest, GUI-less (a QCoreApplication is needed for the SQLite driver).
//       Each test runs against a fresh in-memory database via
//       Database::configureForTesting(), so nothing touches the user's data and
//       the schema is deterministic (no seeded sample products).
// WHY:  These invariants — atomic stock decrement, all-or-nothing rollback on
//       insufficient stock, stock restoration on refund, and the double-refund
//       guard — are the heart of the system's correctness. They were previously
//       untested because Database was a hard singleton bound to AppData.
// =============================================================================
#include <QtTest>
#include <QTemporaryDir>
#include <QDate>

#include "database.h"

class DatabaseTest : public QObject
{
    Q_OBJECT

private:
    Database &dbi() { return Database::instance(); }

    // Adds a product and returns its generated id. costPrice/margin are chosen
    // so the computed selling price equals `price`.
    int addProduct(const QString &barcode, Money price, int stock)
    {
        Product p;
        p.name          = QStringLiteral("Test %1").arg(barcode);
        p.category      = QStringLiteral("Test");
        p.costPrice     = Money::fromCents(price.cents() / 2);
        p.profitMargin  = 100.0;           // selling = cost * 2 = price
        p.stockQuantity = stock;
        p.barcode       = barcode;
        const bool ok = dbi().addProduct(p);
        Q_ASSERT(ok); Q_UNUSED(ok);
        return dbi().getProductByBarcode(barcode).id;
    }

    static SaleItem line(int productId, const QString &name, int qty, Money price)
    {
        SaleItem si;
        si.productId   = productId;
        si.productName = name;
        si.quantity    = qty;
        si.price       = price;
        si.costPrice   = Money::fromCents(price.cents() / 2);
        si.subtotal    = price * qty;
        return si;
    }
    static Money m(qint64 cents) { return Money::fromCents(cents); }

private slots:
    // Fresh in-memory DB for every test — no shared state, no AppData writes.
    void init()
    {
        dbi().configureForTesting();
        QVERIFY(dbi().initialize(/*seedSampleData=*/false));
        QVERIFY(dbi().isOpen());
        // products.category is a FK to categories(name); seed the one we use so
        // addProduct() isn't rejected by the foreign-key constraint.
        QVERIFY(dbi().executeQuery(
            "INSERT OR IGNORE INTO categories (name) VALUES ('Test')"));
    }

    void recordSale_decrementsStockAndPersists()
    {
        const int pid = addProduct("REC1", m(10000), 10);

        QVector<SaleItem> items { line(pid, "Test REC1", 3, m(10000)) };
        const int saleId = dbi().recordSale(items, m(30000), m(0), m(0), m(30000),
                                            "Cash", m(30000), m(0));

        QVERIFY(saleId > 0);
        QCOMPARE(dbi().getStock(pid), 7);                 // 10 - 3
        QCOMPARE(dbi().getSaleById(saleId).id, saleId);
        QCOMPARE(dbi().getSaleItems(saleId).size(), 1);
        QCOMPARE(dbi().getSaleById(saleId).total.cents(), 30000);
    }

    void recordSale_emptyItemsRejected()
    {
        const int saleId = dbi().recordSale({}, m(0), m(0), m(0), m(0), "Cash", m(0), m(0));
        QCOMPARE(saleId, -1);
        QVERIFY(dbi().getAllSales().isEmpty());
    }

    void recordSale_persistsSplitTenders()
    {
        const int pid = addProduct("SPLIT1", m(10000), 10);
        QVector<SaleItem> items { line(pid, "Test SPLIT1", 1, m(10000)) };
        QVector<SalePayment> pays {
            { "Cash",         m(4000), QString() },
            { "Mobile Money", m(6000), "SLJ7X8K2P0" },
        };
        const int saleId = dbi().recordSale(items, m(10000), m(0), m(0), m(10000),
                                            "Cash + Mobile Money", m(10000), m(0),
                                            0, m(0), pays);
        QVERIFY(saleId > 0);

        const QString today = QDate::currentDate().toString("yyyy-MM-dd");
        const auto totals = dbi().getPaymentTotalsByMethod(today, today);

        qint64 cash = -1, mpesa = -1;
        for (const PaymentTotal &t : totals) {
            if (t.method == "Cash")         cash  = t.total.cents();
            if (t.method == "Mobile Money") mpesa = t.total.cents();
        }
        QCOMPARE(cash,  qint64(4000));
        QCOMPARE(mpesa, qint64(6000));
    }

    void recordSale_insufficientStockRollsBackEverything()
    {
        const int p1 = addProduct("OK1", m(5000), 10);
        const int p2 = addProduct("LOW1", m(5000), 1);

        // First line is satisfiable; second oversells. The whole sale must fail
        // atomically: no sale row, and NO stock touched (not even for p1).
        QVector<SaleItem> items {
            line(p1, "Test OK1",  5,  m(5000)),
            line(p2, "Test LOW1", 9999, m(5000)),
        };
        const int saleId = dbi().recordSale(items, m(0), m(0), m(0), m(0), "Cash", m(0), m(0));

        QCOMPARE(saleId, -1);
        QVERIFY(dbi().getAllSales().isEmpty());
        QCOMPARE(dbi().getStock(p1), 10);   // untouched
        QCOMPARE(dbi().getStock(p2), 1);    // untouched
    }

    void processRefund_restoresStockAndMarksRefunded()
    {
        const int pid = addProduct("REF1", m(8000), 10);
        QVector<SaleItem> items { line(pid, "Test REF1", 4, m(8000)) };
        const int saleId = dbi().recordSale(items, m(32000), m(0), m(0), m(32000),
                                            "Cash", m(32000), m(0));
        QVERIFY(saleId > 0);
        QCOMPARE(dbi().getStock(pid), 6);

        QVERIFY(!dbi().isRefunded(saleId));
        QVERIFY(dbi().processRefund(saleId, "customer return", "tester"));
        QVERIFY(dbi().isRefunded(saleId));
        QCOMPARE(dbi().getStock(pid), 10);   // 4 returned to stock
    }

    void processRefund_doubleRefundGuarded()
    {
        const int pid = addProduct("REF2", m(8000), 10);
        QVector<SaleItem> items { line(pid, "Test REF2", 4, m(8000)) };
        const int saleId = dbi().recordSale(items, m(32000), m(0), m(0), m(32000),
                                            "Cash", m(32000), m(0));
        QVERIFY(saleId > 0);

        QVERIFY(dbi().processRefund(saleId, "first", "tester"));
        QCOMPARE(dbi().getStock(pid), 10);

        // A second refund must be refused and must NOT restore stock again.
        QVERIFY(!dbi().processRefund(saleId, "second", "tester"));
        QCOMPARE(dbi().getStock(pid), 10);   // still 10, not 14
    }

    void processRefund_unknownSaleFails()
    {
        QVERIFY(!dbi().processRefund(99999, "n/a", "tester"));
    }

    // The cents migration must run exactly once: PRAGMA user_version has to
    // persist so a reopen does NOT re-scale money (which would both slow every
    // launch and multiply every price by 100). Uses a real file DB so state
    // survives the close/reopen the way a second process launch would.
    void moneyMigration_persistsAcrossReopen()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath("persist.db");

        dbi().configureForTesting(path);
        QVERIFY(dbi().initialize(/*seedSampleData=*/false));
        QVERIFY(dbi().executeQuery(
            "INSERT OR IGNORE INTO categories (name) VALUES ('Test')"));
        const int pid = addProduct("PERSIST1", m(12300), 5);
        QCOMPARE(dbi().getProductByBarcode("PERSIST1").price.cents(), 12300);

        // Reopen the same file, as a fresh launch would.
        dbi().configureForTesting(path);
        QVERIFY(dbi().initialize(/*seedSampleData=*/false));
        QCOMPARE(dbi().getProductById(pid).price.cents(), 12300);  // NOT *100
    }
};

QTEST_GUILESS_MAIN(DatabaseTest)
#include "database_test.moc"
