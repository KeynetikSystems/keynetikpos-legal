// =============================================================================
// refunddialog.h — RefundDialog: process a sale refund
// -----------------------------------------------------------------------------
// WHAT: Modal dialog that loads a past sale by ID, shows its line items, and
//       lets an authorised cashier submit a reason-backed refund. Guards
//       against double-refunds and enforces the ADJUST_STOCK permission.
// HOW:  Loads sale header + items from the DB on demand; delegates to
//       Database::processRefund() which reverses stock and records the refund
//       row in one transaction.
// WHY:  Refunds are destructive — they restock items and credit the tender.
//       Requiring a reason and a permission check creates an audit trail and
//       prevents accidental reversal of valid sales.
// =============================================================================
#ifndef REFUNDDIALOG_H
#define REFUNDDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QTableWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

#include "database.h"
#include "usermanager.h"
#include "money.h"

class RefundDialog : public QDialog
{
    Q_OBJECT

    Database &m_db;   // injected app DB connection (not owned)

public:
    explicit RefundDialog(Database &db, QWidget *parent = nullptr);

private slots:
    void onLoadSale();
    void onProcessRefund();

private:
    void setupUI();
    void populateSaleInfo(const Sale &sale);
    void populateSaleItems(const QVector<SaleItem> &items);
    void setSaleLoaded(bool loaded);

    // ── Top: sale lookup ────────────────────────────────────────────────────
    QSpinBox    *saleIdSpin   { nullptr };
    QPushButton *loadButton   { nullptr };

    // ── Middle: sale info + items ───────────────────────────────────────────
    QGroupBox    *infoGroup       { nullptr };
    QLabel       *saleDateLabel   { nullptr };
    QLabel       *saleTotalLabel  { nullptr };
    QLabel       *paymentLabel    { nullptr };
    QLabel       *refundedLabel   { nullptr };   // shown when already refunded
    QTableWidget *itemsTable      { nullptr };

    // ── Bottom: reason + action ─────────────────────────────────────────────
    QGroupBox    *actionGroup     { nullptr };
    QLineEdit    *reasonEdit      { nullptr };
    QPushButton  *refundButton    { nullptr };
    QPushButton  *closeButton     { nullptr };

    // ── State ───────────────────────────────────────────────────────────────
    int  m_loadedSaleId { -1 };
};

#endif // REFUNDDIALOG_H
