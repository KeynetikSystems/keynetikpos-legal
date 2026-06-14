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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <cmath>

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

    cashRadio = addMethodRadio("💵 Cash", "Pay with cash", 0);
    cashRadio->setChecked(true);
    cardRadio     = addMethodRadio("💳 Credit/Debit Card", "Pay by card", 1);
    mobileRadio   = addMethodRadio("📱 Mobile Money (M-Pesa)",
                                   "Pay with M-Pesa mobile money", 2);
    multipleRadio = addMethodRadio("🔀 Multiple Payment Methods",
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
    referenceEdit->setPlaceholderText("M-Pesa / card transaction code");
    referenceEdit->setProperty("textScale", "md");
    referenceEdit->setAccessibleName("Transaction reference number");
    refLayout->addWidget(referenceEdit);
    amountLayout->addLayout(refLayout);

    changeLabel = new QLabel("Change: " + formatMoney(Money()));
    changeLabel->setProperty("kind", "success");
    changeLabel->setProperty("textScale", "xl");
    changeLabel->setProperty("bold", "true");
    amountLayout->addWidget(changeLabel);

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

    // Reference number only applies to non-cash payments
    const bool nonCash = (selectedId != 0);
    referenceTitleLabel->setEnabled(nonCash);
    referenceEdit->setEnabled(nonCash);
    if (!nonCash)
        referenceEdit->clear();

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
            QString("Change: %1  ⚠ Insufficient").arg(formatMoney(Money::fromMajor(change))));
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

    if (paymentMethod == "Mobile Money" &&
        referenceEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Reference Required",
                             "Please enter the M-Pesa transaction code.");
        referenceEdit->setFocus();
        return;
    }

    accept();
}

QString PaymentDialog::getPaymentMethod()   const { return paymentMethod; }
Money   PaymentDialog::getAmountPaid()      const { return Money::fromMajor(amountPaid); }
Money   PaymentDialog::getChange()          const { return Money::fromMajor(change);     }
QString PaymentDialog::getReferenceNumber() const
{
    return referenceEdit->text().trimmed();
}
