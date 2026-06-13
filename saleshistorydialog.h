// =============================================================================
// saleshistorydialog.h — SalesHistoryDialog: transaction browser + refunds
// -----------------------------------------------------------------------------
// WHAT: Filterable list of all sales (date range, quick filters like Today/
//       This Week, payment method, free-text search), a summary strip (total,
//       count, average, tax), drill-down to line items, CSV export, and the
//       Refund entry point.
// HOW:  Loads Sale rows from Database::getSalesByDateRange(), filters in
//       memory, and shows items from getSaleItems() in a details view. Refunds
//       prompt for a reason, check permissions and isRefunded() (no double
//       refunds), then call Database::processRefund(), which records the
//       refund and restores stock.
// WHY:  Refunds deliberately live behind the sales history rather than a
//       free-standing form: forcing selection of the ACTUAL original sale
//       prevents refunding amounts that were never charged, and ties every
//       refund to a real transaction, a reason, and a user for the audit
//       trail.
// =============================================================================
#ifndef SALESHISTORYDIALOG_H
#define SALESHISTORYDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QDateEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include "database.h"

class SalesHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SalesHistoryDialog(QWidget *parent = nullptr);
    ~SalesHistoryDialog();

private slots:
    void onSearchClicked();
    void onViewDetailsClicked();
    void onRefundClicked();
    void onExportClicked();
    void onDateRangeChanged();
    void onSaleSelected(int row);
    void onQuickFilterChanged(const QString &filter);

private:
    void setupUI();
    void loadSales();
    void updateSalesTable();
    void updateSummary();
    void showSaleDetails(int saleId);
    QString formatCurrency(double amount);

    // UI Components
    QDateEdit *startDateEdit;
    QDateEdit *endDateEdit;
    QComboBox *quickFilterCombo;
    QComboBox *paymentMethodFilter;
    QLineEdit *searchEdit;
    QPushButton *searchBtn;
    QPushButton *viewDetailsBtn;
    QPushButton *refundBtn;
    QPushButton *exportBtn;

    QTableWidget *salesTable;

    QLabel *totalSalesLabel;
    QLabel *transactionCountLabel;
    QLabel *avgTransactionLabel;
    QLabel *totalTaxLabel;

    // Data
    QVector<Sale> sales;
    int selectedSaleId;
};

#endif // SALESHISTORYDIALOG_H
