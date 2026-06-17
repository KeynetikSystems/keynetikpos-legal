#pragma once

// -----------------------------------------------------------------------------
// mainwindow.h — MainWindow: the central POS screen and application hub
// -----------------------------------------------------------------------------
// WHAT: MainWindow builds the POS screen — product grid with search/category
//       filter, the multi-cart tab bar, the cart table, totals panel,
//       PIN-gated discounts, checkout, and the menu bar that launches every
//       other dialog. It owns the subsystem objects (InventoryManager,
//       ReceiptPrinter, ScheduleManager, SettingsManager, BarcodeReader) and
//       the two services that hold the business logic.
// HOW:  The UI is built entirely in code (setupUI() and helpers). Cart state
//       lives in CartService (cart.h/cartservice.h); the cart table is a
//       QTableView backed by CartModel — no per-row widgets. Checkout
//       delegates to CheckoutService after PaymentDialog collects payment.
//       Theming comes from appstyle.h, driven by ThemeManager's themeChanged.
//       Barcode scans arrive via onBarcodeScanned() and add the matching
//       product to the current cart. updateUIPermissions() enables/disables
//       actions from UserManager::hasPermission().
// WHY:  MainWindow used to own the cart state machine, the checkout pipeline,
//       the theme stylesheets, and the messaging-provider wiring — a god
//       object. Those now live in cartservice/checkoutservice/appstyle/
//       providersetup; this class is reduced to widget construction and
//       dialog orchestration. Permission-driven UI disabling complements
//       (not replaces) the role checks in managers.
// -----------------------------------------------------------------------------

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTableView>
#include <QListView>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QTabWidget>
#include <QTimer>
#include <QModelIndex>
#include <QVector>
#include <QPointer>

#include "database.h"
#include "inventorymanager.h"
#include "receiptprinter.h"
#include "schedulemanager.h"
#include "settingsmanager.h"
#include "usermanager.h"
#include "colorscheme.h"
#include "cart.h"
#include "BarcodeReader.h"

class CartService;
class CartModel;
class CheckoutService;
class ProductGridModel;
class ProductFilterProxy;
class InventoryDialog;
class SalesHistoryDialog;
class QStackedWidget;

// -----------------------------------------------------------------------------
// MainWindow
// -----------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    // Emitted after the user confirms logout; main.cpp reacts by returning
    // to the login dialog (no process restart).
    void logoutRequested();

private:
    // ── Sub-system objects ──────────────────────────────────────────────────
    InventoryManager *inventoryManager  { nullptr };
    ReceiptPrinter   *receiptPrinter    { nullptr };
    ScheduleManager  *scheduleManager   { nullptr };
    SettingsManager  *settingsManager   { nullptr };
    BarcodeReader    *m_barcodeReader   { nullptr };

    // ── Services ────────────────────────────────────────────────────────────
    CartService      *cartService       { nullptr };
    CheckoutService  *checkoutService   { nullptr };


    // ── UI — products panel (model/view, see productgridmodel.h) ───────────
    QGroupBox          *productsPanel  { nullptr };
    QStackedWidget     *productStack   { nullptr };  // grid <-> empty placeholder
    QListView          *productView    { nullptr };
    QLabel             *productEmpty    { nullptr };
    ProductGridModel   *productModel   { nullptr };
    ProductFilterProxy *productProxy   { nullptr };
    QLineEdit          *searchEdit     { nullptr };
    QComboBox          *categoryCombo  { nullptr };

    // ── UI — cart panel ─────────────────────────────────────────────────────
    QGroupBox    *cartPanel         { nullptr };
    QWidget      *cartSelectorWidget{ nullptr };
    QTabWidget   *cartTabWidget     { nullptr };
    QStackedWidget *cartStack       { nullptr };  // table <-> empty placeholder
    QTableView   *cartTable         { nullptr };
    QLabel       *cartEmpty         { nullptr };
    CartModel    *cartModel         { nullptr };
    QGroupBox    *totalsGroup       { nullptr };
    QVBoxLayout  *actionsLayout     { nullptr };

    // ── UI — totals labels ──────────────────────────────────────────────────
    QLabel *subtotalLabel   { nullptr };
    QLabel *taxTitleLabel   { nullptr };
    QLabel *taxLabel        { nullptr };
    QLabel *discountLabel   { nullptr };
    QLabel *totalLabel      { nullptr };

    // ── UI — action buttons ─────────────────────────────────────────────────
    QPushButton *discountButton { nullptr };
    QPushButton *checkoutBtn    { nullptr };

    // ── UI — status bar ─────────────────────────────────────────────────────
    QLabel *statusLabel     { nullptr };
    QLabel *dateTimeLabel   { nullptr };
    QLabel *currentUserLabel{ nullptr };

    // ── UI — menu actions ───────────────────────────────────────────────────
    QActionGroup *themeGroup        { nullptr };  // one checkable action per AppTheme
    QAction *analyticsButton        { nullptr };
    QAction *reportsButton          { nullptr };
    QAction *inventoryButton        { nullptr };
    QAction *userManagementAction   { nullptr };
    QAction *changePasswordAction   { nullptr };
    QAction *logoutAction           { nullptr };

    // ── Setup helpers ───────────────────────────────────────────────────────
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupProductsPanel();
    void setupCartPanel();
    void setupCartSelector();
    void setupTotalsSection();
    void setupActionButtons();
    void setupKeyboardShortcuts();
    void setupUserInterface();
    void updateUIPermissions();
    void showCurrentUserInfo();

    // ── Barcode reader init ─────────────────────────────────────────────────
    void initBarcodeReader();

    // Non-critical startup work (messaging/schedule, inventory polling, barcode
    // scanner) run one event-loop tick after show() so the cashier sees the
    // product grid immediately instead of waiting on a (possibly blocking)
    // serial-port open or provider wiring.
    void runDeferredStartup();

    // ── Data helpers ────────────────────────────────────────────────────────
    void loadProducts();
    void updateTotals();
    void refreshTaxTitle();
    void updateCartSelector();
    void updateCurrentCartTabText();   // cheap label refresh, no tab rebuild
    QString cartTabLabel(const Cart &cart) const;
    QString cartTabTooltip(const Cart &cart) const;
    void refreshCartDisplay();
    void applyTheme();

    // ── Cart helpers ────────────────────────────────────────────────────────
    void addToCart(const Product &product, int quantity);

    // ── Utility ─────────────────────────────────────────────────────────────
    static QString formatCurrency(Money amount);
    void    showToast(const QString &message, const QString &type = "info");
    // Only one toast at a time; a new scan replaces the previous one so rapid
    // scans don't stack overlapping labels.
    QPointer<QLabel> m_toast;

    // Modeless reference dialogs — single instance each, so the cashier can
    // keep inventory / sales history open while ringing up a sale.
    QPointer<InventoryDialog>    m_inventoryDlg;
    QPointer<SalesHistoryDialog> m_salesHistoryDlg;

    // Active customer for the current checkout (cleared after each sale)
    Customer    m_selectedCustomer;
    bool        m_hasSelectedCustomer { false };
    QPushButton *customerLabel        { nullptr };   // doubles as selector button
    QPushButton *clearCustomerButton  { nullptr };

    // Toggle the grid/cart between content and an empty-state placeholder.
    void updateProductEmptyState();
    void updateCartEmptyState();

private slots:
    // ── Barcode ─────────────────────────────────────────────────────────────
    void onBarcodeScanned(const QString &barcode);
    void onBarcodeError(const QString &error);

    // ── Products ────────────────────────────────────────────────────────────
    void onProductCardClicked(const QModelIndex &index);

    // ── Cart ────────────────────────────────────────────────────────────────
    void onNewCart();
    void onSwitchCart(int index);
    void onTabCloseRequested(int index);
    void onDeleteCart();
    void onMergeCart();
    void onCartTabContextMenu(const QPoint &pos);
    void onCartContentChanged();
    void onCartTableClicked(const QModelIndex &index);
    void onClearCart();
    void onApplyDiscount();
    void onCheckout();
    void onNewSale();

    // ── Inventory ───────────────────────────────────────────────────────────
    void onManageInventory();
    void onAddProduct();
    void onShowLowStock();
    void onInventoryLow(int productId, const QString &productName, int quantity);
    void onInventoryCritical(int productId, const QString &productName, int quantity);
    void onInventoryOutOfStock(int productId, const QString &productName);

    // ── Purchasing (suppliers / purchase orders) ────────────────────────────
    void onManageSuppliers();
    void onManagePurchaseOrders();

    // ── Customer selection (cart panel) ─────────────────────────────────────
    void onSelectCustomer();
    void onClearCustomer();

    // ── Sales / reports ─────────────────────────────────────────────────────
    void onViewSalesHistory();
    void onReprintReceipt();
    void onEmailReceipt();
    void onShowAnalytics();
    void onDailyReport();
    void onShowReports();

    // ── Settings ────────────────────────────────────────────────────────────
    void onCompanySettings();
    void onReceiptSettings();
    void onManageSchedules();
    void onBackupNow();

    // ── User ────────────────────────────────────────────────────────────────
    void onUserManagement();
    void onChangePassword();
    void onLogout();

    // ── Help ────────────────────────────────────────────────────────────────
    void onAbout();
    void onShowShortcuts();
};
