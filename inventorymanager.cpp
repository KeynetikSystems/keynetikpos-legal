// =============================================================================
// inventorymanager.cpp — Implementation of InventoryManager (see
// inventorymanager.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - cachedInventory is filled lazily from Database; getInventoryInfo() falls
//    back to a DB read with default reorder (20) / optimal (100) levels.
//  - Status thresholds: Critical <= 4, Low <= 50, else Healthy / OutOfStock.
//  - refreshAfterSale() does NOT touch stock — Database::recordSale() already
//    decremented it inside the checkout transaction; this only re-reads the
//    quantity, syncs the cache, and emits the level-warning signals.
// =============================================================================
#include "inventorymanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

InventoryManager::InventoryManager()
    : QObject(nullptr)
    , refreshTimer(new QTimer(this))
{
    loadInventoryCache();

    connect(refreshTimer, &QTimer::timeout, this, &InventoryManager::checkAllInventoryLevels);
}

InventoryManager::~InventoryManager()
{
}

InventoryInfo InventoryManager::getInventoryInfo(int productId)
{
    if (cachedInventory.contains(productId)) {
        return cachedInventory[productId];
    }

    // Load from database
    Product product = Database::instance().getProductById(productId);

    InventoryInfo info;
    info.productId = productId;
    info.productName = product.name;
    info.category = product.category;
    info.currentQuantity = product.stockQuantity;
    info.reorderLevel = 20; // Default
    info.optimalLevel = 100; // Default
    info.status = calculateStatus(product.stockQuantity);
    info.lastRestockDate = "";
    info.supplierName = "";

    cachedInventory[productId] = info;
    return info;
}

bool InventoryManager::canSell(int productId, int quantity)
{
    InventoryInfo info = getInventoryInfo(productId);
    return info.currentQuantity >= quantity;
}

QVector<InventoryInfo> InventoryManager::getLowStockItems()
{
    QVector<InventoryInfo> lowItems;

    QVector<Product> products = Database::instance().getAllProducts();
    for (const Product &p : products) {
        if (p.stockQuantity > 4 && p.stockQuantity <= 50) {
            InventoryInfo info = getInventoryInfo(p.id);
            lowItems.append(info);
        }
    }

    return lowItems;
}

QVector<InventoryInfo> InventoryManager::getCriticalStockItems()
{
    QVector<InventoryInfo> criticalItems;

    QVector<Product> products = Database::instance().getAllProducts();
    for (const Product &p : products) {
        if (p.stockQuantity <= 4) {
            InventoryInfo info = getInventoryInfo(p.id);
            criticalItems.append(info);
        }
    }

    return criticalItems;
}

void InventoryManager::refreshAfterSale(int productId)
{
    // Stock is decremented inside the checkout transaction (Database::recordSale);
    // here we only sync the cache and emit level warnings.
    Product product = Database::instance().getProductById(productId);
    if (product.id <= 0)
        return;

    const int newQty = product.stockQuantity;

    InventoryInfo &info = cachedInventory[productId];
    info.productId = productId;
    info.productName = product.name;
    info.category = product.category;
    info.currentQuantity = newQty;
    if (info.reorderLevel <= 0) info.reorderLevel = 20;
    if (info.optimalLevel <= 0) info.optimalLevel = 100;
    info.status = calculateStatus(newQty);

    emit inventoryUpdated(productId, newQty);

    if (newQty == 0) {
        emit inventoryOutOfStock(productId, product.name);
    } else if (newQty <= 4) {
        emit inventoryCritical(productId, product.name, newQty);
    } else if (newQty <= 20) {
        emit inventoryLow(productId, product.name, newQty);
    }
}

bool InventoryManager::restockProduct(int productId, int quantity, const QString &supplierName)
{
    // Update in database
    QSqlQuery query;
    query.prepare("UPDATE products SET stock_quantity = stock_quantity + ? WHERE id = ?");
    query.addBindValue(quantity);
    query.addBindValue(productId);

    if (!query.exec()) {
        qDebug() << "Error restocking inventory:" << query.lastError().text();
        return false;
    }

    // Update cache
    Product product = Database::instance().getProductById(productId);
    if (cachedInventory.contains(productId)) {
        cachedInventory[productId].currentQuantity = product.stockQuantity;
        cachedInventory[productId].lastRestockDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        cachedInventory[productId].supplierName = supplierName;
        cachedInventory[productId].status = calculateStatus(product.stockQuantity);
    } else {
        getInventoryInfo(productId);
    }

    emit inventoryRestocked(productId, quantity);
    return true;
}

bool InventoryManager::setReorderLevel(int productId, int level)
{
    if (cachedInventory.contains(productId)) {
        cachedInventory[productId].reorderLevel = level;
        return true;
    }
    return false;
}

QString InventoryManager::getStatusColor(InventoryStatus status) const
{
    switch (status) {
    case InventoryStatus::Healthy:
        return "#27AE60"; // Green
    case InventoryStatus::Low:
        return "#F39C12"; // Orange
    case InventoryStatus::Critical:
        return "#E74C3C"; // Red
    case InventoryStatus::OutOfStock:
        return "#95A5A6"; // Gray
    default:
        return "#3498DB"; // Blue
    }
}

QString InventoryManager::getStatusText(InventoryStatus status) const
{
    switch (status) {
    case InventoryStatus::Healthy:
        return "Healthy";
    case InventoryStatus::Low:
        return "Low Stock";
    case InventoryStatus::Critical:
        return "Critical";
    case InventoryStatus::OutOfStock:
        return "Out of Stock";
    default:
        return "Unknown";
    }
}

void InventoryManager::startAutoRefresh(int intervalMs)
{
    refreshTimer->start(intervalMs);
    checkAllInventoryLevels(); // Check immediately
}

void InventoryManager::stopAutoRefresh()
{
    refreshTimer->stop();
}

void InventoryManager::checkAllInventoryLevels()
{
    loadInventoryCache();

    // Check for low/critical items and emit signals
    for (const auto &info : cachedInventory) {
        if (info.status == InventoryStatus::OutOfStock) {
            emit inventoryOutOfStock(info.productId, info.productName);
        } else if (info.status == InventoryStatus::Critical) {
            emit inventoryCritical(info.productId, info.productName, info.currentQuantity);
        } else if (info.status == InventoryStatus::Low) {
            emit inventoryLow(info.productId, info.productName, info.currentQuantity);
        }
    }
}

InventoryStatus InventoryManager::calculateStatus(int quantity)
{
    if (quantity == 0) return InventoryStatus::OutOfStock;
    if (quantity <= 4) return InventoryStatus::Critical;
    if (quantity <= 20) return InventoryStatus::Low;
    return InventoryStatus::Healthy;
}

void InventoryManager::loadInventoryCache()
{
    cachedInventory.clear();

    QVector<Product> products = Database::instance().getAllProducts();
    for (const Product &p : products) {
        InventoryInfo info;
        info.productId = p.id;
        info.productName = p.name;
        info.category = p.category;
        info.currentQuantity = p.stockQuantity;
        info.reorderLevel = 20;
        info.optimalLevel = 100;
        info.status = calculateStatus(p.stockQuantity);
        info.lastRestockDate = "";
        info.supplierName = "";

        cachedInventory[info.productId] = info;
    }

    qDebug() << "Loaded" << cachedInventory.size() << "items into inventory cache";
}
