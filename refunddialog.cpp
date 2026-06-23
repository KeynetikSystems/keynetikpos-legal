// =============================================================================
// refunddialog.cpp — Implementation of RefundDialog (see refunddialog.h).
// =============================================================================
#include "refunddialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QFormLayout>

RefundDialog::RefundDialog(Database &db, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
{
    if (!UserManager::instance().hasPermission(Permission::ADJUST_STOCK)) {
        QMessageBox::warning(nullptr, "Access Denied",
            "You do not have permission to process refunds.");
        // Schedule immediate rejection after construction completes.
        QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
        return;
    }
    setupUI();
}

void RefundDialog::setupUI()
{
    setWindowTitle("Process Refund");
    setMinimumSize(650, 520);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // ── Title ────────────────────────────────────────────────────────────────
    QLabel *title = new QLabel("Process Refund", this);
    title->setProperty("role", "sectionTitle");
    mainLayout->addWidget(title);

    // ── Top: sale lookup ─────────────────────────────────────────────────────
    QGroupBox   *lookupGroup  = new QGroupBox("Load Sale", this);
    QHBoxLayout *lookupLayout = new QHBoxLayout(lookupGroup);

    lookupLayout->addWidget(new QLabel("Sale ID:", lookupGroup));
    saleIdSpin = new QSpinBox(lookupGroup);
    saleIdSpin->setRange(1, 9999999);
    saleIdSpin->setValue(1);
    saleIdSpin->setFixedWidth(100);
    lookupLayout->addWidget(saleIdSpin);

    loadButton = new QPushButton("Load Sale", lookupGroup);
    loadButton->setProperty("kind", "info");
    connect(loadButton, &QPushButton::clicked, this, &RefundDialog::onLoadSale);
    lookupLayout->addWidget(loadButton);
    lookupLayout->addStretch();

    mainLayout->addWidget(lookupGroup);

    // ── Middle: sale info ────────────────────────────────────────────────────
    infoGroup = new QGroupBox("Sale Details", this);
    infoGroup->setEnabled(false);
    QFormLayout *infoLayout = new QFormLayout(infoGroup);

    saleDateLabel  = new QLabel("—", infoGroup);
    saleTotalLabel = new QLabel("—", infoGroup);
    paymentLabel   = new QLabel("—", infoGroup);
    refundedLabel  = new QLabel("⚠ This sale has already been refunded.", infoGroup);
    refundedLabel->setStyleSheet("color: red; font-weight: bold;");
    refundedLabel->setVisible(false);

    infoLayout->addRow("Date:",           saleDateLabel);
    infoLayout->addRow("Total:",          saleTotalLabel);
    infoLayout->addRow("Payment Method:", paymentLabel);
    infoLayout->addRow("",                refundedLabel);

    mainLayout->addWidget(infoGroup);

    // ── Middle: items table ──────────────────────────────────────────────────
    itemsTable = new QTableWidget(this);
    itemsTable->setColumnCount(4);
    itemsTable->setHorizontalHeaderLabels({"Product", "Qty", "Unit Price", "Subtotal"});
    itemsTable->horizontalHeader()->setStretchLastSection(true);
    itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    itemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    itemsTable->setAlternatingRowColors(true);
    itemsTable->setEnabled(false);
    mainLayout->addWidget(itemsTable);

    // ── Bottom: reason + buttons ─────────────────────────────────────────────
    actionGroup = new QGroupBox("Refund Details", this);
    actionGroup->setEnabled(false);
    QFormLayout *actionLayout = new QFormLayout(actionGroup);

    reasonEdit = new QLineEdit(actionGroup);
    reasonEdit->setPlaceholderText("Required — e.g. Customer returned item, damaged goods…");
    actionLayout->addRow("Reason:", reasonEdit);

    mainLayout->addWidget(actionGroup);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    refundButton = new QPushButton("Process Refund", this);
    refundButton->setProperty("kind", "danger");
    refundButton->setEnabled(false);
    connect(refundButton, &QPushButton::clicked, this, &RefundDialog::onProcessRefund);

    closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(refundButton);
    btnLayout->addStretch();
    btnLayout->addWidget(closeButton);
    mainLayout->addLayout(btnLayout);
}

void RefundDialog::onLoadSale()
{
    const int id = saleIdSpin->value();
    Sale sale = m_db.getSaleById(id);

    if (sale.id <= 0) {
        QMessageBox::warning(this, "Sale Not Found",
            QString("No sale found with ID %1.").arg(id));
        setSaleLoaded(false);
        return;
    }

    m_loadedSaleId = id;
    populateSaleInfo(sale);
    populateSaleItems(m_db.getSaleItems(id));

    const bool alreadyRefunded = m_db.isRefunded(id);
    refundedLabel->setVisible(alreadyRefunded);
    refundButton->setEnabled(!alreadyRefunded);
    setSaleLoaded(true);
}

void RefundDialog::populateSaleInfo(const Sale &sale)
{
    saleDateLabel->setText(sale.saleDate.toString("yyyy-MM-dd HH:mm"));
    saleTotalLabel->setText(currencySymbol() + " " +
        QString::number(sale.total.toMajor(), 'f', 2));
    paymentLabel->setText(sale.paymentMethod);
}

void RefundDialog::populateSaleItems(const QVector<SaleItem> &items)
{
    itemsTable->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const SaleItem &item = items[row];
        itemsTable->setItem(row, 0, new QTableWidgetItem(item.productName));
        itemsTable->setItem(row, 1, new QTableWidgetItem(QString::number(item.quantity)));
        itemsTable->setItem(row, 2, new QTableWidgetItem(
            currencySymbol() + " " + QString::number(item.price.toMajor(), 'f', 2)));
        itemsTable->setItem(row, 3, new QTableWidgetItem(
            currencySymbol() + " " + QString::number(item.subtotal.toMajor(), 'f', 2)));
    }
    itemsTable->resizeColumnsToContents();
}

void RefundDialog::setSaleLoaded(bool loaded)
{
    infoGroup->setEnabled(loaded);
    itemsTable->setEnabled(loaded);
    actionGroup->setEnabled(loaded);
    if (!loaded) {
        refundButton->setEnabled(false);
        saleDateLabel->setText("—");
        saleTotalLabel->setText("—");
        paymentLabel->setText("—");
        refundedLabel->setVisible(false);
        itemsTable->setRowCount(0);
        reasonEdit->clear();
        m_loadedSaleId = -1;
    }
}

void RefundDialog::onProcessRefund()
{
    if (m_loadedSaleId <= 0)
        return;

    const QString reason = reasonEdit->text().trimmed();
    if (reason.isEmpty()) {
        QMessageBox::warning(this, "Reason Required",
            "Please enter a reason for the refund.");
        reasonEdit->setFocus();
        return;
    }

    const QString cashier = UserManager::instance().getCurrentUser().fullName;

    if (!m_db.processRefund(m_loadedSaleId, reason, cashier)) {
        QMessageBox::critical(this, "Refund Failed",
            "Could not process refund: " + m_db.getLastError());
        return;
    }

    QMessageBox::information(this, "Refund Processed",
        QString("Sale #%1 has been successfully refunded.").arg(m_loadedSaleId));
    accept();
}
