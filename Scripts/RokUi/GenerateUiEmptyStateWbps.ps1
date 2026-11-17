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

& py -3.12 (Join-Path $PSScriptRoot 'BuildUiEmptyStateWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'BuildUiEmptyStateWbpSpecs.py failed' }

$assets = @(
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionSourceError.uasset',
    'Content/RokUI/Modal/Pages/WBP_RokModal_ChapterDialogEmpty.uasset'
)
foreach ($relative in $assets) {
    if (Test-Path -LiteralPath (Join-Path $projectRoot $relative)) {
        throw "Refusing to overwrite existing asset: $relative"
    }
}

$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $engine)) {
	throw "Authorized UE 5.8.4 commandlet was not found: $engine"
}
$logDir = Join-Path $projectRoot 'Saved/RokUi/Logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir 'gen_ui_empty_state_wbps.log'
Remove-Item -LiteralPath $log -ErrorAction SilentlyContinue
$commands = @(
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_ExpeditionSourceError expedition_source_error -ParentClass=/Script/Rok.RokUiPageWidget -RootName=ExpeditionSourceErrorRoot',
    'Rok.UiWbp.GeneratePage /Game/RokUI/Modal/Pages/WBP_RokModal_ChapterDialogEmpty chapter_dialog_empty -ParentClass=/Script/Rok.RokUiPageWidget -RootName=ChapterDialogEmptyRoot'
) -join ', '
$arguments = @(
    $projectFile, '-NullRHI', '-Unattended', '-NoSplash', '-NoSound', '-NoP4', '-NoLiveCoding', '-RokUiLegacy',
    "-ExecCmds=`"$commands, QUIT_EDITOR`"", "-abslog=$log"
)
$process = Start-Process -FilePath $engine -ArgumentList $arguments -PassThru -NoNewWindow
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "WBP generation timed out after $TimeoutSeconds seconds (process $($process.Id) stopped)"
}
Select-String -Path $log -Pattern 'RokUiWbp|Error:' | ForEach-Object { $_.Line }
Write-Output "exit=$($process.ExitCode) log=$log"
if ($process.ExitCode -ne 0) { throw "WBP generation failed; inspect $log" }
