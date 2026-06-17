// =============================================================================
// expensedialog.h — ExpenseDialog: record and review operating expenses
// -----------------------------------------------------------------------------
// WHAT: Two-tab dialog — "Expenses" (list with date filter + Add button) and
//       "Categories" (manage the expense category list). The category list
//       feeds the combo in the Add Expense form.
// HOW:  All reads/writes go through Database expense methods. The date filter
//       defaults to the current month so the cashier sees today's context
//       without having to set a range.
// WHY:  Without expenses the P&L report can only show gross profit (revenue −
//       COGS). Recording operating costs here unlocks net profit.
// =============================================================================
#ifndef EXPENSEDIALOG_H
#define EXPENSEDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QDateEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class ExpenseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExpenseDialog(QWidget *parent = nullptr);

private slots:
    void onAddExpenseClicked();
    void onFilterClicked();
    void onAddCategoryClicked();
    void onDeactivateCategoryClicked();

private:
    void setupUI();
    void loadExpenses();
    void loadCategories();

    QTabWidget   *tabs;

    // Expenses tab
    QTableWidget *expenseTable;
    QDateEdit    *fromDateEdit;
    QDateEdit    *toDateEdit;
    QLabel       *totalLabel;
    QPushButton  *addExpenseButton;
    QPushButton  *filterButton;

    // Categories tab
    QTableWidget *categoryTable;
    QPushButton  *addCategoryButton;
    QPushButton  *deactivateCategoryButton;
};

#endif // EXPENSEDIALOG_H
