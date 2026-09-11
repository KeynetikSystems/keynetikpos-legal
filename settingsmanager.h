// =============================================================================
// settingsmanager.h — SettingsManager + BusinessSettings: business config
// -----------------------------------------------------------------------------
// WHAT: Business-level configuration: identity (name, address, phone, email,
//       website), currency, receipt footer, full tax configuration (enabled,
//       rate, label, tax-INCLUSIVE pricing), receipt toggle, and the
//       PBKDF2-hashed discount PIN.
// HOW:  Persists to a simple AppSettings(Key, Value) table via INSERT OR
//       REPLACE; load() reads each key with struct defaults as fallbacks, so
//       missing keys are harmless. On load, any legacy plaintext PIN
//       (including the factory default "1234") is migrated to a PBKDF2 hash;
//       verifyDiscountPin() only compares hashes. Tax helpers implement both
//       add-on tax and back-calculation from inclusive prices
//       (total - total/(1+rate)). save() emits settingsChanged so open windows
//       refresh live.
// WHY:  This data lives in the DATABASE (not QSettings) because it is business
//       data that should travel with a backup of pos_database.db. Key-value
//       storage means adding a setting never needs a schema migration.
//       Tax-inclusive support matters because Kenya (like much of the world)
//       displays VAT-inclusive shelf prices.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QSqlDatabase>

struct BusinessSettings {
    QString businessName     = "My Business";
    QString address          = "";
    QString phone            = "";
    QString email            = "";
    QString website          = "";
    QString receiptFooter    = "Thank you for your business!";
    QString currencySymbol   = "KSh";
    QString currencyCode     = "KES";

    // Tax
    bool    taxEnabled       = false;
    double  taxRate          = 0.0;       // e.g. 0.15 = 15%
    QString taxLabel         = "Tax";     // e.g. "VAT", "GST", "Sales Tax"
    bool    taxInclusive     = false;     // true = prices already include tax

    // Receipt
    bool    printReceipt     = true;
    bool    showLogo         = false;

    // Discount PIN — stored as a PBKDF2 hash (see PasswordHasher).
    // The default "1234" is migrated to a hash on first load.
    QString discountPin      = "1234";
    bool    requirePinForDiscount = true;

    // Loyalty programme
    // Points earned: 1 point per loyaltySpendPerPoint cents spent (default 10000 = KSh 100)
    // Points redeemed: each point is worth loyaltyCentsPerPoint cents (default 10 = KSh 0.10)
    int loyaltySpendPerPoint  = 10000;   // cents; 10000 = KSh 100 per point
    int loyaltyCentsPerPoint  = 10;      // cents per point when redeeming

    // SMTP (email receipts). smtpSecurity: 0 = None, 1 = STARTTLS, 2 = SSL/TLS.
    // smtpFromEmail defaults to the business `email` if left blank.
    QString smtpHost         = "";
    int     smtpPort         = 587;
    int     smtpSecurity     = 1;
    QString smtpUsername     = "";
    QString smtpPassword     = "";   // app password; stored in the local DB
    QString smtpFromEmail    = "";
};

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    // db is held BY VALUE (cheap, ref-counted handle) so it stays valid even
    // if the caller's QSqlDatabase local goes out of scope — the same
    // reasoning as every repository class (see e.g. productrepository.h).
    // This used to be a reference, which is what let a MainWindow-ctor-local
    // QSqlDatabase dangle after construction — see the crash this fixed.
    explicit SettingsManager(QSqlDatabase db, QObject *parent = nullptr);

    // Load from DB
    void load();

    // Save to DB
    void save();

    // Accessors
    BusinessSettings settings() const { return m_settings; }
    void setSettings(const BusinessSettings &s) { m_settings = s; }

    // Convenience
    QString currencySymbol() const  { return m_settings.currencySymbol; }
    QString businessName() const    { return m_settings.businessName; }
    bool    taxEnabled() const      { return m_settings.taxEnabled; }
    double  taxRate() const         { return m_settings.taxRate; }
    QString taxLabel() const        { return m_settings.taxLabel; }
    bool    taxInclusive() const    { return m_settings.taxInclusive; }
    bool    requirePinForDiscount() const { return m_settings.requirePinForDiscount; }

    // Discount PIN (never exposes the plaintext)
    void setDiscountPin(const QString &plainPin);
    bool verifyDiscountPin(const QString &pin) const;

    // Tax helpers
    double  applyTax(double subtotal) const;
    double  extractTax(double inclusiveTotal) const;
    double  taxAmount(double subtotal) const;

signals:
    void settingsChanged();

private:
    void createTableIfNotExist();
    QString getSetting(const QString &key, const QString &defaultValue = "") const;
    void    setSetting(const QString &key, const QString &value);

    QSqlDatabase m_db;
    BusinessSettings m_settings;
};
