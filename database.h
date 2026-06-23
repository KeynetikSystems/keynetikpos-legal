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
#include <QDate>
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
    QDateTime saleDate;     // when the sale was recorded (DB CURRENT_TIMESTAMP)
    Money subtotal;
    Money tax;
    Money discount;
    Money total;
    QString paymentMethod;
    Money amountPaid;
    Money changeDue;
    // Audit context: who rang the sale up, and the open shift (0 = none).
    // shiftId is plumbed through but only populated once a live shift is wired
    // into the checkout flow; cashier is captured on every sale.
    QString cashier;
    int shiftId = 0;
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

// One tender of a (possibly split) sale: e.g. "Cash" 500.00, "Mobile Money"
// 1249.77 with the M-Pesa code as the reference. Recorded in sale_payments so
// payment-method totals are accurate even when a sale mixes methods.
struct SalePayment
{
    QString method;       // "Cash" | "Card" | "Mobile Money"
    Money   amount;       // net contribution to the sale total
    QString reference;    // M-Pesa code / card ref (optional)
};

// Aggregated per-method takings over a date range (for reporting).
struct PaymentTotal
{
    QString method;
    Money   total;
    int     count = 0;    // number of tenders
};

// ── Purchasing (suppliers / purchase orders) ───────────────────────────────
// First ERP-facing module: the "other half" of inventory. Sales/stock_adjust-
// ments already cover stock leaving the business; Supplier/PurchaseOrder*
// cover stock arriving, with a per-line cost so margins stay accurate.
struct Supplier
{
    int id = 0;
    QString name;
    QString contactPerson;
    QString phone;
    QString email;
    QString address;
    bool isActive = true;
};

struct PurchaseOrderItem
{
    int id = 0;
    int poId = 0;
    int productId = 0;
    QString productName;   // snapshot, same reasoning as SaleItem
    int quantity = 0;
    Money unitCost;
    Money subtotal;
};

struct PurchaseOrder
{
    int id = 0;
    int supplierId = 0;
    QString supplierName;  // joined in for display convenience
    QString status;        // 'Pending' | 'Received' | 'Cancelled'
    QString orderDate;
    QString receivedDate;
    QString notes;
    QString createdBy;
    Money total;
};

// ── Expense tracking ───────────────────────────────────────────────────────
struct ExpenseCategory
{
    int id = 0;
    QString name;
    bool isActive = true;
};

struct Expense
{
    int id = 0;
    int categoryId = 0;
    QString categoryName;   // joined
    Money amount;
    QString description;
    QDate date;             // the day the expense was incurred
    QString recordedBy;
    QString createdAt;
};

// ── Customer accounts ──────────────────────────────────────────────────────
// Loyalty points are integer units (1 point per whole 100 KSh spent).
// Store credit is integer cents, like all money in the app. Both accrue
// inside the same recordSale() transaction so they're always consistent
// with the actual sale record.
struct Customer
{
    int id = 0;
    QString name;
    QString phone;          // used as the quick-lookup key at checkout
    QString email;
    QString address;
    int loyaltyPoints = 0;
    Money storeCredit;
    bool isActive = true;
    QString createdAt;
};

// Everything needed to record one sale. Bundling these into a value struct
// (recordSale used to take 11 positional arguments — two adjacent Money args
// with no labels were easy to transpose) keeps the call sites readable and
// lets the request grow without re-churning every caller. The money fields are
// the computed CartTotals; customerId/storeCreditUsed/payments/cashier/shiftId
// are optional.
struct SaleRequest
{
    QVector<SaleItem>    items;
    Money                subtotal;
    Money                tax;
    Money                discount;
    Money                total;
    QString              paymentMethod;
    Money                amountPaid;
    Money                changeDue;
    int                  customerId = 0;
    Money                storeCreditUsed = Money::fromCents(0);
    QVector<SalePayment> payments;
    QString              cashier;       // username who processed the sale (audit)
    int                  shiftId = 0;   // open shift at sale time, 0 if none
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

    // The app owns a single Database (created in main(), one per process) and
    // injects it where needed; tests own their own. The connection is set up in
    // the constructor; configureForTesting() can repoint it before initialize().
    Database();
    ~Database();

    // Non-copyable: it owns a QSqlDatabase connection handle.
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

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
    // Atomically validates stock, inserts the sale + items, decrements stock,
    // and — when customerId > 0 — awards loyalty points + deducts any applied
    // store credit from the customer row, all in one transaction.
    // Returns the new sale id, or -1 (see getLastError()).
    int recordSale(const SaleRequest &request);
    QVector<Sale> getAllSales();
    QVector<Sale> getSalesByDateRange(const QDate &startDate, const QDate &endDate);
    QVector<SaleItem> getSaleItems(int saleId);
    Sale getSaleById(int saleId);

    // Analytics
    Money getTotalSalesToday();
    Money getTotalSalesThisMonth();
    int getTotalTransactionsToday();
    QVector<QPair<QString, int>> getTopSellingProducts(int limit);
    // Per-method takings from sale_payments over [startDate, endDate] (inclusive,
    // 'yyyy-MM-dd'). Accurate for split tenders; only covers sales recorded after
    // the sale_payments table was added.
    QVector<PaymentTotal> getPaymentTotalsByMethod(const QDate &startDate,
                                                   const QDate &endDate);

    // Stock adjustment & history
    bool logStockAdjustment(int productId, const QString &productName,
                            int oldQty, int newQty,
                            const QString &reason, const QString &adjustedBy);
    QVector<StockAdjustment> getStockHistory(int productId, int limit) const;

    // Refund operations
    bool processRefund(int saleId, const QString &reason, const QString &processedBy);
    bool isRefunded(int saleId) const;

    // Supplier operations
    QVector<Supplier> getAllSuppliers(bool includeInactive = false);
    Supplier getSupplierById(int id);
    bool addSupplier(const Supplier &supplier);
    bool updateSupplier(const Supplier &supplier);
    // Soft delete: suppliers are referenced by historical purchase orders, so
    // they're deactivated (hidden from pickers) rather than removed.
    bool deactivateSupplier(int id);

    // Purchase order operations
    // Inserts the PO header + line items in one transaction; status starts
    // 'Pending'. Returns the new PO id, or -1 (see getLastError()).
    int createPurchaseOrder(int supplierId, const QVector<PurchaseOrderItem> &items,
                            const QString &notes, const QString &createdBy);
    QVector<PurchaseOrder> getAllPurchaseOrders();
    QVector<PurchaseOrder> getPurchaseOrdersBySupplier(int supplierId);
    PurchaseOrder getPurchaseOrderById(int id);
    QVector<PurchaseOrderItem> getPurchaseOrderItems(int poId);
    // Atomically: increases stock for every line, updates each product's
    // cost_price to the PO's unit cost (latest-cost basis), logs a
    // stock_adjustments row per line, and flips the PO to 'Received'. Fails
    // (no-op) if the PO isn't currently 'Pending'.
    bool receivePurchaseOrder(int poId, const QString &receivedBy);
    bool cancelPurchaseOrder(int poId);

    // Expense category operations
    QVector<ExpenseCategory> getAllExpenseCategories(bool includeInactive = false);
    bool addExpenseCategory(const QString &name);
    bool deactivateExpenseCategory(int id);

    // Expense operations
    bool addExpense(const Expense &expense);
    QVector<Expense> getAllExpenses();
    QVector<Expense> getExpensesByDateRange(const QDate &startDate,
                                            const QDate &endDate);
    // Sum of all expenses in the date range (for P&L computation).
    Money getTotalExpenses(const QDate &startDate, const QDate &endDate);

    // Customer operations
    QVector<Customer> getAllCustomers(bool includeInactive = false);
    Customer getCustomerById(int id);
    // Efficient phone lookup — used at checkout for quick customer selection.
    Customer getCustomerByPhone(const QString &phone);
    bool addCustomer(const Customer &customer);
    bool updateCustomer(const Customer &customer);
    bool deactivateCustomer(int id);
    // Explicit store-credit top-up (used from CustomerDialog — not the same
    // as the credit earned during a sale, which goes through recordSale).
    bool adjustStoreCredit(int customerId, Money delta, const QString &reason);
    QVector<Sale> getCustomerPurchaseHistory(int customerId);

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

    // Integrity checks
    // Runs PRAGMA integrity_check on the live DB. Returns true + empty string
    // on success, or false + description of the first error found.
    bool verifyDatabaseIntegrity(QString *errorOut = nullptr);
    // Opens the backup file on a separate connection and runs the same check.
    // Call after backupTo() to confirm the copy is readable.
    bool verifyBackup(const QString &backupPath, QString *errorOut = nullptr);

    // Profit calculation
    Money getActualGrossProfit(const QDate &startDate, const QDate &endDate);
    Money getActualGrossProfitToday();
    Money getActualGrossProfitThisMonth();

    // P&L: revenue, COGS, expenses, net profit for a date range
    struct ProfitLossRow {
        QString date;          // YYYY-MM or YYYY-MM-DD depending on granularity
        Money revenue;
        Money cogs;
        Money expenses;
        Money grossProfit() const { return revenue - cogs; }
        Money netProfit()   const { return revenue - cogs - expenses; }
    };
    QVector<ProfitLossRow> getProfitLossByDateRange(const QDate &start,
                                                    const QDate &end);

    // Stock valuation: current qty * cost_price per product
    struct StockValuationRow {
        QString productName;
        QString category;
        int     qty;
        Money   costPrice;
        Money   value() const { return costPrice * qty; }
    };
    QVector<StockValuationRow> getStockValuation();

    // Loyalty redemption: burn points to store credit (100 pts = 10 KSh = 1000 cents)
    // Returns false if customer has insufficient points. Caller decides the rate.
    bool redeemLoyaltyPoints(int customerId, int pointsToRedeem, Money creditValue);

private:
    QSqlDatabase db;
    QString lastError;
    QString m_dbPath;           // full path to pos_database.db (for backups)
    bool initialized = false;   // guards against repeated initialize() calls

    bool createTables();
    // Versioned schema migrations. schemaVersion()/setSchemaVersion() track the
    // applied version in schema_meta (key 'schema_version'); runMigrations()
    // applies every numbered step newer than the stored version, each in its own
    // transaction, bumping the version as it goes. Steps are written to be
    // idempotent so a fresh DB and an old DB converge to the same shape.
    int  schemaVersion();
    bool setSchemaVersion(int version);
    bool runMigrations();
    bool migrateMoneyToCents();
    bool ensureColumn(const QString &table, const QString &column,
                      const QString &definition);
    bool insertSampleData();
    bool adjustStockWithLog(int productId, int qtyChange, const QString &reason, const QString &adjustedBy);
};

#endif // DATABASE_H