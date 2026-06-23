// =============================================================================
// receiptprinter.h — ReceiptPrinter + Receipt: receipt generation and output
// -----------------------------------------------------------------------------
// WHAT: Receipt snapshots everything a printed receipt needs (items, totals,
//       tender, change, cashier, timestamp). ReceiptPrinter renders and
//       outputs it: three nominal targets (thermal / standard printer / PDF),
//       company header/footer configuration, last-receipt reprint, and a stub
//       email path.
// HOW:  Receipts are built twice — generateReceiptText() produces fixed-width
//       text (centerText/padRight helpers) for 40-column thermal printers, and
//       generateReceiptHTML() produces styled HTML rendered via QTextDocument +
//       QPrinter for standard printing and PDF export. Company/printer config
//       persists in QSettings and loads in the constructor. The current build
//       always takes the PDF path, naming files Receipt_<saleId>_<timestamp>.
//       The last receipt is cached in memory for reprint.
// WHY:  PDF-by-default makes the system usable with zero printer setup (files
//       can be archived, emailed, or printed later) while the thermal text
//       path stays ready for real ESC/POS hardware. Receipts snapshot data
//       rather than re-query it so a reprint is identical to the original even
//       if products changed since.
// =============================================================================
#ifndef RECEIPTPRINTER_H
#define RECEIPTPRINTER_H

#include <QString>
#include <QVector>
#include <QDateTime>
#include <QSettings>
#include <QPrinter>
#include <QTextDocument>
#include <QStandardPaths>
#include <QDir>
#include "CartItem.h"
#include "smtpclient.h"   // SmtpConfig

struct Receipt {
    int saleId = 0;
    QDateTime dateTime;
    QVector<CartItem> items;
    Money subtotal;
    Money tax;
    Money discount;
    QString discountReason;
    Money total;
    QString paymentMethod;
    QString referenceNumber;   // M-Pesa / card transaction code (non-cash)
    Money amountPaid;
    Money change;
    QString customerName;
    QString cashierName;
};

class ReceiptPrinter
{
public:
    enum PrinterType {
        ThermalPrinter,
        StandardPrinter,
        PDFExport
    };

    ReceiptPrinter();

    // Configuration
    void setPrinterType(PrinterType type);
    void setThermalPrinterPort(const QString &port);
    void setCompanyInfo(const QString &name, const QString &address,
                        const QString &phone, const QString &taxId);
    void setReceiptFooter(const QString &footer);
    void setSmtpConfig(const SmtpConfig &config);

    // Printing
    bool printReceipt(const Receipt &receipt);
    bool reprintLastReceipt();
    bool emailReceipt(const Receipt &receipt, const QString &email);

    // Building blocks for sending a receipt off the UI thread: callers can grab
    // an immutable copy of the mail config + rendered HTML, then run SmtpClient
    // on a worker thread without touching this object's shared state.
    SmtpConfig mailConfig() const { return smtpConfig; }
    QString    renderReceiptHtml(const Receipt &receipt) { return generateReceiptHTML(receipt); }

    // History
    Receipt getLastReceipt();
    bool isConfigured() const;
    QString getLastError() const;
    // Filesystem path of the most recently saved receipt PDF (empty if none).
    QString getLastSavedPath() const { return lastSavedPath; }

private:
    QString generateReceiptText(const Receipt &receipt);
    QString generateReceiptHTML(const Receipt &receipt);
    bool printToThermal(const QString &receiptText);
    bool printToStandard(const QString &receiptHTML);
    bool saveToPDF(const QString &receiptHTML, const QString &filename);
    void saveLastReceipt(const Receipt &receipt);

    QString formatCurrency(Money amount) const;
    QString centerText(const QString &text, int width) const;
    QString padRight(const QString &text, int width) const;

    PrinterType printerType;
    QString thermalPort;
    QString companyName;
    QString companyAddress;
    QString companyPhone;
    QString companyTaxId;
    QString receiptFooter;
    SmtpConfig smtpConfig;
    Receipt lastReceipt;
    QString lastError;
    QString lastSavedPath;
    QSettings settings;
};

#endif // RECEIPTPRINTER_H
