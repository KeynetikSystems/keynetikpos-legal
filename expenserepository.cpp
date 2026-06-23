// =============================================================================
// expenserepository.cpp — Implementation of ExpenseRepository (see
// expenserepository.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Bodies (and the expenseFromQuery/expenseJoin helpers) were lifted verbatim
// from Database; the only edits are db -> m_db and lastError -> m_lastError.
// =============================================================================
#include "expenserepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "money.h"

ExpenseRepository::ExpenseRepository(QSqlDatabase db)
    : m_db(db)
{
}

QVector<ExpenseCategory> ExpenseRepository::getAllExpenseCategories(bool includeInactive)
{
    QVector<ExpenseCategory> cats;
    QSqlQuery q(m_db);
    q.exec(includeInactive
               ? "SELECT id, name, is_active FROM expense_categories ORDER BY name"
               : "SELECT id, name, is_active FROM expense_categories WHERE is_active=1 ORDER BY name");
    while (q.next()) {
        ExpenseCategory c;
        c.id       = q.value(0).toInt();
        c.name     = q.value(1).toString();
        c.isActive = q.value(2).toBool();
        cats.append(c);
    }
    return cats;
}

bool ExpenseRepository::addExpenseCategory(const QString &name)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO expense_categories (name) VALUES (?)");
    q.addBindValue(name.trimmed());
    if (!q.exec()) {
        m_lastError = "Failed to add expense category: " + q.lastError().text();
        return false;
    }
    return true;
}

bool ExpenseRepository::deactivateExpenseCategory(int id)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE expense_categories SET is_active = 0 WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = "Failed to deactivate category: " + q.lastError().text();
        return false;
    }
    return true;
}

bool ExpenseRepository::addExpense(const Expense &expense)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO expenses (category_id, amount, description, date, recorded_by) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(expense.categoryId);
    q.addBindValue(expense.amount.cents());
    q.addBindValue(expense.description);
    q.addBindValue(expense.date.toString(Qt::ISODate));
    q.addBindValue(expense.recordedBy);
    if (!q.exec()) {
        m_lastError = "Failed to record expense: " + q.lastError().text();
        return false;
    }
    return true;
}

static Expense expenseFromQuery(QSqlQuery &q)
{
    Expense e;
    e.id           = q.value(0).toInt();
    e.categoryId   = q.value(1).toInt();
    e.categoryName = q.value(2).toString();
    e.amount       = Money::fromCents(q.value(3).toLongLong());
    e.description  = q.value(4).toString();
    e.date         = q.value(5).toDate();
    e.recordedBy   = q.value(6).toString();
    e.createdAt    = q.value(7).toString();
    return e;
}

static const char *expenseJoin =
    "SELECT e.id, e.category_id, c.name, e.amount, e.description, "
    "       e.date, e.recorded_by, e.created_at "
    "FROM expenses e JOIN expense_categories c ON c.id = e.category_id ";

QVector<Expense> ExpenseRepository::getAllExpenses()
{
    QVector<Expense> list;
    QSqlQuery q(m_db);
    q.exec(QString(expenseJoin) + "ORDER BY e.date DESC, e.created_at DESC");
    while (q.next())
        list.append(expenseFromQuery(q));
    return list;
}

QVector<Expense> ExpenseRepository::getExpensesByDateRange(const QDate &start, const QDate &end)
{
    QVector<Expense> list;
    QSqlQuery q(m_db);
    q.prepare(QString(expenseJoin) + "WHERE e.date BETWEEN ? AND ? ORDER BY e.date DESC");
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    if (q.exec()) {
        while (q.next())
            list.append(expenseFromQuery(q));
    }
    return list;
}

Money ExpenseRepository::getTotalExpenses(const QDate &start, const QDate &end)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(amount), 0) FROM expenses WHERE date BETWEEN ? AND ?");
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    if (q.exec() && q.next())
        return Money::fromCents(q.value(0).toLongLong());
    return Money::fromCents(0);
}
