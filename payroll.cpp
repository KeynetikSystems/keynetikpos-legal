// =============================================================================
// payroll.cpp — Implementation of Payroll (see payroll.h).
// -----------------------------------------------------------------------------
// Statutory order (post-2024 Kenya treatment):
//   NSSF  = nssfRate x min(gross, nssfCap)            (Tier I+II, capped)
//   SHIF  = max(shifMin, shifRate x gross)
//   AHL   = housingRate x gross                        (Housing Levy)
//   taxable = gross - NSSF - SHIF - AHL                (all deductible pre-PAYE)
//   PAYE  = progressive(taxable) - personalRelief      (floored at 0)
//   net   = gross - NSSF - SHIF - AHL - PAYE
// All amounts are integer cents; each step rounds once.
// =============================================================================
#include "payroll.h"
#include "ledger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <algorithm>
#include <cmath>

namespace {
qint64 pct(qint64 cents, double rate) { return std::llround(cents * rate); }
}

Payroll::Payroll(QSqlDatabase db) : m_db(db) {}

bool Payroll::initSchema()
{
    QSqlQuery q(m_db);
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS employees (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT NOT NULL,
            id_number    TEXT,
            kra_pin      TEXT,
            gross_salary INTEGER NOT NULL DEFAULT 0,
            active       INTEGER NOT NULL DEFAULT 1
        )
    )")) { m_lastError = q.lastError().text(); return false; }

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS pay_runs (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            period    TEXT NOT NULL,
            run_date  TEXT NOT NULL,
            posted    INTEGER NOT NULL DEFAULT 0,
            gl_entry  INTEGER NOT NULL DEFAULT 0
        )
    )")) { m_lastError = q.lastError().text(); return false; }

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS payslips (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id      INTEGER NOT NULL,
            employee_id INTEGER NOT NULL,
            emp_name    TEXT NOT NULL,
            gross       INTEGER NOT NULL,
            nssf        INTEGER NOT NULL,
            shif        INTEGER NOT NULL,
            housing     INTEGER NOT NULL,
            paye        INTEGER NOT NULL,
            net         INTEGER NOT NULL,
            FOREIGN KEY (run_id) REFERENCES pay_runs(id)
        )
    )")) { m_lastError = q.lastError().text(); return false; }

    // Single-row config table; defaults from PayrollRates if empty.
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS payroll_config (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            paye_band1 INTEGER, paye_band2 INTEGER, paye_band3 INTEGER, paye_band4 INTEGER,
            personal_relief INTEGER,
            nssf_rate REAL, nssf_cap INTEGER,
            shif_rate REAL, shif_min INTEGER,
            housing_rate REAL
        )
    )")) { m_lastError = q.lastError().text(); return false; }

    return true;
}

Payslip Payroll::computePayslip(Money gross, const PayrollRates &r)
{
    const qint64 g = gross.cents();

    const qint64 nssf    = pct(std::min(g, r.nssfCap.cents()), r.nssfRate);
    const qint64 shif    = std::max(r.shifMin.cents(), pct(g, r.shifRate));
    const qint64 housing = pct(g, r.housingRate);
    const qint64 taxable = std::max<qint64>(0, g - nssf - shif - housing);

    // Progressive PAYE on the taxable amount.
    qint64 tax = 0, prev = 0;
    auto slab = [&](qint64 upper, double rate) {
        if (taxable > prev) {
            const qint64 amt = std::min(taxable, upper) - prev;
            tax += pct(amt, rate);
            prev = std::min(taxable, upper);
        }
    };
    slab(r.payeBand1.cents(), r.payeRate1);
    slab(r.payeBand2.cents(), r.payeRate2);
    slab(r.payeBand3.cents(), r.payeRate3);
    slab(r.payeBand4.cents(), r.payeRate4);
    if (taxable > prev) tax += pct(taxable - prev, r.payeTopRate);

    const qint64 paye = std::max<qint64>(0, tax - r.personalRelief.cents());
    const qint64 net  = g - nssf - shif - housing - paye;

    Payslip p;
    p.gross   = gross;
    p.nssf    = Money::fromCents(nssf);
    p.shif    = Money::fromCents(shif);
    p.housing = Money::fromCents(housing);
    p.taxable = Money::fromCents(taxable);
    p.paye    = Money::fromCents(paye);
    p.net     = Money::fromCents(net);
    return p;
}

PayrollRates Payroll::rates() const
{
    PayrollRates r;   // defaults
    QSqlQuery q(m_db);
    if (q.exec("SELECT paye_band1, paye_band2, paye_band3, paye_band4, personal_relief, "
               "nssf_rate, nssf_cap, shif_rate, shif_min, housing_rate "
               "FROM payroll_config WHERE id = 1") && q.next()) {
        r.payeBand1      = Money::fromCents(q.value(0).toLongLong());
        r.payeBand2      = Money::fromCents(q.value(1).toLongLong());
        r.payeBand3      = Money::fromCents(q.value(2).toLongLong());
        r.payeBand4      = Money::fromCents(q.value(3).toLongLong());
        r.personalRelief = Money::fromCents(q.value(4).toLongLong());
        r.nssfRate       = q.value(5).toDouble();
        r.nssfCap        = Money::fromCents(q.value(6).toLongLong());
        r.shifRate       = q.value(7).toDouble();
        r.shifMin        = Money::fromCents(q.value(8).toLongLong());
        r.housingRate    = q.value(9).toDouble();
    }
    return r;
}

bool Payroll::saveRates(const PayrollRates &r)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO payroll_config "
              "(id, paye_band1, paye_band2, paye_band3, paye_band4, personal_relief, "
              " nssf_rate, nssf_cap, shif_rate, shif_min, housing_rate) "
              "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(r.payeBand1.cents());
    q.addBindValue(r.payeBand2.cents());
    q.addBindValue(r.payeBand3.cents());
    q.addBindValue(r.payeBand4.cents());
    q.addBindValue(r.personalRelief.cents());
    q.addBindValue(r.nssfRate);
    q.addBindValue(r.nssfCap.cents());
    q.addBindValue(r.shifRate);
    q.addBindValue(r.shifMin.cents());
    q.addBindValue(r.housingRate);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

QVector<Employee> Payroll::employees(bool activeOnly) const
{
    QVector<Employee> out;
    QSqlQuery q(m_db);
    q.prepare(QString("SELECT id, name, id_number, kra_pin, gross_salary, active "
                      "FROM employees %1 ORDER BY name")
                  .arg(activeOnly ? "WHERE active = 1" : ""));
    if (q.exec()) {
        while (q.next()) {
            Employee e;
            e.id          = q.value(0).toInt();
            e.name        = q.value(1).toString();
            e.idNumber    = q.value(2).toString();
            e.kraPin      = q.value(3).toString();
            e.grossSalary = Money::fromCents(q.value(4).toLongLong());
            e.active      = q.value(5).toInt() == 1;
            out.append(e);
        }
    }
    return out;
}

int Payroll::addEmployee(const Employee &e)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO employees (name, id_number, kra_pin, gross_salary, active) "
              "VALUES (?, ?, ?, ?, 1)");
    q.addBindValue(e.name);
    q.addBindValue(e.idNumber);
    q.addBindValue(e.kraPin);
    q.addBindValue(e.grossSalary.cents());
    if (!q.exec()) { m_lastError = q.lastError().text(); return -1; }
    return q.lastInsertId().toInt();
}

bool Payroll::updateEmployee(const Employee &e)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE employees SET name=?, id_number=?, kra_pin=?, gross_salary=?, active=? "
              "WHERE id=?");
    q.addBindValue(e.name);
    q.addBindValue(e.idNumber);
    q.addBindValue(e.kraPin);
    q.addBindValue(e.grossSalary.cents());
    q.addBindValue(e.active ? 1 : 0);
    q.addBindValue(e.id);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

bool Payroll::deactivateEmployee(int id)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE employees SET active = 0 WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

int Payroll::runPayroll(const QString &period, const QDate &date, bool post)
{
    const QVector<Employee> staff = employees(true);
    if (staff.isEmpty()) { m_lastError = "No active employees to pay."; return -1; }

    const PayrollRates r = rates();

    if (!m_db.transaction()) { m_lastError = m_db.lastError().text(); return -1; }

    QSqlQuery head(m_db);
    head.prepare("INSERT INTO pay_runs (period, run_date, posted) VALUES (?, ?, 0)");
    head.addBindValue(period);
    head.addBindValue(date.toString(Qt::ISODate));
    if (!head.exec()) { m_lastError = head.lastError().text(); m_db.rollback(); return -1; }
    const int runId = head.lastInsertId().toInt();

    Money totGross, totNssf, totShif, totHousing, totPaye, totNet;
    for (const Employee &e : staff) {
        const Payslip s = computePayslip(e.grossSalary, r);
        QSqlQuery ps(m_db);
        ps.prepare("INSERT INTO payslips "
                   "(run_id, employee_id, emp_name, gross, nssf, shif, housing, paye, net) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        ps.addBindValue(runId);
        ps.addBindValue(e.id);
        ps.addBindValue(e.name);
        ps.addBindValue(s.gross.cents());
        ps.addBindValue(s.nssf.cents());
        ps.addBindValue(s.shif.cents());
        ps.addBindValue(s.housing.cents());
        ps.addBindValue(s.paye.cents());
        ps.addBindValue(s.net.cents());
        if (!ps.exec()) { m_lastError = ps.lastError().text(); m_db.rollback(); return -1; }

        totGross += s.gross; totNssf += s.nssf; totShif += s.shif;
        totHousing += s.housing; totPaye += s.paye; totNet += s.net;
    }

    int glEntry = 0;
    if (post) {
        Ledger ledger(m_db);
        ledger.initSchema();   // idempotent; ensure accounts exist
        QVector<GLLine> lines;
        lines.append({ "6000", totGross,  Money(), "Gross wages" });   // Dr Wages
        auto cr = [&](const QString &acct, Money amt, const QString &memo) {
            if (amt.cents() > 0) lines.append({ acct, Money(), amt, memo });
        };
        cr("2300", totNet,     "Net pay payable");
        cr("2200", totPaye,    "PAYE payable");
        cr("2210", totShif,    "SHIF payable");
        cr("2220", totNssf,    "NSSF payable");
        cr("2230", totHousing, "Housing levy payable");

        glEntry = ledger.postEntry(date, "Payroll " + period, "payroll", lines);
        if (glEntry < 0) { m_lastError = "GL posting failed: " + ledger.lastError(); m_db.rollback(); return -1; }

        QSqlQuery upd(m_db);
        upd.prepare("UPDATE pay_runs SET posted = 1, gl_entry = ? WHERE id = ?");
        upd.addBindValue(glEntry);
        upd.addBindValue(runId);
        if (!upd.exec()) { m_lastError = upd.lastError().text(); m_db.rollback(); return -1; }
    }

    if (!m_db.commit()) { m_lastError = m_db.lastError().text(); m_db.rollback(); return -1; }
    return runId;
}

QVector<PayRun> Payroll::payRuns() const
{
    QVector<PayRun> out;
    QSqlQuery q(m_db);
    if (q.exec("SELECT id, period, run_date, posted, gl_entry FROM pay_runs ORDER BY id DESC")) {
        while (q.next()) {
            PayRun r;
            r.id        = q.value(0).toInt();
            r.period    = q.value(1).toString();
            r.runDate   = q.value(2).toString();
            r.posted    = q.value(3).toInt() == 1;
            r.glEntryId = q.value(4).toInt();
            out.append(r);
        }
    }
    return out;
}

QVector<Payslip> Payroll::payslipsForRun(int runId) const
{
    QVector<Payslip> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT employee_id, emp_name, gross, nssf, shif, housing, paye, net "
              "FROM payslips WHERE run_id = ? ORDER BY emp_name");
    q.addBindValue(runId);
    if (q.exec()) {
        while (q.next()) {
            Payslip s;
            s.employeeId   = q.value(0).toInt();
            s.employeeName = q.value(1).toString();
            s.gross   = Money::fromCents(q.value(2).toLongLong());
            s.nssf    = Money::fromCents(q.value(3).toLongLong());
            s.shif    = Money::fromCents(q.value(4).toLongLong());
            s.housing = Money::fromCents(q.value(5).toLongLong());
            s.paye    = Money::fromCents(q.value(6).toLongLong());
            s.net     = Money::fromCents(q.value(7).toLongLong());
            s.taxable = Money::fromCents(s.gross.cents() - s.nssf.cents()
                                         - s.shif.cents() - s.housing.cents());
            out.append(s);
        }
    }
    return out;
}
