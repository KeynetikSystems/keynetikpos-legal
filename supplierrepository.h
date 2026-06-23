// =============================================================================
// supplierrepository.h — SupplierRepository: supplier CRUD, split out of Database
// -----------------------------------------------------------------------------
// WHAT: Owns the read/write SQL for the suppliers table: list (optionally
//       including deactivated), fetch by id, add, update, and soft-delete
//       (deactivate). The Supplier struct itself still lives in database.h.
// HOW:  Plain class over a QSqlDatabase held BY VALUE — same pattern as Ledger
//       and SaleRepository — so it shares Database's one open connection
//       without owning or reopening it. lastError() mirrors Database's
//       convention for the write methods.
// WHY:  Second slice of the Database "god object" breakup. Suppliers are a
//       small, fully self-contained table (no cross-table writes), so moving
//       them is low-risk and keeps proving out the facade pattern: Database
//       keeps the same public methods but delegates here, leaving call sites
//       untouched.
// =============================================================================
#ifndef SUPPLIERREPOSITORY_H
#define SUPPLIERREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "database.h"   // Supplier

class SupplierRepository
{
public:
    explicit SupplierRepository(QSqlDatabase db);

    QVector<Supplier> getAllSuppliers(bool includeInactive = false);
    Supplier getSupplierById(int id);
    bool addSupplier(const Supplier &supplier);
    bool updateSupplier(const Supplier &supplier);
    // Soft delete: suppliers are referenced by historical purchase orders, so
    // they're deactivated (hidden from pickers) rather than removed.
    bool deactivateSupplier(int id);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase    m_db;
    QString         m_lastError;
};

#endif // SUPPLIERREPOSITORY_H
