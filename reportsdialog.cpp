// =============================================================================
// reportsdialog.cpp — Implementation of ReportsDialog (see reportsdialog.h for
// the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Each generate*Report() fills the shared QTableWidget AND the
//    QVector<QStringList> backing store, so exports don't re-query.
//  - CSV export streams the backing store; PDF export renders an HTML version
//    of the table through QTextDocument/QPrinter.
// =============================================================================
#include "reportsdialog.h"
#include "money.h"         // Money, formatMoney()
#include "colorscheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QPrinter>
#include <QPainter>
#include <QTextDocument>
#include <QApplication>
#include <QPalette>

// Plain two-decimal money string (no currency symbol) for table cells and CSV.
static QString money2(Money m) { return QString::number(m.toMajor(), 'f', 2); }

ReportsDialog::ReportsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Sales Reports & Analytics");
    resize(1000, 700);
    setupUI();
}

ReportsDialog::~ReportsDialog()
{
}

void ReportsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Top controls — styling comes from the app-wide stylesheet
    QGroupBox *controlsGroup = new QGroupBox("Report Settings");
    QHBoxLayout *controlsLayout = new QHBoxLayout(controlsGroup);

    controlsLayout->addWidget(new QLabel("Report Type:"));

    reportTypeCombo = new QComboBox();
    reportTypeCombo->addItem("Sales by Date Range");
    reportTypeCombo->addItem("Sales by Category");
    reportTypeCombo->addItem("Sales by Payment Method");
    reportTypeCombo->addItem("Top Selling Products");
    reportTypeCombo->addItem("Daily Sales Summary");
    reportTypeCombo->setMinimumWidth(200);
    controlsLayout->addWidget(reportTypeCombo);

    controlsLayout->addWidget(new QLabel("From:"));

    startDateEdit = new QDateEdit();
    startDateEdit->setDate(QDate::currentDate().addDays(-30));
    startDateEdit->setCalendarPopup(true);
    controlsLayout->addWidget(startDateEdit);

    controlsLayout->addWidget(new QLabel("To:"));

    endDateEdit = new QDateEdit();
    endDateEdit->setDate(QDate::currentDate());
    endDateEdit->setCalendarPopup(true);
    controlsLayout->addWidget(endDateEdit);

    generateBtn = new QPushButton("Generate Report");
    generateBtn->setProperty("kind", "primary");
    controlsLayout->addWidget(generateBtn);

    mainLayout->addWidget(controlsGroup);

    // Summary label
    summaryLabel = new QLabel();
    summaryLabel->setProperty("role", "banner");
    summaryLabel->setProperty("kind", "info");
    summaryLabel->setProperty("textScale", "lg");
    mainLayout->addWidget(summaryLabel);

    // Report table
    reportTable = new QTableWidget();
    reportTable->setAlternatingRowColors(true);
    reportTable->horizontalHeader()->setStretchLastSection(true);
    reportTable->verticalHeader()->setVisible(false);
    reportTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    reportTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(reportTable);

    // Export buttons
    QHBoxLayout *exportLayout = new QHBoxLayout();
    exportLayout->addStretch();

    exportCsvBtn = new QPushButton("Export to CSV");
    exportCsvBtn->setProperty("kind", "info");
    exportLayout->addWidget(exportCsvBtn);

    exportPdfBtn = new QPushButton("Export to PDF");
    exportPdfBtn->setProperty("kind", "danger");
    exportLayout->addWidget(exportPdfBtn);

    mainLayout->addLayout(exportLayout);

    // Connect signals
    connect(generateBtn, &QPushButton::clicked, this, &ReportsDialog::onGenerateReport);
    connect(exportPdfBtn, &QPushButton::clicked, this, &ReportsDialog::onExportToPDF);
    connect(exportCsvBtn, &QPushButton::clicked, this, &ReportsDialog::onExportToCSV);
    connect(reportTypeCombo, &QComboBox::currentIndexChanged, this, &ReportsDialog::onReportTypeChanged);
}

void ReportsDialog::onReportTypeChanged(int index)
{
    // Enable/disable date range based on report type
    bool useDateRange = (index == 0 || index == 4);
    startDateEdit->setEnabled(useDateRange);
    endDateEdit->setEnabled(useDateRange);
}

void ReportsDialog::onGenerateReport()
{
    currentReportType = reportTypeCombo->currentText();

    if (currentReportType == "Sales by Date Range") {
        generateSalesByDateReport();
    } else if (currentReportType == "Sales by Category") {
        generateSalesByCategoryReport();
    } else if (currentReportType == "Sales by Payment Method") {
        generateSalesByPaymentReport();
    } else if (currentReportType == "Top Selling Products") {
        generateTopSellingProductsReport();
    } else if (currentReportType == "Daily Sales Summary") {
        generateDailySalesReport();
    }
}

void ReportsDialog::generateSalesByDateReport()
{
    QString startDate = startDateEdit->date().toString("yyyy-MM-dd");
    QString endDate = endDateEdit->date().toString("yyyy-MM-dd");

    QVector<Sale> sales = Database::instance().getSalesByDateRange(startDate, endDate);

    reportTable->setColumnCount(6);
    reportTable->setHorizontalHeaderLabels({"Sale ID", "Date", "Subtotal", "Tax", "Discount", "Total"});
    reportTable->setRowCount(0);

    reportData.clear();
    Money totalSales;
    Money totalTax;
    Money totalDiscount;

    for (const Sale &sale : sales) {
        int row = reportTable->rowCount();
        reportTable->insertRow(row);

        reportTable->setItem(row, 0, new QTableWidgetItem(QString::number(sale.id)));
        reportTable->setItem(row, 1, new QTableWidgetItem(sale.saleDate));
        reportTable->setItem(row, 2, new QTableWidgetItem(money2(sale.subtotal)));
        reportTable->setItem(row, 3, new QTableWidgetItem(money2(sale.tax)));
        reportTable->setItem(row, 4, new QTableWidgetItem(money2(sale.discount)));
        reportTable->setItem(row, 5, new QTableWidgetItem(money2(sale.total)));

        totalSales += sale.total;
        totalTax += sale.tax;
        totalDiscount += sale.discount;

        QStringList rowData;
        rowData << QString::number(sale.id) << sale.saleDate
                << money2(sale.subtotal)
                << money2(sale.tax)
                << money2(sale.discount)
                << money2(sale.total);
        reportData.append(rowData);
    }

    summaryLabel->setText(QString("Total Sales: %1 | Total Tax: %2 | Total Discount: %3 | Transactions: %4")
                              .arg(formatMoney(totalSales),
                                   formatMoney(totalTax),
                                   formatMoney(totalDiscount))
                              .arg(sales.size()));
}

void ReportsDialog::generateSalesByCategoryReport()
{
    QVector<Sale> sales = Database::instance().getAllSales();
    QMap<QString, Money> categoryTotals;
    QMap<QString, int> categoryCount;

    for (const Sale &sale : sales) {
        QVector<SaleItem> items = Database::instance().getSaleItems(sale.id);
        for (const SaleItem &item : items) {
            Product product = Database::instance().getProductById(item.productId);
            categoryTotals[product.category] += item.subtotal;
            categoryCount[product.category]++;
        }
    }

    reportTable->setColumnCount(3);
    reportTable->setHorizontalHeaderLabels({"Category", "Total Sales", "Items Sold"});
    reportTable->setRowCount(0);

    reportData.clear();
    Money grandTotal;

    for (auto it = categoryTotals.begin(); it != categoryTotals.end(); ++it) {
        int row = reportTable->rowCount();
        reportTable->insertRow(row);

        reportTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        reportTable->setItem(row, 1, new QTableWidgetItem(money2(it.value())));
        reportTable->setItem(row, 2, new QTableWidgetItem(QString::number(categoryCount[it.key()])));

        grandTotal += it.value();

        QStringList rowData;
        rowData << it.key() << money2(it.value()) << QString::number(categoryCount[it.key()]);
        reportData.append(rowData);
    }

    summaryLabel->setText(QString("Total Sales Across All Categories: %1").arg(formatMoney(grandTotal)));
}

void ReportsDialog::generateSalesByPaymentReport()
{
    QVector<Sale> sales = Database::instance().getAllSales();
    QMap<QString, Money> paymentTotals;
    QMap<QString, int> paymentCount;

    for (const Sale &sale : sales) {
        paymentTotals[sale.paymentMethod] += sale.total;
        paymentCount[sale.paymentMethod]++;
    }

    reportTable->setColumnCount(3);
    reportTable->setHorizontalHeaderLabels({"Payment Method", "Total Sales", "Transactions"});
    reportTable->setRowCount(0);

    reportData.clear();
    Money grandTotal;

    for (auto it = paymentTotals.begin(); it != paymentTotals.end(); ++it) {
        int row = reportTable->rowCount();
        reportTable->insertRow(row);

        reportTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        reportTable->setItem(row, 1, new QTableWidgetItem(money2(it.value())));
        reportTable->setItem(row, 2, new QTableWidgetItem(QString::number(paymentCount[it.key()])));

        grandTotal += it.value();

        QStringList rowData;
        rowData << it.key() << money2(it.value()) << QString::number(paymentCount[it.key()]);
        reportData.append(rowData);
    }

    summaryLabel->setText(QString("Total Sales Across All Payment Methods: %1").arg(formatMoney(grandTotal)));
}

void ReportsDialog::generateTopSellingProductsReport()
{
    QVector<QPair<QString, int>> topProducts = Database::instance().getTopSellingProducts(20);

    reportTable->setColumnCount(2);
    reportTable->setHorizontalHeaderLabels({"Product Name", "Total Quantity Sold"});
    reportTable->setRowCount(0);

    reportData.clear();
    int totalQuantity = 0;

    for (const auto &product : topProducts) {
        int row = reportTable->rowCount();
        reportTable->insertRow(row);

        reportTable->setItem(row, 0, new QTableWidgetItem(product.first));
        reportTable->setItem(row, 1, new QTableWidgetItem(QString::number(product.second)));

        totalQuantity += product.second;

        QStringList rowData;
        rowData << product.first << QString::number(product.second);
        reportData.append(rowData);
    }

    summaryLabel->setText(QString("Top 20 Products | Total Items Sold: %1").arg(totalQuantity));
}

void ReportsDialog::generateDailySalesReport()
{
    QString startDate = startDateEdit->date().toString("yyyy-MM-dd");
    QString endDate = endDateEdit->date().toString("yyyy-MM-dd");

    QVector<Sale> sales = Database::instance().getSalesByDateRange(startDate, endDate);
    QMap<QString, Money> dailyTotals;
    QMap<QString, int> dailyCount;

    for (const Sale &sale : sales) {
        QString date = sale.saleDate.left(10); // Extract date part
        dailyTotals[date] += sale.total;
        dailyCount[date]++;
    }

    reportTable->setColumnCount(3);
    reportTable->setHorizontalHeaderLabels({"Date", "Total Sales", "Transactions"});
    reportTable->setRowCount(0);

    reportData.clear();
    Money grandTotal;

    for (auto it = dailyTotals.begin(); it != dailyTotals.end(); ++it) {
        int row = reportTable->rowCount();
        reportTable->insertRow(row);

        reportTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        reportTable->setItem(row, 1, new QTableWidgetItem(money2(it.value())));
        reportTable->setItem(row, 2, new QTableWidgetItem(QString::number(dailyCount[it.key()])));

        grandTotal += it.value();

        QStringList rowData;
        rowData << it.key() << money2(it.value()) << QString::number(dailyCount[it.key()]);
        reportData.append(rowData);
    }

    summaryLabel->setText(QString("Total Sales: %1 | Total Transactions: %2")
                              .arg(formatMoney(grandTotal))
                              .arg(sales.size()));
}

void ReportsDialog::onExportToCSV()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export to CSV", "", "CSV Files (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Failed", "Could not open file for writing.");
        return;
    }

    QTextStream out(&file);

    // Write headers
    for (int col = 0; col < reportTable->columnCount(); ++col) {
        out << reportTable->horizontalHeaderItem(col)->text();
        if (col < reportTable->columnCount() - 1) out << ",";
    }
    out << "\n";

    // Write data
    for (int row = 0; row < reportTable->rowCount(); ++row) {
        for (int col = 0; col < reportTable->columnCount(); ++col) {
            out << reportTable->item(row, col)->text();
            if (col < reportTable->columnCount() - 1) out << ",";
        }
        out << "\n";
    }

    file.close();
    QMessageBox::information(this, "Export Successful", "Report exported to CSV successfully!");
}

void ReportsDialog::onExportToPDF()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export to PDF", "", "PDF Files (*.pdf)");
    if (fileName.isEmpty()) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageOrientation(QPageLayout::Portrait);

    // PDFs are printed documents, not on-screen UI, so the page background
    // and body text stay white/black for print-friendliness regardless of
    // the app's theme (a Dark-theme textPrimary would be near-invisible on
    // white paper) — but the header accent is pulled from the active theme
    // so the export still feels branded/consistent with the on-screen UI.
    const ColorScheme &scheme = getColorScheme();
    QString html = "<html><head><style>"
                   "table { border-collapse: collapse; width: 100%; }"
                   "th, td { border: 1px solid black; padding: 8px; text-align: left; }"
                   "th { background-color: " + scheme.accentPrimary + "; color: white; }"
                   "h2 { color: #333333; }"
                   "</style></head><body>";

    html += "<h2>" + currentReportType + "</h2>";
    html += "<p>" + summaryLabel->text() + "</p>";
    html += "<table>";

    // Headers
    html += "<tr>";
    for (int col = 0; col < reportTable->columnCount(); ++col) {
        html += "<th>" + reportTable->horizontalHeaderItem(col)->text() + "</th>";
    }
    html += "</tr>";

    // Data
    for (int row = 0; row < reportTable->rowCount(); ++row) {
        html += "<tr>";
        for (int col = 0; col < reportTable->columnCount(); ++col) {
            html += "<td>" + reportTable->item(row, col)->text() + "</td>";
        }
        html += "</tr>";
    }

    html += "</table></body></html>";

    QTextDocument document;
    document.setHtml(html);
    document.print(&printer);

    QMessageBox::information(this, "Export Successful", "Report exported to PDF successfully!");
}
