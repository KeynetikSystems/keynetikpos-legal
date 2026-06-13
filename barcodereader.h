// =============================================================================
// barcodereader.h — BarcodeReader: hardware barcode-scanner input
// -----------------------------------------------------------------------------
// WHAT: The current scanner-input class (replaces BarcodeScannerManager for
//       input). Supports the two ways real scanners present themselves:
//       keyboard-wedge (USB HID — the scanner "types" the code + Enter) and
//       serial (RS-232 / virtual COM). Emits barcodeScanned(QString) with a
//       cleaned code, or errorOccurred.
// HOW:  Keyboard-wedge mode: installed as a Qt event filter (typically on
//       qApp), it buffers incoming key characters; Enter flushes the buffer,
//       and a single-shot inter-character timer (default 100 ms) flushes even
//       without a terminator — scanners burst chars ~1 ms apart, humans can't,
//       which is how scans are told apart from typing. Serial mode: opens a
//       QSerialPort (default 9600-8-N-1), accumulates on readyRead, splits on
//       CR/LF. Configurable prefix/suffix stripping removes STX/ETX framing.
// WHY:  Keyboard-wedge is the cheapest, most common scanner type and needs no
//       drivers, but raw keystrokes would land in whatever widget has focus —
//       the event-filter + timing approach captures scans application-wide
//       without stealing real typing. Serial covers older/industrial scanners.
//       One class for both modes means MainWindow connects a single signal
//       regardless of hardware.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QSerialPort>
#include <QSerialPortInfo>

/**
 * BarcodeReader — Qt C++ class for reading barcodes in a POS system.
 *
 * Supports two input modes:
 *   1. USB HID / Keyboard-wedge scanners  → use installEventFilter() on your
 *      QWidget/QApplication; the scanner types characters + Enter/Return.
 *   2. Serial (RS-232 / virtual COM) scanners → set mode to Serial and call
 *      open() with the desired port name.
 *
 * Signals:
 *   barcodeScanned(QString)  — emitted with a complete barcode string.
 *   errorOccurred(QString)   — emitted on serial/timeout errors.
 */
class BarcodeReader : public QObject
{
    Q_OBJECT

public:
    enum class InputMode {
        KeyboardWedge,   // USB HID; scanner acts as keyboard
        SerialPort       // RS-232 or virtual COM port
    };
    Q_ENUM(InputMode)

    explicit BarcodeReader(QObject *parent = nullptr);
    ~BarcodeReader() override;

    // ── Configuration ────────────────────────────────────────────────────────

    /** Set input mode before calling open(). Default: KeyboardWedge. */
    void setInputMode(InputMode mode);
    InputMode inputMode() const;

    /** Serial port name, e.g. "COM3" on Windows or "/dev/ttyUSB0" on Linux. */
    void setPortName(const QString &portName);
    QString portName() const;

    /** Serial baud rate. Common scanner default is 9600. */
    void setBaudRate(QSerialPort::BaudRate baudRate);

    /**
     * Prefix/suffix stripping: if the scanner adds a known prefix or suffix
     * (e.g. a STX/ETX byte), set them here so they are removed from output.
     */
    void setPrefix(const QString &prefix);
    void setSuffix(const QString &suffix);

    /**
     * Keyboard-wedge inter-character timeout (ms).
     * If no new character arrives within this window, the buffer is flushed.
     * Default: 100 ms — safe for most USB HID scanners (they burst at ~1 ms/char).
     */
    void setInterCharTimeout(int ms);

    // ── Lifecycle ────────────────────────────────────────────────────────────

    /** Open the serial port (SerialPort mode) or start accepting keyboard input. */
    bool open();

    /** Close the port / stop listening. */
    void close();

    bool isOpen() const;

    /** Returns a list of available serial port names on this system. */
    static QStringList availablePorts();

    // ── Keyboard-wedge event filter ──────────────────────────────────────────
    // Install this object as an event filter on QApplication (or a specific
    // widget) to intercept keystrokes before they reach any other widget:
    //
    //   qApp->installEventFilter(reader);
    //
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    /** Emitted with the full, cleaned barcode string. */
    void barcodeScanned(const QString &barcode);

    /** Emitted on error (serial open failure, read error, etc.). */
    void errorOccurred(const QString &errorMessage);

private slots:
    void onSerialDataReady();
    void onInterCharTimeout();

private:
    void processRawBarcode(const QString &raw);
    QString stripPrefixSuffix(const QString &raw) const;

    InputMode        m_mode            { InputMode::KeyboardWedge };
    QString          m_portName;
    QSerialPort     *m_serial          { nullptr };
    QSerialPort::BaudRate m_baudRate   { QSerialPort::Baud9600 };

    QString          m_prefix;
    QString          m_suffix;

    // Keyboard-wedge buffering
    QString          m_keyBuffer;
    QTimer          *m_interCharTimer  { nullptr };
    int              m_interCharTimeoutMs { 100 };

    // Serial buffering
    QByteArray       m_serialBuffer;
};