// =============================================================================
// money.h — Money: integer-minor-unit currency type + formatting
// -----------------------------------------------------------------------------
// WHAT: Money holds an amount as a whole number of minor units (cents), not a
//       floating-point major-unit value. It is the single money type used
//       across the cart, totals, database and receipts. Also hosts the
//       configurable currency symbol and formatMoney().
// HOW:  A value type wrapping a qint64 cent count. Conversions to/from the
//       outside world are EXPLICIT (fromCents/fromMajor, cents()/toMajor()) so
//       a stray double can never silently become money (or vice versa) — the
//       compiler forces every boundary to be spelled out. Arithmetic stays in
//       integers; the only float involved is fromMajor()'s round-half-away
//       conversion at UI input and tax-rate multiplication.
// WHY:  Doubles cannot exactly represent money: 0.1 + 0.2 != 0.3, and
//       round(1.005) floored to 1.00. For a system whose job is to be exact
//       about money that is a latent liability — totals that don't reconcile,
//       unsafe == comparisons, drift that compounds over a day's sales. Integer
//       cents are exact; rounding happens once, deliberately, at the edge.
// =============================================================================
#ifndef MONEY_H
#define MONEY_H

#include <QString>
#include <QChar>
#include <QtGlobal>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Currency symbol — single source of truth for money display.
// Configurable (BusinessSettings.currencySymbol): seeded at startup and on
// settings change via setCurrencySymbol(). Everything that shows money goes
// through formatMoney() so the configured currency is honoured everywhere.
// ─────────────────────────────────────────────────────────────────────────────
inline QString &currencySymbolStore()
{
    static QString s = QStringLiteral("KSh");   // app's primary market default
    return s;
}

inline QString currencySymbol()
{
    return currencySymbolStore();
}

inline void setCurrencySymbol(const QString &sym)
{
    const QString trimmed = sym.trimmed();
    currencySymbolStore() = trimmed.isEmpty() ? QStringLiteral("KSh") : trimmed;
}

// -----------------------------------------------------------------------------
// Money — an exact currency amount in minor units (cents).
// -----------------------------------------------------------------------------
class Money
{
public:
    constexpr Money() = default;

    // Exact construction from a cent count (e.g. database value, qty math).
    static constexpr Money fromCents(qint64 cents) { return Money(cents); }

    // Lossy construction from a major-unit double (e.g. a spin box's 19.99),
    // rounding half away from zero. This is the ONLY place a double becomes
    // money — keep it at UI/parse boundaries.
    static Money fromMajor(double major)
    {
        return Money(static_cast<qint64>(std::llround(major * 100.0)));
    }

    constexpr qint64 cents()  const { return m_cents; }
    constexpr double toMajor() const { return static_cast<double>(m_cents) / 100.0; }
    constexpr bool   isZero() const { return m_cents == 0; }

    constexpr Money operator+(Money o) const { return Money(m_cents + o.m_cents); }
    constexpr Money operator-(Money o) const { return Money(m_cents - o.m_cents); }
    constexpr Money operator-()        const { return Money(-m_cents); }
    constexpr Money operator*(qint64 qty) const { return Money(m_cents * qty); }
    Money &operator+=(Money o) { m_cents += o.m_cents; return *this; }
    Money &operator-=(Money o) { m_cents -= o.m_cents; return *this; }

    constexpr bool operator==(Money o) const { return m_cents == o.m_cents; }
    constexpr bool operator!=(Money o) const { return m_cents != o.m_cents; }
    constexpr bool operator< (Money o) const { return m_cents <  o.m_cents; }
    constexpr bool operator<=(Money o) const { return m_cents <= o.m_cents; }
    constexpr bool operator> (Money o) const { return m_cents >  o.m_cents; }
    constexpr bool operator>=(Money o) const { return m_cents >= o.m_cents; }

private:
    explicit constexpr Money(qint64 cents) : m_cents(cents) {}
    qint64 m_cents = 0;
};

// Formats with the configured symbol and exactly two decimals, e.g. "KSh 1234.50".
inline QString formatMoney(Money amount)
{
    const qint64 a   = amount.cents() < 0 ? -amount.cents() : amount.cents();
    const QString sign = amount.cents() < 0 ? QStringLiteral("-") : QString();
    return QStringLiteral("%1%2 %3.%4")
        .arg(sign, currencySymbol())
        .arg(a / 100)
        .arg(a % 100, 2, 10, QChar('0'));
}

#endif // MONEY_H
