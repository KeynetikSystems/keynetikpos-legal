// =============================================================================
// changepassworddialog.cpp — Implementation of ChangePasswordDialog (see
// changepassworddialog.h for the full WHAT/HOW/WHY).
// =============================================================================
#include "changepassworddialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>

ChangePasswordDialog::ChangePasswordDialog(Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_mode(mode)
{
    setWindowTitle(mode == Mode::Forced ? "Password Change Required"
                                        : "Change Password");
    setModal(true);
    setMinimumWidth(380);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    if (mode == Mode::Forced) {
        QLabel *notice = new QLabel(
            "You must set a new password before continuing.");
        notice->setWordWrap(true);
        mainLayout->addWidget(notice);
    }

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    if (mode == Mode::Voluntary) {
        currentEdit = new QLineEdit();
        currentEdit->setEchoMode(QLineEdit::Password);
        currentEdit->setMinimumHeight(36);
        form->addRow("Current password:", currentEdit);
    }

    newEdit = new QLineEdit();
    newEdit->setEchoMode(QLineEdit::Password);
    newEdit->setMinimumHeight(36);
    newEdit->setPlaceholderText(
        QString("Minimum %1 characters").arg(MIN_PASSWORD_LENGTH));
    form->addRow("New password:", newEdit);

    confirmEdit = new QLineEdit();
    confirmEdit->setEchoMode(QLineEdit::Password);
    confirmEdit->setMinimumHeight(36);
    form->addRow("Confirm password:", confirmEdit);

    mainLayout->addLayout(form);

    errorLabel = new QLabel();
    errorLabel->setProperty("kind", "danger");
    errorLabel->setWordWrap(true);
    errorLabel->setVisible(false);
    mainLayout->addWidget(errorLabel);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();

    QPushButton *cancelBtn = new QPushButton(
        mode == Mode::Forced ? "Cancel && Log Out" : "Cancel");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancelBtn);

    QPushButton *okBtn = new QPushButton("Change Password");
    okBtn->setProperty("kind", "primary");
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &ChangePasswordDialog::onAccept);
    buttons->addWidget(okBtn);

    mainLayout->addLayout(buttons);

    (currentEdit ? currentEdit : newEdit)->setFocus();
}

QString ChangePasswordDialog::currentPassword() const
{
    return currentEdit ? currentEdit->text() : m_knownCurrent;
}

QString ChangePasswordDialog::newPassword() const
{
    return newEdit->text();
}

void ChangePasswordDialog::setKnownCurrentPassword(const QString &password)
{
    m_knownCurrent = password;
}

void ChangePasswordDialog::showError(const QString &message)
{
    errorLabel->setText(message);
    errorLabel->setVisible(true);
}

void ChangePasswordDialog::onAccept()
{
    const QString current = currentPassword();
    const QString next    = newEdit->text();

    if (m_mode == Mode::Voluntary && current.isEmpty()) {
        showError("Please enter your current password.");
        currentEdit->setFocus();
        return;
    }
    if (next.length() < MIN_PASSWORD_LENGTH) {
        showError(QString("The new password must be at least %1 characters.")
                      .arg(MIN_PASSWORD_LENGTH));
        newEdit->setFocus();
        return;
    }
    if (!current.isEmpty() && next == current) {
        showError("The new password must differ from the current one.");
        newEdit->setFocus();
        return;
    }
    if (next != confirmEdit->text()) {
        showError("The passwords do not match.");
        confirmEdit->setFocus();
        confirmEdit->selectAll();
        return;
    }

    accept();
}
