// =============================================================================
// stocktakedialog.h — StockTakeDialog: physical inventory count reconciliation
// -----------------------------------------------------------------------------
// WHAT: Lets a manager do a full physical count of stock. Each product row
//       shows the system quantity; the cashier types the physically counted
//       quantity. On submit, rows with a variance are reconciled via
//       updateStock() + logStockAdjustment() and a summary is shown.
// HOW:  Products are loaded from Database::getAllProducts(). Category filter
//       hides/shows rows without a DB round-trip. Each submission writes an
//       update + audit row per discrepant product in sequence (not a single
//       transaction — atomicity isn't critical here; each adjustment is
//       individually logged).
// WHY:  Stock-take is the standard method to correct accumulated shrinkage,
//       counting errors, and unrecorded loss. The variance column makes
//       discrepancies immediately visible before submitting.
// =============================================================================
#ifndef STOCKTAKEDIALOG_H
#define STOCKTAKEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

#include "database.h"
#include "usermanager.h"
#include "money.h"

class StockTakeDialog : public QDialog
{
    Q_OBJECT

    Database &m_db;   // injected app DB connection (not owned)

public:
    explicit StockTakeDialog(Database &db, QWidget *parent = nullptr);

private slots:
    void onCategoryChanged(const QString &category);
    void onSubmitCount();
    void onCellChanged(int row, int column);

private:
    void setupUI();
    void loadProducts(const QString &categoryFilter = QString());
    void updateVariance(int row);

    // ── Columns ─────────────────────────────────────────────────────────────
    enum Col { COL_ID = 0, COL_PRODUCT, COL_CATEGORY, COL_SYSTEM_QTY,
               COL_COUNTED_QTY, COL_VARIANCE, COL_COUNT };

    QComboBox    *categoryCombo  { nullptr };
    QTableWidget *table          { nullptr };
    QPushButton  *submitButton   { nullptr };
    QPushButton  *cancelButton   { nullptr };

    bool m_loading { false };   // guard against recursive cellChanged signals
};

#endif // STOCKTAKEDIALOG_H
