// =============================================================================
// onboardingwizard.cpp — Implementation of OnboardingWizard
//                        (see onboardingwizard.h for WHAT/HOW/WHY).
// =============================================================================
#include "onboardingwizard.h"

// =============================================================================
// WelcomePage
// =============================================================================
WelcomePage::WelcomePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Welcome to KeynetikPOS");
    setSubTitle("Let's get your business set up in a few quick steps.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *intro = new QLabel(
        "KeynetikPOS helps you run your point-of-sale, manage inventory, "
        "track sales, and grow your business.\n\n"
        "First, what's the name of your business?", this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QFormLayout *form = new QFormLayout();
    m_businessNameEdit = new QLineEdit(this);
    m_businessNameEdit->setPlaceholderText("e.g. Mama Fua General Store");
    form->addRow("Business Name:", m_businessNameEdit);
    layout->addLayout(form);

    // registerField with * makes the field mandatory — QWizard will disable
    // the Next button until the field is non-empty.
    registerField("businessName*", m_businessNameEdit);
}

// =============================================================================
// TaxPage
// =============================================================================
TaxPage::TaxPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Tax Configuration");
    setSubTitle("Set up tax for your sales (you can change this later in Settings).");

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_taxEnabledCheck = new QCheckBox("Enable Tax on Sales", this);
    layout->addWidget(m_taxEnabledCheck);
    registerField("taxEnabled", m_taxEnabledCheck);

    QFormLayout *form = new QFormLayout();

    m_taxRateSpin = new QDoubleSpinBox(this);
    m_taxRateSpin->setRange(0.0, 100.0);
    m_taxRateSpin->setDecimals(2);
    m_taxRateSpin->setSuffix(" %");
    m_taxRateSpin->setValue(16.0);   // common default (Kenya VAT)
    m_taxRateSpin->setEnabled(false);
    form->addRow("Tax Rate:", m_taxRateSpin);
    registerField("taxRate", m_taxRateSpin, "value", "valueChanged");

    m_taxLabelEdit = new QLineEdit(this);
    m_taxLabelEdit->setText("VAT");
    m_taxLabelEdit->setEnabled(false);
    form->addRow("Tax Label:", m_taxLabelEdit);
    registerField("taxLabel", m_taxLabelEdit);

    m_taxInclusiveCheck = new QCheckBox("Prices already include tax (tax-inclusive pricing)", this);
    m_taxInclusiveCheck->setEnabled(false);
    form->addRow("", m_taxInclusiveCheck);
    registerField("taxInclusive", m_taxInclusiveCheck);

    layout->addLayout(form);

    connect(m_taxEnabledCheck, &QCheckBox::toggled,
            this, &TaxPage::onTaxEnabledChanged);
}

void TaxPage::onTaxEnabledChanged(bool enabled)
{
    m_taxRateSpin->setEnabled(enabled);
    m_taxLabelEdit->setEnabled(enabled);
    m_taxInclusiveCheck->setEnabled(enabled);
}

// =============================================================================
// DonePage
// =============================================================================
DonePage::DonePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("You're all set!");
    setSubTitle("KeynetikPOS is ready to use.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    QLabel *hint = new QLabel(
        "You can update your business name, tax settings, receipt footer, "
        "and more at any time from the Settings menu.", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
}

void DonePage::initializePage()
{
    const QString name = field("businessName").toString();
    m_summaryLabel->setText(
        QString("Welcome, <b>%1</b>!\n\n"
                "Click Finish to open the POS screen and start selling.")
            .arg(name.toHtmlEscaped()));
}

// =============================================================================
// OnboardingWizard
// =============================================================================
OnboardingWizard::OnboardingWizard(SettingsManager *settings, QWidget *parent)
    : QWizard(parent)
    , m_settings(settings)
{
    setWindowTitle("KeynetikPOS Setup");
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(560, 400);

    addPage(new WelcomePage(this));
    addPage(new TaxPage(this));
    addPage(new DonePage(this));

    setButtonText(QWizard::FinishButton, "Start POS");

    connect(this, &QWizard::finished, this, &OnboardingWizard::onFinished);
}

void OnboardingWizard::onFinished(int result)
{
    if (result != QDialog::Accepted || !m_settings)
        return;

    BusinessSettings s = m_settings->settings();

    s.businessName  = field("businessName").toString().trimmed();
    s.taxEnabled    = field("taxEnabled").toBool();
    s.taxLabel      = field("taxLabel").toString().trimmed();
    s.taxInclusive  = field("taxInclusive").toBool();

    // taxRate is stored as a fraction (0.16 = 16%) in BusinessSettings.
    const double ratePercent = field("taxRate").toDouble();
    s.taxRate = ratePercent / 100.0;

    m_settings->setSettings(s);
    m_settings->save();
}
