// =============================================================================
// discountdialog.cpp — Implementation of DiscountDialog (see discountdialog.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - calculateDiscount() re-runs on every input change to keep the preview
//    (discount amount + new total) live, and validates percentage <= 100 and
//    fixed <= subtotal before Apply is accepted.
//  - The PIN check happens in MainWindow BEFORE this dialog opens; the
//    accepted amount/reason are stored on the active Cart by the caller.
// =============================================================================
#include "discountdialog.h"
#include "cart.h"          // formatMoney(), currencySymbol()
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>

DiscountDialog::DiscountDialog(double subtotal, QWidget *parent)
    : QDialog(parent)
    , subtotal(subtotal)
    , discountAmount(0.0)
{
    setWindowTitle("Apply Discount");
    setModal(true);
    setupUI();
}

DiscountDialog::~DiscountDialog()
{
}

void DiscountDialog::setupUI()
{
    resize(400, 350);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Subtotal display
    QLabel *subtotalLabel = new QLabel("Subtotal: " + formatMoney(subtotal));
    subtotalLabel->setProperty("role", "banner");
    subtotalLabel->setProperty("kind", "info");
    subtotalLabel->setProperty("textScale", "xl");
    subtotalLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtotalLabel);

    mainLayout->addSpacing(10);

    // Discount type selection
    QGroupBox *typeGroup = new QGroupBox("Discount Type");
    QVBoxLayout *typeLayout = new QVBoxLayout(typeGroup);

    discountTypeGroup = new QButtonGroup(this);

    percentageRadio = new QRadioButton("Percentage (%)");
    percentageRadio->setChecked(true);
    percentageRadio->setProperty("textScale", "md");
    percentageRadio->setAccessibleName("Discount as a percentage");
    discountTypeGroup->addButton(percentageRadio, 0);
    typeLayout->addWidget(percentageRadio);

    fixedRadio = new QRadioButton(QString("Fixed Amount (%1)").arg(currencySymbol()));
    fixedRadio->setProperty("textScale", "md");
    fixedRadio->setAccessibleName("Discount as a fixed amount");
    discountTypeGroup->addButton(fixedRadio, 1);
    typeLayout->addWidget(fixedRadio);

    mainLayout->addWidget(typeGroup);

    // Discount value input
    QGroupBox *valueGroup = new QGroupBox("Discount Value");
    QVBoxLayout *valueLayout = new QVBoxLayout(valueGroup);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel("Value:"));
    discountValueEdit = new QLineEdit();
    discountValueEdit->setPlaceholderText("Enter discount value");
    discountValueEdit->setText("0");
    discountValueEdit->setProperty("textScale", "lg");
    discountValueEdit->setAccessibleName("Discount value");
    inputLayout->addWidget(discountValueEdit);
    valueLayout->addLayout(inputLayout);

    mainLayout->addWidget(valueGroup);

    // Reason
    QHBoxLayout *reasonLayout = new QHBoxLayout();
    reasonLayout->addWidget(new QLabel("Reason:"));
    reasonEdit = new QLineEdit();
    reasonEdit->setPlaceholderText("Optional: Enter reason for discount");
    reasonLayout->addWidget(reasonEdit);
    mainLayout->addLayout(reasonLayout);

    mainLayout->addSpacing(10);

    // Discount summary
    discountAmountLabel = new QLabel("Discount Amount: " + formatMoney(0));
    discountAmountLabel->setProperty("kind", "warning");
    discountAmountLabel->setProperty("textScale", "lg");
    discountAmountLabel->setProperty("bold", "true");
    mainLayout->addWidget(discountAmountLabel);

    newTotalLabel = new QLabel("New Total: " + formatMoney(subtotal));
    newTotalLabel->setProperty("kind", "success");
    newTotalLabel->setProperty("textScale", "xl");
    newTotalLabel->setProperty("bold", "true");
    mainLayout->addWidget(newTotalLabel);

    mainLayout->addSpacing(10);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    cancelBtn = new QPushButton("Cancel");
    buttonLayout->addWidget(cancelBtn);

    applyBtn = new QPushButton("Apply Discount");
    applyBtn->setProperty("kind", "primary");
    applyBtn->setDefault(true);
    buttonLayout->addWidget(applyBtn);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(discountTypeGroup, &QButtonGroup::idClicked, this, &DiscountDialog::onDiscountTypeChanged);
    connect(discountValueEdit, &QLineEdit::textChanged, this, &DiscountDialog::onDiscountValueChanged);
    connect(applyBtn, &QPushButton::clicked, this, &DiscountDialog::onApplyClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    calculateDiscount();
}

void DiscountDialog::onDiscountTypeChanged()
{
    calculateDiscount();
}

void DiscountDialog::onDiscountValueChanged()
{
    calculateDiscount();
}

void DiscountDialog::calculateDiscount()
{
    bool ok;
    double value = discountValueEdit->text().toDouble(&ok);

    if (!ok) {
        value = 0.0;
    }

    int discountType = discountTypeGroup->checkedId();

    if (discountType == 0) { // Percentage
        if (value > 100) value = 100;
        if (value < 0) value = 0;
        discountAmount = subtotal * (value / 100.0);
    } else { // Fixed amount
        if (value > subtotal) value = subtotal;
        if (value < 0) value = 0;
        discountAmount = value;
    }

    double newTotal = subtotal - discountAmount;

    discountAmountLabel->setText("Discount Amount: " + formatMoney(discountAmount));
    newTotalLabel->setText("New Total: " + formatMoney(newTotal));
}

void DiscountDialog::onApplyClicked()
{
    discountReason = reasonEdit->text().trimmed();

    if (discountAmount > 0 && discountReason.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "No Reason Provided",
            "You haven't provided a reason for this discount. Continue anyway?",
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::No) {
            return;
        }
    }

    accept();
}

double DiscountDialog::getDiscountAmount() const
{
    return discountAmount;
}

QString DiscountDialog::getDiscountReason() const
{
    return discountReason;
}
