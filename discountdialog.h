// =============================================================================
// discountdialog.h — DiscountDialog: UI for entering a cart discount
// -----------------------------------------------------------------------------
// WHAT: Modal form to put a discount on the current cart: percentage or fixed
//       radio choice, value field, required reason field, and a live preview
//       of the discount amount and new total.
// HOW:  calculateDiscount() re-runs on each input change and validates ranges
//       (percentage <= 100, fixed <= subtotal). MainWindow performs the PIN
//       prompt BEFORE opening this dialog, then stores the accepted result on
//       the active Cart.
// WHY:  The live preview prevents "oops, I meant 10 not 100" errors, and the
//       mandatory reason feeds the discount audit trail.
// =============================================================================
#ifndef DISCOUNTDIALOG_H
#define DISCOUNTDIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>

#include "money.h"

class DiscountDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiscountDialog(Money subtotal, QWidget *parent = nullptr);
    ~DiscountDialog();

    Money getDiscountAmount() const;
    QString getDiscountReason() const;

private slots:
    void onDiscountTypeChanged();
    void onDiscountValueChanged();
    void onApplyClicked();

private:
    void setupUI();
    void calculateDiscount();

    double subtotal;
    double discountAmount;
    QString discountReason;

    // UI Components
    QButtonGroup *discountTypeGroup;
    QRadioButton *percentageRadio;
    QRadioButton *fixedRadio;

    QLineEdit *discountValueEdit;
    QLineEdit *reasonEdit;
    QLabel *discountAmountLabel;
    QLabel *newTotalLabel;

    QPushButton *applyBtn;
    QPushButton *cancelBtn;
};

#endif // DISCOUNTDIALOG_H
