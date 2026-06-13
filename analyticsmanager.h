// =============================================================================
// analyticsmanager.h — AnalyticsManager: the analytics query engine (no UI)
// -----------------------------------------------------------------------------
// WHAT: Computes daily/hourly sales series, top/bottom sellers, per-category
//       breakdowns with percentages, revenue for arbitrary ranges, average
//       transaction value, peak business hours, period-over-period growth,
//       trend commentary, and the DashboardSummary aggregate.
// HOW:  Each method is a parameterized SQL aggregate over sales/sale_items
//       (SUM/COUNT/GROUP BY with strftime bucketing for hours and days),
//       returning small structs (DailySalesStats, HourlySalesStats,
//       ProductSalesStats, CategoryStats, PeakHourInfo). Holds only a
//       reference to the externally owned QSqlDatabase; refreshAnalytics()
//       emits analyticsUpdated for UI listeners.
// WHY:  Letting SQLite do the aggregation is faster and simpler than loading
//       rows into C++ and looping. Keeping computation UI-free means the same
//       numbers feed AnalyticsDialog, scheduled SMS reports, and anything
//       added later — one definition of "today's revenue" everywhere.
// =============================================================================
#pragma once

#include <QObject>
#include <QVector>
#include <QDate>
#include <QDateTime>
#include <QString>

// Forward declaration of QSqlDatabase (no need to include full header here)
class QSqlDatabase;

// Structs used in public API – defined here for clarity
struct SalesDataPoint {
    QDateTime timestamp;
    int quantity;
    double revenue;
};

struct DailySalesStats {
    QDate date;
    double revenue;
    int transactionCount;
    int itemsSold;
};

struct HourlySalesStats {
    int hour;                // 0-23
    double revenue;
    int transactionCount;
    int itemsSold;
};

struct ProductSalesStats {
    int productId;
    QString productName;
    QString category;
    int totalQuantitySold;
    double totalRevenue;
    double averagePrice;
    int transactionCount;
};

struct CategoryStats {
    QString category;
    int totalQuantitySold;
    double totalRevenue;
    double percentageOfTotal;
};

struct PeakHourInfo {
    int hour;
    double revenue;
    QString timeRange;       // e.g., "14:00-15:00"
};

class AnalyticsManager : public QObject
{
    Q_OBJECT

public:
    // Nested struct for dashboard summary
    struct DashboardSummary {
        double todayRevenue = 0.0;
        double yesterdayRevenue = 0.0;
        double weekRevenue = 0.0;
        double monthRevenue = 0.0;
        int todayTransactions = 0;
        int weekTransactions = 0;
        double avgTransactionValue = 0.0;
        QString topProduct;
        QString topCategory;
        QVector<PeakHourInfo> peakHours;
        double growthRate = 0.0;
    };

    explicit AnalyticsManager(QSqlDatabase &database, QObject *parent = nullptr);

    // Public methods
    QVector<SalesDataPoint> getSalesInDateRange(const QDateTime &start, const QDateTime &end);
    QVector<DailySalesStats> getDailySales(int days);
    QVector<HourlySalesStats> getHourlySalesForToday();
    QVector<HourlySalesStats> getHourlySalesAverage(int days);
    QVector<ProductSalesStats> getTopSellingProducts(int limit, int days);
    QVector<ProductSalesStats> getBottomSellingProducts(int limit, int days);
    ProductSalesStats getProductStats(int productId, int days);
    QVector<CategoryStats> getCategorySales(int days);
    QString getBestPerformingCategory(int days);

    double getTodayRevenue();
    double getRevenueForDate(const QDate &date);
    double getRevenueInRange(const QDateTime &start, const QDateTime &end);
    double getAverageTransactionValue(int days);
    int getTodayTransactionCount();
    int getTransactionCountForDate(const QDate &date);

    QVector<PeakHourInfo> getPeakHours(int days, int topN = 5);
    QString getPeakHourRecommendation(int days);
    double getRevenueGrowth(int daysToCompare);
    QString getTrendAnalysis(int days);

    DashboardSummary getDashboardSummary();
    QString formatCurrency(double amount) const;

    void refreshAnalytics();

signals:
    void analyticsUpdated();

private:
    // Helper methods
    QDateTime getStartOfDay(const QDate &date);
    QDateTime getEndOfDay(const QDate &date);

    QSqlDatabase &db;  // Reference to external database (owned by MainWindow)
};
