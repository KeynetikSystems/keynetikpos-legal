// =============================================================================
// scheduledialog.h — ScheduleDialog: management list of message schedules
// -----------------------------------------------------------------------------
// WHAT: Table of all schedules (name, type, report, recipients, provider,
//       status, last sent) with add / edit / delete / toggle-active / Send Now
//       buttons.
// HOW:  Reads getAllSchedules() into the table; buttons call straight into
//       ScheduleManager; Send Now invokes sendNow() so a schedule (and the
//       provider credentials) can be tested without waiting for the trigger
//       time.
// WHY:  Send Now exists because the worst time to discover a typo'd API key
//       is 21:00 when the nightly report silently fails.
// =============================================================================
#ifndef SCHEDULEDIALOG_H
#define SCHEDULEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include "schedulemanager.h"

class ScheduleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScheduleDialog(ScheduleManager *manager, QWidget *parent = nullptr);

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onToggleActive();
    void onSendNow();
    void onSelectionChanged();
    void refresh();

private:
    void setupUi();

    ScheduleManager *m_manager;
    QTableWidget    *m_table;
    QPushButton     *m_addBtn;
    QPushButton     *m_editBtn;
    QPushButton     *m_deleteBtn;
    QPushButton     *m_toggleBtn;
    QPushButton     *m_sendNowBtn;
};

#endif // SCHEDULEDIALOG_H
