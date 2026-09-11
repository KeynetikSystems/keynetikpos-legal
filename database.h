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
#include <memory>

#include "money.h"

// Data-access repositories. Forward-declared here (the accessors return them by
// reference and the members are unique_ptr); database.cpp includes the full
// definitions. Each repository's own header includes this one for Product/Sale/
// etc., so we must NOT include them here — that would be a circular include.
class ProductRepository;
class SaleRepository;
class SalesAnalyticsRepository;
class RefundRepository;
class SupplierRepository;
class PurchaseOrderRepository;
class ExpenseRepository;
class CustomerRepository;

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

    // ── Data-access repositories ────────────────────────────────────────────
    // All domain reads/writes go through one of these focused repositories
    // (catalog/stock, sales, analytics, refunds, suppliers, POs, expenses,
    // customers). Database owns the single connection and hands out a long-lived
    // repository per group; each repository carries its own lastError(), so a
    // mutation and the error you read after it must use the SAME accessor:
    //
    //     if (!db.customers().addCustomer(c))
    //         show(db.customers().lastError());   // same instance, error intact
    //
    // The accessors return references to Database-owned instances (constructed
    // lazily on first use, after the connection is open), never temporaries.
    ProductRepository         &products();
    SaleRepository            &sales();
    SalesAnalyticsRepository  &salesAnalytics();
    RefundRepository          &refunds();
    SupplierRepository        &suppliers();
    PurchaseOrderRepository   &purchaseOrders();
    ExpenseRepository         &expenses();
    CustomerRepository        &customers();

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

    // Shared row types returned by the analytics/inventory repositories. They
    // live here (rather than in a repository header) because more than one
    // repository and several dialogs refer to them as Database::ProfitLossRow /
    // Database::StockValuationRow.

    // P&L: revenue, COGS, expenses, net profit for a date range
    struct ProfitLossRow {
        QString date;          // YYYY-MM or YYYY-MM-DD depending on granularity
        Money revenue;
        Money cogs;
        Money expenses;
        Money grossProfit() const { return revenue - cogs; }
        Money netProfit()   const { return revenue - cogs - expenses; }
    };

    // Stock valuation: current qty * cost_price per product
    struct StockValuationRow {
        QString productName;
        QString category;
        int     qty;
        Money   costPrice;
        Money   value() const { return costPrice * qty; }
    };

private:
    QSqlDatabase db;
    QString lastError;
    QString m_dbPath;           // full path to pos_database.db (for backups)
    bool initialized = false;   // guards against repeated initialize() calls

    // Lazily-constructed, Database-owned repository instances handed out by the
    // accessors above. Constructed on first access (after the connection is
    // open) so they capture the live connection — including the one swapped in
    // by configureForTesting(). unique_ptr over forward-declared types; the
    // out-of-line ~Database() in database.cpp sees the complete types.
    std::unique_ptr<ProductRepository>        m_products;
    std::unique_ptr<SaleRepository>           m_sales;
    std::unique_ptr<SalesAnalyticsRepository> m_salesAnalytics;
    std::unique_ptr<RefundRepository>         m_refunds;
    std::unique_ptr<SupplierRepository>       m_suppliers;
    std::unique_ptr<PurchaseOrderRepository>  m_purchaseOrders;
    std::unique_ptr<ExpenseRepository>        m_expenses;
    std::unique_ptr<CustomerRepository>       m_customers;

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

    // Releases all lazily-cached repositories so they're rebuilt against the
    // current connection on next access. Called when the connection is swapped
    // (configureForTesting); each repository caches a copy of the handle.
    void resetRepositories();
};

#endif // DATABASE_H