// =============================================================================
// productrepository.h — ProductRepository: catalog + inventory, split out of
// Database
// -----------------------------------------------------------------------------
// WHAT: Owns the products and stock_adjustments tables: product CRUD (soft
//       delete), category list, barcode/id lookup, the raw stock mutators
//       (update/increase/decrease/getStock/setReorderLevel), the audited
//       adjustStockWithLog(), the stock-adjustment history, and the stock
//       valuation report. Product/StockAdjustment/StockValuationRow live in
//       database.h.
// HOW:  Plain class over a QSqlDatabase held BY VALUE. The raw stock mutators
//       (increaseStock/decreaseStock/updateStock/logStockAdjustment) do NOT
//       open their own transactions, so callers in other repositories
//       (PurchaseOrderRepository::receivePurchaseOrder,
//       RefundRepository::processRefund) can invoke them inside their own
//       transaction on the same shared connection. adjustStockWithLog() is the
//       one that wraps a manual change + its audit row in a transaction.
// WHY:  Continues the Database "god object" breakup. Database keeps the public
//       methods and delegates here.
// =============================================================================
#ifndef PRODUCTREPOSITORY_H
#define PRODUCTREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

#include "database.h"   // Product, Database::StockAdjustment, Database::StockValuationRow

class ProductRepository
{
public:
    explicit ProductRepository(QSqlDatabase db);

    // Catalog
    QVector<Product> getAllProducts();
    QVector<Product> getProductsByCategory(const QString &category);
    Product getProductById(int id);
    Product getProductByBarcode(const QString &barcode);
    bool addProduct(const Product &product);
    bool updateProduct(const Product &product);
    bool deleteProduct(int id);
    QStringList getAllCategories();
    // Registers a category if it isn't already known — call before
    // addProduct()/updateProduct() with a category that might be new.
    // products.category is FK-enforced (see database.cpp), so an insert or
    // update naming an unregistered category fails outright otherwise; this
    // is what actually lets a clerk (till or phone) introduce a category on
    // the fly rather than being limited to the install-time seed list.
    bool ensureCategory(const QString &name);

    // Stock
    bool updateStock(int productId, int newQuantity);
    bool setReorderLevel(int productId, int level);
    bool decreaseStock(int productId, int quantity);
    bool increaseStock(int productId, int quantity);
    int getStock(int productId);

    // Audited stock change (manual): wraps the stock update + audit row in one
    // transaction.
    bool adjustStockWithLog(int productId, int qtyChange,
                            const QString &reason, const QString &adjustedBy);
    bool logStockAdjustment(int productId, const QString &productName,
                            int oldQty, int newQty,
                            const QString &reason, const QString &adjustedBy);
    QVector<Database::StockAdjustment> getStockHistory(int productId, int limit) const;

    // Current qty * cost_price per active product.
    QVector<Database::StockValuationRow> getStockValuation();

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    QString         m_lastError;
};

#endif // PRODUCTREPOSITORY_H
