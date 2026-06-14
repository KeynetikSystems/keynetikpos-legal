// =============================================================================
// lowstockdialog.h — LowStockDialog: "what needs reordering NOW" popup
// -----------------------------------------------------------------------------
// WHAT: A focused reorder-triage view: two tables (Critical and Low stock)
//       with double-click or button-triggered restock.
// HOW:  Pulls getCriticalStockItems()/getLowStockItems() from
//       InventoryManager, colour-codes rows by severity, and opens a small
//       restock dialog that calls restockProduct().
// WHY:  Exists separately from InventoryDialog because reacting to a low-stock
//       warning shouldn't require navigating a four-tab workbench — this is
//       the one-click triage view MainWindow opens from alerts.
// =============================================================================
#ifndef LOWSTOCKDIALOG_H
#define LOWSTOCKDIALOG_H

#include <QDialog>
#include <QVector>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>

// Forward declarations
class InventoryManager;
struct InventoryInfo;

class LowStockDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LowStockDialog(InventoryManager *manager, QWidget *parent = nullptr);
    ~LowStockDialog();

private slots:
    void onRefreshClicked();
    void onRestockClicked();
    void onCloseClicked();
    void onCriticalItemDoubleClicked(int row, int column);
    void onLowStockItemDoubleClicked(int row, int column);

private:
    void setupUI();
    void loadInventoryData();
    void populateCriticalTable(const QVector<InventoryInfo> &items);
    void populateLowStockTable(const QVector<InventoryInfo> &items);
    void showRestockDialog(int productId, const QString &productName, int currentQty);

    QString getStatusColor(int quantity, int reorderLevel) const;
    QString formatDate(const QString &dateStr) const;

    // UI Components
    QLabel *titleLabel;
    QLabel *summaryLabel;

    QGroupBox *criticalGroupBox;
    QTableWidget *criticalTable;

    QGroupBox *lowStockGroupBox;
    QTableWidget *lowStockTable;

    QPushButton *refreshButton;
    QPushButton *restockButton;
    QPushButton *closeButton;

    // Data
    InventoryManager *inventoryManager;
};

#endif // LOWSTOCKDIALOG_H
