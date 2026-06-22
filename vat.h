// =============================================================================
// vat.h — VAT: per-product tax codes, VAT-3 return, settlement posting (Tier 4)
// -----------------------------------------------------------------------------
// WHAT: Kenyan VAT support — a tax code per product (standard / zero / exempt),
//       a VAT-3 summary for a filing period, and posting of the VAT payment to
//       the General Ledger.
// HOW:  Retail and purchase amounts are stored VAT-INCLUSIVE, so VAT is backed
//       out with splitInclusive(): vat = gross x rate/(1+rate). The VAT-3 is
//       computed straight from the sales / purchase-order tables (the source of
//       truth for a return), NOT from the GL. settlement posting records the
//       payment to KRA (Dr 2100 VAT Output / Cr bank).
// WHY:  A VAT-registered shop must file VAT-3 (output VAT on sales less input
//       VAT on purchases) and keep tax codes per item; zero-rated and exempt
//       supplies are reported separately from standard-rated.
//
//  !! VERIFY: the standard rate (16%) and the zero/exempt treatment follow the
//     current VAT Act; confirm before filing. eTIMS transmission is stubbed
//     (see etims.h) — real KRA OSCU/VSCU needs device onboarding.
// =============================================================================
#ifndef VAT_H
#define VAT_H

#include <QString>
#include <QDate>
#include <QSqlDatabase>

#include "money.h"

enum class TaxCode { Standard, Zero, Exempt };

QString  taxCodeToString(TaxCode c);   // "standard" / "zero" / "exempt"
TaxCode  taxCodeFromString(const QString &s);
QString  taxCodeLabel(TaxCode c);      // human label incl. rate

struct VatSplit {
    Money net;   // amount excluding VAT
    Money vat;   // VAT portion
};

// A VAT-3 period summary (all amounts in cents).
struct Vat3 {
    Money salesStandardNet;   // net value of standard-rated sales
    Money outputVat;          // VAT charged on those sales
    Money salesZero;          // value of zero-rated sales
    Money salesExempt;        // value of exempt sales
    Money purchasesStandardNet;
    Money inputVat;           // recoverable VAT on standard-rated purchases
    Money netPayable() const { return outputVat - inputVat; }  // +ve = pay KRA
};

class Vat
{
public:
    explicit Vat(QSqlDatabase db);

    bool initSchema();   // adds products.tax_code + vat_config; idempotent

    // ── Engine (pure; unit-tested) ────────────────────────────────────────────
    static VatSplit splitInclusive(Money gross, TaxCode code, double standardRate);

    // ── Rate (persisted) ──────────────────────────────────────────────────────
    double standardRate() const;
    bool   setStandardRate(double rate);

    // ── Per-product tax codes ─────────────────────────────────────────────────
    TaxCode productTaxCode(int productId) const;
    bool    setProductTaxCode(int productId, TaxCode code);

    // ── VAT-3 ─────────────────────────────────────────────────────────────────
    Vat3 computeVat3(const QDate &from, const QDate &to) const;

    // Recoverable input VAT on a single purchase order, summed across its line
    // items by tax code (standard-rated items only). Used when posting a
    // received PO to the GL so inventory is booked net of VAT.
    Money purchaseOrderInputVat(int poId) const;

    // Records a VAT payment to KRA: Dr 2100 VAT Output / Cr <bankAccount>.
    // Returns the GL entry id, or -1. (The liability accrues as sales are
    // posted to the GL; this entry settles it.)
    int  postSettlement(const QDate &date, Money amount,
                        const QString &bankAccount = "1020");

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    mutable QString m_lastError;
};

#endif // VAT_H
