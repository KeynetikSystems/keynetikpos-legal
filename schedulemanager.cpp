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

ScheduleManager::ScheduleManager(QObject *parent)
    : QObject(parent)
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

void ScheduleManager::onTick()
{
    for (auto &s : m_schedules) {
        if (!s.isActive) continue;
        if (shouldFire(s)) dispatchSchedule(s);
    }
}

bool ScheduleManager::shouldFire(const MessageSchedule &s) const
{
    QDateTime now = QDateTime::currentDateTime();
    QTime nowTime = now.time();

    // Don't re-send within the same minute
    if (s.lastSent.isValid() && s.lastSent.secsTo(now) < 60) return false;

    switch (s.type) {
    case MessageSchedule::Daily:
        return nowTime.hour()   == s.sendTime.hour() &&
               nowTime.minute() == s.sendTime.minute();

    case MessageSchedule::Weekly:
        return now.date().dayOfWeek() == s.dayOfWeek &&
               nowTime.hour()         == s.sendTime.hour() &&
               nowTime.minute()       == s.sendTime.minute();

    case MessageSchedule::Monthly:
        return now.date().day()  == s.dayOfMonth &&
               nowTime.hour()    == s.sendTime.hour() &&
               nowTime.minute()  == s.sendTime.minute();

    case MessageSchedule::OnShiftClose:
        // Triggered externally — not time-based
        return false;

    case MessageSchedule::OnSalesThreshold:
        // Triggered externally — not time-based
        return false;

    case MessageSchedule::Custom:
        // Future: use eventCondition
        return false;
    }
    return false;
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
        emit messageError(scheduleId,
                          "Provider not found: " + s.providerName);
        return false;
    }

    // Build the report body if the caller passed an empty string (e.g. manual "Send Now")
    const QString messageBody = body.isEmpty() ? buildReportBody(s) : body;

    bool anySuccess = false;
    for (const QString &recipient : s.recipients) {
        if (provider->sendMessage(recipient, messageBody)) anySuccess = true;
    }

    // Update lastSent
    for (auto &sched : m_schedules) {
        if (sched.scheduleId == scheduleId) {
            sched.lastSent = QDateTime::currentDateTime();
            if (m_dbReady) persistSchedule(sched);
            break;
        }
    }

    emit scheduleFired(scheduleId, anySuccess);
    return anySuccess;
}

QString ScheduleManager::buildReportBody(const MessageSchedule &s) const
{
    QString title = s.scheduleName;
    QString dateStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");

    if (!Database::instance().isOpen()) {
        return QString("Error: Database unavailable");
    }

    switch (s.reportType) {
    case MessageSchedule::ZReport: {
        Money todaySales = Database::instance().getTotalSalesToday();
        int todayTx      = Database::instance().getTotalTransactionsToday();
        Money profit     = Database::instance().getActualGrossProfitToday();
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
        Money todaySales = Database::instance().getTotalSalesToday();
        int todayTx      = Database::instance().getTotalTransactionsToday();
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