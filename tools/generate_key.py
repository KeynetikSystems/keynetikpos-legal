#!/usr/bin/env python3
"""
Offline CD-key generator/verifier for KeynetikPOS.

Produces keys that pass LicenseManager::validateKey() in licensemanager.cpp:
  - 16 chars (shown as XXXX-XXXX-XXXX-XXXX)
  - segment 2 (chars 5-8) = first 4 hex chars of
    SHA256(seg0 + seg2 + "KNK-SALT-2025"), uppercased

NOTE: keys made here pass the client's OFFLINE format check (local
activation). For ONLINE activation they must also exist in the licensing
server's database — create those via the server's POST /admin/keys instead
(see server/licensing-worker/README.md), or INSERT these manually.

Usage:
  python tools/generate_key.py            # generate 1 key
  python tools/generate_key.py 10         # generate 10 keys
  python tools/generate_key.py --check ABCD-1F2E-WXYZ-7Q8R
"""
import hashlib
import secrets
import sys

KEY_SALT = "KNK-SALT-2025"  # must match licensemanager.cpp and worker.js
CHARSET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"  # no 0/O/1/I/L


def _segment(n: int) -> str:
    return "".join(secrets.choice(CHARSET) for _ in range(n))


def _checksum_segment(seg0: str, seg2: str) -> str:
    digest = hashlib.sha256((seg0 + seg2 + KEY_SALT).encode()).hexdigest()
    return digest[:4].upper()


def generate() -> str:
    seg0, seg2, tail = _segment(4), _segment(4), _segment(4)
    seg1 = _checksum_segment(seg0, seg2)
    raw = seg0 + seg1 + seg2 + tail
    return "-".join(raw[i:i + 4] for i in range(0, 16, 4))


def check(key: str) -> bool:
    # Mirrors validateKey(): uppercase, strip dashes, 16 chars, hash prefix test
    clean = key.upper().replace("-", "").strip()
    if len(clean) != 16:
        return False
    seg0, seg1, seg2 = clean[0:4], clean[4:8], clean[8:12]
    digest = hashlib.sha256((seg0 + seg2 + KEY_SALT).encode()).hexdigest()
    return digest.startswith(seg1.lower())


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--check":
        ok = check(args[1] if len(args) > 1 else "")
        print("VALID" if ok else "INVALID")
        sys.exit(0 if ok else 1)
    count = int(args[0]) if args else 1
    for _ in range(count):
        key = generate()
        assert check(key)
        print(key)
