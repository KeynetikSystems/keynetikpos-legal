// =============================================================================
// vat.cpp — Implementation of Vat (see vat.h).
// =============================================================================
#include "vat.h"
#include "ledger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <cmath>

QString taxCodeToString(TaxCode c)
{
    switch (c) {
    case TaxCode::Zero:   return "zero";
    case TaxCode::Exempt: return "exempt";
    case TaxCode::Standard:
    default:              return "standard";
    }
}

TaxCode taxCodeFromString(const QString &s)
{
    if (s == "zero")   return TaxCode::Zero;
    if (s == "exempt") return TaxCode::Exempt;
    return TaxCode::Standard;   // NULL / unknown -> standard-rated
}

QString taxCodeLabel(TaxCode c)
{
    switch (c) {
    case TaxCode::Zero:   return "Zero-rated (0%)";
    case TaxCode::Exempt: return "Exempt";
    case TaxCode::Standard:
    default:              return "Standard-rated";
    }
}

Vat::Vat(QSqlDatabase db) : m_db(db) {}

bool Vat::initSchema()
{
    QSqlQuery q(m_db);

    // Add products.tax_code if it isn't there yet (older DBs).
    bool hasCol = false;
    if (q.exec("PRAGMA table_info(products)")) {
        while (q.next())
            if (q.value(1).toString() == "tax_code") { hasCol = true; break; }
    }
    if (!hasCol) {
        if (!q.exec("ALTER TABLE products ADD COLUMN tax_code TEXT DEFAULT 'standard'")) {
            m_lastError = q.lastError().text();
            return false;
        }
    }

    if (!q.exec("CREATE TABLE IF NOT EXISTS vat_config ("
                "id INTEGER PRIMARY KEY CHECK (id = 1), standard_rate REAL)")) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

VatSplit Vat::splitInclusive(Money gross, TaxCode code, double standardRate)
{
    if (code != TaxCode::Standard || standardRate <= 0.0)
        return { gross, Money() };   // zero-rated / exempt carry no VAT
    // gross = net x (1 + r)  ->  vat = gross x r / (1 + r)
    const qint64 vat = std::llround(gross.cents() * standardRate / (1.0 + standardRate));
    return { Money::fromCents(gross.cents() - vat), Money::fromCents(vat) };
}

double Vat::standardRate() const
{
    QSqlQuery q(m_db);
    if (q.exec("SELECT standard_rate FROM vat_config WHERE id = 1") && q.next())
        return q.value(0).toDouble();
    return 0.16;   // Kenya standard rate default
}

bool Vat::setStandardRate(double rate)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO vat_config (id, standard_rate) VALUES (1, ?)");
    q.addBindValue(rate);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

TaxCode Vat::productTaxCode(int productId) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT tax_code FROM products WHERE id = ?");
    q.addBindValue(productId);
    if (q.exec() && q.next())
        return taxCodeFromString(q.value(0).toString());
    return TaxCode::Standard;
}

bool Vat::setProductTaxCode(int productId, TaxCode code)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE products SET tax_code = ? WHERE id = ?");
    q.addBindValue(taxCodeToString(code));
    q.addBindValue(productId);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

Vat3 Vat::computeVat3(const QDate &from, const QDate &to) const
{
    Vat3 v;
    const double rate = standardRate();
    const QString f = from.toString(Qt::ISODate);
    const QString t = to.toString(Qt::ISODate);

    // ── Output VAT: from sales, grouped by the item's tax code ───────────────
    // Retail line subtotals are VAT-inclusive; deleted products (NULL join)
    // fall back to standard-rated.
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT COALESCE(p.tax_code, 'standard') AS code, "
                  "       SUM(si.subtotal) AS gross "
                  "FROM sale_items si "
                  "JOIN sales s ON s.id = si.sale_id "
                  "LEFT JOIN products p ON p.id = si.product_id "
                  // sale_date is stored UTC (CURRENT_TIMESTAMP); convert before
                  // comparing against the local dates the caller supplies, or a
                  // late-night sale lands in the wrong VAT period.
                  "WHERE date(s.sale_date, 'localtime') BETWEEN ? AND ? "
                  "GROUP BY code");
        q.addBindValue(f);
        q.addBindValue(t);
        if (q.exec()) {
            while (q.next()) {
                const TaxCode code = taxCodeFromString(q.value(0).toString());
                const Money gross = Money::fromCents(q.value(1).toLongLong());
                switch (code) {
                case TaxCode::Standard: {
                    const VatSplit s = splitInclusive(gross, code, rate);
                    v.salesStandardNet += s.net;
                    v.outputVat        += s.vat;
                    break;
                }
                case TaxCode::Zero:   v.salesZero   += gross; break;
                case TaxCode::Exempt: v.salesExempt += gross; break;
                }
            }
        } else {
            m_lastError = q.lastError().text();
        }
    }

    // ── Input VAT: from received purchase orders ─────────────────────────────
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT COALESCE(p.tax_code, 'standard') AS code, "
                  "       SUM(poi.subtotal) AS gross "
                  "FROM purchase_order_items poi "
                  "JOIN purchase_orders po ON po.id = poi.po_id "
                  "LEFT JOIN products p ON p.id = poi.product_id "
                  "WHERE po.status = 'Received' "
                  "  AND date(COALESCE(po.received_date, po.order_date), 'localtime') "
                  "      BETWEEN ? AND ? "
                  "GROUP BY code");
        q.addBindValue(f);
        q.addBindValue(t);
        if (q.exec()) {
            while (q.next()) {
                const TaxCode code = taxCodeFromString(q.value(0).toString());
                const Money gross = Money::fromCents(q.value(1).toLongLong());
                if (code == TaxCode::Standard) {
                    const VatSplit s = splitInclusive(gross, code, rate);
                    v.purchasesStandardNet += s.net;
                    v.inputVat             += s.vat;
                }
                // zero/exempt purchases carry no recoverable input VAT
            }
        }
    }

    return v;
}

Money Vat::purchaseOrderInputVat(int poId) const
{
    const double rate = standardRate();
    Money vat;
    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(p.tax_code, 'standard') AS code, SUM(poi.subtotal) "
              "FROM purchase_order_items poi "
              "LEFT JOIN products p ON p.id = poi.product_id "
              "WHERE poi.po_id = ? GROUP BY code");
    q.addBindValue(poId);
    if (q.exec()) {
        while (q.next()) {
            const TaxCode code = taxCodeFromString(q.value(0).toString());
            const Money gross = Money::fromCents(q.value(1).toLongLong());
            if (code == TaxCode::Standard)
                vat += splitInclusive(gross, code, rate).vat;
        }
    }
    return vat;
}

int Vat::postSettlement(const QDate &date, Money amount, const QString &bankAccount)
{
    if (amount.cents() <= 0) { m_lastError = "Nothing to settle."; return -1; }

    Ledger ledger(m_db);
    ledger.initSchema();   // idempotent

    QVector<GLLine> lines;
    lines.append({ "2100",       amount, Money(), "VAT settlement to KRA" }); // Dr clear payable
    lines.append({ bankAccount,  Money(), amount, "VAT paid" });              // Cr bank

    const int entry = ledger.postEntry(date, "VAT settlement", "vat", lines);
    if (entry < 0) { m_lastError = "GL posting failed: " + ledger.lastError(); return -1; }
    return entry;
}
