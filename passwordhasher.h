// =============================================================================
// passwordhasher.h — PasswordHasher: salted PBKDF2 hashing for passwords/PINs
// -----------------------------------------------------------------------------
// WHAT: Produces and verifies salted PBKDF2-SHA256 hashes in the format
//       "pbkdf2-sha256$<iterations>$<salt-hex>$<hash-hex>", with backward
//       compatibility for legacy unsalted SHA-256 hex digests.
// HOW:  hash() draws a 16-byte salt from the OS CSPRNG and derives a 32-byte
//       key with QPasswordDigestor at 100,000 iterations. verify() parses the
//       stored format (or falls back to legacy SHA-256), re-derives, and
//       compares with constantTimeEquals() — a XOR-accumulator loop that takes
//       the same time wherever the mismatch occurs. needsRehash() flags legacy
//       hashes so callers can upgrade them on successful login.
// WHY:  Unsalted fast hashes are trivially crackable with rainbow tables;
//       per-credential salts + a high iteration count make offline cracking
//       expensive. Constant-time comparison closes the timing side-channel of
//       early '==' returns. Embedding the iteration count in the stored string
//       lets the cost be raised later without breaking existing hashes. It is
//       a stateless inline namespace so both UserManager and SettingsManager
//       (discount PIN) can use it without link-order concerns.
// =============================================================================
#pragma once

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>

// Salted PBKDF2-SHA256 hashing for passwords and PINs.
// Stored format: "pbkdf2-sha256$<iterations>$<salt-hex>$<hash-hex>"
// verify() also accepts the legacy unsalted SHA-256 hex format so existing
// credentials keep working; callers should rehash when needsRehash() is true.
namespace PasswordHasher {

constexpr int ITERATIONS = 100000;
constexpr int SALT_BYTES = 16;
constexpr int KEY_BYTES  = 32;

inline bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}

inline bool isPbkdf2(const QString &stored)
{
    return stored.startsWith(QLatin1String("pbkdf2-sha256$"));
}

inline QString hash(const QString &secret)
{
    QByteArray salt(SALT_BYTES, '\0');
    QRandomGenerator::system()->fillRange(
        reinterpret_cast<quint32 *>(salt.data()), SALT_BYTES / sizeof(quint32));

    const QByteArray key = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, secret.toUtf8(), salt, ITERATIONS, KEY_BYTES);

    return QStringLiteral("pbkdf2-sha256$%1$%2$%3")
        .arg(ITERATIONS)
        .arg(QString::fromLatin1(salt.toHex()),
             QString::fromLatin1(key.toHex()));
}

inline bool verify(const QString &secret, const QString &stored)
{
    if (stored.isEmpty())
        return false;

    if (!isPbkdf2(stored)) {
        // Legacy: unsalted SHA-256 hex digest
        const QByteArray legacy = QCryptographicHash::hash(
            secret.toUtf8(), QCryptographicHash::Sha256).toHex();
        return constantTimeEquals(legacy, stored.toLower().toUtf8());
    }

    const QStringList parts = stored.split(QLatin1Char('$'));
    if (parts.size() != 4)
        return false;

    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    if (!ok || iterations <= 0)
        return false;

    const QByteArray salt     = QByteArray::fromHex(parts.at(2).toUtf8());
    const QByteArray expected = QByteArray::fromHex(parts.at(3).toUtf8());
    if (salt.isEmpty() || expected.isEmpty())
        return false;

    const QByteArray key = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, secret.toUtf8(), salt,
        iterations, expected.size());
    return constantTimeEquals(key, expected);
}

inline bool needsRehash(const QString &stored)
{
    return !isPbkdf2(stored);
}

} // namespace PasswordHasher
