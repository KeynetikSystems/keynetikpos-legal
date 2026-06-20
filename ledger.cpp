// =============================================================================
// ledger.cpp — Implementation of Ledger (see ledger.h).
// -----------------------------------------------------------------------------
// Notes:
//  - Money is stored as INTEGER cents (consistent with products/sales).
//  - postEntry() is the only write path; it enforces the accounting invariant
//    (sum debits == sum credits, > 0) before committing header + lines together.
//  - The seeded chart of accounts covers POS sales, purchasing, VAT and payroll
//    so the other Tier-4 modules have accounts to post into out of the box.
// =============================================================================
#include "ledger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace {
// code, name, type. Kept deliberately small but complete enough for a Kenyan
// retail business: cash/M-Pesa/bank, AR/AP, inventory, VAT input/output, the
// payroll statutory liabilities, equity, sales, COGS and expense buckets.
struct SeedAcct { const char *code; const char *name; AccountType type; };
const SeedAcct kChart[] = {
    { "1000", "Cash on Hand",            AccountType::Asset     },
    { "1010", "M-Pesa / Mobile Money",   AccountType::Asset     },
    { "1020", "Bank",                    AccountType::Asset     },
    { "1100", "Accounts Receivable",     AccountType::Asset     },
    { "1200", "Inventory",               AccountType::Asset     },
    { "1300", "VAT Input (recoverable)", AccountType::Asset     },
    { "2000", "Accounts Payable",        AccountType::Liability },
    { "2100", "VAT Output (payable)",    AccountType::Liability },
    { "2200", "PAYE Payable",            AccountType::Liability },
    { "2210", "SHIF Payable",            AccountType::Liability },
    { "2220", "NSSF Payable",            AccountType::Liability },
    { "2230", "Housing Levy Payable",    AccountType::Liability },
    { "2300", "Net Pay Payable",         AccountType::Liability },
    { "3000", "Owner's Equity",          AccountType::Equity    },
    { "3100", "Retained Earnings",       AccountType::Equity    },
    { "4000", "Sales Revenue",           AccountType::Income    },
    { "5000", "Cost of Goods Sold",      AccountType::Expense   },
    { "6000", "Wages & Salaries",        AccountType::Expense   },
    { "6100", "Rent",                    AccountType::Expense   },
    { "6200", "Utilities",               AccountType::Expense   },
    { "6300", "Other Expenses",          AccountType::Expense   },
};

const char *typeKey(AccountType t)
{
    switch (t) {
    case AccountType::Asset:     return "Asset";
    case AccountType::Liability: return "Liability";
    case AccountType::Equity:    return "Equity";
    case AccountType::Income:    return "Income";
    case AccountType::Expense:   return "Expense";
    }
    return "Asset";
}

AccountType typeFromKey(const QString &k)
{
    if (k == "Liability") return AccountType::Liability;
    if (k == "Equity")    return AccountType::Equity;
    if (k == "Income")    return AccountType::Income;
    if (k == "Expense")   return AccountType::Expense;
    return AccountType::Asset;
}

// Assets and expenses carry a normal DEBIT balance; everything else CREDIT.
bool normalIsDebit(AccountType t)
{
    return t == AccountType::Asset || t == AccountType::Expense;
}
} // namespace

Ledger::Ledger(QSqlDatabase db) : m_db(db) {}

QString Ledger::accountTypeName(AccountType t) { return QString::fromLatin1(typeKey(t)); }

bool Ledger::initSchema()
{
    QSqlQuery q(m_db);
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS gl_accounts (
            code TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            type TEXT NOT NULL
        )
    )")) { m_lastError = q.lastError().text(); return false; }

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS gl_entries (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            entry_date TEXT NOT NULL,
            memo       TEXT,
            source     TEXT NOT NULL DEFAULT 'manual',
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        )
    )")) { m_lastError = q.lastError().text(); return false; }

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS gl_lines (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            entry_id     INTEGER NOT NULL,
            account_code TEXT NOT NULL,
            debit        INTEGER NOT NULL DEFAULT 0,
            credit       INTEGER NOT NULL DEFAULT 0,
            memo         TEXT,
            FOREIGN KEY (entry_id)     REFERENCES gl_entries(id),
            FOREIGN KEY (account_code) REFERENCES gl_accounts(code)
        )
    )")) { m_lastError = q.lastError().text(); return false; }

    return seedChartOfAccounts();
}

bool Ledger::seedChartOfAccounts()
{
    for (const SeedAcct &a : kChart) {
        QSqlQuery q(m_db);
        q.prepare("INSERT OR IGNORE INTO gl_accounts (code, name, type) VALUES (?, ?, ?)");
        q.addBindValue(a.code);
        q.addBindValue(a.name);
        q.addBindValue(typeKey(a.type));
        if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    }
    return true;
}

QVector<GLAccount> Ledger::accounts() const
{
    QVector<GLAccount> out;
    QSqlQuery q(m_db);
    if (q.exec("SELECT code, name, type FROM gl_accounts ORDER BY code")) {
        while (q.next())
            out.append({ q.value(0).toString(), q.value(1).toString(),
                         typeFromKey(q.value(2).toString()) });
    }
    return out;
}

bool Ledger::accountExists(const QString &code) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM gl_accounts WHERE code = ?");
    q.addBindValue(code);
    return q.exec() && q.next();
}

int Ledger::postEntry(const QDate &date, const QString &memo, const QString &source,
                      const QVector<GLLine> &lines)
{
    if (lines.size() < 2) {
        m_lastError = "A journal entry needs at least two lines.";
        return -1;
    }

    qint64 dr = 0, cr = 0;
    for (const GLLine &l : lines) {
        if (l.debit.cents() < 0 || l.credit.cents() < 0) {
            m_lastError = "Line amounts cannot be negative.";
            return -1;
        }
        if (l.debit.cents() != 0 && l.credit.cents() != 0) {
            m_lastError = "A line cannot be both a debit and a credit.";
            return -1;
        }
        if (!accountExists(l.account)) {
            m_lastError = "Unknown account: " + l.account;
            return -1;
        }
        dr += l.debit.cents();
        cr += l.credit.cents();
    }
    if (dr != cr) {
        m_lastError = QStringLiteral("Entry is not balanced (debits %1 != credits %2 cents).")
                          .arg(dr).arg(cr);
        return -1;
    }
    if (dr == 0) {
        m_lastError = "Entry total is zero.";
        return -1;
    }

    if (!m_db.transaction()) { m_lastError = m_db.lastError().text(); return -1; }

    QSqlQuery head(m_db);
    head.prepare("INSERT INTO gl_entries (entry_date, memo, source) VALUES (?, ?, ?)");
    head.addBindValue(date.toString(Qt::ISODate));
    head.addBindValue(memo);
    head.addBindValue(source.isEmpty() ? QStringLiteral("manual") : source);
    if (!head.exec()) { m_lastError = head.lastError().text(); m_db.rollback(); return -1; }
    const int entryId = head.lastInsertId().toInt();

    for (const GLLine &l : lines) {
        QSqlQuery ln(m_db);
        ln.prepare("INSERT INTO gl_lines (entry_id, account_code, debit, credit, memo) "
                   "VALUES (?, ?, ?, ?, ?)");
        ln.addBindValue(entryId);
        ln.addBindValue(l.account);
        ln.addBindValue(l.debit.cents());
        ln.addBindValue(l.credit.cents());
        ln.addBindValue(l.memo.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                         : QVariant(l.memo));
        if (!ln.exec()) { m_lastError = ln.lastError().text(); m_db.rollback(); return -1; }
    }

    if (!m_db.commit()) { m_lastError = m_db.lastError().text(); m_db.rollback(); return -1; }
    return entryId;
}

Money Ledger::accountBalance(const QString &code, const QDate &from, const QDate &to) const
{
    AccountType type = AccountType::Asset;
    {
        QSqlQuery t(m_db);
        t.prepare("SELECT type FROM gl_accounts WHERE code = ?");
        t.addBindValue(code);
        if (t.exec() && t.next()) type = typeFromKey(t.value(0).toString());
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(l.debit),0), COALESCE(SUM(l.credit),0) "
              "FROM gl_lines l JOIN gl_entries e ON l.entry_id = e.id "
              "WHERE l.account_code = ? AND DATE(e.entry_date) BETWEEN ? AND ?");
    q.addBindValue(code);
    q.addBindValue(from.toString(Qt::ISODate));
    q.addBindValue(to.toString(Qt::ISODate));
    if (!q.exec() || !q.next()) return Money();

    const qint64 debit = q.value(0).toLongLong();
    const qint64 credit = q.value(1).toLongLong();
    const qint64 signed_ = normalIsDebit(type) ? (debit - credit) : (credit - debit);
    return Money::fromCents(signed_);
}

QVector<TrialRow> Ledger::trialBalance(const QDate &from, const QDate &to) const
{
    QVector<TrialRow> rows;
    QSqlQuery q(m_db);
    q.prepare(
        "SELECT a.code, a.name, a.type, "
        "       COALESCE(SUM(l.debit),0)  AS dr, "
        "       COALESCE(SUM(l.credit),0) AS cr "
        "FROM gl_accounts a "
        "JOIN gl_lines l   ON l.account_code = a.code "
        "JOIN gl_entries e ON l.entry_id = e.id "
        "WHERE DATE(e.entry_date) BETWEEN ? AND ? "
        "GROUP BY a.code, a.name, a.type "
        "HAVING dr <> 0 OR cr <> 0 "
        "ORDER BY a.code");
    q.addBindValue(from.toString(Qt::ISODate));
    q.addBindValue(to.toString(Qt::ISODate));
    if (q.exec()) {
        while (q.next()) {
            TrialRow r;
            r.code   = q.value(0).toString();
            r.name   = q.value(1).toString();
            r.type   = typeFromKey(q.value(2).toString());
            r.debit  = Money::fromCents(q.value(3).toLongLong());
            r.credit = Money::fromCents(q.value(4).toLongLong());
            const qint64 net = normalIsDebit(r.type)
                                   ? (r.debit.cents() - r.credit.cents())
                                   : (r.credit.cents() - r.debit.cents());
            r.balance = Money::fromCents(net);
            rows.append(r);
        }
    }
    return rows;
}
