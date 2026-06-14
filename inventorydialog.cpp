// =============================================================================
// inventorydialog.cpp — Implementation of InventoryDialog (see
// inventorydialog.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Stock Management tab edits happen in QTableWidget cells; dirty rows are
//    validated (validateProductRow highlights bad cells with tooltips) and
//    committed by explicit Save or the auto-save QTimer; a QUndoStack gives
//    undo/redo.
//  - Subscribes to InventoryManager signals so quantities/alerts update live
//    while the dialog is open.
//  - All persisted changes go through Database / adjustStockWithLog so manual
//    edits keep their audit trail.
// =============================================================================
#include "inventorydialog.h"
#include "database.h"
#include "colorscheme.h"
#include "appstyle.h"
#include "cart.h"          // formatMoney(), currencySymbol()
#include <QVBoxLayout>
// Plain two-decimal money string (no symbol) for editable/auto cells + CSV.
static QString money2(Money m) { return QString::number(m.toMajor(), 'f', 2); }
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QInputDialog>
#include <QShortcut>
#include <QGroupBox>
#include <QKeyEvent>
#include <QCloseEvent>

// ─────────────────────────────────────────────────────────────────────────────
// Anonymous helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

QString getStatusIcon(InventoryStatus status)
{
    switch (status) {
    case InventoryStatus::Healthy:    return "✓";
    case InventoryStatus::Low:        return "⚠";
    case InventoryStatus::Critical:   return "⚠";
    case InventoryStatus::OutOfStock: return "✗";
    default:                          return "?";
    }
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

InventoryDialog::InventoryDialog(InventoryManager *manager, QWidget *parent)
    : QDialog(parent)
    , inventoryManager(manager)
    , selectedProductId(-1)
    , hasUnsavedChanges(false)
    , autoSaveEnabled(false)
    , changesPending(0)
    // ── null-initialise every widget pointer up front ────────────────────────
    , tabWidget(nullptr)
    , inventoryTable(nullptr)
    , searchEdit(nullptr)
    , filterCombo(nullptr)
    , refreshBtn(nullptr)
    , exportBtn(nullptr)
    , printBtn(nullptr)
    , totalItemsLabel(nullptr)
    , lowStockCountLabel(nullptr)
    , criticalStockCountLabel(nullptr)
    , outOfStockCountLabel(nullptr)
    , resultsCountLabel(nullptr)
    , stockManagementTable(nullptr)
    , addNewItemBtn(nullptr)
    , deleteItemBtn(nullptr)
    , saveChangesBtn(nullptr)
    , autoSaveCheckbox(nullptr)
    , unsavedChangesLabel(nullptr)
    , alertsTable(nullptr)
    , alertsCountLabel(nullptr)
    , analyticsTab(nullptr)
    , totalValueLabel(nullptr)
    , avgStockLevelLabel(nullptr)
    , fastMovingLabel(nullptr)
    , slowMovingLabel(nullptr)
    , autoRefreshCheckBox(nullptr)
    , lastUpdatedLabel(nullptr)
{
    setWindowTitle("Inventory Management");
    resize(1200, 800);
    setMinimumSize(1000, 700);

    autoSaveTimer    = new QTimer(this);
    autoRefreshTimer = new QTimer(this);
    undoStack        = new QUndoStack(this);

    // Build the widget tree ONCE; the application-wide stylesheet themes it.
    setupUI();
    setupKeyboardShortcuts();
    loadInventoryData();
    connectSignals();

    // The dialog is styled by the application-wide stylesheet (appstyle.cpp).
    // Per-cell QBrush highlights were derived from the old scheme, so reload
    // the data on theme change to re-derive them from the new one.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](bool) { loadInventoryData(); });
}

InventoryDialog::~InventoryDialog()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// setupUI  — creates every widget; theming comes from the global stylesheet
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    tabWidget = new QTabWidget();

    setupOverviewTab();
    setupStockManagementTab();
    setupAlertsTab();
    setupAnalyticsTab();

    mainLayout->addWidget(tabWidget);

    // ── Bottom action bar ──────────────────────────────────────────────────
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);

    autoRefreshCheckBox = new QCheckBox("Auto-refresh (30s)");
    connect(autoRefreshCheckBox, &QCheckBox::toggled,
            this, &InventoryDialog::onAutoRefreshToggled);
    bottomLayout->addWidget(autoRefreshCheckBox);

    lastUpdatedLabel = new QLabel("Last updated: Never");
    lastUpdatedLabel->setProperty("kind", "secondary");
    bottomLayout->addWidget(lastUpdatedLabel);

    bottomLayout->addStretch();

    QPushButton *closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &InventoryDialog::onCloseClicked);
    bottomLayout->addWidget(closeBtn);

    mainLayout->addLayout(bottomLayout);

    connect(autoRefreshTimer, &QTimer::timeout,
            this, &InventoryDialog::onRefreshClicked);
    connect(autoSaveTimer, &QTimer::timeout,
            this, &InventoryDialog::performAutoSave);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab setup helpers
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::setupOverviewTab()
{
    const ColorScheme scheme = getColorScheme();

    QWidget *overviewTab = new QWidget();
    QVBoxLayout *overviewLayout = new QVBoxLayout(overviewTab);
    overviewLayout->setSpacing(12);
    overviewLayout->setContentsMargins(12, 12, 12, 12);

    QGroupBox *summaryGroup = new QGroupBox("Inventory Summary");
    QHBoxLayout *summaryLayout = new QHBoxLayout(summaryGroup);
    summaryLayout->setSpacing(12);

    QWidget *totalCard = createSummaryCard("Total Items",   &totalItemsLabel,
                                           scheme.textPrimary);
    QWidget *lowCard   = createSummaryCard("Low Stock",     &lowStockCountLabel,
                                           scheme.warning);
    QWidget *critCard  = createSummaryCard("Critical",      &criticalStockCountLabel,
                                           scheme.error);
    QWidget *outCard   = createSummaryCard("Out of Stock",  &outOfStockCountLabel,
                                           scheme.textSecondary);
    summaryLayout->addWidget(totalCard);
    summaryLayout->addWidget(lowCard);
    summaryLayout->addWidget(critCard);
    summaryLayout->addWidget(outCard);
    overviewLayout->addWidget(summaryGroup);

    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("Search by product name, category, or barcode...");
    searchEdit->setClearButtonEnabled(true);
    connect(searchEdit, &QLineEdit::textChanged,
            this, &InventoryDialog::onSearchTextChanged);
    filterLayout->addWidget(searchEdit, 2);

    filterCombo = new QComboBox();
    filterCombo->addItems({"All Items", "Healthy Stock", "Low Stock",
                           "Critical Stock", "Out of Stock"});
    connect(filterCombo, &QComboBox::currentTextChanged,
            this, &InventoryDialog::onFilterChanged);
    filterLayout->addWidget(filterCombo, 1);

    refreshBtn = new QPushButton("Refresh");
    connect(refreshBtn, &QPushButton::clicked, this, &InventoryDialog::onRefreshClicked);
    filterLayout->addWidget(refreshBtn);

    exportBtn = new QPushButton("Export...");
    connect(exportBtn, &QPushButton::clicked, this, &InventoryDialog::onExportClicked);
    filterLayout->addWidget(exportBtn);

    printBtn = new QPushButton("Print...");
    connect(printBtn, &QPushButton::clicked, this, &InventoryDialog::onPrintClicked);
    filterLayout->addWidget(printBtn);

    overviewLayout->addLayout(filterLayout);

    inventoryTable = new QTableWidget();
    inventoryTable->setColumnCount(7);
    inventoryTable->setHorizontalHeaderLabels(
        {"ID", "Product", "Category", "Barcode",
         QString("Price (%1)").arg(currencySymbol()), "Stock", "Status"});
    inventoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    inventoryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    inventoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    inventoryTable->verticalHeader()->setVisible(false);
    inventoryTable->setAlternatingRowColors(true);
    inventoryTable->horizontalHeader()->setStretchLastSection(true);
    inventoryTable->setColumnHidden(0, true);
    inventoryTable->setSortingEnabled(true);
    inventoryTable->setShowGrid(true);
    inventoryTable->setColumnWidth(1, 250);
    inventoryTable->setColumnWidth(2, 120);
    inventoryTable->setColumnWidth(3, 130);
    inventoryTable->setColumnWidth(4, 120);
    inventoryTable->setColumnWidth(5, 90);

    connect(inventoryTable, &QTableWidget::currentCellChanged,
            this, [this](int row, int, int, int) {
                if (row >= 0) onProductSelected(row);
            });
    connect(inventoryTable, &QTableWidget::cellDoubleClicked,
            this, &InventoryDialog::onCellDoubleClicked);

    overviewLayout->addWidget(inventoryTable);

    resultsCountLabel = new QLabel();
    resultsCountLabel->setProperty("kind", "secondary");
    overviewLayout->addWidget(resultsCountLabel);

    tabWidget->addTab(overviewTab, "Inventory Overview");
}

void InventoryDialog::setupStockManagementTab()
{
    QWidget *stockTab = new QWidget();
    QVBoxLayout *stockLayout = new QVBoxLayout(stockTab);
    stockLayout->setSpacing(12);
    stockLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *instructionsLabel = new QLabel(
        "<b>Quick Guide:</b><br>"
        "• <b>Double-click</b> any editable cell to modify<br>"
        "• Enter <b>Cost Price</b> and <b>Profit %</b> → <b>Selling Price</b> calculates automatically<br>"
        "• <b>Yellow highlighted</b> rows are new items awaiting save<br>"
        "• Click <b>Save All Changes</b> to persist modifications to database<br>"
        "• <b>Ctrl+S</b>: Save | <b>Ctrl+N</b>: New Item | <b>Ctrl+Z</b>: Undo | <b>Ctrl+Y</b>: Redo");
    instructionsLabel->setWordWrap(true);
    instructionsLabel->setProperty("role", "banner");
    instructionsLabel->setProperty("kind", "warning");
    stockLayout->addWidget(instructionsLabel);

    stockManagementTable = new QTableWidget();
    stockManagementTable->setColumnCount(10);
    stockManagementTable->setHorizontalHeaderLabels({
                                                     "ID", "Product Name", "Category", "Barcode",
                                                     "Cost Price", "Profit %", "Selling Price", "Stock Qty",
                                                     "Reorder Level", "Status"});
    stockManagementTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    stockManagementTable->setSelectionMode(QAbstractItemView::SingleSelection);
    stockManagementTable->setAlternatingRowColors(true);
    stockManagementTable->verticalHeader()->setVisible(false);
    stockManagementTable->horizontalHeader()->setStretchLastSection(true);
    stockManagementTable->setColumnHidden(0, true);
    stockManagementTable->setSortingEnabled(true);
    stockManagementTable->setColumnWidth(1, 240);
    stockManagementTable->setColumnWidth(2, 120);
    stockManagementTable->setColumnWidth(3, 120);
    stockManagementTable->setColumnWidth(4, 110);
    stockManagementTable->setColumnWidth(5, 90);
    stockManagementTable->setColumnWidth(6, 120);
    stockManagementTable->setColumnWidth(7, 90);
    stockManagementTable->setEditTriggers(
        QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    connect(stockManagementTable, &QTableWidget::cellChanged,
            this, &InventoryDialog::onStockTableCellChanged);

    stockLayout->addWidget(stockManagementTable);

    QHBoxLayout *stockButtonLayout = new QHBoxLayout();
    stockButtonLayout->setSpacing(8);

    addNewItemBtn = new QPushButton("Add New Product");
    connect(addNewItemBtn, &QPushButton::clicked,
            this, &InventoryDialog::onAddNewItemClicked);
    stockButtonLayout->addWidget(addNewItemBtn);

    deleteItemBtn = new QPushButton("Delete Selected");
    connect(deleteItemBtn, &QPushButton::clicked,
            this, &InventoryDialog::onDeleteItemClicked);
    stockButtonLayout->addWidget(deleteItemBtn);

    stockButtonLayout->addStretch();

    autoSaveCheckbox = new QCheckBox("Auto-save");
    connect(autoSaveCheckbox, &QCheckBox::toggled,
            this, &InventoryDialog::onAutoSaveToggled);
    stockButtonLayout->addWidget(autoSaveCheckbox);

    unsavedChangesLabel = new QLabel();
    unsavedChangesLabel->setProperty("role", "banner");
    unsavedChangesLabel->setProperty("kind", "danger");
    unsavedChangesLabel->setVisible(false);
    stockButtonLayout->addWidget(unsavedChangesLabel);

    saveChangesBtn = new QPushButton("Save All Changes");
    saveChangesBtn->setEnabled(false);
    connect(saveChangesBtn, &QPushButton::clicked,
            this, &InventoryDialog::onSaveChangesClicked);
    stockButtonLayout->addWidget(saveChangesBtn);

    stockLayout->addLayout(stockButtonLayout);

    tabWidget->addTab(stockTab, "Stock Management");
}

void InventoryDialog::setupAlertsTab()
{
    QWidget *alertsTab = new QWidget();
    QVBoxLayout *alertsLayout = new QVBoxLayout(alertsTab);
    alertsLayout->setSpacing(12);
    alertsLayout->setContentsMargins(12, 12, 12, 12);

    alertsCountLabel = new QLabel();
    alertsCountLabel->setProperty("role", "banner");
    alertsCountLabel->setProperty("kind", "danger");
    alertsCountLabel->setProperty("textScale", "md");
    alertsLayout->addWidget(alertsCountLabel);

    QHBoxLayout *alertActionsLayout = new QHBoxLayout();
    QPushButton *restockAllBtn = new QPushButton("Bulk Restock...");
    connect(restockAllBtn, &QPushButton::clicked,
            this, &InventoryDialog::onBulkRestockClicked);
    alertActionsLayout->addWidget(restockAllBtn);

    QPushButton *exportAlertsBtn = new QPushButton("Export Alerts...");
    connect(exportAlertsBtn, &QPushButton::clicked,
            this, &InventoryDialog::onExportAlertsClicked);
    alertActionsLayout->addWidget(exportAlertsBtn);
    alertActionsLayout->addStretch();
    alertsLayout->addLayout(alertActionsLayout);

    alertsTable = new QTableWidget();
    alertsTable->setColumnCount(6);
    alertsTable->setHorizontalHeaderLabels({
                                            "Product", "Category", "Current Stock",
                                            "Reorder Level", "Status", "Action Required"});
    alertsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    alertsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    alertsTable->verticalHeader()->setVisible(false);
    alertsTable->setAlternatingRowColors(true);
    alertsTable->horizontalHeader()->setStretchLastSection(true);
    alertsTable->setSortingEnabled(true);
    alertsTable->setColumnWidth(0, 220);
    alertsTable->setColumnWidth(1, 120);
    alertsTable->setColumnWidth(2, 110);
    alertsTable->setColumnWidth(3, 110);
    alertsTable->setColumnWidth(4, 120);
    alertsLayout->addWidget(alertsTable);

    tabWidget->addTab(alertsTab, "Alerts");
}

void InventoryDialog::setupAnalyticsTab()
{
    analyticsTab = new QWidget();
    QVBoxLayout *analyticsLayout = new QVBoxLayout(analyticsTab);
    analyticsLayout->setSpacing(12);
    analyticsLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *headerLabel = new QLabel("Inventory Analytics");
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(12);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    analyticsLayout->addWidget(headerLabel);

    QHBoxLayout *analyticsCardsLayout = new QHBoxLayout();
    analyticsCardsLayout->setSpacing(12);

    QGroupBox *totalValueCard = new QGroupBox("Total Inventory Value");
    QVBoxLayout *totalValueLayout = new QVBoxLayout(totalValueCard);
    totalValueLabel = new QLabel(formatMoney(Money()));
    totalValueLabel->setProperty("role", "statValue");
    totalValueLabel->setProperty("kind", "success");
    totalValueLabel->setAlignment(Qt::AlignCenter);
    totalValueLayout->addWidget(totalValueLabel);
    analyticsCardsLayout->addWidget(totalValueCard);

    QGroupBox *avgStockCard = new QGroupBox("Average Stock Level");
    QVBoxLayout *avgStockLayout = new QVBoxLayout(avgStockCard);
    avgStockLevelLabel = new QLabel("0");
    avgStockLevelLabel->setProperty("role", "statValue");
    avgStockLevelLabel->setProperty("kind", "info");
    avgStockLevelLabel->setAlignment(Qt::AlignCenter);
    avgStockLayout->addWidget(avgStockLevelLabel);
    analyticsCardsLayout->addWidget(avgStockCard);

    analyticsLayout->addLayout(analyticsCardsLayout);

    QHBoxLayout *moversLayout = new QHBoxLayout();
    moversLayout->setSpacing(12);

    QGroupBox *fastMovingGroup = new QGroupBox("Fast Moving Items");
    QVBoxLayout *fastMovingLayout = new QVBoxLayout(fastMovingGroup);
    fastMovingLabel = new QLabel("Loading...");
    fastMovingLabel->setWordWrap(true);
    fastMovingLayout->addWidget(fastMovingLabel);
    moversLayout->addWidget(fastMovingGroup);

    QGroupBox *slowMovingGroup = new QGroupBox("Slow Moving Items");
    QVBoxLayout *slowMovingLayout = new QVBoxLayout(slowMovingGroup);
    slowMovingLabel = new QLabel("Loading...");
    slowMovingLabel->setWordWrap(true);
    slowMovingLayout->addWidget(slowMovingLabel);
    moversLayout->addWidget(slowMovingGroup);

    analyticsLayout->addLayout(moversLayout);
    analyticsLayout->addStretch();

    tabWidget->addTab(analyticsTab, "Analytics");
}

// ─────────────────────────────────────────────────────────────────────────────
// Keyboard shortcuts
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::setupKeyboardShortcuts()
{
    QShortcut *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, [this]() {
        if (tabWidget->currentIndex() == 1 && saveChangesBtn->isEnabled())
            onSaveChangesClicked();
    });

    QShortcut *searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, [this]() {
        searchEdit->setFocus();
        searchEdit->selectAll();
    });

    QShortcut *refreshShortcut = new QShortcut(QKeySequence::Refresh, this);
    connect(refreshShortcut, &QShortcut::activated,
            this, &InventoryDialog::onRefreshClicked);

    QShortcut *newShortcut = new QShortcut(QKeySequence::New, this);
    connect(newShortcut, &QShortcut::activated, [this]() {
        if (tabWidget->currentIndex() == 1)
            onAddNewItemClicked();
    });

    QShortcut *printShortcut = new QShortcut(QKeySequence::Print, this);
    connect(printShortcut, &QShortcut::activated,
            this, &InventoryDialog::onPrintClicked);
}

// ─────────────────────────────────────────────────────────────────────────────
// Summary card helper
// ─────────────────────────────────────────────────────────────────────────────

QWidget* InventoryDialog::createSummaryCard(const QString &title,
                                            QLabel **valueLabel,
                                            const QString &color)
{
    const ColorScheme scheme = getColorScheme();

    QWidget *card = new QWidget();
    card->setStyleSheet(QString("background: %1; border: 1px solid %2; "
                                "border-radius: 6px;")
                            .arg(scheme.bgSecondary, scheme.borderColor));

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(4);
    cardLayout->setContentsMargins(8, 8, 8, 8);

    *valueLabel = new QLabel("0");
    (*valueLabel)->setObjectName("summaryValue");   // font rules in appstyle.cpp
    (*valueLabel)->setStyleSheet(QString("color: %1; border: none;").arg(color));
    (*valueLabel)->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(*valueLabel);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName("summaryDescription");
    titleLabel->setStyleSheet("border: none;");
    titleLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(titleLabel);

    card->setMinimumHeight(100);
    return card;
}

// ─────────────────────────────────────────────────────────────────────────────
// connectSignals
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::connectSignals()
{
    connect(inventoryManager, &InventoryManager::inventoryUpdated,
            this, &InventoryDialog::onInventoryChanged);
    connect(inventoryManager, &InventoryManager::inventoryRestocked,
            this, &InventoryDialog::onInventoryChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// Data loading
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::loadInventoryData()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    currentInventory.clear();
    previousInventory.clear();

    QVector<Product> products = Database::instance().getAllProducts();
    for (const Product &p : products) {
        InventoryInfo info = inventoryManager->getInventoryInfo(p.id);
        currentInventory.append(info);
        previousInventory[info.productId] = info;
    }

    updateProductsList();
    updateStockManagementTable();
    updateAnalytics();
    updateLastUpdatedTime();

    QApplication::restoreOverrideCursor();
}

// ─────────────────────────────────────────────────────────────────────────────
// updateProductsList
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::updateProductsList()
{
    const ColorScheme scheme = getColorScheme();

    inventoryTable->setSortingEnabled(false);
    alertsTable->setSortingEnabled(false);
    inventoryTable->setRowCount(0);

    QString searchText = searchEdit->text().toLower();
    QString filter     = filterCombo->currentText();

    int totalItems = 0, lowStock = 0, criticalStock = 0, outOfStock = 0;
    int displayedRows = 0;

    for (const InventoryInfo &info : currentInventory) {
        Product product = Database::instance().getProductById(info.productId);

        if (!searchText.isEmpty()) {
            bool match = info.productName.toLower().contains(searchText) ||
                         info.category.toLower().contains(searchText)    ||
                         product.barcode.toLower().contains(searchText);
            if (!match) continue;
        }

        bool include = false;
        if      (filter == "All Items")                                           include = true;
        else if (filter == "Healthy Stock"  && info.status == InventoryStatus::Healthy)    include = true;
        else if (filter == "Low Stock"      && info.status == InventoryStatus::Low)        include = true;
        else if (filter == "Critical Stock" && info.status == InventoryStatus::Critical)   include = true;
        else if (filter == "Out of Stock"   && info.status == InventoryStatus::OutOfStock) include = true;
        if (!include) continue;

        totalItems++;
        if (info.status == InventoryStatus::Low)        lowStock++;
        if (info.status == InventoryStatus::Critical)   criticalStock++;
        if (info.status == InventoryStatus::OutOfStock) outOfStock++;

        int row = inventoryTable->rowCount();
        inventoryTable->insertRow(row);
        displayedRows++;

        inventoryTable->setItem(row, 0,
                                new QTableWidgetItem(QString::number(info.productId)));

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.productName);
        nameItem->setFont(QFont(nameItem->font().family(), -1, QFont::Bold));
        inventoryTable->setItem(row, 1, nameItem);

        inventoryTable->setItem(row, 2, new QTableWidgetItem(info.category));
        inventoryTable->setItem(row, 3, new QTableWidgetItem(product.barcode));

        QTableWidgetItem *priceItem = new QTableWidgetItem(
            formatCurrency(product.price));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        priceItem->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(product.price.cents()));
        inventoryTable->setItem(row, 4, priceItem);

        QTableWidgetItem *stockItem = new QTableWidgetItem(
            QString::number(info.currentQuantity));
        stockItem->setTextAlignment(Qt::AlignCenter);
        stockItem->setData(Qt::UserRole, info.currentQuantity);
        if (info.status == InventoryStatus::Critical ||
            info.status == InventoryStatus::OutOfStock)
            stockItem->setBackground(QBrush(QColor(scheme.errorBg)));
        else if (info.status == InventoryStatus::Low)
            stockItem->setBackground(QBrush(QColor(scheme.warningBg)));
        inventoryTable->setItem(row, 5, stockItem);

        QString statusText = getStatusIcon(info.status) + " " +
                             inventoryManager->getStatusText(info.status);
        QTableWidgetItem *statusItem = new QTableWidgetItem(statusText);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(
            QBrush(QColor(inventoryManager->getStatusColor(info.status))));
        statusItem->setFont(QFont(statusItem->font().family(), -1, QFont::Bold));
        inventoryTable->setItem(row, 6, statusItem);
    }

    inventoryTable->resizeColumnsToContents();

    updateSummaryCard(totalItemsLabel,        totalItems);
    updateSummaryCard(lowStockCountLabel,      lowStock);
    updateSummaryCard(criticalStockCountLabel, criticalStock);
    updateSummaryCard(outOfStockCountLabel,    outOfStock);

    resultsCountLabel->setText(
        QString("Showing %1 of %2 products")
            .arg(displayedRows).arg(currentInventory.size()));

    updateAlertsTable();

    inventoryTable->setSortingEnabled(true);
    alertsTable->setSortingEnabled(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// updateAlertsTable
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::updateAlertsTable()
{
    const ColorScheme scheme = getColorScheme();
    alertsTable->setRowCount(0);

    QVector<InventoryInfo> alerts;
    alerts.append(inventoryManager->getCriticalStockItems());
    alerts.append(inventoryManager->getLowStockItems());

    for (const InventoryInfo &info : alerts) {
        int row = alertsTable->rowCount();
        alertsTable->insertRow(row);

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.productName);
        nameItem->setFont(QFont(nameItem->font().family(), -1, QFont::Bold));
        alertsTable->setItem(row, 0, nameItem);

        alertsTable->setItem(row, 1, new QTableWidgetItem(info.category));

        QTableWidgetItem *stockItem = new QTableWidgetItem(
            QString::number(info.currentQuantity));
        stockItem->setTextAlignment(Qt::AlignCenter);
        stockItem->setData(Qt::UserRole, info.currentQuantity);
        if (info.status == InventoryStatus::Critical ||
            info.status == InventoryStatus::OutOfStock) {
            stockItem->setBackground(QBrush(QColor(scheme.errorBg)));
            stockItem->setForeground(QBrush(QColor(scheme.error)));
            stockItem->setFont(QFont(stockItem->font().family(), -1, QFont::Bold));
        }
        alertsTable->setItem(row, 2, stockItem);

        QTableWidgetItem *reorderItem = new QTableWidgetItem(
            QString::number(info.reorderLevel));
        reorderItem->setTextAlignment(Qt::AlignCenter);
        alertsTable->setItem(row, 3, reorderItem);

        QString statusText = getStatusIcon(info.status) + " " +
                             inventoryManager->getStatusText(info.status);
        QTableWidgetItem *statusItem = new QTableWidgetItem(statusText);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(
            QBrush(QColor(inventoryManager->getStatusColor(info.status))));
        statusItem->setFont(QFont(statusItem->font().family(), -1, QFont::Bold));
        alertsTable->setItem(row, 4, statusItem);

        QString action;
        if      (info.status == InventoryStatus::OutOfStock) action = "CRITICAL: Out of stock!";
        else if (info.status == InventoryStatus::Critical)   action = "URGENT: Restock immediately";
        else                                                  action = "Schedule restock soon";

        QTableWidgetItem *actionItem = new QTableWidgetItem(action);
        actionItem->setFont(QFont(actionItem->font().family(), -1, QFont::Bold));
        alertsTable->setItem(row, 5, actionItem);
    }

    alertsTable->resizeColumnsToContents();

    if (alerts.isEmpty()) {
        alertsCountLabel->setText("All inventory levels are healthy!");
        setStyleProperty(alertsCountLabel, "kind", "success");
    } else {
        int critical = 0;
        for (const auto &info : alerts)
            if (info.status == InventoryStatus::Critical ||
                info.status == InventoryStatus::OutOfStock) critical++;

        QString message = QString("%1 product%2 need attention")
                              .arg(alerts.size()).arg(alerts.size() > 1 ? "s" : "");
        if (critical > 0) message += QString(" (%1 CRITICAL)").arg(critical);

        alertsCountLabel->setText(message);
        setStyleProperty(alertsCountLabel, "kind", "danger");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// updateAnalytics
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::updateAnalytics()
{
    Money  totalValue;
    int    totalStock   = 0;
    int    productCount = currentInventory.size();

    for (const InventoryInfo &info : std::as_const(currentInventory)) {
        Product product = Database::instance().getProductById(info.productId);
        totalValue += product.price * info.currentQuantity;
        totalStock += info.currentQuantity;
    }

    totalValueLabel->setText(formatCurrency(totalValue));
    avgStockLevelLabel->setText(
        QString::number(productCount > 0 ? totalStock / productCount : 0));

    QStringList fastMoving, slowMoving;
    for (int i = 0; i < qMin(5, currentInventory.size()); ++i) {
        if (currentInventory[i].status == InventoryStatus::Low ||
            currentInventory[i].status == InventoryStatus::Critical)
            fastMoving.append("• " + currentInventory[i].productName);
    }
    for (int i = currentInventory.size() - 1;
         i >= qMax(0, currentInventory.size() - 5); --i) {
        if (currentInventory[i].status == InventoryStatus::Healthy &&
            currentInventory[i].currentQuantity > 80)
            slowMoving.append("• " + currentInventory[i].productName);
    }

    fastMovingLabel->setText(fastMoving.isEmpty() ? "No data available"
                                                  : fastMoving.join("\n"));
    slowMovingLabel->setText(slowMoving.isEmpty() ? "No data available"
                                                  : slowMoving.join("\n"));
}

void InventoryDialog::updateSummaryCard(QLabel *label, int value)
{
    label->setText(QString::number(value));
}

void InventoryDialog::updateLastUpdatedTime()
{
    lastUpdatedLabel->setText(
        QString("Last updated: %1")
            .arg(QDateTime::currentDateTime().toString("h:mm:ss AP")));
}

// ─────────────────────────────────────────────────────────────────────────────
// updateStockManagementTable
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::updateStockManagementTable()
{
    const ColorScheme scheme = getColorScheme();

    disconnect(stockManagementTable, &QTableWidget::cellChanged,
               this, &InventoryDialog::onStockTableCellChanged);

    stockManagementTable->setRowCount(0);

    for (const InventoryInfo &info : currentInventory) {
        Product product = Database::instance().getProductById(info.productId);

        int row = stockManagementTable->rowCount();
        stockManagementTable->insertRow(row);

        QTableWidgetItem *idItem = new QTableWidgetItem(
            QString::number(info.productId));
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        stockManagementTable->setItem(row, 0, idItem);

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.productName);
        nameItem->setFont(QFont(nameItem->font().family(), -1, QFont::Bold));
        stockManagementTable->setItem(row, 1, nameItem);

        stockManagementTable->setItem(row, 2, new QTableWidgetItem(info.category));
        stockManagementTable->setItem(row, 3, new QTableWidgetItem(product.barcode));

        QTableWidgetItem *costItem = new QTableWidgetItem(
            money2(product.costPrice));
        costItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stockManagementTable->setItem(row, 4, costItem);

        QTableWidgetItem *marginItem = new QTableWidgetItem(
            QString::number(product.profitMargin, 'f', 1));
        marginItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stockManagementTable->setItem(row, 5, marginItem);

        QTableWidgetItem *priceItem = new QTableWidgetItem(
            money2(product.price));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        priceItem->setFlags(priceItem->flags() & ~Qt::ItemIsEditable);
        priceItem->setBackground(QBrush(QColor(scheme.disabledBg)));
        priceItem->setToolTip("Auto-calculated from Cost Price + Profit %");
        stockManagementTable->setItem(row, 6, priceItem);

        QTableWidgetItem *stockItem = new QTableWidgetItem(
            QString::number(info.currentQuantity));
        stockItem->setTextAlignment(Qt::AlignCenter);
        stockManagementTable->setItem(row, 7, stockItem);

        QTableWidgetItem *reorderItem = new QTableWidgetItem(
            QString::number(info.reorderLevel));
        reorderItem->setTextAlignment(Qt::AlignCenter);
        reorderItem->setToolTip("Stock at or below this triggers a Low alert "
                                "(0 = no alerts).");
        stockManagementTable->setItem(row, 8, reorderItem);

        QString statusText = getStatusIcon(info.status) + " " +
                             inventoryManager->getStatusText(info.status);
        QTableWidgetItem *statusItem = new QTableWidgetItem(statusText);
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(
            QBrush(QColor(inventoryManager->getStatusColor(info.status))));
        statusItem->setFont(QFont(statusItem->font().family(), -1, QFont::Bold));
        stockManagementTable->setItem(row, 9, statusItem);
    }

    stockManagementTable->resizeColumnsToContents();

    connect(stockManagementTable, &QTableWidget::cellChanged,
            this, &InventoryDialog::onStockTableCellChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot implementations
// ─────────────────────────────────────────────────────────────────────────────

void InventoryDialog::onRefreshClicked()
{
    loadInventoryData();
    showNotification("Inventory refreshed successfully", NotificationType::Success);
}

void InventoryDialog::onStockTableCellChanged(int row, int column)
{
    const ColorScheme scheme = getColorScheme();

    hasUnsavedChanges = true;
    changesPending++;
    saveChangesBtn->setEnabled(true);
    unsavedChangesLabel->setText(
        QString("%1 unsaved change%2")
            .arg(changesPending).arg(changesPending > 1 ? "s" : ""));
    unsavedChangesLabel->setVisible(true);

    if (column == 4 || column == 5) {
        QTableWidgetItem *costItem   = stockManagementTable->item(row, 4);
        QTableWidgetItem *marginItem = stockManagementTable->item(row, 5);
        QTableWidgetItem *priceItem  = stockManagementTable->item(row, 6);

        if (costItem && marginItem && priceItem) {
            bool costOk, marginOk;
            double cost   = costItem->text().toDouble(&costOk);
            double margin = marginItem->text().toDouble(&marginOk);

            if (costOk && marginOk && cost >= 0 && margin >= 0) {
                disconnect(stockManagementTable, &QTableWidget::cellChanged,
                           this, &InventoryDialog::onStockTableCellChanged);

                priceItem->setText(
                    money2(Product::calculateSellingPrice(Money::fromMajor(cost), margin)));

                priceItem->setBackground(QBrush(QColor(scheme.successBg)));
                const QString readOnlyBg = scheme.disabledBg;
                QTimer::singleShot(500, [priceItem, readOnlyBg]() {
                    priceItem->setBackground(QBrush(QColor(readOnlyBg)));
                });

                connect(stockManagementTable, &QTableWidget::cellChanged,
                        this, &InventoryDialog::onStockTableCellChanged);
            } else {
                if (cost   < 0) costItem->setBackground(QBrush(QColor(scheme.errorBg)));
                if (margin < 0) marginItem->setBackground(QBrush(QColor(scheme.errorBg)));
            }
        }
    }

    if (column == 7 || column == 8) {
        bool qtyOk, reorderOk;
        int qty     = stockManagementTable->item(row, 7)->text().toInt(&qtyOk);
        int reorder = stockManagementTable->item(row, 8)->text().toInt(&reorderOk);
        QTableWidgetItem *edited = stockManagementTable->item(row, column);
        const bool valid = (column == 7) ? (qtyOk && qty >= 0)
                                         : (reorderOk && reorder >= 0);
        if (valid) {
            if (qtyOk && reorderOk) {
                InventoryStatus status =
                    InventoryManager::calculateStatus(qty, reorder);
                if (auto *si = stockManagementTable->item(row, 9)) {
                    si->setText(getStatusIcon(status) + " " +
                                inventoryManager->getStatusText(status));
                    si->setForeground(
                        QBrush(QColor(inventoryManager->getStatusColor(status))));
                }
            }
            edited->setBackground(QBrush(QColor(scheme.inputBg)));
        } else {
            edited->setBackground(QBrush(QColor(scheme.errorBg)));
        }
    }
}

int InventoryDialog::countChangedRows() { return changesPending; }

void InventoryDialog::onAddNewItemClicked()
{
    const ColorScheme scheme = getColorScheme();

    disconnect(stockManagementTable, &QTableWidget::cellChanged,
               this, &InventoryDialog::onStockTableCellChanged);

    int row = stockManagementTable->rowCount();
    stockManagementTable->insertRow(row);

    QColor hlColor(scheme.warningBg);
    auto hl = [&](const QString &text) -> QTableWidgetItem* {
        auto *item = new QTableWidgetItem(text);
        item->setBackground(QBrush(hlColor));
        return item;
    };

    QTableWidgetItem *idItem = new QTableWidgetItem("-1");
    idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    stockManagementTable->setItem(row, 0, idItem);

    QTableWidgetItem *nameItem = hl("New Product");
    nameItem->setFont(QFont(nameItem->font().family(), -1, QFont::Bold));
    stockManagementTable->setItem(row, 1, nameItem);
    stockManagementTable->setItem(row, 2, hl("Uncategorized"));
    stockManagementTable->setItem(row, 3, hl(generateTempBarcode()));

    QTableWidgetItem *costItem = hl("0.00");
    costItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    stockManagementTable->setItem(row, 4, costItem);

    QTableWidgetItem *marginItem = hl("40.0");
    marginItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    stockManagementTable->setItem(row, 5, marginItem);

    QTableWidgetItem *priceItem = new QTableWidgetItem("0.00");
    priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    priceItem->setFlags(priceItem->flags() & ~Qt::ItemIsEditable);
    priceItem->setBackground(QBrush(QColor(scheme.disabledBg)));
    stockManagementTable->setItem(row, 6, priceItem);

    QTableWidgetItem *stockItem = hl("0");
    stockItem->setTextAlignment(Qt::AlignCenter);
    stockManagementTable->setItem(row, 7, stockItem);

    QTableWidgetItem *reorderItem = hl("20");
    reorderItem->setTextAlignment(Qt::AlignCenter);
    reorderItem->setToolTip("Stock at or below this triggers a Low alert "
                            "(0 = no alerts).");
    stockManagementTable->setItem(row, 8, reorderItem);

    QTableWidgetItem *statusItem = new QTableWidgetItem("✗ Out of Stock");
    statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
    statusItem->setTextAlignment(Qt::AlignCenter);
    statusItem->setForeground(QBrush(QColor(scheme.textSecondary)));
    statusItem->setFont(QFont(statusItem->font().family(), -1, QFont::Bold));
    stockManagementTable->setItem(row, 9, statusItem);

    connect(stockManagementTable, &QTableWidget::cellChanged,
            this, &InventoryDialog::onStockTableCellChanged);

    hasUnsavedChanges = true;
    changesPending++;
    saveChangesBtn->setEnabled(true);
    unsavedChangesLabel->setText("New item added (unsaved)");
    unsavedChangesLabel->setVisible(true);

    stockManagementTable->scrollToItem(stockManagementTable->item(row, 1));
    stockManagementTable->setCurrentCell(row, 1);
    stockManagementTable->editItem(stockManagementTable->item(row, 1));

    showNotification("New product row added. Fill in details and click Save.",
                     NotificationType::Info);
}

QString InventoryDialog::generateTempBarcode()
{
    return QString("TEMP%1").arg(QDateTime::currentMSecsSinceEpoch() % 1000000);
}

void InventoryDialog::onDeleteItemClicked()
{
    int currentRow = stockManagementTable->currentRow();
    if (currentRow < 0) {
        showNotification("Please select a product to delete", NotificationType::Warning);
        return;
    }

    QString productName = stockManagementTable->item(currentRow, 1)->text();
    int productId       = stockManagementTable->item(currentRow, 0)->text().toInt();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Confirm Deletion");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(QString("Delete '%1'?").arg(productName));
    msgBox.setInformativeText(
        "This action cannot be undone. Transaction history will be preserved.");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    if (msgBox.exec() == QMessageBox::Yes) {
        if (productId > 0) {
            if (Database::instance().deleteProduct(productId)) {
                stockManagementTable->removeRow(currentRow);
                loadInventoryData();
                showNotification(
                    QString("'%1' deleted successfully").arg(productName),
                    NotificationType::Success);
            } else {
                showNotification("Failed to delete product from database",
                                 NotificationType::Error);
            }
        } else {
            stockManagementTable->removeRow(currentRow);
            showNotification("Unsaved product removed", NotificationType::Info);
        }
    }
}

void InventoryDialog::onSaveChangesClicked()
{
    int savedCount = 0, errorCount = 0;
    QStringList errorMessages;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    for (int row = 0; row < stockManagementTable->rowCount(); ++row) {
        int     productId    = stockManagementTable->item(row, 0)->text().toInt();
        QString name         = stockManagementTable->item(row, 1)->text().trimmed();
        QString category     = stockManagementTable->item(row, 2)->text().trimmed();
        QString barcode      = stockManagementTable->item(row, 3)->text().trimmed();

        bool   costOk, marginOk, qtyOk, reorderOk;
        double costPrice    = stockManagementTable->item(row, 4)->text().toDouble(&costOk);
        double profitMargin = stockManagementTable->item(row, 5)->text().toDouble(&marginOk);
        int    qty          = stockManagementTable->item(row, 7)->text().toInt(&qtyOk);
        int    reorderLevel = stockManagementTable->item(row, 8)->text().toInt(&reorderOk);

        auto fail = [&](const QString &msg) {
            errorMessages.append(msg);
            highlightRowError(row);
            errorCount++;
        };

        if (name.isEmpty())                { fail(QString("Row %1: Product name required").arg(row+1)); continue; }
        if (!costOk   || costPrice    < 0) { fail(QString("Row %1: Invalid cost price").arg(row+1));   continue; }
        if (!marginOk || profitMargin < 0) { fail(QString("Row %1: Invalid profit margin").arg(row+1));continue; }
        if (!qtyOk    || qty          < 0) { fail(QString("Row %1: Invalid quantity").arg(row+1));     continue; }
        if (!reorderOk || reorderLevel < 0){ fail(QString("Row %1: Invalid reorder level").arg(row+1));continue; }

        Product product;
        product.id            = productId;
        product.name          = name;
        product.category      = category;
        product.barcode       = barcode.isEmpty() ? generateTempBarcode() : barcode;
        product.costPrice     = Money::fromMajor(costPrice);
        product.profitMargin  = profitMargin;
        product.price         = Product::calculateSellingPrice(product.costPrice, profitMargin);
        product.stockQuantity = qty;
        product.reorderLevel  = reorderLevel;
        product.isActive      = true;

        bool success = (productId > 0)
                           ? Database::instance().updateProduct(product)
                           : Database::instance().addProduct(product);

        if (!success && productId <= 0) {
            Product np = Database::instance().getProductByBarcode(product.barcode);
            if (np.id > 0) {
                stockManagementTable->item(row, 0)
                ->setText(QString::number(np.id));
                success = true;
            }
        }

        if (success) { savedCount++; clearRowHighlight(row); }
        else         { fail(QString("Row %1: Database error").arg(row+1)); }
    }

    QApplication::restoreOverrideCursor();
    loadInventoryData();

    if (errorCount == 0) {
        showNotification(
            QString("Successfully saved %1 product%2!")
                .arg(savedCount).arg(savedCount > 1 ? "s" : ""),
            NotificationType::Success);
        saveChangesBtn->setEnabled(false);
        hasUnsavedChanges = false;
        changesPending    = 0;
        unsavedChangesLabel->setVisible(false);
    } else {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Save Results");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("Some products could not be saved");
        msgBox.setDetailedText(
            QString("Saved %1, errors %2:\n\n").arg(savedCount).arg(errorCount) +
            errorMessages.join("\n"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
}

void InventoryDialog::highlightRowError(int row)
{
    const ColorScheme scheme = getColorScheme();
    for (int col = 1; col <= 8; ++col) {
        if (col == 6) continue;   // selling price is read-only/derived
        if (auto *item = stockManagementTable->item(row, col))
            item->setBackground(QBrush(QColor(scheme.errorBg)));
    }
}

void InventoryDialog::clearRowHighlight(int row)
{
    const ColorScheme scheme = getColorScheme();
    for (int col = 1; col <= 8; ++col) {
        if (col == 6) continue;   // selling price is read-only/derived
        if (auto *item = stockManagementTable->item(row, col))
            item->setBackground(QBrush(QColor(scheme.inputBg)));
    }
}

void InventoryDialog::onProductSelected(int row)
{
    if (row >= 0)
        selectedProductId = inventoryTable->item(row, 0)->text().toInt();
}

void InventoryDialog::onFilterChanged(const QString &)   { updateProductsList(); }
void InventoryDialog::onSearchTextChanged(const QString &){ updateProductsList(); }

void InventoryDialog::onExportClicked()
{
    QString fn = QFileDialog::getSaveFileName(this, "Export Inventory",
                                              QString("Inventory_Export_%1.csv")
                                                  .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
                                              "CSV Files (*.csv);;All Files (*)");
    if (fn.isEmpty()) return;

    QFile file(fn);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showNotification("Could not create export file", NotificationType::Error);
        return;
    }
    QTextStream out(&file);
    out << "Product ID,Product Name,Category,Barcode,"
           "Cost Price,Profit %,Selling Price,Stock Quantity,Status\n";
    for (const InventoryInfo &info : currentInventory) {
        Product p = Database::instance().getProductById(info.productId);
        out << info.productId << ",\"" << info.productName << "\",\""
            << info.category  << "\"," << p.barcode        << ","
            << money2(p.costPrice) << ","   << p.profitMargin   << ","
            << money2(p.price)     << ","   << info.currentQuantity << ",\""
            << inventoryManager->getStatusText(info.status) << "\"\n";
    }
    file.close();
    QMessageBox::information(this, "Export Complete",
                             QString("Saved to:\n%1").arg(fn));
}

void InventoryDialog::onExportAlertsClicked()
{
    QString fn = QFileDialog::getSaveFileName(this, "Export Alerts",
                                              QString("Inventory_Alerts_%1.csv")
                                                  .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
                                              "CSV Files (*.csv)");
    if (fn.isEmpty()) return;

    QFile file(fn);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showNotification("Could not create file", NotificationType::Error);
        return;
    }
    QTextStream out(&file);
    out << "Product Name,Category,Current Stock,Reorder Level,Status,Action\n";
    QVector<InventoryInfo> alerts;
    alerts.append(inventoryManager->getCriticalStockItems());
    alerts.append(inventoryManager->getLowStockItems());
    for (const InventoryInfo &info : alerts) {
        QString action = (info.status == InventoryStatus::Critical ||
                          info.status == InventoryStatus::OutOfStock)
                             ? "URGENT: Restock immediately" : "Schedule restock soon";
        out << "\"" << info.productName << "\",\"" << info.category << "\","
            << info.currentQuantity << "," << info.reorderLevel << ",\""
            << inventoryManager->getStatusText(info.status) << "\",\"" << action << "\"\n";
    }
    file.close();
    showNotification("Alerts exported", NotificationType::Success);
}

void InventoryDialog::onBulkRestockClicked()
{
    bool ok;
    int qty = QInputDialog::getInt(this, "Bulk Restock",
                                   "Add quantity to all low-stock items:", 10, 1, 1000, 1, &ok);
    if (!ok) return;

    QVector<InventoryInfo> items = inventoryManager->getLowStockItems();
    items.append(inventoryManager->getCriticalStockItems());
    if (items.isEmpty()) { showNotification("No items need restocking", NotificationType::Info); return; }

    int n = 0;
    for (const InventoryInfo &info : items)
        if (inventoryManager->restockProduct(info.productId, qty, "Bulk Restock")) n++;
    loadInventoryData();
    showNotification(QString("Restocked %1 of %2 items").arg(n).arg(items.size()),
                     NotificationType::Success);
}

void InventoryDialog::onAutoRefreshToggled(bool checked)
{
    if (checked) { autoRefreshTimer->start(30000); showNotification("Auto-refresh on (30s)", NotificationType::Info); }
    else         { autoRefreshTimer->stop();        showNotification("Auto-refresh off",      NotificationType::Info); }
}

void InventoryDialog::onInventoryChanged()    { loadInventoryData(); }
void InventoryDialog::onCloseClicked()        { close(); }

void InventoryDialog::showNotification(const QString &message, NotificationType type)
{
    QString icon;
    switch (type) {
    case NotificationType::Success: icon = "✓"; break;
    case NotificationType::Error:   icon = "✗"; break;
    case NotificationType::Warning: icon = "⚠"; break;
    default:                        icon = "ℹ"; break;
    }
    lastUpdatedLabel->setText(QString("%1 %2").arg(icon, message));
    QTimer::singleShot(3000, this, &InventoryDialog::updateLastUpdatedTime);
}

QString InventoryDialog::formatCurrency(Money amount)
{
    return formatMoney(amount);
}

void InventoryDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) onCloseClicked();
    else QDialog::keyPressEvent(event);
}

void InventoryDialog::closeEvent(QCloseEvent *event)
{
    if (hasUnsavedChanges) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Unsaved Changes");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("You have unsaved changes in Stock Management");
        msgBox.setInformativeText("Save before closing?");
        msgBox.setStandardButtons(
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        if      (ret == QMessageBox::Save)    { onSaveChangesClicked(); event->accept(); }
        else if (ret == QMessageBox::Discard) { event->accept(); }
        else                                  { event->ignore(); }
    } else {
        event->accept();
    }
}

// ── Stubs ────────────────────────────────────────────────────────────────────
void InventoryDialog::onAutoSaveToggled(bool en)
{
    autoSaveEnabled = en;
    if (en) { autoSaveTimer->start(60000); showNotification("Auto-save on (60s)", NotificationType::Info); }
    else    { autoSaveTimer->stop();        showNotification("Auto-save off",      NotificationType::Info); }
}
void InventoryDialog::performAutoSave()
{
    if (hasUnsavedChanges && autoSaveEnabled) { onSaveChangesClicked(); showNotification("Auto-saved", NotificationType::Success); }
}
void InventoryDialog::showSaveIndicator(const QString &msg)
{
    lastUpdatedLabel->setText(msg);
    QTimer::singleShot(2000, this, &InventoryDialog::updateLastUpdatedTime);
}
void InventoryDialog::onBulkAdjustClicked()
{
    // Build list of all products for selection
    QStringList productNames;
    QVector<int> productIds;
    for (const InventoryInfo &info : currentInventory) {
        productNames << QString("%1 (current: %2)").arg(info.productName).arg(info.currentQuantity);
        productIds << info.productId;
    }
    if (productNames.isEmpty()) {
        showNotification("No products available", NotificationType::Warning);
        return;
    }

    // Ask for adjustment type
    QStringList types = {"Set absolute quantity", "Add to current quantity", "Subtract from current quantity"};
    bool ok;
    QString type = QInputDialog::getItem(this, "Bulk Stock Adjust",
                                         "Adjustment type:", types, 0, false, &ok);
    if (!ok) return;

    int value = QInputDialog::getInt(this, "Bulk Stock Adjust",
                                     "Quantity value:", 0, -9999, 99999, 1, &ok);
    if (!ok) return;

    QString reason = QInputDialog::getText(this, "Bulk Stock Adjust",
                                           "Reason for adjustment:", QLineEdit::Normal,
                                           "Periodic stock count", &ok);
    if (!ok || reason.trimmed().isEmpty()) return;

    QString adjustedBy = "Manager"; // Could pull from UserManager

    int successCount = 0, failCount = 0;

    for (const InventoryInfo &info : currentInventory) {
        int oldQty = info.currentQuantity;
        int newQty = oldQty;

        if (type == types[0])      newQty = value;
        else if (type == types[1]) newQty = oldQty + value;
        else                       newQty = qMax(0, oldQty - value);

        if (newQty == oldQty) continue;

        if (Database::instance().updateStock(info.productId, newQty)) {
            Database::instance().logStockAdjustment(
                info.productId, info.productName,
                oldQty, newQty, reason, adjustedBy);
            ++successCount;
        } else {
            ++failCount;
        }
    }

    loadInventoryData();
    showNotification(
        QString("Adjusted %1 products%2")
            .arg(successCount)
            .arg(failCount > 0 ? QString(" (%1 failed)").arg(failCount) : ""),
        failCount > 0 ? NotificationType::Warning : NotificationType::Success);
}

void InventoryDialog::onQuickAdjustClicked(int productId)
{
    InventoryInfo info = inventoryManager->getInventoryInfo(productId);
    if (info.productId <= 0) {
        showNotification("Product not found", NotificationType::Error);
        return;
    }

    bool ok;
    int newQty = QInputDialog::getInt(
        this, "Adjust Stock",
        QString("Set new quantity for:\n%1\nCurrent: %2")
            .arg(info.productName).arg(info.currentQuantity),
        info.currentQuantity, 0, 99999, 1, &ok);
    if (!ok || newQty == info.currentQuantity) return;

    QString reason = QInputDialog::getText(
        this, "Adjust Stock",
        "Reason for adjustment:",
        QLineEdit::Normal, "Manual correction", &ok);
    if (!ok) return;

    if (Database::instance().updateStock(productId, newQty)) {
        Database::instance().logStockAdjustment(
            productId, info.productName,
            info.currentQuantity, newQty,
            reason.isEmpty() ? "Manual correction" : reason,
            "Manager");
        loadInventoryData();
        showNotification(
            QString("%1: %2 → %3 units").arg(info.productName).arg(info.currentQuantity).arg(newQty),
            NotificationType::Success);
    } else {
        showNotification("Failed to adjust stock", NotificationType::Error);
    }
}

void InventoryDialog::onViewHistoryClicked(int productId)
{
    InventoryInfo info = inventoryManager->getInventoryInfo(productId);
    auto history = Database::instance().getStockHistory(productId, 50);

    QDialog dlg(this);
    dlg.setWindowTitle(QString("Stock History — %1").arg(info.productName));
    dlg.resize(700, 420);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    QLabel *header = new QLabel(
        QString("<b>%1</b> &nbsp;|&nbsp; Current stock: <b>%2</b>")
            .arg(info.productName.toHtmlEscaped()).arg(info.currentQuantity));
    header->setProperty("textScale", "md");
    layout->addWidget(header);

    QTableWidget *tbl = new QTableWidget(&dlg);
    tbl->setColumnCount(6);
    tbl->setHorizontalHeaderLabels({"Date/Time", "Change", "Old Qty", "New Qty", "Reason", "By"});
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->verticalHeader()->setVisible(false);
    tbl->setAlternatingRowColors(true);
    tbl->horizontalHeader()->setStretchLastSection(true);

    if (history.isEmpty()) {
        tbl->setRowCount(1);
        QTableWidgetItem *noData = new QTableWidgetItem("No adjustment history recorded yet.");
        noData->setTextAlignment(Qt::AlignCenter);
        tbl->setSpan(0, 0, 1, 6);
        tbl->setItem(0, 0, noData);
    } else {
        tbl->setRowCount(history.size());
        for (int i = 0; i < history.size(); ++i) {
            const auto &a = history[i];
            tbl->setItem(i, 0, new QTableWidgetItem(a.adjustedAt));

            QString changeStr = (a.changeQty >= 0 ? "+" : "") + QString::number(a.changeQty);
            QTableWidgetItem *chg = new QTableWidgetItem(changeStr);
            chg->setTextAlignment(Qt::AlignCenter);
            chg->setForeground(QBrush(QColor(a.changeQty >= 0
                                                 ? getColorScheme().success
                                                 : getColorScheme().error)));
            QFont bf = chg->font(); bf.setBold(true); chg->setFont(bf);
            tbl->setItem(i, 1, chg);

            QTableWidgetItem *oldQ = new QTableWidgetItem(QString::number(a.oldQty));
            oldQ->setTextAlignment(Qt::AlignCenter);
            tbl->setItem(i, 2, oldQ);

            QTableWidgetItem *newQ = new QTableWidgetItem(QString::number(a.newQty));
            newQ->setTextAlignment(Qt::AlignCenter);
            tbl->setItem(i, 3, newQ);

            tbl->setItem(i, 4, new QTableWidgetItem(a.reason));
            tbl->setItem(i, 5, new QTableWidgetItem(a.adjustedBy));
        }
    }
    tbl->resizeColumnsToContents();
    layout->addWidget(tbl, 1);

    QPushButton *closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    dlg.exec();
}

void InventoryDialog::onImportClicked()
{
    QString fn = QFileDialog::getOpenFileName(
        this, "Import Inventory from CSV", QString(),
        "CSV Files (*.csv);;All Files (*)");
    if (fn.isEmpty()) return;

    QFile file(fn);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showNotification("Could not open file for reading", NotificationType::Error);
        return;
    }

    QTextStream in(&file);
    QString header = in.readLine();  // skip header row

    int imported = 0, skipped = 0, errors = 0;
    QStringList errorLines;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Support quoted CSV fields
        QStringList fields;
        bool inQuote = false;
        QString field;
        for (QChar c : line) {
            if (c == '"') { inQuote = !inQuote; }
            else if (c == ',' && !inQuote) { fields << field; field.clear(); }
            else { field += c; }
        }
        fields << field;

        // Expected columns: name, category, cost_price, profit_margin, stock_qty [, barcode]
        // Minimum 5 fields
        if (fields.size() < 5) {
            ++skipped;
            errorLines << QString("Skipped (too few columns): %1").arg(line.left(60));
            continue;
        }

        QString name     = fields[0].trimmed();
        QString category = fields[1].trimmed();
        bool    costOk, marginOk, qtyOk;
        double  cost     = fields[2].trimmed().toDouble(&costOk);
        double  margin   = fields[3].trimmed().toDouble(&marginOk);
        int     qty      = fields[4].trimmed().toInt(&qtyOk);
        QString barcode  = fields.size() >= 6 ? fields[5].trimmed() : QString();

        if (name.isEmpty() || !costOk || !marginOk || !qtyOk) {
            ++errors;
            errorLines << QString("Invalid data: %1").arg(line.left(60));
            continue;
        }

        Product p;
        p.name          = name;
        p.category      = category.isEmpty() ? "Uncategorized" : category;
        p.costPrice     = Money::fromMajor(cost);
        p.profitMargin  = margin;
        p.price         = Product::calculateSellingPrice(p.costPrice, margin);
        p.stockQuantity = qty;
        p.barcode       = barcode;
        p.isActive      = true;

        if (Database::instance().addProduct(p)) {
            ++imported;
        } else {
            ++errors;
            errorLines << QString("DB error for: %1").arg(name);
        }
    }
    file.close();

    loadInventoryData();

    QString summary = QString("Import complete:\n  Imported: %1\n  Skipped: %2\n  Errors: %3")
                          .arg(imported).arg(skipped).arg(errors);
    if (!errorLines.isEmpty())
        summary += "\n\nDetails:\n" + errorLines.mid(0, 10).join("\n");

    if (errors > 0 || skipped > 0)
        QMessageBox::warning(this, "Import Results", summary);
    else
        QMessageBox::information(this, "Import Complete", summary);

    showNotification(QString("Imported %1 products").arg(imported),
                     errors > 0 ? NotificationType::Warning : NotificationType::Success);
}
void InventoryDialog::onPrintClicked()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() == QDialog::Accepted) {
        QTextDocument doc;
        QString html = "<html><body><h1>Inventory Report</h1>"
                       "<p>Generated: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "</p>"
                                                                                        "<table border='1' cellpadding='6'><tr><th>Product</th><th>Category</th><th>Price</th><th>Stock</th><th>Status</th></tr>";
        for (const InventoryInfo &info : currentInventory) {
            Product p = Database::instance().getProductById(info.productId);
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                        .arg(info.productName, info.category, formatCurrency(p.price),
                             QString::number(info.currentQuantity),
                             inventoryManager->getStatusText(info.status));
        }
        html += "</table></body></html>";
        doc.setHtml(html);
        doc.print(&printer);
        showNotification("Report printed", NotificationType::Success);
    }
}
void InventoryDialog::onInventoryUpdated(int, int) {}
void InventoryDialog::onInventoryRestocked(int, int) {}

void InventoryDialog::onQuickRestockClicked(int productId)
{

}
void InventoryDialog::onCategoryFilterChanged(const QString &category)
{
    // TODO
}

void InventoryDialog::onSortByChanged(int index)
{
    // TODO
}

void InventoryDialog::onCellDoubleClicked(int row, int column)
{
    // TODO
}
void InventoryDialog::onUndoClicked() { if (undoStack->canUndo()) { undoStack->undo(); showNotification("Undo", NotificationType::Info); } }
void InventoryDialog::onRedoClicked() { if (undoStack->canRedo()) { undoStack->redo(); showNotification("Redo", NotificationType::Info); } }
void InventoryDialog::applyModernStyling() {}
void InventoryDialog::updateStockLevels()  {}
void InventoryDialog::validateAndSaveProduct(int) {}
bool InventoryDialog::validateProductRow(int row, QString &err)
{
    if (row < 0 || row >= stockManagementTable->rowCount()) { err = "Invalid row"; return false; }
    if (stockManagementTable->item(row, 1)->text().trimmed().isEmpty()) { err = "Name required"; return false; }
    return true;
}
void InventoryDialog::highlightInvalidCell(int row, int col, const QString &err)
{
    const ColorScheme scheme = getColorScheme();
    if (auto *item = stockManagementTable->item(row, col)) { item->setBackground(QBrush(QColor(scheme.errorBg))); item->setToolTip(err); }
}
void InventoryDialog::clearCellHighlight(int row, int col)
{
    const ColorScheme scheme = getColorScheme();
    if (auto *item = stockManagementTable->item(row, col)) { item->setBackground(QBrush(QColor(scheme.inputBg))); item->setToolTip(""); }
}
QString InventoryDialog::formatQuantity(int q) { return QString::number(q); }
QString InventoryDialog::getStockTrendIcon(int id)
{
    if (previousInventory.contains(id)) {
        InventoryInfo cur = inventoryManager->getInventoryInfo(id);
        if (cur.currentQuantity > previousInventory[id].currentQuantity) return "↑";
        if (cur.currentQuantity < previousInventory[id].currentQuantity) return "↓";
    }
    return "→";
}
QColor   InventoryDialog::getStatusColor(InventoryStatus s)  { return QColor(inventoryManager->getStatusColor(s)); }
QWidget* InventoryDialog::createStockLevelCell(const InventoryInfo &) { return nullptr; }
QWidget* InventoryDialog::createQuickActionCell(int)                  { return nullptr; }
QWidget* InventoryDialog::createStatusBadge(InventoryStatus)          { return nullptr; }
void     InventoryDialog::bulkUpdateStatus()                          {}
QVector<int> InventoryDialog::getSelectedProductIds()
{
    QVector<int> ids;
    for (auto *item : inventoryTable->selectedItems()) {
        int id = inventoryTable->item(item->row(), 0)->text().toInt();
        if (!ids.contains(id)) ids.append(id);
    }
    return ids;
}