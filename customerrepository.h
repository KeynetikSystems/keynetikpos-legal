// =============================================================================
// customerrepository.h — CustomerRepository: customer accounts, split out of
// Database
// -----------------------------------------------------------------------------
// WHAT: Owns the read/write SQL for the customers table: list, fetch by id or
//       phone (the checkout quick-lookup key), add, update, soft-delete, and
//       the two balance operations — adjustStoreCredit() and
//       redeemLoyaltyPoints(). The Customer struct still lives in database.h.
// HOW:  Plain class over a QSqlDatabase held BY VALUE — same pattern as Ledger
//       / SaleRepository / SupplierRepository — sharing Database's one open
//       connection. lastError() mirrors Database's convention for writes.
//       NOTE: loyalty/credit accrued *during a sale* is handled inside
//       SaleRepository::recordSale()'s transaction; the methods here are the
//       explicit, out-of-band adjustments (manual top-up, points redemption).
// WHY:  Third slice of the Database "god object" breakup, same facade pattern:
//       Database keeps the public methods but delegates here, leaving call
//       sites untouched.
// =============================================================================
#ifndef CUSTOMERREPOSITORY_H
#define CUSTOMERREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "database.h"   // Customer, Money

class CustomerRepository
{
public:
    explicit CustomerRepository(QSqlDatabase db);

    QVector<Customer> getAllCustomers(bool includeInactive = false);
    Customer getCustomerById(int id);
    Customer getCustomerByPhone(const QString &phone);
    bool addCustomer(const Customer &customer);
    bool updateCustomer(const Customer &customer);
    bool deactivateCustomer(int id);
    // Explicit store-credit top-up (not the credit earned during a sale, which
    // goes through SaleRepository::recordSale).
    bool adjustStoreCredit(int customerId, Money delta, const QString &reason);
    // Burn loyalty points into store credit. Returns false if the customer has
    // insufficient points. Caller decides the rate (creditValue).
    bool redeemLoyaltyPoints(int customerId, int pointsToRedeem, Money creditValue);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    QString         m_lastError;
};

#endif // CUSTOMERREPOSITORY_H
