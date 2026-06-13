// =============================================================================
// analyticsdialog.cpp — Implementation of AnalyticsDialog (see
// analyticsdialog.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - All numbers come from AnalyticsManager; this file is pure presentation.
//  - Sections are rendered as themed HTML into QTextBrowsers: the build*/
//    get*Style helpers assemble fragments coloured from the active
//    ColorScheme, so a theme switch just re-renders the same data.
//  - The period combo maps to currentPeriodDays (7/30/90...) and re-runs all
//    queries via loadAnalyticsData().
// =============================================================================
#include "analyticsdialog.h"
#include "analyticsmanager.h"
#include "appstyle.h"
#include <QDebug>
#include <QApplication>
#include <QPalette>
#include <QDateTime>
#include <QEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QSizePolicy>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

AnalyticsDialog::AnalyticsDialog(AnalyticsManager *manager, QWidget *parent)
    : QDialog(parent)
    , analyticsManager(manager)
    , currentPeriodDays(30)
{
    setWindowTitle("Analytics Dashboard");
    resize(950, 720);
    setupUi();
    initializePeriodSelector();
    connectSignals();
    loadAnalyticsData();
}

// ============================================================================
// UI SETUP  — replaces the .ui file entirely
// ============================================================================

void AnalyticsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // ── Top control bar ───────────────────────────────────────────────────
    auto *topBar = new QHBoxLayout();

    auto *titleLabel = new QLabel("📊 Analytics Dashboard", this);
    QFont tf = titleLabel->font();
    tf.setPointSize(14);
    tf.setBold(true);
    titleLabel->setFont(tf);
    topBar->addWidget(titleLabel);
    topBar->addStretch();
    topBar->addWidget(new QLabel("Period:", this));

    periodComboBox = new QComboBox(this);
    periodComboBox->setMinimumWidth(140);
    topBar->addWidget(periodComboBox);

    refreshButton = new QPushButton("🔄 Refresh", this);
    refreshButton->setProperty("kind", "info");
    topBar->addWidget(refreshButton);

    closeButton = new QPushButton("✖ Close", this);
    closeButton->setProperty("kind", "danger");
    topBar->addWidget(closeButton);

    mainLayout->addLayout(topBar);

    // ── Status label ──────────────────────────────────────────────────────
    statusLabel = new QLabel(this);
    statusLabel->setProperty("kind", "secondary");
    statusLabel->setProperty("textScale", "sm");
    mainLayout->addWidget(statusLabel);

    // ── Tab widget ────────────────────────────────────────────────────────
    tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);

    // ── Revenue tab ───────────────────────────────────────────────────────
    {
        auto *tab  = new QWidget();
        auto *grid = new QGridLayout(tab);
        grid->setSpacing(16);
        grid->setContentsMargins(16, 16, 16, 16);

        auto makeKey = [&](const QString &t) {
            auto *l = new QLabel(t, tab);
            l->setProperty("kind", "secondary");
            l->setProperty("textScale", "sm");
            return l;
        };
        auto makeVal = [&](const QString &t) {
            auto *l = new QLabel(t, tab);
            QFont f = l->font(); f.setPointSize(16); f.setBold(true); l->setFont(f);
            return l;
        };

        int r = 0;
        grid->addWidget(makeKey("Today's Revenue:"), r, 0);
        todayRevenueLabel = makeVal("$0.00");
        todayRevenueLabel->setProperty("kind", "success");
        todayRevenueLabel->setProperty("role", "statValueLg");
        grid->addWidget(todayRevenueLabel, r++, 1);

        grid->addWidget(makeKey("Change vs Yesterday:"), r, 0);
        todayChangeLabel = new QLabel("$0.00 vs yesterday", tab);
        todayChangeLabel->setProperty("bold", "true");
        grid->addWidget(todayChangeLabel, r++, 1);

        auto *sep = new QFrame(tab);
        sep->setFrameShape(QFrame::HLine);
        sep->setProperty("role", "hline");
        grid->addWidget(sep, r++, 0, 1, 2);

        grid->addWidget(makeKey("Yesterday:"), r, 0);
        yesterdayRevenueLabel = makeVal("$0.00");
        grid->addWidget(yesterdayRevenueLabel, r++, 1);

        grid->addWidget(makeKey("This Week:"), r, 0);
        weekRevenueLabel = makeVal("$0.00");
        grid->addWidget(weekRevenueLabel, r++, 1);

        grid->addWidget(makeKey("This Month:"), r, 0);
        monthRevenueLabel = makeVal("$0.00");
        grid->addWidget(monthRevenueLabel, r++, 1);

        auto *sep2 = new QFrame(tab);
        sep2->setFrameShape(QFrame::HLine);
        sep2->setProperty("role", "hline");
        grid->addWidget(sep2, r++, 0, 1, 2);

        grid->addWidget(makeKey("Growth Rate:"), r, 0);
        auto *gc = new QWidget(tab);
        auto *gl = new QHBoxLayout(gc);
        gl->setContentsMargins(0,0,0,0);
        growthIconLabel = new QLabel("📈", gc);
        growthIconLabel->setProperty("role", "statValue");
        gl->addWidget(growthIconLabel);
        growthRateLabel = new QLabel("0.0%", gc);
        growthRateLabel->setProperty("role", "statValue");
        gl->addWidget(growthRateLabel);
        gl->addStretch();
        grid->addWidget(gc, r++, 1);

        grid->setRowStretch(r, 1);
        grid->setColumnStretch(1, 1);
        tabWidget->addTab(tab, "💰 Revenue");
    }

    // ── Transactions tab ──────────────────────────────────────────────────
    {
        auto *tab  = new QWidget();
        auto *grid = new QGridLayout(tab);
        grid->setSpacing(16);
        grid->setContentsMargins(16, 16, 16, 16);

        auto makeKey = [&](const QString &t) {
            auto *l = new QLabel(t, tab);
            l->setProperty("kind", "secondary");
            l->setProperty("textScale", "sm");
            return l;
        };
        auto makeVal = [&](const QString &t) {
            auto *l = new QLabel(t, tab);
            QFont f = l->font(); f.setPointSize(16); f.setBold(true); l->setFont(f);
            return l;
        };

        int r = 0;
        grid->addWidget(makeKey("Today's Transactions:"), r, 0);
        todayTransactionsLabel = makeVal("0");
        todayTransactionsLabel->setProperty("kind", "info");
        todayTransactionsLabel->setProperty("role", "statValueLg");
        grid->addWidget(todayTransactionsLabel, r++, 1);

        grid->addWidget(makeKey("This Week:"), r, 0);
        weekTransactionsLabel = makeVal("0");
        grid->addWidget(weekTransactionsLabel, r++, 1);

        grid->addWidget(makeKey("Avg Transaction Value:"), r, 0);
        avgTransactionLabel = makeVal("$0.00");
        avgTransactionLabel->setProperty("kind", "tertiary");
        grid->addWidget(avgTransactionLabel, r++, 1);

        grid->setRowStretch(r, 1);
        grid->setColumnStretch(1, 1);
        tabWidget->addTab(tab, "🧾 Transactions");
    }

    // ── Browser tabs ──────────────────────────────────────────────────────
    auto makeBrowserTab = [&](const QString &tabLabel, QTextBrowser *&ptr) {
        auto *tab    = new QWidget();
        auto *layout = new QVBoxLayout(tab);
        layout->setContentsMargins(4,4,4,4);
        ptr = new QTextBrowser(tab);
        ptr->setOpenExternalLinks(false);
        layout->addWidget(ptr);
        tabWidget->addTab(tab, tabLabel);
    };

    makeBrowserTab("🏆 Top Products", topProductsText);
    makeBrowserTab("📂 Categories",   categoryText);
    makeBrowserTab("🕐 Peak Hours",   peakHoursText);
    makeBrowserTab("⚠️ Slow Moving",  bottomProductsText);

    mainLayout->addWidget(tabWidget, 1);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void AnalyticsDialog::initializePeriodSelector()
{
    const QVector<QPair<QString, int>> periods = {
        {"Last 7 Days",  7},
        {"Last 30 Days", 30},
        {"Last 90 Days", 90}
    };
    for (const auto &[label, days] : periods)
        periodComboBox->addItem(label, days);
    periodComboBox->setCurrentIndex(1);
}

void AnalyticsDialog::connectSignals()
{
    connect(refreshButton, &QPushButton::clicked,
            this, &AnalyticsDialog::on_refreshButton_clicked);
    connect(closeButton, &QPushButton::clicked,
            this, &AnalyticsDialog::on_closeButton_clicked);
    connect(periodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalyticsDialog::on_periodComboBox_currentIndexChanged);
}

// ============================================================================
// EVENTS
// ============================================================================

bool AnalyticsDialog::event(QEvent *e)
{
    if (e->type() == QEvent::ApplicationPaletteChange)
        onThemeChanged();
    return QDialog::event(e);
}

// ============================================================================
// SLOTS
// ============================================================================

void AnalyticsDialog::on_refreshButton_clicked()
{
    loadAnalyticsData();
    statusLabel->setText(QString("Data refreshed at %1")
                             .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void AnalyticsDialog::on_closeButton_clicked() { accept(); }

void AnalyticsDialog::on_periodComboBox_currentIndexChanged(int index)
{
    currentPeriodDays = periodComboBox->itemData(index).toInt();
    loadAnalyticsData();
}

void AnalyticsDialog::onThemeChanged() { loadAnalyticsData(); }

// ============================================================================
// COLOR SCHEME
// ============================================================================

ColorScheme AnalyticsDialog::getCurrentColorScheme() const
{
    return isDarkMode() ? getDarkColorScheme() : getLightColorScheme();
}

// ============================================================================
// DATA LOADING
// ============================================================================

void AnalyticsDialog::loadAnalyticsData()
{
    updateRevenueSection();
    updateTransactionSection();
    updateTopProductsSection();
    updateCategorySection();
    updatePeakHoursSection();
    updateBottomProductsSection();
}

// ============================================================================
// REVENUE
// ============================================================================

void AnalyticsDialog::updateRevenueSection()
{
    auto summary = analyticsManager->getDashboardSummary();

    todayRevenueLabel->setText(formatCurrency(summary.todayRevenue));
    updateChangeLabel(summary.todayRevenue - summary.yesterdayRevenue);
    yesterdayRevenueLabel->setText(formatCurrency(summary.yesterdayRevenue));
    weekRevenueLabel->setText(formatCurrency(summary.weekRevenue));
    monthRevenueLabel->setText(formatCurrency(summary.monthRevenue));
    updateGrowthRate(summary.growthRate);
}

void AnalyticsDialog::updateChangeLabel(double change)
{
    todayChangeLabel->setText(
        QString("%1%2 vs yesterday")
            .arg(change >= 0 ? "+" : "")
            .arg(formatCurrency(change)));
    setStyleProperty(todayChangeLabel, "kind", getChangeKind(change));
}

void AnalyticsDialog::updateGrowthRate(double growthRate)
{
    growthRateLabel->setText(
        QString("%1%2%").arg(growthRate > 0 ? "+" : "").arg(growthRate, 0, 'f', 1));
    setStyleProperty(growthRateLabel, "kind", getChangeKind(growthRate));
    growthIconLabel->setText(growthRate >= 0 ? "📈" : "📉");
}

// ============================================================================
// TRANSACTIONS
// ============================================================================

void AnalyticsDialog::updateTransactionSection()
{
    auto summary = analyticsManager->getDashboardSummary();
    todayTransactionsLabel->setText(QString::number(summary.todayTransactions));
    weekTransactionsLabel->setText(QString::number(summary.weekTransactions));
    avgTransactionLabel->setText(formatCurrency(summary.avgTransactionValue));
}

// ============================================================================
// TOP PRODUCTS
// ============================================================================

void AnalyticsDialog::updateTopProductsSection()
{
    auto topProducts = analyticsManager->getTopSellingProducts(5, currentPeriodDays);
    auto colors      = getCurrentColorScheme();

    if (topProducts.isEmpty()) {
        showEmptyState(topProductsText, "No sales data available yet.", colors);
        return;
    }

    static const QStringList medals = {"🥇", "🥈", "🥉", "", ""};
    QString html = getBaseTableStyle(colors);
    html += "<table><tr><th></th><th>Product</th><th>Units Sold</th><th>Revenue</th></tr>";
    for (int i = 0; i < topProducts.size(); ++i)
        html += buildTopProductRow(i < medals.size() ? medals[i] : "", topProducts[i], colors);
    html += "</table>";
    topProductsText->setHtml(html);
}

QString AnalyticsDialog::getBaseTableStyle(const ColorScheme &colors) const
{
    return QString(
               "<style>"
               "table{width:100%%;border-collapse:collapse;}"
               "th{background-color:%1;color:white;padding:10px;text-align:left;font-weight:bold;}"
               "td{padding:10px;border-bottom:1px solid %2;color:%3;}"
               "tr:hover{background-color:%4;}"
               ".medal{font-size:16pt;text-align:center;width:40px;}"
               ".revenue{color:%5;font-weight:bold;}"
               ".product-name{color:%3;font-weight:bold;font-size:11pt;}"
               ".product-category{color:%6;font-size:9pt;}"
               "</style>")
        .arg(colors.info, colors.borderColor, colors.textPrimary,
             colors.bgSecondary, colors.success, colors.textSecondary);
}

QString AnalyticsDialog::buildTopProductRow(const QString &medal,
                                            const ProductSalesStats &product,
                                            const ColorScheme &colors) const
{
    return QString(
               "<tr>"
               "<td class='medal'>%1</td>"
               "<td><div class='product-name'>%2</div>"
               "<div class='product-category'>%3</div></td>"
               "<td align='center' style='color:%4;'>%5</td>"
               "<td class='revenue'>%6</td>"
               "</tr>")
        .arg(medal, product.productName, product.category, colors.textSecondary)
        .arg(product.totalQuantitySold)
        .arg(formatCurrency(product.totalRevenue));
}

// ============================================================================
// CATEGORIES
// ============================================================================

void AnalyticsDialog::updateCategorySection()
{
    auto categories = analyticsManager->getCategorySales(currentPeriodDays);
    auto colors     = getCurrentColorScheme();

    if (categories.isEmpty()) {
        showEmptyState(categoryText, "No category data available.", colors);
        return;
    }

    QString html = getCategoryStyles(colors);
    html += buildBestCategoryHighlight(categories.first(), colors);
    html += QString("<br/><div style='color:%1;font-weight:bold;'>All Categories:</div><br/>")
                .arg(colors.textPrimary);
    html += buildCategoryItems(categories, colors);
    categoryText->setHtml(html);
}

QString AnalyticsDialog::getCategoryStyles(const ColorScheme &colors) const
{
    return QString(
               "<style>"
               ".category-item{margin:12px 0;padding:12px;background-color:%1;"
               "  border-radius:5px;border-left:3px solid %2;}"
               ".category-name{font-weight:bold;font-size:12pt;color:%3;}"
               ".category-stats{color:%4;margin-top:5px;font-size:10pt;}"
               ".progress-bar{background-color:%5;height:22px;border-radius:11px;"
               "  margin-top:8px;overflow:hidden;border:1px solid %2;}"
               ".progress-fill{background-color:%6;height:100%%;}"
               ".percentage{text-align:right;font-size:10pt;color:%4;margin-top:4px;}"
               ".best-category{background-color:%1;border-left:4px solid %7;}"
               "</style>")
        .arg(colors.bgSecondary, colors.borderColor, colors.textPrimary,
             colors.textSecondary, colors.bgSecondary, colors.info, colors.success);
}

QString AnalyticsDialog::buildBestCategoryHighlight(const CategoryStats &best,
                                                    const ColorScheme &colors) const
{
    Q_UNUSED(colors)
    return QString(
               "<div class='category-item best-category'>"
               "<div class='category-name'>⭐ Best Category: %1</div>"
               "<div class='category-stats'>%2% of total revenue | %3 units | %4</div>"
               "</div>")
        .arg(best.category)
        .arg(best.percentageOfTotal, 0, 'f', 1)
        .arg(best.totalQuantitySold)
        .arg(formatCurrency(best.totalRevenue));
}

QString AnalyticsDialog::buildCategoryItems(const QVector<CategoryStats> &categories,
                                            const ColorScheme &colors) const
{
    Q_UNUSED(colors)
    QString html;
    for (const auto &cat : categories) {
        int w = qBound(0, static_cast<int>(cat.percentageOfTotal), 100);
        html += QString(
                    "<div class='category-item'>"
                    "<div class='category-name'>%1</div>"
                    "<div class='category-stats'>%2 units sold | %3</div>"
                    "<div class='progress-bar'><div class='progress-fill' style='width:%4%%;'></div></div>"
                    "<div class='percentage'>%5%</div>"
                    "</div>")
                    .arg(cat.category).arg(cat.totalQuantitySold)
                    .arg(formatCurrency(cat.totalRevenue)).arg(w)
                    .arg(cat.percentageOfTotal, 0, 'f', 1);
    }
    return html;
}

// ============================================================================
// PEAK HOURS
// ============================================================================

void AnalyticsDialog::updatePeakHoursSection()
{
    auto summary = analyticsManager->getDashboardSummary();
    auto colors  = getCurrentColorScheme();

    if (summary.peakHours.isEmpty()) {
        showEmptyState(peakHoursText, "No peak hour data available.", colors);
        return;
    }

    QString html = getPeakHoursStyle(colors);
    html += QString("<div style='margin-bottom:15px;color:%1;font-weight:bold;'>"
                    "🔥 Busiest Times (Last 7 Days):</div>").arg(colors.textPrimary);
    html += buildPeakHourItems(summary.peakHours, colors);
    html += buildInfoBox("💡 Recommendation",
                         analyticsManager->getPeakHourRecommendation(7), colors);
    peakHoursText->setHtml(html);
}

QString AnalyticsDialog::getPeakHoursStyle(const ColorScheme &colors) const
{
    return QString(
               "<style>"
               ".peak-item{margin:10px 0;padding:12px;background-color:%1;"
               "  border-left:4px solid %2;border-radius:3px;}"
               ".peak-time{font-size:13pt;font-weight:bold;color:%3;}"
               ".peak-revenue{color:%4;font-size:11pt;margin-top:5px;}"
               "</style>")
        .arg(colors.bgSecondary, colors.warning, colors.textPrimary, colors.success);
}

QString AnalyticsDialog::buildPeakHourItems(const QVector<PeakHourInfo> &peakHours,
                                            const ColorScheme &colors) const
{
    Q_UNUSED(colors)
    static const QStringList icons = {"🔥", "⚡", "💫"};
    QString html;
    for (int i = 0; i < peakHours.size(); ++i) {
        html += QString(
                    "<div class='peak-item'>"
                    "<div class='peak-time'>%1 %2</div>"
                    "<div class='peak-revenue'>Revenue: %3</div>"
                    "</div>")
                    .arg(i < icons.size() ? icons[i] : "💫",
                         peakHours[i].timeRange,
                         formatCurrency(peakHours[i].revenue));
    }
    return html;
}

// ============================================================================
// BOTTOM PRODUCTS
// ============================================================================

void AnalyticsDialog::updateBottomProductsSection()
{
    auto bottomProducts = analyticsManager->getBottomSellingProducts(5, currentPeriodDays);
    auto colors         = getCurrentColorScheme();
    auto zeroSales      = filterZeroSales(bottomProducts);

    if (zeroSales.isEmpty()) {
        bottomProductsText->setHtml(
            QString("<p style='color:%1;font-weight:bold;'>✅ All products have sales!</p>")
                .arg(colors.success));
        return;
    }

    QString html = getWarningStyle(colors);
    html += buildWarningHeader(zeroSales.size(), colors);
    html += buildZeroSalesItems(zeroSales, colors);
    html += buildInfoBox("💡 Suggestions", getSuggestionsText(), colors);
    bottomProductsText->setHtml(html);
}

QVector<ProductSalesStats> AnalyticsDialog::filterZeroSales(
    const QVector<ProductSalesStats> &products) const
{
    QVector<ProductSalesStats> result;
    std::copy_if(products.begin(), products.end(), std::back_inserter(result),
                 [](const ProductSalesStats &p){ return p.totalQuantitySold == 0; });
    return result;
}

QString AnalyticsDialog::getWarningStyle(const ColorScheme &colors) const
{
    return QString(
               "<style>"
               ".warning-header{background-color:%1;color:%2;padding:12px;border-radius:5px;"
               "  font-weight:bold;margin-bottom:12px;border-left:4px solid %2;}"
               ".warning-item{padding:10px;border-left:4px solid %2;margin:8px 0;"
               "  background-color:%1;border-radius:3px;}"
               ".warning-item b{color:%3;}"
               ".warning-item small{color:%4;}"
               "</style>")
        .arg(colors.bgSecondary, colors.error, colors.textPrimary, colors.textSecondary);
}

QString AnalyticsDialog::buildWarningHeader(int count, const ColorScheme &colors) const
{
    return QString(
               "<div class='warning-header'>⚠️ Products with Zero Sales</div>"
               "<div style='color:%1;margin:12px 0;'>"
               "These <b>%2 product(s)</b> have had <b>no sales</b> in the last <b>%3 days</b></div>")
        .arg(colors.textPrimary).arg(count).arg(currentPeriodDays);
}

QString AnalyticsDialog::buildZeroSalesItems(const QVector<ProductSalesStats> &products,
                                             const ColorScheme &colors) const
{
    Q_UNUSED(colors)
    QString html;
    for (const auto &p : products) {
        html += QString(
                    "<div class='warning-item'>"
                    "• <b>%1</b> (%2)<br/><small>Regular Price: %3</small>"
                    "</div>")
                    .arg(p.productName, p.category, formatCurrency(p.averagePrice));
    }
    return html;
}

// ============================================================================
// UTILITY
// ============================================================================

void AnalyticsDialog::showEmptyState(QTextBrowser *widget,
                                     const QString &message,
                                     const ColorScheme &colors)
{
    widget->clear();
    widget->setHtml(QString("<p style='color:%1;padding:20px;font-size:11pt;'>%2</p>")
                        .arg(colors.textSecondary, message));
}

QString AnalyticsDialog::buildInfoBox(const QString &title,
                                      const QString &content,
                                      const ColorScheme &colors) const
{
    return QString(
               "<div style='margin-top:20px;padding:15px;background-color:%1;"
               "  border-radius:5px;border-left:4px solid %2;'>"
               "<div style='color:%3;font-weight:bold;margin-bottom:8px;'>%4</div>"
               "<div style='color:%5;line-height:1.6;'>%6</div>"
               "</div>")
        .arg(colors.bgSecondary, colors.info, colors.textPrimary,
             title, colors.textSecondary, content);
}

QString AnalyticsDialog::formatCurrency(double amount) const
{
    return QString("$%1").arg(amount, 0, 'f', 2);
}

QString AnalyticsDialog::getChangeKind(double change) const
{
    if (change > 0) return "success";
    if (change < 0) return "danger";
    return "secondary";
}