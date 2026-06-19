// =============================================================================
// paymentdialog.h — PaymentDialog: the "take payment" step of checkout
// -----------------------------------------------------------------------------
// WHAT: Modal dialog to choose Cash / Card / Mobile / Multiple, enter the
//       amount tendered (validated QDoubleSpinBox — no free-text parsing),
//       capture a transaction reference for non-cash methods, and see change
//       due live.
// HOW:  Radio buttons in a QButtonGroup; calculateChange() re-runs on every
//       amount change and Confirm is disabled while amountPaid < total.
//       Mobile Money additionally requires a non-empty reference (M-Pesa
//       always issues a code). Results are exposed via getters that
//       MainWindow::onCheckout() reads after exec() returns Accepted.
// WHY:  Computing change in the dialog — before the sale is recorded —
//       prevents a half-recorded sale when the cashier mistypes; the sale only
//       hits the database after payment details are confirmed.
// =============================================================================
#ifndef PAYMENTDIALOG_H
#define PAYMENTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>

#include "money.h"
#include "database.h"   // Customer

class QRegularExpressionValidator;

class PaymentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaymentDialog(Money totalAmount, QWidget *parent = nullptr);
    ~PaymentDialog();

    // Optionally attach a customer before exec() — shows their store credit
    // balance and lets the cashier apply it against the total.
    void setCustomer(const Customer &customer);
    // Pass the configurable redemption rate (cents per point) before exec().
    // Defaults to 10 (= KSh 0.10 per point).
    void setLoyaltyCentsPerPoint(int centsPerPoint);

    QString getPaymentMethod() const;
    Money getAmountPaid() const;
    Money getChange() const;
    QString getReferenceNumber() const;
    // Non-zero only when the cashier chose to apply store credit.
    Money getStoreCreditUsed() const;
    int   getCustomerId() const;
    // Points burned via "Redeem Points" this checkout (0 if not used).
    int   getLoyaltyPointsRedeemed() const;

private slots:
    void onPaymentMethodChanged();
    void onAmountPaidChanged();
    void onConfirmClicked();

private:
    void setupUI();
    void calculateChange();

    // UI Components
    QLabel *totalLabel;
    QButtonGroup *paymentMethodGroup;
    QRadioButton *cashRadio;
    QRadioButton *cardRadio;
    QRadioButton *mobileRadio;
    QRadioButton *multipleRadio;
    QDoubleSpinBox *amountPaidSpin;
    QLabel *referenceTitleLabel;
    QLineEdit *referenceEdit;
    QRegularExpressionValidator *m_mpesaValidator { nullptr };  // [A-Za-z0-9]{0,10}
    QLabel *changeLabel;
    QPushButton *confirmBtn;
    QPushButton *cancelBtn;

    // Data
    double total;
    double amountPaid;
    double change;
    QString paymentMethod;

    // Customer store-credit widgets (hidden when no customer is set)
    QLabel       *creditAvailableLabel { nullptr };
    QPushButton  *applyCreditButton    { nullptr };

    // Loyalty redemption widgets (hidden when no customer / no points)
    QLabel       *loyaltyLabel         { nullptr };
    QPushButton  *redeemPointsButton   { nullptr };

    // Customer state
    Customer  m_customer;
    bool      m_hasCustomer        { false };
    Money     m_storeCreditUsed;
    int       m_pointsRedeemed     { 0 };   // points burned this checkout
    int       m_loyaltyCentsPerPt  { 10 };  // configurable redemption rate
};

#endif // PAYMENTDIALOG_H
