// =============================================================================
// mpesaclient.h — MpesaClient: drives an M-Pesa STK Push via the license Worker
// -----------------------------------------------------------------------------
// WHAT: Triggers a Daraja STK Push for an in-store sale and reports the result.
//       The till never holds Daraja credentials — it calls the KeynetikPOS
//       Cloudflare Worker, which initiates the push, receives Safaricom's
//       callback, and exposes a status endpoint this client polls.
// HOW:  requestPayment() POSTs to <server>/pos/mpesa/stkpush authenticated with
//       the till's license key + device id; on acceptance it polls
//       /pos/mpesa/status every few seconds until success/failure/timeout.
//       Everything is async (QNetworkAccessManager + a QTimer); results arrive
//       via the signals below.
// WHY:  A desktop till is behind NAT and can't receive Safaricom's callback, so
//       the public Worker is the rendezvous point; routing through it also keeps
//       the Daraja secrets server-side instead of on every shop PC.
// =============================================================================
#ifndef MPESACLIENT_H
#define MPESACLIENT_H

#include <QObject>
#include <QString>

#include "money.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class MpesaClient : public QObject
{
    Q_OBJECT
public:
    explicit MpesaClient(QObject *parent = nullptr);

    // True only when this till is activated (has a license key) — STK push needs
    // it to authenticate to the Worker.
    static bool isAvailable();

    // Begin an STK push to `phone` for `amount` (rounded to whole shillings —
    // M-Pesa has no cents). accountRef appears on the customer's statement.
    void requestPayment(const QString &phone, Money amount,
                        const QString &accountRef = QStringLiteral("POS"));

    // Stop polling / abort an in-flight request (e.g. cashier cancelled).
    void cancel();

signals:
    void promptSent(const QString &customerMessage);   // STK accepted by Daraja
    void statusChanged(const QString &message);        // human-readable progress
    void paymentConfirmed(const QString &mpesaReceipt);
    void paymentFailed(const QString &reason);

private slots:
    void onStkReply(QNetworkReply *reply);
    void poll();
    void onStatusReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_net;
    QTimer  *m_pollTimer;
    QString  m_checkoutId;
    int      m_polls      = 0;
    bool     m_active     = false;   // a request is in flight
};

#endif // MPESACLIENT_H
