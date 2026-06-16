// =============================================================================
// inventorymanager.cpp — Implementation of InventoryManager (see
// inventorymanager.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - cachedInventory is filled lazily from Database; getInventoryInfo() reads
//    the product's persisted reorder_level (optimal still defaults to 100).
//  - calculateStatus(qty, reorderLevel) is the single severity rule: OutOfStock
//    at <=0, Critical at <= reorderLevel/5 (min 1), Low at <= reorderLevel.
//  - refreshAfterSale() does NOT touch stock — Database::recordSale() already
//    decremented it inside the checkout transaction; this only re-reads the
//    quantity, syncs the cache, and emits the level-warning signals.
// =============================================================================
#include "inventorymanager.h"
#include "colorscheme.h"
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
    info.reorderLevel = product.reorderLevel;
    info.optimalLevel = 100; // Default
    info.status = calculateStatus(product.stockQuantity, product.reorderLevel);
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
        if (calculateStatus(p.stockQuantity, p.reorderLevel) == InventoryStatus::Low) {
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
        if (calculateStatus(p.stockQuantity, p.reorderLevel) == InventoryStatus::Critical) {
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
    info.reorderLevel = product.reorderLevel;
    if (info.optimalLevel <= 0) info.optimalLevel = 100;
    info.status = calculateStatus(newQty, info.reorderLevel);

    emit inventoryUpdated(productId, newQty);

    switch (info.status) {
    case InventoryStatus::OutOfStock:
        emit inventoryOutOfStock(productId, product.name);
        break;
    case InventoryStatus::Critical:
        emit inventoryCritical(productId, product.name, newQty);
        break;
    case InventoryStatus::Low:
        emit inventoryLow(productId, product.name, newQty);
        break;
    case InventoryStatus::Healthy:
        break;
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
        cachedInventory[productId].reorderLevel = product.reorderLevel;
        cachedInventory[productId].status =
            calculateStatus(product.stockQuantity, product.reorderLevel);
    } else {
        getInventoryInfo(productId);
    }

    emit inventoryRestocked(productId, quantity);
    return true;
}

bool InventoryManager::setReorderLevel(int productId, int level)
{
    if (level < 0) level = 0;
    if (!Database::instance().setReorderLevel(productId, level))
        return false;

    if (cachedInventory.contains(productId)) {
        InventoryInfo &info = cachedInventory[productId];
        info.reorderLevel = level;
        info.status = calculateStatus(info.currentQuantity, level);
    }
    return true;
}

QString InventoryManager::getStatusColor(InventoryStatus status) const
{
    // Pulled from the active theme's dedicated stock-health colours (not the
    // warning/error semantic colours) so status pills stay legible across
    // every theme and don't collide visually with danger/warning buttons.
    const ColorScheme &scheme = getColorScheme();
    switch (status) {
    case InventoryStatus::Healthy:
        return scheme.statusHealthy;
    case InventoryStatus::Low:
        return scheme.statusLow;
    case InventoryStatus::Critical:
        return scheme.statusCritical;
    case InventoryStatus::OutOfStock:
        return scheme.statusOutOfStock;
    default:
        return scheme.info;
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

InventoryStatus InventoryManager::calculateStatus(int quantity, int reorderLevel)
{
    if (quantity <= 0) return InventoryStatus::OutOfStock;
    if (reorderLevel <= 0) return InventoryStatus::Healthy;  // alerts disabled
    const int critical = qMax(1, reorderLevel / 5);
    if (quantity <= critical)     return InventoryStatus::Critical;
    if (quantity <= reorderLevel) return InventoryStatus::Low;
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
        info.reorderLevel = p.reorderLevel;
        info.optimalLevel = 100;
        info.status = calculateStatus(p.stockQuantity, p.reorderLevel);
        info.lastRestockDate = "";
        info.supplierName = "";

        cachedInventory[info.productId] = info;
    }

    qDebug() << "Loaded" << cachedInventory.size() << "items into inventory cache";
}
