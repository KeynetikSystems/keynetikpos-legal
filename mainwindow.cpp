// -----------------------------------------------------------------------------
// mainwindow.cpp — Implementation of MainWindow (see mainwindow.h for the full
// WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - The whole UI is built in code (setupUI() + helpers); applyTheme() pulls
//    the stylesheet/palette from appstyle.h on ThemeManager::themeChanged.
//  - Cart state lives in CartService; this class reacts to its signals
//    (cartListChanged / currentCartChanged / cartContentChanged) to refresh
//    the tab bar, totals panel, and checkout button. The cart table is a
//    QTableView on CartModel — qty edits happen in place, the ✖ column
//    removes a line via onCartTableClicked().
//  - onCheckout() collects payment via PaymentDialog, then delegates the
//    pipeline (recordSale -> receipt -> inventory refresh -> audit log) to
//    CheckoutService. Failure leaves the cart intact and shows the error.
//  - BarcodeReader is installed as an application-wide event filter; scans
//    arrive in onBarcodeScanned() and add the matched product to the active
//    cart, with a toast for unknown codes.
//  - updateUIPermissions() disables menu actions/buttons the current role may
//    not use (defence in depth alongside manager-level checks).
// -----------------------------------------------------------------------------

#include "mainwindow.h"
#include "database.h"
#include "paymentdialog.h"
#include "discountdialog.h"
#include "analyticsdashboard.h"
#include "reportsdialog.h"
#include "smtpclient.h"
#include "inventorymanager.h"
#include "receiptprinter.h"
#include "inventorydialog.h"
#include "lowstockdialog.h"
#include "saleshistorydialog.h"
#include "CartItem.h"
#include "scheduledialog.h"
#include "usermanager.h"
#include "usermanagementdialog.h"
#include "colorscheme.h"
#include "settingsdialog.h"
#include "settingsmanager.h"
#include "BarcodeReader.h"
#include "cartservice.h"
#include "cartmodel.h"
#include "checkoutservice.h"
#include "providersetup.h"
#include "appstyle.h"
#include "productgridmodel.h"
#include "changepassworddialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QDateTime>
#include <QDebug>
#include <QApplication>
#include <QSettings>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QShortcut>
#include <QMenu>
#include <QFrame>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPointer>
#include <QSplitter>
#include <QStackedWidget>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPair>
#include <cmath>

// -----------------------------------------------------------------------------
// Configuration constants — single source of truth for all tunable parameters
// -----------------------------------------------------------------------------
namespace POSConfig {
constexpr int STOCK_CRITICAL        = 4;       // Red alert threshold
constexpr int STOCK_MEDIUM          = 20;      // Orange warning threshold

constexpr int INVENTORY_CHECK_MS    = 60000;   // Auto-refresh interval (ms)

constexpr int TOAST_DISPLAY_MS      = 2800;
constexpr int TOAST_FADE_MS         = 200;

constexpr int DATETIME_UPDATE_MS    = 1000;
}

// -----------------------------------------------------------------------------
// File-local helpers
// (Money/formatMoney live in money.h; CartTotals/computeCartTotals in
//  checkoutservice.h)
// -----------------------------------------------------------------------------
static SmtpConfig makeSmtpConfig(const BusinessSettings &bs)
{
    SmtpConfig cfg;
    cfg.host      = bs.smtpHost;
    cfg.port      = bs.smtpPort;
    cfg.security  = static_cast<SmtpConfig::Security>(bs.smtpSecurity);
    cfg.username  = bs.smtpUsername;
    cfg.password  = bs.smtpPassword;
    // Fall back to the business email as the From address if none is given.
    cfg.fromEmail = bs.smtpFromEmail.isEmpty() ? bs.email : bs.smtpFromEmail;
    cfg.fromName  = bs.businessName;
    return cfg;
}

static inline bool checkPermission(QWidget *parent, Permission perm,
                                   const QString &featureName)
{
    if (UserManager::instance().hasPermission(perm))
        return true;
    QMessageBox::warning(parent, "Access Denied",
                         QString("You do not have permission to %1.").arg(featureName));
    return false;
}

static inline QPushButton *createStatusBarButton(const QString &label,
                                                 const char *kind)
{
    QPushButton *btn = new QPushButton(label);
    btn->setProperty("kind", kind);   // styled centrally in appstyle.cpp
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , receiptPrinter(new ReceiptPrinter())
{
    // ── Database must initialise first — everything else depends on it ──────
    if (!Database::instance().initialize()) {
        QMessageBox::critical(this, "Database Error",
                              "Failed to initialize database: "
                                  + Database::instance().getLastError());
        QApplication::quit();
        return;
    }

    QSqlDatabase appDb = QSqlDatabase::database();
    settingsManager = new SettingsManager(appDb, this);

    // Currency symbol is configurable; seed the app-wide money formatter before
    // any widget renders a price (setupUI runs below).
    setCurrencySymbol(settingsManager->settings().currencySymbol);

    inventoryManager = new InventoryManager();

    // ── Schedule / messaging ────────────────────────────────────────────────
    // Object + table must exist now; provider wiring and the scheduler loop are
    // deferred (runDeferredStartup) so they don't delay first paint.
    scheduleManager = new ScheduleManager(this);
    scheduleManager->initDatabase();

    // ── Services (CartService creates the first cart itself) ───────────────
    cartService     = new CartService(this);
    checkoutService = new CheckoutService(inventoryManager, receiptPrinter);

    // ── Theme ───────────────────────────────────────────────────────────────
    isDarkMode = ThemeManager::instance().isDark();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](bool dark) {
                isDarkMode = dark;
                applyTheme();
            });

    setWindowTitle("KeynetikPOS - Point of Sale System");
    resize(1400, 900);
    // Floor below which the two-panel layout starts clipping; keeps the app
    // usable on small/secondary cashier displays.
    setMinimumSize(1024, 640);

    // ── Build UI ─────────────────────────────────────────────────────────────
    setupUI();

    // Cart state changes drive the UI from here on
    connect(cartService, &CartService::currentCartChanged,
            this, [this](int) { refreshCartDisplay(); });
    connect(cartService, &CartService::cartListChanged,
            this, &MainWindow::updateCartSelector);
    connect(cartService, &CartService::cartContentChanged,
            this, [this](int) { onCartContentChanged(); });

    loadProducts();
    updateTotals();
    refreshCartDisplay();
    setupUserInterface();
    updateUIPermissions();
    applyTheme();

    // ── Inventory monitoring ────────────────────────────────────────────────
    // Connections are cheap and set up now; the polling timer starts later in
    // runDeferredStartup() so the first refresh doesn't run before first paint.
    connect(inventoryManager, &InventoryManager::inventoryLow,
            this, &MainWindow::onInventoryLow);
    connect(inventoryManager, &InventoryManager::inventoryCritical,
            this, &MainWindow::onInventoryCritical);
    connect(inventoryManager, &InventoryManager::inventoryOutOfStock,
            this, &MainWindow::onInventoryOutOfStock);

    // ── Receipt printer ─────────────────────────────────────────────────────
    const BusinessSettings &bs = settingsManager->settings();
    receiptPrinter->setCompanyInfo(bs.businessName, bs.address, bs.phone, "");
    receiptPrinter->setReceiptFooter(bs.receiptFooter);
    receiptPrinter->setSmtpConfig(makeSmtpConfig(bs));

    // Keep totals + tax label + currency in sync when settings change
    connect(settingsManager, &SettingsManager::settingsChanged, this, [this]() {
        setCurrencySymbol(settingsManager->settings().currencySymbol);
        refreshTaxTitle();
        updateTotals();
        loadProducts();   // re-render price tags with the (possibly) new symbol
    });

    // ── Deferred startup ────────────────────────────────────────────────────
    // Posted with a 0ms timer: it fires on the first event-loop iteration,
    // which is AFTER main() calls show(), so the window paints first and the
    // cashier can start working while scanning/scheduling spin up.
    QTimer::singleShot(0, this, &MainWindow::runDeferredStartup);
}

// =============================================================================
// Deferred (post-paint) startup
// =============================================================================
void MainWindow::runDeferredStartup()
{
    // Messaging providers + scheduler loop (may touch settings/network).
    reloadMessagingProviders(scheduleManager, this);

    // Surface scheduled-message outcomes (delivery is confirmed asynchronously
    // from the provider, not assumed on dispatch).
    connect(scheduleManager, &ScheduleManager::messageError, this,
            [this](int, const QString &error) {
        showToast("Scheduled message failed: " + error, "error");
    });
    connect(scheduleManager, &ScheduleManager::messageDelivered, this,
            [this](int, const QString &) {
        if (statusLabel) statusLabel->setText("Scheduled report sent");
    });

    scheduleManager->start();

    // Inventory polling.
    inventoryManager->startAutoRefresh(POSConfig::INVENTORY_CHECK_MS);

    // Barcode scanner — in serial mode the port open() can block briefly.
    // statusLabel already exists (setupUI ran in the ctor).
    initBarcodeReader();
}

MainWindow::~MainWindow()
{
    delete checkoutService;
    delete inventoryManager;
    delete receiptPrinter;
    // settingsManager, scheduleManager, cartService, m_barcodeReader are
    // QObject children
}

// =============================================================================
// Barcode reader initialisation
// =============================================================================

void MainWindow::initBarcodeReader()
{
    m_barcodeReader = new BarcodeReader(this);

    QSettings cfg("KeynetikPOS", "KeynetikPOS");
    const QString mode = cfg.value("barcode/inputMode", "keyboard").toString();

    if (mode == "serial") {
        m_barcodeReader->setInputMode(BarcodeReader::InputMode::SerialPort);
        m_barcodeReader->setPortName(
            cfg.value("barcode/portName", "COM3").toString());
        m_barcodeReader->setBaudRate(
            static_cast<QSerialPort::BaudRate>(
                cfg.value("barcode/baudRate",
                          static_cast<int>(QSerialPort::Baud9600)).toInt()));
    } else {
        // USB HID keyboard-wedge — most common scanner type
        m_barcodeReader->setInputMode(BarcodeReader::InputMode::KeyboardWedge);
    }

    // Strip trailing CR that many scanners append after the barcode digits
    m_barcodeReader->setSuffix("\r");

    connect(m_barcodeReader, &BarcodeReader::barcodeScanned,
            this,            &MainWindow::onBarcodeScanned);
    connect(m_barcodeReader, &BarcodeReader::errorOccurred,
            this,            &MainWindow::onBarcodeError);

    if (!m_barcodeReader->open()) {
        statusLabel->setText("Barcode scanner failed to open — check settings.");
        return;
    }

    // Keyboard-wedge: intercept keystrokes app-wide before any widget sees them
    if (mode != "serial")
        qApp->installEventFilter(m_barcodeReader);

    statusLabel->setText("Barcode scanner ready.");
    qDebug() << "[BarcodeReader] Initialised in" << mode << "mode.";
}

// =============================================================================
// Barcode slots
// =============================================================================

// -----------------------------------------------------------------------------
// onBarcodeScanned
// Called by BarcodeReader with the clean barcode string.
// Looks up the product, checks stock, and adds it to the active cart.
// -----------------------------------------------------------------------------
void MainWindow::onBarcodeScanned(const QString &barcode)
{
    if (barcode.trimmed().isEmpty())
        return;

    const QString code = barcode.trimmed();

    // ── 1. Database lookup ───────────────────────────────────────────────
    Product product = Database::instance().getProductByBarcode(code);

    if (product.id <= 0) {
        const QString msg = QString("Unknown barcode: %1").arg(code);
        statusLabel->setText(msg);
        showToast(msg, "warning");
        qDebug() << "[Barcode] Not found:" << code;
        return;
    }

    // ── 2. Stock check ───────────────────────────────────────────────────
    if (product.stockQuantity <= 0) {
        const QString msg = QString("Out of stock: %1").arg(product.name);
        statusLabel->setText(msg);
        showToast(msg, "error");
        qDebug() << "[Barcode] Out of stock:" << product.name;
        return;
    }

    // ── 3. Add to cart ───────────────────────────────────────────────────
    // addToCart() handles duplicate merging, canSell() check, and UI refresh
    addToCart(product, 1);

    // ── 4. Visual feedback ───────────────────────────────────────────────
    showToast(
        QString("%1  —  %2")
            .arg(product.name, formatCurrency(product.price)),
        "success");

    qDebug() << "[Barcode] Added:" << product.name
             << " | stock remaining:" << (product.stockQuantity - 1);
}

void MainWindow::onBarcodeError(const QString &error)
{
    const QString msg = QString("Scanner error: %1").arg(error);
    statusLabel->setText(msg);
    showToast(msg, "error");
    qWarning() << "[BarcodeReader]" << error;
}

// =============================================================================
// UI Setup
// =============================================================================

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(10);

    // A splitter (not fixed layout stretch) lets the cashier favour the
    // catalogue or the cart on their screen; the 2:1 start ratio is preserved.
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(8);

    setupProductsPanel();
    splitter->addWidget(productsPanel);

    setupCartPanel();
    splitter->addWidget(cartPanel);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter);

    setupMenuBar();
    setupStatusBar();
    setupKeyboardShortcuts();
}

void MainWindow::setupUserInterface()
{
    QMenu *userMenu = menuBar()->addMenu("&User");

    userManagementAction = new QAction("User Management", this);
    connect(userManagementAction, &QAction::triggered,
            this, &MainWindow::onUserManagement);
    userMenu->addAction(userManagementAction);

    changePasswordAction = new QAction("Change Password", this);
    connect(changePasswordAction, &QAction::triggered,
            this, &MainWindow::onChangePassword);
    userMenu->addAction(changePasswordAction);

    userMenu->addSeparator();

    logoutAction = new QAction("Logout", this);
    logoutAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(logoutAction, &QAction::triggered, this, &MainWindow::onLogout);
    userMenu->addAction(logoutAction);

    currentUserLabel = new QLabel();
    currentUserLabel->setObjectName("currentUserLabel");
    statusBar()->addPermanentWidget(currentUserLabel);
    showCurrentUserInfo();
}

void MainWindow::updateUIPermissions()
{
    UserManager &userMgr = UserManager::instance();
    userManagementAction->setEnabled(
        userMgr.hasPermission(Permission::MANAGE_USERS) ||
        userMgr.hasPermission(Permission::VIEW_USERS));
    userMgr.logUserAction("Application Started", "Opened POS application");
}

void MainWindow::showCurrentUserInfo()
{
    if (!UserManager::instance().isLoggedIn()) {
        currentUserLabel->setText("Not logged in");
        return;
    }
    User currentUser = UserManager::instance().getCurrentUser();
    currentUserLabel->setText(
        QString("%1 (%2)")
            .arg(currentUser.fullName,
                 RoleManager::roleToString(currentUser.role)));
}

void MainWindow::setupMenuBar()
{
    QMenuBar *mb = new QMenuBar(this);
    setMenuBar(mb);

    // File
    QMenu *fileMenu = mb->addMenu("&File");
    QAction *newSaleAction = fileMenu->addAction("New Sale");
    newSaleAction->setShortcut(QKeySequence::New);
    connect(newSaleAction, &QAction::triggered, this, &MainWindow::onNewSale);
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("Exit");
    // QKeySequence::Quit is empty on Windows — bind Ctrl+Q explicitly
    exitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // Sales
    QMenu *salesMenu = mb->addMenu("&Sales");
    connect(salesMenu->addAction("View Sales History"),
            &QAction::triggered, this, &MainWindow::onViewSalesHistory);
    connect(salesMenu->addAction("Reprint Last Receipt"),
            &QAction::triggered, this, &MainWindow::onReprintReceipt);
    connect(salesMenu->addAction("Email Last Receipt"),
            &QAction::triggered, this, &MainWindow::onEmailReceipt);

    // Inventory
    QMenu *inventoryMenu = mb->addMenu("&Inventory");
    inventoryButton = inventoryMenu->addAction("Manage Inventory");
    connect(inventoryButton, &QAction::triggered,
            this, &MainWindow::onManageInventory);
    connect(inventoryMenu->addAction("Add Product"),
            &QAction::triggered, this, &MainWindow::onAddProduct);
    inventoryMenu->addSeparator();
    connect(inventoryMenu->addAction("Low Stock Alert"),
            &QAction::triggered, this, &MainWindow::onShowLowStock);

    // Reports
    QMenu *reportsMenu = mb->addMenu("&Reports");
    analyticsButton = reportsMenu->addAction("Analytics Dashboard");
    connect(analyticsButton, &QAction::triggered,
            this, &MainWindow::onShowAnalytics);
    reportsButton = reportsMenu->addAction("Daily Report");
    connect(reportsButton, &QAction::triggered,
            this, &MainWindow::onDailyReport);
    connect(reportsMenu->addAction("Detailed Reports"),
            &QAction::triggered, this, &MainWindow::onShowReports);

    // Settings
    QMenu *settingsMenu = mb->addMenu("&Settings");
    connect(settingsMenu->addAction("Company Information"),
            &QAction::triggered, this, &MainWindow::onCompanySettings);
    connect(settingsMenu->addAction("Message Schedules"),
            &QAction::triggered, this, &MainWindow::onManageSchedules);
    connect(settingsMenu->addAction("Receipt Settings"),
            &QAction::triggered, this, &MainWindow::onReceiptSettings);
    connect(settingsMenu->addAction("Backup Now"),
            &QAction::triggered, this, &MainWindow::onBackupNow);
    settingsMenu->addSeparator();
    themeAction = settingsMenu->addAction("Toggle Dark Mode");
    connect(themeAction, &QAction::triggered, this, &MainWindow::onToggleTheme);

    // Help
    QMenu *helpMenu = mb->addMenu("Help");
    connect(helpMenu->addAction("About"),
            &QAction::triggered, this, &MainWindow::onAbout);
    connect(helpMenu->addAction("Keyboard Shortcuts"),
            &QAction::triggered, this, &MainWindow::onShowShortcuts);
}

void MainWindow::setupStatusBar()
{
    QStatusBar *sb = new QStatusBar(this);
    setStatusBar(sb);

    statusLabel = new QLabel("Ready");
    sb->addWidget(statusLabel);
    sb->addPermanentWidget(new QLabel(" | "));

    dateTimeLabel = new QLabel(
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    sb->addPermanentWidget(dateTimeLabel);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [this]() {
        dateTimeLabel->setText(
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    });
    timer->start(POSConfig::DATETIME_UPDATE_MS);

    QFrame *sep = new QFrame();
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sb->addPermanentWidget(sep);

    QPushButton *inventoryBtn = createStatusBarButton("Inventory", "info");
    connect(inventoryBtn, &QPushButton::clicked,
            this, &MainWindow::onManageInventory);
    sb->addPermanentWidget(inventoryBtn);

    QPushButton *salesBtn = createStatusBarButton("Sales", "info");
    connect(salesBtn, &QPushButton::clicked,
            this, &MainWindow::onViewSalesHistory);
    sb->addPermanentWidget(salesBtn);

    QPushButton *analyticsBtn = createStatusBarButton("Analytics", "primary");
    connect(analyticsBtn, &QPushButton::clicked,
            this, &MainWindow::onShowAnalytics);
    sb->addPermanentWidget(analyticsBtn);
}

void MainWindow::setupProductsPanel()
{
    productsPanel = new QGroupBox("Products");
    QVBoxLayout *layout = new QVBoxLayout(productsPanel);

    // Model + proxy first: the search/category signal handlers below use them.
    productModel = new ProductGridModel(this);
    productProxy = new ProductFilterProxy(this);
    productProxy->setSourceModel(productModel);

    // Search bar — filtering is a proxy invalidation, cheap enough to run on
    // every keystroke (no debounce needed).
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("Search products by name or barcode...");
    searchEdit->setMinimumHeight(40);
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setAccessibleName("Product search");
    searchEdit->setToolTip("Search products by name or barcode (F3)");
    connect(searchEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) {
                productProxy->setSearchText(text);
                updateProductEmptyState();
            });
    searchLayout->addWidget(searchEdit);
    layout->addLayout(searchLayout);

    // Category filter
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("Category:"));
    categoryCombo = new QComboBox();
    categoryCombo->addItem("All Categories");
    categoryCombo->setMinimumHeight(35);
    connect(categoryCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &cat) {
                productProxy->setCategory(cat);
                updateProductEmptyState();
            });
    filterLayout->addWidget(categoryCombo);
    filterLayout->addStretch();
    layout->addLayout(filterLayout);

    // Product grid: QListView in icon mode over model + filter proxy —
    // no widget churn on search/checkout, scales to any catalogue size.
    productView = new QListView();
    productView->setObjectName("productGrid");
    productView->setAccessibleName("Product catalogue");
    productView->setModel(productProxy);
    productView->setItemDelegate(new ProductCardDelegate(
        POSConfig::STOCK_CRITICAL, POSConfig::STOCK_MEDIUM, productView));
    productView->setViewMode(QListView::IconMode);
    productView->setResizeMode(QListView::Adjust);
    productView->setMovement(QListView::Static);
    productView->setUniformItemSizes(true);
    productView->setSpacing(8);
    // SingleSelection (not NoSelection) so the arrow keys move a current item;
    // the delegate shades the selected card. Disabled (out-of-stock) cards are
    // skipped by keyboard nav automatically.
    productView->setSelectionMode(QAbstractItemView::SingleSelection);
    productView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    productView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    productView->setMouseTracking(true);   // hover shade in the delegate
    productView->setTabKeyNavigation(true);
    // Click adds; Enter/Return (activated) adds the keyboard-focused card.
    connect(productView, &QListView::clicked,
            this, &MainWindow::onProductCardClicked);
    connect(productView, &QListView::activated,
            this, &MainWindow::onProductCardClicked);

    // Empty state: when a search/filter yields nothing, show a hint instead of
    // a blank grid. A QStackedWidget swaps the view for the placeholder.
    productEmpty = new QLabel(
        "No products match your search.\n\n"
        "Try a different term or clear the filters.");
    productEmpty->setAlignment(Qt::AlignCenter);
    productEmpty->setProperty("kind", "secondary");
    productEmpty->setProperty("textScale", "lg");

    productStack = new QStackedWidget();
    productStack->addWidget(productView);    // index 0 — grid
    productStack->addWidget(productEmpty);   // index 1 — empty hint
    layout->addWidget(productStack);
}

void MainWindow::setupCartPanel()
{
    cartPanel = new QGroupBox("Shopping Cart");
    QVBoxLayout *layout = new QVBoxLayout(cartPanel);
    layout->setSpacing(10);

    setupCartSelector();
    layout->addWidget(cartSelectorWidget);

    cartModel = new CartModel(cartService, this);
    cartModel->setStockProvider([](int productId) {
        return Database::instance().getProductById(productId).stockQuantity;
    });

    cartTable = new QTableView();
    cartTable->setModel(cartModel);
    cartTable->setItemDelegateForColumn(
        CartModel::ColQty, new CartQtyDelegate(cartModel, cartTable));
    cartTable->verticalHeader()->setVisible(false);
    cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    cartTable->setEditTriggers(QAbstractItemView::DoubleClicked |
                               QAbstractItemView::SelectedClicked |
                               QAbstractItemView::EditKeyPressed);
    cartTable->setAlternatingRowColors(true);
    // Sorting stays OFF: rows map 1:1 to Cart::items indices.
    cartTable->setSortingEnabled(false);
    cartTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Column sizing: the product name takes the slack; price/subtotal hug their
    // content; the − / qty / + / ✖ controls are fixed, generously-sized tap
    // targets for touch terminals.
    QHeaderView *hh = cartTable->horizontalHeader();
    hh->setStretchLastSection(false);
    hh->setSectionResizeMode(QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(CartModel::ColProduct, QHeaderView::Stretch);
    hh->setSectionResizeMode(CartModel::ColDec,    QHeaderView::Fixed);
    hh->setSectionResizeMode(CartModel::ColQty,    QHeaderView::Fixed);
    hh->setSectionResizeMode(CartModel::ColInc,    QHeaderView::Fixed);
    hh->setSectionResizeMode(CartModel::ColRemove, QHeaderView::Fixed);
    cartTable->setColumnWidth(CartModel::ColDec,    46);
    cartTable->setColumnWidth(CartModel::ColQty,    56);
    cartTable->setColumnWidth(CartModel::ColInc,    46);
    cartTable->setColumnWidth(CartModel::ColRemove, 56);
    // Taller rows = bigger tap targets and easier reading on a touchscreen.
    cartTable->verticalHeader()->setDefaultSectionSize(46);

    connect(cartTable, &QTableView::clicked,
            this, &MainWindow::onCartTableClicked);

    cartEmpty = new QLabel(
        "Cart is empty.\n\nScan a barcode or tap a product to begin.");
    cartEmpty->setAlignment(Qt::AlignCenter);
    cartEmpty->setProperty("kind", "secondary");
    cartEmpty->setProperty("textScale", "lg");

    cartStack = new QStackedWidget();
    cartStack->addWidget(cartTable);    // index 0 — line items
    cartStack->addWidget(cartEmpty);    // index 1 — empty hint
    layout->addWidget(cartStack, 1);

    setupTotalsSection();
    layout->addWidget(totalsGroup);

    setupActionButtons();
    layout->addLayout(actionsLayout);
}

void MainWindow::setupTotalsSection()
{
    totalsGroup = new QGroupBox("Order Summary");
    QGridLayout *totalsLayout = new QGridLayout(totalsGroup);

    totalsLayout->addWidget(new QLabel("Subtotal:"), 0, 0);
    subtotalLabel = new QLabel(formatCurrency(Money()));
    subtotalLabel->setProperty("role", "amount");
    subtotalLabel->setAlignment(Qt::AlignRight);
    totalsLayout->addWidget(subtotalLabel, 0, 1);

    taxTitleLabel = new QLabel();
    refreshTaxTitle();
    totalsLayout->addWidget(taxTitleLabel, 1, 0);
    taxLabel = new QLabel(formatCurrency(Money()));
    taxLabel->setProperty("role", "amount");
    taxLabel->setAlignment(Qt::AlignRight);
    totalsLayout->addWidget(taxLabel, 1, 1);

    totalsLayout->addWidget(new QLabel("Discount:"), 2, 0);
    discountLabel = new QLabel(formatCurrency(Money()));
    discountLabel->setProperty("role", "amountDiscount");
    discountLabel->setAlignment(Qt::AlignRight);
    totalsLayout->addWidget(discountLabel, 2, 1);

    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    totalsLayout->addWidget(separator, 3, 0, 1, 2);

    QLabel *totalTitle = new QLabel("TOTAL:");
    totalTitle->setProperty("role", "totalTitle");
    totalsLayout->addWidget(totalTitle, 4, 0);

    totalLabel = new QLabel(formatCurrency(Money()));
    totalLabel->setProperty("role", "amountTotal");
    totalLabel->setAlignment(Qt::AlignRight);
    totalsLayout->addWidget(totalLabel, 4, 1);
}

void MainWindow::setupActionButtons()
{
    actionsLayout = new QVBoxLayout();

    discountButton = new QPushButton("Apply Discount");
    discountButton->setMinimumHeight(45);
    discountButton->setProperty("kind", "warning");
    discountButton->setAccessibleName("Apply discount");
    discountButton->setToolTip("Apply a discount to the current cart (F4)");
    connect(discountButton, &QPushButton::clicked,
            this, &MainWindow::onApplyDiscount);
    actionsLayout->addWidget(discountButton);

    QPushButton *clearBtn = new QPushButton("Clear Cart");
    clearBtn->setMinimumHeight(45);
    clearBtn->setProperty("kind", "danger");
    clearBtn->setAccessibleName("Clear cart");
    clearBtn->setToolTip("Remove all items from the current cart (Ctrl+W)");
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearCart);
    actionsLayout->addWidget(clearBtn);

    checkoutBtn = new QPushButton("CHECKOUT");
    checkoutBtn->setObjectName("checkoutButton");
    checkoutBtn->setMinimumHeight(60);
    checkoutBtn->setProperty("kind", "primary");
    checkoutBtn->setEnabled(false);
    checkoutBtn->setAccessibleName("Checkout");
    checkoutBtn->setToolTip("Take payment for the current cart (F2)");
    connect(checkoutBtn, &QPushButton::clicked, this, &MainWindow::onCheckout);
    actionsLayout->addWidget(checkoutBtn);
}

// =============================================================================
// Products
// =============================================================================

void MainWindow::loadProducts()
{
    productModel->setProducts(Database::instance().getAllProducts());

    // Repopulate categories, preserving the cashier's current filter.
    const QString previous = categoryCombo->currentText();
    QStringList categories = Database::instance().getAllCategories();
    categoryCombo->blockSignals(true);
    categoryCombo->clear();
    categoryCombo->addItem("All Categories");
    categoryCombo->addItems(categories);
    const int idx = categoryCombo->findText(previous);
    categoryCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    categoryCombo->blockSignals(false);
    productProxy->setCategory(categoryCombo->currentText());
    updateProductEmptyState();
}

// Swap the grid for a hint when a search/filter yields no products.
void MainWindow::updateProductEmptyState()
{
    if (!productStack || !productProxy) return;
    productStack->setCurrentIndex(productProxy->rowCount() == 0 ? 1 : 0);
}

// Swap the cart table for a hint when there are no line items.
void MainWindow::updateCartEmptyState()
{
    if (!cartStack) return;
    Cart *cart = cartService->current();
    cartStack->setCurrentIndex((!cart || cart->items.isEmpty()) ? 1 : 0);
}

// =============================================================================
// Cart operations
// =============================================================================

void MainWindow::addToCart(const Product &product, int quantity)
{
    if (!inventoryManager->canSell(product.id, quantity)) {
        QMessageBox::warning(this, "Insufficient Stock",
                             QString("Cannot add %1. Only %2 units available.")
                                 .arg(product.name)
                                 .arg(product.stockQuantity));
        return;
    }

    CartItem ci;
    ci.productId  = product.id;
    ci.name       = product.name;
    ci.price      = product.price;
    ci.costPrice  = product.costPrice;
    ci.quantity   = quantity;

    // CartService emits cartContentChanged -> model, totals, tab label and
    // checkout button all refresh from there.
    const bool merged = cartService->addItem(ci);
    statusLabel->setText(merged
                             ? QString("Updated %1 in cart").arg(product.name)
                             : QString("Added %1 to cart").arg(product.name));
}

// -----------------------------------------------------------------------------
// onCartContentChanged — single reaction point for item/discount changes;
// CartModel refreshes itself from the same CartService signal.
// -----------------------------------------------------------------------------
void MainWindow::onCartContentChanged()
{
    Cart *activeCart = cartService->current();
    checkoutBtn->setEnabled(activeCart && !activeCart->items.isEmpty());
    updateTotals();
    updateCartEmptyState();
}

// -----------------------------------------------------------------------------
// onCartTableClicked — the ✖ column removes the clicked line item
// -----------------------------------------------------------------------------
void MainWindow::onCartTableClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    const int row = index.row();
    switch (index.column()) {
    case CartModel::ColDec:
        cartModel->adjustQuantity(row, -1);
        break;
    case CartModel::ColInc:
        cartModel->adjustQuantity(row, +1);
        break;
    case CartModel::ColRemove: {
        const QString itemName =
            cartModel->index(row, CartModel::ColProduct).data().toString();
        if (cartService->removeItemAt(row))
            statusLabel->setText(QString("Removed %1 from cart").arg(itemName));
        break;
    }
    default:
        break;
    }
}

void MainWindow::updateTotals()
{
    Cart *activeCart = cartService->current();
    if (!activeCart) {
        subtotalLabel->setText(formatCurrency(Money()));
        taxLabel->setText(formatCurrency(Money()));
        discountLabel->setText(formatCurrency(Money()));
        totalLabel->setText(formatCurrency(Money()));
        return;
    }

    const CartTotals t = computeCartTotals(activeCart->getSubtotal(),
                                           activeCart->discount,
                                           settingsManager->settings());

    subtotalLabel->setText(formatCurrency(t.subtotal));
    taxLabel->setText(formatCurrency(t.tax));
    discountLabel->setText(formatCurrency(t.discount));
    totalLabel->setText(formatCurrency(t.total));

    updateCurrentCartTabText();
}

void MainWindow::refreshTaxTitle()
{
    if (!taxTitleLabel) return;
    const BusinessSettings &bs = settingsManager->settings();
    if (!bs.taxEnabled || bs.taxRate <= 0.0) {
        taxTitleLabel->setText("Tax:");
    } else {
        taxTitleLabel->setText(QString("%1 (%2%)%3:")
                                   .arg(bs.taxLabel)
                                   .arg(bs.taxRate * 100.0, 0, 'g', 4)
                                   .arg(bs.taxInclusive ? " incl." : ""));
    }
}

// =============================================================================
// Cart slots
// =============================================================================

void MainWindow::onProductCardClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    const int productId = index.data(ProductGridModel::ProductIdRole).toInt();
    Product product = Database::instance().getProductById(productId);
    if (product.id > 0 && product.stockQuantity > 0)
        addToCart(product, 1);
}

void MainWindow::onApplyDiscount()
{
    if (!checkPermission(this, Permission::APPLY_DISCOUNTS, "apply discounts")) {
        UserManager::instance().logUserAction(
            "Attempted Discount", "Access denied - insufficient permissions");
        return;
    }

    Cart *activeCart = cartService->current();
    if (!activeCart) return;
    if (activeCart->items.isEmpty()) {
        QMessageBox::information(this, "Empty Cart", "Add items to cart first.");
        return;
    }

    if (settingsManager->requirePinForDiscount()) {
        bool ok = false;
        const QString pin = QInputDialog::getText(
            this, "Discount PIN", "Enter the discount authorization PIN:",
            QLineEdit::Password, "", &ok);
        if (!ok) return;
        if (!settingsManager->verifyDiscountPin(pin)) {
            QMessageBox::warning(this, "Wrong PIN",
                                 "The PIN entered is incorrect.");
            UserManager::instance().logUserAction(
                "Attempted Discount", "Wrong discount PIN entered");
            return;
        }
    }

    const Money subtotal = activeCart->getSubtotal();
    DiscountDialog dialog(subtotal, this);
    if (dialog.exec() == QDialog::Accepted) {
        cartService->setDiscount(dialog.getDiscountAmount(),
                                 dialog.getDiscountReason());
        statusLabel->setText(
            QString("Discount of %1 applied")
                .arg(formatCurrency(dialog.getDiscountAmount())));
        UserManager::instance().logUserAction(
            "Applied Discount",
            QString("Amount: %1, Reason: %2")
                .arg(formatCurrency(dialog.getDiscountAmount()))
                .arg(dialog.getDiscountReason()));
    }
}

void MainWindow::onClearCart()
{
    Cart *activeCart = cartService->current();
    if (!activeCart || activeCart->items.isEmpty()) return;

    if (QMessageBox::question(this, "Clear Cart",
                              "Are you sure you want to clear the cart?",
                              QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        cartService->clearCurrent();
        statusLabel->setText("Cart cleared");
    }
}

void MainWindow::onCheckout()
{
    Cart *activeCart = cartService->current();
    if (!activeCart || activeCart->items.isEmpty()) return;

    const CartTotals t = computeCartTotals(activeCart->getSubtotal(),
                                           activeCart->discount,
                                           settingsManager->settings());

    PaymentDialog paymentDialog(t.total, this);
    if (paymentDialog.exec() != QDialog::Accepted) return;

    const Money amountPaid = paymentDialog.getAmountPaid();
    const Money change     = paymentDialog.getChange();

    // CheckoutService runs the pipeline: recordSale (atomic stock check +
    // insert + decrement) -> receipt -> inventory refresh -> audit log.
    // On failure the cart is preserved and no receipt is printed.
    const CheckoutResult result = checkoutService->finalizeSale(
        *activeCart, t,
        paymentDialog.getPaymentMethod(),
        paymentDialog.getReferenceNumber(),
        amountPaid, change);

    if (!result.ok) {
        QMessageBox::critical(this, "Checkout Failed",
                              "The sale was NOT recorded:\n" + result.error
                                  + "\n\nThe cart has been kept so you can retry.");
        return;
    }

    // Only the sold products' stock changed — refresh just those grid cells
    // instead of reloading the whole catalogue and resetting the model (which
    // re-lays-out every card and flickers the grid). Capture the ids before
    // clearing the cart, then push the authoritative new stock into the model.
    QVector<int> soldProductIds;
    soldProductIds.reserve(activeCart->items.size());
    for (const CartItem &it : activeCart->items)
        soldProductIds.append(it.productId);

    cartService->clearCurrent();

    for (int pid : soldProductIds)
        productModel->updateStock(pid, Database::instance().getProductById(pid).stockQuantity);

    // A sale may push today's total over a configured sales-threshold schedule.
    scheduleManager->notifySalesThreshold(
        Database::instance().getTotalSalesToday().toMajor());

    // Non-blocking success: a blocking dialog after every sale slows the queue.
    // Change due is the one figure the cashier must act on, so it stays in the
    // persistent status bar in addition to the auto-dismissing toast. Focus
    // returns to the search box, ready for the next customer.
    const QString summary = change.cents() > 0
        ? QString("Sale #%1 complete — Change due: %2")
              .arg(result.saleId).arg(formatCurrency(change))
        : QString("Sale #%1 complete — receipt saved").arg(result.saleId);
    statusLabel->setText(summary);
    showToast(summary, "success");
    searchEdit->clear();
    searchEdit->setFocus();
}

void MainWindow::onNewSale()
{
    Cart *activeCart = cartService->current();
    if (activeCart && !activeCart->items.isEmpty()) {
        if (QMessageBox::question(
                this, "New Sale",
                "Current cart will be cleared. Continue?",
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    cartService->clearCurrent();
    searchEdit->clear();
    categoryCombo->setCurrentIndex(0);
    statusLabel->setText("Ready for new sale");
}

// =============================================================================
// Multi-cart management
// =============================================================================

void MainWindow::setupCartSelector()
{
    cartSelectorWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(cartSelectorWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    cartTabWidget = new QTabWidget();
    cartTabWidget->setTabsClosable(true);
    cartTabWidget->setMovable(true);
    cartTabWidget->setDocumentMode(true);
    cartTabWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(cartTabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::onSwitchCart);
    connect(cartTabWidget, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);
    connect(cartTabWidget, &QWidget::customContextMenuRequested,
            this, &MainWindow::onCartTabContextMenu);

    layout->addWidget(cartTabWidget, 1);

    QPushButton *newCartBtn = new QPushButton("New Cart");
    newCartBtn->setObjectName("newCartButton");
    newCartBtn->setToolTip("Create new shopping cart (F5)");
    newCartBtn->setMinimumHeight(38);
    newCartBtn->setProperty("kind", "primary");
    connect(newCartBtn, &QPushButton::clicked, this, &MainWindow::onNewCart);
    layout->addWidget(newCartBtn);
}

void MainWindow::refreshCartDisplay()
{
    updateCartSelector();
    onCartContentChanged();
    Cart *cart = cartService->current();
    if (cart)
        statusLabel->setText(QString("Switched to: %1").arg(cart->name));
}

QString MainWindow::cartTabLabel(const Cart &cart) const
{
    QString tabText =
        QString("%1 (%2 items)").arg(cart.name).arg(cart.getItemCount());
    if (cart.getSubtotal().cents() > 0)
        tabText += QString(" - %1").arg(formatCurrency(cart.getSubtotal()));
    return tabText;
}

QString MainWindow::cartTabTooltip(const Cart &cart) const
{
    return QString("%1\nItems: %2\nSubtotal: %3\nCreated: %4")
        .arg(cart.name)
        .arg(cart.getItemCount())
        .arg(formatCurrency(cart.getSubtotal()))
        .arg(cart.createdTime.toString("hh:mm"));
}

// Cheap path for item-level changes: refresh the current tab's label in
// place. Full rebuilds (updateCartSelector) are reserved for cart
// creation/deletion/merge/switch.
void MainWindow::updateCurrentCartTabText()
{
    if (!cartTabWidget) return;
    Cart *cart = cartService->current();
    if (!cart) return;

    QTabBar *bar = cartTabWidget->tabBar();
    for (int i = 0; i < bar->count(); ++i) {
        if (bar->tabData(i).toInt() == cartService->currentId()) {
            bar->setTabText(i, cartTabLabel(*cart));
            cartTabWidget->setTabToolTip(i, cartTabTooltip(*cart));
            return;
        }
    }
}

void MainWindow::updateCartSelector()
{
    if (!cartTabWidget) return;

    cartTabWidget->blockSignals(true);

    while (cartTabWidget->count() > 0) {
        QWidget *page = cartTabWidget->widget(0);
        cartTabWidget->removeTab(0);
        delete page;
    }

    int currentIndex = -1;
    const QList<int> sortedIds = cartService->sortedIds();
    for (int cartId : sortedIds) {
        const Cart *cart = cartService->cartById(cartId);
        if (!cart) continue;

        QWidget *dummyPage = new QWidget(this);
        const int tabIdx = cartTabWidget->addTab(dummyPage, cartTabLabel(*cart));
        cartTabWidget->tabBar()->setTabData(tabIdx, cartId);
        cartTabWidget->setTabToolTip(tabIdx, cartTabTooltip(*cart));

        if (cartId == cartService->currentId()) currentIndex = tabIdx;
    }

    if (currentIndex >= 0)
        cartTabWidget->setCurrentIndex(currentIndex);

    cartTabWidget->blockSignals(false);
}

void MainWindow::onNewCart()
{
    const int cartId = cartService->createCart();
    cartService->switchTo(cartId);
    statusLabel->setText("New cart created");
}

void MainWindow::onSwitchCart(int index)
{
    if (index < 0 || !cartTabWidget) return;
    const int cartId = cartTabWidget->tabBar()->tabData(index).toInt();
    if (cartId > 0) cartService->switchTo(cartId);
}

void MainWindow::onTabCloseRequested(int index)
{
    if (index < 0 || cartService->count() <= 1) {
        QMessageBox::warning(this, "Cannot Close",
                             "You must keep at least one cart open.");
        return;
    }

    const int cartId = cartTabWidget->tabBar()->tabData(index).toInt();
    const Cart *cart = cartService->cartById(cartId);
    if (!cart) return;

    if (QMessageBox::question(
            this, "Confirm Delete",
            QString("Delete cart '%1'?\n\nItems: %2\nSubtotal: %3")
                .arg(cart->name)
                .arg(cart->getItemCount())
                .arg(formatCurrency(cart->getSubtotal())),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

        cartService->removeCart(cartId);
        statusLabel->setText("Cart deleted");
    }
}

void MainWindow::onDeleteCart()
{
    for (int i = 0; i < cartTabWidget->count(); ++i) {
        if (cartTabWidget->tabBar()->tabData(i).toInt()
            == cartService->currentId()) {
            onTabCloseRequested(i);
            return;
        }
    }
}

void MainWindow::onMergeCart()
{
    if (cartService->count() < 2) {
        QMessageBox::information(this, "Cannot Merge",
                                 "You need at least 2 carts to merge.");
        return;
    }

    QStringList cartNames;
    QList<int>  cartIds;
    const QList<int> ids = cartService->sortedIds();
    for (int id : ids) {
        if (id == cartService->currentId()) continue;
        const Cart *cart = cartService->cartById(id);
        if (!cart) continue;
        cartNames << QString("%1 (%2 items)")
                         .arg(cart->name)
                         .arg(cart->getItemCount());
        cartIds << id;
    }

    bool ok;
    const QString selected = QInputDialog::getItem(
        this, "Merge Cart",
        "Select cart to merge INTO current cart:",
        cartNames, 0, false, &ok);

    if (ok) {
        const int idx = cartNames.indexOf(selected);
        if (idx >= 0 && cartService->mergeInto(cartIds[idx])) {
            statusLabel->setText("Carts merged");
            QMessageBox::information(this, "Success",
                                     "Carts merged successfully!");
        }
    }
}

void MainWindow::onCartTabContextMenu(const QPoint &pos)
{
    QMenu contextMenu(tr("Cart Actions"), this);
    if (cartService->count() > 1) {
        connect(contextMenu.addAction("Delete Cart"),
                &QAction::triggered, this, &MainWindow::onDeleteCart);
        contextMenu.addSeparator();
        connect(contextMenu.addAction("Merge Cart"),
                &QAction::triggered, this, &MainWindow::onMergeCart);
    }
    contextMenu.exec(cartTabWidget->mapToGlobal(pos));
}

// =============================================================================
// Inventory slots
// =============================================================================

void MainWindow::onManageInventory()
{
    if (!checkPermission(this, Permission::VIEW_INVENTORY, "view inventory"))
        return;
    // Modeless + single instance: the cashier can keep inventory open while
    // ringing up sales instead of being locked out by a modal.
    if (m_inventoryDlg) {
        m_inventoryDlg->raise();
        m_inventoryDlg->activateWindow();
        return;
    }
    auto *dialog = new InventoryDialog(inventoryManager, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_inventoryDlg = dialog;
    connect(dialog, &QDialog::finished, this, [this](int) { loadProducts(); });
    dialog->show();
    UserManager::instance().logUserAction("Managed Inventory",
                                          "Opened inventory management");
}

void MainWindow::onAddProduct()
{
    if (!checkPermission(this, Permission::ADD_PRODUCTS, "add products"))
        return;
    InventoryDialog dlg(inventoryManager, this);
    dlg.exec();
    loadProducts();
    UserManager::instance().logUserAction("Add Product",
                                          "Opened inventory to add a product");
}

void MainWindow::onShowLowStock()
{
    // The dedicated triage dialog shows critical/low items in sortable tables
    // and offers one-click restock — far better than a plain-text dump.
    LowStockDialog dlg(inventoryManager, this);
    dlg.exec();
    loadProducts();
}

void MainWindow::onInventoryLow(int productId, const QString &productName,
                                int quantity)
{
    Q_UNUSED(productId)
    statusLabel->setText(
        QString("Low stock: %1 (%2 units)").arg(productName).arg(quantity));
}

void MainWindow::onInventoryCritical(int productId, const QString &productName,
                                     int quantity)
{
    Q_UNUSED(productId)
    statusLabel->setText(
        QString("CRITICAL: %1 (%2 units)").arg(productName).arg(quantity));
}

void MainWindow::onInventoryOutOfStock(int productId,
                                       const QString &productName)
{
    Q_UNUSED(productId)
    statusLabel->setText(
        QString("OUT OF STOCK: %1").arg(productName));
}

// =============================================================================
// Reports / analytics
// =============================================================================

void MainWindow::onShowAnalytics()
{
    if (!checkPermission(this, Permission::VIEW_ANALYTICS, "view analytics"))
        return;
    AnalyticsDashboard dashboard(this);
    dashboard.exec();
    UserManager::instance().logUserAction("Viewed Analytics",
                                          "Opened analytics dashboard");
}

void MainWindow::onShowReports()
{
    if (!checkPermission(this, Permission::VIEW_REPORTS, "view reports"))
        return;
    ReportsDialog dialog(this);
    dialog.exec();
    UserManager::instance().logUserAction("Viewed Reports",
                                          "Opened detailed reports dialog");
}

void MainWindow::onViewSalesHistory()
{
    if (!checkPermission(this, Permission::VIEW_SALES, "view sales history"))
        return;
    // Modeless + single instance: reference past sales mid-transaction.
    if (m_salesHistoryDlg) {
        m_salesHistoryDlg->raise();
        m_salesHistoryDlg->activateWindow();
        return;
    }
    auto *dialog = new SalesHistoryDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_salesHistoryDlg = dialog;
    // Refunds restore stock, so refresh the grid when it closes.
    connect(dialog, &QDialog::finished, this, [this](int) { loadProducts(); });
    dialog->show();
    UserManager::instance().logUserAction("Viewed Sales History",
                                          "Opened sales history dialog");
}

void MainWindow::onReprintReceipt()
{
    if (receiptPrinter->reprintLastReceipt()) {
        statusLabel->setText("Receipt reprinted successfully");
        UserManager::instance().logUserAction("Reprint Receipt",
                                              "Reprinted last receipt");
    } else {
        QMessageBox::warning(this, "Reprint Failed",
                             receiptPrinter->getLastError());
    }
}

void MainWindow::onEmailReceipt()
{
    Receipt receipt = receiptPrinter->getLastReceipt();
    if (receipt.saleId == 0) {
        QMessageBox::information(this, "No Receipt",
                                 "There is no recent receipt to email.");
        return;
    }

    bool ok = false;
    const QString email = QInputDialog::getText(
        this, "Email Receipt",
        QString("Send receipt #%1 to:").arg(receipt.saleId),
        QLineEdit::Normal, settingsManager->settings().email, &ok).trimmed();
    if (!ok || email.isEmpty())
        return;
    if (!email.contains('@')) {
        QMessageBox::warning(this, "Invalid Email",
                             "Please enter a valid email address.");
        return;
    }

    const SmtpConfig cfg = receiptPrinter->mailConfig();
    if (!cfg.isConfigured()) {
        QMessageBox::warning(this, "Email Not Set Up",
                             "Add your mail server under Settings → Company "
                             "Information before emailing receipts.");
        return;
    }

    // Send on a worker thread: SMTP can block for seconds on a slow server, and
    // the till must stay responsive. We snapshot everything the send needs (a
    // self-contained SmtpClient, the recipient, subject and rendered HTML) so
    // the worker never touches shared UI/printer state.
    const int     saleId  = receipt.saleId;
    const QString subject = QString("Receipt #%1 from %2")
                                .arg(saleId)
                                .arg(settingsManager->businessName());
    const QString html    = receiptPrinter->renderReceiptHtml(receipt);

    statusLabel->setText("Sending receipt to " + email + "…");

    using SendResult = QPair<bool, QString>;   // {ok, error}
    auto *watcher = new QFutureWatcher<SendResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, email, saleId]() {
        const SendResult res = watcher->result();
        watcher->deleteLater();
        if (res.first) {
            statusLabel->setText("Receipt emailed to " + email);
            UserManager::instance().logUserAction(
                "Email Receipt",
                QString("Emailed receipt #%1 to %2").arg(saleId).arg(email));
            QMessageBox::information(this, "Receipt Sent",
                                     "The receipt was emailed to " + email + ".");
        } else {
            statusLabel->setText("Email failed");
            QMessageBox::warning(this, "Email Failed",
                                 "Failed to email receipt: " + res.second);
        }
    });

    watcher->setFuture(QtConcurrent::run([cfg, email, subject, html]() -> SendResult {
        SmtpClient client(cfg);
        QString err;
        const bool ok = client.send(email, subject, html, &err);
        return { ok, err };
    }));
}

void MainWindow::onDailyReport()
{
    if (!checkPermission(this, Permission::VIEW_REPORTS, "view reports"))
        return;

    const Money  todaySales       = Database::instance().getTotalSalesToday();
    const int    todayTransactions =
        Database::instance().getTotalTransactionsToday();
    const Money  average = Money::fromCents(
        todayTransactions > 0 ? todaySales.cents() / todayTransactions : 0);

    // A small structured dialog instead of a plain-text QMessageBox: themed,
    // right-aligned figures, and the total picked out with the totals roles.
    QDialog dlg(this);
    dlg.setWindowTitle("Daily Report");
    QGridLayout *g = new QGridLayout(&dlg);
    g->setColumnStretch(1, 1);

    QLabel *title = new QLabel("Daily Report");
    title->setProperty("role", "dialogTitle");
    g->addWidget(title, 0, 0, 1, 2);

    QLabel *date = new QLabel(QDate::currentDate().toString("dddd, d MMMM yyyy"));
    date->setProperty("kind", "secondary");
    g->addWidget(date, 1, 0, 1, 2);

    auto addRow = [&](int row, const QString &label, const QString &value,
                      const char *role) {
        g->addWidget(new QLabel(label), row, 0);
        QLabel *v = new QLabel(value);
        v->setAlignment(Qt::AlignRight);
        if (role) v->setProperty("role", role);
        g->addWidget(v, row, 1);
    };
    addRow(2, "Transactions:", QString::number(todayTransactions), "amount");
    addRow(3, "Average sale:", formatCurrency(average), "amount");

    QFrame *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    g->addWidget(sep, 4, 0, 1, 2);

    QLabel *totalTitle = new QLabel("Total sales:");
    totalTitle->setProperty("role", "totalTitle");
    g->addWidget(totalTitle, 5, 0);
    QLabel *totalVal = new QLabel(formatCurrency(todaySales));
    totalVal->setProperty("role", "amountTotal");
    totalVal->setAlignment(Qt::AlignRight);
    g->addWidget(totalVal, 5, 1);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    g->addWidget(box, 6, 0, 1, 2);

    dlg.exec();

    UserManager::instance().logUserAction(
        "Viewed Daily Report",
        QString("Sales: %1, Transactions: %2")
            .arg(formatCurrency(todaySales)).arg(todayTransactions));
}

// =============================================================================
// Settings
// =============================================================================

void MainWindow::onManageSchedules()
{
    ScheduleDialog dlg(scheduleManager, this);
    dlg.exec();
}

void MainWindow::onCompanySettings()
{
    if (!checkPermission(this, Permission::ACCESS_SETTINGS, "access settings"))
        return;
    SettingsDialog dlg(settingsManager, this);
    if (dlg.exec() == QDialog::Accepted) {
        const BusinessSettings &bs = settingsManager->settings();
        receiptPrinter->setCompanyInfo(
            bs.businessName, bs.address, bs.phone, "");
        receiptPrinter->setReceiptFooter(bs.receiptFooter);
        receiptPrinter->setSmtpConfig(makeSmtpConfig(bs));
        reloadMessagingProviders(scheduleManager, this);
        statusLabel->setText("Settings saved.");
        UserManager::instance().logUserAction("Updated Settings",
                                              "Company settings changed");
    }
}

void MainWindow::onReceiptSettings()
{
    if (!checkPermission(this, Permission::ACCESS_SETTINGS, "access settings"))
        return;
    SettingsDialog dlg(settingsManager, this);
    if (dlg.exec() == QDialog::Accepted) {
        const BusinessSettings &bs = settingsManager->settings();
        receiptPrinter->setCompanyInfo(
            bs.businessName, bs.address, bs.phone, "");
        receiptPrinter->setReceiptFooter(bs.receiptFooter);
        receiptPrinter->setSmtpConfig(makeSmtpConfig(bs));
        reloadMessagingProviders(scheduleManager, this);
        statusLabel->setText("Receipt settings saved.");
        UserManager::instance().logUserAction("Updated Settings",
                                              "Receipt settings changed");
    }
}

void MainWindow::onBackupNow()
{
    if (!checkPermission(this, Permission::BACKUP_RESTORE, "back up the database"))
        return;

    QString path;
    if (Database::instance().backupTo(Database::instance().backupDirectory(), &path)) {
        Database::instance().rotateBackups(Database::instance().backupDirectory(), 10);
        statusLabel->setText("Backup created.");
        UserManager::instance().logUserAction("Backup", "Created manual backup: " + path);
        QMessageBox::information(this, "Backup Complete",
                                 QString("Database backed up to:\n%1").arg(path));
    } else {
        QMessageBox::warning(this, "Backup Failed",
                             Database::instance().getLastError());
    }
}

void MainWindow::onToggleTheme()
{
    // ThemeManager is the single source of truth: it persists the choice and
    // emits themeChanged(), which restyles this window AND every open dialog
    // (InventoryDialog, AnalyticsDashboard, ...) that listens to it.
    ThemeManager::instance().toggle();
}

void MainWindow::applyTheme()
{
    // Application-wide: dialogs (including parentless ones) inherit the sheet.
    qApp->setStyleSheet(appStylesheet(isDarkMode));
    QApplication::setPalette(appPalette(isDarkMode));
    themeAction->setText(isDarkMode ? "Toggle Light Mode"
                                    : "Toggle Dark Mode");
    statusLabel->setText(isDarkMode ? "Dark mode enabled"
                                    : "Light mode enabled");
}

// =============================================================================
// User management
// =============================================================================

void MainWindow::onUserManagement()
{
    if (!checkPermission(this, Permission::VIEW_USERS, "access user management"))
        return;
    UserManagementDialog dialog(this);
    dialog.exec();
}

void MainWindow::onChangePassword()
{
    ChangePasswordDialog dlg(ChangePasswordDialog::Mode::Voluntary, this);

    // Keep the dialog open on a wrong current password so the user can
    // correct it instead of starting over.
    while (dlg.exec() == QDialog::Accepted) {
        if (UserManager::instance().changeOwnPassword(dlg.currentPassword(),
                                                      dlg.newPassword())) {
            QMessageBox::information(this, "Success",
                                     "Password changed successfully!");
            UserManager::instance().logUserAction(
                "Change Password", "User changed their own password");
            return;
        }
        dlg.showError("Failed to change password. "
                      "Please check your current password.");
    }
}

void MainWindow::onLogout()
{
    if (QMessageBox::question(this, "Logout",
                              "Are you sure you want to logout?",
                              QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        UserManager::instance().logout();
        emit logoutRequested();   // main.cpp loops back to the login dialog
        close();
    }
}

// =============================================================================
// Help
// =============================================================================

void MainWindow::onAbout()
{
    QMessageBox::about(
        this, "About KeynetikPOS",
        QString("KeynetikPOS v1.0\n\n"
                "Professional Point of Sale System\n\n"
                "Features:\n"
                "  Sales Management\n"
                "  Barcode Scanner (USB HID & Serial)\n"
                "  Inventory Tracking\n"
                "  Analytics Dashboard\n"
                "  Receipt Printing\n"
                "  User Management & RBAC\n"
                "  Dark/Light Themes\n\n"
                "This application uses Qt %1, licensed under the GNU LGPL v3.\n"
                "Qt is a trademark of The Qt Company.\n"
                "https://www.qt.io").arg(qVersion()));
}

void MainWindow::onShowShortcuts()
{
    QMessageBox::information(
        this, "Keyboard Shortcuts",
        "Keyboard Shortcuts:\n\n"
        "Ctrl+N  —  New Sale (clears current cart)\n"
        "Ctrl+W  —  Clear Cart\n"
        "Ctrl+L  —  Logout\n"
        "Ctrl+Q  —  Exit\n"
        "F1      —  Show this help\n"
        "F2      —  Checkout\n"
        "F3      —  Search Products\n"
        "F4      —  Apply Discount\n"
        "F5      —  New Cart\n");
}

// =============================================================================
// Keyboard shortcuts
// =============================================================================

void MainWindow::setupKeyboardShortcuts()
{
    auto sc = [&](QKeySequence key, auto slot) {
        QShortcut *s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, slot);
    };

    sc(Qt::Key_F1, &MainWindow::onShowShortcuts);
    sc(Qt::Key_F2, &MainWindow::onCheckout);
    sc(Qt::Key_F4, &MainWindow::onApplyDiscount);
    sc(Qt::Key_F5, &MainWindow::onNewCart);
    // NOTE: Ctrl+N belongs to the File > New Sale menu action; binding it
    // here as well makes the sequence ambiguous and Qt fires neither.
    sc(QKeySequence(Qt::CTRL | Qt::Key_W), &MainWindow::onClearCart);

    QShortcut *f3 = new QShortcut(QKeySequence(Qt::Key_F3), this);
    connect(f3, &QShortcut::activated, [this]() {
        searchEdit->setFocus();
        searchEdit->selectAll();
    });
}

// =============================================================================
// Utility
// =============================================================================

QString MainWindow::formatCurrency(Money amount)
{
    return formatMoney(amount);
}

// -----------------------------------------------------------------------------
// showToast — animated overlay notification
// -----------------------------------------------------------------------------
void MainWindow::showToast(const QString &message, const QString &type)
{
    // Replace any visible toast so rapid scans don't stack overlapping labels.
    if (m_toast)
        m_toast->deleteLater();

    // Anchor to whatever the user is looking at: a modal dialog (checkout,
    // inventory) if one is open, otherwise the main window. Without this the
    // toast renders behind an open dialog and scan feedback is lost.
    QWidget *anchor = QApplication::activeModalWidget();
    if (!anchor) anchor = this;

    QLabel *toast = new QLabel(message, anchor);
    m_toast = toast;
    toast->setAlignment(Qt::AlignCenter);
    toast->setWindowFlags(
        Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    toast->setAttribute(Qt::WA_TranslucentBackground);
    toast->setAttribute(Qt::WA_ShowWithoutActivating);
    toast->setAttribute(Qt::WA_DeleteOnClose);

    const ColorScheme &scheme = ThemeManager::instance().scheme();
    const QString bgColor =
        (type == "success") ? scheme.accentPrimary :
            (type == "error")   ? scheme.error :
            (type == "warning") ? scheme.warning : scheme.accentSecondary;

    toast->setStyleSheet(
        QString("QLabel { background-color: %1; color: white; padding: 14px 28px; "
                "border-radius: 8px; font-size: 14px; font-weight: 600; }")
            .arg(bgColor));

    toast->adjustSize();
    // Centre near the bottom of the anchor widget.
    toast->move(anchor->mapToGlobal(
        QPoint((anchor->width() - toast->width()) / 2,
               anchor->height() - 120)));

    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(toast);
    toast->setGraphicsEffect(effect);

    QPropertyAnimation *fadeIn = new QPropertyAnimation(effect, "opacity");
    fadeIn->setDuration(POSConfig::TOAST_FADE_MS);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    toast->show();

    // Guard with a QPointer: the toast may already have been replaced (and
    // deleted) by a newer one before this fires.
    QPointer<QLabel> guard(toast);
    QTimer::singleShot(POSConfig::TOAST_DISPLAY_MS, [guard, effect]() {
        if (!guard) return;
        QPropertyAnimation *fadeOut = new QPropertyAnimation(effect, "opacity");
        fadeOut->setDuration(POSConfig::TOAST_FADE_MS);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        QTimer::singleShot(POSConfig::TOAST_FADE_MS, guard.data(),
                           &QLabel::deleteLater);
    });
}