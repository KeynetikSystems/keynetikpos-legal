// =============================================================================
// etims.h — eTIMS transmission interface + local stub
// -----------------------------------------------------------------------------
// WHAT: The seam for KRA eTIMS (electronic Tax Invoice Management System).
//       IEtimsClient is the abstract contract the POS calls after a sale;
//       StubEtimsClient queues invoices locally and marks them "stubbed".
// HOW:  Real transmission goes through a KRA-certified OSCU/VSCU device that
//       must be onboarded per-PIN with KRA — that can't be done from here, so
//       a concrete client is left as a future implementation behind this
//       interface. The stub records intent so the wiring + audit trail exist.
// WHY:  Keeping eTIMS behind an interface means the checkout path can call it
//       today and a certified client can drop in later without touching POS code.
// =============================================================================
#ifndef ETIMS_H
#define ETIMS_H

#include <QString>
#include <QSqlDatabase>

struct EtimsResult {
    bool    accepted = false;
    QString invoiceNumber;   // KRA control number when accepted
    QString message;
};

class IEtimsClient
{
public:
    virtual ~IEtimsClient() = default;
    // Transmit a recorded sale to eTIMS. Implementations must be idempotent
    // per saleId (re-sending the same sale must not double-file).
    virtual EtimsResult transmitSale(int saleId) = 0;
    virtual bool        isOnboarded() const = 0;
};

// Local stub: persists each attempt to an etims_log table with status
// 'stubbed' and returns not-onboarded. No data leaves the device.
class StubEtimsClient : public IEtimsClient
{
public:
    explicit StubEtimsClient(QSqlDatabase db);
    bool        initSchema();
    EtimsResult transmitSale(int saleId) override;
    bool        isOnboarded() const override { return false; }

private:
    QSqlDatabase m_db;
};

#endif // ETIMS_H
