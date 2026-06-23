// =============================================================================
// inventorydialog.h — InventoryDialog: the full inventory workbench
// -----------------------------------------------------------------------------
// WHAT: Four tabs — Overview (filter/search table + summary cards), Stock
//       Management (editable grid with add/delete rows, auto-save, undo/redo),
//       Alerts (low/critical lists with bulk restock), and Analytics (stock
//       value, fast/slow movers).
// HOW:  Edits happen directly in QTableWidget cells; onStockTableCellChanged
//       marks rows dirty, validateProductRow() checks them (highlighting
//       invalid cells with explanatory tooltips), and changes commit via
//       explicit Save or a QTimer-driven auto-save. A QUndoStack provides
//       undo/redo for stock edits. Live updates arrive by subscribing to
//       InventoryManager signals. CSV import/export, printing, keyboard
//       shortcuts, and toast notifications round it out.
// WHY:  Stock-take is the most data-entry-heavy task in a POS; spreadsheet-
//       style in-place editing with validation, auto-save, and undo is much
//       faster than one-modal-dialog-per-product, while validation + audit
//       logging keep bulk edits safe.
// =============================================================================
#ifndef INVENTORYDIALOG_H
#define INVENTORYDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QCheckBox>
#include <QTimer>
#include <QUndoStack>
#include <QMap>
#include "money.h"
#include "inventorymanager.h"

// Forward declarations
class QVBoxLayout;
class QHBoxLayout;
class QGroupBox;
class QCloseEvent;
class QKeyEvent;

enum class NotificationType {
    Success,
    Error,
    Warning,
    Info
};

class Database;

class InventoryDialog : public QDialog
{
    Q_OBJECT

    Database &m_db;   // injected app DB connection (not owned)

public:
    explicit InventoryDialog(Database &db, InventoryManager *manager, QWidget *parent = nullptr);
    ~InventoryDialog();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Main tab slots
    void onRefreshClicked();
    void onExportClicked();
    void onPrintClicked();
    void onCloseClicked();

    // Filter and search slots
    void onFilterChanged(const QString &filter);
    void onSearchTextChanged(const QString &text);
    void onProductSelected(int row);

    // Stock management slots
    void onStockTableCellChanged(int row, int column);
    void onAddNewItemClicked();
    void onDeleteItemClicked();
    void onSaveChangesClicked();
    void onAutoSaveToggled(bool enabled);

    // Alert tab slots
    void onExportAlertsClicked();
    void onBulkRestockClicked();

    // Auto-refresh
    void onAutoRefreshToggled(bool checked);

    // Inventory manager signals
    void onInventoryChanged();
    void onInventoryUpdated(int productId, int newQuantity);
    void onInventoryRestocked(int productId, int quantity);

    // Cell interaction
    void onCellDoubleClicked(int row, int column);

    // Additional functionality
    void onSortByChanged(int index);
    void onCategoryFilterChanged(const QString &category);
    void onQuickRestockClicked(int productId);
    void onQuickAdjustClicked(int productId);
    void onViewHistoryClicked(int productId);
    void onImportClicked();
    void onBulkAdjustClicked();
    void onUndoClicked();
    void onRedoClicked();

private:
    // Setup methods
    void setupUI();
    void setupOverviewTab();
    void setupStockManagementTab();
    void setupAlertsTab();
    void setupAnalyticsTab();
    void setupKeyboardShortcuts();
    void connectSignals();

    // Data methods
    void loadInventoryData();
    void updateProductsList();
    void updateAlertsTable();
    void updateStockManagementTable();
    void updateAnalytics();
    void updateSummaryCard(QLabel *label, int value);
    void updateLastUpdatedTime();

    // UI helper methods
    QWidget* createSummaryCard(const QString &title, QLabel **valueLabel, const QString &color);
    void showNotification(const QString &message, NotificationType type);
    QString formatCurrency(Money amount);
    QString formatQuantity(int quantity);
    QString generateTempBarcode();

    // Validation and highlighting
    bool validateProductRow(int row, QString &errorMessage);
    void highlightRowError(int row);
    void clearRowHighlight(int row);
    void highlightInvalidCell(int row, int column, const QString &error);
    void clearCellHighlight(int row, int column);

    // Helper methods
    int countChangedRows();
    void performAutoSave();
    void showSaveIndicator(const QString &message);
    void applyModernStyling();
    void updateStockLevels();
    void validateAndSaveProduct(int row);
    QString getStockTrendIcon(int productId);
    QColor getStatusColor(InventoryStatus status);
    QWidget* createStockLevelCell(const InventoryInfo &info);
    QWidget* createQuickActionCell(int productId);
    QWidget* createStatusBadge(InventoryStatus status);
    void bulkUpdateStatus();
    QVector<int> getSelectedProductIds();

    // Member variables
    InventoryManager *inventoryManager;

    // UI Components - Overview Tab
    QTabWidget *tabWidget;
    QTableWidget *inventoryTable;
    QLineEdit *searchEdit;
    QComboBox *filterCombo;
    QPushButton *refreshBtn;
    QPushButton *exportBtn;
    QPushButton *printBtn;
    QLabel *totalItemsLabel;
    QLabel *lowStockCountLabel;
    QLabel *criticalStockCountLabel;
    QLabel *outOfStockCountLabel;
    QLabel *resultsCountLabel;

    // UI Components - Stock Management Tab
    QTableWidget *stockManagementTable;
    QPushButton *addNewItemBtn;
    QPushButton *deleteItemBtn;
    QPushButton *saveChangesBtn;
    QCheckBox *autoSaveCheckbox;
    QLabel *unsavedChangesLabel;

    // UI Components - Alerts Tab
    QTableWidget *alertsTable;
    QLabel *alertsCountLabel;

    // UI Components - Analytics Tab
    QWidget *analyticsTab;
    QLabel *totalValueLabel;
    QLabel *avgStockLevelLabel;
    QLabel *fastMovingLabel;
    QLabel *slowMovingLabel;

    // UI Components - Bottom Bar
    QCheckBox *autoRefreshCheckBox;
    QLabel *lastUpdatedLabel;

    // Timers
    QTimer *autoSaveTimer;
    QTimer *autoRefreshTimer;

    // Undo/Redo
    QUndoStack *undoStack;

    // Data
    QVector<InventoryInfo> currentInventory;
    QMap<int, InventoryInfo> previousInventory;
    int selectedProductId;

    // State
    bool hasUnsavedChanges;
    bool autoSaveEnabled;
    int changesPending;
};

#endif // INVENTORYDIALOG_H
