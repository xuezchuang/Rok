param(
    [ValidateSet('Legacy', 'Wbp')][string]$Mode = 'Wbp',
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Run = (Get-Date -Format 'yyyyMMdd_HHmmss')
)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$output = Join-Path $project "Saved/RokUi/CommanderTalentAudit/$Run-$Mode"
if (Test-Path -LiteralPath $output) { throw "$output already exists. Use another -Run." }
New-Item -ItemType Directory $output -Force | Out-Null
$log = Join-Path $output 'run.log'
$test = 'Rok.Ui.CommanderTalent.RenderedFlow'
$arguments = @(
    (Join-Path $project 'Rok.uproject'), '/Game/Maps/L_RokUnityGameAppCity', '-game',
    '-RenderOffscreen', '-ResX=1456', '-ResY=910', '-ForceRes',
    '-Unattended', '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding', '-DisablePlugins=ModelContextProtocol',
    '-RokLocalOnly', '-RokNoNet', '-RokOpenModal=commander', '-RokCommanderStaticCapture',
    "-ExecCmds=`"Automation RunTests $test`"", '-TestExit="Automation Test Queue Empty"',
    "-ReportExportPath=`"$output`"", "-abslog=`"$log`""
)
if ($Mode -eq 'Legacy') { $arguments += '-RokUiLegacy' }
$process = Start-Process $engine -ArgumentList $arguments -WorkingDirectory $project -WindowStyle Hidden -PassThru
Write-Output "PID=$($process.Id) evidence=$output"
if (!$process.WaitForExit(120000)) {
    Stop-Process -Id $process.Id -Force
    throw "Commander talent replay timed out. See $log"
}
$index = Join-Path $output 'index.json'
if (!(Test-Path -LiteralPath $index -PathType Leaf)) { throw "Missing automation report. See $log" }
$report = Get-Content -LiteralPath $index -Raw -Encoding utf8 | ConvertFrom-Json
$result = @($report.tests | Where-Object fullTestPath -CEQ $test)
if ($process.ExitCode -ne 0 -or $result.Count -ne 1 -or $result[0].state -ne 'Success') {
    throw "Commander talent result='$($result.state)' exit=$($process.ExitCode). See $log"
}
Write-Output "Success warnings=$($result[0].warnings) errors=$($result[0].errors): $index"
