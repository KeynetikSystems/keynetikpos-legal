// =============================================================================
// logindialog.h — LoginDialog: the pre-main-window credentials prompt
// -----------------------------------------------------------------------------
// WHAT: The modal username/password dialog shown before MainWindow.
// HOW:  Hand-built UI (no .ui file): line edits (password masked), login and
//       cancel buttons, inline error label; themed by the application-wide
//       stylesheet (appstyle.h). It only
//       COLLECTS credentials — main.cpp performs the actual
//       UserManager::login() call and handles attempt counting.
// WHY:  Keeping authentication logic out of the dialog separates presentation
//       from policy: the retry limit, lockout messaging, and forced password
//       change all live in one place (main.cpp / UserManager) instead of
//       being baked into a widget.
// =============================================================================
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString getUsername() const;
    QString getPassword() const;

private slots:
    void onLoginClicked();
    void onCancelClicked();
    void onForgotPassword();

private:
    void setupUI();

    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QPushButton *loginButton;
    QPushButton *cancelButton;
    QPushButton *forgotButton;
    QLabel *titleLabel;
    QLabel *errorLabel;

    QString username;
    QString password;
};

#endif // LOGINDIALOG_H
