param([int]$TimeoutSeconds = 900)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$expectedProject = 'D:\ue\Rok\Rok.uproject'
if (-not $projectRoot.Equals('D:\ue\Rok', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected project root: $projectRoot"
}
$projectFile = Join-Path $projectRoot 'Rok.uproject'
if (-not (Test-Path -LiteralPath $projectFile) -or
    -not (Resolve-Path -LiteralPath $projectFile).Path.Equals($expectedProject, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Project identity check failed: $projectFile"
}
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $engine)) { throw "Authorized UE 5.8.4 commandlet was not found: $engine" }

$tests = @(
    'Rok.Ui.ChapterDialog.Screen',
    'Rok.Ui.Expedition.SourceErrorWbp'
)
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$root = Join-Path $projectRoot "Saved/RokUi/Tests/UiEmptyState_$stamp"
New-Item -ItemType Directory -Force -Path $root | Out-Null
$results = @()
foreach ($mode in @('Wbp', 'Legacy')) {
    foreach ($test in $tests) {
        $safeTest = $test.Replace('.', '_')
        $outDir = Join-Path $root "${mode}_$safeTest"
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null
        $log = Join-Path $outDir 'run.log'
        $arguments = @(
            $projectFile, '-NullRHI', '-Unattended', '-NoSplash', '-NoSound', '-NoP4', '-NoLiveCoding',
            "-ExecCmds=`"Automation RunTests $test`"", '-TestExit="Automation Test Queue Empty"',
            "-ReportExportPath=$outDir", '-RokLocalOnly', '-RokExpeditionSourceErrorFixture', "-abslog=$log"
        )
        if ($mode -eq 'Legacy') { $arguments += '-RokUiLegacy' }
        $process = Start-Process -FilePath $engine -ArgumentList $arguments -PassThru -NoNewWindow
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            throw "$mode $test timed out after $TimeoutSeconds seconds (process $($process.Id) stopped)"
        }
        $index = Join-Path $outDir 'index.json'
        if (-not (Test-Path -LiteralPath $index)) { throw "$mode $test wrote no automation report; see $log" }
        $report = (Get-Content $index -Raw -Encoding utf8).Trim([char]0, [char]0xFEFF) | ConvertFrom-Json
        $matching = @($report.tests | Where-Object { $_.fullTestPath -eq $test })
        if ($matching.Count -ne 1) { throw "$mode run did not execute exactly $test; see $log" }
        $state = $matching[0].state
        $results += [pscustomobject]@{ mode = $mode; test = $test; state = $state; exit = $process.ExitCode; log = $log }
		if ($process.ExitCode -ne 0 -or $state -notin @('Success', 'Passed')) {
			throw "$mode $test failed (exit=$($process.ExitCode), state=$state); see $log"
		}
        Write-Output "$mode $test $state log=$log"
    }
}
$results | Export-Csv -Path (Join-Path $root 'results.csv') -NoTypeInformation -Encoding utf8
Write-Output "All focused WBP/Legacy checks passed; evidence=$root"
