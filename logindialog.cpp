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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

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

    // Connect signals
    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(cancelButton, &QPushButton::clicked, this, &LoginDialog::onCancelClicked);
    connect(passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
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
