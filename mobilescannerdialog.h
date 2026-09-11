// =============================================================================
// mobilescannerdialog.h — MobileScannerDialog: pairing/status UI for the LAN
// mobile-scanner API
// -----------------------------------------------------------------------------
// WHAT: Shows the till's LAN address(es), port, current pairing PIN, and
//       whether a phone is currently paired. "Regenerate PIN" forces
//       re-pairing (e.g. to disconnect a previous phone).
// HOW:  Modeless, single-instance — same QPointer + Qt::WA_DeleteOnClose
//       pattern as InventoryDialog, so a cashier can leave it open (or glance
//       at it) without it blocking checkout. Does not own the PosApiServer.
// WHY:  A cashier needs to read a PIN off a screen and know whether a scan is
//       currently reaching this till — this is the only UI for that.
// =============================================================================
#ifndef MOBILESCANNERDIALOG_H
#define MOBILESCANNERDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;
class PosApiServer;

class MobileScannerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MobileScannerDialog(PosApiServer *server, QWidget *parent = nullptr);

private slots:
    void onRegenerateClicked();
    void refreshStatus();

private:
    void setupUI();

    PosApiServer *m_server;   // not owned

    QLabel      *m_addressLabel  { nullptr };
    QLabel      *m_pinLabel      { nullptr };
    QLabel      *m_statusChip    { nullptr };
    QPushButton *m_regenerateBtn { nullptr };
    QPushButton *m_closeBtn      { nullptr };
};

#endif // MOBILESCANNERDIALOG_H
