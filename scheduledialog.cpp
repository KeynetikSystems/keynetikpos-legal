// =============================================================================
// scheduledialog.cpp — Implementation of ScheduleDialog (see scheduledialog.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - refresh() rebuilds the table from ScheduleManager::getAllSchedules();
//    add/edit open ScheduleEditorDialog and persist through the manager.
//  - Send Now calls ScheduleManager::sendNow() so providers and recipients
//    can be verified without waiting for the trigger time.
// =============================================================================
#include "scheduledialog.h"
#include "scheduleeditordialog.h"
#include "colorscheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

ScheduleDialog::ScheduleDialog(ScheduleManager *manager, QWidget *parent)
    : QDialog(parent), m_manager(manager)
{
    setWindowTitle("Message Schedules");
    resize(900, 550);
    setupUi();
    refresh();
}

void ScheduleDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    auto *titleLbl = new QLabel("Automated Report Schedules", this);
    QFont f = titleLbl->font();
    f.setPointSize(14); f.setBold(true);
    titleLbl->setFont(f);
    mainLayout->addWidget(titleLbl);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        {"Name", "Type", "Report", "Provider", "Recipients", "Status"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    mainLayout->addWidget(m_table);

    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ScheduleDialog::onSelectionChanged);

    auto *btnLayout = new QHBoxLayout;
    m_addBtn     = new QPushButton("Add Schedule",   this);
    m_editBtn    = new QPushButton("Edit",           this);
    m_deleteBtn  = new QPushButton("Delete",         this);
    m_toggleBtn  = new QPushButton("Toggle Active",  this);
    m_sendNowBtn = new QPushButton("Send Now",       this);

    m_addBtn->setProperty("kind", "primary");
    m_deleteBtn->setProperty("kind", "danger");
    m_sendNowBtn->setProperty("kind", "info");

    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_toggleBtn->setEnabled(false);
    m_sendNowBtn->setEnabled(false);

    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_editBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_toggleBtn);
    btnLayout->addWidget(m_sendNowBtn);
    btnLayout->addStretch();

    auto *closeBtn = new QPushButton("Close", this);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_addBtn,     &QPushButton::clicked, this, &ScheduleDialog::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &ScheduleDialog::onEdit);
    connect(m_deleteBtn,  &QPushButton::clicked, this, &ScheduleDialog::onDelete);
    connect(m_toggleBtn,  &QPushButton::clicked, this, &ScheduleDialog::onToggleActive);
    connect(m_sendNowBtn, &QPushButton::clicked, this, &ScheduleDialog::onSendNow);
    connect(closeBtn,     &QPushButton::clicked, this, &QDialog::accept);
}

void ScheduleDialog::refresh()
{
    m_table->setRowCount(0);
    for (const MessageSchedule &s : m_manager->getAllSchedules()) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(s.scheduleName));
        m_table->setItem(row, 1, new QTableWidgetItem(s.typeString()));
        m_table->setItem(row, 2, new QTableWidgetItem(s.reportTypeString()));
        m_table->setItem(row, 3, new QTableWidgetItem(s.providerName));
        m_table->setItem(row, 4, new QTableWidgetItem(
                                     QString::number(s.recipients.size()) + " recipient(s)"));

        const ColorScheme scheme = getColorScheme();
        auto *statusItem = new QTableWidgetItem(s.isActive ? "Active" : "Paused");
        statusItem->setForeground(QBrush(QColor(s.isActive ? scheme.success : scheme.error)));
        QFont f = statusItem->font(); f.setBold(true); statusItem->setFont(f);
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 5, statusItem);

        // Store schedule id in first cell
        m_table->item(row, 0)->setData(Qt::UserRole, s.scheduleId);
    }
    m_table->resizeColumnsToContents();
    onSelectionChanged();
}

void ScheduleDialog::onSelectionChanged()
{
    bool sel = !m_table->selectedItems().isEmpty();
    m_editBtn->setEnabled(sel);
    m_deleteBtn->setEnabled(sel);
    m_toggleBtn->setEnabled(sel);
    m_sendNowBtn->setEnabled(sel);
}

void ScheduleDialog::onAdd()
{
    ScheduleEditorDialog dlg(m_manager, -1, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_manager->addSchedule(dlg.getSchedule());
        refresh();
    }
}

void ScheduleDialog::onEdit()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    ScheduleEditorDialog dlg(m_manager, id, this);
    if (dlg.exec() == QDialog::Accepted) {
        MessageSchedule s = dlg.getSchedule();
        s.scheduleId = id;
        m_manager->updateSchedule(s);
        refresh();
    }
}

void ScheduleDialog::onDelete()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    QString name = m_table->item(row, 0)->text();
    if (QMessageBox::question(this, "Delete Schedule",
                              QString("Delete schedule '%1'?").arg(name),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_manager->deleteSchedule(id);
        refresh();
    }
}

void ScheduleDialog::onToggleActive()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    MessageSchedule s = m_manager->getSchedule(id);
    s.isActive = !s.isActive;
    m_manager->updateSchedule(s);
    refresh();
}

void ScheduleDialog::onSendNow()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    if (m_manager->sendNow(id, QString())) {
        QMessageBox::information(this, "Sent", "Message dispatched successfully.");
    } else {
        QMessageBox::warning(this, "Failed",
                             "Could not send. Check provider configuration.");
    }
}
