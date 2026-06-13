// =============================================================================
// main.cpp — Application entry point and startup sequence
// -----------------------------------------------------------------------------
// WHAT: Orchestrates startup: anti-debug check -> license validation ->
//       database initialization -> login (max 3 attempts) -> forced password
//       change if required -> show MainWindow -> deferred online license
//       heartbeat -> logout on exit.
// HOW:  Runs each gate sequentially, returning non-zero if any fails.
//       forcePasswordChange() loops a QInputDialog until the user supplies a
//       valid new password (>= 8 chars, different from old, confirmed twice)
//       or cancels. The online license revalidation is deferred with
//       QTimer::singleShot(2000, ...) so it runs AFTER the event loop is live.
// WHY:  Each gate protects the next: no point opening the database for an
//       unlicensed copy, or showing the POS to an unauthenticated user. The
//       heartbeat is deferred because a blocking network call at startup would
//       freeze the UI on slow/offline networks — a real risk for shops with
//       poor connectivity. Login attempts are capped to slow down password
//       guessing on a shared terminal.
// =============================================================================
#include "mainwindow.h"
#include "database.h"
#include "logindialog.h"
#include "usermanager.h"
#include "changepassworddialog.h"
#include "colorscheme.h"
#include "appstyle.h"
#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QSysInfo>
#include "licensemanager.h"
#include "antidebug.h"

// Forces the logged-in user to set a new password (first login with a
// default/admin-issued password). Returns false if the user refuses.
static bool forcePasswordChange(const QString &currentPassword)
{
    ChangePasswordDialog dlg(ChangePasswordDialog::Mode::Forced);
    dlg.setKnownCurrentPassword(currentPassword);

    while (dlg.exec() == QDialog::Accepted) {
        if (UserManager::instance().changeOwnPassword(currentPassword,
                                                      dlg.newPassword()))
            return true;
        dlg.showError("Failed to update the password. Please try again.");
    }
    return false;
}

// Runs the login dialog (max 3 attempts per session) including the forced
// password change. Returns true once a user is logged in; on false the app
// should exit with *exitCode.
static bool runLoginFlow(int *exitCode)
{
    int loginAttempts = 0;
    const int maxAttempts = 3;

    while (loginAttempts < maxAttempts) {
        LoginDialog loginDialog;

        if (loginDialog.exec() != QDialog::Accepted) {
            *exitCode = 0;   // user cancelled
            return false;
        }

        if (UserManager::instance().login(loginDialog.getUsername(),
                                          loginDialog.getPassword())) {
            // First login with a default or admin-issued password:
            // require a new one before entering the app
            if (UserManager::instance().getCurrentUser().mustChangePassword) {
                if (!forcePasswordChange(loginDialog.getPassword())) {
                    UserManager::instance().logout();
                    *exitCode = 0;
                    return false;
                }
            }
            return true;
        }

        loginAttempts++;
        const int remainingAttempts = maxAttempts - loginAttempts;
        if (remainingAttempts > 0) {
            QMessageBox::warning(nullptr, "Login Failed",
                                 QString("Invalid username or password.\n"
                                         "Remaining attempts: %1")
                                     .arg(remainingAttempts));
        } else {
            QMessageBox::critical(nullptr, "Login Failed",
                                  "Maximum login attempts exceeded.\n"
                                  "Application will now close.");
            *exitCode = 1;
            return false;
        }
    }

    *exitCode = 1;
    return false;
}

int main(int argc, char *argv[])
{
    // Crisp fractional scaling on mixed-DPI setups (e.g. a laptop driving an
    // external cashier display). Must be set before the QApplication exists.
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a(argc, argv);

    // Theme everything from the start so the license prompts and the login
    // dialog match the persisted light/dark choice. MainWindow::applyTheme()
    // re-applies the same sheet on toggle.
    const bool dark = ThemeManager::instance().isDark();
    a.setStyleSheet(appStylesheet(dark));
    QApplication::setPalette(appPalette(dark));

    // ── Anti-tamper check ────────────────────────────────────────
    if (AntiDebug::isThreatDetected()) {
        // Don't tell them WHY — just exit silently or show generic error
        QMessageBox::critical(nullptr, "Error",
                              "Application failed to start. Error code: 0x00000005");
        return 1;
    }


    // ── 1. Check license BEFORE showing anything ────────────────
    LicenseManager::instance().initialize();

    auto state = LicenseManager::instance().state();

    if (LicenseManager::instance().wasTampered()) {
        QMessageBox::critical(nullptr, "License Error",
                              "License data has been modified and is no longer valid.\n"
                              "Please reinstall or contact support@keynetik.com.");
        return 1;
    }

    if (state == LicenseState::TrialExpired || state == LicenseState::InvalidKey) {
        // Give the user a way to activate instead of dead-ending
        bool ok = false;
        const QString key = QInputDialog::getText(
            nullptr, "License Required",
            (state == LicenseState::TrialExpired
                 ? "Your 30-day trial has expired."
                 : "The stored license is invalid or has been revoked.")
                + QString("\n\nEnter your CD key to activate "
                          "(or Cancel to exit):"),
            QLineEdit::Normal, "", &ok);

        if (!ok) {
            QMessageBox::critical(nullptr, "License Required",
                                  "No valid license key was provided.\n"
                                  "Please contact support@keynetik.com to purchase a key.");
            return 1;
        }

        // Online-first activation: the server enforces device limits and
        // revocation. If no server is reachable (offline shop), fall back to
        // local format validation so a paying customer is never locked out by
        // a missing internet connection.
        auto &lic = LicenseManager::instance();
        bool activated = false;
        QString failReason = "The key is not valid.";

        switch (lic.activateOnServer(key,
                                     QString::fromLatin1(QSysInfo::machineUniqueId()),
                                     QSysInfo::machineHostName())) {
        case LicenseManager::ActivationResult::Success:
            activated = true;
            break;
        case LicenseManager::ActivationResult::ServerUnreachable:
            activated = lic.activateKey(key);   // offline fallback
            break;
        case LicenseManager::ActivationResult::DeviceLimitReached:
            failReason = "This key is already activated on its maximum number of devices.";
            break;
        case LicenseManager::ActivationResult::KeyExpired:
            failReason = "This key has expired.";
            break;
        case LicenseManager::ActivationResult::ServerRejected:
            failReason = "This key has been revoked.";
            break;
        case LicenseManager::ActivationResult::InvalidKey:
            break;
        }

        if (!activated) {
            QMessageBox::critical(nullptr, "License Required",
                                  failReason +
                                  "\nPlease contact support@keynetik.com to purchase a key.");
            return 1;
        }
    }

    if (state == LicenseState::Trial) {
        int days = LicenseManager::instance().trialDaysLeft();
        // Only nag on first run, and when ≤7 days remain
        if (days == LicenseManager::TRIAL_DAYS || days <= 7) {
            QMessageBox::information(nullptr, "Trial Mode",
                                     QString("You are running in trial mode.\n%1 day(s) remaining.\n"
                                             "Purchase a CD Key to unlock the full version.").arg(days));
        }
    }

    // Initialize database
    Database &db = Database::instance();
    if (!db.initialize()) {
        QMessageBox::critical(nullptr, "Database Error",
                              QString("Failed to initialize database: %1").arg(db.getLastError()));
        return 1;
    }

    // ── Login → MainWindow loop ──────────────────────────────────────────
    // Logout emits MainWindow::logoutRequested and closes the window; the
    // loop then shows the login dialog again — no process restart needed.
    int  result = 0;
    bool heartbeatStarted = false;
    bool loopToLogin = true;

    while (loopToLogin) {
        int exitCode = 0;
        if (!runLoginFlow(&exitCode))
            return exitCode;

        loopToLogin = false;

        MainWindow w;
        QObject::connect(&w, &MainWindow::logoutRequested,
                         [&loopToLogin]() { loopToLogin = true; });
        w.show();

        // Online license revalidation — async, once, after the UI is live
        if (!heartbeatStarted) {
            heartbeatStarted = true;
            QTimer::singleShot(2000, []() {
                LicenseManager::instance().startOnlineHeartbeat();
            });
        }

        result = a.exec();
    }

    // Logout on application exit
    UserManager::instance().logout();

    return result;
}
