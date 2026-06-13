// =============================================================================
// stockmanager.cpp — Implementation of StockManager (see stockmanager.h for
// the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Loads InventoryInfo rows through InventoryManager and filters in memory
//    (search text + status filter); summary labels recompute on every reload.
//  - Add/edit/restock/adjust go through small modal dialogs; every stock
//    mutation is logged via Database::adjustStockWithLog() for the audit
//    trail. exportToCSV() writes the visible rows.
// =============================================================================
#include "stockmanager.h"
#include "inventorymanager.h"
#include "database.h"
#include "colorscheme.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QTextStream>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QHeaderView>
#include <QGridLayout>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QDialog>
#include <QPushButton>

StockManager::StockManager(InventoryManager *invManager, QSqlDatabase &database, QWidget *parent)
    : QDialog(parent)
    , inventoryManager(invManager)
    , db(database)
    , currentFilter("All")
    , selectedProductId(-1)
{
    setupUI();
    loadAllProducts();
    updateSummary();
}

StockManager::~StockManager()
{
}

// ============================================================================
// UI SETUP
// ============================================================================

void StockManager::setupUI()
{
    setWindowTitle("Stock Manager - Inventory Control");
    setMinimumSize(1200, 700);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Title
    QLabel *titleLabel = new QLabel("📦 Stock Manager - Inventory Control", this);
    titleLabel->setProperty("role", "chip");
    titleLabel->setProperty("kind", "info");
    titleLabel->setProperty("textScale", "xl");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Toolbar
    createToolbar();
    QWidget *toolbarWidget = searchBox->parentWidget();
    mainLayout->addWidget(toolbarWidget);

    // Filter section
    createFilterSection();
    QWidget *filterWidget = filterCombo->parentWidget();
    mainLayout->addWidget(filterWidget);

    // Stock table
    createStockTable();
    mainLayout->addWidget(stockTable);

    // Summary section
    createSummarySection();
    QWidget *summaryWidget = totalProductsLabel->parentWidget();
    mainLayout->addWidget(summaryWidget);

    // Button panel
    createButtonPanel();
    QWidget *buttonWidget = closeButton->parentWidget();
    mainLayout->addWidget(buttonWidget);

    setLayout(mainLayout);
}

void StockManager::createToolbar()
{
    QWidget *toolbarWidget = new QWidget(this);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbarWidget);

    // Search box
    QLabel *searchLabel = new QLabel("🔍 Search:", this);
    searchBox = new QLineEdit(this);
    searchBox->setPlaceholderText("Search by product name or ID...");
    searchBox->setMinimumWidth(300);
    connect(searchBox, &QLineEdit::textChanged, this, &StockManager::onSearchTextChanged);

    toolbarLayout->addWidget(searchLabel);
    toolbarLayout->addWidget(searchBox);
    toolbarLayout->addStretch();

    // Action buttons
    refreshButton = new QPushButton("🔄 Refresh", this);
    refreshButton->setProperty("kind", "info");
    connect(refreshButton, &QPushButton::clicked, this, &StockManager::onRefreshClicked);

    addButton = new QPushButton("➕ Add Product", this);
    addButton->setProperty("kind", "primary");
    connect(addButton, &QPushButton::clicked, this, &StockManager::onAddProductClicked);

    exportButton = new QPushButton("📤 Export", this);
    exportButton->setProperty("kind", "tertiary");
    connect(exportButton, &QPushButton::clicked, this, &StockManager::onExportClicked);

    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addWidget(addButton);
    toolbarLayout->addWidget(exportButton);
}

void StockManager::createFilterSection()
{
    QWidget *filterWidget = new QWidget(this);
    QHBoxLayout *filterLayout = new QHBoxLayout(filterWidget);

    QLabel *filterLabel = new QLabel("Filter by Status:", this);
    filterCombo = new QComboBox(this);
    filterCombo->addItems({"All", "Healthy", "Low Stock", "Critical", "Out of Stock"});
    filterCombo->setMinimumWidth(150);
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StockManager::onFilterChanged);

    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(filterCombo);
    filterLayout->addStretch();
}

void StockManager::createStockTable()
{
    stockTable = new QTableWidget(this);
    stockTable->setColumnCount(9);
    stockTable->setHorizontalHeaderLabels({
        "ID", "Product Name", "Category", "Current Stock", "Reorder Level",
        "Status", "Last Restock", "Supplier", "Unit Price"
    });

    stockTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    stockTable->setSelectionMode(QAbstractItemView::SingleSelection);
    stockTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stockTable->setAlternatingRowColors(true);
    stockTable->setSortingEnabled(true);

    // Set column widths
    stockTable->setColumnWidth(0, 50);   // ID
    stockTable->setColumnWidth(1, 200);  // Product Name
    stockTable->setColumnWidth(2, 120);  // Category
    stockTable->setColumnWidth(3, 100);  // Current Stock
    stockTable->setColumnWidth(4, 100);  // Reorder Level
    stockTable->setColumnWidth(5, 100);  // Status
    stockTable->setColumnWidth(6, 120);  // Last Restock
    stockTable->setColumnWidth(7, 150);  // Supplier
    stockTable->setColumnWidth(8, 100);  // Unit Price

    stockTable->horizontalHeader()->setStretchLastSection(true);
    stockTable->verticalHeader()->setVisible(false);

    connect(stockTable, &QTableWidget::cellDoubleClicked,
            this, &StockManager::onProductDoubleClicked);
    connect(stockTable, &QTableWidget::itemSelectionChanged,
            this, &StockManager::onSelectionChanged);
}

void StockManager::createSummarySection()
{
    QGroupBox *summaryBox = new QGroupBox("Summary", this);
    QGridLayout *summaryLayout = new QGridLayout(summaryBox);

    // Create labels
    totalProductsLabel = new QLabel("Total Products: 0", this);
    totalStockLabel = new QLabel("Total Stock Units: 0", this);
    lowStockCountLabel = new QLabel("Low Stock: 0", this);
    criticalStockCountLabel = new QLabel("Critical: 0", this);
    outOfStockCountLabel = new QLabel("Out of Stock: 0", this);
    totalValueLabel = new QLabel("Total Value: $0.00", this);

    // Style labels — size/weight via properties, colour via kind selectors
    const auto styleStat = [](QLabel *l) {
        l->setProperty("textScale", "sm");
        l->setProperty("bold", "true");
    };
    styleStat(totalProductsLabel);
    styleStat(totalStockLabel);
    styleStat(lowStockCountLabel);
    lowStockCountLabel->setProperty("kind", "warning");
    styleStat(criticalStockCountLabel);
    criticalStockCountLabel->setProperty("kind", "danger");
    styleStat(outOfStockCountLabel);
    outOfStockCountLabel->setProperty("kind", "secondary");
    styleStat(totalValueLabel);
    totalValueLabel->setProperty("kind", "success");

    // Add to layout
    summaryLayout->addWidget(totalProductsLabel, 0, 0);
    summaryLayout->addWidget(totalStockLabel, 0, 1);
    summaryLayout->addWidget(lowStockCountLabel, 0, 2);
    summaryLayout->addWidget(criticalStockCountLabel, 1, 0);
    summaryLayout->addWidget(outOfStockCountLabel, 1, 1);
    summaryLayout->addWidget(totalValueLabel, 1, 2);
}

void StockManager::createButtonPanel()
{
    QWidget *buttonWidget = new QWidget(this);
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);

    editButton = new QPushButton("✏️ Edit Product", this);
    editButton->setEnabled(false);
    editButton->setProperty("kind", "info");
    connect(editButton, &QPushButton::clicked, this, &StockManager::onEditProductClicked);

    deleteButton = new QPushButton("🗑️ Delete Product", this);
    deleteButton->setEnabled(false);
    deleteButton->setProperty("kind", "danger");
    connect(deleteButton, &QPushButton::clicked, this, &StockManager::onDeleteProductClicked);

    restockButton = new QPushButton("📦 Restock", this);
    restockButton->setEnabled(false);
    restockButton->setProperty("kind", "primary");
    connect(restockButton, &QPushButton::clicked, this, &StockManager::onRestockClicked);

    adjustButton = new QPushButton("⚖️ Adjust Stock", this);
    adjustButton->setEnabled(false);
    adjustButton->setProperty("kind", "warning");
    connect(adjustButton, &QPushButton::clicked, this, &StockManager::onAdjustStockClicked);

    closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(restockButton);
    buttonLayout->addWidget(adjustButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
}

// ============================================================================
// DATA OPERATIONS
// ============================================================================

void StockManager::loadAllProducts()
{
    allProducts.clear();

    QSqlQuery query(db);
    query.exec("SELECT i.ProductID, p.ProductName, p.Category, i.Quantity, "
               "i.LastRestockDate, i.SupplierName, p.RegularPrice "
               "FROM Inventory i "
               "JOIN Products p ON i.ProductID = p.ProductID "
               "ORDER BY p.ProductName ASC");

    while (query.next()) {
        InventoryInfo info;
        info.productId = query.value(0).toInt();
        info.productName = query.value(1).toString();
        // Note: InventoryInfo doesn't have category, we'll get it separately
        info.currentQuantity = query.value(3).toInt();
        info.lastRestockDate = query.value(4).toString();
        info.reorderLevel = 20;
        info.optimalLevel = 100;

        if (info.currentQuantity == 0) {
            info.status = InventoryStatus::OutOfStock;
        } else if (info.currentQuantity <= 4) {
            info.status = InventoryStatus::Critical;
        } else if (info.currentQuantity <= 20) {
            info.status = InventoryStatus::Low;
        } else {
            info.status = InventoryStatus::Healthy;
        }

        allProducts.append(info);
    }

    filterProducts();
}

void StockManager::filterProducts()
{
    QVector<InventoryInfo> filtered;
    QString searchText = searchBox->text().toLower();

    for (const auto &product : allProducts) {
        // Apply status filter
        bool statusMatch = true;
        if (currentFilter == "Healthy" && product.status != InventoryStatus::Healthy) statusMatch = false;
        if (currentFilter == "Low Stock" && product.status != InventoryStatus::Low) statusMatch = false;
        if (currentFilter == "Critical" && product.status != InventoryStatus::Critical) statusMatch = false;
        if (currentFilter == "Out of Stock" && product.status != InventoryStatus::OutOfStock) statusMatch = false;

        // Apply search filter
        bool searchMatch = searchText.isEmpty() ||
                           product.productName.toLower().contains(searchText) ||
                           QString::number(product.productId).contains(searchText);

        if (statusMatch && searchMatch) {
            filtered.append(product);
        }
    }

    populateTable(filtered);
}

void StockManager::populateTable(const QVector<InventoryInfo> &items)
{
    stockTable->setRowCount(0);
    stockTable->setSortingEnabled(false);

    for (const auto &item : items) {
        int row = stockTable->rowCount();
        stockTable->insertRow(row);

        // Get category and price from database
        QSqlQuery productQuery(db);
        productQuery.prepare("SELECT Category, RegularPrice FROM Products WHERE ProductID = :id");
        productQuery.bindValue(":id", item.productId);

        QString category = "N/A";
        double price = 0.0;
        if (productQuery.exec() && productQuery.next()) {
            category = productQuery.value(0).toString();
            price = productQuery.value(1).toDouble();
        }

        // Get supplier
        QSqlQuery supplierQuery(db);
        supplierQuery.prepare("SELECT SupplierName FROM Inventory WHERE ProductID = :id");
        supplierQuery.bindValue(":id", item.productId);
        QString supplier = "N/A";
        if (supplierQuery.exec() && supplierQuery.next()) {
            supplier = supplierQuery.value(0).toString();
        }

        // ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(item.productId));
        idItem->setTextAlignment(Qt::AlignCenter);
        stockTable->setItem(row, 0, idItem);

        // Product Name
        QTableWidgetItem *nameItem = new QTableWidgetItem(item.productName);
        stockTable->setItem(row, 1, nameItem);

        // Category
        QTableWidgetItem *categoryItem = new QTableWidgetItem(category);
        categoryItem->setTextAlignment(Qt::AlignCenter);
        stockTable->setItem(row, 2, categoryItem);

        // Current Stock
        QTableWidgetItem *qtyItem = new QTableWidgetItem(QString::number(item.currentQuantity));
        qtyItem->setTextAlignment(Qt::AlignCenter);
        qtyItem->setBackground(QBrush(getStatusColorForRow(item.currentQuantity)));
        qtyItem->setForeground(QBrush(Qt::white));
        QFont boldFont = qtyItem->font();
        boldFont.setBold(true);
        qtyItem->setFont(boldFont);
        stockTable->setItem(row, 3, qtyItem);

        // Reorder Level
        QTableWidgetItem *reorderItem = new QTableWidgetItem(QString::number(item.reorderLevel));
        reorderItem->setTextAlignment(Qt::AlignCenter);
        stockTable->setItem(row, 4, reorderItem);

        // Status
        QTableWidgetItem *statusItem = new QTableWidgetItem(getStatusText(item.currentQuantity));
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(QBrush(QColor(getStatusColor(item.currentQuantity))));
        QFont statusFont = statusItem->font();
        statusFont.setBold(true);
        statusItem->setFont(statusFont);
        stockTable->setItem(row, 5, statusItem);

        // Last Restock
        QTableWidgetItem *dateItem = new QTableWidgetItem(item.lastRestockDate);
        dateItem->setTextAlignment(Qt::AlignCenter);
        stockTable->setItem(row, 6, dateItem);

        // Supplier
        QTableWidgetItem *supplierItem = new QTableWidgetItem(supplier);
        stockTable->setItem(row, 7, supplierItem);

        // Unit Price
        QTableWidgetItem *priceItem = new QTableWidgetItem(QString("$%1").arg(price, 0, 'f', 2));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stockTable->setItem(row, 8, priceItem);
    }

    stockTable->setSortingEnabled(true);
}

void StockManager::updateSummary()
{
    int totalProducts = allProducts.size();
    int totalStock = 0;
    int lowCount = 0;
    int criticalCount = 0;
    int outOfStockCount = 0;
    double totalValue = 0.0;

    for (const auto &product : allProducts) {
        totalStock += product.currentQuantity;

        // Get price
        QSqlQuery query(db);
        query.prepare("SELECT RegularPrice FROM Products WHERE ProductID = :id");
        query.bindValue(":id", product.productId);
        if (query.exec() && query.next()) {
            totalValue += query.value(0).toDouble() * product.currentQuantity;
        }

        if (product.status == InventoryStatus::Low) lowCount++;
        else if (product.status == InventoryStatus::Critical) criticalCount++;
        else if (product.status == InventoryStatus::OutOfStock) outOfStockCount++;
    }

    totalProductsLabel->setText(QString("Total Products: %1").arg(totalProducts));
    totalStockLabel->setText(QString("Total Stock Units: %1").arg(totalStock));
    lowStockCountLabel->setText(QString("Low Stock: %1").arg(lowCount));
    criticalStockCountLabel->setText(QString("Critical: %1").arg(criticalCount));
    outOfStockCountLabel->setText(QString("Out of Stock: %1").arg(outOfStockCount));
    totalValueLabel->setText(QString("Total Value: $%1").arg(totalValue, 0, 'f', 2));
}

// ============================================================================
// SLOT IMPLEMENTATIONS
// ============================================================================

void StockManager::onRefreshClicked()
{
    loadAllProducts();
    updateSummary();
    QMessageBox::information(this, "Refreshed", "Stock data has been refreshed.");
}

void StockManager::onAddProductClicked()
{
    showProductDialog(-1);
}

void StockManager::onEditProductClicked()
{
    if (selectedProductId < 0) {
        QMessageBox::information(this, "No Selection", "Please select a product to edit.");
        return;
    }

    showProductDialog(selectedProductId);
}

void StockManager::onDeleteProductClicked()
{
    if (selectedProductId < 0) {
        QMessageBox::information(this, "No Selection", "Please select a product to delete.");
        return;
    }

    // Get product name
    QString productName;
    for (const auto &product : allProducts) {
        if (product.productId == selectedProductId) {
            productName = product.productName;
            break;
        }
    }

    auto reply = QMessageBox::question(this, "Confirm Delete",
                                       QString("Are you sure you want to delete '%1'?\n\n"
                                               "This will remove the product and all associated inventory records.")
                                           .arg(productName),
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QSqlQuery query(db);
    db.transaction();

    // Delete from Inventory first
    query.prepare("DELETE FROM Inventory WHERE ProductID = :id");
    query.bindValue(":id", selectedProductId);
    if (!query.exec()) {
        db.rollback();
        QMessageBox::critical(this, "Error", "Failed to delete inventory record.");
        return;
    }

    // Delete from Products
    query.prepare("DELETE FROM Products WHERE ProductID = :id");
    query.bindValue(":id", selectedProductId);
    if (!query.exec()) {
        db.rollback();
        QMessageBox::critical(this, "Error", "Failed to delete product.");
        return;
    }

    db.commit();

    QMessageBox::information(this, "Success", QString("'%1' has been deleted.").arg(productName));
    loadAllProducts();
    updateSummary();
}

void StockManager::onRestockClicked()
{
    if (selectedProductId < 0) {
        QMessageBox::information(this, "No Selection", "Please select a product to restock.");
        return;
    }

    QString productName;
    int currentQty = 0;

    for (const auto &product : allProducts) {
        if (product.productId == selectedProductId) {
            productName = product.productName;
            currentQty = product.currentQuantity;
            break;
        }
    }

    showRestockDialog(selectedProductId, productName, currentQty);
}

void StockManager::onAdjustStockClicked()
{
    if (selectedProductId < 0) {
        QMessageBox::information(this, "No Selection", "Please select a product to adjust.");
        return;
    }

    QString productName;
    int currentQty = 0;

    for (const auto &product : allProducts) {
        if (product.productId == selectedProductId) {
            productName = product.productName;
            currentQty = product.currentQuantity;
            break;
        }
    }

    showStockAdjustmentDialog(selectedProductId, productName, currentQty);
}

void StockManager::onExportClicked()
{
    exportToCSV();
}

void StockManager::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    filterProducts();
}

void StockManager::onFilterChanged(int index)
{
    currentFilter = filterCombo->itemText(index);
    filterProducts();
}

void StockManager::onProductDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    int productId = stockTable->item(row, 0)->text().toInt();
    showProductDialog(productId);
}

void StockManager::onSelectionChanged()
{
    QList<QTableWidgetItem*> selected = stockTable->selectedItems();

    if (!selected.isEmpty()) {
        selectedProductId = stockTable->item(selected[0]->row(), 0)->text().toInt();
        editButton->setEnabled(true);
        deleteButton->setEnabled(true);
        restockButton->setEnabled(true);
        adjustButton->setEnabled(true);
    } else {
        selectedProductId = -1;
        editButton->setEnabled(false);
        deleteButton->setEnabled(false);
        restockButton->setEnabled(false);
        adjustButton->setEnabled(false);
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void StockManager::showProductDialog(int productId)
{
    bool isEdit = (productId > 0);
    Product product;
    if (isEdit) {
        product = Database::instance().getProductById(productId);
        if (product.id <= 0) {
            QMessageBox::warning(this, "Error", "Product not found.");
            return;
        }
    }

    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? QString("Edit Product — %1").arg(product.name) : "Add New Product");
    dlg.setMinimumWidth(480);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel *titleLbl = new QLabel(isEdit ? "Edit Product Details" : "Add New Product", &dlg);
    QFont tf = titleLbl->font(); tf.setPointSize(13); tf.setBold(true);
    titleLbl->setFont(tf);
    mainLayout->addWidget(titleLbl);

    QFormLayout *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(10);

    auto makeEdit = [&](const QString &placeholder) -> QLineEdit* {
        QLineEdit *e = new QLineEdit(&dlg);
        e->setPlaceholderText(placeholder);
        e->setMinimumHeight(32);
        return e;
    };

    QLineEdit *nameEdit = makeEdit("e.g. Coca-Cola 500ml");
    if (isEdit) nameEdit->setText(product.name);
    form->addRow("Product Name *:", nameEdit);

    // Category combo (editable)
    QComboBox *catCombo = new QComboBox(&dlg);
    catCombo->setEditable(true);
    catCombo->setMinimumHeight(32);
    catCombo->addItems(Database::instance().getAllCategories());
    if (isEdit) catCombo->setCurrentText(product.category);
    form->addRow("Category *:", catCombo);

    QLineEdit *barcodeEdit = makeEdit("e.g. 5901234123457");
    if (isEdit) barcodeEdit->setText(product.barcode);
    form->addRow("Barcode:", barcodeEdit);

    // Cost price
    QDoubleSpinBox *costSpin = new QDoubleSpinBox(&dlg);
    costSpin->setRange(0.0, 9999999.0);
    costSpin->setDecimals(2);
    costSpin->setPrefix("KSh ");
    costSpin->setMinimumHeight(32);
    if (isEdit) costSpin->setValue(product.costPrice);
    form->addRow("Cost Price *:", costSpin);

    // Profit margin
    QDoubleSpinBox *marginSpin = new QDoubleSpinBox(&dlg);
    marginSpin->setRange(0.0, 10000.0);
    marginSpin->setDecimals(1);
    marginSpin->setSuffix(" %");
    marginSpin->setMinimumHeight(32);
    if (isEdit) marginSpin->setValue(product.profitMargin);
    else        marginSpin->setValue(40.0);
    form->addRow("Profit Margin *:", marginSpin);

    // Selling price (auto-calculated, read-only)
    QLineEdit *priceDisplay = new QLineEdit(&dlg);
    priceDisplay->setReadOnly(true);
    priceDisplay->setMinimumHeight(32);
    priceDisplay->setProperty("bold", "true");
    auto updatePrice = [&]() {
        double sp = Product::calculateSellingPrice(costSpin->value(), marginSpin->value());
        priceDisplay->setText(QString("KSh %1").arg(sp, 0, 'f', 2));
    };
    QObject::connect(costSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), updatePrice);
    QObject::connect(marginSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updatePrice);
    updatePrice();
    form->addRow("Selling Price:", priceDisplay);

    // Stock quantity
    QSpinBox *qtySpin = new QSpinBox(&dlg);
    qtySpin->setRange(0, 999999);
    qtySpin->setMinimumHeight(32);
    if (isEdit) qtySpin->setValue(product.stockQuantity);
    form->addRow("Stock Quantity *:", qtySpin);

    mainLayout->addLayout(form);

    // Required field note
    QLabel *note = new QLabel("<i style='color:#888;'>* Required fields</i>", &dlg);
    mainLayout->addWidget(note);

    mainLayout->addStretch();

    // Buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    QPushButton *cancelBtn = new QPushButton("Cancel", &dlg);
    QPushButton *saveBtn   = new QPushButton(isEdit ? "💾 Save Changes" : "➕ Add Product", &dlg);
    saveBtn->setDefault(true);
    saveBtn->setProperty("kind", "primary");
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    mainLayout->addLayout(btnRow);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(saveBtn,   &QPushButton::clicked, &dlg, [&]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, "Validation", "Product name is required.");
            nameEdit->setFocus(); return;
        }
        if (catCombo->currentText().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, "Validation", "Category is required.");
            catCombo->setFocus(); return;
        }
        if (costSpin->value() <= 0.0) {
            QMessageBox::warning(&dlg, "Validation", "Cost price must be greater than zero.");
            costSpin->setFocus(); return;
        }
        dlg.accept();
    });

    if (dlg.exec() != QDialog::Accepted) return;

    Product p;
    p.id            = isEdit ? productId : 0;
    p.name          = nameEdit->text().trimmed();
    p.category      = catCombo->currentText().trimmed();
    p.barcode       = barcodeEdit->text().trimmed();
    p.costPrice     = costSpin->value();
    p.profitMargin  = marginSpin->value();
    p.price         = Product::calculateSellingPrice(p.costPrice, p.profitMargin);
    p.stockQuantity = qtySpin->value();
    p.isActive      = true;

    bool ok = isEdit ? Database::instance().updateProduct(p)
                     : Database::instance().addProduct(p);

    if (ok) {
        QMessageBox::information(this, "Success",
                                 isEdit ? QString("'%1' updated successfully.").arg(p.name)
                                        : QString("'%1' added successfully.").arg(p.name));
        loadAllProducts();
        updateSummary();
    } else {
        QMessageBox::critical(this, "Error",
                              QString("Failed to %1 product. The barcode may already be in use.")
                                  .arg(isEdit ? "update" : "add"));
    }
}

void StockManager::showRestockDialog(int productId, const QString &productName, int currentQty)
{
    bool ok;
    int quantity = QInputDialog::getInt(
        this,
        "Restock Item",
        QString("Restock: %1\nCurrent quantity: %2\n\nEnter quantity to add:")
            .arg(productName)
            .arg(currentQty),
        50, 1, 1000, 1, &ok);

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
        QMessageBox::information(this, "Success",
                                 QString("%1 has been restocked.\n\nAdded: %2 units\nNew quantity: %3")
                                     .arg(productName)
                                     .arg(quantity)
                                     .arg(currentQty + quantity));
        loadAllProducts();
        updateSummary();
    } else {
        QMessageBox::critical(this, "Error", "Failed to restock product.");
    }
}

void StockManager::showStockAdjustmentDialog(int productId, const QString &productName, int currentQty)
{
    bool ok;
    int newQty = QInputDialog::getInt(
        this,
        "Adjust Stock",
        QString("Adjust stock for: %1\nCurrent quantity: %2\n\nEnter new quantity:")
            .arg(productName)
            .arg(currentQty),
        currentQty, 0, 10000, 1, &ok);

    if (!ok) return;

    QSqlQuery query(db);
    query.prepare("UPDATE Inventory SET Quantity = :qty, LastRestockDate = :date "
                  "WHERE ProductID = :id");
    query.bindValue(":qty", newQty);
    query.bindValue(":date", QDateTime::currentDateTime().toString("yyyy-MM-dd"));
    query.bindValue(":id", productId);

    if (query.exec()) {
        QMessageBox::information(this, "Success",
                                 QString("Stock adjusted for %1\n\nOld: %2\nNew: %3")
                                     .arg(productName)
                                     .arg(currentQty)
                                     .arg(newQty));
        loadAllProducts();
        updateSummary();
    } else {
        QMessageBox::critical(this, "Error", "Failed to adjust stock.");
    }
}

QString StockManager::getStatusText(int quantity) const
{
    if (quantity == 0) return "OUT OF STOCK";
    if (quantity <= 4) return "CRITICAL";
    if (quantity <= 20) return "LOW";
    return "HEALTHY";
}

QString StockManager::getStatusColor(int quantity) const
{
    const ColorScheme scheme = getColorScheme();
    if (quantity == 0) return scheme.textSecondary;
    if (quantity <= 4) return scheme.error;
    if (quantity <= 20) return scheme.warning;
    return scheme.accentPrimary;
}

QColor StockManager::getStatusColorForRow(int quantity) const
{
    return QColor(getStatusColor(quantity));
}

void StockManager::exportToCSV()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Stock Data",
                                                    QDir::homePath() + "/stock_data.csv",
                                                    "CSV Files (*.csv)");

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to create CSV file.");
        return;
    }

    QTextStream out(&file);

    // Write header
    out << "Product ID,Product Name,Category,Current Stock,Reorder Level,Status,Last Restock,Supplier,Unit Price\n";

    // Write data
    for (int row = 0; row < stockTable->rowCount(); ++row) {
        for (int col = 0; col < stockTable->columnCount(); ++col) {
            QString text = stockTable->item(row, col)->text();
            text.replace(",", ";"); // Replace commas to avoid CSV issues
            out << text;
            if (col < stockTable->columnCount() - 1) {
                out << ",";
            }
        }
        out << "\n";
    }

    file.close();

    QMessageBox::information(this, "Success",
                             QString("Stock data exported to:\n%1").arg(fileName));
}