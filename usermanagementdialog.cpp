// =============================================================================
// usermanagementdialog.cpp — Implementation of UserManagementDialog,
// UserDialog, and PermissionsViewDialog (see usermanagementdialog.h for the
// full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Every mutation (create/update/delete/toggle/password) is delegated to
//    UserManager — these dialogs never touch SQL or password hashes.
//  - The user list is cached in allUsers and filtered in memory for the
//    search box and role combo; loadUsers() refreshes from UserManager.
//  - UserDialog validates input (username uniqueness, email format, password
//    confirmation + strength) before accept(); PermissionsViewDialog renders
//    RoleManager::getPermissionsForRole() read-only.
// =============================================================================
#include "usermanagementdialog.h"
#include "usermanager.h"
#include "colorscheme.h"
#include "appstyle.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QInputDialog>

UserManagementDialog::UserManagementDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    loadUsers();

    setWindowTitle("User Management");
    resize(900, 600);
}

void UserManagementDialog::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Title — use object name so we can style it specifically
    QLabel *titleLabel = new QLabel("User Management");
    titleLabel->setObjectName("titleLabel");
    QFont titleFont;
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    // Force transparent background so it inherits the dialog bg properly
    titleLabel->setAutoFillBackground(false);
    mainLayout->addWidget(titleLabel);

    // Search and filter bar — wrap in a container widget so bg is consistent
    QWidget *filterContainer = new QWidget();
    filterContainer->setObjectName("filterContainer");
    QHBoxLayout *filterLayout = new QHBoxLayout(filterContainer);
    filterLayout->setSpacing(12);
    filterLayout->setContentsMargins(0, 0, 0, 0);

    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("🔍 Search users...");
    searchEdit->setMinimumWidth(300);
    filterLayout->addWidget(searchEdit);

    QLabel *roleLabel = new QLabel("Filter by Role:");
    roleLabel->setObjectName("filterLabel");
    filterLayout->addWidget(roleLabel);

    roleFilterCombo = new QComboBox();
    roleFilterCombo->addItem("All Roles");
    roleFilterCombo->addItems(RoleManager::getAllRoles());
    roleFilterCombo->setMinimumWidth(150);
    filterLayout->addWidget(roleFilterCombo);

    filterLayout->addStretch();

    mainLayout->addWidget(filterContainer);

    // User table
    userTable = new QTableWidget();
    userTable->setColumnCount(7);
    userTable->setHorizontalHeaderLabels(QStringList()
                                         << "ID" << "Username" << "Full Name" << "Email" << "Role" << "Status" << "Last Login");
    userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    userTable->horizontalHeader()->setStretchLastSection(true);
    userTable->verticalHeader()->setVisible(false);
    userTable->setAlternatingRowColors(true);
    userTable->setShowGrid(true);

    userTable->setColumnWidth(0, 50);
    userTable->setColumnWidth(1, 120);
    userTable->setColumnWidth(2, 150);
    userTable->setColumnWidth(3, 200);
    userTable->setColumnWidth(4, 100);
    userTable->setColumnWidth(5, 80);

    mainLayout->addWidget(userTable);

    // Button bar
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    addButton = new QPushButton("➕ Add User");
    editButton = new QPushButton("✏️ Edit User");
    deleteButton = new QPushButton("🗑️ Delete User");
    toggleStatusButton = new QPushButton("🔄 Toggle Status");
    changePasswordButton = new QPushButton("🔑 Change Password");
    viewPermissionsButton = new QPushButton("👁️ View Permissions");
    refreshButton = new QPushButton("🔄 Refresh");

    addButton->setProperty("kind", "primary");
    deleteButton->setProperty("kind", "danger");
    refreshButton->setProperty("kind", "info");

    editButton->setEnabled(false);
    deleteButton->setEnabled(false);
    toggleStatusButton->setEnabled(false);
    changePasswordButton->setEnabled(false);
    viewPermissionsButton->setEnabled(false);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(toggleStatusButton);
    buttonLayout->addWidget(changePasswordButton);
    buttonLayout->addWidget(viewPermissionsButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(refreshButton);

    mainLayout->addLayout(buttonLayout);

    // Close button
    QHBoxLayout *closeLayout = new QHBoxLayout();
    closeLayout->addStretch();
    QPushButton *closeButton = new QPushButton("Close");
    closeButton->setMinimumWidth(120);
    closeLayout->addWidget(closeButton);
    mainLayout->addLayout(closeLayout);

    // Connect signals
    connect(addButton, &QPushButton::clicked, this, &UserManagementDialog::onAddUser);
    connect(editButton, &QPushButton::clicked, this, &UserManagementDialog::onEditUser);
    connect(deleteButton, &QPushButton::clicked, this, &UserManagementDialog::onDeleteUser);
    connect(toggleStatusButton, &QPushButton::clicked, this, &UserManagementDialog::onToggleStatus);
    connect(changePasswordButton, &QPushButton::clicked, this, &UserManagementDialog::onChangePassword);
    connect(viewPermissionsButton, &QPushButton::clicked, this, &UserManagementDialog::onViewPermissions);
    connect(refreshButton, &QPushButton::clicked, this, &UserManagementDialog::onRefresh);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(userTable, &QTableWidget::itemSelectionChanged, this, &UserManagementDialog::onUserSelected);
    connect(searchEdit, &QLineEdit::textChanged, this, &UserManagementDialog::onSearchTextChanged);
    connect(roleFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UserManagementDialog::onRoleFilterChanged);
}

void UserManagementDialog::loadUsers() {
    allUsers = UserManager::instance().getAllUsers();
    filterUsers();
}

void UserManagementDialog::filterUsers() {
    QString searchText = searchEdit->text().toLower();
    QString roleFilter = roleFilterCombo->currentText();

    userTable->setRowCount(0);

    for (const User &user : allUsers) {
        if (!searchText.isEmpty()) {
            if (!user.username.toLower().contains(searchText) &&
                !user.fullName.toLower().contains(searchText) &&
                !user.email.toLower().contains(searchText)) {
                continue;
            }
        }

        if (roleFilter != "All Roles") {
            if (RoleManager::roleToString(user.role) != roleFilter) {
                continue;
            }
        }

        int row = userTable->rowCount();
        userTable->insertRow(row);

        userTable->setItem(row, 0, new QTableWidgetItem(QString::number(user.id)));
        userTable->setItem(row, 1, new QTableWidgetItem(user.username));
        userTable->setItem(row, 2, new QTableWidgetItem(user.fullName));
        userTable->setItem(row, 3, new QTableWidgetItem(user.email));
        userTable->setItem(row, 4, new QTableWidgetItem(RoleManager::roleToString(user.role)));
        userTable->setItem(row, 5, new QTableWidgetItem(user.isActive ? "✓ Active" : "✗ Inactive"));
        userTable->setItem(row, 6, new QTableWidgetItem(
                                       user.lastLogin.isValid() ? user.lastLogin.toString("yyyy-MM-dd hh:mm") : "Never"));

        const ColorScheme scheme = getColorScheme();
        QTableWidgetItem *statusItem = userTable->item(row, 5);
        statusItem->setForeground(QBrush(QColor(user.isActive ? scheme.success : scheme.error)));
        QFont font = statusItem->font();
        font.setBold(true);
        statusItem->setFont(font);

        userTable->item(row, 0)->setTextAlignment(Qt::AlignCenter);
        userTable->item(row, 5)->setTextAlignment(Qt::AlignCenter);
    }
}

void UserManagementDialog::onAddUser() {
    showUserDialog(false);
}

void UserManagementDialog::onEditUser() {
    QList<QTableWidgetItem*> selected = userTable->selectedItems();
    if (selected.isEmpty()) return;
    showUserDialog(true);
}

void UserManagementDialog::showUserDialog(bool isEdit) {
    User user;

    if (isEdit) {
        int row = userTable->currentRow();
        if (row < 0) return;
        int userId = userTable->item(row, 0)->text().toInt();
        user = UserManager::instance().getUserById(userId);
    }

    UserDialog dialog(user, isEdit, this);
    if (dialog.exec() == QDialog::Accepted) {
        User newUser = dialog.getUser();

        if (isEdit) {
            if (UserManager::instance().updateUser(newUser)) {
                QMessageBox::information(this, "Success", "User updated successfully!");
                loadUsers();
            } else {
                QMessageBox::warning(this, "Error", "Failed to update user!");
            }
        } else {
            if (UserManager::instance().createUser(newUser, dialog.getPassword())) {
                QMessageBox::information(this, "Success", "User created successfully!");
                loadUsers();
            } else {
                QMessageBox::warning(this, "Error", "Failed to create user!");
            }
        }
    }
}

void UserManagementDialog::onDeleteUser() {
    int row = userTable->currentRow();
    if (row < 0) return;

    int userId = userTable->item(row, 0)->text().toInt();
    QString username = userTable->item(row, 1)->text();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete",
        QString("Are you sure you want to delete user '%1'?").arg(username),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (UserManager::instance().deleteUser(userId)) {
            QMessageBox::information(this, "Success", "User deleted successfully!");
            loadUsers();
        } else {
            QMessageBox::warning(this, "Error", "Failed to delete user! You cannot delete yourself.");
        }
    }
}

void UserManagementDialog::onToggleStatus() {
    int row = userTable->currentRow();
    if (row < 0) return;

    int userId = userTable->item(row, 0)->text().toInt();

    if (UserManager::instance().toggleUserStatus(userId)) {
        QMessageBox::information(this, "Success", "User status toggled successfully!");
        loadUsers();
    } else {
        QMessageBox::warning(this, "Error", "Failed to toggle user status! You cannot disable yourself.");
    }
}

void UserManagementDialog::onChangePassword() {
    showChangePasswordDialog();
}

void UserManagementDialog::showChangePasswordDialog() {
    int row = userTable->currentRow();
    if (row < 0) return;

    int userId = userTable->item(row, 0)->text().toInt();
    QString username = userTable->item(row, 1)->text();

    bool ok;
    QString newPassword = QInputDialog::getText(
        this, "Change Password",
        QString("Enter new password for %1:").arg(username),
        QLineEdit::Password, "", &ok);

    if (ok && !newPassword.isEmpty()) {
        if (UserManager::instance().changePassword(userId, newPassword)) {
            QMessageBox::information(this, "Success", "Password changed successfully!");
        } else {
            QMessageBox::warning(this, "Error", "Failed to change password!");
        }
    }
}

void UserManagementDialog::onRefresh() {
    loadUsers();
}

void UserManagementDialog::onUserSelected() {
    bool hasSelection = !userTable->selectedItems().isEmpty();
    editButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
    toggleStatusButton->setEnabled(hasSelection);
    changePasswordButton->setEnabled(hasSelection);
    viewPermissionsButton->setEnabled(hasSelection);
}

void UserManagementDialog::onSearchTextChanged(const QString &text) {
    Q_UNUSED(text);
    filterUsers();
}

void UserManagementDialog::onRoleFilterChanged(int index) {
    Q_UNUSED(index);
    filterUsers();
}

void UserManagementDialog::onViewPermissions() {
    int row = userTable->currentRow();
    if (row < 0) return;

    int userId = userTable->item(row, 0)->text().toInt();
    User user = UserManager::instance().getUserById(userId);

    PermissionsViewDialog dialog(user.role, this);
    dialog.exec();
}

// ─────────────────────────────────────────────────────────────────────────────
// UserDialog Implementation
// ─────────────────────────────────────────────────────────────────────────────

UserDialog::UserDialog(const User &user, bool isEdit, QWidget *parent)
    : QDialog(parent), originalUser(user), editMode(isEdit)
{
    setupUI();

    setWindowTitle(isEdit ? "Edit User" : "Add User");
    setModal(true);
    resize(500, 550);
}

void UserDialog::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(25, 25, 25, 25);

    QLabel *titleLabel = new QLabel(editMode ? "Edit User Details" : "Create New User");
    titleLabel->setObjectName("dialogTitle");
    QFont titleFont;
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(15);
    formLayout->setLabelAlignment(Qt::AlignRight);

    usernameEdit = new QLineEdit();
    usernameEdit->setPlaceholderText("Enter username (min 3 characters)");
    if (editMode) usernameEdit->setText(originalUser.username);
    formLayout->addRow("Username:", usernameEdit);

    if (!editMode) {
        passwordEdit = new QLineEdit();
        passwordEdit->setEchoMode(QLineEdit::Password);
        passwordEdit->setPlaceholderText("Enter password");
        formLayout->addRow("Password:", passwordEdit);

        confirmPasswordEdit = new QLineEdit();
        confirmPasswordEdit->setEchoMode(QLineEdit::Password);
        confirmPasswordEdit->setPlaceholderText("Confirm password");
        formLayout->addRow("Confirm Password:", confirmPasswordEdit);

        passwordStrengthLabel = new QLabel();
        passwordStrengthLabel->setObjectName("strengthLabel");
        formLayout->addRow("", passwordStrengthLabel);

        connect(passwordEdit, &QLineEdit::textChanged, this, &UserDialog::onPasswordStrengthCheck);
    }

    fullNameEdit = new QLineEdit();
    fullNameEdit->setPlaceholderText("Enter full name");
    if (editMode) fullNameEdit->setText(originalUser.fullName);
    formLayout->addRow("Full Name:", fullNameEdit);

    emailEdit = new QLineEdit();
    emailEdit->setPlaceholderText("Enter email address");
    if (editMode) emailEdit->setText(originalUser.email);
    formLayout->addRow("Email:", emailEdit);

    roleCombo = new QComboBox();
    roleCombo->addItems(RoleManager::getAllRoles());
    if (editMode) roleCombo->setCurrentText(RoleManager::roleToString(originalUser.role));
    formLayout->addRow("Role:", roleCombo);

    activeCheckBox = new QCheckBox("Account is active");
    activeCheckBox->setChecked(editMode ? originalUser.isActive : true);
    formLayout->addRow("Status:", activeCheckBox);

    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveButton   = new QPushButton(editMode ? "💾 Update User" : "➕ Create User");
    QPushButton *cancelButton = new QPushButton("✖ Cancel");

    saveButton->setProperty("kind", "primary");
    saveButton->setMinimumWidth(140);
    cancelButton->setMinimumWidth(140);

    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(saveButton);

    mainLayout->addLayout(buttonLayout);

    connect(saveButton,   &QPushButton::clicked, this, &UserDialog::onSave);
    connect(cancelButton, &QPushButton::clicked, this, &UserDialog::onCancel);
}

bool UserDialog::validateInput() {
    if (usernameEdit->text().trimmed().length() < 3) {
        QMessageBox::warning(this, "Validation Error", "Username must be at least 3 characters!");
        return false;
    }

    if (!editMode) {
        if (passwordEdit->text().length() < 6) {
            QMessageBox::warning(this, "Validation Error", "Password must be at least 6 characters!");
            return false;
        }
        if (passwordEdit->text() != confirmPasswordEdit->text()) {
            QMessageBox::warning(this, "Validation Error", "Passwords do not match!");
            return false;
        }
    }

    if (fullNameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Full name is required!");
        return false;
    }

    if (emailEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Email is required!");
        return false;
    }

    int excludeId = editMode ? originalUser.id : -1;
    if (!UserManager::instance().validateUsername(usernameEdit->text().trimmed(), excludeId)) {
        QMessageBox::warning(this, "Validation Error", "Username already exists!");
        return false;
    }

    if (!UserManager::instance().validateEmail(emailEdit->text().trimmed(), excludeId)) {
        QMessageBox::warning(this, "Validation Error", "Email is invalid or already exists!");
        return false;
    }

    return true;
}

void UserDialog::onSave() {
    if (validateInput()) accept();
}

void UserDialog::onCancel() {
    reject();
}

void UserDialog::onPasswordStrengthCheck() {
    if (!editMode && passwordEdit) {
        QString strength = UserManager::instance().getPasswordStrength(passwordEdit->text());
        passwordStrengthLabel->setText(QString("Strength: %1").arg(strength));

        const char *kind = strength == "Weak"   ? "danger"
                         : strength == "Medium" ? "warning"
                                                : "success";
        passwordStrengthLabel->setProperty("bold", "true");
        passwordStrengthLabel->setProperty("textScale", "sm");
        setStyleProperty(passwordStrengthLabel, "kind", kind);
    }
}

User UserDialog::getUser() const {
    User user;
    if (editMode) user = originalUser;

    user.username = usernameEdit->text().trimmed();
    user.fullName = fullNameEdit->text().trimmed();
    user.email    = emailEdit->text().trimmed();
    user.role     = RoleManager::stringToRole(roleCombo->currentText());
    user.isActive = activeCheckBox->isChecked();

    return user;
}

QString UserDialog::getPassword() const {
    return editMode ? QString() : passwordEdit->text();
}

// ─────────────────────────────────────────────────────────────────────────────
// PermissionsViewDialog Implementation
// ─────────────────────────────────────────────────────────────────────────────

PermissionsViewDialog::PermissionsViewDialog(UserRole role, QWidget *parent)
    : QDialog(parent), userRole(role)
{
    setupUI();
    loadPermissions();

    setWindowTitle(QString("Role Permissions - %1").arg(RoleManager::roleToString(role)));
    resize(550, 450);
}

void PermissionsViewDialog::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel *titleLabel = new QLabel(QString("Permissions for %1 Role").arg(RoleManager::roleToString(userRole)));
    titleLabel->setObjectName("permTitle");
    QFont titleFont;
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    permissionsTable = new QTableWidget();
    permissionsTable->setColumnCount(2);
    permissionsTable->setHorizontalHeaderLabels(QStringList() << "Permission" << "Access");
    permissionsTable->horizontalHeader()->setStretchLastSection(true);
    permissionsTable->verticalHeader()->setVisible(false);
    permissionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    permissionsTable->setAlternatingRowColors(true);
    permissionsTable->setColumnWidth(0, 350);

    mainLayout->addWidget(permissionsTable);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton *closeButton = new QPushButton("Close");
    closeButton->setMinimumWidth(120);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);
}

void PermissionsViewDialog::loadPermissions() {
    QVector<Permission> permissions = RoleManager::getPermissionsForRole(userRole);

    permissionsTable->setRowCount(permissions.size());

    for (int i = 0; i < permissions.size(); ++i) {
        QString permName = RoleManager::getPermissionDescription(permissions[i]);
        permissionsTable->setItem(i, 0, new QTableWidgetItem(permName));

        QTableWidgetItem *accessItem = new QTableWidgetItem("✓ Granted");
        accessItem->setForeground(QBrush(QColor(getColorScheme().success)));
        QFont font = accessItem->font();
        font.setBold(true);
        accessItem->setFont(font);
        accessItem->setTextAlignment(Qt::AlignCenter);
        permissionsTable->setItem(i, 1, accessItem);
    }
}
