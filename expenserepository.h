// =============================================================================
// expenserepository.h — ExpenseRepository: expense categories + expenses,
// split out of Database
// -----------------------------------------------------------------------------
// WHAT: Owns the expense_categories and expenses tables: category list/add/
//       deactivate, expense add, list, by-date-range, and the date-range total
//       used by P&L. Structs (ExpenseCategory/Expense) live in database.h.
// HOW:  Plain class over a QSqlDatabase held BY VALUE, same pattern as the
//       other repositories. lastError() mirrors Database for the writes.
// WHY:  Continues the Database "god object" breakup; expenses are a small,
//       self-contained slice. Database keeps the public methods and delegates.
// =============================================================================
#ifndef EXPENSEREPOSITORY_H
#define EXPENSEREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QDate>

#include "database.h"   // ExpenseCategory, Expense, Money

class ExpenseRepository
{
public:
    explicit ExpenseRepository(QSqlDatabase db);

    QVector<ExpenseCategory> getAllExpenseCategories(bool includeInactive = false);
    bool addExpenseCategory(const QString &name);
    bool deactivateExpenseCategory(int id);

    bool addExpense(const Expense &expense);
    QVector<Expense> getAllExpenses();
    QVector<Expense> getExpensesByDateRange(const QDate &start, const QDate &end);
    Money getTotalExpenses(const QDate &start, const QDate &end);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    QString         m_lastError;
};

#endif // EXPENSEREPOSITORY_H
