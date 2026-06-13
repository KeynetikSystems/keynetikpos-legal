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
#include <QDateTime>

enum class LicenseState {
    FullLicense,
    Trial,
    TrialExpired,
    InvalidKey
};

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

    // ── State ────────────────────────────────────────────────────
    LicenseState m_state                = LicenseState::Trial;
    int          m_daysLeft             = TRIAL_DAYS;
    bool         m_serverReachable      = false;
    bool         m_offlineGraceExpired  = false;
    int          m_offlineDaysRemaining = MAX_OFFLINE_DAYS;
    bool         m_tampered             = false;
    QString      m_lastOnlineError;
};