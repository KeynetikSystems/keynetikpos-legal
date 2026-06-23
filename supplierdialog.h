// =============================================================================
// supplierdialog.h — SupplierDialog: manage the supplier directory
// -----------------------------------------------------------------------------
// WHAT: A table of suppliers (name/contact/phone/email) with Add/Edit/
//       Deactivate actions. The supplier picker in PurchaseOrderDialog reads
//       straight from Database::getAllSuppliers(), so this dialog is the only
//       place suppliers are created/edited.
// HOW:  Add/Edit opens a small embedded QDialog form (built on the fly, not a
//       separate class — five fields doesn't earn its own file). Deactivate is
//       a soft delete (suppliers stay referenced by historical purchase
//       orders, so rows are never hard-deleted).
// WHY:  Suppliers are the first piece of the purchasing/ERP module — stock
//       arriving from a known source, as opposed to the anonymous manual
//       stock_adjustments the app already had.
// =============================================================================
#ifndef SUPPLIERDIALOG_H
#define SUPPLIERDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "database.h"   // Supplier

class SupplierDialog : public QDialog
{
    Q_OBJECT

    Database &m_db;   // injected app DB connection (not owned)

public:
    explicit SupplierDialog(Database &db, QWidget *parent = nullptr);

private slots:
    void onAddClicked();
    void onEditClicked();
    void onDeactivateClicked();

private:
    void setupUI();
    void loadSuppliers();
    int  selectedSupplierId() const;
    // Shared by Add/Edit: returns true if the user accepted the form, with
    // the edited fields written back into *supplier.
    bool runSupplierForm(const QString &title, Supplier *supplier);

    QTableWidget *table;
    QPushButton  *addButton;
    QPushButton  *editButton;
    QPushButton  *deactivateButton;
    QPushButton  *closeButton;
};

#endif // SUPPLIERDIALOG_H
