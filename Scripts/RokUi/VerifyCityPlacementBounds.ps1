param(
    [Parameter(Mandatory = $true)][ValidateSet('Upgrade', 'Replay')][string]$Action,
    [ValidateSet('Legacy', 'Wbp')][string]$Mode = 'Wbp',
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Run = (Get-Date -Format 'yyyyMMdd_HHmmss')
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$saved = Join-Path $project 'Saved/RokUi'
$page = Join-Path $project 'Content/RokUI/Hud/WBP_RokCityPlacementControls.uasset'

function Invoke-BoundsEditor([string]$Name, [string]$Arguments, [string]$Expected) {
    $command = "Rok.UiWbp.UpgradeCityPlacementBounds $Arguments".TrimEnd()
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name $Name -Commands $command
    $log = Join-Path $saved "Logs/$Name.log"
    $failed = Select-String -LiteralPath $log -Pattern 'LogRokCityPlacementBounds: Error:|Ensure condition failed|=== Critical error' -Quiet
    $verified = Select-String -LiteralPath $log -Pattern $Expected -Quiet
    if ($failed -or !$verified) { throw "City placement bounds verification failed. See $log" }
}

if ($Action -eq 'Upgrade') {
    if (!(Test-Path -LiteralPath $page -PathType Leaf)) { throw "Missing placement WBP: $page" }
    $original = (Get-FileHash -LiteralPath $page -Algorithm SHA256).Hash
    Invoke-BoundsEditor "$Run-placement-inspect" '-Inspect' 'city placement bounds (can be added|verified); changed=0'
    if ((Get-FileHash -LiteralPath $page -Algorithm SHA256).Hash -cne $original) { throw "Preflight changed $page" }

    $backup = Join-Path $saved "Backups/CityPlacementBounds_$Run"
    New-Item -ItemType Directory -Path $backup | Out-Null
    Copy-Item -LiteralPath $page -Destination $backup
    $name = Split-Path $page -Leaf
    if ((Get-FileHash -LiteralPath (Join-Path $backup $name) -Algorithm SHA256).Hash -cne $original) {
        throw "Backup does not match $page"
    }
    Set-Content -LiteralPath (Join-Path $backup 'before.sha256') -Value "$original  $name" -Encoding ascii
    Write-Output "Verified backup: $backup"

    Invoke-BoundsEditor "$Run-placement-apply" '' 'city placement bounds verified; changed=[01]'
    $upgraded = (Get-FileHash -LiteralPath $page -Algorithm SHA256).Hash
    Invoke-BoundsEditor "$Run-placement-reload" '-VerifyOnly' 'city placement bounds verified; changed=0'
    Invoke-BoundsEditor "$Run-placement-repeat" '' 'city placement bounds verified; changed=0'
    if ((Get-FileHash -LiteralPath $page -Algorithm SHA256).Hash -cne $upgraded) {
        throw "Independent reload or repeat apply changed $page"
    }
    Write-Output "Saved and independently verified $name SHA256=$upgraded"
    return
}

$output = Join-Path $saved "CityPlacementBoundsAudit/$Run-$Mode"
if (Test-Path -LiteralPath $output) { throw "$output exists. Use a different -Run." }
New-Item -ItemType Directory -Path $output -Force | Out-Null
$testName = 'Rok.Ui.City.Placement.DesignerBounds'
$log = Join-Path $output 'run.log'
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$ueArgs = @(
    (Join-Path $project 'Rok.uproject'), '/Game/Maps/L_RokUnityGameAppCity', '-game',
    '-RenderOffscreen', '-ResX=1440', '-ResY=810', '-ForceRes', '-Unattended', '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding',
    '-DisablePlugins=ModelContextProtocol', '-RokLocalOnly', '-RokNoNet', "-ExecCmds=""Automation RunTests $testName""",
    '-TestExit="Automation Test Queue Empty"', "-ReportExportPath=""$output""", "-abslog=""$log"""
)
if ($Mode -eq 'Legacy') { $ueArgs += '-RokUiLegacy' }
$process = Start-Process $engine -ArgumentList $ueArgs -WorkingDirectory $project -WindowStyle Hidden -PassThru
Write-Output "PID=$($process.Id) report=$output"
if (!$process.WaitForExit(110000)) {
    Stop-Process -Id $process.Id -Force
    throw "City placement replay timed out. See $log"
}
$index = Join-Path $output 'index.json'
if (!(Test-Path -LiteralPath $index -PathType Leaf)) { throw "Missing Automation report. See $log" }
$report = Get-Content -LiteralPath $index -Raw -Encoding utf8 | ConvertFrom-Json
$result = @($report.tests | Where-Object fullTestPath -CEQ $testName)
if ($process.ExitCode -ne 0 -or $result.Count -ne 1 -or $result[0].state -ne 'Success') {
    throw "City placement replay='$($result.state)' exit=$($process.ExitCode). See $log"
}
Write-Output "Success warnings=$($result[0].warnings) errors=$($result[0].errors): $index"
