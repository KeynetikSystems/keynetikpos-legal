// =============================================================================
// supplierdialog.cpp — Implementation of SupplierDialog (see supplierdialog.h
// for the full WHAT/HOW/WHY).
// =============================================================================
#include "supplierdialog.h"
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>

SupplierDialog::SupplierDialog(Database &db, QWidget *parent)
    : QDialog(parent)
    , m_db(db)
{
    setupUI();
    loadSuppliers();
}

void SupplierDialog::setupUI()
{
    setWindowTitle("Suppliers");
    setMinimumSize(700, 450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *title = new QLabel("Supplier Directory", this);
    title->setProperty("role", "sectionTitle");
    mainLayout->addWidget(title);

    table = new QTableWidget(this);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"ID", "Name", "Contact", "Phone", "Email"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnHidden(0, true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    mainLayout->addWidget(table);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    addButton = new QPushButton("Add Supplier", this);
    addButton->setProperty("kind", "primary");
    editButton = new QPushButton("Edit", this);
    editButton->setProperty("kind", "info");
    deactivateButton = new QPushButton("Deactivate", this);
    deactivateButton->setProperty("kind", "danger");
    closeButton = new QPushButton("Close", this);

    connect(addButton, &QPushButton::clicked, this, &SupplierDialog::onAddClicked);
    connect(editButton, &QPushButton::clicked, this, &SupplierDialog::onEditClicked);
    connect(deactivateButton, &QPushButton::clicked, this, &SupplierDialog::onDeactivateClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deactivateButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);
}

void SupplierDialog::loadSuppliers()
{
    const QVector<Supplier> suppliers = m_db.getAllSuppliers();
    table->setRowCount(suppliers.size());
    for (int row = 0; row < suppliers.size(); ++row) {
        const Supplier &s = suppliers[row];
        table->setItem(row, 0, new QTableWidgetItem(QString::number(s.id)));
        table->setItem(row, 1, new QTableWidgetItem(s.name));
        table->setItem(row, 2, new QTableWidgetItem(s.contactPerson));
        table->setItem(row, 3, new QTableWidgetItem(s.phone));
        table->setItem(row, 4, new QTableWidgetItem(s.email));
    }
}

int SupplierDialog::selectedSupplierId() const
{
    const auto selected = table->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return -1;
    return table->item(selected.first().row(), 0)->text().toInt();
}

bool SupplierDialog::runSupplierForm(const QString &title, Supplier *supplier)
{
    QDialog form(this);
    form.setWindowTitle(title);
    QFormLayout *layout = new QFormLayout(&form);

    QLineEdit *nameEdit    = new QLineEdit(supplier->name, &form);
    QLineEdit *contactEdit = new QLineEdit(supplier->contactPerson, &form);
    QLineEdit *phoneEdit   = new QLineEdit(supplier->phone, &form);
    QLineEdit *emailEdit   = new QLineEdit(supplier->email, &form);
    QLineEdit *addressEdit = new QLineEdit(supplier->address, &form);

    layout->addRow("Name:", nameEdit);
    layout->addRow("Contact Person:", contactEdit);
    layout->addRow("Phone:", phoneEdit);
    layout->addRow("Email:", emailEdit);
    layout->addRow("Address:", addressEdit);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &form);
    connect(buttons, &QDialogButtonBox::accepted, &form, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &form, &QDialog::reject);
    layout->addRow(buttons);

    if (form.exec() != QDialog::Accepted)
        return false;

    if (nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing Name", "Supplier name is required.");
        return false;
    }

    supplier->name          = nameEdit->text().trimmed();
    supplier->contactPerson = contactEdit->text().trimmed();
    supplier->phone         = phoneEdit->text().trimmed();
    supplier->email         = emailEdit->text().trimmed();
    supplier->address       = addressEdit->text().trimmed();
    return true;
}

void SupplierDialog::onAddClicked()
{
    Supplier s;
    s.isActive = true;
    if (!runSupplierForm("Add Supplier", &s))
        return;
    if (!m_db.addSupplier(s)) {
        QMessageBox::critical(this, "Error",
            "Failed to add supplier: " + m_db.getLastError());
        return;
    }
    loadSuppliers();
}

void SupplierDialog::onEditClicked()
{
    const int id = selectedSupplierId();
    if (id < 0) {
        QMessageBox::information(this, "No Selection", "Select a supplier to edit.");
        return;
    }
    Supplier s = m_db.getSupplierById(id);
    if (!runSupplierForm("Edit Supplier", &s))
        return;
    if (!m_db.updateSupplier(s)) {
        QMessageBox::critical(this, "Error",
            "Failed to update supplier: " + m_db.getLastError());
        return;
    }
    loadSuppliers();
}

void SupplierDialog::onDeactivateClicked()
{
    const int id = selectedSupplierId();
    if (id < 0) {
        QMessageBox::information(this, "No Selection", "Select a supplier to deactivate.");
        return;
    }
    if (QMessageBox::question(this, "Deactivate Supplier",
            "Deactivate this supplier? It will no longer appear when creating "
            "new purchase orders, but existing orders are unaffected.")
        != QMessageBox::Yes)
        return;
    if (!m_db.deactivateSupplier(id)) {
        QMessageBox::critical(this, "Error",
            "Failed to deactivate supplier: " + m_db.getLastError());
        return;
    }
    loadSuppliers();
}
