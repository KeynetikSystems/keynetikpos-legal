// =============================================================================
// providersetup.h — messaging-provider credential wiring
// -----------------------------------------------------------------------------
// WHAT: reloadMessagingProviders() registers (or re-configures) the WhatsApp/
//       Twilio and Africa's Talking SMS providers on a ScheduleManager from
//       the credentials stored in QSettings.
// HOW:  Free function — looks up each provider by name, updates credentials
//       if it exists, otherwise creates and registers it with the given
//       QObject as parent for ownership.
// WHY:  This wiring used to live in MainWindow, which had no business knowing
//       about Twilio SIDs. Both the main window (startup, settings-saved) and
//       any future caller can reuse it without dragging in widget code.
// =============================================================================
#ifndef PROVIDERSETUP_H
#define PROVIDERSETUP_H

class ScheduleManager;
class QObject;

void reloadMessagingProviders(ScheduleManager *scheduleManager,
                              QObject *providerParent);

#endif // PROVIDERSETUP_H
