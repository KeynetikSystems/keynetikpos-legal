// =============================================================================
// schedulemanager.h — ScheduleManager: the scheduled-messaging engine
// -----------------------------------------------------------------------------
// WHAT: Provider registry, schedule CRUD with SQLite persistence, and a timer
//       loop that fires due schedules and dispatches generated reports
//       (Z-Report, inventory alerts, sales summaries) through the chosen
//       MessageProvider.
// HOW:  A QTimer ticks every 60 seconds; onTick() runs shouldFire() on each
//       active schedule, comparing current time/day against the schedule and
//       using lastSent to prevent duplicate sends in the same window.
//       buildReportBody() assembles report text (e.g. via ZReportDialog's
//       static text generator); dispatchSchedule() resolves the provider by
//       name and calls sendMessage() per recipient, updating lastSent /
//       failedAttempts and emitting scheduleFired / messageDelivered /
//       messageError. Schedules load from the message_schedules table at start.
// WHY:  A one-minute poll is precise enough for "send the Z-Report at 21:00"
//       and far simpler than per-schedule timers. Persisting schedules makes
//       them survive restarts; persisting lastSent makes them survive restarts
//       WITHOUT re-sending. The owner gets daily numbers by SMS/WhatsApp
//       without standing at the till.
// =============================================================================
#ifndef SCHEDULEMANAGER_H
#define SCHEDULEMANAGER_H

#include <QObject>
#include <QVector>
#include <QTimer>
#include <QHash>
#include <QSqlDatabase>
#include "MessageSchedule.h"
#include "messageprovider.h"

class ScheduleManager : public QObject
{
    Q_OBJECT

public:
    explicit ScheduleManager(QObject *parent = nullptr);
    ~ScheduleManager() override = default;

    // Provider registry
    void registerProvider(MessageProvider *provider);
    QStringList getAvailableProviders() const;
    MessageProvider *getProvider(const QString &name) const;

    // Schedule CRUD
    int addSchedule(const MessageSchedule &schedule);     // returns new id
    bool updateSchedule(const MessageSchedule &schedule);
    bool deleteSchedule(int id);
    MessageSchedule getSchedule(int id) const;
    QVector<MessageSchedule> getAllSchedules() const;

    // Persistence (SQLite)
    bool initDatabase();

    // Engine
    void start();
    void stop();

    // Manual trigger (for testing)
    bool sendNow(int scheduleId, const QString &body);

signals:
    void scheduleFired(int scheduleId, bool success);
    void messageDelivered(int scheduleId, const QString &messageId);
    void messageError(int scheduleId, const QString &error);

private slots:
    void onTick();
    // Async delivery result from a provider (base-class messageSent signal),
    // correlated back to the schedule/recipient that triggered it.
    void onProviderMessageSent(bool success, const QString &messageId,
                               const QString &recipient);

private:
    bool shouldFire(const MessageSchedule &s) const;
    QString buildReportBody(const MessageSchedule &s) const;
    bool dispatchSchedule(MessageSchedule &s);
    bool persistSchedule(MessageSchedule &s);   // INSERT or UPDATE
    void loadFromDb();
    void finalizeInFlight(int scheduleId);      // emits scheduleFired, sets lastSent on success
    static QString phoneKey(const QString &phone);   // digits-only correlation key

    QTimer *m_timer;
    QVector<MessageSchedule> m_schedules;
    QVector<MessageProvider *> m_providers;
    int m_nextId = 1;
    bool m_dbReady = false;

    // ── Async send tracking ─────────────────────────────────────────────────
    // One dispatched message awaiting its provider result.
    struct Pending { int scheduleId; QString recipientKey; QString providerName; };
    QVector<Pending> m_pending;
    // Aggregate state per schedule while its sends are in flight (blocks
    // re-dispatch and decides overall success once all replies are in).
    struct InFlight { int awaiting = 0; bool anyOk = false; };
    QHash<int, InFlight> m_inFlight;
};

#endif // SCHEDULEMANAGER_H
