param(
    [Parameter(Mandatory = $true)][ValidateSet('Export', 'Generate', 'Replay')][string]$Action,
    [ValidateSet('Legacy', 'Wbp')][string]$Mode = 'Wbp',
    [ValidateSet('Resource', 'Barbarian')][string]$Entry = 'Resource',
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Run = (Get-Date -Format 'yyyyMMdd_HHmmss')
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if ($Action -eq 'Generate') {
    $flags = @(& py -3.12 (Join-Path $PSScriptRoot 'PrepareGatherArmySpecs.py'))
    if ($LASTEXITCODE -ne 0 -or $flags.Count -ne 2) { throw 'GatherArmy spec preparation failed.' }
    $target = '/Game/RokUI/Feature/WBP_RokFeature_GatherArmy'
    $commands = @(
        "Rok.UiWbp.GeneratePage $target gather_army_page -RootName=ExpeditionArmyRoot",
        "Rok.UiWbp.GeneratePage ${target}HeroChoiceRow gather_army_hero_row $($flags[0])",
        "Rok.UiWbp.GeneratePage ${target}SoldierRow gather_army_soldier_row $($flags[1])"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_gather_army -Commands ($commands -join ', ')
    foreach ($suffix in '', 'HeroChoiceRow', 'SoldierRow') {
        $file = Join-Path $project "Content/RokUI/Feature/WBP_RokFeature_GatherArmy$suffix.uasset"
        if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing GatherArmy asset $file" }
    }
    return
}

if ($Action -eq 'Export') {
    $Mode = 'Legacy'
    $Entry = 'Resource'
}
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$route = if ($Entry -eq 'Barbarian') { "$Run-Barbarian-$Mode" } else { "$Run-$Mode" }
$output = Join-Path $project "Saved/RokUi/GatherArmyAudit/$route"
if (Test-Path -LiteralPath $output) { throw "$output already exists. Use another -Run." }
New-Item -ItemType Directory $output -Force | Out-Null
$log = Join-Path $output 'run.log'
$test = 'Rok.Ui.World.GatherArmyRenderedFlow'
$arguments = @(
    (Join-Path $project 'Rok.uproject'), '/Game/Maps/L_RokUnityWorldMapPrototype', '-game',
    '-RenderOffscreen', '-ResX=1440', '-ResY=810', '-ForceRes', '-UseFixedTimeStep', '-FPS=60',
    '-Unattended', '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding', '-DisablePlugins=ModelContextProtocol',
    '-RokLocalOnly', '-RokNoNet', "-ExecCmds=""Automation RunTests $test""", '-TestExit="Automation Test Queue Empty"',
    "-ReportExportPath=""$output""", "-RokGatherArmyEvidence=""$output""", "-abslog=""$log"""
)
if ($Mode -eq 'Legacy') { $arguments += '-RokUiLegacy' }
if ($Action -eq 'Export') { $arguments += '-RokGatherArmyExport' }
if ($Entry -eq 'Barbarian') { $arguments += '-RokGatherArmyBarbarian' }
$process = Start-Process $engine -ArgumentList $arguments -WorkingDirectory $project -WindowStyle Hidden -PassThru
Write-Output "PID=$($process.Id) evidence=$output"
if (!$process.WaitForExit(120000)) {
    Stop-Process -Id $process.Id -Force
    throw "GatherArmy replay timed out. See $log"
}
$index = Join-Path $output 'index.json'
if (!(Test-Path -LiteralPath $index -PathType Leaf)) { throw "Missing automation report. See $log" }
$report = Get-Content -LiteralPath $index -Raw -Encoding utf8 | ConvertFrom-Json
$result = @($report.tests | Where-Object fullTestPath -CEQ $test)
if ($process.ExitCode -ne 0 -or $result.Count -ne 1 -or $result[0].state -ne 'Success') {
    throw "GatherArmy result='$($result.state)' exit=$($process.ExitCode). See $log"
}
Write-Output "Success warnings=$($result[0].warnings) errors=$($result[0].errors): $index"
