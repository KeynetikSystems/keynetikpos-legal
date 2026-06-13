// =============================================================================
// shiftmanager.cpp — Implementation of ShiftManager (see shiftmanager.h for
// the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - loadActiveShift() runs at construction and re-adopts any row still
//    flagged IsOpen = 1, so a crash or power cut never orphans a shift.
//  - recordSale()/recordVoid() update the in-memory ShiftRecord AND the
//    database row as transactions occur — running totals are crash-safe
//    rather than recomputed at close.
//  - openShift() refuses to open while another shift is open: one drawer,
//    one accountable cashier at a time.
// =============================================================================

#include "shiftmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// =============================================================================
// ShiftManager Implementation
// =============================================================================

ShiftManager::ShiftManager(QSqlDatabase &db, QObject *parent)
    : QObject(parent), m_db(db)
{
    createTableIfNotExist();
    loadActiveShift();
}

void ShiftManager::createTableIfNotExist()
{
    QSqlQuery q(m_db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS Shifts (
            ShiftID          INTEGER PRIMARY KEY AUTOINCREMENT,
            CashierName      TEXT NOT NULL,
            OpeningFloat     REAL NOT NULL DEFAULT 0,
            ClosingFloat     REAL NOT NULL DEFAULT 0,
            TotalSales       REAL NOT NULL DEFAULT 0,
            TotalDiscounts   REAL NOT NULL DEFAULT 0,
            TransactionCount INTEGER NOT NULL DEFAULT 0,
            OpenedAt         TEXT NOT NULL,
            ClosedAt         TEXT,
            IsOpen           INTEGER NOT NULL DEFAULT 1,
            Notes            TEXT
        )
    )");
}

void ShiftManager::loadActiveShift()
{
    QSqlQuery q(m_db);
    q.exec("SELECT ShiftID, CashierName, OpeningFloat, TotalSales, TotalDiscounts, "
           "TransactionCount, OpenedAt FROM Shifts WHERE IsOpen = 1 ORDER BY ShiftID DESC LIMIT 1");

    if (q.next()) {
        m_currentShift.shiftId          = q.value(0).toInt();
        m_currentShift.cashierName      = q.value(1).toString();
        m_currentShift.openingFloat     = q.value(2).toDouble();
        m_currentShift.totalSales       = q.value(3).toDouble();
        m_currentShift.totalDiscounts   = q.value(4).toDouble();
        m_currentShift.transactionCount = q.value(5).toInt();
        m_currentShift.openedAt         = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
        m_currentShift.isOpen           = true;
    }
}

bool ShiftManager::openShift(const QString &cashierName, double openingFloat)
{
    if (m_currentShift.isOpen) {
        qDebug() << "ShiftManager: A shift is already open for" << m_currentShift.cashierName;
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO Shifts (CashierName, OpeningFloat, OpenedAt, IsOpen)
        VALUES (:name, :float, :opened, 1)
    )");
    q.bindValue(":name",   cashierName);
    q.bindValue(":float",  openingFloat);
    q.bindValue(":opened", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec()) {
        qDebug() << "ShiftManager: failed to open shift:" << q.lastError().text();
        return false;
    }

    m_currentShift              = ShiftRecord();
    m_currentShift.shiftId      = q.lastInsertId().toInt();
    m_currentShift.cashierName  = cashierName;
    m_currentShift.openingFloat = openingFloat;
    m_currentShift.openedAt     = QDateTime::currentDateTime();
    m_currentShift.isOpen       = true;

    emit shiftOpened(cashierName);
    return true;
}

bool ShiftManager::closeShift(double closingFloat, const QString &notes)
{
    if (!m_currentShift.isOpen) return false;

    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE Shifts
        SET ClosingFloat = :cf, ClosedAt = :ca, IsOpen = 0, Notes = :notes
        WHERE ShiftID = :id
    )");
    q.bindValue(":cf",    closingFloat);
    q.bindValue(":ca",    QDateTime::currentDateTime().toString(Qt::ISODate));
    q.bindValue(":notes", notes);
    q.bindValue(":id",    m_currentShift.shiftId);

    if (!q.exec()) {
        qDebug() << "ShiftManager: failed to close shift:" << q.lastError().text();
        return false;
    }

    m_currentShift.closingFloat = closingFloat;
    m_currentShift.closedAt     = QDateTime::currentDateTime();
    m_currentShift.notes        = notes;
    m_currentShift.isOpen       = false;

    ShiftRecord closed = m_currentShift;
    m_currentShift = ShiftRecord();

    emit shiftClosed(closed);
    return true;
}

void ShiftManager::recordSale(double amount, double discountAmount)
{
    if (!m_currentShift.isOpen) return;

    m_currentShift.totalSales       += amount;
    m_currentShift.totalDiscounts   += discountAmount;
    m_currentShift.transactionCount += 1;

    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE Shifts
        SET TotalSales = :sales, TotalDiscounts = :disc, TransactionCount = :tc
        WHERE ShiftID = :id
    )");
    q.bindValue(":sales", m_currentShift.totalSales);
    q.bindValue(":disc",  m_currentShift.totalDiscounts);
    q.bindValue(":tc",    m_currentShift.transactionCount);
    q.bindValue(":id",    m_currentShift.shiftId);
    q.exec();
}

void ShiftManager::recordVoid(double amount)
{
    recordSale(-amount, 0.0);
    m_currentShift.transactionCount -= 1; // Undo the +1 from recordSale
    m_currentShift.transactionCount  = qMax(0, m_currentShift.transactionCount);
}

QVector<ShiftRecord> ShiftManager::getShiftHistory(int limitDays) const
{
    QVector<ShiftRecord> history;
    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT ShiftID, CashierName, OpeningFloat, ClosingFloat, TotalSales,
               TotalDiscounts, TransactionCount, OpenedAt, ClosedAt, Notes
        FROM Shifts
        WHERE OpenedAt >= :since
        ORDER BY ShiftID DESC
    )");
    QString since = QDateTime::currentDateTime().addDays(-limitDays).toString(Qt::ISODate);
    q.bindValue(":since", since);

    if (!q.exec()) return history;

    while (q.next()) {
        ShiftRecord r;
        r.shiftId          = q.value(0).toInt();
        r.cashierName      = q.value(1).toString();
        r.openingFloat     = q.value(2).toDouble();
        r.closingFloat     = q.value(3).toDouble();
        r.totalSales       = q.value(4).toDouble();
        r.totalDiscounts   = q.value(5).toDouble();
        r.transactionCount = q.value(6).toInt();
        r.openedAt         = QDateTime::fromString(q.value(7).toString(), Qt::ISODate);
        r.closedAt         = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
        r.notes            = q.value(9).toString();
        r.isOpen           = r.closedAt.isNull();
        history.append(r);
    }
    return history;
}

ShiftRecord ShiftManager::getShift(int shiftId) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT ShiftID, CashierName, OpeningFloat, ClosingFloat, TotalSales, "
              "TotalDiscounts, TransactionCount, OpenedAt, ClosedAt, Notes "
              "FROM Shifts WHERE ShiftID = :id");
    q.bindValue(":id", shiftId);

    ShiftRecord r;
    if (q.exec() && q.next()) {
        r.shiftId          = q.value(0).toInt();
        r.cashierName      = q.value(1).toString();
        r.openingFloat     = q.value(2).toDouble();
        r.closingFloat     = q.value(3).toDouble();
        r.totalSales       = q.value(4).toDouble();
        r.totalDiscounts   = q.value(5).toDouble();
        r.transactionCount = q.value(6).toInt();
        r.openedAt         = QDateTime::fromString(q.value(7).toString(), Qt::ISODate);
        r.closedAt         = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
        r.notes            = q.value(9).toString();
        r.isOpen           = r.closedAt.isNull();
    }
    return r;
}
