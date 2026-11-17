param(
    [Parameter(Mandatory = $true)][ValidateSet('Export', 'Generate', 'Replay')][string]$Action,
    [ValidateSet('Legacy', 'Wbp')][string]$Mode = 'Wbp',
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Run = (Get-Date -Format 'yyyyMMdd_HHmmss')
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$saved = Join-Path $project 'Saved/RokUi'
if ($Action -eq 'Generate') {
    $existing = Join-Path $project 'Content/RokUI/Scout/WBP_RokScoutScreen.uasset'
    $target = Join-Path $project 'Content/RokUI/Scout/WBP_RokScoutCatalogUnavailable.uasset'
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        Write-Output "Existing notice asset preserved: $target"
        return
    }
    $source = Get-Content -LiteralPath (Join-Path $saved 'Specs/scout_catalog_missing_source.json') -Raw -Encoding utf8 | ConvertFrom-Json
    if ($source.root -cne 'ScoutListContent') { throw 'Expected a native Scout list capture.' }
    $nodes = @($source.nodes | Where-Object name -CEQ 'ScoutCatalogUnavailable')
    if ($nodes.Count -ne 1 -or $nodes[0].class -cne '/Script/UMG.TextBlock') {
        throw 'Expected the original Scout catalog notice text block.'
    }
    $spec = [ordered]@{ version = $source.version; label = 'ScoutCatalogUnavailable'; root = 'ScoutCatalogWarningRoot'; nodes = $nodes }
    $spec | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath (Join-Path $saved 'Specs/scout_catalog_missing_leaf.json') -Encoding utf8
    $baselineHash = (Get-FileHash -LiteralPath $existing -Algorithm SHA256).Hash
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
    $backup = Join-Path $saved "Backups/ScoutCatalogWarning_$stamp"
    New-Item -ItemType Directory -Path $backup | Out-Null
    Copy-Item -LiteralPath $existing -Destination $backup
    $copiedHash = (Get-FileHash -LiteralPath (Join-Path $backup 'WBP_RokScoutScreen.uasset') -Algorithm SHA256).Hash
    if ($copiedHash -cne $baselineHash) { throw "Scout screen backup differs from $existing" }
    Set-Content -LiteralPath (Join-Path $backup 'WBP_RokScoutScreen.sha256') -Value $baselineHash -Encoding ascii
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_scout_catalog_warning -Commands `
        'Rok.UiWbp.GeneratePage /Game/RokUI/Scout/WBP_RokScoutCatalogUnavailable scout_catalog_missing_leaf -RootName=ScoutCatalogWarningRoot'
    if (!(Test-Path -LiteralPath $target -PathType Leaf)) { throw "Expected notice asset at $target" }
    if ((Get-FileHash -LiteralPath $existing -Algorithm SHA256).Hash -cne $baselineHash) {
        throw "Scout screen changed during notice generation. Original backup: $backup"
    }
    Write-Output "Scout screen verified unchanged; backup=$backup"
    return
}

if ($Action -eq 'Export') { $Mode = 'Legacy' }
$out = Join-Path $saved "ScoutCatalogWarningAudit/$Run-$Mode"
if (Test-Path -LiteralPath $out) { throw "$out exists. Use a different -Run." }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$log = Join-Path $out 'run.log'
$testName = 'Rok.Ui.Scout.CatalogWarningRenderedFlow'
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$argsUE = @(
    (Join-Path $project 'Rok.uproject'), '/Game/Maps/L_RokUnityGameAppCity', '-game',
    '-RenderOffscreen', '-ResX=1440', '-ResY=810', '-ForceRes', '-Unattended', '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding',
    '-DisablePlugins=ModelContextProtocol', '-RokLocalOnly', '-RokNoNet', "-ExecCmds=""Automation RunTests $testName""",
    '-TestExit="Automation Test Queue Empty"', "-ReportExportPath=""$out""", "-abslog=""$log"""
)
if ($Mode -eq 'Legacy') { $argsUE += '-RokUiLegacy' }
if ($Action -eq 'Export') { $argsUE += '-RokScoutWarningExport' }
$process = Start-Process $engine -ArgumentList $argsUE -WorkingDirectory $project -WindowStyle Hidden -PassThru
Write-Output "PID=$($process.Id) evidence=$out"
if (!$process.WaitForExit(120000)) {
    Stop-Process -Id $process.Id -Force
    throw "Scout warning test timed out. See $log"
}
$index = Join-Path $out 'index.json'
if (!(Test-Path -LiteralPath $index -PathType Leaf)) { throw "Missing Automation report. See $log" }
$report = Get-Content -LiteralPath $index -Raw -Encoding utf8 | ConvertFrom-Json
$result = @($report.tests | Where-Object fullTestPath -CEQ $testName)
if ($process.ExitCode -ne 0 -or $result.Count -ne 1 -or $result[0].state -ne 'Success') {
    throw "Scout warning test='$($result.state)' exit=$($process.ExitCode). See $log"
}
Write-Output "Success warnings=$($result[0].warnings) errors=$($result[0].errors): $index"
