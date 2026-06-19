// =============================================================================
// mpesaclient.cpp — Implementation of MpesaClient (see mpesaclient.h).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Auth to the Worker reuses the till's license credentials (X-License-Key +
//    X-Device-Id), the same pair licensemanager.cpp uses for /validate.
//  - Polling: every kPollMs up to kMaxPolls (~75s, longer than the STK prompt's
//    own ~60s lifetime) so a slow customer is still captured before we give up.
//  - Friendly mapping turns the Worker's error slugs into cashier-readable text.
// =============================================================================
#include "mpesaclient.h"
#include "licensemanager.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QSysInfo>
#include <cmath>

namespace {
constexpr int kPollMs   = 3000;
constexpr int kMaxPolls = 25;     // 25 * 3s = 75s

QString friendlyError(const QString &slug)
{
    if (slug == "unauthorized")        return "This till isn't authorized for M-Pesa — check its licence/activation.";
    if (slug == "mpesa_not_configured")return "M-Pesa isn't set up on the server yet.";
    if (slug == "daraja_auth_failed")  return "Couldn't reach M-Pesa (Daraja authentication failed).";
    if (slug == "bad_request")         return "Invalid phone number or amount.";
    return slug.isEmpty() ? "M-Pesa request failed." : slug;
}

void addAuth(QNetworkRequest &req)
{
    req.setRawHeader("X-License-Key", LicenseManager::instance().licenseKey().toUtf8());
    req.setRawHeader("X-Device-Id",   QSysInfo::machineUniqueId());
}
} // namespace

MpesaClient::MpesaClient(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(kPollMs);
    connect(m_pollTimer, &QTimer::timeout, this, &MpesaClient::poll);
}

bool MpesaClient::isAvailable()
{
    return !LicenseManager::instance().licenseKey().trimmed().isEmpty();
}

void MpesaClient::requestPayment(const QString &phone, Money amount,
                                 const QString &accountRef)
{
    if (m_active) return;   // one push at a time

    if (!isAvailable()) {
        emit paymentFailed("This till isn't licensed; M-Pesa STK push needs an activated licence.");
        return;
    }

    const int shillings = static_cast<int>(std::llround(amount.toMajor()));
    if (shillings < 1) {
        emit paymentFailed("Amount must be at least KSh 1.");
        return;
    }

    m_active     = true;
    m_polls      = 0;
    m_checkoutId.clear();

    QJsonObject body;
    body["phone"]      = phone;
    body["amount"]     = shillings;
    body["accountRef"] = accountRef;

    QNetworkRequest req(QUrl(LicenseManager::serverBaseUrl() + "/pos/mpesa/stkpush"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    addAuth(req);

    emit statusChanged(QString("Sending M-Pesa prompt to %1…").arg(phone));
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onStkReply(reply); });
}

void MpesaClient::onStkReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_active) return;

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();

    if (reply->error() != QNetworkReply::NoError && obj.isEmpty()) {
        m_active = false;
        emit paymentFailed("Couldn't reach the server: " + reply->errorString());
        return;
    }
    if (!obj.value("ok").toBool()) {
        m_active = false;
        emit paymentFailed(friendlyError(obj.value("error").toString()));
        return;
    }

    m_checkoutId = obj.value("checkoutId").toString();
    if (m_checkoutId.isEmpty()) {
        m_active = false;
        emit paymentFailed("M-Pesa did not return a request id.");
        return;
    }

    emit promptSent(obj.value("customerMessage").toString(
        "Prompt sent — ask the customer to enter their M-Pesa PIN."));
    emit statusChanged("Waiting for the customer to authorize on their phone…");
    m_pollTimer->start();
}

void MpesaClient::poll()
{
    if (!m_active || m_checkoutId.isEmpty()) { m_pollTimer->stop(); return; }

    if (++m_polls > kMaxPolls) {
        m_pollTimer->stop();
        m_active = false;
        emit paymentFailed("Timed out waiting for M-Pesa confirmation. "
                           "If the customer was charged, enter the code manually.");
        return;
    }

    QUrl url(LicenseManager::serverBaseUrl() + "/pos/mpesa/status");
    QUrlQuery q; q.addQueryItem("checkout_id", m_checkoutId); url.setQuery(q);
    QNetworkRequest req(url);
    addAuth(req);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onStatusReply(reply); });
}

void MpesaClient::onStatusReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_active) return;

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    const QString status  = obj.value("status").toString();

    if (status == "success") {
        m_pollTimer->stop();
        m_active = false;
        emit paymentConfirmed(obj.value("receipt").toString());
    } else if (status == "failed") {
        m_pollTimer->stop();
        m_active = false;
        emit paymentFailed(obj.value("resultDesc").toString("The customer did not complete the payment."));
    }
    // "pending"/"unknown" → keep polling until success/failure/timeout.
}

void MpesaClient::cancel()
{
    m_pollTimer->stop();
    m_active = false;
    m_checkoutId.clear();
}
