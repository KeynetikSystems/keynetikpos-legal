// =============================================================================
// purchaseorderrepository.h — PurchaseOrderRepository: purchasing, split out of
// Database
// -----------------------------------------------------------------------------
// WHAT: Owns the purchase_orders / purchase_order_items tables: create (atomic
//       header + lines), the list/by-supplier/by-id/items queries, the atomic
//       receive (stock + cost-basis update + audit log + status flip), and
//       cancel. PurchaseOrder/PurchaseOrderItem live in database.h.
// HOW:  Plain class over a QSqlDatabase held BY VALUE. receivePurchaseOrder()
//       runs in one transaction and reuses ProductRepository (constructed on
//       the same shared connection) for the per-line stock-adjustment audit
//       rows, so the audit logic lives in exactly one place.
// WHY:  Continues the Database "god object" breakup. Database keeps the public
//       methods and delegates here.
// =============================================================================
#ifndef PURCHASEORDERREPOSITORY_H
#define PURCHASEORDERREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "database.h"   // PurchaseOrder, PurchaseOrderItem

class PurchaseOrderRepository
{
public:
    explicit PurchaseOrderRepository(QSqlDatabase db);

    int createPurchaseOrder(int supplierId, const QVector<PurchaseOrderItem> &items,
                            const QString &notes, const QString &createdBy);
    QVector<PurchaseOrder> getAllPurchaseOrders();
    QVector<PurchaseOrder> getPurchaseOrdersBySupplier(int supplierId);
    PurchaseOrder getPurchaseOrderById(int id);
    QVector<PurchaseOrderItem> getPurchaseOrderItems(int poId);
    bool receivePurchaseOrder(int poId, const QString &receivedBy);
    bool cancelPurchaseOrder(int poId);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    QString         m_lastError;
};

#endif // PURCHASEORDERREPOSITORY_H
