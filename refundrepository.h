// =============================================================================
// refundrepository.h — RefundRepository: the refund use case, split out of
// Database
// -----------------------------------------------------------------------------
// WHAT: Owns the refunds table and the processRefund() flow: validate the sale
//       exists and isn't already refunded, insert the refund row, return each
//       line's quantity to stock, and log a stock adjustment per line — all in
//       one transaction. isRefunded() is the double-refund guard.
// HOW:  Plain class over a QSqlDatabase held BY VALUE. processRefund is a
//       cross-aggregate orchestration, so it reuses SaleRepository (to read the
//       sale + items) and ProductRepository (to restock + audit) on the same
//       shared connection, inside its own transaction.
// WHY:  Continues the Database "god object" breakup. Database keeps the public
//       methods and delegates here.
// =============================================================================
#ifndef REFUNDREPOSITORY_H
#define REFUNDREPOSITORY_H

#include <QSqlDatabase>
#include <QString>

#include "database.h"

class RefundRepository
{
public:
    explicit RefundRepository(QSqlDatabase db);

    bool processRefund(int saleId, const QString &reason, const QString &processedBy);
    bool isRefunded(int saleId) const;

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    QString         m_lastError;
};

#endif // REFUNDREPOSITORY_H
