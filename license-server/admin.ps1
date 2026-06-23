# =============================================================================
# admin.ps1 — PowerShell-native admin client for the KeynetikPOS license server
# -----------------------------------------------------------------------------
# WHAT: Register, revoke, and list license keys without fighting curl syntax
#       (PowerShell's `curl` is an alias for Invoke-WebRequest, so the README's
#       curl flags don't work here — use this instead).
# HOW:  Wraps Invoke-RestMethod with the Bearer token. Set the two variables
#       below once (or pass -BaseUrl / -Token), then call the functions.
# WHY:  Day-to-day key management from the same machine you build on.
#
# Usage:
#   . .\admin.ps1                              # dot-source to load the functions
#   New-License -Key "ABCD-1F2E-WXYZ-9876" -Tier 3 -MaxDevices 2 -Note "Mama Njeri, Nakuru"
#   Get-Licenses
#   Revoke-License -Key "ABCD-1F2E-WXYZ-9876"
# =============================================================================

# ── Configure these two once ────────────────────────────────────────────────
$script:BaseUrl = "https://keynetik-license.<account>.workers.dev"   # your deployed Worker URL
$script:Token   = "PASTE_YOUR_ADMIN_TOKEN_HERE"

function New-License {
    param(
        [Parameter(Mandatory)][string]$Key,
        [int]$MaxDevices = 1,
        [ValidateRange(1,4)][int]$Tier = 1,   # 1 POS Core .. 4 ERP Full
        [string]$ExpiresAt,            # e.g. "2027-01-01" — omit for perpetual
        [string]$Note,
        [string]$BaseUrl = $script:BaseUrl,
        [string]$Token   = $script:Token
    )
    $body = @{ key = $Key; max_devices = $MaxDevices; tier = $Tier }
    if ($ExpiresAt) { $body.expires_at = $ExpiresAt }
    if ($Note)      { $body.note       = $Note }
    Invoke-RestMethod -Method Post -Uri "$BaseUrl/admin/keys" `
        -Headers @{ Authorization = "Bearer $Token" } `
        -ContentType "application/json" `
        -Body ($body | ConvertTo-Json -Compress)
}

function Revoke-License {
    param(
        [Parameter(Mandatory)][string]$Key,
        [string]$BaseUrl = $script:BaseUrl,
        [string]$Token   = $script:Token
    )
    $enc = [uri]::EscapeDataString($Key)
    Invoke-RestMethod -Method Post -Uri "$BaseUrl/admin/revoke?key=$enc" `
        -Headers @{ Authorization = "Bearer $Token" }
}

function Get-Licenses {
    param(
        [string]$BaseUrl = $script:BaseUrl,
        [string]$Token   = $script:Token
    )
    (Invoke-RestMethod -Method Get -Uri "$BaseUrl/admin/list" `
        -Headers @{ Authorization = "Bearer $Token" }).keys |
        Format-Table key, tier, devices_used, max_devices, revoked, expires_at, note -AutoSize
}
