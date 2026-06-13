// =============================================================================
// inventorymanager.h — InventoryManager: stock intelligence above raw numbers
// -----------------------------------------------------------------------------
// WHAT: Classifies each product into InventoryStatus (Healthy / Low / Critical
//       / OutOfStock), caches per-product InventoryInfo, answers canSell(),
//       lists low/critical items, performs restocks, and emits signals
//       (inventoryLow/Critical/OutOfStock/Restocked) when levels change.
// HOW:  Keeps a QMap<productId, InventoryInfo> cache loaded lazily from
//       Database; a QTimer (default 60 s) drives periodic level re-scans.
//       refreshAfterSale() deliberately does NOT decrement stock — that already
//       happened inside Database::recordSale()'s transaction — it only re-reads
//       the new quantity, syncs the cache, and fires threshold signals.
// WHY:  The signal/slot design decouples detection from presentation: the main
//       window shows toasts, InventoryDialog updates alert tables, and future
//       listeners (e.g. scheduled SMS alerts) can subscribe without touching
//       this class. Keeping the decrement in the DB transaction and only the
//       *notification* here avoids double-decrement bugs.
// =============================================================================
#ifndef INVENTORYMANAGER_H
#define INVENTORYMANAGER_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QVector>
#include "database.h"

enum class InventoryStatus {
    Healthy,
    Low,
    Critical,
    OutOfStock
};

struct InventoryInfo {
    int productId = 0;
    QString productName;
    QString category;
    int currentQuantity = 0;
    int reorderLevel = 0;
    int optimalLevel = 0;
    InventoryStatus status = InventoryStatus::OutOfStock;
    QString lastRestockDate;
    QString supplierName;
};

class InventoryManager : public QObject
{
    Q_OBJECT

public:
    InventoryManager();
    ~InventoryManager();

    // Query methods
    InventoryInfo getInventoryInfo(int productId);
    bool canSell(int productId, int quantity);
    QVector<InventoryInfo> getLowStockItems();
    QVector<InventoryInfo> getCriticalStockItems();

    // Update methods
    // Sync the cache + emit warnings after Database::recordSale decremented stock
    void refreshAfterSale(int productId);
    bool restockProduct(int productId, int quantity, const QString &supplierName = "");
    bool setReorderLevel(int productId, int level);

    // Utility
    QString getStatusColor(InventoryStatus status) const;
    QString getStatusText(InventoryStatus status) const;
    InventoryStatus calculateStatus(int quantity);  // Made public for InventoryDialog

    // Auto-refresh
    void startAutoRefresh(int intervalMs = 60000);
    void stopAutoRefresh();

signals:
    void inventoryUpdated(int productId, int newQuantity);
    void inventoryLow(int productId, const QString &productName, int quantity);
    void inventoryCritical(int productId, const QString &productName, int quantity);
    void inventoryOutOfStock(int productId, const QString &productName);
    void inventoryRestocked(int productId, int quantity);

private slots:
    void checkAllInventoryLevels();

private:
    void loadInventoryCache();

    QMap<int, InventoryInfo> cachedInventory;
    QTimer *refreshTimer;
};

#endif // INVENTORYMANAGER_H
