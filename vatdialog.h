// =============================================================================
// vatdialog.h — VatDialog: VAT-3 return, product tax codes, rate, settlement
// -----------------------------------------------------------------------------
// WHAT: The VAT module UI — compute a VAT-3 summary for a period, assign tax
//       codes to products, edit the standard rate, and post the VAT payment.
// HOW:  Thin view over Vat (vat.h); all maths/persistence live there.
// =============================================================================
#ifndef VATDIALOG_H
#define VATDIALOG_H

#include <QDialog>

#include "money.h"

class Vat;
class QDateEdit;
class QLabel;
class QTableWidget;

class VatDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VatDialog(Vat *vat, QWidget *parent = nullptr);

private slots:
    void computeReturn();
    void recordPayment();
    void refreshProducts();

private:
    Vat          *m_vat;
    QDateEdit    *m_from { nullptr };
    QDateEdit    *m_to   { nullptr };
    QLabel       *m_outNet { nullptr };
    QLabel       *m_outVat { nullptr };
    QLabel       *m_zero { nullptr };
    QLabel       *m_exempt { nullptr };
    QLabel       *m_inNet { nullptr };
    QLabel       *m_inVat { nullptr };
    QLabel       *m_netPayable { nullptr };
    QLabel       *m_rateLabel { nullptr };
    QTableWidget *m_prodTable { nullptr };
    Money         m_lastNetPayable;
};

#endif // VATDIALOG_H
