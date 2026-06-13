// =============================================================================
// analyticsdashboard.h — AnalyticsDashboard: date-range / profit dashboard
// -----------------------------------------------------------------------------
// WHAT: A date-range driven analytics dialog: quick ranges + explicit start/
//       end pickers, metric cards (sales, profit, margin, tax collected,
//       discounts given, transactions, items), and tabs for top products,
//       slow movers, and an hourly-sales table, with CSV export.
// HOW:  Runs its own SQL directly against the singleton Database connection
//       (not through AnalyticsManager), including profit math from the
//       sale_items.cost_price snapshots, and renders into QTableWidgets.
// WHY:  Its distinguishing features are arbitrary date ranges and a profit/
//       margin focus — it answers "how did the business do between these two
//       dates" where AnalyticsDialog answers "how are we trending lately".
//       (Overlap with the other analytics UIs is acknowledged tech debt.)
// =============================================================================
#ifndef ANALYTICSDASHBOARD_H
#define ANALYTICSDASHBOARD_H

#include <QDialog>
#include <QLabel>
#include <QComboBox>
#include <QDateEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QGroupBox>
#include <QTabWidget>
#include <QDateTime>
#include <QVector>

// ─────────────────────────────────────────────────────────────────────────────
// Data structures
// ─────────────────────────────────────────────────────────────────────────────
struct SalesMetrics {
    double totalSales         = 0.0;
    double totalProfit        = 0.0;
    double taxCollected       = 0.0;
    double discountsGiven     = 0.0;
    int    transactionCount   = 0;
    int    itemsSold          = 0;
    double averageTransaction = 0.0;
};

struct ProductPerformance {
    QString productName;
    QString category;
    int     unitsSold    = 0;
    double  revenue      = 0.0;
    double  profit       = 0.0;
    int     timesOrdered = 0;
};

struct HourlySales {
    int    hour         = 0;
    int    transactions = 0;
    double sales        = 0.0;
};

// ─────────────────────────────────────────────────────────────────────────────
// AnalyticsDashboard
// ─────────────────────────────────────────────────────────────────────────────
class AnalyticsDashboard : public QDialog
{
    Q_OBJECT

public:
    explicit AnalyticsDashboard(QWidget *parent = nullptr);
    ~AnalyticsDashboard();

private slots:
    void onDateRangeChanged();
    void onRefreshClicked();
    void onExportClicked();
    void onQuickRangeSelected(const QString &range);

private:
    // Setup — called once
    void setupUI();
    void setupMetricsSection();
    void setupTabbedAnalyticsSection();
    void setupTopProductsSection();
    void setupSlowMovingSection();
    void setupHourlySalesSection();

    // Data
    void loadData();
    SalesMetrics                calculateMetrics();
    QVector<ProductPerformance> getTopProducts(int limit);
    QVector<ProductPerformance> getSlowMovingProducts(int limit);
    QVector<HourlySales>        getHourlySalesData();

    // Display updates
    void updateMetricsDisplay(const SalesMetrics &metrics);
    void updateTopProductsTable(const QVector<ProductPerformance> &products);
    void updateSlowMovingTable(const QVector<ProductPerformance> &products);
    void updateHourlyChart(const QVector<HourlySales> &data);

    // Helpers
    QString formatCurrency(double amount);
    QString formatPercentage(double value);

    // ── UI Components ──────────────────────────────────────────────────────

    // Date range
    QComboBox   *quickRangeCombo  = nullptr;
    QDateEdit   *startDateEdit    = nullptr;
    QDateEdit   *endDateEdit      = nullptr;
    QPushButton *refreshBtn       = nullptr;
    QPushButton *exportBtn        = nullptr;

    // Metrics
    QGroupBox *metricsGroup       = nullptr;
    QLabel    *totalSalesLabel    = nullptr;
    QLabel    *profitLabel        = nullptr;
    QLabel    *profitMarginLabel  = nullptr;
    QLabel    *transactionsLabel  = nullptr;
    QLabel    *avgTransactionLabel = nullptr;
    QLabel    *taxCollectedLabel  = nullptr;
    QLabel    *discountsLabel     = nullptr;
    QLabel    *itemsSoldLabel     = nullptr;

    // Tabbed section
    QTabWidget   *analyticsTabWidget = nullptr;

    // Top products tab
    QWidget      *topProductsWidget = nullptr;
    QGroupBox    *topProductsGroup  = nullptr;
    QTableWidget *topProductsTable  = nullptr;

    // Slow moving tab
    QWidget      *slowMovingWidget = nullptr;
    QGroupBox    *slowMovingGroup  = nullptr;
    QTableWidget *slowMovingTable  = nullptr;

    // Hourly sales tab
    QWidget      *hourlySalesWidget = nullptr;
    QGroupBox    *hourlySalesGroup  = nullptr;
    QTableWidget *hourlySalesTable  = nullptr;

    // Data
    QDateTime startDate;
    QDateTime endDate;
};

#endif // ANALYTICSDASHBOARD_H
