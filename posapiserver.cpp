#include "posapiserver.h"

#include "productrepository.h"
#include "salerepository.h"
#include "inventorymanager.h"
#include "cartservice.h"
#include "barcodescan.h"
#include "settingsmanager.h"
#include "usermanager.h"
#include "money.h"

#include <QHttpServer>
#include <QTcpServer>
#include <QNetworkInterface>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

namespace {

QJsonObject productJson(const Product &p)
{
    QJsonObject o;
    o["name"]           = p.name;
    o["priceCents"]     = static_cast<qint64>(p.price.cents());
    o["priceFormatted"] = formatMoney(p.price);
    return o;
}

// Sales flushed from a phone's offline queue have no till-side login to
// attribute them to — companion pairing is device-only, no per-user
// identity — so they're stamped with a fixed, clearly-labelled operator
// rather than borrowing whichever cashier happens to be logged into the
// till when the sync happens to run.
OperatorContext syncOperatorContext()
{
    OperatorContext op;
    op.username = QStringLiteral("mobile-sync");
    op.fullName = QStringLiteral("Mobile Sync");
    op.log = [](const QString &action, const QString &details) {
        UserManager::instance().logUserAction(action, details);
    };
    return op;
}

} // namespace

PosApiServer::PosApiServer(Database &db, InventoryManager &inventory,
                           CartService &cart, SettingsManager &settings,
                           QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_inventory(inventory)
    , m_cart(cart)
    , m_settings(settings)
    , m_syncCheckout(db, &inventory, /*printer=*/nullptr, syncOperatorContext())
    , m_pin(generatePin())
{
}

PosApiServer::~PosApiServer()
{
    delete m_server;
    // m_tcpServer is reparented to m_server by QAbstractHttpServer::bind(),
    // so it is destroyed as m_server's child — not deleted separately here.
}

QString PosApiServer::generatePin()
{
    return QString::number(QRandomGenerator::global()->bounded(1000000)).rightJustified(6, '0');
}

QString PosApiServer::generateToken()
{
    static const char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    QString token;
    token.reserve(32);
    for (int i = 0; i < 32; ++i)
        token.append(QLatin1Char(kAlphabet[QRandomGenerator::global()->bounded(36)]));
    return token;
}

void PosApiServer::regeneratePin()
{
    m_pin = generatePin();
    m_token.clear();   // force re-pairing after a PIN reset
    emit pinRegenerated(m_pin);
}

QStringList PosApiServer::lanAddresses()
{
    QStringList out;
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &addr : addresses) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
            out << addr.toString();
    }
    return out;
}

bool PosApiServer::listen(quint16 port)
{
    if (m_server)
        return isListening();

    m_server = new QHttpServer(this);
    setupRoutes();

    auto *tcpServer = new QTcpServer(this);
    if (!tcpServer->listen(QHostAddress::Any, port) || !m_server->bind(tcpServer)) {
        delete tcpServer;
        delete m_server;
        m_server = nullptr;
        return false;
    }

    m_tcpServer = tcpServer;
    m_port = port;
    return true;
}

bool PosApiServer::isListening() const
{
    return m_tcpServer && m_tcpServer->isListening();
}

bool PosApiServer::isAuthorized(const QHttpServerRequest &request) const
{
    if (m_token.isEmpty())
        return false;
    const QString auth = QString::fromUtf8(request.value("Authorization"));
    return auth == QStringLiteral("Bearer ") + m_token;
}

void PosApiServer::setupRoutes()
{
    m_server->route("/health", QHttpServerRequest::Method::Get,
        [](const QHttpServerRequest &) {
            return QHttpServerResponse(QJsonObject{{"ok", true}, {"app", "KeynetikPOS"}});
        });

    m_server->route("/pair", QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &request) {
            const QJsonObject body = QJsonDocument::fromJson(request.body()).object();
            const QString pin = body.value("pin").toString();

            if (pin.isEmpty() || pin != m_pin) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "invalid_pin"}},
                    QHttpServerResponse::StatusCode::Unauthorized);
            }

            m_token = generateToken();
            emit devicePaired();

            return QHttpServerResponse(QJsonObject{{"ok", true}, {"token", m_token}});
        });

    m_server->route("/scan", QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &request) {
            if (!isAuthorized(request)) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "not_paired"}},
                    QHttpServerResponse::StatusCode::Unauthorized);
            }

            const QJsonObject body = QJsonDocument::fromJson(request.body()).object();
            const BarcodeScanResult result =
                addBarcodeToCart(body.value("barcode").toString(),
                                 m_db, m_inventory, m_cart);

            switch (result.status) {
            case BarcodeScanResult::Status::NotFound:
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "not_found"}},
                    QHttpServerResponse::StatusCode::NotFound);
            case BarcodeScanResult::Status::OutOfStock:
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "out_of_stock"},
                                {"product", productJson(result.product)}},
                    QHttpServerResponse::StatusCode::Conflict);
            case BarcodeScanResult::Status::Added:
                return QHttpServerResponse(
                    QJsonObject{{"ok", true}, {"merged", result.merged},
                                {"product", productJson(result.product)}});
            }
            return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
        });

    m_server->route("/inventory/categories", QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest &request) {
            if (!isAuthorized(request)) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "not_paired"}},
                    QHttpServerResponse::StatusCode::Unauthorized);
            }

            QJsonArray categories;
            for (const QString &c : m_db.products().getAllCategories())
                categories.append(c);
            return QHttpServerResponse(QJsonObject{{"ok", true}, {"categories", categories}});
        });

    // Creates a new product from a scanned barcode + a short form filled in
    // on the phone (a barcode only ever carries an identifier, never a name/
    // price, so the phone always supplies the rest). Same bearer-token gate
    // as /scan — see posapiserver.h's WHY for the trust-model reasoning.



    // Commits a sale standalone mode already completed offline. Every line's
    // price is the phone's captured unitPriceCents (what the customer
    // actually paid), never today's till price; totals are always
    // recomputed here via computeCartTotals() with the till's own current
    // tax settings, never trusted from the phone's separate cart_totals.dart
    // — that recompute is what actually closes the two-implementations gap,
    // not just having a shared test fixture for it (see carttotals_test.cpp).
    m_server->route("/sales/sync", QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &request) {
            if (!isAuthorized(request)) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "not_paired"}},
                    QHttpServerResponse::StatusCode::Unauthorized);
            }

            const QJsonObject body = QJsonDocument::fromJson(request.body()).object();
            const QString idempotencyKey = body.value("idempotencyKey").toString().trimmed();
            if (idempotencyKey.isEmpty()) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "validation_error"},
                                {"field", "idempotencyKey"}},
                    QHttpServerResponse::StatusCode::BadRequest);
            }

            // A retried flush (the phone never saw the first response, or
            // resent after a timeout) returns the original outcome instead
            // of recording the sale twice.
            const int existingSaleId = m_db.sales().getSaleIdByExternalRef(idempotencyKey);
            if (existingSaleId > 0) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", true}, {"saleId", existingSaleId}});
            }

            const QJsonArray itemsJson = body.value("items").toArray();
            if (itemsJson.isEmpty()) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "validation_error"}, {"field", "items"}},
                    QHttpServerResponse::StatusCode::BadRequest);
            }

            Cart cart(0, QStringLiteral("Mobile Sync"));
            for (const QJsonValue &v : itemsJson) {
                const QJsonObject item = v.toObject();
                const QString barcode = item.value("barcode").toString();
                const int quantity = item.value("quantity").toInt();
                const qint64 unitPriceCents = item.value("unitPriceCents").toInteger(-1);

                if (barcode.isEmpty() || quantity <= 0 || unitPriceCents < 0) {
                    return QHttpServerResponse(
                        QJsonObject{{"ok", false}, {"error", "validation_error"},
                                    {"field", "items"}},
                        QHttpServerResponse::StatusCode::BadRequest);
                }

                const Product product = m_db.products().getProductByBarcode(barcode);
                if (product.id <= 0) {
                    return QHttpServerResponse(
                        QJsonObject{{"ok", false}, {"error", "product_not_found"},
                                    {"barcode", barcode}},
                        QHttpServerResponse::StatusCode::NotFound);
                }
                if (!m_inventory.canSell(product.id, quantity)) {
                    return QHttpServerResponse(
                        QJsonObject{{"ok", false}, {"error", "insufficient_stock"},
                                    {"barcode", barcode}, {"product", productJson(product)}},
                        QHttpServerResponse::StatusCode::Conflict);
                }

                // Phone-captured price, not product.price — the customer
                // already paid what they paid; a till-side price change
                // since shouldn't retroactively rewrite history.
                cart.items.append(CartItem(product.id, product.name,
                                           Money::fromCents(unitPriceCents), quantity,
                                           product.category, product.costPrice));
            }

            cart.discount = Money::fromCents(qMax<qint64>(0, body.value("discountCents").toInteger(0)));
            cart.discountReason = body.value("discountReason").toString();

            const QJsonArray tendersJson = body.value("tenders").toArray();
            if (tendersJson.isEmpty()) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "validation_error"}, {"field", "tenders"}},
                    QHttpServerResponse::StatusCode::BadRequest);
            }
            QVector<SalePayment> payments;
            Money amountPaid = Money::fromCents(0);
            for (const QJsonValue &v : tendersJson) {
                const QJsonObject t = v.toObject();
                SalePayment p;
                p.method    = t.value("method").toString();
                p.amount    = Money::fromCents(t.value("amountCents").toInteger(0));
                p.reference = t.value("reference").toString();
                payments.append(p);
                amountPaid += p.amount;
            }
            const QString paymentMethod =
                payments.size() == 1 ? payments.first().method : QStringLiteral("Split");

            // Authoritative recompute — never the total the phone already
            // showed the customer while offline.
            const CartTotals totals = computeCartTotals(cart.getSubtotal(), cart.discount,
                                                         m_settings.settings());
            const Money change = amountPaid > totals.total ? amountPaid - totals.total
                                                            : Money::fromCents(0);

            const CheckoutResult result = m_syncCheckout.finalizeSale(
                cart, totals, paymentMethod, QString(), amountPaid, change,
                /*customerId=*/0, /*storeCreditUsed=*/Money::fromCents(0), payments);

            if (!result.ok) {
                return QHttpServerResponse(
                    QJsonObject{{"ok", false}, {"error", "save_failed"}, {"detail", result.error}},
                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            m_db.sales().setExternalRef(result.saleId, idempotencyKey);

            return QHttpServerResponse(
                QJsonObject{{"ok", true}, {"saleId", result.saleId}},
                QHttpServerResponse::StatusCode::Created);
        });
}
