// =============================================================================
// barcodescannermanager.cpp — Implementation of BarcodeScannerManager,
// BarcodeGeneratorDialog, and BarcodeScannerWidget (see barcodescannermanager.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - lookupByBarcode() queries products by barcode and records every scan
//    (hit or miss) in the in-memory scan history.
//  - generateEAN13() derives a 12-digit base from the product id and appends
//    the standard modulo-10 check digit (calculateEAN13Checksum), so generated
//    codes scan correctly on any EAN-13 reader.
//  - BarcodeScannerWidget is the LEGACY input path; new code should use
//    BarcodeReader (barcodereader.h) and keep this class for lookup/generation.
// =============================================================================
#include "barcodescannermanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QMessageBox>
#include <QKeyEvent>
#include <QDateTime>
#include <QApplication>
#include <QRandomGenerator>
#include <qgroupbox.h>
#include "appstyle.h"

// =============================================================================
// BarcodeScannerManager Implementation
// =============================================================================

BarcodeScannerManager::BarcodeScannerManager(QSqlDatabase &db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_autoAddToCart(true)
    , m_beepOnScan(true)
{
    createTablesIfNotExist();
}

void BarcodeScannerManager::createTablesIfNotExist()
{
    QSqlQuery query(m_db);

    // ProductBarcodes table
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS ProductBarcodes (
            BarcodeID INTEGER PRIMARY KEY AUTOINCREMENT,
            ProductID INTEGER NOT NULL UNIQUE,
            Barcode TEXT NOT NULL UNIQUE,
            BarcodeType TEXT DEFAULT 'EAN13',
            CreatedDate TEXT NOT NULL,
            FOREIGN KEY (ProductID) REFERENCES Products(ProductID)
        )
    )");

    // Scan history
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS ScanHistory (
            ScanID INTEGER PRIMARY KEY AUTOINCREMENT,
            Barcode TEXT NOT NULL,
            ProductID INTEGER,
            ScanTime TEXT NOT NULL,
            Successful INTEGER NOT NULL
        )
    )");
}

BarcodeScannerManager::ProductInfo BarcodeScannerManager::lookupByBarcode(const QString &barcode)
{
    ProductInfo info;
    info.found = false;
    info.barcode = barcode;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT p.ProductID, p.ProductName, p.Category, p.RegularPrice, p.HappyHourPrice,
               i.Quantity
        FROM Products p
        JOIN ProductBarcodes pb ON p.ProductID = pb.ProductID
        LEFT JOIN Inventory i ON p.ProductID = i.ProductID
        WHERE pb.Barcode = :barcode
    )");
    query.bindValue(":barcode", barcode);

    if (query.exec() && query.next()) {
        info.productId = query.value(0).toInt();
        info.productName = query.value(1).toString();
        info.category = query.value(2).toString();
        info.regularPrice = query.value(3).toDouble();
        info.happyHourPrice = query.value(4).toDouble();
        info.currentStock = query.value(5).toInt();
        info.found = true;

        recordScan(barcode, info.productId, true);
    } else {
        recordScan(barcode, -1, false);
    }

    return info;
}

void BarcodeScannerManager::processScan(const QString &barcode)
{
    emit barcodeScanned(barcode);

    if (m_beepOnScan) {
        playBeep();
    }

    ProductInfo info = lookupByBarcode(barcode);

    if (info.found) {
        emit productFound(info.productId, info.productName, info.regularPrice);
    } else {
        emit productNotFound(barcode);
    }
}

bool BarcodeScannerManager::assignBarcode(int productId, const QString &barcode)
{
    if (barcodeExists(barcode)) {
        emit scanError("Barcode already exists");
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO ProductBarcodes (ProductID, Barcode, BarcodeType, CreatedDate)
        VALUES (:productId, :barcode, 'EAN13', :date)
    )");
    query.bindValue(":productId", productId);
    query.bindValue(":barcode", barcode);
    query.bindValue(":date", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (query.exec()) {
        emit barcodeAssigned(productId, barcode);
        return true;
    }

    qDebug() << "Error assigning barcode:" << query.lastError().text();
    return false;
}

bool BarcodeScannerManager::removeBarcode(int productId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM ProductBarcodes WHERE ProductID = :id");
    query.bindValue(":id", productId);
    return query.exec();
}

QString BarcodeScannerManager::getBarcode(int productId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT Barcode FROM ProductBarcodes WHERE ProductID = :id");
    query.bindValue(":id", productId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return QString();
}

bool BarcodeScannerManager::barcodeExists(const QString &barcode) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM ProductBarcodes WHERE Barcode = :barcode");
    query.bindValue(":barcode", barcode);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QString BarcodeScannerManager::generateEAN13(int productId)
{
    // Generate 12-digit base (country code + manufacturer + product)
    QString base = QString("590%1").arg(productId, 9, 10, QChar('0'));

    // Calculate checksum
    QString checksum = calculateEAN13Checksum(base);

    return base + checksum;
}

QString BarcodeScannerManager::calculateEAN13Checksum(const QString &base12)
{
    int sum = 0;
    for (int i = 0; i < 12; i++) {
        int digit = base12.mid(i, 1).toInt();
        sum += (i % 2 == 0) ? digit : digit * 3;
    }

    int checksum = (10 - (sum % 10)) % 10;
    return QString::number(checksum);
}

QString BarcodeScannerManager::generateCode128(int productId)
{
    // Simple Code128 format (for display purposes)
    return QString("CODE128-%1").arg(productId, 8, 10, QChar('0'));
}

QString BarcodeScannerManager::generateQRCode(int productId)
{
    // QR code data format
    return QString("PRODUCT-%1").arg(productId);
}

bool BarcodeScannerManager::generateBarcodesForAllProducts()
{
    QSqlQuery query(m_db);
    query.exec(R"(
        SELECT ProductID FROM Products
        WHERE ProductID NOT IN (SELECT ProductID FROM ProductBarcodes)
    )");

    int count = 0;
    while (query.next()) {
        int productId = query.value(0).toInt();
        QString barcode = generateEAN13(productId);
        if (assignBarcode(productId, barcode)) {
            count++;
        }
    }

    return count > 0;
}

int BarcodeScannerManager::getProductsWithoutBarcodes() const
{
    QSqlQuery query(m_db);
    query.exec(R"(
        SELECT COUNT(*) FROM Products
        WHERE ProductID NOT IN (SELECT ProductID FROM ProductBarcodes)
    )");

    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

void BarcodeScannerManager::recordScan(const QString &barcode, int productId, bool successful)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO ScanHistory (Barcode, ProductID, ScanTime, Successful)
        VALUES (:barcode, :productId, :time, :success)
    )");
    query.bindValue(":barcode", barcode);
    query.bindValue(":productId", productId > 0 ? productId : QVariant());
    query.bindValue(":time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":success", successful ? 1 : 0);
    query.exec();

    // Add to in-memory history
    ScanRecord record;
    record.barcode = barcode;
    record.productId = productId;
    record.scanTime = QDateTime::currentDateTime();
    record.successful = successful;
    m_scanHistory.prepend(record);

    // Keep only last 100
    if (m_scanHistory.size() > 100) {
        m_scanHistory.removeLast();
    }
}

QVector<BarcodeScannerManager::ScanRecord> BarcodeScannerManager::getScanHistory(int limit) const
{
    QVector<ScanRecord> history;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT Barcode, ProductID, ScanTime, Successful
        FROM ScanHistory
        ORDER BY ScanTime DESC
        LIMIT :limit
    )");
    query.bindValue(":limit", limit);

    if (query.exec()) {
        while (query.next()) {
            ScanRecord record;
            record.barcode = query.value(0).toString();
            record.productId = query.value(1).toInt();
            record.scanTime = QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
            record.successful = query.value(3).toInt() == 1;
            history.append(record);
        }
    }

    return history;
}

void BarcodeScannerManager::clearScanHistory()
{
    QSqlQuery query(m_db);
    query.exec("DELETE FROM ScanHistory");
    m_scanHistory.clear();
}

void BarcodeScannerManager::playBeep()
{
    QApplication::beep();
}

// =============================================================================
// BarcodeGeneratorDialog Implementation
// =============================================================================

BarcodeGeneratorDialog::BarcodeGeneratorDialog(BarcodeScannerManager *manager,
                                               QSqlDatabase &db,
                                               QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_db(db)
    , m_selectedProductId(-1)
{
    setWindowTitle("Barcode Generator");
    resize(600, 400);
    setupUi();
    loadProducts();
}

void BarcodeGeneratorDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    // Product selection
    auto *selectionLayout = new QFormLayout;

    m_productCombo = new QComboBox(this);
    connect(m_productCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BarcodeGeneratorDialog::onProductSelected);
    selectionLayout->addRow("Product:", m_productCombo);

    m_barcodeTypeCombo = new QComboBox(this);
    m_barcodeTypeCombo->addItem("EAN-13");
    m_barcodeTypeCombo->addItem("Code 128");
    m_barcodeTypeCombo->addItem("QR Code");
    selectionLayout->addRow("Barcode Type:", m_barcodeTypeCombo);

    layout->addLayout(selectionLayout);

    // Current barcode display
    auto *displayGroup = new QGroupBox("Current Barcode", this);
    auto *displayLayout = new QVBoxLayout(displayGroup);

    m_barcodeEdit = new QLineEdit(this);
    m_barcodeEdit->setReadOnly(true);
    m_barcodeEdit->setProperty("textScale", "xl");
    m_barcodeEdit->setProperty("bold", "true");
    displayLayout->addWidget(m_barcodeEdit);

    m_barcodeDisplay = new QLabel(this);
    m_barcodeDisplay->setAlignment(Qt::AlignCenter);
    m_barcodeDisplay->setMinimumHeight(100);
    m_barcodeDisplay->setStyleSheet("border: 1px solid #ccc; background: white;");
    displayLayout->addWidget(m_barcodeDisplay);

    layout->addWidget(displayGroup);

    // Action buttons
    auto *buttonLayout = new QHBoxLayout;

    m_generateButton = new QPushButton("Generate New Barcode", this);
    m_generateButton->setProperty("kind", "primary");
    connect(m_generateButton, &QPushButton::clicked, this, &BarcodeGeneratorDialog::onGenerateBarcode);
    buttonLayout->addWidget(m_generateButton);

    m_printButton = new QPushButton("Print Label", this);
    m_printButton->setProperty("kind", "info");
    m_printButton->setEnabled(false);
    connect(m_printButton, &QPushButton::clicked, this, &BarcodeGeneratorDialog::onPrintLabel);
    buttonLayout->addWidget(m_printButton);

    layout->addLayout(buttonLayout);

    // Bulk operations
    auto *bulkLayout = new QHBoxLayout;

    m_bulkButton = new QPushButton("Generate for All Products", this);
    m_bulkButton->setProperty("kind", "warning");
    connect(m_bulkButton, &QPushButton::clicked, this, &BarcodeGeneratorDialog::onBulkGenerate);
    bulkLayout->addWidget(m_bulkButton);

    auto *testButton = new QPushButton("Test Scanner", this);
    testButton->setProperty("kind", "tertiary");
    connect(testButton, &QPushButton::clicked, this, &BarcodeGeneratorDialog::onTestScan);
    bulkLayout->addWidget(testButton);

    layout->addLayout(bulkLayout);

    // Close button
    auto *closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeButton);
}

void BarcodeGeneratorDialog::loadProducts()
{
    m_productCombo->clear();

    QSqlQuery query(m_db);
    query.exec("SELECT ProductID, ProductName FROM Products ORDER BY ProductName");

    while (query.next()) {
        int productId = query.value(0).toInt();
        QString productName = query.value(1).toString();
        m_productCombo->addItem(productName, productId);
    }
}

void BarcodeGeneratorDialog::onProductSelected(int index)
{
    if (index < 0) return;

    m_selectedProductId = m_productCombo->itemData(index).toInt();

    // Check if product already has barcode
    QString existingBarcode = m_manager->getBarcode(m_selectedProductId);

    if (!existingBarcode.isEmpty()) {
        m_currentBarcode = existingBarcode;
        displayBarcode(existingBarcode);
        m_printButton->setEnabled(true);
        m_generateButton->setText("Regenerate Barcode");
    } else {
        m_barcodeEdit->clear();
        m_barcodeDisplay->setText("No barcode assigned");
        m_printButton->setEnabled(false);
        m_generateButton->setText("Generate New Barcode");
    }
}

void BarcodeGeneratorDialog::onGenerateBarcode()
{
    if (m_selectedProductId < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a product first.");
        return;
    }

    QString barcode;
    QString type = m_barcodeTypeCombo->currentText();

    if (type == "EAN-13") {
        barcode = m_manager->generateEAN13(m_selectedProductId);
    } else if (type == "Code 128") {
        barcode = m_manager->generateCode128(m_selectedProductId);
    } else {
        barcode = m_manager->generateQRCode(m_selectedProductId);
    }

    if (m_manager->assignBarcode(m_selectedProductId, barcode)) {
        m_currentBarcode = barcode;
        displayBarcode(barcode);
        m_printButton->setEnabled(true);

        QMessageBox::information(this, "Success",
                                 QString("Barcode %1 assigned to product").arg(barcode));
    } else {
        QMessageBox::critical(this, "Error", "Failed to assign barcode");
    }
}

void BarcodeGeneratorDialog::displayBarcode(const QString &barcode)
{
    m_barcodeEdit->setText(barcode);

    // Display as large text (in real app, would render actual barcode image)
    QString display = QString("<div style='font-family: \"Libre Barcode 128\", monospace; "
                              "font-size: 48pt; text-align: center;'>%1</div>"
                              "<div style='text-align: center; font-size: 14pt;'>%2</div>")
                          .arg(barcode, barcode);

    m_barcodeDisplay->setText(display);
}

void BarcodeGeneratorDialog::onPrintLabel()
{
    if (m_currentBarcode.isEmpty()) return;

    // In a real implementation, this would send to printer
    QMessageBox::information(this, "Print Label",
                             QString("Printing barcode label for:\n%1\nBarcode: %2")
                                 .arg(m_productCombo->currentText(), m_currentBarcode));
}

void BarcodeGeneratorDialog::onBulkGenerate()
{
    int missing = m_manager->getProductsWithoutBarcodes();

    if (missing == 0) {
        QMessageBox::information(this, "All Set",
                                 "All products already have barcodes assigned!");
        return;
    }

    auto reply = QMessageBox::question(this, "Bulk Generate",
                                       QString("Generate barcodes for %1 products?").arg(missing),
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (m_manager->generateBarcodesForAllProducts()) {
            QMessageBox::information(this, "Success",
                                     QString("Generated barcodes for %1 products").arg(missing));
            loadProducts();
        }
    }
}

void BarcodeGeneratorDialog::onTestScan()
{
    if (m_currentBarcode.isEmpty()) {
        QMessageBox::warning(this, "No Barcode", "Generate a barcode first");
        return;
    }

    m_manager->processScan(m_currentBarcode);
}

// =============================================================================
// BarcodeScannerWidget Implementation
// =============================================================================

BarcodeScannerWidget::BarcodeScannerWidget(BarcodeScannerManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
    , m_enabled(true)
{
    setupUi();

    m_inputTimer = new QTimer(this);
    m_inputTimer->setSingleShot(true);
    m_inputTimer->setInterval(100);
    connect(m_inputTimer, &QTimer::timeout, this, &BarcodeScannerWidget::onTimeout);

    connect(m_manager, &BarcodeScannerManager::productFound,
            this, &BarcodeScannerWidget::onProductFound);
    connect(m_manager, &BarcodeScannerManager::productNotFound,
            this, &BarcodeScannerWidget::onProductNotFound);

    qApp->installEventFilter(this);
}

void BarcodeScannerWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("Scanner Ready", this);
    m_statusLabel->setProperty("role", "chip");
    m_statusLabel->setProperty("kind", "primary");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_barcodeDisplay = new QLineEdit(this);
    m_barcodeDisplay->setReadOnly(true);
    m_barcodeDisplay->setPlaceholderText("Scan barcode...");
    m_barcodeDisplay->setProperty("textScale", "lg");
    layout->addWidget(m_barcodeDisplay);
}

void BarcodeScannerWidget::setEnabled(bool enabled)
{
    m_enabled = enabled;
    m_statusLabel->setText(enabled ? "Scanner Ready" : "Scanner Disabled");
    setStyleProperty(m_statusLabel, "kind", enabled ? "primary" : "secondary");
}

bool BarcodeScannerWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_enabled) return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // Accumulate numeric input
        if (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_9) {
            m_accumulatedInput += keyEvent->text();
            m_barcodeDisplay->setText(m_accumulatedInput);
            m_inputTimer->start();
            return false; // Don't consume the event
        }

        // Enter key = process barcode
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (!m_accumulatedInput.isEmpty()) {
                processAccumulatedInput();
                return true; // Consume the event
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void BarcodeScannerWidget::processAccumulatedInput()
{
    m_inputTimer->stop();

    if (m_accumulatedInput.length() >= 8) { // Minimum barcode length
        m_statusLabel->setText("Processing...");
        setStyleProperty(m_statusLabel, "kind", "warning");

        m_manager->processScan(m_accumulatedInput);
    }

    m_accumulatedInput.clear();
}

void BarcodeScannerWidget::onTimeout()
{
    // Input timeout - clear accumulated input
    m_accumulatedInput.clear();
    m_barcodeDisplay->clear();
}

void BarcodeScannerWidget::onProductFound(int productId, QString productName, double price)
{
    m_statusLabel->setText(QString("✓ Found: %1").arg(productName));
    setStyleProperty(m_statusLabel, "kind", "primary");

    emit productScanned(productId, productName, price);

    // Reset after 2 seconds
    QTimer::singleShot(2000, this, [this]() {
        m_statusLabel->setText("Scanner Ready");
        m_barcodeDisplay->clear();
    });
}

void BarcodeScannerWidget::onProductNotFound(QString barcode)
{
    m_statusLabel->setText(QString("✗ Not Found: %1").arg(barcode));
    setStyleProperty(m_statusLabel, "kind", "danger");

    // Reset after 3 seconds
    QTimer::singleShot(3000, this, [this]() {
        m_statusLabel->setText("Scanner Ready");
        setStyleProperty(m_statusLabel, "kind", "primary");
        m_barcodeDisplay->clear();
    });
}
