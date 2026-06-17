// =============================================================================
// stocktakedialog.cpp — Implementation of StockTakeDialog (see stocktakedialog.h).
// =============================================================================
#include "stocktakedialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSpinBox>

StockTakeDialog::StockTakeDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    loadProducts();
}

void StockTakeDialog::setupUI()
{
    setWindowTitle("Stock Take — Physical Count");
    setMinimumSize(800, 550);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *title = new QLabel("Stock Take — Physical Count", this);
    title->setProperty("role", "sectionTitle");
    mainLayout->addWidget(title);

    // ── Category filter ──────────────────────────────────────────────────────
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("Filter by Category:", this));
    categoryCombo = new QComboBox(this);
    categoryCombo->addItem("All Categories");
    const QStringList cats = Database::instance().getAllCategories();
    for (const QString &cat : cats)
        categoryCombo->addItem(cat);
    connect(categoryCombo, &QComboBox::currentTextChanged,
            this, &StockTakeDialog::onCategoryChanged);
    filterLayout->addWidget(categoryCombo);
    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // ── Products table ───────────────────────────────────────────────────────
    table = new QTableWidget(this);
    table->setColumnCount(COL_COUNT);
    table->setHorizontalHeaderLabels({
        "ID", "Product", "Category", "System Qty", "Counted Qty", "Variance"
    });
    table->setColumnHidden(COL_ID, true);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(COL_PRODUCT, QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    connect(table, &QTableWidget::cellChanged, this, &StockTakeDialog::onCellChanged);
    mainLayout->addWidget(table);

    // ── Buttons ──────────────────────────────────────────────────────────────
    QHBoxLayout *btnLayout = new QHBoxLayout();
    submitButton = new QPushButton("Submit Count", this);
    submitButton->setProperty("kind", "primary");
    connect(submitButton, &QPushButton::clicked, this, &StockTakeDialog::onSubmitCount);

    cancelButton = new QPushButton("Cancel", this);
    cancelButton->setProperty("kind", "danger");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(submitButton);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelButton);
    mainLayout->addLayout(btnLayout);
}

void StockTakeDialog::loadProducts(const QString &categoryFilter)
{
    m_loading = true;

    const QVector<Product> products = Database::instance().getAllProducts();
    table->setRowCount(0);

    for (const Product &p : products) {
        if (!categoryFilter.isEmpty() && categoryFilter != "All Categories"
                && p.category != categoryFilter)
            continue;

        const int row = table->rowCount();
        table->insertRow(row);

        // ID (hidden)
        auto *idItem = new QTableWidgetItem(QString::number(p.id));
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, COL_ID, idItem);

        // Product name
        auto *nameItem = new QTableWidgetItem(p.name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, COL_PRODUCT, nameItem);

        // Category
        auto *catItem = new QTableWidgetItem(p.category);
        catItem->setFlags(catItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, COL_CATEGORY, catItem);

        // System qty (read-only)
        auto *sysItem = new QTableWidgetItem(QString::number(p.stockQuantity));
        sysItem->setFlags(sysItem->flags() & ~Qt::ItemIsEditable);
        sysItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, COL_SYSTEM_QTY, sysItem);

        // Counted qty (editable — starts equal to system qty)
        auto *countedItem = new QTableWidgetItem(QString::number(p.stockQuantity));
        countedItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, COL_COUNTED_QTY, countedItem);

        // Variance (computed, read-only)
        auto *varItem = new QTableWidgetItem("0");
        varItem->setFlags(varItem->flags() & ~Qt::ItemIsEditable);
        varItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, COL_VARIANCE, varItem);
    }

    table->resizeColumnsToContents();
    m_loading = false;
}

void StockTakeDialog::onCategoryChanged(const QString &category)
{
    loadProducts(category);
}

void StockTakeDialog::onCellChanged(int row, int column)
{
    if (m_loading || column != COL_COUNTED_QTY)
        return;
    updateVariance(row);
}

void StockTakeDialog::updateVariance(int row)
{
    QTableWidgetItem *sysItem     = table->item(row, COL_SYSTEM_QTY);
    QTableWidgetItem *countedItem = table->item(row, COL_COUNTED_QTY);
    QTableWidgetItem *varItem     = table->item(row, COL_VARIANCE);
    if (!sysItem || !countedItem || !varItem)
        return;

    bool ok = false;
    const int counted = countedItem->text().toInt(&ok);
    if (!ok) return;

    const int systemQty = sysItem->text().toInt();
    const int variance  = counted - systemQty;
    varItem->setText(QString::number(variance));

    // Colour-code variance
    if (variance < 0)
        varItem->setForeground(Qt::red);
    else if (variance > 0)
        varItem->setForeground(Qt::darkGreen);
    else
        varItem->setForeground(QColor());   // default / theme colour
}

void StockTakeDialog::onSubmitCount()
{
    const QString cashier = UserManager::instance().getCurrentUser().fullName;
    int adjustments = 0;
    QStringList summary;

    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem *idItem      = table->item(row, COL_ID);
        QTableWidgetItem *nameItem    = table->item(row, COL_PRODUCT);
        QTableWidgetItem *sysItem     = table->item(row, COL_SYSTEM_QTY);
        QTableWidgetItem *countedItem = table->item(row, COL_COUNTED_QTY);

        if (!idItem || !nameItem || !sysItem || !countedItem)
            continue;

        bool ok = false;
        const int counted = countedItem->text().toInt(&ok);
        if (!ok) continue;

        const int productId = idItem->text().toInt();
        const int systemQty = sysItem->text().toInt();
        const QString productName = nameItem->text();

        if (counted == systemQty)
            continue;

        // Apply the new stock level and log the adjustment.
        if (!Database::instance().updateStock(productId, counted)) {
            QMessageBox::critical(this, "Error",
                QString("Failed to update stock for %1: %2")
                    .arg(productName, Database::instance().getLastError()));
            continue;
        }

        Database::instance().logStockAdjustment(
            productId, productName,
            systemQty, counted,
            "Stock Take", cashier);

        ++adjustments;
        summary << QString("%1: %2 → %3 (variance %4%5)")
                        .arg(productName)
                        .arg(systemQty)
                        .arg(counted)
                        .arg(counted > systemQty ? "+" : "")
                        .arg(counted - systemQty);
    }

    if (adjustments == 0) {
        QMessageBox::information(this, "No Changes",
            "All counted quantities match the system. No adjustments needed.");
        return;
    }

    QMessageBox::information(this, "Stock Take Complete",
        QString("%1 product(s) adjusted:\n\n%2")
            .arg(adjustments)
            .arg(summary.join("\n")));
    accept();
}
