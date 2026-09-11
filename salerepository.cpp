// =============================================================================
// salerepository.cpp — Implementation of SaleRepository (see salerepository.h
// for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - recordSale() re-validates stock, inserts the sale + items, decrements
//    stock, accrues customer loyalty/credit, and records per-tender rows, all
//    inside ONE transaction; any failure rolls back and returns -1 with
//    lastError set. The bodies here were lifted verbatim from Database so the
//    behaviour (and its tests) is unchanged.
//  - SaleItem/Sale snapshot product name/price/cost at sale time, so historical
//    reports reflect what was actually charged even after products change.
// =============================================================================
#include "salerepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

#include "money.h"

SaleRepository::SaleRepository(QSqlDatabase db)
    : m_db(db)
{
}

int SaleRepository::recordSale(const SaleRequest &request)
{
    // Local aliases keep the (already long) transaction body readable and let
    // the field-by-field SQL binding below stay unchanged.
    const QVector<SaleItem>    &items           = request.items;
    const Money                 subtotal        = request.subtotal;
    const Money                 tax             = request.tax;
    const Money                 discount        = request.discount;
    const Money                 total           = request.total;
    const QString              &paymentMethod   = request.paymentMethod;
    const Money                 amountPaid      = request.amountPaid;
    const Money                 changeDue       = request.changeDue;
    const int                   customerId      = request.customerId;
    const Money                 storeCreditUsed = request.storeCreditUsed;
    const QVector<SalePayment> &payments        = request.payments;

    if (items.isEmpty()) {
        m_lastError = "Cannot record a sale with no items";
        return -1;
    }

    if (!m_db.transaction()) {
        m_lastError = "Failed to start transaction: " + m_db.lastError().text();
        return -1;
    }

    // Validate stock inside the transaction so concurrent sales can't oversell
    for (const SaleItem &item : items) {
        QSqlQuery stockQuery(m_db);
        stockQuery.prepare("SELECT stock_quantity FROM products WHERE id = ?");
        stockQuery.addBindValue(item.productId);
        if (!stockQuery.exec() || !stockQuery.next()) {
            m_lastError = "Product not found: " + item.productName;
            m_db.rollback();
            return -1;
        }
        if (stockQuery.value(0).toInt() < item.quantity) {
            m_lastError = QString("Insufficient stock for %1 (available: %2, requested: %3)")
                            .arg(item.productName)
                            .arg(stockQuery.value(0).toInt())
                            .arg(item.quantity);
            m_db.rollback();
            return -1;
        }
    }

    // Validate customer store-credit before touching anything
    if (customerId > 0 && storeCreditUsed.cents() > 0) {
        QSqlQuery creditCheck(m_db);
        creditCheck.prepare("SELECT store_credit FROM customers WHERE id = ? AND is_active = 1");
        creditCheck.addBindValue(customerId);
        if (!creditCheck.exec() || !creditCheck.next()) {
            m_lastError = "Customer not found";
            m_db.rollback();
            return -1;
        }
        if (creditCheck.value(0).toLongLong() < storeCreditUsed.cents()) {
            m_lastError = "Insufficient store credit";
            m_db.rollback();
            return -1;
        }
    }

    QSqlQuery saleQuery(m_db);
    saleQuery.prepare("INSERT INTO sales "
                      "(subtotal, tax, discount, total, payment_method, "
                      " amount_paid, change_due, customer_id, store_credit_used, "
                      " cashier, shift_id) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    saleQuery.addBindValue(subtotal.cents());
    saleQuery.addBindValue(tax.cents());
    saleQuery.addBindValue(discount.cents());
    saleQuery.addBindValue(total.cents());
    saleQuery.addBindValue(paymentMethod);
    saleQuery.addBindValue(amountPaid.cents());
    saleQuery.addBindValue(changeDue.cents());
    saleQuery.addBindValue(customerId > 0 ? customerId : QVariant(QMetaType(QMetaType::Int)));
    saleQuery.addBindValue(storeCreditUsed.cents());
    saleQuery.addBindValue(request.cashier);
    saleQuery.addBindValue(request.shiftId);
    if (!saleQuery.exec()) {
        m_lastError = saleQuery.lastError().text();
        m_db.rollback();
        return -1;
    }
    const int saleId = saleQuery.lastInsertId().toInt();

    for (const SaleItem &item : items) {
        QSqlQuery itemQuery(m_db);
        itemQuery.prepare("INSERT INTO sale_items "
                          "(sale_id, product_id, product_name, quantity, price, cost_price, subtotal) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?)");
        itemQuery.addBindValue(saleId);
        itemQuery.addBindValue(item.productId);
        itemQuery.addBindValue(item.productName);
        itemQuery.addBindValue(item.quantity);
        itemQuery.addBindValue(item.price.cents());
        itemQuery.addBindValue(item.costPrice.cents());
        itemQuery.addBindValue((item.price * item.quantity).cents());
        if (!itemQuery.exec()) {
            m_lastError = itemQuery.lastError().text();
            m_db.rollback();
            return -1;
        }

        QSqlQuery stockUpdate(m_db);
        stockUpdate.prepare("UPDATE products SET stock_quantity = stock_quantity - ? WHERE id = ?");
        stockUpdate.addBindValue(item.quantity);
        stockUpdate.addBindValue(item.productId);
        if (!stockUpdate.exec()) {
            m_lastError = stockUpdate.lastError().text();
            m_db.rollback();
            return -1;
        }
    }

    // Customer loyalty + store credit update — inside the same transaction so
    // point accrual and credit deduction are always consistent with the sale.
    if (customerId > 0) {
        // 1 point per 100 KSh (10 000 cents) spent, rounded down.
        const int pointsEarned = static_cast<int>(total.cents() / 10000);
        QSqlQuery custUpdate(m_db);
        custUpdate.prepare("UPDATE customers SET "
                           "loyalty_points = loyalty_points + ?, "
                           "store_credit   = store_credit   - ? "
                           "WHERE id = ?");
        custUpdate.addBindValue(pointsEarned);
        custUpdate.addBindValue(storeCreditUsed.cents());
        custUpdate.addBindValue(customerId);
        if (!custUpdate.exec()) {
            m_lastError = "Failed to update customer: " + custUpdate.lastError().text();
            m_db.rollback();
            return -1;
        }
    }

    // Per-tender breakdown (split payments). Only amounts > 0 are recorded.
    for (const SalePayment &p : payments) {
        if (p.amount.cents() <= 0) continue;
        QSqlQuery payQuery(m_db);
        payQuery.prepare("INSERT INTO sale_payments (sale_id, method, amount, reference) "
                         "VALUES (?, ?, ?, ?)");
        payQuery.addBindValue(saleId);
        payQuery.addBindValue(p.method);
        payQuery.addBindValue(p.amount.cents());
        payQuery.addBindValue(p.reference.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                    : QVariant(p.reference));
        if (!payQuery.exec()) {
            m_lastError = payQuery.lastError().text();
            m_db.rollback();
            return -1;
        }
    }

    if (!m_db.commit()) {
        m_lastError = "Failed to commit sale: " + m_db.lastError().text();
        m_db.rollback();
        return -1;
    }
    return saleId;
}

QVector<Sale> SaleRepository::getAllSales()
{
    QVector<Sale> sales;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, sale_date, subtotal, tax, discount, total, payment_method, amount_paid, change_due, cashier, shift_id FROM sales ORDER BY sale_date DESC");
    query.exec();
    while (query.next()) {
        Sale s;
        s.id = query.value(0).toInt();
        s.saleDate = query.value(1).toDateTime();
        s.subtotal = Money::fromCents(query.value(2).toLongLong());
        s.tax = Money::fromCents(query.value(3).toLongLong());
        s.discount = Money::fromCents(query.value(4).toLongLong());
        s.total = Money::fromCents(query.value(5).toLongLong());
        s.paymentMethod = query.value(6).toString();
        s.amountPaid = Money::fromCents(query.value(7).toLongLong());
        s.changeDue = Money::fromCents(query.value(8).toLongLong());
        s.cashier = query.value(9).toString();
        s.shiftId = query.value(10).toInt();
        sales.append(s);
    }
    return sales;
}

QVector<Sale> SaleRepository::getSalesByDateRange(const QDate &startDate, const QDate &endDate)
{
    QVector<Sale> sales;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, sale_date, subtotal, tax, discount, total, payment_method, amount_paid, change_due, cashier, shift_id FROM sales WHERE DATE(sale_date, 'localtime') BETWEEN ? AND ? ORDER BY sale_date DESC");
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(endDate.toString(Qt::ISODate));
    query.exec();
    while (query.next()) {
        Sale s;
        s.id = query.value(0).toInt();
        s.saleDate = query.value(1).toDateTime();
        s.subtotal = Money::fromCents(query.value(2).toLongLong());
        s.tax = Money::fromCents(query.value(3).toLongLong());
        s.discount = Money::fromCents(query.value(4).toLongLong());
        s.total = Money::fromCents(query.value(5).toLongLong());
        s.paymentMethod = query.value(6).toString();
        s.amountPaid = Money::fromCents(query.value(7).toLongLong());
        s.changeDue = Money::fromCents(query.value(8).toLongLong());
        s.cashier = query.value(9).toString();
        s.shiftId = query.value(10).toInt();
        sales.append(s);
    }
    return sales;
}

QVector<SaleItem> SaleRepository::getSaleItems(int saleId)
{
    QVector<SaleItem> items;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, sale_id, product_id, product_name, quantity, price, cost_price, subtotal FROM sale_items WHERE sale_id = ?");
    query.addBindValue(saleId);
    query.exec();
    while (query.next()) {
        SaleItem item;
        item.id = query.value(0).toInt();
        item.saleId = query.value(1).toInt();
        item.productId = query.value(2).toInt();
        item.productName = query.value(3).toString();
        item.quantity = query.value(4).toInt();
        item.price = Money::fromCents(query.value(5).toLongLong());
        item.costPrice = Money::fromCents(query.value(6).toLongLong());
        item.subtotal = Money::fromCents(query.value(7).toLongLong());
        items.append(item);
    }
    return items;
}

Sale SaleRepository::getSaleById(int saleId)
{
    Sale s;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, sale_date, subtotal, tax, discount, total, payment_method, amount_paid, change_due, cashier, shift_id FROM sales WHERE id = ?");
    query.addBindValue(saleId);
    query.exec();
    if (query.next()) {
        s.id = query.value(0).toInt();
        s.saleDate = query.value(1).toDateTime();
        s.subtotal = Money::fromCents(query.value(2).toLongLong());
        s.tax = Money::fromCents(query.value(3).toLongLong());
        s.discount = Money::fromCents(query.value(4).toLongLong());
        s.total = Money::fromCents(query.value(5).toLongLong());
        s.paymentMethod = query.value(6).toString();
        s.amountPaid = Money::fromCents(query.value(7).toLongLong());
        s.changeDue = Money::fromCents(query.value(8).toLongLong());
        s.cashier = query.value(9).toString();
        s.shiftId = query.value(10).toInt();
    }
    return s;
}

int SaleRepository::getSaleIdByExternalRef(const QString &externalRef) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM sales WHERE external_ref = ?");
    query.addBindValue(externalRef);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return -1;
}

bool SaleRepository::setExternalRef(int saleId, const QString &externalRef)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE sales SET external_ref = ? WHERE id = ?");
    query.addBindValue(externalRef);
    query.addBindValue(saleId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

QVector<Sale> SaleRepository::getCustomerPurchaseHistory(int customerId)
{
    QVector<Sale> sales;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, sale_date, subtotal, tax, discount, total, "
              "payment_method, amount_paid, change_due, cashier, shift_id "
              "FROM sales WHERE customer_id = ? ORDER BY sale_date DESC");
    q.addBindValue(customerId);
    if (!q.exec()) return sales;
    while (q.next()) {
        Sale s;
        s.id            = q.value(0).toInt();
        s.saleDate      = q.value(1).toDateTime();
        s.subtotal      = Money::fromCents(q.value(2).toLongLong());
        s.tax           = Money::fromCents(q.value(3).toLongLong());
        s.discount      = Money::fromCents(q.value(4).toLongLong());
        s.total         = Money::fromCents(q.value(5).toLongLong());
        s.paymentMethod = q.value(6).toString();
        s.amountPaid    = Money::fromCents(q.value(7).toLongLong());
        s.changeDue     = Money::fromCents(q.value(8).toLongLong());
        s.cashier       = q.value(9).toString();
        s.shiftId       = q.value(10).toInt();
        sales.append(s);
    }
    return sales;
}
