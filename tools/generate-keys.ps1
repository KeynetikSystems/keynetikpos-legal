# =============================================================================
# generate-keys.ps1 — Mint valid KeynetikPOS CD keys
# -----------------------------------------------------------------------------
# WHAT: Generates keys in the XXXX-XXXX-XXXX-XXXX format that
#       LicenseManager::validateKey() accepts.
# HOW:  Segment 2 (chars 5-8) is a checksum: the first 4 hex chars of
#       SHA256(seg0 + seg2 + "KNK-SALT-2025"). Segments 0, 2, 3 are random
#       from a charset that avoids look-alike characters (no 0/O, 1/I/L).
# WHY:  The client validates this format OFFLINE before any server call, so
#       random typos fail instantly without a network round-trip. NOTE: the
#       salt is embedded in the shipped binary; offline validation alone is a
#       speed bump, not real protection — revocation and device limits come
#       from the server (see license-server/).
#
# Usage:
#   .\generate-keys.ps1                 # one key
#   .\generate-keys.ps1 -Count 20      # twenty keys
# =============================================================================
param(
    [int]$Count = 1,
    [string]$Salt = "KNK-SALT-2025"   # must match KNK-SALT in licensemanager.cpp
)

$charset = "ABCDEFGHJKMNPQRSTUVWXYZ23456789".ToCharArray()
$sha = [System.Security.Cryptography.SHA256]::Create()

function New-Segment {
    -join (1..4 | ForEach-Object { $charset | Get-Random })
}

for ($i = 0; $i -lt $Count; $i++) {
    $seg0 = New-Segment
    $seg2 = New-Segment
    $seg3 = New-Segment

    $hashBytes = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes("$seg0$seg2$Salt"))
    $hashHex   = ($hashBytes | ForEach-Object { $_.ToString('x2') }) -join ''
    $seg1      = $hashHex.Substring(0, 4).ToUpper()

    "$seg0-$seg1-$seg2-$seg3"
}
