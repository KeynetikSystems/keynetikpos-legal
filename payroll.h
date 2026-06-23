// =============================================================================
// payroll.h — Payroll: employees, Kenya statutory deductions, pay runs (Tier 4)
// -----------------------------------------------------------------------------
// WHAT: Employee records, a configurable statutory-deduction engine (PAYE
//       bands, SHIF, NSSF, Housing Levy), pay runs that compute a payslip per
//       employee, and posting of each run into the General Ledger.
// HOW:  computePayslip() is a PURE function of (gross, rates) so the tax maths
//       is unit-tested in isolation. Rates default to the 2024/2025 KRA figures
//       and persist (editable) in payroll_config. runPayroll() stores payslips
//       and posts one balanced GL entry (Dr Wages; Cr net pay + each statutory
//       liability). Money is integer cents throughout.
// WHY:  "ERP Full" payroll must be correct and auditable: deductions reconcile,
//       and every run lands in the ledger as a journal entry.
//
//  !! VERIFY RATES: statutory rates/bands change with each Finance Act. The
//     defaults below reflect the post-2024 treatment (SHIF, NSSF and Housing
//     Levy deductible BEFORE PAYE). Confirm against the current KRA guidance
//     and adjust in the Payroll → Rates editor.
// =============================================================================
#ifndef PAYROLL_H
#define PAYROLL_H

#include <QString>
#include <QVector>
#include <QDate>
#include <QSqlDatabase>

#include "money.h"

struct Employee {
    int     id = 0;
    QString name;
    QString idNumber;     // national ID
    QString kraPin;
    Money   grossSalary;  // monthly gross
    bool    active = true;
};

// Configurable statutory parameters (monthly). Cents for money, fractions for %.
struct PayrollRates {
    // PAYE band UPPER limits (cents) and the marginal rate of each band; income
    // above the last band is taxed at payeTopRate.
    Money  payeBand1 = Money::fromCents(2400000);    // 24,000.00
    Money  payeBand2 = Money::fromCents(3233300);    // 32,333.00
    Money  payeBand3 = Money::fromCents(50000000);   // 500,000.00
    Money  payeBand4 = Money::fromCents(80000000);   // 800,000.00
    double payeRate1 = 0.10, payeRate2 = 0.25, payeRate3 = 0.30,
           payeRate4 = 0.325, payeTopRate = 0.35;
    Money  personalRelief = Money::fromCents(240000);// 2,400.00 / month

    double nssfRate    = 0.06;  Money nssfCap = Money::fromCents(3600000); // 6% of pensionable, cap 36,000
    double shifRate    = 0.0275; Money shifMin = Money::fromCents(30000);  // 2.75%, min 300
    double housingRate = 0.015;                                            // 1.5%

    // ── Provenance (so users can see how current the rates are) ───────────────
    // effectiveDate is the ISO date the CURRENT set took effect; ratesNote names
    // the source. Both are shown in the Payroll dialog + on payslips and are
    // persisted with the rates, so an in-app edit can be re-stamped. ALWAYS bump
    // effectiveDate when you change a rate. See docs/STATUTORY_RATES.md for the
    // line-by-line sources and the update checklist.
    QString effectiveDate = "2024-10-01";
    QString ratesNote =
        "Finance Act 2023 PAYE bands; NSSF enhanced (2024); "
        "SHIF 2.75% (Oct 2024); Housing Levy 1.5%";
};

struct Payslip {
    int     employeeId = 0;
    QString employeeName;
    Money   gross;
    Money   nssf;
    Money   shif;
    Money   housing;
    Money   taxable;   // gross - nssf - shif - housing
    Money   paye;
    Money   net;       // gross - nssf - shif - housing - paye
};

struct PayRun {
    int     id = 0;
    QString period;    // "yyyy-MM"
    QString runDate;
    bool    posted = false;
    int     glEntryId = 0;
};

class Payroll
{
public:
    explicit Payroll(QSqlDatabase db);

    bool initSchema();

    // ── Statutory engine (pure; unit-tested) ─────────────────────────────────
    static Payslip computePayslip(Money gross, const PayrollRates &rates);

    // ── Rates (persisted, editable) ───────────────────────────────────────────
    PayrollRates rates() const;
    bool         saveRates(const PayrollRates &r);

    // ── Employees ─────────────────────────────────────────────────────────────
    QVector<Employee> employees(bool activeOnly = true) const;
    int  addEmployee(const Employee &e);
    bool updateEmployee(const Employee &e);
    bool deactivateEmployee(int id);

    // ── Pay runs ──────────────────────────────────────────────────────────────
    // Computes a payslip per active employee for `period` (yyyy-MM) and, if
    // post==true, writes one balanced GL entry. Returns the run id or -1.
    int  runPayroll(const QString &period, const QDate &date, bool post);
    QVector<PayRun>  payRuns() const;
    QVector<Payslip> payslipsForRun(int runId) const;

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    mutable QString m_lastError;
};

#endif // PAYROLL_H
