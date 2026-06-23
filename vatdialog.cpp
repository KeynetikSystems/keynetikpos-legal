// =============================================================================
// vatdialog.cpp — Implementation of VatDialog (see vatdialog.h).
// =============================================================================
#include "vatdialog.h"
#include "vat.h"
#include "cart.h"      // formatMoney()

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QDateEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>

VatDialog::VatDialog(Vat *vat, QWidget *parent)
    : QDialog(parent), m_vat(vat)
{
    setWindowTitle("VAT");
    setMinimumSize(760, 600);

    auto *root = new QVBoxLayout(this);
    auto *title = new QLabel("VAT");
    title->setProperty("role", "dialogTitle");
    root->addWidget(title);

    auto *tabs = new QTabWidget();
    root->addWidget(tabs, 1);

    // ── Tab 1: VAT-3 return ─────────────────────────────────────────────────
    auto *retTab = new QWidget();
    auto *retLay = new QVBoxLayout(retTab);

    auto *period = new QHBoxLayout();
    period->addWidget(new QLabel("From:"));
    m_from = new QDateEdit(QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1));
    m_from->setCalendarPopup(true);
    period->addWidget(m_from);
    period->addWidget(new QLabel("To:"));
    m_to = new QDateEdit(QDate::currentDate());
    m_to->setCalendarPopup(true);
    period->addWidget(m_to);
    auto *computeBtn = new QPushButton("Compute VAT-3");
    computeBtn->setProperty("kind", "primary");
    connect(computeBtn, &QPushButton::clicked, this, &VatDialog::computeReturn);
    period->addWidget(computeBtn);
    period->addStretch();
    m_rateLabel = new QLabel();
    m_rateLabel->setToolTip("The standard VAT rate follows the sale-tax rate in "
                            "Settings → Tax.");
    period->addWidget(m_rateLabel);
    retLay->addLayout(period);

    auto *grid = new QGroupBox("VAT-3 Summary");
    auto *form = new QFormLayout(grid);
    auto mk = [](){ auto *l = new QLabel("—"); l->setProperty("bold", "true"); return l; };
    m_outNet = mk(); m_outVat = mk(); m_zero = mk(); m_exempt = mk();
    m_inNet = mk(); m_inVat = mk(); m_netPayable = mk();
    m_netPayable->setProperty("role", "amountTotal");
    form->addRow("Standard-rated sales (net):", m_outNet);
    form->addRow("Output VAT (on sales):", m_outVat);
    form->addRow("Zero-rated sales:", m_zero);
    form->addRow("Exempt sales:", m_exempt);
    form->addRow("Standard-rated purchases (net):", m_inNet);
    form->addRow("Input VAT (recoverable):", m_inVat);
    form->addRow("Net VAT payable:", m_netPayable);
    retLay->addWidget(grid);

    auto *payBtn = new QPushButton("Record VAT Payment to KRA...");
    connect(payBtn, &QPushButton::clicked, this, &VatDialog::recordPayment);
    retLay->addWidget(payBtn);

    auto *note = new QLabel(
        "VAT-3 is computed directly from sales and received purchase orders. "
        "Verify rates and treatment against the current VAT Act before filing.");
    note->setProperty("kind", "secondary");
    note->setWordWrap(true);
    retLay->addWidget(note);
    retLay->addStretch();
    tabs->addTab(retTab, "VAT-3 Return");

    // ── Tab 2: product tax codes ────────────────────────────────────────────
    auto *codeTab = new QWidget();
    auto *codeLay = new QVBoxLayout(codeTab);
    codeLay->addWidget(new QLabel("Set the VAT treatment for each product:"));
    m_prodTable = new QTableWidget();
    m_prodTable->setColumnCount(2);
    m_prodTable->setHorizontalHeaderLabels({ "Product", "Tax Code" });
    m_prodTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    // Fixed, comfortably wide column for the tax-code picker so the labels fit.
    m_prodTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_prodTable->setColumnWidth(1, 180);
    m_prodTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // The row-number gutter and full-row selection add noise on a settings grid;
    // each row's only action is its own combo. Taller rows so the styled combos
    // (padding + 2px border) aren't vertically clipped — which made them look
    // like empty boxes.
    m_prodTable->verticalHeader()->setVisible(false);
    m_prodTable->verticalHeader()->setDefaultSectionSize(42);
    m_prodTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_prodTable->setFocusPolicy(Qt::NoFocus);
    codeLay->addWidget(m_prodTable);
    tabs->addTab(codeTab, "Product Tax Codes");

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    m_rateLabel->setText(QString("Standard rate: %1%  (from Settings → Tax)")
                             .arg(m_vat->standardRate() * 100, 0, 'g', 4));
    refreshProducts();
    computeReturn();
}

void VatDialog::computeReturn()
{
    const Vat3 v = m_vat->computeVat3(m_from->date(), m_to->date());
    m_outNet->setText(formatMoney(v.salesStandardNet));
    m_outVat->setText(formatMoney(v.outputVat));
    m_zero->setText(formatMoney(v.salesZero));
    m_exempt->setText(formatMoney(v.salesExempt));
    m_inNet->setText(formatMoney(v.purchasesStandardNet));
    m_inVat->setText(formatMoney(v.inputVat));
    m_lastNetPayable = v.netPayable();
    const bool refund = m_lastNetPayable.cents() < 0;
    m_netPayable->setText(formatMoney(m_lastNetPayable)
                          + (refund ? "  (refund/credit)" : "  (payable)"));
}

void VatDialog::recordPayment()
{
    if (m_lastNetPayable.cents() <= 0) {
        QMessageBox::information(this, "Nothing to Pay",
            "Net VAT for this period is not payable (zero or a credit).");
        return;
    }
    if (QMessageBox::question(this, "Record VAT Payment",
            "Post a VAT payment of " + formatMoney(m_lastNetPayable)
            + " to the ledger (Dr VAT Output / Cr Bank)?") != QMessageBox::Yes)
        return;

    const int entry = m_vat->postSettlement(m_to->date(), m_lastNetPayable);
    if (entry < 0) {
        QMessageBox::warning(this, "Posting Failed", m_vat->lastError());
        return;
    }
    QMessageBox::information(this, "VAT Payment Posted",
        "Recorded as ledger entry #" + QString::number(entry) + ".");
}

void VatDialog::refreshProducts()
{
    m_prodTable->setRowCount(0);
    QSqlQuery q(QSqlDatabase::database());
    if (!q.exec("SELECT id, name, COALESCE(tax_code, 'standard') "
                "FROM products WHERE is_active = 1 ORDER BY name"))
        return;

    while (q.next()) {
        const int id = q.value(0).toInt();
        const int r = m_prodTable->rowCount();
        m_prodTable->insertRow(r);
        m_prodTable->setItem(r, 0, new QTableWidgetItem(q.value(1).toString()));

        auto *combo = new QComboBox();
        combo->addItem(taxCodeLabel(TaxCode::Standard), int(TaxCode::Standard));
        combo->addItem(taxCodeLabel(TaxCode::Zero),     int(TaxCode::Zero));
        combo->addItem(taxCodeLabel(TaxCode::Exempt),   int(TaxCode::Exempt));
        const TaxCode cur = taxCodeFromString(q.value(2).toString());
        combo->setCurrentIndex(combo->findData(int(cur)));
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, id]() {
            m_vat->setProductTaxCode(id, TaxCode(combo->currentData().toInt()));
        });

        // Wrap so the combo has a little breathing room inside the cell rather
        // than butting against the grid lines.
        auto *cell = new QWidget();
        auto *cellLay = new QHBoxLayout(cell);
        cellLay->setContentsMargins(6, 4, 6, 4);
        cellLay->addWidget(combo);
        m_prodTable->setCellWidget(r, 1, cell);
    }
}
