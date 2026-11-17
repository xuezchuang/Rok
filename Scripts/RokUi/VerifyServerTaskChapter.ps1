param(
    [Parameter(Mandatory = $true)][ValidateSet('Baseline', 'Generate', 'Replay', 'Tests', 'Navigation')][string]$Action,
    [ValidatePattern('^[a-z0-9_-]+$')][string]$Run = 'task_chapter_server',
    [ValidateSet('legacy', 'wbp')][string[]]$TestTracks = @('legacy', 'wbp')
)

$ErrorActionPreference = 'Stop'
$project = 'D:\ue\Rok\Rok.uproject'
$saved = 'D:\ue\Rok\Saved\RokUi'
$offline = '-RokLocalOnly -RokNoNet'
$states = @('Waiting', 'NoChapter', 'Empty', 'Unknown', 'Mixed', 'Complete')
$fixtureNames = @{ Waiting = 'waiting'; NoChapter = 'no_chapter'; Empty = 'empty'; Unknown = 'unknown'; Mixed = 'mixed'; Complete = 'complete' }
$assets = @('TaskChapterServer', 'TaskChapterServerRow', 'TaskChapterServerAggregateReward', 'TaskChapterServerTaskReward')
$specs = @('TaskChapterServerPage', 'TaskChapterServerRow', 'TaskChapterServerAggregateReward', 'TaskChapterServerTaskReward')

if ($Action -eq 'Baseline' -or $Action -eq 'Replay') {
    & py -3.12 (Join-Path $PSScriptRoot 'BuildTaskChapterReplayFixtures.py')
    if ($LASTEXITCODE -ne 0) { throw 'The replay fixtures could not be generated.' }
    New-Item -ItemType Directory -Force (Join-Path $saved 'Specs') | Out-Null
    foreach ($state in $states) {
        $name = "feature_TaskChapterServer$state"
        $fixture = Join-Path $saved "Fixtures/TaskChapter/$($fixtureNames[$state]).json"
        $argsForGame = "$offline -RokNetReplay=$fixture -RokOpenModal=tasks -RokTaskPage=chapter -RokTaskChapterStaticCapture"
        if ($Action -eq 'Baseline') {
            $spec = Join-Path $saved "Specs/$name.json"
            Remove-Item -LiteralPath $spec -ErrorAction SilentlyContinue
            & (Join-Path $PSScriptRoot 'Capture.ps1') -Name "${name}_base" -Delay 8 `
                -Extra "$argsForGame -RokUiLegacy -RokUiExportSpec=$spec"
            $log = Join-Path $saved "Captures/${name}_base.log"
            if (!(Test-Path -LiteralPath $spec) -or !(Select-String -LiteralPath $log -Pattern 'Rok UI spec: wrote' -Quiet)) {
                throw "No server chapter spec was exported for $state. See $log"
            }
        }
        else {
            & (Join-Path $PSScriptRoot 'Capture.ps1') -Name "${name}_page" -Delay 8 -Extra $argsForGame
            $log = Join-Path $saved "Captures/${name}_page.log"
            if (!(Select-String -LiteralPath $log -Pattern 'drawing with WBP .*/WBP_RokFeature_TaskChapterServer\.WBP_RokFeature_TaskChapterServer_C' -Quiet)) {
                throw "The chapter WBP did not load through the $state replay. See $log"
            }
            Write-Output $state
            & py -3.12 (Join-Path $PSScriptRoot 'PixelDiff.py') `
                (Join-Path $saved "Captures/${name}_base.png") (Join-Path $saved "Captures/${name}_page.png") `
                --out (Join-Path $saved "Captures/${name}_page_diff.png")
            if ($LASTEXITCODE -notin 0, 1) { throw "Screenshot comparison failed for $state." }
        }
    }
    return
}

if ($Action -eq 'Generate') {
    & py -3.12 (Join-Path $PSScriptRoot 'BuildTaskChapterWbpSpecs.py')
    if ($LASTEXITCODE -ne 0) { throw 'Chapter widget specs could not be prepared.' }
    $commands = for ($i = 0; $i -lt $assets.Count; $i++) {
        "Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_$($assets[$i]) feature_$($specs[$i])"
    }
    $reports = & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name "${Run}_generate" -Commands ($commands -join ', ')
    $reports
    if (!($reports -match '^exit=0 ')) { throw 'Chapter asset generation did not exit successfully.' }
    foreach ($asset in $assets) {
        if (!($reports -match "generated /Game/RokUI/Feature/WBP_RokFeature_$asset widgets=\d+ ")) {
            throw "Generation did not save WBP_RokFeature_$asset"
        }
    }
    return
}

$test = 'Rok.Ui.QuestChapter.ServerNavigation'
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$replay = Join-Path $saved 'Fixtures/TaskChapter/navigation.json'
if (!(Test-Path -LiteralPath $replay)) { throw 'Run the Baseline action to create the navigation snapshot.' }
$failed = @()
foreach ($mode in $TestTracks) {
    $legacy = $mode -eq 'legacy'
    if ($Action -eq 'Tests') {
        $unitName = "$Run-$mode-unit"
        & (Join-Path $PSScriptRoot 'RunTests.ps1') -Name $unitName -Filter 'Rok.Net.ServerTask.Widget' -LocalOnly -Legacy:$legacy -TimeoutSeconds 180
        $results = @(Import-Csv (Join-Path $saved "Tests/$unitName/results.csv"))
        if ($results.Count -ne 1 -or $results[0].state -ne 'Success') {
            $failed += "$mode widget"
            Write-Warning "Chapter widget test failed in $mode. See $saved/Tests/$unitName"
        }
    }

    $out = Join-Path $saved "Tests/$Run-$mode-navigation"
    if (Test-Path -LiteralPath $out) { throw "$out exists. Use a different -Run." }
    New-Item -ItemType Directory -Force $out | Out-Null
    $log = Join-Path $out 'run.log'
    $arguments = @(
        $project, '/Game/Maps/L_RokUnityGameAppCity', '-game', '-RenderOffscreen', '-ResX=1456', '-ResY=910',
        '-ForceRes', '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding', '-Unattended',
        '-DisablePlugins=ModelContextProtocol', '-RokLocalOnly', '-RokNoNet', "-RokNetReplay=$replay",
        '-RokTaskPage=chapter', "-ExecCmds=""Automation RunTests $test""", '-TestExit="Automation Test Queue Empty"',
        "-ReportExportPath=$out", "-abslog=$log"
    )
    if ($legacy) { $arguments += '-RokUiLegacy' }
    $process = Start-Process -FilePath $engine -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(180000)) {
        Stop-Process -Id $process.Id -Force
        $failed += "$mode navigation"
        Write-Warning "Chapter navigation timed out in $mode. See $log"
        continue
    }
    $index = Join-Path $out 'index.json'
    $result = @()
    if (Test-Path -LiteralPath $index) {
        $report = (Get-Content -LiteralPath $index -Raw -Encoding utf8).Trim([char]0, [char]0xFEFF) | ConvertFrom-Json
        $result = @($report.tests | Where-Object fullTestPath -CEQ $test)
    }
    if ($process.ExitCode -ne 0 -or $result.Count -ne 1 -or $result[0].state -ne 'Success') {
        $failed += "$mode navigation"
        Write-Warning "Chapter navigation failed in $mode. See $log"
        continue
    }
    Write-Output "$test $mode passed. Report $index"
}
if ($failed.Count) { throw "Chapter test failures ($($failed -join ', '))." }
