// =============================================================================
// discount.cpp — Implementation of DiscountManager (see discount.h for the
// full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - applyFixedAmount() clamps with qMin(amount, subtotal) so a discount can
//    never push a total negative.
//  - logDiscount() writes shift id, cart id, type, value, amount, subtotal,
//    cashier, and ISO timestamp into the self-created DiscountLog table —
//    the anti-"sweethearting" audit trail.
//  - Presets are hard-coded in loadDefaultPresets(); the PIN here is a plain
//    default — production PIN checks go through SettingsManager (PBKDF2).
// =============================================================================
#include "discount.h"
#include "cart.h"          // formatMoney()
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

DiscountManager::DiscountManager(QSqlDatabase &db, QObject *parent)
    : QObject(parent), m_db(db)
{
    createTableIfNotExist();
    loadDefaultPresets();
}

void DiscountManager::createTableIfNotExist()
{
    QSqlQuery q(m_db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS DiscountLog (
            LogID          INTEGER PRIMARY KEY AUTOINCREMENT,
            ShiftID        INTEGER,
            CartID         TEXT,
            DiscountType   TEXT NOT NULL,
            DiscountValue  REAL NOT NULL,
            DiscountAmount REAL NOT NULL,
            Subtotal       REAL NOT NULL,
            AppliedBy      TEXT,
            AppliedAt      TEXT NOT NULL
        )
    )");
}

void DiscountManager::loadDefaultPresets()
{
    // Fixed-amount labels use the configured currency symbol (see cart.h) so a
    // KSh deployment never shows a stray "$". The symbol is read at construction
    // time, which is after setCurrencySymbol() runs at startup.
    const QString sym = currencySymbol();
    m_presets = {
                 {"5% Off",    DiscountType::PercentageOff, 0.05},
                 {"10% Off",   DiscountType::PercentageOff, 0.10},
                 {"15% Off",   DiscountType::PercentageOff, 0.15},
                 {"20% Off",   DiscountType::PercentageOff, 0.20},
                 {"Staff 25%", DiscountType::PercentageOff, 0.25},
                 {QString("%1 2 Off").arg(sym),  DiscountType::FixedAmount,  2.00},
                 {QString("%1 5 Off").arg(sym),  DiscountType::FixedAmount,  5.00},
                 {QString("%1 10 Off").arg(sym), DiscountType::FixedAmount, 10.00},
                 };
}

bool DiscountManager::verifyPin(const QString &pin) const { return pin == m_pin; }
void DiscountManager::setPin(const QString &pin)          { m_pin = pin; }
void DiscountManager::setRequirePin(bool require)         { m_requirePin = require; }

DiscountResult DiscountManager::applyPercentage(double subtotal, double pct,
                                                const QString &cashier) const
{
    DiscountResult r;
    r.type      = DiscountType::PercentageOff;
    r.value     = pct;
    r.amount    = subtotal * pct;
    r.label     = QString("%1% Off").arg(qRound(pct * 100));
    r.appliedBy = cashier;
    return r;
}

DiscountResult DiscountManager::applyFixedAmount(double subtotal, double amount,
                                                 const QString &cashier) const
{
    DiscountResult r;
    r.type      = DiscountType::FixedAmount;
    r.value     = amount;
    r.amount    = qMin(amount, subtotal);
    r.label     = QString("%1 Off").arg(formatMoney(amount));
    r.appliedBy = cashier;
    return r;
}

void DiscountManager::logDiscount(int shiftId, const DiscountResult &discount,
                                  const QString &cartId, double subtotal)
{
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO DiscountLog
            (ShiftID, CartID, DiscountType, DiscountValue, DiscountAmount,
             Subtotal, AppliedBy, AppliedAt)
        VALUES
            (:sid, :cart, :type, :val, :amt, :sub, :by, :at)
    )");

    QString typeStr;
    switch (discount.type) {
    case DiscountType::PercentageOff: typeStr = "Percentage"; break;
    case DiscountType::FixedAmount:   typeStr = "Fixed";      break;
    case DiscountType::FixedPrice:    typeStr = "Price";      break;
    default:                          typeStr = "None";       break;
    }

    q.bindValue(":sid",  shiftId);
    q.bindValue(":cart", cartId);
    q.bindValue(":type", typeStr);
    q.bindValue(":val",  discount.value);
    q.bindValue(":amt",  discount.amount);
    q.bindValue(":sub",  subtotal);
    q.bindValue(":by",   discount.appliedBy);
    q.bindValue(":at",   QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec())
        qDebug() << "DiscountManager: log failed:" << q.lastError().text();

    emit discountApplied(discount);
}