// =============================================================================
// paymentmanager.h — PaymentManager: persistence & reporting of tender types
// -----------------------------------------------------------------------------
// WHAT: Records and reports HOW sales were paid. Defines PaymentMethod (Cash,
//       Card, MobileMoney, BankTransfer, Check, GiftCard, StoreCredit, Other),
//       PaymentRecord/PaymentSummary structs, and queries for per-transaction
//       history, daily summaries, and per-shift summaries by method.
// HOW:  Creates its own payments table on construction; recordPayment()
//       inserts one row per tender. Static helpers map each method to a
//       display name, icon, and QColor so every screen renders payment methods
//       identically. Aggregation queries GROUP BY method.
// WHY:  Splitting payment records from the sales table supports split tender
//       (one sale paid partly cash, partly card) and enables the cash-
//       reconciliation math Z-Reports and shift closes need ("how much CASH
//       should be in the drawer?" != "what were total sales?"). MobileMoney is
//       first-class because M-Pesa dominates the Kenyan target market.
// =============================================================================
#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <QDate>
#include <QVector>
#include <QMap>
#include <QColor>

// =============================================================================
// Payment enums and data structures
// =============================================================================

enum class PaymentMethod {
    Cash,
    Card,
    MobileMoney,
    BankTransfer,
    Check,
    GiftCard,
    StoreCredit,
    Other
};

struct PaymentRecord {
    PaymentMethod method    = PaymentMethod::Cash;
    double        amount    = 0.0;
    QString       reference;
    QString       notes;
    QDateTime     timestamp;
};

struct PaymentSummary {
    QVector<PaymentRecord> payments;
    double                 totalPaid = 0.0;
};

// =============================================================================
// PaymentManager
// =============================================================================

class PaymentManager : public QObject
{
    Q_OBJECT

public:
    explicit PaymentManager(QSqlDatabase &db, QObject *parent = nullptr);

    // Record payments
    bool recordPayment(int transactionId, const PaymentSummary &payment);

    // Retrieve payments
    QVector<PaymentRecord>      getPaymentHistory(int transactionId) const;
    PaymentSummary              getPaymentSummary(int transactionId) const;

    // Summaries
    double                      getTotalByMethod(PaymentMethod method,
                            const QDate &date) const;
    QMap<PaymentMethod, double> getDailySummary(const QDate &date) const;
    QMap<PaymentMethod, double> getShiftSummary(int shiftId) const;

    // Static helpers
    static QString paymentMethodName(PaymentMethod method);
    static QString paymentMethodIcon(PaymentMethod method);
    static QColor  paymentMethodColor(PaymentMethod method);

signals:
    void paymentRecorded(int transactionId, double totalPaid);

private:
    void createTablesIfNotExist();

    QSqlDatabase &m_db;
};