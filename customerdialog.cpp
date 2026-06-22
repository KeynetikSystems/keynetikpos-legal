// =============================================================================
// customerdialog.cpp — Implementation of CustomerDialog (see customerdialog.h).
// =============================================================================
#include "customerdialog.h"
#include "money.h"
#include <QHeaderView>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QInputDialog>

CustomerDialog::CustomerDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    loadCustomers();
}

void CustomerDialog::setupUI()
{
    setWindowTitle("Customer Accounts");
    setMinimumSize(1000, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *title = new QLabel("Customer Directory", this);
    title->setProperty("role", "sectionTitle");
    mainLayout->addWidget(title);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // ── Left pane: directory ───────────────────────────────────────────────
    QWidget *leftWidget = new QWidget(splitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);

    searchEdit = new QLineEdit(leftWidget);
    searchEdit->setPlaceholderText("Search by name or phone…");
    connect(searchEdit, &QLineEdit::textChanged, this, &CustomerDialog::onSearchChanged);
    leftLayout->addWidget(searchEdit);

    table = new QTableWidget(leftWidget);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"ID", "Name", "Phone", "Credit"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnHidden(0, true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    connect(table, &QTableWidget::itemSelectionChanged,
            this, &CustomerDialog::onCustomerSelected);
    connect(table, &QTableWidget::cellDoubleClicked, this, [this](int /*row*/, int /*col*/) {
        const int id = selectedCustomerId();
        if (id <= 0) return;
        m_selectedCustomer = Database::instance().getCustomerById(id);
        accept();
    });
    leftLayout->addWidget(table);

    QHBoxLayout *dirButtons = new QHBoxLayout();
    addButton = new QPushButton("Add", leftWidget);
    addButton->setProperty("kind", "primary");
    editButton = new QPushButton("Edit", leftWidget);
    editButton->setProperty("kind", "info");
    deactivateButton = new QPushButton("Deactivate", leftWidget);
    deactivateButton->setProperty("kind", "danger");
    connect(addButton, &QPushButton::clicked, this, &CustomerDialog::onAddClicked);
    connect(editButton, &QPushButton::clicked, this, &CustomerDialog::onEditClicked);
    connect(deactivateButton, &QPushButton::clicked, this, &CustomerDialog::onDeactivateClicked);
    dirButtons->addWidget(addButton);
    dirButtons->addWidget(editButton);
    dirButtons->addWidget(deactivateButton);
    dirButtons->addStretch();
    leftLayout->addLayout(dirButtons);

    splitter->addWidget(leftWidget);

    // ── Right pane: detail ─────────────────────────────────────────────────
    detailGroup = new QGroupBox("Customer Details", splitter);
    QVBoxLayout *detailLayout = new QVBoxLayout(detailGroup);

    detailName   = new QLabel(detailGroup);
    detailName->setProperty("role", "sectionTitle");
    detailPhone  = new QLabel(detailGroup);
    detailEmail  = new QLabel(detailGroup);
    detailPoints = new QLabel(detailGroup);
    detailPoints->setProperty("textScale", "md");
    detailCredit = new QLabel(detailGroup);
    detailCredit->setProperty("textScale", "md");
    detailCredit->setProperty("bold", "true");

    detailLayout->addWidget(detailName);
    detailLayout->addWidget(detailPhone);
    detailLayout->addWidget(detailEmail);
    detailLayout->addWidget(detailPoints);
    detailLayout->addWidget(detailCredit);

    topUpButton = new QPushButton("Add Store Credit", detailGroup);
    topUpButton->setProperty("kind", "info");
    topUpButton->setEnabled(false);
    connect(topUpButton, &QPushButton::clicked, this, &CustomerDialog::onTopUpCreditClicked);
    detailLayout->addWidget(topUpButton);

    QLabel *histTitle = new QLabel("Purchase History", detailGroup);
    histTitle->setProperty("role", "chip");
    histTitle->setProperty("kind", "secondary");
    detailLayout->addWidget(histTitle);

    historyTable = new QTableWidget(detailGroup);
    historyTable->setColumnCount(4);
    historyTable->setHorizontalHeaderLabels({"Date", "Total", "Method", "Items"});
    historyTable->horizontalHeader()->setStretchLastSection(true);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setAlternatingRowColors(true);
    detailLayout->addWidget(historyTable);

    splitter->addWidget(detailGroup);
    splitter->setSizes({350, 600});
    mainLayout->addWidget(splitter);

    QHBoxLayout *footerLayout = new QHBoxLayout();
    selectButton = new QPushButton("Select Customer", this);
    selectButton->setProperty("kind", "primary");
    selectButton->setEnabled(false);
    connect(selectButton, &QPushButton::clicked, this, [this]() {
        const int id = selectedCustomerId();
        if (id <= 0) return;
        m_selectedCustomer = Database::instance().getCustomerById(id);
        accept();
    });
    footerLayout->addWidget(selectButton);
    footerLayout->addStretch();
    closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    footerLayout->addWidget(closeButton);
    mainLayout->addLayout(footerLayout);

    clearDetail();
}

void CustomerDialog::loadCustomers(const QString &filter)
{
    const QVector<Customer> customers = Database::instance().getAllCustomers();
    table->setRowCount(0);
    for (const Customer &c : customers) {
        if (!filter.isEmpty()) {
            const bool matches = c.name.contains(filter, Qt::CaseInsensitive)
                              || c.phone.contains(filter, Qt::CaseInsensitive);
            if (!matches) continue;
        }
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::number(c.id)));
        table->setItem(row, 1, new QTableWidgetItem(c.name));
        table->setItem(row, 2, new QTableWidgetItem(c.phone));
        table->setItem(row, 3, new QTableWidgetItem(formatMoney(c.storeCredit)));
    }
}

void CustomerDialog::onSearchChanged(const QString &text)
{
    loadCustomers(text);
    clearDetail();
}

void CustomerDialog::onCustomerSelected()
{
    const int id = selectedCustomerId();
    if (id < 0) { clearDetail(); selectButton->setEnabled(false); return; }
    showCustomerDetail(id);
    selectButton->setEnabled(true);
}

void CustomerDialog::showCustomerDetail(int customerId)
{
    const Customer c = Database::instance().getCustomerById(customerId);
    if (c.id <= 0) { clearDetail(); return; }

    detailName->setText(c.name);
    detailPhone->setText("Phone: " + (c.phone.isEmpty() ? "-" : c.phone));
    detailEmail->setText("Email: " + (c.email.isEmpty() ? "-" : c.email));
    detailPoints->setText(QString("Loyalty Points: %1").arg(c.loyaltyPoints));
    detailCredit->setText("Store Credit: " + formatMoney(c.storeCredit));
    topUpButton->setEnabled(true);
    topUpButton->setProperty("_customerId", customerId);

    const QVector<Sale> history = Database::instance().getCustomerPurchaseHistory(customerId);
    historyTable->setRowCount(history.size());
    for (int row = 0; row < history.size(); ++row) {
        const Sale &s = history[row];
        historyTable->setItem(row, 0, new QTableWidgetItem(s.saleDate.toString("yyyy-MM-dd")));
        historyTable->setItem(row, 1, new QTableWidgetItem(formatMoney(s.total)));
        historyTable->setItem(row, 2, new QTableWidgetItem(s.paymentMethod));
        historyTable->setItem(row, 3, new QTableWidgetItem(QString("#%1").arg(s.id)));
    }
}

void CustomerDialog::clearDetail()
{
    detailName->clear();
    detailPhone->clear();
    detailEmail->clear();
    detailPoints->clear();
    detailCredit->clear();
    topUpButton->setEnabled(false);
    historyTable->setRowCount(0);
}

int CustomerDialog::selectedCustomerId() const
{
    const auto selected = table->selectionModel()->selectedRows();
    if (selected.isEmpty()) return -1;
    return table->item(selected.first().row(), 0)->text().toInt();
}

bool CustomerDialog::runCustomerForm(const QString &title, Customer *customer)
{
    QDialog form(this);
    form.setWindowTitle(title);
    QFormLayout *layout = new QFormLayout(&form);

    QLineEdit *nameEdit  = new QLineEdit(customer->name, &form);
    QLineEdit *phoneEdit = new QLineEdit(customer->phone, &form);
    QLineEdit *emailEdit = new QLineEdit(customer->email, &form);
    QLineEdit *addrEdit  = new QLineEdit(customer->address, &form);

    layout->addRow("Name:", nameEdit);
    layout->addRow("Phone:", phoneEdit);
    layout->addRow("Email:", emailEdit);
    layout->addRow("Address:", addrEdit);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &form);
    connect(buttons, &QDialogButtonBox::accepted, &form, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &form, &QDialog::reject);
    layout->addRow(buttons);

    if (form.exec() != QDialog::Accepted) return false;
    if (nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing Name", "Customer name is required.");
        return false;
    }
    customer->name    = nameEdit->text().trimmed();
    customer->phone   = phoneEdit->text().trimmed();
    customer->email   = emailEdit->text().trimmed();
    customer->address = addrEdit->text().trimmed();
    return true;
}

void CustomerDialog::onAddClicked()
{
    Customer c;
    if (!runCustomerForm("Add Customer", &c)) return;
    if (!Database::instance().addCustomer(c)) {
        QMessageBox::critical(this, "Error",
            "Failed to add customer: " + Database::instance().getLastError());
        return;
    }
    loadCustomers(searchEdit->text());
}

void CustomerDialog::onEditClicked()
{
    const int id = selectedCustomerId();
    if (id < 0) { QMessageBox::information(this, "No Selection", "Select a customer."); return; }
    Customer c = Database::instance().getCustomerById(id);
    if (!runCustomerForm("Edit Customer", &c)) return;
    if (!Database::instance().updateCustomer(c)) {
        QMessageBox::critical(this, "Error",
            "Failed to update customer: " + Database::instance().getLastError());
        return;
    }
    loadCustomers(searchEdit->text());
    showCustomerDetail(id);
}

void CustomerDialog::onDeactivateClicked()
{
    const int id = selectedCustomerId();
    if (id < 0) { QMessageBox::information(this, "No Selection", "Select a customer."); return; }
    if (QMessageBox::question(this, "Deactivate Customer",
            "Deactivate this customer? Their purchase history is preserved.")
        != QMessageBox::Yes) return;
    Database::instance().deactivateCustomer(id);
    loadCustomers(searchEdit->text());
    clearDetail();
}

void CustomerDialog::onTopUpCreditClicked()
{
    const int id = selectedCustomerId();
    if (id < 0) return;

    bool ok;
    const double amount = QInputDialog::getDouble(this, "Add Store Credit",
        "Amount to add (" + currencySymbol() + "):", 0.0, 0.0, 1000000.0, 2, &ok);
    if (!ok || amount <= 0.0) return;

    const Money delta = Money::fromMajor(amount);
    if (!Database::instance().adjustStoreCredit(id, delta, "Manual top-up")) {
        QMessageBox::critical(this, "Error",
            "Failed to add store credit: " + Database::instance().getLastError());
        return;
    }
    showCustomerDetail(id);
    loadCustomers(searchEdit->text());
}
