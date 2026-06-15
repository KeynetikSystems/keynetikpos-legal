// =============================================================================
// database.cpp — Implementation of the Database singleton (see database.h for
// the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - The constructor resolves a per-user AppData directory and opens
//    pos_database.db there; initialize() enables PRAGMA foreign_keys (off by
//    default per SQLite connection), creates tables idempotently, and seeds
//    sample products only when the products table is empty.
//  - recordSale() is the heart of checkout: stock re-validation, sale insert,
//    per-item inserts (snapshotting name/price/cost), and stock decrements all
//    happen inside ONE transaction; any failure rolls back and returns -1 with
//    lastError set.
//  - adjustStockWithLog() wraps a manual stock change and its audit-log row in
//    one transaction so the log can never disagree with the stock.
//  - ensureColumn() adds missing columns to old databases (in-place upgrade).
// =============================================================================
#include "database.h"
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

Database& Database::instance()
{
    static Database instance;
    return instance;
}

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
bool Database::adjustStockWithLog(int productId, int qtyChange,
                                  const QString &reason, const QString &adjustedBy)
{
    if (!db.transaction()) return false;

    Product p = getProductById(productId);
    int oldQty = p.stockQuantity;
    int newQty = oldQty + qtyChange;

    if (!updateStock(productId, newQty)) {
        db.rollback();
        return false;
    }

    if (!logStockAdjustment(productId, p.name, oldQty, newQty, reason, adjustedBy)) {
        db.rollback();
        return false;
    }

    return db.commit();
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

    // Execute all queries
    for (const QString &queryStr : queries) {
        QSqlQuery query(db);  // Explicitly use our connection
        if (!query.exec(queryStr)) {
            lastError = "Failed to create table: " + query.lastError().text();
            qDebug() << lastError;
            return false;
        }
    }

    // Migrations for databases created before these columns existed
    // (CREATE TABLE IF NOT EXISTS does not alter existing tables)
    ensureColumn("sales", "amount_paid", "INTEGER DEFAULT 0");
    ensureColumn("sales", "change_due", "INTEGER DEFAULT 0");
    ensureColumn("users", "must_change_password", "INTEGER DEFAULT 0");
    ensureColumn("products", "reorder_level", "INTEGER NOT NULL DEFAULT 20");

    if (!migrateMoneyToCents())
        return false;

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
        {"Coca-Cola 500ml", "Beverages", 50.0, 50.0, 100, "CC500"},   // 50 -> 75
        {"Pepsi 500ml", "Beverages", 50.0, 50.0, 100, "PP500"},       // 50 -> 75
        {"Water 1L", "Beverages", 50.0, 50.0, 150, "W1L"},            // 50 -> 75
        {"Orange Juice 1L", "Beverages", 250.0, 40.0, 50, "OJ1L"},     // 250 -> 350
        {"Coffee", "Beverages", 180.0, 38.9, 80, "COFFEE"},           // 180 -> 250
        {"Chips - BBQ", "Snacks", 140.0, 42.9, 75, "CHIP001"},        // 140 -> 200
        {"Chips - Salt & Vinegar", "Snacks", 140.0, 42.9, 75, "CHIP002"},
        {"Chocolate Bar", "Snacks", 85.0, 47.1, 120, "CHOC001"},      // 85 -> 125
        {"Cookies", "Snacks", 210.0, 42.9, 60, "COOK001"},            // 210 -> 300
        {"Nuts Mix", "Snacks", 320.0, 40.6, 40, "NUTS001"},           // 320 -> 450
        {"White Bread", "Bakery", 180.0, 38.9, 50, "BREAD001"},       // 180 -> 250
        {"Wheat Bread", "Bakery", 195.0, 41.0, 50, "BREAD002"},       // 195 -> 275
        {"Croissant", "Bakery", 105.0, 42.9, 30, "CROIS001"},         // 105 -> 150
        {"Donut", "Bakery", 70.0, 42.9, 40, "DONUT001"},              // 70 -> 100
        {"Muffin", "Bakery", 140.0, 42.9, 35, "MUFF001"},             // 140 -> 200
        {"Milk 1L", "Dairy", 180.0, 38.9, 60, "MILK1L"},              // 180 -> 250
        {"Cheese 500g", "Dairy", 400.0, 37.5, 40, "CHEESE500"},       // 400 -> 550
        {"Yogurt", "Dairy", 105.0, 42.9, 80, "YOG001"},               // 105 -> 150
        {"Butter 250g", "Dairy", 250.0, 40.0, 50, "BUTT250"},         // 250 -> 350
        {"Eggs (12)", "Dairy", 215.0, 39.5, 70, "EGG12"},             // 215 -> 300
        {"Chicken Breast 1kg", "Meat", 620.0, 37.1, 30, "CHICK1K"},   // 620 -> 850
        {"Ground Beef 500g", "Meat", 430.0, 39.5, 25, "BEEF500"},     // 430 -> 600
        {"Salmon Fillet", "Meat", 880.0, 36.4, 20, "SAL001"},         // 880 -> 1200
        {"Bacon 250g", "Meat", 395.0, 39.2, 35, "BAC250"},            // 395 -> 550
        {"Apples 1kg", "Produce", 250.0, 40.0, 100, "APP1K"},         // 250 -> 350
        {"Bananas 1kg", "Produce", 180.0, 38.9, 120, "BAN1K"},        // 180 -> 250
        {"Tomatoes 500g", "Produce", 140.0, 42.9, 80, "TOM500"},      // 140 -> 200
        {"Lettuce", "Produce", 105.0, 42.9, 60, "LET001"},            // 105 -> 150
        {"Carrots 1kg", "Produce", 125.0, 40.0, 90, "CAR1K"},         // 125 -> 175
        {"Ice Cream 1L", "Frozen", 320.0, 40.6, 40, "ICE1L"},         // 320 -> 450
        {"Frozen Pizza", "Frozen", 430.0, 39.5, 50, "PIZ001"},        // 430 -> 600
        {"Frozen Vegetables", "Frozen", 215.0, 39.5, 60, "FVEG001"},  // 215 -> 300
        {"Dish Soap", "Household", 250.0, 40.0, 50, "SOAP001"},       // 250 -> 350
        {"Paper Towels", "Household", 285.0, 40.4, 40, "TOWEL001"},   // 285 -> 400
        {"Toilet Paper (4)", "Household", 360.0, 38.9, 60, "TP004"}   // 360 -> 500
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

// ==================== Product operations ====================

QVector<Product> Database::getAllProducts()
{
    QVector<Product> products;
    QSqlQuery query(db);
    query.prepare("SELECT id, name, category, price, cost_price, profit_margin, stock_quantity, reorder_level, barcode, is_active FROM products WHERE is_active = 1");
    query.exec();
    while (query.next()) {
        Product p;
        p.id = query.value(0).toInt();
        p.name = query.value(1).toString();
        p.category = query.value(2).toString();
        p.price = Money::fromCents(query.value(3).toLongLong());
        p.costPrice = Money::fromCents(query.value(4).toLongLong());
        p.profitMargin = query.value(5).toDouble();
        p.stockQuantity = query.value(6).toInt();
        p.reorderLevel = query.value(7).toInt();
        p.barcode = query.value(8).toString();
        p.isActive = query.value(9).toBool();
        products.append(p);
    }
    return products;
}

QVector<Product> Database::getProductsByCategory(const QString &category)
{
    QVector<Product> products;
    QSqlQuery query(db);
    query.prepare("SELECT id, name, category, price, cost_price, profit_margin, stock_quantity, reorder_level, barcode, is_active FROM products WHERE category = ? AND is_active = 1");
    query.addBindValue(category);
    query.exec();
    while (query.next()) {
        Product p;
        p.id = query.value(0).toInt();
        p.name = query.value(1).toString();
        p.category = query.value(2).toString();
        p.price = Money::fromCents(query.value(3).toLongLong());
        p.costPrice = Money::fromCents(query.value(4).toLongLong());
        p.profitMargin = query.value(5).toDouble();
        p.stockQuantity = query.value(6).toInt();
        p.reorderLevel = query.value(7).toInt();
        p.barcode = query.value(8).toString();
        p.isActive = query.value(9).toBool();
        products.append(p);
    }
    return products;
}

Product Database::getProductById(int id)
{
    Product p;
    QSqlQuery query(db);
    query.prepare("SELECT id, name, category, price, cost_price, profit_margin, stock_quantity, reorder_level, barcode, is_active FROM products WHERE id = ?");
    query.addBindValue(id);
    query.exec();
    if (query.next()) {
        p.id = query.value(0).toInt();
        p.name = query.value(1).toString();
        p.category = query.value(2).toString();
        p.price = Money::fromCents(query.value(3).toLongLong());
        p.costPrice = Money::fromCents(query.value(4).toLongLong());
        p.profitMargin = query.value(5).toDouble();
        p.stockQuantity = query.value(6).toInt();
        p.reorderLevel = query.value(7).toInt();
        p.barcode = query.value(8).toString();
        p.isActive = query.value(9).toBool();
    }
    return p;
}

Product Database::getProductByBarcode(const QString &barcode)
{
    Product p;
    QSqlQuery query(db);
    query.prepare("SELECT id, name, category, price, cost_price, profit_margin, stock_quantity, reorder_level, barcode, is_active FROM products WHERE barcode = ?");
    query.addBindValue(barcode);
    query.exec();
    if (query.next()) {
        p.id = query.value(0).toInt();
        p.name = query.value(1).toString();
        p.category = query.value(2).toString();
        p.price = Money::fromCents(query.value(3).toLongLong());
        p.costPrice = Money::fromCents(query.value(4).toLongLong());
        p.profitMargin = query.value(5).toDouble();
        p.stockQuantity = query.value(6).toInt();
        p.reorderLevel = query.value(7).toInt();
        p.barcode = query.value(8).toString();
        p.isActive = query.value(9).toBool();
    }
    return p;
}

bool Database::addProduct(const Product &product)
{
    QSqlQuery query(db);
    const Money sellingPrice = Product::calculateSellingPrice(product.costPrice, product.profitMargin);
    query.prepare("INSERT INTO products (name, category, cost_price, profit_margin, price, stock_quantity, reorder_level, barcode) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(product.name);
    query.addBindValue(product.category);
    query.addBindValue(product.costPrice.cents());
    query.addBindValue(product.profitMargin);
    query.addBindValue(sellingPrice.cents());
    query.addBindValue(product.stockQuantity);
    query.addBindValue(product.reorderLevel);
    query.addBindValue(product.barcode);
    if (!query.exec()) {
        lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateProduct(const Product &product)
{
    QSqlQuery query(db);
    const Money sellingPrice = Product::calculateSellingPrice(product.costPrice, product.profitMargin);
    query.prepare("UPDATE products SET name = ?, category = ?, cost_price = ?, profit_margin = ?, price = ?, stock_quantity = ?, reorder_level = ?, barcode = ?, is_active = ? WHERE id = ?");
    query.addBindValue(product.name);
    query.addBindValue(product.category);
    query.addBindValue(product.costPrice.cents());
    query.addBindValue(product.profitMargin);
    query.addBindValue(sellingPrice.cents());
    query.addBindValue(product.stockQuantity);
    query.addBindValue(product.reorderLevel);
    query.addBindValue(product.barcode);
    query.addBindValue(product.isActive);
    query.addBindValue(product.id);
    if (!query.exec()) {
        lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteProduct(int id)
{
    QSqlQuery query(db);
    query.prepare("UPDATE products SET is_active = 0 WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec()) {
        lastError = query.lastError().text();
        return false;
    }
    return true;
}

QStringList Database::getAllCategories()
{
    QStringList categories;
    QSqlQuery query(db);
    query.prepare("SELECT DISTINCT category FROM products WHERE is_active = 1 ORDER BY category");
    query.exec();
    while (query.next()) {
        categories.append(query.value(0).toString());
    }
    return categories;
}

// ==================== Inventory operations ====================

bool Database::updateStock(int productId, int newQuantity)
{
    QSqlQuery query(db);
    query.prepare("UPDATE products SET stock_quantity = ? WHERE id = ?");
    query.addBindValue(newQuantity);
    query.addBindValue(productId);
    return query.exec();
}

bool Database::setReorderLevel(int productId, int level)
{
    QSqlQuery query(db);
    query.prepare("UPDATE products SET reorder_level = ? WHERE id = ?");
    query.addBindValue(level);
    query.addBindValue(productId);
    if (!query.exec()) {
        lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::decreaseStock(int productId, int quantity)
{
    QSqlQuery query(db);
    query.prepare("UPDATE products SET stock_quantity = stock_quantity - ? WHERE id = ?");
    query.addBindValue(quantity);
    query.addBindValue(productId);
    return query.exec();
}

bool Database::increaseStock(int productId, int quantity)
{
    QSqlQuery query(db);
    query.prepare("UPDATE products SET stock_quantity = stock_quantity + ? WHERE id = ?");
    query.addBindValue(quantity);
    query.addBindValue(productId);
    return query.exec();
}

int Database::getStock(int productId)
{
    QSqlQuery query(db);
    query.prepare("SELECT stock_quantity FROM products WHERE id = ?");
    query.addBindValue(productId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

// ==================== Sales operations ====================

int Database::recordSale(const QVector<SaleItem> &items,
                         Money subtotal, Money tax, Money discount, Money total,
                         const QString &paymentMethod,
                         Money amountPaid, Money changeDue)
{
    if (items.isEmpty()) {
        lastError = "Cannot record a sale with no items";
        return -1;
    }

    if (!db.transaction()) {
        lastError = "Failed to start transaction: " + db.lastError().text();
        return -1;
    }

    // Validate stock inside the transaction so concurrent sales can't oversell
    for (const SaleItem &item : items) {
        QSqlQuery stockQuery(db);
        stockQuery.prepare("SELECT stock_quantity FROM products WHERE id = ?");
        stockQuery.addBindValue(item.productId);
        if (!stockQuery.exec() || !stockQuery.next()) {
            lastError = "Product not found: " + item.productName;
            db.rollback();
            return -1;
        }
        if (stockQuery.value(0).toInt() < item.quantity) {
            lastError = QString("Insufficient stock for %1 (available: %2, requested: %3)")
                            .arg(item.productName)
                            .arg(stockQuery.value(0).toInt())
                            .arg(item.quantity);
            db.rollback();
            return -1;
        }
    }

    QSqlQuery saleQuery(db);
    saleQuery.prepare("INSERT INTO sales (subtotal, tax, discount, total, payment_method, amount_paid, change_due) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?)");
    saleQuery.addBindValue(subtotal.cents());
    saleQuery.addBindValue(tax.cents());
    saleQuery.addBindValue(discount.cents());
    saleQuery.addBindValue(total.cents());
    saleQuery.addBindValue(paymentMethod);
    saleQuery.addBindValue(amountPaid.cents());
    saleQuery.addBindValue(changeDue.cents());
    if (!saleQuery.exec()) {
        lastError = saleQuery.lastError().text();
        db.rollback();
        return -1;
    }
    const int saleId = saleQuery.lastInsertId().toInt();

    for (const SaleItem &item : items) {
        QSqlQuery itemQuery(db);
        itemQuery.prepare("INSERT INTO sale_items (sale_id, product_id, product_name, quantity, price, cost_price, subtotal) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?)");
        itemQuery.addBindValue(saleId);
        itemQuery.addBindValue(item.productId);
        itemQuery.addBindValue(item.productName);
        itemQuery.addBindValue(item.quantity);
        itemQuery.addBindValue(item.price.cents());
        itemQuery.addBindValue(item.costPrice.cents());
        itemQuery.addBindValue((item.price * item.quantity).cents());
        if (!itemQuery.exec()) {
            lastError = itemQuery.lastError().text();
            db.rollback();
            return -1;
        }

        QSqlQuery stockUpdate(db);
        stockUpdate.prepare("UPDATE products SET stock_quantity = stock_quantity - ? WHERE id = ?");
        stockUpdate.addBindValue(item.quantity);
        stockUpdate.addBindValue(item.productId);
        if (!stockUpdate.exec()) {
            lastError = stockUpdate.lastError().text();
            db.rollback();
            return -1;
        }
    }

    if (!db.commit()) {
        lastError = "Failed to commit sale: " + db.lastError().text();
        db.rollback();
        return -1;
    }
    return saleId;
}

QVector<Sale> Database::getAllSales()
{
    QVector<Sale> sales;
    QSqlQuery query(db);
    query.prepare("SELECT id, sale_date, subtotal, tax, discount, total, payment_method, amount_paid, change_due FROM sales ORDER BY sale_date DESC");
    query.exec();
    while (query.next()) {
        Sale s;
        s.id = query.value(0).toInt();
        s.saleDate = query.value(1).toString();
        s.subtotal = Money::fromCents(query.value(2).toLongLong());
        s.tax = Money::fromCents(query.value(3).toLongLong());
        s.discount = Money::fromCents(query.value(4).toLongLong());
        s.total = Money::fromCents(query.value(5).toLongLong());
        s.paymentMethod = query.value(6).toString();
        s.amountPaid = Money::fromCents(query.value(7).toLongLong());
        s.changeDue = Money::fromCents(query.value(8).toLongLong());
        sales.append(s);
    }
    return sales;
}

QVector<Sale> Database::getSalesByDateRange(const QString &startDate, const QString &endDate)
{
    QVector<Sale> sales;
    QSqlQuery query(db);
    query.prepare("SELECT id, sale_date, subtotal, tax, discount, total, payment_method, amount_paid, change_due FROM sales WHERE DATE(sale_date) BETWEEN ? AND ? ORDER BY sale_date DESC");
    query.addBindValue(startDate);
    query.addBindValue(endDate);
    query.exec();
    while (query.next()) {
        Sale s;
        s.id = query.value(0).toInt();
        s.saleDate = query.value(1).toString();
        s.subtotal = Money::fromCents(query.value(2).toLongLong());
        s.tax = Money::fromCents(query.value(3).toLongLong());
        s.discount = Money::fromCents(query.value(4).toLongLong());
        s.total = Money::fromCents(query.value(5).toLongLong());
        s.paymentMethod = query.value(6).toString();
        s.amountPaid = Money::fromCents(query.value(7).toLongLong());
        s.changeDue = Money::fromCents(query.value(8).toLongLong());
        sales.append(s);
    }
    return sales;
}

QVector<SaleItem> Database::getSaleItems(int saleId)
{
    QVector<SaleItem> items;
    QSqlQuery query(db);
    query.prepare("SELECT id, sale_id, product_id, product_name, quantity, price, cost_price, subtotal FROM sale_items WHERE sale_id = ?");
    query.addBindValue(saleId);
    query.exec();
    while (query.next()) {
        SaleItem item;
        item.id = query.value(0).toInt();
        item.saleId = query.value(1).toInt();
        item.productId = query.value(2).toInt();
        item.productName = query.value(3).toString();
        item.quantity = query.value(4).toInt();
        item.price = Money::fromCents(query.value(5).toLongLong());
        item.costPrice = Money::fromCents(query.value(6).toLongLong());
        item.subtotal = Money::fromCents(query.value(7).toLongLong());
        items.append(item);
    }
    return items;
}

Sale Database::getSaleById(int saleId)
{
    Sale s;
    QSqlQuery query(db);
    query.prepare("SELECT id, sale_date, subtotal, tax, discount, total, payment_method, amount_paid, change_due FROM sales WHERE id = ?");
    query.addBindValue(saleId);
    query.exec();
    if (query.next()) {
        s.id = query.value(0).toInt();
        s.saleDate = query.value(1).toString();
        s.subtotal = Money::fromCents(query.value(2).toLongLong());
        s.tax = Money::fromCents(query.value(3).toLongLong());
        s.discount = Money::fromCents(query.value(4).toLongLong());
        s.total = Money::fromCents(query.value(5).toLongLong());
        s.paymentMethod = query.value(6).toString();
        s.amountPaid = Money::fromCents(query.value(7).toLongLong());
        s.changeDue = Money::fromCents(query.value(8).toLongLong());
    }
    return s;
}

// ==================== Analytics ====================

Money Database::getTotalSalesToday()
{
    QSqlQuery query(db);
    query.prepare("SELECT SUM(total) FROM sales WHERE DATE(sale_date) = DATE('now')");
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value(0).toLongLong());
    }
    return Money();
}

Money Database::getTotalSalesThisMonth()
{
    QSqlQuery query(db);
    query.prepare("SELECT SUM(total) FROM sales WHERE strftime('%Y-%m', sale_date) = strftime('%Y-%m', 'now')");
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value(0).toLongLong());
    }
    return Money();
}

int Database::getTotalTransactionsToday()
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM sales WHERE DATE(sale_date) = DATE('now')");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QVector<QPair<QString, int>> Database::getTopSellingProducts(int limit)
{
    QVector<QPair<QString, int>> products;
    QSqlQuery query(db);
    query.prepare("SELECT product_name, SUM(quantity) as total_qty FROM sale_items GROUP BY product_name ORDER BY total_qty DESC LIMIT ?");
    query.addBindValue(limit);
    query.exec();
    while (query.next()) {
        products.append(qMakePair(query.value(0).toString(), query.value(1).toInt()));
    }
    return products;
}

bool Database::logStockAdjustment(int productId, const QString &productName,
                                  int oldQty, int newQty,
                                  const QString &reason, const QString &adjustedBy)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO stock_adjustments "
              "(product_id, product_name, change_qty, old_qty, new_qty, reason, adjusted_by) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(productId);
    q.addBindValue(productName);
    q.addBindValue(newQty - oldQty);
    q.addBindValue(oldQty);
    q.addBindValue(newQty);
    q.addBindValue(reason);
    q.addBindValue(adjustedBy);
    return q.exec();
}

QVector<Database::StockAdjustment> Database::getStockHistory(int productId, int limit) const
{
    QVector<StockAdjustment> history;
    QSqlQuery q(db);
    q.prepare("SELECT id, product_id, product_name, change_qty, old_qty, new_qty, "
              "reason, adjusted_by, adjusted_at "
              "FROM stock_adjustments WHERE product_id = ? "
              "ORDER BY adjusted_at DESC LIMIT ?");
    q.addBindValue(productId);
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            StockAdjustment a;
            a.id = q.value(0).toInt();
            a.productId = q.value(1).toInt();
            a.productName = q.value(2).toString();
            a.changeQty = q.value(3).toInt();
            a.oldQty = q.value(4).toInt();
            a.newQty = q.value(5).toInt();
            a.reason = q.value(6).toString();
            a.adjustedBy = q.value(7).toString();
            a.adjustedAt = q.value(8).toString();
            history.append(a);
        }
    }
    return history;
}

bool Database::processRefund(int saleId, const QString &reason, const QString &processedBy)
{
    if (!db.transaction()) {
        lastError = db.lastError().text();
        return false;
    }

    Sale sale = getSaleById(saleId);
    if (sale.id <= 0) {
        db.rollback();
        lastError = QString("Sale #%1 not found").arg(saleId);
        return false;
    }

    // Guard against double-refunds at the DB level: a second refund would
    // insert another refund row AND restore stock again. The check lives
    // inside the transaction so it holds even if a caller forgets to gate it.
    if (isRefunded(saleId)) {
        db.rollback();
        lastError = QString("Sale #%1 has already been refunded").arg(saleId);
        return false;
    }

    // Record refund
    QSqlQuery q(db);
    q.prepare("INSERT INTO refunds (sale_id, total_refunded, reason, processed_by) "
              "VALUES (?, ?, ?, ?)");
    q.addBindValue(saleId);
    q.addBindValue(sale.total.cents());
    q.addBindValue(reason);
    q.addBindValue(processedBy);
    if (!q.exec()) {
        db.rollback();
        lastError = q.lastError().text();
        return false;
    }

    // Return items to stock + log adjustments
    QVector<SaleItem> items = getSaleItems(saleId);
    for (const SaleItem &item : items) {
        if (!increaseStock(item.productId, item.quantity)) {
            db.rollback();
            return false;
        }

        Product p = getProductById(item.productId);
        if (!logStockAdjustment(item.productId, item.productName,
                                p.stockQuantity - item.quantity,
                                p.stockQuantity,
                                QString("Refund for Sale #%1").arg(saleId),
                                processedBy)) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        lastError = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool Database::isRefunded(int saleId) const
{
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM refunds WHERE sale_id = ?");
    q.addBindValue(saleId);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

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

// ==================== Profit calculation methods ====================

Money Database::getActualGrossProfit(const QString &startDate, const QString &endDate)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT SUM((si.price - si.cost_price) * si.quantity) as total_profit "
        "FROM sale_items si "
        "JOIN sales s ON si.sale_id = s.id "
        "WHERE DATE(s.sale_date) BETWEEN ? AND ?");
    query.addBindValue(startDate);
    query.addBindValue(endDate);
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value("total_profit").toLongLong());
    }
    return Money();
}

Money Database::getActualGrossProfitToday()
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT SUM((si.price - si.cost_price) * si.quantity) as total_profit "
        "FROM sale_items si "
        "JOIN sales s ON si.sale_id = s.id "
        "WHERE DATE(s.sale_date) = DATE('now')");
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value("total_profit").toLongLong());
    }
    return Money();
}

Money Database::getActualGrossProfitThisMonth()
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT SUM((si.price - si.cost_price) * si.quantity) as total_profit "
        "FROM sale_items si "
        "JOIN sales s ON si.sale_id = s.id "
        "WHERE strftime('%Y-%m', s.sale_date) = strftime('%Y-%m', 'now')");
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value("total_profit").toLongLong());
    }
    return Money();
}