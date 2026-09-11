// =============================================================================
// zreportdialog.cpp — Implementation of ZReportDialog (see zreportdialog.h for
// the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - buildReport() aggregates sales/sale_items for the chosen date (optionally
//    one shift) into ZReportData; expected cash = opening float + net cash
//    sales, variance = counted closing float - expected.
//  - renderHtml() feeds the preview pane and the print/export actions.
//  - The static generateZReportText() renders the SAME data as plain text with
//    no UI — ScheduleManager calls it to build SMS/WhatsApp report bodies, so
//    the scheduled report can never drift from the on-screen one.
// =============================================================================
#include "zreportdialog.h"
#include "settingsmanager.h"
#include "shiftmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDebug>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QDateTime>
#include <QDoubleSpinBox>

ZReportDialog::ZReportDialog(QSqlDatabase &db,
                             SettingsManager *settings,
                             ShiftManager    *shifts,
                             QWidget *parent)
    : QDialog(parent), m_db(db), m_settings(settings), m_shifts(shifts)
{
    setWindowTitle("Z-Report — End of Day");
    setMinimumSize(700, 650);
    setupUi();
    generateReport();
}

void ZReportDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // ── Controls ─────────────────────────────────────────
    auto *controlGroup = new QGroupBox("Report Options");
    auto *controlLayout = new QHBoxLayout(controlGroup);

    controlLayout->addWidget(new QLabel("Date:"));
    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");
    controlLayout->addWidget(m_dateEdit);

    controlLayout->addSpacing(15);
    controlLayout->addWidget(new QLabel("Shift:"));
    m_shiftCombo = new QComboBox(this);
    m_shiftCombo->addItem("All Shifts Today", -1);
    for (const ShiftRecord &s : m_shifts->getShiftHistory(1)) {
        m_shiftCombo->addItem(
            QString("Shift #%1 — %2 (%3)")
                .arg(s.shiftId).arg(s.cashierName)
                .arg(s.openedAt.toString("HH:mm")),
            s.shiftId);
    }
    controlLayout->addWidget(m_shiftCombo);
    controlLayout->addStretch();

    auto *genBtn = new QPushButton("Generate", this);
    genBtn->setProperty("kind", "info");
    connect(genBtn, &QPushButton::clicked, this, &ZReportDialog::generateReport);
    controlLayout->addWidget(genBtn);

    mainLayout->addWidget(controlGroup);

    // ── Cash-Up / Till Reconciliation ────────────────────
    const QString sym = m_settings->currencySymbol();
    auto *cashGroup   = new QGroupBox("Till Reconciliation (Cash-Up)");
    auto *cashLayout  = new QHBoxLayout(cashGroup);

    cashLayout->addWidget(new QLabel("Opening Float:"));
    m_openingFloatSpin = new QDoubleSpinBox(this);
    m_openingFloatSpin->setRange(0, 9999999);
    m_openingFloatSpin->setDecimals(2);
    m_openingFloatSpin->setPrefix(sym + " ");
    // Pre-fill from current open shift if one exists
    if (m_shifts->isShiftOpen())
        m_openingFloatSpin->setValue(m_shifts->currentShift().openingFloat);
    cashLayout->addWidget(m_openingFloatSpin);

    cashLayout->addSpacing(20);
    cashLayout->addWidget(new QLabel("Counted Cash in Drawer:"));
    m_countedCashSpin = new QDoubleSpinBox(this);
    m_countedCashSpin->setRange(0, 9999999);
    m_countedCashSpin->setDecimals(2);
    m_countedCashSpin->setPrefix(sym + " ");
    cashLayout->addWidget(m_countedCashSpin);

    cashLayout->addSpacing(20);
    auto *signOffBtn = new QPushButton("Sign Off & Record", this);
    signOffBtn->setProperty("kind", "primary");
    connect(signOffBtn, &QPushButton::clicked, this, &ZReportDialog::signOff);
    cashLayout->addWidget(signOffBtn);

    cashLayout->addStretch();
    mainLayout->addWidget(cashGroup);

    // ── Report View ───────────────────────────────────────
    m_reportView = new QTextEdit(this);
    m_reportView->setReadOnly(true);
    m_reportView->setFont(QFont("Courier New", 10));
    mainLayout->addWidget(m_reportView, 1);

    // ── Action Buttons ────────────────────────────────────
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_exportBtn = new QPushButton("Export HTML", this);
    m_exportBtn->setProperty("kind", "primary");
    connect(m_exportBtn, &QPushButton::clicked, this, &ZReportDialog::exportReport);
    btnLayout->addWidget(m_exportBtn);

    m_printBtn = new QPushButton("Print", this);
    connect(m_printBtn, &QPushButton::clicked, this, &ZReportDialog::printReport);
    btnLayout->addWidget(m_printBtn);

    auto *closeBtn = new QPushButton("Close", this);
    closeBtn->setProperty("kind", "danger");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);

    mainLayout->addLayout(btnLayout);
}

void ZReportDialog::generateReport()
{
    QDate date      = m_dateEdit->date();
    int   shiftId   = m_shiftCombo->currentData().toInt();

    m_lastReport = buildReport(date, shiftId);
    m_reportView->setHtml(renderHtml(m_lastReport));
}

ZReportData ZReportDialog::buildReport(const QDate &date, int shiftId)
{
    ZReportData data;
    data.reportDate = date;

    const QString day = date.toString("yyyy-MM-dd");

    QSqlQuery q(m_db);

    // ── Sales summary ────────────────────────────────────
    q.prepare("SELECT COUNT(*), "
              "       COALESCE(SUM(total),    0), "
              "       COALESCE(SUM(discount), 0), "
              "       COALESCE(SUM(tax),      0) "
              "FROM sales "
              "WHERE DATE(sale_date, 'localtime') = ?");
    q.addBindValue(day);
    if (q.exec() && q.next()) {
        data.transactionCount = q.value(0).toInt();
        const double gross    = q.value(1).toLongLong() / 100.0;
        data.totalDiscounts   = q.value(2).toLongLong() / 100.0;
        data.taxCollected     = q.value(3).toLongLong() / 100.0;
        data.grossSales       = gross + data.totalDiscounts; // pre-discount
        data.netSales         = gross;
    }

    // ── Refund summary ───────────────────────────────────
    q.prepare("SELECT COUNT(*), COALESCE(SUM(total_refunded), 0) "
              "FROM refunds "
              "WHERE substr(refund_date, 1, 10) = ?");
    q.addBindValue(day);
    if (q.exec() && q.next()) {
        data.refundCount  = q.value(0).toInt();
        data.totalRefunds = q.value(1).toLongLong() / 100.0;
    }

    // ── Sales by payment method ──────────────────────────
    q.prepare("SELECT payment_method, COUNT(*), COALESCE(SUM(total), 0) "
              "FROM sales "
              "WHERE DATE(sale_date, 'localtime') = ? "
              "GROUP BY payment_method ORDER BY 3 DESC");
    q.addBindValue(day);
    if (q.exec()) {
        while (q.next()) {
            ZReportData::PaymentLine pl;
            pl.method = q.value(0).toString();
            pl.count  = q.value(1).toInt();
            pl.total  = q.value(2).toLongLong() / 100.0;
            data.byPayment.append(pl);
            if (pl.method == "Cash") data.cashSales = pl.total;
        }
    }

    // ── Sales by category ────────────────────────────────
    q.prepare("SELECT p.category, "
              "       COALESCE(SUM(si.quantity), 0), "
              "       COALESCE(SUM(si.subtotal), 0) "
              "FROM sales s "
              "JOIN sale_items si ON si.sale_id = s.id "
              "JOIN products   p  ON p.id = si.product_id "
              "WHERE DATE(s.sale_date, 'localtime') = ? "
              "GROUP BY p.category ORDER BY 3 DESC");
    q.addBindValue(day);
    if (q.exec()) {
        while (q.next()) {
            ZReportData::CategoryLine cl;
            cl.category = q.value(0).toString();
            cl.qty      = q.value(1).toInt();
            cl.sales    = q.value(2).toLongLong() / 100.0;
            data.byCategory.append(cl);
        }
    }

    // ── Top 10 products ───────────────────────────────────
    q.prepare("SELECT si.product_name, "
              "       COALESCE(SUM(si.quantity), 0), "
              "       COALESCE(SUM(si.subtotal), 0) "
              "FROM sales s "
              "JOIN sale_items si ON si.sale_id = s.id "
              "WHERE DATE(s.sale_date, 'localtime') = ? "
              "GROUP BY si.product_id "
              "ORDER BY 3 DESC LIMIT 10");
    q.addBindValue(day);
    if (q.exec()) {
        while (q.next()) {
            ZReportData::ProductLine pl;
            pl.name  = q.value(0).toString();
            pl.qty   = q.value(1).toInt();
            pl.sales = q.value(2).toLongLong() / 100.0;
            data.topProducts.append(pl);
        }
    }

    // ── Shift / float info ────────────────────────────────
    if (shiftId > 0) {
        ShiftRecord sr    = m_shifts->getShift(shiftId);
        data.cashierName  = sr.cashierName;
        data.openingFloat = sr.openingFloat;
        data.closingFloat = sr.closingFloat;
        data.shiftInfo    = QString("Shift #%1 — %2").arg(sr.shiftId).arg(sr.cashierName);
    } else {
        data.shiftInfo = "All Shifts";
        if (m_shifts->isShiftOpen()) {
            data.cashierName  = m_shifts->currentShift().cashierName;
            data.openingFloat = m_shifts->currentShift().openingFloat;
        }
    }
    // Override with cash-up spinbox values if the user has entered them
    if (m_openingFloatSpin && m_openingFloatSpin->value() > 0)
        data.openingFloat = m_openingFloatSpin->value();
    if (m_countedCashSpin)
        data.closingFloat = m_countedCashSpin->value();

    data.expectedCash = data.openingFloat + data.cashSales - data.totalRefunds;
    data.cashVariance = data.closingFloat - data.expectedCash;

    return data;
}

QString ZReportDialog::renderHtml(const ZReportData &data)
{
    QString sym = m_settings->currencySymbol();
    QString biz = m_settings->businessName();

    auto money = [&](double v) {
        return QString("%1%2").arg(sym).arg(v, 0, 'f', 2);
    };

    QString html;
    html += R"(<!DOCTYPE html><html><head><meta charset="UTF-8">
<style>
body { font-family: 'Segoe UI', Arial, sans-serif; margin: 20px; color: #222; background:#fff; }
h1   { color: #1a237e; border-bottom: 3px solid #1a237e; padding-bottom:6px; }
h2   { color: #283593; margin-top:22px; border-left:4px solid #3f51b5; padding-left:8px; }
table{ width:100%; border-collapse:collapse; margin-top:8px; }
th   { background:#3f51b5; color:white; padding:8px; text-align:left; }
td   { padding:7px; border-bottom:1px solid #e0e0e0; }
tr:nth-child(even) td { background:#f5f5f5; }
.total-row td { font-weight:bold; background:#e8eaf6 !important; border-top:2px solid #3f51b5; }
.highlight { color:#e53935; font-weight:bold; }
.section { margin-top:20px; }
.kv { display:flex; justify-content:space-between; padding:5px 0; border-bottom:1px solid #eee; }
.kv-label { color:#555; }
.kv-value { font-weight:bold; }
.positive { color: #2e7d32; }
.negative { color: #c62828; }
.badge { display:inline-block; padding:3px 10px; border-radius:12px; font-size:0.85em; font-weight:bold; }
.badge-green { background:#e8f5e9; color:#2e7d32; }
.badge-red   { background:#ffebee; color:#c62828; }
</style></head><body>
)";

    // Header
    html += QString("<h1>Z-Report — End of Day</h1>");
    html += QString("<p><strong>Business:</strong> %1 &nbsp;|&nbsp; "
                    "<strong>Date:</strong> %2 &nbsp;|&nbsp; "
                    "<strong>Shift:</strong> %3</p>")
                .arg(biz, data.reportDate.toString("dddd, MMMM d yyyy"), data.shiftInfo);
    html += QString("<p><em>Generated: %1</em></p>")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    // ── Sales Summary ──────────────────────────────────────
    html += "<h2>Sales Summary</h2><div class='section'>";
    auto kv = [&](const QString &label, const QString &value, const QString &cls = "") {
        return QString("<div class='kv'><span class='kv-label'>%1</span>"
                       "<span class='kv-value %3'>%2</span></div>").arg(label, value, cls);
    };

    html += kv("Transactions", QString::number(data.transactionCount));
    html += kv("Gross Sales",  money(data.grossSales));

    if (data.totalDiscounts > 0) {
        html += kv("Discounts Applied", "-" + money(data.totalDiscounts), "highlight");
    }

    html += kv("Net Sales", money(data.netSales));

    if (data.refundCount > 0) {
        html += kv(QString("Refunds (%1)").arg(data.refundCount),
                   "-" + money(data.totalRefunds), "highlight");
    }

    if (m_settings->taxEnabled() && data.taxCollected > 0) {
        html += kv(QString("%1 Collected (%2%)")
                       .arg(m_settings->taxLabel())
                       .arg(qRound(m_settings->taxRate() * 100)),
                       money(data.taxCollected));
    }

    html += "</div>";

    // ── Payment Method Breakdown ───────────────────────────
    if (!data.byPayment.isEmpty()) {
        html += "<h2>By Payment Method</h2>";
        html += "<table><tr><th>Method</th><th>Transactions</th><th>Total</th></tr>";
        for (const auto &pl : data.byPayment) {
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                        .arg(pl.method).arg(pl.count).arg(money(pl.total));
        }
        html += "</table>";
    }

    // ── Cash Reconciliation ────────────────────────────────
    if (data.openingFloat > 0 || data.closingFloat > 0) {
        html += "<h2>Cash Reconciliation</h2><div class='section'>";
        html += kv("Opening Float",  money(data.openingFloat));
        html += kv("Net Sales",      money(data.netSales));
        html += kv("Expected Cash",  money(data.expectedCash));
        html += kv("Closing Float (Counted)", money(data.closingFloat));

        QString varianceClass = data.cashVariance >= 0 ? "positive" : "negative";
        QString varianceSign  = data.cashVariance >= 0 ? "+" : "";
        html += kv("Variance",
                   varianceSign + money(data.cashVariance),
                   varianceClass);
        html += "</div>";
    }

    // ── Sales by Category ──────────────────────────────────
    if (!data.byCategory.isEmpty()) {
        html += "<h2>Sales by Category</h2>";
        html += "<table><tr><th>Category</th><th>Qty Sold</th><th>Sales</th><th>Share</th></tr>";

        for (const auto &cl : data.byCategory) {
            double share = data.grossSales > 0 ? (cl.sales / data.grossSales * 100.0) : 0.0;
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4%</td></tr>")
                        .arg(cl.category).arg(cl.qty).arg(money(cl.sales)).arg(share, 0, 'f', 1);
        }

        // Totals row
        int totalQty = 0;
        for (const auto &cl : data.byCategory) totalQty += cl.qty;
        html += QString("<tr class='total-row'><td>TOTAL</td><td>%1</td><td>%2</td><td>100%</td></tr>")
                    .arg(totalQty).arg(money(data.grossSales));
        html += "</table>";
    }

    // ── Top Products ───────────────────────────────────────
    if (!data.topProducts.isEmpty()) {
        html += "<h2>Top Products</h2>";
        html += "<table><tr><th>#</th><th>Product</th><th>Qty</th><th>Sales</th></tr>";

        int rank = 1;
        for (const auto &pl : data.topProducts) {
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
            .arg(rank++).arg(pl.name).arg(pl.qty).arg(money(pl.sales));
        }
        html += "</table>";
    }

    // Footer
    html += QString("<br><hr><p style='color:#999;font-size:0.85em;text-align:center;'>"
                    "%1 &mdash; Report generated by KeynetikPOS</p>").arg(biz);
    html += "</body></html>";

    return html;
}

void ZReportDialog::signOff()
{
    generateReport();   // refresh with current spinbox values
    const ZReportData &d = m_lastReport;

    const QString sym = m_settings->currencySymbol();
    QString msg = QString(
        "Till Sign-Off — %1\n\n"
        "Opening Float:  %2 %3\n"
        "Cash Sales:     %2 %4\n"
        "Expected Cash:  %2 %5\n"
        "Counted Cash:   %2 %6\n"
        "Variance:       %2 %7\n\n"
        "Record this reconciliation?")
        .arg(d.reportDate.toString("yyyy-MM-dd"), sym)
        .arg(d.openingFloat, 0, 'f', 2)
        .arg(d.cashSales,    0, 'f', 2)
        .arg(d.expectedCash, 0, 'f', 2)
        .arg(d.closingFloat, 0, 'f', 2)
        .arg(d.cashVariance, 0, 'f', 2);

    if (QMessageBox::question(this, "Sign Off Till", msg,
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Persist to till_reconciliations table
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO till_reconciliations "
              "(reconciliation_date, cashier_name, opening_float, cash_sales, "
              " expected_cash, counted_cash, variance, signed_off_by) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(d.reportDate.toString("yyyy-MM-dd"));
    q.addBindValue(d.cashierName.isEmpty()
                       ? m_settings->businessName() : d.cashierName);
    q.addBindValue(qRound64(d.openingFloat * 100));
    q.addBindValue(qRound64(d.cashSales    * 100));
    q.addBindValue(qRound64(d.expectedCash * 100));
    q.addBindValue(qRound64(d.closingFloat * 100));
    q.addBindValue(qRound64(d.cashVariance * 100));
    q.addBindValue(d.cashierName);
    if (!q.exec())
        QMessageBox::warning(this, "Sign-Off Failed",
                             "Could not save reconciliation: " + q.lastError().text());
    else
        QMessageBox::information(this, "Signed Off",
                                 "Till reconciliation recorded successfully.");
}

void ZReportDialog::exportReport()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export Z-Report",
        QString("ZReport_%1.html").arg(m_lastReport.reportDate.toString("yyyyMMdd")),
        "HTML Files (*.html);;All Files (*)"
        );

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << renderHtml(m_lastReport);
        file.close();
        QMessageBox::information(this, "Exported", "Report saved to:\n" + fileName);
    } else {
        QMessageBox::critical(this, "Error", "Could not write file.");
    }
}

void ZReportDialog::printReport()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;

    QTextDocument doc;
    doc.setHtml(renderHtml(m_lastReport));
    doc.print(&printer);
}

QString ZReportDialog::generateZReportText(QSqlDatabase &db,
                                           SettingsManager *settings,
                                           ShiftManager *shifts,
                                           const QDate &date,
                                           int shiftId)
{
    ZReportData data;
    data.reportDate = date;
    const QString day = date.toString("yyyy-MM-dd");

    QSqlQuery q(db);

    q.prepare("SELECT COUNT(*), COALESCE(SUM(total),0), "
              "COALESCE(SUM(discount),0), COALESCE(SUM(tax),0) "
              "FROM sales WHERE DATE(sale_date, 'localtime') = ?");
    q.addBindValue(day);
    if (q.exec() && q.next()) {
        const double gross  = q.value(1).toLongLong() / 100.0;
        data.totalDiscounts = q.value(2).toLongLong() / 100.0;
        data.taxCollected   = q.value(3).toLongLong() / 100.0;
        data.transactionCount = q.value(0).toInt();
        data.grossSales     = gross + data.totalDiscounts;
        data.netSales       = gross;
    }

    q.prepare("SELECT COUNT(*), COALESCE(SUM(total_refunded),0) "
              "FROM refunds WHERE substr(refund_date,1,10) = ?");
    q.addBindValue(day);
    if (q.exec() && q.next()) {
        data.refundCount  = q.value(0).toInt();
        data.totalRefunds = q.value(1).toLongLong() / 100.0;
    }

    q.prepare("SELECT payment_method, COUNT(*), COALESCE(SUM(total),0) "
              "FROM sales WHERE DATE(sale_date, 'localtime') = ? "
              "GROUP BY payment_method ORDER BY 3 DESC");
    q.addBindValue(day);
    if (q.exec()) {
        while (q.next()) {
            ZReportData::PaymentLine pl;
            pl.method = q.value(0).toString();
            pl.count  = q.value(1).toInt();
            pl.total  = q.value(2).toLongLong() / 100.0;
            data.byPayment.append(pl);
            if (pl.method == "Cash") data.cashSales = pl.total;
        }
    }

    q.prepare("SELECT p.category, COALESCE(SUM(si.quantity),0), "
              "COALESCE(SUM(si.subtotal),0) "
              "FROM sales s JOIN sale_items si ON si.sale_id=s.id "
              "JOIN products p ON p.id=si.product_id "
              "WHERE DATE(s.sale_date, 'localtime')=? "
              "GROUP BY p.category ORDER BY 3 DESC");
    q.addBindValue(day);
    if (q.exec()) {
        while (q.next()) {
            ZReportData::CategoryLine cl;
            cl.category = q.value(0).toString();
            cl.qty      = q.value(1).toInt();
            cl.sales    = q.value(2).toLongLong() / 100.0;
            data.byCategory.append(cl);
        }
    }

    q.prepare("SELECT si.product_name, COALESCE(SUM(si.quantity),0), "
              "COALESCE(SUM(si.subtotal),0) "
              "FROM sales s JOIN sale_items si ON si.sale_id=s.id "
              "WHERE DATE(s.sale_date, 'localtime')=? "
              "GROUP BY si.product_id ORDER BY 3 DESC LIMIT 10");
    q.addBindValue(day);
    if (q.exec()) {
        while (q.next()) {
            ZReportData::ProductLine pl;
            pl.name  = q.value(0).toString();
            pl.qty   = q.value(1).toInt();
            pl.sales = q.value(2).toLongLong() / 100.0;
            data.topProducts.append(pl);
        }
    }

    if (shiftId > 0 && shifts) {
        ShiftRecord sr    = shifts->getShift(shiftId);
        data.cashierName  = sr.cashierName;
        data.openingFloat = sr.openingFloat;
        data.closingFloat = sr.closingFloat;
        data.shiftInfo    = QString("Shift #%1 — %2").arg(sr.shiftId).arg(sr.cashierName);
        data.expectedCash = data.openingFloat + data.cashSales;
        data.cashVariance = data.closingFloat - data.expectedCash;
    } else {
        data.shiftInfo = "All Shifts";
    }

    // Format as text message
    QString sym = settings->currencySymbol();
    QString msg;

    msg += "📊 *Z-REPORT*\n";
    msg += "━━━━━━━━━━━━━━━━━━━━\n\n";

    msg += QString("📅 Date: %1\n").arg(date.toString("yyyy-MM-dd"));
    msg += QString("🏪 %1\n").arg(settings->businessName());
    msg += QString("👤 %1\n\n").arg(data.shiftInfo);

    msg += "💰 *SALES SUMMARY*\n";
    msg += QString("Transactions: %1\n").arg(data.transactionCount);
    msg += QString("Gross Sales: %1%2\n")
               .arg(sym).arg(data.grossSales, 0, 'f', 2);

    if (data.totalDiscounts > 0) {
        msg += QString("Discounts: -%1%2\n")
        .arg(sym).arg(data.totalDiscounts, 0, 'f', 2);
    }

    msg += QString("*Net Sales: %1%2*\n")
               .arg(sym).arg(data.netSales, 0, 'f', 2);

    if (settings->taxEnabled() && data.taxCollected > 0) {
        msg += QString("%1 (%2%%): %3%4\n")
        .arg(settings->taxLabel())
            .arg(qRound(settings->taxRate() * 100))
            .arg(sym).arg(data.taxCollected, 0, 'f', 2);
    }

    // Cash reconciliation
    if (data.openingFloat > 0 || data.closingFloat > 0) {
        msg += "\n💵 *CASH RECONCILIATION*\n";
        msg += QString("Opening Float: %1%2\n")
                   .arg(sym).arg(data.openingFloat, 0, 'f', 2);
        msg += QString("Expected Cash: %1%2\n")
                   .arg(sym).arg(data.expectedCash, 0, 'f', 2);
        msg += QString("Closing Float: %1%2\n")
                   .arg(sym).arg(data.closingFloat, 0, 'f', 2);

        QString variance = data.cashVariance >= 0 ? "+" : "";
        QString varianceIcon = data.cashVariance >= 0 ? "✅" : "⚠️";
        msg += QString("%1 Variance: %2%3%4\n")
                   .arg(varianceIcon).arg(variance)
                   .arg(sym).arg(qAbs(data.cashVariance), 0, 'f', 2);
    }

    // Sales by category
    if (!data.byCategory.isEmpty()) {
        msg += "\n📂 *SALES BY CATEGORY*\n";
        for (int i = 0; i < qMin(5, data.byCategory.size()); ++i) {
            const auto &cat = data.byCategory[i];
            double share = data.grossSales > 0 ?
                               (cat.sales / data.grossSales * 100) : 0;
            msg += QString("• %1: %2%3 (%4%%)\n")
                       .arg(cat.category)
                       .arg(sym).arg(cat.sales, 0, 'f', 2)
                       .arg(share, 0, 'f', 0);
        }
    }

    // Top products
    if (!data.topProducts.isEmpty()) {
        msg += "\n🏆 *TOP 5 PRODUCTS*\n";
        for (int i = 0; i < qMin(5, data.topProducts.size()); ++i) {
            const auto &prod = data.topProducts[i];
            msg += QString("%1. %2\n   ×%3 — %4%5\n")
                       .arg(i + 1)
                       .arg(prod.name)
                       .arg(prod.qty)
                       .arg(sym).arg(prod.sales, 0, 'f', 2);
        }
    }

    msg += QString("\n_Generated: %1_")
               .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));

    return msg;
}
