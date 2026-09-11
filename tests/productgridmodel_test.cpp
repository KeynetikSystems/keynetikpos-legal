// =============================================================================
// productgridmodel_test.cpp — ProductGridModel::syncProducts() diff contract
// -----------------------------------------------------------------------------
// WHAT: syncProducts() reconciles the grid against a fresh DB read without a
//       full model reset — used by MainWindow's 10s poll so a product added
//       or removed from outside this window (the mobile scanner, a direct DB
//       edit) shows up without disturbing an in-progress search/scroll.
// HOW:  QSignalSpy on rowsInserted/rowsRemoved/dataChanged proves each case
//       fires the right (and only the right) signal — no live DB needed,
//       Product structs are built by hand.
// =============================================================================
#include <QtTest>
#include <QSignalSpy>

#include "productgridmodel.h"

namespace {
Product makeProduct(int id, const QString &barcode, Money price, int stock)
{
    Product p;
    p.id = id;
    p.name = QStringLiteral("Product %1").arg(id);
    p.category = QStringLiteral("Test");
    p.price = price;
    p.barcode = barcode;
    p.stockQuantity = stock;
    return p;
}
} // namespace

class ProductGridModelTest : public QObject
{
    Q_OBJECT

private slots:
    void syncProducts_addsNewRows()
    {
        ProductGridModel model;
        model.setProducts({ makeProduct(1, "A", Money::fromCents(100), 5) });

        QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
        model.syncProducts({
            makeProduct(1, "A", Money::fromCents(100), 5),
            makeProduct(2, "B", Money::fromCents(200), 3),
        });

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(insertedSpy.count(), 1);
    }

    void syncProducts_removesVanishedRows()
    {
        ProductGridModel model;
        model.setProducts({
            makeProduct(1, "A", Money::fromCents(100), 5),
            makeProduct(2, "B", Money::fromCents(200), 3),
        });

        QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
        model.syncProducts({ makeProduct(1, "A", Money::fromCents(100), 5) });

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(removedSpy.count(), 1);
        QCOMPARE(model.index(0).data(ProductGridModel::ProductIdRole).toInt(), 1);
    }

    void syncProducts_updatesChangedFieldsInPlace()
    {
        ProductGridModel model;
        model.setProducts({ makeProduct(1, "A", Money::fromCents(100), 5) });

        QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);

        model.syncProducts({ makeProduct(1, "A", Money::fromCents(150), 9) });

        QCOMPARE(model.rowCount(), 1);   // no size change — in-place update
        QCOMPARE(insertedSpy.count(), 0);
        QCOMPARE(removedSpy.count(), 0);
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(model.index(0).data(ProductGridModel::StockRole).toInt(), 9);
    }

    void syncProducts_noOpWhenNothingChanged()
    {
        ProductGridModel model;
        model.setProducts({ makeProduct(1, "A", Money::fromCents(100), 5) });

        QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
        model.syncProducts({ makeProduct(1, "A", Money::fromCents(100), 5) });

        QCOMPARE(changedSpy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(ProductGridModelTest)
#include "productgridmodel_test.moc"
