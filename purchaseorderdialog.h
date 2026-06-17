// =============================================================================
// purchaseorderdialog.h — PurchaseOrderDialog (list) + NewPurchaseOrderDialog
// (create) for the purchasing/ERP module
// -----------------------------------------------------------------------------
// WHAT: PurchaseOrderDialog lists all purchase orders (Pending/Received/
//       Cancelled) with Receive/Cancel actions. NewPurchaseOrderDialog builds
//       one: pick a supplier, add product+qty+unit-cost lines, submit.
// HOW:  Both read/write through Database's supplier/PO methods. Receiving a
//       PO (Database::receivePurchaseOrder) is the only place purchase orders
//       touch stock — it increases stock_quantity AND updates cost_price per
//       line, inside one transaction.
// WHY:  This is the "other half" of inventory the POS never had: stock
//       arriving from a known supplier at a known cost, not just manual
//       stock_adjustments. Kept as two dialogs (not one with inline editing)
//       because creating a PO and managing existing ones are different
//       workflows with different lifetimes.
// =============================================================================
#ifndef PURCHASEORDERDIALOG_H
#define PURCHASEORDERDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

#include "database.h"   // PurchaseOrder, PurchaseOrderItem, Supplier, Product

// ─────────────────────────────────────────────────────────────────────────────
// NewPurchaseOrderDialog — build and submit one purchase order
// ─────────────────────────────────────────────────────────────────────────────
class NewPurchaseOrderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewPurchaseOrderDialog(QWidget *parent = nullptr);

private slots:
    void onAddLineClicked();
    void onRemoveLineClicked();
    void onCreateClicked();

private:
    void setupUI();
    void refreshTotal();

    QComboBox      *supplierCombo;
    QComboBox      *productCombo;
    QSpinBox        *qtySpin;
    QDoubleSpinBox  *unitCostSpin;
    QTableWidget    *lineTable;
    QLabel          *totalLabel;
    QPushButton     *addLineButton;
    QPushButton     *removeLineButton;
    QPushButton     *createButton;
    QPushButton     *cancelButton;

    QVector<Product> m_products;       // indexed in sync with productCombo
    QVector<PurchaseOrderItem> m_lines;
};

// ─────────────────────────────────────────────────────────────────────────────
// PurchaseOrderDialog — list + receive/cancel existing purchase orders
// ─────────────────────────────────────────────────────────────────────────────
class PurchaseOrderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PurchaseOrderDialog(QWidget *parent = nullptr);

private slots:
    void onNewOrderClicked();
    void onReceiveClicked();
    void onCancelOrderClicked();
    void onViewItemsClicked();

private:
    void setupUI();
    void loadOrders();
    int  selectedOrderId() const;

    QTableWidget *table;
    QPushButton  *newOrderButton;
    QPushButton  *receiveButton;
    QPushButton  *cancelOrderButton;
    QPushButton  *viewItemsButton;
    QPushButton  *closeButton;
};

#endif // PURCHASEORDERDIALOG_H
