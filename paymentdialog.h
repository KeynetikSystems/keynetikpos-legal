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

class PaymentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaymentDialog(Money totalAmount, QWidget *parent = nullptr);
    ~PaymentDialog();

    QString getPaymentMethod() const;
    Money getAmountPaid() const;
    Money getChange() const;
    QString getReferenceNumber() const;

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
    QLabel *changeLabel;
    QPushButton *confirmBtn;
    QPushButton *cancelBtn;

    // Data
    double total;
    double amountPaid;
    double change;
    QString paymentMethod;
};

#endif // PAYMENTDIALOG_H
