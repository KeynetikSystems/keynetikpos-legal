// =============================================================================
// Africatalkingprovider.h — AfricasTalkingProvider: SMS via Africa's Talking
// -----------------------------------------------------------------------------
// WHAT: MessageProvider implementation for plain SMS through Africa's Talking,
//       a Kenyan SMS gateway (~KES 0.80/message; pricing/setup documented in
//       the class doc-comment below).
// HOW:  Header-only implementation. POSTs username/to/message (plus optional
//       sender ID) as form data with an apiKey header to the Africa's Talking
//       messaging endpoint via QNetworkAccessManager, then parses the JSON
//       response's SMSMessageData.Recipients[0] for status code 101/"Success",
//       emitting messageSent or errorOccurred accordingly.
// WHY:  SMS reaches owners on any phone — no smartphone or WhatsApp account
//       needed — and a local provider is cheaper and better supported than
//       Twilio for Kenyan numbers. Together with WhatsAppManager it proves the
//       MessageProvider abstraction works.
// =============================================================================
#ifndef AFRICATALKINGPROVIDER_H
#define AFRICATALKINGPROVIDER_H

#include "messageprovider.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Africa's Talking SMS Provider
 *
 * Perfect for Kenyan businesses!
 *
 * Pricing (Kenya):
 * - ~KES 0.80 per SMS (~$0.006 USD)
 * - No monthly fees
 * - Local support
 * - Integrates with M-Pesa
 *
 * Setup:
 * 1. Sign up at https://africastalking.com
 * 2. Get API Key from dashboard
 * 3. Get username (usually your app name)
 * 4. Optional: Register sender ID
 *
 * Docs: https://developers.africastalking.com/docs/sms/overview
 */
class AfricasTalkingProvider : public MessageProvider
{
    Q_OBJECT

public:
    explicit AfricasTalkingProvider(QObject *parent = nullptr)
        : MessageProvider(parent)
        , m_network(new QNetworkAccessManager(this))
    {
        connect(m_network, &QNetworkAccessManager::finished,
                this, &AfricasTalkingProvider::onReplyFinished);
    }

    // Configuration
    void setApiKey(const QString &key) { m_apiKey = key; }
    void setUsername(const QString &username) { m_username = username; }
    void setSenderId(const QString &sender) { m_senderId = sender; }

    bool sendMessage(const QString &recipient,
                     const QString &message,
                     const QVariantMap &options = QVariantMap()) override
    {
        Q_UNUSED(options);

        if (!isConfigured()) {
            emit errorOccurred("Africa's Talking not configured");
            return false;
        }

        if (recipient.isEmpty() || message.isEmpty()) {
            emit errorOccurred("Recipient or message is empty");
            return false;
        }

        // Build API request
        QUrl url("https://api.africastalking.com/version1/messaging");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/x-www-form-urlencoded");
        request.setRawHeader("apiKey", m_apiKey.toUtf8());
        request.setRawHeader("Accept", "application/json");

        // Format recipient (should have +254 format)
        QString formattedRecipient = recipient;
        if (!formattedRecipient.startsWith("+")) {
            formattedRecipient = "+" + formattedRecipient;
        }

        // Build POST data
        QUrlQuery postData;
        postData.addQueryItem("username", m_username);
        postData.addQueryItem("to", formattedRecipient);
        postData.addQueryItem("message", message);

        if (!m_senderId.isEmpty()) {
            postData.addQueryItem("from", m_senderId);
        }

        emit statusChanged("Sending SMS via Africa's Talking...");

        qDebug() << "Sending SMS via Africa's Talking:";
        qDebug() << "  To:" << formattedRecipient;
        qDebug() << "  Message length:" << message.length();

        QNetworkReply *reply =
            m_network->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
        reply->setProperty("recipient", recipient);   // original, for correlation

        return true;
    }

    bool isConfigured() const override
    {
        return !m_apiKey.isEmpty() && !m_username.isEmpty();
    }

    QString providerName() const override
    {
        return "Africa's Talking SMS";
    }

    ProviderType providerType() const override
    {
        return CustomSMS;
    }

private slots:
    void onReplyFinished(QNetworkReply *reply)
    {
        const QString origRecipient = reply->property("recipient").toString();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            QJsonObject obj = doc.object();

            // Africa's Talking response format:
            // {
            //   "SMSMessageData": {
            //     "Message": "Sent to 1/1 Total Cost: KES 0.8000",
            //     "Recipients": [{
            //       "statusCode": 101,
            //       "number": "+254711XXXYYY",
            //       "status": "Success",
            //       "cost": "KES 0.8000",
            //       "messageId": "ATXid_xxx"
            //     }]
            //   }
            // }

            if (obj.contains("SMSMessageData")) {
                QJsonObject smsData = obj["SMSMessageData"].toObject();
                QJsonArray recipients = smsData["Recipients"].toArray();

                if (!recipients.isEmpty()) {
                    QJsonObject recipient = recipients[0].toObject();
                    QString status = recipient["status"].toString();
                    QString messageId = recipient["messageId"].toString();
                    int statusCode = recipient["statusCode"].toInt();

                    if (status == "Success" || statusCode == 101) {
                        emit statusChanged(QString("SMS sent! Cost: %1")
                                               .arg(recipient["cost"].toString()));
                        emit messageSent(true, messageId, origRecipient);

                        qDebug() << "✅ SMS sent successfully";
                        qDebug() << "   Message ID:" << messageId;
                        qDebug() << "   Cost:" << recipient["cost"].toString();
                    } else {
                        QString error = QString("SMS failed: %1 (Code: %2)")
                        .arg(status).arg(statusCode);
                        emit errorOccurred(error);
                        emit messageSent(false, "", origRecipient);

                        qWarning() << "❌ SMS failed:" << status;
                    }
                } else {
                    emit errorOccurred("No recipients in response");
                    emit messageSent(false, "", origRecipient);
                }
            } else {
                emit errorOccurred("Invalid response format");
                emit messageSent(false, "", origRecipient);
            }
        } else {
            QString error = QString("Network error: %1").arg(reply->errorString());
            emit errorOccurred(error);
            emit messageSent(false, "", origRecipient);

            qWarning() << "❌ Network error:" << reply->errorString();
        }

        reply->deleteLater();
    }

private:
    QNetworkAccessManager *m_network;
    QString m_apiKey;
    QString m_username;
    QString m_senderId;
};

#endif // AFRICATALKINGPROVIDER_H
