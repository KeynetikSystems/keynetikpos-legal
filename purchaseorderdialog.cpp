// =============================================================================
// purchaseorderdialog.cpp — Implementation of NewPurchaseOrderDialog and
// PurchaseOrderDialog (see purchaseorderdialog.h for the full WHAT/HOW/WHY).
// =============================================================================
#include "purchaseorderdialog.h"
#include "usermanager.h"
#include "money.h"
#include "database.h"
#include "productrepository.h"
#include "purchaseorderrepository.h"
#include "supplierrepository.h"
#include "ledger.h"
#include "salejournal.h"
#include "vat.h"
#include <QHeaderView>
#include <QFormLayout>
#include <QMessageBox>
#include <QGroupBox>
#include <QSqlDatabase>
#include <QDate>

// ─────────────────────────────────────────────────────────────────────────────
// NewPurchaseOrderDialog
// ─────────────────────────────────────────────────────────────────────────────
NewPurchaseOrderDialog::NewPurchaseOrderDialog(Database &db, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
{
    setupUI();
}

void NewPurchaseOrderDialog::setupUI()
{
    setWindowTitle("New Purchase Order");
    setMinimumSize(650, 550);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QFormLayout *headerForm = new QFormLayout();
    supplierCombo = new QComboBox(this);
    for (const Supplier &s : m_db.suppliers().getAllSuppliers())
        supplierCombo->addItem(s.name, s.id);
    headerForm->addRow("Supplier:", supplierCombo);
    mainLayout->addLayout(headerForm);

    if (supplierCombo->count() == 0) {
        QLabel *warn = new QLabel(
            "No suppliers yet — add one from Inventory → Suppliers first.", this);
        warn->setProperty("role", "banner");
        warn->setProperty("kind", "warning");
        mainLayout->addWidget(warn);
    }

    // Line item entry row
    QGroupBox *lineGroup = new QGroupBox("Add Line Item", this);
    QHBoxLayout *lineLayout = new QHBoxLayout(lineGroup);

    productCombo = new QComboBox(this);
    m_products = m_db.products().getAllProducts();
    for (const Product &p : m_products)
        productCombo->addItem(p.name, p.id);
    lineLayout->addWidget(new QLabel("Product:", this));
    lineLayout->addWidget(productCombo, 2);

    qtySpin = new QSpinBox(this);
    qtySpin->setRange(1, 100000);
    qtySpin->setValue(1);
    lineLayout->addWidget(new QLabel("Qty:", this));
    lineLayout->addWidget(qtySpin);

    unitCostSpin = new QDoubleSpinBox(this);
    unitCostSpin->setRange(0.0, 1000000.0);
    unitCostSpin->setDecimals(2);
    unitCostSpin->setPrefix(currencySymbol() + " ");
    // Pre-fill with the product's current cost so the common case (re-ordering
    // at the same price) needs no typing; user overrides for a price change.
    auto fillCostFromProduct = [this](int index) {
        if (index < 0 || index >= m_products.size()) return;
        unitCostSpin->setValue(m_products[index].costPrice.toMajor());
    };
    connect(productCombo, &QComboBox::currentIndexChanged, this, fillCostFromProduct);
    if (!m_products.isEmpty())
        fillCostFromProduct(0);
    lineLayout->addWidget(new QLabel("Unit Cost:", this));
    lineLayout->addWidget(unitCostSpin);

    addLineButton = new QPushButton("Add Line", this);
    addLineButton->setProperty("kind", "primary");
    connect(addLineButton, &QPushButton::clicked, this, &NewPurchaseOrderDialog::onAddLineClicked);
    lineLayout->addWidget(addLineButton);

    mainLayout->addWidget(lineGroup);

    lineTable = new QTableWidget(this);
    lineTable->setColumnCount(4);
    lineTable->setHorizontalHeaderLabels({"Product", "Qty", "Unit Cost", "Subtotal"});
    lineTable->horizontalHeader()->setStretchLastSection(true);
    lineTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    lineTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(lineTable);

    QHBoxLayout *lineActionsLayout = new QHBoxLayout();
    removeLineButton = new QPushButton("Remove Selected Line", this);
    removeLineButton->setProperty("kind", "danger");
    connect(removeLineButton, &QPushButton::clicked, this, &NewPurchaseOrderDialog::onRemoveLineClicked);
    lineActionsLayout->addWidget(removeLineButton);
    lineActionsLayout->addStretch();
    mainLayout->addLayout(lineActionsLayout);

    totalLabel = new QLabel(this);
    totalLabel->setProperty("role", "amountTotal");
    totalLabel->setAlignment(Qt::AlignRight);
    mainLayout->addWidget(totalLabel);
    refreshTotal();

    QHBoxLayout *footerLayout = new QHBoxLayout();
    createButton = new QPushButton("Create Purchase Order", this);
    createButton->setProperty("kind", "primary");
    connect(createButton, &QPushButton::clicked, this, &NewPurchaseOrderDialog::onCreateClicked);
    cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    footerLayout->addStretch();
    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(createButton);
    mainLayout->addLayout(footerLayout);
}

void NewPurchaseOrderDialog::onAddLineClicked()
{
    const int index = productCombo->currentIndex();
    if (index < 0 || index >= m_products.size())
        return;

    const Product &p = m_products[index];
    const int qty = qtySpin->value();
    const Money unitCost = Money::fromMajor(unitCostSpin->value());

    PurchaseOrderItem item;
    item.productId   = p.id;
    item.productName = p.name;
    item.quantity     = qty;
    item.unitCost     = unitCost;
    item.subtotal     = unitCost * qty;
    m_lines.append(item);

    const int row = lineTable->rowCount();
    lineTable->insertRow(row);
    lineTable->setItem(row, 0, new QTableWidgetItem(item.productName));
    lineTable->setItem(row, 1, new QTableWidgetItem(QString::number(item.quantity)));
    lineTable->setItem(row, 2, new QTableWidgetItem(formatMoney(item.unitCost)));
    lineTable->setItem(row, 3, new QTableWidgetItem(formatMoney(item.subtotal)));

    refreshTotal();
}

void NewPurchaseOrderDialog::onRemoveLineClicked()
{
    const auto selected = lineTable->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;
    const int row = selected.first().row();
    lineTable->removeRow(row);
    m_lines.remove(row);
    refreshTotal();
}

void NewPurchaseOrderDialog::refreshTotal()
{
    Money total;
    for (const PurchaseOrderItem &line : m_lines)
        total += line.subtotal;
    totalLabel->setText("Total: " + formatMoney(total));
}

void NewPurchaseOrderDialog::onCreateClicked()
{
    if (supplierCombo->count() == 0) {
        QMessageBox::warning(this, "No Supplier", "Add a supplier before creating a purchase order.");
        return;
    }
    if (m_lines.isEmpty()) {
        QMessageBox::warning(this, "No Items", "Add at least one line item.");
        return;
    }

    const int supplierId = supplierCombo->currentData().toInt();
    const QString createdBy = UserManager::instance().getCurrentUsername();

    const int poId = m_db.purchaseOrders().createPurchaseOrder(
        supplierId, m_lines, QString(), createdBy);
    if (poId < 0) {
        QMessageBox::critical(this, "Error",
            "Failed to create purchase order: " + m_db.purchaseOrders().lastError());
        return;
    }
    QMessageBox::information(this, "Purchase Order Created",
        QString("Purchase Order #%1 created. Use 'Receive' once stock arrives "
                "to update inventory and cost.").arg(poId));
    accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// PurchaseOrderDialog
// ─────────────────────────────────────────────────────────────────────────────
PurchaseOrderDialog::PurchaseOrderDialog(Database &db, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
{
    setupUI();
    loadOrders();
}

void PurchaseOrderDialog::setupUI()
{
    setWindowTitle("Purchase Orders");
    setMinimumSize(850, 550);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *title = new QLabel("Purchase Orders", this);
    title->setProperty("role", "sectionTitle");
    mainLayout->addWidget(title);

    table = new QTableWidget(this);
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels(
        {"ID", "Supplier", "Status", "Order Date", "Received Date", "Created By", "Total"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    mainLayout->addWidget(table);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    newOrderButton = new QPushButton("New Purchase Order", this);
    newOrderButton->setProperty("kind", "primary");
    viewItemsButton = new QPushButton("View Items", this);
    viewItemsButton->setProperty("kind", "info");
    // "success" isn't a wired QPushButton kind (only primary/danger/warning/
    // info/tertiary are, see appstyle.cpp's kindBlock calls) — primary is the
    // closest semantic fit for "the main positive action on this row".
    receiveButton = new QPushButton("Receive (Updates Stock)", this);
    receiveButton->setProperty("kind", "primary");
    cancelOrderButton = new QPushButton("Cancel Order", this);
    cancelOrderButton->setProperty("kind", "danger");
    closeButton = new QPushButton("Close", this);

    connect(newOrderButton, &QPushButton::clicked, this, &PurchaseOrderDialog::onNewOrderClicked);
    connect(viewItemsButton, &QPushButton::clicked, this, &PurchaseOrderDialog::onViewItemsClicked);
    connect(receiveButton, &QPushButton::clicked, this, &PurchaseOrderDialog::onReceiveClicked);
    connect(cancelOrderButton, &QPushButton::clicked, this, &PurchaseOrderDialog::onCancelOrderClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(newOrderButton);
    buttonLayout->addWidget(viewItemsButton);
    buttonLayout->addWidget(receiveButton);
    buttonLayout->addWidget(cancelOrderButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);
}

void PurchaseOrderDialog::loadOrders()
{
    const QVector<PurchaseOrder> orders = m_db.purchaseOrders().getAllPurchaseOrders();
    table->setRowCount(orders.size());
    for (int row = 0; row < orders.size(); ++row) {
        const PurchaseOrder &po = orders[row];
        table->setItem(row, 0, new QTableWidgetItem(QString::number(po.id)));
        table->setItem(row, 1, new QTableWidgetItem(po.supplierName));
        table->setItem(row, 2, new QTableWidgetItem(po.status));
        table->setItem(row, 3, new QTableWidgetItem(po.orderDate));
        table->setItem(row, 4, new QTableWidgetItem(po.receivedDate));
        table->setItem(row, 5, new QTableWidgetItem(po.createdBy));
        table->setItem(row, 6, new QTableWidgetItem(formatMoney(po.total)));
    }
}

int PurchaseOrderDialog::selectedOrderId() const
{
    const auto selected = table->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return -1;
    return table->item(selected.first().row(), 0)->text().toInt();
}

void PurchaseOrderDialog::onNewOrderClicked()
{
    NewPurchaseOrderDialog dlg(m_db, this);
    if (dlg.exec() == QDialog::Accepted)
        loadOrders();
}

void PurchaseOrderDialog::onViewItemsClicked()
{
    const int id = selectedOrderId();
    if (id < 0) {
        QMessageBox::information(this, "No Selection", "Select a purchase order to view.");
        return;
    }

    const QVector<PurchaseOrderItem> items = m_db.purchaseOrders().getPurchaseOrderItems(id);
    QString text;
    for (const PurchaseOrderItem &item : items) {
        text += QString("%1  x%2  @ %3  = %4\n")
                    .arg(item.productName)
                    .arg(item.quantity)
                    .arg(formatMoney(item.unitCost))
                    .arg(formatMoney(item.subtotal));
    }
    if (text.isEmpty())
        text = "(no line items)";
    QMessageBox::information(this, QString("Purchase Order #%1 — Items").arg(id), text);
}

void PurchaseOrderDialog::onReceiveClicked()
{
    const int id = selectedOrderId();
    if (id < 0) {
        QMessageBox::information(this, "No Selection", "Select a purchase order to receive.");
        return;
    }
    if (QMessageBox::question(this, "Receive Purchase Order",
            QString("Receive Purchase Order #%1? This will increase stock for "
                    "every line item and update each product's cost price.").arg(id))
        != QMessageBox::Yes)
        return;

    const QString receivedBy = UserManager::instance().getCurrentUsername();
    if (!m_db.purchaseOrders().receivePurchaseOrder(id, receivedBy)) {
        QMessageBox::critical(this, "Error",
            "Failed to receive purchase order: " + m_db.purchaseOrders().lastError());
        return;
    }
    // Auto-post the received goods to the General Ledger:
    // Dr Inventory (net) + Dr VAT Input / Cr Accounts Payable. Best-effort —
    // a ledger hiccup must not undo the stock receipt that already committed.
    {
        Vat vat(QSqlDatabase::database());
        vat.initSchema();
        const Money inputVat = vat.purchaseOrderInputVat(id);
        const PurchaseOrder po = m_db.purchaseOrders().getPurchaseOrderById(id);
        const QVector<GLLine> lines = buildPurchaseJournal(po.total, inputVat);
        if (!lines.isEmpty()) {
            Ledger ledger(QSqlDatabase::database());
            ledger.initSchema();
            if (ledger.postEntry(QDate::currentDate(),
                                 QString("Purchase Order #%1").arg(id),
                                 "purchase", lines) < 0) {
                UserManager::instance().logUserAction(
                    "Ledger Posting Failed",
                    QString("PO #%1 not posted to GL: %2").arg(id).arg(ledger.lastError()));
            }
        }
    }

    QMessageBox::information(this, "Received", "Stock and cost prices updated.");
    loadOrders();
}

void PurchaseOrderDialog::onCancelOrderClicked()
{
    const int id = selectedOrderId();
    if (id < 0) {
        QMessageBox::information(this, "No Selection", "Select a purchase order to cancel.");
        return;
    }
    if (QMessageBox::question(this, "Cancel Purchase Order",
            QString("Cancel Purchase Order #%1?").arg(id))
        != QMessageBox::Yes)
        return;
    if (!m_db.purchaseOrders().cancelPurchaseOrder(id)) {
        QMessageBox::critical(this, "Error",
            "Failed to cancel purchase order: " + m_db.purchaseOrders().lastError());
        return;
    }
    loadOrders();
}
