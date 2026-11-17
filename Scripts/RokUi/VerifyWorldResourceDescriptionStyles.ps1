param(
    [Parameter(Mandatory = $true)][ValidateSet('Apply', 'Verify', 'Replay')][string]$Action,
    [ValidateSet('Legacy', 'Wbp')][string]$Mode = 'Wbp',
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Run = (Get-Date -Format 'yyyyMMdd_HHmmss')
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$saved = Join-Path $project 'Saved/RokUi'
$page = Join-Path $project 'Content/RokUI/Hud/WBP_RokWorldObjectInfo.uasset'
$table = Join-Path $project 'Content/RokUI/Hud/DT_RokWorldResourceDescriptionStyles.uasset'

function Invoke-StyleEditor([string]$Name, [string]$Command) {
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name $Name -Commands $Command
    $log = Join-Path $saved "Logs/$Name.log"
    $failed = Select-String -LiteralPath $log -Pattern 'LogRokWorldResourceDescriptionStyle: Error:|Ensure condition failed|=== Critical error' -Quiet
    $verified = Select-String -LiteralPath $log -Pattern 'world resource description style verified: .*; changed=[01]' -Quiet
    if ($failed -or !$verified) { throw "World description style verification failed. See $log" }
}

if ($Action -in 'Apply', 'Verify') {
    if (!(Test-Path -LiteralPath $page -PathType Leaf)) { throw "Missing WorldObjectInfo WBP: $page" }
    $command = 'Rok.UiWbp.UpgradeWorldResourceDescriptionStyle'
    if ($Action -eq 'Apply') {
        $stamp = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
        $backup = Join-Path $saved "Backups/WorldResourceDescriptionStyles_$stamp"
        New-Item -ItemType Directory -Path $backup | Out-Null
        $hashes = foreach ($file in $page, $table) {
            if (Test-Path -LiteralPath $file -PathType Leaf) {
                $name = Split-Path $file -Leaf
                $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
                Copy-Item -LiteralPath $file -Destination $backup
                if ((Get-FileHash -LiteralPath (Join-Path $backup $name) -Algorithm SHA256).Hash -cne $hash) {
                    throw "Backup mismatch: $file"
                }
                "$hash  $name"
            }
        }
        Set-Content -LiteralPath (Join-Path $backup 'before.sha256') -Value $hashes -Encoding ascii
        Write-Output "Verified original asset backup: $backup"
        Invoke-StyleEditor "$Run-apply-world-resource-style" $command
        return
    }

    $files = @($page, $table | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    $before = $files | Get-FileHash -Algorithm SHA256
    Invoke-StyleEditor "$Run-verify-world-resource-style" "$command -VerifyOnly"
    # Apply again in a separate commandlet to verify the normal updater is idempotent.
    Invoke-StyleEditor "$Run-repeat-world-resource-style" $command
    $after = $files | Get-FileHash -Algorithm SHA256
    if (Compare-Object $before $after -Property Path,Hash) { throw 'Verifying or repeating the style updater changed an asset.' }
    Write-Output 'Independent load and repeat apply left asset hashes unchanged.'
    return
}

$output = Join-Path $saved "WorldResourceDescriptionStyleAudit/$Run-$Mode"
if (Test-Path -LiteralPath $output) { throw "$output exists. Use a different -Run." }
New-Item -ItemType Directory -Path $output -Force | Out-Null
$testName = 'Rok.Ui.World.ResourceDescriptionStyles'
$log = Join-Path $output 'run.log'
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$ueArgs = @(
    (Join-Path $project 'Rok.uproject'), '/Game/Maps/L_RokUnityWorldMapPrototype', '-game',
    '-RenderOffscreen', '-ResX=1440', '-ResY=810', '-ForceRes', '-Unattended', '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding',
    '-DisablePlugins=ModelContextProtocol', '-RokLocalOnly', '-RokNoNet', "-ExecCmds=""Automation RunTests $testName""",
    '-TestExit="Automation Test Queue Empty"', "-ReportExportPath=""$output""", "-abslog=""$log"""
)
if ($Mode -eq 'Legacy') { $ueArgs += '-RokUiLegacy' }
$process = Start-Process $engine -ArgumentList $ueArgs -WorkingDirectory $project -WindowStyle Hidden -PassThru
Write-Output "PID=$($process.Id) report=$output"
if (!$process.WaitForExit(120000)) {
    Stop-Process -Id $process.Id -Force
    throw "World resource description replay timed out. See $log"
}
$index = Join-Path $output 'index.json'
if (!(Test-Path -LiteralPath $index -PathType Leaf)) { throw "Missing Automation report. See $log" }
$report = Get-Content -LiteralPath $index -Raw -Encoding utf8 | ConvertFrom-Json
$result = @($report.tests | Where-Object fullTestPath -CEQ $testName)
if ($process.ExitCode -ne 0 -or $result.Count -ne 1 -or $result[0].state -ne 'Success') {
    throw "World description replay='$($result.state)' exit=$($process.ExitCode). See $log"
}
Write-Output "Success warnings=$($result[0].warnings) errors=$($result[0].errors): $index"
