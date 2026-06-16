// =============================================================================
// paymentmanager.cpp — Implementation of PaymentManager (see paymentmanager.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - One row per tender in the self-created payments table supports split
//    payments (a single sale paid by several methods).
//  - getDailySummary()/getShiftSummary() GROUP BY method — the inputs to
//    Z-Report cash reconciliation.
//  - The static name/icon/colour helpers are the single source of truth for
//    how each PaymentMethod renders anywhere in the app.
// =============================================================================
#include "paymentmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>
#include <QDialogButtonBox>

// =============================================================================
// PaymentManager Implementation
// =============================================================================

PaymentManager::PaymentManager(QSqlDatabase &db, QObject *parent)
    : QObject(parent), m_db(db)
{
    createTablesIfNotExist();
}

void PaymentManager::createTablesIfNotExist()
{
    QSqlQuery query(m_db);

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS PaymentRecords (
            PaymentID      INTEGER PRIMARY KEY AUTOINCREMENT,
            TransactionID  INTEGER NOT NULL,
            PaymentMethod  TEXT NOT NULL,
            Amount         REAL NOT NULL,
            Reference      TEXT,
            Notes          TEXT,
            Timestamp      TEXT NOT NULL
        )
    )");
}

bool PaymentManager::recordPayment(int transactionId, const PaymentSummary &payment)
{
    QSqlQuery query(m_db);

    for (const PaymentRecord &record : payment.payments) {
        query.prepare(R"(
            INSERT INTO PaymentRecords
            (TransactionID, PaymentMethod, Amount, Reference, Notes, Timestamp)
            VALUES (:tid, :method, :amount, :ref, :notes, :time)
        )");

        query.bindValue(":tid", transactionId);
        query.bindValue(":method", paymentMethodName(record.method));
        query.bindValue(":amount", record.amount);
        query.bindValue(":ref", record.reference);
        query.bindValue(":notes", record.notes);
        query.bindValue(":time", record.timestamp.toString(Qt::ISODate));

        if (!query.exec()) {
            qDebug() << "Error recording payment:" << query.lastError().text();
            return false;
        }
    }

    emit paymentRecorded(transactionId, payment.totalPaid);
    return true;
}

QVector<PaymentRecord> PaymentManager::getPaymentHistory(int transactionId) const
{
    QVector<PaymentRecord> payments;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT PaymentMethod, Amount, Reference, Notes, Timestamp
        FROM PaymentRecords
        WHERE TransactionID = :tid
        ORDER BY Timestamp ASC
    )");
    query.bindValue(":tid", transactionId);

    if (query.exec()) {
        while (query.next()) {
            PaymentRecord record;
            QString methodStr = query.value(0).toString();

            // Convert string to enum
            if (methodStr == "Cash") record.method = PaymentMethod::Cash;
            else if (methodStr == "Card") record.method = PaymentMethod::Card;
            else if (methodStr == "Mobile Money") record.method = PaymentMethod::MobileMoney;
            else if (methodStr == "Bank Transfer") record.method = PaymentMethod::BankTransfer;
            else if (methodStr == "Check") record.method = PaymentMethod::Check;
            else if (methodStr == "Gift Card") record.method = PaymentMethod::GiftCard;
            else if (methodStr == "Store Credit") record.method = PaymentMethod::StoreCredit;
            else record.method = PaymentMethod::Other;

            record.amount = query.value(1).toDouble();
            record.reference = query.value(2).toString();
            record.notes = query.value(3).toString();
            record.timestamp = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);

            payments.append(record);
        }
    }

    return payments;
}

PaymentSummary PaymentManager::getPaymentSummary(int transactionId) const
{
    PaymentSummary summary;
    summary.payments = getPaymentHistory(transactionId);
    summary.totalPaid = 0.0;

    for (const PaymentRecord &record : summary.payments) {
        summary.totalPaid += record.amount;
    }

    return summary;
}

double PaymentManager::getTotalByMethod(PaymentMethod method, const QDate &date) const
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT SUM(Amount) FROM PaymentRecords
        WHERE PaymentMethod = :method
        AND DATE(Timestamp) = :date
    )");
    query.bindValue(":method", paymentMethodName(method));
    query.bindValue(":date", date.toString("yyyy-MM-dd"));

    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }

    return 0.0;
}

QMap<PaymentMethod, double> PaymentManager::getDailySummary(const QDate &date) const
{
    QMap<PaymentMethod, double> summary;

    QVector<PaymentMethod> methods = {
        PaymentMethod::Cash,
        PaymentMethod::Card,
        PaymentMethod::MobileMoney,
        PaymentMethod::BankTransfer,
        PaymentMethod::Check,
        PaymentMethod::GiftCard,
        PaymentMethod::StoreCredit,
        PaymentMethod::Other
    };

    for (PaymentMethod method : methods) {
        summary[method] = getTotalByMethod(method, date);
    }

    return summary;
}

QMap<PaymentMethod, double> PaymentManager::getShiftSummary(int shiftId) const
{
    QMap<PaymentMethod, double> summary;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT pr.PaymentMethod, SUM(pr.Amount)
        FROM PaymentRecords pr
        JOIN Transactions t ON pr.TransactionID = t.TransactionID
        WHERE t.ShiftID = :shiftId
        GROUP BY pr.PaymentMethod
    )");
    query.bindValue(":shiftId", shiftId);

    if (query.exec()) {
        while (query.next()) {
            QString methodStr = query.value(0).toString();
            double amount = query.value(1).toDouble();

            PaymentMethod method;
            if (methodStr == "Cash") method = PaymentMethod::Cash;
            else if (methodStr == "Card") method = PaymentMethod::Card;
            else if (methodStr == "Mobile Money") method = PaymentMethod::MobileMoney;
            else if (methodStr == "Bank Transfer") method = PaymentMethod::BankTransfer;
            else if (methodStr == "Check") method = PaymentMethod::Check;
            else if (methodStr == "Gift Card") method = PaymentMethod::GiftCard;
            else if (methodStr == "Store Credit") method = PaymentMethod::StoreCredit;
            else method = PaymentMethod::Other;

            summary[method] = amount;
        }
    }

    return summary;
}

QString PaymentManager::paymentMethodName(PaymentMethod method)
{
    switch (method) {
    case PaymentMethod::Cash: return "Cash";
    case PaymentMethod::Card: return "Card";
    case PaymentMethod::MobileMoney: return "Mobile Money";
    case PaymentMethod::BankTransfer: return "Bank Transfer";
    case PaymentMethod::Check: return "Check";
    case PaymentMethod::GiftCard: return "Gift Card";
    case PaymentMethod::StoreCredit: return "Store Credit";
    default: return "Other";
    }
}

QString PaymentManager::paymentMethodIcon(PaymentMethod /*method*/)
{
    // Icons removed from the UI; kept as a no-op for API compatibility.
    return QString();
}

QColor PaymentManager::paymentMethodColor(PaymentMethod method)
{
    switch (method) {
    case PaymentMethod::Cash: return QColor("#4CAF50");
    case PaymentMethod::Card: return QColor("#2196F3");
    case PaymentMethod::MobileMoney: return QColor("#9C27B0");
    case PaymentMethod::BankTransfer: return QColor("#FF9800");
    case PaymentMethod::Check: return QColor("#795548");
    case PaymentMethod::GiftCard: return QColor("#E91E63");
    case PaymentMethod::StoreCredit: return QColor("#00BCD4");
    default: return QColor("#9E9E9E");
    }
}


// PaymentDialog and CashPaymentDialog are implemented in paymentdialog.cpp