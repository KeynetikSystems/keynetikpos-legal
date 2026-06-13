// =============================================================================
// user.cpp — Implementation of RoleManager (see user.h for the full
// WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - getPermissionsForRole() is the AUTHORITATIVE role -> permission table;
//    hasPermission() and all UI gating derive from it, so changing what a
//    role may do is a one-place edit.
//  - roleToString()/stringToRole() define the strings stored in the users
//    table — keep them stable or migrate existing rows.
// =============================================================================
#include "User.h"

QString RoleManager::roleToString(UserRole role) {
    switch (role) {
    case UserRole::ADMIN:    return "Admin";
    case UserRole::MANAGER:  return "Manager";
    case UserRole::CASHIER:  return "Cashier";
    case UserRole::VIEWER:   return "Viewer";
    default:                 return "Unknown";
    }
}

UserRole RoleManager::stringToRole(const QString &roleStr) {
    if (roleStr == "Admin") return UserRole::ADMIN;
    if (roleStr == "Manager") return UserRole::MANAGER;
    if (roleStr == "Cashier") return UserRole::CASHIER;
    if (roleStr == "Viewer") return UserRole::VIEWER;
    return UserRole::CASHIER; // Default
}

QStringList RoleManager::getAllRoles() {
    return QStringList() << "Admin" << "Manager" << "Cashier" << "Viewer";
}

bool RoleManager::hasPermission(UserRole role, Permission permission) {
    // Admin has all permissions
    if (role == UserRole::ADMIN) {
        return true;
    }

    // Manager permissions
    if (role == UserRole::MANAGER) {
        switch (permission) {
        // Sales
        case Permission::MAKE_SALES:
        case Permission::VIEW_SALES:
        case Permission::APPLY_DISCOUNTS:
        case Permission::VOID_TRANSACTIONS:
        // Inventory
        case Permission::VIEW_INVENTORY:
        case Permission::ADD_PRODUCTS:
        case Permission::EDIT_PRODUCTS:
        case Permission::ADJUST_STOCK:
        // Reports
        case Permission::VIEW_REPORTS:
        case Permission::VIEW_ANALYTICS:
        case Permission::EXPORT_REPORTS:
        // Users
        case Permission::VIEW_USERS:
            return true;
        default:
            return false;
        }
    }

    // Cashier permissions
    if (role == UserRole::CASHIER) {
        switch (permission) {
        case Permission::MAKE_SALES:
        case Permission::VIEW_SALES:
        case Permission::APPLY_DISCOUNTS:
        case Permission::VIEW_INVENTORY:
            return true;
        default:
            return false;
        }
    }

    // Viewer permissions (read-only)
    if (role == UserRole::VIEWER) {
        switch (permission) {
        case Permission::VIEW_SALES:
        case Permission::VIEW_INVENTORY:
        case Permission::VIEW_REPORTS:
        case Permission::VIEW_ANALYTICS:
            return true;
        default:
            return false;
        }
    }

    return false;
}

QString RoleManager::getPermissionDescription(Permission permission) {
    switch (permission) {
    case Permission::MAKE_SALES:         return "Make Sales";
    case Permission::VIEW_SALES:         return "View Sales";
    case Permission::DELETE_SALES:       return "Delete Sales";
    case Permission::APPLY_DISCOUNTS:    return "Apply Discounts";
    case Permission::VOID_TRANSACTIONS:  return "Void Transactions";
    case Permission::VIEW_INVENTORY:     return "View Inventory";
    case Permission::ADD_PRODUCTS:       return "Add Products";
    case Permission::EDIT_PRODUCTS:      return "Edit Products";
    case Permission::DELETE_PRODUCTS:    return "Delete Products";
    case Permission::ADJUST_STOCK:       return "Adjust Stock";
    case Permission::VIEW_REPORTS:       return "View Reports";
    case Permission::VIEW_ANALYTICS:     return "View Analytics";
    case Permission::EXPORT_REPORTS:     return "Export Reports";
    case Permission::MANAGE_USERS:       return "Manage Users";
    case Permission::VIEW_USERS:         return "View Users";
    case Permission::ACCESS_SETTINGS:    return "Access Settings";
    case Permission::BACKUP_RESTORE:     return "Backup & Restore";
    case Permission::VIEW_LOGS:          return "View Logs";
    default:                             return "Unknown Permission";
    }
}

QVector<Permission> RoleManager::getPermissionsForRole(UserRole role) {
    QVector<Permission> permissions;

    // Check all possible permissions
    QVector<Permission> allPerms = {
        Permission::MAKE_SALES, Permission::VIEW_SALES, Permission::DELETE_SALES,
        Permission::APPLY_DISCOUNTS, Permission::VOID_TRANSACTIONS,
        Permission::VIEW_INVENTORY, Permission::ADD_PRODUCTS, Permission::EDIT_PRODUCTS,
        Permission::DELETE_PRODUCTS, Permission::ADJUST_STOCK,
        Permission::VIEW_REPORTS, Permission::VIEW_ANALYTICS, Permission::EXPORT_REPORTS,
        Permission::MANAGE_USERS, Permission::VIEW_USERS,
        Permission::ACCESS_SETTINGS, Permission::BACKUP_RESTORE, Permission::VIEW_LOGS
    };

    for (Permission perm : allPerms) {
        if (hasPermission(role, perm)) {
            permissions.append(perm);
        }
    }

    return permissions;
}
