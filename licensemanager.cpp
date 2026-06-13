// =============================================================================
// licensemanager.cpp — Implementation of LicenseManager (see licensemanager.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - All registry access goes through QSettings(NativeFormat, UserScope) under
//    REG_ORG/REG_APP; every write of a hash-covered value also rewrites the
//    salted integrity hash (TAMPER_SALT) so hand-edits are detectable.
//  - The "breadcrumb" is a second, signed copy of the install date in a file
//    under the user's config dir; initialize() takes the EARLIEST of registry
//    and breadcrumb dates so wiping HKCU alone cannot reset the trial.
//  - initialize() is strictly offline; startOnlineHeartbeat() does the server
//    round-trip asynchronously after the UI is up. SERVER_URL is a per-
//    deployment placeholder.
// =============================================================================
#include "licensemanager.h"
#include <QSettings>
#include <QCryptographicHash>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QSysInfo>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Set this to your deployed licensing server (Cloudflare Worker — see
// server/licensing-worker/README.md for the 15-minute deploy steps).
static const QString SERVER_URL  = "https://keynetik-license.kelvinyabate.workers.dev";  // Cloudflare Worker (see license-server/)
static const QString REG_ORG     = "KeynetikSolutions";
static const QString REG_APP     = "KeynetikPOS";
static const QString TAMPER_SALT = "KNK-TAMPER-2025";              // ← keep secret

// ════════════════════════════════════════════════════════════════
// SINGLETON
// ════════════════════════════════════════════════════════════════
LicenseManager& LicenseManager::instance() {
    static LicenseManager inst;
    return inst;
}

// ════════════════════════════════════════════════════════════════
// REGISTRY HELPERS
// ════════════════════════════════════════════════════════════════
QString LicenseManager::readStoredKey() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    return s.value("License/CDKey", "").toString();
}

QDateTime LicenseManager::readInstallDate() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    QString iso = s.value("License/InstallDate", "").toString();
    return iso.isEmpty() ? QDateTime() : QDateTime::fromString(iso, Qt::ISODate);
}

void LicenseManager::writeInstallDate(const QDateTime& dt) const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    s.setValue("License/InstallDate", dt.toString(Qt::ISODate));
    writeRegistryHash();
}

QString LicenseManager::readActivationSource() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    return s.value("License/ActivationSource", "").toString();
}

void LicenseManager::writeActivationSource(const QString& source) const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    s.setValue("License/ActivationSource", source);
    writeRegistryHash();
}

bool LicenseManager::readRevoked() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    return s.value("License/Revoked", false).toBool();
}

void LicenseManager::writeRevoked(bool revoked) const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    s.setValue("License/Revoked", revoked);
    writeRegistryHash();   // Revoked is covered by the integrity hash
}

QDateTime LicenseManager::readLastOnlineCheck() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    QString iso = s.value("License/LastOnlineCheck", "").toString();
    return iso.isEmpty() ? QDateTime() : QDateTime::fromString(iso, Qt::ISODate);
}

void LicenseManager::writeLastOnlineCheck() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    s.setValue("License/LastOnlineCheck",
               QDateTime::currentDateTime().toString(Qt::ISODate));
}

int LicenseManager::readOfflineDays() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    return s.value("License/OfflineDays", 0).toInt();
}

void LicenseManager::writeOfflineDays(int days) const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    s.setValue("License/OfflineDays", days);
}

// ════════════════════════════════════════════════════════════════
// OUT-OF-REGISTRY BREADCRUMB
// A second copy of the install date, kept in a file under the user's
// config dir. Wiping HKCU alone no longer resets the trial: initialize()
// takes the EARLIEST of this and the registry value. The contents are
// signed (machine-bound) so the date can't be hand-edited to extend the
// trial — same defence-in-depth model as the registry integrity hash.
// ════════════════════════════════════════════════════════════════
QString LicenseManager::breadcrumbPath() const {
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    return base + "/" + REG_ORG + "/.license_state";
}

QDateTime LicenseManager::readBreadcrumbInstallDate() const {
    QFile f(breadcrumbPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QDateTime();

    const QString line = QString::fromUtf8(f.readAll()).trimmed();
    const int sep = line.indexOf('|');
    if (sep <= 0)
        return QDateTime();

    const QString iso  = line.left(sep);
    const QString sig  = line.mid(sep + 1);
    const QString want = QString::fromLatin1(
        QCryptographicHash::hash(
            (iso + QString::fromLatin1(QSysInfo::machineUniqueId()) + TAMPER_SALT).toUtf8(),
            QCryptographicHash::Sha256).toHex());

    if (sig != want)
        return QDateTime();   // forged or copied from another machine
    return QDateTime::fromString(iso, Qt::ISODate);
}

void LicenseManager::writeBreadcrumbInstallDate(const QDateTime& dt) const {
    const QString path = breadcrumbPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    const QString iso = dt.toString(Qt::ISODate);
    const QString sig = QString::fromLatin1(
        QCryptographicHash::hash(
            (iso + QString::fromLatin1(QSysInfo::machineUniqueId()) + TAMPER_SALT).toUtf8(),
            QCryptographicHash::Sha256).toHex());

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write((iso + "|" + sig).toUtf8());
        f.close();
#ifdef Q_OS_WIN
        // Keep it out of the way of a casual file listing
        SetFileAttributesW(reinterpret_cast<const wchar_t*>(path.utf16()),
                           FILE_ATTRIBUTE_HIDDEN);
#endif
    }
}

// ════════════════════════════════════════════════════════════════
// TAMPER DETECTION
// ════════════════════════════════════════════════════════════════
QString LicenseManager::computeRegistryHash(const QString& key,
                                            const QString& installDate,
                                            const QString& source,
                                            bool revoked) const {
    QString combined = key + installDate + source
                       + (revoked ? "1" : "0") + TAMPER_SALT;
    return QCryptographicHash::hash(combined.toUtf8(),
                                    QCryptographicHash::Sha256).toHex();
}

void LicenseManager::writeRegistryHash() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    QString key     = s.value("License/CDKey",            "").toString();
    QString date    = s.value("License/InstallDate",      "").toString();
    QString source  = s.value("License/ActivationSource", "").toString();
    bool    revoked = s.value("License/Revoked",          false).toBool();
    s.setValue("License/Integrity", computeRegistryHash(key, date, source, revoked));
}

bool LicenseManager::verifyRegistryHash() const {
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    QString key     = s.value("License/CDKey",            "").toString();
    QString date    = s.value("License/InstallDate",      "").toString();
    QString source  = s.value("License/ActivationSource", "").toString();
    bool    revoked = s.value("License/Revoked",          false).toBool();
    QString stored  = s.value("License/Integrity",        "").toString();

    if (stored.isEmpty()) {
        // No hash is only legitimate before anything was ever written;
        // a missing hash next to existing license data means it was deleted.
        // (revoked defaults to false, so a lone leftover flag also fails here.)
        return key.isEmpty() && date.isEmpty() && source.isEmpty() && !revoked;
    }
    // Deleting the Revoked value reads back as false and no longer matches the
    // signed hash → tampered → locked out, so removing it is not an escape.
    return stored == computeRegistryHash(key, date, source, revoked);
}

// ════════════════════════════════════════════════════════════════
// OFFLINE KEY FORMAT VALIDATION
// ════════════════════════════════════════════════════════════════
bool LicenseManager::validateKey(const QString& key) const {
    QString clean = key.toUpper().remove('-').trimmed();
    if (clean.length() != 16) return false;

    QString seg0 = clean.mid(0, 4);
    QString seg1 = clean.mid(4, 4);
    QString seg2 = clean.mid(8, 4);

    QByteArray hash = QCryptographicHash::hash(
                          (seg0 + seg2 + "KNK-SALT-2025").toUtf8(),
                          QCryptographicHash::Sha256
                          ).toHex();

    return hash.startsWith(seg1.toLower().toUtf8());
}

// ════════════════════════════════════════════════════════════════
// ONLINE HEARTBEAT — asynchronous, fired after the UI is up.
// Never blocks startup. A confirmed-valid key refreshes the grace
// window; a server-side rejection is recorded and enforced at the
// next launch (no mid-session lockout).
// ════════════════════════════════════════════════════════════════
void LicenseManager::startOnlineHeartbeat() {
    if (m_state != LicenseState::FullLicense)
        return;
    const QString key = readStoredKey();
    if (key.isEmpty())
        return;

    auto* mgr = new QNetworkAccessManager(QCoreApplication::instance());

    QUrl url(SERVER_URL + "/validate");
    QUrlQuery query;
    query.addQueryItem("key", key.trimmed().toUpper());
    query.addQueryItem("device_id", QString::fromLatin1(QSysInfo::machineUniqueId()));
    url.setQuery(query);

    QNetworkReply* reply = mgr->get(QNetworkRequest(url));
    QTimer::singleShot(8000, reply, &QNetworkReply::abort);

    QObject::connect(reply, &QNetworkReply::finished, [this, reply, mgr]() {
        reply->deleteLater();
        mgr->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_serverReachable = false;
            return;
        }

        m_serverReachable = true;
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        if (json["valid"].toBool()) {
            writeLastOnlineCheck();
            writeOfflineDays(0);
            writeRevoked(false);
            m_offlineDaysRemaining = MAX_OFFLINE_DAYS;
            m_offlineGraceExpired  = false;
        } else {
            // Enforced at next launch by initialize()
            writeRevoked(true);
        }
    });
}

// ════════════════════════════════════════════════════════════════
// ONLINE ACTIVATION — called when user enters their key
// ════════════════════════════════════════════════════════════════
LicenseManager::ActivationResult LicenseManager::activateOnServer(
    const QString& key, const QString& deviceId, const QString& deviceName)
{
    if (!validateKey(key))
        return ActivationResult::InvalidKey;

    QNetworkAccessManager mgr;
    QEventLoop            loop;
    QTimer                timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(8000);

    QString url = QString("%1/activate?key=%2&device_id=%3&device_name=%4")
                      .arg(SERVER_URL,
                           key.trimmed().toUpper(),
                           QString(QUrl::toPercentEncoding(deviceId)),
                           QString(QUrl::toPercentEncoding(deviceName)));

    QNetworkReply* reply = mgr.get(QNetworkRequest(QUrl(url)));
    QObject::connect(reply,    &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout,         &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();

    // ── Server unreachable → activation fails (no offline grant).
    // Granting a full license here meant pulling the network cable
    // turned any format-valid key into a permanent activation.
    if (!timeout.isActive() || reply->error() != QNetworkReply::NoError) {
        m_serverReachable = false;
        reply->deleteLater();
        return ActivationResult::ServerUnreachable;
    }

    // ── Server responded ─────────────────────────────────────────
    timeout.stop();
    m_serverReachable = true;
    QJsonObject json  = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    if (json["valid"].toBool()) {
        QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
        s.setValue("License/CDKey", key.toUpper().trimmed());
        writeActivationSource("online");
        writeLastOnlineCheck();
        m_state    = LicenseState::FullLicense;
        m_daysLeft = -1;
        return ActivationResult::Success;
    }

    QString reason = json["reason"].toString();
    if (reason == "device_limit_reached") return ActivationResult::DeviceLimitReached;
    if (reason == "key_revoked")          return ActivationResult::ServerRejected;
    if (reason == "key_expired")          return ActivationResult::KeyExpired;
    return ActivationResult::InvalidKey;
}

// ════════════════════════════════════════════════════════════════
// INITIALIZE — called once at app startup. Offline-only: no network
// I/O here, so the UI never waits on a timeout. Online revalidation
// happens in startOnlineHeartbeat() after the window is shown.
// ════════════════════════════════════════════════════════════════
void LicenseManager::initialize() {

    // ── Tamper check first ───────────────────────────────────────
    if (!verifyRegistryHash()) {
        m_tampered = true;
        m_state    = LicenseState::InvalidKey;
        m_daysLeft = 0;
        return;
    }

    QString key = readStoredKey();

    // Establish the install date from both the registry and the out-of-registry
    // breadcrumb. The trial clock anchors to the EARLIEST known date, so wiping
    // one location can't roll it back; we then re-sync both so a later wipe of
    // the other still has a witness.
    QDateTime regDate   = readInstallDate();
    QDateTime crumbDate = readBreadcrumbInstallDate();

    QDateTime installDate;
    if (regDate.isValid() && crumbDate.isValid())
        installDate = qMin(regDate, crumbDate);
    else if (regDate.isValid())
        installDate = regDate;
    else if (crumbDate.isValid())
        installDate = crumbDate;
    else
        installDate = QDateTime::currentDateTime();   // genuine first run

    if (regDate != installDate)
        writeInstallDate(installDate);
    if (crumbDate != installDate)
        writeBreadcrumbInstallDate(installDate);

    if (!key.isEmpty()) {
        if (readRevoked() || !validateKey(key)) {
            m_state    = LicenseState::InvalidKey;
            m_daysLeft = 0;
            return;
        }

        // Server-backed activations must phone home within the grace
        // window; locally activated keys have no server to check against.
        if (readActivationSource() == "online") {
            QDateTime lastCheck = readLastOnlineCheck();
            if (!lastCheck.isValid()) {
                writeLastOnlineCheck();
                lastCheck = QDateTime::currentDateTime();
            }
            const int daysSince = lastCheck.daysTo(QDateTime::currentDateTime());
            if (daysSince > MAX_OFFLINE_DAYS) {
                m_offlineGraceExpired  = true;
                m_offlineDaysRemaining = 0;
                m_state    = LicenseState::InvalidKey;
                m_daysLeft = 0;
                return;
            }
            m_offlineDaysRemaining = MAX_OFFLINE_DAYS - daysSince;
        }

        m_state    = LicenseState::FullLicense;
        m_daysLeft = -1;
        return;
    }

    // No key → trial
    int elapsed = installDate.daysTo(QDateTime::currentDateTime());
    m_daysLeft  = TRIAL_DAYS - elapsed;
    m_state     = (m_daysLeft > 0)
                  ? LicenseState::Trial
                  : LicenseState::TrialExpired;
}

// ════════════════════════════════════════════════════════════════
// LOCAL-ONLY ACTIVATE
// ════════════════════════════════════════════════════════════════
bool LicenseManager::activateKey(const QString& key) {
    if (!validateKey(key)) return false;
    QSettings s(QSettings::NativeFormat, QSettings::UserScope, REG_ORG, REG_APP);
    s.setValue("License/CDKey", key.toUpper().trimmed());
    writeActivationSource("local");
    writeLastOnlineCheck();
    writeRevoked(false);
    m_state    = LicenseState::FullLicense;
    m_daysLeft = -1;
    return true;
}

// ════════════════════════════════════════════════════════════════
// ACCESSORS
// ════════════════════════════════════════════════════════════════
bool         LicenseManager::isActivated()         const { return m_state == LicenseState::FullLicense; }
int          LicenseManager::trialDaysLeft()        const { return m_daysLeft; }
LicenseState LicenseManager::state()                const { return m_state; }
bool         LicenseManager::serverReachable()      const { return m_serverReachable; }
bool         LicenseManager::offlineGraceExpired()  const { return m_offlineGraceExpired; }
int          LicenseManager::offlineDaysRemaining() const { return m_offlineDaysRemaining; }
bool         LicenseManager::wasTampered()          const { return m_tampered; }

QString LicenseManager::maskedKey() const {
    QString k = readStoredKey();
    if (k.length() < 4) return "Not activated";
    return "****-****-****-" + k.right(4);
}