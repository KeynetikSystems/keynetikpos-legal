// =============================================================================
// providersetup.cpp — Implementation of reloadMessagingProviders (see
// providersetup.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Idempotent: safe to call on startup and again whenever settings are
//    saved; existing providers are re-configured in place so schedules keep
//    their provider references.
// =============================================================================
#include "providersetup.h"

#include "schedulemanager.h"
#include "whatsappmanager.h"
#include "Africatalkingprovider.h"
#include "secretstore.h"

#include <QSettings>

void reloadMessagingProviders(ScheduleManager *scheduleManager,
                              QObject *providerParent)
{
    QSettings cfg("KeynetikPOS", "KeynetikPOS");

    // WhatsApp / Twilio
    MessageProvider *existingWA =
        scheduleManager->getProvider("WhatsApp (Twilio)");
    if (existingWA) {
        auto *wa = qobject_cast<WhatsAppManager *>(existingWA);
        if (wa) {
            wa->setTwilioAccountSid(cfg.value("twilio/sid").toString());
            wa->setTwilioAuthToken(SecretStore::decrypt(cfg.value("twilio/token").toString()));
            wa->setTwilioWhatsAppNumber(cfg.value("twilio/number").toString());
        }
    } else {
        auto *wa = new WhatsAppManager(providerParent);
        wa->setTwilioAccountSid(cfg.value("twilio/sid").toString());
        wa->setTwilioAuthToken(cfg.value("twilio/token").toString());
        wa->setTwilioWhatsAppNumber(cfg.value("twilio/number").toString());
        scheduleManager->registerProvider(wa);
    }

    // Africa's Talking SMS
    MessageProvider *existingAT =
        scheduleManager->getProvider("Africa's Talking SMS");
    if (existingAT) {
        auto *at = qobject_cast<AfricasTalkingProvider *>(existingAT);
        if (at) {
            at->setApiKey(SecretStore::decrypt(cfg.value("africastalking/key").toString()));
            at->setUsername(cfg.value("africastalking/username").toString());
            at->setSenderId(cfg.value("africastalking/senderid").toString());
        }
    } else {
        auto *at = new AfricasTalkingProvider(providerParent);
        at->setApiKey(cfg.value("africastalking/key").toString());
        at->setUsername(cfg.value("africastalking/username").toString());
        at->setSenderId(cfg.value("africastalking/senderid").toString());
        scheduleManager->registerProvider(at);
    }
}
