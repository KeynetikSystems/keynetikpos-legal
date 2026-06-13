// =============================================================================
// analyticsmanager.cpp — Implementation of AnalyticsManager (see
// analyticsmanager.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Every method is a parameterized SQL aggregate over sales/sale_items
//    (SUM/COUNT/GROUP BY, strftime bucketing for hours and days) — SQLite
//    does the heavy lifting, C++ only shapes results into the small structs.
//  - Growth/trend helpers compare equal-length adjacent windows so the
//    percentage is a like-for-like comparison.
//  - Holds only a reference to the externally owned QSqlDatabase.
// =============================================================================
#include "analyticsmanager.h"
#include "cart.h"          // formatMoney()
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

AnalyticsManager::AnalyticsManager(QSqlDatabase &database, QObject *parent)
    : QObject(parent)
    , db(database)
{
}

QVector<SalesDataPoint> AnalyticsManager::getSalesInDateRange(const QDateTime &start, const QDateTime &end)
{
    QVector<SalesDataPoint> dataPoints;

    QSqlQuery query(db);
    query.prepare("SELECT s.SaleDatetime, p.RegularPrice, s.Quantity "
                  "FROM Sales s "
                  "JOIN Products p ON s.ProductID = p.ProductID "
                  "WHERE s.SaleDatetime >= :start AND s.SaleDatetime <= :end "
                  "ORDER BY s.SaleDatetime ASC");
    query.bindValue(":start", start.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":end", end.toString("yyyy-MM-dd HH:mm:ss"));

    if (!query.exec()) {
        qDebug() << "Error fetching sales data:" << query.lastError().text();
        return dataPoints;
    }

    while (query.next()) {
        SalesDataPoint point;
        point.timestamp = QDateTime::fromString(query.value(0).toString(), "yyyy-MM-dd HH:mm:ss");
        double price = query.value(1).toDouble();
        point.quantity = query.value(2).toInt();
        point.revenue = price * point.quantity;
        dataPoints.append(point);
    }

    return dataPoints;
}



QVector<DailySalesStats> AnalyticsManager::getDailySales(int days)
{
    QVector<DailySalesStats> dailyStats;

    QDate endDate = QDate::currentDate();
    QDate startDate = endDate.addDays(-days + 1);

    for (QDate date = startDate; date <= endDate; date = date.addDays(1)) {
        DailySalesStats stats;
        stats.date = date;
        stats.revenue = getRevenueForDate(date);
        stats.transactionCount = getTransactionCountForDate(date);

        // Get items sold
        QSqlQuery query(db);
        query.prepare("SELECT SUM(Quantity) FROM Sales "
                      "WHERE DATE(SaleDatetime) = :date");
        query.bindValue(":date", date.toString("yyyy-MM-dd"));

        if (query.exec() && query.next()) {
            stats.itemsSold = query.value(0).toInt();
        } else {
            stats.itemsSold = 0;
        }

        dailyStats.append(stats);
    }

    return dailyStats;
}

QVector<HourlySalesStats> AnalyticsManager::getHourlySalesForToday()
{
    QVector<HourlySalesStats> hourlyStats;

    QDate today = QDate::currentDate();

    for (int hour = 0; hour < 24; ++hour) {
        HourlySalesStats stats;
        stats.hour = hour;
        stats.revenue = 0.0;
        stats.transactionCount = 0;
        stats.itemsSold = 0;

        QSqlQuery query(db);
        query.prepare("SELECT p.RegularPrice, s.Quantity, COUNT(DISTINCT s.SaleID) "
                      "FROM Sales s "
                      "JOIN Products p ON s.ProductID = p.ProductID "
                      "WHERE DATE(s.SaleDatetime) = :date "
                      "AND CAST(strftime('%H', s.SaleDatetime) AS INTEGER) = :hour "
                      "GROUP BY s.SaleID, p.RegularPrice, s.Quantity");
        query.bindValue(":date", today.toString("yyyy-MM-dd"));
        query.bindValue(":hour", hour);

        if (query.exec()) {
            while (query.next()) {
                double price = query.value(0).toDouble();
                int qty = query.value(1).toInt();
                stats.revenue += price * qty;
                stats.transactionCount++;
                stats.itemsSold += qty;
            }
        }

        hourlyStats.append(stats);
    }

    return hourlyStats;
}

QVector<HourlySalesStats> AnalyticsManager::getHourlySalesAverage(int days)
{
    QVector<HourlySalesStats> hourlyStats;

    QDate endDate = QDate::currentDate();
    QDate startDate = endDate.addDays(-days + 1);

    for (int hour = 0; hour < 24; ++hour) {
        HourlySalesStats stats;
        stats.hour = hour;
        stats.revenue = 0.0;
        stats.transactionCount = 0;
        stats.itemsSold = 0;

        QSqlQuery query(db);
        query.prepare("SELECT SUM(p.RegularPrice * s.Quantity), COUNT(DISTINCT s.SaleID), SUM(s.Quantity) "
                      "FROM Sales s "
                      "JOIN Products p ON s.ProductID = p.ProductID "
                      "WHERE DATE(s.SaleDatetime) >= :start AND DATE(s.SaleDatetime) <= :end "
                      "AND CAST(strftime('%H', s.SaleDatetime) AS INTEGER) = :hour");
        query.bindValue(":start", startDate.toString("yyyy-MM-dd"));
        query.bindValue(":end", endDate.toString("yyyy-MM-dd"));
        query.bindValue(":hour", hour);

        if (query.exec() && query.next()) {
            stats.revenue = query.value(0).toDouble() / days;
            stats.transactionCount = query.value(1).toInt() / days;
            stats.itemsSold = query.value(2).toInt() / days;
        }

        hourlyStats.append(stats);
    }

    return hourlyStats;
}

QVector<ProductSalesStats> AnalyticsManager::getTopSellingProducts(int limit, int days)
{
    QVector<ProductSalesStats> topProducts;

    QDate startDate = QDate::currentDate().addDays(-days + 1);

    QSqlQuery query(db);
    query.prepare("SELECT p.ProductID, p.ProductName, p.Category, "
                  "SUM(s.Quantity) as TotalQty, "
                  "SUM(p.RegularPrice * s.Quantity) as TotalRevenue, "
                  "AVG(p.RegularPrice) as AvgPrice, "
                  "COUNT(DISTINCT s.SaleID) as TransCount "
                  "FROM Sales s "
                  "JOIN Products p ON s.ProductID = p.ProductID "
                  "WHERE DATE(s.SaleDatetime) >= :start "
                  "GROUP BY p.ProductID "
                  "ORDER BY TotalQty DESC "
                  "LIMIT :limit");
    query.bindValue(":start", startDate.toString("yyyy-MM-dd"));
    query.bindValue(":limit", limit);

    if (!query.exec()) {
        qDebug() << "Error fetching top products:" << query.lastError().text();
        return topProducts;
    }

    while (query.next()) {
        ProductSalesStats stats;
        stats.productId = query.value(0).toInt();
        stats.productName = query.value(1).toString();
        stats.category = query.value(2).toString();
        stats.totalQuantitySold = query.value(3).toInt();
        stats.totalRevenue = query.value(4).toDouble();
        stats.averagePrice = query.value(5).toDouble();
        stats.transactionCount = query.value(6).toInt();
        topProducts.append(stats);
    }

    return topProducts;
}

QVector<ProductSalesStats> AnalyticsManager::getBottomSellingProducts(int limit, int days)
{
    QVector<ProductSalesStats> bottomProducts;

    QDate startDate = QDate::currentDate().addDays(-days + 1);

    QSqlQuery query(db);
    query.prepare("SELECT p.ProductID, p.ProductName, p.Category, "
                  "COALESCE(SUM(s.Quantity), 0) as TotalQty, "
                  "COALESCE(SUM(p.RegularPrice * s.Quantity), 0) as TotalRevenue, "
                  "p.RegularPrice as AvgPrice, "
                  "COUNT(DISTINCT s.SaleID) as TransCount "
                  "FROM Products p "
                  "LEFT JOIN Sales s ON p.ProductID = s.ProductID "
                  "AND DATE(s.SaleDatetime) >= :start "
                  "GROUP BY p.ProductID "
                  "ORDER BY TotalQty ASC "
                  "LIMIT :limit");
    query.bindValue(":start", startDate.toString("yyyy-MM-dd"));
    query.bindValue(":limit", limit);

    if (!query.exec()) {
        qDebug() << "Error fetching bottom products:" << query.lastError().text();
        return bottomProducts;
    }

    while (query.next()) {
        ProductSalesStats stats;
        stats.productId = query.value(0).toInt();
        stats.productName = query.value(1).toString();
        stats.category = query.value(2).toString();
        stats.totalQuantitySold = query.value(3).toInt();
        stats.totalRevenue = query.value(4).toDouble();
        stats.averagePrice = query.value(5).toDouble();
        stats.transactionCount = query.value(6).toInt();
        bottomProducts.append(stats);
    }

    return bottomProducts;
}

ProductSalesStats AnalyticsManager::getProductStats(int productId, int days)
{
    ProductSalesStats stats;
    stats.productId = productId;

    QDate startDate = QDate::currentDate().addDays(-days + 1);

    QSqlQuery query(db);
    query.prepare("SELECT p.ProductName, p.Category, "
                  "SUM(s.Quantity) as TotalQty, "
                  "SUM(p.RegularPrice * s.Quantity) as TotalRevenue, "
                  "AVG(p.RegularPrice) as AvgPrice, "
                  "COUNT(DISTINCT s.SaleID) as TransCount "
                  "FROM Sales s "
                  "JOIN Products p ON s.ProductID = p.ProductID "
                  "WHERE s.ProductID = :productId AND DATE(s.SaleDatetime) >= :start "
                  "GROUP BY p.ProductID");
    query.bindValue(":productId", productId);
    query.bindValue(":start", startDate.toString("yyyy-MM-dd"));

    if (query.exec() && query.next()) {
        stats.productName = query.value(0).toString();
        stats.category = query.value(1).toString();
        stats.totalQuantitySold = query.value(2).toInt();
        stats.totalRevenue = query.value(3).toDouble();
        stats.averagePrice = query.value(4).toDouble();
        stats.transactionCount = query.value(5).toInt();
    }

    return stats;
}

QVector<CategoryStats> AnalyticsManager::getCategorySales(int days)
{
    QVector<CategoryStats> categoryStats;

    QDate startDate = QDate::currentDate().addDays(-days + 1);

    // First get total revenue for percentage calculation
    double totalRevenue = getRevenueInRange(getStartOfDay(startDate), QDateTime::currentDateTime());

    QSqlQuery query(db);
    query.prepare("SELECT p.Category, "
                  "SUM(s.Quantity) as TotalQty, "
                  "SUM(p.RegularPrice * s.Quantity) as TotalRevenue "
                  "FROM Sales s "
                  "JOIN Products p ON s.ProductID = p.ProductID "
                  "WHERE DATE(s.SaleDatetime) >= :start "
                  "GROUP BY p.Category "
                  "ORDER BY TotalRevenue DESC");
    query.bindValue(":start", startDate.toString("yyyy-MM-dd"));

    if (query.exec()) {
        while (query.next()) {
            CategoryStats stats;
            stats.category = query.value(0).toString();
            stats.totalQuantitySold = query.value(1).toInt();
            stats.totalRevenue = query.value(2).toDouble();
            stats.percentageOfTotal = (totalRevenue > 0) ? (stats.totalRevenue / totalRevenue * 100.0) : 0.0;
            categoryStats.append(stats);
        }
    }

    return categoryStats;
}

QString AnalyticsManager::getBestPerformingCategory(int days)
{
    QVector<CategoryStats> stats = getCategorySales(days);

    if (stats.isEmpty()) {
        return "No data";
    }

    return stats.first().category; // Already sorted by revenue DESC
}

double AnalyticsManager::getTodayRevenue()
{
    return getRevenueForDate(QDate::currentDate());
}

double AnalyticsManager::getRevenueForDate(const QDate &date)
{
    QSqlQuery query(db);
    query.prepare("SELECT SUM(p.RegularPrice * s.Quantity) "
                  "FROM Sales s "
                  "JOIN Products p ON s.ProductID = p.ProductID "
                  "WHERE DATE(s.SaleDatetime) = :date");
    query.bindValue(":date", date.toString("yyyy-MM-dd"));

    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }

    return 0.0;
}

double AnalyticsManager::getRevenueInRange(const QDateTime &start, const QDateTime &end)
{
    QSqlQuery query(db);
    query.prepare("SELECT SUM(p.RegularPrice * s.Quantity) "
                  "FROM Sales s "
                  "JOIN Products p ON s.ProductID = p.ProductID "
                  "WHERE s.SaleDatetime >= :start AND s.SaleDatetime <= :end");
    query.bindValue(":start", start.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":end", end.toString("yyyy-MM-dd HH:mm:ss"));

    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }

    return 0.0;
}

double AnalyticsManager::getAverageTransactionValue(int days)
{
    QDate startDate = QDate::currentDate().addDays(-days + 1);

    double totalRevenue = getRevenueInRange(getStartOfDay(startDate), QDateTime::currentDateTime());
    int transactionCount = 0;

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(DISTINCT SaleID) FROM Sales "
                  "WHERE DATE(SaleDatetime) >= :start");
    query.bindValue(":start", startDate.toString("yyyy-MM-dd"));

    if (query.exec() && query.next()) {
        transactionCount = query.value(0).toInt();
    }

    return (transactionCount > 0) ? (totalRevenue / transactionCount) : 0.0;
}

int AnalyticsManager::getTodayTransactionCount()
{
    return getTransactionCountForDate(QDate::currentDate());
}

int AnalyticsManager::getTransactionCountForDate(const QDate &date)
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(DISTINCT SaleID) FROM Sales "
                  "WHERE DATE(SaleDatetime) = :date");
    query.bindValue(":date", date.toString("yyyy-MM-dd"));

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

QVector<PeakHourInfo> AnalyticsManager::getPeakHours(int days, int topN)
{
    QVector<PeakHourInfo> peakHours;

    QDate startDate = QDate::currentDate().addDays(-days + 1);

    QSqlQuery query(db);
    query.prepare("SELECT CAST(strftime('%H', s.SaleDatetime) AS INTEGER) as Hour, "
                  "SUM(p.RegularPrice * s.Quantity) as HourRevenue "
                  "FROM Sales s "
                  "JOIN Products p ON s.ProductID = p.ProductID "
                  "WHERE DATE(s.SaleDatetime) >= :start "
                  "GROUP BY Hour "
                  "ORDER BY HourRevenue DESC "
                  "LIMIT :limit");
    query.bindValue(":start", startDate.toString("yyyy-MM-dd"));
    query.bindValue(":limit", topN);

    if (query.exec()) {
        while (query.next()) {
            PeakHourInfo info;
            info.hour = query.value(0).toInt();
            info.revenue = query.value(1).toDouble();
            info.timeRange = QString("%1:00-%2:00")
                                 .arg(info.hour, 2, 10, QChar('0'))
                                 .arg((info.hour + 1) % 24, 2, 10, QChar('0'));
            peakHours.append(info);
        }
    }

    return peakHours;
}

QString AnalyticsManager::getPeakHourRecommendation(int days)
{
    QVector<PeakHourInfo> peaks = getPeakHours(days, 3);

    if (peaks.isEmpty()) {
        return "Insufficient data for recommendations";
    }

    QString recommendation = "Busiest hours: ";
    for (int i = 0; i < peaks.size(); ++i) {
        recommendation += peaks[i].timeRange;
        if (i < peaks.size() - 1) {
            recommendation += ", ";
        }
    }
    recommendation += ". Consider staffing accordingly.";

    return recommendation;
}

double AnalyticsManager::getRevenueGrowth(int daysToCompare)
{
    QDate today = QDate::currentDate();
    QDate startCurrent = today.addDays(-daysToCompare + 1);
    QDate startPrevious = startCurrent.addDays(-daysToCompare);
    QDate endPrevious = startCurrent.addDays(-1);

    double currentRevenue = getRevenueInRange(getStartOfDay(startCurrent), QDateTime::currentDateTime());
    double previousRevenue = getRevenueInRange(getStartOfDay(startPrevious), getEndOfDay(endPrevious));

    if (previousRevenue == 0.0) {
        return 0.0;
    }

    return ((currentRevenue - previousRevenue) / previousRevenue) * 100.0;
}

QString AnalyticsManager::getTrendAnalysis(int days)
{
    double growth = getRevenueGrowth(days);

    QString trend;
    if (growth > 10.0) {
        trend = QString("Strong growth: +%1%").arg(growth, 0, 'f', 1);
    } else if (growth > 0.0) {
        trend = QString("Moderate growth: +%1%").arg(growth, 0, 'f', 1);
    } else if (growth > -10.0) {
        trend = QString("Slight decline: %1%").arg(growth, 0, 'f', 1);
    } else {
        trend = QString("Significant decline: %1%").arg(growth, 0, 'f', 1);
    }

    return trend;
}

AnalyticsManager::DashboardSummary AnalyticsManager::getDashboardSummary()
{
    DashboardSummary summary;

    summary.todayRevenue = getTodayRevenue();
    summary.yesterdayRevenue = getRevenueForDate(QDate::currentDate().addDays(-1));

    QDate weekAgo = QDate::currentDate().addDays(-6);
    summary.weekRevenue = getRevenueInRange(getStartOfDay(weekAgo), QDateTime::currentDateTime());

    QDate monthAgo = QDate::currentDate().addDays(-29);
    summary.monthRevenue = getRevenueInRange(getStartOfDay(monthAgo), QDateTime::currentDateTime());

    summary.todayTransactions = getTodayTransactionCount();

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(DISTINCT SaleID) FROM Sales "
                  "WHERE DATE(SaleDatetime) >= :start");
    query.bindValue(":start", weekAgo.toString("yyyy-MM-dd"));
    if (query.exec() && query.next()) {
        summary.weekTransactions = query.value(0).toInt();
    } else {
        summary.weekTransactions = 0;
    }

    summary.avgTransactionValue = getAverageTransactionValue(30);

    QVector<ProductSalesStats> topProducts = getTopSellingProducts(1, 30);
    summary.topProduct = topProducts.isEmpty() ? "No data" : topProducts.first().productName;

    summary.topCategory = getBestPerformingCategory(30);

    summary.peakHours = getPeakHours(7, 3);

    summary.growthRate = getRevenueGrowth(7);

    return summary;
}

void AnalyticsManager::refreshAnalytics()
{
    emit analyticsUpdated();
}

QDateTime AnalyticsManager::getStartOfDay(const QDate &date)
{
    return QDateTime(date, QTime(0, 0, 0));
}

QDateTime AnalyticsManager::getEndOfDay(const QDate &date)
{
    return QDateTime(date, QTime(23, 59, 59));
}

QString AnalyticsManager::formatCurrency(double amount) const
{
    return formatMoney(amount);
}
