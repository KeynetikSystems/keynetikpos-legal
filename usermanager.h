// =============================================================================
// usermanager.h — UserManager: authentication, session, and user lifecycle
// -----------------------------------------------------------------------------
// WHAT: Singleton owning login/logout, the in-memory current-user session,
//       permission checks, user CRUD, password changes, username/email
//       validation, password-strength feedback, and an audit log of actions.
// HOW:  login() looks up the active user, verifies via PasswordHasher, and —
//       if the stored hash is legacy unsalted SHA-256 — transparently rehashes
//       it to salted PBKDF2 using the just-verified plaintext, then updates
//       last_login and writes an audit row. All queries are prepared
//       statements. Session state is just currentUser + loggedIn.
// WHY:  A singleton session matches a POS terminal: one operator at a time.
//       Rehash-on-login is the only moment the plaintext is available, so it's
//       the standard way to migrate legacy hashes without forcing a mass
//       password reset. Audit logging exists because cash-handling
//       environments need accountability for who did what.
// =============================================================================
#ifndef USERMANAGER_H
#define USERMANAGER_H

#include "User.h"
#include <QString>
#include <QVector>
#include <QCryptographicHash>

class UserManager
{
public:
    static UserManager& instance();

    // Authentication
    bool login(const QString &username, const QString &password);
    void logout();
    bool isLoggedIn() const;
    User getCurrentUser() const;
    QString getCurrentUsername() const;
    UserRole getCurrentUserRole() const;

    // Permission checking
    bool hasPermission(Permission permission) const;
    bool canAccessFeature(const QString &featureName) const;

    // User CRUD operations
    bool createUser(const User &user, const QString &password);
    bool updateUser(const User &user);
    bool deleteUser(int userId);
    bool changePassword(int userId, const QString &newPassword);
    bool changeOwnPassword(const QString &oldPassword, const QString &newPassword);
    User getUserById(int id);
    User getUserByUsername(const QString &username);
    QVector<User> getAllUsers();
    QVector<User> getActiveUsers();
    bool toggleUserStatus(int userId);

    // Validation
    bool validateUsername(const QString &username, int excludeUserId = -1);
    bool validateEmail(const QString &email, int excludeUserId = -1);
    QString getPasswordStrength(const QString &password);

    // Audit logging
    void logUserAction(const QString &action, const QString &details = QString());
    QVector<QPair<QString, QString>> getUserActivityLog(int userId, int limit = 50);

    // Password management
    static QString hashPassword(const QString &password);
    static bool verifyPassword(const QString &password, const QString &hash);

    // Session management
    void updateLastLogin(int userId);
    QDateTime getLastLogin(int userId);

private:
    UserManager();
    ~UserManager();
    UserManager(const UserManager&) = delete;
    UserManager& operator=(const UserManager&) = delete;

    User currentUser;
    bool loggedIn;
};

#endif // USERMANAGER_H
