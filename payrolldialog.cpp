// =============================================================================
// payrolldialog.cpp — Implementation of PayrollDialog (see payrolldialog.h).
// =============================================================================
#include "payrolldialog.h"
#include "payroll.h"
#include "cart.h"          // formatMoney(), currencySymbol()

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QDate>

namespace {
QDoubleSpinBox *moneySpin()
{
    auto *s = new QDoubleSpinBox();
    s->setRange(0.0, 99999999.99);
    s->setDecimals(2);
    s->setPrefix(currencySymbol() + " ");
    return s;
}
QTableWidgetItem *rightItem(const QString &t)
{
    auto *i = new QTableWidgetItem(t);
    i->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return i;
}
}

PayrollDialog::PayrollDialog(Payroll *payroll, QWidget *parent)
    : QDialog(parent), m_payroll(payroll)
{
    setWindowTitle("Payroll");
    setMinimumSize(820, 640);

    auto *root = new QVBoxLayout(this);
    auto *title = new QLabel("Payroll");
    title->setProperty("role", "dialogTitle");
    root->addWidget(title);

    // ── Employees ─────────────────────────────────────────────────────────────
    auto *empGroup = new QGroupBox("Employees");
    auto *empLay = new QVBoxLayout(empGroup);
    m_empTable = new QTableWidget();
    m_empTable->setColumnCount(4);
    m_empTable->setHorizontalHeaderLabels({ "Name", "ID No.", "KRA PIN", "Gross/month" });
    m_empTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_empTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_empTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    empLay->addWidget(m_empTable);
    auto *empBtns = new QHBoxLayout();
    auto *addBtn = new QPushButton("Add");
    auto *editBtn = new QPushButton("Edit");
    auto *delBtn = new QPushButton("Deactivate");
    addBtn->setProperty("kind", "primary");
    connect(addBtn, &QPushButton::clicked, this, &PayrollDialog::addEmployee);
    connect(editBtn, &QPushButton::clicked, this, &PayrollDialog::editEmployee);
    connect(delBtn, &QPushButton::clicked, this, &PayrollDialog::removeEmployee);
    empBtns->addWidget(addBtn); empBtns->addWidget(editBtn); empBtns->addWidget(delBtn);
    empBtns->addStretch();
    auto *ratesBtn = new QPushButton("Statutory Rates...");
    connect(ratesBtn, &QPushButton::clicked, this, &PayrollDialog::editRates);
    empBtns->addWidget(ratesBtn);
    empLay->addLayout(empBtns);
    root->addWidget(empGroup);

    // ── Run ────────────────────────────────────────────────────────────────────
    auto *runGroup = new QGroupBox("Run Payroll");
    auto *runLay = new QHBoxLayout(runGroup);
    runLay->addWidget(new QLabel("Period (yyyy-MM):"));
    m_period = new QLineEdit(QDate::currentDate().toString("yyyy-MM"));
    m_period->setMaximumWidth(120);
    runLay->addWidget(m_period);
    auto *runBtn = new QPushButton("Run && Post to Ledger");
    runBtn->setProperty("kind", "primary");
    connect(runBtn, &QPushButton::clicked, this, &PayrollDialog::runPayroll);
    runLay->addWidget(runBtn);
    runLay->addStretch();
    root->addWidget(runGroup);

    // ── Payslips of the latest run ─────────────────────────────────────────────
    m_slipTable = new QTableWidget();
    m_slipTable->setColumnCount(7);
    m_slipTable->setHorizontalHeaderLabels(
        { "Employee", "Gross", "NSSF", "SHIF", "Housing", "PAYE", "Net" });
    m_slipTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_slipTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_slipTable, 1);

    m_slipTotals = new QLabel();
    m_slipTotals->setProperty("bold", "true");
    root->addWidget(m_slipTotals);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    refreshEmployees();
    const auto runs = m_payroll->payRuns();
    if (!runs.isEmpty()) showRun(runs.first().id);
}

void PayrollDialog::refreshEmployees()
{
    const auto staff = m_payroll->employees(true);
    m_empTable->setRowCount(0);
    for (const Employee &e : staff) {
        const int r = m_empTable->rowCount();
        m_empTable->insertRow(r);
        auto *name = new QTableWidgetItem(e.name);
        name->setData(Qt::UserRole, e.id);
        m_empTable->setItem(r, 0, name);
        m_empTable->setItem(r, 1, new QTableWidgetItem(e.idNumber));
        m_empTable->setItem(r, 2, new QTableWidgetItem(e.kraPin));
        m_empTable->setItem(r, 3, rightItem(formatMoney(e.grossSalary)));
    }
}

void PayrollDialog::addEmployee()
{
    Employee e;
    QDialog dlg(this);
    dlg.setWindowTitle("Add Employee");
    auto *form = new QFormLayout(&dlg);
    auto *name = new QLineEdit();
    auto *idn  = new QLineEdit();
    auto *pin  = new QLineEdit();
    auto *gross = moneySpin();
    form->addRow("Name:", name);
    form->addRow("ID No.:", idn);
    form->addRow("KRA PIN:", pin);
    form->addRow("Gross/month:", gross);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    if (name->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Name Required", "Enter the employee's name.");
        return;
    }
    e.name = name->text().trimmed();
    e.idNumber = idn->text().trimmed();
    e.kraPin = pin->text().trimmed();
    e.grossSalary = Money::fromMajor(gross->value());
    if (m_payroll->addEmployee(e) < 0)
        QMessageBox::warning(this, "Error", m_payroll->lastError());
    refreshEmployees();
}

void PayrollDialog::editEmployee()
{
    const int row = m_empTable->currentRow();
    if (row < 0) return;
    const int id = m_empTable->item(row, 0)->data(Qt::UserRole).toInt();
    Employee cur;
    for (const Employee &e : m_payroll->employees(false))
        if (e.id == id) { cur = e; break; }
    if (cur.id == 0) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Edit Employee");
    auto *form = new QFormLayout(&dlg);
    auto *name = new QLineEdit(cur.name);
    auto *idn  = new QLineEdit(cur.idNumber);
    auto *pin  = new QLineEdit(cur.kraPin);
    auto *gross = moneySpin();
    gross->setValue(cur.grossSalary.toMajor());
    form->addRow("Name:", name);
    form->addRow("ID No.:", idn);
    form->addRow("KRA PIN:", pin);
    form->addRow("Gross/month:", gross);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    cur.name = name->text().trimmed();
    cur.idNumber = idn->text().trimmed();
    cur.kraPin = pin->text().trimmed();
    cur.grossSalary = Money::fromMajor(gross->value());
    m_payroll->updateEmployee(cur);
    refreshEmployees();
}

void PayrollDialog::removeEmployee()
{
    const int row = m_empTable->currentRow();
    if (row < 0) return;
    const int id = m_empTable->item(row, 0)->data(Qt::UserRole).toInt();
    if (QMessageBox::question(this, "Deactivate Employee",
            "Remove this employee from future pay runs?") != QMessageBox::Yes)
        return;
    m_payroll->deactivateEmployee(id);
    refreshEmployees();
}

void PayrollDialog::runPayroll()
{
    const QString period = m_period->text().trimmed();
    if (!QRegularExpression("^\\d{4}-\\d{2}$").match(period).hasMatch()) {
        QMessageBox::warning(this, "Invalid Period", "Use the format yyyy-MM, e.g. 2025-06.");
        return;
    }
    if (QMessageBox::question(this, "Run Payroll",
            "Compute payslips for " + period + " and post the totals to the General Ledger?")
        != QMessageBox::Yes)
        return;

    const int runId = m_payroll->runPayroll(period, QDate::currentDate(), /*post=*/true);
    if (runId < 0) {
        QMessageBox::warning(this, "Payroll Failed", m_payroll->lastError());
        return;
    }
    showRun(runId);
    QMessageBox::information(this, "Payroll Posted",
        "Payroll for " + period + " was computed and posted to the ledger.");
}

void PayrollDialog::showRun(int runId)
{
    const auto slips = m_payroll->payslipsForRun(runId);
    m_slipTable->setRowCount(0);
    Money tGross, tNssf, tShif, tHousing, tPaye, tNet;
    for (const Payslip &s : slips) {
        const int r = m_slipTable->rowCount();
        m_slipTable->insertRow(r);
        m_slipTable->setItem(r, 0, new QTableWidgetItem(s.employeeName));
        m_slipTable->setItem(r, 1, rightItem(formatMoney(s.gross)));
        m_slipTable->setItem(r, 2, rightItem(formatMoney(s.nssf)));
        m_slipTable->setItem(r, 3, rightItem(formatMoney(s.shif)));
        m_slipTable->setItem(r, 4, rightItem(formatMoney(s.housing)));
        m_slipTable->setItem(r, 5, rightItem(formatMoney(s.paye)));
        m_slipTable->setItem(r, 6, rightItem(formatMoney(s.net)));
        tGross += s.gross; tNssf += s.nssf; tShif += s.shif;
        tHousing += s.housing; tPaye += s.paye; tNet += s.net;
    }
    m_slipTotals->setText(QString("Gross %1   NSSF %2   SHIF %3   Housing %4   PAYE %5   Net %6")
        .arg(formatMoney(tGross), formatMoney(tNssf), formatMoney(tShif),
             formatMoney(tHousing), formatMoney(tPaye), formatMoney(tNet)));
}

void PayrollDialog::editRates()
{
    PayrollRates r = m_payroll->rates();

    QDialog dlg(this);
    dlg.setWindowTitle("Statutory Rates");
    dlg.setMinimumWidth(420);
    auto *form = new QFormLayout(&dlg);

    auto bandSpin = [&](Money m) { auto *s = moneySpin(); s->setValue(m.toMajor()); return s; };
    auto pctSpin  = [&](double v) {
        auto *s = new QDoubleSpinBox(); s->setRange(0, 100); s->setDecimals(3);
        s->setSuffix(" %"); s->setValue(v * 100.0); return s; };

    auto *b1 = bandSpin(r.payeBand1), *b2 = bandSpin(r.payeBand2),
         *b3 = bandSpin(r.payeBand3), *b4 = bandSpin(r.payeBand4);
    auto *relief = bandSpin(r.personalRelief);
    auto *nssfR = pctSpin(r.nssfRate); auto *nssfCap = bandSpin(r.nssfCap);
    auto *shifR = pctSpin(r.shifRate); auto *shifMin = bandSpin(r.shifMin);
    auto *houseR = pctSpin(r.housingRate);

    form->addRow("PAYE band 1 upper (10%):", b1);
    form->addRow("PAYE band 2 upper (25%):", b2);
    form->addRow("PAYE band 3 upper (30%):", b3);
    form->addRow("PAYE band 4 upper (32.5%):", b4);
    form->addRow("Personal relief / month:", relief);
    form->addRow("NSSF rate:", nssfR);
    form->addRow("NSSF cap (pensionable):", nssfCap);
    form->addRow("SHIF rate:", shifR);
    form->addRow("SHIF minimum:", shifMin);
    form->addRow("Housing levy rate:", houseR);

    auto *note = new QLabel("Verify against the current KRA / Finance Act figures.");
    note->setProperty("kind", "secondary");
    note->setWordWrap(true);
    form->addRow(note);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    r.payeBand1 = Money::fromMajor(b1->value());
    r.payeBand2 = Money::fromMajor(b2->value());
    r.payeBand3 = Money::fromMajor(b3->value());
    r.payeBand4 = Money::fromMajor(b4->value());
    r.personalRelief = Money::fromMajor(relief->value());
    r.nssfRate = nssfR->value() / 100.0;
    r.nssfCap  = Money::fromMajor(nssfCap->value());
    r.shifRate = shifR->value() / 100.0;
    r.shifMin  = Money::fromMajor(shifMin->value());
    r.housingRate = houseR->value() / 100.0;
    m_payroll->saveRates(r);
}
