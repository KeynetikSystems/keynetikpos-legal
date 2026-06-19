// =============================================================================
// paymentdialog.cpp — Implementation of PaymentDialog (see paymentdialog.h for
// the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - The amount is a QDoubleSpinBox, so only valid currency values can be
//    entered. calculateChange() re-runs on every change; Confirm is disabled
//    while the tendered amount is below the total, so an under-payment can
//    never reach Database::recordSale().
//  - Non-cash methods preset the amount to the exact total (no change due)
//    and enable the transaction-reference field; Mobile Money requires it
//    (M-Pesa always issues a confirmation code).
// =============================================================================
#include "paymentdialog.h"
#include "appstyle.h"
#include "cart.h"          // formatMoney(), currencySymbol()
#include "mpesaclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <cmath>

namespace {
// M-Pesa confirmation codes are 10 alphanumeric characters (e.g. SLJ7X8K2P0).
const QRegularExpression kMpesaCode("^[A-Z0-9]{10}$");
}

PaymentDialog::PaymentDialog(Money totalAmount, QWidget *parent)
    : QDialog(parent)
    , total(totalAmount.toMajor())
    , amountPaid(0.0)
    , change(0.0)
    , paymentMethod("Cash")
{
    setWindowTitle("Process Payment");
    setModal(true);
    setupUI();
}

PaymentDialog::~PaymentDialog()
{
}

void PaymentDialog::setupUI()
{
    resize(450, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Total amount display — themed banner (was hardcoded light-blue, which
    // broke in dark mode)
    totalLabel = new QLabel("Total Amount: " + formatMoney(Money::fromMajor(total)));
    totalLabel->setProperty("role", "banner");
    totalLabel->setProperty("kind", "success");
    totalLabel->setProperty("textScale", "2xl");
    totalLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(totalLabel);

    mainLayout->addSpacing(20);

    // Payment method selection
    QGroupBox *paymentMethodGroup = new QGroupBox("Select Payment Method");
    QVBoxLayout *methodLayout = new QVBoxLayout(paymentMethodGroup);

    this->paymentMethodGroup = new QButtonGroup(this);

    // Payment-method radios share one look; tag them and let appstyle size them.
    auto addMethodRadio = [&](const QString &text, const QString &accName,
                              int id) {
        QRadioButton *r = new QRadioButton(text);
        r->setProperty("textScale", "lg");
        r->setProperty("role", "control");
        r->setAccessibleName(accName);
        this->paymentMethodGroup->addButton(r, id);
        methodLayout->addWidget(r);
        return r;
    };

    cashRadio = addMethodRadio("Cash", "Pay with cash", 0);
    cashRadio->setChecked(true);
    cardRadio     = addMethodRadio("Credit/Debit Card", "Pay by card", 1);
    mobileRadio   = addMethodRadio("Mobile Money (M-Pesa)",
                                   "Pay with M-Pesa mobile money", 2);
    multipleRadio = addMethodRadio("Multiple Payment Methods",
                                   "Split across multiple payment methods", 3);

    mainLayout->addWidget(paymentMethodGroup);

    mainLayout->addSpacing(10);

    // Amount paid section
    QGroupBox *amountGroup = new QGroupBox("Payment Details");
    QVBoxLayout *amountLayout = new QVBoxLayout(amountGroup);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel("Amount Paid:"));
    amountPaidSpin = new QDoubleSpinBox();
    amountPaidSpin->setRange(0.0, 9999999.99);
    amountPaidSpin->setDecimals(2);
    amountPaidSpin->setPrefix(currencySymbol() + " ");
    amountPaidSpin->setValue(total);
    amountPaidSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    amountPaidSpin->setProperty("textScale", "xl");
    amountPaidSpin->setProperty("role", "control");
    amountPaidSpin->setAccessibleName("Amount tendered");
    inputLayout->addWidget(amountPaidSpin);
    amountLayout->addLayout(inputLayout);

    QHBoxLayout *refLayout = new QHBoxLayout();
    referenceTitleLabel = new QLabel("Reference No:");
    refLayout->addWidget(referenceTitleLabel);
    referenceEdit = new QLineEdit();
    referenceEdit->setProperty("textScale", "md");
    referenceEdit->setAccessibleName("Transaction reference number");
    refLayout->addWidget(referenceEdit);
    amountLayout->addLayout(refLayout);

    // While in M-Pesa mode, restrict input to up to 10 alphanumerics and force
    // upper-case live so the cashier can type the code from the SMS as-is.
    m_mpesaValidator = new QRegularExpressionValidator(
        QRegularExpression("[A-Za-z0-9]{0,10}"), this);
    connect(referenceEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        if (paymentMethod != "Mobile Money") return;
        const QString up = t.toUpper();
        if (up != t) {
            const int pos = referenceEdit->cursorPosition();
            QSignalBlocker blocker(referenceEdit);
            referenceEdit->setText(up);
            referenceEdit->setCursorPosition(pos);
        }
    });

    // ── M-Pesa STK push row (visible only in Mobile Money mode) ──────────────
    QHBoxLayout *stkLayout = new QHBoxLayout();
    m_phoneEdit = new QLineEdit();
    m_phoneEdit->setPlaceholderText("Customer phone e.g. 0712345678");
    m_phoneEdit->setProperty("textScale", "md");
    stkLayout->addWidget(m_phoneEdit, 1);
    m_stkButton = new QPushButton("Send M-Pesa Prompt");
    m_stkButton->setProperty("kind", "info");
    connect(m_stkButton, &QPushButton::clicked, this, &PaymentDialog::onStkPushClicked);
    stkLayout->addWidget(m_stkButton);
    amountLayout->addLayout(stkLayout);

    m_stkStatus = new QLabel();
    m_stkStatus->setProperty("role", "banner");
    m_stkStatus->setProperty("kind", "info");
    m_stkStatus->setWordWrap(true);
    m_stkStatus->hide();
    amountLayout->addWidget(m_stkStatus);

    m_mpesa = new MpesaClient(this);
    connect(m_mpesa, &MpesaClient::statusChanged, this, [this](const QString &m) {
        setStyleProperty(m_stkStatus, "kind", "info");
        m_stkStatus->setText(m);
        m_stkStatus->show();
    });
    connect(m_mpesa, &MpesaClient::promptSent, this, [this](const QString &m) {
        m_stkStatus->setText(m);
    });
    connect(m_mpesa, &MpesaClient::paymentConfirmed, this, [this](const QString &receipt) {
        referenceEdit->setText(receipt);        // record the M-Pesa code on the sale
        accept();                                // auto-confirm the payment
    });
    connect(m_mpesa, &MpesaClient::paymentFailed, this, [this](const QString &reason) {
        setStyleProperty(m_stkStatus, "kind", "danger");
        m_stkStatus->setText(reason + "  You can still enter the code manually below.");
        m_stkStatus->show();
        m_stkButton->setEnabled(MpesaClient::isAvailable());
        m_phoneEdit->setEnabled(true);
    });

    changeLabel = new QLabel("Change: " + formatMoney(Money()));
    changeLabel->setProperty("kind", "success");
    changeLabel->setProperty("textScale", "xl");
    changeLabel->setProperty("bold", "true");
    amountLayout->addWidget(changeLabel);

    // Loyalty points row — shown only when customer has points
    loyaltyLabel = new QLabel();
    loyaltyLabel->setProperty("role", "banner");
    loyaltyLabel->setProperty("kind", "secondary");
    loyaltyLabel->hide();
    amountLayout->addWidget(loyaltyLabel);

    redeemPointsButton = new QPushButton("Redeem Loyalty Points");
    redeemPointsButton->setProperty("kind", "info");
    redeemPointsButton->hide();
    connect(redeemPointsButton, &QPushButton::clicked, this, [this]() {
        // Redeem ALL available points; cap at outstanding total
        const int availPts      = m_customer.loyaltyPoints;
        const Money creditPerPt = Money::fromCents(m_loyaltyCentsPerPt);
        const Money maxCredit   = creditPerPt * availPts;
        const Money outstanding = Money::fromMajor(std::max(0.0, total));
        const Money applied     = maxCredit.cents() >= outstanding.cents()
                                      ? outstanding : maxCredit;
        m_pointsRedeemed = static_cast<int>(applied.cents() / creditPerPt.cents());
        const double newTotal = std::max(0.0, total - applied.toMajor());
        amountPaidSpin->setValue(newTotal);
        this->total = newTotal;
        loyaltyLabel->setText(
            QString("Redeemed %1 pts → %2 off  (remaining: %3 pts)")
                .arg(m_pointsRedeemed)
                .arg(formatMoney(applied))
                .arg(availPts - m_pointsRedeemed));
        redeemPointsButton->setEnabled(false);
        calculateChange();
    });
    amountLayout->addWidget(redeemPointsButton);

    // Store credit row — shown only when a customer is attached via setCustomer()
    creditAvailableLabel = new QLabel();
    creditAvailableLabel->setProperty("role", "banner");
    creditAvailableLabel->setProperty("kind", "info");
    creditAvailableLabel->hide();
    amountLayout->addWidget(creditAvailableLabel);

    applyCreditButton = new QPushButton("Apply Store Credit");
    applyCreditButton->setProperty("kind", "info");
    applyCreditButton->hide();
    connect(applyCreditButton, &QPushButton::clicked, this, [this]() {
        // Deduct up to the full available credit from the outstanding total.
        const Money available = m_customer.storeCredit - m_storeCreditUsed;
        const Money outstanding = Money::fromMajor(
            std::max(0.0, total - m_storeCreditUsed.toMajor()));
        m_storeCreditUsed = available.cents() >= outstanding.cents()
                                ? outstanding : available;
        const double newTotal = std::max(0.0, total - m_storeCreditUsed.toMajor());
        amountPaidSpin->setValue(newTotal);
        this->total = newTotal;
        creditAvailableLabel->setText(
            QString("Store credit applied: %1  (balance after: %2)")
                .arg(formatMoney(m_storeCreditUsed))
                .arg(formatMoney(m_customer.storeCredit - m_storeCreditUsed)));
        applyCreditButton->setEnabled(false);
        calculateChange();
    });
    amountLayout->addWidget(applyCreditButton);

    mainLayout->addWidget(amountGroup);

    mainLayout->addSpacing(20);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    cancelBtn = new QPushButton("Cancel");
    cancelBtn->setProperty("kind", "danger");
    cancelBtn->setProperty("textScale", "lg");
    buttonLayout->addWidget(cancelBtn);

    confirmBtn = new QPushButton("Confirm Payment");
    confirmBtn->setProperty("kind", "primary");
    confirmBtn->setProperty("textScale", "lg");
    confirmBtn->setDefault(true);
    buttonLayout->addWidget(confirmBtn);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(this->paymentMethodGroup, &QButtonGroup::idClicked,
            this, &PaymentDialog::onPaymentMethodChanged);
    connect(amountPaidSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &PaymentDialog::onAmountPaidChanged);
    connect(confirmBtn, &QPushButton::clicked,
            this, &PaymentDialog::onConfirmClicked);
    connect(cancelBtn,  &QPushButton::clicked,
            this, &QDialog::reject);

    // Initial calculation
    onPaymentMethodChanged();
    calculateChange();
}

void PaymentDialog::onPaymentMethodChanged()
{
    int selectedId = paymentMethodGroup->checkedId();

    switch (selectedId) {
    case 0: // Cash
        paymentMethod = "Cash";
        amountPaidSpin->setEnabled(true);
        amountPaidSpin->setValue(total);
        break;
    case 1: // Card
        paymentMethod = "Card";
        amountPaidSpin->setEnabled(false);
        amountPaidSpin->setValue(total);
        break;
    case 2: // Mobile Money
        paymentMethod = "Mobile Money";
        amountPaidSpin->setEnabled(false);
        amountPaidSpin->setValue(total);
        break;
    case 3: // Multiple
        paymentMethod = "Multiple";
        amountPaidSpin->setEnabled(true);
        amountPaidSpin->setValue(total);
        break;
    }

    // Reference field — method-specific. Cleared on every switch so a card ref
    // can't linger as an "M-Pesa code".
    const bool needRef = (selectedId != 0);
    referenceEdit->clear();
    referenceTitleLabel->setVisible(needRef);
    referenceEdit->setVisible(needRef);

    if (selectedId == 2) {            // Mobile Money (M-Pesa)
        referenceTitleLabel->setText("M-Pesa Code: *");
        referenceEdit->setPlaceholderText("10-char code, e.g. SLJ7X8K2P0");
        referenceEdit->setValidator(m_mpesaValidator);
        referenceEdit->setFocus();
    } else if (selectedId == 1) {     // Card
        referenceTitleLabel->setText("Card Ref:");
        referenceEdit->setPlaceholderText("Card transaction reference (optional)");
        referenceEdit->setValidator(nullptr);
    } else if (selectedId == 3) {     // Multiple
        referenceTitleLabel->setText("Reference:");
        referenceEdit->setPlaceholderText("Reference (optional)");
        referenceEdit->setValidator(nullptr);
    }

    // "Change" is only meaningful when the tendered amount can differ from the
    // total — cash or a split. Card/M-Pesa are charged the exact total, so the
    // perpetual "Change: 0.00" is just noise there.
    changeLabel->setVisible(selectedId == 0 || selectedId == 3);

    // M-Pesa STK push controls only in Mobile Money mode.
    const bool mobile = (selectedId == 2);
    m_phoneEdit->setVisible(mobile);
    m_stkButton->setVisible(mobile);
    if (!mobile) {
        m_stkStatus->hide();
    } else {
        m_phoneEdit->setEnabled(true);
        const bool avail = MpesaClient::isAvailable();
        m_stkButton->setEnabled(avail);
        m_stkButton->setToolTip(avail ? QString()
            : "Activate this till's licence to trigger M-Pesa prompts — "
              "you can still type the code manually.");
        if (m_hasCustomer && !m_customer.phone.isEmpty() && m_phoneEdit->text().isEmpty())
            m_phoneEdit->setText(m_customer.phone);
    }

    calculateChange();
}

void PaymentDialog::onAmountPaidChanged()
{
    calculateChange();
}

void PaymentDialog::calculateChange()
{
    amountPaid = amountPaidSpin->value();

    // Round BOTH operands to cents before subtracting so that values that
    // display identically (e.g. 2029.66) are treated as identical.
    double paidCents  = std::round(amountPaid * 100.0) / 100.0;
    double totalCents = std::round(total      * 100.0) / 100.0;

    change = std::round((paidCents - totalCents) * 100.0) / 100.0;

    if (change < 0) {
        changeLabel->setText(
            QString("Change: %1  Insufficient").arg(formatMoney(Money::fromMajor(change))));
        setStyleProperty(changeLabel, "kind", "danger");
    } else {
        changeLabel->setText("Change: " + formatMoney(Money::fromMajor(change)));
        setStyleProperty(changeLabel, "kind", "success");
    }

    // Under-payment can never be confirmed
    confirmBtn->setEnabled(change >= 0);
}

void PaymentDialog::onConfirmClicked()
{
    calculateChange();

    if (change < 0)
        return; // button is disabled in this state; belt-and-braces

    if (paymentMethod == "Mobile Money") {
        const QString code = referenceEdit->text().trimmed().toUpper();
        if (!kMpesaCode.match(code).hasMatch()) {
            QMessageBox::warning(this, "M-Pesa Code Required",
                code.isEmpty()
                    ? "Enter the 10-character M-Pesa confirmation code from the "
                      "customer's payment SMS (e.g. SLJ7X8K2P0)."
                    : "That doesn't look like an M-Pesa code — it should be 10 "
                      "letters/numbers (e.g. SLJ7X8K2P0).");
            referenceEdit->setFocus();
            return;
        }
        referenceEdit->setText(code);   // persist the normalized (upper-case) code
    }

    accept();
}

void PaymentDialog::onStkPushClicked()
{
    QString digits = m_phoneEdit->text();
    digits.remove(QRegularExpression("\\D"));
    if (digits.length() < 9) {
        setStyleProperty(m_stkStatus, "kind", "danger");
        m_stkStatus->setText("Enter the customer's phone number (e.g. 0712345678).");
        m_stkStatus->show();
        m_phoneEdit->setFocus();
        return;
    }
    m_stkButton->setEnabled(false);
    m_phoneEdit->setEnabled(false);
    m_stkStatus->show();
    // total is the outstanding amount (already reduced by any store credit /
    // loyalty applied above). M-Pesa is charged whole shillings.
    m_mpesa->requestPayment(m_phoneEdit->text().trimmed(),
                            Money::fromMajor(total), "POS");
}

void PaymentDialog::setCustomer(const Customer &customer)
{
    m_customer    = customer;
    m_hasCustomer = customer.id > 0;
    if (!m_hasCustomer) return;

    if (customer.loyaltyPoints > 0) {
        loyaltyLabel->setText(
            QString("Customer: %1  |  Loyalty Points: %2 (worth %3)")
                .arg(customer.name)
                .arg(customer.loyaltyPoints)
                .arg(formatMoney(Money::fromCents(
                    static_cast<qint64>(customer.loyaltyPoints) * m_loyaltyCentsPerPt))));
        loyaltyLabel->show();
        redeemPointsButton->show();
        redeemPointsButton->setEnabled(true);
    }

    if (!customer.storeCredit.isZero()) {
        creditAvailableLabel->setText(
            QString("Customer: %1  |  Store Credit Available: %2")
                .arg(customer.name, formatMoney(customer.storeCredit)));
        creditAvailableLabel->show();
        applyCreditButton->show();
        applyCreditButton->setEnabled(true);
    }
}

void PaymentDialog::setLoyaltyCentsPerPoint(int centsPerPoint)
{
    if (centsPerPoint > 0)
        m_loyaltyCentsPerPt = centsPerPoint;
}

QString PaymentDialog::getPaymentMethod()   const { return paymentMethod; }
Money   PaymentDialog::getAmountPaid()      const { return Money::fromMajor(amountPaid); }
Money   PaymentDialog::getChange()          const { return Money::fromMajor(change);     }
Money   PaymentDialog::getStoreCreditUsed()       const { return m_storeCreditUsed; }
int     PaymentDialog::getCustomerId()            const { return m_hasCustomer ? m_customer.id : 0; }
int     PaymentDialog::getLoyaltyPointsRedeemed() const { return m_pointsRedeemed; }
QString PaymentDialog::getReferenceNumber()       const { return referenceEdit->text().trimmed(); }
