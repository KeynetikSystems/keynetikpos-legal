// =============================================================================
// licensemanager.h — LicenseManager: trial / CD-key licensing with tamper checks
// -----------------------------------------------------------------------------
// WHAT: Singleton enforcing the licensing model: 30-day trial, CD-key
//       activation (local or server-backed), a 7-day offline grace period for
//       activated copies, revocation, and tamper detection.
// HOW:  All license state (key, install date, activation source, revoked flag,
//       last online check, offline-day counter) lives in HKCU via QSettings.
//       A salted SHA-256 hash over those values detects hand-edited registry
//       entries. A second, machine-bound signed copy of the install date (the
//       "breadcrumb") is written to a file under the user's config dir;
//       initialize() takes the EARLIEST of the two dates so wiping HKCU alone
//       cannot restart the trial. Validation is two-phase: initialize() is
//       strictly offline and never blocks; startOnlineHeartbeat() revalidates
//       against the activation server asynchronously after the UI is up.
// WHY:  A POS terminal must boot with no internet, so offline-first validation
//       with an online heartbeat is the only practical scheme. The redundant
//       breadcrumb + registry hash is defence-in-depth: each alone is easy to
//       defeat, but together an attacker must forge both locations with the
//       correct machine-bound signature.
// =============================================================================
#pragma once
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QWidget>

enum class LicenseState {
    FullLicense,
    Trial,
    TrialExpired,
    InvalidKey
};

// ── Tier definitions ──────────────────────────────────────────────────────────
// Tier 1  POS Core      : checkout, products, basic inventory
// Tier 2  POS Pro       : user management, reports, schedules, barcode, WhatsApp
// Tier 3  ERP Lite      : suppliers, purchasing, expenses, customers, P&L
// Tier 4  ERP Full      : multi-branch, payroll, VAT, approval workflows
//
// During trial the effective tier is 4 (evaluate everything).
// A FullLicense without server-supplied tier data defaults to 1 (safe minimum).

// ── Per-feature string constants ──────────────────────────────────────────────
// The server returns a JSON array of enabled feature slugs; these constants are
// the canonical slug values used on both the server and client.
namespace Feature {
    // Tier 2
    inline constexpr const char *USER_MANAGEMENT  = "user_management";
    inline constexpr const char *ADVANCED_REPORTS = "advanced_reports";
    inline constexpr const char *SCHEDULES        = "schedules";
    inline constexpr const char *BARCODE          = "barcode";
    inline constexpr const char *MESSAGING        = "messaging";
    // Tier 3
    inline constexpr const char *PURCHASING       = "purchasing";
    inline constexpr const char *EXPENSES         = "expenses";
    inline constexpr const char *CUSTOMERS        = "customers";
    inline constexpr const char *PL_REPORT        = "pl_report";
    inline constexpr const char *STOCK_VALUATION  = "stock_valuation";
    // Tier 4 (reserved — not yet implemented)
    inline constexpr const char *MULTI_BRANCH     = "multi_branch";
    inline constexpr const char *PAYROLL          = "payroll";
    inline constexpr const char *VAT_MODULE       = "vat_module";
}

class LicenseManager {
public:
    enum class ActivationResult {
        Success,
        InvalidKey,
        ServerRejected,
        DeviceLimitReached,
        KeyExpired,
        ServerUnreachable
    };

    static LicenseManager& instance();
    static constexpr int TRIAL_DAYS    = 30;
    static constexpr int MAX_OFFLINE_DAYS = 7;

    void             initialize();          // offline-only; never blocks on the network
    void             startOnlineHeartbeat(); // async revalidation, call after the UI is up
    ActivationResult activateOnServer(const QString& key,
                                      const QString& deviceId,
                                      const QString& deviceName);
    bool             activateKey(const QString& key);   // local only

    LicenseState state()                const;
    int          trialDaysLeft()        const;
    bool         isActivated()          const;
    bool         serverReachable()      const;
    bool         offlineGraceExpired()  const;
    int          offlineDaysRemaining() const;
    bool         wasTampered()          const;
    QString      maskedKey()            const;
    // Checks that the supplied key passes format + checksum validation without
    // changing any stored state. Used by the password-recovery dialog to verify
    // that the caller possesses the license key before allowing a reset.
    bool         verifyKeyFormat(const QString &key) const;

    // Tier / feature access
    int          tier()                              const;
    bool         hasTier(int minTier)                const;
    bool         hasFeature(const QString &feature)  const;
    QStringList  features()                          const;

private:
    LicenseManager() = default;

    // ── Validation ───────────────────────────────────────────────
    bool validateKey(const QString& key)                        const;

    // ── Registry read/write ──────────────────────────────────────
    QString   readStoredKey()                                   const;
    QDateTime readInstallDate()                                 const;
    void      writeInstallDate(const QDateTime& dt)             const;
    QString   readActivationSource()                            const;
    void      writeActivationSource(const QString& source)      const;
    bool      readRevoked()                                     const;
    void      writeRevoked(bool revoked)                        const;
    QDateTime readLastOnlineCheck()                             const;
    void      writeLastOnlineCheck()                            const;
    int       readOfflineDays()                                 const;
    void      writeOfflineDays(int days)                        const;

    // ── Out-of-registry breadcrumb (survives an HKCU wipe) ───────
    QString   breadcrumbPath()                                  const;
    QDateTime readBreadcrumbInstallDate()                       const;
    void      writeBreadcrumbInstallDate(const QDateTime& dt)   const;

    // ── Tamper detection ─────────────────────────────────────────
    QString   computeRegistryHash(const QString& key,
                                const QString& installDate,
                                const QString& source,
                                bool revoked)                const;
    void      writeRegistryHash()                               const;
    bool      verifyRegistryHash()                              const;

    // ── Registry helpers for tier/features ───────────────────────
    int         readStoredTier()     const;
    void        writeStoredTier(int tier) const;
    QStringList readStoredFeatures() const;
    void        writeStoredFeatures(const QStringList &features) const;

    // Apply a server-supplied tier + feature list (shared by activate + heartbeat)
    void        applyServerTier(int tier, const QStringList &features);

    // ── State ────────────────────────────────────────────────────
    LicenseState m_state                = LicenseState::Trial;
    int          m_daysLeft             = TRIAL_DAYS;
    bool         m_serverReachable      = false;
    bool         m_offlineGraceExpired  = false;
    int          m_offlineDaysRemaining = MAX_OFFLINE_DAYS;
    bool         m_tampered             = false;
    QString      m_lastOnlineError;

    int          m_tier     = 1;          // effective tier for this session
    QStringList  m_features;              // individual feature slugs from server
};