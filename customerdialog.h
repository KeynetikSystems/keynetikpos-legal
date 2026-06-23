// =============================================================================
// customerdialog.h — CustomerDialog: customer directory + account management
// -----------------------------------------------------------------------------
// WHAT: Searchable customer directory with Add/Edit, purchase history per
//       customer, and store credit top-up. Loyalty point balance is shown
//       but is read-only here — points are awarded automatically in
//       Database::recordSale() at 1 point per 100 KSh spent.
// HOW:  Two-pane layout: left = searchable table, right = detail panel
//       (stats + purchase history + credit controls) that updates on row
//       selection. All writes go through Database customer methods.
// WHY:  Customer accounts are the CRM layer that connects the POS to repeat
//       customers — purchase history, loyalty, and store credit keep the
//       business relationship in the app rather than a separate spreadsheet.
// =============================================================================
#ifndef CUSTOMERDIALOG_H
#define CUSTOMERDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "database.h"   // Customer

class CustomerDialog : public QDialog
{
    Q_OBJECT

    Database &m_db;   // injected app DB connection (not owned)

public:
    explicit CustomerDialog(Database &db, QWidget *parent = nullptr);

    // Returns the customer the user double-clicked or clicked "Select" on.
    // id == 0 means no selection was made (dialog managed/browsed only).
    Customer getSelectedCustomer() const { return m_selectedCustomer; }

private slots:
    void onSearchChanged(const QString &text);
    void onCustomerSelected();
    void onAddClicked();
    void onEditClicked();
    void onDeactivateClicked();
    void onTopUpCreditClicked();

private:
    void setupUI();
    void loadCustomers(const QString &filter = {});
    void showCustomerDetail(int customerId);
    void clearDetail();
    int  selectedCustomerId() const;
    bool runCustomerForm(const QString &title, Customer *customer);

    // Left pane
    QLineEdit    *searchEdit;
    QTableWidget *table;
    QPushButton  *addButton;
    QPushButton  *editButton;
    QPushButton  *deactivateButton;

    // Right pane (detail)
    QGroupBox    *detailGroup;
    QLabel       *detailName;
    QLabel       *detailPhone;
    QLabel       *detailEmail;
    QLabel       *detailPoints;
    QLabel       *detailCredit;
    QTableWidget *historyTable;
    QPushButton  *topUpButton;
    QPushButton  *selectButton;   // visible only when opened from checkout
    QPushButton  *closeButton;

    Customer     m_selectedCustomer;   // set on Select / double-click
};

#endif // CUSTOMERDIALOG_H
