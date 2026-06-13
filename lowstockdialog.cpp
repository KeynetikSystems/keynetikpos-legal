// =============================================================================
// lowstockdialog.cpp — Implementation of LowStockDialog (see lowstockdialog.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Two severity tables fed by InventoryManager::getCriticalStockItems() /
//    getLowStockItems(), colour-coded by quantity.
//  - Double-click (or the Restock button) opens a quantity prompt and calls
//    InventoryManager::restockProduct(), then reloads both tables.
// =============================================================================
#include "lowstockdialog.h"
#include "inventorymanager.h"
#include "appstyle.h"
#include "colorscheme.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QDateTime>
#include <QDebug>

LowStockDialog::LowStockDialog(InventoryManager *manager, QWidget *parent)
    : QDialog(parent)
    , inventoryManager(manager)
{
    setupUI();
    loadInventoryData();
}

LowStockDialog::~LowStockDialog()
{
}

void LowStockDialog::setupUI()
{
    setWindowTitle("Low Stock Alert");
    setMinimumSize(900, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title
    titleLabel = new QLabel("📦 Inventory Alert - Low Stock Items", this);
    titleLabel->setProperty("role", "chip");
    titleLabel->setProperty("kind", "danger");
    titleLabel->setProperty("textScale", "xl");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Summary — kind flips between success/danger in loadInventoryData()
    summaryLabel = new QLabel(this);
    summaryLabel->setProperty("role", "chip");
    summaryLabel->setProperty("textScale", "md");
    mainLayout->addWidget(summaryLabel);

    // Critical Stock Section
    criticalGroupBox = new QGroupBox("🚨 Critical Stock (≤4 units)", this);
    criticalGroupBox->setProperty("kind", "danger");

    QVBoxLayout *criticalLayout = new QVBoxLayout(criticalGroupBox);

    criticalTable = new QTableWidget(this);
    criticalTable->setColumnCount(5);
    criticalTable->setHorizontalHeaderLabels(
        {"Product ID", "Product Name", "Current Qty", "Last Restock", "Status"});
    criticalTable->horizontalHeader()->setStretchLastSection(true);
    criticalTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    criticalTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    criticalTable->setAlternatingRowColors(true);

    // Column widths
    criticalTable->setColumnWidth(0, 80);
    criticalTable->setColumnWidth(1, 250);
    criticalTable->setColumnWidth(2, 100);
    criticalTable->setColumnWidth(3, 120);

    connect(criticalTable, &QTableWidget::cellDoubleClicked,
            this, &LowStockDialog::onCriticalItemDoubleClicked);

    criticalLayout->addWidget(criticalTable);
    mainLayout->addWidget(criticalGroupBox);

    // Low Stock Section
    lowStockGroupBox = new QGroupBox("⚠️ Low Stock (5-20 units)", this);
    lowStockGroupBox->setProperty("kind", "warning");

    QVBoxLayout *lowStockLayout = new QVBoxLayout(lowStockGroupBox);

    lowStockTable = new QTableWidget(this);
    lowStockTable->setColumnCount(5);
    lowStockTable->setHorizontalHeaderLabels(
        {"Product ID", "Product Name", "Current Qty", "Last Restock", "Status"});
    lowStockTable->horizontalHeader()->setStretchLastSection(true);
    lowStockTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    lowStockTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lowStockTable->setAlternatingRowColors(true);

    // Column widths
    lowStockTable->setColumnWidth(0, 80);
    lowStockTable->setColumnWidth(1, 250);
    lowStockTable->setColumnWidth(2, 100);
    lowStockTable->setColumnWidth(3, 120);

    connect(lowStockTable, &QTableWidget::cellDoubleClicked,
            this, &LowStockDialog::onLowStockItemDoubleClicked);

    lowStockLayout->addWidget(lowStockTable);
    mainLayout->addWidget(lowStockGroupBox);

    // Button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    refreshButton = new QPushButton("🔄 Refresh", this);
    refreshButton->setProperty("kind", "info");
    connect(refreshButton, &QPushButton::clicked,
            this, &LowStockDialog::onRefreshClicked);

    restockButton = new QPushButton("📦 Quick Restock", this);
    restockButton->setProperty("kind", "primary");
    connect(restockButton, &QPushButton::clicked,
            this, &LowStockDialog::onRestockClicked);

    closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked,
            this, &LowStockDialog::onCloseClicked);

    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(restockButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void LowStockDialog::loadInventoryData()
{
    QVector<InventoryInfo> criticalItems = inventoryManager->getCriticalStockItems();
    QVector<InventoryInfo> lowStockItems = inventoryManager->getLowStockItems();

    // Update summary
    int totalIssues = criticalItems.size() + lowStockItems.size();
    summaryLabel->setText(
        QString("⚠️ Total items requiring attention: %1 (%2 critical, %3 low stock)")
            .arg(totalIssues)
            .arg(criticalItems.size())
            .arg(lowStockItems.size()));

    if (totalIssues == 0) {
        summaryLabel->setText("✅ All inventory levels are healthy!");
        setStyleProperty(summaryLabel, "kind", "success");
    } else {
        setStyleProperty(summaryLabel, "kind", "danger");
    }

    populateCriticalTable(criticalItems);
    populateLowStockTable(lowStockItems);
}

void LowStockDialog::populateCriticalTable(const QVector<InventoryInfo> &items)
{
    criticalTable->setRowCount(0);

    if (items.isEmpty()) {
        criticalGroupBox->setTitle("🚨 Critical Stock (≤4 units) - None");
        return;
    }

    criticalGroupBox->setTitle(
        QString("🚨 Critical Stock (≤4 units) - %1 items").arg(items.size()));

    for (const auto &item : items) {
        int row = criticalTable->rowCount();
        criticalTable->insertRow(row);

        // Product ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(item.productId));
        idItem->setTextAlignment(Qt::AlignCenter);
        criticalTable->setItem(row, 0, idItem);

        // Product Name
        QTableWidgetItem *nameItem = new QTableWidgetItem(item.productName);
        criticalTable->setItem(row, 1, nameItem);

        // Current Quantity
        QTableWidgetItem *qtyItem = new QTableWidgetItem(QString::number(item.currentQuantity));
        qtyItem->setTextAlignment(Qt::AlignCenter);
        qtyItem->setBackground(QBrush(QColor(getStatusColor(item.currentQuantity))));
        qtyItem->setForeground(QBrush(Qt::white));
        QFont boldFont = qtyItem->font();
        boldFont.setBold(true);
        qtyItem->setFont(boldFont);
        criticalTable->setItem(row, 2, qtyItem);

        // Last Restock
        QTableWidgetItem *dateItem = new QTableWidgetItem(formatDate(item.lastRestockDate));
        dateItem->setTextAlignment(Qt::AlignCenter);
        criticalTable->setItem(row, 3, dateItem);

        // Status
        QString statusText = item.currentQuantity == 0 ? "OUT OF STOCK" : "CRITICAL";
        QTableWidgetItem *statusItem = new QTableWidgetItem(statusText);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(QBrush(QColor(getColorScheme().error)));
        QFont statusFont = statusItem->font();
        statusFont.setBold(true);
        statusItem->setFont(statusFont);
        criticalTable->setItem(row, 4, statusItem);
    }
}

void LowStockDialog::populateLowStockTable(const QVector<InventoryInfo> &items)
{
    lowStockTable->setRowCount(0);

    if (items.isEmpty()) {
        lowStockGroupBox->setTitle("⚠️ Low Stock (5-20 units) - None");
        return;
    }

    lowStockGroupBox->setTitle(
        QString("⚠️ Low Stock (5-20 units) - %1 items").arg(items.size()));

    for (const auto &item : items) {
        int row = lowStockTable->rowCount();
        lowStockTable->insertRow(row);

        // Product ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(item.productId));
        idItem->setTextAlignment(Qt::AlignCenter);
        lowStockTable->setItem(row, 0, idItem);

        // Product Name
        QTableWidgetItem *nameItem = new QTableWidgetItem(item.productName);
        lowStockTable->setItem(row, 1, nameItem);

        // Current Quantity
        QTableWidgetItem *qtyItem = new QTableWidgetItem(QString::number(item.currentQuantity));
        qtyItem->setTextAlignment(Qt::AlignCenter);
        qtyItem->setBackground(QBrush(QColor(getStatusColor(item.currentQuantity))));
        qtyItem->setForeground(QBrush(Qt::white));
        QFont boldFont = qtyItem->font();
        boldFont.setBold(true);
        qtyItem->setFont(boldFont);
        lowStockTable->setItem(row, 2, qtyItem);

        // Last Restock
        QTableWidgetItem *dateItem = new QTableWidgetItem(formatDate(item.lastRestockDate));
        dateItem->setTextAlignment(Qt::AlignCenter);
        lowStockTable->setItem(row, 3, dateItem);

        // Status
        QTableWidgetItem *statusItem = new QTableWidgetItem("LOW STOCK");
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(QBrush(QColor(getColorScheme().warning)));
        QFont statusFont = statusItem->font();
        statusFont.setBold(true);
        statusItem->setFont(statusFont);
        lowStockTable->setItem(row, 4, statusItem);
    }
}

void LowStockDialog::onRefreshClicked()
{
    loadInventoryData();
    QMessageBox::information(this, "Refreshed",
                             "Inventory data has been refreshed.");
}

void LowStockDialog::onRestockClicked()
{
    // Get selected item from either table
    QTableWidget *activeTable = nullptr;

    if (criticalTable->currentRow() >= 0) {
        activeTable = criticalTable;
    } else if (lowStockTable->currentRow() >= 0) {
        activeTable = lowStockTable;
    } else {
        QMessageBox::information(this, "No Selection",
                                 "Please select an item to restock by clicking on a row.");
        return;
    }

    int row = activeTable->currentRow();
    int productId = activeTable->item(row, 0)->text().toInt();
    QString productName = activeTable->item(row, 1)->text();
    int currentQty = activeTable->item(row, 2)->text().toInt();

    showRestockDialog(productId, productName, currentQty);
}

void LowStockDialog::onCloseClicked()
{
    accept();
}

void LowStockDialog::onCriticalItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    int productId = criticalTable->item(row, 0)->text().toInt();
    QString productName = criticalTable->item(row, 1)->text();
    int currentQty = criticalTable->item(row, 2)->text().toInt();

    showRestockDialog(productId, productName, currentQty);
}

void LowStockDialog::onLowStockItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    int productId = lowStockTable->item(row, 0)->text().toInt();
    QString productName = lowStockTable->item(row, 1)->text();
    int currentQty = lowStockTable->item(row, 2)->text().toInt();

    showRestockDialog(productId, productName, currentQty);
}

void LowStockDialog::showRestockDialog(int productId, const QString &productName, int currentQty)
{
    bool ok;
    int quantity = QInputDialog::getInt(
        this,
        "Restock Item",
        QString("Restock %1\nCurrent quantity: %2\n\nEnter quantity to add:")
            .arg(productName)
            .arg(currentQty),
        50,  // Default value
        1,   // Minimum
        1000, // Maximum
        1,   // Step
        &ok);

    if (!ok) return;

    QString supplier = QInputDialog::getText(
        this,
        "Supplier Name",
        "Enter supplier name (optional):",
        QLineEdit::Normal,
        "Main Supplier",
        &ok);

    if (!ok) supplier = "Main Supplier";

    if (inventoryManager->restockProduct(productId, quantity, supplier)) {
        QMessageBox::information(
            this,
            "Restock Successful",
            QString("%1 has been restocked.\n\n"
                    "Added: %2 units\n"
                    "New quantity: %3 units")
                .arg(productName)
                .arg(quantity)
                .arg(currentQty + quantity));

        loadInventoryData(); // Refresh the display
    } else {
        QMessageBox::critical(
            this,
            "Restock Failed",
            QString("Failed to restock %1.\nPlease try again.").arg(productName));
    }
}

QString LowStockDialog::getStatusColor(int quantity) const
{
    const ColorScheme scheme = getColorScheme();
    if (quantity == 0) return scheme.textSecondary;   // Gray - Out of stock
    if (quantity <= 4) return scheme.error;           // Red - Critical
    if (quantity <= 20) return scheme.warning;        // Orange - Low
    return scheme.accentPrimary;                      // Green - Healthy
}

QString LowStockDialog::formatDate(const QString &dateStr) const
{
    if (dateStr.isEmpty()) return "N/A";

    QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");
    if (!date.isValid()) return dateStr;

    return date.toString("MMM dd, yyyy");
}
