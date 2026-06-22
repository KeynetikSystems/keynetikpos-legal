// =============================================================================
// salerepository.h — SaleRepository: the sales aggregate, split out of Database
// -----------------------------------------------------------------------------
// WHAT: Owns the read/write SQL for the sales + sale_items + sale_payments
//       tables: recordSale() (the atomic checkout write) and the Sale-row
//       queries (all / by date range / by id / by customer) plus the line-item
//       fetch. The data structs themselves (Sale/SaleItem/SaleRequest/
//       SalePayment) still live in database.h.
// HOW:  Plain class over a QSqlDatabase held BY VALUE — the same pattern as
//       Ledger. A QSqlDatabase is a cheap, ref-counted handle to the one open
//       connection, so the repository shares Database's connection (and any
//       in-flight transaction semantics) without owning or reopening it.
//       lastError() mirrors Database's convention.
// WHY:  First step of breaking up the Database "God object": the sales path is
//       the most cohesive slice and now has a clean seam (recordSale takes a
//       SaleRequest). Database keeps the same public methods but delegates to
//       this class, so the ~120 existing call sites are untouched while the
//       money-moving logic becomes a focused, separately-reviewable unit.
// =============================================================================
#ifndef SALEREPOSITORY_H
#define SALEREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QDate>

#include "database.h"   // Sale, SaleItem, SaleRequest, SalePayment, Money

class SaleRepository
{
public:
    // Holds the connection handle by value (cheap, ref-counted) so it stays
    // valid even if the caller's QSqlDatabase local goes out of scope.
    explicit SaleRepository(QSqlDatabase db);

    // Atomically validates stock, inserts the sale + items, decrements stock,
    // and — when customerId > 0 — awards loyalty points + deducts any applied
    // store credit, all in one transaction. Returns the new sale id, or -1
    // (see lastError()).
    int recordSale(const SaleRequest &request);

    QVector<Sale> getAllSales();
    QVector<Sale> getSalesByDateRange(const QDate &startDate, const QDate &endDate);
    QVector<SaleItem> getSaleItems(int saleId);
    Sale getSaleById(int saleId);
    QVector<Sale> getCustomerPurchaseHistory(int customerId);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    QString         m_lastError;
};

#endif // SALEREPOSITORY_H
