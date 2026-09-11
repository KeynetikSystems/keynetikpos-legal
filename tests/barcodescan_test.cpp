// =============================================================================
// barcodescan_test.cpp — addBarcodeToCart() stock-validation contract
// -----------------------------------------------------------------------------
// WHAT: addBarcodeToCart() is now the single place stock validation happens
//       for a barcode arriving from either the serial scanner
//       (MainWindow::onBarcodeScanned) or the LAN mobile-scanner API
//       (PosApiServer). This pins its three outcomes so a future change to
//       either caller can't silently diverge from the other.
// HOW:  GUI-less QtTest over an in-memory Database (configureForTesting), a
//       real InventoryManager, and a real CartService — mirrors
//       checkoutservice_test.cpp's dependency closure minus ReceiptPrinter
//       (no checkout happens here).
// WHY:  Before the extraction, this rule was duplicated across two functions
//       with a redundant early-exit check; a test on the shared function is
//       what keeps it from splitting apart again.
// =============================================================================
#include <QtTest>

#include "database.h"
#include "productrepository.h"
#include "inventorymanager.h"
#include "cartservice.h"
#include "barcodescan.h"

class BarcodeScanTest : public QObject
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
        p.price         = price;
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

    void addBarcodeToCart_knownBarcodeSufficientStock_addsToCart()
    {
        addProduct("SCAN1", Money::fromCents(15000), 10);

        InventoryManager inventory(dbi());
        CartService cart;

        const BarcodeScanResult result =
            addBarcodeToCart("SCAN1", dbi(), inventory, cart);

        QVERIFY(result.status == BarcodeScanResult::Status::Added);
        QCOMPARE(result.product.barcode, QStringLiteral("SCAN1"));
        QVERIFY(!result.merged);   // first line in an empty cart, not a merge

        QCOMPARE(cart.current()->items.size(), 1);
        QCOMPARE(cart.current()->items.first().quantity, 1);
    }

    void addBarcodeToCart_unknownBarcode_returnsNotFoundAndLeavesCartEmpty()
    {
        InventoryManager inventory(dbi());
        CartService cart;

        const BarcodeScanResult result =
            addBarcodeToCart("NOSUCHCODE", dbi(), inventory, cart);

        QVERIFY(result.status == BarcodeScanResult::Status::NotFound);
        QCOMPARE(cart.current()->items.size(), 0);
    }

    void addBarcodeToCart_outOfStock_returnsOutOfStockAndLeavesCartEmpty()
    {
        addProduct("SCAN2", Money::fromCents(15000), 0);

        InventoryManager inventory(dbi());
        CartService cart;

        const BarcodeScanResult result =
            addBarcodeToCart("SCAN2", dbi(), inventory, cart);

        QVERIFY(result.status == BarcodeScanResult::Status::OutOfStock);
        QCOMPARE(result.product.barcode, QStringLiteral("SCAN2"));
        QCOMPARE(cart.current()->items.size(), 0);
    }
};

QTEST_GUILESS_MAIN(BarcodeScanTest)
#include "barcodescan_test.moc"
