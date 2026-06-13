// =============================================================================
// enhancedanalytics.cpp — Implementation of the modular analytics widgets and
// EnhancedAnalyticsDialog (see enhancedanalytics.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Every widget follows the same contract: setDateRange() + refresh()
//    re-query (own SQL against the shared QSqlDatabase&) and re-render.
//  - Charts are plain widgets/labels/stylesheets — QChartView was deliberately
//    replaced with QWidget to avoid the Qt Charts dependency; the hourly heat
//    map colours cells by value/maxValue.
//  - EnhancedAnalyticsDialog just hosts the widgets and fans out the shared
//    date range via refreshAllWidgets().
// =============================================================================
#include "enhancedanalytics.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDateEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QPainter>
#include <QFile>
#include <QTextStream>

// Note: This is a simplified version using standard Qt widgets
// For production, consider upgrading to Qt Charts module for better visualizations

// =============================================================================
// Simple Chart Widget (custom painting)
// =============================================================================

class SimpleBarChart : public QWidget
{
public:
    explicit SimpleBarChart(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(200);
    }

    void setData(const QVector<QPair<QString, double>> &data) {
        m_data = data;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        if (m_data.isEmpty()) return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        // Axis/value text must follow the theme, not the default black pen.
        painter.setPen(palette().color(QPalette::WindowText));

        int margin = 40;
        int chartWidth = width() - 2 * margin;
        int chartHeight = height() - 2 * margin;

        // Find max value
        double maxValue = 0;
        for (const auto &pair : m_data) {
            if (pair.second > maxValue) maxValue = pair.second;
        }

        if (maxValue == 0) return;

        // Draw bars
        int barWidth = chartWidth / m_data.size() - 10;
        int x = margin;

        QVector<QColor> colors = {
            QColor("#2196F3"), QColor("#4CAF50"), QColor("#FF9800"),
            QColor("#9C27B0"), QColor("#F44336"), QColor("#00BCD4")
        };

        for (int i = 0; i < m_data.size(); ++i) {
            int barHeight = (m_data[i].second / maxValue) * chartHeight;

            painter.fillRect(x, height() - margin - barHeight, barWidth, barHeight,
                             colors[i % colors.size()]);

            // Label
            painter.save();
            painter.translate(x + barWidth/2, height() - 10);
            painter.rotate(-45);
            painter.drawText(0, 0, m_data[i].first);
            painter.restore();

            // Value
            painter.drawText(x, height() - margin - barHeight - 5,
                             barWidth, 20, Qt::AlignCenter,
                             QString("$%1").arg(m_data[i].second, 0, 'f', 0));

            x += barWidth + 10;
        }
    }

private:
    QVector<QPair<QString, double>> m_data;
};

// =============================================================================
// Sales Trend Widget Implementation
// =============================================================================

SalesTrendWidget::SalesTrendWidget(QSqlDatabase &db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
    , m_startDate(QDate::currentDate().addDays(-7))
    , m_endDate(QDate::currentDate())
{
    setupUi();
    loadData();
}

void SalesTrendWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *titleLabel = new QLabel("📈 Sales Trend", this);
    titleLabel->setProperty("role", "sectionTitle");
    layout->addWidget(titleLabel);

    m_chartView = new SimpleBarChart(this);
    layout->addWidget(m_chartView);

    auto *statsLayout = new QHBoxLayout;

    m_totalLabel = new QLabel(this);
    m_totalLabel->setProperty("role", "chip");
    m_totalLabel->setProperty("kind", "primary");
    statsLayout->addWidget(m_totalLabel);

    m_averageLabel = new QLabel(this);
    m_averageLabel->setProperty("role", "chip");
    m_averageLabel->setProperty("kind", "info");
    statsLayout->addWidget(m_averageLabel);

    m_trendLabel = new QLabel(this);
    m_trendLabel->setProperty("role", "chip");
    m_trendLabel->setProperty("kind", "warning");
    statsLayout->addWidget(m_trendLabel);

    layout->addLayout(statsLayout);
}

void SalesTrendWidget::setDateRange(const QDate &start, const QDate &end)
{
    m_startDate = start;
    m_endDate = end;
}

void SalesTrendWidget::refresh()
{
    loadData();
}

void SalesTrendWidget::loadData()
{
    m_salesData.clear();

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT DATE(SaleDatetime) as SaleDate,
               SUM(p.RegularPrice * s.Quantity) as DailySales,
               COUNT(DISTINCT s.SaleID) as TransCount
        FROM Sales s
        JOIN Products p ON s.ProductID = p.ProductID
        WHERE DATE(SaleDatetime) BETWEEN :start AND :end
        GROUP BY DATE(SaleDatetime)
        ORDER BY SaleDate
    )");
    query.bindValue(":start", m_startDate.toString("yyyy-MM-dd"));
    query.bindValue(":end", m_endDate.toString("yyyy-MM-dd"));

    double total = 0;
    if (query.exec()) {
        while (query.next()) {
            SalesData data;
            data.date = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
            data.amount = query.value(1).toDouble();
            data.transactionCount = query.value(2).toInt();
            m_salesData.append(data);
            total += data.amount;
        }
    }

    createChart();

    // Update labels
    m_totalLabel->setText(QString("Total: $%1").arg(total, 0, 'f', 2));

    double average = m_salesData.isEmpty() ? 0 : total / m_salesData.size();
    m_averageLabel->setText(QString("Avg: $%1/day").arg(average, 0, 'f', 2));

    QString trend = m_salesData.size() >= 2 &&
                            m_salesData.last().amount > m_salesData.first().amount ?
                        "📈 Trending Up" : "📉 Stable";
    m_trendLabel->setText(trend);
}

void SalesTrendWidget::createChart()
{
    QVector<QPair<QString, double>> chartData;
    for (const SalesData &data : m_salesData) {
        chartData.append({data.date.toString("MMM dd"), data.amount});
    }
   static_cast<SimpleBarChart*>(m_chartView)->setData(chartData);
}

// =============================================================================
// Category Breakdown Widget Implementation
// =============================================================================

CategoryBreakdownWidget::CategoryBreakdownWidget(QSqlDatabase &db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
    , m_startDate(QDate::currentDate().addDays(-7))
    , m_endDate(QDate::currentDate())
{
    setupUi();
    loadData();
}

void CategoryBreakdownWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *titleLabel = new QLabel("🥧 Category Breakdown", this);
    titleLabel->setProperty("role", "sectionTitle");
    layout->addWidget(titleLabel);

    m_chartView = new SimpleBarChart(this);
    layout->addWidget(m_chartView);
}

void CategoryBreakdownWidget::setDateRange(const QDate &start, const QDate &end)
{
    m_startDate = start;
    m_endDate = end;
}

void CategoryBreakdownWidget::refresh()
{
    loadData();
}

void CategoryBreakdownWidget::loadData()
{
    m_categoryData.clear();

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT p.Category,
               SUM(p.RegularPrice * s.Quantity) as CategorySales,
               COUNT(*) as ItemCount
        FROM Sales s
        JOIN Products p ON s.ProductID = p.ProductID
        WHERE DATE(SaleDatetime) BETWEEN :start AND :end
        GROUP BY p.Category
        ORDER BY CategorySales DESC
    )");
    query.bindValue(":start", m_startDate.toString("yyyy-MM-dd"));
    query.bindValue(":end", m_endDate.toString("yyyy-MM-dd"));

    double total = 0;
    if (query.exec()) {
        while (query.next()) {
            CategorySales cat;
            cat.category = query.value(0).toString();
            cat.amount = query.value(1).toDouble();
            cat.itemCount = query.value(2).toInt();
            total += cat.amount;
            m_categoryData.append(cat);
        }
    }

    // Calculate percentages
    for (CategorySales &cat : m_categoryData) {
        cat.percentage = total > 0 ? (cat.amount / total) * 100 : 0;
    }

    createPieChart();
}

void CategoryBreakdownWidget::createPieChart()
{
    QVector<QPair<QString, double>> chartData;
    for (const CategorySales &cat : m_categoryData) {
        chartData.append({
            QString("%1 (%2%)").arg(cat.category).arg(cat.percentage, 0, 'f', 1),
            cat.amount
        });
    }
  static_cast<SimpleBarChart*>(m_chartView)->setData(chartData);
}

// =============================================================================
// Top Products Widget Implementation
// =============================================================================

TopProductsWidget::TopProductsWidget(QSqlDatabase &db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
    , m_startDate(QDate::currentDate().addDays(-7))
    , m_endDate(QDate::currentDate())
    , m_limit(10)
{
    setupUi();
    loadData();
}

void TopProductsWidget::setupUi()
{
    m_tableLayout = new QVBoxLayout(this);

    auto *titleLabel = new QLabel("🏆 Top Products", this);
    titleLabel->setProperty("role", "sectionTitle");
    m_tableLayout->addWidget(titleLabel);
}

void TopProductsWidget::setDateRange(const QDate &start, const QDate &end)
{
    m_startDate = start;
    m_endDate = end;
}

void TopProductsWidget::refresh()
{
    loadData();
}

void TopProductsWidget::loadData()
{
    m_rankings.clear();

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT p.ProductID, p.ProductName,
               SUM(s.Quantity) as TotalQty,
               SUM(p.RegularPrice * s.Quantity) as Revenue
        FROM Sales s
        JOIN Products p ON s.ProductID = p.ProductID
        WHERE DATE(SaleDatetime) BETWEEN :start AND :end
        GROUP BY p.ProductID, p.ProductName
        ORDER BY Revenue DESC
        LIMIT :limit
    )");
    query.bindValue(":start", m_startDate.toString("yyyy-MM-dd"));
    query.bindValue(":end", m_endDate.toString("yyyy-MM-dd"));
    query.bindValue(":limit", m_limit);

    if (query.exec()) {
        while (query.next()) {
            ProductRanking ranking;
            ranking.productId = query.value(0).toInt();
            ranking.productName = query.value(1).toString();
            ranking.quantitySold = query.value(2).toInt();
            ranking.revenue = query.value(3).toDouble();
            ranking.profit = ranking.revenue * 0.3; // Assume 30% margin
            m_rankings.append(ranking);
        }
    }

    displayTable();
}

void TopProductsWidget::displayTable()
{
    // Clear existing widgets
    while (m_tableLayout->count() > 1) {
        QLayoutItem *item = m_tableLayout->takeAt(1);
        delete item->widget();
        delete item;
    }

    auto *table = new QTableWidget(m_rankings.size(), 4, this);
    table->setHorizontalHeaderLabels({"Product", "Qty Sold", "Revenue", "Est. Profit"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    for (int i = 0; i < m_rankings.size(); ++i) {
        const ProductRanking &rank = m_rankings[i];

        table->setItem(i, 0, new QTableWidgetItem(QString("%1. %2")
                                                      .arg(i + 1)
                                                      .arg(rank.productName)));
        table->setItem(i, 1, new QTableWidgetItem(QString::number(rank.quantitySold)));
        table->setItem(i, 2, new QTableWidgetItem(QString("$%1").arg(rank.revenue, 0, 'f', 2)));
        table->setItem(i, 3, new QTableWidgetItem(QString("$%1").arg(rank.profit, 0, 'f', 2)));
    }

    table->resizeColumnsToContents();
    m_tableLayout->addWidget(table);
}

// =============================================================================
// Payment Analysis Widget Implementation
// =============================================================================

PaymentAnalysisWidget::PaymentAnalysisWidget(QSqlDatabase &db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
    , m_startDate(QDate::currentDate().addDays(-7))
    , m_endDate(QDate::currentDate())
{
    setupUi();
    loadData();
}

void PaymentAnalysisWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *titleLabel = new QLabel("💳 Payment Methods", this);
    titleLabel->setProperty("role", "sectionTitle");
    layout->addWidget(titleLabel);

    m_chartView = new SimpleBarChart(this);
    layout->addWidget(m_chartView);
}

void PaymentAnalysisWidget::setDateRange(const QDate &start, const QDate &end)
{
    m_startDate = start;
    m_endDate = end;
}

void PaymentAnalysisWidget::refresh()
{
    loadData();
}

void PaymentAnalysisWidget::loadData()
{
    m_paymentData.clear();

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT PaymentMethod, SUM(Amount) as Total
        FROM PaymentRecords
        WHERE DATE(Timestamp) BETWEEN :start AND :end
        GROUP BY PaymentMethod
        ORDER BY Total DESC
    )");
    query.bindValue(":start", m_startDate.toString("yyyy-MM-dd"));
    query.bindValue(":end", m_endDate.toString("yyyy-MM-dd"));

    if (query.exec()) {
        while (query.next()) {
            QString method = query.value(0).toString();
            double amount = query.value(1).toDouble();
            m_paymentData[method] = amount;
        }
    }

    createChart();
}

void PaymentAnalysisWidget::createChart()
{
    QVector<QPair<QString, double>> chartData;
    for (auto it = m_paymentData.begin(); it != m_paymentData.end(); ++it) {
        chartData.append({it.key(), it.value()});
    }
    static_cast<SimpleBarChart*>(m_chartView)->setData(chartData);
}

// =============================================================================
// Customer Analytics Widget Implementation
// =============================================================================

CustomerAnalyticsWidget::CustomerAnalyticsWidget(QSqlDatabase &db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
{
    setupUi();
    loadData();
}

void CustomerAnalyticsWidget::setupUi()
{
    auto *layout = new QGridLayout(this);

    auto *titleLabel = new QLabel("👥 Customer Analytics", this);
    titleLabel->setProperty("role", "sectionTitle");
    layout->addWidget(titleLabel, 0, 0, 1, 2);

    auto makeStatChip = [this](const char *kind) -> QLabel* {
        QLabel *l = new QLabel(this);
        l->setProperty("role", "chip");
        l->setProperty("kind", kind);
        l->setProperty("textScale", "lg");
        return l;
    };

    m_totalCustomersLabel = makeStatChip("info");
    layout->addWidget(m_totalCustomersLabel, 1, 0);

    m_newCustomersLabel = makeStatChip("primary");
    layout->addWidget(m_newCustomersLabel, 1, 1);

    m_returningCustomersLabel = makeStatChip("warning");
    layout->addWidget(m_returningCustomersLabel, 2, 0);

    m_averageValueLabel = makeStatChip("tertiary");
    layout->addWidget(m_averageValueLabel, 2, 1);

    m_topTierLabel = makeStatChip("danger");
    layout->addWidget(m_topTierLabel, 3, 0, 1, 2);
}

void CustomerAnalyticsWidget::refresh()
{
    loadData();
}

void CustomerAnalyticsWidget::loadData()
{
    QSqlQuery query(m_db);

    // Total customers
    query.exec("SELECT COUNT(*) FROM Customers WHERE IsActive = 1");
    int total = query.next() ? query.value(0).toInt() : 0;
    m_totalCustomersLabel->setText(QString("Total Customers\n%1").arg(total));

    // New customers (last 30 days)
    query.exec("SELECT COUNT(*) FROM Customers WHERE DateJoined >= date('now', '-30 days')");
    int newCust = query.next() ? query.value(0).toInt() : 0;
    m_newCustomersLabel->setText(QString("New (30 days)\n%1").arg(newCust));

    // Returning customers
    query.exec("SELECT COUNT(*) FROM Customers WHERE PurchaseCount > 1");
    int returning = query.next() ? query.value(0).toInt() : 0;
    m_returningCustomersLabel->setText(QString("Returning\n%1").arg(returning));

    // Average value
    query.exec("SELECT AVG(TotalSpent) FROM Customers WHERE TotalSpent > 0");
    double avgValue = query.next() ? query.value(0).toDouble() : 0;
    m_averageValueLabel->setText(QString("Avg Lifetime Value\n$%1").arg(avgValue, 0, 'f', 2));

    // Top tier count
    query.exec("SELECT COUNT(*) FROM Customers WHERE Tier IN ('VIP', 'Platinum')");
    int topTier = query.next() ? query.value(0).toInt() : 0;
    m_topTierLabel->setText(QString("VIP/Platinum Members: %1").arg(topTier));
}

// =============================================================================
// Enhanced Analytics Dialog Implementation
// =============================================================================

EnhancedAnalyticsDialog::EnhancedAnalyticsDialog(QSqlDatabase &db, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
    , m_startDate(QDate::currentDate().addDays(-7))
    , m_endDate(QDate::currentDate())
{
    setWindowTitle("Enhanced Analytics Dashboard");
    resize(1200, 800);
    setupUi();
    refreshAllWidgets();
}

void EnhancedAnalyticsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Date range selector
    auto *controlsLayout = new QHBoxLayout;

    controlsLayout->addWidget(new QLabel("From:", this));
    m_startDateEdit = new QDateEdit(m_startDate, this);
    connect(m_startDateEdit, &QDateEdit::dateChanged, this, &EnhancedAnalyticsDialog::onDateRangeChanged);
    controlsLayout->addWidget(m_startDateEdit);

    controlsLayout->addWidget(new QLabel("To:", this));
    m_endDateEdit = new QDateEdit(m_endDate, this);
    connect(m_endDateEdit, &QDateEdit::dateChanged, this, &EnhancedAnalyticsDialog::onDateRangeChanged);
    controlsLayout->addWidget(m_endDateEdit);

    auto *refreshBtn = new QPushButton("🔄 Refresh", this);
    connect(refreshBtn, &QPushButton::clicked, this, &EnhancedAnalyticsDialog::onRefreshAll);
    controlsLayout->addWidget(refreshBtn);

    auto *exportBtn = new QPushButton("📄 Export", this);
    connect(exportBtn, &QPushButton::clicked, this, &EnhancedAnalyticsDialog::onExportReport);
    controlsLayout->addWidget(exportBtn);

    controlsLayout->addStretch();

    mainLayout->addLayout(controlsLayout);

    // Scroll area for widgets
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    auto *scrollWidget = new QWidget();
    auto *scrollLayout = new QVBoxLayout(scrollWidget);

    // Add all analytics widgets
    m_salesTrendWidget = new SalesTrendWidget(m_db, this);
    scrollLayout->addWidget(m_salesTrendWidget);

    m_categoryWidget = new CategoryBreakdownWidget(m_db, this);
    scrollLayout->addWidget(m_categoryWidget);

    m_topProductsWidget = new TopProductsWidget(m_db, this);
    scrollLayout->addWidget(m_topProductsWidget);

    m_paymentWidget = new PaymentAnalysisWidget(m_db, this);
    scrollLayout->addWidget(m_paymentWidget);

    m_customerWidget = new CustomerAnalyticsWidget(m_db, this);
    scrollLayout->addWidget(m_customerWidget);

    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // Close button
    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeBtn);
}

void EnhancedAnalyticsDialog::onDateRangeChanged()
{
    m_startDate = m_startDateEdit->date();
    m_endDate = m_endDateEdit->date();
    refreshAllWidgets();
}

void EnhancedAnalyticsDialog::onRefreshAll()
{
    refreshAllWidgets();
}

void EnhancedAnalyticsDialog::refreshAllWidgets()
{
    m_salesTrendWidget->setDateRange(m_startDate, m_endDate);
    m_salesTrendWidget->refresh();

    m_categoryWidget->setDateRange(m_startDate, m_endDate);
    m_categoryWidget->refresh();

    m_topProductsWidget->setDateRange(m_startDate, m_endDate);
    m_topProductsWidget->refresh();

    m_paymentWidget->setDateRange(m_startDate, m_endDate);
    m_paymentWidget->refresh();

    m_customerWidget->refresh();
}

void EnhancedAnalyticsDialog::onExportReport()
{
    QMessageBox::information(this, "Export", "Report export functionality would go here");
}

void EnhancedAnalyticsDialog::onPrintReport()
{
    QMessageBox::information(this, "Print", "Print functionality would go here");
}

// =============================================================================
// Quick Stats Widget Implementation
// =============================================================================

QuickStatsWidget::QuickStatsWidget(QSqlDatabase &db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
{
    setupUi();
    loadTodayStats();
}

void QuickStatsWidget::setupUi()
{
    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(10);

    auto makeStatChip = [this](const char *kind) -> QLabel* {
        QLabel *l = new QLabel(this);
        l->setProperty("role", "chip");
        l->setProperty("kind", kind);
        l->setProperty("textScale", "md");
        return l;
    };

    m_todaySalesLabel = makeStatChip("primary");
    layout->addWidget(m_todaySalesLabel);

    m_todayTransactionsLabel = makeStatChip("info");
    layout->addWidget(m_todayTransactionsLabel);

    m_todayCustomersLabel = makeStatChip("warning");
    layout->addWidget(m_todayCustomersLabel);

    m_averageTicketLabel = makeStatChip("tertiary");
    layout->addWidget(m_averageTicketLabel);
}

void QuickStatsWidget::refresh()
{
    loadTodayStats();
}

void QuickStatsWidget::loadTodayStats()
{
    QSqlQuery query(m_db);

    // Today's sales
    query.exec(R"(
        SELECT SUM(p.RegularPrice * s.Quantity)
        FROM Sales s
        JOIN Products p ON s.ProductID = p.ProductID
        WHERE DATE(SaleDatetime) = date('now')
    )");
    double todaySales = query.next() ? query.value(0).toDouble() : 0;
    m_todaySalesLabel->setText(QString("Today: $%1").arg(todaySales, 0, 'f', 2));

    // Today's transactions
    query.exec("SELECT COUNT(DISTINCT SaleID) FROM Sales WHERE DATE(SaleDatetime) = date('now')");
    int transactions = query.next() ? query.value(0).toInt() : 0;
    m_todayTransactionsLabel->setText(QString("%1 Sales").arg(transactions));

    // Unique customers today (would need customer tracking in sales)
    m_todayCustomersLabel->setText("-- Customers");

    // Average ticket
    double avgTicket = transactions > 0 ? todaySales / transactions : 0;
    m_averageTicketLabel->setText(QString("Avg: $%1").arg(avgTicket, 0, 'f', 2));
}
