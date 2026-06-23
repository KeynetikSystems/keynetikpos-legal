// =============================================================================
// schedulemanager.cpp — Implementation of ScheduleManager (see
// schedulemanager.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - A 60-second QTimer drives onTick(); shouldFire() compares the current
//    time/day to each active schedule and uses lastSent to avoid duplicate
//    sends within the same window (and across restarts).
//  - Schedules persist in the message_schedules table; the in-memory list is
//    the working copy and persistSchedule() does INSERT-or-UPDATE.
//  - Providers are registered by name with duplicate protection; dispatch
//    resolves the provider by name at fire time so credentials can change
//    without re-creating schedules.
// =============================================================================
#include "schedulemanager.h"
#include "database.h"      // for sales data used in report bodies
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

ScheduleManager::ScheduleManager(Database &db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_timer(new QTimer(this))
{
    // Tick every minute to check schedules
    m_timer->setInterval(60 * 1000);
    connect(m_timer, &QTimer::timeout, this, &ScheduleManager::onTick);
}

// ── Provider registry ─────────────────────────────────────────────────────────

void ScheduleManager::registerProvider(MessageProvider *provider)
{
    if (!provider) return;
    // Avoid duplicates
    for (auto *p : m_providers)
        if (p->providerName() == provider->providerName()) return;
    m_providers.append(provider);
    // Observe the actual (async) delivery result, not just dispatch success.
    connect(provider, &MessageProvider::messageSent,
            this, &ScheduleManager::onProviderMessageSent,
            Qt::UniqueConnection);
    qDebug() << "ScheduleManager: registered provider" << provider->providerName();
}

QStringList ScheduleManager::getAvailableProviders() const
{
    QStringList names;
    for (auto *p : m_providers)
        names << p->providerName();
    return names;
}

MessageProvider *ScheduleManager::getProvider(const QString &name) const
{
    for (auto *p : m_providers)
        if (p->providerName() == name) return p;
    return nullptr;
}

// ── Schedule CRUD ─────────────────────────────────────────────────────────────

int ScheduleManager::addSchedule(const MessageSchedule &schedule)
{
    MessageSchedule s = schedule;
    s.scheduleId = m_nextId++;
    if (m_dbReady) persistSchedule(s);
    m_schedules.append(s);
    return s.scheduleId;
}

bool ScheduleManager::updateSchedule(const MessageSchedule &schedule)
{
    for (auto &s : m_schedules) {
        if (s.scheduleId == schedule.scheduleId) {
            s = schedule;
            if (m_dbReady) persistSchedule(s);
            return true;
        }
    }
    return false;
}

bool ScheduleManager::deleteSchedule(int id)
{
    for (int i = 0; i < m_schedules.size(); ++i) {
        if (m_schedules[i].scheduleId == id) {
            m_schedules.removeAt(i);
            if (m_dbReady) {
                QSqlQuery q;
                q.prepare("DELETE FROM message_schedules WHERE id = ?");
                q.addBindValue(id);
                q.exec();
            }
            return true;
        }
    }
    return false;
}

MessageSchedule ScheduleManager::getSchedule(int id) const
{
    for (const auto &s : m_schedules)
        if (s.scheduleId == id) return s;
    MessageSchedule empty;
    empty.scheduleId = -1;
    return empty;
}

QVector<MessageSchedule> ScheduleManager::getAllSchedules() const
{
    return m_schedules;
}

// ── Persistence ───────────────────────────────────────────────────────────────

bool ScheduleManager::initDatabase()
{
    QSqlQuery q;
    bool ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS message_schedules (
            id              INTEGER PRIMARY KEY,
            name            TEXT    NOT NULL,
            type            INTEGER NOT NULL DEFAULT 0,
            report_type     INTEGER NOT NULL DEFAULT 0,
            send_time       TEXT,
            day_of_week     INTEGER DEFAULT 1,
            day_of_month    INTEGER DEFAULT 1,
            sales_threshold REAL    DEFAULT 0,
            recipients      TEXT,
            provider_name   TEXT,
            is_active       INTEGER DEFAULT 1,
            last_sent       TEXT,
            notes           TEXT,
            created_date    TEXT,
            created_by      TEXT
        )
    )");
    if (!ok) {
        qWarning() << "ScheduleManager: failed to create table:" << q.lastError().text();
        return false;
    }
    m_dbReady = true;
    loadFromDb();
    return true;
}

bool ScheduleManager::persistSchedule(MessageSchedule &s)
{
    QSqlQuery q;
    if (s.scheduleId < 0) return false;

    // Check if row exists
    QSqlQuery check;
    check.prepare("SELECT COUNT(*) FROM message_schedules WHERE id = ?");
    check.addBindValue(s.scheduleId);
    check.exec();
    bool exists = check.next() && check.value(0).toInt() > 0;

    if (exists) {
        q.prepare(R"(
            UPDATE message_schedules SET
                name=?, type=?, report_type=?, send_time=?,
                day_of_week=?, day_of_month=?, sales_threshold=?,
                recipients=?, provider_name=?, is_active=?,
                last_sent=?, notes=?, created_by=?
            WHERE id=?
        )");
    } else {
        q.prepare(R"(
            INSERT INTO message_schedules
                (id, name, type, report_type, send_time, day_of_week,
                 day_of_month, sales_threshold, recipients, provider_name,
                 is_active, last_sent, notes, created_date, created_by)
            VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
        )");
    }

    auto bind = [&]() {
        q.addBindValue(s.scheduleName);
        q.addBindValue(static_cast<int>(s.type));
        q.addBindValue(static_cast<int>(s.reportType));
        q.addBindValue(s.sendTime.toString("HH:mm"));
        q.addBindValue(s.dayOfWeek);
        q.addBindValue(s.dayOfMonth);
        q.addBindValue(s.salesThreshold);
        q.addBindValue(s.recipients.join(","));
        q.addBindValue(s.providerName);
        q.addBindValue(s.isActive ? 1 : 0);
        q.addBindValue(s.lastSent.isValid() ? s.lastSent.toString(Qt::ISODate) : QString());
        q.addBindValue(s.notes);
        q.addBindValue(s.createdBy);
    };

    if (exists) {
        bind();
        q.addBindValue(s.scheduleId);
    } else {
        q.addBindValue(s.scheduleId);
        bind();
        q.addBindValue(s.createdDate.toString(Qt::ISODate));
    }

    if (!q.exec()) {
        qWarning() << "ScheduleManager: persist failed:" << q.lastError().text();
        return false;
    }
    return true;
}

void ScheduleManager::loadFromDb()
{
    m_schedules.clear();
    m_nextId = 1;

    QSqlQuery q("SELECT id, name, type, report_type, send_time, day_of_week, "
                "day_of_month, sales_threshold, recipients, provider_name, "
                "is_active, last_sent, notes, created_date, created_by "
                "FROM message_schedules ORDER BY id");

    while (q.next()) {
        MessageSchedule s;
        s.scheduleId    = q.value(0).toInt();
        s.scheduleName  = q.value(1).toString();
        s.type          = static_cast<MessageSchedule::ScheduleType>(q.value(2).toInt());
        s.reportType    = static_cast<MessageSchedule::ReportType>(q.value(3).toInt());
        s.sendTime      = QTime::fromString(q.value(4).toString(), "HH:mm");
        s.dayOfWeek     = q.value(5).toInt();
        s.dayOfMonth    = q.value(6).toInt();
        s.salesThreshold = q.value(7).toDouble();
        s.recipients    = q.value(8).toString().split(",", Qt::SkipEmptyParts);
        s.providerName  = q.value(9).toString();
        s.isActive      = q.value(10).toInt() == 1;
        s.lastSent      = QDateTime::fromString(q.value(11).toString(), Qt::ISODate);
        s.notes         = q.value(12).toString();
        s.createdDate   = QDateTime::fromString(q.value(13).toString(), Qt::ISODate);
        s.createdBy     = q.value(14).toString();
        m_schedules.append(s);
        if (s.scheduleId >= m_nextId) m_nextId = s.scheduleId + 1;
    }

    qDebug() << "ScheduleManager: loaded" << m_schedules.size() << "schedules";
}

// ── Engine ────────────────────────────────────────────────────────────────────

void ScheduleManager::start()
{
    m_timer->start();
    qDebug() << "ScheduleManager: started (1-min tick)";
}

void ScheduleManager::stop()
{
    m_timer->stop();
}

namespace {
constexpr int kMaxAttemptsPerWindow = 3;     // give up after this many tries...
constexpr int kRetrySpacingSecs     = 300;   // ...spaced at least 5 minutes apart
}

void ScheduleManager::onTick()
{
    const QDateTime now = QDateTime::currentDateTime();
    for (auto &s : m_schedules) {
        if (!s.isActive) continue;
        if (m_inFlight.contains(s.scheduleId)) continue;   // a send is still pending
        if (!shouldFire(s)) continue;

        // Record the attempt for retry throttling (reset the counter when we
        // enter a new due window).
        const QDateTime due = mostRecentDue(s, now);
        if (m_windowMark.value(s.scheduleId) != due) {
            m_windowMark[s.scheduleId]     = due;
            m_windowAttempts[s.scheduleId] = 0;
        }
        m_windowAttempts[s.scheduleId]++;
        m_lastAttempt[s.scheduleId] = now;

        dispatchSchedule(s);
    }
}

QDateTime ScheduleManager::mostRecentDue(const MessageSchedule &s, const QDateTime &now)
{
    switch (s.type) {
    case MessageSchedule::Daily: {
        QDateTime due(now.date(), s.sendTime);
        if (due > now) due = due.addDays(-1);
        return due;
    }
    case MessageSchedule::Weekly: {
        // Walk back to the most recent occurrence of dayOfWeek at/before now.
        const int delta = (now.date().dayOfWeek() - s.dayOfWeek + 7) % 7;
        QDateTime due(now.date().addDays(-delta), s.sendTime);
        if (due > now) due = due.addDays(-7);
        return due;
    }
    case MessageSchedule::Monthly: {
        const QDate today = now.date();
        QDate d(today.year(), today.month(),
                qMin(s.dayOfMonth, today.daysInMonth()));
        QDateTime due(d, s.sendTime);
        if (due > now) {
            const QDate pm = today.addMonths(-1);
            due = QDateTime(QDate(pm.year(), pm.month(),
                                  qMin(s.dayOfMonth, pm.daysInMonth())),
                            s.sendTime);
        }
        return due;
    }
    default:
        return QDateTime();   // event-based types are not time-driven
    }
}

bool ScheduleManager::shouldFire(const MessageSchedule &s) const
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime due = mostRecentDue(s, now);
    if (!due.isValid()) return false;   // event-based: fired via notify*(), not here

    // Already delivered this window, or the window predates the schedule's
    // creation? Then nothing to do. lastSent is set only on confirmed delivery,
    // so a failed send leaves the window open for a (bounded) retry.
    const QDateTime baseline = s.lastSent.isValid() ? s.lastSent : s.createdDate;
    if (baseline.isValid() && baseline >= due) return false;

    // Bounded retry within the same window: cap the count and space attempts.
    if (m_windowMark.value(s.scheduleId) == due) {
        if (m_windowAttempts.value(s.scheduleId) >= kMaxAttemptsPerWindow)
            return false;
        const QDateTime last = m_lastAttempt.value(s.scheduleId);
        if (last.isValid() && last.secsTo(now) < kRetrySpacingSecs)
            return false;
    }
    return true;
}

void ScheduleManager::notifySalesThreshold(double todaysSalesTotal)
{
    for (auto &s : m_schedules) {
        if (!s.isActive || s.type != MessageSchedule::OnSalesThreshold) continue;
        if (s.salesThreshold <= 0.0 || todaysSalesTotal < s.salesThreshold) continue;
        if (m_inFlight.contains(s.scheduleId)) continue;
        // Once per calendar day (lastSent is set on confirmed delivery).
        if (s.lastSent.isValid() && s.lastSent.date() == QDate::currentDate()) continue;
        dispatchSchedule(s);
    }
}

bool ScheduleManager::dispatchSchedule(MessageSchedule &s)
{
    QString body = buildReportBody(s);
    return sendNow(s.scheduleId, body);
}

bool ScheduleManager::sendNow(int scheduleId, const QString &body)
{
    MessageSchedule s = getSchedule(scheduleId);
    if (s.scheduleId < 0) return false;

    MessageProvider *provider = getProvider(s.providerName);
    if (!provider) {
        emit messageError(scheduleId, "Provider not found: " + s.providerName);
        emit scheduleFired(scheduleId, false);
        return false;
    }
    if (s.recipients.isEmpty()) {
        emit messageError(scheduleId, "No recipients configured");
        emit scheduleFired(scheduleId, false);
        return false;
    }

    // Build the report body if the caller passed an empty string (e.g. manual "Send Now")
    const QString messageBody = body.isEmpty() ? buildReportBody(s) : body;

    // Mark in-flight up front so a second tick within the same minute can't
    // re-dispatch while we wait for the providers' async replies. lastSent is
    // set ONLY when a reply confirms success (see finalizeInFlight()).
    m_inFlight.insert(scheduleId, InFlight{});

    for (const QString &recipient : s.recipients) {
        m_pending.append({scheduleId, phoneKey(recipient), provider->providerName()});
        if (provider->sendMessage(recipient, messageBody)) {
            m_inFlight[scheduleId].awaiting++;   // expect an async result
        } else {
            // Rejected synchronously (e.g. not configured) — no reply will come.
            m_pending.removeLast();
            emit messageError(scheduleId, "Send rejected for " + recipient);
        }
    }

    // If nothing was actually dispatched, finalize immediately as a failure.
    if (m_inFlight[scheduleId].awaiting == 0) {
        finalizeInFlight(scheduleId);
        return false;
    }
    return true;   // outcome reported asynchronously via onProviderMessageSent()
}

void ScheduleManager::onProviderMessageSent(bool success,
                                            const QString &messageId,
                                            const QString &recipient)
{
    auto *prov = qobject_cast<MessageProvider *>(sender());
    const QString provName = prov ? prov->providerName() : QString();
    const QString key = phoneKey(recipient);

    // Correlate to the oldest pending send from this provider; prefer an exact
    // recipient match, else fall back to FIFO for this provider.
    int idx = -1;
    for (int i = 0; i < m_pending.size(); ++i)
        if (m_pending[i].providerName == provName && m_pending[i].recipientKey == key) { idx = i; break; }
    if (idx < 0)
        for (int i = 0; i < m_pending.size(); ++i)
            if (m_pending[i].providerName == provName) { idx = i; break; }
    if (idx < 0) return;   // nothing to correlate (e.g. a one-off test send)

    const int scheduleId = m_pending[idx].scheduleId;
    m_pending.removeAt(idx);

    if (!m_inFlight.contains(scheduleId)) return;
    InFlight &fl = m_inFlight[scheduleId];
    if (fl.awaiting > 0) fl.awaiting--;

    if (success) {
        fl.anyOk = true;
        emit messageDelivered(scheduleId, messageId);
    } else {
        emit messageError(scheduleId,
                          messageId.isEmpty() ? QStringLiteral("Delivery failed") : messageId);
    }

    if (fl.awaiting == 0) finalizeInFlight(scheduleId);
}

void ScheduleManager::finalizeInFlight(int scheduleId)
{
    const bool ok = m_inFlight.value(scheduleId).anyOk;
    m_inFlight.remove(scheduleId);

    for (auto &sched : m_schedules) {
        if (sched.scheduleId != scheduleId) continue;
        if (ok)
            sched.lastSent = QDateTime::currentDateTime();   // only on confirmed success
        else
            sched.failedAttempts++;
        if (m_dbReady) persistSchedule(sched);
        break;
    }
    emit scheduleFired(scheduleId, ok);
}

QString ScheduleManager::phoneKey(const QString &phone)
{
    QString digits;
    for (const QChar &c : phone)
        if (c.isDigit()) digits.append(c);
    return digits;
}

QString ScheduleManager::buildReportBody(const MessageSchedule &s) const
{
    QString title = s.scheduleName;
    QString dateStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");

    if (!m_db.isOpen()) {
        return QString("Error: Database unavailable");
    }

    switch (s.reportType) {
    case MessageSchedule::ZReport: {
        Money todaySales = m_db.getTotalSalesToday();
        int todayTx      = m_db.getTotalTransactionsToday();
        Money profit     = m_db.getActualGrossProfitToday();
        return QString("📊 *%1*\n"
                       "📅 %2\n\n"
                       "💰 Total Sales: KES %3\n"
                       "🧾 Transactions: %4\n"
                       "📈 Gross Profit: KES %5")
            .arg(title, dateStr)
            .arg(todaySales.toMajor(), 0, 'f', 2)
            .arg(todayTx)
            .arg(profit.toMajor(), 0, 'f', 2);
    }
    case MessageSchedule::SalesSummary: {
        Money todaySales = m_db.getTotalSalesToday();
        int todayTx      = m_db.getTotalTransactionsToday();
        return QString("💵 *%1*\n"
                       "📅 %2\n\n"
                       "Sales: KES %3  |  Transactions: %4")
            .arg(title, dateStr)
            .arg(todaySales.toMajor(), 0, 'f', 2)
            .arg(todayTx);
    }
    case MessageSchedule::InventoryAlert:
        return QString("📦 *%1*\n📅 %2\n\nPlease review low-stock items in the POS system.")
            .arg(title, dateStr);
    default:
        return QString("📋 *%1*\n📅 %2").arg(title, dateStr);
    }
}