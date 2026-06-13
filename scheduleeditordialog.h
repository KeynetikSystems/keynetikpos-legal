// =============================================================================
// scheduleeditordialog.h — ScheduleEditorDialog: add/edit one schedule
// -----------------------------------------------------------------------------
// WHAT: The form for a single MessageSchedule: name, type, report type, time,
//       day-of-week/month, provider, recipient list, sales threshold, notes,
//       active flag.
// HOW:  Created with an optional scheduleId (-1 = new); loadSchedule()
//       populates fields for editing. updateVisibility() shows only the
//       controls relevant to the chosen type (day-of-week for Weekly,
//       threshold for OnSalesThreshold, etc.); validate() gates the save
//       button; the result is read back via getSchedule().
// WHY:  Conditional field visibility prevents semantically invalid schedules
//       (e.g. a "day of month" on a daily schedule) at the input stage rather
//       than at fire time, where the user wouldn't see the error.
// =============================================================================
#ifndef SCHEDULEEDITORDIALOG_H
#define SCHEDULEEDITORDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QTimeEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include "messageschedule.h"
#include "schedulemanager.h"

class ScheduleEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScheduleEditorDialog(ScheduleManager *manager,
                                  int scheduleId = -1,
                                  QWidget *parent = nullptr);

    MessageSchedule getSchedule() const;

private slots:
    void onScheduleTypeChanged(int index);
    void onAddRecipient();
    void onRemoveRecipient();
    void validate();

private:
    ScheduleManager *m_manager;
    int m_scheduleId;

    QLineEdit *m_nameEdit;
    QComboBox *m_typeCombo;
    QComboBox *m_reportTypeCombo;
    QTimeEdit *m_timeEdit;
    QSpinBox *m_dayOfWeekSpin;
    QSpinBox *m_dayOfMonthSpin;
    QComboBox *m_providerCombo;
    QTextEdit *m_recipientsEdit;
    QDoubleSpinBox *m_thresholdSpin;
    QTextEdit *m_notesEdit;
    QCheckBox *m_activeCheck;

    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;

    void setupUi();
    void loadSchedule();
    void updateVisibility();
};

#endif // SCHEDULEEDITORDIALOG_H
