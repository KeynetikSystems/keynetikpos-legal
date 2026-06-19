// =============================================================================
// paymentdialog.cpp — Implementation of PaymentDialog (see paymentdialog.h).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Each method is a checkbox revealing a detail panel; recompute() sums the
//    active panels and drives the Remaining/Change line and the Confirm button.
//  - Ticking a method prefills its amount with whatever is still outstanding, so
//    the common single-tender case is one click; for a split the cashier edits
//    the amounts and the remainder follows.
//  - M-Pesa: the STK push charges the M-Pesa panel's amount (whole shillings);
//    a confirmed push fills the receipt code, which onConfirmClicked validates.
// =============================================================================
#include "paymentdialog.h"
#include "appstyle.h"
#include "cart.h"          // formatMoney(), currencySymbol()
#include "mpesaclient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {
// M-Pesa confirmation codes are 10 alphanumeric characters (e.g. SLJ7X8K2P0).
const QRegularExpression kMpesaCode("^[A-Z0-9]{10}$");
constexpr double kEps = 0.0001;

QDoubleSpinBox *makeAmount(const QString &prefix)
{
    auto *s = new QDoubleSpinBox();
    s->setRange(0.0, 9999999.99);
    s->setDecimals(2);
    s->setPrefix(prefix + " ");
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    s->setProperty("textScale", "lg");
    s->setProperty("role", "control");
    return s;
}
} // namespace

PaymentDialog::PaymentDialog(Money totalAmount, QWidget *parent)
    : QDialog(parent)
    , total(totalAmount.toMajor())
    , amountPaid(0.0)
    , change(0.0)
{
    setWindowTitle("Process Payment");
    setModal(true);
    setupUI();
    recompute();
}

PaymentDialog::~PaymentDialog() {}

double PaymentDialog::dueMajor() const { return std::max(0.0, total); }

double PaymentDialog::enteredMajor() const
{
    double sum = 0.0;
    if (cashCheck->isChecked())  sum += cashAmount->value();
    if (cardCheck->isChecked())  sum += cardAmount->value();
    if (mpesaCheck->isChecked()) sum += mpesaAmount->value();
    return sum;
}

void PaymentDialog::setupUI()
{
    setMinimumWidth(460);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // ── Amount due banner ────────────────────────────────────────────────────
    totalLabel = new QLabel();
    totalLabel->setProperty("role", "banner");
    totalLabel->setProperty("kind", "success");
    totalLabel->setProperty("textScale", "2xl");
    totalLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(totalLabel);

    // ── Loyalty + store credit (shown only when a customer is attached) ───────
    loyaltyLabel = new QLabel();
    loyaltyLabel->setProperty("role", "banner");
    loyaltyLabel->setProperty("kind", "secondary");
    loyaltyLabel->hide();
    mainLayout->addWidget(loyaltyLabel);

    redeemPointsButton = new QPushButton("Redeem Loyalty Points");
    redeemPointsButton->setProperty("kind", "info");
    redeemPointsButton->hide();
    connect(redeemPointsButton, &QPushButton::clicked, this, [this]() {
        const int availPts      = m_customer.loyaltyPoints;
        const Money creditPerPt = Money::fromCents(m_loyaltyCentsPerPt);
        const Money maxCredit   = creditPerPt * availPts;
        const Money outstanding = Money::fromMajor(dueMajor());
        const Money applied     = maxCredit.cents() >= outstanding.cents()
                                      ? outstanding : maxCredit;
        m_pointsRedeemed = creditPerPt.cents() > 0
                               ? static_cast<int>(applied.cents() / creditPerPt.cents()) : 0;
        total = std::max(0.0, total - applied.toMajor());
        loyaltyLabel->setText(QString("Redeemed %1 pts -> %2 off  (remaining: %3 pts)")
            .arg(m_pointsRedeemed).arg(formatMoney(applied)).arg(availPts - m_pointsRedeemed));
        redeemPointsButton->setEnabled(false);
        recompute();
    });
    mainLayout->addWidget(redeemPointsButton);

    creditAvailableLabel = new QLabel();
    creditAvailableLabel->setProperty("role", "banner");
    creditAvailableLabel->setProperty("kind", "info");
    creditAvailableLabel->hide();
    mainLayout->addWidget(creditAvailableLabel);

    applyCreditButton = new QPushButton("Apply Store Credit");
    applyCreditButton->setProperty("kind", "info");
    applyCreditButton->hide();
    connect(applyCreditButton, &QPushButton::clicked, this, [this]() {
        const Money available   = m_customer.storeCredit - m_storeCreditUsed;
        const Money outstanding = Money::fromMajor(dueMajor());
        const Money add = available.cents() >= outstanding.cents() ? outstanding : available;
        m_storeCreditUsed += add;
        total = std::max(0.0, total - add.toMajor());
        creditAvailableLabel->setText(QString("Store credit applied: %1  (balance after: %2)")
            .arg(formatMoney(m_storeCreditUsed))
            .arg(formatMoney(m_customer.storeCredit - m_storeCreditUsed)));
        applyCreditButton->setEnabled(false);
        recompute();
    });
    mainLayout->addWidget(applyCreditButton);

    // ── Method selection (checkboxes — any combination) ──────────────────────
    QGroupBox *methodGroup = new QGroupBox("Select Payment Method(s)");
    QVBoxLayout *methodLayout = new QVBoxLayout(methodGroup);
    cashCheck  = new QCheckBox("Cash");
    cardCheck  = new QCheckBox("Credit/Debit Card");
    mpesaCheck = new QCheckBox("Mobile Money (M-Pesa)");
    for (QCheckBox *c : { cashCheck, cardCheck, mpesaCheck }) {
        c->setProperty("textScale", "lg");
        methodLayout->addWidget(c);
    }
    mainLayout->addWidget(methodGroup);

    // ── Cash panel ────────────────────────────────────────────────────────────
    cashBox = new QGroupBox("Cash");
    {
        QFormLayout *f = new QFormLayout(cashBox);
        cashAmount = makeAmount(currencySymbol());
        connect(cashAmount, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, &PaymentDialog::recompute);
        f->addRow("Cash given:", cashAmount);
    }
    cashBox->hide();
    mainLayout->addWidget(cashBox);

    // ── Card panel ────────────────────────────────────────────────────────────
    cardBox = new QGroupBox("Credit/Debit Card");
    {
        QFormLayout *f = new QFormLayout(cardBox);
        cardAmount = makeAmount(currencySymbol());
        connect(cardAmount, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, &PaymentDialog::recompute);
        f->addRow("Card amount:", cardAmount);
        cardRef = new QLineEdit();
        cardRef->setPlaceholderText("Card reference (optional)");
        f->addRow("Card ref:", cardRef);
    }
    cardBox->hide();
    mainLayout->addWidget(cardBox);

    // ── M-Pesa panel ──────────────────────────────────────────────────────────
    mpesaBox = new QGroupBox("Mobile Money (M-Pesa)");
    {
        QFormLayout *f = new QFormLayout(mpesaBox);
        mpesaAmount = makeAmount(currencySymbol());
        connect(mpesaAmount, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, &PaymentDialog::recompute);
        f->addRow("M-Pesa amount:", mpesaAmount);

        m_phoneEdit = new QLineEdit();
        m_phoneEdit->setPlaceholderText("Customer phone e.g. 0712345678");
        f->addRow("Phone:", m_phoneEdit);

        QHBoxLayout *codeRow = new QHBoxLayout();
        referenceEdit = new QLineEdit();
        referenceEdit->setPlaceholderText("M-Pesa code (10 chars) — or use STK");
        m_mpesaValidator = new QRegularExpressionValidator(
            QRegularExpression("[A-Za-z0-9]{0,10}"), this);
        referenceEdit->setValidator(m_mpesaValidator);
        connect(referenceEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
            const QString up = t.toUpper();
            if (up != t) {
                const int pos = referenceEdit->cursorPosition();
                QSignalBlocker b(referenceEdit);
                referenceEdit->setText(up);
                referenceEdit->setCursorPosition(pos);
            }
        });
        codeRow->addWidget(referenceEdit, 1);
        m_stkButton = new QPushButton("Send STK Prompt");
        m_stkButton->setProperty("kind", "info");
        connect(m_stkButton, &QPushButton::clicked, this, &PaymentDialog::onStkPushClicked);
        codeRow->addWidget(m_stkButton);
        f->addRow("M-Pesa code:", codeRow);

        m_stkStatus = new QLabel();
        m_stkStatus->setWordWrap(true);
        m_stkStatus->hide();
        f->addRow("", m_stkStatus);
    }
    mpesaBox->hide();
    mainLayout->addWidget(mpesaBox);

    m_mpesa = new MpesaClient(this);
    connect(m_mpesa, &MpesaClient::statusChanged, this, [this](const QString &m) {
        setStyleProperty(m_stkStatus, "kind", "info"); m_stkStatus->setText(m); m_stkStatus->show();
    });
    connect(m_mpesa, &MpesaClient::promptSent, this, [this](const QString &m) { m_stkStatus->setText(m); });
    connect(m_mpesa, &MpesaClient::paymentConfirmed, this, [this](const QString &receipt) {
        referenceEdit->setText(receipt);
        setStyleProperty(m_stkStatus, "kind", "success");
        m_stkStatus->setText("M-Pesa received — code " + receipt);
        m_stkButton->setEnabled(true);
        m_phoneEdit->setEnabled(true);
        recompute();
    });
    connect(m_mpesa, &MpesaClient::paymentFailed, this, [this](const QString &reason) {
        setStyleProperty(m_stkStatus, "kind", "danger");
        m_stkStatus->setText(reason + "  You can also type the code manually.");
        m_stkStatus->show();
        m_stkButton->setEnabled(MpesaClient::isAvailable());
        m_phoneEdit->setEnabled(true);
    });

    // Checkbox wiring — show/hide the panel, prefill the outstanding amount.
    auto wire = [this](QCheckBox *chk, QGroupBox *box, QDoubleSpinBox *amt) {
        connect(chk, &QCheckBox::toggled, this, [this, box, amt](bool on) {
            box->setVisible(on);
            if (on) {
                if (amt->value() < kEps)
                    amt->setValue(std::max(0.0, dueMajor() - enteredMajor()));
            } else {
                amt->setValue(0.0);
            }
            recompute();
        });
    };
    wire(cashCheck, cashBox, cashAmount);
    wire(cardCheck, cardBox, cardAmount);
    wire(mpesaCheck, mpesaBox, mpesaAmount);

    // ── Summary ────────────────────────────────────────────────────────────────
    paidLabel = new QLabel();
    paidLabel->setProperty("textScale", "lg");
    mainLayout->addWidget(paidLabel);

    changeLabel = new QLabel();
    changeLabel->setProperty("kind", "success");
    changeLabel->setProperty("textScale", "xl");
    changeLabel->setProperty("bold", "true");
    mainLayout->addWidget(changeLabel);

    // ── Buttons ──────────────────────────────────────────────────────────────
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    cancelBtn = new QPushButton("Cancel");
    cancelBtn->setProperty("kind", "danger");
    cancelBtn->setProperty("textScale", "lg");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);
    confirmBtn = new QPushButton("Confirm Payment");
    confirmBtn->setProperty("kind", "primary");
    confirmBtn->setProperty("textScale", "lg");
    confirmBtn->setDefault(true);
    connect(confirmBtn, &QPushButton::clicked, this, &PaymentDialog::onConfirmClicked);
    buttonLayout->addWidget(confirmBtn);
    mainLayout->addLayout(buttonLayout);

    // Default to Cash ticked (the common case).
    cashCheck->setChecked(true);
}

void PaymentDialog::onMethodToggled() { recompute(); }   // (wiring done via lambdas)

void PaymentDialog::recompute()
{
    amountPaid = enteredMajor();
    const double due = dueMajor();
    change = amountPaid - due;

    totalLabel->setText("Amount Due: " + formatMoney(Money::fromMajor(due)));
    paidLabel->setText("Entered: " + formatMoney(Money::fromMajor(amountPaid)));

    if (amountPaid + kEps < due) {
        setStyleProperty(changeLabel, "kind", "danger");
        changeLabel->setText("Remaining: " + formatMoney(Money::fromMajor(due - amountPaid)));
    } else {
        setStyleProperty(changeLabel, "kind", "success");
        changeLabel->setText("Change: " + formatMoney(Money::fromMajor(std::max(0.0, change))));
    }

    // M-Pesa STK availability (licensed tills only).
    if (m_stkButton) {
        const bool avail = MpesaClient::isAvailable();
        m_stkButton->setEnabled(avail && mpesaCheck->isChecked());
        m_stkButton->setToolTip(avail ? QString()
            : "Activate this till's licence to trigger M-Pesa prompts — "
              "you can still type the code manually.");
    }

    confirmBtn->setEnabled(amountPaid + kEps >= due);
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
    if (mpesaAmount->value() < 1.0) {
        setStyleProperty(m_stkStatus, "kind", "danger");
        m_stkStatus->setText("Enter the M-Pesa amount (at least KSh 1) first.");
        m_stkStatus->show();
        return;
    }
    m_stkButton->setEnabled(false);
    m_phoneEdit->setEnabled(false);
    m_stkStatus->show();
    m_mpesa->requestPayment(m_phoneEdit->text().trimmed(),
                            Money::fromMajor(mpesaAmount->value()), "POS");
}

void PaymentDialog::onConfirmClicked()
{
    const double due = dueMajor();
    const double entered = enteredMajor();

    if (entered + kEps < due) {
        QMessageBox::warning(this, "Insufficient Payment",
            QString("Entered %1 but %2 is due.")
                .arg(formatMoney(Money::fromMajor(entered)),
                     formatMoney(Money::fromMajor(due))));
        return;
    }
    // Need at least one tender unless the whole bill was cleared by credit/points.
    if (entered < kEps && due > kEps) {
        QMessageBox::warning(this, "No Payment Method",
                             "Tick a payment method and enter an amount.");
        return;
    }
    // M-Pesa portion requires a valid confirmation code.
    if (mpesaCheck->isChecked() && mpesaAmount->value() > kEps) {
        const QString code = referenceEdit->text().trimmed().toUpper();
        if (!kMpesaCode.match(code).hasMatch()) {
            QMessageBox::warning(this, "M-Pesa Code Required",
                code.isEmpty()
                    ? "Enter the 10-character M-Pesa code (or use Send STK Prompt)."
                    : "That doesn't look like an M-Pesa code — 10 letters/numbers.");
            referenceEdit->setFocus();
            return;
        }
        referenceEdit->setText(code);
    }
    accept();
}

void PaymentDialog::setCustomer(const Customer &customer)
{
    m_customer    = customer;
    m_hasCustomer = customer.id > 0;
    if (!m_hasCustomer) return;

    if (customer.loyaltyPoints > 0) {
        loyaltyLabel->setText(QString("Customer: %1  |  Loyalty Points: %2 (worth %3)")
            .arg(customer.name).arg(customer.loyaltyPoints)
            .arg(formatMoney(Money::fromCents(
                static_cast<qint64>(customer.loyaltyPoints) * m_loyaltyCentsPerPt))));
        loyaltyLabel->show();
        redeemPointsButton->show();
        redeemPointsButton->setEnabled(true);
    }
    if (!customer.storeCredit.isZero()) {
        creditAvailableLabel->setText(QString("Customer: %1  |  Store Credit Available: %2")
            .arg(customer.name, formatMoney(customer.storeCredit)));
        creditAvailableLabel->show();
        applyCreditButton->show();
        applyCreditButton->setEnabled(true);
    }
    // Prefill the M-Pesa phone from the customer record.
    if (!customer.phone.isEmpty())
        m_phoneEdit->setText(customer.phone);
}

void PaymentDialog::setLoyaltyCentsPerPoint(int centsPerPoint)
{
    if (centsPerPoint > 0) m_loyaltyCentsPerPt = centsPerPoint;
}

QString PaymentDialog::getPaymentMethod() const
{
    QStringList parts;
    if (cashCheck->isChecked()  && cashAmount->value()  > kEps) parts << "Cash";
    if (cardCheck->isChecked()  && cardAmount->value()  > kEps) parts << "Card";
    if (mpesaCheck->isChecked() && mpesaAmount->value() > kEps) parts << "Mobile Money";
    if (parts.isEmpty())
        return m_storeCreditUsed.isZero() ? QStringLiteral("Cash") : QStringLiteral("Store Credit");
    return parts.join(" + ");
}

Money   PaymentDialog::getAmountPaid()  const { return Money::fromMajor(enteredMajor()); }
Money   PaymentDialog::getChange()      const { return Money::fromMajor(std::max(0.0, enteredMajor() - dueMajor())); }

QString PaymentDialog::getReferenceNumber() const
{
    QStringList parts;
    if (mpesaCheck->isChecked() && mpesaAmount->value() > kEps
        && !referenceEdit->text().trimmed().isEmpty())
        parts << "M-Pesa " + referenceEdit->text().trimmed();
    if (cardCheck->isChecked() && cardAmount->value() > kEps
        && !cardRef->text().trimmed().isEmpty())
        parts << "Card " + cardRef->text().trimmed();
    return parts.join("; ");
}

QVector<SalePayment> PaymentDialog::getTenders() const
{
    QVector<SalePayment> tenders;
    const double due = dueMajor();

    double others = 0.0;   // exact (non-cash) tenders
    if (cardCheck->isChecked()  && cardAmount->value()  > kEps) others += cardAmount->value();
    if (mpesaCheck->isChecked() && mpesaAmount->value() > kEps) others += mpesaAmount->value();

    if (cardCheck->isChecked() && cardAmount->value() > kEps)
        tenders.append({ QStringLiteral("Card"),
                         Money::fromMajor(cardAmount->value()),
                         cardRef->text().trimmed() });
    if (mpesaCheck->isChecked() && mpesaAmount->value() > kEps)
        tenders.append({ QStringLiteral("Mobile Money"),
                         Money::fromMajor(mpesaAmount->value()),
                         referenceEdit->text().trimmed() });
    if (cashCheck->isChecked()) {
        // Cash absorbs change, so its net contribution is the remainder.
        const double cashNet = std::max(0.0, due - others);
        if (cashNet > kEps)
            tenders.append({ QStringLiteral("Cash"), Money::fromMajor(cashNet), QString() });
    }
    return tenders;
}

Money PaymentDialog::getStoreCreditUsed()       const { return m_storeCreditUsed; }
int   PaymentDialog::getCustomerId()            const { return m_hasCustomer ? m_customer.id : 0; }
int   PaymentDialog::getLoyaltyPointsRedeemed() const { return m_pointsRedeemed; }
