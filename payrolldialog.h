// =============================================================================
// payrolldialog.h — PayrollDialog: employees, pay runs, statutory-rate editor
// -----------------------------------------------------------------------------
// WHAT: The Payroll module's UI — manage employees, run a month's payroll
//       (which posts to the General Ledger), view the resulting payslips, and
//       edit the statutory rates.
// HOW:  Thin view over Payroll (payroll.h); all maths/persistence live there.
// =============================================================================
#ifndef PAYROLLDIALOG_H
#define PAYROLLDIALOG_H

#include <QDialog>

class Payroll;
class QTableWidget;
class QLineEdit;
class QLabel;

class PayrollDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PayrollDialog(Payroll *payroll, QWidget *parent = nullptr);

    // Opens the statutory-rates editor (PAYE bands, NSSF, SHIF, Housing Levy +
    // the effective date and source note). Standalone/static so it can be
    // reached from both the Payroll screen and the Settings menu. Returns true
    // if the user saved.
    static bool editStatutoryRates(QWidget *parent, Payroll &payroll);

private slots:
    void refreshEmployees();
    void addEmployee();
    void editEmployee();
    void removeEmployee();
    void runPayroll();
    void editRates();

private:
    void showRun(int runId);
    void refreshRatesCaption();

    Payroll      *m_payroll;
    QTableWidget *m_empTable  { nullptr };
    QLineEdit    *m_period    { nullptr };   // yyyy-MM
    QTableWidget *m_slipTable { nullptr };
    QLabel       *m_slipTotals{ nullptr };
    QLabel       *m_ratesCaption{ nullptr };  // "Statutory rates as of <date>"
};

#endif // PAYROLLDIALOG_H
