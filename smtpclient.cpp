// =============================================================================
// smtpclient.cpp — Implementation of SmtpClient (see smtpclient.h).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - One QSslSocket handles all three modes: SSL connects encrypted up front;
//    STARTTLS connects plain then upgrades in place; None stays plain.
//  - command()/expect() wrap the request/response pattern: every SMTP reply is
//    a 3-digit code, possibly multi-line (a '-' after the code continues it, a
//    space ends it). We read until the final line for the current code.
//  - The DATA body is dot-stuffed (a leading '.' on any line is doubled) and
//    CRLF-terminated, per RFC 5321, so an HTML line starting with '.' can't end
//    the message early.
// =============================================================================
#include "smtpclient.h"

#include <QSslSocket>
#include <QByteArray>
#include <QStringList>

namespace {
constexpr int kTimeoutMs = 15000;

QString encodeHeaderAddress(const QString &name, const QString &email)
{
    if (name.trimmed().isEmpty())
        return QString("<%1>").arg(email);
    return QString("\"%1\" <%2>").arg(name, email);
}
}

SmtpClient::SmtpClient(const SmtpConfig &config)
    : m_config(config)
{
}

bool SmtpClient::send(const QString &toEmail, const QString &subject,
                      const QString &htmlBody, QString *error)
{
    auto fail = [&](const QString &msg) {
        if (error) *error = msg;
        return false;
    };

    if (!m_config.isConfigured())
        return fail("SMTP is not configured. Set the mail server in Settings.");
    if (toEmail.trimmed().isEmpty())
        return fail("No recipient email address.");

    QSslSocket socket;

    // ── Connect (encrypted immediately for SSL, plain otherwise) ─────────────
    if (m_config.security == SmtpConfig::Ssl) {
        socket.connectToHostEncrypted(m_config.host, m_config.port);
        if (!socket.waitForEncrypted(kTimeoutMs))
            return fail("TLS connection failed: " + socket.errorString());
    } else {
        socket.connectToHost(m_config.host, m_config.port);
        if (!socket.waitForConnected(kTimeoutMs))
            return fail("Could not connect to " + m_config.host + ": "
                        + socket.errorString());
    }

    // Reads one full SMTP reply and checks its leading code matches `expected`.
    auto expect = [&](int expected, QString *replyOut = nullptr) -> bool {
        QByteArray reply;
        while (true) {
            if (!socket.waitForReadyRead(kTimeoutMs))
                return false;
            reply += socket.readAll();
            // A complete reply ends with "<code><space>...\r\n"; while the 4th
            // char after the last line's code is '-', more lines are coming.
            const int lastNl = reply.lastIndexOf('\n');
            if (lastNl < 0) continue;
            int lineStart = reply.lastIndexOf('\n', lastNl - 1) + 1;
            const QByteArray lastLine = reply.mid(lineStart);
            if (lastLine.size() >= 4 && lastLine[3] == ' ')
                break;   // final line of the reply
        }
        if (replyOut) *replyOut = QString::fromUtf8(reply);
        return reply.left(3).toInt() == expected;
    };

    // Sends a command line and validates the response code.
    auto command = [&](const QByteArray &line, int expected) -> bool {
        socket.write(line + "\r\n");
        if (!socket.waitForBytesWritten(kTimeoutMs)) return false;
        return expect(expected);
    };

    if (!expect(220))
        return fail("SMTP server did not greet the connection.");

    const QByteArray ehlo = "EHLO keynetikpos";
    if (!command(ehlo, 250))
        return fail("EHLO was rejected by the server.");

    // ── Upgrade to TLS for STARTTLS, then re-EHLO over the secure channel ────
    if (m_config.security == SmtpConfig::StartTls) {
        if (!command("STARTTLS", 220))
            return fail("Server did not accept STARTTLS.");
        socket.startClientEncryption();
        if (!socket.waitForEncrypted(kTimeoutMs))
            return fail("STARTTLS handshake failed: " + socket.errorString());
        if (!command(ehlo, 250))
            return fail("EHLO after STARTTLS was rejected.");
    }

    // ── AUTH LOGIN (username/password each base64-encoded) ───────────────────
    if (!m_config.username.isEmpty()) {
        if (!command("AUTH LOGIN", 334))
            return fail("Server did not accept AUTH LOGIN.");
        if (!command(m_config.username.toUtf8().toBase64(), 334))
            return fail("SMTP username was rejected.");
        if (!command(m_config.password.toUtf8().toBase64(), 235))
            return fail("Authentication failed — check the username/password "
                        "(many providers require an app password).");
    }

    // ── Envelope ─────────────────────────────────────────────────────────────
    if (!command("MAIL FROM:<" + m_config.fromEmail.toUtf8() + ">", 250))
        return fail("Server rejected the sender address.");
    if (!command("RCPT TO:<" + toEmail.trimmed().toUtf8() + ">", 250))
        return fail("Server rejected the recipient address.");
    if (!command("DATA", 354))
        return fail("Server did not accept the DATA command.");

    // ── Message (headers + dot-stuffed HTML body) ────────────────────────────
    QByteArray message;
    message += "From: " + encodeHeaderAddress(m_config.fromName,
                                              m_config.fromEmail).toUtf8() + "\r\n";
    message += "To: <" + toEmail.trimmed().toUtf8() + ">\r\n";
    message += "Subject: " + subject.toUtf8() + "\r\n";
    message += "MIME-Version: 1.0\r\n";
    message += "Content-Type: text/html; charset=UTF-8\r\n";
    message += "Content-Transfer-Encoding: 8bit\r\n";
    message += "\r\n";

    const QStringList lines = htmlBody.split('\n');
    for (QString line : lines) {
        if (line.endsWith('\r')) line.chop(1);
        QByteArray l = line.toUtf8();
        if (l.startsWith('.')) l.prepend('.');   // dot-stuffing
        message += l + "\r\n";
    }
    message += ".\r\n";

    socket.write(message);
    if (!socket.waitForBytesWritten(kTimeoutMs) || !expect(250))
        return fail("The server rejected the message body.");

    command("QUIT", 221);   // best-effort; the mail is already accepted
    socket.disconnectFromHost();
    return true;
}
