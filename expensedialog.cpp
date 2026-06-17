// =============================================================================
// expensedialog.cpp — Implementation of ExpenseDialog (see expensedialog.h).
// =============================================================================
#include "expensedialog.h"
#include "database.h"
#include "usermanager.h"
#include "money.h"
#include <QHeaderView>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QDate>
#include <QGroupBox>

ExpenseDialog::ExpenseDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    loadExpenses();
    loadCategories();
}

void ExpenseDialog::setupUI()
{
    setWindowTitle("Expense Tracking");
    setMinimumSize(800, 550);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *title = new QLabel("Operating Expenses", this);
    title->setProperty("role", "sectionTitle");
    mainLayout->addWidget(title);

    tabs = new QTabWidget(this);

    // ── Expenses tab ──────────────────────────────────────────────────────
    QWidget *expTab = new QWidget();
    QVBoxLayout *expLayout = new QVBoxLayout(expTab);

    // Filter row
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("From:", expTab));
    fromDateEdit = new QDateEdit(QDate::currentDate().addDays(1 - QDate::currentDate().day()), expTab);
    fromDateEdit->setCalendarPopup(true);
    fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    filterLayout->addWidget(fromDateEdit);
    filterLayout->addWidget(new QLabel("To:", expTab));
    toDateEdit = new QDateEdit(QDate::currentDate(), expTab);
    toDateEdit->setCalendarPopup(true);
    toDateEdit->setDisplayFormat("yyyy-MM-dd");
    filterLayout->addWidget(toDateEdit);
    filterButton = new QPushButton("Filter", expTab);
    filterButton->setProperty("kind", "info");
    connect(filterButton, &QPushButton::clicked, this, &ExpenseDialog::onFilterClicked);
    filterLayout->addWidget(filterButton);
    filterLayout->addStretch();
    expLayout->addLayout(filterLayout);

    expenseTable = new QTableWidget(expTab);
    expenseTable->setColumnCount(5);
    expenseTable->setHorizontalHeaderLabels({"Date", "Category", "Amount", "Description", "Recorded By"});
    expenseTable->horizontalHeader()->setStretchLastSection(true);
    expenseTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    expenseTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    expenseTable->setAlternatingRowColors(true);
    expLayout->addWidget(expenseTable);

    QHBoxLayout *expFooter = new QHBoxLayout();
    totalLabel = new QLabel(this);
    totalLabel->setProperty("role", "amountTotal");
    expFooter->addWidget(totalLabel);
    expFooter->addStretch();
    addExpenseButton = new QPushButton("Record Expense", expTab);
    addExpenseButton->setProperty("kind", "primary");
    connect(addExpenseButton, &QPushButton::clicked, this, &ExpenseDialog::onAddExpenseClicked);
    expFooter->addWidget(addExpenseButton);
    expLayout->addLayout(expFooter);

    tabs->addTab(expTab, "Expenses");

    // ── Categories tab ─────────────────────────────────────────────────────
    QWidget *catTab = new QWidget();
    QVBoxLayout *catLayout = new QVBoxLayout(catTab);

    categoryTable = new QTableWidget(catTab);
    categoryTable->setColumnCount(2);
    categoryTable->setHorizontalHeaderLabels({"ID", "Category Name"});
    categoryTable->horizontalHeader()->setStretchLastSection(true);
    categoryTable->setColumnHidden(0, true);
    categoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    categoryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    categoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    catLayout->addWidget(categoryTable);

    QHBoxLayout *catFooter = new QHBoxLayout();
    addCategoryButton = new QPushButton("Add Category", catTab);
    addCategoryButton->setProperty("kind", "primary");
    connect(addCategoryButton, &QPushButton::clicked, this, &ExpenseDialog::onAddCategoryClicked);
    deactivateCategoryButton = new QPushButton("Remove", catTab);
    deactivateCategoryButton->setProperty("kind", "danger");
    connect(deactivateCategoryButton, &QPushButton::clicked, this, &ExpenseDialog::onDeactivateCategoryClicked);
    catFooter->addWidget(addCategoryButton);
    catFooter->addWidget(deactivateCategoryButton);
    catFooter->addStretch();
    catLayout->addLayout(catFooter);

    tabs->addTab(catTab, "Categories");
    mainLayout->addWidget(tabs);

    QPushButton *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout *bottom = new QHBoxLayout();
    bottom->addStretch();
    bottom->addWidget(closeBtn);
    mainLayout->addLayout(bottom);
}

void ExpenseDialog::loadExpenses()
{
    const QString from = fromDateEdit->date().toString("yyyy-MM-dd");
    const QString to   = toDateEdit->date().toString("yyyy-MM-dd");
    const QVector<Expense> expenses = Database::instance().getExpensesByDateRange(from, to);

    expenseTable->setRowCount(expenses.size());
    Money total;
    for (int row = 0; row < expenses.size(); ++row) {
        const Expense &e = expenses[row];
        expenseTable->setItem(row, 0, new QTableWidgetItem(e.date));
        expenseTable->setItem(row, 1, new QTableWidgetItem(e.categoryName));
        expenseTable->setItem(row, 2, new QTableWidgetItem(formatMoney(e.amount)));
        expenseTable->setItem(row, 3, new QTableWidgetItem(e.description));
        expenseTable->setItem(row, 4, new QTableWidgetItem(e.recordedBy));
        total += e.amount;
    }
    totalLabel->setText("Total: " + formatMoney(total));
}

void ExpenseDialog::loadCategories()
{
    const QVector<ExpenseCategory> cats = Database::instance().getAllExpenseCategories();
    categoryTable->setRowCount(cats.size());
    for (int row = 0; row < cats.size(); ++row) {
        categoryTable->setItem(row, 0, new QTableWidgetItem(QString::number(cats[row].id)));
        categoryTable->setItem(row, 1, new QTableWidgetItem(cats[row].name));
    }
}

void ExpenseDialog::onFilterClicked()
{
    loadExpenses();
}

void ExpenseDialog::onAddExpenseClicked()
{
    const QVector<ExpenseCategory> cats = Database::instance().getAllExpenseCategories();
    if (cats.isEmpty()) {
        QMessageBox::warning(this, "No Categories",
            "Add at least one expense category in the Categories tab first.");
        tabs->setCurrentIndex(1);
        return;
    }

    QDialog form(this);
    form.setWindowTitle("Record Expense");
    QFormLayout *layout = new QFormLayout(&form);

    QComboBox *catCombo = new QComboBox(&form);
    for (const ExpenseCategory &c : cats)
        catCombo->addItem(c.name, c.id);

    QDateEdit *dateEdit = new QDateEdit(QDate::currentDate(), &form);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");

    QDoubleSpinBox *amountSpin = new QDoubleSpinBox(&form);
    amountSpin->setRange(0.01, 10000000.0);
    amountSpin->setDecimals(2);
    amountSpin->setPrefix(currencySymbol() + " ");

    QLineEdit *descEdit = new QLineEdit(&form);
    descEdit->setPlaceholderText("e.g. Monthly rent");

    layout->addRow("Category:", catCombo);
    layout->addRow("Date:", dateEdit);
    layout->addRow("Amount:", amountSpin);
    layout->addRow("Description:", descEdit);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &form);
    connect(buttons, &QDialogButtonBox::accepted, &form, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &form, &QDialog::reject);
    layout->addRow(buttons);

    if (form.exec() != QDialog::Accepted)
        return;

    Expense e;
    e.categoryId  = catCombo->currentData().toInt();
    e.amount      = Money::fromMajor(amountSpin->value());
    e.description = descEdit->text().trimmed();
    e.date        = dateEdit->date().toString("yyyy-MM-dd");
    e.recordedBy  = UserManager::instance().getCurrentUsername();

    if (!Database::instance().addExpense(e)) {
        QMessageBox::critical(this, "Error",
            "Failed to record expense: " + Database::instance().getLastError());
        return;
    }
    loadExpenses();
}

void ExpenseDialog::onAddCategoryClicked()
{
    bool ok;
    const QString name = QInputDialog::getText(this, "Add Category",
        "Category name:", QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (!Database::instance().addExpenseCategory(name)) {
        QMessageBox::critical(this, "Error",
            "Failed to add category: " + Database::instance().getLastError());
        return;
    }
    loadCategories();
}

void ExpenseDialog::onDeactivateCategoryClicked()
{
    const auto selected = categoryTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "No Selection", "Select a category to remove.");
        return;
    }
    const int id = categoryTable->item(selected.first().row(), 0)->text().toInt();
    if (QMessageBox::question(this, "Remove Category", "Remove this expense category?")
        != QMessageBox::Yes) return;
    Database::instance().deactivateExpenseCategory(id);
    loadCategories();
}
