// =============================================================================
// settingsmanager.cpp — Implementation of SettingsManager (see
// settingsmanager.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Storage is a single AppSettings(Key, Value) table written with INSERT OR
//    REPLACE; load() falls back to the BusinessSettings defaults for missing
//    keys, so new settings never need a schema migration.
//  - load() migrates any legacy plaintext discount PIN (including the factory
//    default "1234") to a PBKDF2 hash on first sight; the plaintext is never
//    stored or exposed afterwards.
//  - extractTax() back-calculates the tax portion of a tax-INCLUSIVE total:
//    total - total/(1+rate).
// =============================================================================
#include "settingsmanager.h"
#include "passwordhasher.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

SettingsManager::SettingsManager(QSqlDatabase &db, QObject *parent)
    : QObject(parent), m_db(db)
{
    createTableIfNotExist();
    load();
}

void SettingsManager::createTableIfNotExist()
{
    QSqlQuery q(m_db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS AppSettings (
            Key   TEXT PRIMARY KEY,
            Value TEXT NOT NULL
        )
    )");
}

QString SettingsManager::getSetting(const QString &key, const QString &defaultValue) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT Value FROM AppSettings WHERE Key = :key");
    q.bindValue(":key", key);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return defaultValue;
}

void SettingsManager::setSetting(const QString &key, const QString &value)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO AppSettings (Key, Value) VALUES (:key, :value)");
    q.bindValue(":key", key);
    q.bindValue(":value", value);
    if (!q.exec()) {
        qDebug() << "SettingsManager: failed to save" << key << q.lastError().text();
    }
}

void SettingsManager::load()
{
    m_settings.businessName         = getSetting("businessName",         m_settings.businessName);
    m_settings.address              = getSetting("address",              m_settings.address);
    m_settings.phone                = getSetting("phone",                m_settings.phone);
    m_settings.email                = getSetting("email",                m_settings.email);
    m_settings.website              = getSetting("website",              m_settings.website);
    m_settings.receiptFooter        = getSetting("receiptFooter",        m_settings.receiptFooter);
    m_settings.currencySymbol       = getSetting("currencySymbol",       m_settings.currencySymbol);
    m_settings.currencyCode         = getSetting("currencyCode",         m_settings.currencyCode);

    m_settings.taxEnabled           = getSetting("taxEnabled",   "0") == "1";
    m_settings.taxRate              = getSetting("taxRate",       "0").toDouble();
    m_settings.taxLabel             = getSetting("taxLabel",      m_settings.taxLabel);
    m_settings.taxInclusive         = getSetting("taxInclusive",  "0") == "1";

    m_settings.printReceipt         = getSetting("printReceipt",  "1") == "1";
    m_settings.discountPin          = getSetting("discountPin",   m_settings.discountPin);
    m_settings.requirePinForDiscount= getSetting("requirePinForDiscount", "1") == "1";

    m_settings.smtpHost             = getSetting("smtpHost",      m_settings.smtpHost);
    m_settings.smtpPort             = getSetting("smtpPort",      QString::number(m_settings.smtpPort)).toInt();
    m_settings.smtpSecurity         = getSetting("smtpSecurity",  QString::number(m_settings.smtpSecurity)).toInt();
    m_settings.smtpUsername         = getSetting("smtpUsername",  m_settings.smtpUsername);
    m_settings.smtpPassword         = getSetting("smtpPassword",  m_settings.smtpPassword);
    m_settings.smtpFromEmail        = getSetting("smtpFromEmail", m_settings.smtpFromEmail);

    // Migrate a legacy plaintext PIN (including the factory default) to a hash
    if (!m_settings.discountPin.isEmpty()
        && !PasswordHasher::isPbkdf2(m_settings.discountPin)) {
        m_settings.discountPin = PasswordHasher::hash(m_settings.discountPin);
        setSetting("discountPin", m_settings.discountPin);
    }
}

void SettingsManager::setDiscountPin(const QString &plainPin)
{
    m_settings.discountPin = PasswordHasher::hash(plainPin);
}

bool SettingsManager::verifyDiscountPin(const QString &pin) const
{
    return PasswordHasher::verify(pin, m_settings.discountPin);
}

void SettingsManager::save()
{
    setSetting("businessName",          m_settings.businessName);
    setSetting("address",               m_settings.address);
    setSetting("phone",                 m_settings.phone);
    setSetting("email",                 m_settings.email);
    setSetting("website",               m_settings.website);
    setSetting("receiptFooter",         m_settings.receiptFooter);
    setSetting("currencySymbol",        m_settings.currencySymbol);
    setSetting("currencyCode",          m_settings.currencyCode);

    setSetting("taxEnabled",            m_settings.taxEnabled ? "1" : "0");
    setSetting("taxRate",               QString::number(m_settings.taxRate));
    setSetting("taxLabel",              m_settings.taxLabel);
    setSetting("taxInclusive",          m_settings.taxInclusive ? "1" : "0");

    setSetting("printReceipt",          m_settings.printReceipt ? "1" : "0");
    setSetting("discountPin",           m_settings.discountPin);
    setSetting("requirePinForDiscount", m_settings.requirePinForDiscount ? "1" : "0");

    setSetting("smtpHost",              m_settings.smtpHost);
    setSetting("smtpPort",              QString::number(m_settings.smtpPort));
    setSetting("smtpSecurity",          QString::number(m_settings.smtpSecurity));
    setSetting("smtpUsername",          m_settings.smtpUsername);
    setSetting("smtpPassword",          m_settings.smtpPassword);
    setSetting("smtpFromEmail",         m_settings.smtpFromEmail);

    emit settingsChanged();
}

double SettingsManager::taxAmount(double subtotal) const
{
    if (!m_settings.taxEnabled) return 0.0;
    return subtotal * m_settings.taxRate;
}

double SettingsManager::applyTax(double subtotal) const
{
    return subtotal + taxAmount(subtotal);
}

double SettingsManager::extractTax(double inclusiveTotal) const
{
    if (!m_settings.taxEnabled || m_settings.taxRate <= 0.0) return 0.0;
    return inclusiveTotal - (inclusiveTotal / (1.0 + m_settings.taxRate));
}
