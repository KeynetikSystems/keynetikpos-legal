// =============================================================================
// ledgerdialog.cpp — Implementation of LedgerDialog (see ledgerdialog.h).
// =============================================================================
#include "ledgerdialog.h"
#include "ledger.h"
#include "cart.h"          // formatMoney()
#include "appstyle.h"      // setStyleProperty()

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QDateEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QDialogButtonBox>

LedgerDialog::LedgerDialog(Ledger *ledger, QWidget *parent)
    : QDialog(parent), m_ledger(ledger)
{
    setWindowTitle("General Ledger");
    setMinimumSize(720, 560);

    auto *root = new QVBoxLayout(this);

    auto *title = new QLabel("General Ledger — Trial Balance");
    title->setProperty("role", "dialogTitle");
    root->addWidget(title);

    // Date range + actions
    auto *bar = new QHBoxLayout();
    bar->addWidget(new QLabel("From:"));
    m_from = new QDateEdit(QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1));
    m_from->setCalendarPopup(true);
    bar->addWidget(m_from);
    bar->addWidget(new QLabel("To:"));
    m_to = new QDateEdit(QDate::currentDate());
    m_to->setCalendarPopup(true);
    bar->addWidget(m_to);
    auto *refreshBtn = new QPushButton("Refresh");
    connect(refreshBtn, &QPushButton::clicked, this, &LedgerDialog::refresh);
    bar->addWidget(refreshBtn);
    bar->addStretch();
    auto *newBtn = new QPushButton("New Journal Entry...");
    newBtn->setProperty("kind", "primary");
    connect(newBtn, &QPushButton::clicked, this, &LedgerDialog::newEntry);
    bar->addWidget(newBtn);
    root->addLayout(bar);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({ "Code", "Account", "Type", "Debit", "Credit" });
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);

    m_totals = new QLabel();
    m_totals->setProperty("bold", "true");
    root->addWidget(m_totals);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(box);

    refresh();
}

void LedgerDialog::refresh()
{
    const QDate from = m_from->date();
    const QDate to   = m_to->date();
    const auto rows  = m_ledger->trialBalance(from, to);

    m_table->setRowCount(0);
    Money totalDr, totalCr;
    for (const TrialRow &r : rows) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(r.code));
        m_table->setItem(row, 1, new QTableWidgetItem(r.name));
        m_table->setItem(row, 2, new QTableWidgetItem(Ledger::accountTypeName(r.type)));
        auto *dr = new QTableWidgetItem(r.debit.isZero()  ? QString() : formatMoney(r.debit));
        auto *cr = new QTableWidgetItem(r.credit.isZero() ? QString() : formatMoney(r.credit));
        dr->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cr->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 3, dr);
        m_table->setItem(row, 4, cr);
        totalDr += r.debit;
        totalCr += r.credit;
    }

    const bool balanced = totalDr.cents() == totalCr.cents();
    m_totals->setText(QString("Total Debits: %1     Total Credits: %2     %3")
        .arg(formatMoney(totalDr), formatMoney(totalCr),
             balanced ? "Balanced" : "OUT OF BALANCE"));
    setStyleProperty(m_totals, "kind", balanced ? "success" : "danger");
}

void LedgerDialog::newEntry()
{
    QDialog dlg(this);
    dlg.setWindowTitle("New Journal Entry");
    dlg.setMinimumWidth(560);
    auto *v = new QVBoxLayout(&dlg);

    auto *form = new QFormLayout();
    auto *date = new QDateEdit(QDate::currentDate());
    date->setCalendarPopup(true);
    form->addRow("Date:", date);
    auto *memo = new QLineEdit();
    memo->setPlaceholderText("What is this entry for?");
    form->addRow("Memo:", memo);
    v->addLayout(form);

    // Line rows: account, debit, credit. Start with four blank rows.
    auto *lines = new QTableWidget(&dlg);
    lines->setColumnCount(3);
    lines->setHorizontalHeaderLabels({ "Account", "Debit", "Credit" });
    lines->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    const auto accts = m_ledger->accounts();
    auto addRow = [&]() {
        const int r = lines->rowCount();
        lines->insertRow(r);
        auto *combo = new QComboBox();
        combo->addItem("—", QString());
        for (const GLAccount &a : accts)
            combo->addItem(a.code + "  " + a.name, a.code);
        lines->setCellWidget(r, 0, combo);
        for (int c : { 1, 2 }) {
            auto *spin = new QDoubleSpinBox();
            spin->setRange(0.0, 99999999.99);
            spin->setDecimals(2);
            lines->setCellWidget(r, c, spin);
        }
    };
    for (int i = 0; i < 4; ++i) addRow();
    v->addWidget(lines);

    auto *addBtn = new QPushButton("Add line");
    connect(addBtn, &QPushButton::clicked, &dlg, [&]() { addRow(); });
    v->addWidget(addBtn, 0, Qt::AlignLeft);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText("Post Entry");
    v->addWidget(box);

    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, [&]() {
        QVector<GLLine> entryLines;
        for (int r = 0; r < lines->rowCount(); ++r) {
            auto *combo = qobject_cast<QComboBox *>(lines->cellWidget(r, 0));
            auto *dr    = qobject_cast<QDoubleSpinBox *>(lines->cellWidget(r, 1));
            auto *cr    = qobject_cast<QDoubleSpinBox *>(lines->cellWidget(r, 2));
            const QString code = combo ? combo->currentData().toString() : QString();
            if (code.isEmpty()) continue;
            if (dr->value() < 0.005 && cr->value() < 0.005) continue;
            entryLines.append({ code, Money::fromMajor(dr->value()),
                                Money::fromMajor(cr->value()), QString() });
        }
        const int id = m_ledger->postEntry(date->date(), memo->text().trimmed(),
                                           "manual", entryLines);
        if (id < 0) {
            QMessageBox::warning(&dlg, "Entry Not Posted", m_ledger->lastError());
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted)
        refresh();
}
