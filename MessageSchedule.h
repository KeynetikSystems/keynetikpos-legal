// =============================================================================
// MessageSchedule.h — MessageSchedule: one scheduled-message definition
// -----------------------------------------------------------------------------
// WHAT: Plain struct describing one scheduled message: schedule type (Daily /
//       Weekly / Monthly / OnShiftClose / OnSalesThreshold / Custom), report
//       type (Z-Report / X-Report / Inventory Alert / Sales Summary / Custom),
//       send time, day-of-week/month, recipients, provider name, active flag,
//       last-sent timestamp, failure counter, threshold, and metadata.
// HOW:  Pure value type with typeString()/reportTypeString() display helpers;
//       persisted by ScheduleManager into the message_schedules table.
// WHY:  A copyable POD moves freely between the editor dialog, the manager's
//       in-memory list, and the SQLite row without lifetime concerns. Storing
//       lastSent is what makes once-per-day semantics possible across
//       restarts.
// =============================================================================
#ifndef MESSAGESCHEDULE_H
#define MESSAGESCHEDULE_H

#include <QString>
#include <QDateTime>
#include <QStringList>

struct MessageSchedule
{
    int scheduleId = -1;
    QString scheduleName;

    enum ScheduleType {
        Daily,
        Weekly,
        Monthly,
        OnShiftClose,
        OnSalesThreshold,
        Custom
    };
    ScheduleType type = Daily;

    enum ReportType {
        ZReport,
        XReport,
        InventoryAlert,
        SalesSummary,
        CustomReport
    };
    ReportType reportType = ZReport;

    // Time settings
    QTime sendTime;                    // Time of day to send
    int dayOfWeek = 1;                 // 1=Monday, 7=Sunday (for weekly)
    int dayOfMonth = 1;                // 1-31 (for monthly)

    // Recipients
    QStringList recipients;
    QString providerName;              // "Twilio", "AfricasTalking", etc.

    // Status
    bool isActive = true;
    QDateTime lastSent;
    QDateTime nextScheduled;
    int failedAttempts = 0;

    // Event-based settings
    double salesThreshold = 0.0;      // For OnSalesThreshold
    QString eventCondition;            // Additional conditions

    // Metadata
    QDateTime createdDate;
    QString createdBy;
    QString notes;

    // Helper methods
    QString typeString() const {
        switch(type) {
        case Daily: return "Daily";
        case Weekly: return "Weekly";
        case Monthly: return "Monthly";
        case OnShiftClose: return "On Shift Close";
        case OnSalesThreshold: return "On Sales Threshold";
        case Custom: return "Custom";
        }
        return "Unknown";
    }

    QString reportTypeString() const {
        switch(reportType) {
        case ZReport: return "Z-Report";
        case XReport: return "X-Report";
        case InventoryAlert: return "Inventory Alert";
        case SalesSummary: return "Sales Summary";
        case CustomReport: return "Custom Report";
        }
        return "Unknown";
    }
};

#endif // MESSAGESCHEDULE_H
