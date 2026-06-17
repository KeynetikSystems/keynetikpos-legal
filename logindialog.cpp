// =============================================================================
// logindialog.cpp — Implementation of LoginDialog (see logindialog.h for the
// full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Pure presentation: collects username/password and accepts the dialog;
//    main.cpp performs UserManager::login() and owns the retry/lockout policy.
//  - Theming comes entirely from the application-wide stylesheet (appstyle.h),
//    applied in main.cpp before this dialog is shown; widgets opt into
//    semantic styling via "kind" properties.
// =============================================================================
#include "LoginDialog.h"
#include "licensemanager.h"
#include "usermanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDialog>
#include <QInputDialog>
#include <QSysInfo>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle("KEYNETIK POS - Login");
    setModal(true);
    resize(400, 320);
    setMinimumSize(360, 280);
}

void LoginDialog::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Title
    titleLabel = new QLabel("KEYNETIK POS");
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont;
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // Subtitle
    QLabel *subtitle = new QLabel("Please log in to continue");
    subtitle->setAlignment(Qt::AlignCenter);
    QFont subtitleFont;
    subtitleFont.setPointSize(10);
    subtitle->setFont(subtitleFont);
    mainLayout->addWidget(subtitle);

    mainLayout->addSpacing(20);

    // Form layout
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(15);

    usernameEdit = new QLineEdit();
    usernameEdit->setPlaceholderText("Enter username");
    usernameEdit->setMinimumHeight(40);
    formLayout->addRow("Username:", usernameEdit);

    passwordEdit = new QLineEdit();
    passwordEdit->setPlaceholderText("Enter password");
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setMinimumHeight(40);
    formLayout->addRow("Password:", passwordEdit);

    mainLayout->addLayout(formLayout);

    // Error label
    errorLabel = new QLabel();
    errorLabel->setProperty("kind", "danger");
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setVisible(false);
    mainLayout->addWidget(errorLabel);

    mainLayout->addStretch();

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    loginButton = new QPushButton("Login");
    loginButton->setMinimumHeight(45);
    loginButton->setProperty("kind", "primary");
    loginButton->setDefault(true);

    cancelButton = new QPushButton("Cancel");
    cancelButton->setMinimumHeight(45);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(loginButton);

    mainLayout->addLayout(buttonLayout);

    // Forgot password link-style button
    forgotButton = new QPushButton("Forgot password?");
    forgotButton->setFlat(true);
    forgotButton->setStyleSheet("color: #0078d4; text-decoration: underline; "
                                "border: none; background: transparent;");
    QHBoxLayout *forgotLayout = new QHBoxLayout();
    forgotLayout->addStretch();
    forgotLayout->addWidget(forgotButton);
    mainLayout->addLayout(forgotLayout);

    // Connect signals
    connect(loginButton,  &QPushButton::clicked,      this, &LoginDialog::onLoginClicked);
    connect(cancelButton, &QPushButton::clicked,       this, &LoginDialog::onCancelClicked);
    connect(passwordEdit, &QLineEdit::returnPressed,   this, &LoginDialog::onLoginClicked);
    connect(forgotButton, &QPushButton::clicked,       this, &LoginDialog::onForgotPassword);
}

void LoginDialog::onLoginClicked() {
    QString user = usernameEdit->text().trimmed();
    QString pass = passwordEdit->text();

    if (user.isEmpty() || pass.isEmpty()) {
        errorLabel->setText("Please enter username and password");
        errorLabel->setVisible(true);
        return;
    }

    username = user;
    password = pass;
    accept();
}

void LoginDialog::onCancelClicked() {
    reject();
}

QString LoginDialog::getUsername() const {
    return username;
}

QString LoginDialog::getPassword() const {
    return password;
}

void LoginDialog::onForgotPassword()
{
    // Step 1 — tell the user what they need and show the device ID for support.
    const QString deviceId = QString::fromLatin1(QSysInfo::machineUniqueId());
    QMessageBox info(this);
    info.setWindowTitle("Password Recovery");
    info.setIcon(QMessageBox::Information);
    info.setText("<b>Password Recovery</b>");
    info.setInformativeText(
        "To reset a password you need your <b>license key</b>.<br><br>"
        "If you don't have it, contact:<br>"
        "<b>support@keynetik.com</b><br><br>"
        "Device ID (quote this to support):<br>"
        "<code>" + deviceId + "</code>");
    info.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    info.setDefaultButton(QMessageBox::Ok);
    info.button(QMessageBox::Ok)->setText("I have my key");
    if (info.exec() != QMessageBox::Ok)
        return;

    // Step 2 — get the username to reset.
    bool ok = false;
    const QString targetUser = QInputDialog::getText(
        this, "Password Recovery",
        "Enter the username whose password you want to reset:",
        QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || targetUser.isEmpty()) return;

    // Step 3 — verify the license key.
    const QString licKey = QInputDialog::getText(
        this, "Password Recovery",
        "Enter your license key to confirm your identity:",
        QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || licKey.isEmpty()) return;

    if (!LicenseManager::instance().verifyKeyFormat(licKey)) {
        QMessageBox::warning(this, "Password Recovery",
            "The license key you entered is not valid.\n"
            "Contact support@keynetik.com for help.");
        return;
    }

    // Step 4 — choose a temporary password and reset.
    const QString tempPwd = QInputDialog::getText(
        this, "Password Recovery",
        "Enter a temporary password for this account:\n"
        "(The user will be asked to change it on next login)",
        QLineEdit::Password, "", &ok).trimmed();
    if (!ok || tempPwd.length() < 4) {
        if (ok)
            QMessageBox::warning(this, "Password Recovery",
                "Temporary password must be at least 4 characters.");
        return;
    }

    if (UserManager::instance().adminResetPassword(targetUser, tempPwd)) {
        QMessageBox::information(this, "Password Recovery",
            QString("Password for <b>%1</b> has been reset.<br><br>"
                    "Temporary password: <b>%2</b><br><br>"
                    "The user will be required to set a new password on next login.")
                .arg(targetUser.toHtmlEscaped(), tempPwd));
        usernameEdit->setText(targetUser);
        passwordEdit->clear();
        passwordEdit->setFocus();
    } else {
        QMessageBox::warning(this, "Password Recovery",
            QString("Could not reset password for <b>%1</b>.<br>"
                    "Check the username and try again.").arg(targetUser.toHtmlEscaped()));
    }
}
