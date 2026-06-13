// =============================================================================
// usermanagementdialog.h — Admin UI for managing user accounts
// -----------------------------------------------------------------------------
// WHAT: Three classes. UserManagementDialog: filterable/searchable user table
//       with add / edit / delete / toggle-status / change-password actions.
//       UserDialog: the add/edit form with live password-strength feedback.
//       PermissionsViewDialog: read-only table of what a given role may do.
// HOW:  All three delegate every mutation to UserManager (which enforces
//       validation and hashes passwords); the dialogs never touch SQL or
//       hashes themselves. The list dialog keeps a local QVector<User> cache
//       and filters it in memory for search/role filters.
//       UserDialog::validateInput() checks username uniqueness and email
//       format via UserManager before accepting.
// WHY:  Routing everything through UserManager guarantees the same rules apply
//       however a user is created, and keeps password handling in exactly one
//       audited code path. The permissions viewer exists so admins can SEE the
//       effective RBAC matrix instead of guessing what "Manager" means.
// =============================================================================
#ifndef USERMANAGEMENTDIALOG_H
#define USERMANAGEMENTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include "user.h"

class UserManagementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserManagementDialog(QWidget *parent = nullptr);

private slots:
    void onAddUser();
    void onEditUser();
    void onDeleteUser();
    void onToggleStatus();
    void onChangePassword();
    void onRefresh();
    void onUserSelected();
    void onSearchTextChanged(const QString &text);
    void onRoleFilterChanged(int index);
    void onViewPermissions();

private:
    void setupUI();
    void loadUsers();
    void filterUsers();
    void showUserDialog(bool isEdit = false);
    void showChangePasswordDialog();

    QTableWidget *userTable;
    QPushButton *addButton;
    QPushButton *editButton;
    QPushButton *deleteButton;
    QPushButton *toggleStatusButton;
    QPushButton *changePasswordButton;
    QPushButton *refreshButton;
    QPushButton *viewPermissionsButton;
    QLineEdit *searchEdit;
    QComboBox *roleFilterCombo;

    QVector<User> allUsers;
};

class UserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserDialog(const User &user, bool isEdit, QWidget *parent = nullptr);

    User getUser() const;
    QString getPassword() const;

private slots:
    void onSave();
    void onCancel();
    void onPasswordStrengthCheck();

private:
    void setupUI();
    bool validateInput();

    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLineEdit *confirmPasswordEdit;
    QLineEdit *fullNameEdit;
    QLineEdit *emailEdit;
    QComboBox *roleCombo;
    QCheckBox *activeCheckBox;
    QLabel *passwordStrengthLabel;

    User originalUser;
    bool editMode;
};

class PermissionsViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PermissionsViewDialog(UserRole role, QWidget *parent = nullptr);

private:
    void setupUI();
    void loadPermissions();

    QTableWidget *permissionsTable;
    UserRole userRole;
};

#endif // USERMANAGEMENTDIALOG_H
