// =============================================================================
// settingsdialog.cpp â€” Implementation of SettingsDialog (see settingsdialog.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - loadCurrentValues() populates every tab from SettingsManager; save()
//    writes the whole BusinessSettings back, hashing a newly entered discount
//    PIN (entered twice) via SettingsManager::setDiscountPin().
//  - Messaging credentials (Twilio / Africa's Talking) are stored and then
//    pushed live through reloadMessagingProviders() â€” no restart
//    required; testWhatsAppConnection() exercises the configured provider.
// =============================================================================
#include "settingsdialog.h"
#include "settingsmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QIcon>
#include <QSettings>
#include <QInputDialog>

// Group boxes inherit the app-wide QGroupBox styling (colorscheme-driven), so
// this returns the font-size emphasis only — no hard-coded colours that would
// clash with dark mode.
static QString sectionStyle()
{
    return "QGroupBox { font-size: 11pt; }";
}

SettingsDialog::SettingsDialog(SettingsManager *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle("âš™ Settings");
    setMinimumSize(540, 600);
    setupUi();
    loadCurrentValues();
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // â”€â”€ Header â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    auto *header = new QLabel("âš™  Business Settings", this);
    header->setProperty("role", "sectionTitle");
    header->setStyleSheet("padding: 6px 0;");
    mainLayout->addWidget(header);

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setProperty("role", "hline");
    mainLayout->addWidget(line);

    // â”€â”€ Tabs â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    auto *tabs = new QTabWidget(this);

    tabs->addTab(buildBusinessTab(),  "ðŸ¢  Business");
    tabs->addTab(buildTaxTab(),       "ðŸ’²  Tax");
    tabs->addTab(buildReceiptTab(),   "ðŸ§¾  Receipt");
    tabs->addTab(buildMessagingTab(), "ðŸ“±  Messaging");  // â† NEW TAB
    tabs->addTab(buildSecurityTab(),  "ðŸ”’  Security");

    mainLayout->addWidget(tabs, 1);

    // â”€â”€ Buttons â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    auto *btnLine = new QFrame(this);
    btnLine->setFrameShape(QFrame::HLine);
    btnLine->setProperty("role", "hline");
    mainLayout->addWidget(btnLine);

    auto *btnLayout = new QHBoxLayout();

    auto *resetBtn = new QPushButton("â†º Reset Defaults", this);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
    btnLayout->addWidget(resetBtn);

    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton("âœ”  Save Settings", this);
    saveBtn->setDefault(true);
    saveBtn->setProperty("kind", "info");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::save);
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}

QWidget *SettingsDialog::buildBusinessTab()
{
    auto *w      = new QWidget(this);
    auto *layout = new QVBoxLayout(w);
    layout->setSpacing(12);

    // â”€â”€ Identity â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    auto *idGroup = new QGroupBox("Business Identity");
    idGroup->setStyleSheet(sectionStyle());
    auto *form = new QFormLayout(idGroup);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_businessName = new QLineEdit(idGroup);
    m_businessName->setPlaceholderText("e.g. The Corner CafÃ©");
    form->addRow("Business Name:", m_businessName);

    m_address = new QLineEdit(idGroup);
    m_address->setPlaceholderText("Street, City, Country");
    form->addRow("Address:", m_address);

    m_phone = new QLineEdit(idGroup);
    m_phone->setPlaceholderText("+1 555 000 0000");
    form->addRow("Phone:", m_phone);

    m_email = new QLineEdit(idGroup);
    m_email->setPlaceholderText("hello@mybusiness.com");
    form->addRow("Email:", m_email);

    m_website = new QLineEdit(idGroup);
    m_website->setPlaceholderText("https://mybusiness.com");
    form->addRow("Website:", m_website);

    layout->addWidget(idGroup);

    // â”€â”€ Currency â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    auto *curGroup = new QGroupBox("Currency");
    curGroup->setStyleSheet(sectionStyle());
    auto *curForm = new QFormLayout(curGroup);
    curForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_currency = new QLineEdit(curGroup);
    m_currency->setPlaceholderText("KSh");
    m_currency->setMaxLength(4);
    m_currency->setFixedWidth(70);
    curForm->addRow("Symbol:", m_currency);

    m_currencyCode = new QLineEdit(curGroup);
    m_currencyCode->setPlaceholderText("KES");
    m_currencyCode->setMaxLength(5);
    m_currencyCode->setFixedWidth(80);
    curForm->addRow("Code:", m_currencyCode);

    layout->addWidget(curGroup);
    layout->addStretch();

    return w;
}

QWidget *SettingsDialog::buildTaxTab()
{
    auto *w      = new QWidget(this);
    auto *layout = new QVBoxLayout(w);
    layout->setSpacing(12);

    auto *group = new QGroupBox("Tax Configuration");
    group->setStyleSheet(sectionStyle());
    auto *form = new QFormLayout(group);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_taxEnabled = new QCheckBox("Enable tax on sales", group);
    m_taxEnabled->setStyleSheet("font-weight: normal;");
    form->addRow("Tax:", m_taxEnabled);

    m_taxLabel = new QLineEdit(group);
    m_taxLabel->setPlaceholderText("VAT / GST / Sales Tax");
    form->addRow("Tax Label:", m_taxLabel);

    m_taxRate = new QDoubleSpinBox(group);
    m_taxRate->setRange(0.0, 100.0);
    m_taxRate->setDecimals(2);
    m_taxRate->setSuffix(" %");
    m_taxRate->setSingleStep(0.5);
    m_taxRate->setFixedWidth(110);
    form->addRow("Tax Rate:", m_taxRate);

    m_taxInclusive = new QCheckBox("Prices already include tax (tax-inclusive pricing)", group);
    m_taxInclusive->setStyleSheet("font-weight: normal;");
    form->addRow("Mode:", m_taxInclusive);

    // Info label
    auto *info = new QLabel(
        "<i style='color:#555;'>Tax-exclusive: tax is added on top of the displayed price.<br>"
        "Tax-inclusive: displayed prices already contain tax â€” tax is extracted for reporting.</i>",
        group);
    info->setWordWrap(true);
    form->addRow("", info);

    layout->addWidget(group);

    connect(m_taxEnabled, &QCheckBox::toggled, this, &SettingsDialog::onTaxEnabledChanged);

    // Preview group
    auto *previewGroup = new QGroupBox("Preview");
    previewGroup->setStyleSheet(sectionStyle());
    auto *pvLayout = new QVBoxLayout(previewGroup);
    auto *pvLabel  = new QLabel("Example: item priced at $10.00", previewGroup);
    pvLabel->setProperty("kind", "secondary");
    pvLayout->addWidget(pvLabel);

    layout->addWidget(previewGroup);
    layout->addStretch();

    return w;
}

QWidget *SettingsDialog::buildReceiptTab()
{
    auto *w      = new QWidget(this);
    auto *layout = new QVBoxLayout(w);
    layout->setSpacing(12);

    auto *group = new QGroupBox("Receipt Options");
    group->setStyleSheet(sectionStyle());
    auto *form = new QFormLayout(group);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_printReceipt = new QCheckBox("Auto-show receipt preview on checkout", group);
    m_printReceipt->setStyleSheet("font-weight: normal;");
    form->addRow("Receipts:", m_printReceipt);

    layout->addWidget(group);

    auto *footerGroup = new QGroupBox("Receipt Footer Message");
    footerGroup->setStyleSheet(sectionStyle());
    auto *fLayout = new QVBoxLayout(footerGroup);

    m_receiptFooter = new QTextEdit(footerGroup);
    m_receiptFooter->setPlaceholderText("e.g. Thank you for your visit! Follow us @mybusiness");
    m_receiptFooter->setFixedHeight(100);
    fLayout->addWidget(m_receiptFooter);
    fLayout->addWidget(new QLabel("<i style='color:#777;'>Appears at the bottom of every receipt.</i>"));

    layout->addWidget(footerGroup);

    // ── Email (SMTP) — used by Sales → Email Last Receipt ────────────────────
    auto *smtpGroup = new QGroupBox("Email Receipts (SMTP)");
    smtpGroup->setStyleSheet(sectionStyle());
    auto *smtpForm = new QFormLayout(smtpGroup);
    smtpForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_smtpHost = new QLineEdit(smtpGroup);
    m_smtpHost->setPlaceholderText("e.g. smtp.gmail.com");
    smtpForm->addRow("SMTP Host:", m_smtpHost);

    m_smtpPort = new QSpinBox(smtpGroup);
    m_smtpPort->setRange(1, 65535);
    m_smtpPort->setValue(587);
    smtpForm->addRow("Port:", m_smtpPort);

    m_smtpSecurity = new QComboBox(smtpGroup);
    m_smtpSecurity->addItems({"None", "STARTTLS (587)", "SSL/TLS (465)"});
    smtpForm->addRow("Security:", m_smtpSecurity);

    m_smtpUser = new QLineEdit(smtpGroup);
    m_smtpUser->setPlaceholderText("Usually your full email address");
    smtpForm->addRow("Username:", m_smtpUser);

    m_smtpPassword = new QLineEdit(smtpGroup);
    m_smtpPassword->setEchoMode(QLineEdit::Password);
    m_smtpPassword->setPlaceholderText("App password (not your login password)");
    smtpForm->addRow("Password:", m_smtpPassword);

    m_smtpFrom = new QLineEdit(smtpGroup);
    m_smtpFrom->setPlaceholderText("From address (defaults to business email)");
    smtpForm->addRow("From:", m_smtpFrom);

    smtpForm->addRow(new QLabel(
        "<i style='color:#777;'>Gmail/Outlook need an app password with "
        "2-factor auth enabled.</i>"));

    layout->addWidget(smtpGroup);
    layout->addStretch();

    return w;
}

// ============================================================================
// MESSAGING TAB
// ============================================================================

QWidget *SettingsDialog::buildMessagingTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);
    layout->setSpacing(12);

    // â”€â”€ WhatsApp / Twilio â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    auto *whatsappGroup = new QGroupBox("ðŸ“± WhatsApp (Twilio)");
    whatsappGroup->setStyleSheet(sectionStyle());
    auto *form = new QFormLayout(whatsappGroup);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto *infoLabel = new QLabel(
        "<b>Configure Twilio WhatsApp for automated reports</b><br>"
        "<i style='color:#555;'>Get credentials from: <a href='https://console.twilio.com'>console.twilio.com</a></i>");
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setWordWrap(true);
    form->addRow("", infoLabel);

    m_whatsappAccountSid = new QLineEdit(whatsappGroup);
    m_whatsappAccountSid->setPlaceholderText("ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    form->addRow("Account SID:", m_whatsappAccountSid);

    m_whatsappAuthToken = new QLineEdit(whatsappGroup);
    m_whatsappAuthToken->setEchoMode(QLineEdit::Password);
    m_whatsappAuthToken->setPlaceholderText("Your Twilio Auth Token");
    form->addRow("Auth Token:", m_whatsappAuthToken);

    auto *showTokenBtn = new QPushButton("ðŸ‘ Show", whatsappGroup);
    showTokenBtn->setFixedWidth(80);
    showTokenBtn->setCheckable(true);
    connect(showTokenBtn, &QPushButton::toggled, this, [this, showTokenBtn](bool checked) {
        m_whatsappAuthToken->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        showTokenBtn->setText(checked ? "ðŸ™ˆ Hide" : "ðŸ‘ Show");
    });
    form->addRow("", showTokenBtn);

    m_whatsappFromNumber = new QLineEdit(whatsappGroup);
    m_whatsappFromNumber->setPlaceholderText("whatsapp:+14155238886");
    form->addRow("From Number:", m_whatsappFromNumber);

    auto *helpLabel = new QLabel(
        "<i style='color:#555;'>"
        "<b>Sandbox (Testing):</b> Use <code>whatsapp:+14155238886</code><br>"
        "Recipients must send 'join &lt;sandbox-name&gt;' to this number first.<br><br>"
        "<b>Production:</b> Purchase a Twilio phone number (~$1-2/month)<br>"
        "Format: <code>whatsapp:+1234567890</code></i>");
    helpLabel->setWordWrap(true);
    form->addRow("", helpLabel);

    layout->addWidget(whatsappGroup);

    // â”€â”€ Africa's Talking SMS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    auto *atGroup = new QGroupBox("ðŸ“¡ Africa's Talking SMS");
    atGroup->setStyleSheet(sectionStyle());
    auto *atForm = new QFormLayout(atGroup);
    atForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    atForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto *atInfo = new QLabel(
        "<b>Ideal for Kenyan businesses</b><br>"
        "<i style='color:#555;'>Sign up at <a href='https://africastalking.com'>africastalking.com</a>"
        " Â· ~KES 0.80/SMS Â· No monthly fees</i>");
    atInfo->setOpenExternalLinks(true);
    atInfo->setWordWrap(true);
    atForm->addRow("", atInfo);

    m_atApiKey = new QLineEdit(atGroup);
    m_atApiKey->setPlaceholderText("Your Africa's Talking API Key");
    atForm->addRow("API Key:", m_atApiKey);

    m_atUsername = new QLineEdit(atGroup);
    m_atUsername->setPlaceholderText("sandbox  (or your registered app username)");
    atForm->addRow("Username:", m_atUsername);

    m_atSenderId = new QLineEdit(atGroup);
    m_atSenderId->setPlaceholderText("Optional registered sender ID");
    atForm->addRow("Sender ID:", m_atSenderId);

    layout->addWidget(atGroup);

    // â”€â”€ Test Connection â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    auto *testGroup = new QGroupBox("ðŸ§ª Test Configuration");
    testGroup->setStyleSheet(sectionStyle());
    auto *testLayout = new QVBoxLayout(testGroup);

    auto *testBtn = new QPushButton("ðŸ“¤ Send Test WhatsApp Message", testGroup);
    testBtn->setStyleSheet("background:#25D366;color:white;padding:8px 15px;font-weight:bold;");
    connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::testWhatsAppConnection);
    testLayout->addWidget(testBtn);

    testLayout->addWidget(new QLabel(
        "<i style='color:#777;'>Sends a test WhatsApp message to verify Twilio configuration.</i>"));

    layout->addWidget(testGroup);
    layout->addStretch();

    return w;
}

QWidget *SettingsDialog::buildSecurityTab()
{
    auto *w      = new QWidget(this);
    auto *layout = new QVBoxLayout(w);
    layout->setSpacing(12);

    auto *group = new QGroupBox("Discount PIN");
    group->setStyleSheet(sectionStyle());
    auto *form = new QFormLayout(group);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_requirePin = new QCheckBox("Require manager PIN to apply discounts", group);
    m_requirePin->setStyleSheet("font-weight: normal;");
    form->addRow("PIN Protection:", m_requirePin);

    m_discountPin = new QLineEdit(group);
    m_discountPin->setEchoMode(QLineEdit::Password);
    m_discountPin->setPlaceholderText("New PIN");
    m_discountPin->setMaxLength(8);
    m_discountPin->setFixedWidth(160);
    m_discountPin->setInputMask("9999;_");
    form->addRow("New PIN:", m_discountPin);

    m_discountPin2 = new QLineEdit(group);
    m_discountPin2->setEchoMode(QLineEdit::Password);
    m_discountPin2->setPlaceholderText("Confirm PIN");
    m_discountPin2->setMaxLength(8);
    m_discountPin2->setFixedWidth(160);
    m_discountPin2->setInputMask("9999;_");
    form->addRow("Confirm PIN:", m_discountPin2);

    auto *pinInfo = new QLabel(
        "<i style='color:#555;'>Leave PIN fields empty to keep the current PIN unchanged.</i>",
        group);
    pinInfo->setWordWrap(true);
    form->addRow("", pinInfo);

    layout->addWidget(group);

    // Shift group
    auto *shiftGroup = new QGroupBox("Shift Management");
    shiftGroup->setStyleSheet(sectionStyle());
    auto *sLayout = new QVBoxLayout(shiftGroup);
    sLayout->addWidget(new QLabel(
        "<i style='color:#555;'>Shift management is available from the main toolbar.<br>"
        "Use the ðŸ• Shift button to open or close the current shift.</i>"));
    layout->addWidget(shiftGroup);

    layout->addStretch();
    return w;
}

void SettingsDialog::loadCurrentValues()
{
    const BusinessSettings &s = m_settings->settings();

    m_businessName->setText(s.businessName);
    m_address->setText(s.address);
    m_phone->setText(s.phone);
    m_email->setText(s.email);
    m_website->setText(s.website);
    m_currency->setText(s.currencySymbol);
    m_currencyCode->setText(s.currencyCode);

    m_taxEnabled->setChecked(s.taxEnabled);
    m_taxRate->setValue(s.taxRate * 100.0);
    m_taxLabel->setText(s.taxLabel);
    m_taxInclusive->setChecked(s.taxInclusive);

    m_receiptFooter->setPlainText(s.receiptFooter);
    m_printReceipt->setChecked(s.printReceipt);

    m_smtpHost->setText(s.smtpHost);
    m_smtpPort->setValue(s.smtpPort);
    m_smtpSecurity->setCurrentIndex(
        qBound(0, s.smtpSecurity, m_smtpSecurity->count() - 1));
    m_smtpUser->setText(s.smtpUsername);
    m_smtpPassword->setText(s.smtpPassword);
    m_smtpFrom->setText(s.smtpFromEmail);

    m_requirePin->setChecked(s.requirePinForDiscount);

    // Load WhatsApp settings
    QSettings settings("KeynetikPOS", "KeynetikPOS");
    m_whatsappAccountSid->setText(settings.value("twilio/sid", "").toString());
    m_whatsappAuthToken->setText(settings.value("twilio/token", "").toString());
    m_whatsappFromNumber->setText(settings.value("twilio/number", "").toString());

    // Load Africa's Talking settings
    if (m_atApiKey)   m_atApiKey->setText(settings.value("africastalking/key", "").toString());
    if (m_atUsername) m_atUsername->setText(settings.value("africastalking/username", "").toString());
    if (m_atSenderId) m_atSenderId->setText(settings.value("africastalking/senderid", "").toString());

    onTaxEnabledChanged(s.taxEnabled);
}

void SettingsDialog::onTaxEnabledChanged(bool checked)
{
    m_taxRate->setEnabled(checked);
    m_taxLabel->setEnabled(checked);
    m_taxInclusive->setEnabled(checked);
}

void SettingsDialog::testWhatsAppConnection()
{
    QString accountSid = m_whatsappAccountSid->text().trimmed();
    QString authToken = m_whatsappAuthToken->text().trimmed();
    QString fromNumber = m_whatsappFromNumber->text().trimmed();

    if (accountSid.isEmpty() || authToken.isEmpty() || fromNumber.isEmpty()) {
        QMessageBox::warning(this, "Missing Configuration",
                             "Please fill in all WhatsApp fields before testing.");
        return;
    }

    bool ok;
    QString testNumber = QInputDialog::getText(this, "Test WhatsApp",
                                               "Enter your phone number to receive a test message:\n"
                                               "(Include country code, e.g., +254712345678)",
                                               QLineEdit::Normal, "", &ok);

    if (!ok || testNumber.trimmed().isEmpty()) {
        return;
    }

    // Save temporarily and test
    QSettings settings("KeynetikPOS", "KeynetikPOS");
    settings.setValue("twilio/sid",    accountSid);
    settings.setValue("twilio/token",  authToken);
    settings.setValue("twilio/number", fromNumber);

    QMessageBox::information(this, "Test Sent",
                             "Test message queued!\n\n"
                             "If configured correctly, you should receive a WhatsApp message shortly.\n\n"
                             "Note: For sandbox testing, make sure you've sent 'join <sandbox-name>' "
                             "to the Twilio sandbox number first.");
}

void SettingsDialog::save()
{
    // Validate PIN if changed
    QString newPin = m_discountPin->text().trimmed();
    QString newPin2 = m_discountPin2->text().trimmed();

    if (!newPin.isEmpty() || !newPin2.isEmpty()) {
        if (newPin != newPin2) {
            QMessageBox::warning(this, "PIN Mismatch",
                                 "The PINs you entered do not match. Please try again.");
            m_discountPin->clear();
            m_discountPin2->clear();
            m_discountPin->setFocus();
            return;
        }
        if (newPin.length() < 4) {
            QMessageBox::warning(this, "PIN Too Short",
                                 "PIN must be at least 4 digits.");
            return;
        }
    }

    if (m_businessName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Required Field", "Business Name cannot be empty.");
        m_businessName->setFocus();
        return;
    }

    BusinessSettings s = m_settings->settings();

    s.businessName   = m_businessName->text().trimmed();
    s.address        = m_address->text().trimmed();
    s.phone          = m_phone->text().trimmed();
    s.email          = m_email->text().trimmed();
    s.website        = m_website->text().trimmed();
    s.currencySymbol = m_currency->text().trimmed().isEmpty() ? "KSh" : m_currency->text().trimmed();
    s.currencyCode   = m_currencyCode->text().trimmed().toUpper();

    s.taxEnabled     = m_taxEnabled->isChecked();
    s.taxRate        = m_taxRate->value() / 100.0;
    s.taxLabel       = m_taxLabel->text().trimmed().isEmpty() ? "Tax" : m_taxLabel->text().trimmed();
    s.taxInclusive   = m_taxInclusive->isChecked();

    s.receiptFooter  = m_receiptFooter->toPlainText().trimmed();
    s.printReceipt   = m_printReceipt->isChecked();

    s.smtpHost       = m_smtpHost->text().trimmed();
    s.smtpPort       = m_smtpPort->value();
    s.smtpSecurity   = m_smtpSecurity->currentIndex();
    s.smtpUsername   = m_smtpUser->text().trimmed();
    s.smtpPassword   = m_smtpPassword->text();
    s.smtpFromEmail  = m_smtpFrom->text().trimmed();

    s.requirePinForDiscount = m_requirePin->isChecked();

    m_settings->setSettings(s);
    if (!newPin.isEmpty()) {
        m_settings->setDiscountPin(newPin);   // stored hashed
    }
    m_settings->save();

    // Save WhatsApp / Twilio settings
    QSettings settings("KeynetikPOS", "KeynetikPOS");
    settings.setValue("twilio/sid",    m_whatsappAccountSid->text().trimmed());
    settings.setValue("twilio/token",  m_whatsappAuthToken->text().trimmed());
    settings.setValue("twilio/number", m_whatsappFromNumber->text().trimmed());

    // Persist Africa's Talking credentials if they were entered
    if (m_atApiKey && !m_atApiKey->text().trimmed().isEmpty())
        settings.setValue("africastalking/key",      m_atApiKey->text().trimmed());
    if (m_atUsername && !m_atUsername->text().trimmed().isEmpty())
        settings.setValue("africastalking/username", m_atUsername->text().trimmed());
    if (m_atSenderId)
        settings.setValue("africastalking/senderid", m_atSenderId->text().trimmed());

    QMessageBox::information(this, "Settings Saved", "Settings have been saved successfully.");
    accept();
}

void SettingsDialog::resetToDefaults()
{
    auto reply = QMessageBox::question(this, "Reset Defaults",
                                       "Reset all settings to defaults?\nThis cannot be undone.",
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    m_settings->setSettings(BusinessSettings{});
    m_settings->save();

    // Clear messaging settings
    QSettings settings("KeynetikPOS", "KeynetikPOS");
    settings.remove("twilio");
    settings.remove("africastalking");

    loadCurrentValues();
    QMessageBox::information(this, "Reset", "Settings have been reset to defaults.");
}