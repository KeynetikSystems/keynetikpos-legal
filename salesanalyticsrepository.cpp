// =============================================================================
// salesanalyticsrepository.cpp — Implementation of SalesAnalyticsRepository
// (see salesanalyticsrepository.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Bodies lifted verbatim from Database (db -> m_db); these are pure reads.
// =============================================================================
#include "salesanalyticsrepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QMap>

#include "money.h"

SalesAnalyticsRepository::SalesAnalyticsRepository(QSqlDatabase db)
    : m_db(db)
{
}

Money SalesAnalyticsRepository::getTotalSalesToday()
{
    QSqlQuery query(m_db);
    query.prepare("SELECT SUM(total) FROM sales WHERE DATE(sale_date) = DATE('now')");
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value(0).toLongLong());
    }
    return Money();
}

Money SalesAnalyticsRepository::getTotalSalesThisMonth()
{
    QSqlQuery query(m_db);
    query.prepare("SELECT SUM(total) FROM sales WHERE strftime('%Y-%m', sale_date) = strftime('%Y-%m', 'now')");
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value(0).toLongLong());
    }
    return Money();
}

int SalesAnalyticsRepository::getTotalTransactionsToday()
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM sales WHERE DATE(sale_date) = DATE('now')");
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QVector<QPair<QString, int>> SalesAnalyticsRepository::getTopSellingProducts(int limit)
{
    QVector<QPair<QString, int>> products;
    QSqlQuery query(m_db);
    query.prepare("SELECT product_name, SUM(quantity) as total_qty FROM sale_items GROUP BY product_name ORDER BY total_qty DESC LIMIT ?");
    query.addBindValue(limit);
    query.exec();
    while (query.next()) {
        products.append(qMakePair(query.value(0).toString(), query.value(1).toInt()));
    }
    return products;
}

QVector<PaymentTotal> SalesAnalyticsRepository::getPaymentTotalsByMethod(const QDate &startDateArg,
                                                                         const QDate &endDateArg)
{
    const QString startDate = startDateArg.toString(Qt::ISODate);
    const QString endDate   = endDateArg.toString(Qt::ISODate);
    QVector<PaymentTotal> totals;
    QSqlQuery query(m_db);
    // Prefer the per-tender rows; for sales recorded before sale_payments
    // existed (no rows), fall back to the sale's own payment_method/total so no
    // history is lost.
    query.prepare(
        "SELECT method, SUM(amount) AS total, COUNT(*) AS cnt FROM ("
        "  SELECT sp.method AS method, sp.amount AS amount "
        "    FROM sale_payments sp JOIN sales s ON sp.sale_id = s.id "
        "    WHERE DATE(s.sale_date) BETWEEN ? AND ? "
        "  UNION ALL "
        "  SELECT s.payment_method AS method, s.total AS amount "
        "    FROM sales s "
        "    WHERE DATE(s.sale_date) BETWEEN ? AND ? "
        "      AND NOT EXISTS (SELECT 1 FROM sale_payments sp2 WHERE sp2.sale_id = s.id)"
        ") GROUP BY method ORDER BY total DESC");
    query.addBindValue(startDate);
    query.addBindValue(endDate);
    query.addBindValue(startDate);
    query.addBindValue(endDate);
    if (query.exec()) {
        while (query.next()) {
            PaymentTotal t;
            t.method = query.value(0).toString();
            t.total  = Money::fromCents(query.value(1).toLongLong());
            t.count  = query.value(2).toInt();
            totals.append(t);
        }
    }
    return totals;
}

Money SalesAnalyticsRepository::getActualGrossProfit(const QDate &startDate, const QDate &endDate)
{
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT SUM((si.price - si.cost_price) * si.quantity) as total_profit "
        "FROM sale_items si "
        "JOIN sales s ON si.sale_id = s.id "
        "WHERE DATE(s.sale_date) BETWEEN ? AND ?");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value("total_profit").toLongLong());
    }
    return Money();
}

Money SalesAnalyticsRepository::getActualGrossProfitToday()
{
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT SUM((si.price - si.cost_price) * si.quantity) as total_profit "
        "FROM sale_items si "
        "JOIN sales s ON si.sale_id = s.id "
        "WHERE DATE(s.sale_date) = DATE('now')");
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value("total_profit").toLongLong());
    }
    return Money();
}

Money SalesAnalyticsRepository::getActualGrossProfitThisMonth()
{
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT SUM((si.price - si.cost_price) * si.quantity) as total_profit "
        "FROM sale_items si "
        "JOIN sales s ON si.sale_id = s.id "
        "WHERE strftime('%Y-%m', s.sale_date) = strftime('%Y-%m', 'now')");
    if (query.exec() && query.next()) {
        return Money::fromCents(query.value("total_profit").toLongLong());
    }
    return Money();
}

QVector<Database::ProfitLossRow> SalesAnalyticsRepository::getProfitLossByDateRange(
    const QDate &startArg, const QDate &endArg)
{
    const QString start = startArg.toString(Qt::ISODate);
    const QString end   = endArg.toString(Qt::ISODate);
    // Revenue + COGS grouped by day from sales/sale_items
    QMap<QString, Database::ProfitLossRow> rows;

    QSqlQuery q(m_db);
    q.prepare(
        "SELECT substr(s.sale_date,1,10) AS day, "
        "       SUM(s.total)             AS revenue, "
        "       SUM(si.quantity * si.cost_price) AS cogs "
        "FROM sales s "
        "JOIN sale_items si ON si.sale_id = s.id "
        "WHERE substr(s.sale_date,1,10) BETWEEN ? AND ? "
        "GROUP BY day ORDER BY day");
    q.addBindValue(start);
    q.addBindValue(end);
    if (q.exec()) {
        while (q.next()) {
            Database::ProfitLossRow r;
            r.date    = q.value(0).toString();
            r.revenue = Money::fromCents(q.value(1).toLongLong());
            r.cogs    = Money::fromCents(q.value(2).toLongLong());
            rows[r.date] = r;
        }
    }

    // Expenses grouped by day
    QSqlQuery eq(m_db);
    eq.prepare(
        "SELECT date, SUM(amount) FROM expenses "
        "WHERE date BETWEEN ? AND ? GROUP BY date");
    eq.addBindValue(start);
    eq.addBindValue(end);
    if (eq.exec()) {
        while (eq.next()) {
            const QString day = eq.value(0).toString();
            rows[day].date     = day;
            rows[day].expenses = Money::fromCents(eq.value(1).toLongLong());
        }
    }

    QVector<Database::ProfitLossRow> result;
    result.reserve(rows.size());
    for (const auto &r : std::as_const(rows))
        result.append(r);
    return result;
}
