// =============================================================================
// settingsdialog.h — SettingsDialog: tabbed editor for all configuration
// -----------------------------------------------------------------------------
// WHAT: Tabs for Business, Tax, Receipt, Security (discount PIN + require-PIN
//       toggle), and Messaging (Twilio WhatsApp SID/token/number and Africa's
//       Talking API key/username/sender ID, with a test-connection button).
// HOW:  loadCurrentValues() populates widgets from SettingsManager; save()
//       writes back the struct, hashes a newly entered PIN (entered twice for
//       confirmation), stores messaging credentials, and triggers
//       reloadMessagingProviders() so providers pick up new keys
//       without a restart. Tax controls enable/disable as a group via
//       onTaxEnabledChanged.
// WHY:  One dialog for all configuration keeps administration discoverable;
//       the PIN confirmation field and write-only PIN handling prevent both
//       typos and shoulder-surfing of the stored value.
// =============================================================================
#pragma once

#include <QDialog>

class SettingsManager;
class QTabWidget;
class QLineEdit;
class QTextEdit;
class QCheckBox;
class QDoubleSpinBox;
class QComboBox;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(SettingsManager *settings, QWidget *parent = nullptr);

private slots:
    void save();
    void resetToDefaults();
    void onTaxEnabledChanged(bool checked);

private:
    void setupUi();
    QWidget *buildBusinessTab();
    QWidget *buildTaxTab();
    QWidget *buildStatutoryTab();
    QWidget *buildReceiptTab();
    QWidget *buildSecurityTab();

    void loadCurrentValues();

    SettingsManager *m_settings;

    // Business tab
    QLineEdit   *m_businessName = nullptr;
    QLineEdit   *m_address      = nullptr;
    QLineEdit   *m_phone        = nullptr;
    QLineEdit   *m_email        = nullptr;
    QLineEdit   *m_website      = nullptr;
    QLineEdit   *m_currency     = nullptr;
    QLineEdit   *m_currencyCode = nullptr;

    QLineEdit *m_whatsappAccountSid = nullptr;
    QLineEdit *m_whatsappAuthToken  = nullptr;
    QLineEdit *m_whatsappFromNumber = nullptr;

    // Africa's Talking SMS fields (built in buildMessagingTab)
    QLineEdit *m_atApiKey    = nullptr;
    QLineEdit *m_atUsername  = nullptr;
    QLineEdit *m_atSenderId  = nullptr;

    QWidget *buildMessagingTab();
    void testWhatsAppConnection();

    // Tax tab
    QCheckBox       *m_taxEnabled   = nullptr;
    QDoubleSpinBox  *m_taxRate      = nullptr;
    QLineEdit       *m_taxLabel     = nullptr;
    QCheckBox       *m_taxInclusive = nullptr;

    // Statutory tab (payroll Finance-Act rates) — only present on the ERP-Full
    // tier; null otherwise, so load/save must check before use.
    class StatutoryRatesForm *m_statutoryForm = nullptr;

    // Receipt tab
    QTextEdit   *m_receiptFooter = nullptr;
    QCheckBox   *m_printReceipt  = nullptr;

    // Receipt tab — SMTP (email receipts)
    QLineEdit   *m_smtpHost      = nullptr;
    QSpinBox    *m_smtpPort      = nullptr;
    QComboBox   *m_smtpSecurity  = nullptr;
    QLineEdit   *m_smtpUser      = nullptr;
    QLineEdit   *m_smtpPassword  = nullptr;
    QLineEdit   *m_smtpFrom      = nullptr;

    // Security tab
    QLineEdit   *m_discountPin   = nullptr;
    QLineEdit   *m_discountPin2  = nullptr;
    QCheckBox   *m_requirePin    = nullptr;

    // Loyalty tab
    QSpinBox    *m_loyaltySpend  = nullptr;   // KSh per point (spend threshold)
    QSpinBox    *m_loyaltyRedeem = nullptr;   // cents per point when redeeming
    QWidget *buildLoyaltyTab();
};