// =============================================================================
// productrepository.cpp — Implementation of ProductRepository (see
// productrepository.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Bodies were lifted verbatim from Database (db -> m_db, lastError ->
// m_lastError). adjustStockWithLog()'s internal getProductById/updateStock/
// logStockAdjustment calls are now member calls on this class.
// =============================================================================
#include "productrepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "money.h"

ProductRepository::ProductRepository(QSqlDatabase db)
    : m_db(db)
{
}

QVector<Product> ProductRepository::getAllProducts()
{
    QVector<Product> products;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, category, price, cost_price, profit_margin, stock_quantity, reorder_level, barcode, is_active "
                  "FROM products "
                  "WHERE is_active = 1 "
                  "ORDER BY name ASC");
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

QVector<Product> ProductRepository::getProductsByCategory(const QString &category)
{
    QVector<Product> products;
    QSqlQuery query(m_db);
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

Product ProductRepository::getProductById(int id)
{
    Product p;
    QSqlQuery query(m_db);
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

Product ProductRepository::getProductByBarcode(const QString &barcode)
{
    Product p;
    QSqlQuery query(m_db);
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

bool ProductRepository::addProduct(const Product &product)
{
    QSqlQuery query(m_db);
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
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool ProductRepository::updateProduct(const Product &product)
{
    QSqlQuery query(m_db);
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
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool ProductRepository::deleteProduct(int id)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE products SET is_active = 0 WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

QStringList ProductRepository::getAllCategories()
{
    QStringList categories;
    QSqlQuery query(m_db);
    query.prepare("SELECT DISTINCT category FROM products WHERE is_active = 1 ORDER BY category");
    query.exec();
    while (query.next()) {
        categories.append(query.value(0).toString());
    }
    return categories;
}

bool ProductRepository::ensureCategory(const QString &name)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO categories (name) VALUES (?)");
    query.addBindValue(name);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool ProductRepository::updateStock(int productId, int newQuantity)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE products SET stock_quantity = ? WHERE id = ?");
    query.addBindValue(newQuantity);
    query.addBindValue(productId);
    return query.exec();
}

bool ProductRepository::setReorderLevel(int productId, int level)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE products SET reorder_level = ? WHERE id = ?");
    query.addBindValue(level);
    query.addBindValue(productId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool ProductRepository::decreaseStock(int productId, int quantity)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE products SET stock_quantity = stock_quantity - ? WHERE id = ?");
    query.addBindValue(quantity);
    query.addBindValue(productId);
    return query.exec();
}

bool ProductRepository::increaseStock(int productId, int quantity)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE products SET stock_quantity = stock_quantity + ? WHERE id = ?");
    query.addBindValue(quantity);
    query.addBindValue(productId);
    return query.exec();
}

int ProductRepository::getStock(int productId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT stock_quantity FROM products WHERE id = ?");
    query.addBindValue(productId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool ProductRepository::adjustStockWithLog(int productId, int qtyChange,
                                           const QString &reason, const QString &adjustedBy)
{
    if (!m_db.transaction()) return false;

    Product p = getProductById(productId);
    int oldQty = p.stockQuantity;
    int newQty = oldQty + qtyChange;

    if (!updateStock(productId, newQty)) {
        m_db.rollback();
        return false;
    }

    if (!logStockAdjustment(productId, p.name, oldQty, newQty, reason, adjustedBy)) {
        m_db.rollback();
        return false;
    }

    return m_db.commit();
}

bool ProductRepository::logStockAdjustment(int productId, const QString &productName,
                                           int oldQty, int newQty,
                                           const QString &reason, const QString &adjustedBy)
{
    QSqlQuery q(m_db);
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

QVector<Database::StockAdjustment> ProductRepository::getStockHistory(int productId, int limit) const
{
    QVector<Database::StockAdjustment> history;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, product_id, product_name, change_qty, old_qty, new_qty, "
              "reason, adjusted_by, adjusted_at "
              "FROM stock_adjustments WHERE product_id = ? "
              "ORDER BY adjusted_at DESC LIMIT ?");
    q.addBindValue(productId);
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            Database::StockAdjustment a;
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

QVector<Database::StockValuationRow> ProductRepository::getStockValuation()
{
    QVector<Database::StockValuationRow> rows;
    QSqlQuery q(m_db);
    q.prepare(
        "SELECT name, category, stock_quantity, cost_price "
        "FROM products WHERE is_active = 1 ORDER BY category, name");
    if (!q.exec()) return rows;
    while (q.next()) {
        Database::StockValuationRow r;
        r.productName = q.value(0).toString();
        r.category    = q.value(1).toString();
        r.qty         = q.value(2).toInt();
        r.costPrice   = Money::fromCents(q.value(3).toLongLong());
        rows.append(r);
    }
    return rows;
}
