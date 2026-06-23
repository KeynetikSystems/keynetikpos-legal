// =============================================================================
// purchaseorderrepository.cpp — Implementation of PurchaseOrderRepository (see
// purchaseorderrepository.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Bodies were lifted verbatim from Database (db -> m_db, lastError ->
// m_lastError). The one behavioural seam: receivePurchaseOrder's per-line
// audit log now goes through ProductRepository::logStockAdjustment on the same
// connection/transaction instead of a Database member call.
// =============================================================================
#include "purchaseorderrepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "money.h"
#include "productrepository.h"

PurchaseOrderRepository::PurchaseOrderRepository(QSqlDatabase db)
    : m_db(db)
{
}

int PurchaseOrderRepository::createPurchaseOrder(int supplierId,
                                                 const QVector<PurchaseOrderItem> &items,
                                                 const QString &notes, const QString &createdBy)
{
    if (items.isEmpty()) {
        m_lastError = "Cannot create a purchase order with no items";
        return -1;
    }

    if (!m_db.transaction()) {
        m_lastError = "Failed to start transaction: " + m_db.lastError().text();
        return -1;
    }

    qint64 totalCents = 0;
    for (const PurchaseOrderItem &item : items)
        totalCents += item.subtotal.cents();

    QSqlQuery poQuery(m_db);
    poQuery.prepare("INSERT INTO purchase_orders (supplier_id, status, notes, created_by, total) "
                    "VALUES (?, 'Pending', ?, ?, ?)");
    poQuery.addBindValue(supplierId);
    poQuery.addBindValue(notes);
    poQuery.addBindValue(createdBy);
    poQuery.addBindValue(totalCents);
    if (!poQuery.exec()) {
        m_lastError = "Failed to create purchase order: " + poQuery.lastError().text();
        m_db.rollback();
        return -1;
    }
    const int poId = poQuery.lastInsertId().toInt();

    for (const PurchaseOrderItem &item : items) {
        QSqlQuery itemQuery(m_db);
        itemQuery.prepare("INSERT INTO purchase_order_items "
                          "(po_id, product_id, product_name, quantity, unit_cost, subtotal) "
                          "VALUES (?, ?, ?, ?, ?, ?)");
        itemQuery.addBindValue(poId);
        itemQuery.addBindValue(item.productId);
        itemQuery.addBindValue(item.productName);
        itemQuery.addBindValue(item.quantity);
        itemQuery.addBindValue(item.unitCost.cents());
        itemQuery.addBindValue(item.subtotal.cents());
        if (!itemQuery.exec()) {
            m_lastError = "Failed to add purchase order item: " + itemQuery.lastError().text();
            m_db.rollback();
            return -1;
        }
    }

    if (!m_db.commit()) {
        m_lastError = "Failed to commit purchase order: " + m_db.lastError().text();
        m_db.rollback();
        return -1;
    }
    return poId;
}

// Shared row->struct mapping for the two list queries below.
static PurchaseOrder poFromQuery(QSqlQuery &query)
{
    PurchaseOrder po;
    po.id           = query.value(0).toInt();
    po.supplierId   = query.value(1).toInt();
    po.supplierName = query.value(2).toString();
    po.status       = query.value(3).toString();
    po.orderDate    = query.value(4).toString();
    po.receivedDate = query.value(5).toString();
    po.notes        = query.value(6).toString();
    po.createdBy    = query.value(7).toString();
    po.total        = Money::fromCents(query.value(8).toLongLong());
    return po;
}

QVector<PurchaseOrder> PurchaseOrderRepository::getAllPurchaseOrders()
{
    QVector<PurchaseOrder> orders;
    QSqlQuery query(m_db);
    query.exec("SELECT po.id, po.supplier_id, s.name, po.status, po.order_date, "
              "po.received_date, po.notes, po.created_by, po.total "
              "FROM purchase_orders po JOIN suppliers s ON s.id = po.supplier_id "
              "ORDER BY po.order_date DESC");
    while (query.next())
        orders.append(poFromQuery(query));
    return orders;
}

QVector<PurchaseOrder> PurchaseOrderRepository::getPurchaseOrdersBySupplier(int supplierId)
{
    QVector<PurchaseOrder> orders;
    QSqlQuery query(m_db);
    query.prepare("SELECT po.id, po.supplier_id, s.name, po.status, po.order_date, "
                  "po.received_date, po.notes, po.created_by, po.total "
                  "FROM purchase_orders po JOIN suppliers s ON s.id = po.supplier_id "
                  "WHERE po.supplier_id = ? ORDER BY po.order_date DESC");
    query.addBindValue(supplierId);
    if (query.exec()) {
        while (query.next())
            orders.append(poFromQuery(query));
    }
    return orders;
}

PurchaseOrder PurchaseOrderRepository::getPurchaseOrderById(int id)
{
    PurchaseOrder po;
    QSqlQuery query(m_db);
    query.prepare("SELECT po.id, po.supplier_id, s.name, po.status, po.order_date, "
                  "po.received_date, po.notes, po.created_by, po.total "
                  "FROM purchase_orders po JOIN suppliers s ON s.id = po.supplier_id "
                  "WHERE po.id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next())
        po = poFromQuery(query);
    return po;
}

QVector<PurchaseOrderItem> PurchaseOrderRepository::getPurchaseOrderItems(int poId)
{
    QVector<PurchaseOrderItem> items;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, po_id, product_id, product_name, quantity, unit_cost, subtotal "
                  "FROM purchase_order_items WHERE po_id = ?");
    query.addBindValue(poId);
    if (query.exec()) {
        while (query.next()) {
            PurchaseOrderItem item;
            item.id          = query.value(0).toInt();
            item.poId        = query.value(1).toInt();
            item.productId   = query.value(2).toInt();
            item.productName = query.value(3).toString();
            item.quantity    = query.value(4).toInt();
            item.unitCost     = Money::fromCents(query.value(5).toLongLong());
            item.subtotal     = Money::fromCents(query.value(6).toLongLong());
            items.append(item);
        }
    }
    return items;
}

bool PurchaseOrderRepository::receivePurchaseOrder(int poId, const QString &receivedBy)
{
    if (!m_db.transaction()) {
        m_lastError = "Failed to start transaction: " + m_db.lastError().text();
        return false;
    }

    QSqlQuery statusQuery(m_db);
    statusQuery.prepare("SELECT status FROM purchase_orders WHERE id = ?");
    statusQuery.addBindValue(poId);
    if (!statusQuery.exec() || !statusQuery.next()) {
        m_lastError = "Purchase order not found";
        m_db.rollback();
        return false;
    }
    if (statusQuery.value(0).toString() != "Pending") {
        m_lastError = "Purchase order is not pending — already received or cancelled";
        m_db.rollback();
        return false;
    }

    QSqlQuery itemsQuery(m_db);
    itemsQuery.prepare("SELECT product_id, product_name, quantity, unit_cost "
                       "FROM purchase_order_items WHERE po_id = ?");
    itemsQuery.addBindValue(poId);
    if (!itemsQuery.exec()) {
        m_lastError = "Failed to read purchase order items: " + itemsQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    struct Line { int productId; QString name; int qty; qint64 unitCostCents; };
    QVector<Line> lines;
    while (itemsQuery.next()) {
        lines.append({ itemsQuery.value(0).toInt(), itemsQuery.value(1).toString(),
                       itemsQuery.value(2).toInt(), itemsQuery.value(3).toLongLong() });
    }

    ProductRepository products(m_db);
    for (const Line &line : lines) {
        QSqlQuery stockQuery(m_db);
        stockQuery.prepare("SELECT stock_quantity FROM products WHERE id = ?");
        stockQuery.addBindValue(line.productId);
        if (!stockQuery.exec() || !stockQuery.next()) {
            m_lastError = "Product not found: " + line.name;
            m_db.rollback();
            return false;
        }
        const int oldQty = stockQuery.value(0).toInt();
        const int newQty = oldQty + line.qty;

        // Receiving updates stock AND the product's cost basis to the PO's
        // unit cost (latest-cost, not weighted-average — simple and matches
        // how the rest of the app treats cost_price as "current cost").
        QSqlQuery updateQuery(m_db);
        updateQuery.prepare("UPDATE products SET stock_quantity = ?, cost_price = ? WHERE id = ?");
        updateQuery.addBindValue(newQty);
        updateQuery.addBindValue(line.unitCostCents);
        updateQuery.addBindValue(line.productId);
        if (!updateQuery.exec()) {
            m_lastError = "Failed to update stock for " + line.name + ": " + updateQuery.lastError().text();
            m_db.rollback();
            return false;
        }

        if (!products.logStockAdjustment(line.productId, line.name, oldQty, newQty,
                                         QString("PO #%1 Receipt").arg(poId), receivedBy)) {
            m_lastError = "Failed to log stock adjustment for " + line.name;
            m_db.rollback();
            return false;
        }
    }

    QSqlQuery finishQuery(m_db);
    finishQuery.prepare("UPDATE purchase_orders SET status = 'Received', "
                        "received_date = CURRENT_TIMESTAMP WHERE id = ?");
    finishQuery.addBindValue(poId);
    if (!finishQuery.exec()) {
        m_lastError = "Failed to finalize purchase order: " + finishQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        m_lastError = "Failed to commit purchase order receipt: " + m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool PurchaseOrderRepository::cancelPurchaseOrder(int poId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE purchase_orders SET status = 'Cancelled' WHERE id = ? AND status = 'Pending'");
    query.addBindValue(poId);
    if (!query.exec()) {
        m_lastError = "Failed to cancel purchase order: " + query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() == 0) {
        m_lastError = "Purchase order is not pending — cannot cancel";
        return false;
    }
    return true;
}
