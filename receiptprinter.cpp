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
#include <QTextStream>
#include <QDebug>
#include <QPainter>
#include <QFont>
#include <QFileInfo>
#include <QAbstractTextDocumentLayout>

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

    if (receipt.discount.cents() > 0) {
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
    if (receipt.change.cents() > 0) {
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
    // NOTE: QTextDocument (used for the PDF) ignores <style> blocks and CSS
    // classes — it only honours INLINE style="" attributes and a handful of
    // element attributes (align/valign/width). Email clients also render inline
    // styles most reliably. So everything here is styled inline. All
    // user/product text is HTML-escaped so a stray '&' or '<' can't break the
    // layout.
    auto esc = [](const QString &s) { return s.toHtmlEscaped(); };
    const QString HR =
        "<div style=\"border-top:1px dashed #000; margin:6px 0; height:0;\"></div>";

    QString html;
    QTextStream stream(&html);

    stream << "<html><body style=\"font-family:'Courier New',monospace; "
              "font-size:10pt; color:#000;\">";

    // ── Header (centred) ────────────────────────────────────────────────────
    stream << "<div style=\"text-align:center;\">";
    stream << "<div style=\"font-size:15pt; font-weight:bold;\">" << esc(companyName) << "</div>";
    if (!companyAddress.isEmpty())
        stream << "<div>" << esc(companyAddress) << "</div>";
    if (!companyPhone.isEmpty())
        stream << "<div>Tel: " << esc(companyPhone) << "</div>";
    if (!companyTaxId.isEmpty())
        stream << "<div style=\"font-size:8pt; color:#444;\">TAX ID: " << esc(companyTaxId) << "</div>";
    stream << "</div>";
    stream << HR;

    // ── Sale info ───────────────────────────────────────────────────────────
    stream << "<div style=\"font-weight:bold;\">Receipt #" << receipt.saleId << "</div>";
    stream << "<div style=\"font-size:8pt; color:#444;\">"
           << esc(receipt.dateTime.toString("yyyy-MM-dd hh:mm:ss")) << "</div>";
    if (!receipt.customerName.isEmpty())
        stream << "<div style=\"font-size:8pt;\">Customer: " << esc(receipt.customerName) << "</div>";
    if (!receipt.cashierName.isEmpty())
        stream << "<div style=\"font-size:8pt;\">Cashier: " << esc(receipt.cashierName) << "</div>";
    stream << HR;

    // ── Items ───────────────────────────────────────────────────────────────
    stream << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\" "
              "style=\"font-size:9pt;\">";
    stream << "<tr style=\"font-weight:bold;\">"
              "<td width=\"58%\">Item</td>"
              "<td width=\"12%\" align=\"center\">Qty</td>"
              "<td width=\"30%\" align=\"right\">Amount</td></tr>";
    for (const CartItem &item : receipt.items) {
        stream << "<tr>";
        stream << "<td valign=\"top\">" << esc(item.name)
               << "<br><span style=\"font-size:7.5pt; color:#555;\">@ "
               << formatCurrency(item.price) << "</span></td>";
        stream << "<td valign=\"top\" align=\"center\">" << item.quantity << "</td>";
        stream << "<td valign=\"top\" align=\"right\">" << formatCurrency(item.getSubtotal()) << "</td>";
        stream << "</tr>";
    }
    stream << "</table>";
    stream << HR;

    // ── Totals ──────────────────────────────────────────────────────────────
    stream << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"1\" style=\"font-size:9.5pt;\">";
    stream << "<tr><td>Subtotal</td><td align=\"right\">" << formatCurrency(receipt.subtotal) << "</td></tr>";
    stream << "<tr><td>Tax</td><td align=\"right\">" << formatCurrency(receipt.tax) << "</td></tr>";
    if (receipt.discount.cents() > 0) {
        stream << "<tr><td>Discount";
        if (!receipt.discountReason.isEmpty())
            stream << " <span style=\"font-size:7.5pt; color:#555;\">(" << esc(receipt.discountReason) << ")</span>";
        stream << "</td><td align=\"right\">-" << formatCurrency(receipt.discount) << "</td></tr>";
    }
    stream << "<tr style=\"font-weight:bold; font-size:12pt;\">"
              "<td>TOTAL</td><td align=\"right\">" << formatCurrency(receipt.total) << "</td></tr>";
    stream << "</table>";
    stream << HR;

    // ── Payment ─────────────────────────────────────────────────────────────
    stream << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"1\" style=\"font-size:9.5pt;\">";
    stream << "<tr><td>Payment</td><td align=\"right\">" << esc(receipt.paymentMethod) << "</td></tr>";
    if (!receipt.referenceNumber.isEmpty())
        stream << "<tr><td>Reference</td><td align=\"right\">" << esc(receipt.referenceNumber) << "</td></tr>";
    stream << "<tr><td>Amount Paid</td><td align=\"right\">" << formatCurrency(receipt.amountPaid) << "</td></tr>";
    if (receipt.change.cents() > 0)
        stream << "<tr style=\"font-weight:bold;\"><td>Change</td><td align=\"right\">"
               << formatCurrency(receipt.change) << "</td></tr>";
    stream << "</table>";
    stream << HR;

    // ── Footer (centred) ────────────────────────────────────────────────────
    stream << "<div style=\"text-align:center;\">";
    const QStringList footerLines = receiptFooter.split('\n');
    for (const QString &line : footerLines)
        stream << "<div>" << esc(line) << "</div>";
    stream << "<div style=\"font-size:7.5pt; color:#666; margin-top:6px;\">Powered by KeynetikPOS</div>";
    stream << "</div>";

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
    // Auto-save to Documents/KeynetikPOS/Receipts.
    const QString receiptsDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + "/KeynetikPOS/Receipts";
    QDir dir;
    if (!dir.exists(receiptsDir) && !dir.mkpath(receiptsDir)) {
        lastError = "Could not create the receipts folder: " + receiptsDir;
        qWarning() << lastError;
        return false;
    }
    const QString filepath = receiptsDir + "/" + filename;

    // Thermal-style roll: a fixed 80 mm width with the page HEIGHT driven by the
    // content, so a short receipt isn't stranded on an A4-tall page and nothing
    // is clipped off the right edge. We lay the document out at the content
    // width, measure it, size the page to fit, then paint it once (no
    // pagination, no second blank page).
    const double widthMM    = 80.0;
    const double marginMM   = 5.0;
    const double contentWmm = widthMM - 2 * marginMM;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filepath);
    printer.setFullPage(true);   // we apply our own margins via the painter

    const double pxPerMM = printer.resolution() / 25.4;

    QTextDocument doc;
    // Lay the document out using the PRINTER's resolution, not the screen's —
    // otherwise font metrics and doc.size() are measured at ~96 dpi while we
    // scale by the printer's ~1200 dpi, collapsing the page to a sliver.
    doc.documentLayout()->setPaintDevice(&printer);
    doc.setDocumentMargin(0);
    doc.setDefaultFont(QFont("Courier New", 9));
    doc.setHtml(receiptHTML);
    doc.setTextWidth(contentWmm * pxPerMM);

    const double contentHmm = doc.size().height() / pxPerMM;
    const double pageHmm    = contentHmm + 2 * marginMM;
    printer.setPageSize(QPageSize(QSizeF(widthMM, pageHmm), QPageSize::Millimeter));

    QPainter painter;
    if (!painter.begin(&printer)) {
        lastError = "Could not open the receipt PDF for writing: " + filepath;
        qWarning() << lastError;
        return false;
    }
    painter.translate(marginMM * pxPerMM, marginMM * pxPerMM);
    doc.drawContents(&painter);
    painter.end();

    if (!QFileInfo::exists(filepath) || QFileInfo(filepath).size() == 0) {
        lastError = "The receipt PDF could not be saved: " + filepath;
        qWarning() << lastError;
        return false;
    }

    lastSavedPath = filepath;
    lastError.clear();
    qDebug() << "Receipt saved to:" << filepath;
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

QString ReceiptPrinter::formatCurrency(Money amount) const
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
