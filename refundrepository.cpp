// =============================================================================
// refundrepository.cpp — Implementation of RefundRepository (see
// refundrepository.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Body lifted verbatim from Database (db -> m_db, lastError -> m_lastError).
// The internal getSaleById/getSaleItems calls now go through SaleRepository,
// and increaseStock/getProductById/logStockAdjustment through ProductRepository
// — all on the same connection/transaction.
// =============================================================================
#include "refundrepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "salerepository.h"
#include "productrepository.h"

RefundRepository::RefundRepository(QSqlDatabase db)
    : m_db(db)
{
}

bool RefundRepository::processRefund(int saleId, const QString &reason, const QString &processedBy)
{
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    SaleRepository sales(m_db);
    ProductRepository products(m_db);

    Sale sale = sales.getSaleById(saleId);
    if (sale.id <= 0) {
        m_db.rollback();
        m_lastError = QString("Sale #%1 not found").arg(saleId);
        return false;
    }

    // Guard against double-refunds at the DB level: a second refund would
    // insert another refund row AND restore stock again. The check lives
    // inside the transaction so it holds even if a caller forgets to gate it.
    if (isRefunded(saleId)) {
        m_db.rollback();
        m_lastError = QString("Sale #%1 has already been refunded").arg(saleId);
        return false;
    }

    // Record refund
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO refunds (sale_id, total_refunded, reason, processed_by) "
              "VALUES (?, ?, ?, ?)");
    q.addBindValue(saleId);
    q.addBindValue(sale.total.cents());
    q.addBindValue(reason);
    q.addBindValue(processedBy);
    if (!q.exec()) {
        m_db.rollback();
        m_lastError = q.lastError().text();
        return false;
    }

    // Return items to stock + log adjustments
    QVector<SaleItem> items = sales.getSaleItems(saleId);
    for (const SaleItem &item : items) {
        if (!products.increaseStock(item.productId, item.quantity)) {
            m_db.rollback();
            return false;
        }

        Product p = products.getProductById(item.productId);
        if (!products.logStockAdjustment(item.productId, item.productName,
                                         p.stockQuantity - item.quantity,
                                         p.stockQuantity,
                                         QString("Refund for Sale #%1").arg(saleId),
                                         processedBy)) {
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool RefundRepository::isRefunded(int saleId) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM refunds WHERE sale_id = ?");
    q.addBindValue(saleId);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}
