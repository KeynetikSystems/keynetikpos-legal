// =============================================================================
// paymentdialog.h — PaymentDialog: the "take payment" step of checkout
// -----------------------------------------------------------------------------
// WHAT: Modal dialog to settle a sale with ONE OR MORE tenders. Each method
//       (Cash / Card / Mobile Money) is a checkbox; ticking it reveals a detail
//       panel with that method's amount and any extra fields (M-Pesa phone +
//       STK push, card reference). A live summary shows amount entered vs. the
//       outstanding total, with the remaining balance or change. Store credit
//       and loyalty points (if a customer is attached) reduce the total first.
// HOW:  recompute() sums the active panels' amounts on every change; Confirm is
//       enabled only once the entered total covers the due amount and each
//       active method passes its own validation (e.g. an M-Pesa code present).
//       Results are exposed via getters MainWindow::onCheckout() reads after
//       exec() returns Accepted; a split tender is reported as a combined
//       method string ("Cash + M-Pesa") with the entered total and change.
// WHY:  Real tills routinely split a bill across cash + M-Pesa; modelling each
//       tender explicitly (rather than one amount + one method) makes change,
//       references and the M-Pesa STK amount correct per method.
// =============================================================================
#ifndef PAYMENTDIALOG_H
#define PAYMENTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>

#include "money.h"
#include "database.h"   // Customer

class QRegularExpressionValidator;
class MpesaClient;

class PaymentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PaymentDialog(Money totalAmount, QWidget *parent = nullptr);
    ~PaymentDialog();

    // Optionally attach a customer before exec() — shows store credit / loyalty
    // and lets the cashier apply them against the total.
    void setCustomer(const Customer &customer);
    void setLoyaltyCentsPerPoint(int centsPerPoint);

    QString getPaymentMethod() const;     // "Cash", "Mobile Money", "Cash + Card", …
    Money   getAmountPaid() const;        // total entered across all tenders
    Money   getChange() const;            // entered - due (cash overpay), else 0
    QString getReferenceNumber() const;   // combined method references (M-Pesa code, …)
    Money   getStoreCreditUsed() const;
    int     getCustomerId() const;
    int     getLoyaltyPointsRedeemed() const;
    // Per-tender breakdown (net contributions summing to the amount due) for
    // the sale_payments table. Cash absorbs any change; card/M-Pesa are exact.
    QVector<SalePayment> getTenders() const;

private slots:
    void recompute();             // re-sum tenders, refresh summary + Confirm state
    void onMethodToggled();       // show/hide a method panel, prefill its amount
    void onConfirmClicked();
    void onStkPushClicked();      // M-Pesa STK push for the M-Pesa portion

private:
    void   setupUI();
    double dueMajor() const;      // outstanding total (after credit/loyalty)
    double enteredMajor() const;  // sum of active tenders

    // ── Method selection ────────────────────────────────────────────────────
    QCheckBox *cashCheck   { nullptr };
    QCheckBox *cardCheck   { nullptr };
    QCheckBox *mpesaCheck  { nullptr };

    // ── Per-method detail panels (hidden until the method is ticked) ──────────
    QGroupBox      *cashBox     { nullptr };
    QDoubleSpinBox *cashAmount  { nullptr };

    QGroupBox      *cardBox     { nullptr };
    QDoubleSpinBox *cardAmount  { nullptr };
    QLineEdit      *cardRef     { nullptr };

    QGroupBox      *mpesaBox    { nullptr };
    QDoubleSpinBox *mpesaAmount { nullptr };
    QLineEdit      *m_phoneEdit { nullptr };
    QPushButton    *m_stkButton { nullptr };
    QLabel         *m_stkStatus { nullptr };
    QLineEdit      *referenceEdit { nullptr };   // M-Pesa confirmation code
    QRegularExpressionValidator *m_mpesaValidator { nullptr };
    MpesaClient    *m_mpesa     { nullptr };

    // ── Summary + actions ─────────────────────────────────────────────────────
    QLabel      *totalLabel  { nullptr };
    QLabel      *paidLabel   { nullptr };
    QLabel      *changeLabel { nullptr };
    QPushButton *confirmBtn  { nullptr };
    QPushButton *cancelBtn   { nullptr };

    // ── Amounts (major units) ─────────────────────────────────────────────────
    double total;        // outstanding due, after store credit / loyalty
    double amountPaid;   // entered sum (computed in recompute())
    double change;       // entered - due (computed)

    // ── Customer store-credit widgets (hidden when no customer) ───────────────
    QLabel      *creditAvailableLabel { nullptr };
    QPushButton *applyCreditButton    { nullptr };
    QLabel      *loyaltyLabel         { nullptr };
    QPushButton *redeemPointsButton   { nullptr };

    // ── Customer state ────────────────────────────────────────────────────────
    Customer m_customer;
    bool     m_hasCustomer       { false };
    Money    m_storeCreditUsed;
    int      m_pointsRedeemed    { 0 };
    int      m_loyaltyCentsPerPt { 10 };
};

#endif // PAYMENTDIALOG_H
