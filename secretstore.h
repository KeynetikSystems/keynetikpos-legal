// =============================================================================
// secretstore.h — At-rest encryption for small secrets (e.g. the SMTP password)
// -----------------------------------------------------------------------------
// WHAT: Encrypt/decrypt a short secret before it is written to the local
//       settings database, so credentials aren't stored in clear text.
// HOW:  On Windows the secret is protected with DPAPI (CryptProtectData) scoped
//       to the current user account, then base64-encoded with a version marker.
//       Decrypt reverses it; an unmarked value is treated as legacy plaintext
//       and returned unchanged so existing settings keep working (and get
//       re-encrypted on the next save). On non-Windows builds (CI/tests) the
//       functions are pass-throughs — the shipping platform is Windows.
// WHY:  A POS terminal is shared; a cashier with file access shouldn't be able
//       to read the mailbox app-password straight out of pos_database.db.
//       DPAPI ties the ciphertext to the Windows user, requiring no key
//       management from us.
// =============================================================================
#ifndef SECRETSTORE_H
#define SECRETSTORE_H

#include <QString>

namespace SecretStore {

// Encrypts a secret for storage. Empty input yields empty output. On any
// failure the plaintext is returned (never silently dropped).
QString encrypt(const QString &plain);

// Inverse of encrypt(). A value not produced by encrypt() (legacy plaintext)
// is returned unchanged. Returns empty if decryption fails (e.g. the blob was
// created by a different Windows user).
QString decrypt(const QString &stored);

// True if `stored` is in this module's ciphertext format (vs. legacy plaintext).
bool isEncrypted(const QString &stored);

} // namespace SecretStore

#endif // SECRETSTORE_H
