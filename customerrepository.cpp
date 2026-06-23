// =============================================================================
// customerrepository.cpp — Implementation of CustomerRepository (see
// customerrepository.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Bodies (and the customerFromQuery/customerSelect helpers) were lifted
//    verbatim from Database; the only edits are db -> m_db and
//    lastError -> m_lastError.
//  - redeemLoyaltyPoints checks the points balance before debiting; both it
//    and adjustStoreCredit are single-statement updates (the during-a-sale
//    accrual lives in SaleRepository::recordSale's transaction instead).
// =============================================================================
#include "customerrepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "money.h"

static Customer customerFromQuery(QSqlQuery &q)
{
    Customer c;
    c.id            = q.value(0).toInt();
    c.name          = q.value(1).toString();
    c.phone         = q.value(2).toString();
    c.email         = q.value(3).toString();
    c.address       = q.value(4).toString();
    c.loyaltyPoints = q.value(5).toInt();
    c.storeCredit   = Money::fromCents(q.value(6).toLongLong());
    c.isActive      = q.value(7).toBool();
    c.createdAt     = q.value(8).toString();
    return c;
}

static const char *customerSelect =
    "SELECT id, name, phone, email, address, loyalty_points, store_credit, "
    "       is_active, created_at FROM customers ";

CustomerRepository::CustomerRepository(QSqlDatabase db)
    : m_db(db)
{
}

QVector<Customer> CustomerRepository::getAllCustomers(bool includeInactive)
{
    QVector<Customer> list;
    QSqlQuery q(m_db);
    q.exec(QString(customerSelect)
           + (includeInactive ? "" : "WHERE is_active = 1 ")
           + "ORDER BY name");
    while (q.next())
        list.append(customerFromQuery(q));
    return list;
}

Customer CustomerRepository::getCustomerById(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QString(customerSelect) + "WHERE id = ?");
    q.addBindValue(id);
    if (q.exec() && q.next())
        return customerFromQuery(q);
    return Customer{};
}

Customer CustomerRepository::getCustomerByPhone(const QString &phone)
{
    QSqlQuery q(m_db);
    q.prepare(QString(customerSelect) + "WHERE phone = ? AND is_active = 1");
    q.addBindValue(phone.trimmed());
    if (q.exec() && q.next())
        return customerFromQuery(q);
    return Customer{};
}

bool CustomerRepository::addCustomer(const Customer &customer)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO customers (name, phone, email, address) VALUES (?, ?, ?, ?)");
    q.addBindValue(customer.name);
    q.addBindValue(customer.phone.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : customer.phone);
    q.addBindValue(customer.email);
    q.addBindValue(customer.address);
    if (!q.exec()) {
        m_lastError = "Failed to add customer: " + q.lastError().text();
        return false;
    }
    return true;
}

bool CustomerRepository::updateCustomer(const Customer &customer)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE customers SET name=?, phone=?, email=?, address=? WHERE id=?");
    q.addBindValue(customer.name);
    q.addBindValue(customer.phone.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : customer.phone);
    q.addBindValue(customer.email);
    q.addBindValue(customer.address);
    q.addBindValue(customer.id);
    if (!q.exec()) {
        m_lastError = "Failed to update customer: " + q.lastError().text();
        return false;
    }
    return true;
}

bool CustomerRepository::deactivateCustomer(int id)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE customers SET is_active = 0 WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = "Failed to deactivate customer: " + q.lastError().text();
        return false;
    }
    return true;
}

bool CustomerRepository::adjustStoreCredit(int customerId, Money delta, const QString &reason)
{
    Q_UNUSED(reason)   // available for a future audit log
    QSqlQuery q(m_db);
    q.prepare("UPDATE customers SET store_credit = store_credit + ? WHERE id = ?");
    q.addBindValue(delta.cents());
    q.addBindValue(customerId);
    if (!q.exec()) {
        m_lastError = "Failed to adjust store credit: " + q.lastError().text();
        return false;
    }
    return true;
}

bool CustomerRepository::redeemLoyaltyPoints(int customerId, int pointsToRedeem,
                                             Money creditValue)
{
    // Verify customer has enough points
    QSqlQuery check(m_db);
    check.prepare("SELECT loyalty_points FROM customers WHERE id = ?");
    check.addBindValue(customerId);
    if (!check.exec() || !check.next()) {
        m_lastError = "Customer not found";
        return false;
    }
    if (check.value(0).toInt() < pointsToRedeem) {
        m_lastError = "Insufficient loyalty points";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE customers SET "
              "loyalty_points = loyalty_points - ?, "
              "store_credit   = store_credit   + ? "
              "WHERE id = ?");
    q.addBindValue(pointsToRedeem);
    q.addBindValue(creditValue.cents());
    q.addBindValue(customerId);
    if (!q.exec()) {
        m_lastError = "Failed to redeem loyalty points: " + q.lastError().text();
        return false;
    }
    return true;
}
