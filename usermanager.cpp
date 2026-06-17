// =============================================================================
// usermanager.cpp — Implementation of UserManager (see usermanager.h for the
// full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - login() only matches ACTIVE users, verifies via PasswordHasher, and
//    transparently rehashes legacy unsalted SHA-256 hashes to salted PBKDF2 —
//    login is the only moment the plaintext is available for migration.
//  - Every authentication event and user mutation goes through
//    logUserAction() so cash-handling environments have an audit trail.
//  - All queries are prepared statements with bound values (no SQL injection).
// =============================================================================
#include "UserManager.h"
#include "database.h"
#include "passwordhasher.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QRegularExpression>

UserManager::UserManager() : loggedIn(false) {
}

UserManager::~UserManager() {
}

UserManager& UserManager::instance() {
    static UserManager instance;
    return instance;
}

QString UserManager::hashPassword(const QString &password) {
    // Salted PBKDF2-SHA256 (see passwordhasher.h)
    return PasswordHasher::hash(password);
}

bool UserManager::verifyPassword(const QString &password, const QString &hash) {
    // Also accepts legacy unsalted SHA-256 hashes; login() rehashes those
    return PasswordHasher::verify(password, hash);
}

bool UserManager::login(const QString &username, const QString &password) {
    QSqlQuery query;
    query.prepare("SELECT id, username, password_hash, full_name, email, role, "
                  "is_active, created_date, last_login, created_by, must_change_password "
                  "FROM users WHERE username = ? AND is_active = 1");
    query.addBindValue(username);

    if (!query.exec()) {
        qDebug() << "Login query failed:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        QString storedHash = query.value(2).toString();

        if (verifyPassword(password, storedHash)) {
            currentUser.id = query.value(0).toInt();
            currentUser.username = query.value(1).toString();
            currentUser.passwordHash = storedHash;
            currentUser.fullName = query.value(3).toString();
            currentUser.email = query.value(4).toString();
            currentUser.role = RoleManager::stringToRole(query.value(5).toString());
            currentUser.isActive = query.value(6).toBool();
            currentUser.createdDate = query.value(7).toDateTime();
            currentUser.lastLogin = query.value(8).toDateTime();
            currentUser.createdBy = query.value(9).toString();
            currentUser.mustChangePassword = query.value(10).toBool();

            // Transparently migrate legacy SHA-256 hashes to salted PBKDF2
            if (PasswordHasher::needsRehash(storedHash)) {
                const QString newHash = hashPassword(password);
                QSqlQuery rehash;
                rehash.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
                rehash.addBindValue(newHash);
                rehash.addBindValue(currentUser.id);
                if (rehash.exec()) {
                    currentUser.passwordHash = newHash;
                }
            }

            loggedIn = true;
            updateLastLogin(currentUser.id);
            logUserAction("Login", "User logged in successfully");

            return true;
        }
    }

    return false;
}

void UserManager::logout() {
    if (loggedIn) {
        logUserAction("Logout", "User logged out");
        currentUser = User();
        loggedIn = false;
    }
}

bool UserManager::isLoggedIn() const {
    return loggedIn;
}

User UserManager::getCurrentUser() const {
    return currentUser;
}

QString UserManager::getCurrentUsername() const {
    return currentUser.username;
}

UserRole UserManager::getCurrentUserRole() const {
    return currentUser.role;
}

bool UserManager::hasPermission(Permission permission) const {
    if (!loggedIn) {
        return false;
    }
    return RoleManager::hasPermission(currentUser.role, permission);
}

bool UserManager::canAccessFeature(const QString &featureName) const {
    if (!loggedIn) {
        return false;
    }

    // Map feature names to permissions
    if (featureName == "inventory") {
        return hasPermission(Permission::VIEW_INVENTORY);
    } else if (featureName == "reports") {
        return hasPermission(Permission::VIEW_REPORTS);
    } else if (featureName == "analytics") {
        return hasPermission(Permission::VIEW_ANALYTICS);
    } else if (featureName == "users") {
        return hasPermission(Permission::VIEW_USERS);
    } else if (featureName == "settings") {
        return hasPermission(Permission::ACCESS_SETTINGS);
    }

    return false;
}

bool UserManager::createUser(const User &user, const QString &password) {
    if (!hasPermission(Permission::MANAGE_USERS)) {
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO users (username, password_hash, full_name, email, "
                  "role, is_active, created_date, created_by) "
                  "VALUES (?, ?, ?, ?, ?, ?, datetime('now'), ?)");

    query.addBindValue(user.username);
    query.addBindValue(hashPassword(password));
    query.addBindValue(user.fullName);
    query.addBindValue(user.email);
    query.addBindValue(RoleManager::roleToString(user.role));
    query.addBindValue(user.isActive);
    query.addBindValue(currentUser.username);

    if (query.exec()) {
        logUserAction("Create User", "Created user: " + user.username);
        return true;
    }

    qDebug() << "Create user failed:" << query.lastError().text();
    return false;
}

bool UserManager::updateUser(const User &user) {
    if (!hasPermission(Permission::MANAGE_USERS)) {
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE users SET username = ?, full_name = ?, email = ?, "
                  "role = ?, is_active = ? WHERE id = ?");

    query.addBindValue(user.username);
    query.addBindValue(user.fullName);
    query.addBindValue(user.email);
    query.addBindValue(RoleManager::roleToString(user.role));
    query.addBindValue(user.isActive);
    query.addBindValue(user.id);

    if (query.exec()) {
        logUserAction("Update User", "Updated user: " + user.username);
        return true;
    }

    qDebug() << "Update user failed:" << query.lastError().text();
    return false;
}

bool UserManager::deleteUser(int userId) {
    if (!hasPermission(Permission::MANAGE_USERS)) {
        return false;
    }

    // Don't allow deleting yourself
    if (userId == currentUser.id) {
        return false;
    }

    // user_activity_log has a FK to users; with foreign keys enforced the
    // log rows must go in the same transaction as the user row
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery logQuery;
    logQuery.prepare("DELETE FROM user_activity_log WHERE user_id = ?");
    logQuery.addBindValue(userId);
    if (!logQuery.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM users WHERE id = ?");
    query.addBindValue(userId);

    if (query.exec() && db.commit()) {
        logUserAction("Delete User", "Deleted user ID: " + QString::number(userId));
        return true;
    }

    db.rollback();
    return false;
}

bool UserManager::changePassword(int userId, const QString &newPassword) {
    if (!hasPermission(Permission::MANAGE_USERS)) {
        return false;
    }

    // Admin-set passwords are temporary: the user must pick their own at next login
    QSqlQuery query;
    query.prepare("UPDATE users SET password_hash = ?, must_change_password = 1 WHERE id = ?");
    query.addBindValue(hashPassword(newPassword));
    query.addBindValue(userId);

    if (query.exec()) {
        logUserAction("Change Password", "Password changed for user ID: " + QString::number(userId));
        return true;
    }

    return false;
}

bool UserManager::adminResetPassword(const QString &username, const QString &tempPassword) {
    // No session required — this is called from the login screen before any
    // user is logged in. Caller must have verified the license key first.
    QSqlQuery q;
    q.prepare("UPDATE users SET password_hash = :h, must_change_password = 1 "
              "WHERE username = :u AND is_active = 1");
    q.bindValue(":h", hashPassword(tempPassword));
    q.bindValue(":u", username);
    return q.exec() && q.numRowsAffected() > 0;
}

bool UserManager::changeOwnPassword(const QString &oldPassword, const QString &newPassword) {
    if (!loggedIn) {
        return false;
    }

    if (!verifyPassword(oldPassword, currentUser.passwordHash)) {
        return false;
    }

    const QString newHash = hashPassword(newPassword);
    QSqlQuery query;
    query.prepare("UPDATE users SET password_hash = ?, must_change_password = 0 WHERE id = ?");
    query.addBindValue(newHash);
    query.addBindValue(currentUser.id);

    if (query.exec()) {
        currentUser.passwordHash = newHash;
        currentUser.mustChangePassword = false;
        logUserAction("Change Own Password", "Password changed successfully");
        return true;
    }

    return false;
}

User UserManager::getUserById(int id) {
    User user;
    QSqlQuery query;
    query.prepare("SELECT id, username, password_hash, full_name, email, role, "
                  "is_active, created_date, last_login, created_by "
                  "FROM users WHERE id = ?");
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.passwordHash = query.value(2).toString();
        user.fullName = query.value(3).toString();
        user.email = query.value(4).toString();
        user.role = RoleManager::stringToRole(query.value(5).toString());
        user.isActive = query.value(6).toBool();
        user.createdDate = query.value(7).toDateTime();
        user.lastLogin = query.value(8).toDateTime();
        user.createdBy = query.value(9).toString();
    }

    return user;
}

User UserManager::getUserByUsername(const QString &username) {
    User user;
    QSqlQuery query;
    query.prepare("SELECT id, username, password_hash, full_name, email, role, "
                  "is_active, created_date, last_login, created_by "
                  "FROM users WHERE username = ?");
    query.addBindValue(username);

    if (query.exec() && query.next()) {
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.passwordHash = query.value(2).toString();
        user.fullName = query.value(3).toString();
        user.email = query.value(4).toString();
        user.role = RoleManager::stringToRole(query.value(5).toString());
        user.isActive = query.value(6).toBool();
        user.createdDate = query.value(7).toDateTime();
        user.lastLogin = query.value(8).toDateTime();
        user.createdBy = query.value(9).toString();
    }

    return user;
}

QVector<User> UserManager::getAllUsers() {
    QVector<User> users;
    QSqlQuery query("SELECT id, username, password_hash, full_name, email, role, "
                    "is_active, created_date, last_login, created_by FROM users");

    while (query.next()) {
        User user;
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.passwordHash = query.value(2).toString();
        user.fullName = query.value(3).toString();
        user.email = query.value(4).toString();
        user.role = RoleManager::stringToRole(query.value(5).toString());
        user.isActive = query.value(6).toBool();
        user.createdDate = query.value(7).toDateTime();
        user.lastLogin = query.value(8).toDateTime();
        user.createdBy = query.value(9).toString();

        users.append(user);
    }

    return users;
}

QVector<User> UserManager::getActiveUsers() {
    QVector<User> users;
    QSqlQuery query("SELECT id, username, password_hash, full_name, email, role, "
                    "is_active, created_date, last_login, created_by FROM users WHERE is_active = 1");

    while (query.next()) {
        User user;
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.passwordHash = query.value(2).toString();
        user.fullName = query.value(3).toString();
        user.email = query.value(4).toString();
        user.role = RoleManager::stringToRole(query.value(5).toString());
        user.isActive = query.value(6).toBool();
        user.createdDate = query.value(7).toDateTime();
        user.lastLogin = query.value(8).toDateTime();
        user.createdBy = query.value(9).toString();

        users.append(user);
    }

    return users;
}

bool UserManager::toggleUserStatus(int userId) {
    if (!hasPermission(Permission::MANAGE_USERS)) {
        return false;
    }

    // Don't allow disabling yourself
    if (userId == currentUser.id) {
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE users SET is_active = NOT is_active WHERE id = ?");
    query.addBindValue(userId);

    if (query.exec()) {
        logUserAction("Toggle User Status", "Toggled status for user ID: " + QString::number(userId));
        return true;
    }

    return false;
}

bool UserManager::validateUsername(const QString &username, int excludeUserId) {
    if (username.length() < 3) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE username = ? AND id != ?");
    query.addBindValue(username);
    query.addBindValue(excludeUserId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() == 0;
    }

    return false;
}

bool UserManager::validateEmail(const QString &email, int excludeUserId) {
    // Basic email validation
    QRegularExpression emailRegex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    if (!emailRegex.match(email).hasMatch()) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE email = ? AND id != ?");
    query.addBindValue(email);
    query.addBindValue(excludeUserId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() == 0;
    }

    return false;
}

QString UserManager::getPasswordStrength(const QString &password) {
    int strength = 0;

    if (password.length() >= 8) strength++;
    if (password.length() >= 12) strength++;
    if (password.contains(QRegularExpression("[a-z]"))) strength++;
    if (password.contains(QRegularExpression("[A-Z]"))) strength++;
    if (password.contains(QRegularExpression("[0-9]"))) strength++;
    if (password.contains(QRegularExpression("[^a-zA-Z0-9]"))) strength++;

    if (strength <= 2) return "Weak";
    if (strength <= 4) return "Medium";
    return "Strong";
}

void UserManager::logUserAction(const QString &action, const QString &details) {
    if (!loggedIn) {
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO user_activity_log (user_id, username, action, details, timestamp) "
                  "VALUES (?, ?, ?, ?, datetime('now'))");
    query.addBindValue(currentUser.id);
    query.addBindValue(currentUser.username);
    query.addBindValue(action);
    query.addBindValue(details);
    query.exec();
}

QVector<QPair<QString, QString>> UserManager::getUserActivityLog(int userId, int limit) {
    QVector<QPair<QString, QString>> log;

    QSqlQuery query;
    query.prepare("SELECT action || ' - ' || details, timestamp FROM user_activity_log "
                  "WHERE user_id = ? ORDER BY timestamp DESC LIMIT ?");
    query.addBindValue(userId);
    query.addBindValue(limit);

    if (query.exec()) {
        while (query.next()) {
            log.append(qMakePair(query.value(0).toString(), query.value(1).toString()));
        }
    }

    return log;
}

void UserManager::updateLastLogin(int userId) {
    QSqlQuery query;
    query.prepare("UPDATE users SET last_login = datetime('now') WHERE id = ?");
    query.addBindValue(userId);
    query.exec();
}

QDateTime UserManager::getLastLogin(int userId) {
    QSqlQuery query;
    query.prepare("SELECT last_login FROM users WHERE id = ?");
    query.addBindValue(userId);

    if (query.exec() && query.next()) {
        return query.value(0).toDateTime();
    }

    return QDateTime();
}
