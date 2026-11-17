# Runs automation tests in a headless editor (UnrealEditor-Cmd -NullRHI: no window)
# and writes a pass/fail list. -Legacy adds -RokUiLegacy (C++-built UI), so a
# WBP conversion is checked by comparing a -Legacy run with a normal run (gate G4).
# Output: Saved/RokUi/Tests/<Name>/index.json (engine report) and results.csv.
# Example:
#   pwsh Scripts/RokUi/RunTests.ps1 -Name rok_legacy -Filter Rok. -Legacy
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Filter = 'Rok.',
    [switch]$Legacy,
    [switch]$LocalOnly,
    [int]$TimeoutSeconds = 5400
)

$ErrorActionPreference = 'Stop'
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = 'D:\ue\Rok\Rok.uproject'
$outDir = Join-Path 'D:\ue\Rok\Saved\RokUi\Tests' $Name
if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
New-Item -ItemType Directory -Force $outDir | Out-Null
$log = Join-Path $outDir 'run.log'

$arguments = @(
    $project, '-NullRHI', '-Unattended', '-NoSplash', '-NoSound', '-NoP4', '-NoLiveCoding',
    "-ExecCmds=""Automation RunTests $Filter""", '-TestExit="Automation Test Queue Empty"',
    "-ReportExportPath=$outDir", "-abslog=$log"
)
if ($Legacy) { $arguments += '-RokUiLegacy' }
if ($LocalOnly) { $arguments += '-RokLocalOnly' }
$process = Start-Process -FilePath $engine -ArgumentList $arguments -PassThru -NoNewWindow
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "test run $Name timed out after $TimeoutSeconds s (process $($process.Id) stopped)"
}

$index = Join-Path $outDir 'index.json'
if (-not (Test-Path $index)) { throw "test run $Name wrote no report; see $log" }
# The engine writes the report with a BOM and sometimes trailing NULs.
$report = (Get-Content $index -Raw -Encoding utf8).Trim([char]0, [char]0xFEFF) | ConvertFrom-Json
$rows = foreach ($test in $report.tests) {
    [pscustomobject]@{
        test = $test.fullTestPath
        state = $test.state
        warnings = $test.warnings
        errors = $test.errors
        first_error = (@($test.entries | Where-Object { $_.event.type -eq 'Error' } | Select-Object -First 1).event.message)
    }
}
$rows | Sort-Object test | Export-Csv -Path (Join-Path $outDir 'results.csv') -Encoding utf8
$rows | Group-Object state | ForEach-Object { Write-Output ("{0}: {1}" -f $_.Name, $_.Count) }
Write-Output "exit=$($process.ExitCode) report=$outDir"
