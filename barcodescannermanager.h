// =============================================================================
// barcodescannermanager.h — Barcode DATA layer + generator UI + legacy input
// -----------------------------------------------------------------------------
// WHAT: Three classes. BarcodeScannerManager: product lookup by barcode,
//       barcode assignment/removal, EAN-13 generation with valid check digits
//       (plus Code128/QR strings), bulk generation, scan history, and
//       beep-on-scan. BarcodeGeneratorDialog: UI for generating and printing
//       barcode labels. BarcodeScannerWidget: the older event-filter input
//       widget, superseded by BarcodeReader.
// HOW:  lookupByBarcode() queries the products table and returns ProductInfo
//       with a found flag; every scan is recorded to history. generateEAN13()
//       builds a 12-digit base from the product id and appends the standard
//       modulo-10 checksum. Signals (productFound/productNotFound) let UIs
//       react without polling.
// WHY:  Many small shops stock unlabelled goods; generating printable EAN-13
//       labels in-app removes the need for a GS1 subscription or external
//       tooling. Lookup/generation stays separate from BarcodeReader so the
//       same data logic serves any input source — scanner, keyboard, or test
//       button.
// =============================================================================
#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <QVector>
#include <qdialog.h>
#include <QWidget>
#include <QDialog>
#include <QTimer>
#include <QGroupBox>

// =============================================================================
// Barcode Scanner Manager - Handles barcode scanning and product lookup
// =============================================================================

class BarcodeScannerManager : public QObject
{
    Q_OBJECT

public:
    explicit BarcodeScannerManager(QSqlDatabase &db, QObject *parent = nullptr);

    // Product lookup by barcode
    struct ProductInfo {
        int productId;
        QString productName;
        QString category;
        double regularPrice;
        double happyHourPrice;
        int currentStock;
        QString barcode;
        bool found;
    };

    // Scan and lookup product
    ProductInfo lookupByBarcode(const QString &barcode);

    // Barcode management
    bool assignBarcode(int productId, const QString &barcode);
    bool removeBarcode(int productId);
    QString getBarcode(int productId) const;
    bool barcodeExists(const QString &barcode) const;

    // Generate barcodes
    QString generateEAN13(int productId);
    QString generateCode128(int productId);
    QString generateQRCode(int productId);

    // Bulk operations
    bool generateBarcodesForAllProducts();
    int getProductsWithoutBarcodes() const;

    // Scanner configuration
    void setAutoAddToCart(bool enabled) { m_autoAddToCart = enabled; }
    bool autoAddToCart() const { return m_autoAddToCart; }

    void setScanBeepEnabled(bool enabled) { m_beepOnScan = enabled; }
    bool scanBeepEnabled() const { return m_beepOnScan; }

    // Scan history
    struct ScanRecord {
        QString barcode;
        int productId;
        QDateTime scanTime;
        bool successful;
    };

    QVector<ScanRecord> getScanHistory(int limit = 100) const;
    void clearScanHistory();

signals:
    void barcodeScanned(QString barcode);
    void productFound(int productId, QString productName, double price);
    void productNotFound(QString barcode);
    void scanError(QString errorMessage);
    void barcodeAssigned(int productId, QString barcode);

public slots:
    void processScan(const QString &barcode);

private:
    void createTablesIfNotExist();
    void recordScan(const QString &barcode, int productId, bool successful);
    QString calculateEAN13Checksum(const QString &base12);
    void playBeep();

    QSqlDatabase &m_db;
    bool m_autoAddToCart;
    bool m_beepOnScan;
    QVector<ScanRecord> m_scanHistory;
};

// =============================================================================
// Barcode Generator Dialog - UI for generating and printing barcodes
// =============================================================================

class QLineEdit;
class QPushButton;
class QComboBox;
class QLabel;
class QSpinBox;

class BarcodeGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BarcodeGeneratorDialog(BarcodeScannerManager *manager,
                                    QSqlDatabase &db,
                                    QWidget *parent = nullptr);

private slots:
    void onProductSelected(int index);
    void onGenerateBarcode();
    void onPrintLabel();
    void onBulkGenerate();
    void onTestScan();

private:
    void setupUi();
    void loadProducts();
    void displayBarcode(const QString &barcode);
    void printBarcodeLabel(int productId, const QString &barcode);

    BarcodeScannerManager *m_manager;
    QSqlDatabase &m_db;
    int m_selectedProductId;
    QString m_currentBarcode;

    QComboBox *m_productCombo;
    QComboBox *m_barcodeTypeCombo;
    QLineEdit *m_barcodeEdit;
    QLabel *m_barcodeDisplay;
    QSpinBox *m_quantitySpin;
    QPushButton *m_generateButton;
    QPushButton *m_printButton;
    QPushButton *m_bulkButton;
};

// =============================================================================
// Barcode Scanner Input Widget - Captures barcode scanner input
// =============================================================================

class BarcodeScannerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BarcodeScannerWidget(BarcodeScannerManager *manager, QWidget *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

signals:
    void productScanned(int productId, QString productName, double price);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onProductFound(int productId, QString productName, double price);
    void onProductNotFound(QString barcode);
    void onTimeout();

private:
    void setupUi();
    void processAccumulatedInput();

    BarcodeScannerManager *m_manager;
    bool m_enabled;
    QString m_accumulatedInput;
    QTimer *m_inputTimer;

    QLabel *m_statusLabel;
    QLineEdit *m_barcodeDisplay;
};
