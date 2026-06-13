// =============================================================================
// shift_dialogs.cpp — Implementation of OpenShiftDialog / CloseShiftDialog
// (see shift_dialogs.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Both are thin form dialogs exposing values via getters; the caller passes
//    them to ShiftManager.
//  - CloseShiftDialog displays the running ShiftRecord totals (with currency
//    symbol) so the cashier can see expected cash while counting the drawer —
//    the counted closing float is what makes the over/short math meaningful.
// =============================================================================

#include "shift_dialogs.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QFrame>

// =============================================================================
// OpenShiftDialog
// =============================================================================

OpenShiftDialog::OpenShiftDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("🕐 Open Shift");
    setFixedSize(380, 260);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto *header = new QLabel("Open a New Shift", this);
    header->setProperty("role", "sectionTitle");
    header->setProperty("textScale", "lg");
    layout->addWidget(header);

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setProperty("role", "hline");
    layout->addWidget(line);

    auto *group = new QGroupBox("Shift Details");
    auto *form = new QFormLayout(group);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_cashierName = new QLineEdit(group);
    m_cashierName->setPlaceholderText("e.g. Alice");
    form->addRow("Cashier Name:", m_cashierName);

    m_openingFloat = new QDoubleSpinBox(group);
    m_openingFloat->setRange(0.0, 99999.99);
    m_openingFloat->setDecimals(2);
    m_openingFloat->setPrefix("$ ");
    m_openingFloat->setFixedWidth(130);
    form->addRow("Opening Float:", m_openingFloat);

    layout->addWidget(group);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto *openBtn = new QPushButton("✔ Open Shift", this);
    openBtn->setDefault(true);
    openBtn->setProperty("kind", "primary");
    connect(openBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(openBtn);

    layout->addLayout(btnLayout);
}

QString OpenShiftDialog::cashierName()  const { return m_cashierName->text().trimmed(); }
double  OpenShiftDialog::openingFloat() const { return m_openingFloat->value(); }

// =============================================================================
// CloseShiftDialog
// =============================================================================

CloseShiftDialog::CloseShiftDialog(const ShiftRecord &shift,
                                   const QString &sym,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("🕐 Close Shift");
    setMinimumSize(420, 380);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *header = new QLabel("Close Current Shift", this);
    header->setProperty("role", "sectionTitle");
    header->setProperty("kind", "danger");
    header->setProperty("textScale", "lg");
    layout->addWidget(header);

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setProperty("role", "hline");
    layout->addWidget(line);

    // Summary
    auto *summaryGroup = new QGroupBox(QString("Shift Summary — %1").arg(shift.cashierName));
    auto *sform = new QFormLayout(summaryGroup);
    sform->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // kind drives the value colour via the app-wide QLabel[kind=...] selectors;
    // empty kind leaves the default themed text colour.
    auto addRow = [&](const QString &label, const QString &value, const char *kind = "") {
        auto *lbl = new QLabel(value, summaryGroup);
        lbl->setProperty("bold", "true");
        if (kind[0]) lbl->setProperty("kind", kind);
        sform->addRow(label, lbl);
    };

    addRow("Opened At:",     shift.openedAt.toString("hh:mm AP"));
    addRow("Transactions:",  QString::number(shift.transactionCount));
    addRow("Total Sales:",   QString("%1%2").arg(sym).arg(shift.totalSales, 0, 'f', 2), "success");
    if (shift.totalDiscounts > 0)
        addRow("Discounts Given:", QString("-%1%2").arg(sym).arg(shift.totalDiscounts, 0, 'f', 2), "warning");
    addRow("Opening Float:", QString("%1%2").arg(sym).arg(shift.openingFloat, 0, 'f', 2));
    addRow("Expected Cash:", QString("%1%2").arg(sym).arg(
                                 shift.openingFloat + shift.totalSales, 0, 'f', 2), "info");

    layout->addWidget(summaryGroup);

    // Reconciliation
    auto *closeGroup = new QGroupBox("Reconciliation");
    auto *cform = new QFormLayout(closeGroup);
    cform->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_closingFloat = new QDoubleSpinBox(closeGroup);
    m_closingFloat->setRange(0.0, 99999.99);
    m_closingFloat->setDecimals(2);
    m_closingFloat->setPrefix(sym + " ");
    m_closingFloat->setFixedWidth(140);
    cform->addRow("Closing Float (Counted):", m_closingFloat);

    m_notes = new QTextEdit(closeGroup);
    m_notes->setPlaceholderText("Optional notes about this shift...");
    m_notes->setFixedHeight(65);
    cform->addRow("Notes:", m_notes);

    layout->addWidget(closeGroup);

    // Buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto *closeBtn = new QPushButton("✔ Close Shift", this);
    closeBtn->setDefault(true);
    closeBtn->setProperty("kind", "danger");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);

    layout->addLayout(btnLayout);
}

double  CloseShiftDialog::closingFloat() const { return m_closingFloat->value(); }
QString CloseShiftDialog::notes()        const { return m_notes->toPlainText().trimmed(); }
