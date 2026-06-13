// =============================================================================
// user.h — RBAC model: UserRole, Permission, User, RoleManager
// -----------------------------------------------------------------------------
// WHAT: The role-based access control model. Four hierarchical roles (ADMIN >
//       MANAGER > CASHIER > VIEWER), granular Permission values for sales/
//       inventory/reports/users/system, the User record struct (including
//       mustChangePassword), and RoleManager — the static helper that maps
//       roles <-> strings and answers hasPermission(role, permission).
// HOW:  Plain enums plus a static class. getPermissionsForRole() holds the
//       authoritative role->permission table in one place; hasPermission() and
//       all UI gating derive from it. Role strings are used for DB storage.
// WHY:  Separating roles from permissions means features check "can this user
//       APPLY_DISCOUNTS?" rather than "is this a manager?", so role definitions
//       can change without touching feature code. Centralizing the mapping in
//       RoleManager prevents permission logic drifting across dialogs.
// =============================================================================
#ifndef USER_H
#define USER_H

#include <QString>
#include <QDateTime>

// Define user roles with hierarchical permissions
enum class UserRole {
    ADMIN,          // Full access to all features
    MANAGER,        // Access to reports, inventory, some settings
    CASHIER,        // Basic POS operations, limited access
    VIEWER          // Read-only access to reports
};

// Define specific permissions
enum class Permission {
    // Sales permissions
    MAKE_SALES,
    VIEW_SALES,
    DELETE_SALES,
    APPLY_DISCOUNTS,
    VOID_TRANSACTIONS,

    // Inventory permissions
    VIEW_INVENTORY,
    ADD_PRODUCTS,
    EDIT_PRODUCTS,
    DELETE_PRODUCTS,
    ADJUST_STOCK,

    // Reports permissions
    VIEW_REPORTS,
    VIEW_ANALYTICS,
    EXPORT_REPORTS,

    // User management permissions
    MANAGE_USERS,
    VIEW_USERS,

    // System permissions
    ACCESS_SETTINGS,
    BACKUP_RESTORE,
    VIEW_LOGS
};

struct User {
    int id;
    QString username;
    QString passwordHash;   // Store hashed password
    QString fullName;
    QString email;
    UserRole role;
    bool isActive;
    QDateTime createdDate;
    QDateTime lastLogin;
    QString createdBy;      // Username of who created this user
    bool mustChangePassword = false;   // Forces a password change at next login

    // Default constructor
    User() : id(-1), role(UserRole::CASHIER), isActive(true) {}
};

// Helper class for role management
class RoleManager {
public:
    static QString roleToString(UserRole role);
    static UserRole stringToRole(const QString &roleStr);
    static QStringList getAllRoles();
    static bool hasPermission(UserRole role, Permission permission);
    static QString getPermissionDescription(Permission permission);
    static QVector<Permission> getPermissionsForRole(UserRole role);
};

#endif // USER_H
