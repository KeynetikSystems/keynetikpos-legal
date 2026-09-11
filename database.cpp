// =============================================================================
// database.cpp — Implementation of the Database singleton (see database.h for
// the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - The constructor resolves a per-user AppData directory and opens
//    pos_database.db there; initialize() enables PRAGMA foreign_keys (off by
//    default per SQLite connection), creates tables idempotently, and seeds
//    sample products only when the products table is empty.
//  - This file is now connection lifecycle + schema bootstrap/migrations +
//    backup/integrity ONLY. All domain reads/writes (products, sales, refunds,
//    suppliers, POs, expenses, customers, analytics) live in the *Repository
//    classes; the products()/sales()/... accessors hand out Database-owned
//    instances. recordSale() etc. moved to SaleRepository.
//  - ensureColumn() adds missing columns to old databases (in-place upgrade).
// =============================================================================
#include "database.h"
#include "salerepository.h"
#include "supplierrepository.h"
#include "customerrepository.h"
#include "productrepository.h"
#include "expenserepository.h"
#include "purchaseorderrepository.h"
#include "refundrepository.h"
#include "salesanalyticsrepository.h"
#include "passwordhasher.h"
#include "money.h"           // Money
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>
#include <functional>

Database::Database()
{
    db = QSqlDatabase::addDatabase("QSQLITE");

    // Use proper cross-platform data directory (the includes for QStandardPaths/QDir/QCoreApplication were present but unused)
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataPath.isEmpty()) {
        dataPath = QCoreApplication::applicationDirPath();
    }

    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    m_dbPath = dataPath + "/pos_database.db";
    db.setDatabaseName(m_dbPath);
}

Database::~Database()
{
    if (db.isOpen()) {
        db.close();
    }
}

void Database::configureForTesting(const QString &dbPath)
{
    const QString conn = QStringLiteral("keynetik_test");
    // Drop the cached repositories first: each holds a COPY of the current
    // connection handle, and they'd dangle (and crash on next use) once we
    // remove that connection below. They're lazily rebuilt against the new
    // connection on next access. Releasing them here also clears the only other
    // references, so removeDatabase() won't warn that the connection is in use.
    resetRepositories();
    if (db.isOpen())
        db.close();
    db = QSqlDatabase();   // drop our handle so removeDatabase() won't warn
    if (QSqlDatabase::contains(conn))
        QSqlDatabase::removeDatabase(conn);
    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
    db.setDatabaseName(dbPath);
    m_dbPath = dbPath;
    initialized = false;
}

bool Database::initialize(bool seedSampleData)
{
    // Idempotent: main() initializes the DB, and so does the MainWindow ctor
    // (which is rebuilt on every logout->login). Re-running open() makes the
    // SQLite driver close and reopen the file, and re-runs all the schema
    // bootstrap below for nothing. Once we've succeeded, later calls are no-ops.
    if (initialized && db.isOpen())
        return true;

    if (!db.open()) {
        lastError = "Failed to open database: " + db.lastError().text();
        qDebug() << lastError;
        return false;
    }
    qDebug() << "Database opened successfully";

    // Per-connection PRAGMAs. SQLite leaves foreign keys off unless asked.
    // WAL improves read/write concurrency and crash durability; busy_timeout
    // makes a second connection (e.g. a future second instance pointed at the
    // same DB file) wait briefly for a lock instead of failing with SQLITE_BUSY.
    {
        QSqlQuery pragma(db);
        pragma.exec("PRAGMA foreign_keys = ON");
        pragma.exec("PRAGMA journal_mode = WAL");
        pragma.exec("PRAGMA busy_timeout = 5000");
    }

    if (!createTables()) {
        qDebug() << "Failed to create tables";
        return false;
    }
    qDebug() << "Tables created successfully";

    // Insert sample data if tables are empty (skipped under tests)
    if (seedSampleData) {
        QSqlQuery query(db);
        if (query.exec("SELECT COUNT(*) FROM products") && query.next()) {
            int count = query.value(0).toInt();
            qDebug() << "Current product count:" << count;
            if (count == 0) {
                qDebug() << "Inserting sample data...";
                if (insertSampleData()) {
                    qDebug() << "Sample data inserted successfully";
                } else {
                    qDebug() << "Failed to insert sample data";
                }
            } else {
                qDebug() << "Database already has products, skipping sample data";
            }
        } else {
            qDebug() << "Failed to query product count:" << query.lastError().text();
        }
    }
    initialized = true;
    return true;
}

bool Database::isOpen() const
{
    return db.isOpen();
}

// ── Repository accessors ─────────────────────────────────────────────────────
// Each returns a reference to a single, Database-owned repository, created on
// first use so it captures whatever connection is live by then (including the
// test connection installed by configureForTesting()). Holding one instance per
// group — rather than constructing a throwaway per call — is what makes the
// "mutate, then read lastError() off the same accessor" pattern work.
ProductRepository &Database::products()
{
    if (!m_products) m_products = std::make_unique<ProductRepository>(db);
    return *m_products;
}

SaleRepository &Database::sales()
{
    if (!m_sales) m_sales = std::make_unique<SaleRepository>(db);
    return *m_sales;
}

SalesAnalyticsRepository &Database::salesAnalytics()
{
    if (!m_salesAnalytics) m_salesAnalytics = std::make_unique<SalesAnalyticsRepository>(db);
    return *m_salesAnalytics;
}

RefundRepository &Database::refunds()
{
    if (!m_refunds) m_refunds = std::make_unique<RefundRepository>(db);
    return *m_refunds;
}

SupplierRepository &Database::suppliers()
{
    if (!m_suppliers) m_suppliers = std::make_unique<SupplierRepository>(db);
    return *m_suppliers;
}

PurchaseOrderRepository &Database::purchaseOrders()
{
    if (!m_purchaseOrders) m_purchaseOrders = std::make_unique<PurchaseOrderRepository>(db);
    return *m_purchaseOrders;
}

ExpenseRepository &Database::expenses()
{
    if (!m_expenses) m_expenses = std::make_unique<ExpenseRepository>(db);
    return *m_expenses;
}

CustomerRepository &Database::customers()
{
    if (!m_customers) m_customers = std::make_unique<CustomerRepository>(db);
    return *m_customers;
}

void Database::resetRepositories()
{
    m_products.reset();
    m_sales.reset();
    m_salesAnalytics.reset();
    m_refunds.reset();
    m_suppliers.reset();
    m_purchaseOrders.reset();
    m_expenses.reset();
    m_customers.reset();
}

bool Database::createTables()
{
    QStringList queries;
    // Schema metadata (key/value) — tracks one-time migrations like the
    // money-to-cents conversion. A normal table persists reliably across reopen.
    queries << R"(
        CREATE TABLE IF NOT EXISTS schema_meta (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )";
    // Categories table
    queries << R"(
        CREATE TABLE IF NOT EXISTS categories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            description TEXT
        )
    )";
    // Products table
    queries << R"(
        CREATE TABLE IF NOT EXISTS products (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            category TEXT NOT NULL,
            price INTEGER NOT NULL,
            cost_price INTEGER NOT NULL DEFAULT 0,
            profit_margin REAL NOT NULL DEFAULT 0,
            stock_quantity INTEGER DEFAULT 0,
            reorder_level INTEGER NOT NULL DEFAULT 20,
            barcode TEXT UNIQUE,
            is_active INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (category) REFERENCES categories(name)
        )
    )";
    // Sales table
    //
    // TIMEZONE INVARIANT: sale_date defaults to CURRENT_TIMESTAMP, which SQLite
    // writes in UTC. Every query that filters or groups by it MUST convert
    // first — DATE(sale_date, 'localtime') — because callers pass local dates
    // from QDate::currentDate(). Comparing a UTC column against a local date
    // silently drops sales made between local midnight and the UTC rollover
    // (03:00 in Kenya), which is exactly when late-night shops trade.
    queries << R"(
        CREATE TABLE IF NOT EXISTS sales (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            subtotal INTEGER NOT NULL,
            tax INTEGER DEFAULT 0,
            discount INTEGER DEFAULT 0,
            total INTEGER NOT NULL,
            payment_method TEXT DEFAULT 'Cash',
            amount_paid INTEGER DEFAULT 0,
            change_due INTEGER DEFAULT 0
        )
    )";
    // Sale items table
    queries << R"(
        CREATE TABLE IF NOT EXISTS sale_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_id INTEGER NOT NULL,
            product_id INTEGER NOT NULL,
            product_name TEXT NOT NULL,
            quantity INTEGER NOT NULL,
            price INTEGER NOT NULL,
            cost_price INTEGER NOT NULL DEFAULT 0,
            subtotal INTEGER NOT NULL,
            FOREIGN KEY (sale_id) REFERENCES sales(id),
            FOREIGN KEY (product_id) REFERENCES products(id)
        )
    )";
    // Per-tender breakdown of a (possibly split) sale, for accurate
    // payment-method reporting. Written inside the recordSale() transaction.
    queries << R"(
        CREATE TABLE IF NOT EXISTS sale_payments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_id INTEGER NOT NULL,
            method TEXT NOT NULL,
            amount INTEGER NOT NULL,
            reference TEXT,
            FOREIGN KEY (sale_id) REFERENCES sales(id)
        )
    )";
    // Users table for RBAC
    queries << R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            full_name TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL,
            role TEXT NOT NULL DEFAULT 'Cashier',
            is_active INTEGER DEFAULT 1,
            created_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_login TIMESTAMP,
            created_by TEXT,
            must_change_password INTEGER DEFAULT 0,
            CHECK (role IN ('Admin', 'Manager', 'Cashier', 'Viewer'))
        )
    )";
    // User activity log table
    queries << R"(
        CREATE TABLE IF NOT EXISTS user_activity_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            username TEXT NOT NULL,
            action TEXT NOT NULL,
            details TEXT,
            timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    )";
    // Create indexes for better performance
    // Category browsing/filtering scans products(category, is_active); without
    // this it's a full table scan on every category switch and reload.
    queries << "CREATE INDEX IF NOT EXISTS idx_products_category ON products(category, is_active)";
    queries << "CREATE INDEX IF NOT EXISTS idx_users_username ON users(username)";
    queries << "CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)";
    queries << "CREATE INDEX IF NOT EXISTS idx_activity_log_user ON user_activity_log(user_id)";
    queries << "CREATE INDEX IF NOT EXISTS idx_activity_log_timestamp ON user_activity_log(timestamp)";
    // Stock adjustment log
    queries << R"(
        CREATE TABLE IF NOT EXISTS stock_adjustments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            product_id INTEGER NOT NULL,
            product_name TEXT NOT NULL,
            change_qty INTEGER NOT NULL,
            old_qty INTEGER NOT NULL,
            new_qty INTEGER NOT NULL,
            reason TEXT NOT NULL DEFAULT 'Manual Adjustment',
            adjusted_by TEXT NOT NULL DEFAULT 'System',
            adjusted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (product_id) REFERENCES products(id)
        )
    )";
    queries << "CREATE INDEX IF NOT EXISTS idx_adj_product ON stock_adjustments(product_id)";
    queries << "CREATE INDEX IF NOT EXISTS idx_adj_time ON stock_adjustments(adjusted_at)";
    // Refunds table
    queries << R"(
        CREATE TABLE IF NOT EXISTS refunds (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_id INTEGER NOT NULL,
            refund_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            total_refunded INTEGER NOT NULL,
            reason TEXT,
            processed_by TEXT,
            FOREIGN KEY (sale_id) REFERENCES sales(id)
        )
    )";
    // Suppliers table
    queries << R"(
        CREATE TABLE IF NOT EXISTS suppliers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            contact_person TEXT,
            phone TEXT,
            email TEXT,
            address TEXT,
            is_active INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    // Purchase orders (header)
    queries << R"(
        CREATE TABLE IF NOT EXISTS purchase_orders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            supplier_id INTEGER NOT NULL,
            status TEXT NOT NULL DEFAULT 'Pending',
            order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            received_date TIMESTAMP,
            notes TEXT,
            created_by TEXT,
            total INTEGER NOT NULL DEFAULT 0,
            CHECK (status IN ('Pending', 'Received', 'Cancelled')),
            FOREIGN KEY (supplier_id) REFERENCES suppliers(id)
        )
    )";
    // Purchase order line items
    queries << R"(
        CREATE TABLE IF NOT EXISTS purchase_order_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            po_id INTEGER NOT NULL,
            product_id INTEGER NOT NULL,
            product_name TEXT NOT NULL,
            quantity INTEGER NOT NULL,
            unit_cost INTEGER NOT NULL,
            subtotal INTEGER NOT NULL,
            FOREIGN KEY (po_id) REFERENCES purchase_orders(id),
            FOREIGN KEY (product_id) REFERENCES products(id)
        )
    )";
    queries << "CREATE INDEX IF NOT EXISTS idx_po_supplier ON purchase_orders(supplier_id)";
    queries << "CREATE INDEX IF NOT EXISTS idx_po_status ON purchase_orders(status)";
    queries << "CREATE INDEX IF NOT EXISTS idx_po_items_po ON purchase_order_items(po_id)";
    // Expense categories
    queries << R"(
        CREATE TABLE IF NOT EXISTS expense_categories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            is_active INTEGER DEFAULT 1
        )
    )";
    // Expenses
    queries << R"(
        CREATE TABLE IF NOT EXISTS expenses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            category_id INTEGER NOT NULL,
            amount INTEGER NOT NULL,
            description TEXT,
            date TEXT NOT NULL,
            recorded_by TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (category_id) REFERENCES expense_categories(id)
        )
    )";
    queries << "CREATE INDEX IF NOT EXISTS idx_expenses_date ON expenses(date)";
    queries << "CREATE INDEX IF NOT EXISTS idx_expenses_category ON expenses(category_id)";
    // Customers
    queries << R"(
        CREATE TABLE IF NOT EXISTS customers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            phone TEXT UNIQUE,
            email TEXT,
            address TEXT,
            loyalty_points INTEGER DEFAULT 0,
            store_credit INTEGER DEFAULT 0,
            is_active INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    queries << "CREATE INDEX IF NOT EXISTS idx_customers_phone ON customers(phone)";
    // Till reconciliation sign-off records
    queries << R"(
        CREATE TABLE IF NOT EXISTS till_reconciliations (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            reconciliation_date TEXT NOT NULL,
            cashier_name        TEXT,
            opening_float       INTEGER NOT NULL DEFAULT 0,
            cash_sales          INTEGER NOT NULL DEFAULT 0,
            expected_cash       INTEGER NOT NULL DEFAULT 0,
            counted_cash        INTEGER NOT NULL DEFAULT 0,
            variance            INTEGER NOT NULL DEFAULT 0,
            signed_off_by       TEXT,
            signed_off_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    queries << "CREATE INDEX IF NOT EXISTS idx_till_date ON till_reconciliations(reconciliation_date)";

    // Execute all queries
    for (const QString &queryStr : queries) {
        QSqlQuery query(db);  // Explicitly use our connection
        if (!query.exec(queryStr)) {
            lastError = "Failed to create table: " + query.lastError().text();
            qDebug() << lastError;
            return false;
        }
    }

    // Numbered, version-tracked schema migrations (replaces the old loose pile
    // of ensureColumn() calls). CREATE TABLE IF NOT EXISTS above is the baseline;
    // runMigrations() brings both fresh and pre-existing databases up to the
    // current version.
    if (!runMigrations())
        return false;

    // One-time REAL->cents data conversion. Kept SEPARATE from runMigrations()
    // and gated by its own marker row, never by schema_version: re-running it
    // would re-scale every amount by 100, so it must never key off a version a
    // legacy DB might still report as 0.
    if (!migrateMoneyToCents())
        return false;

    return true;
}

// Current applied schema version (0 when the marker row is absent — i.e. a
// database created before versioned migrations existed).
int Database::schemaVersion()
{
    QSqlQuery q(db);
    q.prepare("SELECT value FROM schema_meta WHERE key = 'schema_version'");
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

bool Database::setSchemaVersion(int version)
{
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO schema_meta (key, value) "
              "VALUES ('schema_version', ?)");
    q.addBindValue(QString::number(version));
    return q.exec();
}

// Applies every migration whose version is newer than the stored one, each in
// its own transaction (DDL + version bump commit together). Every step is
// written to be idempotent — ensureColumn() no-ops when the column already
// exists — so an old database (reporting version 0 but already carrying some of
// these columns) and a brand-new one both converge on the same final schema.
bool Database::runMigrations()
{
    struct Migration {
        int version;
        const char *description;
        std::function<bool()> apply;
    };

    const QVector<Migration> migrations = {
        { 1, "sales: amount_paid / change_due / customer_id / store_credit_used",
          [this] {
              return ensureColumn("sales", "amount_paid",       "INTEGER DEFAULT 0")
                  && ensureColumn("sales", "change_due",        "INTEGER DEFAULT 0")
                  && ensureColumn("sales", "customer_id",       "INTEGER DEFAULT 0")
                  && ensureColumn("sales", "store_credit_used", "INTEGER DEFAULT 0");
          } },
        { 2, "users: must_change_password",
          [this] {
              return ensureColumn("users", "must_change_password", "INTEGER DEFAULT 0");
          } },
        { 3, "products: reorder_level",
          [this] {
              return ensureColumn("products", "reorder_level", "INTEGER NOT NULL DEFAULT 20");
          } },
        { 4, "sales: cashier / shift_id (audit context)",
          [this] {
              return ensureColumn("sales", "cashier",  "TEXT DEFAULT ''")
                  && ensureColumn("sales", "shift_id", "INTEGER DEFAULT 0");
          } },
        { 5, "sales: external_ref (mobile offline-sync idempotency key)",
          [this] {
              // SQLite's ALTER TABLE ADD COLUMN can't carry a UNIQUE
              // constraint, so the column and its uniqueness are two steps.
              // Multiple NULLs (every sale rung up normally at the till, never
              // synced from a phone) are fine — SQLite treats NULLs as
              // distinct in a UNIQUE index, so they never collide.
              if (!ensureColumn("sales", "external_ref", "TEXT"))
                  return false;
              QSqlQuery idx(db);
              return idx.exec(
                  "CREATE UNIQUE INDEX IF NOT EXISTS idx_sales_external_ref "
                  "ON sales(external_ref)");
          } },
        { 6, "categories: broaden the seed list past the original 8 small-shop "
             "categories for larger-format stores",
          [this] {
              static const QStringList newCategories = {
                  "Fish & Seafood", "Cereals & Grains", "Cooking Oil & Fats",
                  "Rice, Pasta & Noodles", "Sugar & Baking Supplies", "Spices & Condiments",
                  "Canned & Packaged Foods", "Health & Beauty", "Personal Care",
                  "Baby Care", "Feminine Care", "Cleaning Supplies",
                  "Kitchenware & Utensils", "Electronics", "Stationery & Office",
                  "Clothing & Apparel", "Footwear", "Toys & Games",
                  "Sports & Outdoor", "Automotive", "Hardware & Tools",
                  "Pet Supplies", "Alcohol & Spirits", "Tobacco",
                  "Pharmacy", "Mobile & Airtime", "Gifts & Party Supplies",
                  "Books & Magazines",
              };
              QSqlQuery q(db);
              q.prepare("INSERT OR IGNORE INTO categories (name) VALUES (?)");
              for (const QString &name : newCategories) {
                  q.addBindValue(name);
                  if (!q.exec())
                      return false;
              }
              return true;
          } },
    };

    const int current = schemaVersion();
    for (const Migration &m : migrations) {
        if (m.version <= current)
            continue;
        if (!db.transaction()) {
            lastError = "Migration " + QString::number(m.version)
                      + " could not start a transaction: " + db.lastError().text();
            return false;
        }
        if (!m.apply() || !setSchemaVersion(m.version)) {
            db.rollback();
            lastError = "Migration " + QString::number(m.version) + " ("
                      + m.description + ") failed: " + lastError;
            return false;
        }
        if (!db.commit()) {
            lastError = "Migration " + QString::number(m.version)
                      + " could not commit: " + db.lastError().text();
            return false;
        }
        qDebug() << "Applied schema migration" << m.version << m.description;
    }
    return true;
}

// One-time conversion of money columns from REAL major units (e.g. 19.99) to
// INTEGER minor units (1999). Idempotency is tracked by a marker row in
// schema_meta (a normal table write, which persists reliably across reopen —
// unlike a PRAGMA user_version header write under WAL via the Qt driver, which
// did not). The marker is written in the SAME transaction as the UPDATEs, so
// the data scaling and the "done" flag commit atomically and the migration can
// never run twice (which would re-scale every amount by 100). A freshly-created
// DB has no rows yet (sample data is seeded afterwards), so the UPDATEs are
// harmless no-ops there; only a pre-existing (legacy) database has values to
// scale.
bool Database::migrateMoneyToCents()
{
    {
        QSqlQuery done(db);
        done.prepare("SELECT value FROM schema_meta WHERE key = 'money_in_cents'");
        if (done.exec() && done.next())
            return true;   // already migrated
    }

    static const char *const updates[] = {
        "UPDATE products SET price=CAST(ROUND(price*100) AS INTEGER), "
        "cost_price=CAST(ROUND(cost_price*100) AS INTEGER)",
        "UPDATE sales SET subtotal=CAST(ROUND(subtotal*100) AS INTEGER), "
        "tax=CAST(ROUND(tax*100) AS INTEGER), "
        "discount=CAST(ROUND(discount*100) AS INTEGER), "
        "total=CAST(ROUND(total*100) AS INTEGER), "
        "amount_paid=CAST(ROUND(amount_paid*100) AS INTEGER), "
        "change_due=CAST(ROUND(change_due*100) AS INTEGER)",
        "UPDATE sale_items SET price=CAST(ROUND(price*100) AS INTEGER), "
        "cost_price=CAST(ROUND(cost_price*100) AS INTEGER), "
        "subtotal=CAST(ROUND(subtotal*100) AS INTEGER)",
        "UPDATE refunds SET total_refunded=CAST(ROUND(total_refunded*100) AS INTEGER)",
    };

    db.transaction();
    for (const char *sql : updates) {
        QSqlQuery u(db);
        if (!u.exec(QLatin1String(sql))) {
            lastError = "Money migration failed: " + u.lastError().text();
            qDebug() << lastError;
            db.rollback();
            return false;
        }
    }
    QSqlQuery mark(db);
    mark.exec("INSERT OR REPLACE INTO schema_meta (key, value) "
              "VALUES ('money_in_cents', '1')");
    return db.commit();
}

bool Database::ensureColumn(const QString &table, const QString &column,
                            const QString &definition)
{
    QSqlQuery info(db);
    info.exec(QString("PRAGMA table_info(%1)").arg(table));
    while (info.next()) {
        if (info.value(1).toString() == column)
            return true;
    }

    QSqlQuery alter(db);
    if (!alter.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3")
                        .arg(table, column, definition))) {
        lastError = alter.lastError().text();
        qDebug() << "Migration failed for" << table << column << ":" << lastError;
        return false;
    }
    return true;
}

bool Database::insertSampleData()
{
    qDebug() << "Starting insertSampleData()";

    // Insert categories
    QStringList categories = {"Beverages", "Snacks", "Bakery", "Dairy", "Meat", "Produce", "Frozen", "Household"};
    for (const QString &cat : categories) {
        QSqlQuery query(db);
        query.prepare("INSERT INTO categories (name) VALUES (?)");
        query.addBindValue(cat);
        if (!query.exec()) {
            qDebug() << "Failed to insert category:" << cat << query.lastError().text();
            // Continue (name is UNIQUE, so duplicates are harmless on re-run)
        }
    }
    qDebug() << "Categories inserted";

    // Insert sample products (fixed inconsistent cost values for realism; all now in whole numbers as per most of your original data)
    // Structure: {name, category, cost_price, profit_margin, stock, barcode}
    QVector<QVariantList> products = {
        {"Coca-Cola 500ml", "Beverages", 50.0, 33.0, 100, "CC500"},
        {"Pepsi 500ml", "Beverages", 50.0, 33.0, 100, "PP500"},
        {"Water 1L", "Beverages", 50.0, 33.0, 150, "W1L"},
        {"Orange Juice 1L", "Beverages", 245.0, 30.0, 50, "OJ1L"},
        {"Coffee", "Beverages", 180.0, 28.0, 80, "COFFEE"},
        {"Old Spice","PE",120.0,85.0,45,"50001740033451"},

        {"Chips - BBQ", "Snacks", 140.0, 30.0, 75, "CHIP001"},
        {"Chips - Salt & Vinegar", "Snacks", 140.0, 30.0, 75, "CHIP002"},
        {"Chocolate Bar", "Snacks", 85.0, 32.0, 120, "CHOC001"},
        {"Cookies", "Snacks", 215.0, 28.0, 60, "COOK001"},
        {"Nuts Mix", "Snacks", 325.0, 28.0, 40, "NUTS001"},

        {"White Bread", "Bakery", 180.0, 28.0, 50, "BREAD001"},
        {"Wheat Bread", "Bakery", 195.0, 29.0, 50, "BREAD002"},
        {"Croissant", "Bakery", 105.0, 30.0, 30, "CROIS001"},
        {"Donut", "Bakery", 70.0, 30.0, 40, "DONUT001"},
        {"Muffin", "Bakery", 140.0, 30.0, 35, "MUFF001"},

        {"Milk 1L", "Dairy", 180.0, 28.0, 60, "MILK1L"},
        {"Cheese 500g", "Dairy", 400.0, 27.0, 40, "CHEESE500"},
        {"Yogurt", "Dairy", 105.0, 30.0, 80, "YOG001"},
        {"Butter 250g", "Dairy", 250.0, 28.0, 50, "BUTT250"},
        {"Eggs (12)", "Dairy", 215.0, 28.0, 70, "EGG12"},

        {"Chicken Breast 1kg", "Meat", 620.0, 27.0, 30, "CHICK1K"},
        {"Ground Beef 500g", "Meat", 430.0, 28.0, 25, "BEEF500"},
        {"Salmon Fillet", "Meat", 880.0, 27.0, 20, "SAL001"},
        {"Bacon 250g", "Meat", 395.0, 28.0, 35, "BAC250"},

        {"Apples 1kg", "Produce", 250.0, 28.0, 100, "APP1K"},
        {"Bananas 1kg", "Produce", 180.0, 28.0, 120, "BAN1K"},
        {"Tomatoes 500g", "Produce", 140.0, 30.0, 80, "TOM500"},
        {"Lettuce", "Produce", 105.0, 30.0, 60, "LET001"},
        {"Carrots 1kg", "Produce", 125.0, 29.0, 90, "CAR1K"},

        {"Ice Cream 1L", "Frozen", 320.0, 28.0, 40, "ICE1L"},
        {"Frozen Pizza", "Frozen", 430.0, 28.0, 50, "PIZ001"},
        {"Frozen Vegetables", "Frozen", 215.0, 28.0, 60, "FVEG001"},

        {"Dish Soap", "Household", 250.0, 28.0, 50, "SOAP001"},
        {"Paper Towels", "Household", 285.0, 29.0, 40, "TOWEL001"},
        {"Toilet Paper (4)", "Household", 360.0, 28.0, 60, "TP004"}
    };

    for (const QVariantList &product : products) {
        const Money  costPrice    = Money::fromMajor(product[2].toDouble());
        const double profitMargin = product[3].toDouble();
        const Money  sellingPrice = Product::calculateSellingPrice(costPrice, profitMargin);

        QSqlQuery query(db);
        query.prepare("INSERT INTO products (name, category, cost_price, profit_margin, price, stock_quantity, barcode) VALUES (?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(product[0]);
        query.addBindValue(product[1]);
        query.addBindValue(costPrice.cents());
        query.addBindValue(profitMargin);
        query.addBindValue(sellingPrice.cents());
        query.addBindValue(product[4]);
        query.addBindValue(product[5]);

        if (!query.exec()) {
            qDebug() << "Failed to insert product:" << product[0].toString() << query.lastError().text();
        }
    }

    // Create default admin user if users table is empty
    QSqlQuery checkUsers(db);
    checkUsers.prepare("SELECT COUNT(*) FROM users");
    if (checkUsers.exec() && checkUsers.next()) {
        int userCount = checkUsers.value(0).toInt();
        if (userCount == 0) {
            qDebug() << "Creating default admin user...";
            // must_change_password forces a new password at first login
            QString defaultPasswordHash = PasswordHasher::hash("admin123");
            QSqlQuery createAdmin(db);
            createAdmin.prepare(
                "INSERT INTO users (username, password_hash, full_name, email, role, is_active, created_by, must_change_password) "
                "VALUES ('admin', ?, 'System Administrator', 'admin@pos.local', 'Admin', 1, 'System', 1)"
                );
            createAdmin.addBindValue(defaultPasswordHash);
            if (createAdmin.exec()) {
                qDebug() << "===========================================";
                qDebug() << "DEFAULT ADMIN USER CREATED";
                qDebug() << "Username: admin";
                qDebug() << "Password: admin123";
                qDebug() << "A new password is required at first login.";
                qDebug() << "===========================================";
            } else {
                qDebug() << "Failed to create default admin user:" << createAdmin.lastError().text();
            }
        }
    }

    qDebug() << "Finished inserting" << products.size() << "products";
    return true;
}

// ── Domain data access ───────────────────────────────────────────────────────
// All product/sale/analytics/refund/supplier/PO/expense/customer reads and
// writes live in their repositories now (see the accessors above and the
// *repository.cpp files). Database only owns connection lifecycle, schema
// bootstrap/migrations, and backup/integrity below.

// ==================== Backup ====================

QString Database::backupDirectory() const
{
    return QFileInfo(m_dbPath).absolutePath() + "/backups";
}

bool Database::backupTo(const QString &destDir, QString *outPath)
{
    if (!db.isOpen()) {
        lastError = "Cannot back up: database is not open";
        return false;
    }

    QDir dir(destDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        lastError = "Could not create backup directory: " + destDir;
        return false;
    }

    // Fold the WAL back into the main DB file so the copy is a complete,
    // self-contained snapshot (the app is single-threaded, so nothing is
    // writing concurrently during this call).
    {
        QSqlQuery checkpoint(db);
        checkpoint.exec("PRAGMA wal_checkpoint(TRUNCATE)");
    }

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString destPath = QString("%1/pos_database_%2.db").arg(destDir, stamp);

    if (QFile::exists(destPath))
        QFile::remove(destPath);   // same-second re-run: overwrite

    if (!QFile::copy(m_dbPath, destPath)) {
        lastError = "Failed to copy database to " + destPath;
        return false;
    }

    if (outPath)
        *outPath = destPath;

    // Verify the copy is readable and not corrupted before reporting success.
    QString verifyErr;
    if (!verifyBackup(destPath, &verifyErr)) {
        QFile::remove(destPath);   // discard the bad copy
        lastError = "Backup copy failed integrity check: " + verifyErr;
        return false;
    }
    return true;
}

void Database::rotateBackups(const QString &destDir, int keep)
{
    if (keep < 0) keep = 0;

    QDir dir(destDir);
    // Names are timestamped (pos_database_YYYYMMDD_HHmmss.db), so a plain name
    // sort is chronological. Newest last.
    QStringList files = dir.entryList(QStringList() << "pos_database_*.db",
                                      QDir::Files, QDir::Name);
    while (files.size() > keep) {
        const QString oldest = files.takeFirst();
        QFile::remove(dir.absoluteFilePath(oldest));
    }
}

bool Database::backupIfDue(int keep)
{
    QSettings settings("KeynetikPOS", "KeynetikPOS");
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    if (settings.value("backup/lastDate").toString() == today)
        return true;   // already backed up today — nothing to do

    if (!backupTo(backupDirectory()))
        return false;

    rotateBackups(backupDirectory(), keep);
    settings.setValue("backup/lastDate", today);
    return true;
}

QString Database::getLastError() const
{
    return lastError;
}

bool Database::executeQuery(const QString &queryStr)
{
    QSqlQuery q(db);
    return q.exec(queryStr);
}

bool Database::verifyDatabaseIntegrity(QString *errorOut)
{
    QSqlQuery q(db);
    if (!q.exec("PRAGMA integrity_check")) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }
    if (q.next()) {
        const QString result = q.value(0).toString();
        if (result != "ok") {
            if (errorOut) *errorOut = result;
            return false;
        }
    }
    return true;
}

bool Database::verifyBackup(const QString &backupPath, QString *errorOut)
{
    const QString connName = "backup_verify_" +
                             QString::number(QDateTime::currentMSecsSinceEpoch());
    {
        QSqlDatabase bdb = QSqlDatabase::addDatabase("QSQLITE", connName);
        bdb.setDatabaseName(backupPath);
        if (!bdb.open()) {
            if (errorOut) *errorOut = "Cannot open backup file: " + bdb.lastError().text();
            QSqlDatabase::removeDatabase(connName);
            return false;
        }
        QSqlQuery q(bdb);
        if (!q.exec("PRAGMA integrity_check") || !q.next()) {
            if (errorOut) *errorOut = "integrity_check failed: " + q.lastError().text();
            bdb.close();
            QSqlDatabase::removeDatabase(connName);
            return false;
        }
        const QString result = q.value(0).toString();
        if (result != "ok") {
            if (errorOut) *errorOut = "Backup corrupted: " + result;
            bdb.close();
            QSqlDatabase::removeDatabase(connName);
            return false;
        }
        // Spot-check that core tables have rows
        q.exec("SELECT COUNT(*) FROM sales");
        bdb.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return true;
}
