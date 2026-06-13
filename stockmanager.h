// =============================================================================
// stockmanager.h — StockManager: single-screen stock management dialog
// -----------------------------------------------------------------------------
// WHAT: A dialog-driven stock workbench: toolbar (refresh / add / edit /
//       delete / restock / adjust / export CSV), a filterable product table,
//       and a summary strip (counts by status, total stock value).
// HOW:  Loads InventoryInfo rows through InventoryManager, filters in memory,
//       edits via small modal dialogs, and logs every manual change through
//       Database::adjustStockWithLog() so each adjustment has an audit row.
// WHY:  Functionally overlaps InventoryDialog; it survives as a simpler modal-
//       form alternative to InventoryDialog's in-place grid editing. A future
//       cleanup could consolidate the two.
// =============================================================================
#ifndef STOCKMANAGER_H
#define STOCKMANAGER_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSpinBox>
#include <QDateEdit>

// Forward declarations
class InventoryManager;
class QSqlDatabase;
struct InventoryInfo;

class StockManager : public QDialog
{
    Q_OBJECT

public:
    explicit StockManager(InventoryManager *invManager, QSqlDatabase &database, QWidget *parent = nullptr);
    ~StockManager();

private slots:
    // Toolbar actions
    void onRefreshClicked();
    void onAddProductClicked();
    void onEditProductClicked();
    void onDeleteProductClicked();
    void onRestockClicked();
    void onAdjustStockClicked();
    void onExportClicked();
    void onSearchTextChanged(const QString &text);
    void onFilterChanged(int index);

    // Table interactions
    void onProductDoubleClicked(int row, int column);
    void onSelectionChanged();

private:
    // UI Setup
    void setupUI();
    void createToolbar();
    void createFilterSection();
    void createStockTable();
    void createSummarySection();
    void createButtonPanel();

    // Data operations
    void loadAllProducts();
    void filterProducts();
    void updateSummary();
    void showProductDialog(int productId = -1);
    void showRestockDialog(int productId, const QString &productName, int currentQty);
    void showStockAdjustmentDialog(int productId, const QString &productName, int currentQty);

    // Helpers
    void populateTable(const QVector<InventoryInfo> &items);
    QString getStatusText(int quantity) const;
    QString getStatusColor(int quantity) const;
    QColor getStatusColorForRow(int quantity) const;
    void exportToCSV();

    // UI Components - Toolbar
    QLineEdit *searchBox;
    QComboBox *filterCombo;
    QPushButton *refreshButton;
    QPushButton *addButton;
    QPushButton *editButton;
    QPushButton *deleteButton;
    QPushButton *restockButton;
    QPushButton *adjustButton;
    QPushButton *exportButton;

    // UI Components - Main table
    QTableWidget *stockTable;

    // UI Components - Summary
    QLabel *totalProductsLabel;
    QLabel *totalStockLabel;
    QLabel *lowStockCountLabel;
    QLabel *criticalStockCountLabel;
    QLabel *outOfStockCountLabel;
    QLabel *totalValueLabel;

    // UI Components - Bottom buttons
    QPushButton *closeButton;

    // Data
    InventoryManager *inventoryManager;
    QSqlDatabase &db;
    QVector<InventoryInfo> allProducts;

    // State
    QString currentFilter;
    int selectedProductId;
};

#endif // STOCKMANAGER_H
