// =============================================================================
// whatsappmanager.cpp — Implementation of WhatsAppManager (see
// whatsappmanager.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Sends via Twilio's Messages REST endpoint: form-encoded POST with HTTP
//    Basic auth (Account SID + auth token) and "whatsapp:"-prefixed numbers.
//  - formatPhoneNumber() normalizes to E.164: strips non-digits, ensures '+',
//    and applies the Kenyan 254 prefix to local 0xx numbers.
//  - All sending is asynchronous; onReplyFinished() parses Twilio's JSON and
//    reports through the MessageProvider signals.
// =============================================================================
#include "whatsappmanager.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDebug>
#include <QRegularExpression>

WhatsAppManager::WhatsAppManager(QObject *parent)
    : MessageProvider(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &WhatsAppManager::onReplyFinished);
}

WhatsAppManager::~WhatsAppManager()
{
}

// Implement MessageProvider interface
bool WhatsAppManager::sendMessage(const QString &recipient,
                                  const QString &message,
                                  const QVariantMap &options)
{
    Q_UNUSED(options);
    return sendTextMessage(recipient, message);
}

void WhatsAppManager::setTwilioAccountSid(const QString &sid)
{
    m_accountSid = sid;
}

void WhatsAppManager::setTwilioAuthToken(const QString &token)
{
    m_authToken = token;
}

void WhatsAppManager::setTwilioWhatsAppNumber(const QString &number)
{
    m_whatsappNumber = number;
}

bool WhatsAppManager::isConfigured() const
{
    return !m_accountSid.isEmpty() &&
           !m_authToken.isEmpty() &&
           !m_whatsappNumber.isEmpty();
}

bool WhatsAppManager::validateConfiguration() const
{
    if (m_accountSid.isEmpty()) {
        const_cast<WhatsAppManager*>(this)->emit errorOccurred("Twilio Account SID is not configured");
        return false;
    }

    if (m_authToken.isEmpty()) {
        const_cast<WhatsAppManager*>(this)->emit errorOccurred("Twilio Auth Token is not configured");
        return false;
    }

    if (m_whatsappNumber.isEmpty()) {
        const_cast<WhatsAppManager*>(this)->emit errorOccurred("Twilio WhatsApp number is not configured");
        return false;
    }

    return true;
}

QString WhatsAppManager::formatPhoneNumber(const QString &phone) const
{
    // Normalize to E.164. Default country is Kenya (+254), the target market.
    QString cleaned = phone;
    cleaned.remove(QRegularExpression("[^0-9+]"));

    QString e164;
    if (cleaned.startsWith("+")) {
        e164 = cleaned;                                   // already international
    } else if (cleaned.startsWith("254")) {
        e164 = "+" + cleaned;                             // 2547... -> +2547...
    } else if (cleaned.startsWith("0")) {
        e164 = "+254" + cleaned.mid(1);                   // 07... -> +2547...
    } else {
        e164 = "+" + cleaned;                             // bare digits, assume intl
    }

    // Add whatsapp: prefix for Twilio
    return "whatsapp:" + e164;
}

bool WhatsAppManager::sendTextMessage(const QString &recipientPhone, const QString &message)
{
    if (!validateConfiguration()) {
        return false;
    }

    if (recipientPhone.isEmpty()) {
        emit errorOccurred("Recipient phone number is empty");
        return false;
    }

    if (message.isEmpty()) {
        emit errorOccurred("Message is empty");
        return false;
    }

    emit statusChanged("Preparing to send WhatsApp message...");

    // Build Twilio API URL
    QUrl url(QString("https://api.twilio.com/2010-04-01/Accounts/%1/Messages.json")
                 .arg(m_accountSid));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    // Add Basic Authentication
    QString credentials = QString("%1:%2").arg(m_accountSid, m_authToken);
    QString authHeader = "Basic " + credentials.toUtf8().toBase64();
    request.setRawHeader("Authorization", authHeader.toUtf8());

    // Format phone numbers
    QString fromNumber = formatPhoneNumber(m_whatsappNumber);
    QString toNumber = formatPhoneNumber(recipientPhone);

    // Build POST data
    QUrlQuery postData;
    postData.addQueryItem("From", fromNumber);
    postData.addQueryItem("To", toNumber);
    postData.addQueryItem("Body", message);

    emit statusChanged("Sending message via Twilio...");

    qDebug() << "Sending WhatsApp message:";
    qDebug() << "  From:" << fromNumber;
    qDebug() << "  To:" << toNumber;
    qDebug() << "  Message length:" << message.length();

    // Send POST request; tag the reply with the original recipient so
    // onReplyFinished can report it back (lets ScheduleManager correlate the
    // async result to the schedule/recipient that triggered it).
    QNetworkReply *reply =
        m_networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("recipient", recipientPhone);

    return true;
}

bool WhatsAppManager::sendZReport(const QString &recipientPhone,
                                  const QString &reportTitle,
                                  const QString &reportContent)
{
    // Format the Z-Report message
    QString message = QString("📊 *%1*\n\n%2")
                          .arg(reportTitle, reportContent);

    return sendTextMessage(recipientPhone, message);
}

void WhatsAppManager::onReplyFinished(QNetworkReply *reply)
{
    QString status;
    bool success = false;
    QString messageId;
    const QString recipient = reply->property("recipient").toString();

    if (reply->error() == QNetworkReply::NoError) {
        // Parse response
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();

        if (obj.contains("sid")) {
            messageId = obj["sid"].toString();
            QString messageStatus = obj["status"].toString();

            success = true;
            status = QString("Message sent successfully!\nMessage ID: %1\nStatus: %2")
                         .arg(messageId, messageStatus);

            emit statusChanged(status);
            emit messageSent(true, messageId, recipient);

            qDebug() << "✅ WhatsApp message sent successfully";
            qDebug() << "   Message ID:" << messageId;
            qDebug() << "   Status:" << messageStatus;
        } else if (obj.contains("message")) {
            // Error response from Twilio
            QString errorMsg = obj["message"].toString();
            int errorCode = obj["code"].toInt();

            status = QString("Twilio API Error %1: %2").arg(errorCode).arg(errorMsg);
            emit statusChanged(status);
            emit errorOccurred(status);
            emit messageSent(false, QString(), recipient);

            qWarning() << "❌ Twilio error:" << errorCode << errorMsg;

            // Provide helpful hints for common errors
            if (errorCode == 21608) {
                qWarning() << "   Hint: Recipient needs to join Twilio sandbox first";
                qWarning() << "   Send 'join <sandbox-name>' to the WhatsApp sandbox number";
            } else if (errorCode == 20003) {
                qWarning() << "   Hint: Check your Account SID and Auth Token";
            }
        } else {
            // Unrecognized 2xx response — report failure so callers don't hang.
            status = "Unexpected response from Twilio.";
            emit statusChanged(status);
            emit errorOccurred(status);
            emit messageSent(false, QString(), recipient);
        }
    } else {
        // Network error
        status = QString("Network Error: %1\n%2")
                     .arg(reply->errorString())
                     .arg(QString::fromUtf8(reply->readAll()));

        emit statusChanged(status);
        emit errorOccurred(status);
        emit messageSent(false, QString(), recipient);

        qWarning() << "❌ Network error sending WhatsApp:" << reply->errorString();
    }

    reply->deleteLater();
}
