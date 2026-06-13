#pragma once

// =============================================================================
// shiftmanager.h — ShiftManager: shift lifecycle and cash accounting
// -----------------------------------------------------------------------------
// WHAT: Opens a shift with a counted starting float, accumulates sales/
//       discounts/voids as they happen, closes with a counted ending float and
//       notes, and queries shift history.
// HOW:  Self-creates the Shifts table. loadActiveShift() runs at construction
//       and re-adopts any shift still flagged IsOpen = 1, so a crash or power
//       cut doesn't orphan the shift. recordSale()/recordVoid() update both
//       the in-memory ShiftRecord and the database row as transactions occur.
//       Signals shiftOpened/shiftClosed notify the UI and report triggers.
// WHY:  Shifts are the unit of cash accountability: the float counted at open
//       plus recorded cash sales is what the drawer SHOULD contain at close,
//       and the Z-Report computes the variance. Persisting running totals
//       (rather than recomputing at close) keeps open-shift state crash-safe.
// =============================================================================

#include "shift_type.h"
#include <QObject>
#include <QSqlDatabase>
#include <QVector>

// =============================================================================
// ShiftManager — Handles shift lifecycle, tracking, and persistence
// =============================================================================

class ShiftManager : public QObject
{
    Q_OBJECT

public:
    explicit ShiftManager(QSqlDatabase &db, QObject *parent = nullptr);

    // Shift lifecycle
    bool openShift(const QString &cashierName, double openingFloat);
    bool closeShift(double closingFloat, const QString &notes = "");

    // State queries
    bool         isShiftOpen()    const { return m_currentShift.isOpen; }
    ShiftRecord  currentShift()   const { return m_currentShift; }
    QString      currentCashier() const { return m_currentShift.cashierName; }
    int          currentShiftId() const { return m_currentShift.shiftId; }

    // Transaction tracking — call these as sales happen
    void recordSale(double amount, double discountAmount = 0.0);
    void recordVoid(double amount);

    // History and retrieval
    QVector<ShiftRecord> getShiftHistory(int limitDays = 30) const;
    ShiftRecord          getShift(int shiftId) const;

signals:
    void shiftOpened(QString cashierName);
    void shiftClosed(ShiftRecord record);

private:
    void createTableIfNotExist();
    void loadActiveShift();

    QSqlDatabase &m_db;
    ShiftRecord   m_currentShift;
};
