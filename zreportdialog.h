// =============================================================================
// zreportdialog.h — ZReportDialog + ZReportData: end-of-day fiscal summary
// -----------------------------------------------------------------------------
// WHAT: The classic POS "Z-Report": gross sales, discounts, net sales, tax
//       collected, transaction count, sales by category, top products, and
//       cash reconciliation — opening float, closing float, expected cash,
//       and the variance (over/short).
// HOW:  buildReport() aggregates sales/sale_items for the chosen date
//       (optionally a specific shift from ShiftManager) into ZReportData;
//       renderHtml() formats it for the preview pane, with print and export
//       actions. The static generateZReportText() produces a plain-text
//       rendering callable WITHOUT any UI — this is what ScheduleManager uses
//       to build SMS/WhatsApp report bodies.
// WHY:  The Z-Report is the standard retail control document for detecting
//       drawer shortages and summarizing the day. The static text generator
//       exists so scheduled messaging reuses exactly the same numbers as the
//       on-screen report — no second implementation to drift.
// =============================================================================
#pragma once

#include <QDialog>
#include <QSqlDatabase>
#include <QDateTime>
#include "schedulemanager.h"

class SettingsManager;
class ShiftManager;
class QTextEdit;
class QPushButton;
class QComboBox;
class QDateEdit;

struct ZReportData {
    // Period
    QDate reportDate;
    QString shiftInfo;

    // Sales summary
    double grossSales       = 0.0;
    double totalDiscounts   = 0.0;
    double netSales         = 0.0;
    double taxCollected     = 0.0;
    int    transactionCount = 0;

    // By category
    struct CategoryLine {
        QString category;
        int     qty;
        double  sales;
    };
    QVector<CategoryLine> byCategory;

    // Top products
    struct ProductLine {
        QString name;
        int     qty;
        double  sales;
    };
    QVector<ProductLine> topProducts;

    // Shift info
    double openingFloat = 0.0;
    double closingFloat = 0.0;
    double expectedCash = 0.0;   // openingFloat + netSales - taxCollected (approx)
    double cashVariance = 0.0;   // closingFloat - expectedCash
    QString cashierName;
};

class ZReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ZReportDialog(QSqlDatabase &db,
                           SettingsManager *settings,
                           ShiftManager    *shifts,
                           QWidget *parent = nullptr);
    static QString generateZReportText(QSqlDatabase &db,
                                       SettingsManager *settings,
                                       ShiftManager *shifts,
                                       const QDate &date,
                                       int shiftId = 0);

private slots:
    void generateReport();
    void exportReport();
    void printReport();

private:
    void setupUi();
    ZReportData buildReport(const QDate &date, int shiftId = -1);
    QString     renderHtml(const ZReportData &data);

    QSqlDatabase    &m_db;
    SettingsManager *m_settings;
    ShiftManager    *m_shifts;

    QComboBox  *m_shiftCombo   = nullptr;
    QDateEdit  *m_dateEdit     = nullptr;
    QTextEdit  *m_reportView   = nullptr;
    QPushButton *m_exportBtn   = nullptr;
    QPushButton *m_printBtn    = nullptr;

    ZReportData m_lastReport;
};
