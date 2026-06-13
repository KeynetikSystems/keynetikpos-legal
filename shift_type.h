#pragma once

// =============================================================================
// shift_type.h — ShiftRecord: core data struct for shift management
// -----------------------------------------------------------------------------
// WHAT: Plain struct for one cashier shift: id, cashier, opening/closing
//       float, running totals (sales, discounts, transaction count),
//       open/close timestamps, open flag, notes.
// HOW:  Header-only POD shared by ShiftManager, the shift dialogs, and
//       ZReportDialog.
// WHY:  Keeping it dependency-free avoids include cycles between the three
//       consumers, and a copyable value type moves freely between the UI and
//       persistence layers.
// =============================================================================

#include <QString>
#include <QDateTime>

// =============================================================================
// ShiftRecord — Plain data struct representing a cashier shift
// =============================================================================

struct ShiftRecord {
    int       shiftId          = -1;
    QString   cashierName;
    double    openingFloat     = 0.0;
    double    closingFloat     = 0.0;
    double    totalSales       = 0.0;
    double    totalDiscounts   = 0.0;
    int       transactionCount = 0;
    QDateTime openedAt;
    QDateTime closedAt;
    bool      isOpen           = false;
    QString   notes;
};
