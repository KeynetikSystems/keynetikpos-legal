// =============================================================================
// whatsappmanager.h — WhatsAppManager: WhatsApp delivery via the Twilio API
// -----------------------------------------------------------------------------
// WHAT: MessageProvider implementation for WhatsApp through Twilio, including
//       a formatted Z-Report convenience (sendZReport).
// HOW:  POSTs form-encoded requests to Twilio's Messages endpoint with HTTP
//       Basic auth (Account SID + auth token) and "whatsapp:"-prefixed
//       numbers, via QNetworkAccessManager. formatPhoneNumber() normalizes
//       input to E.164 (strips non-digits, ensures '+', applies the Kenyan 254
//       prefix to local numbers). Replies are parsed in onReplyFinished and
//       surfaced through the standard provider signals. Credentials come from
//       SettingsDialog's Messaging tab.
// WHY:  WhatsApp is the default business communication channel in the target
//       market, and Twilio's sandbox makes it testable for free before
//       production setup (pricing/setup notes in the class doc-comment below).
// =============================================================================
#ifndef WHATSAPPMANAGER_H
#define WHATSAPPMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include "messageprovider.h"

/**
 * @brief WhatsApp messaging manager using Twilio API
 *
 * Twilio provides a free WhatsApp sandbox for testing:
 * - Send messages to pre-approved numbers
 * - Free tier includes some free credits
 * - Production requires a Twilio phone number ($1-2/month + usage)
 *
 * Pricing (as of 2024):
 * - Sandbox: Free for testing
 * - Production: ~$0.005 per message (outbound)
 * - Phone number: $1-2/month
 *
 * Setup: https://www.twilio.com/console/sms/whatsapp/sandbox
 */
class WhatsAppManager : public MessageProvider
{
    Q_OBJECT

public:
    explicit WhatsAppManager(QObject *parent = nullptr);
    ~WhatsAppManager();

    // Implement MessageProvider interface
    bool sendMessage(const QString &recipient,
                     const QString &message,
                     const QVariantMap &options = QVariantMap()) override;
    bool isConfigured() const override;
    QString providerName() const override { return "WhatsApp (Twilio)"; }
    ProviderType providerType() const override { return ProviderType::Twilio; }

    // Configuration
    void setTwilioAccountSid(const QString &sid);
    void setTwilioAuthToken(const QString &token);
    void setTwilioWhatsAppNumber(const QString &number);

    // Check if configured

    // Send text message
    bool sendTextMessage(const QString &recipientPhone, const QString &message);

    // Send formatted Z-Report
    bool sendZReport(const QString &recipientPhone,
                     const QString &reportTitle,
                     const QString &reportContent);

signals:
    void messageSent(bool success, const QString &messageId);
    void errorOccurred(const QString &error);
    void statusChanged(const QString &status);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_accountSid;
    QString m_authToken;
    QString m_whatsappNumber;

    QString formatPhoneNumber(const QString &phone) const;
    bool validateConfiguration() const;
};

#endif // WHATSAPPMANAGER_H
