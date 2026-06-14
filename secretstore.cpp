// =============================================================================
// secretstore.cpp — Implementation of SecretStore (see secretstore.h).
// -----------------------------------------------------------------------------
// The Windows path uses DPAPI (CryptProtectData/CryptUnprotectData) so the
// ciphertext is bound to the current user account; no key material lives in the
// app. windows.h is confined to this translation unit to keep its macros out of
// the rest of the codebase.
// =============================================================================
#include "secretstore.h"

#include <QByteArray>
#include <QLatin1String>

namespace {
// Version-tagged so the format can evolve without misreading old blobs.
const QString kPrefix = QStringLiteral("dpapi:v1:");
}

namespace SecretStore {

bool isEncrypted(const QString &stored)
{
    return stored.startsWith(kPrefix);
}

} // namespace SecretStore

#ifdef Q_OS_WIN

#include <windows.h>
#include <wincrypt.h>

namespace SecretStore {

QString encrypt(const QString &plain)
{
    if (plain.isEmpty())
        return QString();

    const QByteArray in = plain.toUtf8();
    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(in.constData()));
    inBlob.cbData = static_cast<DWORD>(in.size());

    DATA_BLOB outBlob = {};
    if (!CryptProtectData(&inBlob, L"KeynetikPOS secret", nullptr, nullptr,
                          nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        // Protection unavailable — fall back to plaintext rather than losing
        // the credential. isEncrypted() will report false so it migrates later.
        return plain;
    }

    const QByteArray enc(reinterpret_cast<const char *>(outBlob.pbData),
                         static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return kPrefix + QString::fromLatin1(enc.toBase64());
}

QString decrypt(const QString &stored)
{
    if (!isEncrypted(stored))
        return stored;   // legacy plaintext — hand back unchanged

    const QByteArray enc =
        QByteArray::fromBase64(stored.mid(kPrefix.size()).toLatin1());

    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(enc.constData()));
    inBlob.cbData = static_cast<DWORD>(enc.size());

    DATA_BLOB outBlob = {};
    if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &outBlob))
        return QString();

    const QString result = QString::fromUtf8(
        reinterpret_cast<const char *>(outBlob.pbData),
        static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return result;
}

} // namespace SecretStore

#else   // non-Windows (CI / tests): no platform secret store available

namespace SecretStore {

QString encrypt(const QString &plain) { return plain; }

QString decrypt(const QString &stored)
{
    return isEncrypted(stored) ? stored.mid(kPrefix.size()) : stored;
}

} // namespace SecretStore

#endif
