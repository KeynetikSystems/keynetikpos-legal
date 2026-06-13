// =============================================================================
// scheduleeditordialog.cpp — Implementation of ScheduleEditorDialog (see
// scheduleeditordialog.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - updateVisibility() shows only the controls relevant to the selected
//    schedule type, so invalid combinations can't be entered.
//  - validate() gates the Save button (name, provider, at least one
//    recipient); getSchedule() assembles the MessageSchedule the caller
//    persists via ScheduleManager.
// =============================================================================
#include "scheduleeditordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>

ScheduleEditorDialog::ScheduleEditorDialog(ScheduleManager *manager,
                                           int scheduleId,
                                           QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_scheduleId(scheduleId)
{
    bool isEdit = (scheduleId >= 0);
    setWindowTitle(isEdit ? "✏️ Edit Schedule" : "➕ Add Schedule");
    resize(600, 700);

    setupUi();

    if (isEdit) {
        loadSchedule();
    }

    updateVisibility();
}

void ScheduleEditorDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // Basic Info Group
    auto *basicGroup = new QGroupBox("📋 Basic Information", this);
    auto *basicLayout = new QFormLayout(basicGroup);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("e.g., Daily Z-Report to Manager");
    basicLayout->addRow("Schedule Name*:", m_nameEdit);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("📅 Daily", MessageSchedule::Daily);
    m_typeCombo->addItem("📆 Weekly", MessageSchedule::Weekly);
    m_typeCombo->addItem("📊 Monthly", MessageSchedule::Monthly);
    m_typeCombo->addItem("🔚 On Shift Close", MessageSchedule::OnShiftClose);
    m_typeCombo->addItem("💰 On Sales Threshold", MessageSchedule::OnSalesThreshold);
    m_typeCombo->addItem("⚙️ Custom", MessageSchedule::Custom);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScheduleEditorDialog::onScheduleTypeChanged);
    basicLayout->addRow("Schedule Type*:", m_typeCombo);

    m_reportTypeCombo = new QComboBox(this);
    m_reportTypeCombo->addItem("📊 Z-Report", MessageSchedule::ZReport);
    m_reportTypeCombo->addItem("📈 X-Report", MessageSchedule::XReport);
    m_reportTypeCombo->addItem("📦 Inventory Alert", MessageSchedule::InventoryAlert);
    m_reportTypeCombo->addItem("💵 Sales Summary", MessageSchedule::SalesSummary);
    m_reportTypeCombo->addItem("📄 Custom Report", MessageSchedule::CustomReport);
    basicLayout->addRow("Report Type*:", m_reportTypeCombo);

    mainLayout->addWidget(basicGroup);

    // Time Settings Group
    auto *timeGroup = new QGroupBox("⏰ Time Settings", this);
    auto *timeLayout = new QFormLayout(timeGroup);

    m_timeEdit = new QTimeEdit(QTime(23, 0), this);
    m_timeEdit->setDisplayFormat("HH:mm");
    timeLayout->addRow("Send Time:", m_timeEdit);

    m_dayOfWeekSpin = new QSpinBox(this);
    m_dayOfWeekSpin->setRange(1, 7);
    m_dayOfWeekSpin->setValue(1);
    m_dayOfWeekSpin->setSpecialValueText("Monday");
    auto *dowLabel = new QLabel("(1=Monday, 7=Sunday)", this);
    dowLabel->setProperty("kind", "secondary");
    dowLabel->setStyleSheet("font-size:10px;");
    timeLayout->addRow("Day of Week:", m_dayOfWeekSpin);
    timeLayout->addRow("", dowLabel);

    m_dayOfMonthSpin = new QSpinBox(this);
    m_dayOfMonthSpin->setRange(1, 31);
    m_dayOfMonthSpin->setValue(1);
    timeLayout->addRow("Day of Month:", m_dayOfMonthSpin);

    mainLayout->addWidget(timeGroup);

    // Event Settings Group
    auto *eventGroup = new QGroupBox("🎯 Event Settings", this);
    auto *eventLayout = new QFormLayout(eventGroup);

    m_thresholdSpin = new QDoubleSpinBox(this);
    m_thresholdSpin->setRange(0, 999999999);
    m_thresholdSpin->setDecimals(2);
    m_thresholdSpin->setValue(0);
    m_thresholdSpin->setPrefix("KES ");
    eventLayout->addRow("Sales Threshold:", m_thresholdSpin);

    mainLayout->addWidget(eventGroup);

    // Provider & Recipients Group
    auto *providerGroup = new QGroupBox("📱 Provider & Recipients", this);
    auto *providerLayout = new QFormLayout(providerGroup);

    m_providerCombo = new QComboBox(this);
    // Populate with available providers
    QStringList providers = m_manager->getAvailableProviders();
    if (providers.isEmpty()) {
        m_providerCombo->addItem("⚠️ No providers configured");
        m_providerCombo->setEnabled(false);
    } else {
        m_providerCombo->addItems(providers);
    }
    providerLayout->addRow("Provider*:", m_providerCombo);

    m_recipientsEdit = new QTextEdit(this);
    m_recipientsEdit->setPlaceholderText(
        "Enter phone numbers or emails, one per line:\n"
        "+254712345678\n"
        "+254723456789\n"
        "manager@example.com");
    m_recipientsEdit->setMaximumHeight(100);
    providerLayout->addRow("Recipients*:", m_recipientsEdit);

    auto *recipientHint = new QLabel(
        "💡 Tip: Include country code for phone numbers (+254 for Kenya)", this);
    recipientHint->setWordWrap(true);
    recipientHint->setProperty("kind", "secondary");
    recipientHint->setStyleSheet("font-size:10px;");
    providerLayout->addRow("", recipientHint);

    mainLayout->addWidget(providerGroup);

    // Notes Group
    auto *notesGroup = new QGroupBox("📝 Notes", this);
    auto *notesLayout = new QVBoxLayout(notesGroup);

    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setPlaceholderText("Optional notes about this schedule...");
    m_notesEdit->setMaximumHeight(80);
    notesLayout->addWidget(m_notesEdit);

    mainLayout->addWidget(notesGroup);

    // Status
    m_activeCheck = new QCheckBox("✅ Schedule is Active", this);
    m_activeCheck->setChecked(true);
    m_activeCheck->setProperty("bold", "true");
    mainLayout->addWidget(m_activeCheck);

    mainLayout->addStretch();

    // Buttons
    auto *buttonLayout = new QHBoxLayout;

    m_saveBtn = new QPushButton(m_scheduleId >= 0 ? "💾 Save Changes" : "➕ Create Schedule", this);
    m_saveBtn->setProperty("kind", "primary");
    connect(m_saveBtn, &QPushButton::clicked, this, &ScheduleEditorDialog::validate);
    buttonLayout->addWidget(m_saveBtn);

    m_cancelBtn = new QPushButton("Cancel", this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(buttonLayout);
}

void ScheduleEditorDialog::loadSchedule()
{
    MessageSchedule schedule = m_manager->getSchedule(m_scheduleId);

    if (schedule.scheduleId == -1) {
        QMessageBox::critical(this, "Error", "Schedule not found!");
        reject();
        return;
    }

    m_nameEdit->setText(schedule.scheduleName);

    // Set type
    int typeIndex = m_typeCombo->findData(schedule.type);
    if (typeIndex >= 0) {
        m_typeCombo->setCurrentIndex(typeIndex);
    }

    // Set report type
    int reportIndex = m_reportTypeCombo->findData(schedule.reportType);
    if (reportIndex >= 0) {
        m_reportTypeCombo->setCurrentIndex(reportIndex);
    }

    m_timeEdit->setTime(schedule.sendTime);
    m_dayOfWeekSpin->setValue(schedule.dayOfWeek);
    m_dayOfMonthSpin->setValue(schedule.dayOfMonth);
    m_thresholdSpin->setValue(schedule.salesThreshold);

    // Set provider
    int providerIndex = m_providerCombo->findText(schedule.providerName);
    if (providerIndex >= 0) {
        m_providerCombo->setCurrentIndex(providerIndex);
    }

    // Set recipients
    m_recipientsEdit->setPlainText(schedule.recipients.join("\n"));

    m_notesEdit->setPlainText(schedule.notes);
    m_activeCheck->setChecked(schedule.isActive);
}

void ScheduleEditorDialog::updateVisibility()
{
    MessageSchedule::ScheduleType type =
        static_cast<MessageSchedule::ScheduleType>(
            m_typeCombo->currentData().toInt());

    // Show/hide based on type
    bool showTime = (type == MessageSchedule::Daily ||
                     type == MessageSchedule::Weekly ||
                     type == MessageSchedule::Monthly);
    bool showDayOfWeek = (type == MessageSchedule::Weekly);
    bool showDayOfMonth = (type == MessageSchedule::Monthly);
    bool showThreshold = (type == MessageSchedule::OnSalesThreshold);

    m_timeEdit->setVisible(showTime);
    m_dayOfWeekSpin->setVisible(showDayOfWeek);
    m_dayOfMonthSpin->setVisible(showDayOfMonth);
    m_thresholdSpin->setVisible(showThreshold);

    // Update labels
    findChild<QGroupBox*>()->findChild<QFormLayout*>();
}

void ScheduleEditorDialog::validate()
{
    // Validate name
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error",
                             "Please enter a schedule name.");
        m_nameEdit->setFocus();
        return;
    }

    // Validate recipients
    QStringList recipients = m_recipientsEdit->toPlainText()
                                 .split('\n', Qt::SkipEmptyParts);

    if (recipients.isEmpty()) {
        QMessageBox::warning(this, "Validation Error",
                             "Please enter at least one recipient.");
        m_recipientsEdit->setFocus();
        return;
    }

    // Clean up recipients
    QStringList cleanRecipients;
    for (QString recipient : recipients) {
        recipient = recipient.trimmed();
        if (!recipient.isEmpty()) {
            cleanRecipients << recipient;
        }
    }

    if (cleanRecipients.isEmpty()) {
        QMessageBox::warning(this, "Validation Error",
                             "Please enter valid recipients.");
        m_recipientsEdit->setFocus();
        return;
    }

    // Validate provider
    if (m_providerCombo->currentText().startsWith("⚠️")) {
        QMessageBox::warning(this, "Validation Error",
                             "No messaging providers are configured.\n\n"
                             "Please configure a provider (WhatsApp, SMS, etc.) first.");
        return;
    }

    // All valid, accept dialog
    accept();
}

MessageSchedule ScheduleEditorDialog::getSchedule() const
{
    MessageSchedule schedule;

    if (m_scheduleId >= 0) {
        schedule = m_manager->getSchedule(m_scheduleId);
    }

    schedule.scheduleName = m_nameEdit->text().trimmed();
    schedule.type = static_cast<MessageSchedule::ScheduleType>(
        m_typeCombo->currentData().toInt());
    schedule.reportType = static_cast<MessageSchedule::ReportType>(
        m_reportTypeCombo->currentData().toInt());

    schedule.sendTime = m_timeEdit->time();
    schedule.dayOfWeek = m_dayOfWeekSpin->value();
    schedule.dayOfMonth = m_dayOfMonthSpin->value();
    schedule.salesThreshold = m_thresholdSpin->value();

    schedule.providerName = m_providerCombo->currentText();

    // Parse recipients
    schedule.recipients.clear();
    QStringList recipients = m_recipientsEdit->toPlainText()
                                 .split('\n', Qt::SkipEmptyParts);
    for (const QString &recipient : recipients) {
        QString clean = recipient.trimmed();
        if (!clean.isEmpty()) {
            schedule.recipients << clean;
        }
    }

    schedule.notes = m_notesEdit->toPlainText().trimmed();
    schedule.isActive = m_activeCheck->isChecked();

    if (m_scheduleId < 0) {
        // New schedule
        schedule.createdDate = QDateTime::currentDateTime();
        schedule.createdBy = "User"; // Could get from login system
    }

    return schedule;
}

void ScheduleEditorDialog::onScheduleTypeChanged(int index)
{
    Q_UNUSED(index);
    updateVisibility();
}

void ScheduleEditorDialog::onAddRecipient()
{
    // Could add a dialog to select from contacts
    // For now, just show a hint
    QMessageBox::information(this, "Add Recipient",
                             "Enter recipients directly in the text box,\n"
                             "one per line.");
}

void ScheduleEditorDialog::onRemoveRecipient()
{
    // Could add functionality to remove selected recipient
    // For now, users can just edit the text
}
