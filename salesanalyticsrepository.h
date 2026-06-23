// =============================================================================
// salesanalyticsrepository.h — SalesAnalyticsRepository: reporting queries,
// split out of Database
// -----------------------------------------------------------------------------
// WHAT: Read-only aggregate queries over sales / sale_items / expenses: today's
//       and this-month's takings, transaction count, top sellers, per-method
//       payment totals, actual gross profit (range / today / month), and the
//       per-day profit-and-loss rollup. These are pure reads — no writes, no
//       lastError.
// HOW:  Plain class over a QSqlDatabase held BY VALUE. Returns the row structs
//       still nested in Database (PaymentTotal, ProfitLossRow).
// WHY:  Continues the Database "god object" breakup. Database keeps the public
//       methods and delegates here.
// =============================================================================
#ifndef SALESANALYTICSREPOSITORY_H
#define SALESANALYTICSREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QPair>
#include <QDate>

#include "database.h"   // PaymentTotal, Database::ProfitLossRow, Money

class SalesAnalyticsRepository
{
public:
    explicit SalesAnalyticsRepository(QSqlDatabase db);

    Money getTotalSalesToday();
    Money getTotalSalesThisMonth();
    int getTotalTransactionsToday();
    QVector<QPair<QString, int>> getTopSellingProducts(int limit);
    QVector<PaymentTotal> getPaymentTotalsByMethod(const QDate &startDate, const QDate &endDate);

    Money getActualGrossProfit(const QDate &startDate, const QDate &endDate);
    Money getActualGrossProfitToday();
    Money getActualGrossProfitThisMonth();

    QVector<Database::ProfitLossRow> getProfitLossByDateRange(const QDate &start, const QDate &end);

private:
    QSqlDatabase    m_db;
};

#endif // SALESANALYTICSREPOSITORY_H
