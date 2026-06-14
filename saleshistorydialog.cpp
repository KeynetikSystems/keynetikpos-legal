// =============================================================================
// saleshistorydialog.cpp — Implementation of SalesHistoryDialog (see
// saleshistorydialog.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Loads Sale rows via Database::getSalesByDateRange() and filters in memory
//    for the quick filters / payment method / text search.
//  - The refund path prompts for a reason, checks permission and
//    Database::isRefunded() (no double refunds), then calls processRefund(),
//    which records the refund and restores stock.
// =============================================================================
#include "saleshistorydialog.h"
#include "colorscheme.h"
#include "cart.h"          // formatMoney()
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>

SalesHistoryDialog::SalesHistoryDialog(QWidget *parent)
    : QDialog(parent)
    , selectedSaleId(-1)
{
    setWindowTitle("Sales History");
    resize(1200, 700);
    setupUI();
    loadSales();
}

SalesHistoryDialog::~SalesHistoryDialog()
{
}

void SalesHistoryDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Header
    QLabel *titleLabel = new QLabel("📊 Sales History & Transactions");
    titleLabel->setProperty("role", "dialogTitle");
    mainLayout->addWidget(titleLabel);

    // Filters section
    QGroupBox *filterGroup = new QGroupBox("Search & Filter");
    QVBoxLayout *filterLayout = new QVBoxLayout(filterGroup);

    // Date range
    QHBoxLayout *dateLayout = new QHBoxLayout();

    quickFilterCombo = new QComboBox();
    quickFilterCombo->addItems({"Today", "Yesterday", "Last 7 Days", "Last 30 Days",
                                "This Month", "Last Month", "Custom Range"});
    quickFilterCombo->setCurrentText("Last 7 Days");
    connect(quickFilterCombo, &QComboBox::currentTextChanged,
            this, &SalesHistoryDialog::onQuickFilterChanged);
    dateLayout->addWidget(new QLabel("Quick Filter:"));
    dateLayout->addWidget(quickFilterCombo);

    dateLayout->addWidget(new QLabel("From:"));
    startDateEdit = new QDateEdit();
    startDateEdit->setDate(QDate::currentDate().addDays(-7));
    startDateEdit->setCalendarPopup(true);
    connect(startDateEdit, &QDateEdit::dateChanged, this, &SalesHistoryDialog::onDateRangeChanged);
    dateLayout->addWidget(startDateEdit);

    dateLayout->addWidget(new QLabel("To:"));
    endDateEdit = new QDateEdit();
    endDateEdit->setDate(QDate::currentDate());
    endDateEdit->setCalendarPopup(true);
    connect(endDateEdit, &QDateEdit::dateChanged, this, &SalesHistoryDialog::onDateRangeChanged);
    dateLayout->addWidget(endDateEdit);

    filterLayout->addLayout(dateLayout);

    // Search and payment method
    QHBoxLayout *searchLayout = new QHBoxLayout();

    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("🔍 Search by sale ID...");
    searchLayout->addWidget(searchEdit);

    paymentMethodFilter = new QComboBox();
    paymentMethodFilter->addItems({"All Payment Methods", "Cash", "Card", "Mobile Money", "Multiple"});
    connect(paymentMethodFilter, &QComboBox::currentTextChanged, this, &SalesHistoryDialog::onDateRangeChanged);
    searchLayout->addWidget(paymentMethodFilter);

    searchBtn = new QPushButton("🔍 Search");
    searchBtn->setProperty("kind", "info");
    connect(searchBtn, &QPushButton::clicked, this, &SalesHistoryDialog::onSearchClicked);
    searchLayout->addWidget(searchBtn);

    filterLayout->addLayout(searchLayout);
    mainLayout->addWidget(filterGroup);

    // Summary section
    QGroupBox *summaryGroup = new QGroupBox("Summary");
    QHBoxLayout *summaryLayout = new QHBoxLayout(summaryGroup);

    // Cards inherit the app-wide QGroupBox styling; the value labels use the
    // shared statValue role + kind colours.
    auto makeStatLabel = [](const QString &text, const char *kind) -> QLabel* {
        QLabel *l = new QLabel(text);
        l->setProperty("role", "statValue");
        l->setProperty("kind", kind);
        l->setAlignment(Qt::AlignCenter);
        return l;
    };

    QGroupBox *salesCard = new QGroupBox("Total Sales");
    QVBoxLayout *salesLayout = new QVBoxLayout(salesCard);
    totalSalesLabel = makeStatLabel(formatMoney(Money()), "success");
    salesLayout->addWidget(totalSalesLabel);
    summaryLayout->addWidget(salesCard);

    QGroupBox *countCard = new QGroupBox("Transactions");
    QVBoxLayout *countLayout = new QVBoxLayout(countCard);
    transactionCountLabel = makeStatLabel("0", "info");
    countLayout->addWidget(transactionCountLabel);
    summaryLayout->addWidget(countCard);

    QGroupBox *avgCard = new QGroupBox("Average Sale");
    QVBoxLayout *avgLayout = new QVBoxLayout(avgCard);
    avgTransactionLabel = makeStatLabel(formatMoney(Money()), "tertiary");
    avgLayout->addWidget(avgTransactionLabel);
    summaryLayout->addWidget(avgCard);

    QGroupBox *taxCard = new QGroupBox("Total Tax");
    QVBoxLayout *taxLayout = new QVBoxLayout(taxCard);
    totalTaxLabel = makeStatLabel(formatMoney(Money()), "danger");
    taxLayout->addWidget(totalTaxLabel);
    summaryLayout->addWidget(taxCard);

    mainLayout->addWidget(summaryGroup);

    // Sales table
    salesTable = new QTableWidget();
    salesTable->setSortingEnabled(true);
    salesTable->setColumnCount(7);
    salesTable->setHorizontalHeaderLabels({"Sale ID", "Date & Time", "Payment", "Subtotal", "Tax", "Discount", "Total"});
    salesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    salesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    salesTable->verticalHeader()->setVisible(false);
    salesTable->setAlternatingRowColors(true);
    salesTable->horizontalHeader()->setStretchLastSection(true);

    connect(salesTable, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row >= 0) onSaleSelected(row);
    });
    mainLayout->addWidget(salesTable);

    // Action buttons
    QHBoxLayout *actionLayout = new QHBoxLayout();

    viewDetailsBtn = new QPushButton("👁️ View Details");
    viewDetailsBtn->setProperty("kind", "info");
    viewDetailsBtn->setEnabled(false);
    connect(viewDetailsBtn, &QPushButton::clicked, this, &SalesHistoryDialog::onViewDetailsClicked);
    actionLayout->addWidget(viewDetailsBtn);

    refundBtn = new QPushButton("💸 Process Refund");
    refundBtn->setProperty("kind", "danger");
    refundBtn->setEnabled(false);
    connect(refundBtn, &QPushButton::clicked, this, &SalesHistoryDialog::onRefundClicked);
    actionLayout->addWidget(refundBtn);

    actionLayout->addStretch();

    exportBtn = new QPushButton("📥 Export CSV");
    exportBtn->setProperty("kind", "primary");
    connect(exportBtn, &QPushButton::clicked, this, &SalesHistoryDialog::onExportClicked);
    actionLayout->addWidget(exportBtn);

    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet("QPushButton { padding: 10px 20px; }");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    actionLayout->addWidget(closeBtn);

    mainLayout->addLayout(actionLayout);
}

void SalesHistoryDialog::loadSales()
{
    QString startDate = startDateEdit->date().toString("yyyy-MM-dd");
    QString endDate = endDateEdit->date().toString("yyyy-MM-dd");

    sales = Database::instance().getSalesByDateRange(startDate, endDate);
    updateSalesTable();
    updateSummary();
}

void SalesHistoryDialog::updateSalesTable()
{
    const ColorScheme scheme = getColorScheme();
    salesTable->setSortingEnabled(false);
    salesTable->setRowCount(0);

    QString searchText = searchEdit->text();
    QString paymentFilter = paymentMethodFilter->currentText();

    for (const Sale &sale : sales) {
        // Apply search filter
        if (!searchText.isEmpty() && !QString::number(sale.id).contains(searchText)) {
            continue;
        }

        // Apply payment method filter
        if (paymentFilter != "All Payment Methods" && sale.paymentMethod != paymentFilter) {
            continue;
        }

        int row = salesTable->rowCount();
        salesTable->insertRow(row);

        // Sale ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString("#%1").arg(sale.id));
        idItem->setFont(QFont(idItem->font().family(), -1, QFont::Bold));
        salesTable->setItem(row, 0, idItem);

        // Date & Time
        salesTable->setItem(row, 1, new QTableWidgetItem(sale.saleDate));

        // Payment Method
        QString paymentIcon;
        if (sale.paymentMethod == "Cash") paymentIcon = "💵";
        else if (sale.paymentMethod == "Card") paymentIcon = "💳";
        else if (sale.paymentMethod == "Mobile Money") paymentIcon = "📱";
        else paymentIcon = "🔀";

        QTableWidgetItem *paymentItem = new QTableWidgetItem(paymentIcon + " " + sale.paymentMethod);
        salesTable->setItem(row, 2, paymentItem);

        // Subtotal
        salesTable->setItem(row, 3, new QTableWidgetItem(formatCurrency(sale.subtotal)));

        // Tax
        salesTable->setItem(row, 4, new QTableWidgetItem(formatCurrency(sale.tax)));

        // Discount
        QTableWidgetItem *discountItem = new QTableWidgetItem(formatCurrency(sale.discount));
        if (sale.discount.cents() > 0) {
            discountItem->setForeground(QBrush(QColor(scheme.warning)));
            discountItem->setFont(QFont(discountItem->font().family(), -1, QFont::Bold));
        }
        salesTable->setItem(row, 5, discountItem);

        // Total
        QTableWidgetItem *totalItem = new QTableWidgetItem(formatCurrency(sale.total));
        totalItem->setFont(QFont(totalItem->font().family(), -1, QFont::Bold));
        totalItem->setForeground(QBrush(QColor(scheme.success)));
        salesTable->setItem(row, 6, totalItem);

        // Visually mark refunded sales
        if (Database::instance().isRefunded(sale.id)) {
            for (int col = 0; col < salesTable->columnCount(); ++col) {
                if (auto *cell = salesTable->item(row, col)) {
                    cell->setBackground(QBrush(QColor(scheme.errorBg)));
                    cell->setForeground(QBrush(QColor(scheme.error)));
                }
            }
            paymentItem->setText("↩ REFUNDED");
        }
    }

    salesTable->resizeColumnsToContents();
    salesTable->setSortingEnabled(true);
}

void SalesHistoryDialog::updateSummary()
{
    Money totalSales;
    Money totalTax;
    int count = salesTable->rowCount();

    for (const Sale &sale : sales) {
        totalSales += sale.total;
        totalTax += sale.tax;
    }

    Money avgTransaction = Money::fromCents(count > 0 ? totalSales.cents() / count : 0);

    totalSalesLabel->setText(formatCurrency(totalSales));
    transactionCountLabel->setText(QString::number(count));
    avgTransactionLabel->setText(formatCurrency(avgTransaction));
    totalTaxLabel->setText(formatCurrency(totalTax));
}

void SalesHistoryDialog::showSaleDetails(int saleId)
{
    Sale sale = Database::instance().getSaleById(saleId);
    QVector<SaleItem> items = Database::instance().getSaleItems(saleId);

    QString details = QString("═══════════════════════════════\n")
                      + QString("         SALE DETAILS\n")
                      + QString("═══════════════════════════════\n\n")
                      + QString("Sale ID: #%1\n").arg(sale.id)
                      + QString("Date: %1\n").arg(sale.saleDate)
                      + QString("Payment: %1\n\n").arg(sale.paymentMethod)
                      + QString("ITEMS PURCHASED:\n")
                      + QString("───────────────────────────────\n");

    for (const SaleItem &item : items) {
        details += QString("%1 x %2\n").arg(item.quantity).arg(item.productName);
        details += QString("  @ %1 = %2\n\n")
                       .arg(formatCurrency(item.price))
                       .arg(formatCurrency(item.subtotal));
    }

    details += QString("───────────────────────────────\n")
               + QString("Subtotal:  %1\n").arg(formatCurrency(sale.subtotal))
               + QString("Tax (16%%): %1\n").arg(formatCurrency(sale.tax));

    if (sale.discount.cents() > 0) {
        details += QString("Discount:  -%1\n").arg(formatCurrency(sale.discount));
    }

    details += QString("═══════════════════════════════\n")
               + QString("TOTAL:     %1\n").arg(formatCurrency(sale.total))
               + QString("═══════════════════════════════\n");

    if (Database::instance().isRefunded(saleId)) {
        details += QString("\n⚠ THIS SALE HAS BEEN REFUNDED\n");
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Sale Details");
    msgBox.setText(details);
    msgBox.setStyleSheet("QLabel { font-family: 'Courier New', monospace; }");
    msgBox.exec();
}

void SalesHistoryDialog::onSearchClicked()
{
    updateSalesTable();
    updateSummary();
}

void SalesHistoryDialog::onViewDetailsClicked()
{
    if (selectedSaleId <= 0) {
        QMessageBox::warning(this, "No Selection", "Please select a sale first.");
        return;
    }

    showSaleDetails(selectedSaleId);
}

void SalesHistoryDialog::onRefundClicked()
{
    if (selectedSaleId <= 0) {
        QMessageBox::warning(this, "No Selection", "Please select a sale first.");
        return;
    }

    // Guard against double refund
    if (Database::instance().isRefunded(selectedSaleId)) {
        QMessageBox::information(this, "Already Refunded",
                                 QString("Sale #%1 has already been refunded.").arg(selectedSaleId));
        return;
    }

    Sale sale = Database::instance().getSaleById(selectedSaleId);
    QVector<SaleItem> items = Database::instance().getSaleItems(selectedSaleId);

    // Build a summary of items that will be returned
    QString itemList;
    for (const SaleItem &item : items)
        itemList += QString("  • %1 × %2  (%3)\n")
                        .arg(item.quantity)
                        .arg(item.productName)
                        .arg(formatCurrency(item.subtotal));

    QString msg = QString(
                      "Process full refund for Sale #%1?\n\n"
                      "Items to return to inventory:\n%2\n"
                      "Refund amount: %3\n\n"
                      "This action cannot be undone.")
                      .arg(sale.id)
                      .arg(itemList)
                      .arg(formatCurrency(sale.total));

    if (QMessageBox::question(this, "Process Refund", msg,
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Ask for a reason
    bool ok;
    QString reason = QInputDialog::getText(
        this, "Refund Reason",
        "Enter reason for refund (required):",
        QLineEdit::Normal, "Customer return", &ok);
    if (!ok || reason.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Reason Required",
                             "A reason is required to process a refund.");
        return;
    }

    // Pull processed-by from UserManager if available, else fallback
    QString processedBy = "Manager";

    if (Database::instance().processRefund(selectedSaleId, reason.trimmed(), processedBy)) {
        // Mark the row visually in the table
        const ColorScheme scheme = getColorScheme();
        for (int row = 0; row < salesTable->rowCount(); ++row) {
            QString idText = salesTable->item(row, 0)->text();
            if (idText.remove("#").toInt() == selectedSaleId) {
                for (int col = 0; col < salesTable->columnCount(); ++col) {
                    if (auto *cell = salesTable->item(row, col)) {
                        cell->setBackground(QBrush(QColor(scheme.errorBg)));
                        cell->setForeground(QBrush(QColor(scheme.error)));
                    }
                }
                // Update payment column to show refunded status
                if (auto *cell = salesTable->item(row, 2))
                    cell->setText("↩ REFUNDED");
                break;
            }
        }

        // Disable refund button so it can't be pressed again this session
        refundBtn->setEnabled(false);

        QMessageBox::information(this, "Refund Processed",
                                 QString("Refund for Sale #%1 processed successfully.\n\n"
                                         "Amount refunded: %2\n"
                                         "Items returned to inventory: %3\n"
                                         "Reason: %4")
                                     .arg(sale.id)
                                     .arg(formatCurrency(sale.total))
                                     .arg(items.size())
                                     .arg(reason));
    } else {
        QMessageBox::critical(this, "Refund Failed",
                              QString("Failed to process refund for Sale #%1.\n"
                                      "Please try again or contact support.").arg(selectedSaleId));
    }
}

void SalesHistoryDialog::onExportClicked()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Sales",
                                                    QString("Sales_%1.csv")
                                                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd")),
                                                    "CSV Files (*.csv)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Error", "Could not create export file.");
        return;
    }

    QTextStream out(&file);
    out << "Sale ID,Date,Payment Method,Subtotal,Tax,Discount,Total\n";

    for (const Sale &sale : sales) {
        out << sale.id << ","
            << sale.saleDate << ","
            << sale.paymentMethod << ","
            << sale.subtotal.toMajor() << ","
            << sale.tax.toMajor() << ","
            << sale.discount.toMajor() << ","
            << sale.total.toMajor() << "\n";
    }

    file.close();
    QMessageBox::information(this, "Export Complete",
                             QString("Sales exported to:\n%1").arg(filename));
}

void SalesHistoryDialog::onDateRangeChanged()
{
    loadSales();
}

void SalesHistoryDialog::onSaleSelected(int row)
{
    if (row < 0) return;

    QString idText = salesTable->item(row, 0)->text();
    selectedSaleId = idText.remove("#").toInt();

    viewDetailsBtn->setEnabled(true);

    // Disable refund if already refunded
    bool alreadyRefunded = Database::instance().isRefunded(selectedSaleId);
    refundBtn->setEnabled(!alreadyRefunded);
    refundBtn->setToolTip(alreadyRefunded ? "This sale has already been refunded" : "Process refund");
}

void SalesHistoryDialog::onQuickFilterChanged(const QString &filter)
{
    QDate now = QDate::currentDate();

    if (filter == "Today") {
        startDateEdit->setDate(now);
        endDateEdit->setDate(now);
    } else if (filter == "Yesterday") {
        startDateEdit->setDate(now.addDays(-1));
        endDateEdit->setDate(now.addDays(-1));
    } else if (filter == "Last 7 Days") {
        startDateEdit->setDate(now.addDays(-7));
        endDateEdit->setDate(now);
    } else if (filter == "Last 30 Days") {
        startDateEdit->setDate(now.addDays(-30));
        endDateEdit->setDate(now);
    } else if (filter == "This Month") {
        startDateEdit->setDate(QDate(now.year(), now.month(), 1));
        endDateEdit->setDate(now);
    } else if (filter == "Last Month") {
        QDate lastMonth = now.addMonths(-1);
        startDateEdit->setDate(QDate(lastMonth.year(), lastMonth.month(), 1));
        endDateEdit->setDate(QDate(lastMonth.year(), lastMonth.month(), lastMonth.daysInMonth()));
    }
    // Custom Range - don't change dates
}

QString SalesHistoryDialog::formatCurrency(Money amount)
{
    return formatMoney(amount);
}