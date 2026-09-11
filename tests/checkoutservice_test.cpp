// =============================================================================
// checkoutservice_test.cpp — the checkout pipeline runs without a UserManager
// -----------------------------------------------------------------------------
// WHAT: Proves CheckoutService::finalizeSale() can be constructed and exercised
//       with an INJECTED operator identity — no logged-in UserManager session.
//       This is the capability the singleton decouple unlocked: before, the
//       service reached into UserManager::instance() for the cashier name and
//       audit log, so it could only run inside a live app session.
// HOW:  GUI-less QtTest over an in-memory Database (configureForTesting). A real
//       InventoryManager is wired in; the printer is null (finalizeSale tolerates
//       it, so the test stays off the QPrinter/PDF path) and the OperatorContext
//       carries a fixed identity plus a recording audit sink.
// WHY:  Pins that the cashier flows from the injected OperatorContext onto the
//       sale's audit field, that stock decrements atomically, and that the audit
//       callback fires — all with zero global session state.
// =============================================================================
#include <QtTest>

#include "database.h"
#include "productrepository.h"
#include "salerepository.h"
#include "checkoutservice.h"
#include "inventorymanager.h"
#include "cart.h"
#include "carttotals.h"

class CheckoutServiceTest : public QObject
{
    Q_OBJECT

private:
    Database m_database;
    Database &dbi() { return m_database; }

    int addProduct(const QString &barcode, Money price, int stock)
    {
        Product p;
        p.name          = QStringLiteral("Test %1").arg(barcode);
        p.category      = QStringLiteral("Test");
        p.costPrice     = Money::fromCents(price.cents() / 2);
        p.profitMargin  = 100.0;
        p.stockQuantity = stock;
        p.barcode       = barcode;
        const bool ok = dbi().products().addProduct(p);
        Q_ASSERT(ok); Q_UNUSED(ok);
        return dbi().products().getProductByBarcode(barcode).id;
    }

private slots:
    void init()
    {
        dbi().configureForTesting();
        QVERIFY(dbi().initialize(/*seedSampleData=*/false));
        QVERIFY(dbi().executeQuery(
            "INSERT OR IGNORE INTO categories (name) VALUES ('Test')"));
    }

    // The headline test: a sale finalizes with an injected operator and no
    // UserManager anywhere in the call.
    void finalizeSale_usesInjectedOperator_noSession()
    {
        const int pid = addProduct("CHK1", Money::fromCents(10000), 10);

        Cart cart(1, "Cart 1");
        cart.items.append(CartItem(pid, "Test CHK1", Money::fromCents(10000),
                                   3, "Test", Money::fromCents(5000)));

        CartTotals t;
        t.subtotal = Money::fromCents(30000);
        t.discount = Money::fromCents(0);
        t.tax      = Money::fromCents(0);
        t.total    = Money::fromCents(30000);

        // Record what the pipeline audit-logs, so we can assert it fired.
        QVector<QPair<QString, QString>> audit;
        OperatorContext op;
        op.username = QStringLiteral("tilluser");
        op.fullName = QStringLiteral("Till User");
        op.log = [&audit](const QString &action, const QString &details) {
            audit.append({ action, details });
        };

        InventoryManager inventory(dbi());
        // Null printer: finalizeSale must tolerate it (keeps this a GUI-less
        // unit test, off the QPrinter path).
        CheckoutService service(dbi(), &inventory, /*printer=*/nullptr,
                                std::move(op));

        const CheckoutResult r = service.finalizeSale(
            cart, t, "Cash", QString(),
            Money::fromCents(30000), Money::fromCents(0));

        QVERIFY2(r.ok, "finalizeSale should succeed");
        QVERIFY(r.saleId > 0);

        // Stock decremented atomically (10 - 3).
        QCOMPARE(dbi().products().getStock(pid), 7);

        // The injected identity — not a UserManager session — is stamped on the
        // sale's audit field.
        QCOMPARE(dbi().sales().getSaleById(r.saleId).cashier,
                 QStringLiteral("tilluser"));

        // The audit sink was invoked for the completed sale.
        bool sawCompleted = false;
        for (const auto &e : audit)
            if (e.first == QStringLiteral("Sale Completed"))
                sawCompleted = true;
        QVERIFY2(sawCompleted, "audit log should record Sale Completed");
    }

    // A failed sale (insufficient stock) leaves stock untouched and reports the
    // error — the operator injection doesn't change the all-or-nothing contract.
    void finalizeSale_insufficientStock_failsAndPreservesStock()
    {
        const int pid = addProduct("CHK2", Money::fromCents(10000), 2);

        Cart cart(1, "Cart 1");
        cart.items.append(CartItem(pid, "Test CHK2", Money::fromCents(10000),
                                   5, "Test", Money::fromCents(5000)));   // want 5, have 2

        CartTotals t;
        t.subtotal = Money::fromCents(50000);
        t.total    = Money::fromCents(50000);

        OperatorContext op;
        op.username = QStringLiteral("tilluser");

        InventoryManager inventory(dbi());
        CheckoutService service(dbi(), &inventory, nullptr, std::move(op));

        const CheckoutResult r = service.finalizeSale(
            cart, t, "Cash", QString(),
            Money::fromCents(50000), Money::fromCents(0));

        QVERIFY(!r.ok);
        QVERIFY(!r.error.isEmpty());
        QCOMPARE(dbi().products().getStock(pid), 2);   // untouched
    }
};

QTEST_GUILESS_MAIN(CheckoutServiceTest)
#include "checkoutservice_test.moc"
