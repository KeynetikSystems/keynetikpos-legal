// =============================================================================
// receiptprinter.cpp — Implementation of ReceiptPrinter (see receiptprinter.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Company info and printer config load from QSettings in the constructor
//    and are written back by every setter, so configuration survives restarts.
//  - printReceipt() currently always takes the PDF path (zero-setup default),
//    naming files Receipt_<saleId>_<timestamp>.pdf; the fixed-width thermal
//    text generator stays ready for real ESC/POS hardware.
//  - The last receipt is cached in memory so reprintLastReceipt() reproduces
//    the original exactly, even if products changed since.
// =============================================================================
#include "receiptprinter.h"
#include "cart.h"          // formatMoney()
#include "smtpclient.h"
#include <QFileDialog>
#include <QTextStream>
#include <QDebug>
#include <QMessageBox>

ReceiptPrinter::ReceiptPrinter()
    : printerType(PDFExport)  // Default to PDF
    , thermalPort("COM1")
    , companyName("Keynetik POS")
    , companyAddress("Nakuru, Kenya")
    , companyPhone("+254 700 000000")
    , companyTaxId("TAX-123456")
    , receiptFooter("Thank you for your purchase!\nCome again!")
    , settings("KeynetikPOS", "ReceiptPrinter")
{
    // Load settings
    companyName = settings.value("company/name", companyName).toString();
    companyAddress = settings.value("company/address", companyAddress).toString();
    companyPhone = settings.value("company/phone", companyPhone).toString();
    companyTaxId = settings.value("company/taxId", companyTaxId).toString();
    receiptFooter = settings.value("receipt/footer", receiptFooter).toString();
}

void ReceiptPrinter::setPrinterType(PrinterType type)
{
    printerType = type;
    settings.setValue("printer/type", static_cast<int>(type));
}

void ReceiptPrinter::setThermalPrinterPort(const QString &port)
{
    thermalPort = port;
    settings.setValue("printer/thermalPort", port);
}

void ReceiptPrinter::setCompanyInfo(const QString &name, const QString &address,
                                    const QString &phone, const QString &taxId)
{
    companyName = name;
    companyAddress = address;
    companyPhone = phone;
    companyTaxId = taxId;

    settings.setValue("company/name", name);
    settings.setValue("company/address", address);
    settings.setValue("company/phone", phone);
    settings.setValue("company/taxId", taxId);
}

void ReceiptPrinter::setReceiptFooter(const QString &footer)
{
    receiptFooter = footer;
    settings.setValue("receipt/footer", footer);
}

bool ReceiptPrinter::printReceipt(const Receipt &receipt)
{
    saveLastReceipt(receipt);

    // Always print to PDF
    QString filename = QString("Receipt_%1_%2.pdf")
                           .arg(receipt.saleId)
                           .arg(receipt.dateTime.toString("yyyyMMdd_hhmmss"));

    return saveToPDF(generateReceiptHTML(receipt), filename);
}

bool ReceiptPrinter::reprintLastReceipt()
{
    if (lastReceipt.saleId == 0) {
        lastError = "No previous receipt to reprint";
        return false;
    }

    return printReceipt(lastReceipt);
}

QString ReceiptPrinter::generateReceiptText(const Receipt &receipt)
{
    QString text;
    QTextStream stream(&text);

    const int width = 42;

    // Header
    stream << centerText(companyName, width) << "\n";
    stream << centerText(companyAddress, width) << "\n";
    stream << centerText("Tel: " + companyPhone, width) << "\n";
    stream << centerText("TAX ID: " + companyTaxId, width) << "\n";
    stream << QString(width, '=') << "\n";

    // Sale info
    stream << "Sale #: " << receipt.saleId << "\n";
    stream << "Date: " << receipt.dateTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
    if (!receipt.customerName.isEmpty()) {
        stream << "Customer: " << receipt.customerName << "\n";
    }
    if (!receipt.cashierName.isEmpty()) {
        stream << "Cashier: " << receipt.cashierName << "\n";
    }
    stream << QString(width, '=') << "\n\n";

    // Items
    stream << padRight("Item", 24) << padRight("Qty", 6) << padRight("Total", 12) << "\n";
    stream << QString(width, '-') << "\n";

    for (const CartItem &item : receipt.items) {
        QString itemName = item.name;
        if (itemName.length() > 24) {
            itemName = itemName.left(21) + "...";
        }

        stream << padRight(itemName, 24);
        stream << padRight(QString::number(item.quantity), 6);
        stream << padRight(formatCurrency(item.getSubtotal()), 12) << "\n";
        stream << "  @ " << formatCurrency(item.price) << " each\n";
    }

    stream << QString(width, '-') << "\n\n";

    // Totals
    stream << padRight("Subtotal:", 30) << formatCurrency(receipt.subtotal) << "\n";
    stream << padRight("Tax:", 30) << formatCurrency(receipt.tax) << "\n";

    if (receipt.discount > 0) {
        stream << padRight("Discount:", 30) << "-" << formatCurrency(receipt.discount) << "\n";
        if (!receipt.discountReason.isEmpty()) {
            stream << "  (" << receipt.discountReason << ")\n";
        }
    }

    stream << QString(width, '=') << "\n";
    stream << padRight("TOTAL:", 30) << formatCurrency(receipt.total) << "\n";
    stream << QString(width, '=') << "\n\n";

    // Payment
    stream << "Payment: " << receipt.paymentMethod << "\n";
    if (!receipt.referenceNumber.isEmpty())
        stream << "Ref: " << receipt.referenceNumber << "\n";
    stream << padRight("Paid:", 30) << formatCurrency(receipt.amountPaid) << "\n";
    if (receipt.change > 0) {
        stream << padRight("Change:", 30) << formatCurrency(receipt.change) << "\n";
    }

    // Footer
    stream << "\n" << QString(width, '=') << "\n";
    QStringList footerLines = receiptFooter.split('\n');
    for (const QString &line : footerLines) {
        stream << centerText(line, width) << "\n";
    }

    return text;
}

QString ReceiptPrinter::generateReceiptHTML(const Receipt &receipt)
{
    QString html;
    QTextStream stream(&html);

    stream << "<!DOCTYPE html><html><head><meta charset='utf-8'><style>";
    stream << "body { font-family: 'Courier New', monospace; width: 80mm; margin: 10mm auto; }";
    stream << "h1, h2, p { text-align: center; margin: 5px 0; }";
    stream << "h1 { font-size: 16pt; font-weight: bold; }";
    stream << "table { width: 100%; border-collapse: collapse; margin: 10px 0; }";
    stream << "th, td { text-align: left; padding: 5px; font-size: 10pt; }";
    stream << "th { border-bottom: 2px solid #000; font-weight: bold; }";
    stream << ".right { text-align: right; }";
    stream << ".center { text-align: center; }";
    stream << ".total { font-size: 12pt; font-weight: bold; border-top: 2px solid #000; }";
    stream << ".hr { border-top: 1px dashed #000; margin: 10px 0; }";
    stream << ".small { font-size: 8pt; color: #666; }";
    stream << "</style></head><body>";

    // Header
    stream << "<h1>" << companyName << "</h1>";
    stream << "<p>" << companyAddress << "</p>";
    stream << "<p>Tel: " << companyPhone << "</p>";
    stream << "<p class='small'>TAX ID: " << companyTaxId << "</p>";
    stream << "<div class='hr'></div>";

    // Sale info
    stream << "<p><strong>Receipt #" << receipt.saleId << "</strong></p>";
    stream << "<p class='small'>" << receipt.dateTime.toString("yyyy-MM-dd hh:mm:ss") << "</p>";
    if (!receipt.customerName.isEmpty()) {
        stream << "<p class='small'><strong>Customer:</strong> " << receipt.customerName << "</p>";
    }
    if (!receipt.cashierName.isEmpty()) {
        stream << "<p class='small'><strong>Cashier:</strong> " << receipt.cashierName << "</p>";
    }
    stream << "<div class='hr'></div>";

    // Items table
    stream << "<table>";
    stream << "<tr><th>Item</th><th class='center'>Qty</th><th class='right'>Total</th></tr>";

    for (const CartItem &item : receipt.items) {
        stream << "<tr>";
        stream << "<td>" << item.name << "<br><span class='small'>@ " << formatCurrency(item.price) << "</span></td>";
        stream << "<td class='center'>" << item.quantity << "</td>";
        stream << "<td class='right'>" << formatCurrency(item.getSubtotal()) << "</td>";
        stream << "</tr>";
    }

    stream << "</table>";
    stream << "<div class='hr'></div>";

    // Totals
    stream << "<table>";
    stream << "<tr><td>Subtotal:</td><td class='right'>" << formatCurrency(receipt.subtotal) << "</td></tr>";
    stream << "<tr><td>Tax (16%):</td><td class='right'>" << formatCurrency(receipt.tax) << "</td></tr>";

    if (receipt.discount > 0) {
        stream << "<tr><td>Discount:";
        if (!receipt.discountReason.isEmpty()) {
            stream << "<br><span class='small'>(" << receipt.discountReason << ")</span>";
        }
        stream << "</td><td class='right'>-" << formatCurrency(receipt.discount) << "</td></tr>";
    }

    stream << "<tr class='total'><td><strong>TOTAL:</strong></td><td class='right'><strong>"
           << formatCurrency(receipt.total) << "</strong></td></tr>";
    stream << "</table>";

    // Payment
    stream << "<div class='hr'></div>";
    stream << "<table>";
    stream << "<tr><td>Payment Method:</td><td class='right'>" << receipt.paymentMethod << "</td></tr>";
    if (!receipt.referenceNumber.isEmpty())
        stream << "<tr><td>Reference:</td><td class='right'>"
               << receipt.referenceNumber.toHtmlEscaped() << "</td></tr>";
    stream << "<tr><td>Amount Paid:</td><td class='right'>" << formatCurrency(receipt.amountPaid) << "</td></tr>";
    if (receipt.change > 0) {
        stream << "<tr><td><strong>Change:</strong></td><td class='right'><strong>"
               << formatCurrency(receipt.change) << "</strong></td></tr>";
    }
    stream << "</table>";

    // Footer
    stream << "<div class='hr'></div>";
    QStringList footerLines = receiptFooter.split('\n');
    for (const QString &line : footerLines) {
        stream << "<p class='center'>" << line << "</p>";
    }

    stream << "<p class='center small'>Powered by Keynetik POS</p>";

    stream << "</body></html>";

    return html;
}

bool ReceiptPrinter::printToThermal(const QString &receiptText)
{
    Q_UNUSED(receiptText);
    lastError = "Thermal printing not supported - use PDF instead";
    return false;
}

bool ReceiptPrinter::printToStandard(const QString &receiptHTML)
{
    Q_UNUSED(receiptHTML);
    lastError = "Standard printing not supported - use PDF instead";
    return false;
}

bool ReceiptPrinter::saveToPDF(const QString &receiptHTML, const QString &filename)
{
    // Auto-save to Documents/Receipts folder
    QString receiptsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/KeynetikPOS/Receipts";
    QDir dir;
    if (!dir.exists(receiptsDir)) {
        dir.mkpath(receiptsDir);
    }

    QString filepath = receiptsDir + "/" + filename;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filepath);
    printer.setPageSize(QPageSize(QSizeF(80, 297), QPageSize::Millimeter)); // Thermal receipt size

    QTextDocument document;
    document.setHtml(receiptHTML);
    document.print(&printer);

    qDebug() << "Receipt saved to:" << filepath;

    // Show confirmation
    QMessageBox::information(nullptr, "Receipt Saved",
                             QString("Receipt saved to:\n%1").arg(filepath));

    return true;
}

void ReceiptPrinter::setSmtpConfig(const SmtpConfig &config)
{
    smtpConfig = config;
}

bool ReceiptPrinter::emailReceipt(const Receipt &receipt, const QString &email)
{
    if (!smtpConfig.isConfigured()) {
        lastError = "Email is not set up. Add your mail server under "
                    "Settings → Company Information.";
        return false;
    }

    const QString subject = QString("Receipt #%1 from %2")
                                .arg(receipt.saleId).arg(companyName);

    SmtpClient client(smtpConfig);
    QString err;
    if (!client.send(email, subject, generateReceiptHTML(receipt), &err)) {
        lastError = "Failed to email receipt: " + err;
        return false;
    }

    lastError.clear();
    return true;
}

void ReceiptPrinter::saveLastReceipt(const Receipt &receipt)
{
    lastReceipt = receipt;
}

Receipt ReceiptPrinter::getLastReceipt()
{
    return lastReceipt;
}

bool ReceiptPrinter::isConfigured() const
{
    return !companyName.isEmpty();
}

QString ReceiptPrinter::getLastError() const
{
    return lastError;
}

QString ReceiptPrinter::formatCurrency(double amount) const
{
    return formatMoney(amount);
}

QString ReceiptPrinter::centerText(const QString &text, int width) const
{
    int padding = (width - text.length()) / 2;
    return QString(padding, ' ') + text;
}

QString ReceiptPrinter::padRight(const QString &text, int width) const
{
    QString result = text;
    while (result.length() < width) {
        result += " ";
    }
    return result;
}
