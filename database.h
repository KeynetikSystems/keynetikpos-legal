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
// WHY:  Validating stock inside the same transaction that decrements it keeps
//       the check and the write atomic, so the sale can't be built on a stock
//       count that changed under it. The app is single-threaded today, but WAL
//       + busy_timeout (see initialize()) mean a second connection on the same
//       DB file would also be handled safely. Sale items
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
#include <cmath>

#include "money.h"

struct Product
{
    int id = 0;
    QString name;
    QString category;
    Money price;
    Money costPrice;
    double profitMargin = 0.0;
    int stockQuantity = 0;
    QString barcode;
    bool isActive = true;
    int reorderLevel = 20;   // per-product low-stock threshold (0 = no alerts)

    static Money calculateSellingPrice(Money costPrice, double profitMargin)
    {
        return Money::fromCents(
            std::llround(costPrice.cents() * (1.0 + profitMargin / 100.0)));
    }
};

struct Sale
{
    int id = 0;
    QString saleDate;
    Money subtotal;
    Money tax;
    Money discount;
    Money total;
    QString paymentMethod;
    Money amountPaid;
    Money changeDue;
};

struct SaleItem
{
    int id = 0;
    int saleId = 0;
    int productId = 0;
    QString productName;
    int quantity = 0;
    Money price;
    Money costPrice;
    Money subtotal;
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

    // seedSampleData inserts the demo products + default admin on an empty DB.
    // Tests pass false so they start from a clean, deterministic schema.
    bool initialize(bool seedSampleData = true);
    bool isOpen() const;

    // Test hook: repoint the singleton at an arbitrary SQLite file (default an
    // in-memory DB) on a private connection, so unit tests exercise the real
    // recordSale/processRefund logic without touching the user's AppData file.
    // Must be called before initialize().
    void configureForTesting(const QString &dbPath = QStringLiteral(":memory:"));

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
    bool setReorderLevel(int productId, int level);
    bool decreaseStock(int productId, int quantity);
    bool increaseStock(int productId, int quantity);
    int getStock(int productId);

    // Sales operations
    // Atomically validates stock, inserts the sale + items, and decrements
    // stock. Returns the new sale id, or -1 (see getLastError()).
    int recordSale(const QVector<SaleItem> &items,
                   Money subtotal, Money tax, Money discount, Money total,
                   const QString &paymentMethod,
                   Money amountPaid, Money changeDue);
    QVector<Sale> getAllSales();
    QVector<Sale> getSalesByDateRange(const QString &startDate, const QString &endDate);
    QVector<SaleItem> getSaleItems(int saleId);
    Sale getSaleById(int saleId);

    // Analytics
    Money getTotalSalesToday();
    Money getTotalSalesThisMonth();
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

    // Backup
    // Checkpoints the WAL and copies the live DB file to destDir as
    // pos_database_YYYYMMDD_HHmmss.db. On success, *outPath (if given) gets the
    // backup file path. The default backup directory is <AppData>/backups.
    QString backupDirectory() const;
    bool backupTo(const QString &destDir, QString *outPath = nullptr);
    // Runs at most one backup per calendar day (tracked in QSettings) and prunes
    // the backup directory to the newest `keep` files. Safe to call every launch.
    bool backupIfDue(int keep = 10);
    // Keeps only the newest `keep` pos_database_*.db files in destDir.
    void rotateBackups(const QString &destDir, int keep);

    // Utility
    QString getLastError() const;
    bool executeQuery(const QString &queryStr);

    // Profit calculation
    Money getActualGrossProfit(const QString &startDate, const QString &endDate);
    Money getActualGrossProfitToday();
    Money getActualGrossProfitThisMonth();

private:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase db;
    QString lastError;
    QString m_dbPath;           // full path to pos_database.db (for backups)
    bool initialized = false;   // guards against repeated initialize() calls

    bool createTables();
    bool migrateMoneyToCents();
    bool ensureColumn(const QString &table, const QString &column,
                      const QString &definition);
    bool insertSampleData();
    bool adjustStockWithLog(int productId, int qtyChange, const QString &reason, const QString &adjustedBy);
};

#endif // DATABASE_H