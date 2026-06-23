// =============================================================================
// supplierrepository.cpp — Implementation of SupplierRepository (see
// supplierrepository.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Bodies were lifted verbatim from Database so behaviour is unchanged; the
//    only edits are db -> m_db and lastError -> m_lastError.
//  - deactivateSupplier is a soft delete (is_active = 0) because purchase
//    orders reference suppliers by id.
// =============================================================================
#include "supplierrepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

SupplierRepository::SupplierRepository(QSqlDatabase db)
    : m_db(db)
{
}

QVector<Supplier> SupplierRepository::getAllSuppliers(bool includeInactive)
{
    QVector<Supplier> suppliers;
    QSqlQuery query(m_db);
    query.exec(includeInactive
                   ? "SELECT id, name, contact_person, phone, email, address, is_active "
                     "FROM suppliers ORDER BY name"
                   : "SELECT id, name, contact_person, phone, email, address, is_active "
                     "FROM suppliers WHERE is_active = 1 ORDER BY name");
    while (query.next()) {
        Supplier s;
        s.id            = query.value(0).toInt();
        s.name          = query.value(1).toString();
        s.contactPerson = query.value(2).toString();
        s.phone         = query.value(3).toString();
        s.email         = query.value(4).toString();
        s.address       = query.value(5).toString();
        s.isActive      = query.value(6).toBool();
        suppliers.append(s);
    }
    return suppliers;
}

Supplier SupplierRepository::getSupplierById(int id)
{
    Supplier s;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, contact_person, phone, email, address, is_active "
                  "FROM suppliers WHERE id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        s.id            = query.value(0).toInt();
        s.name          = query.value(1).toString();
        s.contactPerson = query.value(2).toString();
        s.phone         = query.value(3).toString();
        s.email         = query.value(4).toString();
        s.address       = query.value(5).toString();
        s.isActive      = query.value(6).toBool();
    }
    return s;
}

bool SupplierRepository::addSupplier(const Supplier &supplier)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO suppliers (name, contact_person, phone, email, address, is_active) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(supplier.name);
    query.addBindValue(supplier.contactPerson);
    query.addBindValue(supplier.phone);
    query.addBindValue(supplier.email);
    query.addBindValue(supplier.address);
    query.addBindValue(supplier.isActive);
    if (!query.exec()) {
        m_lastError = "Failed to add supplier: " + query.lastError().text();
        return false;
    }
    return true;
}

bool SupplierRepository::updateSupplier(const Supplier &supplier)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE suppliers SET name = ?, contact_person = ?, phone = ?, "
                  "email = ?, address = ?, is_active = ? WHERE id = ?");
    query.addBindValue(supplier.name);
    query.addBindValue(supplier.contactPerson);
    query.addBindValue(supplier.phone);
    query.addBindValue(supplier.email);
    query.addBindValue(supplier.address);
    query.addBindValue(supplier.isActive);
    query.addBindValue(supplier.id);
    if (!query.exec()) {
        m_lastError = "Failed to update supplier: " + query.lastError().text();
        return false;
    }
    return true;
}

bool SupplierRepository::deactivateSupplier(int id)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE suppliers SET is_active = 0 WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec()) {
        m_lastError = "Failed to deactivate supplier: " + query.lastError().text();
        return false;
    }
    return true;
}
