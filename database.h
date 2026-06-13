// =============================================================================
// database.h — Database singleton + core data structs (Product/Sale/SaleItem)
// -----------------------------------------------------------------------------
// WHAT: The central persistence layer. Wraps the single SQLite connection and
//       provides product CRUD, stock operations, atomic sale recording, sales
//       queries, refunds, stock-adjustment audit logging, and profit analytics.
//       Product = catalog item; Sale = transaction header; SaleItem = line item
//       snapshot (name/price/cost copied at sale time).
// HOW:  Singleton (one connection, one schema bootstrap). The DB file lives in
//       the per-user AppData directory. initialize() enables foreign keys,
//       creates all tables idempotently, and seeds sample data on first run;
//       ensureColumn() adds missing columns so old databases upgrade in place.
//       recordSale() re-validates stock, inserts sale + items, and decrements
//       stock inside ONE transaction — any failure rolls everything back.
// WHY:  Stock validation inside the transaction is what makes concurrent
//       overselling impossible (a UI-side check would be a race). Sale items
//       snapshot name/price/cost because products get renamed/repriced/deleted
//       later, and historical reports must reflect what was actually charged.
//       AppData keeps the DB out of read-only Program Files.
// =============================================================================
#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QPair>
#include <QDateTime>

struct Product
{
    int id = 0;
    QString name;
    QString category;
    double price = 0.0;
    double costPrice = 0.0;
    double profitMargin = 0.0;
    int stockQuantity = 0;
    QString barcode;
    bool isActive = true;

    static double calculateSellingPrice(double costPrice, double profitMargin)
    {
        return costPrice * (1.0 + profitMargin / 100.0);
    }
};

struct Sale
{
    int id = 0;
    QString saleDate;
    double subtotal = 0.0;
    double tax = 0.0;
    double discount = 0.0;
    double total = 0.0;
    QString paymentMethod;
    double amountPaid = 0.0;
    double changeDue = 0.0;
};

struct SaleItem
{
    int id = 0;
    int saleId = 0;
    int productId = 0;
    QString productName;
    int quantity = 0;
    double price = 0.0;
    double costPrice = 0.0;
    double subtotal = 0.0;
};

class Database
{
public:
    struct StockAdjustment
    {
        int id = 0;
        int productId = 0;
        QString productName;
        int changeQty = 0;
        int oldQty = 0;
        int newQty = 0;
        QString reason;
        QString adjustedBy;
        QString adjustedAt;
    };

    static Database& instance();

    bool initialize();
    bool isOpen() const;

    // Product operations
    QVector<Product> getAllProducts();
    QVector<Product> getProductsByCategory(const QString &category);
    Product getProductById(int id);
    Product getProductByBarcode(const QString &barcode);
    bool addProduct(const Product &product);
    bool updateProduct(const Product &product);
    bool deleteProduct(int id);
    QStringList getAllCategories();

    // Inventory operations
    bool updateStock(int productId, int newQuantity);
    bool decreaseStock(int productId, int quantity);
    bool increaseStock(int productId, int quantity);
    int getStock(int productId);

    // Sales operations
    // Atomically validates stock, inserts the sale + items, and decrements
    // stock. Returns the new sale id, or -1 (see getLastError()).
    int recordSale(const QVector<SaleItem> &items,
                   double subtotal, double tax, double discount, double total,
                   const QString &paymentMethod,
                   double amountPaid, double changeDue);
    QVector<Sale> getAllSales();
    QVector<Sale> getSalesByDateRange(const QString &startDate, const QString &endDate);
    QVector<SaleItem> getSaleItems(int saleId);
    Sale getSaleById(int saleId);

    // Analytics
    double getTotalSalesToday();
    double getTotalSalesThisMonth();
    int getTotalTransactionsToday();
    QVector<QPair<QString, int>> getTopSellingProducts(int limit);

    // Stock adjustment & history
    bool logStockAdjustment(int productId, const QString &productName,
                            int oldQty, int newQty,
                            const QString &reason, const QString &adjustedBy);
    QVector<StockAdjustment> getStockHistory(int productId, int limit) const;

    // Refund operations
    bool processRefund(int saleId, const QString &reason, const QString &processedBy);
    bool isRefunded(int saleId) const;

    // Utility
    QString getLastError() const;
    bool executeQuery(const QString &queryStr);

    // Profit calculation
    double getActualGrossProfit(const QString &startDate, const QString &endDate);
    double getActualGrossProfitToday();
    double getActualGrossProfitThisMonth();

private:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase db;
    QString lastError;

    bool createTables();
    bool ensureColumn(const QString &table, const QString &column,
                      const QString &definition);
    bool insertSampleData();
    bool adjustStockWithLog(int productId, int qtyChange, const QString &reason, const QString &adjustedBy);
};

#endif // DATABASE_H