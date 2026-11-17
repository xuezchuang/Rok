# Update the three existing flag editor assets after validating and backing up their current bytes.
param(
    [switch]$ValidateOnly,
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'
$editor = Join-Path $PSScriptRoot 'EditorCmd.ps1'
$project = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$command = 'Rok.UiWbp.UpgradeAllianceFlagStyles'
$assets = @('WBP_RokFeature_AllianceFlagEditor', 'WBP_RokFeature_AllianceFlagSpriteOption', 'WBP_RokFeature_AllianceFlagColorOption')
$widgetError = 'Handled ensure|Ensure condition failed|Log(?:Widget)?Blueprint[^\r\n]*Error:'

if ($VerifyOnly) {
    $verify = & $editor -Name alliance_flag_styles_verify -Commands "$command VerifyOnly"
    $verify | Write-Output
    $verifyText = $verify -join "`n"
    if ($verifyText -notmatch 'alliance flag styles verification passed pending=0' -or $verifyText -match $widgetError) {
        throw 'Flag selection style verification failed.'
    }
    return
}

$preflight = & $editor -Name alliance_flag_styles_preflight -Commands "$command ValidateOnly"
$preflight | Write-Output
if (($preflight -join "`n") -notmatch 'alliance flag styles preflight passed pending=(\d+)') {
    throw 'Flag selection style preflight failed.'
}
if ($ValidateOnly -or [int]$Matches[1] -eq 0) { return }

$backupId = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$backupDir = Join-Path $project "Saved/RokUi/AllianceFlagStyleUpgradeBackups/$backupId"
New-Item -ItemType Directory -Path $backupDir | Out-Null
$backupReport = foreach ($name in $assets) {
    $path = Join-Path $project "Content/RokUI/Feature/$name.uasset"
    $file = Get-Item -LiteralPath $path
    if ($file.IsReadOnly) { throw "Flag asset is read-only: $path" }
    $backup = Join-Path $backupDir $file.Name
    Copy-Item -LiteralPath $path -Destination $backup
    $hash = (Get-FileHash -LiteralPath $path).Hash
    if ((Get-FileHash -LiteralPath $backup).Hash -ne $hash) { throw "Flag asset backup differs: $name" }
    [pscustomobject]@{ Asset = $path; Backup = $backup; BeforeSha256 = $hash }
}
$backupReport | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $backupDir 'before.json')
Write-Output "Flag assets backed up to $backupDir"

$upgrade = & $editor -Name alliance_flag_styles_apply -Commands "$command Apply $backupId"
$upgrade | Write-Output
$upgradeText = $upgrade -join "`n"
if ($upgradeText -notmatch 'alliance flag styles upgrade saved assets=\d+ styles=\d+' -or $upgradeText -match $widgetError) {
    throw "Flag style upgrade did not report success. The backup is at $backupDir."
}
& $PSCommandPath -VerifyOnly
$backupReport | ForEach-Object {
    [pscustomobject]@{ Asset = $_.Asset; BeforeSha256 = $_.BeforeSha256; AfterSha256 = (Get-FileHash -LiteralPath $_.Asset).Hash }
} | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $backupDir 'after.json')
