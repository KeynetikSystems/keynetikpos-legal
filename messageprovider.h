// =============================================================================
// messageprovider.h — MessageProvider: abstract message-delivery interface
// -----------------------------------------------------------------------------
// WHAT: Abstract base for anything that can deliver a message: sendMessage(),
//       isConfigured(), providerName(), providerType(), an optional native
//       scheduleMessage(), and common signals (messageSent, errorOccurred,
//       statusChanged).
// HOW:  Pure-virtual QObject interface; concrete providers (WhatsAppManager,
//       AfricasTalkingProvider) implement transport details and report results
//       asynchronously via the signals.
// WHY:  Strategy pattern applied to messaging: ScheduleManager and the
//       settings UI talk only to this interface, so adding email or another
//       SMS gateway means writing one subclass — zero changes to scheduling
//       code.
// =============================================================================
#ifndef MESSAGEPROVIDER_H
#define MESSAGEPROVIDER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

/**
 * @brief Abstract base class for message providers
 *
 * This allows switching between different providers (Twilio, Africa's Talking,
 * WhatsApp Business API, SMS providers, etc.) without changing scheduling code.
 */
class MessageProvider : public QObject
{
    Q_OBJECT

public:
    enum ProviderType {
        Twilio,
        AfricasTalking,
        WhatsAppBusinessAPI,
        CustomSMS,
        Email
    };

    explicit MessageProvider(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~MessageProvider() = default;

    // Pure virtual functions that must be implemented by each provider
    virtual bool sendMessage(const QString &recipient,
                             const QString &message,
                             const QVariantMap &options = QVariantMap()) = 0;

    virtual bool isConfigured() const = 0;
    virtual QString providerName() const = 0;
    virtual ProviderType providerType() const = 0;

    // Optional: Override if provider supports native scheduling
    virtual bool scheduleMessage(const QString &recipient,
                                 const QString &message,
                                 const QDateTime &sendTime) {
        Q_UNUSED(recipient);
        Q_UNUSED(message);
        Q_UNUSED(sendTime);
        return false; // Default: no native scheduling
    }

signals:
    void messageSent(bool success, const QString &messageId, const QString &recipient);
    void errorOccurred(const QString &error);
    void statusChanged(const QString &status);
};

#endif // MESSAGEPROVIDER_H
