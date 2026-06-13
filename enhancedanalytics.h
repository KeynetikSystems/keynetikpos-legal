// =============================================================================
// enhancedanalytics.h — Modular analytics widget toolkit
// -----------------------------------------------------------------------------
// WHAT: Composable analytics widgets: SalesTrendWidget (trend + total/average/
//       trend labels), CategoryBreakdownWidget (pie-style breakdown),
//       HourlySalesWidget (24-hour heat map), TopProductsWidget (ranked
//       table), PaymentAnalysisWidget (tender mix), CustomerAnalyticsWidget
//       (placeholder customer stats), QuickStatsWidget (today's numbers for
//       embedding in the main window), all hosted by EnhancedAnalyticsDialog
//       with shared date-range controls.
// HOW:  Every widget follows the same contract: constructor takes
//       QSqlDatabase&, setDateRange() + refresh() re-query and re-render.
//       Charts are drawn with plain widgets/labels/stylesheets — QChartView
//       was deliberately replaced with QWidget (see comments below); e.g. the
//       heat map colours cells by value/maxValue.
// WHY:  Avoiding the Qt Charts module keeps deployment smaller and the
//       dependency list shorter. The uniform widget contract makes dashboards
//       rearrangeable, and each widget is independently testable.
// =============================================================================
#pragma once

#include <QWidget>
#include <QDialog>
#include <QSqlDatabase>
#include <QVector>
#include <QMap>
#include <QDate>

// Forward declarations
class QLabel;
class QPushButton;
class QComboBox;
class QDateEdit;
class QVBoxLayout;
class QHBoxLayout;

// =============================================================================
// Enhanced Analytics Widgets - Modern charts and dashboards
// =============================================================================

struct SalesData {
    QDate date;
    double amount;
    int transactionCount;
};

struct CategorySales {
    QString category;
    double amount;
    int itemCount;
    double percentage;
};

struct HourlyData {
    int hour; // 0-23
    double sales;
    int transactions;
};

struct ProductRanking {
    int productId;
    QString productName;
    int quantitySold;
    double revenue;
    double profit;
};

// =============================================================================
// Sales Trend Chart Widget
// =============================================================================

class SalesTrendWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SalesTrendWidget(QSqlDatabase &db, QWidget *parent = nullptr);

    void setDateRange(const QDate &start, const QDate &end);
    void refresh();

private:
    void setupUi();
    void loadData();
    void createChart();

    QSqlDatabase &m_db;
    QDate m_startDate;
    QDate m_endDate;
    QVector<SalesData> m_salesData;
    QWidget *m_chartView;
    QLabel *m_totalLabel;
    QLabel *m_averageLabel;
    QLabel *m_trendLabel;
};

// =============================================================================
// Category Breakdown Widget (Pie Chart)
// =============================================================================

class CategoryBreakdownWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CategoryBreakdownWidget(QSqlDatabase &db, QWidget *parent = nullptr);

    void setDateRange(const QDate &start, const QDate &end);
    void refresh();

private:
    void setupUi();
    void loadData();
    void createPieChart();

    QSqlDatabase &m_db;
    QDate m_startDate;
    QDate m_endDate;
    QVector<CategorySales> m_categoryData;

   QWidget *m_chartView;
};

// =============================================================================
// Hourly Sales Heatmap Widget
// =============================================================================

class HourlySalesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HourlySalesWidget(QSqlDatabase &db, QWidget *parent = nullptr);

    void setDate(const QDate &date);
    void refresh();

private:
    void setupUi();
    void loadData();
    void createHeatmap();
    QString getHeatmapColor(double value, double maxValue);

    QSqlDatabase &m_db;
    QDate m_date;
    QVector<HourlyData> m_hourlyData;

    QVBoxLayout *m_heatmapLayout;
};

// =============================================================================
// Top Products Widget
// =============================================================================

class TopProductsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TopProductsWidget(QSqlDatabase &db, QWidget *parent = nullptr);

    void setDateRange(const QDate &start, const QDate &end);
    void setLimit(int limit) { m_limit = limit; }
    void refresh();

private:
    void setupUi();
    void loadData();
    void displayTable();

    QSqlDatabase &m_db;
    QDate m_startDate;
    QDate m_endDate;
    int m_limit;
    QVector<ProductRanking> m_rankings;

    QVBoxLayout *m_tableLayout;
};

// =============================================================================
// Payment Methods Analysis Widget
// =============================================================================

// =============================================================================
// Payment Methods Analysis Widget
// =============================================================================

class PaymentAnalysisWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PaymentAnalysisWidget(QSqlDatabase &db, QWidget *parent = nullptr);

    void setDateRange(const QDate &start, const QDate &end);
    void refresh();

private:
    void setupUi();
    void loadData();
    void createChart();

    QSqlDatabase &m_db;
    QDate m_startDate;
    QDate m_endDate;
    QMap<QString, double> m_paymentData;

    QWidget *m_chartView;  // Changed from QChartView to QWidget
    // Removed: QChart *m_chart;
};
// =============================================================================
// Customer Analytics Widget
// =============================================================================

class CustomerAnalyticsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CustomerAnalyticsWidget(QSqlDatabase &db, QWidget *parent = nullptr);

    void refresh();

private:
    void setupUi();
    void loadData();

    QSqlDatabase &m_db;

    QLabel *m_totalCustomersLabel;
    QLabel *m_newCustomersLabel;
    QLabel *m_returningCustomersLabel;
    QLabel *m_averageValueLabel;
    QLabel *m_topTierLabel;
};

// =============================================================================
// Enhanced Analytics Dashboard Dialog
// =============================================================================

class EnhancedAnalyticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EnhancedAnalyticsDialog(QSqlDatabase &db, QWidget *parent = nullptr);

private slots:
    void onDateRangeChanged();
    void onRefreshAll();
    void onExportReport();
    void onPrintReport();

private:
    void setupUi();
    void refreshAllWidgets();

    QSqlDatabase &m_db;
    QDate m_startDate;
    QDate m_endDate;

    QDateEdit *m_startDateEdit;
    QDateEdit *m_endDateEdit;

    SalesTrendWidget *m_salesTrendWidget;
    CategoryBreakdownWidget *m_categoryWidget;
    HourlySalesWidget *m_hourlyWidget;
    TopProductsWidget *m_topProductsWidget;
    PaymentAnalysisWidget *m_paymentWidget;
    CustomerAnalyticsWidget *m_customerWidget;
};

// =============================================================================
// Quick Stats Dashboard Widget (for main window)
// =============================================================================

class QuickStatsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QuickStatsWidget(QSqlDatabase &db, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    void setupUi();
    void loadTodayStats();

    QSqlDatabase &m_db;

    QLabel *m_todaySalesLabel;
    QLabel *m_todayTransactionsLabel;
    QLabel *m_todayCustomersLabel;
    QLabel *m_averageTicketLabel;
};
