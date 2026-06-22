// =============================================================================
// etims.cpp — Implementation of StubEtimsClient (see etims.h).
// =============================================================================
#include "etims.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

StubEtimsClient::StubEtimsClient(QSqlDatabase db) : m_db(db) {}

bool StubEtimsClient::initSchema()
{
    QSqlQuery q(m_db);
    return q.exec(R"(
        CREATE TABLE IF NOT EXISTS etims_log (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_id   INTEGER NOT NULL,
            status    TEXT NOT NULL,
            message   TEXT,
            logged_at TEXT NOT NULL
        )
    )");
}

EtimsResult StubEtimsClient::transmitSale(int saleId)
{
    // Idempotent: don't log the same sale twice.
    QSqlQuery chk(m_db);
    chk.prepare("SELECT COUNT(*) FROM etims_log WHERE sale_id = ?");
    chk.addBindValue(saleId);
    if (chk.exec() && chk.next() && chk.value(0).toInt() == 0) {
        QSqlQuery ins(m_db);
        ins.prepare("INSERT INTO etims_log (sale_id, status, message, logged_at) "
                    "VALUES (?, 'stubbed', ?, ?)");
        ins.addBindValue(saleId);
        ins.addBindValue("eTIMS device not onboarded; invoice queued locally.");
        ins.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        ins.exec();
    }
    return { false, QString(),
             "eTIMS not onboarded — invoice recorded locally only. "
             "Onboard a KRA OSCU/VSCU device to enable transmission." };
}
