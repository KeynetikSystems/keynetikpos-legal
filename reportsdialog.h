// =============================================================================
// reportsdialog.h — ReportsDialog: tabular, exportable operational reports
// -----------------------------------------------------------------------------
// WHAT: Sales by date, by category, by payment method, top sellers, and a
//       daily summary — selectable by type and date range, exportable to CSV
//       and PDF.
// HOW:  A combo box picks the generator method; each generate*Report() fills
//       the shared QTableWidget and a QVector<QStringList> backing store from
//       SQL aggregates. PDF export renders an HTML version of the table via
//       QTextDocument/QPrinter; CSV export writes the backing store directly.
// WHY:  These are the printable documents an owner hands to an accountant —
//       distinct from the on-screen exploratory dashboards, hence table-first
//       layout and file export as the primary action.
// =============================================================================
#ifndef REPORTSDIALOG_H
#define REPORTSDIALOG_H

#include <QDialog>
#include <QDate>
#include <QTableWidget>
#include <QComboBox>
#include <QDateEdit>
#include <QPushButton>
#include <QLabel>
#include "database.h"

class ReportsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportsDialog(QWidget *parent = nullptr);
    ~ReportsDialog();

private slots:
    void onGenerateReport();
    void onExportToPDF();
    void onExportToCSV();
    void onReportTypeChanged(int index);

private:
    void setupUI();
    void generateSalesByDateReport();
    void generateSalesByCategoryReport();
    void generateSalesByPaymentReport();
    void generateTopSellingProductsReport();
    void generateDailySalesReport();

    // UI Components
    QComboBox *reportTypeCombo;
    QDateEdit *startDateEdit;
    QDateEdit *endDateEdit;
    QPushButton *generateBtn;
    QPushButton *exportPdfBtn;
    QPushButton *exportCsvBtn;
    QTableWidget *reportTable;
    QLabel *summaryLabel;

    // Data
    QString currentReportType;
    QVector<QStringList> reportData;
};

#endif // REPORTSDIALOG_H
