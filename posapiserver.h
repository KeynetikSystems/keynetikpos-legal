// =============================================================================
// posapiserver.h — PosApiServer: LAN-local HTTP API for the mobile scanner
// -----------------------------------------------------------------------------
// WHAT: A small HTTP server (QHttpServer) exposing /health, /pair, /scan
//       (scan an existing product into the till's current cart),
//       /inventory/categories + /inventory/products (create a new product
//       from a scanned barcode), and /sales/sync (commit a sale the phone
//       already completed offline, in standalone mode) — a phone on the
//       shop's WiFi pairs once and can sell, stock-in, or flush a queue.
//       Not a general POS API.
// HOW:  QHttpServer is signal/event-loop driven (bound to a QTcpServer), so
//       this runs on the app's existing single-threaded event loop — the same
//       model as the existing QNetworkAccessManager-based clients elsewhere
//       (licensemanager.cpp, mpesaclient.cpp). No new threading.
// WHY:  A phone can't open another machine's SQLite file, so *something* has
//       to sit between it and the till's data. This is deliberately the
//       smallest possible version of that: LAN-only, single paired device,
//       no cloud — see the milestone-1 plan for what's explicitly deferred
//       (multi-location sync). /sales/sync exists so standalone mode's local
//       queue has a till-authoritative place to land: totals are always
//       recomputed here via computeCartTotals(), never trusted from the
//       phone's own (separately implemented) cart_totals.dart.
// =============================================================================
#ifndef POSAPISERVER_H
#define POSAPISERVER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "database.h"          // Database
#include "checkoutservice.h"   // CheckoutService (value member, needs the full type)

class InventoryManager;
class CartService;
class SettingsManager;
class QHttpServer;
class QTcpServer;
class QHttpServerRequest;

// Pairing is in-memory only and single-device: a random PIN is shown on the
// till (MobileScannerDialog); POST /pair exchanges it for a bearer token, and
// regenerating the PIN invalidates the previous token. This is a companion
// for one till, not a multi-device fleet.
class PosApiServer : public QObject
{
    Q_OBJECT

public:
    explicit PosApiServer(Database &db, InventoryManager &inventory,
                          CartService &cart, SettingsManager &settings,
                          QObject *parent = nullptr);
    ~PosApiServer() override;

    // Starts listening on the given port across all local interfaces. Call
    // from runDeferredStartup(), not the constructor, so opening the listen
    // socket never delays first paint. Returns false if the port is already
    // in use (server is left not-listening; caller should surface this).
    bool listen(quint16 port);
    bool isListening() const;
    quint16 port() const { return m_port; }

    // LAN-facing IPv4 addresses this machine can be reached on (for display
    // in MobileScannerDialog) — excludes loopback.
    static QStringList lanAddresses();

    QString currentPin() const { return m_pin; }
    bool    isPaired()   const { return !m_token.isEmpty(); }

public slots:
    // Generates a fresh 6-digit PIN and invalidates any current pairing.
    void regeneratePin();

signals:
    void devicePaired();                        // a phone exchanged PIN for a token
    void pinRegenerated(const QString &pin);

private:
    void setupRoutes();
    bool isAuthorized(const QHttpServerRequest &request) const;
    static QString generatePin();
    static QString generateToken();

    Database         &m_db;
    InventoryManager &m_inventory;
    CartService      &m_cart;
    SettingsManager  &m_settings;

    // Own instance (null printer) for committing sales flushed from a
    // phone's offline queue — never MainWindow's, so a sale synced hours
    // after the fact doesn't physically print a receipt at the till.
    CheckoutService m_syncCheckout;

    QHttpServer *m_server    { nullptr };
    QTcpServer  *m_tcpServer { nullptr };
    quint16      m_port      { 0 };

    QString m_pin;
    QString m_token;   // empty = not paired
};

#endif // POSAPISERVER_H
