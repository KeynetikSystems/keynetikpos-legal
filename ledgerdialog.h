// =============================================================================
// ledgerdialog.h — LedgerDialog: view the trial balance + post journal entries
// -----------------------------------------------------------------------------
// WHAT: The General Ledger's on-screen face — a date-ranged trial balance table
//       and a "New Journal Entry" form for manual double-entry postings.
// HOW:  Thin view over Ledger (ledger.h); the dialog never touches SQL directly.
//       newEntry() builds a small modal form whose OK calls Ledger::postEntry(),
//       which enforces the balanced-entry rule.
// WHY:  Tier-4 accounting needs at least a trial balance and a way to record
//       adjustments the automated postings don't cover (opening balances,
//       corrections, accruals).
// =============================================================================
#ifndef LEDGERDIALOG_H
#define LEDGERDIALOG_H

#include <QDialog>

class Ledger;
class QDateEdit;
class QTableWidget;
class QLabel;

class LedgerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LedgerDialog(Ledger *ledger, QWidget *parent = nullptr);

private slots:
    void refresh();
    void newEntry();

private:
    Ledger       *m_ledger;
    QDateEdit    *m_from   { nullptr };
    QDateEdit    *m_to     { nullptr };
    QTableWidget *m_table  { nullptr };
    QLabel       *m_totals { nullptr };
};

#endif // LEDGERDIALOG_H
