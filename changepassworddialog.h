// =============================================================================
// changepassworddialog.h — ChangePasswordDialog: single-form password change
// -----------------------------------------------------------------------------
// WHAT: One dialog with current/new/confirm fields and inline validation,
//       used both for voluntary password changes (User menu) and the forced
//       first-login change (main.cpp).
// HOW:  Validation (min length, confirmation match, new != current) happens
//       inline with an error label, so a typo means fixing one field — not
//       restarting a chain of prompts. In forced mode the current-password
//       field is hidden (the caller already authenticated) and Cancel is
//       relabelled to make the consequence (logout) clear.
// WHY:  Replaces two triple-QInputDialog flows that lost all input on the
//       third-step mistake, and unifies the password policy: minimum 8
//       characters everywhere (MainWindow previously said 6, main.cpp 8).
// =============================================================================
#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT

public:
    static constexpr int MIN_PASSWORD_LENGTH = 8;

    enum class Mode {
        Voluntary,   // user-initiated: asks for the current password
        Forced       // first login: current password already known to caller
    };

    explicit ChangePasswordDialog(Mode mode, QWidget *parent = nullptr);

    QString currentPassword() const;
    QString newPassword() const;

    // Forced mode: lets the caller reject "new == current" inline.
    void setKnownCurrentPassword(const QString &password);

    // Show a failure from the caller (e.g. wrong current password) without
    // closing the dialog.
    void showError(const QString &message);

private slots:
    void onAccept();

private:
    Mode       m_mode;
    QString    m_knownCurrent;
    QLineEdit *currentEdit { nullptr };
    QLineEdit *newEdit     { nullptr };
    QLineEdit *confirmEdit { nullptr };
    QLabel    *errorLabel  { nullptr };
};

#endif // CHANGEPASSWORDDIALOG_H
