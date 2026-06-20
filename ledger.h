// =============================================================================
// ledger.h — Ledger: the double-entry general-ledger spine (Tier 4 ERP)
// -----------------------------------------------------------------------------
// WHAT: A minimal but real general ledger — a seeded chart of accounts, balanced
//       journal entries (every entry's debits must equal its credits), account
//       balances, and a trial balance / P&L / balance-sheet rollup. Payroll and
//       VAT post into this so the books stay consistent.
// HOW:  Plain class over the shared QSqlDatabase (same pattern as
//       SettingsManager / ShiftManager). Money is integer cents throughout, like
//       the rest of the app. postEntry() validates and writes header + lines in
//       one transaction; an unbalanced or empty entry is rejected.
// WHY:  "ERP Full" means the numbers reconcile. A general ledger is the spine
//       every other money movement (sales, purchases, payroll, VAT) hangs off,
//       turning a POS with reports into an accounting system with statements.
// =============================================================================
#ifndef LEDGER_H
#define LEDGER_H

#include <QString>
#include <QVector>
#include <QDate>
#include <QSqlDatabase>

#include "money.h"

// Account classification. normalSide (Debit/Credit) decides the sign of a
// balance: assets/expenses grow on the debit side, the rest on the credit side.
enum class AccountType { Asset, Liability, Equity, Income, Expense };

struct GLAccount {
    QString     code;        // e.g. "4000"
    QString     name;        // e.g. "Sales Revenue"
    AccountType type = AccountType::Asset;
};

// One leg of a journal entry. Exactly one of debit/credit is normally non-zero.
struct GLLine {
    QString account;         // account code
    Money   debit;
    Money   credit;
    QString memo;
};

// A row of the trial balance.
struct TrialRow {
    QString     code;
    QString     name;
    AccountType type = AccountType::Asset;
    Money       debit;       // period debit total
    Money       credit;      // period credit total
    Money       balance;     // signed to the account's normal side
};

class Ledger
{
public:
    // Holds the connection handle by value (cheap, ref-counted) so it stays
    // valid even if the caller's QSqlDatabase local goes out of scope.
    explicit Ledger(QSqlDatabase db);

    // Create tables + seed the standard chart of accounts (idempotent).
    bool initSchema();

    QVector<GLAccount> accounts() const;
    bool   accountExists(const QString &code) const;

    // Post a balanced journal entry atomically. Returns the new entry id, or -1
    // (see lastError()). `source` tags the origin ("manual"/"sale"/"payroll"/…).
    int postEntry(const QDate &date, const QString &memo, const QString &source,
                  const QVector<GLLine> &lines);

    // Net balance of one account over [from, to] (inclusive), signed to its
    // normal side (so a normal asset/expense balance is positive on debit).
    Money accountBalance(const QString &code,
                         const QDate &from, const QDate &to) const;

    // Trial balance for the period; rows with no activity are omitted.
    QVector<TrialRow> trialBalance(const QDate &from, const QDate &to) const;

    QString lastError() const { return m_lastError; }

    static QString accountTypeName(AccountType t);

private:
    bool seedChartOfAccounts();

    QSqlDatabase    m_db;
    mutable QString m_lastError;
};

#endif // LEDGER_H
