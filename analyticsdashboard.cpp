// =============================================================================
// analyticsdashboard.cpp — Implementation of AnalyticsDashboard (see
// analyticsdashboard.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Runs its own SQL directly against the singleton Database connection;
//    profit/margin comes from the cost_price snapshots stored in sale_items.
//  - calculateMetrics()/getTopProducts()/getSlowMovingProducts()/
//    getHourlySalesData() are bounded by the user-chosen start/end dates;
//    results render into QTableWidgets, and onExportClicked() writes CSV.
// =============================================================================
#include "analyticsdashboard.h"
#include "colorscheme.h"
#include "database.h"
#include "cart.h"          // formatMoney()
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QTabWidget>
#include <QApplication>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Local helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// roundCents() / formatMoney() come from cart.h (single source of truth).

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

AnalyticsDashboard::AnalyticsDashboard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Analytics Dashboard");
    resize(1200, 800);

    startDate = QDateTime::currentDateTime().addDays(-7);
    endDate   = QDateTime::currentDateTime();

    setupUI();
    loadData();

    // Re-render so the per-cell highlight brushes pick up the new scheme;
    // widget styling follows the app-wide stylesheet automatically.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](bool) { loadData(); });
}

AnalyticsDashboard::~AnalyticsDashboard() {}

// ─────────────────────────────────────────────────────────────────────────────
// setupUI
// ─────────────────────────────────────────────────────────────────────────────

void AnalyticsDashboard::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Header
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("📊 Sales Analytics Dashboard");
    titleLabel->setProperty("role", "dialogTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Date range
    QGroupBox *dateRangeGroup = new QGroupBox("Date Range");
    QHBoxLayout *dateLayout = new QHBoxLayout(dateRangeGroup);

    dateLayout->addWidget(new QLabel("Quick Range:"));
    quickRangeCombo = new QComboBox();
    quickRangeCombo->addItems({"Today","Yesterday","Last 7 Days","Last 30 Days",
                               "This Month","Last Month","This Year","Custom"});
    quickRangeCombo->setCurrentText("Last 7 Days");
    connect(quickRangeCombo, &QComboBox::currentTextChanged,
            this, &AnalyticsDashboard::onQuickRangeSelected);
    dateLayout->addWidget(quickRangeCombo);

    dateLayout->addWidget(new QLabel("From:"));
    startDateEdit = new QDateEdit();
    startDateEdit->setDate(startDate.date());
    startDateEdit->setCalendarPopup(true);
    connect(startDateEdit, &QDateEdit::dateChanged,
            this, &AnalyticsDashboard::onDateRangeChanged);
    dateLayout->addWidget(startDateEdit);

    dateLayout->addWidget(new QLabel("To:"));
    endDateEdit = new QDateEdit();
    endDateEdit->setDate(endDate.date());
    endDateEdit->setCalendarPopup(true);
    connect(endDateEdit, &QDateEdit::dateChanged,
            this, &AnalyticsDashboard::onDateRangeChanged);
    dateLayout->addWidget(endDateEdit);

    refreshBtn = new QPushButton("🔄 Refresh");
    refreshBtn->setProperty("kind", "primary");
    connect(refreshBtn, &QPushButton::clicked,
            this, &AnalyticsDashboard::onRefreshClicked);
    dateLayout->addWidget(refreshBtn);

    exportBtn = new QPushButton("📥 Export CSV");
    exportBtn->setProperty("kind", "info");
    connect(exportBtn, &QPushButton::clicked,
            this, &AnalyticsDashboard::onExportClicked);
    dateLayout->addWidget(exportBtn);

    dateLayout->addStretch();
    mainLayout->addWidget(dateRangeGroup);

    setupMetricsSection();
    mainLayout->addWidget(metricsGroup);

    setupTabbedAnalyticsSection();
    mainLayout->addWidget(analyticsTabWidget);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton *closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);
    mainLayout->addLayout(buttonLayout);
}

// ─────────────────────────────────────────────────────────────────────────────
// Metrics section
// ─────────────────────────────────────────────────────────────────────────────

void AnalyticsDashboard::setupMetricsSection()
{
    metricsGroup = new QGroupBox("Key Metrics");
    QGridLayout *metricsLayout = new QGridLayout(metricsGroup);
    metricsLayout->setSpacing(15);

    // Cards inherit the app-wide QGroupBox styling; values are coloured via
    // the statValue/statValueLg roles + kind selectors in appstyle.cpp.
    auto makeMetricLabel = [](const QString &text, const char *role,
                              const char *kind) -> QLabel* {
        QLabel *l = new QLabel(text);
        l->setProperty("role", role);
        l->setProperty("kind", kind);
        l->setAlignment(Qt::AlignCenter);
        return l;
    };

    // Row 0
    QGroupBox *salesCard = new QGroupBox("Total Sales");
    totalSalesLabel = makeMetricLabel(formatMoney(0), "statValueLg", "success");
    (new QVBoxLayout(salesCard))->addWidget(totalSalesLabel);
    metricsLayout->addWidget(salesCard, 0, 0);

    QGroupBox *profitCard = new QGroupBox("Gross Profit");
    QVBoxLayout *pl = new QVBoxLayout(profitCard);
    profitLabel = makeMetricLabel(formatMoney(0), "statValueLg", "success");
    pl->addWidget(profitLabel);
    profitMarginLabel = new QLabel("Margin: 0%");
    profitMarginLabel->setProperty("kind", "secondary");
    profitMarginLabel->setAlignment(Qt::AlignCenter);
    pl->addWidget(profitMarginLabel);
    metricsLayout->addWidget(profitCard, 0, 1);

    QGroupBox *transCard = new QGroupBox("Transactions");
    transactionsLabel = makeMetricLabel("0", "statValueLg", "info");
    (new QVBoxLayout(transCard))->addWidget(transactionsLabel);
    metricsLayout->addWidget(transCard, 0, 2);

    QGroupBox *avgCard = new QGroupBox("Avg Transaction");
    avgTransactionLabel = makeMetricLabel(formatMoney(0), "statValueLg", "tertiary");
    (new QVBoxLayout(avgCard))->addWidget(avgTransactionLabel);
    metricsLayout->addWidget(avgCard, 0, 3);

    // Row 1
    QGroupBox *taxCard = new QGroupBox("Tax Collected");
    taxCollectedLabel = makeMetricLabel(formatMoney(0), "statValue", "secondary");
    (new QVBoxLayout(taxCard))->addWidget(taxCollectedLabel);
    metricsLayout->addWidget(taxCard, 1, 0);

    QGroupBox *discountCard = new QGroupBox("Discounts Given");
    discountsLabel = makeMetricLabel(formatMoney(0), "statValue", "secondary");
    (new QVBoxLayout(discountCard))->addWidget(discountsLabel);
    metricsLayout->addWidget(discountCard, 1, 1);

    QGroupBox *itemsCard = new QGroupBox("Items Sold");
    itemsSoldLabel = makeMetricLabel("0", "statValue", "secondary");
    (new QVBoxLayout(itemsCard))->addWidget(itemsSoldLabel);
    metricsLayout->addWidget(itemsCard, 1, 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab sections
// ─────────────────────────────────────────────────────────────────────────────

void AnalyticsDashboard::setupTabbedAnalyticsSection()
{
    analyticsTabWidget = new QTabWidget();
    setupTopProductsSection();
    analyticsTabWidget->addTab(topProductsWidget, "🏆 Top Products");
    setupSlowMovingSection();
    analyticsTabWidget->addTab(slowMovingWidget, "⚠️ Slow Moving");
    setupHourlySalesSection();
    analyticsTabWidget->addTab(hourlySalesWidget, "🕐 Hourly Sales");
}

void AnalyticsDashboard::setupTopProductsSection()
{
    topProductsWidget = new QWidget();
    QVBoxLayout *l = new QVBoxLayout(topProductsWidget);
    l->setContentsMargins(0,0,0,0);
    topProductsGroup = new QGroupBox("Top 10 Best Selling Products");
    QVBoxLayout *gl = new QVBoxLayout(topProductsGroup);
    topProductsTable = new QTableWidget();
    topProductsTable->setColumnCount(5);
    topProductsTable->setHorizontalHeaderLabels(
        {"Rank","Product","Category","Units Sold","Revenue"});
    topProductsTable->horizontalHeader()->setStretchLastSection(true);
    topProductsTable->verticalHeader()->setVisible(false);
    topProductsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    topProductsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    topProductsTable->setAlternatingRowColors(true);
    gl->addWidget(topProductsTable);
    l->addWidget(topProductsGroup);
}

void AnalyticsDashboard::setupSlowMovingSection()
{
    slowMovingWidget = new QWidget();
    QVBoxLayout *l = new QVBoxLayout(slowMovingWidget);
    l->setContentsMargins(0,0,0,0);
    slowMovingGroup = new QGroupBox("Slow Moving Products (Need Attention)");
    slowMovingGroup->setProperty("kind", "danger");
    QVBoxLayout *gl = new QVBoxLayout(slowMovingGroup);
    slowMovingTable = new QTableWidget();
    slowMovingTable->setColumnCount(4);
    slowMovingTable->setHorizontalHeaderLabels(
        {"Product","Category","Units Sold","Revenue"});
    slowMovingTable->horizontalHeader()->setStretchLastSection(true);
    slowMovingTable->verticalHeader()->setVisible(false);
    slowMovingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    slowMovingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    slowMovingTable->setAlternatingRowColors(true);
    gl->addWidget(slowMovingTable);
    l->addWidget(slowMovingGroup);
}

void AnalyticsDashboard::setupHourlySalesSection()
{
    hourlySalesWidget = new QWidget();
    QVBoxLayout *l = new QVBoxLayout(hourlySalesWidget);
    l->setContentsMargins(0,0,0,0);
    hourlySalesGroup = new QGroupBox("Sales by Hour of Day");
    QVBoxLayout *gl = new QVBoxLayout(hourlySalesGroup);
    hourlySalesTable = new QTableWidget();
    hourlySalesTable->setColumnCount(4);
    hourlySalesTable->setHorizontalHeaderLabels(
        {"Hour","Transactions","Revenue","% of Total"});
    hourlySalesTable->horizontalHeader()->setStretchLastSection(true);
    hourlySalesTable->verticalHeader()->setVisible(false);
    hourlySalesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    hourlySalesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hourlySalesTable->setAlternatingRowColors(true);
    gl->addWidget(hourlySalesTable);
    l->addWidget(hourlySalesGroup);
}

// ─────────────────────────────────────────────────────────────────────────────
// Data — roundCents() applied at every DB read site
// ─────────────────────────────────────────────────────────────────────────────

void AnalyticsDashboard::loadData()
{
    updateMetricsDisplay(calculateMetrics());
    updateTopProductsTable(getTopProducts(10));
    updateSlowMovingTable(getSlowMovingProducts(10));
    updateHourlyChart(getHourlySalesData());
}

SalesMetrics AnalyticsDashboard::calculateMetrics()
{
    SalesMetrics metrics;
    QSqlQuery query;
    query.prepare(
        "SELECT SUM(subtotal) as total_subtotal, SUM(tax) as total_tax,"
        "       SUM(discount) as total_discount, SUM(total) as total_sales,"
        "       COUNT(*) as transaction_count"
        " FROM sales WHERE sale_date >= ? AND sale_date <= ?");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));

    if (query.exec() && query.next()) {
        metrics.totalSales     = roundCents(query.value("total_sales").toDouble());
        metrics.taxCollected   = roundCents(query.value("total_tax").toDouble());
        metrics.discountsGiven = roundCents(query.value("total_discount").toDouble());
        metrics.transactionCount = query.value("transaction_count").toInt();
        if (metrics.transactionCount > 0)
            metrics.averageTransaction =
                roundCents(metrics.totalSales / metrics.transactionCount);
    }

    query.prepare(
        "SELECT SUM(si.quantity) as total_items"
        " FROM sale_items si JOIN sales s ON si.sale_id = s.id"
        " WHERE s.sale_date >= ? AND s.sale_date <= ?");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));
    if (query.exec() && query.next())
        metrics.itemsSold = query.value("total_items").toInt();

    metrics.totalProfit = roundCents(
        Database::instance().getActualGrossProfit(
            startDate.toString("yyyy-MM-dd"),
            endDate.toString("yyyy-MM-dd")));

    return metrics;
}

QVector<ProductPerformance> AnalyticsDashboard::getTopProducts(int limit)
{
    QVector<ProductPerformance> products;
    QSqlQuery query;
    query.prepare(
        "SELECT si.product_name, p.category,"
        "       SUM(si.quantity) as units_sold, SUM(si.subtotal) as revenue,"
        "       SUM((si.price - si.cost_price)*si.quantity) as actual_profit,"
        "       COUNT(DISTINCT si.sale_id) as times_ordered"
        " FROM sale_items si JOIN sales s ON si.sale_id = s.id"
        " LEFT JOIN products p ON si.product_name = p.name"
        " WHERE s.sale_date >= ? AND s.sale_date <= ?"
        " GROUP BY si.product_name ORDER BY revenue DESC LIMIT ?");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));
    query.addBindValue(limit);

    if (query.exec()) {
        while (query.next()) {
            ProductPerformance p;
            p.productName  = query.value("product_name").toString();
            p.category     = query.value("category").toString();
            p.unitsSold    = query.value("units_sold").toInt();
            p.revenue      = roundCents(query.value("revenue").toDouble());
            p.profit       = roundCents(query.value("actual_profit").toDouble());
            p.timesOrdered = query.value("times_ordered").toInt();
            products.append(p);
        }
    }
    return products;
}

QVector<ProductPerformance> AnalyticsDashboard::getSlowMovingProducts(int limit)
{
    QVector<ProductPerformance> products;
    QSqlQuery query;
    query.prepare(
        "SELECT si.product_name, p.category,"
        "       SUM(si.quantity) as units_sold, SUM(si.subtotal) as revenue"
        " FROM sale_items si JOIN sales s ON si.sale_id = s.id"
        " LEFT JOIN products p ON si.product_name = p.name"
        " WHERE s.sale_date >= ? AND s.sale_date <= ?"
        " GROUP BY si.product_name HAVING units_sold > 0"
        " ORDER BY units_sold ASC LIMIT ?");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));
    query.addBindValue(limit);

    if (query.exec()) {
        while (query.next()) {
            ProductPerformance p;
            p.productName = query.value("product_name").toString();
            p.category    = query.value("category").toString();
            p.unitsSold   = query.value("units_sold").toInt();
            p.revenue     = roundCents(query.value("revenue").toDouble());
            products.append(p);
        }
    }
    return products;
}

QVector<HourlySales> AnalyticsDashboard::getHourlySalesData()
{
    QVector<HourlySales> hourlyData;
    for (int i = 0; i < 24; ++i) { HourlySales d; d.hour = i; hourlyData.append(d); }

    QSqlQuery query;
    query.prepare(
        "SELECT CAST(strftime('%H', sale_date) AS INTEGER) as hour,"
        "       COUNT(*) as transaction_count, SUM(total) as total_sales"
        " FROM sales WHERE sale_date >= ? AND sale_date <= ?"
        " GROUP BY hour ORDER BY hour");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));

    if (query.exec()) {
        while (query.next()) {
            int h = query.value("hour").toInt();
            if (h >= 0 && h < 24) {
                hourlyData[h].transactions = query.value("transaction_count").toInt();
                hourlyData[h].sales = roundCents(query.value("total_sales").toDouble());
            }
        }
    }
    return hourlyData;
}

// ─────────────────────────────────────────────────────────────────────────────
// Display updates
// ─────────────────────────────────────────────────────────────────────────────

void AnalyticsDashboard::updateMetricsDisplay(const SalesMetrics &metrics)
{
    totalSalesLabel->setText(formatCurrency(metrics.totalSales));
    profitLabel->setText(formatCurrency(metrics.totalProfit));
    transactionsLabel->setText(QString::number(metrics.transactionCount));
    avgTransactionLabel->setText(formatCurrency(metrics.averageTransaction));
    taxCollectedLabel->setText(formatCurrency(metrics.taxCollected));
    discountsLabel->setText(formatCurrency(metrics.discountsGiven));
    itemsSoldLabel->setText(QString::number(metrics.itemsSold));

    double margin = (metrics.totalSales > 0.0)
                        ? roundCents((metrics.totalProfit / metrics.totalSales) * 100.0)
                        : 0.0;
    profitMarginLabel->setText(QString("Margin: %1%").arg(margin, 0, 'f', 1));
}

void AnalyticsDashboard::updateTopProductsTable(
    const QVector<ProductPerformance> &products)
{
    const ColorScheme scheme = getColorScheme();
    topProductsTable->setSortingEnabled(false);
    topProductsTable->setRowCount(products.size());

    for (int i = 0; i < products.size(); ++i) {
        const ProductPerformance &p = products[i];
        QString rank = QString::number(i + 1);
        if      (i == 0) rank = "🥇 1";
        else if (i == 1) rank = "🥈 2";
        else if (i == 2) rank = "🥉 3";

        QTableWidgetItem *ri = new QTableWidgetItem(rank);
        ri->setTextAlignment(Qt::AlignCenter);
        topProductsTable->setItem(i, 0, ri);
        topProductsTable->setItem(i, 1, new QTableWidgetItem(p.productName));
        topProductsTable->setItem(i, 2, new QTableWidgetItem(p.category));

        QTableWidgetItem *ui = new QTableWidgetItem(QString::number(p.unitsSold));
        ui->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        topProductsTable->setItem(i, 3, ui);

        QTableWidgetItem *revi = new QTableWidgetItem(formatCurrency(p.revenue));
        revi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        topProductsTable->setItem(i, 4, revi);

        if (i < 3) {
            for (int col = 0; col < 5; ++col) {
                if (auto *item = topProductsTable->item(i, col)) {
                    item->setBackground(QBrush(QColor(scheme.infoBg)));
                    item->setFont(QFont(item->font().family(), -1, QFont::Bold));
                }
            }
        }
    }
    topProductsTable->resizeColumnsToContents();
    topProductsTable->setSortingEnabled(true);
}

void AnalyticsDashboard::updateSlowMovingTable(
    const QVector<ProductPerformance> &products)
{
    const ColorScheme scheme = getColorScheme();
    slowMovingTable->setSortingEnabled(false);
    slowMovingTable->setRowCount(products.size());

    for (int i = 0; i < products.size(); ++i) {
        const ProductPerformance &p = products[i];
        slowMovingTable->setItem(i, 0, new QTableWidgetItem(p.productName));
        slowMovingTable->setItem(i, 1, new QTableWidgetItem(p.category));

        QTableWidgetItem *ui = new QTableWidgetItem(QString::number(p.unitsSold));
        ui->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        slowMovingTable->setItem(i, 2, ui);

        QTableWidgetItem *ri = new QTableWidgetItem(formatCurrency(p.revenue));
        ri->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        slowMovingTable->setItem(i, 3, ri);

        if (p.unitsSold < 5)
            for (int col = 0; col < 4; ++col)
                if (auto *item = slowMovingTable->item(i, col))
                    item->setBackground(QBrush(QColor(scheme.errorBg)));
    }
    slowMovingTable->resizeColumnsToContents();
    slowMovingTable->setSortingEnabled(true);
}

void AnalyticsDashboard::updateHourlyChart(const QVector<HourlySales> &data)
{
    const ColorScheme scheme = getColorScheme();
    hourlySalesTable->setSortingEnabled(false);
    hourlySalesTable->setRowCount(24);

    double totalSales = 0.0;
    for (const HourlySales &d : data) totalSales += d.sales;
    totalSales = roundCents(totalSales);

    for (int i = 0; i < data.size(); ++i) {
        const HourlySales &d = data[i];

        hourlySalesTable->setItem(i, 0, new QTableWidgetItem(
                                            QString("%1:00 - %2:00")
                                                .arg(d.hour, 2, 10, QChar('0'))
                                                .arg((d.hour+1)%24, 2, 10, QChar('0'))));

        QTableWidgetItem *ti = new QTableWidgetItem(QString::number(d.transactions));
        ti->setTextAlignment(Qt::AlignCenter);
        hourlySalesTable->setItem(i, 1, ti);

        QTableWidgetItem *si = new QTableWidgetItem(formatCurrency(d.sales));
        si->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hourlySalesTable->setItem(i, 2, si);

        // Round to 1 dp to avoid 99.9999% style artefacts
        double pct = (totalSales > 0.0)
                         ? std::round((d.sales / totalSales) * 1000.0) / 10.0
                         : 0.0;
        QTableWidgetItem *pi = new QTableWidgetItem(formatPercentage(pct));
        pi->setTextAlignment(Qt::AlignCenter);
        hourlySalesTable->setItem(i, 3, pi);

        if (d.sales > 0.0 && pct > 10.0) {
            for (int col = 0; col < 4; ++col)
                if (auto *item = hourlySalesTable->item(i, col)) {
                    item->setBackground(QBrush(QColor(scheme.warningBg)));
                    item->setFont(QFont(item->font().family(), -1, QFont::Bold));
                }
        }
    }
    hourlySalesTable->resizeColumnsToContents();
    hourlySalesTable->setSortingEnabled(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void AnalyticsDashboard::onDateRangeChanged()
{
    startDate = QDateTime(startDateEdit->date(), QTime(0,  0,  0));
    endDate   = QDateTime(endDateEdit->date(),   QTime(23, 59, 59));

    // Mark combo Custom without triggering onQuickRangeSelected again
    quickRangeCombo->blockSignals(true);
    quickRangeCombo->setCurrentText("Custom");
    quickRangeCombo->blockSignals(false);

    loadData();
}

void AnalyticsDashboard::onRefreshClicked()
{
    loadData();
    QMessageBox::information(this, "Refreshed", "Analytics data has been refreshed.");
}

void AnalyticsDashboard::onExportClicked()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Analytics",
                                                    QString("Analytics_%1.csv")
                                                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd")),
                                                    "CSV Files (*.csv)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Error", "Could not create export file.");
        return;
    }

    QTextStream out(&file);
    out << "Keynetik POS - Analytics Report\n"
        << "Generated: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n"
        << "Period: " << startDate.toString("yyyy-MM-dd")
        << " to " << endDate.toString("yyyy-MM-dd") << "\n\n";

    SalesMetrics m = calculateMetrics();
    out << "KEY METRICS\n"
        << "Total Sales,"         << m.totalSales         << "\n"
        << "Gross Profit,"        << m.totalProfit        << "\n"
        << "Transactions,"        << m.transactionCount   << "\n"
        << "Average Transaction," << m.averageTransaction << "\n"
        << "Tax Collected,"       << m.taxCollected       << "\n"
        << "Discounts Given,"     << m.discountsGiven     << "\n"
        << "Items Sold,"          << m.itemsSold          << "\n\n";

    out << "TOP 10 PRODUCTS\nRank,Product,Category,Units Sold,Revenue\n";
    const auto top = getTopProducts(10);
    for (int i = 0; i < top.size(); ++i)
        out << (i+1) << "," << top[i].productName << ","
            << top[i].category << "," << top[i].unitsSold
            << "," << top[i].revenue << "\n";

    file.close();
    QMessageBox::information(this, "Export Complete",
                             QString("Analytics exported to:\n%1").arg(filename));
}

void AnalyticsDashboard::onQuickRangeSelected(const QString &range)
{
    if (range == "Custom") return; // set by onDateRangeChanged; nothing to do

    QDateTime now = QDateTime::currentDateTime();

    if      (range == "Today")
    { startDate = QDateTime(now.date(), QTime(0,0,0)); endDate = now; }
    else if (range == "Yesterday")
    { startDate = QDateTime(now.date().addDays(-1), QTime(0,0,0));
        endDate   = QDateTime(now.date().addDays(-1), QTime(23,59,59)); }
    else if (range == "Last 7 Days")
    { startDate = now.addDays(-7); endDate = now; }
    else if (range == "Last 30 Days")
    { startDate = now.addDays(-30); endDate = now; }
    else if (range == "This Month")
    { startDate = QDateTime(QDate(now.date().year(), now.date().month(), 1), QTime(0,0,0));
        endDate = now; }
    else if (range == "Last Month") {
        QDate lm = now.date().addMonths(-1);
        startDate = QDateTime(QDate(lm.year(), lm.month(), 1), QTime(0,0,0));
        endDate   = QDateTime(QDate(lm.year(), lm.month(), lm.daysInMonth()), QTime(23,59,59));
    }
    else if (range == "This Year")
    { startDate = QDateTime(QDate(now.date().year(), 1, 1), QTime(0,0,0));
        endDate = now; }
    else return;

    // Update the date edit widgets WITHOUT firing onDateRangeChanged
    startDateEdit->blockSignals(true);
    endDateEdit->blockSignals(true);
    startDateEdit->setDate(startDate.date());
    endDateEdit->setDate(endDate.date());
    startDateEdit->blockSignals(false);
    endDateEdit->blockSignals(false);

    loadData(); // called exactly once
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

QString AnalyticsDashboard::formatCurrency(double amount)
{
    // formatMoney() applies roundCents() (prevents "KSh -0.00") and the
    // configured currency symbol.
    return formatMoney(amount);
}

QString AnalyticsDashboard::formatPercentage(double value)
{
    return QString("%1%").arg(value, 0, 'f', 1);
}
