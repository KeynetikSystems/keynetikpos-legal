// =============================================================================
// analyticsdialog.h — AnalyticsDialog: the primary trend-analytics screen
// -----------------------------------------------------------------------------
// WHAT: Revenue cards (today vs yesterday with change indicator, week, month,
//       growth), transaction stats, and tabbed rich-text views for top
//       products (with medals), category breakdown, peak hours, and a
//       zero-sales warning list with suggested actions.
// HOW:  Builds its UI in code (no .ui file) and renders sections as themed
//       HTML inside QTextBrowsers — the many build*/get*Style helpers assemble
//       HTML fragments coloured from the active ColorScheme. A period combo
//       (7/30/90 days...) re-runs all AnalyticsManager queries; it listens for
//       themeChanged and re-renders on theme switches.
// WHY:  HTML-in-QTextBrowser gives rich, styled report layouts without a
//       dependency on Qt Charts; regenerating from data on each refresh keeps
//       the view stateless and makes theme switching trivial.
// =============================================================================
#ifndef ANALYTICSDIALOG_H
#define ANALYTICSDIALOG_H

#include <QDialog>
#include <QVector>
#include <QString>
#include <QTextBrowser>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include "ColorScheme.h"

class AnalyticsManager;
struct ProductSalesStats;
struct CategoryStats;
struct PeakHourInfo;

class AnalyticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnalyticsDialog(AnalyticsManager *manager, QWidget *parent = nullptr);
    ~AnalyticsDialog() override = default;

    void refresh() { loadAnalyticsData(); }

protected:
    bool event(QEvent *e) override;

private slots:
    void on_refreshButton_clicked();
    void on_closeButton_clicked();
    void on_periodComboBox_currentIndexChanged(int index);
    void onThemeChanged();

private:
    void setupUi();
    void initializePeriodSelector();
    void connectSignals();

    void loadAnalyticsData();
    ColorScheme getCurrentColorScheme() const;

    void updateRevenueSection();
    void updateTransactionSection();
    void updateTopProductsSection();
    void updateCategorySection();
    void updatePeakHoursSection();
    void updateBottomProductsSection();

    void updateChangeLabel(double change);
    void updateGrowthRate(double growthRate);

    QString getBaseTableStyle(const ColorScheme &colors) const;
    QString buildTopProductRow(const QString &medal,
                               const ProductSalesStats &product,
                               const ColorScheme &colors) const;
    QString getCategoryStyles(const ColorScheme &colors) const;
    QString buildBestCategoryHighlight(const CategoryStats &bestCategory,
                                       const ColorScheme &colors) const;
    QString buildCategoryItems(const QVector<CategoryStats> &categories,
                               const ColorScheme &colors) const;
    QString getPeakHoursStyle(const ColorScheme &colors) const;
    QString buildPeakHourItems(const QVector<PeakHourInfo> &peakHours,
                               const ColorScheme &colors) const;
    QString getWarningStyle(const ColorScheme &colors) const;
    QString buildWarningHeader(int productCount, const ColorScheme &colors) const;
    QString buildZeroSalesItems(const QVector<ProductSalesStats> &products,
                                const ColorScheme &colors) const;
    QString buildInfoBox(const QString &title,
                         const QString &content,
                         const ColorScheme &colors) const;

    QVector<ProductSalesStats> filterZeroSales(
        const QVector<ProductSalesStats> &products) const;
    void showEmptyState(QTextBrowser *widget,
                        const QString &message,
                        const ColorScheme &colors);
    QString formatCurrency(double amount) const;
    QString getChangeKind(double change) const;
    QString getSuggestionsText() const
    {
        return "• Consider promoting these products<br/>"
               "• Review pricing strategy<br/>"
               "• Check if items are properly displayed<br/>"
               "• Consider seasonal adjustments";
    }

    AnalyticsManager *analyticsManager = nullptr;
    int currentPeriodDays = 30;

    // Widgets — built in setupUi(), no .ui file needed
    QComboBox    *periodComboBox         = nullptr;
    QPushButton  *refreshButton          = nullptr;
    QPushButton  *closeButton            = nullptr;
    QLabel       *statusLabel            = nullptr;
    QTabWidget   *tabWidget              = nullptr;

    QLabel       *todayRevenueLabel      = nullptr;
    QLabel       *todayChangeLabel       = nullptr;
    QLabel       *yesterdayRevenueLabel  = nullptr;
    QLabel       *weekRevenueLabel       = nullptr;
    QLabel       *monthRevenueLabel      = nullptr;
    QLabel       *growthIconLabel        = nullptr;
    QLabel       *growthRateLabel        = nullptr;

    QLabel       *todayTransactionsLabel = nullptr;
    QLabel       *weekTransactionsLabel  = nullptr;
    QLabel       *avgTransactionLabel    = nullptr;

    QTextBrowser *topProductsText        = nullptr;
    QTextBrowser *categoryText           = nullptr;
    QTextBrowser *peakHoursText          = nullptr;
    QTextBrowser *bottomProductsText     = nullptr;
};

#endif // ANALYTICSDIALOG_H