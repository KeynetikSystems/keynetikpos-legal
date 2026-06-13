// =============================================================================
// discount.h — DiscountManager: discount business logic + audit trail
// -----------------------------------------------------------------------------
// WHAT: PIN-gated discount authorization, percentage and fixed-amount
//       calculators, preset discounts ("10% Off", "Staff 25%", "$5 Off"...),
//       and an audit log of every discount applied.
// HOW:  applyPercentage()/applyFixedAmount() return a DiscountResult (type,
//       value, computed amount, label, who applied it); fixed discounts are
//       clamped with qMin(amount, subtotal) so a total can never go negative.
//       logDiscount() writes shift id, cart id, type, amounts, cashier, and
//       ISO timestamp into the self-created DiscountLog table and emits
//       discountApplied. NOTE: this class's own m_pin is a plain default
//       ("1234"); the production PIN path used by MainWindow is
//       SettingsManager::verifyDiscountPin(), which is PBKDF2-hashed.
// WHY:  Discounts are the easiest way for an employee to leak money
//       ("sweethearting"), so every application requires a PIN and leaves an
//       audit row tied to the shift and cashier. Presets reduce keying errors
//       at the till.
// =============================================================================
#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

// =============================================================================
// Enums and data structures
// =============================================================================

enum class DiscountType {
    None,
    PercentageOff,
    FixedAmount,
    FixedPrice
};

struct DiscountPreset {
    QString      label;
    DiscountType type;
    double       value;
};

struct DiscountResult {
    DiscountType type      = DiscountType::None;
    double       value     = 0.0;
    double       amount    = 0.0;
    QString      label;
    QString      appliedBy;
};

// =============================================================================
// DiscountManager
// =============================================================================

class DiscountManager : public QObject
{
    Q_OBJECT

public:
    explicit DiscountManager(QSqlDatabase &db, QObject *parent = nullptr);

    bool verifyPin(const QString &pin) const;
    void setPin(const QString &pin);
    void setRequirePin(bool require);
    bool requiresPin() const { return m_requirePin; }

    DiscountResult applyPercentage(double subtotal, double pct,
                                   const QString &cashier = QString()) const;
    DiscountResult applyFixedAmount(double subtotal, double amount,
                                    const QString &cashier = QString()) const;

    QVector<DiscountPreset> presets() const { return m_presets; }

    void logDiscount(int shiftId, const DiscountResult &discount,
                     const QString &cartId, double subtotal);

signals:
    void discountApplied(const DiscountResult &discount);

private:
    void createTableIfNotExist();
    void loadDefaultPresets();

    QSqlDatabase           &m_db;
    QString                 m_pin        = "1234";
    bool                    m_requirePin = true;
    QVector<DiscountPreset> m_presets;
};