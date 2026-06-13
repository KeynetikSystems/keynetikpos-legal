// =============================================================================
// barcodereader.cpp — Implementation of BarcodeReader (see barcodereader.h for
// the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Keyboard-wedge mode buffers keystrokes via eventFilter(); Enter/Return
//    flushes the buffer, and the single-shot inter-character timer flushes
//    even without a terminator (scanners burst ~1 ms/char, humans can't).
//  - Serial mode configures 8-N-1 with no flow control (the near-universal
//    scanner default) and splits the incoming byte stream on CR/LF.
//  - processRawBarcode() strips the configured prefix/suffix before emitting
//    barcodeScanned().
// =============================================================================
#include "barcodeReader.h"

#include <QEvent>
#include <QKeyEvent>
#include <QDebug>

// ── Constructor / Destructor ─────────────────────────────────────────────────

BarcodeReader::BarcodeReader(QObject *parent)
    : QObject(parent)
{
    // Timer used in KeyboardWedge mode to detect end-of-barcode
    m_interCharTimer = new QTimer(this);
    m_interCharTimer->setSingleShot(true);
    connect(m_interCharTimer, &QTimer::timeout,
            this,             &BarcodeReader::onInterCharTimeout);
}

BarcodeReader::~BarcodeReader()
{
    close();
}

// ── Configuration ────────────────────────────────────────────────────────────

void BarcodeReader::setInputMode(InputMode mode)   { m_mode = mode; }
BarcodeReader::InputMode BarcodeReader::inputMode() const { return m_mode; }

void BarcodeReader::setPortName(const QString &portName)   { m_portName = portName; }
QString BarcodeReader::portName() const                    { return m_portName; }

void BarcodeReader::setBaudRate(QSerialPort::BaudRate baudRate) { m_baudRate = baudRate; }

void BarcodeReader::setPrefix(const QString &prefix) { m_prefix = prefix; }
void BarcodeReader::setSuffix(const QString &suffix) { m_suffix = suffix; }

void BarcodeReader::setInterCharTimeout(int ms)
{
    m_interCharTimeoutMs = ms;
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

bool BarcodeReader::open()
{
    if (m_mode == InputMode::KeyboardWedge) {
        // Nothing to "open" — the caller must do:
        //   qApp->installEventFilter(reader);
        qDebug() << "[BarcodeReader] Keyboard-wedge mode active. "
                    "Install as event filter on QApplication.";
        return true;
    }

    // Serial mode
    if (m_portName.isEmpty()) {
        emit errorOccurred("No serial port name configured.");
        return false;
    }

    if (!m_serial) {
        m_serial = new QSerialPort(this);
        connect(m_serial, &QSerialPort::readyRead,
                this,     &BarcodeReader::onSerialDataReady);
        connect(m_serial, &QSerialPort::errorOccurred,
                this, [this](QSerialPort::SerialPortError err) {
                    if (err != QSerialPort::NoError)
                        emit errorOccurred(m_serial->errorString());
                });
    }

    m_serial->setPortName(m_portName);
    m_serial->setBaudRate(m_baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("Cannot open %1: %2")
                               .arg(m_portName, m_serial->errorString()));
        return false;
    }

    qDebug() << "[BarcodeReader] Serial port" << m_portName << "opened.";
    return true;
}

void BarcodeReader::close()
{
    m_interCharTimer->stop();
    m_keyBuffer.clear();
    m_serialBuffer.clear();

    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
        qDebug() << "[BarcodeReader] Serial port closed.";
    }
}

bool BarcodeReader::isOpen() const
{
    if (m_mode == InputMode::KeyboardWedge)
        return true; // always "open" once the event filter is installed
    return m_serial && m_serial->isOpen();
}

QStringList BarcodeReader::availablePorts()
{
    QStringList ports;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ports << info.portName();
    return ports;
}

// ── Keyboard-wedge event filter ──────────────────────────────────────────────

bool BarcodeReader::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);

    auto *keyEvent = static_cast<QKeyEvent *>(event);

    // Scanners terminate with Enter or Return
    if (keyEvent->key() == Qt::Key_Return ||
        keyEvent->key() == Qt::Key_Enter)
    {
        m_interCharTimer->stop();
        if (!m_keyBuffer.isEmpty()) {
            processRawBarcode(m_keyBuffer);
            m_keyBuffer.clear();
        }
        // Consume the event so it doesn't trigger buttons etc.
        return true;
    }

    const QString ch = keyEvent->text();
    if (!ch.isEmpty()) {
        m_keyBuffer += ch;
        // Reset inter-character timer on every keystroke
        m_interCharTimer->start(m_interCharTimeoutMs);
    }

    // Do NOT consume regular keystrokes — pass through so manual typing works
    return QObject::eventFilter(watched, event);
}

// ── Serial data handler ───────────────────────────────────────────────────────

void BarcodeReader::onSerialDataReady()
{
    m_serialBuffer += m_serial->readAll();

    // Most serial scanners terminate with CR (\r), LF (\n), or CR+LF
    while (true) {
        int crIdx = m_serialBuffer.indexOf('\r');
        int lfIdx = m_serialBuffer.indexOf('\n');

        int termIdx = -1;
        if (crIdx >= 0 && lfIdx >= 0)
            termIdx = qMin(crIdx, lfIdx);
        else if (crIdx >= 0)
            termIdx = crIdx;
        else if (lfIdx >= 0)
            termIdx = lfIdx;

        if (termIdx < 0)
            break; // no complete barcode yet

        QByteArray raw = m_serialBuffer.left(termIdx);
        // Skip past terminator(s)
        int skip = termIdx + 1;
        if (skip < m_serialBuffer.size() &&
            (m_serialBuffer[skip] == '\r' || m_serialBuffer[skip] == '\n'))
            ++skip;
        m_serialBuffer = m_serialBuffer.mid(skip);

        if (!raw.isEmpty())
            processRawBarcode(QString::fromLatin1(raw));
    }
}

// ── Inter-character timeout (keyboard-wedge fallback) ────────────────────────

void BarcodeReader::onInterCharTimeout()
{
    if (!m_keyBuffer.isEmpty()) {
        processRawBarcode(m_keyBuffer);
        m_keyBuffer.clear();
    }
}

// ── Internal helpers ──────────────────────────────────────────────────────────

void BarcodeReader::processRawBarcode(const QString &raw)
{
    const QString cleaned = stripPrefixSuffix(raw.trimmed());
    if (!cleaned.isEmpty()) {
        qDebug() << "[BarcodeReader] Scanned:" << cleaned;
        emit barcodeScanned(cleaned);
    }
}

QString BarcodeReader::stripPrefixSuffix(const QString &raw) const
{
    QString result = raw;
    if (!m_prefix.isEmpty() && result.startsWith(m_prefix))
        result = result.mid(m_prefix.size());
    if (!m_suffix.isEmpty() && result.endsWith(m_suffix))
        result = result.chopped(m_suffix.size());
    return result;
}