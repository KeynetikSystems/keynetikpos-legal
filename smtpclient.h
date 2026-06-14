// =============================================================================
// smtpclient.h — SmtpClient: a minimal synchronous SMTP sender
// -----------------------------------------------------------------------------
// WHAT: Sends a single HTML email through an authenticated SMTP server. Supports
//       the three real-world connection modes — plain, STARTTLS (port 587), and
//       implicit TLS/SSL (port 465) — plus AUTH LOGIN, which is what Gmail,
//       Outlook/Office365 and most providers expect with an app password.
// HOW:  Drives the SMTP conversation synchronously over a QSslSocket using
//       waitFor*() with a per-step timeout. Qt ships no SMTP class, so the
//       EHLO/STARTTLS/AUTH/MAIL/RCPT/DATA handshake is hand-rolled here. The
//       call blocks (seconds at most); callers should show a wait cursor.
// WHY:  Email receipts were a stub that only wrote HTML to disk. A small,
//       dependency-free client (QtNetwork only, already linked) makes real
//       sending possible without pulling in a third-party library.
// =============================================================================
#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include <QString>

struct SmtpConfig {
    enum Security { None, StartTls, Ssl };

    QString  host;
    int      port      = 587;
    Security security  = StartTls;
    QString  username;
    QString  password;
    QString  fromEmail;
    QString  fromName;

    bool isConfigured() const { return !host.isEmpty() && !fromEmail.isEmpty(); }
};

class SmtpClient
{
public:
    explicit SmtpClient(const SmtpConfig &config);

    // Sends one HTML email. Returns true on success; on failure returns false
    // and sets *error (if given) to a human-readable reason.
    bool send(const QString &toEmail,
              const QString &subject,
              const QString &htmlBody,
              QString *error = nullptr);

private:
    SmtpConfig m_config;
};

#endif // SMTPCLIENT_H
